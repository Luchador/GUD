#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <limits.h>
#include <fr.h>
#include "bg.h"
#include "bondview.h"
#include "environment.h"


// Fade optimization definitions.
#define ENVFADE_START_PX 12.5f
#define ENVFADE_END_PX   10.0f
#define ENVFADE_DIAMETER 200.0f

s32 g_FogSkyIsEnabled;
NearFogRecord *g_NearFogValuesP;

struct FogDetails
{
    f32 g_CurFogDetails;
    f32 scaled_far_fog_dist;
    u32 unk08;
    u32 unk0c;
    f32 far_fog_dist_scaled;
    f32 near_fog_dist_scaled;
} g_CurFogDetails;

f32 g_FarFogIntensity;
f32 g_DifferenceFromFarFogIntensity;
EnvironmentRecord *g_EnvironmentFoundp;
EnvironmentRecord * g_EnvironmentMainp;
EnvironmentRecord * g_EnvironmentAltp;

f32 g_ScaledFarFogIntensity = FLT_MAX;
f32 g_ScaledDifferenceFromFarFogIntensity = 0.0;

CurrentEnvironmentRecord g_CurrentEnvironment = {
    0x384, // s32 DifferenceFromFarIntensity;
    0x3e8, // u32 FarIntensity;
        0,     // u8  Red;
        0,     // u8  Green;
        0,     // u8  Blue;
        0,     // u8  Clouds;
        0.0f,  // f32 CloudRepeat;
        0,     // s16 SkyImageId;
        0,
        0.0f,  // f32 CloudRed;
        0.0f,  // f32 CloudGreen;
        0.0f,  // f32 CloudBlue;
        0,     // u8  IsWater;
        0,0,0,
        0.0f,  // f32 WaterRepeat;
        0,     // s16 WaterImageId;
        0,
        0.0f,  // f32 WaterRed;
        0.0f,  // f32 WaterGreen;
        0.0f,  // f32 WaterBlue;
        0.0f  // f32 WaterConcavity;
};

