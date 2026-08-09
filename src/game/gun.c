#include <ultra64.h>
#include "include/limits.h"
#include <bondconstants.h>
#include <bondtypes.h>
#include <bondgame.h>
#include <music.h>
#include <snd.h>
#include "bondview.h"
#include "bondinv.h"
#include "gun.h"
#include "chrobjdata.h"
#include "game/propobj.h"
#include "game/objective_status.h"
#include "quaternion.h"
#include "image_bank.h"
#include "bondwalk2.h"
#include "othermodemicrocode.h"
#include "player.h"
#include "lv.h"
#include "random.h"
#include "math_asinfacosf.h"
#include "loadobjectmodel.h"
#include "objecthandler.h"
#include "image.h"
#include "tex.h"
#include "debugmenu_handler.h"
#include "fr.h"
#include "assets/obseg/text/LgunE.h"
#include "textrelated.h"
#include "chrai.h"
#include "model.h"
#include "options.h"
#include "mpmenu.h"
#include "joy.h"
#include "matrixmath.h"
#include "bondinv.h"
#include "stan.h"


// bss
ALSoundState *g_CasingSfxState;
ALSoundState* g_UnusedSfxState; // Unused, type assumed from surrounding variables.
ALSoundState* g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];

CasingRecord g_Casings[20];
s32 dword_CODE_bss_80076A48; // Unused

#ifdef REFRESH_PAL
    /* PAL */
    #define THROWN_ITEM_REFRESH_RATE                   50
    #define THROWN_ITEM_TIMER_SOLO                     250
    #define THROWN_ITEM_TIMER_MULTI                    150
    #define THROWN_ITEM_TIMER_DEFAULT                  200
    #define GLGRENADE_TIMER                            1000
    #define DUAL_WIELD_TRIGGER_SWAP_TICKS              24
    #define DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS       36
    #define WATCH_SOUND_DURATION_TICKS                 250
    #define GUN_SPRING_DAMP                            0.9402999877929688f
    #define GUN_SPRING_SCALE                           0.05970001220703125f
#else
    /* NTSC */
    #define THROWN_ITEM_REFRESH_RATE                   60
    #define THROWN_ITEM_TIMER_SOLO                     300
    #define THROWN_ITEM_TIMER_MULTI                    180
    #define THROWN_ITEM_TIMER_DEFAULT                  240
    #define GLGRENADE_TIMER                            1200
    #define DUAL_WIELD_TRIGGER_SWAP_TICKS              20
    #define DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS       30
    #define WATCH_SOUND_DURATION_TICKS                 300
    #define GUN_SPRING_DAMP                            0.95f
    #define GUN_SPRING_SCALE                           0.050000012f
#endif

// data
////D:80032440
//rgba_u8 D_80032440[] = {
//	{0x96, 0x96, 0x96, 0},
//	{0x96, 0x96, 0x96, 0}
//};
//
////D:80032448
//rgba_u8 D_80032448[] = {
//	{0xFF, 0xFF, 0xFF, 0},
//	{0xFF, 0xFF, 0xFF, 0},
//	{0xB2, 0x4D, 0x2E, 0}
//};
/**
 * Controls the lighting on environment mapped weapons such as the Cougar Magnum and Golden Gun.
 */
Lights1 g_WeaponEnvmapLight = gdSPDefLights1(
    0x96, 0x96, 0x96,   // ambient RGB
    0xff, 0xff, 0xff,   // diffuse RGB
    0xb2, 0x4d, 0x2e);  // direction
//D:80032454
//u32 D_80032454 = 0;

//D:80032458
u32 D_80032458 = 0;

//D:8003245C
u32 size_item_buffer[] = {0x14820, 0x14820};

//D:80032464
u32 D_80032464[] ={0x7530, 0x7530};



//D:8003246C
CartridgeModelFileRecord ejected_cartridge[] = {
	{&cartridge_header, "GcartridgeZ"},
	{&cartrifle_header, "GcartrifleZ"},
	{&cartblue_header, "GcartblueZ"},
	{&cartshell_header, "GcartshellZ"},
	{0, ""}
};

#include <assets/obseg/gun/gunWeaponStats.inc.c>

//D:80033924
#include <assets/obseg/gun/gunModelFileRecord.inc.c>

//D:80034C9C
u32 cartridges_eject = 0;
//D:80034CA0
u32 g_gunDebKeyframeIndex = 0;

//D:80034CA4
u32 D_80034CA4[] = {
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41000000,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	0x3F000000,    0x41000000,           0x0,    0x40C00000,
	0xBFC00000,           0x0,    0x40B487B1,    0x3E70C0AD,
	0x3E0AE536,    0x3F000000,    0x41000000,           0x0,
	0x41480000,    0xC0600000,           0x0,    0x40C159EC,
	0x3D374BC7,    0x3F0E4378,    0x3F000000,    0x41000000,
	       0x0,    0xC1200000,    0xC1300000,           0x0,
	0x3F9ED962,    0x3EA24C40,    0x3F8B0DF1,    0x3F000000,
	0x41000000,           0x0,    0xC1600000,    0xC1700000,
	       0x0,    0x3FEA4780,    0x40C498E3,    0x3FA316D3,
	0x3F000000,    0x41200000,           0x0,    0xBF800000,
	0xC1100000,           0x0,    0x3EC4BBA1,    0x3EB87C42,
	0x3DD75968,    0x3F000000,    0x41200000,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,    0x3F000000,    0x41A00000,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41A00000,           0x1,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	       0,           0
};

u32 D_80034E0C[] = {
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41000000,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	0x3F000000,    0x41000000,           0x0,    0xC1080000,
	0xC0C00000,           0x0,    0x40AF7506,    0x40BAB4B9,
	0x40C2A5C2,    0x3F000000,    0x41000000,           0x0,
	0xC0400000,    0xC0600000,           0x0,    0x3ECE08F2,
	0x40B75721,    0x40B62409,    0x3F000000,    0x41000000,
	       0x0,    0xBF000000,    0xC1080000,           0x0,
	0x3F9DFD7A,    0x40B768CD,    0x40B37BDF,    0x3F000000,
	0x41000000,           0x0,    0x40E00000,    0xC1E40000,
	0xBFC00000,    0x3FA74949,    0x40B63EBC,    0x40B6443D,
	0x3F000000,    0x41200000,           0x0,    0xBFC00000,
	0xC1100000,           0x0,    0x3D8ADEEC,    0x40C84E72,
	0x3E506749,    0x3F000000,    0x41200000,           0x0,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,    0x3F000000,    0x41A00000,
	       0x0,           0x0,           0x0,           0x0,
	       0x0,           0x0,           0x0,    0x3F000000,
	0x41A00000,           0x1,           0x0,           0x0,
	       0x0,           0x0,           0x0,           0x0,
           0x0,           0x0
};

/**
 * Throwing Knife animation for when Z is pressed/held down.
 */
struct Weapon1PTransformKeyframe throwKnifeDrawBackKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Throwing Knife animation for when Z is released.
 */
struct Weapon1PTransformKeyframe throwKnifeReleaseKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, -20.0f, 18.0f}, { 0.785458f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 0, { 0.0f, -20.0f, 18.0f}, { 0.785458f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Grenade which is strange since you cannot see it on screen. Perhaps the developers once intended to have a proper first person Grenade throwing animation?
 */
struct Weapon1PTransformKeyframe grenadeThrowKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 10.0f, 12.5f, 17.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Timed Mine, but changing the durations has no effect.
 */
struct Weapon1PTransformKeyframe timedMineThrowKeyframes[6] = {
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 10.0f, 34.5f, 25.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 10.0f, 12.5f, 17.5f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Proximity Mine. Changing the durations does effect the time it takes to throw the mine, although nothing is seen on screen.
 */
struct Weapon1PTransformKeyframe proxMineThrowKeyframes[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 4.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Keyframes for the Remote Mine. Changing the durations does not effect the time it takes to throw them,
 * but it does change the time it takes before you can throw another.
 */
struct Weapon1PTransformKeyframe remoteMineThrowKeyframes[7] = {
    { 0, { 0.0f, 0.0f, 4.5f}, { 5.576369f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 20.5f}, { 5.26209f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 3.0f, 5.5f}, { 0.031375f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, -20.0f, 18.0f}, { 0.785458f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Slapper attack when the hand starts at the right of the screen then chops downward and to the left.
 */
Weapon1PTransformKeyframe fistMeleeKeyframes1[10] = {
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 10.0f },
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 10.0f },
    { 0, {   6.0f, 23.0f, 0.0f }, {  5.91572f, 0.085832f, 0.219482f }, 0.5f, 10.0f },
    { 0, {  18.0f, 35.0f, 9.5f }, { 4.998193f, 0.084203f, 0.268954f }, 0.5f, 10.0f },
    { 0, { -20.0f, 25.5f, 4.0f }, { 0.126148f, 0.304284f, 0.548047f }, 0.5f, 10.0 },
    { 0, { -28.0f, -4.0f, 2.0f }, { 0.506821f,  0.51473f, 0.484098f }, 0.5f,  1.0f },
    { 0, { -28.0f, -4.0f, 2.0f }, { 0.506821f,  0.51473f, 0.484098f }, 0.5f,  1.0f },
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 20.0f },
    { 0, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.5f, 20.0f },
    { 1, {   0.0f,  0.0f, 0.0f }, {      0.0f,      0.0f,      0.0f }, 0.0f,  0.0f }
};

/**
 * Slapper attack when the hand moves to the left of the screen then chops downward and to the right.
 */
Weapon1PTransformKeyframe fistMeleeKeyframes2[10] = {
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 10.0f},
       { 0, { -6.0f, 23.0f, 0.0f}, { 5.08683f, 6.131295f, 5.534376f}, 0.5f, 10.0f},
       { 0, { -18.0f, 35.0f, 9.5f}, { 4.880698f, 0.070396f, 5.53615f}, 0.5f, 10.0f},
       { 0, { 8.0f, 25.5f, 4.0f}, { 0.107213f, 6.062361f, 5.404225f}, 0.5f, 10.0f},
       { 0, { 28.0f, -4.0f, 2.0f}, { 0.107213f, 6.062361f, 5.404225f}, 0.5f, 1.0f},
       { 0, { 28.0f, -4.0f, 2.0f}, { 0.107213f, 6.062361f, 5.404225f}, 0.5f, 1.0f},
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
       { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
       { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Sniper swing right to left.
 */
Weapon1PTransformKeyframe sniperMeleeKeyframes1[11] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 9.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, {9.5f, -0.5f, 3.5f}, { 0.291053f, 5.584375f, 6.212358f}, 0.5f, 8.0f},
    { 0, {18.0f, 7.5f, 3.5f}, { 0.439372f, 5.945201f, 5.993666f}, 0.5f, 8.0f},
    { 0, {-9.0f, 8.5f, 5.5f}, { 0.704803f, 0.194459f, 6.168447f}, 0.5f, 7.0f},
    { 0, {-29.0f, -5.5f, 5.5f}, { 2.281831f, 1.106353f, 1.489998f}, 0.5f, 7.0f},
    { 0, {-57.5f, -27.5f, 5.5f}, { 2.281831f, 1.106353f, 1.489998f}, 0.5f, 7.0f},
    { 0, {-19.5f, -20.0f, 5.5f}, { 1.22519f, 0.726087f, 1.210713f}, 0.5f, 15.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 20.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Sniper swing left to right.
 */
Weapon1PTransformKeyframe sniperMeleeKeyframes2[11] = {
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f,  9.0f },
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f,  8.0f },
    { 0, { -15.5f,  0.5f, 15.0f }, {  0.9344959855079651f,  0.6256099939346313f,  0.2237969934940338f }, 0.5f,  8.0f },
    { 0, { -23.0f,  2.0f, 12.0f }, {  1.8016400337219238f,  0.9494050145149231f,  0.6307389736175537f }, 0.5f,  8.0f },
    { 0, { -18.0f, -0.5f,  4.0f }, {  0.8478249907493591f,  0.9247649908065796f, 0.07744300365447998f }, 0.5f,  7.0f },
    { 0, {  10.5f,  5.0f,  2.5f }, { 0.22940599918365479f, 0.24570399522781372f, 0.09906300157308578f }, 0.5f,  7.0f },
    { 0, {  18.0f,  5.0f,  2.5f }, { 0.03281300142407417f,    6.20933723449707f,  0.1350640058517456f }, 0.5f,  7.0f },
    { 0, {   9.5f,  3.5f, -1.5f }, {   6.273238182067871f,   6.005795001983643f, 0.08971499651670456f }, 0.5f,  7.0f },
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f, 20.0f },
    { 0, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.5f, 20.0f },
    { 1, {   0.0f,  0.0f,  0.0f }, {                 0.0f,                 0.0f,                 0.0f }, 0.0f,  0.0f },
};

/**
 * Animation when the Taser is lowering to fire position.
 */
Weapon1PTransformKeyframe taserFireKeyFrames[6] = {
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    { 0, { 0.5f, -6.0f, -8.0f}, { 0.439468f, 0.278829f, 0.195178f}, 0.5f, 8.0f},
    { 0, { -2.0f, -8.0f, -10.0f}, { 1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    { 0, { -2.0f, -8.0f, -10.0f}, { 1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    { 1, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

/**
 * Animation once the Taser has fired and goes back to idle position.
 */
Weapon1PTransformKeyframe taserRaiseKeyframes[6] = {
    {0, { -2.0f, -8.0f, -10.0f}, {1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    {0, { -2.0f, -8.0f, -10.0f}, {1.101655f, 0.460753f, 0.570961f}, 0.5f, 8.0f},
    {0, { 0.5f, -6.0f, -8.0f}, {0.439468f, 0.278829f, 0.195178f}, 0.5f, 8.0f},
    {0, { 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    {0, { 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f, 8.0f},
    {1, { 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}
};

coord3d D_80035C40 = {0.0f, 0.0f, 0.0f};
coord3d D_80035C4C = {0.0f, 0.0f, 0.0f};
coord3d D_80035C58 = {0.0f, 0.0f, -1.0f};
coord3d D_80035C64 = {0.0f, 1.0f, 0.0f};
coord3d D_80035C70 = {6.2536321f, 6.2592888f, 0.204238f};
coord3d D_80035C7C = {0.25044999f, 0.90482301f, 0.28716999f};
coord3d D_80035C88 = {1.715736f, 0.37460899f, 0.92193699f};

//D:80035C94
f32 D_80035C94 = 0;


//D:80035C98
u32 D_80035C98 = 0;
//D:80035C9C
u32 D_80035C9C = 0;
//D:80035CA0
u32 D_80035CA0 = 0;
//D:80035CA4
s32 D_80035CA4 = 0xFFFFFFFF;
//D:80035CA8
u32 D_80035CA8 = 0;
//D:80035CAC
u32 D_80035CAC = 0;
//D:80035CB0
u32 D_80035CB0 = 0;
//D:80035CB4
u32 D_80035CB4 = 0;
//D:80035CB8
u32 D_80035CB8 = 0;
//D:80035CBC
u32 D_80035CBC = 0;
//D:80035CC0
u32 D_80035CC0 = 0;



//D:80035CC4
u32 D_80035CC4[] =                      { 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,           0,  0};
/* ModelRenderData D_8002CCBC = {NULL,
                                TRUE,
                                0x00000003,
                                NULL,
                                NULL,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0,
                                {0, 0, 0, 0},
                                {0, 0, 0, 0},
                                CULLMODE_BOTH};
*/
//D:80035D00
u32 D_80035D00 = 0;
//D:80035D04
u32 D_80035D04[] = {1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//D:80035D44
u32 watchControllerButtonBases[] = {
	1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//D:0x80035E04
struct RicochetSoundsSmall ricochet_sounds_small = {
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_5_A_SFX,      RICO_5_B_SFX,      RICO_5_C_SFX,      RICO_5_D_SFX,
    RICO_6_HBBA_A_SFX, RICO_6_HBBA_B_SFX, RICO_6_HBBA_C_SFX, RICO_6_HBBA_D_SFX
};

//D:80035E2C
struct PunchSounds punch_sounds = {
    PUNCH1_SFX,
    PUNCH2_SFX,
    PUNCH3_SFX
};

//D:80035E34
struct BulletFleshSounds bullet_flesh_sounds = {
    HIT_BULLET_FLESH_SFX,
    HIT_BULLET_FLESH_SFX
};

struct LaserRichochetSounds laser_ricochet_sounds = {
    RICO_LASER2_SFX,
    RICO_LASER3_SFX
};

struct RicochetSoundsLarge ricochet_sounds_large = {
	RICO_12_GBU_A_SFX, RICO_12_GBU_B_SFX, RICO_12_GBU_C_SFX, RICO_12_GBU_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_5_A_SFX,      RICO_5_B_SFX,      RICO_5_C_SFX,      RICO_5_D_SFX,
    RICO_6_HBBA_A_SFX, RICO_6_HBBA_B_SFX, RICO_6_HBBA_C_SFX, RICO_6_HBBA_D_SFX
};

//D:80035E84
struct EarWhistleSounds ear_whistle_sounds = {
    RICO_EAR_WHISTLE1_SFX,
    RICO_EAR_WHISTLE2_SFX,
    RICO_EAR_WHISTLE3_SFX,
    RICO_EAR_WHISTLE4_SFX,
    RICO_EAR_WHISTLE5_SFX
};

//D:80035E90
struct sfx2 watchlaser_fire_sounds = { RICO_LASER2_SFX, RICO_LASER3_SFX };
//D:80035E94
struct sfx3 knife_throw_sounds = { KNIFE_THROW1_SFX, KNIFE_THROW2_SFX, KNIFE_THROW3_SFX };
//D:80035E9C
struct gun_trigger_state g_ZeroTriggerState = { 0, 0 };
//D:80035EA0
//u32 D_80035EA0 = 0;
//D:80035EA4
u32 D_80035EA4 = 0;
//D:80035EA8
u32 D_80035EA8 = 0;
//D:80035EAC
u32 D_80035EAC = 0;
//D:80035EB0
u32 g_DefaultCasingModelRenderData[] = {0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//D:80035EEC
u32 dword_D_80035EEC = 0; // Unused

//D:80035EF0
#define AMMO_RELATED_MAX 30
AmmoStats ammo_related[AMMO_RELATED_MAX] = {
    { 0x0    , 0x00000000,   0.0f, },
    { 0x320  , 0x02000C84,   0.0f, },
    { 0xC8   , 0x00000000,   0.0f, },
    { 0x190  , 0x02000C90,  -2.0f, },
    { 0x64   , 0x02000C9C,   0.0f, },
    { 0xC    , 0x02000CD8,   0.0f, },
    { 0x3    , 0x02000CC0,  -2.0f, },
    { 0xA    , 0x02000CFC,   1.0f, },
    { 0xA    , 0x02000D14,   1.0f, },
    { 0xA    , 0x02000D08,   1.0f, },
    { 0xA    , 0x02000CA8,   0.0f, },
    { 0xC    , 0x02000CB4,   0.0f, },
    { 0xC8   , 0x02000CE4,   0.0f, },
    { 0x64   , 0x02000CF0,   0.0f, },
    { 0x32   , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x2    , 0x00000000,   0.0f, },
    { 0x8    , 0x00000000,   0.0f, },
    { 0x6    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x1    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x3E8  , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0xA    , 0x00000000,   0.0f, },
    { 0x32   , 0x02000D20,  -1.0f, },
    { 0x1    , 0x00000000,   0.0f, },
};

//was previously attached to ammo_related[] (array at D:80035EF0)
//D:80036058
u16 D_80036058[] = { 0, 0, 0, 0, };

extern struct ModelSkeleton skeleton_gun_kf7;

//i may belong to objecthandler.c
// D:80036060 canonically freedist
struct ModelHitEntry *g_ModelHitFreeList = NULL;

typedef struct ModelHeader {
            s16                unk00;
            s16                Type;
            struct ChrRecord  *chr;
            ModelFileHeader   *obj;
            RenderPosView     *render_pos;
            union ModelRwData **datas;
            f32               scale;
            struct Model     *attachedto;
            ModelNode        *attachedto_objinst;
} ModelHeader;


// forward declarations

void bullet_path_from_screen_center(coord3d* arg0, coord3d* arg1, enum GUNHAND arg2);
void gunInitProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *arg2, coord3d *velocity, Mtxf *arg4);
s32 gunSample1PTransform(Weapon1PTransformKeyframe *keyframes, f32 time, Mtxf *matrix, GUNHAND hand);
void analyzeGEKey(void);
void give_weapon_case_items(void);
struct ModelFileHeader * get_ptr_weapon_model_header_line(ITEM_IDS weapon);
s32 get_ammo_in_hands_weapon(enum GUNHAND hand);
s32 get_ammo_type_for_weapon(ITEM_IDS weapon);
f32 gunSetHorizontalOffset(GUNHAND hand);
void give_weapon_case_items(void);
void sub_GAME_7F05DA8C(GUNHAND hand, ITEM_IDS weaponnum_watchmenu);
void sub_GAME_7F05E808(GUNHAND hand);
void sub_GAME_7F0649D8(enum GUNHAND hand);
void gunCreateBeamForHand(enum GUNHAND hand);
CasingRecord* casingCreate(ModelFileHeader* header, Mtxf* mtx);
void sub_GAME_7F068508(GUNHAND handnum, f32 floor_y_pos);

// end forward declarations

// current debug keyframes
#define DEB_KEYFRAMES sniperMeleeKeyframes2


void set_cartridges_eject(u32 uParm1)
{
    cartridges_eject = uParm1;
}


u32 get_cartridges_eject(void)
{
    return cartridges_eject;
}


// Unreferenced debPrintKeyframe
// Address: ~ 7F05C538
void nullsub_73(void)
{
#ifdef DEBUG
    osSyncPrintf("\t{");
    osSyncPrintf("0");
    osSyncPrintf(",{%ff,%ff,%ff}", DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos.x, DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos.y, DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos.z);
    osSyncPrintf(",{%ff,%ff,%ff}", DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot.x, DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot.y, DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot.z);
    osSyncPrintf(",0.5f,20.0f");
    osSyncPrintf("},\n");
#endif
    return;
}


// Unreferenced - force keyframe to position
// Address: 7F05C540
void sub_GAME_7F05C540(coord3d* pos)
{
    Weapon1PTransformKeyframe* temp_v0;

    temp_v0 = &DEB_KEYFRAMES[g_gunDebKeyframeIndex];
    temp_v0->pos.x += pos->x;
    temp_v0->pos.y += pos->y;
    temp_v0->pos.z += pos->z;
}


// Unreferenced
void sub_GAME_7F05C594(Mtxf* mtxf)
{
    Mtxf sp18;
    matrix_4x4_set_rotation_around_xyz(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot, &sp18);
    matrix_4x4_multiply_in_place(mtxf, &sp18);
    matrix_4x4_get_rotation_around_xyz(&sp18, &DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot);
}


void sub_GAME_7F05C614(void)
{
    if (!cartridges_eject) { return; }

    g_CurrentPlayer->hands[0].field_92C = 1;
    matrix_4x4_set_rotation_around_xyz(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].rot, (Mtxf *)&g_CurrentPlayer->hands[0].field_8EC);
    matrix_4x4_set_position(&DEB_KEYFRAMES[g_gunDebKeyframeIndex].pos, (Mtxf *)&g_CurrentPlayer->hands[0].field_8EC);
    cartridges_eject = 0;
}


// Unreferenced increment keyframe index, loop back to start if final keyframe is reached
// Address: 7F05C6B8
void gunDebAdvanceKeyframe(void)
{
    g_gunDebKeyframeIndex++;
    if (DEB_KEYFRAMES[g_gunDebKeyframeIndex].isFinalKey & 1)
    {
        g_gunDebKeyframeIndex = 0;
    }
}


/**
 * Address: 7F05C6FC
 * 
 * Sample a first person weapon transform keyframe animation at `time` and
 * write the interpolated transform into `matrix`
 * 
 * @returns 1 when an in-progress frame was interpolated, 0 when `time` reached
 * the final keyframe and the static end pose was written.
 */
s32 gunSample1PTransform(Weapon1PTransformKeyframe *keyframes, f32 time, Mtxf *matrix, GUNHAND hand)
{
    Weapon1PTransformKeyframe *current;
    f32 frac;
    f32 tangent;
    coord3d posResult;
    quatf qResult;
    quatf q0;
    quatf q1;
    quatf q2;
    quatf q3;
    s32 i;

    i = 1;
    frac = keyframes[1].duration;
    current = keyframes + i;

    while (time >= current->duration)
    {
        time -= current->duration;
        i++;
        current++;

        if (current[2].isFinalKey & 1)
        {
            break;
        }
    }

     // Comma operator is intentional: nudges IDO to emit addu s0,a0,t8.
    current = (0, keyframes) + i;

    i = 2;

    if (current[i].isFinalKey & 1)
    {
        matrix_4x4_set_rotation_around_xyz(&current->rot, matrix);
        matrix_4x4_set_position(&current->pos, matrix);
        return 0;
    }

    frac = time / current->duration;
    tangent = current->interpParam;

    quaternion_set_rotation_around_xyzf(&current[-1].rot, &q0);
    quaternion_set_rotation_around_xyzf(&current->rot, &q1);
    quaternion_set_rotation_around_xyzf(&current[1].rot, &q2);
    quaternion_set_rotation_around_xyzf(&current[2].rot, &q3);

    quaternion_ensure_shortest_path(&q1, &q2);
    quaternion_ensure_shortest_path(&q2, &q3);
    quaternion_ensure_shortest_path(&q1, &q0);

    quaternion_7F05C2F0(&q0, &q1, &q2, &q3, frac, &qResult);

    coord3dCubicSplineInterp(&current[-1].pos, &current->pos, &current[1].pos, &current[2].pos, frac, tangent, &posResult);

    if (hand == GUNLEFT)
    {
        posResult.x = -posResult.x;
        qResult[0] = -qResult[0];
        qResult[1] = -qResult[1];
    }

    quaternion_to_matrix(&qResult, matrix);
    matrix_4x4_set_position(&posResult, matrix);

    return 1;
}


WeaponStats *get_ptr_item_statistics(ITEM_IDS item)
{
    if (gitem_structs[item].has_no_model == 0)
    { /* weapon has model, return stats struct */
        return gitem_structs[item].item_weapon_stats;
    }
    return &default_weaponstats; /* no model, return defaults */
}




void copy_item_in_hand(coord3d *pos)
{
    ITEM_IDS item;
    WeaponStats *stats;

    item = getCurrentPlayerWeaponId(0);
    stats = get_ptr_item_statistics(item);

    pos->x = stats->PosX;
    pos->y = stats->PosY;
    pos->z = stats->PosZ;
}


void copy_item_in_hand_to_main_list(coord3d *pos) {

    WeaponStats *stats;
    ITEM_IDS item;

    item = getCurrentPlayerWeaponId(0);
    stats = get_ptr_item_statistics(item);

    stats->PosX = pos->x;
    stats->PosY = pos->y;
    stats->PosZ = pos->z;
}


void bgunCalculateBlend(enum GUNHAND handnum)
{
    s32 sp60[2];
    s32 sp58[2];
    f32 mult = get_ptr_item_statistics(getCurrentPlayerWeaponId(handnum))->Sway;

    sp60[handnum] = (g_CurrentPlayer->hands[handnum].curblendpos + 2) % 4;
    sp58[handnum] = (g_CurrentPlayer->hands[handnum].curblendpos + 1) % 4;
    g_CurrentPlayer->hands[handnum].curblendpos = sp58[handnum];

    g_CurrentPlayer->hands[handnum].blendlook[sp60[handnum]].x = (RANDOMFRAC() - 0.5f) * 0.08f * mult;
    g_CurrentPlayer->hands[handnum].blendlook[sp60[handnum]].y = (RANDOMFRAC() - 0.5f) * 0.1f * mult;
    g_CurrentPlayer->hands[handnum].blendlook[sp60[handnum]].z = -1;

    g_CurrentPlayer->hands[handnum].blendup[sp60[handnum]].x = (RANDOMFRAC() - 0.5f) * 0.1f * mult;
    g_CurrentPlayer->hands[handnum].blendup[sp60[handnum]].y = 1;
    g_CurrentPlayer->hands[handnum].blendup[sp60[handnum]].z = (RANDOMFRAC() - 0.5f) * 0.1f * mult;

    g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].x = (RANDOMFRAC() * 0.75f) + 1.5f;
    g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].y = (2 + RANDOMFRAC()) * g_CurrentPlayer->hands[handnum].blendscale1;
    g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].z = (RANDOMFRAC() - 0.5f) * 2.5f;

    if (g_CurrentPlayer->hands[handnum].sideflag < 0)
    {
        g_CurrentPlayer->hands[handnum].blendpos[sp60[handnum]].x *= -1;

        if (g_CurrentPlayer->hands[handnum].sideflag == -2)
        {
            g_CurrentPlayer->hands[handnum].sideflag = 1;
        }
        else
        {
            g_CurrentPlayer->hands[handnum].sideflag = -2;
        }
    }
    else
    {
        if (g_CurrentPlayer->hands[handnum].sideflag == 2)
        {
            g_CurrentPlayer->hands[handnum].sideflag = -1;
        }
        else
        {
            g_CurrentPlayer->hands[handnum].sideflag = 2;
        }
    }

    g_CurrentPlayer->hands[handnum].blendscale1 = -g_CurrentPlayer->hands[handnum].blendscale1;
}


s32 Gun_hand_without_item(enum GUNHAND arg0)
{
    return g_CurrentPlayer->hand_invisible[arg0] > 0
        || (g_CurrentPlayer->hand_item[arg0] == 0 && g_CurrentPlayer->field_2A44[arg0] < 0);
}


s32 get_itemtype_in_hand(GUNHAND hand)
{
    return g_CurrentPlayer->hand_item[hand];
}


ModelFileHeader *get_ptr_itemheader_in_hand(GUNHAND hand)
{
    return &g_CurrentPlayer->copy_of_body_obj_header[hand];
}


u8 * getPlayerWeaponBufferForHand(GUNHAND hand)
{
    return g_CurrentPlayer->ptr_hand_weapon_buffer[hand];
}


u32 getSizeBufferWeaponInHand(s32 hand)
{
    return size_item_buffer[hand];
}


void remove_item_in_hand(GUNHAND hand)
{
  g_CurrentPlayer->hand_invisible[hand] = 0;
  g_CurrentPlayer->hand_item[hand] = ITEM_UNARMED;
  g_CurrentPlayer->field_2A44[hand] = -1;
  g_CurrentPlayer->lock_hand_model[hand] = 1;
  return;
}


void place_item_in_hand_swap_and_make_visible(GUNHAND hand, ITEM_IDS item)
{
    if (g_CurrentPlayer->lock_hand_model[hand]) { return; }

    if (g_CurrentPlayer->hand_invisible[hand] >= 0)
    {
        if (item != g_CurrentPlayer->hand_item[hand])
        {
            g_CurrentPlayer->hand_invisible[hand] = -1;
            g_CurrentPlayer->field_2A44[hand] = item;
        }
        return;
    }

    if (item != g_CurrentPlayer->hand_item[hand])
    {
        g_CurrentPlayer->field_2A44[hand] = item;
        return;
    }

    g_CurrentPlayer->hand_invisible[hand] = 1;
}


char *get_ptr_item_text_call_line(ITEM_IDS item)
{
    if (item == ITEM_FIST)
    {
        item = g_CurrentPlayer->cur_item_weapon_getname;
    }
    return gitem_structs[item].item_file_name;
}


 ModelFileHeader *get_ptr_weapon_model_header_line(ITEM_IDS weapon)
{
    if (weapon == ITEM_FIST)
    {
        weapon = g_CurrentPlayer->cur_item_weapon_getname;
    }
    return gitem_structs[weapon].item_header;
}


int getCurrentWeaponOrItem(void)
{
    return g_CurrentPlayer->cur_item_weapon_getname;
}


void used_to_load_1st_person_model_on_demand(GUNHAND hand)
{
    u32              size_buffer_weapon;
    s8              *ptr_item_text;
    ModelFileHeader *ptr_weapon_model;
    u8              *buffer_weapon;
    enum ITEM_IDS    item;

    if ((g_CurrentPlayer->hand_invisible[hand] < 0) && (g_CurrentPlayer->lock_hand_model[hand] == 0))
    {
        if ((g_CurrentPlayer->hand_invisible[hand] < -2) || (g_CurrentPlayer->hand_item[hand] == ITEM_UNARMED))
        {
            item             = g_CurrentPlayer->field_2A44[hand];
            ptr_item_text    = (s8 *)get_ptr_item_text_call_line(item);
            ptr_weapon_model = get_ptr_weapon_model_header_line(item);

            if ((ptr_item_text != NULL) && (ptr_weapon_model != NULL))
            {
                buffer_weapon      = getPlayerWeaponBufferForHand(hand);
                size_buffer_weapon = getSizeBufferWeaponInHand(hand);

                g_CurrentPlayer->copy_of_body_obj_header[hand] = *ptr_weapon_model;

                if (item == ITEM_SUIT_LF_HAND)
                {
                    texInitPool(&g_CurrentPlayer->item_related[hand], buffer_weapon + 0xBD70, size_buffer_weapon + 0xFFFF4290);
                    load_object_fill_header(&g_CurrentPlayer->copy_of_body_obj_header[hand], (u8 *)ptr_item_text, buffer_weapon, 0xBD70, &g_CurrentPlayer->item_related[hand]);
                }
                else if ((item == ITEM_TRIGGER) || (item == ITEM_WATCHLASER))
                {
                    texInitPool(&g_CurrentPlayer->item_related[hand], buffer_weapon + 0xAFD0, size_buffer_weapon + 0xFFFF5030);
                    load_object_fill_header(&g_CurrentPlayer->copy_of_body_obj_header[hand], (u8 *)ptr_item_text, buffer_weapon, 0xAFD0, &g_CurrentPlayer->item_related[hand]);
                }
                else
                {
                    texInitPool(&g_CurrentPlayer->item_related[hand], &buffer_weapon[D_80032464[hand]], size_buffer_weapon - D_80032464[hand]);
                    load_object_fill_header(&g_CurrentPlayer->copy_of_body_obj_header[hand], (u8 *)ptr_item_text, buffer_weapon, D_80032464[hand], &g_CurrentPlayer->item_related[hand]);
                }
            }

            g_CurrentPlayer->hand_invisible[hand] = 1;
            g_CurrentPlayer->hand_item[hand]      = item;
            g_CurrentPlayer->field_2A44[hand]     = -1;
        }
        else
        {
            g_CurrentPlayer->hand_invisible[hand]--;
        }
    }
}


// Called by unused functions.
ITEM_IDS sub_GAME_7F05D334(ITEM_IDS item, s32 arg1)
{
    while (arg1 > 0)
    {
        do
        {
            item = (item + 1) % ITEM_BOMBCASE;
        } while (bondinvItemAvailable(item) == 0);
        arg1--;
    }

    while (arg1 < 0)
    {
        do
        {
            item--;
            if (item < 0)
            {
                item = 0x20 - (-(item + 1) % ITEM_BOMBCASE);
            }
        } while (bondinvItemAvailable(item) == 0);
        arg1++;
    }

    return item;
}


ITEM_IDS get_next_weapon_in_cycle_for_hand(GUNHAND hand, s32 direction)
{
	if (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_LOWER) 
    {
		if (
			(direction < 0 && (g_CurrentPlayer->hands[hand].field_8B8 > 0)) ||
			(direction > 0 && (g_CurrentPlayer->hands[hand].field_8B8 < 0)) ) 
        {
			return getCurrentPlayerWeaponId(hand);
		}
		else 
        {
			return g_CurrentPlayer->hands[hand].weapon_next_weapon;
		}

    }
    
    if (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP) 
    {
        return g_CurrentPlayer->hands[hand].weapon_next_weapon;
    }

    return getCurrentPlayerWeaponId(hand);
}


void gunRequestHandWeaponChange(enum GUNHAND hand, s32 nextWeapon, s32 cycleDirection)
{
    if ((g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_LOWER) || (g_CurrentPlayer->hands[hand].weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP))
    {
        g_CurrentPlayer->hands[hand].field_8B0 = g_CurrentPlayer->hands[hand].field_890;

#ifdef VERSION_EU
        if (getPlayerCount() == 1) {
            g_CurrentPlayer->hands[hand].field_8B0 += 0xE;
        } else {
            g_CurrentPlayer->hands[hand].field_8B0 += 0xA;
        }
#else
        if (getPlayerCount() == 1) {
            g_CurrentPlayer->hands[hand].field_8B0 += 0x11;
        } else {
            g_CurrentPlayer->hands[hand].field_8B0 += 0xD;
        }
#endif
    }

    if (get_next_weapon_in_cycle_for_hand(hand, 0) != nextWeapon)
    {
        if ((g_CurrentPlayer->hands[hand].weapon_action_state != GUN_ANIM_STATE_SWITCH_LOWER) && (g_CurrentPlayer->hands[hand].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP))
        {
            g_CurrentPlayer->hands[hand].weapon_current_animation = 5;
        }

        g_CurrentPlayer->hands[hand].weapon_next_weapon = nextWeapon;
        g_CurrentPlayer->hands[hand].weapon_animation_trigger = 1;
        g_CurrentPlayer->hands[hand].field_8B8 = cycleDirection;
    }
}


// Unused
void sub_GAME_7F05D610(GUNHAND hand)
{
    gunRequestHandWeaponChange(hand, sub_GAME_7F05D334(get_next_weapon_in_cycle_for_hand(hand, 0), 1), 0);
}


// Unused
void sub_GAME_7F05D650(GUNHAND hand)
{
    gunRequestHandWeaponChange(hand, sub_GAME_7F05D334(get_next_weapon_in_cycle_for_hand(hand, 0), -1), 0);
}


void sub_GAME_7F05D690(void)
{
    currentPlayerEquipWeaponWrapper(GUNRIGHT, g_CurrentPlayer->hands[GUNRIGHT].previous_weapon);
    currentPlayerEquipWeaponWrapper(GUNLEFT, g_CurrentPlayer->hands[GUNLEFT].previous_weapon);
}


void advance_through_inventory(void)
{
    ITEM_IDS nextright;
    ITEM_IDS nextleft;

    nextright = get_next_weapon_in_cycle_for_hand(GUNRIGHT, 1);
    nextleft = get_next_weapon_in_cycle_for_hand(GUNLEFT, 1);

    if ((nextright >= ITEM_BOMBCASE) || (nextleft >= ITEM_BOMBCASE))
    {
        nextright = g_CurrentPlayer->hands[GUNRIGHT].previous_weapon;
        nextleft = g_CurrentPlayer->hands[GUNLEFT].previous_weapon;
    }
    else
    {
        bondinvCycleForward(&nextright, &nextleft, FALSE);
    }

    gunRequestHandWeaponChange(GUNRIGHT, nextright, 1);
    gunRequestHandWeaponChange(GUNLEFT, nextleft, 1);
}


void backstep_through_inventory(void)
{
    ITEM_IDS nextright;
    ITEM_IDS nextleft;

    nextright = get_next_weapon_in_cycle_for_hand(GUNRIGHT, -1);
    nextleft = get_next_weapon_in_cycle_for_hand(GUNLEFT, -1);

    if ((nextright >= ITEM_BOMBCASE) || (nextleft >= ITEM_BOMBCASE))
    {
        nextright = g_CurrentPlayer->hands[GUNRIGHT].previous_weapon;
        nextleft = g_CurrentPlayer->hands[GUNLEFT].previous_weapon;
    }
    else
    {
        bondinvCycleBackward(&nextright, &nextleft, FALSE);
    }

    gunRequestHandWeaponChange(GUNRIGHT, nextright, -1);
    gunRequestHandWeaponChange(GUNLEFT, nextleft, -1);
}

void autoadvance_on_deplete_all_ammo(void)
{
	ITEM_IDS nextright;
	ITEM_IDS nextleft;
	ITEM_IDS duperight;
	ITEM_IDS dupeleft;

    nextright = get_next_weapon_in_cycle_for_hand(GUNRIGHT, 1);
    duperight = nextright;

    nextleft = get_next_weapon_in_cycle_for_hand(GUNLEFT, 1);
    dupeleft = nextleft;

    if ((duperight >= ITEM_BOMBCASE) || (dupeleft >= ITEM_BOMBCASE))
    {
        duperight = g_CurrentPlayer->hands[GUNRIGHT].previous_weapon;
        dupeleft = g_CurrentPlayer->hands[GUNLEFT].previous_weapon;
    }
    else if ((duperight == ITEM_REMOTEMINE) && ((bondinvItemAvailable(ITEM_TRIGGER))))
    {
        duperight = ITEM_TRIGGER;
        dupeleft = ITEM_UNARMED;
    }
    else
    {
        bondinvCycleForward(&duperight, &dupeleft, TRUE);

        if ((duperight < nextright) || ((duperight == nextright) && (nextleft >= dupeleft)))
        {
			duperight = nextright;
			dupeleft = nextleft;
			bondinvCycleBackward(&duperight, &dupeleft, TRUE);
        }
    }

    gunRequestHandWeaponChange(GUNRIGHT, duperight, 1);
    gunRequestHandWeaponChange(GUNLEFT, dupeleft, 1);
}

s32 currentPlayerEquipWeaponWrapper(GUNHAND hand, s32 next_weapon) {
    g_CurrentPlayer->hands[hand].weapon_current_animation = 5;
    g_CurrentPlayer->hands[hand].weapon_next_weapon = next_weapon;
    g_CurrentPlayer->hands[hand].weapon_animation_trigger = 0;
}

void attempt_reload_item_in_hand(GUNHAND hand) {
    s32 ammo_type = get_ammo_type_for_weapon(getCurrentPlayerWeaponId(hand));
    if (ammo_type != 0) {
        if (g_CurrentPlayer->hands[hand].weapon_current_animation == 0) {
            g_CurrentPlayer->hands[hand].weapon_current_animation = 9;
        }
    }
}

ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weaponnum;
}

void draw_item_in_hand(GUNHAND hand, s32 next_weapon) {
	g_CurrentPlayer->hands[hand].weapon_current_animation = 0xE;
	g_CurrentPlayer->hands[hand].weapon_next_weapon = next_weapon;
}

ITEM_IDS get_item_in_hand_or_watch_menu(GUNHAND hand) {
	if (g_CurrentPlayer->hands[hand].weaponnum_watchmenu >= 0) {
		return g_CurrentPlayer->hands[hand].weaponnum_watchmenu;
	} else {
		return g_CurrentPlayer->hands[hand].weaponnum;
	}
}

void sub_GAME_7F05DA8C(GUNHAND hand, ITEM_IDS weaponnum_watchmenu) {
    place_item_in_hand_swap_and_make_visible(hand, weaponnum_watchmenu);
	g_CurrentPlayer->hands[hand].weaponnum_watchmenu = weaponnum_watchmenu;
}

void sub_GAME_7F05DAE4(GUNHAND hand) {
    if (g_CurrentPlayer->hands[hand].weaponnum_watchmenu >= 0) {
        place_item_in_hand_swap_and_make_visible(hand, g_CurrentPlayer->hands[hand].weaponnum);
		g_CurrentPlayer->hands[hand].weaponnum_watchmenu = -1;
    }
}


void currentPlayerUnEquipWeaponWrapper(enum GUNHAND hand, enum ITEM_IDS weapid)
{
    enum ITEM_IDS weapon_num;
    s32 ammo_type;

    weapon_num = g_CurrentPlayer->hands[hand].weaponnum;
    ammo_type = get_ammo_type_for_weapon(weapon_num);

    if (g_CurrentPlayer->hands[hand].weaponnum_watchmenu < 0)
    {
        place_item_in_hand_swap_and_make_visible(hand, weapid);
    }

    if (g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine > 0)
    {
        g_CurrentPlayer->ammoheldarr[ammo_type] += g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine;
    }

    if (weapon_num < ITEM_BOMBCASE)
    {
        g_CurrentPlayer->hands[hand].previous_weapon = weapon_num;
    }

    if (getPlayerCount() >= 2)
    {
        sub_GAME_7F09B368(hand);
    }

    sub_GAME_7F05FB00(hand);
    g_CurrentPlayer->hands[hand].weaponnum = weapid;
    g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine = 0;
    g_CurrentPlayer->hands[hand].field_A4C = 0;
    g_CurrentPlayer->hands[hand].field_A50 = 0;
    bondinvDetermineEquippedItem();
}


s8 get_hands_firing_status(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weapon_firing_status;
}

f32 sub_GAME_7F05DCB8(GUNHAND hand) {
	return g_CurrentPlayer->hands[hand].field_A34;
}

/**
 * Positions the gun to the right or left side of the screen depending on which hand is holding it.
 */
f32 gunSetHorizontalOffset(GUNHAND hand)
{
	f32 offset;

	if (hand == GUNRIGHT)
	{
		offset = get_ptr_item_statistics(get_item_in_hand_or_watch_menu(GUNRIGHT))->PosX;
	}
	else
	{
		offset = -get_ptr_item_statistics(get_item_in_hand_or_watch_menu(GUNLEFT))->PosX;
	}

	return offset;
}

f32 get_item_in_hand_zoom(void) {
    if (get_item_in_hand_or_watch_menu(GUNRIGHT) == ITEM_SNIPERRIFLE) {
        return g_CurrentPlayer->sniper_zoom;
    }
    if (get_item_in_hand_or_watch_menu(GUNRIGHT) == ITEM_CAMERA) {
        return g_CurrentPlayer->camera_zoom;
    }
    return get_ptr_item_statistics(get_item_in_hand_or_watch_menu(GUNRIGHT))->Zoom;
}

void camera_sniper_zoom_out(f32 zoom)
{
	if (get_item_in_hand_or_watch_menu(GUNRIGHT) == ITEM_SNIPERRIFLE) {
		g_CurrentPlayer->sniper_zoom *= (1.0f + (zoom * 0.1f));
		if (g_CurrentPlayer->sniper_zoom > 60.0f) {
			g_CurrentPlayer->sniper_zoom = 60.0f;
		}
	}
	else
	{
		if (get_item_in_hand_or_watch_menu(GUNRIGHT) == ITEM_CAMERA) {
			g_CurrentPlayer->camera_zoom *= (1.0f + (zoom * 0.1f));
			if (g_CurrentPlayer->camera_zoom > 60.0f) {
				g_CurrentPlayer->camera_zoom = 60.0f;
			}
		}
	}
}

void camera_sniper_zoom_in(f32 zoom)
{
	if (get_item_in_hand_or_watch_menu(GUNRIGHT) == ITEM_SNIPERRIFLE) {
		g_CurrentPlayer->sniper_zoom /= (1.0f + (zoom * 0.1f));
		if (g_CurrentPlayer->sniper_zoom < 7.0f) {
			g_CurrentPlayer->sniper_zoom = 7.0f;
		}
	}
	else
	{
		if (get_item_in_hand_or_watch_menu(GUNRIGHT) == ITEM_CAMERA) {
			g_CurrentPlayer->camera_zoom /= (1.0f + (zoom * 0.1f));
			if (g_CurrentPlayer->camera_zoom < 7.0f) {
				g_CurrentPlayer->camera_zoom = 7.0f;
			}
		}
	}
}

f32 gunItemGetDestructionAmount(ITEM_IDS item)
{
  return get_ptr_item_statistics(item)->DestructionAmount;
}


f32 bondwalkItemGetForceOfImpact(ITEM_IDS item)
{
	return get_ptr_item_statistics(item)->ForceOfImpact;
}

/**
 * Address 0x7F05DFCC
 */
s8 bondwalkItemGetAutomaticFiringRate(ITEM_IDS item) {
    return get_ptr_item_statistics(item)->AutomaticFiringRate;
}


u8 bondwalkItemGetSoundTriggerRate(ITEM_IDS item) {
    return get_ptr_item_statistics(item)->SoundTriggerRate;
}


u16 bondwalkItemGetSound(ITEM_IDS item)
{
  return get_ptr_item_statistics(item)->Sound;
}


u8 bondwalkItemGetObjectsShootThrough(ITEM_IDS item)
{
  return get_ptr_item_statistics(item)->ObjectsShootThrough;
}


s32 bondwalkItemHasAmmo(ITEM_IDS item)
{
    if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_HAS_AMMO) != 0)
    {
        if ((get_ammo_type_for_weapon(item) == 0) || (get_ammo_count_for_weapon(item) > 0))
        {
            return 1;
        }
    }
    return 0;
}


u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask)
{
  return ((get_ptr_item_statistics(item)->BitFlags & mask) != 0);
}


void gunSetBondWeaponSway(f32 breathing, f32 arg1, f32 arg2, f32 arg3)
{
    f32 dampt[2];
    s32 i;
    u32 unused[2];
    f32 sp50 = arg2;
    f32 sp4c;
    u32 stack2;
    f32 minbreathing;

    if (sp50 < 0.0f) { sp50 = -sp50; }

    if (arg1 > 0.8f)
    {
        g_CurrentPlayer->gunposamplitude = 1.0f;
    }
    else
    {
        if (arg1 > 0.1f)
        {
            f32 tmp = (1.0f - cosf((arg1 - 0.1f) * M_TAU_F / 2.8f));
            g_CurrentPlayer->gunposamplitude = 0.8f * tmp + 0.2f;
        }
        else
        {
            g_CurrentPlayer->gunposamplitude = 0.1f;
        }
    }

    if (g_CurrentPlayer->gunposamplitude < (bondviewGetBondBreathing() * 0.3f))
    {
        g_CurrentPlayer->gunposamplitude = bondviewGetBondBreathing() * 0.3f;
    }

    if (g_CurrentPlayer->gunposamplitude < 0.5f * sp50)
    {
        g_CurrentPlayer->gunposamplitude = 0.5f * sp50;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->field_1080 = (g_CurrentPlayer->field_1080 * (PAL ? 0.9403f : 0.95f)) + g_CurrentPlayer->gunposamplitude;
    }

    g_CurrentPlayer->gunposamplitude = g_CurrentPlayer->field_1080 * (PAL ? 0.059700012f : 0.050000012f);

    minbreathing = 0.016666668f * sp50;
    if (breathing < minbreathing)
    {
        breathing = minbreathing;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->field_107C = (g_CurrentPlayer->field_107C * (PAL ? 0.9403f : 0.95f)) + breathing;
    }

    breathing = g_CurrentPlayer->field_107C * (PAL ? 0.059700012f : 0.050000012f);

    sp4c = breathing * g_GlobalTimerDelta;
    dampt[0] = g_CurrentPlayer->hands[0].dampt + sp4c;

    while (dampt[0] >= 1.0f)
    {
        bgunCalculateBlend(GUNRIGHT);
        dampt[0] -= 1.0f;
        g_CurrentPlayer->syncoffset++;
    }

    g_CurrentPlayer->synccount += g_GlobalTimerDelta;

    if (g_CurrentPlayer->synccount > 60.0f)
    {
        g_CurrentPlayer->synccount = 0.0f;
        g_CurrentPlayer->syncchange = (RANDOMFRAC() - 0.5f) * 0.2f / 60.0f;
    }

    if (g_CurrentPlayer->syncchange + sp4c > 0.0f)
    {
        g_CurrentPlayer->gunsync += g_CurrentPlayer->syncchange;
    }

    if (g_CurrentPlayer->gunsync > 0.5f)
    {
        g_CurrentPlayer->gunsync = 0.5f;
    }
    else if (g_CurrentPlayer->gunsync < -0.5f)
    {
        g_CurrentPlayer->gunsync = -0.5f;
    }
    else if (g_CurrentPlayer->gunsync < 0.1f && g_CurrentPlayer->gunsync > -0.1f)
    {
        if (g_CurrentPlayer->gunsync > 0.0f)
        {
            g_CurrentPlayer->gunsync = -0.1f;
        }
        else
        {
            g_CurrentPlayer->gunsync = 0.1f;
        }
    }

    dampt[1] = dampt[0] + g_CurrentPlayer->syncoffset + g_CurrentPlayer->gunsync;

    while (dampt[1] >= 1.0f)
    {
        bgunCalculateBlend(GUNLEFT);
        dampt[1] -= 1.0f;
        g_CurrentPlayer->syncoffset--;
    }

    for (i = 0; i < 2; i++)
    {
        g_CurrentPlayer->hands[i].dampt = dampt[i];
        g_CurrentPlayer->hands[i].weapon_theta_displacement = -1.75f * arg3;
        g_CurrentPlayer->hands[i].weapon_verta_displacement = -2.0f * arg2;
    }
}


void gunSetOffsetRelated(f32 param_1)
{
    g_CurrentPlayer->hands[GUNRIGHT].gunofs2_z = (1.0f - cosf(param_1)) * 5.0f;
    g_CurrentPlayer->hands[GUNLEFT].gunofs2_z = (1.0f - cosf(param_1)) * 5.0f;
}


f32 get_value_if_watch_is_on_hand_or_not(GUNHAND hand)
{
  if ((getCurrentPlayerWeaponId(hand) == ITEM_TRIGGER) || (getCurrentPlayerWeaponId(hand) == ITEM_WATCHLASER))
  {
    return 0.08726647f;
  }
  else
  {
    return 0.17453294f;
  }
}


void sub_GAME_7F05E6B4(enum GUNHAND hand, s32 arg1)
{
    if (arg1 != 0)
    {
        if (g_CurrentPlayer->hands[hand].field_A84 < get_value_if_watch_is_on_hand_or_not(hand))
        {
            g_CurrentPlayer->hands[hand].field_A84 += (0.029088823f * g_GlobalTimerDelta);
        }
        if (g_CurrentPlayer->hands[hand].field_A84 > get_value_if_watch_is_on_hand_or_not(hand)) {
            g_CurrentPlayer->hands[hand].field_A84 = get_value_if_watch_is_on_hand_or_not(hand);
        }
    }
    else
    {
        if (g_CurrentPlayer->hands[hand].field_A84 > 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A84 -= (0.017453294f * g_GlobalTimerDelta);
        }
        if (g_CurrentPlayer->hands[hand].field_A84 < 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A84 = 0.0f;
        }
    }
}


void sub_GAME_7F05E808(GUNHAND hand) {
	g_CurrentPlayer->hands[hand].field_A8C = 1;
}


void sub_GAME_7F05E83C(GUNHAND hand)
{
    f32 recoil_back;

    recoil_back = get_ptr_item_statistics(get_item_in_hand_or_watch_menu(hand))->BoltRecoilBack;

    if (g_CurrentPlayer->hands[hand].field_A8C != 0)
    {
        if (g_CurrentPlayer->hands[hand].field_A88 < recoil_back)
        {
            g_CurrentPlayer->hands[hand].field_A88 = (g_CurrentPlayer->hands[hand].field_A88 + (recoil_back * 0.25f * g_GlobalTimerDelta));

        }
        if (recoil_back <= g_CurrentPlayer->hands[hand].field_A88) {
            g_CurrentPlayer->hands[hand].field_A88 = recoil_back;
            g_CurrentPlayer->hands[hand].field_A8C = 0;
        }
    }
    else if (g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine > 0)
    {
        if (g_CurrentPlayer->hands[hand].field_A88 > 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A88 = (g_CurrentPlayer->hands[hand].field_A88 - (recoil_back * 0.16666667f * g_GlobalTimerDelta));

        }
        if (g_CurrentPlayer->hands[hand].field_A88 < 0.0f)
        {
            g_CurrentPlayer->hands[hand].field_A88 = 0.0f;
        }
    }
}


void sub_GAME_7F05E978(Model* model, s32 val)
{
    if (model->obj->Switches[8] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[8])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[9] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[9])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[10] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[10])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[11] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[11])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[12] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[12])->DisplayList.unk00 = val;
    }

    if (model->obj->Switches[13] != NULL)
    {
        modelGetNodeRwData(model, model->obj->Switches[13])->DisplayList.unk00 = val;
    }

    if (model->obj->numSwitches >= 0x24)
    {
        if (model->obj->Switches[35] != NULL)
        {
            modelGetNodeRwData(model, model->obj->Switches[35])->DisplayList.unk00 = val;
        }
    }
}


void sub_GAME_7F05EA94(Model* model, s32 val)
{
    ModelNode* switch_14;
    ModelNode* switch_15;

    if (model->obj->numSwitches >= 0x10)
    {
        switch_14 = model->obj->Switches[14];
        if (switch_14 != NULL)
        {
            // Guessing DisplayList here
            modelGetNodeRwData(model, switch_14)->DisplayList.unk00 = val;
        }

        switch_15 = model->obj->Switches[15];
        if (switch_15 != NULL)
        {
            // Guessing DisplayList here
            modelGetNodeRwData(model, switch_15)->DisplayList.unk00 = val;
        }
    }
}


/**
 * Address 0x7F05EB0C.
*/
void gunInitProjectileObject(ObjectRecord *obj, coord3d *pos, StandTile *stan, Mtxf *matrix, coord3d *velocity, Mtxf *arg5, PropRecord *owner)
{
    PropRecord *temp_s1;
    Projectile *temp_v0;

    temp_s1 = obj->prop;

    if (temp_s1 != NULL)
    {
        chrpropActivate(temp_s1);
        chrpropEnable(temp_s1);
        matrix_scalar_multiply(obj->model->scale, matrix);
        objChangeShading(obj, pos, matrix, stan);

        // loadobjectmodel.c
        setupUpdateObjectRoomPosition(obj);

        chrobjCollisionRelated(obj);
        sub_GAME_7F03FDA8(temp_s1);

        if (obj->runtime_bitflags & RUNTIMEBITFLAG_DEPOSIT)
        {
            temp_v0 = obj->projectile;
            temp_v0->flags |= 0x41;
            obj->projectile->ownerprop = owner;
            projectileSetSticky(temp_s1);
            matrix_4x4_copy(arg5, &obj->projectile->mtx);
            obj->projectile->speed.f[0] = velocity->f[0];
            obj->projectile->speed.f[1] = velocity->f[1];
            obj->projectile->speed.f[2] = velocity->f[2];
            obj->projectile->obj = obj;
            obj->projectile->unkE8 = D_80048380;
        }
    }
}


/**
 * Address: 7F05EC1C
 *
 * Determines where the projectile may safely enter the world. Ideally that is the targetpos position, but if targetpos
 * is obstructed the player's position used as a fallback. This prevents the player from launching
 * projectiles through nearby surfaces.
 */
void gunInitProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *arg2, coord3d *velocity, Mtxf *arg4)
{
    PropRecord *playerprop;
    coord3d pos;
    StandTile *tile;
    u32 pad_c[2];
    f32 yhi;
    f32 ylo;
    s32 usedfallback;
    f32 stanheight;
    u8 rooms[2];
    s32 pad_rooms;
    u8 pad_a[0x4c];
    s32 sp54;
    s32 sp50;
    s32 pad_sp;

    // fake
    if (obj->prop);
    if (obj->prop == NULL) {
        return;
    }

    playerprop = getCurrentPlayerProp();
    stanheight = bondviewGetPlayerStanHeight(g_CurrentPlayer);

    usedfallback = 0;

    if (targetpos->y < playerprop->pos.y) {
        yhi = playerprop->pos.y - stanheight;
        ylo = targetpos->y - stanheight;
    }
    else
    {
        yhi = targetpos->y - stanheight;
        ylo = playerprop->pos.y - stanheight;
    }

    tile = playerprop->stan;
    bondviewUpdateGuardTankFlagsRelated(playerprop, 0);

    // If there is no obstruction, spawn the projectile at the target position.
    if (stanTestLineUnobstructed(&tile, playerprop->pos.x, playerprop->pos.z, targetpos->x, targetpos->z, 0x1f, yhi, ylo, 0.0f, 1.0f))
    {
        pos.x = targetpos->x;
        pos.y = targetpos->y;
        pos.z = targetpos->z;
    }
    // Otherwise spawn it from the player's position.
    else
    {
        tile = playerprop->stan;
        pos.x = playerprop->pos.x;
        pos.y = playerprop->pos.y;
        pos.z = playerprop->pos.z;
        usedfallback = 1;
    }

    bondviewUpdateGuardTankFlagsRelated(playerprop, 1);

    gunInitProjectileObject(obj, &pos, tile, arg2, velocity, arg4, playerprop);

    if (obj->runtime_bitflags & 0x80) {
        if (usedfallback) {
            obj->projectile->flags |= PROJECTILEFLAG_00000100;
            ((coord3d *)&obj->projectile->unkd4)->x = targetpos->x;
            ((coord3d *)&obj->projectile->unkd4)->y = targetpos->y;
            ((coord3d *)&obj->projectile->unkd4)->z = targetpos->z;
        }

        rooms[0] = bondviewGetCurrentPlayersRoom();
        rooms[1] = 0xff;

        sub_GAME_7F0B4AB4(bondviewGetCurrentPlayersPosition3(), &pos, rooms, obj->projectile->unkCC, &sp54, &sp50, 0x14);
    }
}


/**
 * Address 0x7F05EE24 (NTSC)
 * Address 0x7F05F2DC (PAL)
*/
void generate_player_thrown_grenade(s32 hand)
{
    s32 padding;
    Mtxf spFC;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d spE0;
    Mtxf spA0_a;
    struct WeaponObjRecord *wor;
    s32 new_prop_type;
    s32 sp94; // sp148
    struct coord3d base_speed_vec; // sp136
    struct PropRecord* player_prop; // sp132
    struct coord3d *bondprevpos;  // sp128
    Mtxf sp40_f;
    ALSoundState *sfx_state;
    s32 current_weapon;
    s32 unused;

    wor = NULL;
    base_velocity = 16.666666f;

    player_prop = getCurrentPlayerProp();
    bondprevpos = getCurrentPlayerPrevPos();
    current_weapon = getCurrentPlayerWeaponId(hand);

    sub_GAME_7F057C14(&throw_speed_vec, &spFC);
    bullet_path_from_screen_center(&sp94, &base_speed_vec, hand);
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), (f32*)&base_speed_vec);

    throw_speed_vec.f[0] = (base_speed_vec.f[0] * base_velocity);
    throw_speed_vec.f[1] = (base_speed_vec.f[1] * base_velocity) + 5.0f;
    throw_speed_vec.f[2] = (base_speed_vec.f[2] * base_velocity);

    if (g_ClockTimer > 0)
    {
        throw_speed_vec.f[0] = ((player_prop->pos.f[0] - bondprevpos->f[0]) / g_GlobalTimerDelta) + throw_speed_vec.f[0];
        throw_speed_vec.f[1] = ((player_prop->pos.f[1] - bondprevpos->f[1]) / g_GlobalTimerDelta) + throw_speed_vec.f[1];
        throw_speed_vec.f[2] = ((player_prop->pos.f[2] - bondprevpos->f[2]) / g_GlobalTimerDelta) + throw_speed_vec.f[2];
    }

    spE0.f[0] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][0];
    spE0.f[1] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][1];
    spE0.f[2] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][2];

    matrix_4x4_set_identity(&spA0_a);
    matrix_4x4_copy(&g_CurrentPlayer->hands[hand].throw_item_pos_related, &sp40_f);
    sp40_f.m[3][0] = 0.0f;
    sp40_f.m[3][1] = 0.0f;
    sp40_f.m[3][2] = 0.0f;
    matrix_4x4_multiply_in_place(&sp40_f, &spA0_a);

    wor = create_new_item_instance_of_model(PROP_CHRGRENADE, current_weapon);

    if (wor != NULL)
    {
        wor->timer = THROWN_ITEM_TIMER_DEFAULT - g_CurrentPlayer->last_z_trigger_timer;

        if (wor->timer < 0)
        {
            wor->timer = 0;
        }

        wor->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
        wor->runtime_bitflags |= get_cur_playernum() << RUNTIMEBITSHIFT_OWNER;

        gunInitProjectileFromPlayer(wor, &spE0, &spA0_a, &throw_speed_vec, &spFC);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_DEPOSIT) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->unk8C = 0.3f;
            wor->projectile->unk94 = 0.13333333f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            sfx_state = sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GRENADE_THROW_SFX, NULL);

            if (sfx_state != NULL)
            {
                chrobjSndCreatePostEventDefault(sfx_state, (struct coord3d *) &wor->runtime_pos);
            }
        }
    }
}


/**
 * Address 0x7F05F09C (NTSC)
 * Address 0x7F05F554 (PAL)
*/
void generate_player_thrown_knife(s32 hand)
{
    struct WeaponObjRecord *wor;
    Mtxf spFC;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d spE0;
    Mtxf spA0_a;
    s32 padding;
    s32 new_prop_type;
    s32 sp94;
    struct coord3d base_speed_vec;
    Mtxf sp40_f;
    struct PropRecord* player_prop;
    struct coord3d *bondprevpos;

    wor = NULL;
    base_velocity = 25.0f;

    player_prop = getCurrentPlayerProp();
    bondprevpos = getCurrentPlayerPrevPos();

    sub_GAME_7F057C14(&throw_speed_vec, &spFC);
    bullet_path_from_screen_center(&sp94, &base_speed_vec, hand);
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), (f32*)&base_speed_vec);

    throw_speed_vec.f[0] = (base_speed_vec.f[0] * base_velocity);
    throw_speed_vec.f[1] = (base_speed_vec.f[1] * base_velocity) + 5.0f;
    throw_speed_vec.f[2] = (base_speed_vec.f[2] * base_velocity);

    if (g_ClockTimer > 0)
    {
        throw_speed_vec.f[0] = ((player_prop->pos.f[0] - bondprevpos->f[0]) / g_GlobalTimerDelta) + throw_speed_vec.f[0];
        throw_speed_vec.f[1] = ((player_prop->pos.f[1] - bondprevpos->f[1]) / g_GlobalTimerDelta) + throw_speed_vec.f[1];
        throw_speed_vec.f[2] = ((player_prop->pos.f[2] - bondprevpos->f[2]) / g_GlobalTimerDelta) + throw_speed_vec.f[2];
    }

    spE0.f[0] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][0];
    spE0.f[1] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][1];
    spE0.f[2] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][2];

    matrix_4x4_set_rotation_around_z(4.712389f, &spA0_a);
    matrix_4x4_set_rotation_around_x(M_PI_F, &sp40_f);
    matrix_4x4_multiply_in_place(&sp40_f, &spA0_a);
    matrix_4x4_copy(&g_CurrentPlayer->hands[hand].throw_item_pos_related, &sp40_f);

    sp40_f.m[3][0] = 0.0f;
    sp40_f.m[3][1] = 0.0f;
    sp40_f.m[3][2] = 0.0f;
    matrix_4x4_multiply_in_place(&sp40_f, &spA0_a);

    guRotateF(&spFC, 360.0f / ((randomGetNext() * (0.5f / (f32)INT_MAX)) + 12.1f), spA0_a.m[1][0], spA0_a.m[1][1], spA0_a.m[1][2]);

    wor = create_new_item_instance_of_model(PROP_CHRKNIFE, ITEM_THROWKNIFE);

    if (wor != NULL)
    {
        wor->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
        wor->runtime_bitflags |= get_cur_playernum() << RUNTIMEBITSHIFT_OWNER;

        gunInitProjectileFromPlayer(wor, &spE0, &spA0_a, &throw_speed_vec, &spFC);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_DEPOSIT) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->unk8C = 0.1f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            wor->runtime_bitflags |= RUNTIMEBITFLAG_THROWING_KNIFE_RELATED;
        }

        sub_GAME_7F043650(wor);
    }
}





/**
 * Address 0x7F05F358 (NTSC)
 * Address 0x7F05F810 (PAL)
*/
void generate_player_thrown_object(s32 hand)
{
/*
    else {
        assertPrint_8291E690(".\\ported\\gun.cpp",0x8df,"throwmineremote - Not a mine!");
    }
*/

    s32 padding;
    Mtxf unk_mtxf;
    struct coord3d throw_speed_vec;
    f32 base_velocity;
    struct coord3d spE0;
    Mtxf spA0_a;
    struct WeaponObjRecord *wor;
    s32 new_prop_type;
    s32 sp94; // sp148
    struct coord3d base_speed_vec; // sp136
    struct PropRecord* player_prop; // sp132
    struct coord3d *bondprevpos;  // sp128
    Mtxf sp40_f;
    ALSoundState *sfx_state;
    s32 current_weapon;
    s32 unused;

    wor = NULL;
    base_velocity = 16.666666f;

    player_prop = getCurrentPlayerProp();
    bondprevpos = getCurrentPlayerPrevPos();
    current_weapon = getCurrentPlayerWeaponId(hand);

    if (current_weapon == ITEM_GOLDENEYEKEY)
    {
        base_velocity = 6.6666665f;
    }

    sub_GAME_7F057C14(&throw_speed_vec, &unk_mtxf);
    bullet_path_from_screen_center(&sp94, &base_speed_vec, hand);
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), (f32*)&base_speed_vec);

    throw_speed_vec.f[0] = (base_speed_vec.f[0] * base_velocity);
    throw_speed_vec.f[1] = (base_speed_vec.f[1] * base_velocity) + 5.0f;
    throw_speed_vec.f[2] = (base_speed_vec.f[2] * base_velocity);

    if (g_ClockTimer > 0)
    {
        throw_speed_vec.f[0] = ((player_prop->pos.f[0] - bondprevpos->f[0]) / g_GlobalTimerDelta) + throw_speed_vec.f[0];
        throw_speed_vec.f[1] = ((player_prop->pos.f[1] - bondprevpos->f[1]) / g_GlobalTimerDelta) + throw_speed_vec.f[1];
        throw_speed_vec.f[2] = ((player_prop->pos.f[2] - bondprevpos->f[2]) / g_GlobalTimerDelta) + throw_speed_vec.f[2];
    }

    spE0.f[0] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][0];
    spE0.f[1] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][1];
    spE0.f[2] = g_CurrentPlayer->hands[hand].throw_item_pos_related.m[3][2];

    matrix_4x4_set_identity(&spA0_a);
    matrix_4x4_copy(&g_CurrentPlayer->hands[hand].throw_item_pos_related, &sp40_f);
    sp40_f.m[3][0] = 0.0f;
    sp40_f.m[3][1] = 0.0f;
    sp40_f.m[3][2] = 0.0f;
    matrix_4x4_multiply_in_place(&sp40_f, &spA0_a);

    if (current_weapon == ITEM_GOLDENEYEKEY)
    {
        wor = bondinvRemovePropWeaponByID(current_weapon);
        bondinvRemoveItemByID(current_weapon);

        if (wor != NULL)
        {
            objDetach(wor->prop);
        }

        sub_GAME_7F05D690();
    }

    if (wor == NULL)
    {
        new_prop_type = PROP_CHRREMOTEMINE;

        switch (current_weapon)
        {
        case ITEM_PROXIMITYMINE:
            new_prop_type = PROP_CHRPROXIMITYMINE;
            break;
        case ITEM_TIMEDMINE:
            new_prop_type = PROP_CHRTIMEDMINE;
            break;
        case ITEM_BOMBCASE:
            new_prop_type = PROP_CHRBOMBCASE;
            break;
        case ITEM_BUG:
            new_prop_type = PROP_CHRBUG;
            break;
        case ITEM_MICROCAMERA:
            new_prop_type = PROP_CHRMICROCAMERA;
            break;
        case ITEM_GOLDENEYEKEY:
            new_prop_type = PROP_CHRGOLDENEYEKEY;
            break;
        case ITEM_PLASTIQUE:
            new_prop_type = PROP_CHRPLASTIQUE;
            break;
#ifdef DEBUG
        default:
            assertmsg2(current_weapon = PROP_CHRREMOTEMINE, "throwmineremote - Not a mine!");
#endif

        }

        wor = create_new_item_instance_of_model(new_prop_type, current_weapon);
    }

    if (wor != NULL)
    {
        switch (current_weapon)
        {
            case ITEM_REMOTEMINE:
            if (getPlayerCount() == 1)
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_PROXIMITYMINE:
            if (getPlayerCount() == 1)
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_TIMEDMINE:
            if (getPlayerCount() == 1)
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_BOMBCASE:
            if (getPlayerCount() == 1)
            {
                wor->timer = THROWN_ITEM_TIMER_SOLO;
            }
            else
            {
                wor->timer = THROWN_ITEM_TIMER_MULTI;
            }
            break;

            case ITEM_PLASTIQUE:
            case ITEM_BUG:
            case ITEM_MICROCAMERA:
            case ITEM_GOLDENEYEKEY:
                wor->timer = 1;
            break;

            default:
                wor->timer = THROWN_ITEM_TIMER_DEFAULT;
            break;
        }

        wor->runtime_bitflags &= ~(RUNTIMEBITFLAG_OWNER);
        wor->runtime_bitflags |= get_cur_playernum() << RUNTIMEBITSHIFT_OWNER;

        gunInitProjectileFromPlayer(wor, &spE0, &spA0_a, &throw_speed_vec, &unk_mtxf);

        if ((wor->runtime_bitflags & RUNTIMEBITFLAG_DEPOSIT) != 0)
        {
            wor->projectile->flags = (s32) (wor->projectile->flags | 2);

            wor->projectile->unk8C = 0.1f;
            wor->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            sfx_state = sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GRENADE_THROW_SFX, NULL);

            if (sfx_state != NULL)
            {
                chrobjSndCreatePostEventDefault(sfx_state, (struct coord3d *) &wor->runtime_pos);
            }
        }
    }
}


/**
 * Address: 7F05F73C
 *
 * Spawns Grenade Launcher rounds and makes them inherit the player's momentum.
 */
void gunSpawnGLGrenade(s32 handnum)
{
    WeaponObjRecord *grenadeobj;
    struct hand *hand;
    Mtxf identitymtx;
    coord3d launchvel;
    s32 pad;
    Mtxf launchmtx;
    coord3d aimpos;
    coord3d aimdir;
    PropRecord *playerprop;
    coord3d *prevplayerpos;

    hand = &g_CurrentPlayer->hands[handnum];

    playerprop = getCurrentPlayerProp();
    prevplayerpos = getCurrentPlayerPrevPos();

    matrix_4x4_set_identity(&identitymtx);
    bullet_path_from_screen_center(&aimpos, &aimdir, handnum);
    mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &aimdir);

    launchvel.x = aimdir.x * 33.333332f;
    launchvel.y = aimdir.y * 33.333332f;
    launchvel.z = aimdir.z * 33.333332f;

    if (g_ClockTimer > 0)
    {
        launchvel.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
        launchvel.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
        launchvel.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
    }

    matrix_4x4_copy(&g_CurrentPlayer->hands[handnum].throw_item_pos_related, &launchmtx);

    launchmtx.m[3][0] = 0.0f;
    launchmtx.m[3][1] = 0.0f;
    launchmtx.m[3][2] = 0.0f;

    grenadeobj = create_new_item_instance_of_model(PROP_CHRGRENADEROUND, ITEM_GRENADEROUND);

    if (grenadeobj != NULL)
    {
        grenadeobj->timer = GLGRENADE_TIMER;
        grenadeobj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
        grenadeobj->runtime_bitflags |= get_cur_playernum() << RUNTIMEBITSHIFT_OWNER;

        gunInitProjectileFromPlayer(grenadeobj, &hand->field_B58, &launchmtx, &launchvel, (s32 *)&identitymtx);

        if (grenadeobj->runtime_bitflags & RUNTIMEBITFLAG_00000080)
        {
            grenadeobj->projectile->unk8C = 0.3f;
            grenadeobj->projectile->unk94 = 0.13333333f;
            grenadeobj->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;
        }
    }
}


/**
 * Address: 0x7F05F928
 * This function is responsible for attaching a rocket to the end of the Rocket Launcher and updating its matrices.
 */
void gunUpdateAttachedRocket(s32 handIndex)
{
    struct hand *entry;
    AttachedObj *attachedRocket;
    Model *rocketModel;
    Mtxf worldMtx;
    PropRecord *prop;
    AttachmentChild *attachmentChild;

    entry = &g_CurrentPlayer->hands[handIndex];

    attachedRocket = entry->rocket;

    if (attachedRocket == NULL)
    {
        return;
    }

    attachmentChild = attachedRocket->child;

    if (attachmentChild == NULL)
    {
        return;
    }

    prop = getCurrentPlayerProp();
    rocketModel = attachedRocket->model;

    matrix_4x4_copy(&entry->throw_item_pos_related, &worldMtx);

    worldMtx.m[3][0] = 0.0f;
    worldMtx.m[3][1] = 0.0f;
    worldMtx.m[3][2] = 0.0f;

    matrix_scalar_multiply(attachedRocket->model->scale, (f32 *)&worldMtx);

    objChangeShading(attachedRocket, &entry->field_B58, &worldMtx, prop->stan);
    chrobjCollisionRelated(attachedRocket);

    rocketModel->render_pos = dynAllocate((s32)rocketModel->obj->numMatrices << 6);

    matrix_4x4_copy(&attachedRocket->transform, &worldMtx);
    matrix_4x4_set_position((Mtxf *)&attachedRocket->position, &worldMtx);

    matrix_4x4_multiply_homogeneous(camGetWorldToScreenMtxf(), &worldMtx, rocketModel->render_pos);
    modelUpdateRelationsQuick(rocketModel, rocketModel->obj->RootNode);

    attachmentChild->flags1 |= 2;
    attachmentChild->unk18 = -rocketModel->render_pos->pos.m[3][2];
}


/*
* Address: 0x7f05fa7c
*/
void currentPlayerCreateRocket(GUNHAND hand)
{
    struct hand * hand_ptr;
    struct WeaponObjRecord * rocket;

    hand_ptr = &g_CurrentPlayer->hands[hand];

    if ((hand_ptr->rocket == NULL) && (hand_ptr->weapon_ammo_in_magazine > 0))
    {
        rocket = (struct WeaponObjRecord *)create_new_item_instance_of_model(PROP_CHRROCKET, ITEM_ROCKETROUND);

        if (rocket != NULL)
        {
            hand_ptr->rocket = (ObjectRecord *)rocket;
            hand_ptr->firedrocket = 0;
            rocket->timer = 1;
        }
    }
}


/*
* Address: 0x7F05FB00
* This function frees some sort of ObjectRecord from the given hand
*/
void sub_GAME_7F05FB00(enum GUNHAND hand)
{
    struct hand* hand_ptr;
    ObjectRecord* hand_obj_record;

    hand_ptr = &g_CurrentPlayer->hands[hand];
    hand_obj_record = hand_ptr->rocket;

    if (hand_obj_record != NULL)
    {
        objFreePermanently(hand_obj_record, 1);
        hand_ptr->rocket = NULL;
    }
}


extern f32 D_80053DD8;
extern f32 D_80053DDC;

/*
* Address: 0x7F05FB64
*/
void gunFireTankShell(s32 handnum)
{
    WeaponObjRecord *obj;
    struct hand *hand;
    Mtxf identitymtx;
    coord3d velocity;
    ObjectRecord *tankobj;
    coord3d unscaledvelocity;
    Mtxf shellmtx;
    coord3d screenpos;
    coord3d aimdir;
    PropRecord *playerprop;
    coord3d *prevplayerpos;
    ITEM_IDS weaponid;
    coord3d spawnpos;
    PropRecord *tankprop;

    hand = &g_CurrentPlayer->hands[handnum];

    playerprop = getCurrentPlayerProp();
    prevplayerpos = getCurrentPlayerPrevPos();
    weaponid = getCurrentPlayerWeaponId(handnum);

    matrix_4x4_set_identity(&identitymtx);

    if (weaponid == ITEM_TANKSHELLS) {
        tankprop = get_ptr_for_players_tank();

        if (1);

        if ((tankprop != NULL) && (tankprop->flags & TANK_RUN_STATE_RUNNING)) {
            bondviewSet3dCoord7F07CEB0(&aimdir);
        } else {
            sub_GAME_7F068190(&screenpos, &aimdir);
            mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &aimdir);
        }

        velocity.x = aimdir.x * D_80053DD8;
        velocity.y = aimdir.y * D_80053DD8;
        velocity.z = aimdir.z * D_80053DD8;

        if (g_ClockTimer > 0) {
            velocity.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
            velocity.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
            velocity.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
        }

        if ((tankprop != NULL) && (tankprop->flags & TANK_RUN_STATE_RUNNING)) {
            tankobj = tankprop->obj;
            spawnpos.x = tankobj->model->render_pos[4].pos.m[3][0];
            spawnpos.y = tankobj->model->render_pos[4].pos.m[3][1];
            spawnpos.z = tankobj->model->render_pos[4].pos.m[3][2];

            mtx4TransformVecInPlace(currentPlayerGetViewToWorldMtxf(), &spawnpos);
        } else {
            spawnpos.x = playerprop->pos.x;
            spawnpos.y = playerprop->pos.y;
            spawnpos.z = playerprop->pos.z;
        }

        if ((g_CurrentPlayer && g_CurrentPlayer));

        setSixExplosionAndSmokeEntries();
    } else {
        bullet_path_from_screen_center(&screenpos, &aimdir, handnum);
        mtx4RotateVecInPlace(currentPlayerGetViewToWorldMtxf(), &aimdir);

        spawnpos.x = hand->field_B58.x;
        spawnpos.y = hand->field_B58.y;
        spawnpos.z = hand->field_B58.z;

        if (1);

        unscaledvelocity.x = aimdir.x * D_80053DDC;
        unscaledvelocity.y = aimdir.y * D_80053DDC;
        unscaledvelocity.z = aimdir.z * D_80053DDC;

        velocity.x = unscaledvelocity.x * g_GlobalTimerDelta;
        velocity.y = unscaledvelocity.y * g_GlobalTimerDelta;
        velocity.z = unscaledvelocity.z * g_GlobalTimerDelta;

        if (g_ClockTimer > 0) {
            velocity.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
            velocity.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
            velocity.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
        }
    }

    matrix_4x4_copy(&g_CurrentPlayer->hands[handnum].throw_item_pos_related, &shellmtx);

    shellmtx.m[3][0] = 0.0f;
    shellmtx.m[3][1] = 0.0f;
    shellmtx.m[3][2] = 0.0f;

    if (hand->rocket != NULL) {
        obj = (WeaponObjRecord *) hand->rocket;
        hand->firedrocket = 1;
    } else {
        obj = (WeaponObjRecord *) create_new_item_instance_of_model(PROP_CHRROCKET, ITEM_ROCKETROUND);
    }

    if (obj == NULL) {
        return;
    }

    obj->timer = -1;
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
    obj->runtime_bitflags |= get_cur_playernum() << RUNTIMEBITSHIFT_OWNER;

    gunInitProjectileFromPlayer(obj, &spawnpos, &shellmtx, &velocity, (s32 *) &identitymtx);

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_00000080)
    {
        obj->projectile->flags |= PROJECTILEFLAG_LAUNCHING;

        if (weaponid != ITEM_TANKSHELLS)
        {
            obj->projectile->flags |= PROJECTILEFLAG_00000020;
            obj->projectile->unkB0 = obj->runtime_pos.y;
            obj->projectile->unkB4 = obj->projectile->speed.y;
            obj->projectile->unk10.x = unscaledvelocity.x;
            obj->projectile->unk10.y = unscaledvelocity.y;
            obj->projectile->unk10.z = unscaledvelocity.z;
            obj->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            if (obj->projectile->sounds[0] == NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, 1, &obj->projectile->sounds[0]);
            } 
            else if (obj->projectile->sounds[1] == NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, 1, &obj->projectile->sounds[1]);
            }
        }
    }
}


/**
 * D_80053DD8 and D_80053DDC belong to gunFireTankShell, but must sit here
 * immediately ahead of this function's pool for that function to match.
 */
GLOBAL_ASM(
.late_rodata
glabel D_80053DD8
.word 0x42855555 /*66.666664*/
glabel D_80053DDC
.word 0x3f8e38e3 /*1.111111*/
)


void gunUpdateAndFire(GUNHAND handnum)
{
    Mtxf *rwmtx;
    Mtxf gunmtx;
    Mtxf flashmtx;
    Mtxf flash2mtx;
    Mtxf tmpmtx;
    ModelFileHeader *mdlhdr;
    coord3d gunofs = D_80035C40;
    Mtxf rotmtx;
    Mtxf aimmtx;
    struct hand *hand;
    s32 *flashvisptr;
    f32 *flashdata;
    ModelNode *node;
    s32 j;
    ITEM_IDS item;
    WeaponStats *itemstats;
    f32 rndf;
    u32 rnd;
    f32 stackpad_scale;
    coord3d blendedpos;
    coord3d blendedlook;
    coord3d blendedup;
    s32 i;
    coord3d trigrot;
    coord3d taserrot;
    coord3d fistrot;
    f32 *cylinderdata;
    u8 stackpad1[4];
    ModelNode **hammerdata;
    Model *model;
    coord3d flashpos;
    f32 flashscale;
    f32 flashext;
    f32 *nodeptr;
    f32 *nodepos;
    f32 *swdata;
    s32 sw6mtxidx;
    f32 *hingedata;
    f32 *sw7data;
    s32 sw7mtxidx;
    s32 shellidx;
    u8 stackpad2[4];

    flashvisptr = NULL;
    flashdata = NULL;
    hand = &g_CurrentPlayer->hands[handnum];
    item = get_item_in_hand_or_watch_menu(handnum);
    itemstats = get_ptr_item_statistics(item);

    /**
     * When switching from a single weapon to dual wielding, both gun models interpolate a little to the sides of the screen
     * over a period of 4 seconds. And when switching back to a single weapon that weapon moves back more towards the
     * center of the screen.
     */
    if (handnum == GUNRIGHT)
    {
        if (bondwalkItemCheckBitflags(get_item_in_hand_or_watch_menu(1), WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) != 0)
        {
            hand->field_A34 += (2.0f * g_GlobalTimerDelta) / 240.0f;

            if (hand->field_A34 > 2.0f)
            {
                hand->field_A34 = 2.0f;
            }
        }
        else
        {
            hand->field_A34 -= (2.0f * g_GlobalTimerDelta) / 240.0f;

            if (hand->field_A34 < 0.0f)
            {
                hand->field_A34 = 0.0f;
            }
        }
    }
    else if (bondwalkItemCheckBitflags(get_item_in_hand_or_watch_menu(0), WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) != 0)
    {
        hand->field_A34 -= (2.0f * g_GlobalTimerDelta) / 240.0f;

        if (hand->field_A34 < (-2.0f))
        {
            hand->field_A34 = -2.0f;
        }
    }
    else
    {
        hand->field_A34 += (2.0f * g_GlobalTimerDelta) / 240.0f;

        if (hand->field_A34 > 0.0f)
        {
            hand->field_A34 = 0.0f;
        }
    }

    /**
     * Gun sway system. This moves the held weapons in figure-eight pattern which becomes bigger depending how fast the player is moving.
     */
    blendedpos = D_80035C4C;
    blendedlook = D_80035C58;
    blendedup = D_80035C64;

    i = hand->curblendpos;

    coord3dCatmullRomInterp(&hand->blendpos[(i + 3) % 4], &hand->blendpos[i], &hand->blendpos[(i + 1) % 4], &hand->blendpos[(i + 2) % 4], hand->dampt, &blendedpos);
    coord3dCatmullRomInterp(&hand->blendlook[(i + 3) % 4], &hand->blendlook[i], &hand->blendlook[(i + 1) % 4], &hand->blendlook[(i + 2) % 4], hand->dampt, &blendedlook);
    coord3dCatmullRomInterp(&hand->blendup[(i + 3) % 4], &hand->blendup[i], &hand->blendup[(i + 1) % 4], &hand->blendup[(i + 2) % 4], hand->dampt, &blendedup);

    blendedpos.x *= g_CurrentPlayer->gunposamplitude;
    blendedpos.y *= g_CurrentPlayer->gunposamplitude;
    blendedpos.z *= g_CurrentPlayer->gunposamplitude;
    blendedpos.x += hand->weapon_theta_displacement;
    blendedpos.y += hand->weapon_verta_displacement;
    blendedpos.x += sub_GAME_7F05DCB8(handnum);

    j = 0;

    if (g_ClockTimer > 0)
    {
        do
        {
            j += 1;
            hand->spring_pos_x = (GUN_SPRING_DAMP * hand->spring_pos_x) + ((f32 *) (&blendedpos))[0];
            hand->spring_pos_y = (GUN_SPRING_DAMP * hand->spring_pos_y) + ((f32 *) (&blendedpos))[1];
            hand->spring_pos_z = (GUN_SPRING_DAMP * hand->spring_pos_z) + ((f32 *) (&blendedpos))[2];
            hand->spring_look_x = (GUN_SPRING_DAMP * hand->spring_look_x) + ((f32 *) (&blendedlook))[0];
            hand->spring_look_y = (GUN_SPRING_DAMP * hand->spring_look_y) + ((f32 *) (&blendedlook))[1];
            hand->spring_look_z = (GUN_SPRING_DAMP * hand->spring_look_z) + ((f32 *) (&blendedlook))[2];
            hand->spring_up_x = (GUN_SPRING_DAMP * hand->spring_up_x) + ((f32 *) (&blendedup))[0];
            hand->spring_up_y = (GUN_SPRING_DAMP * hand->spring_up_y) + ((f32 *) (&blendedup))[1];
            hand->spring_up_z = (GUN_SPRING_DAMP * hand->spring_up_z) + ((f32 *) (&blendedup))[2];
        }
        while (j < g_ClockTimer);
    }

    hand->sway_pos_x = hand->spring_pos_x * GUN_SPRING_SCALE;
    hand->sway_pos_y = hand->spring_pos_y * GUN_SPRING_SCALE;
    hand->sway_pos_z = hand->spring_pos_z * GUN_SPRING_SCALE;
    hand->sway_look_x = hand->spring_look_x * GUN_SPRING_SCALE;
    hand->sway_look_y = hand->spring_look_y * GUN_SPRING_SCALE;
    hand->sway_look_z = hand->spring_look_z * GUN_SPRING_SCALE;
    hand->sway_up_x = hand->spring_up_x * GUN_SPRING_SCALE;
    hand->sway_up_y = hand->spring_up_y * GUN_SPRING_SCALE;
    hand->sway_up_z = hand->spring_up_z * GUN_SPRING_SCALE;

    // Offset the weapon to the right or left side of the screen depending on which hand it's in.
    if (handnum == GUNRIGHT)
    {
        gunofs.x = (gunSetHorizontalOffset(handnum) + hand->sway_pos_x) + (*hand).gunofs2_x;
    }
    else
    {
        gunofs.x = (gunSetHorizontalOffset(handnum) + hand->sway_pos_x) - hand->gunofs2_x;
    }

    gunofs.y = hand->gunofs2_y + (itemstats->PosY + hand->sway_pos_y);
    gunofs.z = hand->gunofs2_z + (itemstats->PosZ + hand->sway_pos_z);

    if (((item == ITEM_ROCKETLAUNCH) || (item == ITEM_TRIGGER)) || (item == ITEM_WATCHLASER))
    {
        gunofs.y += g_CurrentPlayer->ducking_height_offset / (-100.0f);
        gunofs.z += (3.0f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);

        if ((item == ITEM_ROCKETLAUNCH) && (((cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN) || (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)) || (get_screen_ratio() == SCREEN_RATIO_16_9)))
        {
            gunofs.y -= 3.0f;
        }
    }
    else if (item == ITEM_TASER)
    {
        gunofs.y += (2.5f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
        gunofs.z += (7.5f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
    }
    else
    {
        gunofs.y += (5.0f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
        gunofs.z += (15.0f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
    }

    if ((hand->weapon_firing_status != 0) && (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_00000020) != 0))
    {
        if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_00000040) != 0)
        {
            rnd = randomGetNext();
            rndf = (f32) ((u32) rnd);
            gunofs.x += 0.3f - ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.6f);
        }

        rnd = randomGetNext();
        rndf = (f32) ((u32) rnd);
        gunofs.y += 0.3f - ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.6f);
        rnd = randomGetNext();
        rndf = (f32) ((u32) rnd);
        gunofs.z += 0.3f - ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.6f);
    }

    gunofs.x += (((g_CurrentPlayer->field_FFC.x - getPlayer_c_screenleft()) - (getPlayer_c_screenwidth() * 0.5f)) * itemstats->PlayZ) / (getPlayer_c_screenwidth() * 0.5f);

    if ((g_CurrentPlayer->field_FFC.y - getPlayer_c_screentop()) > (getPlayer_c_screenheight() * 0.5f))
    {
        gunofs.y -= (((g_CurrentPlayer->field_FFC.y - getPlayer_c_screentop()) - (getPlayer_c_screenheight() * 0.5f)) * itemstats->PlayY) / (getPlayer_c_screenheight() * 0.5f);
    }
    else
    {
        gunofs.y -= (((g_CurrentPlayer->field_FFC.y - getPlayer_c_screentop()) - (getPlayer_c_screenheight() * 0.5f)) * itemstats->PlayX) / (getPlayer_c_screenheight() * 0.5f);
    }

    sub_GAME_7F05C614();
    matrix_4x4_set_identity(&rotmtx);

    if ((item == ITEM_TRIGGER) || (item == ITEM_WATCHLASER))
    {
            trigrot = D_80035C70;
            matrix_4x4_set_rotation_around_xyz(&trigrot, &tmpmtx);
            matrix_4x4_multiply_homogeneous_in_place(&tmpmtx, &rotmtx);
    }
    else if (item == ITEM_TASER)
    {
        taserrot = D_80035C7C;
        matrix_4x4_set_rotation_around_xyz(&taserrot, &tmpmtx);
        matrix_4x4_multiply_homogeneous_in_place(&tmpmtx, &rotmtx);
    }
    else if ((item == ITEM_FIST) && (g_CurrentPlayer->cur_item_weapon_getname == ITEM_SNIPERRIFLE))
    {
        fistrot = D_80035C88;
        matrix_4x4_set_rotation_around_xyz(&fistrot, &tmpmtx);
        matrix_4x4_multiply_homogeneous_in_place(&tmpmtx, &rotmtx);
        gunofs.x += -2.5f;
        gunofs.y += 27.8f;
        gunofs.z += 2.0f;
    }

    if (hand->field_92C != 0)
    {
        gunofs.x += hand->field_8EC.m[3][0];
        gunofs.y += hand->field_8EC.m[3][1];
        gunofs.z += hand->field_8EC.m[3][2];
        matrix_4x4_multiply_homogeneous_in_place(&hand->field_8EC, &rotmtx);
        rotmtx.m[3][0] = 0.0f;
        rotmtx.m[3][1] = 0.0f;
        rotmtx.m[3][2] = 0.0f;
    }
    else
    {
        hand->field_8E8 = 0.0f;
        hand->field_8DC = (-0.0f) + 0.0f;
        hand->field_8E0 = 0.0f;
        hand->field_8E4 = (-0.0f) + 0.0f;
    }

    matrix_4x4_set_basis_and_position_target(&tmpmtx, 0.0f, 0.0f, 0.0f, hand->sway_look_x, hand->sway_look_y, hand->sway_look_z, hand->sway_up_x, hand->sway_up_y, hand->sway_up_z);
    matrix_4x4_multiply_homogeneous_in_place(&tmpmtx, &rotmtx);
    matrix_4x4_align(&tmpmtx, 0.0f, gunofs.x - hand->field_A38, gunofs.y - hand->field_A3C, gunofs.z - hand->field_A40);
    matrix_4x4_multiply_homogeneous_in_place(&tmpmtx, &rotmtx);
    matrix_4x4_copy(&rotmtx, &gunmtx);
    matrix_4x4_set_position(&gunofs, &gunmtx);
    matrix_4x4_copy(&gunmtx, &hand->gunmtx_camspace);
    matrix_4x4_copy(&hand->throw_item_pos_related, &hand->throw_item_pos_related_prev);
    matrix_4x4_multiply_homogeneous(currentPlayerGetViewToWorldMtxf(), &hand->gunmtx_camspace, &hand->throw_item_pos_related);
    hand->field_87F = 1;

    if (((((((get_ptr_weapon_model_header_line(item) == 0) || (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)) || (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0)) 
    || (hand->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP)) || (hand->weapon_action_state == GUN_ANIM_STATE_SWITCH_HOLD)) || (Gun_hand_without_item(handnum) == 0)) || (get_itemtype_in_hand(handnum) == 0))
    {
        hand->field_87F = 0;
    }

    if ((hand->weapon_ammo_in_magazine <= 0) && (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_SINGLE_USE_RELOAD) != 0))
    {
        hand->field_87F = 0;
    }

    if (hand->field_87F != 0)
    {
        mdlhdr = &g_CurrentPlayer->copy_of_body_obj_header[handnum];
        rwmtx = (Mtxf *) dynAllocate(mdlhdr->numMatrices * ((s32) (sizeof(Mtxf))));
        j = 0;

        if (mdlhdr->numMatrices > 0)
        {
            do
            {
                matrix_4x4_set_identity((Mtxf *) (((u8 *) rwmtx) + (j * ((s32) (sizeof(Mtxf))))));
                j += 1;
            }
            while (j < mdlhdr->numMatrices);
        }

        modelCalculateRwDataLen(mdlhdr);
#ifdef DEBUG
        /** 
         * The model's runtime data is written into hand->modeldatas, which is a
         * fixed run of 32 words (modeldatas .. field_C04, ending at volley).
         */
        if (mdlhdr->numRecords >= 32)
        {
                osSyncPrintf("Increase GUNSAVESIZE to %d!!! ", mdlhdr->numRecords);
        }
#endif
        model = (Model *) (&hand->field_B68);

        if (mdlhdr->Switches);

        modelInit(model, mdlhdr, (s32 *) (&hand->modeldatas));
        sub_GAME_7F05E978(model, 1);
        sub_GAME_7F05EA94(model, hand->field_87E);
        node = mdlhdr->Switches[1];

        if (node != NULL)
        {
            if (&node->Data->Switch);

            flashvisptr = ((s32 *) (&hand->modeldatas)) + node->Data->Switch.RwDataIndex;
        }

        if (mdlhdr->Switches[3] != NULL)
        {
            flashdata = (f32 *) mdlhdr->Switches[3]->Data;
        }

        hand->mtxlist = rwmtx;

        if ((bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_MIRROR_DUAL) != 0) && (handnum == GUNLEFT))
        {
            matrix_column_1_scalar_multiply(-1.0f, gunmtx.m[0]);
        }

        matrix_scalar_multiply(IDO_POINT_ONE, gunmtx.m[0]);
        matrix_4x4_copy(&gunmtx, rwmtx);

        if (mdlhdr->Skeleton == (&skeleton_gun_revolver))
        {
            swdata = (f32 *) mdlhdr->Switches[4];

            if (swdata != NULL)
            {
                rndf = 0.0f;
                cylinderdata = (f32 *) ((ModelNode *) swdata)->Data;

                if (item == ITEM_RUGER)
                {
                    if (hand->weapon_action_state == 1)
                    {
#if defined(VERSION_EU)
                        rndf = (((hand->field_890 - (hand->weapon_ammo_in_magazine * 5)) + 0x19) * M_TAU_F) / 30.0f;
#else
                        rndf = (((hand->field_890 - (hand->weapon_ammo_in_magazine * 6)) + 0x1E) * M_TAU_F) / 36.0f;
#endif
                    }
                    else
                    {
                        rndf = ((6 - hand->weapon_ammo_in_magazine) * M_TAU_F) / 6.0f;
                    }
                }
                else if (hand->weapon_action_state == 1)
                {
#if defined(VERSION_EU)
                if (hand->field_890 < 5)
                {
                    rndf = (hand->field_890 * M_TAU_F) / 30.0f;
                }
#else
                if (hand->field_890 < 6)
                {
                    rndf = (hand->field_890 * M_TAU_F) / 36.0f;
                }
#endif
            }

                matrix_4x4_set_rotation_around_z(rndf, &tmpmtx);
                matrix_4x4_set_position((coord3d *) cylinderdata, &tmpmtx);
                matrix_4x4_multiply(&gunmtx, &tmpmtx, &rwmtx[3]);
            }

            swdata = (f32 *) mdlhdr->Switches[5];

            if (swdata != NULL)
            {
                hammerdata = (ModelNode **) ((ModelNode *) swdata)->Data;

                if (hand->weapon_action_state == 1)
                {
#if defined(VERSION_EU)
                    if (hand->field_890 < 2)
                    {
                        rndf = (2.0f * ((-((f32) hand->field_890)) * DegToRad(30.0f))) / 5.0f;
                    }
                    else
                    {
                        rndf = (2.0f * ((-((f32) (5 - hand->field_890))) * DegToRad(30.0f))) / 5.0f;
                    }
#else
                    if (hand->field_890 < 3)
                    {
                        rndf = (2.0f * ((-((f32) hand->field_890)) * DegToRad(30.0f))) / 6.0f;
                    }
                    else
                    {
                        rndf = (2.0f * ((-((f32) (6 - hand->field_890))) * DegToRad(30.0f))) / 6.0f;
                    }
#endif
                    matrix_4x4_set_rotation_around_x(rndf, &tmpmtx);
                    matrix_4x4_set_position((coord3d *) hammerdata, &tmpmtx);
                }
                else
                {
                    matrix_4x4_set_identity_and_position((coord3d *) hammerdata, &tmpmtx);
                }

                matrix_4x4_multiply(&gunmtx, &tmpmtx, &rwmtx[4]);
            }
        }

        if (flashvisptr != NULL)
        {
            *flashvisptr = 0;
        }

        if (flashdata != NULL)
        {
            rnd = randomGetNext();
            rndf = (f32) ((u32) rnd);
            flashscale = ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.25f) + 1.0f;
            flashext = itemstats->MuzzleFlashExtension;

            if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_00000001) != 0)
            {
                rnd = randomGetNext();
                rndf = (f32) ((u32) rnd);
                matrix_4x4_set_rotation_around_z((rndf * (1.0f / M_U32_MAX_VALUE_F)) * M_TAU_F, &flashmtx);
                matrix_4x4_set_position((coord3d *) flashdata, &flashmtx);
            }
            else
            {
                matrix_4x4_set_identity_and_position((coord3d *) flashdata, &flashmtx);
            }

            matrix_scalar_multiply(flashscale, flashmtx.m[0]);
            matrix_column_3_scalar_multiply(flashext, flashmtx.m[0]);
            matrix_4x4_multiply_in_place(&gunmtx, &flashmtx);
            matrix_4x4_copy(&flashmtx, &rwmtx[1]);

            hand->field_B58.x = flashmtx.m[3][0];
            hand->field_B58.y = flashmtx.m[3][1];
            hand->field_B58.z = flashmtx.m[3][2];
            mtx4TransformVecInPlace(currentPlayerGetViewToWorldMtxf(), &hand->field_B58);
            hand->field_B64 = -flashmtx.m[3][2];

            if (hand->field_87D != 0)
            {
                if (flashvisptr != NULL)
                {
                    *flashvisptr = 1;
                }

                nodeptr = (f32 *) mdlhdr->Switches[2];

                if (nodeptr != NULL)
                {
                    nodepos = (f32 *) ((ModelNode *) nodeptr)->Data;
                    flashpos.x = (((((f32 *) nodepos)[0] * flashmtx.m[0][0]) + (((f32 *) nodepos)[1] * flashmtx.m[1][0])) + (((f32 *) nodepos)[2] * flashmtx.m[2][0])) + flashmtx.m[3][0];
                    flashpos.y = (((((f32 *) nodepos)[0] * flashmtx.m[0][1]) + (((f32 *) nodepos)[1] * flashmtx.m[1][1])) + (((f32 *) nodepos)[2] * flashmtx.m[2][1])) + flashmtx.m[3][1];
                    flashpos.z = (((((f32 *) nodepos)[0] * flashmtx.m[0][2]) + (((f32 *) nodepos)[1] * flashmtx.m[1][2])) + (((f32 *) nodepos)[2] * flashmtx.m[2][2])) + flashmtx.m[3][2];

                    matrix_4x4_align(&flash2mtx, (randomGetNext() * (1.0f / M_U32_MAX_VALUE_F)) * M_TAU_F, -flashpos.x, -flashpos.y, -flashpos.z);
                    matrix_scalar_multiply(IDO_POINT_ONE * flashscale, flash2mtx.m[0]);
                    matrix_4x4_set_rotation_axis_angle(&aimmtx, 0, gunofs.x - hand->field_A38, gunofs.y - hand->field_A3C, gunofs.z - hand->field_A40);
                    matrix_4x4_multiply_in_place(&aimmtx, &flash2mtx);
                    matrix_row_3_scalar_multiply(flashext, flash2mtx.m[0]);
                    matrix_4x4_multiply_in_place(&rotmtx, &flash2mtx);
                    matrix_4x4_set_position(&flashpos, &flash2mtx);
                    matrix_4x4_copy(&flash2mtx, &rwmtx[2]);
                }

                if (mdlhdr->Skeleton == (&skeleton_gun_kf7))
                {
                    nodeptr = (f32 *) mdlhdr->Switches[4];

                    if (nodeptr != NULL)
                    {
                        nodepos = (f32 *) ((ModelNode *) nodeptr)->Data;
                        flashpos.x = (((((f32 *) nodepos)[0] * flashmtx.m[0][0]) + (((f32 *) nodepos)[1] * flashmtx.m[1][0])) + (((f32 *) nodepos)[2] * flashmtx.m[2][0])) + flashmtx.m[3][0];
                        flashpos.y = (((((f32 *) nodepos)[0] * flashmtx.m[0][1]) + (((f32 *) nodepos)[1] * flashmtx.m[1][1])) + (((f32 *) nodepos)[2] * flashmtx.m[2][1])) + flashmtx.m[3][1];
                        flashpos.z = (((((f32 *) nodepos)[0] * flashmtx.m[0][2]) + (((f32 *) nodepos)[1] * flashmtx.m[1][2])) + (((f32 *) nodepos)[2] * flashmtx.m[2][2])) + flashmtx.m[3][2];
                        ((f32 *) stackpad2)[-8] = IDO_POINT_ONE * flashscale;
                        matrix_4x4_align(&flash2mtx, (randomGetNext() * (1.0f / M_U32_MAX_VALUE_F)) * M_TAU_F, -flashpos.x, -flashpos.y, -flashpos.z);
                        matrix_scalar_multiply(((f32 *) stackpad2)[-8], flash2mtx.m[0]);
                        matrix_4x4_set_rotation_axis_angle(&aimmtx, 0, gunofs.x - hand->field_A38, gunofs.y - hand->field_A3C, gunofs.z - hand->field_A40);
                        matrix_4x4_multiply_in_place(&aimmtx, &flash2mtx);
                        matrix_row_3_scalar_multiply(flashext, flash2mtx.m[0]);
                        matrix_4x4_multiply_in_place(&rotmtx, &flash2mtx);
                        matrix_4x4_set_position(&flashpos, &flash2mtx);
                        matrix_4x4_copy(&flash2mtx, &rwmtx[3]);
                    }
                }
            }
        }
        else
        {
            hand->field_B58.x = hand->throw_item_pos_related.m[3][0];
            hand->field_B58.y = hand->throw_item_pos_related.m[3][1];
            hand->field_B58.z = hand->throw_item_pos_related.m[3][2];
            hand->field_B64 = -hand->gunmtx_camspace.m[3][2];
        }

        node = mdlhdr->Switches[6];

        if (node != NULL)
        {
            swdata = (f32 *) node->Data;
            sw6mtxidx = modelFindNodeMtxIndex(node, 0);
            sub_GAME_7F05E6B4(handnum, hand->weapon_hold_time);

            if ((mdlhdr->numSwitches >= 0x1D) && (mdlhdr->Switches[28] != NULL))
            {
                hingedata = (f32 *) mdlhdr->Switches[28]->Data;
                guRotateF(tmpmtx.m, (((hand->field_A84 + M_TAU_F) - get_value_if_watch_is_on_hand_or_not(handnum)) * 360.0f) / M_TAU_F, hingedata[0] - hingedata[3], hingedata[1] - hingedata[4], hingedata[2] - hingedata[5]);
                matrix_4x4_set_position((coord3d *) swdata, &tmpmtx);
            }
            else
            {
                matrix_4x4_set_position_and_rotation_around_y(swdata, hand->field_A84, &tmpmtx);
            }

            matrix_4x4_multiply_homogeneous(&gunmtx, &tmpmtx, &rwmtx[sw6mtxidx]);
        }

        if (mdlhdr->numSwitches >= 0x1E)
        {
            bondviewSelectCuff(model, mdlhdr, 0x1D);
        }

        node = mdlhdr->Switches[7];

        if (node != NULL)
        {
            sw7data = (f32 *) node->Data;
            sw7mtxidx = modelFindNodeMtxIndex(node, 0);
            sub_GAME_7F05E83C(handnum);
            matrix_4x4_set_identity_and_position((coord3d *) sw7data, &tmpmtx);
            tmpmtx.m[3][2] -= hand->field_A88;
            matrix_4x4_multiply(&gunmtx, &tmpmtx, &rwmtx[sw7mtxidx]);
        }

        shellidx = 0;

        /**
         * For the shotguns, show or hide the shells in the shell holder based on how much ammunition is in the player's reserve.
         */
        if (mdlhdr->numSwitches >= 0x13)
        {
            do
            {
                if (mdlhdr->Switches[18 + shellidx] != 0)
                {
                    nodeptr = (f32 *) modelGetNodeRwData(model, mdlhdr->Switches[18 + shellidx]);

                    if (nodeptr != NULL)
                    {
                        *((s32 *) nodeptr) = hand->numvisibleshells >= (5 - shellidx);
                    }
                }

                if (mdlhdr->Switches[23 + shellidx] != 0)
                {
                    nodeptr = (f32 *) modelGetNodeRwData(model, mdlhdr->Switches[23 + shellidx]);

                    if (nodeptr != NULL)
                    {
                        *((s32 *) nodeptr) = hand->numvisibleshells >= (5 - shellidx);
                    }
                }

                shellidx += 1;
            }
            while (shellidx != 5);
        }

        modelUpdateNodeRelations(model);

        if (hand->weapon_firing_status != 0)
        {
            switch (item)
            {
            case ITEM_WPPK:
            case ITEM_WPPKSIL:
            case ITEM_TT33:
            case ITEM_SKORPION:
            case ITEM_AK47:
            case ITEM_UZI:
            case ITEM_MP5K:
            case ITEM_MP5KSIL:
            case ITEM_SPECTRE:
            case ITEM_M16:
            case ITEM_FNP90:
            case ITEM_SNIPERRIFLE:
            case ITEM_RUGER:
            case ITEM_GOLDENGUN:
            case ITEM_SILVERWPPK:
            case ITEM_GOLDWPPK:
                gunCreateBeamForHand(handnum);
                hand->field_8A0 = hand->field_8A0 + 1;
                break;
            case ITEM_LASER:
            case ITEM_WATCHLASER:
#if defined(VERSION_JP) || defined(VERSION_EU)
                hand->field_8A0 = hand->field_8A0 + 1;
#endif
                gunCreateBeamForHand(handnum);
                break;
            }
        }
    }

    if (item == ITEM_ROCKETLAUNCH)
    {
        gunUpdateAttachedRocket(handnum);
    }

    if (hand->weapon_firing_status != 0)
    {
        sub_GAME_7F068508(handnum, bondviewGetPlayerStanHeight(g_CurrentPlayer));

        if (item == ITEM_GRENADELAUNCH)
        {
            gunSpawnGLGrenade(handnum);
            return;
        }

        if (item == ITEM_GRENADE)
        {
            generate_player_thrown_grenade(handnum);
            return;
        }

        if (item == ITEM_ROCKETLAUNCH)
        {
            gunFireTankShell(handnum);
            return;
        }

        if (item == ITEM_THROWKNIFE)
        {
            generate_player_thrown_knife(handnum);
            return;
        }

        if ((((((((item == ITEM_REMOTEMINE) || (item == ITEM_PROXIMITYMINE)) || (item == ITEM_TIMEDMINE)) || (item == ITEM_BOMBCASE)) || (item == ITEM_BUG)) || (item == ITEM_MICROCAMERA)) || (item == ITEM_GOLDENEYEKEY)) || (item == ITEM_PLASTIQUE))
        {
            generate_player_thrown_object(handnum);
            return;
        }

        if (item == ITEM_FLAREPISTOL)
        {
            gunSpawnGLGrenade(handnum);
            return;
        }

        if (item == ITEM_PITONGUN)
        {
            gunSpawnGLGrenade(handnum);
        }
    }
}


void gunUpdateAndFireBothHands(void)
{
    gunUpdateAndFire(GUNRIGHT);
    gunUpdateAndFire(GUNLEFT);
}


/**
 * @param arg0:
 * @param item:
 * @param arg2:
 * @param arg3:
 *
 * Address 0x7F061948.
 *
 * This function adjusts the length of the bullet beam that's rendered on screen.
 * This function is used for both player and guard beams.
 *
 * The watch laser has a very short beam, in accordance with its range.
 * The laser also has a shortened one, but it appears this is to avoid graphical glitches.
 * Other weapons have their bullet beam capped at 10000 max length, otherwise if the player
 * fires into the void, there may be graphical glitches with the beam.
 *
*/
void CapBeamLengthAndDecideIfRendered(struct BeamRecord *arg0, ITEM_IDS item, coord3d *arg2, coord3d *arg3)
{
    f32 phi_f12_2;

    //arg0->pos.x = arg2->x;
    //arg0->pos.y = arg2->y;
    //arg0->pos.z = arg2->z;

    //arg0->delta.x = arg3->x - arg2->x;
    //arg0->delta.y = arg3->y - arg2->y;
    //arg0->delta.z = arg3->z - arg2->z;

    //phi_f12_2 = sqrtf((arg0->delta.f[0] * arg0->delta.f[0]) + (arg0->delta.f[1] * arg0->delta.f[1]) + (arg0->delta.f[2] * arg0->delta.f[2]));

    //arg0->delta.x *= 1.0f / phi_f12_2;
    //arg0->delta.y *= 1.0f / phi_f12_2;
    //arg0->delta.z *= 1.0f / phi_f12_2;


    arg0->pos.f[0] = arg2->f[0];
    arg0->pos.f[1] = arg2->f[1];
    arg0->pos.f[2] = arg2->f[2];

    arg0->delta.f[0] = arg3->x - arg2->x;
    arg0->delta.f[1] = arg3->f[1] - arg2->f[1];
    arg0->delta.f[2] = arg3->f[2] - arg2->f[2];

    phi_f12_2 = sqrtf((arg0->delta.f[0] * arg0->delta.f[0]) + (arg0->delta.f[1] * arg0->delta.f[1]) + (arg0->delta.f[2] * arg0->delta.f[2]));

    arg0->delta.f[0] *= 1.0f / phi_f12_2;
    arg0->delta.f[1] *= 1.0f / phi_f12_2;
    arg0->delta.f[2] *= 1.0f / phi_f12_2;

    if (item == ITEM_WATCHLASER)
    {
        if (phi_f12_2 > 300.0f)
        {
            phi_f12_2 = 300.0f;
        }
    }
    else
    {
        if (phi_f12_2 > 10000.0f)
        {
            phi_f12_2 = 10000.0f;
        }
    }

    arg0->unk00 = 0;
    arg0->item_id = (s8) item;
    arg0->unk1c = phi_f12_2;

    if (phi_f12_2 < 500.0f)
    {
        phi_f12_2 = 500.0f;
    }

    if (item == ITEM_LASER)
    {
        arg0->unk20 = 0.25f * phi_f12_2;
        arg0->unk24 = 0.6f * phi_f12_2;

        if (arg0->unk24 > 3000.0f)
        {
            arg0->unk24 = 3000.0f;
        }

        // Laser beams are rendered more often than other normal weapons
        arg0->unk28 = (-0.1f - ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX) * 0.3f)) * phi_f12_2;
    }
    else if (item == ITEM_WATCHLASER)
    {
        arg0->unk24 = phi_f12_2;
        arg0->unk20 = 2.0f * phi_f12_2;

        if (phi_f12_2 > 3000.0f)
        {
            arg0->unk24 = 3000.0f;
        }

        // Always render the beam for the watch laser
        arg0->unk28 = 0.0f;
    }
    else
    {
        arg0->unk20 = 0.2f * phi_f12_2;
        arg0->unk24 = arg0->unk20;

        if (arg0->unk20 > 3000.0f)
        {
            arg0->unk24 = 3000.0f;
        }

        // Decide if a beam should be rendered for normal weapon bullets
        arg0->unk28 = ((2.0f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX))) - 1.0f) * arg0->unk20;
    }

    if (arg0->unk1c <= arg0->unk28)
    {
        // No beam will be rendered
        arg0->unk00 = -1;
    }
}


void gunCreateBeamForHand(enum GUNHAND hand)
{
    coord3d *field_2A18;
    Mtxf *player_matrix;
    struct hand *hand_ptr;
    f32 val;
    struct ChrRecord *chr;
    f32 diff1_z;
    f32 diff1_y;
    f32 diff1_x;
    f32 diff2_z;
    f32 diff2_y;
    f32 diff2_x;
    BeamRecord *weapon_beam;

    hand_ptr = &g_CurrentPlayer->hands[hand];
    player_matrix = camGetWorldToScreenMtxf();

    val = -((((hand_ptr->item_related.x * player_matrix->m[0][2]) + (hand_ptr->item_related.y * player_matrix->m[1][2])) + (hand_ptr->item_related.z * player_matrix->m[2][2])) + player_matrix->m[3][2]);

    if (val < hand_ptr->field_B64)
    { 
        return; 
    }

    weapon_beam = &hand_ptr->weapon_beam;

    CapBeamLengthAndDecideIfRendered(weapon_beam, getCurrentPlayerWeaponId(hand), &hand_ptr->field_B58, &hand_ptr->item_related);

    if ((g_CurrentPlayer->prop->chr == NULL) || (getPlayerCount() < 2)) 
    { 
        return; 
    }

    chr = g_CurrentPlayer->prop->chr;

    diff1_x = hand_ptr->item_related.x - g_CurrentPlayer->field_2A18[hand].x;
    diff1_y = hand_ptr->item_related.y - g_CurrentPlayer->field_2A18[hand].y;
    diff1_z = hand_ptr->item_related.z - g_CurrentPlayer->field_2A18[hand].z;
    guNormalize(&diff1_x, &diff1_y, &diff1_z);

    diff2_x = hand_ptr->item_related.x - hand_ptr->field_B58.x;
    diff2_y = hand_ptr->item_related.y - hand_ptr->field_B58.y;
    diff2_z = hand_ptr->item_related.z - hand_ptr->field_B58.z;
    guNormalize(&diff2_x, &diff2_y, &diff2_z);

    val = acosf(
        + (diff2_z * diff1_z)
        + ((diff1_x * diff2_x)
        + (diff1_y * diff2_y)));

    if (val > 0.08726647f) 
    { 
        return; 
    }

    CapBeamLengthAndDecideIfRendered(&chr->beams[hand], getCurrentPlayerWeaponId(hand), &g_CurrentPlayer->field_2A18[hand], &hand_ptr->item_related);
}


#ifdef NONMATCHING
void sub_GAME_7F061E18(void) {

}
#else

#if defined(VERSION_US) || defined(VERSION_JP)
GLOBAL_ASM(
.late_rodata
glabel D_80053EAC
.word 0x3fb50481 /*1.4141999*/
glabel D_80053EB0
.word 0x3dcccccd /*0.1*/
glabel D_80053EB4
.word 0x3f666666 /*0.89999998*/
glabel D_80053EB8
.word 0x3f666666 /*0.89999998*/
glabel D_80053EBC
.word 0x3f666666 /*0.89999998*/
glabel D_80053EC0
.word 0x3f666666 /*0.89999998*/
glabel D_80053EC4
.word 0x3f666666 /*0.89999998*/
glabel D_80053EC8
.word 0x3f666666 /*0.89999998*/
glabel D_80053ECC
.word 0x3fb50481 /*1.4141999*/
glabel D_80053ED0
.word 0x3f666666 /*0.89999998*/
.text
glabel sub_GAME_7F061E18
/* 096948 7F061E18 27BDFEA0 */  addiu $sp, $sp, -0x160
/* 09694C 7F061E1C AFBF002C */  sw    $ra, 0x2c($sp)
/* 096950 7F061E20 AFB10028 */  sw    $s1, 0x28($sp)
/* 096954 7F061E24 AFB00024 */  sw    $s0, 0x24($sp)
/* 096958 7F061E28 F7B40018 */  sdc1  $f20, 0x18($sp)
/* 09695C 7F061E2C AFA40160 */  sw    $a0, 0x160($sp)
/* 096960 7F061E30 AFA60168 */  sw    $a2, 0x168($sp)
/* 096964 7F061E34 80AB0000 */  lb    $t3, ($a1)
/* 096968 7F061E38 3C0E8003 */  lui   $t6, %hi(D_80035C98)
/* 09696C 7F061E3C 00A08825 */  move  $s1, $a1
/* 096970 7F061E40 05600328 */  bltz  $t3, .L7F062AE4
/* 096974 7F061E44 25CE5C98 */   addiu $t6, %lo(D_80035C98) # addiu $t6, $t6, 0x5c98
/* 096978 7F061E48 8DC10000 */  lw    $at, ($t6)
/* 09697C 7F061E4C 8DD90004 */  lw    $t9, 4($t6)
/* 096980 7F061E50 27A90108 */  addiu $t1, $sp, 0x108
/* 096984 7F061E54 AD210000 */  sw    $at, ($t1)
/* 096988 7F061E58 AD390004 */  sw    $t9, 4($t1)
/* 09698C 7F061E5C 8DD9000C */  lw    $t9, 0xc($t6)
/* 096990 7F061E60 8DC10008 */  lw    $at, 8($t6)
/* 096994 7F061E64 AD39000C */  sw    $t9, 0xc($t1)
/* 096998 7F061E68 0FC227F5 */  jal   bondviewGetCurrentPlayersPosition
/* 09699C 7F061E6C AD210008 */   sw    $at, 8($t1)
/* 0969A0 7F061E70 AFA200F8 */  sw    $v0, 0xf8($sp)
/* 0969A4 7F061E74 3C0D8003 */  lui   $t5, %hi(D_80035CA8)
/* 0969A8 7F061E78 25AD5CA8 */  addiu $t5, %lo(D_80035CA8) # addiu $t5, $t5, 0x5ca8
/* 0969AC 7F061E7C 8DA10000 */  lw    $at, ($t5)
/* 0969B0 7F061E80 C6200028 */  lwc1  $f0, 0x28($s1)
/* 0969B4 7F061E84 C6340024 */  lwc1  $f20, 0x24($s1)
/* 0969B8 7F061E88 27AF00C4 */  addiu $t7, $sp, 0xc4
/* 0969BC 7F061E8C ADE10000 */  sw    $at, ($t7)
/* 0969C0 7F061E90 8DA10008 */  lw    $at, 8($t5)
/* 0969C4 7F061E94 8DAB0004 */  lw    $t3, 4($t5)
/* 0969C8 7F061E98 3C098003 */  lui   $t1, %hi(D_80035CB4)
/* 0969CC 7F061E9C 25295CB4 */  addiu $t1, %lo(D_80035CB4) # addiu $t1, $t1, 0x5cb4
/* 0969D0 7F061EA0 ADE10008 */  sw    $at, 8($t7)
/* 0969D4 7F061EA4 ADEB0004 */  sw    $t3, 4($t7)
/* 0969D8 7F061EA8 8D210000 */  lw    $at, ($t1)
/* 0969DC 7F061EAC 27B800B8 */  addiu $t8, $sp, 0xb8
/* 0969E0 7F061EB0 8D2A0004 */  lw    $t2, 4($t1)
/* 0969E4 7F061EB4 AF010000 */  sw    $at, ($t8)
/* 0969E8 7F061EB8 8D210008 */  lw    $at, 8($t1)
/* 0969EC 7F061EBC AF0A0004 */  sw    $t2, 4($t8)
/* 0969F0 7F061EC0 3C0C8009 */  lui   $t4, %hi(flareimage3)
/* 0969F4 7F061EC4 AF010008 */  sw    $at, 8($t8)
/* 0969F8 7F061EC8 3C018005 */  lui   $at, %hi(D_80053EAC)
/* 0969FC 7F061ECC C4243EAC */  lwc1  $f4, %lo(D_80053EAC)($at)
/* 096A00 7F061ED0 8D8CD0D0 */  lw    $t4, %lo(flareimage3)($t4)
/* 096A04 7F061ED4 E7A000E8 */  swc1  $f0, 0xe8($sp)
/* 096A08 7F061ED8 E7A400B4 */  swc1  $f4, 0xb4($sp)
/* 096A0C 7F061EDC 0FC1E0F1 */  jal   camGetWorldToScreenMtxf
/* 096A10 7F061EE0 AFAC00B0 */   sw    $t4, 0xb0($sp)
/* 096A14 7F061EE4 AFA200A8 */  sw    $v0, 0xa8($sp)
/* 096A18 7F061EE8 82230001 */  lb    $v1, 1($s1)
/* 096A1C 7F061EEC 24010016 */  li    $at, 22
/* 096A20 7F061EF0 C7A000E8 */  lwc1  $f0, 0xe8($sp)
/* 096A24 7F061EF4 14610007 */  bne   $v1, $at, .L7F061F14
/* 096A28 7F061EF8 3C014248 */   li    $at, 0x42480000 # 50.000000
/* 096A2C 7F061EFC 44819000 */  mtc1  $at, $f18
/* 096A30 7F061F00 3C0F8009 */  lui   $t7, %hi(flareimage4)
/* 096A34 7F061F04 8DEFD0D4 */  lw    $t7, %lo(flareimage4)($t7)
/* 096A38 7F061F08 E7B200F4 */  swc1  $f18, 0xf4($sp)
/* 096A3C 7F061F0C 10000026 */  b     .L7F061FA8
/* 096A40 7F061F10 AFAF00B0 */   sw    $t7, 0xb0($sp)
.L7F061F14:
/* 096A44 7F061F14 24010017 */  li    $at, 23
/* 096A48 7F061F18 1461001F */  bne   $v1, $at, .L7F061F98
/* 096A4C 7F061F1C 3C0D8009 */   lui   $t5, %hi(flareimage4)
/* 096A50 7F061F20 3C014120 */  li    $at, 0x41200000 # 10.000000
/* 096A54 7F061F24 44813000 */  mtc1  $at, $f6
/* 096A58 7F061F28 8DADD0D4 */  lw    $t5, %lo(flareimage4)($t5)
/* 096A5C 7F061F2C E7A000E8 */  swc1  $f0, 0xe8($sp)
/* 096A60 7F061F30 E7A600F4 */  swc1  $f6, 0xf4($sp)
/* 096A64 7F061F34 0C002914 */  jal   randomGetNext
/* 096A68 7F061F38 AFAD00B0 */   sw    $t5, 0xb0($sp)
/* 096A6C 7F061F3C 24010032 */  li    $at, 50
/* 096A70 7F061F40 0041001B */  divu  $zero, $v0, $at
/* 096A74 7F061F44 00005810 */  mfhi  $t3
/* 096A78 7F061F48 25790096 */  addiu $t9, $t3, 0x96
/* 096A7C 7F061F4C 0C002914 */  jal   randomGetNext
/* 096A80 7F061F50 A3B90117 */   sb    $t9, 0x117($sp)
/* 096A84 7F061F54 24010005 */  li    $at, 5
/* 096A88 7F061F58 0041001B */  divu  $zero, $v0, $at
/* 096A8C 7F061F5C 00007010 */  mfhi  $t6
/* 096A90 7F061F60 C7A000E8 */  lwc1  $f0, 0xe8($sp)
/* 096A94 7F061F64 55C00011 */  bnezl $t6, .L7F061FAC
/* 096A98 7F061F68 C6240004 */   lwc1  $f4, 4($s1)
/* 096A9C 7F061F6C 0C002914 */  jal   randomGetNext
/* 096AA0 7F061F70 E7A000E8 */   swc1  $f0, 0xe8($sp)
/* 096AA4 7F061F74 24010064 */  li    $at, 100
/* 096AA8 7F061F78 0041001B */  divu  $zero, $v0, $at
/* 096AAC 7F061F7C 0000C010 */  mfhi  $t8
/* 096AB0 7F061F80 240900FF */  li    $t1, 255
/* 096AB4 7F061F84 01381823 */  subu  $v1, $t1, $t8
/* 096AB8 7F061F88 A3A30115 */  sb    $v1, 0x115($sp)
/* 096ABC 7F061F8C A3A30114 */  sb    $v1, 0x114($sp)
/* 096AC0 7F061F90 10000005 */  b     .L7F061FA8
/* 096AC4 7F061F94 C7A000E8 */   lwc1  $f0, 0xe8($sp)
.L7F061F98:
/* 096AC8 7F061F98 3C0141F0 */  li    $at, 0x41F00000 # 30.000000
/* 096ACC 7F061F9C 44815000 */  mtc1  $at, $f10
/* 096AD0 7F061FA0 00000000 */  nop
/* 096AD4 7F061FA4 E7AA00F4 */  swc1  $f10, 0xf4($sp)
.L7F061FA8:
/* 096AD8 7F061FA8 C6240004 */  lwc1  $f4, 4($s1)
.L7F061FAC:
/* 096ADC 7F061FAC 44807000 */  mtc1  $zero, $f14
/* 096AE0 7F061FB0 E7A400FC */  swc1  $f4, 0xfc($sp)
/* 096AE4 7F061FB4 C6280008 */  lwc1  $f8, 8($s1)
/* 096AE8 7F061FB8 4600703C */  c.lt.s $f14, $f0
/* 096AEC 7F061FBC E7A80100 */  swc1  $f8, 0x100($sp)
/* 096AF0 7F061FC0 C626000C */  lwc1  $f6, 0xc($s1)
/* 096AF4 7F061FC4 45000011 */  bc1f  .L7F06200C
/* 096AF8 7F061FC8 E7A60104 */   swc1  $f6, 0x104($sp)
/* 096AFC 7F061FCC C6240010 */  lwc1  $f4, 0x10($s1)
/* 096B00 7F061FD0 C7AA00FC */  lwc1  $f10, 0xfc($sp)
/* 096B04 7F061FD4 46040202 */  mul.s $f8, $f0, $f4
/* 096B08 7F061FD8 C7A40100 */  lwc1  $f4, 0x100($sp)
/* 096B0C 7F061FDC 46085180 */  add.s $f6, $f10, $f8
/* 096B10 7F061FE0 E7A600FC */  swc1  $f6, 0xfc($sp)
/* 096B14 7F061FE4 C62A0014 */  lwc1  $f10, 0x14($s1)
/* 096B18 7F061FE8 460A0202 */  mul.s $f8, $f0, $f10
/* 096B1C 7F061FEC C7AA0104 */  lwc1  $f10, 0x104($sp)
/* 096B20 7F061FF0 46082180 */  add.s $f6, $f4, $f8
/* 096B24 7F061FF4 E7A60100 */  swc1  $f6, 0x100($sp)
/* 096B28 7F061FF8 C6240018 */  lwc1  $f4, 0x18($s1)
/* 096B2C 7F061FFC 46040202 */  mul.s $f8, $f0, $f4
/* 096B30 7F062000 46085180 */  add.s $f6, $f10, $f8
/* 096B34 7F062004 10000003 */  b     .L7F062014
/* 096B38 7F062008 E7A60104 */   swc1  $f6, 0x104($sp)
.L7F06200C:
/* 096B3C 7F06200C 4600A500 */  add.s $f20, $f20, $f0
/* 096B40 7F062010 46007006 */  mov.s $f0, $f14
.L7F062014:
/* 096B44 7F062014 46140100 */  add.s $f4, $f0, $f20
/* 096B48 7F062018 C622001C */  lwc1  $f2, 0x1c($s1)
/* 096B4C 7F06201C 4604103C */  c.lt.s $f2, $f4
/* 096B50 7F062020 00000000 */  nop
/* 096B54 7F062024 45020003 */  bc1fl .L7F062034
/* 096B58 7F062028 C62C0018 */   lwc1  $f12, 0x18($s1)
/* 096B5C 7F06202C 46001501 */  sub.s $f20, $f2, $f0
/* 096B60 7F062030 C62C0018 */  lwc1  $f12, 0x18($s1)
.L7F062034:
/* 096B64 7F062034 C7AA0104 */  lwc1  $f10, 0x104($sp)
/* 096B68 7F062038 8FA200F8 */  lw    $v0, 0xf8($sp)
/* 096B6C 7F06203C 460CA202 */  mul.s $f8, $f20, $f12
/* 096B70 7F062040 C6220014 */  lwc1  $f2, 0x14($s1)
/* 096B74 7F062044 C4440008 */  lwc1  $f4, 8($v0)
/* 096B78 7F062048 E7AA0030 */  swc1  $f10, 0x30($sp)
/* 096B7C 7F06204C 46085180 */  add.s $f6, $f10, $f8
/* 096B80 7F062050 C44A0004 */  lwc1  $f10, 4($v0)
/* 096B84 7F062054 46062201 */  sub.s $f8, $f4, $f6
/* 096B88 7F062058 46081102 */  mul.s $f4, $f2, $f8
/* 096B8C 7F06205C C7A80100 */  lwc1  $f8, 0x100($sp)
/* 096B90 7F062060 46141182 */  mul.s $f6, $f2, $f20
/* 096B94 7F062064 46083180 */  add.s $f6, $f6, $f8
/* 096B98 7F062068 46065281 */  sub.s $f10, $f10, $f6
/* 096B9C 7F06206C 460C5182 */  mul.s $f6, $f10, $f12
/* 096BA0 7F062070 46062281 */  sub.s $f10, $f4, $f6
/* 096BA4 7F062074 C7A400FC */  lwc1  $f4, 0xfc($sp)
/* 096BA8 7F062078 E7AA00D0 */  swc1  $f10, 0xd0($sp)
/* 096BAC 7F06207C C6200010 */  lwc1  $f0, 0x10($s1)
/* 096BB0 7F062080 C62C0018 */  lwc1  $f12, 0x18($s1)
/* 096BB4 7F062084 E7A80034 */  swc1  $f8, 0x34($sp)
/* 096BB8 7F062088 4600A182 */  mul.s $f6, $f20, $f0
/* 096BBC 7F06208C C4480000 */  lwc1  $f8, ($v0)
/* 096BC0 7F062090 E7AA0038 */  swc1  $f10, 0x38($sp)
/* 096BC4 7F062094 C7AA0030 */  lwc1  $f10, 0x30($sp)
/* 096BC8 7F062098 46062180 */  add.s $f6, $f4, $f6
/* 096BCC 7F06209C 46064201 */  sub.s $f8, $f8, $f6
/* 096BD0 7F0620A0 46086182 */  mul.s $f6, $f12, $f8
/* 096BD4 7F0620A4 00000000 */  nop
/* 096BD8 7F0620A8 46146202 */  mul.s $f8, $f12, $f20
/* 096BDC 7F0620AC 460A4200 */  add.s $f8, $f8, $f10
/* 096BE0 7F0620B0 C44A0008 */  lwc1  $f10, 8($v0)
/* 096BE4 7F0620B4 46085281 */  sub.s $f10, $f10, $f8
/* 096BE8 7F0620B8 46005202 */  mul.s $f8, $f10, $f0
/* 096BEC 7F0620BC 46083281 */  sub.s $f10, $f6, $f8
/* 096BF0 7F0620C0 C7A60034 */  lwc1  $f6, 0x34($sp)
/* 096BF4 7F0620C4 E7AA00D4 */  swc1  $f10, 0xd4($sp)
/* 096BF8 7F0620C8 C6220014 */  lwc1  $f2, 0x14($s1)
/* 096BFC 7F0620CC C6200010 */  lwc1  $f0, 0x10($s1)
/* 096C00 7F0620D0 4602A202 */  mul.s $f8, $f20, $f2
/* 096C04 7F0620D4 46083180 */  add.s $f6, $f6, $f8
/* 096C08 7F0620D8 C4480004 */  lwc1  $f8, 4($v0)
/* 096C0C 7F0620DC 46064201 */  sub.s $f8, $f8, $f6
/* 096C10 7F0620E0 46080182 */  mul.s $f6, $f0, $f8
/* 096C14 7F0620E4 00000000 */  nop
/* 096C18 7F0620E8 46140202 */  mul.s $f8, $f0, $f20
/* 096C1C 7F0620EC 46044200 */  add.s $f8, $f8, $f4
/* 096C20 7F0620F0 C4440000 */  lwc1  $f4, ($v0)
/* 096C24 7F0620F4 46082101 */  sub.s $f4, $f4, $f8
/* 096C28 7F0620F8 46022202 */  mul.s $f8, $f4, $f2
/* 096C2C 7F0620FC 46083101 */  sub.s $f4, $f6, $f8
/* 096C30 7F062100 C7A60038 */  lwc1  $f6, 0x38($sp)
/* 096C34 7F062104 46067032 */  c.eq.s $f14, $f6
/* 096C38 7F062108 E7A400D8 */  swc1  $f4, 0xd8($sp)
/* 096C3C 7F06210C 45000008 */  bc1f  .L7F062130
/* 096C40 7F062110 00000000 */   nop
/* 096C44 7F062114 460A7032 */  c.eq.s $f14, $f10
/* 096C48 7F062118 00000000 */  nop
/* 096C4C 7F06211C 45020005 */  bc1fl .L7F062134
/* 096C50 7F062120 27A400D0 */   addiu $a0, $sp, 0xd0
/* 096C54 7F062124 46047032 */  c.eq.s $f14, $f4
/* 096C58 7F062128 00000000 */  nop
/* 096C5C 7F06212C 4501000F */  bc1t  .L7F06216C
.L7F062130:
/* 096C60 7F062130 27A400D0 */   addiu $a0, $sp, 0xd0
.L7F062134:
/* 096C64 7F062134 27A500D4 */  addiu $a1, $sp, 0xd4
/* 096C68 7F062138 0C007DD4 */  jal   guNormalize
/* 096C6C 7F06213C 27A600D8 */   addiu $a2, $sp, 0xd8
/* 096C70 7F062140 C7A000F4 */  lwc1  $f0, 0xf4($sp)
/* 096C74 7F062144 C7A800D0 */  lwc1  $f8, 0xd0($sp)
/* 096C78 7F062148 C7AA00D4 */  lwc1  $f10, 0xd4($sp)
/* 096C7C 7F06214C 46004182 */  mul.s $f6, $f8, $f0
/* 096C80 7F062150 C7A800D8 */  lwc1  $f8, 0xd8($sp)
/* 096C84 7F062154 46005102 */  mul.s $f4, $f10, $f0
/* 096C88 7F062158 E7A600D0 */  swc1  $f6, 0xd0($sp)
/* 096C8C 7F06215C 46004182 */  mul.s $f6, $f8, $f0
/* 096C90 7F062160 E7A400D4 */  swc1  $f4, 0xd4($sp)
/* 096C94 7F062164 10000005 */  b     .L7F06217C
/* 096C98 7F062168 E7A600D8 */   swc1  $f6, 0xd8($sp)
.L7F06216C:
/* 096C9C 7F06216C C7AA00F4 */  lwc1  $f10, 0xf4($sp)
/* 096CA0 7F062170 E7AE00D0 */  swc1  $f14, 0xd0($sp)
/* 096CA4 7F062174 E7AE00D8 */  swc1  $f14, 0xd8($sp)
/* 096CA8 7F062178 E7AA00D4 */  swc1  $f10, 0xd4($sp)
.L7F06217C:
/* 096CAC 7F06217C C6240014 */  lwc1  $f4, 0x14($s1)
/* 096CB0 7F062180 C7A800D8 */  lwc1  $f8, 0xd8($sp)
/* 096CB4 7F062184 C7AA00D4 */  lwc1  $f10, 0xd4($sp)
/* 096CB8 7F062188 27A400DC */  addiu $a0, $sp, 0xdc
/* 096CBC 7F06218C 46082182 */  mul.s $f6, $f4, $f8
/* 096CC0 7F062190 C6240018 */  lwc1  $f4, 0x18($s1)
/* 096CC4 7F062194 27A500E0 */  addiu $a1, $sp, 0xe0
/* 096CC8 7F062198 27A600E4 */  addiu $a2, $sp, 0xe4
/* 096CCC 7F06219C 46045102 */  mul.s $f4, $f10, $f4
/* 096CD0 7F0621A0 46043181 */  sub.s $f6, $f6, $f4
/* 096CD4 7F0621A4 E7A600DC */  swc1  $f6, 0xdc($sp)
/* 096CD8 7F0621A8 C6240018 */  lwc1  $f4, 0x18($s1)
/* 096CDC 7F0621AC C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 096CE0 7F0621B0 E7AA0038 */  swc1  $f10, 0x38($sp)
/* 096CE4 7F0621B4 C62A0010 */  lwc1  $f10, 0x10($s1)
/* 096CE8 7F0621B8 46062102 */  mul.s $f4, $f4, $f6
/* 096CEC 7F0621BC 00000000 */  nop
/* 096CF0 7F0621C0 460A4202 */  mul.s $f8, $f8, $f10
/* 096CF4 7F0621C4 46082281 */  sub.s $f10, $f4, $f8
/* 096CF8 7F0621C8 C7A80038 */  lwc1  $f8, 0x38($sp)
/* 096CFC 7F0621CC E7AA00E0 */  swc1  $f10, 0xe0($sp)
/* 096D00 7F0621D0 C6240010 */  lwc1  $f4, 0x10($s1)
/* 096D04 7F0621D4 46082282 */  mul.s $f10, $f4, $f8
/* 096D08 7F0621D8 C6240014 */  lwc1  $f4, 0x14($s1)
/* 096D0C 7F0621DC 46043202 */  mul.s $f8, $f6, $f4
/* 096D10 7F0621E0 46085181 */  sub.s $f6, $f10, $f8
/* 096D14 7F0621E4 0C007DD4 */  jal   guNormalize
/* 096D18 7F0621E8 E7A600E4 */   swc1  $f6, 0xe4($sp)
/* 096D1C 7F0621EC C7A000F4 */  lwc1  $f0, 0xf4($sp)
/* 096D20 7F0621F0 C7A400DC */  lwc1  $f4, 0xdc($sp)
/* 096D24 7F0621F4 C7A800E0 */  lwc1  $f8, 0xe0($sp)
/* 096D28 7F0621F8 24010016 */  li    $at, 22
/* 096D2C 7F0621FC 46002282 */  mul.s $f10, $f4, $f0
/* 096D30 7F062200 C7A400E4 */  lwc1  $f4, 0xe4($sp)
/* 096D34 7F062204 46004182 */  mul.s $f6, $f8, $f0
/* 096D38 7F062208 E7AA00DC */  swc1  $f10, 0xdc($sp)
/* 096D3C 7F06220C 46002282 */  mul.s $f10, $f4, $f0
/* 096D40 7F062210 E7A600E0 */  swc1  $f6, 0xe0($sp)
/* 096D44 7F062214 E7AA00E4 */  swc1  $f10, 0xe4($sp)
/* 096D48 7F062218 822A0001 */  lb    $t2, 1($s1)
/* 096D4C 7F06221C 15410005 */  bne   $t2, $at, .L7F062234
/* 096D50 7F062220 00000000 */   nop
/* 096D54 7F062224 0FC2F5B1 */  jal   dynAllocateVertices
/* 096D58 7F062228 24040008 */   li    $a0, 8
/* 096D5C 7F06222C 10000004 */  b     .L7F062240
/* 096D60 7F062230 00408025 */   move  $s0, $v0
.L7F062234:
/* 096D64 7F062234 0FC2F5B1 */  jal   dynAllocateVertices
/* 096D68 7F062238 24040004 */   li    $a0, 4
/* 096D6C 7F06223C 00408025 */  move  $s0, $v0
.L7F062240:
/* 096D70 7F062240 0FC2F5B8 */  jal   dynAllocateMatrix
/* 096D74 7F062244 00000000 */   nop
/* 096D78 7F062248 AFA20158 */  sw    $v0, 0x158($sp)
/* 096D7C 7F06224C 27A400FC */  addiu $a0, $sp, 0xfc
/* 096D80 7F062250 0FC16259 */  jal   matrix_4x4_set_identity_and_position
/* 096D84 7F062254 27A50118 */   addiu $a1, $sp, 0x118
/* 096D88 7F062258 3C018005 */  lui   $at, %hi(D_80053EB0)
/* 096D8C 7F06225C C42C3EB0 */  lwc1  $f12, %lo(D_80053EB0)($at)
/* 096D90 7F062260 0FC1629F */  jal   matrix_scalar_multiply
/* 096D94 7F062264 27A50118 */   addiu $a1, $sp, 0x118
/* 096D98 7F062268 8FA400A8 */  lw    $a0, 0xa8($sp)
/* 096D9C 7F06226C 0FC16026 */  jal   matrix_4x4_multiply_homogeneous_in_place
/* 096DA0 7F062270 27A50118 */   addiu $a1, $sp, 0x118
/* 096DA4 7F062274 27A40118 */  addiu $a0, $sp, 0x118
/* 096DA8 7F062278 0FC16327 */  jal   matrix_4x4_f32_to_s32
/* 096DAC 7F06227C 8FA50158 */   lw    $a1, 0x158($sp)
/* 096DB0 7F062280 27A20108 */  addiu $v0, $sp, 0x108
/* 096DB4 7F062284 8C410000 */  lw    $at, ($v0)
/* 096DB8 7F062288 AE010000 */  sw    $at, ($s0)
/* 096DBC 7F06228C 8C4D0004 */  lw    $t5, 4($v0)
/* 096DC0 7F062290 AE0D0004 */  sw    $t5, 4($s0)
/* 096DC4 7F062294 8C410008 */  lw    $at, 8($v0)
/* 096DC8 7F062298 AE010008 */  sw    $at, 8($s0)
/* 096DCC 7F06229C 8C4D000C */  lw    $t5, 0xc($v0)
/* 096DD0 7F0622A0 AE0D000C */  sw    $t5, 0xc($s0)
/* 096DD4 7F0622A4 8C410000 */  lw    $at, ($v0)
/* 096DD8 7F0622A8 AE010010 */  sw    $at, 0x10($s0)
/* 096DDC 7F0622AC 8C4E0004 */  lw    $t6, 4($v0)
/* 096DE0 7F0622B0 AE0E0014 */  sw    $t6, 0x14($s0)
/* 096DE4 7F0622B4 8C410008 */  lw    $at, 8($v0)
/* 096DE8 7F0622B8 AE010018 */  sw    $at, 0x18($s0)
/* 096DEC 7F0622BC 8C4E000C */  lw    $t6, 0xc($v0)
/* 096DF0 7F0622C0 AE0E001C */  sw    $t6, 0x1c($s0)
/* 096DF4 7F0622C4 8C410000 */  lw    $at, ($v0)
/* 096DF8 7F0622C8 AE010020 */  sw    $at, 0x20($s0)
/* 096DFC 7F0622CC 8C4A0004 */  lw    $t2, 4($v0)
/* 096E00 7F0622D0 AE0A0024 */  sw    $t2, 0x24($s0)
/* 096E04 7F0622D4 8C410008 */  lw    $at, 8($v0)
/* 096E08 7F0622D8 AE010028 */  sw    $at, 0x28($s0)
/* 096E0C 7F0622DC 8C4A000C */  lw    $t2, 0xc($v0)
/* 096E10 7F0622E0 AE0A002C */  sw    $t2, 0x2c($s0)
/* 096E14 7F0622E4 8C410000 */  lw    $at, ($v0)
/* 096E18 7F0622E8 AE010030 */  sw    $at, 0x30($s0)
/* 096E1C 7F0622EC 8C4D0004 */  lw    $t5, 4($v0)
/* 096E20 7F0622F0 AE0D0034 */  sw    $t5, 0x34($s0)
/* 096E24 7F0622F4 8C410008 */  lw    $at, 8($v0)
/* 096E28 7F0622F8 AE010038 */  sw    $at, 0x38($s0)
/* 096E2C 7F0622FC 8C4D000C */  lw    $t5, 0xc($v0)
/* 096E30 7F062300 24010016 */  li    $at, 22
/* 096E34 7F062304 AE0D003C */  sw    $t5, 0x3c($s0)
/* 096E38 7F062308 82230001 */  lb    $v1, 1($s1)
/* 096E3C 7F06230C 54610023 */  bnel  $v1, $at, .L7F06239C
/* 096E40 7F062310 24010017 */   li    $at, 23
/* 096E44 7F062314 8C410000 */  lw    $at, ($v0)
/* 096E48 7F062318 AE010040 */  sw    $at, 0x40($s0)
/* 096E4C 7F06231C 8C4B0004 */  lw    $t3, 4($v0)
/* 096E50 7F062320 AE0B0044 */  sw    $t3, 0x44($s0)
/* 096E54 7F062324 8C410008 */  lw    $at, 8($v0)
/* 096E58 7F062328 AE010048 */  sw    $at, 0x48($s0)
/* 096E5C 7F06232C 8C4B000C */  lw    $t3, 0xc($v0)
/* 096E60 7F062330 AE0B004C */  sw    $t3, 0x4c($s0)
/* 096E64 7F062334 8C410000 */  lw    $at, ($v0)
/* 096E68 7F062338 AE010050 */  sw    $at, 0x50($s0)
/* 096E6C 7F06233C 8C580004 */  lw    $t8, 4($v0)
/* 096E70 7F062340 AE180054 */  sw    $t8, 0x54($s0)
/* 096E74 7F062344 8C410008 */  lw    $at, 8($v0)
/* 096E78 7F062348 AE010058 */  sw    $at, 0x58($s0)
/* 096E7C 7F06234C 8C58000C */  lw    $t8, 0xc($v0)
/* 096E80 7F062350 AE18005C */  sw    $t8, 0x5c($s0)
/* 096E84 7F062354 8C410000 */  lw    $at, ($v0)
/* 096E88 7F062358 AE010060 */  sw    $at, 0x60($s0)
/* 096E8C 7F06235C 8C4A0004 */  lw    $t2, 4($v0)
/* 096E90 7F062360 AE0A0064 */  sw    $t2, 0x64($s0)
/* 096E94 7F062364 8C410008 */  lw    $at, 8($v0)
/* 096E98 7F062368 AE010068 */  sw    $at, 0x68($s0)
/* 096E9C 7F06236C 8C4A000C */  lw    $t2, 0xc($v0)
/* 096EA0 7F062370 AE0A006C */  sw    $t2, 0x6c($s0)
/* 096EA4 7F062374 8C410000 */  lw    $at, ($v0)
/* 096EA8 7F062378 AE010070 */  sw    $at, 0x70($s0)
/* 096EAC 7F06237C 8C4F0004 */  lw    $t7, 4($v0)
/* 096EB0 7F062380 AE0F0074 */  sw    $t7, 0x74($s0)
/* 096EB4 7F062384 8C410008 */  lw    $at, 8($v0)
/* 096EB8 7F062388 AE010078 */  sw    $at, 0x78($s0)
/* 096EBC 7F06238C 8C4F000C */  lw    $t7, 0xc($v0)
/* 096EC0 7F062390 AE0F007C */  sw    $t7, 0x7c($s0)
/* 096EC4 7F062394 82230001 */  lb    $v1, 1($s1)
/* 096EC8 7F062398 24010017 */  li    $at, 23
.L7F06239C:
/* 096ECC 7F06239C 5461004F */  bnel  $v1, $at, .L7F0624DC
/* 096ED0 7F0623A0 3C014120 */   lui   $at, 0x4120
/* 096ED4 7F0623A4 C6280010 */  lwc1  $f8, 0x10($s1)
/* 096ED8 7F0623A8 C7A400FC */  lwc1  $f4, 0xfc($sp)
/* 096EDC 7F0623AC 8FA400A8 */  lw    $a0, 0xa8($sp)
/* 096EE0 7F0623B0 46144182 */  mul.s $f6, $f8, $f20
/* 096EE4 7F0623B4 27A5009C */  addiu $a1, $sp, 0x9c
/* 096EE8 7F0623B8 46043280 */  add.s $f10, $f6, $f4
/* 096EEC 7F0623BC C7A40100 */  lwc1  $f4, 0x100($sp)
/* 096EF0 7F0623C0 E7AA009C */  swc1  $f10, 0x9c($sp)
/* 096EF4 7F0623C4 C6280014 */  lwc1  $f8, 0x14($s1)
/* 096EF8 7F0623C8 46144182 */  mul.s $f6, $f8, $f20
/* 096EFC 7F0623CC 46043280 */  add.s $f10, $f6, $f4
/* 096F00 7F0623D0 C7A40104 */  lwc1  $f4, 0x104($sp)
/* 096F04 7F0623D4 E7AA00A0 */  swc1  $f10, 0xa0($sp)
/* 096F08 7F0623D8 C6280018 */  lwc1  $f8, 0x18($s1)
/* 096F0C 7F0623DC 46144182 */  mul.s $f6, $f8, $f20
/* 096F10 7F0623E0 46043280 */  add.s $f10, $f6, $f4
/* 096F14 7F0623E4 0FC1611D */  jal   mtx4TransformVecInPlace
/* 096F18 7F0623E8 E7AA00A4 */   swc1  $f10, 0xa4($sp)
/* 096F1C 7F0623EC 3C014120 */  li    $at, 0x41200000 # 10.000000
/* 096F20 7F0623F0 44813000 */  mtc1  $at, $f6
/* 096F24 7F0623F4 C7A800F4 */  lwc1  $f8, 0xf4($sp)
/* 096F28 7F0623F8 C7AE00A4 */  lwc1  $f14, 0xa4($sp)
/* 096F2C 7F0623FC 27A40088 */  addiu $a0, $sp, 0x88
/* 096F30 7F062400 46064003 */  div.s $f0, $f8, $f6
/* 096F34 7F062404 27A60090 */  addiu $a2, $sp, 0x90
/* 096F38 7F062408 46007087 */  neg.s $f2, $f14
/* 096F3C 7F06240C 44051000 */  mfc1  $a1, $f2
/* 096F40 7F062410 E7A0008C */  swc1  $f0, 0x8c($sp)
/* 096F44 7F062414 0FC1E03C */  jal   divide3DCoordinates
/* 096F48 7F062418 E7A00088 */   swc1  $f0, 0x88($sp)
/* 096F4C 7F06241C 3C014000 */  li    $at, 0x40000000 # 2.000000
/* 096F50 7F062420 C7B00090 */  lwc1  $f16, 0x90($sp)
/* 096F54 7F062424 44812000 */  mtc1  $at, $f4
/* 096F58 7F062428 3C013F00 */  li    $at, 0x3F000000 # 0.500000
/* 096F5C 7F06242C 4604803C */  c.lt.s $f16, $f4
/* 096F60 7F062430 00000000 */  nop
/* 096F64 7F062434 4500000E */  bc1f  .L7F062470
/* 096F68 7F062438 00000000 */   nop
/* 096F6C 7F06243C 44815000 */  mtc1  $at, $f10
/* 096F70 7F062440 C7A2009C */  lwc1  $f2, 0x9c($sp)
/* 096F74 7F062444 C7AC00A0 */  lwc1  $f12, 0xa0($sp)
/* 096F78 7F062448 460A8002 */  mul.s $f0, $f16, $f10
/* 096F7C 7F06244C C7AE00A4 */  lwc1  $f14, 0xa4($sp)
/* 096F80 7F062450 46001082 */  mul.s $f2, $f2, $f0
/* 096F84 7F062454 00000000 */  nop
/* 096F88 7F062458 46006302 */  mul.s $f12, $f12, $f0
/* 096F8C 7F06245C 00000000 */  nop
/* 096F90 7F062460 46007382 */  mul.s $f14, $f14, $f0
/* 096F94 7F062464 E7A2009C */  swc1  $f2, 0x9c($sp)
/* 096F98 7F062468 E7AC00A0 */  swc1  $f12, 0xa0($sp)
/* 096F9C 7F06246C E7AE00A4 */  swc1  $f14, 0xa4($sp)
.L7F062470:
/* 096FA0 7F062470 0FC1E111 */  jal   currentPlayerGetViewToWorldMtxf
/* 096FA4 7F062474 00000000 */   nop
/* 096FA8 7F062478 00402025 */  move  $a0, $v0
/* 096FAC 7F06247C 0FC1611D */  jal   mtx4TransformVecInPlace
/* 096FB0 7F062480 27A5009C */   addiu $a1, $sp, 0x9c
/* 096FB4 7F062484 C7A2009C */  lwc1  $f2, 0x9c($sp)
/* 096FB8 7F062488 C7A800FC */  lwc1  $f8, 0xfc($sp)
/* 096FBC 7F06248C C7AC00A0 */  lwc1  $f12, 0xa0($sp)
/* 096FC0 7F062490 C7A60100 */  lwc1  $f6, 0x100($sp)
/* 096FC4 7F062494 46081081 */  sub.s $f2, $f2, $f8
/* 096FC8 7F062498 3C014120 */  li    $at, 0x41200000 # 10.000000
/* 096FCC 7F06249C 44810000 */  mtc1  $at, $f0
/* 096FD0 7F0624A0 46066301 */  sub.s $f12, $f12, $f6
/* 096FD4 7F0624A4 C7AE00A4 */  lwc1  $f14, 0xa4($sp)
/* 096FD8 7F0624A8 C7A40104 */  lwc1  $f4, 0x104($sp)
/* 096FDC 7F0624AC 46001282 */  mul.s $f10, $f2, $f0
/* 096FE0 7F0624B0 E7AC00A0 */  swc1  $f12, 0xa0($sp)
/* 096FE4 7F0624B4 46047381 */  sub.s $f14, $f14, $f4
/* 096FE8 7F0624B8 46006202 */  mul.s $f8, $f12, $f0
/* 096FEC 7F0624BC E7A2009C */  swc1  $f2, 0x9c($sp)
/* 096FF0 7F0624C0 46007182 */  mul.s $f6, $f14, $f0
/* 096FF4 7F0624C4 E7AA00C4 */  swc1  $f10, 0xc4($sp)
/* 096FF8 7F0624C8 E7AE00A4 */  swc1  $f14, 0xa4($sp)
/* 096FFC 7F0624CC E7A800C8 */  swc1  $f8, 0xc8($sp)
/* 097000 7F0624D0 1000000E */  b     .L7F06250C
/* 097004 7F0624D4 E7A600CC */   swc1  $f6, 0xcc($sp)
/* 097008 7F0624D8 3C014120 */  li    $at, 0x41200000 # 10.000000
.L7F0624DC:
/* 09700C 7F0624DC 44812000 */  mtc1  $at, $f4
/* 097010 7F0624E0 C62A0010 */  lwc1  $f10, 0x10($s1)
/* 097014 7F0624E4 4604A002 */  mul.s $f0, $f20, $f4
/* 097018 7F0624E8 00000000 */  nop
/* 09701C 7F0624EC 46005202 */  mul.s $f8, $f10, $f0
/* 097020 7F0624F0 E7A800C4 */  swc1  $f8, 0xc4($sp)
/* 097024 7F0624F4 C6260014 */  lwc1  $f6, 0x14($s1)
/* 097028 7F0624F8 46003102 */  mul.s $f4, $f6, $f0
/* 09702C 7F0624FC E7A400C8 */  swc1  $f4, 0xc8($sp)
/* 097030 7F062500 C62A0018 */  lwc1  $f10, 0x18($s1)
/* 097034 7F062504 46005202 */  mul.s $f8, $f10, $f0
/* 097038 7F062508 E7A800CC */  swc1  $f8, 0xcc($sp)
.L7F06250C:
/* 09703C 7F06250C C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 097040 7F062510 8FA500B0 */  lw    $a1, 0xb0($sp)
/* 097044 7F062514 3C018005 */  lui   $at, %hi(D_80053EB4)
/* 097048 7F062518 4600310D */  trunc.w.s $f4, $f6
/* 09704C 7F06251C 44192000 */  mfc1  $t9, $f4
/* 097050 7F062520 00000000 */  nop
/* 097054 7F062524 A6190000 */  sh    $t9, ($s0)
/* 097058 7F062528 C7AA00D4 */  lwc1  $f10, 0xd4($sp)
/* 09705C 7F06252C 4600520D */  trunc.w.s $f8, $f10
/* 097060 7F062530 440E4000 */  mfc1  $t6, $f8
/* 097064 7F062534 00000000 */  nop
/* 097068 7F062538 A60E0002 */  sh    $t6, 2($s0)
/* 09706C 7F06253C C7A600D8 */  lwc1  $f6, 0xd8($sp)
/* 097070 7F062540 4600310D */  trunc.w.s $f4, $f6
/* 097074 7F062544 44092000 */  mfc1  $t1, $f4
/* 097078 7F062548 00000000 */  nop
/* 09707C 7F06254C A6090004 */  sh    $t1, 4($s0)
/* 097080 7F062550 90AA0004 */  lbu   $t2, 4($a1)
/* 097084 7F062554 A600000A */  sh    $zero, 0xa($s0)
/* 097088 7F062558 000A6140 */  sll   $t4, $t2, 5
/* 09708C 7F06255C A60C0008 */  sh    $t4, 8($s0)
/* 097090 7F062560 C7AA00D0 */  lwc1  $f10, 0xd0($sp)
/* 097094 7F062564 46005207 */  neg.s $f8, $f10
/* 097098 7F062568 4600418D */  trunc.w.s $f6, $f8
/* 09709C 7F06256C 440D3000 */  mfc1  $t5, $f6
/* 0970A0 7F062570 00000000 */  nop
/* 0970A4 7F062574 A60D0010 */  sh    $t5, 0x10($s0)
/* 0970A8 7F062578 C7A400D4 */  lwc1  $f4, 0xd4($sp)
/* 0970AC 7F06257C 46002287 */  neg.s $f10, $f4
/* 0970B0 7F062580 4600520D */  trunc.w.s $f8, $f10
/* 0970B4 7F062584 440B4000 */  mfc1  $t3, $f8
/* 0970B8 7F062588 00000000 */  nop
/* 0970BC 7F06258C A60B0012 */  sh    $t3, 0x12($s0)
/* 0970C0 7F062590 C7A600D8 */  lwc1  $f6, 0xd8($sp)
/* 0970C4 7F062594 A6000018 */  sh    $zero, 0x18($s0)
/* 0970C8 7F062598 A600001A */  sh    $zero, 0x1a($s0)
/* 0970CC 7F06259C 46003107 */  neg.s $f4, $f6
/* 0970D0 7F0625A0 4600228D */  trunc.w.s $f10, $f4
/* 0970D4 7F0625A4 44185000 */  mfc1  $t8, $f10
/* 0970D8 7F0625A8 00000000 */  nop
/* 0970DC 7F0625AC A6180014 */  sh    $t8, 0x14($s0)
/* 0970E0 7F0625B0 C7A800D0 */  lwc1  $f8, 0xd0($sp)
/* 0970E4 7F0625B4 C4263EB4 */  lwc1  $f6, %lo(D_80053EB4)($at)
/* 0970E8 7F0625B8 C7AA00C4 */  lwc1  $f10, 0xc4($sp)
/* 0970EC 7F0625BC 3C018005 */  lui   $at, %hi(D_80053EB8)
/* 0970F0 7F0625C0 46064102 */  mul.s $f4, $f8, $f6
/* 0970F4 7F0625C4 460A2200 */  add.s $f8, $f4, $f10
/* 0970F8 7F0625C8 4600418D */  trunc.w.s $f6, $f8
/* 0970FC 7F0625CC 440A3000 */  mfc1  $t2, $f6
/* 097100 7F0625D0 00000000 */  nop
/* 097104 7F0625D4 A60A0020 */  sh    $t2, 0x20($s0)
/* 097108 7F0625D8 C7A400D4 */  lwc1  $f4, 0xd4($sp)
/* 09710C 7F0625DC C42A3EB8 */  lwc1  $f10, %lo(D_80053EB8)($at)
/* 097110 7F0625E0 C7A600C8 */  lwc1  $f6, 0xc8($sp)
/* 097114 7F0625E4 3C018005 */  lui   $at, %hi(D_80053EBC)
/* 097118 7F0625E8 460A2202 */  mul.s $f8, $f4, $f10
/* 09711C 7F0625EC 46064100 */  add.s $f4, $f8, $f6
/* 097120 7F0625F0 4600228D */  trunc.w.s $f10, $f4
/* 097124 7F0625F4 440F5000 */  mfc1  $t7, $f10
/* 097128 7F0625F8 00000000 */  nop
/* 09712C 7F0625FC A60F0022 */  sh    $t7, 0x22($s0)
/* 097130 7F062600 C7A800D8 */  lwc1  $f8, 0xd8($sp)
/* 097134 7F062604 C4263EBC */  lwc1  $f6, %lo(D_80053EBC)($at)
/* 097138 7F062608 C7AA00CC */  lwc1  $f10, 0xcc($sp)
/* 09713C 7F06260C 3C018005 */  lui   $at, %hi(D_80053EC0)
/* 097140 7F062610 46064102 */  mul.s $f4, $f8, $f6
/* 097144 7F062614 460A2200 */  add.s $f8, $f4, $f10
/* 097148 7F062618 4600418D */  trunc.w.s $f6, $f8
/* 09714C 7F06261C 44193000 */  mfc1  $t9, $f6
/* 097150 7F062620 00000000 */  nop
/* 097154 7F062624 A6190024 */  sh    $t9, 0x24($s0)
/* 097158 7F062628 90AB0004 */  lbu   $t3, 4($a1)
/* 09715C 7F06262C 000B7140 */  sll   $t6, $t3, 5
/* 097160 7F062630 A60E0028 */  sh    $t6, 0x28($s0)
/* 097164 7F062634 90B80005 */  lbu   $t8, 5($a1)
/* 097168 7F062638 00184940 */  sll   $t1, $t8, 5
/* 09716C 7F06263C A609002A */  sh    $t1, 0x2a($s0)
/* 097170 7F062640 C42A3EC0 */  lwc1  $f10, %lo(D_80053EC0)($at)
/* 097174 7F062644 C7A400D0 */  lwc1  $f4, 0xd0($sp)
/* 097178 7F062648 C7A600C4 */  lwc1  $f6, 0xc4($sp)
/* 09717C 7F06264C 3C018005 */  lui   $at, %hi(D_80053EC4)
/* 097180 7F062650 460A2202 */  mul.s $f8, $f4, $f10
/* 097184 7F062654 46083101 */  sub.s $f4, $f6, $f8
/* 097188 7F062658 4600228D */  trunc.w.s $f10, $f4
/* 09718C 7F06265C 440C5000 */  mfc1  $t4, $f10
/* 097190 7F062660 00000000 */  nop
/* 097194 7F062664 A60C0030 */  sh    $t4, 0x30($s0)
/* 097198 7F062668 C4283EC4 */  lwc1  $f8, %lo(D_80053EC4)($at)
/* 09719C 7F06266C C7A600D4 */  lwc1  $f6, 0xd4($sp)
/* 0971A0 7F062670 C7AA00C8 */  lwc1  $f10, 0xc8($sp)
/* 0971A4 7F062674 3C018005 */  lui   $at, %hi(D_80053EC8)
/* 0971A8 7F062678 46083102 */  mul.s $f4, $f6, $f8
/* 0971AC 7F06267C 46045181 */  sub.s $f6, $f10, $f4
/* 0971B0 7F062680 4600320D */  trunc.w.s $f8, $f6
/* 0971B4 7F062684 440D4000 */  mfc1  $t5, $f8
/* 0971B8 7F062688 00000000 */  nop
/* 0971BC 7F06268C A60D0032 */  sh    $t5, 0x32($s0)
/* 0971C0 7F062690 C4243EC8 */  lwc1  $f4, %lo(D_80053EC8)($at)
/* 0971C4 7F062694 C7AA00D8 */  lwc1  $f10, 0xd8($sp)
/* 0971C8 7F062698 C7A800CC */  lwc1  $f8, 0xcc($sp)
/* 0971CC 7F06269C A6000038 */  sh    $zero, 0x38($s0)
/* 0971D0 7F0626A0 46045182 */  mul.s $f6, $f10, $f4
/* 0971D4 7F0626A4 24010016 */  li    $at, 22
/* 0971D8 7F0626A8 46064281 */  sub.s $f10, $f8, $f6
/* 0971DC 7F0626AC 4600510D */  trunc.w.s $f4, $f10
/* 0971E0 7F0626B0 440B2000 */  mfc1  $t3, $f4
/* 0971E4 7F0626B4 00000000 */  nop
/* 0971E8 7F0626B8 A60B0034 */  sh    $t3, 0x34($s0)
/* 0971EC 7F0626BC 90AE0005 */  lbu   $t6, 5($a1)
/* 0971F0 7F0626C0 000EC140 */  sll   $t8, $t6, 5
/* 0971F4 7F0626C4 A618003A */  sh    $t8, 0x3a($s0)
/* 0971F8 7F0626C8 82290001 */  lb    $t1, 1($s1)
/* 0971FC 7F0626CC C7A800FC */  lwc1  $f8, 0xfc($sp)
/* 097200 7F0626D0 8FAA00F8 */  lw    $t2, 0xf8($sp)
/* 097204 7F0626D4 552100AD */  bnel  $t1, $at, .L7F06298C
/* 097208 7F0626D8 8FAC0160 */   lw    $t4, 0x160($sp)
/* 09720C 7F0626DC C54C0000 */  lwc1  $f12, ($t2)
/* 097210 7F0626E0 C5420004 */  lwc1  $f2, 4($t2)
/* 097214 7F0626E4 C7A60100 */  lwc1  $f6, 0x100($sp)
/* 097218 7F0626E8 46086381 */  sub.s $f14, $f12, $f8
/* 09721C 7F0626EC C5400008 */  lwc1  $f0, 8($t2)
/* 097220 7F0626F0 E7A80038 */  swc1  $f8, 0x38($sp)
/* 097224 7F0626F4 46061401 */  sub.s $f16, $f2, $f6
/* 097228 7F0626F8 460E7102 */  mul.s $f4, $f14, $f14
/* 09722C 7F0626FC C7AA0104 */  lwc1  $f10, 0x104($sp)
/* 097230 7F062700 3C018005 */  lui   $at, %hi(D_80053ECC)
/* 097234 7F062704 46108202 */  mul.s $f8, $f16, $f16
/* 097238 7F062708 460A0481 */  sub.s $f18, $f0, $f10
/* 09723C 7F06270C 46082100 */  add.s $f4, $f4, $f8
/* 097240 7F062710 46129202 */  mul.s $f8, $f18, $f18
/* 097244 7F062714 46082100 */  add.s $f4, $f4, $f8
/* 097248 7F062718 E7A40078 */  swc1  $f4, 0x78($sp)
/* 09724C 7F06271C C6280010 */  lwc1  $f8, 0x10($s1)
/* 097250 7F062720 E7A60034 */  swc1  $f6, 0x34($sp)
/* 097254 7F062724 C7A60038 */  lwc1  $f6, 0x38($sp)
/* 097258 7F062728 46144202 */  mul.s $f8, $f8, $f20
/* 09725C 7F06272C 46064200 */  add.s $f8, $f8, $f6
/* 097260 7F062730 C6260014 */  lwc1  $f6, 0x14($s1)
/* 097264 7F062734 46086381 */  sub.s $f14, $f12, $f8
/* 097268 7F062738 46143202 */  mul.s $f8, $f6, $f20
/* 09726C 7F06273C C7A60034 */  lwc1  $f6, 0x34($sp)
/* 097270 7F062740 46064200 */  add.s $f8, $f8, $f6
/* 097274 7F062744 C6260018 */  lwc1  $f6, 0x18($s1)
/* 097278 7F062748 46081401 */  sub.s $f16, $f2, $f8
/* 09727C 7F06274C 46143202 */  mul.s $f8, $f6, $f20
/* 097280 7F062750 460A4180 */  add.s $f6, $f8, $f10
/* 097284 7F062754 460E7202 */  mul.s $f8, $f14, $f14
/* 097288 7F062758 00000000 */  nop
/* 09728C 7F06275C 46108282 */  mul.s $f10, $f16, $f16
/* 097290 7F062760 46060481 */  sub.s $f18, $f0, $f6
/* 097294 7F062764 460A4180 */  add.s $f6, $f8, $f10
/* 097298 7F062768 46129202 */  mul.s $f8, $f18, $f18
/* 09729C 7F06276C 46083280 */  add.s $f10, $f6, $f8
/* 0972A0 7F062770 C7A600C4 */  lwc1  $f6, 0xc4($sp)
/* 0972A4 7F062774 4604503C */  c.lt.s $f10, $f4
/* 0972A8 7F062778 00000000 */  nop
/* 0972AC 7F06277C 4500000B */  bc1f  .L7F0627AC
/* 0972B0 7F062780 00000000 */   nop
/* 0972B4 7F062784 C4243ECC */  lwc1  $f4, %lo(D_80053ECC)($at)
/* 0972B8 7F062788 E7A600B8 */  swc1  $f6, 0xb8($sp)
/* 0972BC 7F06278C 3C018005 */  lui   $at, %hi(D_80053ED0)
/* 0972C0 7F062790 C4263ED0 */  lwc1  $f6, %lo(D_80053ED0)($at)
/* 0972C4 7F062794 C7A800C8 */  lwc1  $f8, 0xc8($sp)
/* 0972C8 7F062798 C7AA00CC */  lwc1  $f10, 0xcc($sp)
/* 0972CC 7F06279C 46062002 */  mul.s $f0, $f4, $f6
/* 0972D0 7F0627A0 E7A800BC */  swc1  $f8, 0xbc($sp)
/* 0972D4 7F0627A4 E7AA00C0 */  swc1  $f10, 0xc0($sp)
/* 0972D8 7F0627A8 E7A000B4 */  swc1  $f0, 0xb4($sp)
.L7F0627AC:
/* 0972DC 7F0627AC C7A000B4 */  lwc1  $f0, 0xb4($sp)
/* 0972E0 7F0627B0 C7A800DC */  lwc1  $f8, 0xdc($sp)
/* 0972E4 7F0627B4 C7A400B8 */  lwc1  $f4, 0xb8($sp)
/* 0972E8 7F0627B8 3C088009 */  lui   $t0, %hi(flareimage5)
/* 0972EC 7F0627BC 46004282 */  mul.s $f10, $f8, $f0
/* 0972F0 7F0627C0 2508D0D8 */  addiu $t0, %lo(flareimage5) # addiu $t0, $t0, -0x2f28
/* 0972F4 7F0627C4 46045180 */  add.s $f6, $f10, $f4
/* 0972F8 7F0627C8 4600320D */  trunc.w.s $f8, $f6
/* 0972FC 7F0627CC 440F4000 */  mfc1  $t7, $f8
/* 097300 7F0627D0 00000000 */  nop
/* 097304 7F0627D4 A60F0040 */  sh    $t7, 0x40($s0)
/* 097308 7F0627D8 C7AA00E0 */  lwc1  $f10, 0xe0($sp)
/* 09730C 7F0627DC C7A600BC */  lwc1  $f6, 0xbc($sp)
/* 097310 7F0627E0 46005102 */  mul.s $f4, $f10, $f0
/* 097314 7F0627E4 46062200 */  add.s $f8, $f4, $f6
/* 097318 7F0627E8 4600428D */  trunc.w.s $f10, $f8
/* 09731C 7F0627EC 44195000 */  mfc1  $t9, $f10
/* 097320 7F0627F0 00000000 */  nop
/* 097324 7F0627F4 A6190042 */  sh    $t9, 0x42($s0)
/* 097328 7F0627F8 C7A400E4 */  lwc1  $f4, 0xe4($sp)
/* 09732C 7F0627FC C7A800C0 */  lwc1  $f8, 0xc0($sp)
/* 097330 7F062800 46002182 */  mul.s $f6, $f4, $f0
/* 097334 7F062804 46083280 */  add.s $f10, $f6, $f8
/* 097338 7F062808 4600510D */  trunc.w.s $f4, $f10
/* 09733C 7F06280C 440E2000 */  mfc1  $t6, $f4
/* 097340 7F062810 00000000 */  nop
/* 097344 7F062814 A60E0044 */  sh    $t6, 0x44($s0)
/* 097348 7F062818 8D180000 */  lw    $t8, ($t0)
/* 09734C 7F06281C 93090004 */  lbu   $t1, 4($t8)
/* 097350 7F062820 00095140 */  sll   $t2, $t1, 5
/* 097354 7F062824 A60A0048 */  sh    $t2, 0x48($s0)
/* 097358 7F062828 8D0C0000 */  lw    $t4, ($t0)
/* 09735C 7F06282C 918F0005 */  lbu   $t7, 5($t4)
/* 097360 7F062830 000F6940 */  sll   $t5, $t7, 5
/* 097364 7F062834 A60D004A */  sh    $t5, 0x4a($s0)
/* 097368 7F062838 C7A800DC */  lwc1  $f8, 0xdc($sp)
/* 09736C 7F06283C C7A600B8 */  lwc1  $f6, 0xb8($sp)
/* 097370 7F062840 46004282 */  mul.s $f10, $f8, $f0
/* 097374 7F062844 460A3101 */  sub.s $f4, $f6, $f10
/* 097378 7F062848 4600220D */  trunc.w.s $f8, $f4
/* 09737C 7F06284C 440B4000 */  mfc1  $t3, $f8
/* 097380 7F062850 00000000 */  nop
/* 097384 7F062854 A60B0050 */  sh    $t3, 0x50($s0)
/* 097388 7F062858 C7AA00E0 */  lwc1  $f10, 0xe0($sp)
/* 09738C 7F06285C C7A600BC */  lwc1  $f6, 0xbc($sp)
/* 097390 7F062860 46005102 */  mul.s $f4, $f10, $f0
/* 097394 7F062864 46043201 */  sub.s $f8, $f6, $f4
/* 097398 7F062868 4600428D */  trunc.w.s $f10, $f8
/* 09739C 7F06286C 44185000 */  mfc1  $t8, $f10
/* 0973A0 7F062870 00000000 */  nop
/* 0973A4 7F062874 A6180052 */  sh    $t8, 0x52($s0)
/* 0973A8 7F062878 C7A400E4 */  lwc1  $f4, 0xe4($sp)
/* 0973AC 7F06287C C7A600C0 */  lwc1  $f6, 0xc0($sp)
/* 0973B0 7F062880 A6000058 */  sh    $zero, 0x58($s0)
/* 0973B4 7F062884 46002202 */  mul.s $f8, $f4, $f0
/* 0973B8 7F062888 A600005A */  sh    $zero, 0x5a($s0)
/* 0973BC 7F06288C 46083281 */  sub.s $f10, $f6, $f8
/* 0973C0 7F062890 4600510D */  trunc.w.s $f4, $f10
/* 0973C4 7F062894 440A2000 */  mfc1  $t2, $f4
/* 0973C8 7F062898 00000000 */  nop
/* 0973CC 7F06289C A60A0054 */  sh    $t2, 0x54($s0)
/* 0973D0 7F0628A0 C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 0973D4 7F0628A4 C7AA00B8 */  lwc1  $f10, 0xb8($sp)
/* 0973D8 7F0628A8 46003202 */  mul.s $f8, $f6, $f0
/* 0973DC 7F0628AC 460A4100 */  add.s $f4, $f8, $f10
/* 0973E0 7F0628B0 4600218D */  trunc.w.s $f6, $f4
/* 0973E4 7F0628B4 440F3000 */  mfc1  $t7, $f6
/* 0973E8 7F0628B8 00000000 */  nop
/* 0973EC 7F0628BC A60F0060 */  sh    $t7, 0x60($s0)
/* 0973F0 7F0628C0 C7A800D4 */  lwc1  $f8, 0xd4($sp)
/* 0973F4 7F0628C4 C7A400BC */  lwc1  $f4, 0xbc($sp)
/* 0973F8 7F0628C8 46004282 */  mul.s $f10, $f8, $f0
/* 0973FC 7F0628CC 46045180 */  add.s $f6, $f10, $f4
/* 097400 7F0628D0 4600320D */  trunc.w.s $f8, $f6
/* 097404 7F0628D4 44194000 */  mfc1  $t9, $f8
/* 097408 7F0628D8 00000000 */  nop
/* 09740C 7F0628DC A6190062 */  sh    $t9, 0x62($s0)
/* 097410 7F0628E0 C7AA00D8 */  lwc1  $f10, 0xd8($sp)
/* 097414 7F0628E4 C7A600C0 */  lwc1  $f6, 0xc0($sp)
/* 097418 7F0628E8 A6000068 */  sh    $zero, 0x68($s0)
/* 09741C 7F0628EC 46005102 */  mul.s $f4, $f10, $f0
/* 097420 7F0628F0 46062200 */  add.s $f8, $f4, $f6
/* 097424 7F0628F4 4600428D */  trunc.w.s $f10, $f8
/* 097428 7F0628F8 440E5000 */  mfc1  $t6, $f10
/* 09742C 7F0628FC 00000000 */  nop
/* 097430 7F062900 A60E0064 */  sh    $t6, 0x64($s0)
/* 097434 7F062904 8D180000 */  lw    $t8, ($t0)
/* 097438 7F062908 93090005 */  lbu   $t1, 5($t8)
/* 09743C 7F06290C 00095140 */  sll   $t2, $t1, 5
/* 097440 7F062910 A60A006A */  sh    $t2, 0x6a($s0)
/* 097444 7F062914 C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 097448 7F062918 C7A400B8 */  lwc1  $f4, 0xb8($sp)
/* 09744C 7F06291C 46003202 */  mul.s $f8, $f6, $f0
/* 097450 7F062920 46082281 */  sub.s $f10, $f4, $f8
/* 097454 7F062924 4600518D */  trunc.w.s $f6, $f10
/* 097458 7F062928 440F3000 */  mfc1  $t7, $f6
/* 09745C 7F06292C 00000000 */  nop
/* 097460 7F062930 A60F0070 */  sh    $t7, 0x70($s0)
/* 097464 7F062934 C7A800D4 */  lwc1  $f8, 0xd4($sp)
/* 097468 7F062938 C7A400BC */  lwc1  $f4, 0xbc($sp)
/* 09746C 7F06293C 46004282 */  mul.s $f10, $f8, $f0
/* 097470 7F062940 460A2181 */  sub.s $f6, $f4, $f10
/* 097474 7F062944 4600320D */  trunc.w.s $f8, $f6
/* 097478 7F062948 44194000 */  mfc1  $t9, $f8
/* 09747C 7F06294C 00000000 */  nop
/* 097480 7F062950 A6190072 */  sh    $t9, 0x72($s0)
/* 097484 7F062954 C7AA00D8 */  lwc1  $f10, 0xd8($sp)
/* 097488 7F062958 C7A400C0 */  lwc1  $f4, 0xc0($sp)
/* 09748C 7F06295C 46005182 */  mul.s $f6, $f10, $f0
/* 097490 7F062960 46062201 */  sub.s $f8, $f4, $f6
/* 097494 7F062964 4600428D */  trunc.w.s $f10, $f8
/* 097498 7F062968 440E5000 */  mfc1  $t6, $f10
/* 09749C 7F06296C 00000000 */  nop
/* 0974A0 7F062970 A60E0074 */  sh    $t6, 0x74($s0)
/* 0974A4 7F062974 8D180000 */  lw    $t8, ($t0)
/* 0974A8 7F062978 93090004 */  lbu   $t1, 4($t8)
/* 0974AC 7F06297C A600007A */  sh    $zero, 0x7a($s0)
/* 0974B0 7F062980 00095140 */  sll   $t2, $t1, 5
/* 0974B4 7F062984 A60A0078 */  sh    $t2, 0x78($s0)
/* 0974B8 7F062988 8FAC0160 */  lw    $t4, 0x160($sp)
.L7F06298C:
/* 0974BC 7F06298C 3C0DB600 */  lui   $t5, 0xb600
/* 0974C0 7F062990 24192000 */  li    $t9, 8192
/* 0974C4 7F062994 258F0008 */  addiu $t7, $t4, 8
/* 0974C8 7F062998 AFAF0160 */  sw    $t7, 0x160($sp)
/* 0974CC 7F06299C AD990004 */  sw    $t9, 4($t4)
/* 0974D0 7F0629A0 AD8D0000 */  sw    $t5, ($t4)
/* 0974D4 7F0629A4 8FAB0160 */  lw    $t3, 0x160($sp)
/* 0974D8 7F0629A8 3C180102 */  lui   $t8, (0x01020040 >> 16) # lui $t8, 0x102
/* 0974DC 7F0629AC 37180040 */  ori   $t8, (0x01020040 & 0xFFFF) # ori $t8, $t8, 0x40
/* 0974E0 7F0629B0 256E0008 */  addiu $t6, $t3, 8
/* 0974E4 7F0629B4 AFAE0160 */  sw    $t6, 0x160($sp)
/* 0974E8 7F0629B8 AD780000 */  sw    $t8, ($t3)
/* 0974EC 7F0629BC 8FA40158 */  lw    $a0, 0x158($sp)
/* 0974F0 7F0629C0 0C003A2C */  jal   osVirtualToPhysical
/* 0974F4 7F0629C4 AFAB006C */   sw    $t3, 0x6c($sp)
/* 0974F8 7F0629C8 8FA3006C */  lw    $v1, 0x6c($sp)
/* 0974FC 7F0629CC 3C088009 */  lui   $t0, %hi(flareimage5)
/* 097500 7F0629D0 24010016 */  li    $at, 22
/* 097504 7F0629D4 AC620004 */  sw    $v0, 4($v1)
/* 097508 7F0629D8 82290001 */  lb    $t1, 1($s1)
/* 09750C 7F0629DC 2508D0D8 */  addiu $t0, %lo(flareimage5) # addiu $t0, $t0, -0x2f28
/* 097510 7F0629E0 8FA500B0 */  lw    $a1, 0xb0($sp)
/* 097514 7F0629E4 15210029 */  bne   $t1, $at, .L7F062A8C
/* 097518 7F0629E8 27A40160 */   addiu $a0, $sp, 0x160
/* 09751C 7F0629EC 240A0002 */  li    $t2, 2
/* 097520 7F0629F0 AFAA0010 */  sw    $t2, 0x10($sp)
/* 097524 7F0629F4 27A40160 */  addiu $a0, $sp, 0x160
/* 097528 7F0629F8 8D050000 */  lw    $a1, ($t0)
/* 09752C 7F0629FC 24060004 */  li    $a2, 4
/* 097530 7F062A00 0FC1DB5A */  jal   texSelect
/* 097534 7F062A04 8FA70168 */   lw    $a3, 0x168($sp)
/* 097538 7F062A08 8FB10160 */  lw    $s1, 0x160($sp)
/* 09753C 7F062A0C 3C0D0470 */  lui   $t5, (0x04700080 >> 16) # lui $t5, 0x470
/* 097540 7F062A10 35AD0080 */  ori   $t5, (0x04700080 & 0xFFFF) # ori $t5, $t5, 0x80
/* 097544 7F062A14 262F0008 */  addiu $t7, $s1, 8
/* 097548 7F062A18 AFAF0160 */  sw    $t7, 0x160($sp)
/* 09754C 7F062A1C 02002025 */  move  $a0, $s0
/* 097550 7F062A20 0C003A2C */  jal   osVirtualToPhysical
/* 097554 7F062A24 AE2D0000 */   sw    $t5, ($s1)
/* 097558 7F062A28 AE220004 */  sw    $v0, 4($s1)
/* 09755C 7F062A2C 8FB90160 */  lw    $t9, 0x160($sp)
/* 097560 7F062A30 3C0EB100 */  lui   $t6, (0xB1000076 >> 16) # lui $t6, 0xb100          # gSP4Triangles(8,8,6,5,1,7,2,0,0,0,0,0
/* 097564 7F062A34 35CE0076 */  ori   $t6, (0xB1000076 & 0xFFFF) # ori $t6, $t6, 0x76
/* 097568 7F062A38 272B0008 */  addiu $t3, $t9, 8
/* 09756C 7F062A3C AFAB0160 */  sw    $t3, 0x160($sp)
/* 097570 7F062A40 24185454 */  li    $t8, 21588
/* 097574 7F062A44 AF380004 */  sw    $t8, 4($t9)                                       # ),
/* 097578 7F062A48 AF2E0000 */  sw    $t6, ($t9)
/* 09757C 7F062A4C 24090002 */  li    $t1, 2
/* 097580 7F062A50 AFA90010 */  sw    $t1, 0x10($sp)
/* 097584 7F062A54 8FA70168 */  lw    $a3, 0x168($sp)
/* 097588 7F062A58 8FA500B0 */  lw    $a1, 0xb0($sp)
/* 09758C 7F062A5C 27A40160 */  addiu $a0, $sp, 0x160
/* 097590 7F062A60 0FC1DB5A */  jal   texSelect
/* 097594 7F062A64 24060004 */   li    $a2, 4
/* 097598 7F062A68 8FAA0160 */  lw    $t2, 0x160($sp)
/* 09759C 7F062A6C 3C0FB100 */  lui   $t7, (0xB1000013 >> 16) # lui $t7, 0xb100          # gSP4Triangles(0,2,3,3,2,1,1,0,0,0,0
/* 0975A0 7F062A70 35EF0013 */  ori   $t7, (0xB1000013 & 0xFFFF) # ori $t7, $t7, 0x13
/* 0975A4 7F062A74 254C0008 */  addiu $t4, $t2, 8
/* 0975A8 7F062A78 AFAC0160 */  sw    $t4, 0x160($sp)
/* 0975AC 7F062A7C 240D3020 */  li    $t5, 12320
/* 0975B0 7F062A80 AD4D0004 */  sw    $t5, 4($t2)
/* 0975B4 7F062A84 10000017 */  b     .L7F062AE4
/* 0975B8 7F062A88 AD4F0000 */   sw    $t7, ($t2)                                        # ),
.L7F062A8C:
/* 0975BC 7F062A8C 24190002 */  li    $t9, 2
/* 0975C0 7F062A90 AFB90010 */  sw    $t9, 0x10($sp)
/* 0975C4 7F062A94 24060004 */  li    $a2, 4
/* 0975C8 7F062A98 0FC1DB5A */  jal   texSelect
/* 0975CC 7F062A9C 8FA70168 */   lw    $a3, 0x168($sp)
/* 0975D0 7F062AA0 8FB10160 */  lw    $s1, 0x160($sp)
/* 0975D4 7F062AA4 3C180430 */  lui   $t8, (0x04300040 >> 16) # lui $t8, 0x430
/* 0975D8 7F062AA8 37180040 */  ori   $t8, (0x04300040 & 0xFFFF) # ori $t8, $t8, 0x40
/* 0975DC 7F062AAC 262E0008 */  addiu $t6, $s1, 8
/* 0975E0 7F062AB0 AFAE0160 */  sw    $t6, 0x160($sp)
/* 0975E4 7F062AB4 02002025 */  move  $a0, $s0
/* 0975E8 7F062AB8 0C003A2C */  jal   osVirtualToPhysical
/* 0975EC 7F062ABC AE380000 */   sw    $t8, ($s1)
/* 0975F0 7F062AC0 AE220004 */  sw    $v0, 4($s1)
/* 0975F4 7F062AC4 8FA90160 */  lw    $t1, 0x160($sp)
/* 0975F8 7F062AC8 3C0CB100 */  lui   $t4, (0xB1000013 >> 16) # lui $t4, 0xb100
/* 0975FC 7F062ACC 358C0013 */  ori   $t4, (0xB1000013 & 0xFFFF) # ori $t4, $t4, 0x13
/* 097600 7F062AD0 252A0008 */  addiu $t2, $t1, 8
/* 097604 7F062AD4 AFAA0160 */  sw    $t2, 0x160($sp)
/* 097608 7F062AD8 240F3020 */  li    $t7, 12320
/* 09760C 7F062ADC AD2F0004 */  sw    $t7, 4($t1)
/* 097610 7F062AE0 AD2C0000 */  sw    $t4, ($t1)
.L7F062AE4:
/* 097614 7F062AE4 8FBF002C */  lw    $ra, 0x2c($sp)
/* 097618 7F062AE8 8FA20160 */  lw    $v0, 0x160($sp)
/* 09761C 7F062AEC D7B40018 */  ldc1  $f20, 0x18($sp)
/* 097620 7F062AF0 8FB00024 */  lw    $s0, 0x24($sp)
/* 097624 7F062AF4 8FB10028 */  lw    $s1, 0x28($sp)
/* 097628 7F062AF8 03E00008 */  jr    $ra
/* 09762C 7F062AFC 27BD0160 */   addiu $sp, $sp, 0x160
)
#endif

#if defined(VERSION_EU)
GLOBAL_ASM(
.late_rodata
glabel D_80053EAC
.word 0x3fb50481 /*1.4141999*/
glabel D_80053EB0
.word 0x3dcccccd /*0.1*/
glabel D_80053EB4
.word 0x3f666666 /*0.89999998*/
glabel D_80053EB8
.word 0x3f666666 /*0.89999998*/
glabel D_80053EBC
.word 0x3f666666 /*0.89999998*/
glabel D_80053EC0
.word 0x3f666666 /*0.89999998*/
glabel D_80053EC4
.word 0x3f666666 /*0.89999998*/
glabel D_80053EC8
.word 0x3f666666 /*0.89999998*/
glabel D_80053ECC
.word 0x3fb50481 /*1.4141999*/
glabel D_80053ED0
.word 0x3f666666 /*0.89999998*/
.text
glabel sub_GAME_7F061E18
/* 094CCC 7F0622DC 27BDFEA0 */  addiu $sp, $sp, -0x160
/* 094CD0 7F0622E0 AFBF002C */  sw    $ra, 0x2c($sp)
/* 094CD4 7F0622E4 AFB10028 */  sw    $s1, 0x28($sp)
/* 094CD8 7F0622E8 AFB00024 */  sw    $s0, 0x24($sp)
/* 094CDC 7F0622EC F7B40018 */  sdc1  $f20, 0x18($sp)
/* 094CE0 7F0622F0 AFA40160 */  sw    $a0, 0x160($sp)
/* 094CE4 7F0622F4 AFA60168 */  sw    $a2, 0x168($sp)
/* 094CE8 7F0622F8 80AB0000 */  lb    $t3, ($a1)
/* 094CEC 7F0622FC 3C0E8003 */  lui   $t6, %hi(D_80035C98) # $t6, 0x8003
/* 094CF0 7F062300 00A08825 */  move  $s1, $a1
/* 094CF4 7F062304 05600328 */  bltz  $t3, .L7F062FA8
/* 094CF8 7F062308 25CE11E8 */   addiu $t6, %lo(D_80035C98) # addiu $t6, $t6, 0x11e8
/* 094CFC 7F06230C 8DC10000 */  lw    $at, ($t6)
/* 094D00 7F062310 8DD90004 */  lw    $t9, 4($t6)
/* 094D04 7F062314 27A90108 */  addiu $t1, $sp, 0x108
/* 094D08 7F062318 AD210000 */  sw    $at, ($t1)
/* 094D0C 7F06231C AD390004 */  sw    $t9, 4($t1)
/* 094D10 7F062320 8DD9000C */  lw    $t9, 0xc($t6)
/* 094D14 7F062324 8DC10008 */  lw    $at, 8($t6)
/* 094D18 7F062328 AD39000C */  sw    $t9, 0xc($t1)
/* 094D1C 7F06232C 0FC22868 */  jal   bondviewGetCurrentPlayersPosition
/* 094D20 7F062330 AD210008 */   sw    $at, 8($t1)
/* 094D24 7F062334 AFA200F8 */  sw    $v0, 0xf8($sp)
/* 094D28 7F062338 3C0D8003 */  lui   $t5, %hi(D_80035CA8) # $t5, 0x8003
/* 094D2C 7F06233C 25AD11F8 */  addiu $t5, %lo(D_80035CA8) # addiu $t5, $t5, 0x11f8
/* 094D30 7F062340 8DA10000 */  lw    $at, ($t5)
/* 094D34 7F062344 C6200028 */  lwc1  $f0, 0x28($s1)
/* 094D38 7F062348 C6340024 */  lwc1  $f20, 0x24($s1)
/* 094D3C 7F06234C 27AF00C4 */  addiu $t7, $sp, 0xc4
/* 094D40 7F062350 ADE10000 */  sw    $at, ($t7)
/* 094D44 7F062354 8DA10008 */  lw    $at, 8($t5)
/* 094D48 7F062358 8DAB0004 */  lw    $t3, 4($t5)
/* 094D4C 7F06235C 3C098003 */  lui   $t1, %hi(D_80035CB4) # $t1, 0x8003
/* 094D50 7F062360 25291204 */  addiu $t1, %lo(D_80035CB4) # addiu $t1, $t1, 0x1204
/* 094D54 7F062364 ADE10008 */  sw    $at, 8($t7)
/* 094D58 7F062368 ADEB0004 */  sw    $t3, 4($t7)
/* 094D5C 7F06236C 8D210000 */  lw    $at, ($t1)
/* 094D60 7F062370 27B800B8 */  addiu $t8, $sp, 0xb8
/* 094D64 7F062374 8D2A0004 */  lw    $t2, 4($t1)
/* 094D68 7F062378 AF010000 */  sw    $at, ($t8)
/* 094D6C 7F06237C 8D210008 */  lw    $at, 8($t1)
/* 094D70 7F062380 AF0A0004 */  sw    $t2, 4($t8)
/* 094D74 7F062384 3C0C8007 */  lui   $t4, %hi(flareimage3) # $t4, 0x8007
/* 094D78 7F062388 AF010008 */  sw    $at, 8($t8)
/* 094D7C 7F06238C 3C018005 */  lui   $at, %hi(D_80053EAC) # $at, 0x8005
/* 094D80 7F062390 C4249FEC */  lwc1  $f4, %lo(D_80053EAC)($at)
/* 094D84 7F062394 8D8C44B0 */  lw    $t4, %lo(flareimage3)($t4)
/* 094D88 7F062398 E7A000E8 */  swc1  $f0, 0xe8($sp)
/* 094D8C 7F06239C E7A400B4 */  swc1  $f4, 0xb4($sp)
/* 094D90 7F0623A0 0FC1E111 */  jal   camGetWorldToScreenMtxf
/* 094D94 7F0623A4 AFAC00B0 */   sw    $t4, 0xb0($sp)
/* 094D98 7F0623A8 AFA200A8 */  sw    $v0, 0xa8($sp)
/* 094D9C 7F0623AC 82230001 */  lb    $v1, 1($s1)
/* 094DA0 7F0623B0 24010016 */  li    $at, 22
/* 094DA4 7F0623B4 C7A000E8 */  lwc1  $f0, 0xe8($sp)
/* 094DA8 7F0623B8 14610007 */  bne   $v1, $at, .L7F0623D8
/* 094DAC 7F0623BC 3C014248 */   li    $at, 0x42480000 # 50.000000
/* 094DB0 7F0623C0 44819000 */  mtc1  $at, $f18
/* 094DB4 7F0623C4 3C0F8007 */  lui   $t7, %hi(flareimage4) # $t7, 0x8007
/* 094DB8 7F0623C8 8DEF44B4 */  lw    $t7, %lo(flareimage4)($t7)
/* 094DBC 7F0623CC E7B200F4 */  swc1  $f18, 0xf4($sp)
/* 094DC0 7F0623D0 10000026 */  b     .L7F06246C
/* 094DC4 7F0623D4 AFAF00B0 */   sw    $t7, 0xb0($sp)
.L7F0623D8:
/* 094DC8 7F0623D8 24010017 */  li    $at, 23
/* 094DCC 7F0623DC 1461001F */  bne   $v1, $at, .L7F06245C
/* 094DD0 7F0623E0 3C0D8007 */   lui   $t5, %hi(flareimage4) # $t5, 0x8007
/* 094DD4 7F0623E4 3C014120 */  li    $at, 0x41200000 # 10.000000
/* 094DD8 7F0623E8 44813000 */  mtc1  $at, $f6
/* 094DDC 7F0623EC 8DAD44B4 */  lw    $t5, %lo(flareimage4)($t5)
/* 094DE0 7F0623F0 E7A000E8 */  swc1  $f0, 0xe8($sp)
/* 094DE4 7F0623F4 E7A600F4 */  swc1  $f6, 0xf4($sp)
/* 094DE8 7F0623F8 0C00262C */  jal   randomGetNext
/* 094DEC 7F0623FC AFAD00B0 */   sw    $t5, 0xb0($sp)
/* 094DF0 7F062400 24010032 */  li    $at, 50
/* 094DF4 7F062404 0041001B */  divu  $zero, $v0, $at
/* 094DF8 7F062408 00005810 */  mfhi  $t3
/* 094DFC 7F06240C 25790096 */  addiu $t9, $t3, 0x96
/* 094E00 7F062410 0C00262C */  jal   randomGetNext
/* 094E04 7F062414 A3B90117 */   sb    $t9, 0x117($sp)
/* 094E08 7F062418 24010005 */  li    $at, 5
/* 094E0C 7F06241C 0041001B */  divu  $zero, $v0, $at
/* 094E10 7F062420 00007010 */  mfhi  $t6
/* 094E14 7F062424 C7A000E8 */  lwc1  $f0, 0xe8($sp)
/* 094E18 7F062428 55C00011 */  bnezl $t6, .L7F062470
/* 094E1C 7F06242C C6240004 */   lwc1  $f4, 4($s1)
/* 094E20 7F062430 0C00262C */  jal   randomGetNext
/* 094E24 7F062434 E7A000E8 */   swc1  $f0, 0xe8($sp)
/* 094E28 7F062438 24010064 */  li    $at, 100
/* 094E2C 7F06243C 0041001B */  divu  $zero, $v0, $at
/* 094E30 7F062440 0000C010 */  mfhi  $t8
/* 094E34 7F062444 240900FF */  li    $t1, 255
/* 094E38 7F062448 01381823 */  subu  $v1, $t1, $t8
/* 094E3C 7F06244C A3A30115 */  sb    $v1, 0x115($sp)
/* 094E40 7F062450 A3A30114 */  sb    $v1, 0x114($sp)
/* 094E44 7F062454 10000005 */  b     .L7F06246C
/* 094E48 7F062458 C7A000E8 */   lwc1  $f0, 0xe8($sp)
.L7F06245C:
/* 094E4C 7F06245C 3C0141F0 */  li    $at, 0x41F00000 # 30.000000
/* 094E50 7F062460 44815000 */  mtc1  $at, $f10
/* 094E54 7F062464 00000000 */  nop
/* 094E58 7F062468 E7AA00F4 */  swc1  $f10, 0xf4($sp)
.L7F06246C:
/* 094E5C 7F06246C C6240004 */  lwc1  $f4, 4($s1)
.L7F062470:
/* 094E60 7F062470 44807000 */  mtc1  $zero, $f14
/* 094E64 7F062474 E7A400FC */  swc1  $f4, 0xfc($sp)
/* 094E68 7F062478 C6280008 */  lwc1  $f8, 8($s1)
/* 094E6C 7F06247C 4600703C */  c.lt.s $f14, $f0
/* 094E70 7F062480 E7A80100 */  swc1  $f8, 0x100($sp)
/* 094E74 7F062484 C626000C */  lwc1  $f6, 0xc($s1)
/* 094E78 7F062488 45000011 */  bc1f  .L7F0624D0
/* 094E7C 7F06248C E7A60104 */   swc1  $f6, 0x104($sp)
/* 094E80 7F062490 C6240010 */  lwc1  $f4, 0x10($s1)
/* 094E84 7F062494 C7AA00FC */  lwc1  $f10, 0xfc($sp)
/* 094E88 7F062498 46040202 */  mul.s $f8, $f0, $f4
/* 094E8C 7F06249C C7A40100 */  lwc1  $f4, 0x100($sp)
/* 094E90 7F0624A0 46085180 */  add.s $f6, $f10, $f8
/* 094E94 7F0624A4 E7A600FC */  swc1  $f6, 0xfc($sp)
/* 094E98 7F0624A8 C62A0014 */  lwc1  $f10, 0x14($s1)
/* 094E9C 7F0624AC 460A0202 */  mul.s $f8, $f0, $f10
/* 094EA0 7F0624B0 C7AA0104 */  lwc1  $f10, 0x104($sp)
/* 094EA4 7F0624B4 46082180 */  add.s $f6, $f4, $f8
/* 094EA8 7F0624B8 E7A60100 */  swc1  $f6, 0x100($sp)
/* 094EAC 7F0624BC C6240018 */  lwc1  $f4, 0x18($s1)
/* 094EB0 7F0624C0 46040202 */  mul.s $f8, $f0, $f4
/* 094EB4 7F0624C4 46085180 */  add.s $f6, $f10, $f8
/* 094EB8 7F0624C8 10000003 */  b     .L7F0624D8
/* 094EBC 7F0624CC E7A60104 */   swc1  $f6, 0x104($sp)
.L7F0624D0:
/* 094EC0 7F0624D0 4600A500 */  add.s $f20, $f20, $f0
/* 094EC4 7F0624D4 46007006 */  mov.s $f0, $f14
.L7F0624D8:
/* 094EC8 7F0624D8 46140100 */  add.s $f4, $f0, $f20
/* 094ECC 7F0624DC C622001C */  lwc1  $f2, 0x1c($s1)
/* 094ED0 7F0624E0 4604103C */  c.lt.s $f2, $f4
/* 094ED4 7F0624E4 00000000 */  nop
/* 094ED8 7F0624E8 45020003 */  bc1fl .L7F0624F8
/* 094EDC 7F0624EC C62C0018 */   lwc1  $f12, 0x18($s1)
/* 094EE0 7F0624F0 46001501 */  sub.s $f20, $f2, $f0
/* 094EE4 7F0624F4 C62C0018 */  lwc1  $f12, 0x18($s1)
.L7F0624F8:
/* 094EE8 7F0624F8 C7AA0104 */  lwc1  $f10, 0x104($sp)
/* 094EEC 7F0624FC 8FA200F8 */  lw    $v0, 0xf8($sp)
/* 094EF0 7F062500 460CA202 */  mul.s $f8, $f20, $f12
/* 094EF4 7F062504 C6220014 */  lwc1  $f2, 0x14($s1)
/* 094EF8 7F062508 C4440008 */  lwc1  $f4, 8($v0)
/* 094EFC 7F06250C E7AA0030 */  swc1  $f10, 0x30($sp)
/* 094F00 7F062510 46085180 */  add.s $f6, $f10, $f8
/* 094F04 7F062514 C44A0004 */  lwc1  $f10, 4($v0)
/* 094F08 7F062518 46062201 */  sub.s $f8, $f4, $f6
/* 094F0C 7F06251C 46081102 */  mul.s $f4, $f2, $f8
/* 094F10 7F062520 C7A80100 */  lwc1  $f8, 0x100($sp)
/* 094F14 7F062524 46141182 */  mul.s $f6, $f2, $f20
/* 094F18 7F062528 46083180 */  add.s $f6, $f6, $f8
/* 094F1C 7F06252C 46065281 */  sub.s $f10, $f10, $f6
/* 094F20 7F062530 460C5182 */  mul.s $f6, $f10, $f12
/* 094F24 7F062534 46062281 */  sub.s $f10, $f4, $f6
/* 094F28 7F062538 C7A400FC */  lwc1  $f4, 0xfc($sp)
/* 094F2C 7F06253C E7AA00D0 */  swc1  $f10, 0xd0($sp)
/* 094F30 7F062540 C6200010 */  lwc1  $f0, 0x10($s1)
/* 094F34 7F062544 C62C0018 */  lwc1  $f12, 0x18($s1)
/* 094F38 7F062548 E7A80034 */  swc1  $f8, 0x34($sp)
/* 094F3C 7F06254C 4600A182 */  mul.s $f6, $f20, $f0
/* 094F40 7F062550 C4480000 */  lwc1  $f8, ($v0)
/* 094F44 7F062554 E7AA0038 */  swc1  $f10, 0x38($sp)
/* 094F48 7F062558 C7AA0030 */  lwc1  $f10, 0x30($sp)
/* 094F4C 7F06255C 46062180 */  add.s $f6, $f4, $f6
/* 094F50 7F062560 46064201 */  sub.s $f8, $f8, $f6
/* 094F54 7F062564 46086182 */  mul.s $f6, $f12, $f8
/* 094F58 7F062568 00000000 */  nop
/* 094F5C 7F06256C 46146202 */  mul.s $f8, $f12, $f20
/* 094F60 7F062570 460A4200 */  add.s $f8, $f8, $f10
/* 094F64 7F062574 C44A0008 */  lwc1  $f10, 8($v0)
/* 094F68 7F062578 46085281 */  sub.s $f10, $f10, $f8
/* 094F6C 7F06257C 46005202 */  mul.s $f8, $f10, $f0
/* 094F70 7F062580 46083281 */  sub.s $f10, $f6, $f8
/* 094F74 7F062584 C7A60034 */  lwc1  $f6, 0x34($sp)
/* 094F78 7F062588 E7AA00D4 */  swc1  $f10, 0xd4($sp)
/* 094F7C 7F06258C C6220014 */  lwc1  $f2, 0x14($s1)
/* 094F80 7F062590 C6200010 */  lwc1  $f0, 0x10($s1)
/* 094F84 7F062594 4602A202 */  mul.s $f8, $f20, $f2
/* 094F88 7F062598 46083180 */  add.s $f6, $f6, $f8
/* 094F8C 7F06259C C4480004 */  lwc1  $f8, 4($v0)
/* 094F90 7F0625A0 46064201 */  sub.s $f8, $f8, $f6
/* 094F94 7F0625A4 46080182 */  mul.s $f6, $f0, $f8
/* 094F98 7F0625A8 00000000 */  nop
/* 094F9C 7F0625AC 46140202 */  mul.s $f8, $f0, $f20
/* 094FA0 7F0625B0 46044200 */  add.s $f8, $f8, $f4
/* 094FA4 7F0625B4 C4440000 */  lwc1  $f4, ($v0)
/* 094FA8 7F0625B8 46082101 */  sub.s $f4, $f4, $f8
/* 094FAC 7F0625BC 46022202 */  mul.s $f8, $f4, $f2
/* 094FB0 7F0625C0 46083101 */  sub.s $f4, $f6, $f8
/* 094FB4 7F0625C4 C7A60038 */  lwc1  $f6, 0x38($sp)
/* 094FB8 7F0625C8 46067032 */  c.eq.s $f14, $f6
/* 094FBC 7F0625CC E7A400D8 */  swc1  $f4, 0xd8($sp)
/* 094FC0 7F0625D0 45000008 */  bc1f  .L7F0625F4
/* 094FC4 7F0625D4 00000000 */   nop
/* 094FC8 7F0625D8 460A7032 */  c.eq.s $f14, $f10
/* 094FCC 7F0625DC 00000000 */  nop
/* 094FD0 7F0625E0 45020005 */  bc1fl .L7F0625F8
/* 094FD4 7F0625E4 27A400D0 */   addiu $a0, $sp, 0xd0
/* 094FD8 7F0625E8 46047032 */  c.eq.s $f14, $f4
/* 094FDC 7F0625EC 00000000 */  nop
/* 094FE0 7F0625F0 4501000F */  bc1t  .L7F062630
.L7F0625F4:
/* 094FE4 7F0625F4 27A400D0 */   addiu $a0, $sp, 0xd0
.L7F0625F8:
/* 094FE8 7F0625F8 27A500D4 */  addiu $a1, $sp, 0xd4
/* 094FEC 7F0625FC 0C0075F0 */  jal   guNormalize
/* 094FF0 7F062600 27A600D8 */   addiu $a2, $sp, 0xd8
/* 094FF4 7F062604 C7A000F4 */  lwc1  $f0, 0xf4($sp)
/* 094FF8 7F062608 C7A800D0 */  lwc1  $f8, 0xd0($sp)
/* 094FFC 7F06260C C7AA00D4 */  lwc1  $f10, 0xd4($sp)
/* 095000 7F062610 46004182 */  mul.s $f6, $f8, $f0
/* 095004 7F062614 C7A800D8 */  lwc1  $f8, 0xd8($sp)
/* 095008 7F062618 46005102 */  mul.s $f4, $f10, $f0
/* 09500C 7F06261C E7A600D0 */  swc1  $f6, 0xd0($sp)
/* 095010 7F062620 46004182 */  mul.s $f6, $f8, $f0
/* 095014 7F062624 E7A400D4 */  swc1  $f4, 0xd4($sp)
/* 095018 7F062628 10000005 */  b     .L7F062640
/* 09501C 7F06262C E7A600D8 */   swc1  $f6, 0xd8($sp)
.L7F062630:
/* 095020 7F062630 C7AA00F4 */  lwc1  $f10, 0xf4($sp)
/* 095024 7F062634 E7AE00D0 */  swc1  $f14, 0xd0($sp)
/* 095028 7F062638 E7AE00D8 */  swc1  $f14, 0xd8($sp)
/* 09502C 7F06263C E7AA00D4 */  swc1  $f10, 0xd4($sp)
.L7F062640:
/* 095030 7F062640 C6240014 */  lwc1  $f4, 0x14($s1)
/* 095034 7F062644 C7A800D8 */  lwc1  $f8, 0xd8($sp)
/* 095038 7F062648 C7AA00D4 */  lwc1  $f10, 0xd4($sp)
/* 09503C 7F06264C 27A400DC */  addiu $a0, $sp, 0xdc
/* 095040 7F062650 46082182 */  mul.s $f6, $f4, $f8
/* 095044 7F062654 C6240018 */  lwc1  $f4, 0x18($s1)
/* 095048 7F062658 27A500E0 */  addiu $a1, $sp, 0xe0
/* 09504C 7F06265C 27A600E4 */  addiu $a2, $sp, 0xe4
/* 095050 7F062660 46045102 */  mul.s $f4, $f10, $f4
/* 095054 7F062664 46043181 */  sub.s $f6, $f6, $f4
/* 095058 7F062668 E7A600DC */  swc1  $f6, 0xdc($sp)
/* 09505C 7F06266C C6240018 */  lwc1  $f4, 0x18($s1)
/* 095060 7F062670 C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 095064 7F062674 E7AA0038 */  swc1  $f10, 0x38($sp)
/* 095068 7F062678 C62A0010 */  lwc1  $f10, 0x10($s1)
/* 09506C 7F06267C 46062102 */  mul.s $f4, $f4, $f6
/* 095070 7F062680 00000000 */  nop
/* 095074 7F062684 460A4202 */  mul.s $f8, $f8, $f10
/* 095078 7F062688 46082281 */  sub.s $f10, $f4, $f8
/* 09507C 7F06268C C7A80038 */  lwc1  $f8, 0x38($sp)
/* 095080 7F062690 E7AA00E0 */  swc1  $f10, 0xe0($sp)
/* 095084 7F062694 C6240010 */  lwc1  $f4, 0x10($s1)
/* 095088 7F062698 46082282 */  mul.s $f10, $f4, $f8
/* 09508C 7F06269C C6240014 */  lwc1  $f4, 0x14($s1)
/* 095090 7F0626A0 46043202 */  mul.s $f8, $f6, $f4
/* 095094 7F0626A4 46085181 */  sub.s $f6, $f10, $f8
/* 095098 7F0626A8 0C0075F0 */  jal   guNormalize
/* 09509C 7F0626AC E7A600E4 */   swc1  $f6, 0xe4($sp)
/* 0950A0 7F0626B0 C7A000F4 */  lwc1  $f0, 0xf4($sp)
/* 0950A4 7F0626B4 C7A400DC */  lwc1  $f4, 0xdc($sp)
/* 0950A8 7F0626B8 C7A800E0 */  lwc1  $f8, 0xe0($sp)
/* 0950AC 7F0626BC 24010016 */  li    $at, 22
/* 0950B0 7F0626C0 46002282 */  mul.s $f10, $f4, $f0
/* 0950B4 7F0626C4 C7A400E4 */  lwc1  $f4, 0xe4($sp)
/* 0950B8 7F0626C8 46004182 */  mul.s $f6, $f8, $f0
/* 0950BC 7F0626CC E7AA00DC */  swc1  $f10, 0xdc($sp)
/* 0950C0 7F0626D0 46002282 */  mul.s $f10, $f4, $f0
/* 0950C4 7F0626D4 E7A600E0 */  swc1  $f6, 0xe0($sp)
/* 0950C8 7F0626D8 E7AA00E4 */  swc1  $f10, 0xe4($sp)
/* 0950CC 7F0626DC 822A0001 */  lb    $t2, 1($s1)
/* 0950D0 7F0626E0 15410005 */  bne   $t2, $at, .L7F0626F8
/* 0950D4 7F0626E4 00000000 */   nop
/* 0950D8 7F0626E8 0FC2F29D */  jal   dynAllocateVertices
/* 0950DC 7F0626EC 24040008 */   li    $a0, 8
/* 0950E0 7F0626F0 10000004 */  b     .L7F062704
/* 0950E4 7F0626F4 00408025 */   move  $s0, $v0
.L7F0626F8:
/* 0950E8 7F0626F8 0FC2F29D */  jal   dynAllocateVertices
/* 0950EC 7F0626FC 24040004 */   li    $a0, 4
/* 0950F0 7F062700 00408025 */  move  $s0, $v0
.L7F062704:
/* 0950F4 7F062704 0FC2F2A4 */  jal   dynAllocateMatrix
/* 0950F8 7F062708 00000000 */   nop
/* 0950FC 7F06270C AFA20158 */  sw    $v0, 0x158($sp)
/* 095100 7F062710 27A400FC */  addiu $a0, $sp, 0xfc
/* 095104 7F062714 0FC16383 */  jal   matrix_4x4_set_identity_and_position
/* 095108 7F062718 27A50118 */   addiu $a1, $sp, 0x118
/* 09510C 7F06271C 3C018005 */  lui   $at, %hi(D_80053EB0) # $at, 0x8005
/* 095110 7F062720 C42C9FF0 */  lwc1  $f12, %lo(D_80053EB0)($at)
/* 095114 7F062724 0FC163C9 */  jal   matrix_scalar_multiply
/* 095118 7F062728 27A50118 */   addiu $a1, $sp, 0x118
/* 09511C 7F06272C 8FA400A8 */  lw    $a0, 0xa8($sp)
/* 095120 7F062730 0FC16150 */  jal   matrix_4x4_multiply_homogeneous_in_place
/* 095124 7F062734 27A50118 */   addiu $a1, $sp, 0x118
/* 095128 7F062738 27A40118 */  addiu $a0, $sp, 0x118
/* 09512C 7F06273C 0FC16451 */  jal   matrix_4x4_f32_to_s32
/* 095130 7F062740 8FA50158 */   lw    $a1, 0x158($sp)
/* 095134 7F062744 27A20108 */  addiu $v0, $sp, 0x108
/* 095138 7F062748 8C410000 */  lw    $at, ($v0)
/* 09513C 7F06274C AE010000 */  sw    $at, ($s0)
/* 095140 7F062750 8C4D0004 */  lw    $t5, 4($v0)
/* 095144 7F062754 AE0D0004 */  sw    $t5, 4($s0)
/* 095148 7F062758 8C410008 */  lw    $at, 8($v0)
/* 09514C 7F06275C AE010008 */  sw    $at, 8($s0)
/* 095150 7F062760 8C4D000C */  lw    $t5, 0xc($v0)
/* 095154 7F062764 AE0D000C */  sw    $t5, 0xc($s0)
/* 095158 7F062768 8C410000 */  lw    $at, ($v0)
/* 09515C 7F06276C AE010010 */  sw    $at, 0x10($s0)
/* 095160 7F062770 8C4E0004 */  lw    $t6, 4($v0)
/* 095164 7F062774 AE0E0014 */  sw    $t6, 0x14($s0)
/* 095168 7F062778 8C410008 */  lw    $at, 8($v0)
/* 09516C 7F06277C AE010018 */  sw    $at, 0x18($s0)
/* 095170 7F062780 8C4E000C */  lw    $t6, 0xc($v0)
/* 095174 7F062784 AE0E001C */  sw    $t6, 0x1c($s0)
/* 095178 7F062788 8C410000 */  lw    $at, ($v0)
/* 09517C 7F06278C AE010020 */  sw    $at, 0x20($s0)
/* 095180 7F062790 8C4A0004 */  lw    $t2, 4($v0)
/* 095184 7F062794 AE0A0024 */  sw    $t2, 0x24($s0)
/* 095188 7F062798 8C410008 */  lw    $at, 8($v0)
/* 09518C 7F06279C AE010028 */  sw    $at, 0x28($s0)
/* 095190 7F0627A0 8C4A000C */  lw    $t2, 0xc($v0)
/* 095194 7F0627A4 AE0A002C */  sw    $t2, 0x2c($s0)
/* 095198 7F0627A8 8C410000 */  lw    $at, ($v0)
/* 09519C 7F0627AC AE010030 */  sw    $at, 0x30($s0)
/* 0951A0 7F0627B0 8C4D0004 */  lw    $t5, 4($v0)
/* 0951A4 7F0627B4 AE0D0034 */  sw    $t5, 0x34($s0)
/* 0951A8 7F0627B8 8C410008 */  lw    $at, 8($v0)
/* 0951AC 7F0627BC AE010038 */  sw    $at, 0x38($s0)
/* 0951B0 7F0627C0 8C4D000C */  lw    $t5, 0xc($v0)
/* 0951B4 7F0627C4 24010016 */  li    $at, 22
/* 0951B8 7F0627C8 AE0D003C */  sw    $t5, 0x3c($s0)
/* 0951BC 7F0627CC 82230001 */  lb    $v1, 1($s1)
/* 0951C0 7F0627D0 54610023 */  bnel  $v1, $at, .L7F062860
/* 0951C4 7F0627D4 24010017 */   li    $at, 23
/* 0951C8 7F0627D8 8C410000 */  lw    $at, ($v0)
/* 0951CC 7F0627DC AE010040 */  sw    $at, 0x40($s0)
/* 0951D0 7F0627E0 8C4B0004 */  lw    $t3, 4($v0)
/* 0951D4 7F0627E4 AE0B0044 */  sw    $t3, 0x44($s0)
/* 0951D8 7F0627E8 8C410008 */  lw    $at, 8($v0)
/* 0951DC 7F0627EC AE010048 */  sw    $at, 0x48($s0)
/* 0951E0 7F0627F0 8C4B000C */  lw    $t3, 0xc($v0)
/* 0951E4 7F0627F4 AE0B004C */  sw    $t3, 0x4c($s0)
/* 0951E8 7F0627F8 8C410000 */  lw    $at, ($v0)
/* 0951EC 7F0627FC AE010050 */  sw    $at, 0x50($s0)
/* 0951F0 7F062800 8C580004 */  lw    $t8, 4($v0)
/* 0951F4 7F062804 AE180054 */  sw    $t8, 0x54($s0)
/* 0951F8 7F062808 8C410008 */  lw    $at, 8($v0)
/* 0951FC 7F06280C AE010058 */  sw    $at, 0x58($s0)
/* 095200 7F062810 8C58000C */  lw    $t8, 0xc($v0)
/* 095204 7F062814 AE18005C */  sw    $t8, 0x5c($s0)
/* 095208 7F062818 8C410000 */  lw    $at, ($v0)
/* 09520C 7F06281C AE010060 */  sw    $at, 0x60($s0)
/* 095210 7F062820 8C4A0004 */  lw    $t2, 4($v0)
/* 095214 7F062824 AE0A0064 */  sw    $t2, 0x64($s0)
/* 095218 7F062828 8C410008 */  lw    $at, 8($v0)
/* 09521C 7F06282C AE010068 */  sw    $at, 0x68($s0)
/* 095220 7F062830 8C4A000C */  lw    $t2, 0xc($v0)
/* 095224 7F062834 AE0A006C */  sw    $t2, 0x6c($s0)
/* 095228 7F062838 8C410000 */  lw    $at, ($v0)
/* 09522C 7F06283C AE010070 */  sw    $at, 0x70($s0)
/* 095230 7F062840 8C4F0004 */  lw    $t7, 4($v0)
/* 095234 7F062844 AE0F0074 */  sw    $t7, 0x74($s0)
/* 095238 7F062848 8C410008 */  lw    $at, 8($v0)
/* 09523C 7F06284C AE010078 */  sw    $at, 0x78($s0)
/* 095240 7F062850 8C4F000C */  lw    $t7, 0xc($v0)
/* 095244 7F062854 AE0F007C */  sw    $t7, 0x7c($s0)
/* 095248 7F062858 82230001 */  lb    $v1, 1($s1)
/* 09524C 7F06285C 24010017 */  li    $at, 23
.L7F062860:
/* 095250 7F062860 5461004F */  bnel  $v1, $at, .L7F0629A0
/* 095254 7F062864 3C014120 */   lui   $at, 0x4120
/* 095258 7F062868 C6280010 */  lwc1  $f8, 0x10($s1)
/* 09525C 7F06286C C7A400FC */  lwc1  $f4, 0xfc($sp)
/* 095260 7F062870 8FA400A8 */  lw    $a0, 0xa8($sp)
/* 095264 7F062874 46144182 */  mul.s $f6, $f8, $f20
/* 095268 7F062878 27A5009C */  addiu $a1, $sp, 0x9c
/* 09526C 7F06287C 46043280 */  add.s $f10, $f6, $f4
/* 095270 7F062880 C7A40100 */  lwc1  $f4, 0x100($sp)
/* 095274 7F062884 E7AA009C */  swc1  $f10, 0x9c($sp)
/* 095278 7F062888 C6280014 */  lwc1  $f8, 0x14($s1)
/* 09527C 7F06288C 46144182 */  mul.s $f6, $f8, $f20
/* 095280 7F062890 46043280 */  add.s $f10, $f6, $f4
/* 095284 7F062894 C7A40104 */  lwc1  $f4, 0x104($sp)
/* 095288 7F062898 E7AA00A0 */  swc1  $f10, 0xa0($sp)
/* 09528C 7F06289C C6280018 */  lwc1  $f8, 0x18($s1)
/* 095290 7F0628A0 46144182 */  mul.s $f6, $f8, $f20
/* 095294 7F0628A4 46043280 */  add.s $f10, $f6, $f4
/* 095298 7F0628A8 0FC16247 */  jal   mtx4TransformVecInPlace
/* 09529C 7F0628AC E7AA00A4 */   swc1  $f10, 0xa4($sp)
/* 0952A0 7F0628B0 3C014120 */  li    $at, 0x41200000 # 10.000000
/* 0952A4 7F0628B4 44813000 */  mtc1  $at, $f6
/* 0952A8 7F0628B8 C7A800F4 */  lwc1  $f8, 0xf4($sp)
/* 0952AC 7F0628BC C7AE00A4 */  lwc1  $f14, 0xa4($sp)
/* 0952B0 7F0628C0 27A40088 */  addiu $a0, $sp, 0x88
/* 0952B4 7F0628C4 46064003 */  div.s $f0, $f8, $f6
/* 0952B8 7F0628C8 27A60090 */  addiu $a2, $sp, 0x90
/* 0952BC 7F0628CC 46007087 */  neg.s $f2, $f14
/* 0952C0 7F0628D0 44051000 */  mfc1  $a1, $f2
/* 0952C4 7F0628D4 E7A0008C */  swc1  $f0, 0x8c($sp)
/* 0952C8 7F0628D8 0FC1E05C */  jal   divide3DCoordinates
/* 0952CC 7F0628DC E7A00088 */   swc1  $f0, 0x88($sp)
/* 0952D0 7F0628E0 3C014000 */  li    $at, 0x40000000 # 2.000000
/* 0952D4 7F0628E4 C7B00090 */  lwc1  $f16, 0x90($sp)
/* 0952D8 7F0628E8 44812000 */  mtc1  $at, $f4
/* 0952DC 7F0628EC 3C013F00 */  li    $at, 0x3F000000 # 0.500000
/* 0952E0 7F0628F0 4604803C */  c.lt.s $f16, $f4
/* 0952E4 7F0628F4 00000000 */  nop
/* 0952E8 7F0628F8 4500000E */  bc1f  .L7F062934
/* 0952EC 7F0628FC 00000000 */   nop
/* 0952F0 7F062900 44815000 */  mtc1  $at, $f10
/* 0952F4 7F062904 C7A2009C */  lwc1  $f2, 0x9c($sp)
/* 0952F8 7F062908 C7AC00A0 */  lwc1  $f12, 0xa0($sp)
/* 0952FC 7F06290C 460A8002 */  mul.s $f0, $f16, $f10
/* 095300 7F062910 C7AE00A4 */  lwc1  $f14, 0xa4($sp)
/* 095304 7F062914 46001082 */  mul.s $f2, $f2, $f0
/* 095308 7F062918 00000000 */  nop
/* 09530C 7F06291C 46006302 */  mul.s $f12, $f12, $f0
/* 095310 7F062920 00000000 */  nop
/* 095314 7F062924 46007382 */  mul.s $f14, $f14, $f0
/* 095318 7F062928 E7A2009C */  swc1  $f2, 0x9c($sp)
/* 09531C 7F06292C E7AC00A0 */  swc1  $f12, 0xa0($sp)
/* 095320 7F062930 E7AE00A4 */  swc1  $f14, 0xa4($sp)
.L7F062934:
/* 095324 7F062934 0FC1E131 */  jal   currentPlayerGetViewToWorldMtxf
/* 095328 7F062938 00000000 */   nop
/* 09532C 7F06293C 00402025 */  move  $a0, $v0
/* 095330 7F062940 0FC16247 */  jal   mtx4TransformVecInPlace
/* 095334 7F062944 27A5009C */   addiu $a1, $sp, 0x9c
/* 095338 7F062948 C7A2009C */  lwc1  $f2, 0x9c($sp)
/* 09533C 7F06294C C7A800FC */  lwc1  $f8, 0xfc($sp)
/* 095340 7F062950 C7AC00A0 */  lwc1  $f12, 0xa0($sp)
/* 095344 7F062954 C7A60100 */  lwc1  $f6, 0x100($sp)
/* 095348 7F062958 46081081 */  sub.s $f2, $f2, $f8
/* 09534C 7F06295C 3C014120 */  li    $at, 0x41200000 # 10.000000
/* 095350 7F062960 44810000 */  mtc1  $at, $f0
/* 095354 7F062964 46066301 */  sub.s $f12, $f12, $f6
/* 095358 7F062968 C7AE00A4 */  lwc1  $f14, 0xa4($sp)
/* 09535C 7F06296C C7A40104 */  lwc1  $f4, 0x104($sp)
/* 095360 7F062970 46001282 */  mul.s $f10, $f2, $f0
/* 095364 7F062974 E7AC00A0 */  swc1  $f12, 0xa0($sp)
/* 095368 7F062978 46047381 */  sub.s $f14, $f14, $f4
/* 09536C 7F06297C 46006202 */  mul.s $f8, $f12, $f0
/* 095370 7F062980 E7A2009C */  swc1  $f2, 0x9c($sp)
/* 095374 7F062984 46007182 */  mul.s $f6, $f14, $f0
/* 095378 7F062988 E7AA00C4 */  swc1  $f10, 0xc4($sp)
/* 09537C 7F06298C E7AE00A4 */  swc1  $f14, 0xa4($sp)
/* 095380 7F062990 E7A800C8 */  swc1  $f8, 0xc8($sp)
/* 095384 7F062994 1000000E */  b     .L7F0629D0
/* 095388 7F062998 E7A600CC */   swc1  $f6, 0xcc($sp)
/* 09538C 7F06299C 3C014120 */  li    $at, 0x41200000 # 10.000000
.L7F0629A0:
/* 095390 7F0629A0 44812000 */  mtc1  $at, $f4
/* 095394 7F0629A4 C62A0010 */  lwc1  $f10, 0x10($s1)
/* 095398 7F0629A8 4604A002 */  mul.s $f0, $f20, $f4
/* 09539C 7F0629AC 00000000 */  nop
/* 0953A0 7F0629B0 46005202 */  mul.s $f8, $f10, $f0
/* 0953A4 7F0629B4 E7A800C4 */  swc1  $f8, 0xc4($sp)
/* 0953A8 7F0629B8 C6260014 */  lwc1  $f6, 0x14($s1)
/* 0953AC 7F0629BC 46003102 */  mul.s $f4, $f6, $f0
/* 0953B0 7F0629C0 E7A400C8 */  swc1  $f4, 0xc8($sp)
/* 0953B4 7F0629C4 C62A0018 */  lwc1  $f10, 0x18($s1)
/* 0953B8 7F0629C8 46005202 */  mul.s $f8, $f10, $f0
/* 0953BC 7F0629CC E7A800CC */  swc1  $f8, 0xcc($sp)
.L7F0629D0:
/* 0953C0 7F0629D0 C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 0953C4 7F0629D4 8FA500B0 */  lw    $a1, 0xb0($sp)
/* 0953C8 7F0629D8 3C018005 */  lui   $at, %hi(D_80053EB4) # $at, 0x8005
/* 0953CC 7F0629DC 4600310D */  trunc.w.s $f4, $f6
/* 0953D0 7F0629E0 44192000 */  mfc1  $t9, $f4
/* 0953D4 7F0629E4 00000000 */  nop
/* 0953D8 7F0629E8 A6190000 */  sh    $t9, ($s0)
/* 0953DC 7F0629EC C7AA00D4 */  lwc1  $f10, 0xd4($sp)
/* 0953E0 7F0629F0 4600520D */  trunc.w.s $f8, $f10
/* 0953E4 7F0629F4 440E4000 */  mfc1  $t6, $f8
/* 0953E8 7F0629F8 00000000 */  nop
/* 0953EC 7F0629FC A60E0002 */  sh    $t6, 2($s0)
/* 0953F0 7F062A00 C7A600D8 */  lwc1  $f6, 0xd8($sp)
/* 0953F4 7F062A04 4600310D */  trunc.w.s $f4, $f6
/* 0953F8 7F062A08 44092000 */  mfc1  $t1, $f4
/* 0953FC 7F062A0C 00000000 */  nop
/* 095400 7F062A10 A6090004 */  sh    $t1, 4($s0)
/* 095404 7F062A14 90AA0004 */  lbu   $t2, 4($a1)
/* 095408 7F062A18 A600000A */  sh    $zero, 0xa($s0)
/* 09540C 7F062A1C 000A6140 */  sll   $t4, $t2, 5
/* 095410 7F062A20 A60C0008 */  sh    $t4, 8($s0)
/* 095414 7F062A24 C7AA00D0 */  lwc1  $f10, 0xd0($sp)
/* 095418 7F062A28 46005207 */  neg.s $f8, $f10
/* 09541C 7F062A2C 4600418D */  trunc.w.s $f6, $f8
/* 095420 7F062A30 440D3000 */  mfc1  $t5, $f6
/* 095424 7F062A34 00000000 */  nop
/* 095428 7F062A38 A60D0010 */  sh    $t5, 0x10($s0)
/* 09542C 7F062A3C C7A400D4 */  lwc1  $f4, 0xd4($sp)
/* 095430 7F062A40 46002287 */  neg.s $f10, $f4
/* 095434 7F062A44 4600520D */  trunc.w.s $f8, $f10
/* 095438 7F062A48 440B4000 */  mfc1  $t3, $f8
/* 09543C 7F062A4C 00000000 */  nop
/* 095440 7F062A50 A60B0012 */  sh    $t3, 0x12($s0)
/* 095444 7F062A54 C7A600D8 */  lwc1  $f6, 0xd8($sp)
/* 095448 7F062A58 A6000018 */  sh    $zero, 0x18($s0)
/* 09544C 7F062A5C A600001A */  sh    $zero, 0x1a($s0)
/* 095450 7F062A60 46003107 */  neg.s $f4, $f6
/* 095454 7F062A64 4600228D */  trunc.w.s $f10, $f4
/* 095458 7F062A68 44185000 */  mfc1  $t8, $f10
/* 09545C 7F062A6C 00000000 */  nop
/* 095460 7F062A70 A6180014 */  sh    $t8, 0x14($s0)
/* 095464 7F062A74 C7A800D0 */  lwc1  $f8, 0xd0($sp)
/* 095468 7F062A78 C4269FF4 */  lwc1  $f6, %lo(D_80053EB4)($at)
/* 09546C 7F062A7C C7AA00C4 */  lwc1  $f10, 0xc4($sp)
/* 095470 7F062A80 3C018005 */  lui   $at, %hi(D_80053EB8) # $at, 0x8005
/* 095474 7F062A84 46064102 */  mul.s $f4, $f8, $f6
/* 095478 7F062A88 460A2200 */  add.s $f8, $f4, $f10
/* 09547C 7F062A8C 4600418D */  trunc.w.s $f6, $f8
/* 095480 7F062A90 440A3000 */  mfc1  $t2, $f6
/* 095484 7F062A94 00000000 */  nop
/* 095488 7F062A98 A60A0020 */  sh    $t2, 0x20($s0)
/* 09548C 7F062A9C C7A400D4 */  lwc1  $f4, 0xd4($sp)
/* 095490 7F062AA0 C42A9FF8 */  lwc1  $f10, %lo(D_80053EB8)($at)
/* 095494 7F062AA4 C7A600C8 */  lwc1  $f6, 0xc8($sp)
/* 095498 7F062AA8 3C018005 */  lui   $at, %hi(D_80053EBC) # $at, 0x8005
/* 09549C 7F062AAC 460A2202 */  mul.s $f8, $f4, $f10
/* 0954A0 7F062AB0 46064100 */  add.s $f4, $f8, $f6
/* 0954A4 7F062AB4 4600228D */  trunc.w.s $f10, $f4
/* 0954A8 7F062AB8 440F5000 */  mfc1  $t7, $f10
/* 0954AC 7F062ABC 00000000 */  nop
/* 0954B0 7F062AC0 A60F0022 */  sh    $t7, 0x22($s0)
/* 0954B4 7F062AC4 C7A800D8 */  lwc1  $f8, 0xd8($sp)
/* 0954B8 7F062AC8 C4269FFC */  lwc1  $f6, %lo(D_80053EBC)($at)
/* 0954BC 7F062ACC C7AA00CC */  lwc1  $f10, 0xcc($sp)
/* 0954C0 7F062AD0 3C018005 */  lui   $at, %hi(D_80053EC0) # $at, 0x8005
/* 0954C4 7F062AD4 46064102 */  mul.s $f4, $f8, $f6
/* 0954C8 7F062AD8 460A2200 */  add.s $f8, $f4, $f10
/* 0954CC 7F062ADC 4600418D */  trunc.w.s $f6, $f8
/* 0954D0 7F062AE0 44193000 */  mfc1  $t9, $f6
/* 0954D4 7F062AE4 00000000 */  nop
/* 0954D8 7F062AE8 A6190024 */  sh    $t9, 0x24($s0)
/* 0954DC 7F062AEC 90AB0004 */  lbu   $t3, 4($a1)
/* 0954E0 7F062AF0 000B7140 */  sll   $t6, $t3, 5
/* 0954E4 7F062AF4 A60E0028 */  sh    $t6, 0x28($s0)
/* 0954E8 7F062AF8 90B80005 */  lbu   $t8, 5($a1)
/* 0954EC 7F062AFC 00184940 */  sll   $t1, $t8, 5
/* 0954F0 7F062B00 A609002A */  sh    $t1, 0x2a($s0)
/* 0954F4 7F062B04 C42AA000 */  lwc1  $f10, %lo(D_80053EC0)($at)
/* 0954F8 7F062B08 C7A400D0 */  lwc1  $f4, 0xd0($sp)
/* 0954FC 7F062B0C C7A600C4 */  lwc1  $f6, 0xc4($sp)
/* 095500 7F062B10 3C018005 */  lui   $at, %hi(D_80053EC4) # $at, 0x8005
/* 095504 7F062B14 460A2202 */  mul.s $f8, $f4, $f10
/* 095508 7F062B18 46083101 */  sub.s $f4, $f6, $f8
/* 09550C 7F062B1C 4600228D */  trunc.w.s $f10, $f4
/* 095510 7F062B20 440C5000 */  mfc1  $t4, $f10
/* 095514 7F062B24 00000000 */  nop
/* 095518 7F062B28 A60C0030 */  sh    $t4, 0x30($s0)
/* 09551C 7F062B2C C428A004 */  lwc1  $f8, %lo(D_80053EC4)($at)
/* 095520 7F062B30 C7A600D4 */  lwc1  $f6, 0xd4($sp)
/* 095524 7F062B34 C7AA00C8 */  lwc1  $f10, 0xc8($sp)
/* 095528 7F062B38 3C018005 */  lui   $at, %hi(D_80053EC8) # $at, 0x8005
/* 09552C 7F062B3C 46083102 */  mul.s $f4, $f6, $f8
/* 095530 7F062B40 46045181 */  sub.s $f6, $f10, $f4
/* 095534 7F062B44 4600320D */  trunc.w.s $f8, $f6
/* 095538 7F062B48 440D4000 */  mfc1  $t5, $f8
/* 09553C 7F062B4C 00000000 */  nop
/* 095540 7F062B50 A60D0032 */  sh    $t5, 0x32($s0)
/* 095544 7F062B54 C424A008 */  lwc1  $f4, %lo(D_80053EC8)($at)
/* 095548 7F062B58 C7AA00D8 */  lwc1  $f10, 0xd8($sp)
/* 09554C 7F062B5C C7A800CC */  lwc1  $f8, 0xcc($sp)
/* 095550 7F062B60 A6000038 */  sh    $zero, 0x38($s0)
/* 095554 7F062B64 46045182 */  mul.s $f6, $f10, $f4
/* 095558 7F062B68 24010016 */  li    $at, 22
/* 09555C 7F062B6C 46064281 */  sub.s $f10, $f8, $f6
/* 095560 7F062B70 4600510D */  trunc.w.s $f4, $f10
/* 095564 7F062B74 440B2000 */  mfc1  $t3, $f4
/* 095568 7F062B78 00000000 */  nop
/* 09556C 7F062B7C A60B0034 */  sh    $t3, 0x34($s0)
/* 095570 7F062B80 90AE0005 */  lbu   $t6, 5($a1)
/* 095574 7F062B84 000EC140 */  sll   $t8, $t6, 5
/* 095578 7F062B88 A618003A */  sh    $t8, 0x3a($s0)
/* 09557C 7F062B8C 82290001 */  lb    $t1, 1($s1)
/* 095580 7F062B90 C7A800FC */  lwc1  $f8, 0xfc($sp)
/* 095584 7F062B94 8FAA00F8 */  lw    $t2, 0xf8($sp)
/* 095588 7F062B98 552100AD */  bnel  $t1, $at, .L7F062E50
/* 09558C 7F062B9C 8FAC0160 */   lw    $t4, 0x160($sp)
/* 095590 7F062BA0 C54C0000 */  lwc1  $f12, ($t2)
/* 095594 7F062BA4 C5420004 */  lwc1  $f2, 4($t2)
/* 095598 7F062BA8 C7A60100 */  lwc1  $f6, 0x100($sp)
/* 09559C 7F062BAC 46086381 */  sub.s $f14, $f12, $f8
/* 0955A0 7F062BB0 C5400008 */  lwc1  $f0, 8($t2)
/* 0955A4 7F062BB4 E7A80038 */  swc1  $f8, 0x38($sp)
/* 0955A8 7F062BB8 46061401 */  sub.s $f16, $f2, $f6
/* 0955AC 7F062BBC 460E7102 */  mul.s $f4, $f14, $f14
/* 0955B0 7F062BC0 C7AA0104 */  lwc1  $f10, 0x104($sp)
/* 0955B4 7F062BC4 3C018005 */  lui   $at, %hi(D_80053ECC) # $at, 0x8005
/* 0955B8 7F062BC8 46108202 */  mul.s $f8, $f16, $f16
/* 0955BC 7F062BCC 460A0481 */  sub.s $f18, $f0, $f10
/* 0955C0 7F062BD0 46082100 */  add.s $f4, $f4, $f8
/* 0955C4 7F062BD4 46129202 */  mul.s $f8, $f18, $f18
/* 0955C8 7F062BD8 46082100 */  add.s $f4, $f4, $f8
/* 0955CC 7F062BDC E7A40078 */  swc1  $f4, 0x78($sp)
/* 0955D0 7F062BE0 C6280010 */  lwc1  $f8, 0x10($s1)
/* 0955D4 7F062BE4 E7A60034 */  swc1  $f6, 0x34($sp)
/* 0955D8 7F062BE8 C7A60038 */  lwc1  $f6, 0x38($sp)
/* 0955DC 7F062BEC 46144202 */  mul.s $f8, $f8, $f20
/* 0955E0 7F062BF0 46064200 */  add.s $f8, $f8, $f6
/* 0955E4 7F062BF4 C6260014 */  lwc1  $f6, 0x14($s1)
/* 0955E8 7F062BF8 46086381 */  sub.s $f14, $f12, $f8
/* 0955EC 7F062BFC 46143202 */  mul.s $f8, $f6, $f20
/* 0955F0 7F062C00 C7A60034 */  lwc1  $f6, 0x34($sp)
/* 0955F4 7F062C04 46064200 */  add.s $f8, $f8, $f6
/* 0955F8 7F062C08 C6260018 */  lwc1  $f6, 0x18($s1)
/* 0955FC 7F062C0C 46081401 */  sub.s $f16, $f2, $f8
/* 095600 7F062C10 46143202 */  mul.s $f8, $f6, $f20
/* 095604 7F062C14 460A4180 */  add.s $f6, $f8, $f10
/* 095608 7F062C18 460E7202 */  mul.s $f8, $f14, $f14
/* 09560C 7F062C1C 00000000 */  nop
/* 095610 7F062C20 46108282 */  mul.s $f10, $f16, $f16
/* 095614 7F062C24 46060481 */  sub.s $f18, $f0, $f6
/* 095618 7F062C28 460A4180 */  add.s $f6, $f8, $f10
/* 09561C 7F062C2C 46129202 */  mul.s $f8, $f18, $f18
/* 095620 7F062C30 46083280 */  add.s $f10, $f6, $f8
/* 095624 7F062C34 C7A600C4 */  lwc1  $f6, 0xc4($sp)
/* 095628 7F062C38 4604503C */  c.lt.s $f10, $f4
/* 09562C 7F062C3C 00000000 */  nop
/* 095630 7F062C40 4500000B */  bc1f  .L7F062C70
/* 095634 7F062C44 00000000 */   nop
/* 095638 7F062C48 C424A00C */  lwc1  $f4, %lo(D_80053ECC)($at)
/* 09563C 7F062C4C E7A600B8 */  swc1  $f6, 0xb8($sp)
/* 095640 7F062C50 3C018005 */  lui   $at, %hi(D_80053ED0) # $at, 0x8005
/* 095644 7F062C54 C426A010 */  lwc1  $f6, %lo(D_80053ED0)($at)
/* 095648 7F062C58 C7A800C8 */  lwc1  $f8, 0xc8($sp)
/* 09564C 7F062C5C C7AA00CC */  lwc1  $f10, 0xcc($sp)
/* 095650 7F062C60 46062002 */  mul.s $f0, $f4, $f6
/* 095654 7F062C64 E7A800BC */  swc1  $f8, 0xbc($sp)
/* 095658 7F062C68 E7AA00C0 */  swc1  $f10, 0xc0($sp)
/* 09565C 7F062C6C E7A000B4 */  swc1  $f0, 0xb4($sp)
.L7F062C70:
/* 095660 7F062C70 C7A000B4 */  lwc1  $f0, 0xb4($sp)
/* 095664 7F062C74 C7A800DC */  lwc1  $f8, 0xdc($sp)
/* 095668 7F062C78 C7A400B8 */  lwc1  $f4, 0xb8($sp)
/* 09566C 7F062C7C 3C088007 */  lui   $t0, %hi(flareimage5) # $t0, 0x8007
/* 095670 7F062C80 46004282 */  mul.s $f10, $f8, $f0
/* 095674 7F062C84 250844B8 */  addiu $t0, %lo(flareimage5) # addiu $t0, $t0, 0x44b8
/* 095678 7F062C88 46045180 */  add.s $f6, $f10, $f4
/* 09567C 7F062C8C 4600320D */  trunc.w.s $f8, $f6
/* 095680 7F062C90 440F4000 */  mfc1  $t7, $f8
/* 095684 7F062C94 00000000 */  nop
/* 095688 7F062C98 A60F0040 */  sh    $t7, 0x40($s0)
/* 09568C 7F062C9C C7AA00E0 */  lwc1  $f10, 0xe0($sp)
/* 095690 7F062CA0 C7A600BC */  lwc1  $f6, 0xbc($sp)
/* 095694 7F062CA4 46005102 */  mul.s $f4, $f10, $f0
/* 095698 7F062CA8 46062200 */  add.s $f8, $f4, $f6
/* 09569C 7F062CAC 4600428D */  trunc.w.s $f10, $f8
/* 0956A0 7F062CB0 44195000 */  mfc1  $t9, $f10
/* 0956A4 7F062CB4 00000000 */  nop
/* 0956A8 7F062CB8 A6190042 */  sh    $t9, 0x42($s0)
/* 0956AC 7F062CBC C7A400E4 */  lwc1  $f4, 0xe4($sp)
/* 0956B0 7F062CC0 C7A800C0 */  lwc1  $f8, 0xc0($sp)
/* 0956B4 7F062CC4 46002182 */  mul.s $f6, $f4, $f0
/* 0956B8 7F062CC8 46083280 */  add.s $f10, $f6, $f8
/* 0956BC 7F062CCC 4600510D */  trunc.w.s $f4, $f10
/* 0956C0 7F062CD0 440E2000 */  mfc1  $t6, $f4
/* 0956C4 7F062CD4 00000000 */  nop
/* 0956C8 7F062CD8 A60E0044 */  sh    $t6, 0x44($s0)
/* 0956CC 7F062CDC 8D180000 */  lw    $t8, ($t0)
/* 0956D0 7F062CE0 93090004 */  lbu   $t1, 4($t8)
/* 0956D4 7F062CE4 00095140 */  sll   $t2, $t1, 5
/* 0956D8 7F062CE8 A60A0048 */  sh    $t2, 0x48($s0)
/* 0956DC 7F062CEC 8D0C0000 */  lw    $t4, ($t0)
/* 0956E0 7F062CF0 918F0005 */  lbu   $t7, 5($t4)
/* 0956E4 7F062CF4 000F6940 */  sll   $t5, $t7, 5
/* 0956E8 7F062CF8 A60D004A */  sh    $t5, 0x4a($s0)
/* 0956EC 7F062CFC C7A800DC */  lwc1  $f8, 0xdc($sp)
/* 0956F0 7F062D00 C7A600B8 */  lwc1  $f6, 0xb8($sp)
/* 0956F4 7F062D04 46004282 */  mul.s $f10, $f8, $f0
/* 0956F8 7F062D08 460A3101 */  sub.s $f4, $f6, $f10
/* 0956FC 7F062D0C 4600220D */  trunc.w.s $f8, $f4
/* 095700 7F062D10 440B4000 */  mfc1  $t3, $f8
/* 095704 7F062D14 00000000 */  nop
/* 095708 7F062D18 A60B0050 */  sh    $t3, 0x50($s0)
/* 09570C 7F062D1C C7AA00E0 */  lwc1  $f10, 0xe0($sp)
/* 095710 7F062D20 C7A600BC */  lwc1  $f6, 0xbc($sp)
/* 095714 7F062D24 46005102 */  mul.s $f4, $f10, $f0
/* 095718 7F062D28 46043201 */  sub.s $f8, $f6, $f4
/* 09571C 7F062D2C 4600428D */  trunc.w.s $f10, $f8
/* 095720 7F062D30 44185000 */  mfc1  $t8, $f10
/* 095724 7F062D34 00000000 */  nop
/* 095728 7F062D38 A6180052 */  sh    $t8, 0x52($s0)
/* 09572C 7F062D3C C7A400E4 */  lwc1  $f4, 0xe4($sp)
/* 095730 7F062D40 C7A600C0 */  lwc1  $f6, 0xc0($sp)
/* 095734 7F062D44 A6000058 */  sh    $zero, 0x58($s0)
/* 095738 7F062D48 46002202 */  mul.s $f8, $f4, $f0
/* 09573C 7F062D4C A600005A */  sh    $zero, 0x5a($s0)
/* 095740 7F062D50 46083281 */  sub.s $f10, $f6, $f8
/* 095744 7F062D54 4600510D */  trunc.w.s $f4, $f10
/* 095748 7F062D58 440A2000 */  mfc1  $t2, $f4
/* 09574C 7F062D5C 00000000 */  nop
/* 095750 7F062D60 A60A0054 */  sh    $t2, 0x54($s0)
/* 095754 7F062D64 C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 095758 7F062D68 C7AA00B8 */  lwc1  $f10, 0xb8($sp)
/* 09575C 7F062D6C 46003202 */  mul.s $f8, $f6, $f0
/* 095760 7F062D70 460A4100 */  add.s $f4, $f8, $f10
/* 095764 7F062D74 4600218D */  trunc.w.s $f6, $f4
/* 095768 7F062D78 440F3000 */  mfc1  $t7, $f6
/* 09576C 7F062D7C 00000000 */  nop
/* 095770 7F062D80 A60F0060 */  sh    $t7, 0x60($s0)
/* 095774 7F062D84 C7A800D4 */  lwc1  $f8, 0xd4($sp)
/* 095778 7F062D88 C7A400BC */  lwc1  $f4, 0xbc($sp)
/* 09577C 7F062D8C 46004282 */  mul.s $f10, $f8, $f0
/* 095780 7F062D90 46045180 */  add.s $f6, $f10, $f4
/* 095784 7F062D94 4600320D */  trunc.w.s $f8, $f6
/* 095788 7F062D98 44194000 */  mfc1  $t9, $f8
/* 09578C 7F062D9C 00000000 */  nop
/* 095790 7F062DA0 A6190062 */  sh    $t9, 0x62($s0)
/* 095794 7F062DA4 C7AA00D8 */  lwc1  $f10, 0xd8($sp)
/* 095798 7F062DA8 C7A600C0 */  lwc1  $f6, 0xc0($sp)
/* 09579C 7F062DAC A6000068 */  sh    $zero, 0x68($s0)
/* 0957A0 7F062DB0 46005102 */  mul.s $f4, $f10, $f0
/* 0957A4 7F062DB4 46062200 */  add.s $f8, $f4, $f6
/* 0957A8 7F062DB8 4600428D */  trunc.w.s $f10, $f8
/* 0957AC 7F062DBC 440E5000 */  mfc1  $t6, $f10
/* 0957B0 7F062DC0 00000000 */  nop
/* 0957B4 7F062DC4 A60E0064 */  sh    $t6, 0x64($s0)
/* 0957B8 7F062DC8 8D180000 */  lw    $t8, ($t0)
/* 0957BC 7F062DCC 93090005 */  lbu   $t1, 5($t8)
/* 0957C0 7F062DD0 00095140 */  sll   $t2, $t1, 5
/* 0957C4 7F062DD4 A60A006A */  sh    $t2, 0x6a($s0)
/* 0957C8 7F062DD8 C7A600D0 */  lwc1  $f6, 0xd0($sp)
/* 0957CC 7F062DDC C7A400B8 */  lwc1  $f4, 0xb8($sp)
/* 0957D0 7F062DE0 46003202 */  mul.s $f8, $f6, $f0
/* 0957D4 7F062DE4 46082281 */  sub.s $f10, $f4, $f8
/* 0957D8 7F062DE8 4600518D */  trunc.w.s $f6, $f10
/* 0957DC 7F062DEC 440F3000 */  mfc1  $t7, $f6
/* 0957E0 7F062DF0 00000000 */  nop
/* 0957E4 7F062DF4 A60F0070 */  sh    $t7, 0x70($s0)
/* 0957E8 7F062DF8 C7A800D4 */  lwc1  $f8, 0xd4($sp)
/* 0957EC 7F062DFC C7A400BC */  lwc1  $f4, 0xbc($sp)
/* 0957F0 7F062E00 46004282 */  mul.s $f10, $f8, $f0
/* 0957F4 7F062E04 460A2181 */  sub.s $f6, $f4, $f10
/* 0957F8 7F062E08 4600320D */  trunc.w.s $f8, $f6
/* 0957FC 7F062E0C 44194000 */  mfc1  $t9, $f8
/* 095800 7F062E10 00000000 */  nop
/* 095804 7F062E14 A6190072 */  sh    $t9, 0x72($s0)
/* 095808 7F062E18 C7AA00D8 */  lwc1  $f10, 0xd8($sp)
/* 09580C 7F062E1C C7A400C0 */  lwc1  $f4, 0xc0($sp)
/* 095810 7F062E20 46005182 */  mul.s $f6, $f10, $f0
/* 095814 7F062E24 46062201 */  sub.s $f8, $f4, $f6
/* 095818 7F062E28 4600428D */  trunc.w.s $f10, $f8
/* 09581C 7F062E2C 440E5000 */  mfc1  $t6, $f10
/* 095820 7F062E30 00000000 */  nop
/* 095824 7F062E34 A60E0074 */  sh    $t6, 0x74($s0)
/* 095828 7F062E38 8D180000 */  lw    $t8, ($t0)
/* 09582C 7F062E3C 93090004 */  lbu   $t1, 4($t8)
/* 095830 7F062E40 A600007A */  sh    $zero, 0x7a($s0)
/* 095834 7F062E44 00095140 */  sll   $t2, $t1, 5
/* 095838 7F062E48 A60A0078 */  sh    $t2, 0x78($s0)
/* 09583C 7F062E4C 8FAC0160 */  lw    $t4, 0x160($sp)
.L7F062E50:
/* 095840 7F062E50 3C0DB600 */  lui   $t5, 0xb600
/* 095844 7F062E54 24192000 */  li    $t9, 8192
/* 095848 7F062E58 258F0008 */  addiu $t7, $t4, 8
/* 09584C 7F062E5C AFAF0160 */  sw    $t7, 0x160($sp)
/* 095850 7F062E60 AD990004 */  sw    $t9, 4($t4)
/* 095854 7F062E64 AD8D0000 */  sw    $t5, ($t4)
/* 095858 7F062E68 8FAB0160 */  lw    $t3, 0x160($sp)
/* 09585C 7F062E6C 3C180102 */  lui   $t8, (0x01020040 >> 16) # lui $t8, 0x102
/* 095860 7F062E70 37180040 */  ori   $t8, (0x01020040 & 0xFFFF) # ori $t8, $t8, 0x40
/* 095864 7F062E74 256E0008 */  addiu $t6, $t3, 8
/* 095868 7F062E78 AFAE0160 */  sw    $t6, 0x160($sp)
/* 09586C 7F062E7C AD780000 */  sw    $t8, ($t3)
/* 095870 7F062E80 8FA40158 */  lw    $a0, 0x158($sp)
/* 095874 7F062E84 0C003838 */  jal   osVirtualToPhysical
/* 095878 7F062E88 AFAB006C */   sw    $t3, 0x6c($sp)
/* 09587C 7F062E8C 8FA3006C */  lw    $v1, 0x6c($sp)
/* 095880 7F062E90 3C088007 */  lui   $t0, %hi(flareimage5) # $t0, 0x8007
/* 095884 7F062E94 24010016 */  li    $at, 22
/* 095888 7F062E98 AC620004 */  sw    $v0, 4($v1)
/* 09588C 7F062E9C 82290001 */  lb    $t1, 1($s1)
/* 095890 7F062EA0 250844B8 */  addiu $t0, %lo(flareimage5) # addiu $t0, $t0, 0x44b8
/* 095894 7F062EA4 8FA500B0 */  lw    $a1, 0xb0($sp)
/* 095898 7F062EA8 15210029 */  bne   $t1, $at, .L7F062F50
/* 09589C 7F062EAC 27A40160 */   addiu $a0, $sp, 0x160
/* 0958A0 7F062EB0 240A0002 */  li    $t2, 2
/* 0958A4 7F062EB4 AFAA0010 */  sw    $t2, 0x10($sp)
/* 0958A8 7F062EB8 27A40160 */  addiu $a0, $sp, 0x160
/* 0958AC 7F062EBC 8D050000 */  lw    $a1, ($t0)
/* 0958B0 7F062EC0 24060004 */  li    $a2, 4
/* 0958B4 7F062EC4 0FC1DB7A */  jal   texSelect
/* 0958B8 7F062EC8 8FA70168 */   lw    $a3, 0x168($sp)
/* 0958BC 7F062ECC 8FB10160 */  lw    $s1, 0x160($sp)
/* 0958C0 7F062ED0 3C0D0470 */  lui   $t5, (0x04700080 >> 16) # lui $t5, 0x470
/* 0958C4 7F062ED4 35AD0080 */  ori   $t5, (0x04700080 & 0xFFFF) # ori $t5, $t5, 0x80
/* 0958C8 7F062ED8 262F0008 */  addiu $t7, $s1, 8
/* 0958CC 7F062EDC AFAF0160 */  sw    $t7, 0x160($sp)
/* 0958D0 7F062EE0 02002025 */  move  $a0, $s0
/* 0958D4 7F062EE4 0C003838 */  jal   osVirtualToPhysical
/* 0958D8 7F062EE8 AE2D0000 */   sw    $t5, ($s1)
/* 0958DC 7F062EEC AE220004 */  sw    $v0, 4($s1)
/* 0958E0 7F062EF0 8FB90160 */  lw    $t9, 0x160($sp)
/* 0958E4 7F062EF4 3C0EB100 */  lui   $t6, (0xB1000076 >> 16) # lui $t6, 0xb100
/* 0958E8 7F062EF8 35CE0076 */  ori   $t6, (0xB1000076 & 0xFFFF) # ori $t6, $t6, 0x76
/* 0958EC 7F062EFC 272B0008 */  addiu $t3, $t9, 8
/* 0958F0 7F062F00 AFAB0160 */  sw    $t3, 0x160($sp)
/* 0958F4 7F062F04 24185454 */  li    $t8, 21588
/* 0958F8 7F062F08 AF380004 */  sw    $t8, 4($t9)
/* 0958FC 7F062F0C AF2E0000 */  sw    $t6, ($t9)
/* 095900 7F062F10 24090002 */  li    $t1, 2
/* 095904 7F062F14 AFA90010 */  sw    $t1, 0x10($sp)
/* 095908 7F062F18 8FA70168 */  lw    $a3, 0x168($sp)
/* 09590C 7F062F1C 8FA500B0 */  lw    $a1, 0xb0($sp)
/* 095910 7F062F20 27A40160 */  addiu $a0, $sp, 0x160
/* 095914 7F062F24 0FC1DB7A */  jal   texSelect
/* 095918 7F062F28 24060004 */   li    $a2, 4
/* 09591C 7F062F2C 8FAA0160 */  lw    $t2, 0x160($sp)
/* 095920 7F062F30 3C0FB100 */  lui   $t7, (0xB1000013 >> 16) # lui $t7, 0xb100
/* 095924 7F062F34 35EF0013 */  ori   $t7, (0xB1000013 & 0xFFFF) # ori $t7, $t7, 0x13
/* 095928 7F062F38 254C0008 */  addiu $t4, $t2, 8
/* 09592C 7F062F3C AFAC0160 */  sw    $t4, 0x160($sp)
/* 095930 7F062F40 240D3020 */  li    $t5, 12320
/* 095934 7F062F44 AD4D0004 */  sw    $t5, 4($t2)
/* 095938 7F062F48 10000017 */  b     .L7F062FA8
/* 09593C 7F062F4C AD4F0000 */   sw    $t7, ($t2)
.L7F062F50:
/* 095940 7F062F50 24190002 */  li    $t9, 2
/* 095944 7F062F54 AFB90010 */  sw    $t9, 0x10($sp)
/* 095948 7F062F58 24060004 */  li    $a2, 4
/* 09594C 7F062F5C 0FC1DB7A */  jal   texSelect
/* 095950 7F062F60 8FA70168 */   lw    $a3, 0x168($sp)
/* 095954 7F062F64 8FB10160 */  lw    $s1, 0x160($sp)
/* 095958 7F062F68 3C180430 */  lui   $t8, (0x04300040 >> 16) # lui $t8, 0x430
/* 09595C 7F062F6C 37180040 */  ori   $t8, (0x04300040 & 0xFFFF) # ori $t8, $t8, 0x40
/* 095960 7F062F70 262E0008 */  addiu $t6, $s1, 8
/* 095964 7F062F74 AFAE0160 */  sw    $t6, 0x160($sp)
/* 095968 7F062F78 02002025 */  move  $a0, $s0
/* 09596C 7F062F7C 0C003838 */  jal   osVirtualToPhysical
/* 095970 7F062F80 AE380000 */   sw    $t8, ($s1)
/* 095974 7F062F84 AE220004 */  sw    $v0, 4($s1)
/* 095978 7F062F88 8FA90160 */  lw    $t1, 0x160($sp)
/* 09597C 7F062F8C 3C0CB100 */  lui   $t4, (0xB1000013 >> 16) # lui $t4, 0xb100
/* 095980 7F062F90 358C0013 */  ori   $t4, (0xB1000013 & 0xFFFF) # ori $t4, $t4, 0x13
/* 095984 7F062F94 252A0008 */  addiu $t2, $t1, 8
/* 095988 7F062F98 AFAA0160 */  sw    $t2, 0x160($sp)
/* 09598C 7F062F9C 240F3020 */  li    $t7, 12320
/* 095990 7F062FA0 AD2F0004 */  sw    $t7, 4($t1)
/* 095994 7F062FA4 AD2C0000 */  sw    $t4, ($t1)
.L7F062FA8:
/* 095998 7F062FA8 8FBF002C */  lw    $ra, 0x2c($sp)
/* 09599C 7F062FAC 8FA20160 */  lw    $v0, 0x160($sp)
/* 0959A0 7F062FB0 D7B40018 */  ldc1  $f20, 0x18($sp)
/* 0959A4 7F062FB4 8FB00024 */  lw    $s0, 0x24($sp)
/* 0959A8 7F062FB8 8FB10028 */  lw    $s1, 0x28($sp)
/* 0959AC 7F062FBC 03E00008 */  jr    $ra
/* 0959B0 7F062FC0 27BD0160 */   addiu $sp, $sp, 0x160
)
#endif
#endif

/*
* Address: 0x7F062B00
*/
void gunAdvanceBeamTimer(BeamRecord* beam)
{
    if (beam->unk00 >= 0)
    {
        if (g_ClockTimer < 3)
        {
#ifdef VERSION_US
            beam->unk28 += beam->unk20 * g_GlobalTimerDelta;
#else
            beam->unk28 += beam->unk20 * g_JP_GlobalTimerDelta;
#endif
        }
        else
        {
            beam->unk28 += beam->unk20 * (2.0f + ((f32) randomGetNext() * 2.3283064e-10f * 0.5f));
        }

        if (beam->unk1c <= beam->unk28)
        {
            beam->unk00 = -1;
            return;
        }

        beam->unk00++;
    }
}


void sub_GAME_7F062BE4(Gfx **gdlptr)
{
    Gfx *gdl = *gdlptr;
    ModelRenderData renderdata;
    s32 handnum;
    Model *model;
 
    renderdata = *(ModelRenderData *)&D_80035CC0;
 
    for (handnum = 0; handnum != 2; handnum++) 
    {
        struct hand *handptr = &g_CurrentPlayer->hands[handnum];
        s32 item = get_item_in_hand_or_watch_menu(handnum);
 
        if (handptr->field_87F == 0) 
        {
            continue;
        }
 
        if (item != ITEM_WATCHLASER) 
        {
            gdl = sub_GAME_7F061E18(gdl, &handptr->weapon_beam, 0);
        }
 
        if (item == ITEM_GOLDENGUN || item == ITEM_RUGER || item == ITEM_KNIFE || item == ITEM_THROWKNIFE || item == ITEM_SILVERWPPK || item == ITEM_GOLDWPPK) 
        {
            gSPSetLights1(gdl++, g_WeaponEnvmapLight);
            gSPLookAt(gdl++, sub_GAME_7F078474());
        }
 
        gSPPerspNormalize(gdl++, matrix_4x4_calc_depth_scale(0.0f, 300.0f));
 
        if ((*(Model *)&handptr->field_B68).obj->numSwitches >= 0x11 && (*(Model *)&handptr->field_B68).obj->Switches[16] != NULL)
        {
            union ModelRwData *rwdata;
            model = (Model *)&handptr->field_B68;
            rwdata = modelGetNodeRwData(model, (*(Model *)&handptr->field_B68).obj->Switches[17]);
 
            if (rwdata != NULL) 
            {
                rwdata->Raw.unk00 = 1;
            }
 
            if (item == ITEM_ROCKETLAUNCH) 
            {
                save_img_index_to_obj_ani_slot(&g_UnknownAnimController, crosshairimage);
                gdl = process_monitor_animation_microcode(model, (*(Model *)&handptr->field_B68).obj->Switches[16], &g_UnknownAnimController, gdl, 0, 4);
            } 
            else 
            {
                gdl = process_monitor_animation_microcode(model, (*(Model *)&handptr->field_B68).obj->Switches[16], &g_TaserAnimController, gdl, 0, 1);
            }
        }
 
        renderdata.gdl = gdl;
        renderdata.PropType = 4;
        renderdata.envcolour.word = g_CurrentPlayer->tileColor.a
                                  | ((u32)g_CurrentPlayer->tileColor.r << 24)
                                  | ((u32)g_CurrentPlayer->tileColor.g << 16)
                                  | ((u32)g_CurrentPlayer->tileColor.b << 8);
        renderdata.zbufferenabled = 0;
 
        matrix_4x4_7F058C64();
 
        if (item == ITEM_ROCKETLAUNCH && handptr->rocket != NULL) 
        {
            model = handptr->rocket->model;
 
            subdraw(&renderdata, model);
            bondviewTransformManyPosToViewMatrix(model->render_pos, model->obj->numMatrices);
 
            if (handptr->firedrocket != 0) 
            {
                handptr->rocket = NULL;
            }
        }
 
        if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_MIRROR_DUAL) != 0) 
        {
            gSPClearGeometryMode(renderdata.gdl++, G_CULL_BOTH);
 
            if (handnum == 0) 
            {
                renderdata.cullmode = 3;
            } 
            else 
            {
                renderdata.cullmode = 2;
            }
        }
 
        subdraw(&renderdata, (Model *)&handptr->field_B68);
        gdl = renderdata.gdl;
 
        if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_MIRROR_DUAL) != 0) 
        {
            gSPClearGeometryMode(gdl++, G_CULL_BOTH);
        }
 
        bondviewTransformManyPosToViewMatrix((*(Model *)&handptr->field_B68).render_pos, (*(Model *)&handptr->field_B68).obj->numMatrices);
        matrix_4x4_7F058C88();
 
        gSPPerspNormalize(gdl++, viGetPerspNorm());
 
        if (item == ITEM_WATCHLASER) 
        {
            gdl = sub_GAME_7F061E18(gdl, &handptr->weapon_beam, 0);
        }
    }
 
    *gdlptr = gdl;
}


Gfx *set_enviro_fog_for_items_in_solo_watch_menu(Gfx *gdl, ITEM_IDS itemid, Mtxf *mtx, s32 arg3, s32 arg4)
{
    ModelRenderData renderdata = *((ModelRenderData *) (&D_80035D00));
    ModelHeader model;
    u8 spb8[0x80];
    s32 padb4;
    Mtxf sp74;
    Mtxf *matrices;
    union ModelRwData *rwdata;
    s32 i;
    s32 j;
    ModelFileHeader *bodymodel;

    if ((itemid == ITEM_TRIGGER) || (itemid == ITEM_WATCHLASER))
    {
        itemid = ITEM_WATCHMAGNETATTRACT;
    }

    sub_GAME_7F05DA8C(GUNRIGHT, itemid);

    if ((!Gun_hand_without_item(GUNRIGHT)) || (!get_itemtype_in_hand(GUNRIGHT)))
    {
        return gdl;
    }

    bodymodel = &g_CurrentPlayer->copy_of_body_obj_header[GUNRIGHT];

    if (!get_ptr_weapon_model_header_line(itemid))
    {
        goto earlyreturn;
    }

    if (bondwalkItemCheckBitflags(itemid, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_MENU))
    {
        goto earlyreturn;
    }

    matrices = dynAllocate(bodymodel->numMatrices << 6);
    
    for (i = 0; i < bodymodel->numMatrices; i++)
    {
        matrix_4x4_set_identity(&matrices[i]);
    }

    i = 0;
    ((Model *) &model)->render_pos = matrices;
    modelCalculateRwDataLen(bodymodel);
    modelInit((Model *) &model, bodymodel, spb8);
    sub_GAME_7F05E978((Model *) &model, 0);
    sub_GAME_7F05EA94((Model *) &model, 1);

    if (bodymodel->Switches[1] != NULL)
    {
        rwdata = modelGetNodeRwData((Model *) &model, bodymodel->Switches[1]);

        if (rwdata != 0)
        {
            rwdata->Raw.unk00 = 0;
        }
    }

    matrix_4x4_copy(mtx, matrices);
  
    if (bodymodel->Skeleton == (&skeleton_gun_revolver))
    {
        if (bodymodel->Switches[4] != NULL)
        {
            matrix_4x4_set_identity_and_position((coord3d *) bodymodel->Switches[4]->Data, &sp74);
            matrix_4x4_multiply(mtx, &sp74, &matrices[3]);
        }
        if (bodymodel->Switches[5] != NULL)
        {
            matrix_4x4_set_identity_and_position((coord3d *) bodymodel->Switches[5]->Data, &sp74);
            matrix_4x4_multiply(mtx, &sp74, &matrices[4]);
        }
    }

    if (bodymodel->Switches[6] != NULL)
    {
        coord3d *pos;
        s32 index;

        pos = (coord3d *) bodymodel->Switches[6]->Data;
        index = modelFindNodeMtxIndex(bodymodel->Switches[6], 0);
        matrix_4x4_set_identity_and_position(pos, &sp74);
        matrix_4x4_multiply(mtx, &sp74, &matrices[index]);
    }

    if (bodymodel->Switches[7] != NULL)
    {
        coord3d *pos;
        s32 index;
        u8 pad[8];

        pos = (coord3d *) bodymodel->Switches[7]->Data;
        index = modelFindNodeMtxIndex(bodymodel->Switches[7], 0);
        matrix_4x4_set_identity_and_position(pos, &sp74);
        matrix_4x4_multiply(mtx, &sp74, &matrices[index]);
    }

    if (bodymodel->numSwitches >= 19)
    {
        for (j = 0; j != 20; j += 4)
        {
            if ((*((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x48))) != NULL)
            {
                rwdata = modelGetNodeRwData((Model *) &model, *((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x48)));

                if (rwdata != NULL)
                {
                    rwdata->Raw.unk00 = 1;
                }
            }

            if ((*((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x5c))) != NULL)
            {
                rwdata = modelGetNodeRwData((Model *) &model, *((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x5c)));

                if (rwdata != NULL)
                {
                    rwdata->Raw.unk00 = 1;
                }
            }
        }
    }

    modelUpdateNodeRelations((Model *) &model);

    if ((((((itemid == ITEM_GOLDENGUN) || (itemid == ITEM_RUGER)) || (itemid == ITEM_KNIFE)) || (itemid == ITEM_THROWKNIFE)) || (itemid == ITEM_SILVERWPPK)) || (itemid == ITEM_GOLDWPPK))
    {
        gSPSetLights1(gdl++, g_WeaponEnvmapLight);
        gSPLookAt(gdl++, sub_GAME_7F078474());
    }

    if (bodymodel->numSwitches >= 17)
    {
        if (bodymodel->Switches[16] != NULL)
        {
            rwdata = modelGetNodeRwData((Model *) &model, bodymodel->Switches[17]);

            if (rwdata != 0)
            {
                rwdata->Raw.unk00 = 0;
            }
        }
    }

    renderdata.gdl = gdl;

    if (arg3 >= 0xff)
    {
        renderdata.PropType = PROP_TYPE_WEAPON;
        renderdata.envcolour.word = arg4;
    }
    else
    {
        renderdata.PropType = PROP_TYPE_PLAYER;
        renderdata.envcolour.word = arg3;
        renderdata.fogcolour.word = arg4;
    }

    renderdata.zbufferenabled = FALSE;
    subdraw(&renderdata, (Model *) &model);
    gdl = renderdata.gdl;
    matrix_4x4_7F058C64();
    j = 0;

    if (bodymodel->numMatrices > 0)
    {
        do
        {
            matrix_4x4_copy((Mtxf *) (((u8 *) ((Model *) &model)->render_pos) + j), &sp74);
            matrix_4x4_f32_to_s32(&sp74, (Mtxf *) ((i << 6) + (u8 *) ((Model *) &model)->render_pos));
            i++;
            j += 0x40;
        }
        while (i < bodymodel->numMatrices);
    }

    matrix_4x4_7F058C88();

earlyreturn:
    return gdl;

}


void sub_GAME_7F0634D8(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    set_enviro_fog_for_items_in_solo_watch_menu(arg0, arg1, arg2, arg3, -256);
}


// Unreferenced
void sub_GAME_7F0634FC(s32 arg0, s32 arg1, s32 arg2)
{
    sub_GAME_7F0634D8(arg0, arg1, arg2, 0xFF);
}


void sub_GAME_7F06351C(struct coord3d* arg0, Mtxf* arg1, Mtxf* arg2, Mtxf* arg3, struct coord3d* arg4, Mtxf* arg5, Mtxf* arg6)
{
    Mtxf sp20;

    matrix_4x4_set_identity_and_position(arg0, arg6);
    matrix_4x4_multiply_in_place(arg1, arg6);
    matrix_4x4_multiply_in_place(arg2, arg6);
    matrix_4x4_multiply_in_place(arg3, arg6);
    matrix_4x4_set_identity_and_position(arg4, &sp20);
    matrix_4x4_multiply_in_place(&sp20, arg6);
    matrix_4x4_multiply_in_place(arg5, arg6);
}


/**
 * Address: 7F06359C
 */
Gfx* watchRenderController(Gfx* gdl, Mtxf* basemtx, s32 envcolour, bool animatebuttons, WatchContButtonPositions* buttonpositions, s8* contpadnum)
{
    ModelRenderData renderdata;
    struct ModelHeader modelstack;
    s32 i;
    s32 j;
    s32 offset;
    f32 angle;
    u32 rwdata[26];
    u32 pad2;
    Mtxf sp41c;
    Mtxf sp3dc;
    struct ModelFileHeader* objheader;
    struct coord3d* position;
    Mtxf* matrices;
    ModelNode* node;
    Mtxf sp38c;
    Mtxf sp34c;
    Mtxf sp30c;
    Mtxf sp2cc;
    Mtxf sp28c;
    Mtxf sp24c;
    Mtxf sp20c;
    Mtxf sp1cc;
    struct coord3d coord_node2;
    struct coord3d coord_node11_pos;
    struct coord3d coord_node11_base;
    struct coord3d coord_node4_pos;
    struct coord3d coord_node4_base;
    struct coord3d coord_node5_pos;
    struct coord3d coord_node5_base;
    struct coord3d coord_node6_pos;
    struct coord3d coord_node6_base;
    struct coord3d coord_node7_pos;
    struct coord3d coord_node7_base;
    struct coord3d coord_node9_pos;
    struct coord3d coord_node9_base;
    struct coord3d coord_node8_pos;
    struct coord3d coord_node8_base;
    struct coord3d coord_node10_pos;
    struct coord3d coord_node10_base;
    struct coord3d coord_node3_pos;
    Mtxf spb4;
    struct coord3d coord_node3_base;
    struct coord3d coord_node1_pos;
    struct coord3d coord_node1_base;
    struct coord3d coord_node12_pos;
    struct coord3d coord_node12_base;

    renderdata = *(ModelRenderData *)((u8 *)D_80035D04 + 0x3c);

    sub_GAME_7F05DA8C(GUNRIGHT, 0x55);

    if (!(Gun_hand_without_item(GUNRIGHT) && get_itemtype_in_hand(GUNRIGHT)))
    {
        return gdl;
    }

    objheader = g_CurrentPlayer->copy_of_body_obj_header;
    matrices = dynAllocate(objheader->numMatrices * (sizeof(Mtxf)));
    modelCalculateRwDataLen(objheader);

    if (objheader);

    modelInit(&modelstack, objheader, rwdata);
    modelstack.render_pos = (RenderPosView*) matrices;
    matrix_4x4_copy(basemtx, &matrices[0]);
    
    for (i = 1; i < 13; i++)
    {
        position = objheader->Switches[i]->Data;

        // Update joy stick position and rotation.
        if (i == 2)
        {
            angle = ((((-(f32) joyGetStickX(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
            matrix_4x4_set_rotation_around_z(angle, &sp41c);
            angle = ((((-(f32) joyGetStickY(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
            matrix_4x4_set_rotation_around_x(angle, &sp3dc);
            matrix_4x4_multiply_in_place(&sp3dc, &sp41c);
            matrix_4x4_set_position(position, &sp41c);
        }
        else
        {
            matrix_4x4_set_identity_and_position(position, &sp41c);
        }

        matrix_4x4_multiply(basemtx, &sp41c, &matrices[i]);
    }

    modelUpdateNodeRelations(&modelstack);
    renderdata.gdl = gdl;

    if (envcolour >= 0xff)
    {
        renderdata.PropType = PROP_TYPE_OBJ;
    }
    else
    {
        renderdata.PropType = PROP_TYPE_PLAYER;
        renderdata.envcolour.word = envcolour;
        renderdata.fogcolour.word = 0xFFFFFF00;
    }

    renderdata.zbufferenabled = TRUE;
    subdraw(&renderdata, &modelstack);
    gdl = renderdata.gdl;
    matrix_4x4_7F058C64();

    for (i = 0; i < objheader->numMatrices; i++)
    {
        matrix_4x4_copy((u32)modelstack.render_pos + i * sizeof(Mtxf), &sp41c);
        matrix_4x4_f32_to_s32(&sp41c, &modelstack.render_pos[i]);
    }

    matrix_4x4_7F058C88();

    if (animatebuttons)
    {
        node = objheader->Switches[13];

        if (node)
        {
            *((s32*) modelGetNodeRwData(&modelstack, node)) = 0;
        }

        matrices = dynAllocate(objheader->numMatrices * (sizeof(Mtxf)));
        modelstack.render_pos = (RenderPosView*) matrices;

        // Update the positions and/or rotations of the free floating buttons on the sides of the screen.
        for (i = 0; i < 13; i++)
        {
            matrix_4x4_set_lookat_target(&sp20c, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
            matrix_4x4_set_lookat_target(&sp38c, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
            matrix_4x4_set_identity(&sp24c);
            matrix_4x4_set_identity(&sp1cc);
            matrix_4x4_copy(&sp1cc, &sp2cc);
            
            // Joy stick
            if (i == 2)
            {
                coord_node2 = buttonpositions->joystick;
                matrix_4x4_set_identity_and_position(&coord_node2, &sp34c);
                angle = ((((-(f32) joyGetStickX(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
                matrix_4x4_set_rotation_around_z(angle, &sp41c);
                angle = ((((-(f32) joyGetStickY(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
                matrix_4x4_set_rotation_around_x(angle, &sp3dc);
                matrix_4x4_multiply_in_place(&sp3dc, &sp41c);
                matrix_4x4_multiply_in_place(&sp34c, &sp41c);
                matrix_4x4_multiply(&sp20c, &sp41c, &sp30c);
                matrix_4x4_copy(&sp30c, &matrices[i]);
            }
            // R
            else if (i == 11)
            {
                coord_node11_base = ((struct coord3d *)watchControllerButtonBases)[5];
                coord_node11_pos = buttonpositions->r;

                if (joyGetButtons(*contpadnum, R_TRIG))
                {
                    matrix_4x4_set_rotation_around_y(-0.17453294f, &sp24c);
                }

                matrix_4x4_set_rotation_around_x(1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node11_base, &sp24c, &sp28c, &sp1cc, &coord_node11_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // C-Up
            else if (i == 4)
            {
                coord_node4_base = ((struct coord3d *)watchControllerButtonBases)[6];
                coord_node4_pos = buttonpositions->cUp;

                if (joyGetButtons(*contpadnum, U_CBUTTONS))
                {
                    coord_node4_base.y += -10.0f;
                }

                matrix_4x4_set_rotation_around_x(-1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node4_base, &sp24c, &sp28c, &sp1cc, &coord_node4_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // C-Down
            else if (i == 5)
            {
                coord_node5_base = ((struct coord3d *)watchControllerButtonBases)[7];
                coord_node5_pos = buttonpositions->cDown;

                if (joyGetButtons(*contpadnum, D_CBUTTONS))
                {
                    coord_node5_base.y += -10.0f;
                }

                matrix_4x4_set_rotation_around_x(-1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node5_base, &sp24c, &sp28c, &sp1cc, &coord_node5_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // C-Left
            else if (i == 6)
            {
                coord_node6_base = ((struct coord3d *)watchControllerButtonBases)[8];
                coord_node6_pos = buttonpositions->cLeft;

                if (joyGetButtons(*contpadnum, L_CBUTTONS))
                {
                    coord_node6_base.y += -10.0f;
                }

                matrix_4x4_set_rotation_around_x(-1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node6_base, &sp24c, &sp28c, &sp1cc, &coord_node6_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // C-Right
            else if (i == 7)
            {
                coord_node7_base = ((struct coord3d *)watchControllerButtonBases)[9];
                coord_node7_pos = buttonpositions->cRight;

                if (joyGetButtons(*contpadnum, R_CBUTTONS))
                {
                    coord_node7_base.y += -10.0f;
                }

                matrix_4x4_set_rotation_around_x(-1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node7_base, &sp24c, &sp28c, &sp1cc, &coord_node7_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // B
            else if (i == 9)
            {
                coord_node9_base = ((struct coord3d *)watchControllerButtonBases)[10];
                coord_node9_pos = buttonpositions->b;

                if (joyGetButtons(*contpadnum, B_BUTTON))
                {
                    coord_node9_base.y += -10.0f;
                }

                matrix_4x4_set_rotation_around_x(-1.0471976, &sp28c);
                sub_GAME_7F06351C(&coord_node9_base, &sp24c, &sp28c, &sp1cc, &coord_node9_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // A
            else if (i == 8)
            {
                coord_node8_base = ((struct coord3d *)watchControllerButtonBases)[11];
                coord_node8_pos = buttonpositions->a;

                if (joyGetButtons(*contpadnum, A_BUTTON))
                {
                    coord_node8_base.y += -10.0f;
                }

                matrix_4x4_set_rotation_around_x(-1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node8_base, &sp24c, &sp28c, &sp1cc, &coord_node8_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // L
            else if (i == 10)
            {
                coord_node10_base = ((struct coord3d *)watchControllerButtonBases)[12];
                coord_node10_pos = buttonpositions->l;

                if (joyGetButtons(*contpadnum, L_TRIG))
                {
                    matrix_4x4_set_rotation_around_y(0.17453294f, &sp24c);
                }

                matrix_4x4_set_rotation_around_x(1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node10_base, &sp24c, &sp28c, &sp1cc, &coord_node10_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // D-pad
            else if (i == 3)
            {
                coord_node3_base = ((struct coord3d *)watchControllerButtonBases)[13];
                coord_node3_pos = buttonpositions->dPad;
                matrix_4x4_set_identity(&spb4);

                if (joyGetButtons(*contpadnum, U_JPAD))
                {
                    matrix_4x4_set_rotation_around_x(-0.17453294f, &sp24c);
                }
                else if (joyGetButtons(*contpadnum, D_JPAD))
                {
                    matrix_4x4_set_rotation_around_x(0.17453294f, &sp24c);
                }

                if (joyGetButtons(*contpadnum, L_JPAD))
                {
                    matrix_4x4_set_rotation_around_z(0.17453294f, &spb4);
                }
                else if (joyGetButtons(*contpadnum, R_JPAD))
                {
                    matrix_4x4_set_rotation_around_z(-0.17453294f, &spb4);
                }

                matrix_4x4_multiply_in_place(&spb4, &sp24c);
                matrix_4x4_set_rotation_around_x(-0.89759791f, &sp28c);
                sub_GAME_7F06351C(&coord_node3_base, &sp24c, &sp28c, &sp1cc, &coord_node3_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // Start
            else if (i == 1)
            {
                coord_node1_base = ((struct coord3d *)watchControllerButtonBases)[14];
                coord_node1_pos = buttonpositions->start;

                if (joyGetButtons(*contpadnum, START_BUTTON))
                {
                    coord_node1_base.y += -10.0f;
                }

                matrix_4x4_set_rotation_around_x(-1.0471976f, &sp28c);
                sub_GAME_7F06351C(&coord_node1_base, &sp24c, &sp28c, &sp1cc, &coord_node1_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            // Z
            else if (i == 12)
            {
                coord_node12_base = ((struct coord3d *)watchControllerButtonBases)[15];
                coord_node12_pos = buttonpositions->z;

                if (joyGetButtons(*contpadnum, Z_TRIG))
                {
                    matrix_4x4_set_rotation_around_x(-0.17453294f, &sp24c);
                }

                matrix_4x4_set_rotation_around_z(M_PI_F, &sp28c);
                sub_GAME_7F06351C(&coord_node12_base, &sp24c, &sp28c, &sp1cc, &coord_node12_pos, &sp20c, &sp38c);
                matrix_4x4_copy(&sp38c, &matrices[i]);
            }
            else
            {
                matrix_4x4_copy(basemtx, &matrices[i]);
            }
        }

        modelUpdateNodeRelations(&modelstack);
        renderdata.gdl = gdl;
        subdraw(&renderdata, &modelstack);
        gdl = renderdata.gdl;
        matrix_4x4_7F058C64();

        for (i = 0; i < objheader->numMatrices; i++)
        {
            matrix_4x4_copy((u32)modelstack.render_pos + i * sizeof(Mtxf), &sp41c);
            matrix_4x4_f32_to_s32(&sp41c, &modelstack.render_pos[i]);
        }

        matrix_4x4_7F058C88();
    }

    return gdl;
}


/**
 * Address: 7F064364
 */
Gfx *watchRenderControllerOpaque(Gfx *gdl, Mtxf *basemtx, bool animatebuttons, WatchContButtonPositions *buttonpositions, s8 *contpadnum)
{
    return watchRenderController(gdl, basemtx, 0xff, animatebuttons, buttonpositions, contpadnum);
}


/**
 * Address: 7F0643A0
 */
ALSoundState* gunGetFreeSfxState(void)
{
    s32 i;

    for (i = 0; i < NUM_IMPACT_SFX_STATES; i++) 
    {
        if (g_ImpactSfxStates[i] == NULL) 
        {
            return &g_ImpactSfxStates[i];
        }
    }
    
    return NULL;
}


void recall_joy2_hits_edit_detail_edit_flag(enum ITEM_IDS item, PropRecord* prop, s32 texture_index)
{
    s32 sp6C;
    u32 rnd1;
    u32 rnd2;
    ALSoundState* sound_state;
    struct RicochetSoundsSmall ricochet_sounds_small_copy;
    struct PunchSounds punch_sounds_copy;
    struct BulletFleshSounds bullet_flesh_sounds_copy;
    u32 sfx_index;

    sp6C = sub_GAME_7F0539E4(&prop->pos);

    rnd1 = randomGetNext();
    rnd2 = randomGetNext();

    D_800483C4 = texture_index;

    if (get_debug_joy2hitsedit_flag() == 0)
    {
        get_debug_joy2detailedit_flag();
    }

    if ((item == ITEM_REMOTEMINE)
        || (item == ITEM_PROXIMITYMINE)
        || (item == ITEM_TIMEDMINE)
        || (item == ITEM_BOMBCASE)
        || (item == ITEM_BUG)
        || (item == ITEM_MICROCAMERA)
        || (item == ITEM_PLASTIQUE)
        || (item == ITEM_WATCHLASER)
        || (item == ITEM_WATCHMAGNETATTRACT))
    {
        return;
    }

#ifdef BUGFIX_R1
    if (g_ClockTimer <= 0) { return; }
#endif

    sound_state = gunGetFreeSfxState();
    if (sound_state != NULL)
    {
        if ((prop->type != PROP_TYPE_CHR) && (prop->type != PROP_TYPE_VIEWER))
        {
            if (item == ITEM_LASER)
            {
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, RICO_LASER1_SFX, sound_state);
            }
            else
            {
                ricochet_sounds_small_copy = ricochet_sounds_small;
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, ricochet_sounds_small_copy.arr[rnd1 % 20], sound_state);
            }

            if (sound_state->link.next != NULL)
            {
                sndCreatePostEvent((ALSoundState* ) sound_state->link.next, 8, sp6C);
            }
        }
        else
        {
            if (item == ITEM_KNIFE)
            {
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, HIT_BULLET_SNOW_SFX, sound_state);
            }
            else if (item == ITEM_FIST)
            {
                punch_sounds_copy = punch_sounds;
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, punch_sounds_copy.arr[rnd1 % 3], sound_state);
            }
            else
            {
                bullet_flesh_sounds_copy = bullet_flesh_sounds;
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, bullet_flesh_sounds_copy.arr[rnd1 % 2], sound_state);
            }

            if (sound_state->link.next != NULL) {
                sndCreatePostEvent((ALSoundState* ) sound_state->link.next, 8, sp6C);
            }
        }
    }

    sound_state = gunGetFreeSfxState();
    if ((sound_state != NULL) && (texture_index >= 0))
    {
        if (g_HitTypeSounds[g_Textures[texture_index].hitSound] != NULL)
        {
            if (g_HitTypeSounds[g_Textures[texture_index].hitSound]->sfx_len > 0)
            {
                sfx_index = rnd2 % g_HitTypeSounds[g_Textures[texture_index].hitSound]->sfx_len;
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, g_HitTypeSounds[g_Textures[texture_index].hitSound]->sfx[sfx_index], sound_state);
            }

            if (sound_state->link.next != NULL)
            {
                chrobjSndCreatePostEventDefault((ALSoundState* ) sound_state->link.next, &prop->pos);
            }
        }
    }
#ifdef DEBUG
    osSyncPrintf("Shot prop: hittype %d\n", g_Textures[texture_index].hitSound);
#endif
#ifdef ENABLE_LOG
    osSyncPrintf("Shot prop:  %S\n", HIT_TYPE_ToString[g_Textures[texture_index].hitSound]);
#endif
}


void sub_GAME_7F064720(coord3d* pos)
{
    ALSoundState* sound;
    ALLink* link;

#ifdef BUGFIX_R1
    if (g_ClockTimer <= 0) { return; }
#endif

    sound = gunGetFreeSfxState();

    if (sound != NULL)
    {
        sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, HIT_BULLET_GLASS_SFX, sound);

        link = sound->link.next;
        if (link != NULL)
        {
            chrobjSndCreatePostEventDefault((ALSoundState* ) link, pos);
        }
    }
}


void recall_joy2_hits_edit_flag(enum ITEM_IDS item, coord3d* arg1, s32 texture_index)
{
    ALSoundState *sound_state;
    u32 rnd1;
    u32 rnd2;
    struct LaserRichochetSounds laser_copied;
    struct RicochetSoundsLarge rico_copied;
    u32 sfx_index;
    struct image_sound *img_sound;

    rnd1 = randomGetNext();
    rnd2 = randomGetNext();

    D_800483C4 = texture_index;
    get_debug_joy2hitsedit_flag();

#ifdef BUGFIX_R1
    if (g_ClockTimer <= 0) { return; }
#endif

    sound_state = gunGetFreeSfxState();
    if (sound_state != NULL)
    {
        if (item != ITEM_WATCHLASER)
        {
            if (item == ITEM_LASER)
            {
                laser_copied = laser_ricochet_sounds;
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, laser_copied.arr[rnd1 % 2], sound_state);
            }
            else
            {
                rico_copied = ricochet_sounds_large;
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, rico_copied.arr[rnd1 % 36], sound_state);
            }
        }

        if (sound_state->link.next != NULL)
        {
            chrobjSndCreatePostEventDefault((ALSoundState* ) sound_state->link.next, arg1);
        }
    }

    sound_state = gunGetFreeSfxState();
    if ((sound_state != NULL) && (texture_index >= 0))
    {
        img_sound = g_HitTypeSounds[g_Textures[texture_index].hitSound];
        if (img_sound->sfx_len > 0)
        {
            if (img_sound != NULL)
            {
                sfx_index = rnd2 % img_sound->sfx_len;
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, img_sound->sfx[sfx_index], sound_state);
            }

            if (sound_state->link.next != NULL)
            {
                chrobjSndCreatePostEventDefault((ALSoundState* ) sound_state->link.next, arg1);
            }
        }
    }
}


void sub_GAME_7F064934(ITEM_IDS item)
{
    struct EarWhistleSounds copied;

#ifdef BUGFIX_R1
    if (g_ClockTimer <= 0) { return; }
#endif
    if ((item != ITEM_LASER) && (item != ITEM_WATCHLASER))
    {
        copied = ear_whistle_sounds;
        sndPlaySfx((struct ALBankAlt_s*) g_musicSfxBufferPtr, copied.arr[randomGetNext() % 5], 0);
    }
}


f32 sub_GAME_7F0649AC(s32 param_1)
{
  f32 fVar1;

  fVar1 = -60.0f;
  if (param_1 == 0x19) {
    fVar1 -= 20.0f;
  }
  return fVar1;
}



void sub_GAME_7F0649D8(enum GUNHAND hand)
{
    struct hand* hand_ptr;
    enum ITEM_IDS item_id;
    s32 ammo_in_magazine;
    s32 ammo_in_hands;
    WeaponStats* item_stats;
    s32 magsize;
    s32 ammo_total;

    hand_ptr = &g_CurrentPlayer->hands[hand];
    item_id = getCurrentPlayerWeaponId(hand);
    ammo_in_magazine = hand_ptr->weapon_ammo_in_magazine;
    ammo_in_hands = get_ammo_in_hands_weapon(hand);
    item_stats = get_ptr_item_statistics(item_id);
    ammo_total = ammo_in_hands + ammo_in_magazine;

    hand_ptr->weapon_ammo_in_magazine = (ammo_total >= item_stats->MagSize)
        ? item_stats->MagSize
        : ammo_total;

    g_CurrentPlayer->ammoheldarr[item_stats->AmmoType] = (bondwalkItemCheckBitflags(item_id, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0)
        ? 0
        : (g_CurrentPlayer->ammoheldarr[item_stats->AmmoType] - hand_ptr->weapon_ammo_in_magazine) + ammo_in_magazine;

    if (item_id == ITEM_ROCKETLAUNCH)
    {
        currentPlayerCreateRocket(hand);
        return;
    }

    if ((item_id == ITEM_SHOTGUN) || (item_id == ITEM_AUTOSHOT))
    {
        ammo_in_hands = get_ammo_in_hands_weapon(hand);
        if (ammo_in_hands >= 5)
        {
            hand_ptr->numvisibleshells = 5;
            return;
        }
        hand_ptr->numvisibleshells = ammo_in_hands;
    }
}

#if defined(VERSION_US) || defined(VERSION_JP)
    #define WEAPON_1P_ANIM_TIME(x) ((f32)(x))
    #define WHEN_1_CASE_GRENADELAUNCH_FLD890 6
    #define WHEN_1_CASE_GRENADE_FLD890 0xf0
    #define WHEN_D_FLD890 0x14
    #define WHEN_5_SP188_INIT 0x10
    #define WHEN_5_SP188_MULTI 0xc
    #define WHEN_5_FLD8B0_SP 0x11
    #define WHEN_5_FLD8B0_MULTI 0xd
    #define WHEN_8_SP178_INIT 0x17
    #define WHEN_8_SP178_MULTI 0xc
    #define WHEN_A_FLD890 0x10
    #define WHEN_A_FLD8B0 0x11
    #define WHEN_C_FLD890 0x17
    #define WHEN_E_FLD890 0x10
    #define WHEN_10_FLD890 0x17
    #define WHEN_11_FLD890_1 0x10
    #define WHEN_11_FLD890_2 0x18
    #define WHEN_1E_FLD890 0x1e
#endif
#if defined(VERSION_EU)
    #define WEAPON_1P_ANIM_TIME(x) ((f32)(x)) * 60.0f / 50.0f
    #define WHEN_1_CASE_GRENADELAUNCH_FLD890 5
    #define WHEN_1_CASE_GRENADE_FLD890 0xc8
    #define WHEN_D_FLD890 0x10
    #define WHEN_5_SP188_INIT 0xd
    #define WHEN_5_SP188_MULTI 0xa
    #define WHEN_5_FLD8B0_SP 0xe
    #define WHEN_5_FLD8B0_MULTI 0xa
    #define WHEN_8_SP178_INIT 0x13
    #define WHEN_8_SP178_MULTI 0xa
    #define WHEN_A_FLD890 0xd
    #define WHEN_A_FLD8B0 0xe
    #define WHEN_C_FLD890 0x13
    #define WHEN_E_FLD890 0xd
    #define WHEN_10_FLD890 0x13
    #define WHEN_11_FLD890_1 0xd
    #define WHEN_11_FLD890_2 0x14
    #define WHEN_1E_FLD890 0x19
#endif


/**
 * Address: 7F064B28
 */
void gunTickHandState(enum GUNHAND hand, s32 triggerOn)
{
#if defined(VERSION_US)
    s32 stack1;
    s32 stack2;
    s32 sp1C4;
    s32 stack3;
    struct hand *sp1BC;
    s32 stack4;
    s32 sp1B4;
    struct sfx2 sp1B0;
    s32 stack5;
    struct WeaponStats *weapon_stats;
    s32 sp1A4;
    s32 sp1A0;
    f32 sp19C;
    f32 sp198;
    s32 stack7;
    f32 sp190;
    f32 sp18C;
    s32 sp188;
    f32 sp184;
    s32 stack8;
    s32 stack9;
    s32 sp178;
    f32 sp174;
    s32 stack10;
    s32 stack11;
    Mtxf sp12C;
    f32 sp128;
    s32 stack12;
    Mtxf spE4;
    f32 tempf;
    struct hand *handptr;
    Mtxf sp9C;
    f32 sp98;
    f32 sp94;
    enum ITEM_IDS temp_v0_3;
    f32 sp8C;
    f32 sp88;
    enum ITEM_IDS var_s1;
    struct sfx3 sp7C;
    struct PropRecord *temp_v0_8;
    Weapon1PTransformKeyframe *sp74;
    f32 temp_f0_2;
    u32 var_a0_2;
    f32 temp_v1_9;
    struct hand *temp_v1_5;
    f32 un_f32_num = 0.0f;
    f32 un_f32_div_1 = 16.0f;
    f32 un_f32_div_2 = 23.0f;
    s32 stack14;
    s32 stack15;
#endif
#if defined(VERSION_JP)
    s32 stack1;
    s32 stack2;
    s32 sp1C4;
    s32 stack3;
    struct hand *sp1BC;
    s32 stack4;
    s32 sp1B4;
    struct sfx2 sp1B0;
    s32 stack5;
    struct WeaponStats *weapon_stats;
    s32 sp1A4;
    s32 sp1A0;
    f32 sp19C;
    s32 stat_2;
    s32 stat_3;
    s32 stat_4;
    f32 sp198;
    s32 stack14;
    f32 sp190;
    f32 sp18C;
    s32 sp188;
    f32 sp184;
    s32 stack7;
    s32 stack8;
    s32 sp178;
    f32 sp174;
    s32 stack9;
    s32 stack10;
    Mtxf sp12C;
    f32 sp128;
    s32 stack11;
    Mtxf spE4;
    s32 stack12;
    f32 tempf;
    Mtxf sp9C;
    f32 sp98;
    f32 sp94;
    struct hand *handptr;
    f32 sp8C;
    f32 sp88;
    enum ITEM_IDS temp_v0_3;
    struct sfx3 sp7C;
    enum ITEM_IDS var_s1;
    Weapon1PTransformKeyframe *sp74;
    struct PropRecord *temp_v0_8;
    f32 temp_f0_2;
    u32 var_a0_2;
    f32 temp_v1_9;
    struct hand *temp_v1_5;
    f32 un_f32_num = 0.0f;
    f32 un_f32_div_1 = 16.0f;
    f32 un_f32_div_2 = 23.0f;
    s32 stack15;
#endif
#if defined(VERSION_EU)
    s32 stack1;
    s32 stack2;
    s32 sp1C4;
    s32 stack3;
    struct hand *sp1BC;
    s32 stack4;
    s32 sp1B4;
    struct sfx2 sp1B0;
    s32 stack5;
    struct WeaponStats *weapon_stats;
    s32 sp1A4;
    s32 sp1A0;
    f32 sp19C;
    s32 stat_2;
    s32 stat_3;
    s32 stat_4;
    f32 sp198;
    s32 stack14;
    f32 sp190;
    f32 sp18C;
    s32 sp188;
    f32 sp184;
    s32 stack7;
    s32 stack8;
    s32 sp178;
    f32 sp174;
    s32 stack9;
    s32 stack10;
    Mtxf sp12C;
    f32 sp128;
    s32 stack11;
    Mtxf spE4;
    s32 stack12;
    f32 tempf;
    Mtxf sp9C;
    f32 sp98;
    f32 sp94;
    struct hand *handptr;
    f32 sp8C;
    f32 sp88;
    enum ITEM_IDS temp_v0_3;
    struct sfx3 sp7C;
    enum ITEM_IDS var_s1;
    Weapon1PTransformKeyframe *sp74;
    struct PropRecord *temp_v0_8;
    f32 temp_f0_2;
    u32 var_a0_2;
    f32 temp_v1_9;
    struct hand *temp_v1_5;
    f32 un_f32_num = 0.0f;
    f32 un_f32_div_1 = 13.0f;
    f32 un_f32_div_2 = 19.0f;
    s32 stack15;
#endif

    handptr = &g_CurrentPlayer->hands[hand];
    var_s1 = get_item_in_hand_or_watch_menu(hand);
    sp1C4 = get_ammo_type_for_weapon(var_s1);

    handptr->field_884 = handptr->weapon_hold_time;
    handptr->weapon_hold_time = triggerOn;

    if (triggerOn == 0)
    {
        handptr->field_888 = 1;
    }

    handptr->weapon_firing_status = 0;
    handptr->field_87D = 0;

    if (g_ClockTimer > 0)
    {
        handptr->field_890 += g_ClockTimer;
        handptr->field_88C += 1;
    }

    handptr->field_92C = 0;

    if (handptr->weapon_action_state == GUN_ANIM_STATE_IDLE)
    {
#if defined(VERSION_JP) || defined(VERSION_EU)
        if ((var_s1 == ITEM_LASER) && (handptr->field_888 != 0))
        {
            handptr->field_8A0 = 0;
        }
#endif
        if (
            (handptr->weapon_hold_time != 0)
            && (var_s1 != ITEM_UNARMED)
            && (((bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_CLICKY) != 0)) || (handptr->weapon_ammo_in_magazine > 0))
#if defined(VERSION_JP) || defined(VERSION_EU)
            && ((var_s1 != ITEM_LASER) || (handptr->field_8A0 < 0xC8))
#endif
        )
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_TRIGGER_PRESS;
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            handptr->field_888 = 0;
        }
        else
        {
            if (handptr->weapon_current_animation != 0)
            {
                handptr->weapon_action_state = handptr->weapon_current_animation;
                handptr->field_890 = 0;
                handptr->field_88C = 0;
            }
        }

        handptr->weapon_current_animation = 0;

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_IDLE)
            && (handptr->weapon_ammo_in_magazine == 0)
            && (sp1C4 != 0))
        {
            if ((lvlGetControlsLockedFlag() == 0) && (g_CurrentPlayer->mpmenuon == 0))
            {
                /**
                 * D_80032458 is always 0 so this branch can never execute.
                 */
                if ((D_80032458 != 0) && (sp1C4 == 1) && (g_CurrentPlayer->ammoheldarr[sp1C4] <= 0))
                {
                    g_CurrentPlayer->ammoheldarr[sp1C4] = 1;
                }

                if (get_ammo_in_hands_weapon(hand) > 0)
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_START;
                    handptr->field_890 = 0;
                    handptr->field_88C = 0;
                }
                else
                {
                    if (g_CurrentPlayer->trigger_released != 0)
                    {
                        temp_v0_3 = get_item_in_hand_or_watch_menu(1 - hand);

                        sp1BC = (g_CurrentPlayer->hands - hand) + 1;

                        if ((sp1BC->weapon_action_state == GUN_ANIM_STATE_IDLE)
                            && (sp1BC->weapon_current_animation == 0)
                            && (
                                (temp_v0_3 == ITEM_UNARMED)
                                || ((sp1BC->weapon_ammo_in_magazine == 0)
                                    && ((get_ammo_type_for_weapon(temp_v0_3) != 0))
                                    && ((get_ammo_in_hands_weapon(1 - hand) <= 0)))))
                        {
                            autoadvance_on_deplete_all_ammo();

                            handptr->field_88C = 0;
                            handptr->field_890 = 0;
                            handptr->weapon_action_state = handptr->weapon_current_animation;
                            handptr->weapon_current_animation = 0;

                            sp1BC->field_88C = 0;
                            sp1BC->field_890 = 0;
                            sp1BC->weapon_action_state = sp1BC->weapon_current_animation;
                            sp1BC->weapon_current_animation = 0;
                        }
                    }
                }
            }
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_TRIGGER_PRESS)
    {
        switch (var_s1)
        {
        case ITEM_RUGER:
        case ITEM_GRENADELAUNCH:
            if (handptr->field_890 >= WHEN_1_CASE_GRENADELAUNCH_FLD890)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
                handptr->field_890 = 0;
                handptr->field_88C = 0;
            }
            break;
        case ITEM_CAMERA:
            if (handptr->field_88C == 0)
            {
                currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            }
            else if (handptr->field_890 > 0)
            {
                currentPlayerAdjustFade(8.0f, 0, 0, 0, 0.0f);
                handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
                handptr->field_890 = 0;
                handptr->field_88C = 0;
            }
            break;
        case ITEM_WPPK:
        case ITEM_WPPKSIL:
        case ITEM_TT33:
        case ITEM_SKORPION:
        case ITEM_AK47:
        case ITEM_UZI:
        case ITEM_MP5K:
        case ITEM_MP5KSIL:
        case ITEM_SPECTRE:
        case ITEM_M16:
        case ITEM_FNP90:
        case ITEM_SHOTGUN:
        case ITEM_AUTOSHOT:
        case ITEM_SNIPERRIFLE:
        case ITEM_GOLDENGUN:
        case ITEM_SILVERWPPK:
        case ITEM_GOLDWPPK:
        case ITEM_LASER:
        case ITEM_WATCHLASER:
        case ITEM_ROCKETLAUNCH:
        case ITEM_TRIGGER:
        case ITEM_TANKSHELLS:
        case ITEM_FLAREPISTOL:
        case ITEM_PITONGUN:
        case ITEM_WATCHMAGNETATTRACT:
            handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            break;
        case ITEM_TIMEDMINE:
        case ITEM_PROXIMITYMINE:
        case ITEM_REMOTEMINE:
        case ITEM_BOMBCASE:
        case ITEM_PLASTIQUE:
        case ITEM_BUG:
        case ITEM_MICROCAMERA:
        case ITEM_GOLDENEYEKEY:
            handptr->weapon_action_state = GUN_ANIM_STATE_MINE_PLACE;
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            break;
        case ITEM_KNIFE:
            if (!(randomGetNext() & 1))
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_BEGIN;
            }
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            break;
        case ITEM_GRENADE:
            if ((handptr->field_888 != 0) || (handptr->field_890 >= WHEN_1_CASE_GRENADE_FLD890))
            {
                g_CurrentPlayer->last_z_trigger_timer = handptr->field_890;
                handptr->weapon_action_state = GUN_ANIM_STATE_GRENADE_THROW;
                handptr->field_88C = 0;
                handptr->field_890 = 0;
            }
            break;
        case ITEM_FIST:
            if (!(randomGetNext() & 1))
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH1_STRIKE;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH2_STRIKE;
            }
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            break;
        case ITEM_THROWKNIFE:
            handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_DRAW;
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            break;
        case ITEM_TASER:
            tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
            if (gunSample1PTransform(taserFireKeyFrames, tempf, &handptr->field_8EC, hand) != 0)
            {
                handptr->field_92C = 1;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
                handptr->field_890 = 0;
                handptr->field_88C = 0;
            }
            break;
        case ITEM_BUNGEE:
        case ITEM_DOORDECODER:
        case ITEM_BOMBDEFUSER:
        case ITEM_LOCKEXPLODER:
        case ITEM_DOOREXPLODER:
        case ITEM_WEAPONCASE:
        case ITEM_SAFECRACKERCASE:
        case ITEM_KEYANALYSERCASE:
        case ITEM_BUGDETECTOR:
        case ITEM_EXPLOSIVEFLOPPY:
        case ITEM_POLARIZEDGLASSES:
        case ITEM_DARKGLASSES:
        case ITEM_CREDITCARD:
        case ITEM_GASKEYRING:
        case ITEM_DATATHIEF:
        case ITEM_WATCHIDENTIFIER:
        case ITEM_WATCHCOMMUNICATOR:
        case ITEM_WATCHGEIGERCOUNTER:
        case ITEM_WATCHMAGNETREPEL:
        case ITEM_DATTAPE:
        case ITEM_KEYCARD:
        case ITEM_KEYYALE:
        case ITEM_KEYBOLT:
            handptr->weapon_action_state = GUN_ANIM_STATE_USE_ITEM;
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            break;
        case ITEM_SUIT_LF_HAND:
        case ITEM_JOYPAD:
        case ITEM_ROCKETROUND:
        case ITEM_GRENADEROUND:
        case ITEM_TOKEN:
        default:
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0;
            handptr->field_88C = 0;
            break;
        }

        handptr->volley = 0;
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_FIRE)
    {
        if ((get_ammo_type_for_weapon(var_s1) == 0) || (handptr->weapon_ammo_in_magazine > 0))
        {
            switch (var_s1)
            {
            case ITEM_CAMERA:
            case ITEM_WATCHMAGNETATTRACT:
                if (handptr->field_88C == 0)
                {
                    handptr->weapon_firing_status = (lvlGetControlsLockedFlag() == 0) && (g_CurrentPlayer->mpmenuon == 0);
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->field_890 = 0;
                    handptr->field_88C = 0;
                }
                break;
            case ITEM_WPPK:
            case ITEM_WPPKSIL:
            case ITEM_TT33:
            case ITEM_SHOTGUN:
            case ITEM_AUTOSHOT:
            case ITEM_SNIPERRIFLE:
            case ITEM_RUGER:
            case ITEM_GOLDENGUN:
            case ITEM_SILVERWPPK:
            case ITEM_GOLDWPPK:
            case ITEM_LASER:
            case ITEM_WATCHLASER:
            case ITEM_GRENADELAUNCH:
            case ITEM_ROCKETLAUNCH:
            case ITEM_TRIGGER:
            case ITEM_TANKSHELLS:
            case ITEM_FLAREPISTOL:
            case ITEM_PITONGUN:
                if (handptr->field_88C == 0)
                {
                    if ((getPlayerCount() == 1) || ((checkGamePaused() == 0) && (g_CurrentPlayer->mpmenuon == 0)))
                    {
                        handptr->field_87D = 1;
                    }

                    handptr->weapon_firing_status = (lvlGetControlsLockedFlag() == 0) && (g_CurrentPlayer->mpmenuon == 0);

                    sub_GAME_7F05E808(hand);
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->field_890 = 0;
                    handptr->field_88C = 0;
                }
                break;
            case ITEM_SKORPION:
            case ITEM_AK47:
            case ITEM_UZI:
            case ITEM_MP5K:
            case ITEM_MP5KSIL:
            case ITEM_SPECTRE:
            case ITEM_M16:
            case ITEM_FNP90:
                if ((handptr->field_88C == 0)
                    || (handptr->weapon_hold_time != 0)
                    || ((bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_BURST_FIRE) != 0)
                        && (currentPlayerGetIsAiming() == 0)
                        && (((s32) handptr->volley % 3) != 0)))
                {
                    if (((s32) handptr->field_88C % bondwalkItemGetAutomaticFiringRate(var_s1)) == 0)
                    {
                        if ((getPlayerCount() == 1) || ((checkGamePaused() == 0) && (g_CurrentPlayer->mpmenuon == 0)))
                        {
                            handptr->field_87D = 1;
                        }

                        handptr->weapon_firing_status = (lvlGetControlsLockedFlag() == 0)
                            && (g_CurrentPlayer->mpmenuon == 0);
                    }
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->field_890 = 0;
                    handptr->field_88C = 0;
                }
                break;
            case ITEM_KNIFE:
                if ((handptr->field_88C == 0) || (handptr->weapon_hold_time != 0))
                {
                    handptr->weapon_firing_status = 0;
                    handptr->field_87D = handptr->weapon_firing_status;
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->field_890 = 0;
                    handptr->field_88C = 0;
                }
                break;
            case ITEM_TASER:
                if ((handptr->field_88C == 0) || (handptr->weapon_hold_time != 0))
                {
                    gunSample1PTransform(taserRaiseKeyframes, 0.0f, &handptr->field_8EC, hand);

                    handptr->weapon_firing_status = 0;
                    handptr->field_92C = 1;
                    handptr->field_87D = handptr->weapon_firing_status;

                    if (handptr->field_88C == 0)
                    {
                        handptr->weapon_firing_status = (lvlGetControlsLockedFlag() == 0)
                            && (g_CurrentPlayer->mpmenuon == 0);
                    }
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->field_890 = 0;
                    handptr->field_88C = 0;
                }
                break;
            }

            if (handptr->weapon_firing_status != 0)
            {
                if (var_s1 != ITEM_CAMERA)
                {
                    joyRumblePakStart(get_cur_playernum(), 0.1f);

                    if (cur_player_get_control_type() >= 4)
                    {
                        joyRumblePakStart(get_cur_playernum() + getPlayerCount(), 0.1f);
                    }
                }

                handptr->weapon_ammo_in_magazine -= 1;
                handptr->volley += 1;
            }

            if (handptr->weapon_action_state == GUN_ANIM_STATE_FIRE)
            {
                sp1B4 = 0;

                if (bondwalkItemGetSoundTriggerRate(var_s1) > 0)
                {
                    if ((g_CurrentPlayer->hands[1 - hand].field_A50 != g_GlobalTimer)
                        && (handptr->field_A4C < g_GlobalTimer))
                    {
                        handptr->field_A4C = bondwalkItemGetSoundTriggerRate(var_s1) + g_GlobalTimer;
                        sp1B4 = 1;
                    }
                }
                else if (handptr->weapon_firing_status != 0)
                {
                    sp1B4 = 1;
                }

                if ((getPlayerCount() == 1) || ((checkGamePaused() == 0) && (g_CurrentPlayer->mpmenuon == 0)))
                {
                    if (sp1B4 != 0)
                    {
                        if ((handptr->audioHandle != NULL) && (sndGetPlayingState(handptr->audioHandle) != 0))
                        {
                            sndDeactivate(handptr->audioHandle);
                        }

                        if (((struct ALSoundState *)handptr->field_A48 != 0)
                            && (sndGetPlayingState((struct ALSoundState *) handptr->field_A48) != 0))
                        {
                            sndDeactivate((struct ALSoundState *) handptr->field_A48);
                        }

                        if (bondwalkItemGetSound(var_s1) != 0)
                        {
                            if (handptr->audioHandle == NULL)
                            {
                                sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, bondwalkItemGetSound(var_s1), (struct ALSoundState *) &handptr->audioHandle);
                            }
                            else if ((struct ALSoundState *)handptr->field_A48 == 0)
                            {
                                sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, bondwalkItemGetSound(var_s1), (struct ALSoundState *) &handptr->field_A48);
                            }

                            handptr->field_A50 = g_GlobalTimer;
                        }
                    }

                    if (var_s1 == ITEM_WATCHLASER)
                    {
                        sp1B0 = watchlaser_fire_sounds;
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, sp1B0.half[randomGetNext() & 1], NULL);
                    }
                }
            }
        }
        else if (handptr->field_88C > 0)
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
            handptr->field_890 = 0;
            handptr->field_88C = 0;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_DRY_FIRE;
            handptr->field_890 = 0;
            handptr->field_88C = 0;

            if ((getPlayerCount() == 1)
#if defined(VERSION_JP) || defined(VERSION_EU)
                || ((checkGamePaused() == 0) && (g_CurrentPlayer->mpmenuon == 0))
#else
                || (checkGamePaused() == 0)
#endif
               )
            {
                sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, EMPTY_GUN_FIRE_SFX, NULL);
            }
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RECOIL1)
    {
        if (var_s1 == ITEM_TASER)
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
            if (gunSample1PTransform(taserRaiseKeyframes, tempf, &handptr->field_8EC, hand) != 0)
            {
                handptr->field_92C = 1;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
                handptr->field_890 = 0.0f;
                handptr->field_88C = 0;
            }
        }
        else
        {
            weapon_stats = get_ptr_item_statistics(var_s1);

#if defined(VERSION_US)
            sp1A4 = weapon_stats->b44[0];
            sp1A0 = weapon_stats->b44[1];
#endif
#if defined(VERSION_JP)
            sp1A4 = weapon_stats->b44[0];
            sp1A0 = weapon_stats->b44[1];
            stat_2 = weapon_stats->b44[2];
            stat_3 = weapon_stats->b44[3];
            stat_4 = weapon_stats->SingleFiringRate;
#endif
#if defined(VERSION_EU)
            sp1A4 = ((s32)weapon_stats->b44[0] * 50) / 60;
            sp1A0 = ((s32)weapon_stats->b44[1] * 50) / 60;
            stat_2 = ((s32)weapon_stats->b44[2] * 50) / 60;
            stat_3 = ((s32)weapon_stats->b44[3] * 50) / 60;
            stat_4 = weapon_stats->SingleFiringRate * 50 / 60;
#endif

            if ((
                    (handptr->field_888 != 0)
                    && (handptr->field_890 >= (sp1A4 + sp1A0))
                )
                ||
                (
                    ((weapon_stats->SingleFiringRate >= 0))
                    && (handptr->field_888 == 0)
#if defined(VERSION_US)
                    && (handptr->field_890 >= (sp1A4 + sp1A0 + weapon_stats->SingleFiringRate))
#endif
#if defined(VERSION_JP) ||  defined(VERSION_EU)
                    && (handptr->field_890 >= (sp1A4 + sp1A0 + stat_4))
#endif
                )
               )
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
                handptr->field_890 = 0.0f;
                handptr->field_88C = 0;
            }
            else if (
                (handptr->field_888 != 0)
                && (handptr->weapon_hold_time != 0)

#if defined(VERSION_US)
                && (handptr->field_890 >= weapon_stats->b44[2])
#endif
#if defined(VERSION_JP) ||  defined(VERSION_EU)
                && (handptr->field_890 >= stat_2)
#endif

                && (weapon_stats->b44[3] >= 0)

#if defined(VERSION_US)
                // HACK: registers are swapped
                // addu a1, v1, a0
                && (handptr->field_890 + weapon_stats->b44[3] < (0,sp1A4) + sp1A0)
                && (handptr->field_890 + weapon_stats->b44[3] >= (s32)weapon_stats->b44[2])
#endif
#if defined(VERSION_JP) ||  defined(VERSION_EU)
                && (handptr->field_890 + stat_3 < sp1A4 + sp1A0)
                && (handptr->field_890 + stat_3 >= (s32)stat_2)
#endif
            )
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL2;
                handptr->field_890 = 0;
                handptr->field_88C = 0;
#if defined(VERSION_US)
                handptr->field_8A8 = weapon_stats->b44[3];
#endif
#if defined(VERSION_JP) ||  defined(VERSION_EU)
                handptr->field_8A8 = stat_3;
#endif
            }
            else if (handptr->field_890 < sp1A4 + sp1A0)
            {
                sp198 = weapon_stats->RecoilBack;
                sp19C = weapon_stats->RecoilUp;

                if (handptr->field_890 == 0)
                {
                    handptr->field_8C8 = handptr->field_8E8;
                    handptr->field_8BC = handptr->field_8DC;
                    handptr->field_8C0 = handptr->field_8E0;
                    handptr->field_8C4 = handptr->field_8E4;
                }

                if (handptr->field_890 < sp1A4)
                {
                    handptr->field_8D8 = M_TAU_F - ((sp19C * M_TAU_F) / 360.0f);

                    handptr->field_8CC = ((gunSetHorizontalOffset(hand) - handptr->field_A38) * sp198) / 1000.0f;
                    handptr->field_8D0 = 0;
                    handptr->field_8D4 = ((weapon_stats->PosZ - handptr->field_A40) * sp198) / 1000.0f;

                    sp190 = sinf(((f32) handptr->field_890 * M_PI_2F) / (f32) sp1A4);
                }
                else
                {
                    handptr->field_8D8 = M_TAU_F - ((sp19C * M_TAU_F) / 360.0f);

                    handptr->field_8CC = ((gunSetHorizontalOffset(hand) - handptr->field_A38) * sp198) / 1000.0f;
                    handptr->field_8D0 = 0;
                    handptr->field_8D4 = ((weapon_stats->PosZ - handptr->field_A40) * sp198) / 1000.0f;

                    sp190 = (cosf(((f32) (handptr->field_890 - sp1A4) * M_PI_F) / (f32) sp1A0) * 0.5f) + 0.5f;
                }

                temp_f0_2 = sub_GAME_7F06D0CC(handptr->field_8C8, handptr->field_8D8, sp190);

                handptr->field_8E8 = temp_f0_2;
                handptr->field_92C = 1;
                handptr->field_8DC = ((handptr->field_8CC - handptr->field_8BC) * sp190) + handptr->field_8BC;
                handptr->field_8E0 = ((handptr->field_8D0 - handptr->field_8C0) * sp190) + handptr->field_8C0;
                handptr->field_8E4 = ((handptr->field_8D4 - handptr->field_8C4) * sp190) + handptr->field_8C4;

                matrix_4x4_set_rotation_around_x(temp_f0_2, &handptr->field_8EC);
                matrix_4x4_set_position((struct coord3d *)&handptr->field_8DC, &handptr->field_8EC);
            }
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RECOIL2)
    {
        if (handptr->field_890 == 0)
        {
            handptr->field_8C8 = handptr->field_8E8;
            handptr->field_8BC = handptr->field_8DC;
            handptr->field_8C0 = handptr->field_8E0;
            handptr->field_8C4 = handptr->field_8E4;
            handptr->field_8D8 = 0.0f;
            handptr->field_8CC = 0.0f;
            handptr->field_8D0 = 0.0f;
            handptr->field_8D4 = 0.0f;
        }

        if (handptr->field_890 < handptr->field_8A8)
        {
            sp18C = (cosf(((f32) (handptr->field_8A8 - handptr->field_890) * M_PI_2F) / (f32) handptr->field_8A8) * 0.5f) + 0.5f;

            temp_f0_2 = sub_GAME_7F06D0CC(handptr->field_8C8, handptr->field_8D8, sp18C);

            handptr->field_8E8 = temp_f0_2;
            handptr->field_92C = 1;
            handptr->field_8DC = ((handptr->field_8CC - handptr->field_8BC) * sp18C) + handptr->field_8BC;
            handptr->field_8E0 = ((handptr->field_8D0 - handptr->field_8C0) * sp18C) + handptr->field_8C0;
            handptr->field_8E4 = ((handptr->field_8D4 - handptr->field_8C4) * sp18C) + handptr->field_8C4;

            matrix_4x4_set_rotation_around_x(temp_f0_2, &handptr->field_8EC);
            matrix_4x4_set_position((struct coord3d *)&handptr->field_8DC, &handptr->field_8EC);
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_DRY_FIRE)
    {
        if (handptr->field_88C == 0)
        {
            sub_GAME_7F05E808(hand);
        }

        if ((handptr->field_888 != 0) || ((handptr->field_888 == 0) && (handptr->field_890 >= WHEN_D_FLD890)))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    sp188 = WHEN_5_SP188_INIT;
    if (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_LOWER)
    {
        if (getPlayerCount() >= 2)
        {
            sp188 = WHEN_5_SP188_MULTI;
        }

        if (handptr->field_88C == 0)
        {
            if (getPlayerCount() == 1)
            {
                handptr->field_8B0 = WHEN_5_FLD8B0_SP;
            }
            else
            {
                handptr->field_8B0 = WHEN_5_FLD8B0_MULTI;
            }
        }

        if (handptr->field_890 >= sp188)
        {
            g_CurrentPlayer->ammoheldarr[get_ammo_type_for_weapon(var_s1)] += handptr->weapon_ammo_in_magazine;
            handptr->weapon_ammo_in_magazine = 0;

            if (getPlayerCount() >= 2)
            {
                sub_GAME_7F09B368(hand);
            }

            sub_GAME_7F05FB00(hand);

            handptr->weapon_action_state = GUN_ANIM_STATE_SWITCH_SWAP;

            if (bondinvItemAvailable(ITEM_SNIPERRIFLE) != 0)
            {
                g_CurrentPlayer->cur_item_weapon_getname = ITEM_SNIPERRIFLE;
            }
            else
            {
                g_CurrentPlayer->cur_item_weapon_getname = ITEM_FIST;
            }
        }
        else
        {
            sp184 = ((f32) handptr->field_890 * M_LN2F) / (f32) sp188;
            handptr->field_92C = 1;

            matrix_4x4_set_rotation_around_x(sp184, &handptr->field_8EC);

            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = (1.0f - cosf(sp184)) * -60.0f;
            handptr->field_8EC.m[3][2] = sinf(sp184) * 15.0f;
        }
    }

    if ((handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP) || (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_HOLD))
    {
        if ((handptr->weapon_animation_trigger == 0) || (handptr->field_890 >= handptr->field_8B0))
        {
            if (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP)
            {
                temp_v1_5 = (g_CurrentPlayer->hands - hand) + 1;

                if ((temp_v1_5->weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP) && (temp_v1_5->weapon_action_state != GUN_ANIM_STATE_SWITCH_LOWER))
                {
                    if (
                        (temp_v1_5->weapon_current_animation != 5)
                        && (temp_v1_5->weapon_action_state != GUN_ANIM_STATE_WATCH_LOWER)
                        && (temp_v1_5->weapon_action_state != GUN_ANIM_STATE_WATCH_SWAP)
                        && (temp_v1_5->weapon_action_state != GUN_ANIM_STATE_WATCH_RAISE)
                        && (temp_v1_5->weapon_current_animation != 0xE))
                    {
                        if (hand == GUNRIGHT)
                        {
                            if (bondinvItemAvailableForHand(handptr->weapon_next_weapon, getCurrentPlayerWeaponId(GUNLEFT)) == 0)
                            {
                                currentPlayerEquipWeaponWrapper(GUNLEFT, 0);
                            }
                        }
                        else if (bondinvItemAvailableForHand(getCurrentPlayerWeaponId(GUNRIGHT), handptr->weapon_next_weapon) == 0)
                        {
                            handptr->weapon_next_weapon = ITEM_UNARMED;
                        }
                    }
                }
                currentPlayerUnEquipWeaponWrapper(hand, handptr->weapon_next_weapon);
                var_s1 = get_item_in_hand_or_watch_menu(hand);
                handptr->weapon_action_state = GUN_ANIM_STATE_SWITCH_HOLD;
            }
            else if (Gun_hand_without_item(hand) != 0)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_SWITCH_RAISE;
                handptr->field_890 = 0.0f;
                handptr->field_88C = 0;
            }
        }

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP) || (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_HOLD))
        {
            handptr->field_92C = 1;
            matrix_4x4_set_rotation_around_x(M_LN2F, &handptr->field_8EC);
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = (1.0f - cosf(M_LN2F)) * -60.0f;
            handptr->field_8EC.m[3][2] = sinf(M_LN2F) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_RAISE)
    {
        sp178 = WHEN_8_SP178_INIT;

        if (getPlayerCount() >= 2)
        {
            sp178 = WHEN_8_SP178_MULTI;
        }

        if (handptr->field_88C == 0)
        {
            if (getPlayerCount() >= 2)
            {
                sub_GAME_7F09B398(hand);
            }

            sub_GAME_7F0649D8(hand);

            g_CurrentPlayer->trigger_released = 0;

            if ((g_ClockTimer > 0)
                && (g_CurrentPlayer->cameramode != CAMERAMODE_INTRO)
                && (Gun_hand_without_item(hand) != 0)
                && (g_PlayerInvincible == FALSE)
#if defined(VERSION_JP) || defined(VERSION_EU)
                && (g_CurrentPlayer->bonddead == 0)
#endif
               )
            {
                switch (var_s1)
                {
                    case ITEM_LASER:
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, PICKUP_LASER_SFX, NULL);
                        break;

                    case ITEM_KNIFE:
                    case ITEM_THROWKNIFE:
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, PICKUP_KNIFE_SFX, NULL);
                        break;

                    case ITEM_TIMEDMINE:
                    case ITEM_PROXIMITYMINE:
                    case ITEM_REMOTEMINE:
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, PICKUP_MINE_SFX, NULL);
                        break;

                    case ITEM_UNARMED:
                    case ITEM_FIST:
                    case ITEM_WATCHLASER:
                    case ITEM_GRENADE:
                    case ITEM_TRIGGER:
                    case ITEM_TASER:
                    case ITEM_TANKSHELLS:
                    case ITEM_BOMBCASE:
                    case ITEM_PLASTIQUE:
                    case ITEM_CAMERA:
                    case ITEM_BUG:
                    case ITEM_MICROCAMERA:
                    case ITEM_WATCHMAGNETATTRACT:
                    case ITEM_GOLDENEYEKEY:
                    case ITEM_TOKEN:
                        break;

                    default:
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, PICKUP_GUN_SFX, NULL);
                        break;
                }
            }
        }

        if ((handptr->field_890 >= sp178)
            || (get_ptr_weapon_model_header_line(var_s1) == NULL)
            || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)
            || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
        else
        {
            sp174 = ((f32) (sp178 - handptr->field_890) * M_LN2F) / (f32) sp178;
            handptr->field_92C = 1;
            matrix_4x4_set_rotation_around_x(sp174, &handptr->field_8EC);
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = (1.0f - cosf(sp174)) * -60.0f;
            handptr->field_8EC.m[3][2] = sinf(sp174) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_START)
    {
        if (((handptr->weapon_ammo_in_magazine < get_ptr_item_statistics(var_s1)->MagSize)
             || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0))
            && ((get_ammo_in_hands_weapon(hand) > 0)))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_LOWER;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_LOWER)
    {
        if ((handptr->field_890 >= WHEN_A_FLD890) || (handptr->field_87F == 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_SWAP;
            handptr->field_8B0 = WHEN_A_FLD8B0;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
        else
        {
            sp128 = ((f32) handptr->field_890 * M_LN2F) / un_f32_div_1;
            handptr->field_92C = 1;

            if (hand == GUNRIGHT)
            {
                matrix_4x4_set_rotation_around_z((un_f32_num / un_f32_div_1), &handptr->field_8EC);
            }
            else
            {
                matrix_4x4_set_rotation_around_z(-(un_f32_num / un_f32_div_1), &handptr->field_8EC);
            }

            matrix_4x4_set_rotation_around_x(sp128, &sp12C);
            matrix_4x4_multiply_in_place(&sp12C, &handptr->field_8EC);
            sinf((un_f32_num / un_f32_div_1));
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = sub_GAME_7F0649AC(var_s1) * (1.0f - cosf(sp128));
            handptr->field_8EC.m[3][2] = sinf(sp128) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_SWAP)
    {
        if ((handptr->field_88C == 0)
#if defined(VERSION_JP) || defined(VERSION_EU)
            && (g_ClockTimer > 0)
#endif
            && (g_CurrentPlayer->cameramode != CAMERAMODE_INTRO)
            && (Gun_hand_without_item(hand) != 0)
            && (g_PlayerInvincible == FALSE)
#if defined(VERSION_JP) || defined(VERSION_EU)
            && (g_CurrentPlayer->bonddead == 0)
#endif
           )
        {
            switch (var_s1)
            {
            case ITEM_UNARMED:
            case ITEM_FIST:
            case ITEM_KNIFE:
            case ITEM_THROWKNIFE:
            case ITEM_LASER:
            case ITEM_WATCHLASER:
            case ITEM_GRENADE:
            case ITEM_TIMEDMINE:
            case ITEM_PROXIMITYMINE:
            case ITEM_REMOTEMINE:
            case ITEM_TRIGGER:
            case ITEM_TASER:
            case ITEM_TANKSHELLS:
            case ITEM_BOMBCASE:
            case ITEM_PLASTIQUE:
            case ITEM_CAMERA:
            case ITEM_BUG:
            case ITEM_MICROCAMERA:
            case ITEM_WATCHMAGNETATTRACT:
            case ITEM_GOLDENEYEKEY:
            case ITEM_TOKEN:
                break;
            default:
            case ITEM_WPPK:
            case ITEM_WPPKSIL:
            case ITEM_TT33:
            case ITEM_SKORPION:
            case ITEM_AK47:
            case ITEM_UZI:
            case ITEM_MP5K:
            case ITEM_MP5KSIL:
            case ITEM_SPECTRE:
            case ITEM_M16:
            case ITEM_FNP90:
            case ITEM_SHOTGUN:
            case ITEM_AUTOSHOT:
            case ITEM_SNIPERRIFLE:
            case ITEM_RUGER:
            case ITEM_GOLDENGUN:
            case ITEM_SILVERWPPK:
            case ITEM_GOLDWPPK:
            case ITEM_GRENADELAUNCH:
            case ITEM_ROCKETLAUNCH:
            case ITEM_FLAREPISTOL:
            case ITEM_PITONGUN:
            case ITEM_BUNGEE:
            case ITEM_DOORDECODER:
            case ITEM_BOMBDEFUSER:
            case ITEM_LOCKEXPLODER:
            case ITEM_DOOREXPLODER:
            case ITEM_BRIEFCASE:
            case ITEM_WEAPONCASE:
            case ITEM_SAFECRACKERCASE:
            case ITEM_KEYANALYSERCASE:
            case ITEM_BUGDETECTOR:
            case ITEM_EXPLOSIVEFLOPPY:
            case ITEM_POLARIZEDGLASSES:
            case ITEM_DARKGLASSES:
            case ITEM_CREDITCARD:
            case ITEM_GASKEYRING:
            case ITEM_DATATHIEF:
            case ITEM_WATCHIDENTIFIER:
            case ITEM_WATCHCOMMUNICATOR:
            case ITEM_WATCHGEIGERCOUNTER:
            case ITEM_WATCHMAGNETREPEL:
                sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GUN_RIFLECOCK_SFX, NULL);
                break;
            }
        }

        if ((handptr->field_890 >= handptr->field_8B0) && !(((handptr->field_88C < 2))))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_RAISE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
        else
        {
            handptr->field_92C = 1;

            if (hand == GUNRIGHT)
            {
                matrix_4x4_set_rotation_around_z(un_f32_num, &handptr->field_8EC);
            }
            else
            {
                matrix_4x4_set_rotation_around_z(-un_f32_num, &handptr->field_8EC);
            }

            matrix_4x4_set_rotation_around_x(M_LN2F, &spE4);
            matrix_4x4_multiply_in_place(&spE4, &handptr->field_8EC);
            sinf(un_f32_num);
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = sub_GAME_7F0649AC(var_s1) * (1.0f - cosf(M_LN2F));
            handptr->field_8EC.m[3][2] = sinf(M_LN2F) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_RAISE)
    {
        if (handptr->field_88C == 0)
        {
            sub_GAME_7F0649D8(hand);
            g_CurrentPlayer->trigger_released = 0;
        }

        if ((handptr->field_890 >= WHEN_C_FLD890)
            || (get_ptr_weapon_model_header_line(var_s1) == NULL)
            || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)
            || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
        else
        {
            sp98 = ((f32) (WHEN_C_FLD890 - handptr->field_890) * M_LN2F) / un_f32_div_2;
            handptr->field_92C = 1;

            if (hand == GUNRIGHT)
            {
                matrix_4x4_set_rotation_around_z((un_f32_num / un_f32_div_2), &handptr->field_8EC);
            }
            else
            {
                matrix_4x4_set_rotation_around_z(-(un_f32_num / un_f32_div_2), &handptr->field_8EC);
            }

            matrix_4x4_set_rotation_around_x(sp98, &sp9C);
            matrix_4x4_multiply_in_place(&sp9C, &handptr->field_8EC);
            sinf(un_f32_num / un_f32_div_2);
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = sub_GAME_7F0649AC(var_s1) * (1.0f - cosf(sp98));
            handptr->field_8EC.m[3][2] = sinf(sp98) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_WATCH_LOWER)
    {
        if ((handptr->field_890 >= WHEN_E_FLD890) || (handptr->field_87F == 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_WATCH_SWAP;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
        else
        {
            sp94 = ((f32) handptr->field_890 * M_LN2F) / un_f32_div_1;
            handptr->field_92C = 1;

            matrix_4x4_set_rotation_around_x(sp94, &handptr->field_8EC);
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = (1.0f - cosf(sp94)) * -60.0f;
            handptr->field_8EC.m[3][2] = sinf(sp94) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_WATCH_SWAP)
    {
        if ((handptr->field_88C == 0) || (Gun_hand_without_item(hand) == 0))
        {
            sub_GAME_7F05DA8C(hand, handptr->weapon_next_weapon);
            var_s1 = get_item_in_hand_or_watch_menu(hand);
        }

        if (Gun_hand_without_item(hand) != 0)
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_WATCH_RAISE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
        else
        {
            handptr->field_92C = 1;
            matrix_4x4_set_rotation_around_x(M_LN2F, &handptr->field_8EC);
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = (1.0f - cosf(M_LN2F)) * -60.0f;
            handptr->field_8EC.m[3][2] = sinf(M_LN2F) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_WATCH_RAISE)
    {
        if ((handptr->field_88C == 0) && (var_s1 < 0x21))
        {
            if (getPlayerCount() >= 2)
            {
                sub_GAME_7F09B398(hand);
            }
            sub_GAME_7F0649D8(hand);
            g_CurrentPlayer->trigger_released = 0;
        }

        if ((handptr->field_890 >= WHEN_10_FLD890)
            || (get_ptr_weapon_model_header_line(var_s1) == NULL)
            || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)
            || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
        else
        {
            sp8C = ((f32) (WHEN_10_FLD890 - handptr->field_890) * M_LN2F) / un_f32_div_2;
            handptr->field_92C = 1;
            matrix_4x4_set_rotation_around_x(sp8C, &handptr->field_8EC);
            handptr->field_8EC.m[3][0] = 0.0f;
            handptr->field_8EC.m[3][1] = (1.0f - cosf(sp8C)) * -60.0f;
            handptr->field_8EC.m[3][2] = sinf(sp8C) * 15.0f;
        }
    }

    if ((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_BEGIN)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER))
    {
        sp88 = WEAPON_1P_ANIM_TIME(handptr->field_890);

        if (((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
                || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_BEGIN))
                && (handptr->field_890 >= WHEN_11_FLD890_1))
        {
            sp7C = knife_throw_sounds;
            sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, sp7C.half[randomGetNext() % 3U], NULL);


            if (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_STRIKE;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_STRIKE;
            }
        }

        if ((handptr->weapon_action_state != GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER)
            && (handptr->weapon_action_state != GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER)
            && (handptr->field_890 >= WHEN_11_FLD890_2))
        {
            handptr->weapon_firing_status = 1;
            if ((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN) || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE))
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER;
            }
        }

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
            || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE)
            || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER))
        {
            var_a0_2 = D_80034CA4;
        }
        else
        {
            var_a0_2 = D_80034E0C;
        }

        if (gunSample1PTransform(var_a0_2, sp88, &handptr->field_8EC, hand) != 0)
        {
            handptr->field_92C = 1;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if ((handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_RECOVER)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_RECOVER))
    {
        temp_v1_9 = WEAPON_1P_ANIM_TIME(handptr->field_890);

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_STRIKE) || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_RECOVER))
        {
            if (g_CurrentPlayer->cur_item_weapon_getname == ITEM_SNIPERRIFLE)
            {
                sp74 = sniperMeleeKeyframes1;
            }
            else
            {
                sp74 = fistMeleeKeyframes1;
            }

            if ((handptr->weapon_action_state != GUN_ANIM_STATE_PUNCH1_RECOVER) && (handptr->field_890 >= WHEN_1E_FLD890))
            {
                handptr->weapon_firing_status = 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH1_RECOVER;
            }
        }
        else if ((handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_STRIKE) || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_RECOVER))
        {
            if (g_CurrentPlayer->cur_item_weapon_getname == ITEM_SNIPERRIFLE)
            {
                sp74 = sniperMeleeKeyframes2;
            }
            else
            {
                sp74 = fistMeleeKeyframes2;
            }

            if ((handptr->weapon_action_state != GUN_ANIM_STATE_PUNCH2_RECOVER) && (handptr->field_890 >= WHEN_1E_FLD890))
            {
                handptr->weapon_firing_status = 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH2_RECOVER;
            }
        }

        if (gunSample1PTransform(sp74, temp_v1_9, &handptr->field_8EC, hand) != 0)
        {
            handptr->field_92C = 1;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_GRENADE_THROW)
    {
        if (handptr->weapon_ammo_in_magazine > 0)
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
            if (gunSample1PTransform(grenadeThrowKeyframes, tempf, &handptr->field_8EC, hand) != 0)
            {
                handptr->field_92C = 1;
            }
            else
            {
                handptr->field_87E = 0;
                handptr->weapon_firing_status = 1;
                handptr->weapon_ammo_in_magazine -= 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_GRENADE_RECOVER;
                handptr->field_890 = 0.0f;
                handptr->field_88C = 0;
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_GRENADE_RECOVER)
    {
        tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
        if (gunSample1PTransform(timedMineThrowKeyframes, tempf, &handptr->field_8EC, hand) != 0)
        {
            handptr->field_92C = 1;
        }
        else
        {
            handptr->field_87E = 1;
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_THROWKNIFE_DRAW)
    {
        if (handptr->weapon_ammo_in_magazine > 0)
        {
            if (handptr->field_888 != 0)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_THROW;
            }
            else
            {
                tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
                if (gunSample1PTransform(throwKnifeDrawBackKeyframes, tempf, &handptr->field_8EC, hand) != 0)
                {
                    handptr->field_92C = 1;
                }
                else if (gunSample1PTransform(throwKnifeReleaseKeyframes, 0.0f, &handptr->field_8EC, hand) != 0)
                {
                    handptr->field_92C = 1;
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_THROW;
                }
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_THROWKNIFE_THROW)
    {
        if (handptr->weapon_ammo_in_magazine > 0)
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
            if (gunSample1PTransform(throwKnifeDrawBackKeyframes, tempf, &handptr->field_8EC, hand) != 0)
            {
                handptr->field_92C = 1;
            }
            else
            {
                handptr->field_87E = 0;
                handptr->weapon_firing_status = 1;
                handptr->weapon_ammo_in_magazine -= 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_RECOVER;
                handptr->field_890 = 0.0f;
                handptr->field_88C = 0;
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_THROWKNIFE_RECOVER)
    {
        tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
        if (gunSample1PTransform(throwKnifeReleaseKeyframes, tempf, &handptr->field_8EC, hand) != 0)
        {
            handptr->field_92C = 1;
        }
        else
        {
            handptr->field_87E = 1;
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_MINE_PLACE)
    {
        if ((handptr->weapon_ammo_in_magazine > 0) || (bondwalkItemCheckBitflags(var_s1, WEAPONSTATBITFLAG_CLICKY) != 0))
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
            
            if (gunSample1PTransform(proxMineThrowKeyframes, tempf, &handptr->field_8EC, hand) != 0)
            {
                handptr->field_92C = 1;
            }
            else
            {
                handptr->field_87E = 0;
                handptr->weapon_firing_status = 1;
                handptr->weapon_ammo_in_magazine -= 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_MINE_RECOVER;
                handptr->field_890 = 0.0f;
                handptr->field_88C = 0;
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_MINE_RECOVER)
    {
        tempf = WEAPON_1P_ANIM_TIME(handptr->field_890);
        if (gunSample1PTransform(remoteMineThrowKeyframes, tempf, &handptr->field_8EC, hand) != 0)
        {
            handptr->field_92C = 1;
        }
        else
        {
            handptr->field_87E = 1;
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_USE_ITEM)
    {
        if (var_s1 == ITEM_KEYANALYSERCASE)
        {
            if (handptr->field_88C == 0)
            {
                analyzeGEKey();
            }
        }
        else if (var_s1 == ITEM_WEAPONCASE)
        {
            if (handptr->field_88C == 0)
            {
                give_weapon_case_items();
            }
        }
        else if ((var_s1 == ITEM_BOMBDEFUSER)
            || (var_s1 == ITEM_DATATHIEF)
            || (var_s1 == ITEM_DOORDECODER)
            || (var_s1 == ITEM_EXPLOSIVEFLOPPY)
            || (var_s1 == ITEM_DATTAPE))
        {
            if (handptr->field_88C == 0)
            {
                temp_v0_8 = propFindForInteract();

                if (temp_v0_8 != NULL)
                {
                    temp_v0_8->obj->state |= 0x40;
                }
            }
        }
        else if ((var_s1 != ITEM_POLARIZEDGLASSES)
            && (var_s1 != ITEM_DARKGLASSES)
            && (var_s1 != ITEM_WATCHGEIGERCOUNTER)
            && (var_s1 != ITEM_WATCHMAGNETREPEL)
            && (var_s1 != ITEM_KEYCARD)
            && (var_s1 != ITEM_KEYYALE)
            && (var_s1 != ITEM_KEYBOLT)
            && (var_s1 != ITEM_SAFECRACKERCASE)
            && (var_s1 != ITEM_LOCKEXPLODER)
            && (var_s1 != ITEM_DOOREXPLODER)
            && (var_s1 != ITEM_CREDITCARD)
            && (var_s1 != ITEM_GASKEYRING)
            && (var_s1 == ITEM_BUNGEE))
        {
            // removed
        }
        else if (var_s1 == ITEM_PITONGUN
            || var_s1 == ITEM_GASKEYRING
            || var_s1 == ITEM_BUNGEE)
        {
            // removed
        }

        if (handptr->field_888 != 0)
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->field_890 = 0.0f;
            handptr->field_88C = 0;
        }
    }
}

#undef WEAPON_1P_ANIM_TIME
#undef WHEN_1_CASE_GRENADELAUNCH_FLD890
#undef WHEN_1_CASE_GRENADE_FLD890
#undef WHEN_D_FLD890
#undef WHEN_5_SP188_INIT
#undef WHEN_5_SP188_MULTI
#undef WHEN_5_FLD8B0_SP
#undef WHEN_5_FLD8B0_MULTI
#undef WHEN_8_SP178_INIT
#undef WHEN_8_SP178_MULTI
#undef WHEN_A_FLD890
#undef WHEN_A_FLD8B0
#undef WHEN_C_FLD890
#undef WHEN_E_FLD890
#undef WHEN_10_FLD890
#undef WHEN_11_FLD890_1
#undef WHEN_11_FLD890_2
#undef WHEN_1E_FLD890


void analyzeGEKey(void)
{
    if (bondinvHasGEKey())
    {
   	    HUDMESSAGEBOTTOM(langGet(getStringID(LGUN, GUN_STR_D8_ANALYZINGTHEGOLDENEYEKEY_LF))); //Analyzing the GoldenEye key...
    	g_CurrentPlayer->copiedgoldeneye = TRUE;
    	sndPlaySfx(g_musicSfxBufferPtr, KEY_ANALYSER_SFX, 0x0);
    	currentPlayerEquipWeaponWrapper(GUNRIGHT, ITEM_GOLDENEYEKEY);
    	currentPlayerEquipWeaponWrapper(GUNLEFT, ITEM_UNARMED);
  	}
  	else
  	{
	    HUDMESSAGEBOTTOM(langGet(getStringID(LGUN, GUN_STR_D9_YOUDONOTHAVETHEGOLDENEYEKEY_LF))); //You do not have the GoldenEye key.
	    sub_GAME_7F05D690();
  	}
  	return;
}


s32 get_keyanalyzer_flag(void)
{
  return g_CurrentPlayer->copiedgoldeneye;
}


void give_weapon_case_items(void)
{
  add_ammo_to_inventory(AMMO_KNIFE, 2, 0, 1);
  add_ammo_to_inventory(AMMO_GRENADE, 2, 0, 1);
  bondinvAddInvItem(ITEM_SNIPERRIFLE);
  set_sound_effect_for_weapontype_collection(ITEM_SNIPERRIFLE);
  display_text_for_weapon_in_lower_left_corner(ITEM_SNIPERRIFLE);
  give_cur_player_ammo(sniperrifle_stats.AmmoType, check_cur_player_ammo_amount_in_inventory(sniperrifle_stats.AmmoType) + sniperrifle_stats.MagSize);
  bondinvRemoveItemByID(ITEM_WEAPONCASE);
  currentPlayerEquipWeaponWrapper(GUNRIGHT,ITEM_SNIPERRIFLE);
  currentPlayerEquipWeaponWrapper(GUNLEFT,ITEM_UNARMED);
}


f32 get_vertical_position_solo_watch_menu_main_page_for_item(ITEM_IDS item)
{
  return gitem_structs[item].watch_pos_x;
}


f32 get_lateral_position_solo_watch_menu_main_page_for_item(ITEM_IDS item)
{
  return gitem_structs[item].watch_pos_y;
}


f32 get_depth_on_solo_watch_menu_page_for_item(ITEM_IDS item)
{
  return gitem_structs[item].watch_pos_z;
}


f32 get_xrotation_solo_watch_menu_for_item(ITEM_IDS item)

{
  return gitem_structs[item].x_rotation;
}


f32 get_yrotation_solo_watch_menu_for_item(ITEM_IDS item)
{
  return gitem_structs[item].y_rotation;
}


f32 get_45_degree_angle(s32 unk) {
  return 45.0f;
}


u16 *get_ptr_first_title_line_item(ITEM_IDS item)
{
  return langGet(gitem_structs[item].upper_watch_text);
}


u16 *get_ptr_second_title_line_item(ITEM_IDS item)
{
    return langGet(gitem_structs[item].lower_watch_text);
}


u16 *get_ptr_short_watch_text_for_item(ITEM_IDS item)
{
    return langGet(gitem_structs[item].watch_equipment_text);
}


u16 *get_ptr_long_watch_text_for_item(ITEM_IDS item)
{
    return langGet(gitem_structs[item].weapon_of_choice_text);
}


f32 get_45_degree_angle_0(s32 unk)
{
	return 45.0f;
}


f32 get_horizontal_offset_on_solo_watch_menu_for_item(ITEM_IDS item)
{
  return gitem_structs[item].equip_watch_x;
}


f32 get_vertical_offset_on_solo_watch_menu_for_item(ITEM_IDS item)
{
  return gitem_structs[item].equip_watch_y;
}


f32 get_depth_offset_solo_watch_menu_inventory_page_for_item(ITEM_IDS item)
{
  return gitem_structs[item].equip_watch_z;
}


f32 getCurrentPlayerNoise(GUNHAND hand)
{
    return g_CurrentPlayer->hands[hand].noise;
}


void gunTickNoise(void)
{
    enum ITEM_IDS weapon_id_right;
    enum ITEM_IDS weapon_id_left;
    s32 unused2;
    f32 noise_reduction;
    WeaponStats *item_right_stats;
    WeaponStats *item_left_stats;
    f32 noise_reduction_max;
    s32 unused;

    weapon_id_right = getCurrentPlayerWeaponId(GUNRIGHT);
    weapon_id_left = getCurrentPlayerWeaponId(GUNLEFT);
    item_right_stats = get_ptr_item_statistics(weapon_id_right);
    item_left_stats = get_ptr_item_statistics(weapon_id_left);

    if (weapon_id_right != ITEM_UNARMED && get_hands_firing_status(GUNRIGHT))
    {
        g_CurrentPlayer->hands[GUNRIGHT].noise += item_right_stats->NoiseIncreasePerShot;

        if (item_right_stats->LoudnessMax < g_CurrentPlayer->hands[GUNRIGHT].noise)
        {
            g_CurrentPlayer->hands[GUNRIGHT].noise = item_right_stats->LoudnessMax;
        }
    }

    if (weapon_id_left != ITEM_UNARMED && get_hands_firing_status(GUNLEFT))
    {
        g_CurrentPlayer->hands[GUNLEFT].noise += item_left_stats->NoiseIncreasePerShot;

        if (item_left_stats->LoudnessMax < g_CurrentPlayer->hands[GUNLEFT].noise)
        {
            g_CurrentPlayer->hands[GUNLEFT].noise = item_left_stats->LoudnessMax;
        }
    }

    noise_reduction = (item_right_stats->NoiseIncreasePerShot * g_GlobalTimerDelta) / (item_right_stats->NoiseDecayLinearTime * 60.0f);
    noise_reduction_max = ((g_CurrentPlayer->hands[GUNRIGHT].noise - item_right_stats->LoudnessMin) * g_GlobalTimerDelta) / (item_right_stats->NoiseDecayScaledTime * 60.0f);

    if (noise_reduction < noise_reduction_max)
    {
        noise_reduction = noise_reduction_max;
    }

    g_CurrentPlayer->hands[GUNRIGHT].noise -= noise_reduction;

    if (g_CurrentPlayer->hands[GUNRIGHT].noise < item_right_stats->LoudnessMin)
    {
        g_CurrentPlayer->hands[GUNRIGHT].noise = item_right_stats->LoudnessMin;
    }

    noise_reduction = (item_left_stats->NoiseIncreasePerShot * g_GlobalTimerDelta) / (item_left_stats->NoiseDecayLinearTime * 60.0f);
    noise_reduction_max = ((g_CurrentPlayer->hands[GUNLEFT].noise - item_left_stats->LoudnessMin) * g_GlobalTimerDelta) / (item_left_stats->NoiseDecayScaledTime * 60.0f);

    if (noise_reduction < noise_reduction_max)
    {
        noise_reduction = noise_reduction_max;
    }

    g_CurrentPlayer->hands[GUNLEFT].noise -= noise_reduction;

    if (g_CurrentPlayer->hands[GUNLEFT].noise < item_left_stats->LoudnessMin)
    {
        g_CurrentPlayer->hands[GUNLEFT].noise = item_left_stats->LoudnessMin;
    }
}


/**
 * Returns true if the hand has a melee weapon or has ammo in the magazine.
 */
s32 gunCanUseWeapon(enum GUNHAND hand)
{
    return (get_ammo_type_for_weapon(getCurrentPlayerWeaponId(hand)) == 0)
        || (g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine > 0);
}


/**
 * US address 7F067420.
 * Perfect Dark method bgunTickGameplay.
 *
 * Handles logic for single gun and dual wield trigger presses.
 * Calls updates to first person gun animations, gun model loading, noise to AI, and updating color from collision tiles.
 * Also handles the Watch Magnet Attract hum noise.
*/
void gunTickGameplay(s32 triggerOn)
{
    struct gun_trigger_state trigger_state;
    enum ITEM_IDS weapon_id_right;
    enum ITEM_IDS weapon_id_left;
    enum GUNHAND hand = GUNLEFT;
    struct rgba_u8 weapon_color;

    trigger_state = g_ZeroTriggerState;

    // Save previous trigger state.
    g_CurrentPlayer->prev_trigger_down = g_CurrentPlayer->trigger_down;

    // Save raw trigger state.
    g_CurrentPlayer->trigger_down = triggerOn;

    if ((g_CurrentPlayer->trigger_down == 0) && (g_CurrentPlayer->prev_trigger_down != 0))
    {
        g_CurrentPlayer->trigger_released = 1;
    }

    // Z button pressed this frame.
    if (g_CurrentPlayer->trigger_down != 0)
    {
        weapon_id_right = getCurrentPlayerWeaponId(GUNRIGHT);
        weapon_id_left = getCurrentPlayerWeaponId(GUNLEFT);

        g_CurrentPlayer->z_trigger_timer += g_ClockTimer;

        // Dual wielding.
        if ((weapon_id_right != ITEM_UNARMED) && (weapon_id_left != ITEM_UNARMED))
        {
            // Both guns prefer to take turns firing in dual wield.
            if ((bondwalkItemCheckBitflags(weapon_id_right, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0) && (bondwalkItemCheckBitflags(weapon_id_left, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0))
            {
                // Trigger has been held longer than 20 ticks on NTSC (24 on PAL).
                if (g_CurrentPlayer->z_trigger_timer > DUAL_WIELD_TRIGGER_SWAP_TICKS)
                {
                    // 'hand' still has its default value here, which behaves like trigger-on.
                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = hand;

                    // If the gun in the other hand is usable or has been held for any amount of time, depress its trigger as well.
                    if (gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) || g_CurrentPlayer->hands[1 - g_CurrentPlayer->current_trigger_hand].weapon_hold_time)
                    {
                        trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 1;
                    }
                }
                // Z has been held for less than or equal to 20 ticks on NTSC (24 on PAL).
                else
                {
                    if ((g_CurrentPlayer->prev_trigger_down == 0) &&
                        ((gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) != 0) || (gunCanUseWeapon(g_CurrentPlayer->current_trigger_hand) == 0)))
                    {
                        g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
                    }

                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
                    trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
                }
            }
            /**
             * One gun prefers to take turns firing in dual wield.
             * This doesn't happen much in the vanilla US version with the notable
             * exception of equipping Xenia's RC-P90 and Grenade Launcher in Jungle
            */
            else if ((bondwalkItemCheckBitflags(weapon_id_right, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0) || (bondwalkItemCheckBitflags(weapon_id_left, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0))
            {
                // Z has been held more than 30 ticks on NTSC (36 for PAL), depress trigger on both guns.
                if (g_CurrentPlayer->z_trigger_timer > DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS)
                {
                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = hand;

                    if ((gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) != 0) || g_CurrentPlayer->hands[1 - g_CurrentPlayer->current_trigger_hand].weapon_hold_time != 0)
                    {
                        trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 1;
                    }
                }
                // Before the hold threshold (30 ticks NTSC or 36 PAL), prefer the hand whose weapon uses alternating dual wield fire.
                else
                {
                    hand = bondwalkItemCheckBitflags(weapon_id_right, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) ? GUNRIGHT : GUNLEFT;

                    if (gunCanUseWeapon(hand) != 0 || g_CurrentPlayer->hands[hand].weapon_hold_time != 0)
                    {
                        g_CurrentPlayer->current_trigger_hand = hand;
                    }
                    else
                    {
                        if ((gunCanUseWeapon(1 - hand) != 0) || g_CurrentPlayer->hands[1 - hand].weapon_hold_time != 0)
                        {
                            g_CurrentPlayer->current_trigger_hand = 1 - hand;
                        }
                        else
                        {
                            g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
                        }
                    }

                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
                    trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
                }
            }
            /**
             * Neither weapon uses alternating dual wield fire.
             * Once the hold threshold is exceeded, allow the off-hand to become active too.
             */
            else if (g_CurrentPlayer->z_trigger_timer > DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS)
            {
                trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = hand;

                if (gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) || g_CurrentPlayer->hands[1 - g_CurrentPlayer->current_trigger_hand].weapon_hold_time)
                {
                    trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 1;
                }
            }
            /**
             * Neither weapon uses alternating dual wield fire.
             * On a fresh Z press, switch lead hands if the other hand is usable or the current hand cannot be used.
             * The lead hand continues being the lead hand.
             */
            else
            {
                if ((g_CurrentPlayer->prev_trigger_down == 0) &&
                    ((gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) != 0) || (gunCanUseWeapon(g_CurrentPlayer->current_trigger_hand) == 0)))
                {
                    g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
                }

                trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
                trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
            }
        }
        // Not dual wielding.
        else
        {
            if ((getCurrentPlayerWeaponId(g_CurrentPlayer->current_trigger_hand) == ITEM_UNARMED) && (getCurrentPlayerWeaponId(1 - g_CurrentPlayer->current_trigger_hand) != ITEM_UNARMED))
            {
                g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
            }

            trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
            trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
        }
    }
    // Z button not pressed. Reset the trigger timer.
    else
    {
        g_CurrentPlayer->z_trigger_timer = 0;
    }

    gunTickHandState(0, trigger_state.triggerOn[0]); // Right hand
    gunTickHandState(1, trigger_state.triggerOn[1]); // Left hand
    used_to_load_1st_person_model_on_demand(0);
    used_to_load_1st_person_model_on_demand(1);
    gunTickNoise();

    if (g_CurrentPlayer->resetshadecol)
    {
        set_color_shading_from_tile(getCurrentPlayerProp(), (struct rgba_u8 *) &g_CurrentPlayer->tileColor);
        g_CurrentPlayer->resetshadecol = FALSE;
    }
    else
    {
        set_color_shading_from_tile(getCurrentPlayerProp(), &weapon_color);
        update_color_shading(&g_CurrentPlayer->tileColor, &weapon_color);
    }

    bondinvIncrementHeldTime(getCurrentPlayerWeaponId(GUNRIGHT), getCurrentPlayerWeaponId(GUNLEFT));

    if(1);

    if (g_CurrentPlayer->magnetattracttime >= 0)
    {
        struct hand *hand_right = &g_CurrentPlayer->hands[0];

        g_CurrentPlayer->magnetattracttime += g_ClockTimer;

        if (g_CurrentPlayer->magnetattracttime < WATCH_SOUND_DURATION_TICKS)
        {
            // Start or restart the hum sound if needed
            if (hand_right->audioHandle == NULL
                || sndGetPlayingState((struct ALSoundState *) hand_right->audioHandle) == 0)
            {
                if (lvlGetControlsLockedFlag() == 0)
                {
                    sndPlaySfx(
                        (struct ALBankAlt_s *) g_musicSfxBufferPtr,
                        MAGNETIC_HUM_SFX,
                        (struct ALSoundState *) &hand_right->audioHandle);
                }
            }
        }
        else
        {
            g_CurrentPlayer->magnetattracttime = -1;

            if (hand_right->audioHandle != NULL)
            {
                if (sndGetPlayingState((struct ALSoundState *) hand_right->audioHandle) != 0)
                {
                    sndDeactivate((struct ALSoundState *) hand_right->audioHandle);
                }
            }
        }
    }
}


void gunSetAimType(s32 param_1)
{
  g_CurrentPlayer->aimtype = param_1;
}


void sub_GAME_7F067AB4(coord3d *param_1)
{
  g_CurrentPlayer->hands[GUNRIGHT].field_A38 = sub_GAME_7F05DCB8(GUNRIGHT) + param_1->x;
  g_CurrentPlayer->hands[GUNRIGHT].field_A3C = param_1->y;
  g_CurrentPlayer->hands[GUNRIGHT].field_A40 = param_1->z;

  g_CurrentPlayer->hands[GUNLEFT].field_A38 = sub_GAME_7F05DCB8(GUNLEFT) + param_1->x;
  g_CurrentPlayer->hands[GUNLEFT].field_A3C = param_1->y;
  g_CurrentPlayer->hands[GUNLEFT].field_A40 = param_1->z;

}


void gunSetTracerTarget(coord3d* pos)
{
    g_CurrentPlayer->hands[GUNLEFT].item_related.x = g_CurrentPlayer->hands[GUNRIGHT].item_related.x = pos->x;
    g_CurrentPlayer->hands[GUNLEFT].item_related.y = g_CurrentPlayer->hands[GUNRIGHT].item_related.y = pos->y;
    g_CurrentPlayer->hands[GUNLEFT].item_related.z = g_CurrentPlayer->hands[GUNRIGHT].item_related.z = pos->z;
}


void caclulate_gun_crosshair_position_rotation(f32 turn_x, f32 turn_y, f32 guncrossdamp, f32 gunaimdamp)
{
    s32 i;
    f32 screen_width;
    f32 screen_height;
    coord3d coords;

    screen_width = getPlayer_c_screenwidth();
    screen_height = getPlayer_c_screenheight();

    if (guncrossdamp != g_CurrentPlayer->guncrossdamp)
    {
        g_CurrentPlayer->crosshair_x_pos = (g_CurrentPlayer->crosshair_x_pos * (1.0f - g_CurrentPlayer->guncrossdamp)) / (1.0f - guncrossdamp);
        g_CurrentPlayer->crosshair_y_pos = (g_CurrentPlayer->crosshair_y_pos * (1.0f - g_CurrentPlayer->guncrossdamp)) / (1.0f - guncrossdamp);
        g_CurrentPlayer->guncrossdamp = guncrossdamp;
    }

    if (gunaimdamp != g_CurrentPlayer->gunaimdamp)
    {
        g_CurrentPlayer->gun_azimuth_angle = (g_CurrentPlayer->gun_azimuth_angle * (1.0f - g_CurrentPlayer->gunaimdamp)) / (1.0f - gunaimdamp);
        g_CurrentPlayer->gun_azimuth_turning = (g_CurrentPlayer->gun_azimuth_turning * (1.0f - g_CurrentPlayer->gunaimdamp)) / (1.0f - gunaimdamp);
        g_CurrentPlayer->gunaimdamp = gunaimdamp;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->crosshair_x_pos = (g_CurrentPlayer->crosshair_x_pos * guncrossdamp) + turn_x;
        g_CurrentPlayer->crosshair_y_pos = (g_CurrentPlayer->crosshair_y_pos * guncrossdamp) + turn_y;
    }

    g_CurrentPlayer->crosshair_angle.f[0] = (g_CurrentPlayer->crosshair_x_pos * (1.0f - guncrossdamp) * screen_width * 0.5f) + (screen_width * 0.5f);
    g_CurrentPlayer->crosshair_angle.f[1] = (g_CurrentPlayer->crosshair_y_pos * (1.0f - guncrossdamp) * screen_height * 0.5f) + (screen_height * 0.5f);

    if (g_CurrentPlayer->crosshair_angle.f[0] < 3.0f)
    {
        g_CurrentPlayer->crosshair_angle.f[0] = 3.0f;
    }
    else if ((screen_width - 4.0f) < g_CurrentPlayer->crosshair_angle.f[0])
    {
        g_CurrentPlayer->crosshair_angle.f[0] = screen_width - 4.0f;
    }

    if (g_CurrentPlayer->crosshair_angle.f[1] < 3.0f)
    {
        g_CurrentPlayer->crosshair_angle.f[1] = 3.0f;
    }
    else if ((screen_height - 4.0f) < g_CurrentPlayer->crosshair_angle.f[1])
    {
        g_CurrentPlayer->crosshair_angle.f[1] = (screen_height - 4.0f);
    }

    g_CurrentPlayer->crosshair_angle.f[0] += getPlayer_c_screenleft();
    g_CurrentPlayer->crosshair_angle.f[1] += getPlayer_c_screentop();

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->gun_azimuth_angle = (g_CurrentPlayer->gun_azimuth_angle * gunaimdamp) + turn_x;
        g_CurrentPlayer->gun_azimuth_turning = (g_CurrentPlayer->gun_azimuth_turning * gunaimdamp) + turn_y;
    }

    g_CurrentPlayer->field_FFC.x = (g_CurrentPlayer->gun_azimuth_angle * (1.0f - gunaimdamp) * screen_width * 0.5f) + (screen_width * 0.5f);
    g_CurrentPlayer->field_FFC.y = (g_CurrentPlayer->gun_azimuth_turning * (1.0f - gunaimdamp) * screen_height * 0.5f) + (screen_height * 0.5f);

    g_CurrentPlayer->field_FFC.x += getPlayer_c_screenleft();
    g_CurrentPlayer->field_FFC.y += getPlayer_c_screentop();

    transformAndNormalizeByLength2Dto3D(&g_CurrentPlayer->field_FFC, &coords, 1000.0f);
    sub_GAME_7F067AB4(&coords);
}


void sub_GAME_7F067F58(f32 turn_x, f32 turn_y, f32 max_aim_lock_speed)
{
    f32 aim_lock_speed;

#if defined(VERSION_US) || defined(VERSION_JP)
    aim_lock_speed = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT))->AimLockSpeed;
#elif defined(VERSION_EU)
    aim_lock_speed = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT))->CrosshairSpeed;
#endif

    if (aim_lock_speed < max_aim_lock_speed)
    {
        aim_lock_speed = max_aim_lock_speed;
    }

    caclulate_gun_crosshair_position_rotation(turn_x, turn_y, max_aim_lock_speed, aim_lock_speed);
}


void sub_GAME_7F067FBC(f32 turn_x, f32 turn_y)
{
    WeaponStats * item_stats;
    f32 guncrossdamp;
    f32 gunaimdamp;

    item_stats = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT));

#if defined(VERSION_US)
    guncrossdamp = item_stats->CrosshairSpeed;
    gunaimdamp = item_stats->AimLockSpeed;
#elif defined(VERSION_EU)
    guncrossdamp = 0.7651f;
    gunaimdamp = item_stats->CrosshairSpeed;
#elif defined(VERSION_JP)
    guncrossdamp = 0.8f;
    gunaimdamp = item_stats->AimLockSpeed;
#endif

    caclulate_gun_crosshair_position_rotation(turn_x, turn_y, guncrossdamp, gunaimdamp);
}


/*
* Address: 0x7f068008
*/
void get_bullet_angle(f32* horizontal_angle, f32* vertical_angle) {
	*horizontal_angle = g_CurrentPlayer->crosshair_angle.f[0];
	*vertical_angle = g_CurrentPlayer->crosshair_angle.f[1];
}


void sub_GAME_7F06802C(void)
{
    coord3d coord;
    f32 tmp;

    tmp = getPlayer_c_screenleft() + (getPlayer_c_screenwidth() * 0.5f);
    g_CurrentPlayer->crosshair_angle.x = tmp;
    g_CurrentPlayer->field_FFC.x = tmp;

    tmp = getPlayer_c_screentop() + (getPlayer_c_screenheight() * 0.5f);
    g_CurrentPlayer->crosshair_angle.y = tmp;
    g_CurrentPlayer->field_FFC.y = tmp;

    transformAndNormalizeByLength2Dto3D((coord2d *) &g_CurrentPlayer->field_FFC, &coord, 1000.0f);
    sub_GAME_7F067AB4(&coord);
}


void sub_GAME_7F0680D4(coord3d * coord)
{
    coord3d tmp;

    g_CurrentPlayer->field_1010.x = coord->x;
    g_CurrentPlayer->field_1010.y = coord->y;
    g_CurrentPlayer->field_1010.z = coord->z;
    matrix_4x4_set_rotation_around_xyz(coord->f, &g_CurrentPlayer->field_101C);

    tmp.x = g_CurrentPlayer->field_101C.m[2][0] * 1000.0f;
    tmp.y = g_CurrentPlayer->field_101C.m[2][1] * 1000.0f;
    tmp.z = g_CurrentPlayer->field_101C.m[2][2] * 1000.0f;
    transform3Dto2DCoords(&tmp, (coord3d* ) &g_CurrentPlayer->crosshair_angle);

    g_CurrentPlayer->field_FFC.x = g_CurrentPlayer->crosshair_angle.x;
    g_CurrentPlayer->field_FFC.y = g_CurrentPlayer->crosshair_angle.y;

    sub_GAME_7F067AB4(&tmp);
}


/**
 * Address 0x7F068190.
*/
void sub_GAME_7F068190(coord3d *zeropos, coord3d *vec)
{
    zeropos->x = 0.0f;
    zeropos->y = 0.0f;
    zeropos->z = 0.0f;

    transformAndNormalizeByLength2Dto3D(&g_CurrentPlayer->crosshair_angle, vec, 1.0f);
}


/*
* Address: 0x7f0681cc
* This function computes the angle the player's bullets are fired at
*/
void bullet_path_from_screen_center(coord3d* arg0, coord3d* result, enum GUNHAND arg2)
{
    coord2d crosspos;
    s32 unused;
    f32 inaccuracy;
    f32 scaledspread;
    f32 randfactor;

    inaccuracy = get_ptr_item_statistics(getCurrentPlayerWeaponId(arg2))->Inaccuracy;
    if ((bondwalkItemCheckBitflags(get_item_in_hand_or_watch_menu(arg2), WEAPONSTATBITFLAG_FIRST_SHOT_ACCURACY) != 0) && (g_CurrentPlayer->hands[arg2].volley == 1))
    {
        // Single shots are four times more accurate
        inaccuracy *= 0.25f;
    }

    scaledspread = (120.0f * inaccuracy) / viGetFovY();

    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.x = g_CurrentPlayer->crosshair_angle.f[0] + randfactor * scaledspread * getPlayer_c_screenwidth() * (PAL ? ASPECT_RATIO_PAL : ASPECT_RATIO_SD)
        / (getPlayer_c_perspaspect() * 320.0f);

    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.y =  g_CurrentPlayer->crosshair_angle.f[1] + randfactor * scaledspread * getPlayer_c_screenheight()
        / (PAL ? (f32)(SCREEN_HEIGHT_272) : (f32)(SCREEN_HEIGHT_240));

    arg0->x = 0.0f;
    arg0->y = 0.0f;
    arg0->z = 0.0f;

    // Result is a normalized vector describing the path the bullet will follow
    // Can be used to compute x,y,z displacement off the center of the screen if done for a projectile
    transformAndNormalizeByLength2Dto3D(&crosspos, result, 1.0f);
}


/*
* Address: 0x7f068420
*/
CasingRecord* casingCreate(ModelFileHeader* header, Mtxf* mtx)
{
    CasingRecord* entry = g_Casings;
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);

    while (entry < end && entry->header != NULL)
    {
        entry++;
    }

    if (entry < end)
    {
        entry->header = header;

        entry->pos.x = mtx->m[3][0];
        entry->pos.y = mtx->m[3][1];
        entry->pos.z = mtx->m[3][2];
#if VERSION_EU
        matrix_7f05842c_eu(mtx, entry->rot_mtx);
#else
        entry->rot_mtx.m[0][0] = mtx->m[0][0];
        entry->rot_mtx.m[0][1] = mtx->m[0][1];
        entry->rot_mtx.m[0][2] = mtx->m[0][2];
        entry->rot_mtx.m[0][3] = 0.0f;

        entry->rot_mtx.m[1][0] = mtx->m[1][0];
        entry->rot_mtx.m[1][1] = mtx->m[1][1];
        entry->rot_mtx.m[1][2] = mtx->m[1][2];
        entry->rot_mtx.m[1][3] = 0.0f;

        entry->rot_mtx.m[2][0] = mtx->m[2][0];
        entry->rot_mtx.m[2][1] = mtx->m[2][1];
        entry->rot_mtx.m[2][2] = mtx->m[2][2];
        entry->rot_mtx.m[2][3] = 0.0f;

        entry->rot_mtx.m[3][0] = 0.0f;
        entry->rot_mtx.m[3][1] = 0.0f;
        entry->rot_mtx.m[3][2] = 0.0f;
        entry->rot_mtx.m[3][3] = 1.0f;
#endif
        return entry;
    }

    return NULL;
}


#if VERSION_EU
#define THROWMTX_OFFSET      0xAD0
#define THROWPOS_OFFSET      0xB00
#define THROWPOS_PREV_OFFSET 0xB40
#else
#define THROWMTX_OFFSET      0xAD8
#define THROWPOS_OFFSET      0xB08
#define THROWPOS_PREV_OFFSET 0xB48
#endif
 
#define THROWMTX     ((Mtxf *) ((u8 *) g_CurrentPlayer + handoffset + THROWMTX_OFFSET))
#define THROWPOS(k)  (((f32 *) ((u8 *) g_CurrentPlayer + handoffset + THROWPOS_OFFSET))[k])
#define THROWPREV(k) (((f32 *) ((u8 *) g_CurrentPlayer + handoffset + THROWPOS_PREV_OFFSET))[k])
 
extern f32 D_800543B4;
extern f32 D_800543B8;
extern f32 D_800543BC;
extern f32 D_800543C0;
extern f32 D_800543C4;
extern f32 D_800543C8;
extern f32 D_800543CC;
extern f32 D_800543D0;
extern f32 D_800543D4;
extern f32 expended_shell_initial_gravity_modifier_pistol;
extern f32 D_800543DC;
extern f32 D_800543E0;
extern f32 D_800543E4;
extern f32 D_800543E8;
extern f32 D_800543EC;
extern f32 D_800543F0;
extern f32 D_800543F4;
extern f32 D_800543F8;
extern f32 D_800543FC;
extern f32 expended_shell_initial_gravity_modifier_non_pistol;
 
#ifndef VERSION_EU
GLOBAL_ASM(
.late_rodata
glabel D_800543B4
.word 0x3dccccce /*0.10000001*/
glabel D_800543B8
.word 0x3f088888 /*0.5333333*/
glabel D_800543BC
.word 0x40c90fdb /*6.2831855*/
glabel D_800543C0
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543C4
.word 0x40c90fdb /*6.2831855*/
glabel D_800543C8
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543CC
.word 0x40c90fdb /*6.2831855*/
glabel D_800543D0
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543D4
.word 0x493d6c30 /*775875.0*/
glabel expended_shell_initial_gravity_modifier_pistol
.word 0x3e8e38e4 /*0.27777779*/
glabel D_800543DC
.word 0x3fb55555 /*1.4166666*/
glabel D_800543E0
.word 0x3fd55555 /*1.6666666*/
glabel D_800543E4
.word 0x40c90fdb /*6.2831855*/
glabel D_800543E8
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543EC
.word 0x40c90fdb /*6.2831855*/
glabel D_800543F0
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543F4
.word 0x40c90fdb /*6.2831855*/
glabel D_800543F8
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543FC
.word 0x493d6c30 /*775875.0*/
glabel expended_shell_initial_gravity_modifier_non_pistol
.word 0x3e8e38e4 /*0.27777779*/
)
#else
GLOBAL_ASM(
.late_rodata
glabel D_800543B4
.word 0x3dccccce /*0.10000001*/
glabel D_800543B8
.word 0x3f088888 /*0.5333333*/
glabel D_800543BC
.word 0x40c90fdb /*6.2831855*/
glabel D_800543C0
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543C4
.word 0x40c90fdb /*6.2831855*/
glabel D_800543C8
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543CC
.word 0x40c90fdb /*6.2831855*/
glabel D_800543D0
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543D4
.word 0x49634ea0 /* 931050.0 */
glabel expended_shell_initial_gravity_modifier_pistol
.word 0x3e8e38e4 /*0.27777779*/
glabel D_800543DC
.word 0x3fb55555 /*1.4166666*/
glabel D_800543E0
.word 0x3fd55555 /*1.6666666*/
glabel D_800543E4
.word 0x40c90fdb /*6.2831855*/
glabel D_800543E8
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543EC
.word 0x40c90fdb /*6.2831855*/
glabel D_800543F0
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543F4
.word 0x40c90fdb /*6.2831855*/
glabel D_800543F8
.word 0x3ec90fdb /*0.39269909*/
glabel D_800543FC
.word 0x49634ea0 /* 931050.0 */
glabel expended_shell_initial_gravity_modifier_non_pistol
.word 0x3e8e38e4 /*0.27777779*/
)
#endif
 
/**
 * Address: 7F068508
 * 
 * Ejects a spent cartridge casing from the gun in the given hand.
 */
void sub_GAME_7F068508(GUNHAND handnum, f32 floor_y_pos)
{
    CasingRecord *casing;
    Mtxf mtx;
    ITEM_IDS weaponid;
    coord3d *switchdata;
    ModelFileHeader *cartridge_header;
    coord3d switchpos;
    ModelNode *switch0;
    coord3d rot;
#if VERSION_EU
    Mtxf rotmtx;
#endif
    f32 rand;
    s32 new_var; /* dead but declared on EU — still reserves its frame slot */
    f32 frac;
#if VERSION_EU
    s32 randlimit;
#endif
    f32 oldvely;
    f32 newvely;
#ifndef VERSION_EU
    s32 randlimit;
#endif
    s32 handoffset;
    u32 randval;
#if VERSION_EU
    s32 pad[2];
#endif
 
    weaponid = getCurrentPlayerWeaponId(handnum);
    cartridge_header = get_ptr_item_statistics(weaponid)->ptr_cartridge_struct;
 
    // Do not create ejected casings in multiplayer.
    if ((cartridge_header == NULL) || (getPlayerCount() >= 2))
    {
        return;
    }
 
    handoffset = handnum * sizeof(struct hand);
    switch0 = g_CurrentPlayer->copy_of_body_obj_header[handnum].Switches[0];
 
    if (switch0 != NULL)
    {
        switchdata = (coord3d *) switch0->Data;
 
        switchpos.x = switchdata->x * D_800543B4;
        switchpos.y = switchdata->y * D_800543B4;
        switchpos.z = switchdata->z * D_800543B4;
 
        matrix_4x4_set_identity_and_position(&switchpos, &mtx);
        matrix_4x4_multiply_in_place(THROWMTX, &mtx);
    }
    else
    {
        matrix_4x4_copy(THROWMTX, &mtx);
    }
 
    casing = casingCreate(cartridge_header, &mtx);
 
    if (casing == NULL)
    {
        return;
    }
 
    rot = *((coord3d *) (&D_80035EA4));
    casing->floor_y_pos = floor_y_pos;
 
    if (((((weaponid == ITEM_WPPK) || (weaponid == ITEM_WPPKSIL)) || (weaponid == ITEM_TT33)) || (weaponid == ITEM_SILVERWPPK)) || (weaponid == ITEM_GOLDWPPK))
    {
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        newvely = 0.0625f;
        casing->vel.x = -(((rand * D_800543B8) * newvely) + D_800543B8);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.y = ((rand * 2.5f) * 0.0625f) + 2.5f;
        casing->vel.z = frac * 0.0f;
 
        mtx4RotateVecInPlace(THROWMTX, &casing->vel);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.x = (((rand + rand) * D_800543BC) * newvely) - D_800543C0;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.y = (((rand + rand) * D_800543C4) * newvely) - D_800543C8;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.z = (((rand + rand) * D_800543CC) * newvely) - D_800543D0;
 
#if VERSION_EU
        matrix_4x4_set_rotation_around_xyz(&rot, &rotmtx);
        matrix_7f05842c_eu(&rotmtx, casing->rot_velocity_mtx);
        if (handoffset);
#else
        matrix_4x4_set_rotation_around_xyz(&rot, &casing->rot_velocity_mtx);
#endif
 
#if VERSION_EU
        randlimit = ((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10;
        randlimit = randlimit + 0x158679;
        randval = randomGetNext();
        oldvely = casing->vel.y;
        frac = ((f32) ((u32) (randval % randlimit))) / D_800543D4;
#else
        randlimit = (((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10) + 0x158679;
        new_var = randlimit;
        randval = randomGetNext();
        oldvely = casing->vel.y;
        frac = ((f32) ((u32) (randval % new_var))) / D_800543D4;
#endif
        newvely = oldvely - (frac * expended_shell_initial_gravity_modifier_pistol);
 
        casing->vel.y = newvely;
        casing->pos.y += (frac * (oldvely + newvely)) * 0.5f;
        casing->pos.x += frac * casing->vel.x;
        casing->pos.z += frac * casing->vel.z;
 
        // Keep the 0 + 1 for matching.
        if (g_ClockTimer >= (0 + 1))
        {
            casing->vel.x += (THROWPOS(0) - THROWPREV(0)) / g_GlobalTimerDelta;
            casing->vel.y += (THROWPOS(1) - THROWPREV(1)) / g_GlobalTimerDelta;
            casing->vel.z += (THROWPOS(2) - THROWPREV(2)) / g_GlobalTimerDelta;
        }
    }
    else
    {
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.x = -(((rand * D_800543DC) * 0.125f) + D_800543DC);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.y = ((rand * D_800543E0) * 0.125f) + D_800543E0;
        casing->vel.z = 0.0f;
 
        mtx4RotateVecInPlace(THROWMTX, &casing->vel);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.x = (((rand + rand) * D_800543E4) * 0.0625f) - D_800543E8;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.y = (((rand + rand) * D_800543EC) * 0.0625f) - D_800543F0;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.z = (((rand + rand) * D_800543F4) * 0.0625f) - D_800543F8;
 
#if VERSION_EU
        matrix_4x4_set_rotation_around_xyz(&rot, &rotmtx);
        matrix_7f05842c_eu(&rotmtx, casing->rot_velocity_mtx);
        if (handoffset);
#else
        matrix_4x4_set_rotation_around_xyz(&rot, &casing->rot_velocity_mtx);
#endif
        randval = ((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10;
#if VERSION_EU
        randval = randval + 0x158679;
        randlimit = randomGetNext();
        oldvely = (&casing->vel)->y;
        frac = ((f32) ((u32) (randlimit % randval))) / D_800543FC;
#else
        randlimit = randval + 0x158679;
        randval = randomGetNext();
        oldvely = (&casing->vel)->y;
        frac = ((f32) ((u32) (randval % randlimit))) / D_800543FC;
#endif
        newvely = (casing->vel.y = oldvely - (frac * expended_shell_initial_gravity_modifier_non_pistol));
 
        casing->pos.y += (frac * (oldvely + newvely)) * 0.5f;
        casing->pos.x += frac * casing->vel.x;
        casing->pos.z += frac * casing->vel.z;
 
        if (g_ClockTimer > 0)
        {
            casing->vel.x += (THROWPOS(0) - THROWPREV(0)) / g_GlobalTimerDelta;
            casing->vel.y += (THROWPOS(1) - THROWPREV(1)) / g_GlobalTimerDelta;
            casing->vel.z += (THROWPOS(2) - THROWPREV(2)) / g_GlobalTimerDelta;
        }
    }
 
    if (handoffset);
}


extern f32 g_CasingGravity;
extern f32 g_CasingModelScale;
extern f32 g_CasingMinMatrixTranslation;
extern f32 g_CasingMaxMatrixTranslation;

GLOBAL_ASM(
.late_rodata
glabel g_CasingGravity
.word 0x3e8e38e4 /*0.27777779*/
glabel g_CasingModelScale
.word 0x3dccccce /*0.10000001*/
glabel g_CasingMinMatrixTranslation
.word 0xc6ea6000 /*-30000.0*/
glabel g_CasingMaxMatrixTranslation
.word 0x46ea6000 /*30000.0*/
)

void update_bullet_casing(CasingRecord* casing)
{
    f32 new_val_y;
    f32 delta;
    s32 i;
    struct player* current_player;

    delta = g_GlobalTimerDelta;
    new_val_y = casing->vel.y - (delta * g_CasingGravity);

    casing->pos.y += delta * 0.5f * (casing->vel.y + new_val_y);

    if (casing->pos.y < casing->floor_y_pos)
    {
#if defined(BUGFIX_R1)
        if (g_CasingSfxState == 0 && (g_ClockTimer > 0))
#else
        if (g_CasingSfxState == 0)
#endif
        {
            if ((g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_FIRE) && (g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_FIRE))
            {
                // Play bullet casing rolling on floor sound
                sndPlaySfx((struct ALBankAlt_s* ) g_musicSfxBufferPtr, CART_SPENT_SFX, (ALSoundState* ) &g_CasingSfxState);
            }
        }

        // This casing is removed and not updated anymore
        casing->header = NULL;
        return;
    }

    casing->vel.y = new_val_y;
    casing->pos.x += delta * casing->vel.x;
    casing->pos.z += delta * casing->vel.z;

    for (i = 0; i < g_ClockTimer; i++)
    {
#if defined(VERSION_US) || defined(VERSION_JP)
        matrix_4x4_multiply_homogeneous_in_place(&casing->rot_velocity_mtx, &casing->rot_mtx);
#else
        matrix_4x4_multiply_homogeneous_in_place_eu(casing->rot_velocity_mtx, casing->rot_mtx);
#endif
    }
}


void update_bullet_casings(void)
{
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);
    CasingRecord* entry = g_Casings;

    while (entry < end)
    {
        if (entry->header)
        {
            update_bullet_casing(entry);
        }

        entry++;
    }
}

typedef struct ModelHead {
    s16 unk00;
    s16 rwdatalen;
    void *chr;
    ModelFileHeader *obj;
    RenderPosView *render_pos;
    union ModelRwData **datas;
    f32 scale;
    Model *attachedto;
    ModelNode *attachedto_objinst;
} ModelHead;

void sub_GAME_7F068EC4(CasingRecord *casing, Gfx **gdl)
{
    Gfx             *savedgdl = *gdl;
    ModelFileHeader *model_header = casing->header;
    RenderPosView   *model_matrices = dynAllocate(model_header->numMatrices * sizeof(RenderPosView));
    ModelHead        model;
    ModelRenderData  render_data = *(ModelRenderData *)g_DefaultCasingModelRenderData;
    Mtxf             casing_model_mtx;
    s32              axis_offset;
    s32              matrix_translation_in_range = TRUE;
    f32              max_matrix_translation;
    f32              model_scale_or_min_translation;
    u8              *matrix_axis_ptr;

    modelCalculateRwDataLen(model_header);
    modelInit((Model *)&model, model_header, NULL);

    model.render_pos = model_matrices;

#if defined(VERSION_EU)
    matrix_4x4_copy_eu(casing->rot_mtx, casing_model_mtx.m);
#else
    matrix_4x4_copy(&casing->rot_mtx, &casing_model_mtx);
#endif

    model_scale_or_min_translation = g_CasingModelScale;
    matrix_scalar_multiply(model_scale_or_min_translation, &casing_model_mtx);

    matrix_4x4_set_position(&casing->pos, &casing_model_mtx);

    matrix_4x4_multiply_homogeneous(
        camGetWorldToScreenMtxf(),
        &casing_model_mtx,
        (Mtxf *)model.render_pos);

    model_scale_or_min_translation = g_CasingMinMatrixTranslation;
    max_matrix_translation         = g_CasingMaxMatrixTranslation;

    axis_offset     = 0;
    matrix_axis_ptr = (u8 *)model.render_pos;

    // Offset 0x30 is m[3][0]; advancing the pointer checks translation X, Y and Z.
    while (axis_offset != 12)
    {
        if (max_matrix_translation < *(f32 *)(matrix_axis_ptr + 0x30))
        {
            matrix_translation_in_range = FALSE;
        }
        else if (*(f32 *)(matrix_axis_ptr + 0x30) < model_scale_or_min_translation)
        {
            matrix_translation_in_range = FALSE;
        }

        axis_offset += 4;
        matrix_axis_ptr += 4;
    }

    if (matrix_translation_in_range)
    {
        render_data.zbufferenabled = 0;
        render_data.gdl            = savedgdl;
        render_data.mtxlist        = (Mtxf *)model_matrices;
        render_data.PropType       = PROP_TYPE_WEAPON;

        render_data.envcolour.word =
            ((g_CurrentPlayer->tileColor.a |
              (g_CurrentPlayer->tileColor.r << 24)) |
             (g_CurrentPlayer->tileColor.g << 16)) |
            (g_CurrentPlayer->tileColor.b << 8);

        subdraw(&render_data, (Model *)&model);

        *gdl = render_data.gdl;

        bondviewTransformManyPosToViewMatrix(model_matrices, model_header->numMatrices);
    }
}


void sub_GAME_7F06908C(Gfx **gdl)
{
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);
    CasingRecord* entry = g_Casings;

    while (entry < end)
    {
        if (entry->header)
        {
            sub_GAME_7F068EC4(entry, gdl);
        }
        
        entry++;
    }
}


void gunSetGunAmmoVisible(s32 reason, bool enable) {

	if (enable)
    {
		g_CurrentPlayer->gunammooff &= ~reason;
		return;
	}

	g_CurrentPlayer->gunammooff |= reason;
}



void give_cur_player_ammo(s32 ammo_type, s32 ammo_amount) {
    enum ITEM_IDS weapon_id;
    s32 max_ammo;

    weapon_id = getCurrentPlayerWeaponId(GUNRIGHT);
    if ((get_ammo_type_for_weapon(weapon_id) == ammo_type) && (bondwalkItemCheckBitflags(weapon_id, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0))
    {
        g_CurrentPlayer->hands[0].weapon_ammo_in_magazine += ammo_amount;
        if (get_ptr_item_statistics(weapon_id)->MagSize < g_CurrentPlayer->hands[0].weapon_ammo_in_magazine)
        {
            g_CurrentPlayer->hands[0].weapon_ammo_in_magazine = (s32) get_ptr_item_statistics(weapon_id)->MagSize;
        }
        g_CurrentPlayer->ammoheldarr[ammo_type] = 0;
        return;
    }

    max_ammo = ammo_related[ammo_type].MaxAmmo;
    if (max_ammo < ammo_amount)
    {
        g_CurrentPlayer->ammoheldarr[ammo_type] = max_ammo;
        return;
    }

    g_CurrentPlayer->ammoheldarr[ammo_type] = ammo_amount;
}




s32 check_cur_player_ammo_amount_in_inventory(AMMOTYPE ammotype) {
    return g_CurrentPlayer->ammoheldarr[ammotype];
}

s32 currentPlayerGetAmmoCount(AMMOTYPE ammotype) {

    s32 total_ammo = check_cur_player_ammo_amount_in_inventory(ammotype);

    if (get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNRIGHT)) == ammotype) {
        total_ammo += get_ammo_in_hands_magazine(GUNRIGHT);
    }

    if (get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNLEFT)) == ammotype) {
        total_ammo += get_ammo_in_hands_magazine(GUNLEFT);
    }

    return total_ammo;
}



s32 get_max_ammo_for_type(s32 arg0)
{
    return ammo_related[arg0].MaxAmmo;
}




void set_max_ammo_for_cur_player(void)
{
    s32 ammo_type;

    for (ammo_type = 0; ammo_type < AMMO_RELATED_MAX; ammo_type++)
    {
        give_cur_player_ammo(ammo_type, ammo_related[ammo_type].MaxAmmo);
    }
}



s32 get_ammo_in_hands_magazine(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine;
}



s32 get_ammo_in_hands_weapon(enum GUNHAND hand)
{
    s32 weapon_id;
    s32 ammo_count;

    weapon_id = getCurrentPlayerWeaponId(hand);
    ammo_count = get_ammo_count_for_weapon(weapon_id);

    if ((weapon_id == ITEM_SHOTGUN) || (weapon_id == ITEM_AUTOSHOT))
    {
        s32 other_weapon_id;
        other_weapon_id = getCurrentPlayerWeaponId(1 - hand);

        if ((other_weapon_id == ITEM_SHOTGUN) || (other_weapon_id == ITEM_AUTOSHOT))
        {
            return ammo_count - g_CurrentPlayer->hands[1 - hand].numvisibleshells;
        }

        /* I don't know why there's an extra return here, but it's needed to match */
        return ammo_count;
    }

    return ammo_count;
}



s32 get_ammo_type_for_weapon(ITEM_IDS weapon) {
    return get_ptr_item_statistics(weapon)->AmmoType;
}

s32 get_ammo_count_for_weapon(ITEM_IDS weapon) {
  WeaponStats *weaponstats = get_ptr_item_statistics(weapon);
  return g_CurrentPlayer->ammoheldarr[weaponstats->AmmoType];
}

void add_ammo_to_weapon(ITEM_IDS weapon, s32 ammo) {
    give_cur_player_ammo(get_ptr_item_statistics(weapon)->AmmoType, ammo);
}

s32 get_max_ammo_for_weapon(enum ITEM_IDS weapon)
{
    return ammo_related[get_ptr_item_statistics(weapon)->AmmoType].MaxAmmo;
}


Gfx *microcode_generation_ammo_related(Gfx *gdl, struct sImageTableEntry *tconfig, f32 x, f32 y, f32 arg4, s32 arg5, f32 arg6, s32 arg7, s32 red, s32 green, s32 blue, s32 alpha)
{
    f32 xy[2];
    f32 halfed[2];
 
    gDPSetColorDither(gdl++, G_CD_DISABLE);
    gDPSetTexturePersp(gdl++, G_TP_NONE);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetTextureFilter(gdl++, G_TF_POINT);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    xy[0] = ((u32)tconfig->width * 0.5f) - (f32)(tconfig->width / 2);

    if (arg5 != 0) 
    { 
        xy[0] = -xy[0]; 
    }

    xy[0] = xy[0] + x;
 
    if (0.0f <= y)
    {
        xy[1] = y - (((f32) ((u32) tconfig->height)) * 0.5f);
    }
    else
    {
        xy[1] = -((((f32)((u32)tconfig->height)) * 0.5f) - ((f32)(tconfig->height / 2)));
        xy[1] = arg4 + xy[1];
        xy[1] = xy[1] + arg6;
    }
 
    halfed[0] = ((f32) ((u32) tconfig->width)) * 0.5f;
    halfed[1] = ((f32) ((u32) tconfig->height)) * 0.5f;
 
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0, 0, 0, 0);
    gDPFillRectangle(gdl++, ((s32)(xy[0] - halfed[0])) - 1, ((s32)(xy[1] - halfed[1])) - 1, ((s32)(xy[0] + halfed[0])) + 1, ((s32)(xy[1] + halfed[1])) + 1);
 
    texSelect(&gdl, tconfig, (arg7 != 0) ? (2) : (1), 0, 0);
    display_image_at_position(&gdl, xy, halfed, tconfig->width, tconfig->height, 0, 0, 1, red, green, blue, alpha, 0 < tconfig->level, 0);
 
    gDPPipeSync(gdl++);
    gDPSetColorDither(gdl++, G_CD_BAYER);
    gDPSetTexturePersp(gdl++, G_TP_PERSP);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_LOD);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    return gdl;
}


/**
 * Address: TODO
 * WARNING: This function is missing a "return". This will cause bugs on other compilers.
 */
Gfx *set_rgba_redirect_generate_microcode(Gfx *gdl, sImageTableEntry *tconfig, f32 x, f32 y, f32 arg4, s32 arg5, f32 arg6, s32 arg7)
{
    microcode_generation_ammo_related(gdl, tconfig, x, y, arg4, arg5, arg6, arg7, 0xff, 0xff, 0xff, 0xff);
}


/**
 * Address: TODO
 */
Gfx *gunDrawHudString(Gfx *gdl, s8 *text, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    s32 x1;
    s32 y1;
    s32 x2;
    s32 y2;
    s32 textheight;
    s32 textwidth;

    x1 = 0;
    y1 = 0;
    x2 = 0;
    y2 = 0;
    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, text, ptrFontBankGothicChars, ptrFontBankGothic, 0);

    if (halign == HUDHALIGN_LEFT) { // left
		x2 = x + textwidth;
		x1 = x;
	} else if (halign == HUDHALIGN_RIGHT) { // right
		x1 = x - textwidth;
		x2 = x;
	} else if (halign == HUDHALIGN_MIDDLE) { // middle
		x2 = x + textwidth / 2;
		x1 = x2 - textwidth;
	}

    if (valign == HUDVALIGN_TOP) { // top
		y2 = y + textheight;
		y1 = y;
	} else if (valign == HUDVALIGN_BOTTOM) { // bottom
		y1 = y - textheight;
		y2 = y;
	} else if (valign == HUDVALIGN_MIDDLE) { // middle
		y2 = y + textheight / 2;
		y1 = y2 - textheight;
	}

    gdl = draw_blackbox_to_screen(gdl, &x1, &y1, &x2, &y2);

    if (outline) {
        gdl = textRenderOutlined(gdl, &x1, &y1, text, ptrFontBankGothicChars, ptrFontBankGothic, -1, 0x646464FF, (s32) viGetX(), viGetY(), 0, 0);
    } else {
        gdl = textRender(gdl, &x1, &y1, text, ptrFontBankGothicChars, ptrFontBankGothic, 0xFF00B0, (s32) viGetX(), viGetY(), 0, 0);
    }

    return gdl;
}


/**
 * Address: TODO
 */
Gfx *gunDrawHudInteger(Gfx *gdl, s32 value, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    char buffer[12];
    sprintf(buffer, "%d\n", value);
    return gunDrawHudString(gdl, buffer, x, halign, y, valign, outline);
}


/**
 * Draw magazine ammo number, ammo type icon, and total ammo number at the bottom right of the viewport.
 * Render an additional ammo counter at the bottom left of the viewport when dual wielding.
 */
Gfx *generate_ammo_total_microcode(Gfx *gdl)
{
    ITEM_IDS weapon_left;
    ITEM_IDS weapon_right;
    s32 ammotype;
    s32 leftx;
    s32 rightx;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset_r;
    s32 textwidth_r;
    u32 imageoffset_l;
    s32 textwidth_l;

    if (g_CurrentPlayer->gunammooff == 0)
    {
        if (g_CurrentPlayer->mpmenuon == 0)
        {
            weapon_left = getCurrentPlayerWeaponId(GUNLEFT);
            weapon_right = getCurrentPlayerWeaponId(GUNRIGHT);

            if (getPlayerCount() < 3)
            {
                leftx = 59;
                rightx = 59;
            }
            else if (get_cur_playernum() & 1)
            {
                leftx = 43;
                rightx = 127;
            }
            else
            {
                leftx = 59;
                rightx = 109;
            }

            if (weapon_right != ITEM_UNARMED)
            {
                ammotype = get_ammo_type_for_weapon(weapon_right);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_r = ammo_related[ammotype].IconImage;
                    textwidth_r = 5;

                    if (imageoffset_r != 0)
                    {
                        imageoffset_r += globalbank_rdram_offset;
                        gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset_r, (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) - (f32)rightx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 0,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 0,
#endif
                            ammo_related[ammotype].IconYOffset, 1);
                        textwidth_r = ((u8 *)imageoffset_r)[4];
                    }

                    gdl = microcode_constructor(gdl);

                    if (bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) - (textwidth_r / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || bondwalkItemCheckBitflags(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) + ((textwidth_r + 1) / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = combiner_bayer_lod_perspective(gdl);
                }
            }

            if (weapon_left != ITEM_UNARMED)
            {
                ammotype = get_ammo_type_for_weapon(weapon_left);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_l = ammo_related[ammotype].IconImage;
                    textwidth_l = 5;

                    if (imageoffset_l != 0)
                    {
                        imageoffset_l += globalbank_rdram_offset;
                        gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset_l, getPlayer_c_screenleft() + (f32)leftx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 1,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 1,
#endif
                            ammo_related[ammotype].IconYOffset, 1);
                        textwidth_l = ((u8 *)imageoffset_l)[4];
                    }

                    gdl = microcode_constructor(gdl);

                    if (bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, ((viGetViewLeft() + leftx) + (textwidth_l / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || bondwalkItemCheckBitflags(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, ((viGetViewLeft() + leftx) - ((textwidth_l + 1) / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = combiner_bayer_lod_perspective(gdl);
                }
            }
        }
    }

    return gdl;
}


/**
 * Address: 7F06A334
 */
Gfx *gunDrawWatchAmmoDisplay(Gfx *gdl)
{
    ITEM_IDS offhanditem;
    ITEM_IDS item;
    s32 ammotype;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset;
    s32 textwidth;
    s32 pad;

    offhanditem = getCurrentPlayerWeaponId(1);
    item = getCurrentPlayerWeaponId(0);

    if (item != ITEM_UNARMED)
    {
        ammotype = get_ammo_type_for_weapon(item);

        if (ammotype != 0
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
            && !bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
        {
            imageoffset = ammo_related[ammotype].IconImage;
            textwidth = 5;

            get_ptr_item_statistics(item);

            if (imageoffset != 0)
            {
                imageoffset += globalbank_rdram_offset;

                // Draw the ammo icon
#if defined(VERSION_EU)
                gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset, 200.0f, 208.0f, (viGetViewTop() + viGetViewHeight()) - 30, 0, ammo_related[ammotype].IconYOffset, 1);
#else
                gdl = set_rgba_redirect_generate_microcode(gdl, (u8 *)imageoffset, 200.0f, 180.0f, (viGetViewTop() + viGetViewHeight()) - 20, 0, ammo_related[ammotype].IconYOffset, 1);
#endif

                textwidth = ((u8 *)imageoffset)[4];
            }

            gdl = microcode_constructor(gdl);

            if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                magammo = 0;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;

                if (offhanditem == item)
                {
                    reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                }
            }
            else
            {
                magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
            }

            if (!bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the magazine ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 177, 2, 0);
#endif
            }

            if (reserveammo > 0 || bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the reserve ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 177, 2, 0);
#endif
            }

            gdl = combiner_bayer_lod_perspective(gdl);
        }
    }

    return gdl;
}


void gunSetSightVisible(s32 reason, bool visible)
{
    if (visible)
    {
        g_CurrentPlayer->gunsightmode &= ~reason;
        return;
    }

    g_CurrentPlayer->gunsightmode |= reason;
}


void gunDrawSight(s32 *gdl) {

    s32 sp54;
    f32 xypos[2];
    f32 halfedxy[2];

    if ((g_CurrentPlayer->gunsightmode == 0) && (g_CurrentPlayer->mpmenuon == FALSE)) {
        sp54 = *gdl;
        texSelect(&sp54, crosshairimage, 4, 0, 0);

        xypos[0] = g_CurrentPlayer->crosshair_angle.f[0];
        xypos[1] = g_CurrentPlayer->crosshair_angle.f[1];
        halfedxy[0] = 16.0f;
        halfedxy[1] = 16.0f;

        if (get_screen_ratio() == SCREEN_RATIO_16_9) {
            halfedxy[0] = halfedxy[0] * 0.75f;
        }
#ifdef VERSION_EU
        halfedxy[1] = halfedxy[1] * 1.19047617912f;
#endif
        display_image_at_position(&sp54, &xypos, &halfedxy, 0x20, 0x20, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0x6E, (crosshairimage->level > 0), 0);
        *gdl = sp54;
    }
}


void inc_curplayer_hitcount_with_weapon(ITEM_IDS item, SHOT_REGISTER shot_register) {

    if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_PLAYER_STAT_HIT)) {
        g_playerPerm->shot_count[shot_register] = g_playerPerm->shot_count[shot_register]+1;
    }
}


s32 get_curplayer_shot_register(SHOT_REGISTER shot_register)
{
  return g_playerPerm->shot_count[shot_register];
}


void inc_cur_civilian_casualties(void)
{
    g_playerPerm->killed_civilians++;
}


s32 get_civilian_casualties(void)
{
    return g_playerPerm->killed_civilians;
}


//D:80053BF8
const char aSD[] = "%s: %d\n";

void increment_num_kills_display_text_in_MP(void)
{
    s8 buffer[256];
    s32 time_since_kill;
    s32 recent_kill_count;
    s32 mission_time;
    s32 unused; // needed this variable to match

    g_playerPerm->kill_count += 1;
    g_CurrentPlayer->kills_this_life += 1;

    if (getPlayerCount() < 2) { return; }

    mission_time = getMissiontimer();
    sprintf(&buffer, aSD, langGet(getStringID(LGUN, GUN_STR_DA_KILLCOUNT)), g_playerPerm->kill_count); // "kill count"

#if defined(VERSION_US)
    hudmsgBottomShow(&buffer);
#elif defined(VERSION_JP) || defined(VERSION_EU)
    jp_hudmsgBottomShow(&buffer);
#endif

    if (g_playerPerm->kill_count >= 2)
    {
        time_since_kill = mission_time - g_CurrentPlayer->last_kill_time[0];
        if (g_playerPerm->max_time_between_kills < time_since_kill)
        {
            g_playerPerm->max_time_between_kills = time_since_kill;
        }

        if (time_since_kill < g_playerPerm->min_time_between_kills)
        {
            g_playerPerm->min_time_between_kills = time_since_kill;
        }
    }

    recent_kill_count = 1;
    g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
    g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
    g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
    g_CurrentPlayer->last_kill_time[0] = mission_time;

    // I tried to turn this into a loop but it didn't match
    if (g_CurrentPlayer->last_kill_time[1] != -1 && (g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78)
    {
        recent_kill_count++;
        if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78))
        {
            recent_kill_count++;
            if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78))
            {
                recent_kill_count++;
            }
        }
    }

    if (g_playerPerm->most_killed_one_time < recent_kill_count)
    {
        g_playerPerm->most_killed_one_time = recent_kill_count;
    }
}



s32 get_curplay_killcount(void) {
    return g_playerPerm->kill_count;
}

void increment_num_times_killed_MwtGC(void){
    g_playerPerm->killed_gg_owner_count++;
}

s32 get_times_killed_mwtgx(void) {
    return g_playerPerm->killed_gg_owner_count;
}


void increment_num_deaths(void)
{
	char buffer[256];
    g_CurrentPlayer->deathcount = (s32) (g_CurrentPlayer->deathcount + 1);
    if (getPlayerCount() >= 2)
    {
        if (g_CurrentPlayer->deathcount == 1)
        {
            sprintf(buffer, langGet(getStringID(LGUN, GUN_STR_DB_DIEDONCE_LF))); //died once
        }
        else
        {
            sprintf(buffer, "%s %d %s\n", langGet(getStringID(LGUN, GUN_STR_DC_DIED)), g_CurrentPlayer->deathcount, langGet(getStringID(LGUN, GUN_STR_DD_TIMES))); //died times
        }
#if defined(VERSION_JP) || defined(VERSION_EU)
		jp_hudmsgBottomShow(buffer);
#else
		hudmsgBottomShow(buffer);
#endif
    }
}


s32 get_curplayer_numdeaths(void) {
    return g_CurrentPlayer->deathcount;
}

//D:80053C0C
const char aSD_0[] = "%s: %d\n";

void increment_num_suicides_display_MP(void) {
    char buffer[256];
    s32 time_diff;
    s32 recent_kill_count;
    s32 currentTime;

    g_CurrentPlayer->num_suicides += 1;
    if (getPlayerCount() >= 2) {

        currentTime = getMissiontimer();

        sprintf(&buffer, &aSD_0, langGet(getStringID(LGUN, GUN_STR_DE_SUICIDECOUNT)), g_CurrentPlayer->num_suicides); // "suicide count"

#if defined(VERSION_JP) || defined(VERSION_EU)
		jp_hudmsgBottomShow(&buffer);
#else
		hudmsgBottomShow(&buffer);
#endif

        if (g_playerPerm->kill_count >= 2) {
            time_diff = currentTime - g_CurrentPlayer->last_kill_time[0];
            if (g_playerPerm->max_time_between_kills < time_diff) {
                g_playerPerm->max_time_between_kills = time_diff;
            }
            if (time_diff < g_playerPerm->min_time_between_kills) {
                g_playerPerm->min_time_between_kills = time_diff;
            }
        }
        recent_kill_count = 1;
        g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
        g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
        g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
        g_CurrentPlayer->last_kill_time[0] = currentTime;

        if ( g_CurrentPlayer->last_kill_time[1] != -1) {

            if ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78) {

                recent_kill_count += 1;

                if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78)) {

                    recent_kill_count += 1;

                    if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78)) {
                        recent_kill_count += 1;
                    }
                }
            }
        }

        if (g_playerPerm->most_killed_one_time < recent_kill_count) {
            g_playerPerm->most_killed_one_time = recent_kill_count;
        }
    }
}

s32 get_curplayer_numsuicides(void) {
    return g_CurrentPlayer->num_suicides;
}
