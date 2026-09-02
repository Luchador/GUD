#ifndef _CLEANUP_H_
#define _CLEANUP_H_

#include <ultra64.h>

void cleanupGuardData(void);
void cleanupObjectSounds(void);
void cleanupAlarms(void);
void cleanupObjects(s32 stage);
void cleanupObjectives(void);
void cleanupSFXRelated(void);
void cleanupExplosions(void);
void cleanup_window_pieces(void);
void cleanupplayersoundrelated(void);

#endif