EnvironmentRecord g_EnvTable[] = {
     //stageID                              blendmultiplier    farfog    nearfog  mvisrng  mobfnrng  mnvisrng   intensity  dif_ght  far_alight    red     green     blue    clouds   cloudrept skyimid reserved  cloudred   green    blue   iswater  padding[3]  waterrepeat  waterid reserved2   water red,green,blue  waterconcavity    propStartFade       propEndFade
    {LEVELID_STATUE                             ,        15,      3500,    2000,     2500,    2000,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      5000,    0,        0,        170,    100,     40,        0,    0,0,0,         -1000,       0,        0,        0,     0,    0,    30.0 ,          20.0f,              15.0f},
    {LEVELID_CONTROL                            ,        10,     10000,    2500,     5000,     800,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 ,          25.0f,              21.0f},
    {LEVELID_ARCHIVES                           ,        10,      3000,    2000,     3000,     500,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,     0,    0,     0.0 ,          26.0f,              23.0f},
    {LEVELID_TRAIN                              ,        10,      1500,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       1,        0,        0,   255,  255,    25.0 },
    {LEVELID_TRAIN + ENVIRONMENTDATA_ALT        ,        20,     15000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       1,        0,        0,   255,  255,    25.0 },
    {LEVELID_STREETS                            ,        10,      7500,    5000,     6000,    1000,        0,      0x3E7,    0x3E4,    0x3E8,    0x10,    0x18,    0x20,        1,      5000,    0,        0,        225,    175,    100,        0,    0,0,0,         -1000,       0,        0,        0,     0,    0,    25.0 ,          15.0f,              12.0f},
    {LEVELID_DEPOT                              ,        10,      3000,    1600,     2000,     800,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      7500,    0,        0,         70,    199,    186,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,    25.0 ,          22.0f,              19.0f},
    {LEVELID_COMPLEX                            ,        10,      5000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_DAM                                ,         5,     15000,    3333,     4444,     600,        0,      0x3E7,    0x3E3,    0x3E8,    0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,     0,    0,     0.0 ,          13.0f,              11.0f},
    {LEVELID_DAM + ENVIRONMENTDATA_CINEMA       ,        30,     15000,    3333,     4444,     600,        0,      0x3E7,    0x3E3,    0x3E8,    0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_FACILITY                           ,        10,      5000,       0,        0,       0,        0,      0x3E7,    0x3DE,    0x3E8,    0x10,    0x20,    0x10,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_FACILITY + ENVIRONMENTDATA_ALT     ,        10,      1000,       0,        0,       0,        0,      0x3E7,    0x3DE,    0x3E8,    0x40,    0x80,    0x40,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_RUNWAY                             ,        10,     15000,    6000,     8000,     800,        0,      0x3E7,    0x3E4,    0x3E8,    0x10,    0x30,    0x40,        1,      5000,    0,        0,         25,     25,     25,        0,    0,0,0,         -1000,       0,        0,        0,     0,    0,     0.0 ,          11.0f,               9.0f},
    {LEVELID_SURFACE                            ,         2,      2500,    4444,     5555,     800,        0,      0x3E7,    0x3E4,    0x3E8,    0x60,    0x60,    0x80,        1,     10000,    0,        0,        240,    120,    30,         0,    0,0,0,             0,       1,        0,        0,   255,  255,     7.0 ,          10.0f,               8.0f},
    {LEVELID_JUNGLE                             ,        10,      2500,    1500,     2500,    1000,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x20,       0,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_TEMPLE                             ,        10,      6000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x28,        1,     10000,    0,        0,        160,    160,    190,        0,    0,0,0,          -500,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_CAVERNS                            ,        10,      6000,       0,        0,       0,        0,      0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CRADLE                             ,        10,      9500,    3333,     4444,     800,        0,      0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_SURFACE2                           ,         2,      2000,    2500,     3055,     750,        0,      0x3E7,    0x3BD,    0x3E8,    0x20,    0x10,    0x10,        1,      5000,    0,        0,         58,     17,      0,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,    20.0 },
    {LEVELID_SURFACE2 + ENVIRONMENTDATA_CINEMA  ,         2,      8000,    6000,     8000,     800,        0,      0x3E7,    0x3DF,    0x3E8,    0x20,    0x10,    0x10,        1,      5000,    0,        0,         58,     17,      0,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_BUNKER2                            ,        10,     10000,    1000,    15000,     750,        0,      0x3E7,    0x3E4,    0x41A,    0x10,       0,       0,        1,      5000,    0,        0,         58,     17,      0,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_2,        10,      2500,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_3,        10,      2000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_4,        10,      1500,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_2 ,        10,      7000,    3333,     4444,     800,        0,      0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_3 ,        10,      5500,    3333,     4444,     800,        0,      0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_4 ,        10,      4000,    3333,     4444,     800,        0,      0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_2 ,        10,      6000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    0,0,0,          -500,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_3 ,        10,      6000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    0,0,0,          -500,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_4 ,        10,      6000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    0,0,0,          -500,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_2,        10,      7500,       0,        0,       0,        0,      0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_3,        10,      6000,       0,        0,       0,        0,      0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_4,        10,      5000,       0,        0,       0,        0,      0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_2,        10,      7500,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_3,        10,      6000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_4,        10,      5000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_CITADEL                            ,        10,     20000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_2,        10,     20000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_3,        10,     20000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_4,        10,     20000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,   255,  255,     0.0 },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_2,        10,      5000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_3,        10,      5000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_4,        10,      5000,       0,        0,       0,        0,      0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_AZTEC                              ,        10,     15000,    3000,     4000,     600,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        1,      7500,    0,        0,         83,     72,     65,        0,    0,0,0,         -5000,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_AZTEC + ENVIRONMENTDATA_ALT        ,        30,     15000,    3000,     4000,     600,        0,      0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        1,      7500,    0,        0,         83,     72,     65,        0,    0,0,0,         -5000,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_EGYPT                              ,        10,     20000,    3000,     4000,     600,        0,      0x3E7,    0x3E4,    0x3E8,    0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -5000,       0,        0,        0,     0,    0,     0.0 },
    {LEVELID_EGYPT + ENVIRONMENTDATA_ALT        ,        10,     10000,    3000,     4000,     600,        0,      0x3E7,    0x3CC,    0x3E8,       0,       0,       0,        1,      7500,    0,        0,         83,     10,     10,        0,    0,0,0,         -5000,       0,        0,        0,     0,    0,     0.0 },
    {ENVIRONMENTDATA_END},
};

