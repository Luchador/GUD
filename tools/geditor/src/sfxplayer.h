#ifndef GEDITOR_SFXPLAYER_H
#define GEDITOR_SFXPLAYER_H
#include "sfxpreview.h"

/* Transfers PCM ownership once the previous preview has stopped. */
BOOL SfxPlayerPlay(SfxPcm *pcm, const char **reason);
BOOL SfxPlayerStop(const char **reason);
BOOL SfxPlayerIsPlaying(void);
BOOL SfxPlayerHasOutput(void);

#endif
