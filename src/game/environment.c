#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <limits.h>
#include <fr.h>
#include "bg.h"
#include "bondview.h"
#include "environment.h"


/** 
 * GUD: per-level screen-size prop fade: 0/0 = engine defaults, negative
 * start = disabled. Cached here at environment load, and on sky/alt fog transitions.
 */
f32 g_PropFadeStartPx = 0.0f;
f32 g_PropFadeEndPx = 0.0f;

static NearFogSettings *g_NearFogValuesP;

static struct FogDetails
{
    f32 g_CurFogDetails;
    f32 scaled_far_fog_dist;
    u32 unk08;
    u32 unk0c;
    f32 far_fog_dist_scaled;
    f32 near_fog_dist_scaled;
} g_CurFogDetails;

static f32 g_FarFogIntensity;
static f32 g_DifferenceFromFarFogIntensity;
static EnvironmentRecord *g_MainEnvironment;
static EnvironmentRecord *g_AlternateEnvironment;

static f32 g_ScaledFarFogIntensity = FLT_MAX;
static f32 g_ScaledDifferenceFromFarFogIntensity = 0.0;

static EnvironmentRecord g_CurrentEnvironment = {
    ENVIRONMENTDATA_END,
    TRUE,
    {
        15.0f,
        10000.0f,
        {0.0f, 0.0f, 0.0f},
        0.0f,
        0,
        0x384,
        0x3e8,
    },
    {0},
    {0.0f, 0.0f},
};

/**
 * Fogged and fogless stages use the same record shape. Fogless rows carry the
 * standard 15/10000 clipping range and skip only the fog calculations.
 */
