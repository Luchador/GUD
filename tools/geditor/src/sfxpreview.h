#ifndef GEDITOR_SFXPREVIEW_H
#define GEDITOR_SFXPREVIEW_H

#include <stdint.h>
#include "sfxcatalog.h"

typedef struct SfxPcm {
    int16_t *samples; /* Interleaved stereo, owned. */
    DWORD frames, sampleRate;
    BOOL limited;
} SfxPcm;

/* Native mono sample decoding, independent of playback and effect metadata. */
BOOL SfxDecodeSample(const unsigned char *data, DWORD bytes, unsigned format,
                     const unsigned char *book, DWORD bookBytes,
                     int16_t **samples, DWORD *count, const char **reason);
BOOL SfxPreviewRender(const RomFile *rom, const SfxCatalog *catalog, DWORD id,
                      SfxPcm *pcm, const char **reason);
void SfxPcmFree(SfxPcm *pcm);

#endif
