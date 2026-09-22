#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "sfxplayer.h"
#include <mmsystem.h>

static int error, opened, prepared, queued;
static WAVEHDR *header;
MMRESULT waveOutOpen(HWAVEOUT *out, UINT device, const WAVEFORMATEX *fmt, DWORD_PTR cb, DWORD_PTR instance, DWORD flags)
{
    assert(!opened && device == WAVE_MAPPER && flags == CALLBACK_NULL && !cb);
    assert(fmt->nChannels == 2 && fmt->wBitsPerSample == 16 && fmt->nSamplesPerSec == 44100);
    if (error == 1) { return 1; }
    *out = (void *)1; opened = 1; return 0;
}
MMRESULT waveOutPrepareHeader(HWAVEOUT out, WAVEHDR *h, UINT size)
{
    assert(opened && !prepared && h->lpData && h->dwBufferLength == 400);
    if (error == 2) { return 1; }
    header = h; prepared = 1; return 0;
}
MMRESULT waveOutWrite(HWAVEOUT out, WAVEHDR *h, UINT size)
{
    assert(opened && prepared && header == h && !queued);
    if (error == 3) { return 1; }
    h->dwFlags &= ~WHDR_DONE; queued = 1; return 0;
}
MMRESULT waveOutReset(HWAVEOUT out)
{
    assert(opened);
    if (error == 4) { return 1; }
    queued = 0; if (header) { header->dwFlags |= WHDR_DONE; } return 0;
}
MMRESULT waveOutUnprepareHeader(HWAVEOUT out, WAVEHDR *h, UINT size)
{
    assert(opened && prepared && !queued);
    if (error == 5) { return 1; }
    prepared = 0; header = NULL; return 0;
}
MMRESULT waveOutClose(HWAVEOUT out)
{
    assert(opened && !prepared && !queued);
    if (error == 6) { return 1; }
    opened = 0; return 0;
}
static SfxPcm pcm(void)
{
    SfxPcm p = {0}; p.frames = 100; p.sampleRate = 44100;
    p.samples = calloc(200, sizeof(*p.samples)); assert(p.samples); return p;
}
int main(void)
{
    const char *why;
    SfxPcm p = pcm();
    assert(SfxPlayerPlay(&p, &why) && !p.samples && SfxPlayerIsPlaying());
    p = pcm(); /* Replacement must reset/unprepare/close before queuing new audio. */
    assert(SfxPlayerPlay(&p, &why) && !p.samples && SfxPlayerIsPlaying());
    header->dwFlags |= WHDR_DONE; queued = 0;
    assert(!SfxPlayerIsPlaying() && SfxPlayerStop(&why) && !opened);
    assert(SfxPlayerStop(&why));
    for (error = 1; error <= 3; error++)
    {
        p = pcm(); assert(!SfxPlayerPlay(&p, &why) && why[0]);
        assert(!opened && !p.samples); SfxPcmFree(&p);
    }
    for (int fail = 4; fail <= 6; fail++)
    {
        error = 0; p = pcm(); assert(SfxPlayerPlay(&p, &why));
        error = fail; assert(!SfxPlayerStop(&why) && opened);
        p = pcm(); assert(!SfxPlayerPlay(&p, &why) && p.samples); SfxPcmFree(&p);
        error = 0; assert(SfxPlayerStop(&why) && !opened);
    }
    puts("PASS: asynchronous playback, replacement, completion, repeated stop and driver-error buffer ownership.");
    return 0;
}