EnvironmentFoglessRecord g_EnvTable2[] = {
    // stage ID          Red      Green    Blue     Clouds    Cloudrepeat    SkyImgID  Res.      CloudRed      CloudGreen    CloudBlue     IsWater                                 WaterRepeat    WImgID    Res2.    WRed          WGreen        WBlue         WaterConcavity
    {LEVELID_NONE   ,    0,       0x10,    0x40,    0,        5000.0,        0,        0,        255.0,        255.0,        255.0,        0,        0,        0,        0,           0.0,        0,        0,          0.0,          0.0,          0.0,        0.0},
    {LEVELID_SILO   ,    0,       0x10,    0x60,    1,        5000.0,        0,        0,        231.0,        216.0,        237.0,        0,        0,        0,        0,           0.0,        0,        0,          0.0,          0.0,          0.0,        0.0},
    {LEVELID_FRIGATE,    0x10,    0x30,    0x60,    1,        3000.0,        0,        0,        230.0,        230.0,        230.0,        1,        0,        0,        0,        -150.0,        2,        0,        255.0,        255.0,        150.0,        0.0},
    {LEVELID_CUBA   ,    0x30,    0x40,    0x10,    0,        5000.0,        0,        0,        255.0,        255.0,        255.0,        0,        0,        0,        0,           0.0,        0,        0,          0.0,          0.0,          0.0,        0.0},
    {ENVIRONMENTDATA_END}
};

// Begin forward declarations.

void envLoadCurrentEnvironment(EnvironmentRecord *arg0);
void envLoadFoglessCurrentEnvironment(EnvironmentFoglessRecord *arg0);

// End forward declarations.


CurrentEnvironmentRecord *envGetCurrent(void)
{
    return &g_CurrentEnvironment;
}


f32 envGetScaledFarFogIntensitySquared(void)
{
    return g_ScaledFarFogIntensity * g_ScaledFarFogIntensity;
}


/** GUD: per-level screen-size prop fade: 0/0 = engine defaults, negative
  * start = disabled. Cached here at environment load, and on sky/alt fog transitions.
  */
f32 g_PropFadeStartPx = 0.0f;
f32 g_PropFadeEndPx = 0.0f;