static EnvironmentRecord g_EnvTable[] = {
     //stageID                                           fogEnabled  nearclip  farclip   nearfog  maxvisrng  maxobfnrng  minvisrng  intensity  fogstart  fogend     red     green     blue    clouds   skyheight skyimid reserved  cloudred   green    blue   iswater  padding[3]  waterheight  waterid reserved2 water red,    green,   blue    horizonyoffset    propStartFade       propEndFade
    {LEVELID_STATUE                             ,        TRUE,        15,      3500,     2000,     2500,    2000,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      5000,    0,        0,        170,    100,     40,        0,    0,0,0,         -1000,       0,        0,        0,        0,      0,     30.0 ,            20.0f,              15.0f},
    {LEVELID_CONTROL                            ,        TRUE,        10,     10000,     2500,     5000,     800,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 ,            16.0f,              13.0f},
    {LEVELID_ARCHIVES                           ,        TRUE,        10,      3000,     2000,     3000,     500,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,        0,      0,      0.0 ,            26.0f,              23.0f},
    {LEVELID_TRAIN                              ,        TRUE,        10,      1500,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       1,        0,        0,      255,    255,     25.0 },
    {LEVELID_TRAIN + ENVIRONMENTDATA_ALT        ,        TRUE,        20,     15000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       1,        0,        0,      255,    255,     25.0 },
    {LEVELID_STREETS                            ,        TRUE,        10,      7500,     5000,     6000,    1000,        0,         0x3E7,    0x3E4,    0x3E8,    0x10,    0x18,    0x20,        1,      5000,    0,        0,        225,    175,    100,        0,    0,0,0,         -1000,       0,        0,        0,        0,      0,     25.0 ,            15.0f,              12.0f},
    {LEVELID_DEPOT                              ,        TRUE,        10,      3000,     1600,     2000,     800,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       8,        1,      7500,    0,        0,         70,    199,    186,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,     25.0 ,            22.0f,              19.0f},
    {LEVELID_COMPLEX                            ,        TRUE,        10,      5000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_DAM                                ,        TRUE,         5,     15000,     3333,     4444,     600,        0,         0x3E7,    0x3E3,    0x3E8,    0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_DAM + ENVIRONMENTDATA_CINEMA       ,        TRUE,        30,     15000,     3333,     4444,     600,        0,         0x3E7,    0x3E3,    0x3E8,    0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_FACILITY                           ,        TRUE,        10,      5000,        0,        0,       0,        0,         0x3E7,    0x3DE,    0x3E8,    0x10,    0x20,    0x10,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -1000,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_FACILITY + ENVIRONMENTDATA_ALT     ,        TRUE,        10,      1000,        0,        0,       0,        0,         0x3E7,    0x3DE,    0x3E8,    0x40,    0x80,    0x40,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_RUNWAY                             ,        TRUE,        10,     15000,     6000,     8000,     800,        0,         0x3E7,    0x3E4,    0x3E8,    0x10,    0x30,    0x40,        1,      5000,    0,        0,         25,     25,     25,        0,    0,0,0,         -1000,       0,        0,        0,        0,      0,      0.0 ,            12.0f,               10.0f},
    {LEVELID_SURFACE                            ,        TRUE,         2,      2000,     4444,     5555,     800,        0,         0x3E7,    0x3E4,    0x3E8,    0x60,    0x60,    0x80,        1,     10000,    0,        0,        240,    120,    30,         0,    0,0,0,             0,       1,        0,        0,      255,    255,      7.0 ,            9.0f,                7.0f},
    {LEVELID_JUNGLE                             ,        TRUE,        10,      2500,     1500,     2500,    1000,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x20,       0,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 ,            25.0f,               23.0f},
    {LEVELID_TEMPLE                             ,        TRUE,        10,      6000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x28,        1,     10000,    0,        0,        160,    160,    190,        0,    0,0,0,          -500,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_CAVERNS                            ,        TRUE,        10,      6000,        0,        0,       0,        0,         0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CRADLE                             ,        TRUE,        10,      9500,     3333,     4444,     800,        0,         0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_SURFACE2                           ,        TRUE,         2,      2000,     2500,     3055,     750,        0,         0x3E7,    0x3BD,    0x3E8,    0x20,    0x10,    0x10,        1,      5000,    0,        0,         58,     17,      0,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,     20.0 },
    {LEVELID_SURFACE2 + ENVIRONMENTDATA_CINEMA  ,        TRUE,         2,      8000,     6000,     8000,     800,        0,         0x3E7,    0x3DF,    0x3E8,    0x20,    0x10,    0x10,        1,      5000,    0,        0,         58,     17,      0,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_BUNKER2                            ,        TRUE,        10,     10000,     1000,    15000,     750,        0,         0x3E7,    0x3E4,    0x41A,    0x10,       0,       0,        1,      5000,    0,        0,         58,     17,      0,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_2,        TRUE,        10,      2500,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_3,        TRUE,        10,      2000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_4,        TRUE,        10,      1500,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_2 ,        TRUE,        10,      7000,     3333,     4444,     800,        0,         0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_3 ,        TRUE,        10,      5500,     3333,     4444,     800,        0,         0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_4 ,        TRUE,        10,      4000,     3333,     4444,     800,        0,         0x3E7,    0x3E4,    0x3E8,    0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    0,0,0,        -10000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_2 ,        TRUE,        10,      6000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    0,0,0,          -500,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_3 ,        TRUE,        10,      6000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    0,0,0,          -500,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_4 ,        TRUE,        10,      6000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    0,0,0,          -500,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_2,        TRUE,        10,      7500,        0,        0,       0,        0,         0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_3,        TRUE,        10,      6000,        0,        0,       0,        0,         0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_4,        TRUE,        10,      5000,        0,        0,       0,        0,         0x3E7,    0x3E1,    0x3E8,       8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -3000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_2,        TRUE,        10,      7500,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_3,        TRUE,        10,      6000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_4,        TRUE,        10,      5000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_CITADEL                            ,        TRUE,        10,     20000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_2,        TRUE,        10,     20000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_3,        TRUE,        10,     20000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_4,        TRUE,        10,     20000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    0,0,0,         -1000,       1,        0,        0,      255,    255,      0.0 },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_2,        TRUE,        10,      5000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_3,        TRUE,        10,      5000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_4,        TRUE,        10,      5000,        0,        0,       0,        0,         0x3E7,    0x3E4,    0x3E8,    0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    0,0,0,             0,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_AZTEC                              ,        TRUE,        10,     15000,     3000,     4000,     600,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        1,      7500,    0,        0,         83,     72,     65,        0,    0,0,0,         -5000,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_AZTEC + ENVIRONMENTDATA_ALT        ,        TRUE,        30,     15000,     3000,     4000,     600,        0,         0x3E7,    0x3E4,    0x3E8,       0,       0,       0,        1,      7500,    0,        0,         83,     72,     65,        0,    0,0,0,         -5000,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_EGYPT                              ,        TRUE,        10,     20000,     3000,     4000,     600,        0,         0x3E7,    0x3E4,    0x3E8,    0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    0,0,0,         -5000,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_EGYPT + ENVIRONMENTDATA_ALT        ,        TRUE,        10,     10000,     3000,     4000,     600,        0,         0x3E7,    0x3CC,    0x3E8,       0,       0,       0,        1,      7500,    0,        0,         83,     10,     10,        0,    0,0,0,         -5000,       0,        0,        0,        0,      0,      0.0 },
    {LEVELID_NONE                               ,        FALSE,    {15.0f,    10000.0f, {0.0f,     0.0f,     0.0f},      0.0f,          0,        0,        0},     {0,    0x10,    0x40,        0,   5000.0f,    0,        0,     255.0f, 255.0f, 255.0f,        0,   {0, 0, 0},       0.0f,       0,        0,        0.0f,     0.0f,   0.0f,   0.0f},          {0.0f,           0.0f},},
    {LEVELID_SILO                               ,        FALSE,    {15.0f,    10000.0f, {0.0f,     0.0f,     0.0f},      0.0f,          0,        0,        0},     {0,    0x10,    0x60,        1,   5000.0f,    0,        0,     231.0f, 216.0f, 237.0f,        0,   {0, 0, 0},       0.0f,       0,        0,        0.0f,     0.0f,   0.0f,   0.0f},          {24.0f,          22.0f},},
    {LEVELID_FRIGATE                            ,        FALSE,    {15.0f,    10000.0f, {0.0f,     0.0f,     0.0f},      0.0f,          0,        0,        0},  {0x10,    0x30,    0x60,        1,   3000.0f,    0,        0,     230.0f, 230.0f, 230.0f,        1,   {0, 0, 0},    -150.0f,       2,        0,        255.0f, 255.0f, 150.0f,   0.0f},          {0.0f,           0.0f},},
    {LEVELID_CUBA                               ,        FALSE,    {15.0f,    10000.0f, {0.0f,     0.0f,     0.0f},      0.0f,          0,        0,        0},  {0x30,    0x40,    0x10,        0,   5000.0f,    0,        0,     255.0f, 255.0f, 255.0f,        0,   {0, 0, 0},       0.0f,       0,        0,        0.0f,     0.0f,   0.0f,   0.0f},          {0.0f,           0.0f},
    },
    {ENVIRONMENTDATA_END}
};


