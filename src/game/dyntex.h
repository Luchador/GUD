#ifndef _DYN_TEX_H_
#define _DYN_TEX_H_
#include <ultra64.h>
#include "bondview.h"


extern f32 g_DyntexWaterScrollS;
extern f32 g_DyntexWaterScrollT;
extern f32 g_DyntexWaterBlendPhase;

void dyntexWaterController(void);
Gfx* dyntexConfigureTwoLayerWater(Gfx *gdl, s32 useIntensityTexture);
Gfx* dyntexConfigureTwoLayerCiWater(Gfx *gdl);

#endif