void envLoadCurrentEnvironment(EnvironmentRecord *arg0)
{
    f32 zrange[2];
    f32 pk0;
    f32 pk4;
    f32 levelScale;
    f32 sp20;
    f32 sp1C;

    g_PropFadeStartPx = arg0->PropFadeStartPx;
    g_PropFadeEndPx = arg0->PropFadeEndPx;

    viSetZRange(arg0->Visibility.BlendMultiplier, arg0->Visibility.FarFog);
    viGetZRange(&zrange);

    levelScale = bgGetLevelVisibilityScale();
    zrange[0] /= levelScale;
    zrange[1] /= levelScale;

    g_DifferenceFromFarFogIntensity = ((f32) arg0->Fog.DifferenceFromFarIntensity / 1000.0f);
    g_FarFogIntensity = ((f32) arg0->Fog.FarIntensity / 1000.0f);

    g_ScaledFarFogIntensity = ((zrange[1] - zrange[0]) *  g_FarFogIntensity) + zrange[0];
    g_ScaledDifferenceFromFarFogIntensity = ((zrange[1] - zrange[0]) * g_DifferenceFromFarFogIntensity) + zrange[0];

    g_CurFogDetails.g_CurFogDetails = (arg0->Visibility.BlendMultiplier / levelScale);
    pk0 = g_CurFogDetails.g_CurFogDetails;
    g_CurFogDetails.scaled_far_fog_dist = (arg0->Visibility.FarFog / levelScale);
    pk4 = g_CurFogDetails.scaled_far_fog_dist;

    // numerator is constant 128.0f
    sp20 = (256.0f * (0.5f -         0.0f)) / (g_FarFogIntensity - g_DifferenceFromFarFogIntensity);
    sp1C = (256.0f * (0.5f - g_DifferenceFromFarFogIntensity)) / (g_FarFogIntensity - g_DifferenceFromFarFogIntensity);

    g_CurFogDetails.far_fog_dist_scaled = ((pk4 * -sp20 * (pk0 + 1.0f)) / (pk4 - pk0)) / 255.0f;
    g_CurFogDetails.near_fog_dist_scaled = ((sp20 * (pk4 + 1.0f) / (pk4 - pk0)) + sp1C) / 255.0f;

    g_CurrentEnvironment.DifferenceFromFarIntensity = (s32) arg0->Fog.DifferenceFromFarIntensity;
    g_CurrentEnvironment.FarIntensity = arg0->Fog.FarIntensity;
    g_CurrentEnvironment.Red = arg0->Sky.Red;
    g_CurrentEnvironment.Green = arg0->Sky.Green;
    g_CurrentEnvironment.Blue = arg0->Sky.Blue;
    g_CurrentEnvironment.Clouds = arg0->Sky.Clouds;
    g_CurrentEnvironment.CloudRepeat = arg0->Sky.CloudRepeat;
    g_CurrentEnvironment.SkyImageId = (s16) arg0->Sky.SkyImageId;
    g_CurrentEnvironment.CloudRed = arg0->Sky.CloudRed;
    g_CurrentEnvironment.CloudGreen = arg0->Sky.CloudGreen;
    g_CurrentEnvironment.CloudBlue = arg0->Sky.CloudBlue;
    g_CurrentEnvironment.IsWater = arg0->Sky.IsWater;
    g_CurrentEnvironment.WaterRepeat = arg0->Sky.WaterRepeat;
    g_CurrentEnvironment.WaterImageId = (s16) arg0->Sky.WaterImageId;
    g_CurrentEnvironment.WaterRed = arg0->Sky.WaterRed;
    g_CurrentEnvironment.WaterGreen = arg0->Sky.WaterGreen;
    g_CurrentEnvironment.WaterBlue = arg0->Sky.WaterBlue;
    g_CurrentEnvironment.WaterConcavity = arg0->Sky.WaterConcavity;

    if (arg0->Visibility.Nfd.NearFog == 0.0f)
    {
        g_NearFogValuesP = NULL;
    }
    else
    {
        g_NearFogValuesP = &arg0->Visibility.Nfd;
    }

    g_FogSkyIsEnabled = 1;
}


void envLoadFoglessCurrentEnvironment(EnvironmentFoglessRecord *foglessRecord)
{
    g_PropFadeStartPx = foglessRecord->PropFadeStartPx;
    g_PropFadeEndPx = foglessRecord->PropFadeEndPx;

    g_CurrentEnvironment.Red = foglessRecord->Red;
    g_CurrentEnvironment.Green = foglessRecord->Green;
    g_CurrentEnvironment.Blue = foglessRecord->Blue;
    g_CurrentEnvironment.Clouds = foglessRecord->Clouds;
    g_CurrentEnvironment.CloudRepeat = foglessRecord->CloudRepeat;
    g_CurrentEnvironment.SkyImageId = foglessRecord->SkyImageId;
    g_CurrentEnvironment.CloudRed = foglessRecord->CloudRed;
    g_CurrentEnvironment.CloudGreen = foglessRecord->CloudGreen;
    g_CurrentEnvironment.CloudBlue = foglessRecord->CloudBlue;
    g_CurrentEnvironment.IsWater = foglessRecord->IsWater;
    g_CurrentEnvironment.WaterRepeat = foglessRecord->WaterRepeat;
    g_CurrentEnvironment.WaterImageId = foglessRecord->WaterImageId;
    g_CurrentEnvironment.WaterRed = foglessRecord->WaterRed;
    g_CurrentEnvironment.WaterGreen = foglessRecord->WaterGreen;
    g_CurrentEnvironment.WaterBlue = foglessRecord->WaterBlue;
    g_CurrentEnvironment.WaterConcavity = foglessRecord->WaterConcavity;
}