EnvironmentRecord *envGetCurrent(void)
{
    return &g_CurrentEnvironment;
}


f32 envGetScaledFarFogIntensitySquared(void)
{
    return g_ScaledFarFogIntensity * g_ScaledFarFogIntensity;
}


static EnvironmentRecord *envFindEnvironment(u32 id)
{
    EnvironmentRecord *record;

    for (record = g_EnvTable; record->Id != ENVIRONMENTDATA_END; record++)
    {
        if (record->Id == id)
        {
            return record;
        }
    }

    return NULL;
}


static void envLoadCurrentEnvironment(EnvironmentRecord *record)
{
    f32 zrange[2];
    f32 pk0;
    f32 pk4;
    f32 levelScale;
    f32 sp20;
    f32 sp1C;

    g_CurrentEnvironment = *record;
    g_PropFadeStartPx = record->PropVisibility.FadeStartPx;
    g_PropFadeEndPx = record->PropVisibility.FadeEndPx;

    viSetZRange(record->Visibility.NearClipDistance, record->Visibility.FarClipDistance);
    viGetZRange(&zrange);

    if (!record->FogEnabled)
    {
        g_FarFogIntensity = 0.0f;
        g_DifferenceFromFarFogIntensity = 0.0f;
        g_ScaledFarFogIntensity = FLT_MAX;
        g_ScaledDifferenceFromFarFogIntensity = 0.0f;
        g_NearFogValuesP = NULL;
        return;
    }

    levelScale = bgGetLevelVisibilityScale();
    zrange[0] /= levelScale;
    zrange[1] /= levelScale;

    g_DifferenceFromFarFogIntensity = (f32)record->Visibility.FogStart / 1000.0f;
    g_FarFogIntensity = (f32)record->Visibility.FogEnd / 1000.0f;

    g_ScaledFarFogIntensity = ((zrange[1] - zrange[0]) *  g_FarFogIntensity) + zrange[0];
    g_ScaledDifferenceFromFarFogIntensity = ((zrange[1] - zrange[0]) * g_DifferenceFromFarFogIntensity) + zrange[0];

    g_CurFogDetails.g_CurFogDetails = record->Visibility.NearClipDistance / levelScale;
    pk0 = g_CurFogDetails.g_CurFogDetails;
    g_CurFogDetails.scaled_far_fog_dist = record->Visibility.FarClipDistance / levelScale;
    pk4 = g_CurFogDetails.scaled_far_fog_dist;

    // numerator is constant 128.0f
    sp20 = (256.0f * (0.5f -         0.0f)) / (g_FarFogIntensity - g_DifferenceFromFarFogIntensity);
    sp1C = (256.0f * (0.5f - g_DifferenceFromFarFogIntensity)) / (g_FarFogIntensity - g_DifferenceFromFarFogIntensity);

    g_CurFogDetails.far_fog_dist_scaled = ((pk4 * -sp20 * (pk0 + 1.0f)) / (pk4 - pk0)) / 255.0f;
    g_CurFogDetails.near_fog_dist_scaled = ((sp20 * (pk4 + 1.0f) / (pk4 - pk0)) + sp1C) / 255.0f;

    if (record->Visibility.NearFog.NearFog == 0.0f)
    {
        g_NearFogValuesP = NULL;
    }
    else
    {
        g_NearFogValuesP = &g_CurrentEnvironment.Visibility.NearFog;
    }
}


