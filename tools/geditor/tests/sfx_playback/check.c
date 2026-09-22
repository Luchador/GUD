#define main CatalogChecks
#include "../sfx_catalog/check.c"
#undef main
#include "sfxpreview.h"

enum { ENV = 0x1100, KEY = 0x1120, BOOK = 0x1200, LOOP = 0x1240 };
static void playable(RomFile *rom)
{
    fixture(rom, 2);
    unsigned char *c = rom->data + CTL;
    put32(c + SOUND, ENV); put32(c + SOUND + 4, KEY);
    c[SOUND + 12] = 64; c[SOUND + 13] = 127;
    put32(c + ENV + 4, 100000); put32(c + ENV + 8, 10000);
    c[ENV + 12] = c[ENV + 13] = 127;
    c[KEY + 4] = 60;
    put32(c + WAVE + 16, BOOK);
    put32(c + BOOK, 2); put32(c + BOOK + 4, 1);
    put32(c + WAVE + 4, 18);
    rom->data[TBL + 12] = rom->data[TBL + 21] = 0x80;
    memset(rom->data + TBL + 13, 0x71, 8);
    memset(rom->data + TBL + 22, 0x71, 8);
}

static void decodeChecks(void)
{
    unsigned char data[18] = {0,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
    unsigned char book[72] = {0};
    int16_t *samples; DWORD count; const char *why;
    put32(book, 2); put32(book + 4, 2);
    assert(SfxDecodeSample(data, 9, 0, book, sizeof(book), &samples, &count, &why));
    assert(count == 16);
    for (int i = 0; i < 16; i++) { assert(samples[i] == (i < 8 ? i : i - 16)); }
    free(samples);
    /* Predictor 1: each output repeats the preceding reconstructed sample. */
    for (int i = 0; i < 8; i++) { put16(book + 8 + 32 + 16 + i * 2, 2048); }
    data[9] = 1;
    assert(SfxDecodeSample(data, 18, 0, book, sizeof(book), &samples, &count, &why));
    for (int i = 16; i < 32; i++) { assert(samples[i] == -1); }
    free(samples);
    data[0] = 0xf0; /* RSP clamps a scale above 12 to 12. */
    assert(SfxDecodeSample(data, 9, 0, book, sizeof(book), &samples, &count, &why));
    assert(samples[7] == 28672 && samples[8] == -32768); free(samples);
    data[0] = 2;
    assert(!SfxDecodeSample(data, 9, 0, book, sizeof(book), &samples, &count, &why));
    assert(!samples && !count && why[0]);
    assert(!SfxDecodeSample(data, 9, 0, book, 12, &samples, &count, &why));
    const unsigned char raw[] = {0x80,0, 0x7f,0xff, 0xff,0xff, 0,1};
    assert(SfxDecodeSample(raw, sizeof(raw), 1, NULL, 0, &samples, &count, &why));
    assert(count == 4 && samples[0] == -32768 && samples[1] == 32767 && samples[2] == -1 && samples[3] == 1);
    free(samples);
}

static void renderChecks(void)
{
    RomFile rom; SfxCatalog catalog = {0}; SfxPcm pcm = {0}; const char *why;
    playable(&rom);
    assert(SfxCatalogReadRom(&rom, &catalog, &why));
    assert(SfxPreviewRender(&rom, &catalog, 1, &pcm, &why));
    assert(pcm.frames == 64 && pcm.sampleRate == 44100 && !pcm.limited);
    int original = pcm.samples[0]; assert(original > 0);
    rom.data[CTL + SOUND + 13] = 63;
    assert(SfxPreviewRender(&rom, &catalog, 1, &pcm, &why));
    assert(pcm.samples[0] < original / 3); /* Quiet variants must not be normalized. */
    rom.data[CTL + KEY + 4] = 48;
    assert(SfxPreviewRender(&rom, &catalog, 1, &pcm, &why) && pcm.frames == 128);

    /* A linked second sound has its own delay and volume. */
    put32(rom.data + CTL + INST + 20, SOUND + 64);
    memcpy(rom.data + CTL + SOUND + 64, rom.data + CTL + SOUND, 16);
    put32(rom.data + CTL + SOUND + 68, KEY + 16);
    memcpy(rom.data + CTL + KEY + 16, rom.data + CTL + KEY, 6);
    rom.data[CTL + KEY] = 2; rom.data[CTL + KEY + 17] = 3;
    assert(SfxCatalogReadRom(&rom, &catalog, &why));
    assert(SfxPreviewRender(&rom, &catalog, 1, &pcm, &why) && pcm.frames > 4410);
    rom.data[CTL + KEY + 16] = 1;
    assert(!SfxPreviewRender(&rom, &catalog, 1, &pcm, &why) && !pcm.samples);
    rom.data[CTL + KEY] = 0;
    put32(rom.data + CTL + WAVE + 12, LOOP);
    put32(rom.data + CTL + LOOP, 2); put32(rom.data + CTL + LOOP + 4, 30);
    put32(rom.data + CTL + LOOP + 8, 0xffffffffu);
    put32(rom.data + CTL + ENV + 4, 0xffffffffu);
    assert(SfxPreviewRender(&rom, &catalog, 1, &pcm, &why) && pcm.limited && pcm.frames == 44100 * 5);
    put32(rom.data + CTL + LOOP + 4, 10000);
    assert(!SfxPreviewRender(&rom, &catalog, 1, &pcm, &why));
    put32(rom.data + CTL + SOUND, 0xfffffffc);
    assert(!SfxPreviewRender(&rom, &catalog, 1, &pcm, &why));
    SfxPcmFree(&pcm); SfxCatalogFree(&catalog); free(rom.data);
}

static void renderReal(const char *path)
{
    RomFile rom = {0}; SfxCatalog catalog = {0}; SfxPcm pcm = {0}; const char *why;
    FILE *f = fopen(path, "rb"); assert(f); fseek(f, 0, SEEK_END);
    long size = ftell(f); assert(size > 0 && size <= 64 * 1024 * 1024); rewind(f);
    rom.size = (DWORD)size; rom.data = malloc(rom.size); assert(rom.data);
    assert(fread(rom.data, 1, rom.size, f) == rom.size); fclose(f);
    DWORD m;
    for (m = 0; m + 24 <= rom.size; m += 4)
        if (!memcmp(rom.data + m, "GUDGEDITORMANIF", 16)) { break; }
    assert(m + 24 <= rom.size); rom.info.entrycount = get32(rom.data + m + 20);
    assert(rom.info.entrycount <= ROM_MAX_ENTRIES && m + 24 + rom.info.entrycount * 16 <= rom.size);
    for (DWORD i = 0; i < rom.info.entrycount; i++)
    {
        unsigned char *e = rom.data + m + 24 + i * 16;
        rom.info.entries[i] = (RomManifestEntry){get32(e), get32(e + 4), get32(e + 8), get32(e + 12)};
    }
    assert(SfxCatalogReadRom(&rom, &catalog, &why));
    for (DWORD id = 1; id <= catalog.count; id++)
    {
        if (!SfxPreviewRender(&rom, &catalog, id, &pcm, &why)) { fprintf(stderr, "SFX %u: %s\n", id, why); abort(); }
        assert(pcm.frames && pcm.frames <= pcm.sampleRate * 10);
        BOOL audible = FALSE;
        for (DWORD i = 0; i < pcm.frames * 2; i++) { if (pcm.samples[i]) { audible = TRUE; break; } }
        assert(audible);
    }
    printf("PASS: all %u ROM effects render non-silent stereo previews.\n", catalog.count);
    SfxPcmFree(&pcm); SfxCatalogFree(&catalog); free(rom.data);
}

int main(int argc, char **argv)
{
    decodeChecks(); renderChecks();
    puts("PASS: ADPCM predictor/history/scaling, PCM endianness, pitch/volume, linked effects, loops and invalid metadata.");
    if (argc == 2) { renderReal(argv[1]); }
    return 0;
}
