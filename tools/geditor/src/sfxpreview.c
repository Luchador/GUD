#include "sfxpreview.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PREVIEW_RATE 44100u
#define PREVIEW_SECONDS 10u
#define LOOP_SECONDS 5u
#define MAX_DECODE_SAMPLES (4u * 1024u * 1024u)

static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static int16_t Read16(const unsigned char *p)
{ unsigned n = (unsigned)p[0] << 8 | p[1]; return n < 32768 ? (int)n : (int)n - 65536; }
static int32_t Signed32(DWORD n)
{ return n <= 0x7fffffffu ? (int32_t)n : (int32_t)((int64_t)n - 0x100000000LL); }
static int16_t Clamp(int64_t n)
{ return (int16_t)(n < -32768 ? -32768 : n > 32767 ? 32767 : n); }

BOOL SfxDecodeSample(const unsigned char *data, DWORD bytes, unsigned format,
                     const unsigned char *book, DWORD bookBytes,
                     int16_t **samples, DWORD *count, const char **reason)
{
    int16_t *out = NULL;
    const char *why = "Invalid sound sample or ADPCM predictor book.";
    DWORD predictors = 0;
    *samples = NULL; *count = 0;
    if (!data || format > 1) { why = "This sound's sample format is not supported."; goto fail; }
    /* libultra ignores incomplete ADPCM frames at the end of a wavetable. */
    DWORD frames = format == 0 ? bytes / 9 : bytes / 2;
    if (!frames || frames > MAX_DECODE_SAMPLES / (format == 0 ? 16 : 1)) { goto fail; }
    DWORD length = frames * (format == 0 ? 16 : 1);
    if (format == 0)
    {
        if (!book || bookBytes < 8 || Read32(book) != 2) { goto fail; }
        predictors = Read32(book + 4);
        if (!predictors || predictors > 16 || predictors > (bookBytes - 8) / 32) { goto fail; }
    }
    out = malloc((size_t)length * sizeof(*out));
    if (!out) { why = "Out of memory decoding sound."; goto fail; }
    if (format == 1)
    {
        for (DWORD i = 0; i < length; i++) { out[i] = Read16(data + i * 2); }
    }
    else
    {
        int history[2] = {0, 0};
        for (DWORD frame = 0; frame < frames; frame++)
        {
            const unsigned char *packet = data + frame * 9;
            unsigned predictor = packet[0] & 15, shift = packet[0] >> 4;
            if (predictor >= predictors) { goto fail; }
            if (shift > 12) { shift = 12; }
            const unsigned char *coefficients = book + 8 + predictor * 32;
            /* Each 9-byte frame contains two groups of eight signed residuals.
             * The Q11 predictor uses the last two reconstructed samples and
             * earlier residuals in this group, then saturates to signed PCM. */
            for (unsigned group = 0; group < 2; group++)
            {
                int residual[8];
                int16_t *decoded = out + frame * 16 + group * 8;
                for (unsigned i = 0; i < 8; i++)
                {
                    unsigned packed = packet[1 + group * 4 + i / 2];
                    int nibble = i & 1 ? packed & 15 : packed >> 4;
                    residual[i] = (nibble >= 8 ? nibble - 16 : nibble) * (1 << shift);
                    int64_t value = (int64_t)residual[i] * 2048
                        + (int64_t)Read16(coefficients + i * 2) * history[0]
                        + (int64_t)Read16(coefficients + 16 + i * 2) * history[1];
                    for (unsigned j = 0; j < i; j++)
                    { value += (int64_t)Read16(coefficients + 16 + (i - j - 1) * 2) * residual[j]; }
                    decoded[i] = Clamp(value < 0 ? -((-value + 2047) / 2048) : value / 2048);
                }
                history[0] = decoded[6]; history[1] = decoded[7];
            }
        }
    }
    *samples = out; *count = length;
    if (reason) { *reason = ""; }
    return TRUE;
fail:
    free(out);
    if (reason) { *reason = why; }
    return FALSE;
}

