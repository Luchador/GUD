#include "sfxplayer.h"
#include <mmsystem.h>
#include <string.h>

static HWAVEOUT g_Output;
static WAVEHDR g_Header;
static SfxPcm g_Pcm;
static BOOL g_Prepared, g_Queued;

BOOL SfxPlayerStop(const char **reason)
{
    /* A failed driver call must not free memory still owned by waveOut. Keep
     * the device/buffer available for a subsequent Stop attempt. */
    if (g_Output)
    {
        if (waveOutReset(g_Output) != MMSYSERR_NOERROR) { goto fail; }
        g_Queued = FALSE;
        if (g_Prepared)
        {
            if (waveOutUnprepareHeader(g_Output, &g_Header, sizeof(g_Header)) != MMSYSERR_NOERROR) { goto fail; }
            g_Prepared = FALSE;
        }
        if (waveOutClose(g_Output) != MMSYSERR_NOERROR) { goto fail; }
        g_Output = NULL;
    }
    SfxPcmFree(&g_Pcm);
    memset(&g_Header, 0, sizeof(g_Header));
    if (reason) { *reason = ""; }
    return TRUE;
fail:
    if (reason) { *reason = "Windows could not stop the sound preview. Try Stop again."; }
    return FALSE;
}

BOOL SfxPlayerIsPlaying(void)
{ return g_Output && g_Queued && !(g_Header.dwFlags & WHDR_DONE); }

BOOL SfxPlayerHasOutput(void)
{ return g_Output != NULL; }

BOOL SfxPlayerPlay(SfxPcm *pcm, const char **reason)
{
    const char *why = "Windows could not start the sound preview.";
    if (!SfxPlayerStop(reason)) { return FALSE; }
    if (!pcm->samples || !pcm->frames || pcm->frames > 0xffffffffu / 4 || !pcm->sampleRate
        || pcm->sampleRate > 192000)
    { if (reason) { *reason = "Invalid sound preview buffer."; } return FALSE; }
    g_Pcm = *pcm; memset(pcm, 0, sizeof(*pcm));
    WAVEFORMATEX format = {0};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2; format.wBitsPerSample = 16;
    format.nSamplesPerSec = g_Pcm.sampleRate;
    format.nBlockAlign = 4; format.nAvgBytesPerSec = format.nSamplesPerSec * 4;
    if (waveOutOpen(&g_Output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    { g_Output = NULL; why = "No Windows audio output is available for the sound preview."; goto fail; }
    g_Header.lpData = (LPSTR)g_Pcm.samples;
    g_Header.dwBufferLength = g_Pcm.frames * 4;
    if (waveOutPrepareHeader(g_Output, &g_Header, sizeof(g_Header)) != MMSYSERR_NOERROR) { goto fail; }
    g_Prepared = TRUE;
    if (waveOutWrite(g_Output, &g_Header, sizeof(g_Header)) != MMSYSERR_NOERROR) { goto fail; }
    g_Queued = TRUE;
    if (reason) { *reason = ""; }
    return TRUE;
fail:
    SfxPlayerStop(NULL);
    if (reason) { *reason = why; }
    return FALSE;
}
