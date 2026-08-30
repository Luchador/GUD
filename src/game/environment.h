#ifndef _ENVIRONMENT_H_
#define _ENVIRONMENT_H_

#include <ultra64.h>
#include <bondtypes.h>


typedef struct NearFogRecord {
    f32 NearFog;
    f32 MaxVisRange;
    f32 MaxObfuscationRange;
} NearFogRecord;

// Skybox and Water Plane
typedef struct SkyBoxRecord
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
    f32 WaterRepeat;
    s16 WaterImageId;
    u16 Reserved2;
    f32 WaterRed;
    f32 WaterGreen;
    f32 WaterBlue;
    f32 HorizonYOffset; // Screen Y-offset for where cloud and water planes meet.
} SkyBoxRecord;

// Fog intensity
typedef struct FogRecord
{
    s32 FogStart;
    s32 FogEnd;
} FogRecord;

// Visibility distances and Z-Buffer accuriacy
typedef struct VisibilityRecord
{
    f32 NearClipDistance;
    f32 FarClipDistance;
    NearFogRecord Nfd;
    f32 MinVisrange;
    u32 Intensity;
} VisibilityRecord;

//Main Records

// Current Environment for rendering
typedef struct CurrentEnvironmentRecord
{
    s32 FogStart;
    s32 FogEnd;
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
    f32 WaterRepeat;
    s16 WaterImageId;
    u16 Reserved2;
    f32 WaterRed;
    f32 WaterGreen;
    f32 WaterBlue;
    f32 HorizonYOffset;
} CurrentEnvironmentRecord;

// Environment Record, Holds Visibility, Fog and Skybox
typedef struct EnvironmentRecord
{
    /**
     * ID = StageID + Token eg, Bunker Cinema is 9 + 900 = 909
    */
    u32 Id;
    VisibilityRecord Visibility;
    FogRecord Fog;
    SkyBoxRecord Sky;

    /**
     * GUD: New per-level screen-size prop fade override, applied to characters
     * and objects alike. See chrCalcScreenFadeAlpha and objCalcScreenFadeAlpha.
     * 0 = engine defaults. Negative PropFadeStartPx = fade disabled on this
     * level. When overriding, PropFadeEndPx must be less than PropFadeStartPx.
     * Rows that end before these fields zero-fill, so untouched levels keep
     * the defaults.
     */
    f32 PropFadeStartPx;
    f32 PropFadeEndPx;
} EnvironmentRecord;

// Environment Record, Holds only Skybox
typedef struct EnvironmentFoglessRecord
{
    /**
     * ID = StageID + Token eg, Bunker Cinema is 9 + 900 = 909
    */
    u32 Id;
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
    u8 padding[3]; // This always exists whether declared or not due to compiler byte alignment.
    f32 WaterRepeat;
    s16 WaterImageId;
    u16 Reserved2;
    f32 WaterRed;
    f32 WaterGreen;
    f32 WaterBlue;
    f32 HorizonYOffset;

    /** GUD: same per-level prop fade override as EnvironmentRecord. */
    f32 PropFadeStartPx;
    f32 PropFadeEndPx;
} EnvironmentFoglessRecord;

extern s32 g_FogSkyIsEnabled;
extern f32 g_PropFadeStartPx;
extern f32 g_PropFadeEndPx;

struct CurrentEnvironmentRecord *envGetCurrent(void);
f32 envGetScaledFarFogIntensitySquared(void);
void envLoadLevelEnvironment(s32 level_id, s32 arg1);
s32 envPositionIsVisibleThroughFog(coord3d *pos, f32 range);
Gfx *envSetRenderFogColor(Gfx *gdl);
Gfx *envRenderClearFogMode(Gfx *gdl);
s32 envGetPropDistColor(PropRecord *prop, struct rgba_f32 *color);
void envSwitchToSoloSky2(f32 transitionTime);
struct NearFogRecord *envGetNearFogValues(void);

#endif