void envLoadLevelEnvironment(s32 levelId, s32 useCinemaEnvironment)
{
    EnvironmentRecord *record = NULL;
    s32 num_players;

    num_players = getPlayerCount();

    if (num_players == 1)
    {
        num_players = 0;
    }

    if (useCinemaEnvironment)
    {
        record = envFindEnvironment(levelId + ENVIRONMENTDATA_CINEMA);
    }

    if (record == NULL)
    {
        record = envFindEnvironment(levelId + (num_players * 100));
    }

    if (record == NULL && num_players >= 2)
    {
        record = envFindEnvironment(num_players * 100);
    }

    if (record == NULL)
    {
        record = envFindEnvironment(LEVELID_NONE);
    }

    if (record->FogEnabled)
    {
        g_MainEnvironment = record;
        g_AlternateEnvironment = record + 1;
    }
    else
    {
        g_MainEnvironment = NULL;
        g_AlternateEnvironment = NULL;
    }

    envLoadCurrentEnvironment(record);
}


/**
 * Switch to next Environment.
 * @param transitionTime: Usually 0 for instant switch or 1 to transition gradually
 */
void envSwitchToSoloSky2(f32 transitionTime)
{
    static EnvironmentRecord static_envr;

    static_envr = *g_MainEnvironment;

    static_envr.Visibility.NearClipDistance =
        g_MainEnvironment->Visibility.NearClipDistance
        + (transitionTime * ((f32)g_AlternateEnvironment->Visibility.NearClipDistance
        - (f32)g_MainEnvironment->Visibility.NearClipDistance));

    static_envr.Visibility.FarClipDistance =
        g_MainEnvironment->Visibility.FarClipDistance
        + (transitionTime * ((f32)g_AlternateEnvironment->Visibility.FarClipDistance
        - (f32)g_MainEnvironment->Visibility.FarClipDistance));

    static_envr.Visibility.FogStart =
        (f32)g_MainEnvironment->Visibility.FogStart
        + (transitionTime * ((f32)g_AlternateEnvironment->Visibility.FogStart
        - (f32)g_MainEnvironment->Visibility.FogStart));

    static_envr.Visibility.FogEnd =
        (f32)g_MainEnvironment->Visibility.FogEnd
        + (transitionTime * ((f32)g_AlternateEnvironment->Visibility.FogEnd
        - (f32)g_MainEnvironment->Visibility.FogEnd));

    static_envr.Sky.Red =
        (f32)g_MainEnvironment->Sky.Red
        + (transitionTime * ((f32)g_AlternateEnvironment->Sky.Red
        - (f32)g_MainEnvironment->Sky.Red));

    static_envr.Sky.Green =
        (f32)g_MainEnvironment->Sky.Green
        + (transitionTime * ((f32)g_AlternateEnvironment->Sky.Green
        - (f32)g_MainEnvironment->Sky.Green));

    static_envr.Sky.Blue =
        (f32)g_MainEnvironment->Sky.Blue
        + (transitionTime * ((f32)g_AlternateEnvironment->Sky.Blue
        - (f32)g_MainEnvironment->Sky.Blue));

    static_envr.Sky.Red &= 0xf8;
    static_envr.Sky.Green &= 0xf8;
    static_envr.Sky.Blue &= 0xf8;

    envLoadCurrentEnvironment(&static_envr);
}


