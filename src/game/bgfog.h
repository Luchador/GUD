#ifndef _FOG_H_
#define _FOG_H_

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
    f32 CloudRepeat;
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
    f32 WaterConcavity;
} SkyBoxRecord;

// Fog intensity
typedef struct FogRecord
{
    /**
     * Inverse NearFog
    */
    s32 DifferenceFromFarIntensity;
    s32 FarIntensity;
} FogRecord;

// Visibility distances and Z-Buffer accuriacy
typedef struct VisibilityRecord
{
    f32 BlendMultiplier;
    f32 FarFog;
    NearFogRecord Nfd;
    f32 MinVisrange;
    u32 Intensity;
} VisibilityRecord;

//Main Records

// Current Environment for rendering
typedef struct CurrentEnvironmentRecord
{
    s32 DifferenceFromFarIntensity;
    s32 FarIntensity;
    u8 Red;
    u8 Green;
    u8 Blue;
    u8 Clouds;
    f32 CloudRepeat; // canonically skyheight
    s16 SkyImageId;
    u16 Reserved;
    f32 CloudRed;
    f32 CloudGreen;
    f32 CloudBlue;
    u8 IsWater;
    u8 Padding[3];
    f32 WaterRepeat; // canonically seaheight
    s16 WaterImageId;
    u16 Reserved2;
    f32 WaterRed;
    f32 WaterGreen;
    f32 WaterBlue;
    f32 WaterConcavity;
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
    f32 CloudRepeat;
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
    f32 WaterConcavity;
} EnvironmentFoglessRecord;

extern s32 g_FogSkyIsEnabled;

struct CurrentEnvironmentRecord *fogGetCurrentEnvironmentp(void);
f32 fogGetScaledFarFogIntensitySquared(void);
void fogLoadLevelEnvironment(s32 level_id, s32 arg1);
s32 fogPositionIsVisibleThroughFog(coord3d *pos, f32 range);
Gfx *fogSetRenderFogColor(Gfx *gdl);
Gfx *fogRenderClearFogMode(Gfx *gdl);
s32 fogGetPropDistColor(PropRecord *prop, struct rgba_f32 *color);
void fogSwitchToSolosky2(f32 arg0);
struct NearFogRecord *fogGetNearFogValuesP(void);

#endif