void envLoadLevelEnvironment(s32 level_id, s32 arg1)
{
    EnvironmentRecord *phi_v1;
    EnvironmentFoglessRecord *phi_v2;
    EnvironmentFoglessRecord *sp1C;
    s32 num_players;

    sp1C = NULL;

    num_players = getPlayerCount();

    if (num_players == 1)
    {
        num_players = 0;
    }

    g_ScaledFarFogIntensity = FLT_MAX;
    g_ScaledDifferenceFromFarFogIntensity = 0.0f;

    if (arg1)
    {
        for (phi_v1 = &g_EnvTable[0]; phi_v1->Id != 0; phi_v1++)
        {
            if (phi_v1->Id == (level_id + 900))
            {
                g_EnvironmentFoundp = phi_v1;
                g_EnvironmentMainp = phi_v1;
                g_EnvironmentAltp = phi_v1 + 1;

                envLoadCurrentEnvironment(g_EnvironmentFoundp);

                return;
            }
        }
    }

    for (phi_v1 = &g_EnvTable[0]; phi_v1->Id != 0; phi_v1++)
    {
        if (phi_v1->Id == (level_id + (num_players * 100)))
        {
            g_EnvironmentFoundp = phi_v1;
            g_EnvironmentMainp = phi_v1;
            g_EnvironmentAltp = phi_v1 + 1;

            envLoadCurrentEnvironment(g_EnvironmentFoundp);

            return;
        }
    }

    if (num_players >= 2)
    {
        for (phi_v1 = &g_EnvTable[0]; phi_v1->Id != 0; phi_v1++)
        {
            if (phi_v1->Id == (num_players * 100))
            {
                g_EnvironmentFoundp = phi_v1;
                g_EnvironmentMainp = phi_v1;
                g_EnvironmentAltp = phi_v1 + 1;

                envLoadCurrentEnvironment(g_EnvironmentFoundp);

                return;
            }
        }
    }

    viSetZRange(15.0f, 10000.0f);
    g_FogSkyIsEnabled = 0;

    for (phi_v2 = g_EnvTable2; phi_v2->Id != 0; phi_v2++)
    {
        if (phi_v2->Id == level_id)
        {
            sp1C = phi_v2;
        }
    }

    if (sp1C == NULL)
    {
        sp1C = &g_EnvTable2[0];
    }

    envLoadFoglessCurrentEnvironment(sp1C);
    g_EnvironmentFoundp = NULL;
}


/**
 * Switch to next Environment.
 * @param transitionTime: Usually 0 for instant switch or 1 to transition gradually
 */
void envSwitchToSoloSky2(f32 transitionTime)
{
    static EnvironmentRecord static_envr;

    static_envr = *g_EnvironmentMainp;

    static_envr.Visibility.BlendMultiplier =
        g_EnvironmentMainp->Visibility.BlendMultiplier + (transitionTime * ((f32)g_EnvironmentAltp->Visibility.BlendMultiplier - (f32)g_EnvironmentMainp->Visibility.BlendMultiplier));

    static_envr.Visibility.FarFog =
        g_EnvironmentMainp->Visibility.FarFog + (transitionTime * ((f32)g_EnvironmentAltp->Visibility.FarFog - (f32)g_EnvironmentMainp->Visibility.FarFog));

    static_envr.Fog.DifferenceFromFarIntensity =
        (f32)g_EnvironmentMainp->Fog.DifferenceFromFarIntensity
        + (transitionTime * ((f32)g_EnvironmentAltp->Fog.DifferenceFromFarIntensity - (f32)g_EnvironmentMainp->Fog.DifferenceFromFarIntensity));

    static_envr.Fog.FarIntensity =
        (f32)g_EnvironmentMainp->Fog.FarIntensity
        + (transitionTime * ((f32)g_EnvironmentAltp->Fog.FarIntensity - (f32)g_EnvironmentMainp->Fog.FarIntensity));

    static_envr.Sky.Red =
        (f32)g_EnvironmentMainp->Sky.Red
        + (transitionTime * ((f32)g_EnvironmentAltp->Sky.Red - (f32)g_EnvironmentMainp->Sky.Red));

    static_envr.Sky.Green =
        (f32)g_EnvironmentMainp->Sky.Green
        + (transitionTime * ((f32)g_EnvironmentAltp->Sky.Green - (f32)g_EnvironmentMainp->Sky.Green));

    static_envr.Sky.Blue =
        (f32)g_EnvironmentMainp->Sky.Blue
        + (transitionTime * ((f32)g_EnvironmentAltp->Sky.Blue - (f32)g_EnvironmentMainp->Sky.Blue));

    static_envr.Sky.Red &= 0xf8;
    static_envr.Sky.Green &= 0xf8;
    static_envr.Sky.Blue &= 0xf8;

    envLoadCurrentEnvironment(&static_envr);
}