/**
 * Initializes fog state that is constant for the duration of one player's
 * world render. Fog position and alpha dithering do not need to be resent for
 * every room.
 */
Gfx *envBeginWorldFog(Gfx *gdl)
{
    if (!g_CurrentEnvironment.FogEnabled)
    {
        return gdl;
    }

    gSPFogPosition(gdl++, g_CurrentEnvironment.Visibility.FogStart,
            g_CurrentEnvironment.Visibility.FogEnd);
    gDPSetAlphaDither(gdl++, G_AD_NOISE);

    return gdl;
}


/**
 * Restores the environment fog colour after prop/model rendering and enables
 * hardware fog for background geometry.
 */
Gfx *envSetRenderFogColor(Gfx *gdl)
{
    if (!g_CurrentEnvironment.FogEnabled)
    {
        return gdl;
    }

    gDPSetFogColor(gdl++, g_CurrentEnvironment.Sky.Red, g_CurrentEnvironment.Sky.Green,
            g_CurrentEnvironment.Sky.Blue, 0xff);
    gSPSetGeometryMode(gdl++, G_FOG);

    return gdl;
}


Gfx *envRenderClearFogMode(Gfx *gdl)
{
    if (!g_CurrentEnvironment.FogEnabled)
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

    if (!g_CurrentEnvironment.FogEnabled)
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


NearFogSettings *envGetNearFogValues(void)
{
    return g_NearFogValuesP;
}


s32 envGetPropDistColor(PropRecord *prop, rgba_f32 *color)
{
    if (!g_CurrentEnvironment.FogEnabled)
    {
        return 2; // No fog, props cannot be obscured by fog
    }

    if (prop->zDepth < 0.0f)
    {
        return 2; // Prop is behind the camera
    }

    color->rgba[0] = (f32)g_CurrentEnvironment.Sky.Red / 255.0f;
    color->rgba[1] = (f32)g_CurrentEnvironment.Sky.Green / 255.0f;
    color->rgba[2] = (f32)g_CurrentEnvironment.Sky.Blue / 255.0f;
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
