#ifndef _ENVIRONMENT_H_
#define _ENVIRONMENT_H_

#include <ultra64.h>
#include <bondtypes.h>


typedef struct NearFogSettings {
    f32 NearFog;
    f32 MaxVisRange;
    f32 MaxObfuscationRange;
} NearFogSettings;

typedef struct SkySettings
{
    u8 Red;
    u8 Green;
    u8 Blue;
    u8 Clouds;
    f32 SkyHeight;
    s16 SkyImageId;
    u16 Reserved;
    f32 CloudRed;
    f32 CloudGreen;
    f32 CloudBlue;
    u8 IsWater;
    u8 Padding[3];
    f32 WaterHeight;
    s16 WaterImageId;
    u16 Reserved2;
    f32 WaterRed;
    f32 WaterGreen;
    f32 WaterBlue;
    f32 HorizonYOffset; // Screen Y-offset for where cloud and water planes meet.
} SkySettings;

typedef struct EnvironmentVisibilitySettings
{
    f32 NearClipDistance;
    f32 FarClipDistance;
    NearFogSettings NearFog;
    f32 MinVisRange;
    u32 Intensity;
    s32 FogStart;
    s32 FogEnd;
} EnvironmentVisibilitySettings;

typedef struct PropVisibilitySettings
{
    f32 FadeStartPx;
    f32 FadeEndPx;
} PropVisibilitySettings;

typedef struct EnvironmentRecord
{
    /**
     * ID = StageID + Token eg, Bunker Cinema is 9 + 900 = 909
     */
    u32 Id;
    bool FogEnabled;
    EnvironmentVisibilitySettings Visibility;
    SkySettings Sky;

    /**
     * GUD: New per-level screen-size prop fade override, applied to characters
     * and objects alike. See chrCalcScreenFadeAlpha and objCalcScreenFadeAlpha.
     * 0 = engine defaults. Negative FadeStartPx disables fading on this level.
     * When overriding, FadeEndPx must be less than FadeStartPx.
     * Rows that end before these fields zero-fill, so untouched levels keep
     * the defaults.
     */
    PropVisibilitySettings PropVisibility;
} EnvironmentRecord;

extern f32 g_PropFadeStartPx;
extern f32 g_PropFadeEndPx;

EnvironmentRecord *envGetCurrent(void);
f32 envGetScaledFarFogIntensitySquared(void);
void envLoadLevelEnvironment(s32 level_id, s32 arg1);
s32 envPositionIsVisibleThroughFog(coord3d *pos, f32 range);
Gfx *envSetRenderFogColor(Gfx *gdl);
Gfx *envRenderClearFogMode(Gfx *gdl);
s32 envGetPropDistColor(PropRecord *prop, struct rgba_f32 *color);
void envSwitchToSoloSky2(f32 transitionTime);
NearFogSettings *envGetNearFogValues(void);

#endif
