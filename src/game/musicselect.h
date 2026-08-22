#ifndef _MUSICSELECT_H_
#define _MUSICSELECT_H_

#include <ultra64.h>

s32 musicGetAmbientTrackForStage(s32 stageID);
s32 musicGetXTrackForStage(s32 stageID);
s32 musicGetMainTrackOrRandom(s32 stageID);

#endif