void SfxPcmFree(SfxPcm *pcm)
{ free(pcm->samples); memset(pcm, 0, sizeof(*pcm)); }

static const unsigned char *Control(const RomFile *rom, const SfxCatalog *catalog, DWORD offset, DWORD bytes)
{
    return offset && offset <= catalog->controlBytes && bytes <= catalog->controlBytes - offset
        ? rom->data + catalog->controlOffset + offset : NULL;
}

static double Envelope(double t, double attack, double decay, double release, double av, double dv)
{
    if (t < attack) { return av * t / attack; }
    t -= attack;
    if (decay < 0) { return av; }
    if (t < decay) { return av + (dv - av) * t / decay; }
    t -= decay;
    return t < release ? dv * (1 - t / release) : 0;
}

BOOL SfxPreviewRender(const RomFile *rom, const SfxCatalog *catalog, DWORD id,
                      SfxPcm *pcm, const char **reason)
{
    const char *why = "This sound's playback metadata is invalid.";
    float *mix = NULL;
    unsigned char *visited = NULL;
    int16_t *sample = NULL;
    DWORD end = 0, layers = 0, capacity = PREVIEW_RATE * PREVIEW_SECONDS;
    BOOL limited = FALSE;
    SfxPcmFree(pcm);
    if (!rom || !rom->data || !catalog || !catalog->entries || !id || id > catalog->count || catalog->count > 32767
        || catalog->controlOffset > rom->size || catalog->controlBytes > rom->size - catalog->controlOffset
        || catalog->sampleRate < 4000 || catalog->sampleRate > 192000) { goto fail; }
    mix = calloc((size_t)capacity * 2, sizeof(*mix));
    visited = calloc(catalog->count, 1);
    if (!mix || !visited) { why = "Out of memory preparing sound preview."; goto fail; }
    double totalDelay = 0;
    while (id)
    {
        if (++layers > 64 || id > catalog->count || visited[id - 1])
        { why = "This sound contains an invalid or excessively long linked-sound sequence."; goto fail; }
        visited[id - 1] = 1;
        const SfxEntry *entry = &catalog->entries[id - 1];
        if (entry->definitionOffset < catalog->controlOffset) { goto fail; }
        const unsigned char *sound = Control(rom, catalog, entry->definitionOffset - catalog->controlOffset, 16);
        if (!sound) { goto fail; }
        const unsigned char *env = Control(rom, catalog, Read32(sound), 16);
        const unsigned char *key = Control(rom, catalog, Read32(sound + 4), 6);
        const unsigned char *wave = Control(rom, catalog, Read32(sound + 8), 20);
        if (!env || !key || !wave || sound[12] > 127 || sound[13] > 127 || env[12] > 127 || env[13] > 127) { goto fail; }
        DWORD bookOffset = Read32(wave + 16), count = 0;
        const unsigned char *book = entry->format == 0 ? Control(rom, catalog, bookOffset, 8) : NULL;
        if (entry->sampleOffset > rom->size || entry->sampleBytes > rom->size - entry->sampleOffset) { goto fail; }
        if (!SfxDecodeSample(rom->data + entry->sampleOffset, entry->sampleBytes, entry->format,
                book, book ? catalog->controlBytes - bookOffset : 0, &sample, &count, &why)) { goto fail; }
        why = "This sound's playback metadata is invalid.";
        DWORD loopStart = 0, loopEnd = 0, loops = 0;
        if (Read32(wave + 12))
        {
            const unsigned char *loop = Control(rom, catalog, Read32(wave + 12), entry->format == 0 ? 44 : 12);
            if (!loop) { goto fail; }
            loopStart = Read32(loop); loopEnd = Read32(loop + 4); loops = Read32(loop + 8);
            if (loops && (loopStart >= loopEnd || loopEnd > count)) { goto fail; }
        }
        int detune = key[5] < 128 ? key[5] : (int)key[5] - 256;
        BOOL slide = (key[3] & 0x20) != 0;
        double pitch = pow(2.0, ((int)key[4] * 100 - 6000 + (slide ? 0 : detune)) / 1200.0);
        double attack = Signed32(Read32(env)) / (1000000.0 * pitch);
        int32_t decayUs = Signed32(Read32(env + 4));
        double decay = decayUs == -1 ? -1 : decayUs / (1000000.0 * pitch);
        double release = Signed32(Read32(env + 8)) / (1000000.0 * pitch);
        if (attack < 0 || decayUs < -1 || release < 0) { goto fail; }
        double delay = (key[3] & 0x10) ? totalDelay : key[1] * 0.033333;
        totalDelay += key[1] * 0.033333;
        double duration = decay < 0 ? LOOP_SECONDS : attack + decay + release;
        if (duration > PREVIEW_SECONDS - delay) { duration = PREVIEW_SECONDS - delay; limited = TRUE; }
        if (decay < 0 || (key[3] & 0x10)) { limited = TRUE; }
        double av = env[12] * sound[13] / 16129.0, dv = env[13] * sound[13] / 16129.0;
        /* The N64 envelope mixer squares volume and uses equal-power panning. */
        av *= av; dv *= dv;
        double angle = sound[12] * (1.5707963267948966 / 127.0);
        double left = cos(angle), right = sin(angle), position = 0;
        DWORD start = delay < PREVIEW_SECONDS ? (DWORD)(delay * PREVIEW_RATE) : capacity;
        DWORD length = duration > 0 ? (DWORD)(duration * PREVIEW_RATE) : 0;
        if (length > capacity - start) { length = capacity - start; }
        for (DWORD i = 0; i < length; i++)
        {
            if (loops && position >= loopEnd)
            {
                double repeats = floor((position - loopEnd) / (loopEnd - loopStart)) + 1;
                double use = loops == 0xffffffffu || repeats < loops ? repeats : loops;
                position -= use * (loopEnd - loopStart);
                if (loops != 0xffffffffu) { loops -= (DWORD)use; }
            }
            if (position >= count) { break; }
            DWORD a = (DWORD)position, b = a + 1;
            if (loops && b >= loopEnd) { b = loopStart; }
            if (b >= count) { b = a; }
            double t = (double)i / PREVIEW_RATE;
            double gain = Envelope(t, attack, decay, release, av, dv);
            /* Short tail fade avoids an abrupt cutoff at the preview limit. */
            if (length - i < PREVIEW_RATE / 100) { gain *= (double)(length - i) / (PREVIEW_RATE / 100); }
            double value = (sample[a] + (sample[b] - sample[a]) * (position - a)) * gain;
            mix[(start + i) * 2] += (float)(value * left);
            mix[(start + i) * 2 + 1] += (float)(value * right);
            if (start + i + 1 > end) { end = start + i + 1; }
            double ratio = pitch;
            if (slide && decay >= 0 && t >= attack)
            { ratio *= pow(2.0, detune * floor((t - attack) / 0.033333) / 1200.0); }
            if (ratio > 2) { ratio = 2; }
            position += ratio * catalog->sampleRate / PREVIEW_RATE;
        }
        free(sample); sample = NULL;
        id = key[0] + ((key[2] & 0xc0u) << 2);
    }
    if (!end) { why = "This sound has no playable samples."; goto fail; }
    pcm->samples = malloc((size_t)end * 2 * sizeof(*pcm->samples));
    if (!pcm->samples) { why = "Out of memory preparing sound preview."; goto fail; }
    for (DWORD i = 0; i < end * 2; i++) { pcm->samples[i] = Clamp((int64_t)mix[i]); }
    pcm->frames = end; pcm->sampleRate = PREVIEW_RATE; pcm->limited = limited;
    free(mix); free(visited);
    if (reason) { *reason = ""; }
    return TRUE;
fail:
    free(sample); free(mix); free(visited); SfxPcmFree(pcm);
    if (reason) { *reason = why; }
    return FALSE;
}