Gfx *envSetRenderFogColor(Gfx *gdl)
{
    if (!g_FogSkyIsEnabled)
    {
        return gdl;
    }

    gDPSetFogColor(gdl++, g_CurrentEnvironment.Red, g_CurrentEnvironment.Green, g_CurrentEnvironment.Blue, 0xff);
    gSPFogPosition(gdl++, g_CurrentEnvironment.DifferenceFromFarIntensity, g_CurrentEnvironment.FarIntensity);
    gSPSetGeometryMode(gdl++, G_FOG);
    gDPSetAlphaDither(gdl++, G_AD_NOISE);

    return gdl;
}


Gfx *envRenderClearFogMode(Gfx *gdl)
{
    if (!g_FogSkyIsEnabled)
    {
        return gdl;
    }

    gSPClearGeometryMode(gdl++, G_FOG);

    return gdl;
}


s32 envPositionIsVisibleThroughFog(coord3d *pos, f32 range)
{
    coord3d sp24;
    f32 ff;
    coord3d *player_pos;
    Mtxf *player_mtx;

    if (!g_FogSkyIsEnabled)
    {
        return TRUE;
    }

    player_pos = bondviewGetPlayerPosition();
    player_mtx = camGetWorldToScreenMtxf();

    sp24.f[0] = pos->f[0] - player_pos->f[0];
    sp24.f[1] = pos->f[1] - player_pos->f[1];
    sp24.f[2] = pos->f[2] - player_pos->f[2];

    ff = (((sp24.f[0] * player_mtx->m[0][0]) + (sp24.f[1] * player_mtx->m[0][1]) + (sp24.f[2] * player_mtx->m[0][2])));

    if (ff > (g_ScaledFarFogIntensity + range))
    {
        return FALSE;
    }

    return TRUE;
}


NearFogRecord *envGetNearFogValues(void)
{
    return g_NearFogValuesP;
}


s32 envGetPropDistColor(PropRecord *prop, rgba_f32 *color)
{
    if (!g_FogSkyIsEnabled)
    {
        return 2; // No fog, props cannot be obscured by fog
    }

    if (prop->zDepth < 0.0f)
    {
        return 2; // Prop is behind the camera
    }

    color->rgba[0] = (f32) g_CurrentEnvironment.Red / 255.0f;
    color->rgba[1] = (f32) g_CurrentEnvironment.Green / 255.0f;
    color->rgba[2] = (f32) g_CurrentEnvironment.Blue / 255.0f;
    color->rgba[3] = (g_CurFogDetails.far_fog_dist_scaled / prop->zDepth) + g_CurFogDetails.near_fog_dist_scaled;

    if (color->rgba[3] < 0.0f)
    {
        return 2; // Prop has no fog effect coloring applied to it
    }

    if (color->rgba[3] > 1.0f)
    {
        return 0; // Prop is completely obscured by fog (don't render)
    }

    return 1; // Fog color will be applied to prop
}
