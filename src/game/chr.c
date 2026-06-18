#include <ultra64.h>
#include <PR/gbi.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <random.h>
#include <snd.h>
#include <math.h>
#include "bondaicommands.h"
#include "bg.h"
#include "cheat.h"
#include "chr.h"
#include "chrai.h"
#include "chraction.h"
#include "chrobjdata.h"
#include "debugmenu_handler.h"
#include "dyn.h"
#include "glass.h"
#include "file2.h"
#include "propobj.h"
#include "explosion.h"
#include "file.h"
#include "gun.h"
#include "initanitable.h"
#include "joy.h"
#include "lv.h"
#include "language.h"
#include "matrixmath.h"
#include "objecthandler.h"
#include "player.h"
#include "propobj.h"
#include "stan.h"
#include "model.h"
#include "tex.h"

#ifdef VERSION_EU
#define GROUND_SMOOTH_FACTOR 0.118799984f /* 0x3DF34D68 (PAL-scaled 0.1) */
#define FALLSPEED_DECAY      0.8812f      /* 0x3F619653 (PAL-scaled 0.9) */
#else
#define GROUND_SMOOTH_FACTOR 0.100000024f /* 0x3DCCCCD0 */
#define FALLSPEED_DECAY      0.9f         /* 0x3F666666 */
#endif

// forward declarations

void chrUpdateAimProperties( ChrRecord *arg0);
void chrPositionRelated7F020E40( ChrRecord *chr, s32 arg1);
void sub_GAME_7F057D44(f32 *arg0, f32 *arg1, f32 arg2);

// end forward declarations

// data
f32 animation_rate = 0;
s32 D_8002C904 = 0;

/**
 * Address 0x8002C908.
 */
s32 g_AnimationTablePointerCountRelated = 0;
s32 D_8002C90C = 0;
s32 D_8002C910 = 0;

//D:8002C914
struct animation_something D_8002C914[] = {
    {0         , 0, 0,    0, 0, 0,    0,       NULL, 0,       NULL, 0},
    {1         , 1, 0, 17.0, 3, 0, 34.0, D_8002CE54, 0, D_8002DF10, 0},
    {2         , 1, 0, 17.0, 3, 0, 39.0, D_8002CEE0, 0, D_8002DF64, 0},
    {3         , 1, 0, 21.0, 3, 0, 43.0, D_8002CF6C, 0, D_8002DFB8, 0},
    {4         , 1, 0, 17.0, 3, 0, 34.0, D_8002D014, 0, D_8002E028, 0},
    {5         , 1, 0, 17.0, 3, 0, 39.0, D_8002D0A0, 0, D_8002E07C, 0},
    {6         , 1, 0, 21.0, 3, 0, 43.0, D_8002D12C, 0, D_8002E0D0, 0},
    {7         , 1, 0, 21.0, 3, 0, 52.0, D_8002D1D4, 0, D_8002E140, 0},
    {8         , 1, 0, 21.0, 3, 0, 43.0, D_8002D3B0, 0, D_8002E23C, 0},
    {9         , 1, 0, 17.0, 3, 0, 34.0, D_8002D6DC, 0, D_8002E300, 0}, // previously D_8002CAA0 = &D_8002C914 + (0x2c * 9)
    {0xA       , 1, 0, 17.0, 3, 0, 43.0, D_8002D768, 0, D_8002E354, 0}, // previously D_8002CACC
    {0xB       , 1, 0, 21.0, 3, 0, 52.0, D_8002D7F4, 0, D_8002E3A8, 0},
    {0xC       , 1, 0, 17.0, 3, 0, 34.0, D_8002D880, 0, D_8002E418, 0}, // previously D_8002CB24
    {0xD       , 1, 0, 17.0, 3, 0, 43.0, D_8002D90C, 0, D_8002E46C, 0}, // previously D_8002CB50
    {0xE       , 1, 0, 21.0, 3, 0, 52.0, D_8002D998, 0, D_8002E4C0, 0},
    {0xF       , 1, 0, 26.0, 3, 0, 60.0, D_8002DA24, 0, D_8002E530, 0},
    {0x64      , 1, 0, 26.0, 0, 0,  0.0, D_8002DCE0, 0, D_8002E5BC, 0},
    {0x6E      , 1, 0, 21.0, 0, 0,  0.0,       NULL, 0,       NULL, 0},
    {0xFFFFFFFF, 0, 0,  0.0, 0, 0,  0.0,       NULL, 0,       NULL, 0},
};




s32 D_8002CC58 = 0;
s32 show_patrols_flag = FALSE;
s32 player1_guardID = 5000;
 ChrRecord *g_ChrSlots = 0;
s32 g_NumChrSlots = 0;
 ModelRenderData D_8002CC6C        = {NULL,
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
coord3d D_8002CCAC = {0, 0, 0};

 rgba_u8 gBloodColour = { 0x5a, 0, 0, 0};

/**
 * Address 0x8002CCBC.
*/

ModelRenderData D_8002CCBC = {NULL,
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
//s32 D_8002CCF8 = 0;
s32 D_8002CCFC = 0;
u32 num_bodies = 0;
u32 num_male_heads = 0;
u32 num_female_heads = 0;

s32 list_of_bodies[] = {
    BODY_Jungle_Commando,BODY_St_Petersburg_Guard,BODY_Russian_Soldier,BODY_Russian_Infantry,
    BODY_Janus_Special_Forces,BODY_Brosnan_Tuxedo,BODY_Boris,BODY_Ourumov,
    BODY_Trevelyan_Janus,BODY_Valentin_,BODY_Xenia,BODY_Baron_Samedi,
    BODY_Jaws,BODY_Mayday,BODY_Oddjob,BODY_Natalya_Skirt,
    BODY_Janus_Marine,BODY_Russian_Commandant,BODY_Siberian_Guard_1_Mishkin,BODY_Naval_Officer,
    BODY_Siberian_Special_Forces,BODY_Special_Operations_Uniform,BODY_Formal_Wear,BODY_Jungle_Fatigues,
    BODY_Unused_Female,BODY_Rosika,BODY_Scientist_2_Female,BODY_Civilian_1_Female,
    BODY_Unused_Male_1,BODY_Unused_Male_2,BODY_Civilian_4,BODY_Civilian_2,
    BODY_Civilian_3,BODY_Scientist_1_Male,BODY_Brosnan_Tuxedo,BODY_Brosnan_Tuxedo,
    BODY_Brosnan_Tuxedo,BODY_Helicopter_Pilot,BODY_Siberian_Guard_2,BODY_Arctic_Commando,
    BODY_Moonraker_Elite_1_Male,BODY_Moonraker_Elite_2_Female,-1,
};

s32 random_male_heads[] = {
    HEAD_Male_Jim,HEAD_Male_Chris,HEAD_Male_Lee,HEAD_Male_Graeme,HEAD_Male_Steve_H,
    HEAD_Male_Neil,HEAD_Male_Robin,HEAD_Male_Des,HEAD_Male_Grant,HEAD_Male_Dave_Dr_Doak,
    HEAD_Male_Karl,HEAD_Male_Alan,HEAD_Male_Pete,HEAD_Male_Martin,HEAD_Male_Mark,
    HEAD_Male_Duncan,HEAD_Male_Shaun,HEAD_Male_Dwayne,HEAD_Male_B,HEAD_Male_Steve_Ellis,
    HEAD_Male_Joel,HEAD_Male_Scott,HEAD_Male_Joe_Altered,HEAD_Male_Ken,HEAD_Male_Joe,
    -1
};

s32 random_female_heads[] = {
    HEAD_Female_Sally,HEAD_Female_Marion_Rosika,HEAD_Female_Mandy,HEAD_Female_Vivien, -1
};

u32 current_random_body = 0;
u32 current_random_male_head = 0;
u32 current_random_female_head = 0;

/*
* Enemy accuracy modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiAccuracyModifier = 1.0f;

/*
* Enemy damage modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiDamageModifier = 1.0f;

f32 g_AiHealthModifier = 1.0f;

/*
* Enemy reaction speed modifier.
* Set on level load.
* One of the values that can be set with the 007 slider.
*/
f32 g_AiReactionSpeed = 1.0f;

/**
 * Count of number of guards that have recently seen bond:
 *     (guard->lastseetarget60 > 0) && (g_GlobalTimer - guard->lastseetarget60 < 120)
 * Updated every tick.
 * Address 0x8002CE50.
*/
s32 g_SeenBondRecentlyGuardCount = 0;

struct struck_animation_table D_8002CE54[] = {
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002CEE0[] = {
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002CF6C[] = {
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 1, 23.0, -1.0 },
    { PTR_ANIM_death_left_leg, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D014[] = {
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D0A0[] = {
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D12C[] = {
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 1, 26.0, -1.0 },
    { PTR_ANIM_death_left_leg, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, 0.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D1D4[] = {
    { PTR_ANIM_death_forward_face_down, 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_face_down, 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_spin_face_up, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_forward_spin_face_up, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_hard, 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_forward_face_down_hard, 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_fetal_position_right, 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_right, 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 1, -1.0, 0.5, 0, 28.0, -1.0 },
    { PTR_ANIM_death_genitalia, 0, -1.0, 0.5, 0, 79.0, 415.0 },
    { PTR_ANIM_death_genitalia, 1, -1.0, 0.5, 0, 79.0, 415.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D3B0[] = {
    { PTR_ANIM_death_forward_face_down, 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_face_down, 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_spin_face_up, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_forward_spin_face_up, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 1, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 1, 26.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_hard, 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_forward_face_down_hard, 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_forward_face_down_soft, 0, -1.0, 0.5, 0, 94.0, 66.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, -1.0, 0.5, 0, 94.0, 66.0 },
    { PTR_ANIM_death_fetal_position_right, 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_right, 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 1, -1.0, 0.5, 0, 28.0, -1.0 },
    { PTR_ANIM_death_neck, 0, -1.0, 0.5, 0, 87.0, 203.0 },
    { PTR_ANIM_death_neck, 1, -1.0, 0.5, 0, 87.0, 203.0 },
    { PTR_ANIM_death_head, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_head, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D6DC[] = {
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D768[] = {
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 0, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 0, 23.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D7F4[] = {
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 1, 23.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D880[] = {
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D90C[] = {
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 0, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 0, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 0, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 0, 26.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002D998[] = {
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 1, 26.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002DA24[] = {
    { PTR_ANIM_death_forward_face_down, 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_face_down, 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_spin_face_up, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_forward_spin_face_up, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 0, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_right, 1, -1.0, 0.5, 1, 25.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 0, -1.0, 0.5, 1, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_right, 1, -1.0, 0.5, 1, 23.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 0, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_down_left, 1, -1.0, 0.5, 1, 27.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 0, -1.0, 0.5, 1, 26.0, -1.0 },
    { PTR_ANIM_death_backward_spin_face_up_left, 1, -1.0, 0.5, 1, 26.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_hard, 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_forward_face_down_hard, 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_forward_face_down_soft, 0, -1.0, 0.5, 0, 94.0, 66.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, -1.0, 0.5, 0, 94.0, 66.0 },
    { PTR_ANIM_death_fetal_position_right, 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_right, 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 1, -1.0, 0.5, 0, 28.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002DCE0[] = {
    { PTR_ANIM_death_forward_face_down, 0, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_face_down, 1, -1.0, 0.5, 0, 55.0, 39.0 },
    { PTR_ANIM_death_forward_spin_face_up, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_forward_spin_face_up, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 0, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up1, 1, -1.0, 0.5, 1, 29.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_hard, 0, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_forward_face_down_hard, 1, -1.0, 0.5, 0, 97.0, 64.0 },
    { PTR_ANIM_death_forward_face_down_soft, 0, -1.0, 0.5, 0, 94.0, 66.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, -1.0, 0.5, 0, 94.0, 66.0 },
    { PTR_ANIM_death_fetal_position_right, 0, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_right, 1, -1.0, 0.5, 0, 31.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 0, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_fetal_position_left, 1, -1.0, 0.5, 0, 36.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 0, -1.0, 0.5, 0, 28.0, -1.0 },
    { PTR_ANIM_death_backward_fall_face_up2, 1, -1.0, 0.5, 0, 28.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002DEBC[] = {
    { PTR_ANIM_death_stagger_back_to_wall, 0, -1.0, 0.5, 0, 67.0, 54.0 },
    { PTR_ANIM_death_stagger_back_to_wall, 1, -1.0, 0.5, 0, 67.0, 54.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002DF10[] = {
    { PTR_ANIM_hit_left_leg, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_right_leg, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002DF64[] = {
    { PTR_ANIM_hit_left_leg, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_right_leg, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002DFB8[] = {
    { PTR_ANIM_hit_left_leg, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_right_leg, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_left_leg, 1, 20.0, 0.40000001, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E028[] = {
    { PTR_ANIM_hit_right_leg, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_left_leg, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E07C[] = {
    { PTR_ANIM_hit_right_leg, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_left_leg, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E0D0[] = {
    { PTR_ANIM_hit_right_leg, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_left_leg, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_left_leg, 0, 20.0, 0.40000001, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E140[] = {
    { PTR_ANIM_death_genitalia, 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_genitalia, 1, 30.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_soft, 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 1, 15.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_fetal_position_right, 0, 10.0, 0.25, 0, -1.0, -1.0 },
    { PTR_ANIM_death_fetal_position_right, 1, 10.0, 0.25, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E23C[] = {
    { PTR_ANIM_death_neck, 0, 15.0, 0.5, 0, 87.0, 203.0 },
    { PTR_ANIM_death_neck, 1, 15.0, 0.5, 0, 87.0, 203.0 },
    { PTR_ANIM_death_forward_face_down_soft, 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 1, 15.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E300[] = {
    { PTR_ANIM_hit_left_hand, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_right_hand, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E354[] = {
    { PTR_ANIM_hit_left_arm, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_right_arm, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E3A8[] = {
    { PTR_ANIM_hit_left_shoulder, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_right_shoulder, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_soft, 0, 20.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E418[] = {
    { PTR_ANIM_hit_right_hand, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_left_hand, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E46C[] = {
    { PTR_ANIM_hit_right_arm, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_left_arm, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E4C0[] = {
    { PTR_ANIM_hit_right_shoulder, 0, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_hit_left_shoulder, 1, -1.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, 20.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E530[] = {
    { PTR_ANIM_death_forward_face_down_soft, 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 1, 15.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};

struct struck_animation_table D_8002E5BC[] = {
    { PTR_ANIM_death_forward_face_down_soft, 0, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down_soft, 1, 20.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 0, 15.0, 0.5, 0, -1.0, -1.0 },
    { PTR_ANIM_death_forward_face_down, 1, 15.0, 0.5, 0, -1.0, -1.0 },
    {0, 0, -1.0, 0.5, 0, -1.0, -1.0}
};



struct explosion_death_animation D_8002E648[] = {
    { PTR_ANIM_death_explosion_forward, 0, 0.5, 9.0, 18.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_forward, 1, 0.5, 9.0, 18.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_face_down, 0, 0.5, 11.0, 19.0, 31.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_face_down, 1, 0.5, 11.0, 19.0, 31.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_roll, 0, 0.5, 6.0, 20.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_roll, 1, 0.5, 6.0, 20.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2, 0, 0.5, 29.0, 36.0, 48.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2, 1, 0.5, 29.0, 36.0, 48.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2_alt, 0, 0.5, 29.0, 38.0, 49.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right2_alt, 1, 0.5, 29.0, 38.0, 49.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right3, 0, 0.5, 19.0, 30.0, 42.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right3, 1, 0.5, 19.0, 30.0, 42.0, -1.0 },
    { PTR_ANIM_death_explosion_left1, 0, 0.5, 9.0, 21.0, 29.0, 55.0 },
    { PTR_ANIM_death_explosion_left1, 1, 0.5, 9.0, 21.0, 29.0, 55.0 },
    { PTR_ANIM_death_explosion_right, 0, 0.5, 6.0, 18.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_right, 1, 0.5, 6.0, 18.0, 27.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right1, 0, 0.5, 6.0, 19.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_forward_right1, 1, 0.5, 6.0, 19.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_back_left, 0, 0.5, 8.0, 14.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back_left, 1, 0.5, 8.0, 14.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back1, 0, 0.5, 8.0, 19.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back1, 1, 0.5, 8.0, 19.0, 25.0, -1.0 },
    { PTR_ANIM_death_explosion_back2, 0, 0.5, 12.0, 21.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_back2, 1, 0.5, 12.0, 21.0, 29.0, -1.0 },
    { PTR_ANIM_death_explosion_left2, 0, 0.5, 22.0, 30.0, 41.0, -1.0 },
    { PTR_ANIM_death_explosion_left2, 1, 0.5, 22.0, 30.0, 41.0, -1.0 },
    {0, 0, 0.5, 0.0, 0.0, 0.0, -1.0},
};

s8 expl_forward[] = {0x0, 0x01, 0x02, 0x03, 0x4, 0x05, 0x00, 0x00};
s8 expl_f_left[] = {0x7, 0x09, 0x0B, 0x00};
s8 expl_f_right[] = {0x06, 0x08, 0x0A, 0x00};
s8 expl_left[] = {0x0C, 0x0F, 0x11, 0x00};
s8 expl_right[] = {0x0D, 0x0E, 0x10, 0x00};
s8 expl_back[] = {0x14, 0x15, 0x16, 0x17};
s8 expl_b_right[] = {0x12, 0x18, 0x00, 0x00};
s8 expl_b_left[] = {0x13, 0x19, 0x00, 0x00};

struct explosion_anim_group_info explosion_animation_table[EXPLOSION_ANIMATION_TABLE_LEN] = {
    {expl_forward, 6},
    {expl_f_left, 3},
    {expl_left, 3},
    {expl_b_right, 2},
    {expl_back, 4},
    {expl_b_left, 2},
    {expl_right, 3},
    {expl_f_right, 3}
};

struct weapon_firing_animation_table rifle_firing_animation_group1[] = {
    { PTR_ANIM_fire_standing_fast, 28.0, 0, 0, 0, -1.0, 23.0, 54.0, -1.0, -1.0, 18.0, 54.0, 0.87266463, -0.52359879, 1.0471976, -0.34906587, 1.6, 1.8 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group1 = { &rifle_firing_animation_group1, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group2[] = {
    { PTR_ANIM_fire_standing, 37.0, 0, 0, 0, -1.0, 30.0, 81.0, -1.0, -1.0, 25.0, 81.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 1.6, 1.75 },
    { PTR_ANIM_fire_hip, 27.0, 0, 0, 0, -1.0, 22.0, 61.0, -1.0, -1.0, 17.0, 61.0, 0.87266463, -0.2617994, 0.69813174, -0.69813174, 2.0, 1.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group2 = { &rifle_firing_animation_group2, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group5[] = {
    { PTR_ANIM_fire_standing, 37.0, 0, 0, 0, -1.0, 30.0, 81.0, -1.0, -1.0, 25.0, 81.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 1.6, 1.75 },
    { PTR_ANIM_fire_hip, 27.0, 0, 0, 0, -1.0, 22.0, 61.0, -1.0, -1.0, 17.0, 61.0, 0.87266463, -0.2617994, 0.69813174, -0.69813174, 2.0, 1.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group5 = { &rifle_firing_animation_group5, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group3[] = {
    { PTR_ANIM_fire_shoulder_left, 19.0, 0, 1.5707964, 0, -1.0, 19.0, 61.0, -1.0, -1.0, 14.0, 61.0, 0.87266463, -0.34906587, 0.43633232, -1.0471976, 2.5, 2.5 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group3 = { &rifle_firing_animation_group3, -1 };

struct weapon_firing_animation_table rifle_firing_animation_group4[] = {
    { PTR_ANIM_fire_turn_right2, 27.0, 0, 0, 0, -1.0, 39.0, 74.0, -1.0, -1.0, 34.0, 74.0, 0.87266463, -0.69813174, 0.78539819, -0.69813174, 1.5, 1.5 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_rifle_firing_animation_group4 = { &rifle_firing_animation_group4, -1 };

struct anim_group_info *ptr_rifle_firing_animation_groups[] = {
    &ptr_rifle_firing_animation_group1,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group2,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group3,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group4,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group5,
    &ptr_rifle_firing_animation_group1
};

struct weapon_firing_animation_table pistol_firing_animation_group1[] = {
    { PTR_ANIM_fire_standing_one_handed_weapon, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { PTR_ANIM_fire_hip_one_handed_weapon_fast, 0.0, 0, 0, 17.0, 100.0, 25.0, 87.0, 30.0, 55.0, 20.0, 93.0, 0.87266463, -0.69813174, 0.69813174, -1.0471976, 0.0, 0.0 },
    { PTR_ANIM_fire_hip_one_handed_weapon_slow, 0.0, 0, 0, 12.0, 64.0, 19.0, 51.0, 24.0, 46.0, 14.0, 58.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_hip_forward_one_handed_weapon, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group1 = { &pistol_firing_animation_group1, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group2[] = {
    { PTR_ANIM_fire_standing_one_handed_weapon, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { PTR_ANIM_fire_hip_forward_one_handed_weapon, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group2 = { &pistol_firing_animation_group2, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group3[] = {
    { PTR_ANIM_fire_standing_one_handed_weapon, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { PTR_ANIM_fire_hip_forward_one_handed_weapon, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_standing_left_one_handed_weapon_slow, 0.0, 0, 1.5707964, 7.0, 130.0, 45.0, 93.0, 56.0, 73.0, 26.0, 107.0, 0.87266463, -0.69813174, 0.34906587, -0.52359879, 0.0, 0.0 },
    { PTR_ANIM_fire_standing_left_one_handed_weapon_fast, 15.0, 0, 1.5707964, 5.0, 76.0, 20.0, 31.0, 31.0, 38.0, 15.0, 49.0, 0.87266463, -0.69813174, 0.52359879, -1.0471976, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group3 = { &pistol_firing_animation_group3, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group6[] = {
    { PTR_ANIM_fire_standing_one_handed_weapon, 26.0, 0, 0, 12.0, 140.0, 58.0, 92.0, 60.0, 79.0, 20.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    { PTR_ANIM_fire_hip_forward_one_handed_weapon, 22.0, 0, 0, 4.0, 69.0, 22.0, 49.0, 22.0, 33.0, 8.0, 58.0, 0.87266463, -0.69813174, 0.43633232, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_standing_right_one_handed_weapon, 0.0, 0, 4.712389, 7.0, 139.0, 54.0, 105.0, 61.0, 88.0, 26.0, 120.0, 0.87266463, -0.69813174, 0.69813174, -0.61086529, 0.0, 0.0 },
    { PTR_ANIM_fire_step_right_one_handed_weapon, 19.0, 0, 4.712389, 4.0, 79.0, 21.0, 50.0, 26.0, 42.0, 10.0, 64.0, 0.87266463, -0.69813174, 0.69813174, -0.61086529, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group6 = { &pistol_firing_animation_group6, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group4[] = {
    { PTR_ANIM_fire_standing_left_one_handed_weapon_fast, 19.0, 0, 1.5707964, 5.0, 76.0, 20.0, 31.0, 31.0, 38.0, 15.0, 49.0, 0.87266463, -0.69813174, 0.52359879, -1.0471976, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group4 = { &pistol_firing_animation_group4, -1 };

struct weapon_firing_animation_table pistol_firing_animation_group5[] = {
    { PTR_ANIM_fire_step_right_one_handed_weapon, 19.0, 0, 4.712389, 4.0, 79.0, 21.0, 50.0, 26.0, 42.0, 10.0, 64.0, 0.87266463, -0.69813174, 0.69813174, -0.61086529, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_pistol_firing_animation_group5 = { &pistol_firing_animation_group5, -1 };

struct anim_group_info *ptr_pistol_firing_animation_groups[] = {
    &ptr_pistol_firing_animation_group1,
    &ptr_pistol_firing_animation_group1,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group3,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group4,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group5,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group6,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group2,
    &ptr_pistol_firing_animation_group1,
    &ptr_pistol_firing_animation_group1
};

struct weapon_firing_animation_table doubles_firing_animation_group1[] = {
    { PTR_ANIM_fire_standing_dual_wield, 26.0, 0, 0, 7.0, 92.0, 28.0, 68.0, -1.0, -1.0, 11.0, 73.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_doubles_firing_animation_group1 = { &doubles_firing_animation_group1, -1 };

struct weapon_firing_animation_table doubles_firing_animation_group2[] = {
    { PTR_ANIM_fire_standing_dual_wield_left, 26.0, 0, 1.5707964, 9.0, 112.0, 38.0, 87.0, -1.0, -1.0, 19.0, 98.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    { PTR_ANIM_fire_standing_dual_wield_hands_crossed_left, 25.0, 0, 1.5707964, 10.0, 112.0, 32.0, 86.0, -1.0, -1.0, 19.0, 97.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_doubles_firing_animation_group2 = { &doubles_firing_animation_group2, -1 };

struct weapon_firing_animation_table doubles_firing_animation_group3[] = {
    { PTR_ANIM_fire_standing_dual_wield_right, 39.0, 0, 4.712389, 22.0, 127.0, 44.0, 102.0, -1.0, -1.0, 28.0, 112.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    { PTR_ANIM_fire_standing_dual_wield_hands_crossed_right, 39.0, 0, 4.712389, 23.0, 130.0, 46.0, 100.0, -1.0, -1.0, 30.0, 110.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_doubles_firing_animation_group3 = { &doubles_firing_animation_group3, -1 };

struct anim_group_info *ptr_doubles_firing_animation_groups[] = {
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group2,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group3,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1,
    &ptr_doubles_firing_animation_group1
};

struct weapon_firing_animation_table crouched_rifle_firing_animation_group1[] = {
    { PTR_ANIM_fire_kneel_right_leg, 27.0, 0, 0, 0, -1.0, 35.0, 75.0, -1.0, -1.0, 31.0, 75.0, 0.87266463, -0.69813174, 0.90757126, -0.69813174, 1.5, 1.5 },
};

struct weapon_firing_animation_table crouched_rifle_firing_animation_groupA[] = {
    { PTR_ANIM_fire_kneel_left_leg, 24.0, 0, 0, 0, -1.0, 46.0, 98.0, -1.0, -1.0, 41.0, 98.0, 0.87266463, -0.52359879, 1.134464, -0.69813174, 1.6, 1.6 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_rifle_firing_animation_group1 = { &crouched_rifle_firing_animation_group1, -1 };

struct weapon_firing_animation_table crouched_rifle_firing_animation_group2[] = {
    { PTR_ANIM_fire_kneel_left, 26.0, 0, 0, 0, -1.0, 34.0, 87.0, -1.0, -1.0, 29.0, 87.0, 0.87266463, -0.52359879, 0.69813174, -0.95993108, 1.6, 2.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_rifle_firing_animation_group2 = { &crouched_rifle_firing_animation_group2, -1 };

struct weapon_firing_animation_table crouched_rifle_firing_animation_group3[] = {
    { PTR_ANIM_fire_kneel_right, 28.0, 0, 0, 0, -1.0, 36.0, 88.0, -1.0, -1.0, 31.0, 88.0, 0.87266463, -0.69813174, 0.87266463, -0.43633232, 1.6, 1.5 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_rifle_firing_animation_group3 = { &crouched_rifle_firing_animation_group3, -1 };

struct anim_group_info *ptr_crouched_rifle_firing_animation_groups[] = {
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group2,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group3,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1,
    &ptr_crouched_rifle_firing_animation_group1
};

struct weapon_firing_animation_table crouched_pistol_firing_animation_group1[] = {
    { PTR_ANIM_fire_kneel_forward_one_handed_weapon_slow, 25.0, 0, 0, 12.0, 132.0, 55.0, 87.0, 67.0, 87.0, 26.0, 111.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_kneel_forward_one_handed_weapon_fast, 26.0, 0, 0, 8.0, 89.0, 31.0, 63.0, 41.0, 51.0, 21.0, 80.0, 0.87266463, -0.69813174, 0.34906587, -1.134464, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_pistol_firing_animation_group1 = { &crouched_pistol_firing_animation_group1, -1 };

struct weapon_firing_animation_table crouched_pistol_firing_animation_group2[] = {
    { PTR_ANIM_fire_kneel_left_one_handed_weapon_slow, 47.0, 0, 1.5707964, 7.0, 128.0, 33.0, 86.0,47.0, 74.0, 23.0, 106.0, 0.87266463, -0.52359879, 0.52359879, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_kneel_left_one_handed_weapon_fast, 18.0, 0, 1.5707964, 7.0, 78.0, 28.0, 52.0, 35.0, 45.0, 15.0, 66.0, 0.87266463, -0.087266468, 0.69813174, -0.78539819, 1.5, 1.0 },
    { PTR_ANIM_fire_kneel_left_one_handed_weapon, 20.0, 0, 1.5707964, 13.0, 92.0, 37.0, 67.0, 42.0, 55.0, 25.0, 84.0, 0.87266463, -0.52359879, 0.34906587, -0.69813174, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_pistol_firing_animation_group2 = { &crouched_pistol_firing_animation_group2, -1 };

struct weapon_firing_animation_table crouched_pistol_firing_animation_group3[] = {
    { PTR_ANIM_fire_kneel_right_one_handed_weapon_slow, 28.0, 0, 4.712389, 15.0, 124.0, 38.0, 97.0, 60.0, 84.0, 20.0, 106.0, 0.87266463, -0.69813174, 0.52359879, -0.87266463, 0.0, 0.0 },
    { PTR_ANIM_fire_kneel_right_one_handed_weapon_fast, 23.0, 0, 4.712389, 0, 85.0, 32.0, 38.0, 38.0, 60.0, 14.0, 71.0, 0.87266463, -0.69813174, 0.61086529, -0.95993108, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_pistol_firing_animation_group3 = { &crouched_pistol_firing_animation_group3, -1 };

struct anim_group_info *ptr_crouched_pistol_firing_animation_groups[] = {
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group2,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group3,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1,
    &ptr_crouched_pistol_firing_animation_group1
};

struct weapon_firing_animation_table crouched_doubles_firing_animation_group1[] = {
    { PTR_ANIM_fire_kneel_dual_wield, 22.0, 0, 0, 10.0, 111.0, 34.0, 87.0, -1.0, -1.0, 17.0, 104.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_kneel_dual_wield_hands_crossed, 25.0, 0, 0, 9.0, 92.0, 33.0, 62.0, -1.0, -1.0, 18.0, 69.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_doubles_firing_animation_group1 = { &crouched_doubles_firing_animation_group1, -1 };

struct weapon_firing_animation_table crouched_doubles_firing_animation_group2[] = {
    { PTR_ANIM_fire_kneel_dual_wield_left, 28.0, 0, 1.5707964, 15.0, 108.0, 34.0, 73.0, -1.0, -1.0, 17.0, 93.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_kneel_dual_wield_hands_crossed_left, 19.0, 0, 1.5707964, 3.0, 95.0, 30.0, 64.0, -1.0, -1.0, 14.0, 71.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 1.5, 1.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_doubles_firing_animation_group2 = { &crouched_doubles_firing_animation_group2, -1 };

struct weapon_firing_animation_table crouched_doubles_firing_animation_group3[] = {
    { PTR_ANIM_fire_kneel_dual_wield_right, 31.0, 0, 4.712389, 14.0, 111.0, 40.0, 83.0,-1.0, -1.0, 21.0, 94.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    { PTR_ANIM_fire_kneel_dual_wield_hands_crossed_right, 26.0, 0, 4.712389, 7.0, 89.0, 34.0, 60.0, -1.0, -1.0, 20.0, 68.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },
    {0, 0.0, 0, 0, 0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct anim_group_info ptr_crouched_doubles_firing_animation_group3 = { &crouched_doubles_firing_animation_group3, -1 };

struct anim_group_info* ptr_crouched_doubles_firing_animation_groups[] = {
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group2,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group3,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1,
    &ptr_crouched_doubles_firing_animation_group1
};

struct weapon_firing_animation_table D_80030078[] = {
    // sizeof(struct weapon_firing_animation_table) = 0x48 = 72.
    // address 0xD_80030078. Index 0, = D_80030078 + 0.
    { PTR_ANIM_fire_roll_left, 76.0, 0.0, 0.0, 20.0, -1.0, 98.0, 161.0, -1.0, -1.0, 93.0, 161.0, 0.87266463, -0.52359879, 0.69813174, -0.69813174, 1.7, 2.0 },

    // address 0x800300C0. Index 1, = D_80030078 + 72.
    { PTR_ANIM_fire_roll_right1, 58.0, 0.0, 0.0, 10.0, -1.0, 77.0, 104.0, -1.0, -1.0, 72.0, 104.0, 0.87266463, -0.34906587, 0.61086529, -0.69813174, 1.55, 1.5 },

    // address 0x80030108. Index 2, = D_80030078 + 144.
    { PTR_ANIM_fire_roll_left_fast, 61.0, 0.0, 0.0, 10.0, -1.0, 83.0, 128.0, -1.0, -1.0, 78.0, 128.0, 0.87266463, -0.52359879, 0.87266463, -0.52359879, 1.2, 1.3 },

    // address 0x80030150. Index 3, = D_80030078 + 216.
    { PTR_ANIM_fire_roll_right2, 63.0, 0.0, 0.0, 10.0, -1.0, 73.0, 114.0, -1.0, -1.0, 68.0, 114.0, 0.87266463, -0.52359879, 0.61086529, -0.61086529, 1.65, 1.5 },

    // address 0x80030198. Index 4, = D_80030078 + 288.
    { PTR_ANIM_fire_roll_left, 76.0, 0.0, 0.0, 20.0, 76.0, 98.0, 161.0, -1.0, -1.0, 93.0, 161.0, 0.87266463, -0.52359879, 0.69813174, -0.69813174, 1.7, 2.0 },

    // address 0x800301E0. Index 5, = D_80030078 + 360.
    { PTR_ANIM_fire_roll_right1, 58.0, 0.0, 0.0, 10.0, 63.0, 77.0, 104.0, -1.0, -1.0, 72.0, 104.0, 0.87266463, -0.34906587, 0.61086529, -0.69813174, 1.55, 1.5 },

    // address 0x80030228. Index 6, = D_80030078 + 432.
    { PTR_ANIM_fire_roll_left_fast, 61.0, 0.0, 0.0, 10.0, 56.0, 83.0, 128.0, -1.0, -1.0, 78.0, 128.0, 0.87266463, -0.52359879, 0.87266463, -0.52359879, 1.2, 1.3 },

    // address 0x80030270. Index 7, = D_80030078 + 504.
    { PTR_ANIM_fire_roll_right2, 63.0, 0.0, 0.0, 10.0, 50.0, 73.0, 114.0, -1.0, -1.0, 68.0, 114.0, 0.87266463, -0.52359879, 0.61086529, -0.61086529, 1.65, 1.5 },

    // address 0x800302B8. Index 8, = D_80030078 + 576.
    { PTR_ANIM_fire_hip_one_handed_weapon_slow, 0.0, 0.0, 0.0, 7.0, 64.0, 19.0, 51.0, 24.0, 46.0, 14.0, 58.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },

    // address 0x80030300. Index 9, = D_80030078 + 648.
    { PTR_ANIM_fire_standing_left_one_handed_weapon_fast, 0.0, 0.0, 1.5707964, 14.0, 76.0, 26.0, 31.0, 31.0, 38.0, 15.0, 49.0, 0.87266463, -0.69813174, 0.52359879, -1.0471976, 0.0, 0.0 },

    // address 0x80030348. Index 10, = D_80030078 + 720.
    { PTR_ANIM_fire_kneel_forward_one_handed_weapon_fast, 26.0, 0.0, 0.0, 25.0, 89.0, 41.0, 63.0, 41.0, 51.0, 21.0, 80.0, 0.87266463, -0.69813174, 0.34906587, -1.134464, 0.0, 0.0 },

    // address 0x80030390. Index 11, = D_80030078 + 792.
    { PTR_ANIM_fire_kneel_left_one_handed_weapon_fast, 18.0, 0.0, 1.5707964, 11.0, 78.0, 33.0, 52.0, 35.0, 45.0, 15.0, 66.0, 0.87266463, -0.087266468, 0.69813174, -0.78539819, 1.5, 1.0 },

    // address 0x800303D8. Index 12, = D_80030078 + 864.
    { PTR_ANIM_fire_standing_dual_wield, 26.0, 0.0, 0.0, 7.0, 92.0, 28.0, 68.0, -1.0, -1.0, 11.0, 73.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },

    // address 0x80030420. Index 13, = D_80030078 + 936.
    { PTR_ANIM_fire_standing_dual_wield_left, 26.0, 0.0, 1.5707964, 9.0, 112.0, 38.0, 87.0, -1.0, -1.0, 19.0, 98.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },

    // address 0x80030468. Index 14, = D_80030078 + 1008.
    { PTR_ANIM_fire_kneel_dual_wield, 22.0, 0.0, 0.0, 10.0, 11.0, 34.0, 87.0, -1.0, -1.0, 17.0, 104.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },

    // address 0x800304B0. Index 15, = D_80030078 + 1080.
    { PTR_ANIM_fire_kneel_dual_wield_left, 28.0, 0.0, 1.5707964, 15.0, 108.0, 34.0, 73.0, -1.0, -1.0, 17.0, 93.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 0.0, 0.0 },

    // address 0x800304F8. Index 16, = D_80030078 + 1152.
    { PTR_ANIM_fire_standing_dual_wield, 26.0, 0.0, 0.0, 7.0, 92.0, 28.0, 68.0, -1.0, -1.0, 11.0, 73.0, 0.87266463, -0.69813174, 0.69813174, -0.69813174, 0.0, 0.0 },

    // address 0x80030540. Index 17, = D_80030078 + 1224.
    { PTR_ANIM_fire_standing_dual_wield_hands_crossed_left, 25.0, 0.0, 1.5707964, 10.0, 112.0, 32.0, 86.0, -1.0, -1.0, 19.0, 97.0, 0.87266463, -0.69813174, 0.43633232, -0.43633232, 0.0, 0.0 },

    // address 0x80030588. Index 18, = D_80030078 + 1296.
    { PTR_ANIM_fire_kneel_dual_wield_hands_crossed, 25.0, 0.0, 0.0, 9.0, 92.0, 33.0, 62.0, -1.0, -1.0, 18.0, 69.0, 0.87266463, -0.69813174, 0.61086529, -0.78539819, 0.0, 0.0 },

    // address 0x800305D0. Index 19, = D_80030078 + 1368.
    { PTR_ANIM_fire_kneel_dual_wield_hands_crossed_left, 19.0, 0.0, 1.5707964, 3.0, 95.0, 30.0, 64.0, -1.0, -1.0, 14.0, 71.0, 0.87266463, -0.69813174, 0.52359879, -0.78539819, 1.5, 1.0 },

    // address 0x80030618. Index 20, = D_80030078 + 1440.
    {0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

struct weapon_firing_animation_table D_80030660[] = {
    // address 0x80030660. Index 0, = D_80030660 + 0.
    { PTR_ANIM_fire_walking, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 1.4, 1.3 },

    // address 0x800306A8. Index 1, = D_80030660 + 72.
    { PTR_ANIM_fire_running, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 1.1, 1.2 },

    // address 0x800306F0. Index 2, = D_80030660 + 144.
    { PTR_ANIM_aim_walking_one_handed_weapon, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030738. Index 3, = D_80030660 + 216.
    { PTR_ANIM_aim_running_one_handed_weapon, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030780. Index 4, = D_80030660 + 288.
    { PTR_ANIM_fire_walking_dual_wield, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x800307C8. Index 5, = D_80030660 + 360.
    { PTR_ANIM_fire_running_dual_wield, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030810. Index 6, = D_80030660 + 432.
    { PTR_ANIM_fire_walking_dual_wield_hands_crossed, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030858. Index 7, = D_80030660 + 504.
    { PTR_ANIM_fire_running_dual_wield_hands_crossed, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x800308A0. Index 8, = D_80030660 + 576.
    { PTR_ANIM_aim_running_left_one_handed_weapon, 0.0, 0.0, 1.5707964, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x800308E8. Index 9, = D_80030660 + 648.
    { PTR_ANIM_aim_running_right_one_handed_weapon, 0.0, 0.0, 4.712389, 0.0, -1.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0, 0.87266463, -0.52359879, 0.52359879, -0.52359879, 0.0, 0.0 },

    // address 0x80030930. Index 10, = D_80030660 + 720.
    {0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

s32 objectiveregisters1 = 0;

/**
 * List of characters updated every tick.
 *
 * Address 0x8003097C.
*/
ChrRecord* g_ActiveChrs = 0;

/**
 * Number of items in g_ActiveChrs.
 *
 * Adress 0x0x80030980.
*/
s32 g_ActiveChrsCount = 0;

/**
 * Default factor in chrlvModelScaleAnimationRelated.
 * Address 0x80030984.
*/
f32 D_80030984 = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by ANIM_DATA_running.
 * Address 0x80030988.
*/
f32 D_80030988 = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by ANIM_DATA_sprinting.
 * Address 0x8003098C.
*/
f32 D_8003098C = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by ANIM_DATA_walking_unarmed.
 * Address 0x80030990.
*/
f32 D_80030990 = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by ANIM_DATA_running_one_handed_weapon.
 * Address 0x80030994.
*/
f32 D_80030994 = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by first ANIM_DATA_sprinting_one_handed_weapon.
 * Address 0x80030998.
*/
f32 D_80030998 = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by ANIM_DATA_walking_female.
 * Address 0x8003099C.
*/
f32 D_8003099C = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by ANIM_DATA_running_female.
 * Address 0x800309A0.
*/
f32 D_800309A0 = 0;

/**
 * Scale factor in chrlvModelScaleAnimationRelated, used by duplicate ANIM_DATA_sprinting_one_handed_weapon.
 * Address 0x800309A4.
*/
f32 D_800309A4 = 0;

point2d D_800309A8 = {0, 0};
//s32 D_800309AC = 0;
point2d D_800309B0 = {0, 0};
//s32 D_800309B4 = 0;

/**
 * Default firing state, left and right hand.
 * Address 0x800309B8.
*/
point2d D_800309B8 = {0, 0};

/**
 * Default firing state when crouched, left and right hand.
 * Address 0x800309C0.
*/
point2d D_800309C0 = {0, 0};

point2d D_800309C8 = {0, 0};
point2d D_800309D0 = {0, 0};
point2d D_800309D8 = {0, 0};

point2d D_800309E0 = {0, 0};

point2d D_800309E8 = {0, 0};


void sub_GAME_7F02083C(enum CHR_RENDER_PART bodypart, Mtxf *matrix);


s32 get_numguards(void)
{
  return g_NumChrSlots;
}

void get_ptr_allocated_block_for_vertices(int param_1)
{
  dynAllocate(param_1 << 4);
}

void set_show_patrols_flag(s32 flag)
{
  show_patrols_flag = flag;
}

s32 get_show_patrols_flag(void)
{
  return show_patrols_flag;
}


/**
 * Unreferenced.
 *
 * Address 0x7F01F574.
 */
f32 chrUnusedYPositionRelated(PropRecord *arg0)
{
    if (arg0->stan != 0)
    {
        if (sub_GAME_7F0B20D0(&arg0->stan, arg0->pos.x, arg0->pos.z, 1.0f) < 0)
        {
            return stanGetPositionYValue(arg0->stan, arg0->pos.x, arg0->pos.z);
        }

        return 0.0f;
    }

    return 0.0f;
}


void chrSetMoving(ChrRecord *self, bool unset)
{
    if (unset)
    {
        self->hidden &= ~CHRHIDDEN_MOVING;
    }
    else
    {
        self->hidden |= CHRHIDDEN_MOVING;
    }
    return;
}


StandTile *sub_GAME_7F01F614(ChrRecord *guard, StandTile *stan, coord3d *src, coord3d *dst, s32 updateLastMoveOk)
{
    StandTile *ret;
    StandTile *tile;
    f32 height;
    f32 always_20;
    f32 width;
    coord3d edgeA;
    coord3d edgeB;
    coord3d delta;
    coord3d dir;
    f32 tmp;
    s32 hasprojection;
    coord3d newpos;

    ret = NULL;
    tile = stan;

    chrGetChrWidthHeight(guard->prop, &width, &height, &always_20);
    chrSetMoving(guard, 0);
    stanResetHits();

    if (stanTestLineUnobstructed(&tile, src->x, src->z, dst->x, dst->z, 0x1f, height, always_20, 0.0f, 1.0f))
    {
        if (stanTestVolume(&tile, dst->x, dst->z, width, 0x1f, height, always_20) < 0)
        {
            if (updateLastMoveOk)
            {
                if (&newpos);
                guard->invalidmove = 0;
                guard->lastmoveok60 = g_GlobalTimer;
            }

            ret = tile;
            goto done;
        }
    }

    hasprojection = 0;

    if (getCollisionEdge_maybe(&edgeA, &edgeB))
    {
        delta.x = dst->x - src->x;
        delta.z = dst->z - src->z;

        if ((edgeA.x != edgeB.x) || (edgeA.z != edgeB.z))
        {
            hasprojection = 1;

            dir.x = edgeB.x - edgeA.x;
            dir.z = edgeB.z - edgeA.z;

            tmp = 1.0f / sqrtf((dir.z * dir.z) + (dir.x * dir.x));

            dir.x *= tmp;
            dir.z *= tmp;

            tmp = (dir.z * delta.z) + (delta.x * dir.x);

            newpos.x = (dir.x * tmp) + src->x;
            newpos.z = (dir.z * tmp) + src->z;

            if (&dir);

            tile = stan;
        }

        if (hasprojection)
        {
            if (stanTestLineUnobstructed(&tile, src->x, src->z, newpos.x, newpos.z, 0x1f, height, always_20, 0.0f, 1.0f))
            {
                if (stanTestVolume(&tile, newpos.x, newpos.z, width, 0x1f, height, always_20) < 0)
                {
                    dst->x = newpos.x;
                    dst->z = newpos.z;
                    guard->invalidmove = 2;
                    ret = tile;
                    goto done;
                }
            }
        }

        tmp = width * width;

        dir.x = edgeA.x - dst->x;
        dir.z = edgeA.z - dst->z;

        if (((dir.z * dir.z) + (dir.x * dir.x)) <= tmp)
        {
            if ((edgeA.x == src->x) && (edgeA.z == src->z))
            {
                goto done;
            }

            dir.x = -(edgeA.z - src->z);
            dir.z = edgeA.x - src->x;

            tmp = 1.0f / sqrtf((dir.z * dir.z) + (dir.x * dir.x));

            dir.x *= tmp;
            dir.z *= tmp;

            tmp = (dir.z * delta.z) + (delta.x * dir.x);

            newpos.x = (dir.x * tmp) + src->x;
            newpos.z = (dir.z * tmp) + src->z;

            tile = stan;

            if (stanTestLineUnobstructed(&tile, src->x, src->z, newpos.x, newpos.z, 0x1f, height, always_20, 0.0f, 1.0f))
            {
                if (stanTestVolume(&tile, newpos.x, newpos.z, width, 0x1f, height, always_20) < 0)
                {
                    dst->x = newpos.x;
                    dst->z = newpos.z;
                    guard->invalidmove = 2;
                    ret = tile;
                    goto done;
                }
            }
        }
        else
        {
            dir.x = edgeB.x - dst->x;
            dir.z = edgeB.z - dst->z;

            if (((dir.z * dir.z) + (dir.x * dir.x)) <= tmp)
            {
                if ((edgeB.x == src->x) && (edgeB.z == src->z))
                {
                    goto done;
                }

                dir.x = -(edgeB.z - src->z);
                dir.z = edgeB.x - src->x;

                tmp = 1.0f / sqrtf((dir.z * dir.z) + (dir.x * dir.x));

                dir.x *= tmp;
                dir.z *= tmp;

                tmp = (dir.z * delta.z) + (delta.x * dir.x);

                newpos.x = (dir.x * tmp) + src->x;
                newpos.z = (dir.z * tmp) + src->z;

                tile = stan;

                if (stanTestLineUnobstructed(&tile, src->x, src->z, newpos.x, newpos.z, 0x1f, height, always_20, 0.0f, 1.0f))
                {
                    if (stanTestVolume(&tile, newpos.x, newpos.z, width, 0x1f, height, always_20) < 0)
                    {
                        dst->x = newpos.x;
                        dst->z = newpos.z;
                        guard->invalidmove = 2;
                        ret = tile;
                    }
                }
            }
        }
    }

done:
    chrSetMoving(guard, 1);

    if (ret == NULL)
    {
        guard->invalidmove = 1;
    }

    return ret;
}


s32 sub_GAME_7F01FC10(Model *model, coord3d *src, coord3d *dst, f32 *ground_y)
{
    ChrRecord *chr;
    s32 moved;
    f32 ground;
    coord3d *groundpos;
    f32 tmp;
    s32 i;
    StandTile *tile;
    union ModelRwData *rwdata;

    chr = model->chr;
    moved = 0;
    ground = 0.0f;
    groundpos = src;

    if (chr->prop->stan != NULL)
    {
        if ((chr->actiontype == ACT_DIE) && (chr->act_die.timeextra > ground))
        {
#ifdef BUGFIX_R1
            tmp = ((model->playspeed * g_JP_GlobalTimerDelta) * (chr->act_die.timeextra - chr->act_die.elapseextra)) / chr->act_die.timeextra;
#else
            tmp = ((model->playspeed * g_GlobalTimerDelta) * (chr->act_die.timeextra - chr->act_die.elapseextra)) / chr->act_die.timeextra;
#endif

            dst->x += chr->act_die.extraspeed.x * tmp;
            dst->y += chr->act_die.extraspeed.y * tmp;
            dst->z += chr->act_die.extraspeed.z * tmp;

#ifdef BUGFIX_R1
            chr->act_die.elapseextra += g_JP_GlobalTimerDelta * model->playspeed;
#else
            chr->act_die.elapseextra += g_GlobalTimerDelta * model->playspeed;
#endif

            if (chr->act_die.timeextra < chr->act_die.elapseextra)
            {
                chr->act_die.timeextra = 0.0f;
            }
        }

        dst->x += chr->fallspeed.x * g_GlobalTimerDelta;
        dst->z += chr->fallspeed.z * g_GlobalTimerDelta;

        tile = sub_GAME_7F01F614(chr, chr->prop->stan, src, dst, 1);

        if (tile != NULL)
        {
            chr->prop->stan = tile;
            groundpos = dst;
            moved = 1;
        }

        if (!(chr->chrflags & CHRFLAG_LOCK_Y_POS))
        {
            ground = stanGetPositionYValue(chr->prop->stan, groundpos->x, groundpos->z);
            chr->ground = ground;

            if (chr->chrflags & CHRFLAG_INIT)
            {
                rwdata = modelGetNodeRwData(model, model->obj->RootNode);

                chr->chrflags &= ~1;
                chr->manground = chr->ground;
                chr->sumground = chr->ground / GROUND_SMOOTH_FACTOR;

                rwdata->Header.unk34.y = rwdata->Header.unk24.y;
            }
            else
            {
                if ((chr->fallspeed.y != 0.0f) || (chr->manground > chr->ground))
                {
                    sub_GAME_7F057D44(&chr->manground, &chr->fallspeed.y, g_GlobalTimerDelta);

                    if (chr->manground <= chr->ground)
                    {
                        chr->manground = chr->ground;
                        chr->sumground = chr->ground / GROUND_SMOOTH_FACTOR;
                        chr->fallspeed.y = 0.0f;
                    }
                }

                if (chr->manground <= chr->ground)
                {
                    i = 0;

                    if (g_ClockTimer > 0)
                    {
                        tmp = FALLSPEED_DECAY;

                        do
                        {
                            chr->sumground = (chr->sumground * tmp) + chr->ground;
                            chr->fallspeed.x *= tmp;
                            chr->fallspeed.z *= tmp;
                            i++;
                        }
                        while (i < g_ClockTimer);
                    }

                    tmp = 0.1f;
                    chr->manground = chr->sumground * GROUND_SMOOTH_FACTOR;

                    if (chr->fallspeed.x < tmp)
                    {
                        if (-0.1f < chr->fallspeed.x)
                        {
                            if (chr->fallspeed.z < tmp)
                            {
                                if (-0.1f < chr->fallspeed.z)
                                {
                                    chr->fallspeed.z = 0.0f;
                                    chr->fallspeed.x = 0.0f;
                                }
                            }
                        }
                    }
                }
            }

            dst->y += chr->manground - ground;
        }
        else
        {
            ground = chr->ground;
        }
    }

    *ground_y = ground;

    if (!moved)
    {
        dst->x = src->x;
        dst->z = src->z;
    }

    return 1;
}

#undef GROUND_SMOOTH_FACTOR
#undef FALLSPEED_DECAY


s32 chrGetNumFree(void)
{
    s32 count = 0;
    s32 i;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model == 0)
        {
            count++;
        }
    }

    return count;
}


f32  get_007_health_mod(void);

void chrSetMaxDamage(ChrRecord *chr, f32 maxdamage)
{
    chr->maxdamage = (get_007_health_mod() * maxdamage);
}

f32 chrGetMaxDamage(ChrRecord *chr)
{
    return chr->maxdamage;
}

void chrAddHealth(ChrRecord *chr, f32 health)
{
    chr->damage -= (health * get_007_health_mod());
}

f32 chrGetArmor(ChrRecord *chr)
{
    if (chr->damage < 0)
    {
        return -chr->damage;
    }

    return 0;
}

PropRecord *init_GUARDdata_with_set_values(PropRecord *arg0, Model *arg1, struct coord3d *arg2, f32 arg3, StandTile *arg4, struct AIListRecord *arg5)
{
    ChrRecord *var_s0;
    s32 var_v0;

    var_s0 = NULL;
    var_v0 = 0;

    for (var_v0 = 0; var_v0 < g_NumChrSlots; var_v0++)
    {
        if (g_ChrSlots[var_v0].model == NULL)
        {
            var_s0 = &g_ChrSlots[var_v0];
            break;
        }
    }
    
    arg0->type = PROP_TYPE_CHR;
    arg0->chr = var_s0;
    arg0->pos.f[0] = arg2->f[0];
    arg0->pos.f[1] = arg2->f[1];
    arg0->pos.f[2] = arg2->f[2];
    arg0->stan = arg4;
    
    sub_GAME_7F06FF5C(arg1, (s32) sub_GAME_7F01FC10);
    
    arg1->unk00 = 0xA;
    arg1->chr = var_s0;
    
    setsuboffset(arg1, arg2);
    setsubroty(arg1, arg3);

    #if defined(VERSION_EU)
    modelSetAnimPlaySpeed(arg1, animation_rate * 1.2f, 0.0f);
    #else
    modelSetAnimPlaySpeed(arg1, animation_rate, 0.0f);
    #endif
    
    var_s0->chrnum = (s16) player1_guardID;
    player1_guardID += 1;
    var_s0->headnum = 0;
    var_s0->bodynum = 0;
    var_s0->prop = arg0;
    var_s0->model = arg1;
    var_s0->field_20 = NULL;
    var_s0->numarghs = 0;
    var_s0->lastwalk60 = 0;
    var_s0->invalidmove = 0;
    var_s0->lastmoveok60 = g_GlobalTimer;
    var_s0->lastseetarget60 = 0;
    var_s0->lastknowntargetpos.f[0] = 0.0f;
    var_s0->lastknowntargetpos.f[1] = 0.0f;
    var_s0->lastknowntargetpos.f[2] = 0.0f;
    var_s0->targetTile = NULL;
    var_s0->seen_bond_time = 0;
    var_s0->lastheartarget60 = 0;
    var_s0->numclosearghs = 0;
    var_s0->shotbondsum = 0.0f;
    var_s0->damage = 0.0f;
    var_s0->visionrange = 250.0f;
    var_s0->hearingscale = 1.0f;
    
    var_s0->maxdamage = get_007_health_mod() * 4.0f;
    set_color_shading_from_tile(arg0, &var_s0->nextcol);
    
    var_s0->shadecol.rgba[0] = var_s0->nextcol.rgba[0];
    var_s0->shadecol.rgba[1] = var_s0->nextcol.rgba[1];
    var_s0->shadecol.rgba[2] = var_s0->nextcol.rgba[2];
    var_s0->shadecol.rgba[3] = var_s0->nextcol.rgba[3];
    var_s0->fadealpha = 0xFF;
    var_s0->field_160[0].ptr_SEbuffer1 = NULL;
    var_s0->field_160[0].ptr_SEbuffer2 = NULL;
    var_s0->field_160[1].ptr_SEbuffer1 = NULL;
    var_s0->field_160[1].ptr_SEbuffer2 = NULL;
    var_s0->field_178[0] = 0;
    var_s0->field_178[1] = 0;
    var_s0->chrflags = CHRFLAG_INIT;
    var_s0->hidden = CHRHIDDEN_NONE;
    var_s0->sumground = 0.0f;
    var_s0->manground = 0.0f;
    var_s0->ground = 0.0f;
    var_s0->fallspeed.f[0] = 0.0f;
    var_s0->fallspeed.f[1] = 0.0f;
    var_s0->fallspeed.f[2] = 0.0f;
    var_s0->prevpos.f[0] = arg2->f[0];
    var_s0->prevpos.f[1] = arg2->f[1];
    var_s0->prevpos.f[2] = arg2->f[2];
    var_s0->actiontype = 0;
    var_s0->sleep = 0;
    var_s0->ailist = (AIRecord *) arg5;
    var_s0->aioffset = 0;
    var_s0->aireturnlist = -1;
    var_s0->morale = 0;
    var_s0->alertness = 0;
    var_s0->flags2 = 0;
    var_s0->random = 0;
    var_s0->timer60 = 0;
    var_s0->padpreset1 = -1;
    var_s0->chrseeshot = -1;
    var_s0->chrseedie = -1;
    var_s0->chrpreset1 = -1;
    var_s0->beams[0].unk00 = -1;
    var_s0->beams[1].unk00 = -1;
    var_s0->firecount[0] = 0;
    var_s0->firecount[1] = 0;
    var_s0->grenadeprob = 0;
    var_s0->accuracyrating = 0;
    var_s0->speedrating = 0;
    var_s0->arghrating = 0;
    var_s0->flinchcnt = -1;
    var_s0->aimuplshoulder = 0.0f;
    var_s0->aimuprshoulder = 0.0f;
    var_s0->aimupback = 0.0f;
    var_s0->aimsideback = 0.0f;
    var_s0->aimendlshoulder = 0.0f;
    var_s0->aimendrshoulder = 0.0f;
    var_s0->aimendback = 0.0f;
    var_s0->aimendsideback = 0.0f;
    var_s0->aimendcount = 0;
    var_s0->weapons_held[0] = NULL;
    var_s0->weapons_held[1] = NULL;
    var_s0->handle_positiondata_hat = NULL;
    var_s0->chrwidth = 20.0f;
    var_s0->chrheight = 185.0f;
    
    sub_GAME_7F01FC10(arg1, &arg0->pos, &arg0->pos, &var_s0->ground);
    chrPositionRelated7F020D94(var_s0);
    
    return arg0;
}




/**
 * Address 0x7F0203B8.
 */
PropRecord * chrAllocate( Model * arg0, coord3d * arg1, f32 arg2,  StandTile * arg3, s32 arg4)
{
    PropRecord * ret;
    s32 phi_a0;

    ret = chrpropAllocate();

    if (ret != 0)
    {
        ret = init_GUARDdata_with_set_values(ret, arg0, arg1, arg2, arg3, arg4);
    }

    return ret;
}


/**
 * Address: 7F020414.
 */
void chrpropCleanupForRemoval(PropRecord *prop)
{
    ChrRecord *chr;
    Model *model;
    PropRecord *child;
    PropRecord *prev;
    ObjectRecord *obj;
    
    chr = prop->chr;
    model = chr->model;
    
    if (chr->field_160[0].ptr_SEbuffer1 != NULL && sndGetPlayingState(chr->field_160[0].ptr_SEbuffer1) != 0) {
        sndDeactivate(chr->field_160[0].ptr_SEbuffer1);
    }
    if (chr->field_160[0].ptr_SEbuffer2 != NULL && sndGetPlayingState(chr->field_160[0].ptr_SEbuffer2) != 0) {
        sndDeactivate(chr->field_160[0].ptr_SEbuffer2);
    }
    if (chr->field_160[1].ptr_SEbuffer1 != NULL && sndGetPlayingState(chr->field_160[1].ptr_SEbuffer1) != 0) {
        sndDeactivate(chr->field_160[1].ptr_SEbuffer1);
    }
    if (chr->field_160[1].ptr_SEbuffer2 != NULL && sndGetPlayingState(chr->field_160[1].ptr_SEbuffer2) != 0) {
        sndDeactivate(chr->field_160[1].ptr_SEbuffer2);
    }
    
    sub_GAME_7F050DE8(model);
    chrpropDeregisterRooms(prop);
    
    child = prop->child;
    
    if (child != NULL) {
        do {
            obj = (ObjectRecord *)child->obj;
            prev = child->prev;
            objDetach(child);
            objFreePermanently(obj, TRUE);
            child = prev;
        } while (child != NULL);
    }
    
    clear_aircraft_model_obj(model);
    
    chr->model = NULL;
    chr->chrnum = -1;
    
    if (chr->field_20 != NULL) {
        sub_GAME_7F06B248(chr->field_20);
    }
}


/**
 * Address 0x7F020540 (VERSION_US, VERSION_JP).
 * Address 0x7F0203B4 (VERSION_EU).
 */
void setAnimationRate(f32 arg0)
{
    s32 i;

    animation_rate = arg0;

    for (i=0; i<g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL)
        {
#if defined(REFRESH_PAL)
/* should reference D_80047E4C (1.2f) */
            modelSetAnimPlaySpeed(g_ChrSlots[i].model, animation_rate * 1.2f, 600.0f);
#else
            modelSetAnimPlaySpeed(g_ChrSlots[i].model, animation_rate, 600.0f);
#endif
        }
    }
}




f32 getAnimationRate(void)
{
  return animation_rate;
}



/**
 * Address 0x7F0205F0 (all versions).
 */
void chrUpdateAimProperties( ChrRecord *self)
{
    f32 mult;

    if (self->aimendcount >= 2)
    {
#if defined(BUGFIX_R1)
        mult = g_JP_GlobalTimerDelta / (f32) self->aimendcount;
#else
        mult = g_GlobalTimerDelta / (f32) self->aimendcount;
#endif

        if (mult > 1.0f)
        {
            mult = 1.0f;
        }

        self->aimuplshoulder += ((self->aimendlshoulder - self->aimuplshoulder) * mult);
        self->aimuprshoulder += ((self->aimendrshoulder - self->aimuprshoulder) * mult);
        self->aimupback += ((self->aimendback - self->aimupback) * mult);
        self->aimsideback += ((self->aimendsideback - self->aimsideback) * mult);
        self->aimendcount -= g_ClockTimer;

        return;
    }

    self->aimuplshoulder = self->aimendlshoulder;
    self->aimuprshoulder = self->aimendrshoulder;
    self->aimupback = self->aimendback;
    self->aimsideback = self->aimendsideback;
}



/**
 * Address 0x7F0206D4.
 */
void chrSetHiddenToRandom(ChrRecord *self)
{
    ChrRecord *temp_a0;
    u32 rand;

    if ((s32) self->flinchcnt < 0)
    {
        self->flinchcnt = 1;
        self->hidden &= ~CHRHIDDEN_RAND_FLINCH_MASK;

        // roll for bits 12,13.
        // rand -> value
        // 2 => nothing
        // 0 => set bit 12
        // 1 => set bit 13
        rand = randomGetNext() % 3U;

        if (rand == 0)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_1;
        }
        else if (rand == 1)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_2;
        }

        // roll for bits 14,15.
        // rand -> value
        // 2 => nothing
        // 0 => set bit 14
        // 1 => set bit 15
        rand = randomGetNext() % 3U;

        if (rand == 0)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_4;
        }
        else if (rand == 1)
        {
            self->hidden |= CHRHIDDEN_RAND_FLINCH_8;
        }
    }
}

 
f32 sub_GAME_7F020794(ChrRecord *arg0)
{
    f32 temp_f2;
    f32 phi_f2;
#if defined(LEFTOVERDEBUG)
    f32 rise = 10.0f;
    f32 fall = 20.0f;
#else
    f32 rise = 8.0f;
    f32 fall = 16.0f;
#endif
 
    phi_f2 = arg0->flinchcnt;
    temp_f2 = (f32) phi_f2;
 
    if (temp_f2 < rise)
    {
        phi_f2 = sinf(((temp_f2 * M_TAU_F) * 0.25f) / rise);
    }
    else
    {
        phi_f2 = 1.0f - sinf((((temp_f2 - rise) * M_TAU_F) * 0.25f) / fall);
    }
 
    return phi_f2;
}


#ifdef BUGFIX_R1
bool chrCanUseDKModeScaling(s32 bodynum, s32 headnum)
{
    if (j_text_trigger == 0)
    {
        return 1;
    }

    if ((bodynum != BODY_Boris) &&
        (bodynum != BODY_Ourumov) &&
        (bodynum != BODY_Trevelyan_Janus) &&
        (bodynum != BODY_Trevelyan_006) &&
        (bodynum != BODY_Valentin_) &&
        (bodynum != BODY_Xenia) &&
        (bodynum != BODY_Baron_Samedi) &&
        (bodynum != BODY_Jaws) &&
        (bodynum != BODY_Mayday) &&
        (bodynum != BODY_Oddjob) &&
        (bodynum != BODY_Natalya_Skirt) &&
        (bodynum != BODY_Natalya_Jungle_Fatigues) &&

        (headnum != BODY_Male_Pierce_Bond_1) &&
        (headnum != BODY_Male_Pierce_Bond_2) &&
        (headnum != BODY_Male_Pierce_Bond_3) &&
        (headnum != BODY_Male_Pierce_Bond_Parka) &&
        (headnum != BODY_Male_Pierce_Bond_Tuxedo) &&
        (headnum != BODY_Male_Mishkin))
    {
        return 1;
    }

    return 0;
}
#endif


#define PI_OVER_3 1.0471976f
#define FIVEPI_OVER_18 0.87266463f


/**
 * Address: 7F02083C
 */
void sub_GAME_7F02083C(enum CHR_RENDER_PART bodypart, Mtxf *matrix)
{
    f32 scale; 
    f32 xrot; 
    f32 yrot; 
    f32 zrot; 
    f32 tmp; 
    f32 amount;
    f32 saved38; 
    f32 saved34; 
    f32 saved30; 
    f32 sideback;
    Mtxf sp30; 
    u16 hidden; 
    ChrRecord *chr;

    scale = 1.0f;

#ifdef BUGFIX_R1
    if (cheatIsActive(CHEAT_DK_MODE)) 
    {
        chr = (ChrRecord *) dword_CODE_bss_80069B60;

        if (chrCanUseDKModeScaling(chr->bodynum, chr->headnum)) 
        {
            if (bodypart == CHR_RENDERPART_HEAD) 
            { 
                scale = 4.0f;
            }
            else if ((bodypart == CHR_RENDERPART_LEFT_ARM) || (bodypart == CHR_RENDERPART_RIGHT_ARM)) 
            {
                if (!(((ChrRecord *) dword_CODE_bss_80069B60)->chrflags & CHRFLAG_08000000)) 
                { 
                    scale = 2.5f; 
                }
            }
        }
    }
#else
    if (cheatIsActive(CHEAT_DK_MODE)) 
    {
        if (bodypart == CHR_RENDERPART_HEAD) 
        { 
            scale = 4.0f; 
        }
        else if ((bodypart == CHR_RENDERPART_LEFT_ARM) || (bodypart == CHR_RENDERPART_RIGHT_ARM)) 
        { 
            scale = 2.5f; 
        }
    }
#endif
    if ((((bodypart != CHR_RENDERPART_LEFT_ARM) && (bodypart != CHR_RENDERPART_RIGHT_ARM)) && (bodypart != CHR_RENDERPART_TORSO)) && (bodypart != CHR_RENDERPART_HEAD)) 
    { 
        return; 
    }

    zrot = (yrot = (xrot = 0.0f));

#ifdef BUGFIX_R1
    chr = (ChrRecord *) dword_CODE_bss_80069B60;
#endif
    if (bodypart == CHR_RENDERPART_RIGHT_ARM) 
    {
#ifdef BUGFIX_R1
        xrot = chr->aimuprshoulder;
#else
        xrot = ((ChrRecord *) dword_CODE_bss_80069B60)->aimuprshoulder;
#endif
    } 
    else if (bodypart == CHR_RENDERPART_LEFT_ARM) 
    {
#ifdef BUGFIX_R1
        xrot = chr->aimuplshoulder;
#else
        xrot = ((ChrRecord *) dword_CODE_bss_80069B60)->aimuplshoulder;
#endif
    } 
    else if (bodypart == CHR_RENDERPART_TORSO) 
    {
#ifndef BUGFIX_R1
        chr = (ChrRecord *) dword_CODE_bss_80069B60;
#endif
        xrot = chr->aimupback;
        if (chr->hidden & CHRHIDDEN_0400) 
        {
            if (PI_OVER_3 < xrot) 
            { 
                xrot -= PI_OVER_3; 
            }
            else if (-FIVEPI_OVER_18 > xrot) 
            { 
                xrot += FIVEPI_OVER_18; 
            }
            else 
            { 
                xrot = 0.0f; 
            }
        }

        yrot = chr->aimsideback;

    } 
    else if (bodypart == CHR_RENDERPART_HEAD) 
    {
#ifndef BUGFIX_R1
        chr = (ChrRecord *) dword_CODE_bss_80069B60;
#endif
        if (chr->hidden & CHRHIDDEN_0400) 
        {
            xrot = chr->aimupback;

            if (chr->hidden & CHRHIDDEN_0400) 
            {
                if (PI_OVER_3 < xrot) 
                { 
                    xrot = PI_OVER_3; 
                }
                else if (xrot < -FIVEPI_OVER_18) 
                { 
                    xrot = -FIVEPI_OVER_18; 
                }
            }
        }
        else if (chr->model->gunhand != GUNRIGHT) 
        { 
            xrot = chr->aimuplshoulder; 
        }
        else 
        { 
            xrot = chr->aimuprshoulder; 
        }
    }

    chr = (ChrRecord *) dword_CODE_bss_80069B60;

    if (chr->flinchcnt >= 0) 
    {
        if (0);

        if ((bodypart == CHR_RENDERPART_RIGHT_ARM) || (bodypart == CHR_RENDERPART_LEFT_ARM)) 
        {
            amount = sub_GAME_7F020794(chr) * M_TAU_F * 15.0f / 360.0f;
            chr = (ChrRecord *) dword_CODE_bss_80069B60;
            hidden = chr->hidden;
            xrot -= amount;

            if (hidden & CHRHIDDEN_RAND_FLINCH_1) 
            { 
                yrot -= amount; 
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_2) 
            { 
                yrot += amount; 
            }
        } 
        else if (bodypart == CHR_RENDERPART_TORSO) 
        {
            tmp = sub_GAME_7F020794(chr);
            tmp *= M_TAU_F;
            amount = tmp * 15.0f;
            chr = (ChrRecord *) dword_CODE_bss_80069B60;
            hidden = chr->hidden;
            amount /= 360.0f;
            xrot += amount;

            if (hidden & CHRHIDDEN_RAND_FLINCH_1) 
            { 
                yrot += amount; 
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_2) 
            { 
                yrot -= amount; 
            }

            if (hidden & CHRHIDDEN_RAND_FLINCH_4) 
            {
                 zrot += (tmp * 10.0f) / 360.0f; 
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_8) 
            { 
                zrot -= (tmp * 10.0f) / 360.0f; 
            }
        }
    }

    if ((((xrot == 0.0f) && (yrot == 0.0f)) && (zrot == 0.0f)) && (scale == 1.0f)) 
    { 
        return; 
    }

    sideback = chrlvGetSubrotySideback(chr);

    if (xrot < 0.0f) 
    { 
        xrot = -xrot;
    } 
    else 
    { 
        xrot = M_TAU_F - xrot; 
    }

    if (yrot < 0.0f) 
    { 
        yrot += M_TAU_F; 
    }

    matrix_4x4_multiply_homogeneous_in_place(currentPlayerGetViewToWorldMtxf(), matrix);

    saved30 = matrix->m[3][0]; 
    saved34 = matrix->m[3][1]; 
    saved38 = matrix->m[3][2];

    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f; 
    matrix->m[3][2] = 0.0f; 

    if ((xrot != 0.0f) || (zrot != 0.0f)) 
    {
        yrot -= sideback;

        if (yrot < 0.0f)
        { 
            yrot += M_TAU_F; 
        }

        matrix_4x4_set_rotation_around_y(yrot, &sp30);
        matrix_4x4_multiply_homogeneous_in_place(&sp30, matrix);

        if (xrot != 0.0f) 
        { 
            matrix_4x4_set_rotation_around_x(xrot, &sp30); 
            matrix_4x4_multiply_homogeneous_in_place(&sp30, matrix); 
        }

        if (zrot != 0.0f) 
        { 
            matrix_4x4_set_rotation_around_z(zrot, &sp30); 
            matrix_4x4_multiply_homogeneous_in_place(&sp30, matrix); 
        }

        matrix_4x4_set_rotation_around_y(sideback, &sp30);
        matrix_4x4_multiply_homogeneous_in_place(&sp30, matrix);
    } 
    else 
    {
        matrix_4x4_set_rotation_around_y(yrot, &sp30);
        matrix_4x4_multiply_homogeneous_in_place(&sp30, matrix);
    }

    if (scale != 1.0f) 
    { 
        matrix_scalar_multiply(scale, (f32 *) matrix); 
    }

    matrix->m[3][0] = saved30; 
    matrix->m[3][1] = saved34; 
    matrix->m[3][2] = saved38;

    matrix_4x4_multiply_homogeneous_in_place(camGetWorldToScreenMtxf(), matrix);
}


#undef PI_OVER_3
#undef FIVEPI_OVER_18


/**
 * Address 0x7F020D94.
 */
void chrPositionRelated7F020D94(ChrRecord *self)
{
    PropRecord *myprop;
    coord3d     lowerbounds;
    coord3d     upperbounds;

    myprop        = self->prop;
    lowerbounds.x = myprop->pos.x - 50.0f;
    lowerbounds.y = self->ground - 1.0f;
    lowerbounds.z = myprop->pos.z - 50.0f;
    upperbounds.x = myprop->pos.x + 50.0f;
    upperbounds.y = myprop->pos.y + 100.0f;
    upperbounds.z = myprop->pos.z + 50.0f;
    chrpropDeregisterRooms(myprop);
    chrpropUpdateRoomList(myprop, &lowerbounds, &upperbounds, 50.0f);
    chrpropRegisterRooms(myprop);
}


/**
 * Address 0x7F020E40.
 */
void chrPositionRelated7F020E40(ChrRecord *chr, s32 arg1)
{
    Model *model;
    PropRecord* prop;

    model = chr->model;
    prop = chr->prop;

    if (!(chr->hidden & CHRHIDDEN_FREEZE))
    {
        getsuboffset(model, &chr->prevpos);
        modelTickAnim(model, arg1, 1);
        subcalcpos(model);
        set_color_shading_from_tile(prop, &chr->nextcol);
        getsuboffset(model, &prop->pos);
        chrPositionRelated7F020D94(chr);

        return;
    }

    subcalcpos(model);
    getsuboffset(model, &prop->pos);
}


#ifdef NONMATCHING
/*
* Address: 
*   US: 0x7F020EF0
*   JP: 0x7F021188
*   EU: 0x7F020E68
*/
void chrTickBeams(void) {
    #ifdef DEBUG
    osSyncPrintf("anim=%d frame=%f backy=%f\n", DAT_83bcf9d8, *(local_e8 + 0x28), (*(psVar2 + 0xa6) * 360.0) / 6.283185);
    #endif
    #ifdef DEBUG
    osSyncPrintf("\nVISIBLE MAGIC MODE!!!!\n\n");
    #endif
}
#else

#if defined(VERSION_US) || defined(VERSION_JP) 

extern void *D_80036090;
extern f32 D_80051D58;
extern f32 D_80051D5C;
extern f32 D_80051D60;
extern f32 D_80051D64;
extern f32 D_80051D68;
extern f32 D_80051D6C;
extern f32 D_80051D70;
extern f32 D_80051D74;
extern f32 D_80051D78;
extern f32 D_80051D7C;
extern f32 D_80051D80;
extern f32 D_80051D84;
extern f32 D_80051D88;

#ifdef VERSION_JP
extern f32 D_jp80051D88;
#endif

s32 chrTickBeams(PropRecord *prop)
{
    ModelRenderData renderdata;
    ChrRecord *chr;
    Model *model;
    s32 headSwitchVisible;
    s32 headVisible;
    s32 tickamount;

    renderdata = D_8002CC6C;
    chr = prop->chr;
    model = chr->model;
    headVisible = 1;
    tickamount = g_ClockTimer;

    if ((!(chr->chrflags & CHRFLAG_HIDDEN)) || (chr->chrflags & CHRFLAG_00040000))
    {
        if (D_8002C904)
        {
            if (((ModelAnimation *)animation_table_ptrs1[g_AnimationTablePointerCountRelated]) != ((ModelAnimation *)1))
            {
                if (objecthandlerGetModelAnim(model) != ((ModelAnimation *)animation_table_ptrs1[g_AnimationTablePointerCountRelated]))
                {
                    modelSetAnimation(model, (ModelAnimation *)animation_table_ptrs1[g_AnimationTablePointerCountRelated], 0, 0.0f, 0.5f, 0.0f);
                }
            }
        }
        else
        {
            chrlvActionTick(chr);

            if (chr->model == NULL)
            {
                return 1;
            }
        }

        if (D_8002C90C)
        {
            tickamount = 0;

            if (D_8002C910)
            {
                tickamount = 1;
            }
        }
    }

    if (chr->hidden & CHRHIDDEN_REMOVE)
    {
        chrpropCleanupForRemoval(prop);
        return 1;
    }

    if (chr->weapons_held[GUNRIGHT] != NULL)
    {
        if (chr->weapons_held[GUNRIGHT]->obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
        {
            objFreePermanently(chr->weapons_held[GUNRIGHT]->obj, 1);
        }
    }

    if (chr->weapons_held[GUNLEFT] != NULL)
    {
        if (chr->weapons_held[GUNLEFT]->obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)
        {
            objFreePermanently(chr->weapons_held[GUNLEFT]->obj, 1);
        }
    }

    if (chr->chrflags & CHRFLAG_HIDDEN)
    {
        headSwitchVisible = 0;
    }
    else
    {
        if (((prop->type == 6) && (g_playerPointers[getPlayerPointerIndex(prop)]->cameramode == 1)) || (chr->chrflags & CHRFLAG_CULL_USING_HITBOX))
        {
            headSwitchVisible = 1;

            if (((chr->actiontype == ACT_ANIM) && (chr->act_anim.unk02c == 0)) && (chr->act_anim.noTranslate != 0))
            {
                modelTickAnim(model, tickamount, 0);
            }
            else
            {
                chrPositionRelated7F020E40(chr, tickamount);
            }

            goto after_position_update;
        }

        if ((chr->actiontype == ACT_PATROL) || (chr->actiontype == ACT_GOPOS))
        {
            if (((chr->actiontype == ACT_PATROL) && (chr->act_patrol.waydata.mode == WAYMODE_MAGIC)) || ((chr->actiontype == ACT_GOPOS) && (chr->act_gopos.waydata.mode == WAYMODE_MAGIC)))
            {
                headSwitchVisible = sub_GAME_7F054D6C(prop, &prop->pos, getinstsize(model), 1);

                if (headSwitchVisible)
                {
                    getsuboffset(model, &chr->prevpos);
                    subcalcpos(model);
                    set_color_shading_from_tile(prop, &chr->nextcol);
                    getsuboffset(model, &prop->pos);
                    chrPositionRelated7F020D94(chr);
                }
            }
            else
            {
                chrPositionRelated7F020E40(chr, tickamount);
                headSwitchVisible = sub_GAME_7F054D6C(prop, &prop->pos, getinstsize(model), 1);

                if (headSwitchVisible)
                {
                    if (chr->actiontype == ACT_PATROL)
                    {
                        chr->act_patrol.lastvisible60 = g_GlobalTimer;
                    }
                    else if (chr->actiontype == ACT_GOPOS)
                    {
                        chr->act_gopos.unk9c = g_GlobalTimer;
                    }
                }
            }
        }
        else if ((chr->actiontype == ACT_ANIM) && (chr->act_anim.unk02c == 0))
        {
            headSwitchVisible = sub_GAME_7F054D6C(prop, &prop->pos, getinstsize(model), 1);

            if (headSwitchVisible && (chr->act_anim.noTranslate == 0))
            {
                chrPositionRelated7F020E40(chr, tickamount);
            }
            else
            {
                modelTickAnim(model, tickamount, 0);
            }
        }
        else if (chr->actiontype == ACT_STAND)
        {
            headSwitchVisible = sub_GAME_7F054D6C(prop, &prop->pos, getinstsize(model), 1);

            if (headSwitchVisible || (chr->chrflags & CHRFLAG_INIT))
            {
                chrPositionRelated7F020E40(chr, tickamount);
            }
            else if (model->anim2 != NULL)
            {
                modelTickAnim(model, tickamount, 0);
            }
        }
        else
        {
            if (chr->chrflags & CHRFLAG_IGNORE_ANIM_TRANSLATION)
            {
                modelTickAnim(model, tickamount, 0);
            }
            else
            {
                chrPositionRelated7F020E40(chr, tickamount);
            }

            headSwitchVisible = sub_GAME_7F054D6C(prop, &prop->pos, getinstsize(model), 1);
        }
    }

after_position_update:
    if (((chr->actiontype != ACT_STAND) || (model->anim2 != NULL)) || (prop->type == PROP_TYPE_VIEWER))
    {
        chr->hidden |= CHRHIDDEN_BACKGROUND_AI;
    }

    chrUpdateAimProperties(chr);

    if (chr->field_20 != NULL)
    {
        sub_GAME_7F06B248(chr->field_20);
        chr->field_20 = NULL;
    }

    if (headSwitchVisible)
    {
        if (get_debug_chrnum_flag()) {}

        prop->flags |= PROPFLAG_ONSCREEN;
        chr->chrflags |= CHRFLAG_HAS_BEEN_ON_SCREEN;

#ifdef BUGFIX_R1
    if (cheatIsActive(12))
    {
        if (chrCanUseDKModeScaling(chr->bodynum, chr->headnum))
        {
            modelSetDistanceScale(0.3125f);

            if (chr->chrflags & CHRFLAG_10000000)
            {
                chr->chrflags &= ~CHRFLAG_10000000;
                modelSetScale(chr->model, chr->model->scale / D_jp80051D88);
            }
        }
    }
#else
        if (cheatIsActive(CHEAT_DK_MODE))
        {
            modelSetDistanceScale(0.3125f);
        }
#endif

        D_80036090 = sub_GAME_7F02083C;
        dword_CODE_bss_80069B60 = (u32)chr;

        renderdata.unk_matrix = camGetWorldToScreenMtxf();
        renderdata.mtxlist = dynAllocate(model->obj->numMatrices * (sizeof(Mtxf)));

        if (((ChrRecord *)dword_CODE_bss_80069B60)->flinchcnt >= 0)
        {
            ((ChrRecord *)dword_CODE_bss_80069B60)->flinchcnt += g_ClockTimer;

            if (((ChrRecord *)dword_CODE_bss_80069B60)->flinchcnt >= 30)
            {
                ((ChrRecord *)dword_CODE_bss_80069B60)->flinchcnt = -1;
            }
        }

        subcalcmatrices(&renderdata, model);

        D_80036090 = NULL;
        modelSetDistanceScale(1.0f);

        update_color_shading(&chr->shadecol, &chr->nextcol);

        prop->zDepth = sub_GAME_7F06C768(model);

        chr->field_20 = sub_GAME_7F06B120(NULL, model);

        chrRenderHeldWeapon(prop, GUNRIGHT, (Gfx **)(&chr->field_20));
        chrRenderHeldWeapon(prop, GUNLEFT, (Gfx **)(&chr->field_20));

        if (chr->handle_positiondata_hat != NULL)
        {
            ObjectRecord *hatobj;
            Model *hatmodel;

            hatobj = chr->handle_positiondata_hat->obj;
            hatmodel = hatobj->model;

            chr->handle_positiondata_hat->flags |= PROPFLAG_ONSCREEN;

            renderdata.unk_matrix = modelFindNodeMtx(model, hatmodel->attachedto_objinst, 0);
            renderdata.mtxlist = dynAllocate(hatmodel->obj->numMatrices * (sizeof(Mtxf)));

            instcalcmatrices(&renderdata, hatmodel);

            if ((chr->headnum >= 0x2a) && (chr->headnum < 0x46))
            {
                coord3d pos;
                f32 xscale;
                f32 yscale;
                f32 zscale;
                Mtxf mtx;
                Mtxf tmp;
                HATTYPE hat;
                s32 unusedv;
                struct headHat *entry;
                volatile s32 changed;
                s32 headindex;

                pos = D_8002CCAC;

                hat = get_hat_model(chr->handle_positiondata_hat);

                headindex = chr->headnum - 0x2a;
                entry = (struct headHat *)((((u8 *)headHat_array_8003E464) + (headindex * 0x90)) + (hat * 0x18));

                if (!get_debug_render_raster())
                {
                    changed = 0;

                    if (joyGetButtons(PLAYER_1, L_TRIG))
                    {
                        if (joyGetButtons(PLAYER_1, A_BUTTON))
                        {
                            entry->zoffset -= D_80051D58;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, B_BUTTON))
                        {
                            entry->zoffset += D_80051D5C;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, D_CBUTTONS))
                        {
                            entry->yoffset -= D_80051D60;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, U_CBUTTONS))
                        {
                            entry->yoffset += D_80051D64;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, L_CBUTTONS))
                        {
                            entry->xoffset -= D_80051D68;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, R_CBUTTONS))
                        {
                            entry->xoffset += D_80051D6C;
                            changed = 1;
                        }
                    }
                    
                    if (joyGetButtons(PLAYER_1, R_TRIG))
                    {
                        if (joyGetButtons(PLAYER_1, A_BUTTON))
                        {
                            entry->zsize *= D_80051D70;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, B_BUTTON))
                        {
                            entry->zsize *= D_80051D74;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, D_CBUTTONS))
                        {
                            entry->ysize *= D_80051D78;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, U_CBUTTONS))
                        {
                            entry->ysize *= D_80051D7C;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, L_CBUTTONS))
                        {
                            entry->xsize *= D_80051D80;
                            changed = 1;
                        }
                    
                        if (joyGetButtons(PLAYER_1, R_CBUTTONS))
                        {
                            entry->xsize *= D_80051D84;
                            changed = 1;
                        }
                    }
                }

                pos.x = entry->xoffset * D_80051D88;
                pos.y = entry->yoffset * D_80051D88;
                pos.z = entry->zoffset * D_80051D88;

                xscale = entry->xsize;
                yscale = entry->ysize;
                zscale = entry->zsize;

                matrix_4x4_set_identity_and_position(&pos, &mtx);

                matrix_column_1_scalar_multiply(xscale, (f32 *)(&mtx));
                matrix_column_2_scalar_multiply(yscale, (f32 *)(&mtx));
                matrix_column_3_scalar_multiply_2(zscale, (f32 *)(&mtx));

                matrix_4x4_multiply_homogeneous((Mtxf *)hatmodel->render_pos, &mtx, &tmp);
                matrix_4x4_copy(&tmp, (Mtxf *)hatmodel->render_pos);

                if (hat == HATTYPE_PEAKED)
                {
                    headVisible = 0;
                }
            }

            if ((!(chr->hidden & CHRHIDDEN_DROP_HELD_ITEMS)) || (!(hatobj->runtime_bitflags & RUNTIMEBITFLAG_00000080)))
            {
                chr->field_20 = sub_GAME_7F06B120(chr->field_20, hatmodel);
            }
        }

        if (model->obj->Switches[4] != NULL)
        {
            union ModelRwData *rwdata;
            ModelFileHeader *headfile;

            rwdata = modelGetNodeRwData(model, model->obj->Switches[4]);
            headfile = rwdata->HeadPlaceholder.ModelFileHeader;

            if ((headfile != NULL) && (headfile->Switches[1] != NULL))
            {
                modelGetNodeRwData(model, headfile->Switches[1])->Switch.visible = headVisible;
            }
        }

        sub_GAME_7F06B29C(chr->field_20);
        chr->field_20 = sub_GAME_7F06BB28(chr->field_20);
    }
    else
    {
        if (chr->weapons_held[GUNRIGHT] != NULL)
        {
            chr->weapons_held[GUNRIGHT]->flags &= ~PROPFLAG_ONSCREEN;
        }

        if (chr->weapons_held[GUNLEFT] != NULL)
        {
            chr->weapons_held[GUNLEFT]->flags &= ~PROPFLAG_ONSCREEN;
        }

        if (chr->handle_positiondata_hat != NULL)
        {
            chr->handle_positiondata_hat->flags &= ~PROPFLAG_ONSCREEN;
        }

        prop->flags &= ~PROPFLAG_ONSCREEN;

        chr->shadecol.r = chr->nextcol.r;
        chr->shadecol.g = chr->nextcol.g;
        chr->shadecol.b = chr->nextcol.b;
        chr->shadecol.a = chr->nextcol.a;
    }

    if (!(chr->chrflags & CHRFLAG_HIDDEN))
    {
        if (chr->hidden & CHRHIDDEN_DROP_HELD_ITEMS)
        {
            PropRecord *dropprop = prop->child;
            PropRecord *unusedprop;

            while (dropprop != NULL)
            {
                PropRecord *nextprop = dropprop->prev;

                objDrop(dropprop);

                dropprop = nextprop;
            }

            chr->hidden &= ~CHRHIDDEN_DROP_HELD_ITEMS;
        }

        chrlvTriggerFireWeapon(chr);
    }

    return 0;
}
#endif

#ifdef VERSION_EU
GLOBAL_ASM(
/* i belong to this function*/
.late_rodata
glabel D_jp80051D88
.word 0x3f4ccccd
glabel D_80051D58
.word 0x3ca3d70a /*0.02*/
glabel D_80051D5C
.word 0x3ca3d70a /*0.02*/
glabel D_80051D60
.word 0x3ca3d70a /*0.02*/
glabel D_80051D64
.word 0x3ca3d70a /*0.02*/
glabel D_80051D68
.word 0x3ca3d70a /*0.02*/
glabel D_80051D6C
.word 0x3ca3d70a /*0.02*/
glabel D_80051D70
.word 0x3f7eb852 /*0.995*/
glabel D_80051D74
.word 0x3f80a3d7 /*1.005*/
glabel D_80051D78
.word 0x3f7eb852 /*0.995*/
glabel D_80051D7C
.word 0x3f80a3d7 /*1.005*/
glabel D_80051D80
.word 0x3f7eb852 /*0.995*/
glabel D_80051D84
.word 0x3f80a3d7 /*1.005*/
glabel D_80051D88
.word 0x41aa6666 /*21.299999*/
.text
glabel chrTickBeams
/* 053858 7F020E68 27BDFEA8 */  addiu $sp, $sp, -0x158
/* 05385C 7F020E6C 3C0F8003 */  lui   $t7, %hi(D_8002CC6C) # $t7, 0x8003
/* 053860 7F020E70 25EF81BC */  addiu $t7, %lo(D_8002CC6C) # addiu $t7, $t7, -0x7e44
/* 053864 7F020E74 AFBF0024 */  sw    $ra, 0x24($sp)
/* 053868 7F020E78 AFB00020 */  sw    $s0, 0x20($sp)
/* 05386C 7F020E7C AFA40158 */  sw    $a0, 0x158($sp)
/* 053870 7F020E80 25E8003C */  addiu $t0, $t7, 0x3c
/* 053874 7F020E84 27AE0118 */  addiu $t6, $sp, 0x118
.L7F020E88:
/* 053878 7F020E88 8DE10000 */  lw    $at, ($t7)
/* 05387C 7F020E8C 25EF000C */  addiu $t7, $t7, 0xc
/* 053880 7F020E90 25CE000C */  addiu $t6, $t6, 0xc
/* 053884 7F020E94 ADC1FFF4 */  sw    $at, -0xc($t6)
/* 053888 7F020E98 8DE1FFF8 */  lw    $at, -8($t7)
/* 05388C 7F020E9C ADC1FFF8 */  sw    $at, -8($t6)
/* 053890 7F020EA0 8DE1FFFC */  lw    $at, -4($t7)
/* 053894 7F020EA4 15E8FFF8 */  bne   $t7, $t0, .L7F020E88
/* 053898 7F020EA8 ADC1FFFC */   sw    $at, -4($t6)
/* 05389C 7F020EAC 8DE10000 */  lw    $at, ($t7)
/* 0538A0 7F020EB0 3C0C8004 */  lui   $t4, %hi(g_ClockTimer) # $t4, 0x8004
/* 0538A4 7F020EB4 240B0001 */  li    $t3, 1
/* 0538A8 7F020EB8 ADC10000 */  sw    $at, ($t6)
/* 0538AC 7F020EBC 8FA90158 */  lw    $t1, 0x158($sp)
/* 0538B0 7F020EC0 8D8C0FF4 */  lw    $t4, %lo(g_ClockTimer)($t4)
/* 0538B4 7F020EC4 3C188002 */  lui   $t8, %hi(D_8002C904) # $t8, 0x8002
/* 0538B8 7F020EC8 8D300004 */  lw    $s0, 4($t1)
/* 0538BC 7F020ECC 8E0A001C */  lw    $t2, 0x1c($s0)
/* 0538C0 7F020ED0 AFAB0108 */  sw    $t3, 0x108($sp)
/* 0538C4 7F020ED4 AFAC0104 */  sw    $t4, 0x104($sp)
/* 0538C8 7F020ED8 AFAA0110 */  sw    $t2, 0x110($sp)
/* 0538CC 7F020EDC 8E020014 */  lw    $v0, 0x14($s0)
/* 0538D0 7F020EE0 304D0400 */  andi  $t5, $v0, 0x400
/* 0538D4 7F020EE4 11A00003 */  beqz  $t5, .L7F020EF4
/* 0538D8 7F020EE8 0002CB40 */   sll   $t9, $v0, 0xd
/* 0538DC 7F020EEC 07230035 */  bgezl $t9, .L7F020FC4
/* 0538E0 7F020EF0 96180012 */   lhu   $t8, 0x12($s0)
.L7F020EF4:
/* 0538E4 7F020EF4 8F187E54 */  lw    $t8, %lo(D_8002C904)($t8)
/* 0538E8 7F020EF8 3C088002 */  lui   $t0, %hi(g_AnimationTablePointerCountRelated) # $t0, 0x8002
/* 0538EC 7F020EFC 1300001E */  beqz  $t8, .L7F020F78
/* 0538F0 7F020F00 00000000 */   nop
/* 0538F4 7F020F04 8D087E58 */  lw    $t0, %lo(g_AnimationTablePointerCountRelated)($t0)
/* 0538F8 7F020F08 3C0E8002 */  lui   $t6, %hi(animation_table_ptrs1)
/* 0538FC 7F020F0C 24010001 */  li    $at, 1
/* 053900 7F020F10 00087880 */  sll   $t7, $t0, 2
/* 053904 7F020F14 01CF7021 */  addu  $t6, $t6, $t7
/* 053908 7F020F18 8DCE52BC */  lw    $t6, %lo(animation_table_ptrs1)($t6)
/* 05390C 7F020F1C 11C1001D */  beq   $t6, $at, .L7F020F94
/* 053910 7F020F20 00000000 */   nop
/* 053914 7F020F24 0FC1BDD3 */  jal   objecthandlerGetModelAnim
/* 053918 7F020F28 8FA40110 */   lw    $a0, 0x110($sp)
/* 05391C 7F020F2C 3C098002 */  lui   $t1, %hi(g_AnimationTablePointerCountRelated) # $t1, 0x8002
/* 053920 7F020F30 8D297E58 */  lw    $t1, %lo(g_AnimationTablePointerCountRelated)($t1)
/* 053924 7F020F34 3C058002 */  lui   $a1, %hi(animation_table_ptrs1)
/* 053928 7F020F38 8FA40110 */  lw    $a0, 0x110($sp)
/* 05392C 7F020F3C 00095080 */  sll   $t2, $t1, 2
/* 053930 7F020F40 00AA2821 */  addu  $a1, $a1, $t2
/* 053934 7F020F44 8CA552BC */  lw    $a1, %lo(animation_table_ptrs1)($a1)
/* 053938 7F020F48 00003025 */  move  $a2, $zero
/* 05393C 7F020F4C 10450011 */  beq   $v0, $a1, .L7F020F94
/* 053940 7F020F50 00000000 */   nop
/* 053944 7F020F54 44800000 */  mtc1  $zero, $f0
/* 053948 7F020F58 3C013F00 */  li    $at, 0x3F000000 # 0.500000
/* 05394C 7F020F5C 44812000 */  mtc1  $at, $f4
/* 053950 7F020F60 44070000 */  mfc1  $a3, $f0
/* 053954 7F020F64 E7A00014 */  swc1  $f0, 0x14($sp)
/* 053958 7F020F68 0FC1BF92 */  jal   modelSetAnimation
/* 05395C 7F020F6C E7A40010 */   swc1  $f4, 0x10($sp)
/* 053960 7F020F70 10000008 */  b     .L7F020F94
/* 053964 7F020F74 00000000 */   nop
.L7F020F78:
/* 053968 7F020F78 0FC0C9C0 */  jal   chrlvActionTick
/* 05396C 7F020F7C 02002025 */   move  $a0, $s0
/* 053970 7F020F80 8E0B001C */  lw    $t3, 0x1c($s0)
/* 053974 7F020F84 15600003 */  bnez  $t3, .L7F020F94
/* 053978 7F020F88 00000000 */   nop
/* 05397C 7F020F8C 100002D1 */  b     .L7F021AD4
/* 053980 7F020F90 24020001 */   li    $v0, 1
.L7F020F94:
/* 053984 7F020F94 3C0C8002 */  lui   $t4, %hi(D_8002C90C) # $t4, 0x8002
/* 053988 7F020F98 8D8C7E5C */  lw    $t4, %lo(D_8002C90C)($t4)
/* 05398C 7F020F9C 3C0D8002 */  lui   $t5, %hi(D_8002C910) # $t5, 0x8002
/* 053990 7F020FA0 51800008 */  beql  $t4, $zero, .L7F020FC4
/* 053994 7F020FA4 96180012 */   lhu   $t8, 0x12($s0)
/* 053998 7F020FA8 8DAD7E60 */  lw    $t5, %lo(D_8002C910)($t5)
/* 05399C 7F020FAC AFA00104 */  sw    $zero, 0x104($sp)
/* 0539A0 7F020FB0 24190001 */  li    $t9, 1
/* 0539A4 7F020FB4 51A00003 */  beql  $t5, $zero, .L7F020FC4
/* 0539A8 7F020FB8 96180012 */   lhu   $t8, 0x12($s0)
/* 0539AC 7F020FBC AFB90104 */  sw    $t9, 0x104($sp)
/* 0539B0 7F020FC0 96180012 */  lhu   $t8, 0x12($s0)
.L7F020FC4:
/* 0539B4 7F020FC4 33080020 */  andi  $t0, $t8, 0x20
/* 0539B8 7F020FC8 51000006 */  beql  $t0, $zero, .L7F020FE4
/* 0539BC 7F020FCC 8E020160 */   lw    $v0, 0x160($s0)
/* 0539C0 7F020FD0 0FC080A2 */  jal   chrpropCleanupForRemoval
/* 0539C4 7F020FD4 8FA40158 */   lw    $a0, 0x158($sp)
/* 0539C8 7F020FD8 100002BE */  b     .L7F021AD4
/* 0539CC 7F020FDC 24020001 */   li    $v0, 1
/* 0539D0 7F020FE0 8E020160 */  lw    $v0, 0x160($s0)
.L7F020FE4:
/* 0539D4 7F020FE4 50400009 */  beql  $v0, $zero, .L7F02100C
/* 0539D8 7F020FE8 8E020164 */   lw    $v0, 0x164($s0)
/* 0539DC 7F020FEC 8C440004 */  lw    $a0, 4($v0)
/* 0539E0 7F020FF0 8C8F0064 */  lw    $t7, 0x64($a0)
/* 0539E4 7F020FF4 31EE0004 */  andi  $t6, $t7, 4
/* 0539E8 7F020FF8 51C00004 */  beql  $t6, $zero, .L7F02100C
/* 0539EC 7F020FFC 8E020164 */   lw    $v0, 0x164($s0)
/* 0539F0 7F021000 0FC10439 */  jal   objFreePermanently
/* 0539F4 7F021004 24050001 */   li    $a1, 1
/* 0539F8 7F021008 8E020164 */  lw    $v0, 0x164($s0)
.L7F02100C:
/* 0539FC 7F02100C 50400009 */  beql  $v0, $zero, .L7F021034
/* 053A00 7F021010 8E020014 */   lw    $v0, 0x14($s0)
/* 053A04 7F021014 8C440004 */  lw    $a0, 4($v0)
/* 053A08 7F021018 8C890064 */  lw    $t1, 0x64($a0)
/* 053A0C 7F02101C 312A0004 */  andi  $t2, $t1, 4
/* 053A10 7F021020 51400004 */  beql  $t2, $zero, .L7F021034
/* 053A14 7F021024 8E020014 */   lw    $v0, 0x14($s0)
/* 053A18 7F021028 0FC10439 */  jal   objFreePermanently
/* 053A1C 7F02102C 24050001 */   li    $a1, 1
/* 053A20 7F021030 8E020014 */  lw    $v0, 0x14($s0)
.L7F021034:
/* 053A24 7F021034 8FAC0158 */  lw    $t4, 0x158($sp)
/* 053A28 7F021038 304B0400 */  andi  $t3, $v0, 0x400
/* 053A2C 7F02103C 51600005 */  beql  $t3, $zero, .L7F021054
/* 053A30 7F021040 918D0000 */   lbu   $t5, ($t4)
/* 053A34 7F021044 AFA0010C */  sw    $zero, 0x10c($sp)
/* 053A38 7F021048 100000BD */  b     .L7F021340
/* 053A3C 7F02104C 82030007 */   lb    $v1, 7($s0)
/* 053A40 7F021050 918D0000 */  lbu   $t5, ($t4)
.L7F021054:
/* 053A44 7F021054 24010006 */  li    $at, 6
/* 053A48 7F021058 55A1000D */  bnel  $t5, $at, .L7F021090
/* 053A4C 7F02105C 00027A00 */   sll   $t7, $v0, 8
/* 053A50 7F021060 0FC269A7 */  jal   getPlayerPointerIndex
/* 053A54 7F021064 01802025 */   move  $a0, $t4
/* 053A58 7F021068 0002C880 */  sll   $t9, $v0, 2
/* 053A5C 7F02106C 3C188007 */  lui   $t8, %hi(g_playerPointers)
/* 053A60 7F021070 0319C021 */  addu  $t8, $t8, $t9
/* 053A64 7F021074 8F1889F0 */  lw    $t8, %lo(g_playerPointers)($t8)
/* 053A68 7F021078 24010001 */  li    $at, 1
/* 053A6C 7F02107C 8F080000 */  lw    $t0, ($t8)
/* 053A70 7F021080 51010005 */  beql  $t0, $at, .L7F021098
/* 053A74 7F021084 240E0001 */   li    $t6, 1
/* 053A78 7F021088 8E020014 */  lw    $v0, 0x14($s0)
/* 053A7C 7F02108C 00027A00 */  sll   $t7, $v0, 8
.L7F021090:
/* 053A80 7F021090 05E10017 */  bgez  $t7, .L7F0210F0
/* 053A84 7F021094 240E0001 */   li    $t6, 1
.L7F021098:
/* 053A88 7F021098 AFAE010C */  sw    $t6, 0x10c($sp)
/* 053A8C 7F02109C 82090007 */  lb    $t1, 7($s0)
/* 053A90 7F0210A0 24010003 */  li    $at, 3
/* 053A94 7F0210A4 02002025 */  move  $a0, $s0
/* 053A98 7F0210A8 1521000D */  bne   $t1, $at, .L7F0210E0
/* 053A9C 7F0210AC 00000000 */   nop
/* 053AA0 7F0210B0 8E0A002C */  lw    $t2, 0x2c($s0)
/* 053AA4 7F0210B4 1540000A */  bnez  $t2, .L7F0210E0
/* 053AA8 7F0210B8 00000000 */   nop
/* 053AAC 7F0210BC 8E0B003C */  lw    $t3, 0x3c($s0)
/* 053AB0 7F0210C0 8FA50104 */  lw    $a1, 0x104($sp)
/* 053AB4 7F0210C4 00003025 */  move  $a2, $zero
/* 053AB8 7F0210C8 11600005 */  beqz  $t3, .L7F0210E0
/* 053ABC 7F0210CC 00000000 */   nop
/* 053AC0 7F0210D0 0FC1C323 */  jal   modelTickAnim
/* 053AC4 7F0210D4 8FA40110 */   lw    $a0, 0x110($sp)
/* 053AC8 7F0210D8 10000003 */  b     .L7F0210E8
/* 053ACC 7F0210DC 00000000 */   nop
.L7F0210E0:
/* 053AD0 7F0210E0 0FC0836E */  jal   chrPositionRelated7F020E40
/* 053AD4 7F0210E4 8FA50104 */   lw    $a1, 0x104($sp)
.L7F0210E8:
/* 053AD8 7F0210E8 10000095 */  b     .L7F021340
/* 053ADC 7F0210EC 82030007 */   lb    $v1, 7($s0)
.L7F0210F0:
/* 053AE0 7F0210F0 82030007 */  lb    $v1, 7($s0)
/* 053AE4 7F0210F4 2406000E */  li    $a2, 14
/* 053AE8 7F0210F8 2401000F */  li    $at, 15
/* 053AEC 7F0210FC 10C30003 */  beq   $a2, $v1, .L7F02110C
/* 053AF0 7F021100 00000000 */   nop
/* 053AF4 7F021104 54610041 */  bnel  $v1, $at, .L7F02120C
/* 053AF8 7F021108 24010003 */   li    $at, 3
.L7F02110C:
/* 053AFC 7F02110C 54C30005 */  bnel  $a2, $v1, .L7F021124
/* 053B00 7F021110 2401000F */   li    $at, 15
/* 053B04 7F021114 820D0038 */  lb    $t5, 0x38($s0)
/* 053B08 7F021118 24010006 */  li    $at, 6
/* 053B0C 7F02111C 11A10007 */  beq   $t5, $at, .L7F02113C
/* 053B10 7F021120 2401000F */   li    $at, 15
.L7F021124:
/* 053B14 7F021124 1461001E */  bne   $v1, $at, .L7F0211A0
/* 053B18 7F021128 02002025 */   move  $a0, $s0
/* 053B1C 7F02112C 820C005C */  lb    $t4, 0x5c($s0)
/* 053B20 7F021130 24010006 */  li    $at, 6
/* 053B24 7F021134 1581001A */  bne   $t4, $at, .L7F0211A0
/* 053B28 7F021138 00000000 */   nop
.L7F02113C:
/* 053B2C 7F02113C 0FC1B525 */  jal   getinstsize
/* 053B30 7F021140 8FA40110 */   lw    $a0, 0x110($sp)
/* 053B34 7F021144 8FA40158 */  lw    $a0, 0x158($sp)
/* 053B38 7F021148 44060000 */  mfc1  $a2, $f0
/* 053B3C 7F02114C 24070001 */  li    $a3, 1
/* 053B40 7F021150 24850008 */  addiu $a1, $a0, 8
/* 053B44 7F021154 0FC15413 */  jal   sub_GAME_7F054D6C
/* 053B48 7F021158 AFA50030 */   sw    $a1, 0x30($sp)
/* 053B4C 7F02115C 10400028 */  beqz  $v0, .L7F021200
/* 053B50 7F021160 AFA2010C */   sw    $v0, 0x10c($sp)
/* 053B54 7F021164 8FA40110 */  lw    $a0, 0x110($sp)
/* 053B58 7F021168 0FC1B476 */  jal   getsuboffset
/* 053B5C 7F02116C 260500BC */   addiu $a1, $s0, 0xbc
/* 053B60 7F021170 0FC1B6B8 */  jal   subcalcpos
/* 053B64 7F021174 8FA40110 */   lw    $a0, 0x110($sp)
/* 053B68 7F021178 8FA40158 */  lw    $a0, 0x158($sp)
/* 053B6C 7F02117C 0FC10057 */  jal   set_color_shading_from_tile
/* 053B70 7F021180 260500F8 */   addiu $a1, $s0, 0xf8
/* 053B74 7F021184 8FA40110 */  lw    $a0, 0x110($sp)
/* 053B78 7F021188 0FC1B476 */  jal   getsuboffset
/* 053B7C 7F02118C 8FA50030 */   lw    $a1, 0x30($sp)
/* 053B80 7F021190 0FC08343 */  jal   chrPositionRelated7F020D94
/* 053B84 7F021194 02002025 */   move  $a0, $s0
/* 053B88 7F021198 10000019 */  b     .L7F021200
/* 053B8C 7F02119C 00000000 */   nop
.L7F0211A0:
/* 053B90 7F0211A0 0FC0836E */  jal   chrPositionRelated7F020E40
/* 053B94 7F0211A4 8FA50104 */   lw    $a1, 0x104($sp)
/* 053B98 7F0211A8 0FC1B525 */  jal   getinstsize
/* 053B9C 7F0211AC 8FA40110 */   lw    $a0, 0x110($sp)
/* 053BA0 7F0211B0 8FA40158 */  lw    $a0, 0x158($sp)
/* 053BA4 7F0211B4 44060000 */  mfc1  $a2, $f0
/* 053BA8 7F0211B8 24070001 */  li    $a3, 1
/* 053BAC 7F0211BC 0FC15413 */  jal   sub_GAME_7F054D6C
/* 053BB0 7F0211C0 24850008 */   addiu $a1, $a0, 8
/* 053BB4 7F0211C4 1040000E */  beqz  $v0, .L7F021200
/* 053BB8 7F0211C8 AFA2010C */   sw    $v0, 0x10c($sp)
/* 053BBC 7F0211CC 82030007 */  lb    $v1, 7($s0)
/* 053BC0 7F0211D0 2401000E */  li    $at, 14
/* 053BC4 7F0211D4 3C088004 */  lui   $t0, %hi(g_GlobalTimer) # $t0, 0x8004
/* 053BC8 7F0211D8 54610005 */  bnel  $v1, $at, .L7F0211F0
/* 053BCC 7F0211DC 2401000F */   li    $at, 15
/* 053BD0 7F0211E0 8D080FFC */  lw    $t0, %lo(g_GlobalTimer)($t0)
/* 053BD4 7F0211E4 10000006 */  b     .L7F021200
/* 053BD8 7F0211E8 AE080078 */   sw    $t0, 0x78($s0)
/* 053BDC 7F0211EC 2401000F */  li    $at, 15
.L7F0211F0:
/* 053BE0 7F0211F0 14610003 */  bne   $v1, $at, .L7F021200
/* 053BE4 7F0211F4 3C0F8004 */   lui   $t7, %hi(g_GlobalTimer) # $t7, 0x8004
/* 053BE8 7F0211F8 8DEF0FFC */  lw    $t7, %lo(g_GlobalTimer)($t7)
/* 053BEC 7F0211FC AE0F009C */  sw    $t7, 0x9c($s0)
.L7F021200:
/* 053BF0 7F021200 1000004F */  b     .L7F021340
/* 053BF4 7F021204 82030007 */   lb    $v1, 7($s0)
/* 053BF8 7F021208 24010003 */  li    $at, 3
.L7F02120C:
/* 053BFC 7F02120C 5461001C */  bnel  $v1, $at, .L7F021280
/* 053C00 7F021210 24010001 */   li    $at, 1
/* 053C04 7F021214 8E0E002C */  lw    $t6, 0x2c($s0)
/* 053C08 7F021218 55C00019 */  bnezl $t6, .L7F021280
/* 053C0C 7F02121C 24010001 */   li    $at, 1
/* 053C10 7F021220 0FC1B525 */  jal   getinstsize
/* 053C14 7F021224 8FA40110 */   lw    $a0, 0x110($sp)
/* 053C18 7F021228 8FA40158 */  lw    $a0, 0x158($sp)
/* 053C1C 7F02122C 44060000 */  mfc1  $a2, $f0
/* 053C20 7F021230 24070001 */  li    $a3, 1
/* 053C24 7F021234 0FC15413 */  jal   sub_GAME_7F054D6C
/* 053C28 7F021238 24850008 */   addiu $a1, $a0, 8
/* 053C2C 7F02123C 10400009 */  beqz  $v0, .L7F021264
/* 053C30 7F021240 AFA2010C */   sw    $v0, 0x10c($sp)
/* 053C34 7F021244 8E0A003C */  lw    $t2, 0x3c($s0)
/* 053C38 7F021248 02002025 */  move  $a0, $s0
/* 053C3C 7F02124C 55400006 */  bnezl $t2, .L7F021268
/* 053C40 7F021250 8FA40110 */   lw    $a0, 0x110($sp)
/* 053C44 7F021254 0FC0836E */  jal   chrPositionRelated7F020E40
/* 053C48 7F021258 8FA50104 */   lw    $a1, 0x104($sp)
/* 053C4C 7F02125C 10000005 */  b     .L7F021274
/* 053C50 7F021260 00000000 */   nop
.L7F021264:
/* 053C54 7F021264 8FA40110 */  lw    $a0, 0x110($sp)
.L7F021268:
/* 053C58 7F021268 8FA50104 */  lw    $a1, 0x104($sp)
/* 053C5C 7F02126C 0FC1C323 */  jal   modelTickAnim
/* 053C60 7F021270 00003025 */   move  $a2, $zero
.L7F021274:
/* 053C64 7F021274 10000032 */  b     .L7F021340
/* 053C68 7F021278 82030007 */   lb    $v1, 7($s0)
/* 053C6C 7F02127C 24010001 */  li    $at, 1
.L7F021280:
/* 053C70 7F021280 1461001C */  bne   $v1, $at, .L7F0212F4
/* 053C74 7F021284 30484000 */   andi  $t0, $v0, 0x4000
/* 053C78 7F021288 0FC1B525 */  jal   getinstsize
/* 053C7C 7F02128C 8FA40110 */   lw    $a0, 0x110($sp)
/* 053C80 7F021290 8FA40158 */  lw    $a0, 0x158($sp)
/* 053C84 7F021294 44060000 */  mfc1  $a2, $f0
/* 053C88 7F021298 24070001 */  li    $a3, 1
/* 053C8C 7F02129C 0FC15413 */  jal   sub_GAME_7F054D6C
/* 053C90 7F0212A0 24850008 */   addiu $a1, $a0, 8
/* 053C94 7F0212A4 14400005 */  bnez  $v0, .L7F0212BC
/* 053C98 7F0212A8 AFA2010C */   sw    $v0, 0x10c($sp)
/* 053C9C 7F0212AC 8E0D0014 */  lw    $t5, 0x14($s0)
/* 053CA0 7F0212B0 8FB90110 */  lw    $t9, 0x110($sp)
/* 053CA4 7F0212B4 31AC0001 */  andi  $t4, $t5, 1
/* 053CA8 7F0212B8 11800005 */  beqz  $t4, .L7F0212D0
.L7F0212BC:
/* 053CAC 7F0212BC 02002025 */   move  $a0, $s0
/* 053CB0 7F0212C0 0FC0836E */  jal   chrPositionRelated7F020E40
/* 053CB4 7F0212C4 8FA50104 */   lw    $a1, 0x104($sp)
/* 053CB8 7F0212C8 10000008 */  b     .L7F0212EC
/* 053CBC 7F0212CC 00000000 */   nop
.L7F0212D0:
/* 053CC0 7F0212D0 8F380054 */  lw    $t8, 0x54($t9)
/* 053CC4 7F0212D4 03202025 */  move  $a0, $t9
/* 053CC8 7F0212D8 8FA50104 */  lw    $a1, 0x104($sp)
/* 053CCC 7F0212DC 13000003 */  beqz  $t8, .L7F0212EC
/* 053CD0 7F0212E0 00000000 */   nop
/* 053CD4 7F0212E4 0FC1C323 */  jal   modelTickAnim
/* 053CD8 7F0212E8 00003025 */   move  $a2, $zero
.L7F0212EC:
/* 053CDC 7F0212EC 10000014 */  b     .L7F021340
/* 053CE0 7F0212F0 82030007 */   lb    $v1, 7($s0)
.L7F0212F4:
/* 053CE4 7F0212F4 11000007 */  beqz  $t0, .L7F021314
/* 053CE8 7F0212F8 02002025 */   move  $a0, $s0
/* 053CEC 7F0212FC 8FA40110 */  lw    $a0, 0x110($sp)
/* 053CF0 7F021300 8FA50104 */  lw    $a1, 0x104($sp)
/* 053CF4 7F021304 0FC1C323 */  jal   modelTickAnim
/* 053CF8 7F021308 00003025 */   move  $a2, $zero
/* 053CFC 7F02130C 10000003 */  b     .L7F02131C
/* 053D00 7F021310 00000000 */   nop
.L7F021314:
/* 053D04 7F021314 0FC0836E */  jal   chrPositionRelated7F020E40
/* 053D08 7F021318 8FA50104 */   lw    $a1, 0x104($sp)
.L7F02131C:
/* 053D0C 7F02131C 0FC1B525 */  jal   getinstsize
/* 053D10 7F021320 8FA40110 */   lw    $a0, 0x110($sp)
/* 053D14 7F021324 8FA40158 */  lw    $a0, 0x158($sp)
/* 053D18 7F021328 44060000 */  mfc1  $a2, $f0
/* 053D1C 7F02132C 24070001 */  li    $a3, 1
/* 053D20 7F021330 0FC15413 */  jal   sub_GAME_7F054D6C
/* 053D24 7F021334 24850008 */   addiu $a1, $a0, 8
/* 053D28 7F021338 AFA2010C */  sw    $v0, 0x10c($sp)
/* 053D2C 7F02133C 82030007 */  lb    $v1, 7($s0)
.L7F021340:
/* 053D30 7F021340 24010001 */  li    $at, 1
/* 053D34 7F021344 14610009 */  bne   $v1, $at, .L7F02136C
/* 053D38 7F021348 8FAE0110 */   lw    $t6, 0x110($sp)
/* 053D3C 7F02134C 8DC90054 */  lw    $t1, 0x54($t6)
/* 053D40 7F021350 8FAA0158 */  lw    $t2, 0x158($sp)
/* 053D44 7F021354 55200006 */  bnezl $t1, .L7F021370
/* 053D48 7F021358 960D0012 */   lhu   $t5, 0x12($s0)
/* 053D4C 7F02135C 914B0000 */  lbu   $t3, ($t2)
/* 053D50 7F021360 24010006 */  li    $at, 6
/* 053D54 7F021364 15610004 */  bne   $t3, $at, .L7F021378
/* 053D58 7F021368 00000000 */   nop
.L7F02136C:
/* 053D5C 7F02136C 960D0012 */  lhu   $t5, 0x12($s0)
.L7F021370:
/* 053D60 7F021370 35AC0200 */  ori   $t4, $t5, 0x200
/* 053D64 7F021374 A60C0012 */  sh    $t4, 0x12($s0)
.L7F021378:
/* 053D68 7F021378 0FC08120 */  jal   chrUpdateAimProperties
/* 053D6C 7F02137C 02002025 */   move  $a0, $s0
/* 053D70 7F021380 8E040020 */  lw    $a0, 0x20($s0)
/* 053D74 7F021384 50800005 */  beql  $a0, $zero, .L7F02139C
/* 053D78 7F021388 8FB8010C */   lw    $t8, 0x10c($sp)
/* 053D7C 7F02138C 0FC1AE7E */  jal   sub_GAME_7F06B248
/* 053D80 7F021390 00000000 */   nop
/* 053D84 7F021394 AE000020 */  sw    $zero, 0x20($s0)
/* 053D88 7F021398 8FB8010C */  lw    $t8, 0x10c($sp)
.L7F02139C:
/* 053D8C 7F02139C 261900F8 */  addiu $t9, $s0, 0xf8
/* 053D90 7F0213A0 53000196 */  beql  $t8, $zero, .L7F0219FC
/* 053D94 7F0213A4 8E020160 */   lw    $v0, 0x160($s0)
/* 053D98 7F0213A8 0FC24128 */  jal   get_debug_chrnum_flag
/* 053D9C 7F0213AC AFB9002C */   sw    $t9, 0x2c($sp)
/* 053DA0 7F0213B0 8FA20158 */  lw    $v0, 0x158($sp)
/* 053DA4 7F0213B4 2404000C */  li    $a0, 12
/* 053DA8 7F0213B8 90480001 */  lbu   $t0, 1($v0)
/* 053DAC 7F0213BC 350F0002 */  ori   $t7, $t0, 2
/* 053DB0 7F0213C0 A04F0001 */  sb    $t7, 1($v0)
/* 053DB4 7F0213C4 8E0E0014 */  lw    $t6, 0x14($s0)
/* 053DB8 7F0213C8 35C90008 */  ori   $t1, $t6, 8
/* 053DBC 7F0213CC 0FC24737 */  jal   cheatIsActive
/* 053DC0 7F0213D0 AE090014 */   sw    $t1, 0x14($s0)
/* 053DC4 7F0213D4 10400018 */  beqz  $v0, .L7F021438
/* 053DC8 7F0213D8 00000000 */   nop
/* 053DCC 7F0213DC 8204000F */  lb    $a0, 0xf($s0)
/* 053DD0 7F0213E0 0FC081B3 */  jal   chrCanUseDKModeScaling
/* 053DD4 7F0213E4 82050006 */   lb    $a1, 6($s0)
/* 053DD8 7F0213E8 10400013 */  beqz  $v0, .L7F021438
/* 053DDC 7F0213EC 3C013EA0 */   li    $at, 0x3EA00000 # 0.312500
/* 053DE0 7F0213F0 44816000 */  mtc1  $at, $f12
/* 053DE4 7F0213F4 0FC1B2D3 */  jal   modelSetDistanceScale
/* 053DE8 7F0213F8 00000000 */   nop
/* 053DEC 7F0213FC 8E020014 */  lw    $v0, 0x14($s0)
/* 053DF0 7F021400 3C01EFFF */  lui   $at, (0xEFFFFFFF >> 16) # lui $at, 0xefff
/* 053DF4 7F021404 3421FFFF */  ori   $at, (0xEFFFFFFF & 0xFFFF) # ori $at, $at, 0xffff
/* 053DF8 7F021408 000250C0 */  sll   $t2, $v0, 3
/* 053DFC 7F02140C 0541000A */  bgez  $t2, .L7F021438
/* 053E00 7F021410 00415824 */   and   $t3, $v0, $at
/* 053E04 7F021414 8E04001C */  lw    $a0, 0x1c($s0)
/* 053E08 7F021418 AE0B0014 */  sw    $t3, 0x14($s0)
/* 053E0C 7F02141C 3C018004 */  lui   $at, %hi(D_jp80051D88) # $at, 0x8004
/* 053E10 7F021420 C4287E80 */  lwc1  $f8, %lo(D_jp80051D88)($at)
/* 053E14 7F021424 C4860014 */  lwc1  $f6, 0x14($a0)
/* 053E18 7F021428 46083283 */  div.s $f10, $f6, $f8
/* 053E1C 7F02142C 44055000 */  mfc1  $a1, $f10
/* 053E20 7F021430 0FC1B4CF */  jal   modelSetScale
/* 053E24 7F021434 00000000 */   nop
.L7F021438:
/* 053E28 7F021438 3C0D7F02 */  lui   $t5, %hi(sub_GAME_7F02083C) # $t5, 0x7f02
/* 053E2C 7F02143C 25AD078C */  addiu $t5, %lo(sub_GAME_7F02083C) # addiu $t5, $t5, 0x78c
/* 053E30 7F021440 3C018003 */  lui   $at, %hi(D_80036090) # $at, 0x8003
/* 053E34 7F021444 AC2D15E0 */  sw    $t5, %lo(D_80036090)($at)
/* 053E38 7F021448 3C018006 */  lui   $at, %hi(dword_CODE_bss_80069B60) # $at, 0x8006
/* 053E3C 7F02144C 0FC1E111 */  jal   camGetWorldToScreenMtxf
/* 053E40 7F021450 AC308AA0 */   sw    $s0, %lo(dword_CODE_bss_80069B60)($at)
/* 053E44 7F021454 8FAC0110 */  lw    $t4, 0x110($sp)
/* 053E48 7F021458 AFA20118 */  sw    $v0, 0x118($sp)
/* 053E4C 7F02145C 8D980008 */  lw    $t8, 8($t4)
/* 053E50 7F021460 8704000E */  lh    $a0, 0xe($t8)
/* 053E54 7F021464 0004C980 */  sll   $t9, $a0, 6
/* 053E58 7F021468 0FC2F2B1 */  jal   dynAllocate
/* 053E5C 7F02146C 03202025 */   move  $a0, $t9
/* 053E60 7F021470 3C058006 */  lui   $a1, %hi(dword_CODE_bss_80069B60) # $a1, 0x8006
/* 053E64 7F021474 24A58AA0 */  addiu $a1, %lo(dword_CODE_bss_80069B60) # addiu $a1, $a1, -0x7560
/* 053E68 7F021478 8CA30000 */  lw    $v1, ($a1)
/* 053E6C 7F02147C AFA20128 */  sw    $v0, 0x128($sp)
/* 053E70 7F021480 80640011 */  lb    $a0, 0x11($v1)
/* 053E74 7F021484 0480000B */  bltz  $a0, .L7F0214B4
/* 053E78 7F021488 3C088004 */   lui   $t0, %hi(g_ClockTimer) # $t0, 0x8004
/* 053E7C 7F02148C 8D080FF4 */  lw    $t0, %lo(g_ClockTimer)($t0)
/* 053E80 7F021490 2409FFFF */  li    $t1, -1
/* 053E84 7F021494 00887821 */  addu  $t7, $a0, $t0
/* 053E88 7F021498 A06F0011 */  sb    $t7, 0x11($v1)
/* 053E8C 7F02149C 8CA30000 */  lw    $v1, ($a1)
/* 053E90 7F0214A0 806E0011 */  lb    $t6, 0x11($v1)
/* 053E94 7F0214A4 29C10018 */  slti  $at, $t6, 0x18
/* 053E98 7F0214A8 54200003 */  bnezl $at, .L7F0214B8
/* 053E9C 7F0214AC 27A40118 */   addiu $a0, $sp, 0x118
/* 053EA0 7F0214B0 A0690011 */  sb    $t1, 0x11($v1)
.L7F0214B4:
/* 053EA4 7F0214B4 27A40118 */  addiu $a0, $sp, 0x118
.L7F0214B8:
/* 053EA8 7F0214B8 0FC1BD9C */  jal   subcalcmatrices
/* 053EAC 7F0214BC 8FA50110 */   lw    $a1, 0x110($sp)
/* 053EB0 7F0214C0 3C018003 */  lui   $at, %hi(D_80036090) # $at, 0x8003
/* 053EB4 7F0214C4 AC2015E0 */  sw    $zero, %lo(D_80036090)($at)
/* 053EB8 7F0214C8 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 053EBC 7F0214CC 44816000 */  mtc1  $at, $f12
/* 053EC0 7F0214D0 0FC1B2D3 */  jal   modelSetDistanceScale
/* 053EC4 7F0214D4 00000000 */   nop
/* 053EC8 7F0214D8 260400F4 */  addiu $a0, $s0, 0xf4
/* 053ECC 7F0214DC 0FC100F4 */  jal   update_color_shading
/* 053ED0 7F0214E0 8FA5002C */   lw    $a1, 0x2c($sp)
/* 053ED4 7F0214E4 0FC1B396 */  jal   sub_GAME_7F06C768
/* 053ED8 7F0214E8 8FA40110 */   lw    $a0, 0x110($sp)
/* 053EDC 7F0214EC 8FAA0158 */  lw    $t2, 0x158($sp)
/* 053EE0 7F0214F0 00002025 */  move  $a0, $zero
/* 053EE4 7F0214F4 E5400018 */  swc1  $f0, 0x18($t2)
/* 053EE8 7F0214F8 0FC1AE34 */  jal   sub_GAME_7F06B120
/* 053EEC 7F0214FC 8FA50110 */   lw    $a1, 0x110($sp)
/* 053EF0 7F021500 AE020020 */  sw    $v0, 0x20($s0)
/* 053EF4 7F021504 26060020 */  addiu $a2, $s0, 0x20
/* 053EF8 7F021508 AFA60030 */  sw    $a2, 0x30($sp)
/* 053EFC 7F02150C 8FA40158 */  lw    $a0, 0x158($sp)
/* 053F00 7F021510 0FC149B6 */  jal   chrRenderHeldWeapon
/* 053F04 7F021514 00002825 */   move  $a1, $zero
/* 053F08 7F021518 8FA60030 */  lw    $a2, 0x30($sp)
/* 053F0C 7F02151C 8FA40158 */  lw    $a0, 0x158($sp)
/* 053F10 7F021520 0FC149B6 */  jal   chrRenderHeldWeapon
/* 053F14 7F021524 24050001 */   li    $a1, 1
/* 053F18 7F021528 8E0201D8 */  lw    $v0, 0x1d8($s0)
/* 053F1C 7F02152C 5040011A */  beql  $v0, $zero, .L7F021998
/* 053F20 7F021530 8FB80110 */   lw    $t8, 0x110($sp)
/* 053F24 7F021534 8C470004 */  lw    $a3, 4($v0)
/* 053F28 7F021538 904B0001 */  lbu   $t3, 1($v0)
/* 053F2C 7F02153C 00003025 */  move  $a2, $zero
/* 053F30 7F021540 8CE30014 */  lw    $v1, 0x14($a3)
/* 053F34 7F021544 356D0002 */  ori   $t5, $t3, 2
/* 053F38 7F021548 A04D0001 */  sb    $t5, 1($v0)
/* 053F3C 7F02154C 8C65001C */  lw    $a1, 0x1c($v1)
/* 053F40 7F021550 8FA40110 */  lw    $a0, 0x110($sp)
/* 053F44 7F021554 AFA70100 */  sw    $a3, 0x100($sp)
/* 053F48 7F021558 0FC1B366 */  jal   modelFindNodeMtx
/* 053F4C 7F02155C AFA300FC */   sw    $v1, 0xfc($sp)
/* 053F50 7F021560 8FAC00FC */  lw    $t4, 0xfc($sp)
/* 053F54 7F021564 AFA20118 */  sw    $v0, 0x118($sp)
/* 053F58 7F021568 8D980008 */  lw    $t8, 8($t4)
/* 053F5C 7F02156C 8704000E */  lh    $a0, 0xe($t8)
/* 053F60 7F021570 0004C980 */  sll   $t9, $a0, 6
/* 053F64 7F021574 0FC2F2B1 */  jal   dynAllocate
/* 053F68 7F021578 03202025 */   move  $a0, $t9
/* 053F6C 7F02157C AFA20128 */  sw    $v0, 0x128($sp)
/* 053F70 7F021580 27A40118 */  addiu $a0, $sp, 0x118
/* 053F74 7F021584 0FC1BD8D */  jal   instcalcmatrices
/* 053F78 7F021588 8FA500FC */   lw    $a1, 0xfc($sp)
/* 053F7C 7F02158C 82020006 */  lb    $v0, 6($s0)
/* 053F80 7F021590 2841002A */  slti  $at, $v0, 0x2a
/* 053F84 7F021594 142000F2 */  bnez  $at, .L7F021960
/* 053F88 7F021598 28410046 */   slti  $at, $v0, 0x46
/* 053F8C 7F02159C 102000F0 */  beqz  $at, .L7F021960
/* 053F90 7F0215A0 3C0F8003 */   lui   $t7, %hi(D_8002CCAC) # $t7, 0x8003
/* 053F94 7F0215A4 25EF81FC */  addiu $t7, %lo(D_8002CCAC) # addiu $t7, $t7, -0x7e04
/* 053F98 7F0215A8 8DE10000 */  lw    $at, ($t7)
/* 053F9C 7F0215AC 27A800F0 */  addiu $t0, $sp, 0xf0
/* 053FA0 7F0215B0 8DE90004 */  lw    $t1, 4($t7)
/* 053FA4 7F0215B4 AD010000 */  sw    $at, ($t0)
/* 053FA8 7F0215B8 8DE10008 */  lw    $at, 8($t7)
/* 053FAC 7F0215BC AD090004 */  sw    $t1, 4($t0)
/* 053FB0 7F0215C0 AD010008 */  sw    $at, 8($t0)
/* 053FB4 7F0215C4 0FC14A59 */  jal   get_hat_model
/* 053FB8 7F0215C8 8E0401D8 */   lw    $a0, 0x1d8($s0)
/* 053FBC 7F0215CC AFA20060 */  sw    $v0, 0x60($sp)
/* 053FC0 7F0215D0 82040006 */  lb    $a0, 6($s0)
/* 053FC4 7F0215D4 00025880 */  sll   $t3, $v0, 2
/* 053FC8 7F0215D8 01625823 */  subu  $t3, $t3, $v0
/* 053FCC 7F0215DC 2484FFD6 */  addiu $a0, $a0, -0x2a
/* 053FD0 7F0215E0 000450C0 */  sll   $t2, $a0, 3
/* 053FD4 7F0215E4 01445021 */  addu  $t2, $t2, $a0
/* 053FD8 7F0215E8 000A5100 */  sll   $t2, $t2, 4
/* 053FDC 7F0215EC 000B58C0 */  sll   $t3, $t3, 3
/* 053FE0 7F0215F0 3C0C8004 */  lui   $t4, %hi(headHat_array_8003E464) # $t4, 0x8004
/* 053FE4 7F0215F4 258C80B4 */  addiu $t4, %lo(headHat_array_8003E464) # addiu $t4, $t4, -0x7f4c
/* 053FE8 7F0215F8 014B6821 */  addu  $t5, $t2, $t3
/* 053FEC 7F0215FC 01AC1821 */  addu  $v1, $t5, $t4
/* 053FF0 7F021600 0FC240F8 */  jal   get_debug_render_raster
/* 053FF4 7F021604 AFA30058 */   sw    $v1, 0x58($sp)
/* 053FF8 7F021608 144000AA */  bnez  $v0, .L7F0218B4
/* 053FFC 7F02160C 8FA30058 */   lw    $v1, 0x58($sp)
/* 054000 7F021610 AFA00054 */  sw    $zero, 0x54($sp)
/* 054004 7F021614 00002025 */  move  $a0, $zero
/* 054008 7F021618 24050020 */  li    $a1, 32
/* 05400C 7F02161C 0C002C20 */  jal   joyGetButtons
/* 054010 7F021620 AFA30058 */   sw    $v1, 0x58($sp)
/* 054014 7F021624 1040004F */  beqz  $v0, .L7F021764
/* 054018 7F021628 8FA30058 */   lw    $v1, 0x58($sp)
/* 05401C 7F02162C 00002025 */  move  $a0, $zero
/* 054020 7F021630 34058000 */  li    $a1, 32768
/* 054024 7F021634 0C002C20 */  jal   joyGetButtons
/* 054028 7F021638 AFA30058 */   sw    $v1, 0x58($sp)
/* 05402C 7F02163C 10400008 */  beqz  $v0, .L7F021660
/* 054030 7F021640 8FA30058 */   lw    $v1, 0x58($sp)
/* 054034 7F021644 3C018004 */  lui   $at, %hi(D_80051D58) # $at, 0x8004
/* 054038 7F021648 C4327E84 */  lwc1  $f18, %lo(D_80051D58)($at)
/* 05403C 7F02164C C4700008 */  lwc1  $f16, 8($v1)
/* 054040 7F021650 24180001 */  li    $t8, 1
/* 054044 7F021654 46128101 */  sub.s $f4, $f16, $f18
/* 054048 7F021658 E4640008 */  swc1  $f4, 8($v1)
/* 05404C 7F02165C AFB80054 */  sw    $t8, 0x54($sp)
.L7F021660:
/* 054050 7F021660 00002025 */  move  $a0, $zero
/* 054054 7F021664 24054000 */  li    $a1, 16384
/* 054058 7F021668 0C002C20 */  jal   joyGetButtons
/* 05405C 7F02166C AFA30058 */   sw    $v1, 0x58($sp)
/* 054060 7F021670 10400008 */  beqz  $v0, .L7F021694
/* 054064 7F021674 8FA30058 */   lw    $v1, 0x58($sp)
/* 054068 7F021678 3C018004 */  lui   $at, %hi(D_80051D5C) # $at, 0x8004
/* 05406C 7F02167C C4287E88 */  lwc1  $f8, %lo(D_80051D5C)($at)
/* 054070 7F021680 C4660008 */  lwc1  $f6, 8($v1)
/* 054074 7F021684 24190001 */  li    $t9, 1
/* 054078 7F021688 46083280 */  add.s $f10, $f6, $f8
/* 05407C 7F02168C E46A0008 */  swc1  $f10, 8($v1)
/* 054080 7F021690 AFB90054 */  sw    $t9, 0x54($sp)
.L7F021694:
/* 054084 7F021694 00002025 */  move  $a0, $zero
/* 054088 7F021698 24050004 */  li    $a1, 4
/* 05408C 7F02169C 0C002C20 */  jal   joyGetButtons
/* 054090 7F0216A0 AFA30058 */   sw    $v1, 0x58($sp)
/* 054094 7F0216A4 10400008 */  beqz  $v0, .L7F0216C8
/* 054098 7F0216A8 8FA30058 */   lw    $v1, 0x58($sp)
/* 05409C 7F0216AC 3C018004 */  lui   $at, %hi(D_80051D60) # $at, 0x8004
/* 0540A0 7F0216B0 C4327E8C */  lwc1  $f18, %lo(D_80051D60)($at)
/* 0540A4 7F0216B4 C4700004 */  lwc1  $f16, 4($v1)
/* 0540A8 7F0216B8 240E0001 */  li    $t6, 1
/* 0540AC 7F0216BC 46128101 */  sub.s $f4, $f16, $f18
/* 0540B0 7F0216C0 E4640004 */  swc1  $f4, 4($v1)
/* 0540B4 7F0216C4 AFAE0054 */  sw    $t6, 0x54($sp)
.L7F0216C8:
/* 0540B8 7F0216C8 00002025 */  move  $a0, $zero
/* 0540BC 7F0216CC 24050008 */  li    $a1, 8
/* 0540C0 7F0216D0 0C002C20 */  jal   joyGetButtons
/* 0540C4 7F0216D4 AFA30058 */   sw    $v1, 0x58($sp)
/* 0540C8 7F0216D8 10400008 */  beqz  $v0, .L7F0216FC
/* 0540CC 7F0216DC 8FA30058 */   lw    $v1, 0x58($sp)
/* 0540D0 7F0216E0 3C018004 */  lui   $at, %hi(D_80051D64) # $at, 0x8004
/* 0540D4 7F0216E4 C4287E90 */  lwc1  $f8, %lo(D_80051D64)($at)
/* 0540D8 7F0216E8 C4660004 */  lwc1  $f6, 4($v1)
/* 0540DC 7F0216EC 24080001 */  li    $t0, 1
/* 0540E0 7F0216F0 46083280 */  add.s $f10, $f6, $f8
/* 0540E4 7F0216F4 E46A0004 */  swc1  $f10, 4($v1)
/* 0540E8 7F0216F8 AFA80054 */  sw    $t0, 0x54($sp)
.L7F0216FC:
/* 0540EC 7F0216FC 00002025 */  move  $a0, $zero
/* 0540F0 7F021700 24050002 */  li    $a1, 2
/* 0540F4 7F021704 0C002C20 */  jal   joyGetButtons
/* 0540F8 7F021708 AFA30058 */   sw    $v1, 0x58($sp)
/* 0540FC 7F02170C 10400008 */  beqz  $v0, .L7F021730
/* 054100 7F021710 8FA30058 */   lw    $v1, 0x58($sp)
/* 054104 7F021714 3C018004 */  lui   $at, %hi(D_80051D68) # $at, 0x8004
/* 054108 7F021718 C4327E94 */  lwc1  $f18, %lo(D_80051D68)($at)
/* 05410C 7F02171C C4700000 */  lwc1  $f16, ($v1)
/* 054110 7F021720 240F0001 */  li    $t7, 1
/* 054114 7F021724 46128101 */  sub.s $f4, $f16, $f18
/* 054118 7F021728 E4640000 */  swc1  $f4, ($v1)
/* 05411C 7F02172C AFAF0054 */  sw    $t7, 0x54($sp)
.L7F021730:
/* 054120 7F021730 00002025 */  move  $a0, $zero
/* 054124 7F021734 24050001 */  li    $a1, 1
/* 054128 7F021738 0C002C20 */  jal   joyGetButtons
/* 05412C 7F02173C AFA30058 */   sw    $v1, 0x58($sp)
/* 054130 7F021740 10400008 */  beqz  $v0, .L7F021764
/* 054134 7F021744 8FA30058 */   lw    $v1, 0x58($sp)
/* 054138 7F021748 3C018004 */  lui   $at, %hi(D_80051D6C) # $at, 0x8004
/* 05413C 7F02174C C4287E98 */  lwc1  $f8, %lo(D_80051D6C)($at)
/* 054140 7F021750 C4660000 */  lwc1  $f6, ($v1)
/* 054144 7F021754 24090001 */  li    $t1, 1
/* 054148 7F021758 46083280 */  add.s $f10, $f6, $f8
/* 05414C 7F02175C E46A0000 */  swc1  $f10, ($v1)
/* 054150 7F021760 AFA90054 */  sw    $t1, 0x54($sp)
.L7F021764:
/* 054154 7F021764 00002025 */  move  $a0, $zero
/* 054158 7F021768 24050010 */  li    $a1, 16
/* 05415C 7F02176C 0C002C20 */  jal   joyGetButtons
/* 054160 7F021770 AFA30058 */   sw    $v1, 0x58($sp)
/* 054164 7F021774 1040004F */  beqz  $v0, .L7F0218B4
/* 054168 7F021778 8FA30058 */   lw    $v1, 0x58($sp)
/* 05416C 7F02177C 00002025 */  move  $a0, $zero
/* 054170 7F021780 34058000 */  li    $a1, 32768
/* 054174 7F021784 0C002C20 */  jal   joyGetButtons
/* 054178 7F021788 AFA30058 */   sw    $v1, 0x58($sp)
/* 05417C 7F02178C 10400008 */  beqz  $v0, .L7F0217B0
/* 054180 7F021790 8FA30058 */   lw    $v1, 0x58($sp)
/* 054184 7F021794 3C018004 */  lui   $at, %hi(D_80051D70) # $at, 0x8004
/* 054188 7F021798 C4327E9C */  lwc1  $f18, %lo(D_80051D70)($at)
/* 05418C 7F02179C C4700014 */  lwc1  $f16, 0x14($v1)
/* 054190 7F0217A0 240A0001 */  li    $t2, 1
/* 054194 7F0217A4 46128102 */  mul.s $f4, $f16, $f18
/* 054198 7F0217A8 E4640014 */  swc1  $f4, 0x14($v1)
/* 05419C 7F0217AC AFAA0054 */  sw    $t2, 0x54($sp)
.L7F0217B0:
/* 0541A0 7F0217B0 00002025 */  move  $a0, $zero
/* 0541A4 7F0217B4 24054000 */  li    $a1, 16384
/* 0541A8 7F0217B8 0C002C20 */  jal   joyGetButtons
/* 0541AC 7F0217BC AFA30058 */   sw    $v1, 0x58($sp)
/* 0541B0 7F0217C0 10400008 */  beqz  $v0, .L7F0217E4
/* 0541B4 7F0217C4 8FA30058 */   lw    $v1, 0x58($sp)
/* 0541B8 7F0217C8 3C018004 */  lui   $at, %hi(D_80051D74) # $at, 0x8004
/* 0541BC 7F0217CC C4287EA0 */  lwc1  $f8, %lo(D_80051D74)($at)
/* 0541C0 7F0217D0 C4660014 */  lwc1  $f6, 0x14($v1)
/* 0541C4 7F0217D4 240B0001 */  li    $t3, 1
/* 0541C8 7F0217D8 46083282 */  mul.s $f10, $f6, $f8
/* 0541CC 7F0217DC E46A0014 */  swc1  $f10, 0x14($v1)
/* 0541D0 7F0217E0 AFAB0054 */  sw    $t3, 0x54($sp)
.L7F0217E4:
/* 0541D4 7F0217E4 00002025 */  move  $a0, $zero
/* 0541D8 7F0217E8 24050004 */  li    $a1, 4
/* 0541DC 7F0217EC 0C002C20 */  jal   joyGetButtons
/* 0541E0 7F0217F0 AFA30058 */   sw    $v1, 0x58($sp)
/* 0541E4 7F0217F4 10400008 */  beqz  $v0, .L7F021818
/* 0541E8 7F0217F8 8FA30058 */   lw    $v1, 0x58($sp)
/* 0541EC 7F0217FC 3C018004 */  lui   $at, %hi(D_80051D78) # $at, 0x8004
/* 0541F0 7F021800 C4327EA4 */  lwc1  $f18, %lo(D_80051D78)($at)
/* 0541F4 7F021804 C4700010 */  lwc1  $f16, 0x10($v1)
/* 0541F8 7F021808 240D0001 */  li    $t5, 1
/* 0541FC 7F02180C 46128102 */  mul.s $f4, $f16, $f18
/* 054200 7F021810 E4640010 */  swc1  $f4, 0x10($v1)
/* 054204 7F021814 AFAD0054 */  sw    $t5, 0x54($sp)
.L7F021818:
/* 054208 7F021818 00002025 */  move  $a0, $zero
/* 05420C 7F02181C 24050008 */  li    $a1, 8
/* 054210 7F021820 0C002C20 */  jal   joyGetButtons
/* 054214 7F021824 AFA30058 */   sw    $v1, 0x58($sp)
/* 054218 7F021828 10400008 */  beqz  $v0, .L7F02184C
/* 05421C 7F02182C 8FA30058 */   lw    $v1, 0x58($sp)
/* 054220 7F021830 3C018004 */  lui   $at, %hi(D_80051D7C) # $at, 0x8004
/* 054224 7F021834 C4287EA8 */  lwc1  $f8, %lo(D_80051D7C)($at)
/* 054228 7F021838 C4660010 */  lwc1  $f6, 0x10($v1)
/* 05422C 7F02183C 240C0001 */  li    $t4, 1
/* 054230 7F021840 46083282 */  mul.s $f10, $f6, $f8
/* 054234 7F021844 E46A0010 */  swc1  $f10, 0x10($v1)
/* 054238 7F021848 AFAC0054 */  sw    $t4, 0x54($sp)
.L7F02184C:
/* 05423C 7F02184C 00002025 */  move  $a0, $zero
/* 054240 7F021850 24050002 */  li    $a1, 2
/* 054244 7F021854 0C002C20 */  jal   joyGetButtons
/* 054248 7F021858 AFA30058 */   sw    $v1, 0x58($sp)
/* 05424C 7F02185C 10400008 */  beqz  $v0, .L7F021880
/* 054250 7F021860 8FA30058 */   lw    $v1, 0x58($sp)
/* 054254 7F021864 3C018004 */  lui   $at, %hi(D_80051D80) # $at, 0x8004
/* 054258 7F021868 C4327EAC */  lwc1  $f18, %lo(D_80051D80)($at)
/* 05425C 7F02186C C470000C */  lwc1  $f16, 0xc($v1)
/* 054260 7F021870 24180001 */  li    $t8, 1
/* 054264 7F021874 46128102 */  mul.s $f4, $f16, $f18
/* 054268 7F021878 E464000C */  swc1  $f4, 0xc($v1)
/* 05426C 7F02187C AFB80054 */  sw    $t8, 0x54($sp)
.L7F021880:
/* 054270 7F021880 00002025 */  move  $a0, $zero
/* 054274 7F021884 24050001 */  li    $a1, 1
/* 054278 7F021888 0C002C20 */  jal   joyGetButtons
/* 05427C 7F02188C AFA30058 */   sw    $v1, 0x58($sp)
/* 054280 7F021890 10400008 */  beqz  $v0, .L7F0218B4
/* 054284 7F021894 8FA30058 */   lw    $v1, 0x58($sp)
/* 054288 7F021898 3C018004 */  lui   $at, %hi(D_80051D84) # $at, 0x8004
/* 05428C 7F02189C C4287EB0 */  lwc1  $f8, %lo(D_80051D84)($at)
/* 054290 7F0218A0 C466000C */  lwc1  $f6, 0xc($v1)
/* 054294 7F0218A4 24190001 */  li    $t9, 1
/* 054298 7F0218A8 46083282 */  mul.s $f10, $f6, $f8
/* 05429C 7F0218AC E46A000C */  swc1  $f10, 0xc($v1)
/* 0542A0 7F0218B0 AFB90054 */  sw    $t9, 0x54($sp)
.L7F0218B4:
/* 0542A4 7F0218B4 3C018004 */  lui   $at, %hi(D_80051D88) # $at, 0x8004
/* 0542A8 7F0218B8 C4207EB4 */  lwc1  $f0, %lo(D_80051D88)($at)
/* 0542AC 7F0218BC C4700000 */  lwc1  $f16, ($v1)
/* 0542B0 7F0218C0 27A400F0 */  addiu $a0, $sp, 0xf0
/* 0542B4 7F0218C4 27A500A4 */  addiu $a1, $sp, 0xa4
/* 0542B8 7F0218C8 46008482 */  mul.s $f18, $f16, $f0
/* 0542BC 7F0218CC E7B200F0 */  swc1  $f18, 0xf0($sp)
/* 0542C0 7F0218D0 C4640004 */  lwc1  $f4, 4($v1)
/* 0542C4 7F0218D4 46002182 */  mul.s $f6, $f4, $f0
/* 0542C8 7F0218D8 E7A600F4 */  swc1  $f6, 0xf4($sp)
/* 0542CC 7F0218DC C4680008 */  lwc1  $f8, 8($v1)
/* 0542D0 7F0218E0 46004282 */  mul.s $f10, $f8, $f0
/* 0542D4 7F0218E4 E7AA00F8 */  swc1  $f10, 0xf8($sp)
/* 0542D8 7F0218E8 C470000C */  lwc1  $f16, 0xc($v1)
/* 0542DC 7F0218EC E7B000EC */  swc1  $f16, 0xec($sp)
/* 0542E0 7F0218F0 C4720010 */  lwc1  $f18, 0x10($v1)
/* 0542E4 7F0218F4 E7B200E8 */  swc1  $f18, 0xe8($sp)
/* 0542E8 7F0218F8 C4640014 */  lwc1  $f4, 0x14($v1)
/* 0542EC 7F0218FC 0FC16383 */  jal   matrix_4x4_set_identity_and_position
/* 0542F0 7F021900 E7A400E4 */   swc1  $f4, 0xe4($sp)
/* 0542F4 7F021904 C7AC00EC */  lwc1  $f12, 0xec($sp)
/* 0542F8 7F021908 0FC16397 */  jal   matrix_column_1_scalar_multiply
/* 0542FC 7F02190C 27A500A4 */   addiu $a1, $sp, 0xa4
/* 054300 7F021910 C7AC00E8 */  lwc1  $f12, 0xe8($sp)
/* 054304 7F021914 0FC163A3 */  jal   matrix_column_2_scalar_multiply
/* 054308 7F021918 27A500A4 */   addiu $a1, $sp, 0xa4
/* 05430C 7F02191C C7AC00E4 */  lwc1  $f12, 0xe4($sp)
/* 054310 7F021920 0FC163BD */  jal   matrix_column_3_scalar_multiply_2
/* 054314 7F021924 27A500A4 */   addiu $a1, $sp, 0xa4
/* 054318 7F021928 8FAE00FC */  lw    $t6, 0xfc($sp)
/* 05431C 7F02192C 27A500A4 */  addiu $a1, $sp, 0xa4
/* 054320 7F021930 27A60064 */  addiu $a2, $sp, 0x64
/* 054324 7F021934 0FC1618D */  jal   matrix_4x4_multiply_homogeneous
/* 054328 7F021938 8DC4000C */   lw    $a0, 0xc($t6)
/* 05432C 7F02193C 8FA800FC */  lw    $t0, 0xfc($sp)
/* 054330 7F021940 27A40064 */  addiu $a0, $sp, 0x64
/* 054334 7F021944 0FC16132 */  jal   matrix_4x4_copy
/* 054338 7F021948 8D05000C */   lw    $a1, 0xc($t0)
/* 05433C 7F02194C 8FAF0060 */  lw    $t7, 0x60($sp)
/* 054340 7F021950 24010002 */  li    $at, 2
/* 054344 7F021954 55E10003 */  bnel  $t7, $at, .L7F021964
/* 054348 7F021958 96090012 */   lhu   $t1, 0x12($s0)
/* 05434C 7F02195C AFA00108 */  sw    $zero, 0x108($sp)
.L7F021960:
/* 054350 7F021960 96090012 */  lhu   $t1, 0x12($s0)
.L7F021964:
/* 054354 7F021964 8FAB0100 */  lw    $t3, 0x100($sp)
/* 054358 7F021968 8FA500FC */  lw    $a1, 0xfc($sp)
/* 05435C 7F02196C 312A0001 */  andi  $t2, $t1, 1
/* 054360 7F021970 11400005 */  beqz  $t2, .L7F021988
/* 054364 7F021974 00000000 */   nop
/* 054368 7F021978 8D6D0064 */  lw    $t5, 0x64($t3)
/* 05436C 7F02197C 31AC0080 */  andi  $t4, $t5, 0x80
/* 054370 7F021980 55800005 */  bnezl $t4, .L7F021998
/* 054374 7F021984 8FB80110 */   lw    $t8, 0x110($sp)
.L7F021988:
/* 054378 7F021988 0FC1AE34 */  jal   sub_GAME_7F06B120
/* 05437C 7F02198C 8E040020 */   lw    $a0, 0x20($s0)
/* 054380 7F021990 AE020020 */  sw    $v0, 0x20($s0)
/* 054384 7F021994 8FB80110 */  lw    $t8, 0x110($sp)
.L7F021998:
/* 054388 7F021998 8F190008 */  lw    $t9, 8($t8)
/* 05438C 7F02199C 8F2E0008 */  lw    $t6, 8($t9)
/* 054390 7F0219A0 8DC50010 */  lw    $a1, 0x10($t6)
/* 054394 7F0219A4 10A0000E */  beqz  $a1, .L7F0219E0
/* 054398 7F0219A8 00000000 */   nop
/* 05439C 7F0219AC 0FC1B3A3 */  jal   modelGetNodeRwData
/* 0543A0 7F0219B0 03002025 */   move  $a0, $t8
/* 0543A4 7F0219B4 8C430000 */  lw    $v1, ($v0)
/* 0543A8 7F0219B8 10600009 */  beqz  $v1, .L7F0219E0
/* 0543AC 7F0219BC 00000000 */   nop
/* 0543B0 7F0219C0 8C680008 */  lw    $t0, 8($v1)
/* 0543B4 7F0219C4 8D050004 */  lw    $a1, 4($t0)
/* 0543B8 7F0219C8 10A00005 */  beqz  $a1, .L7F0219E0
/* 0543BC 7F0219CC 00000000 */   nop
/* 0543C0 7F0219D0 0FC1B3A3 */  jal   modelGetNodeRwData
/* 0543C4 7F0219D4 8FA40110 */   lw    $a0, 0x110($sp)
/* 0543C8 7F0219D8 8FAF0108 */  lw    $t7, 0x108($sp)
/* 0543CC 7F0219DC AC4F0000 */  sw    $t7, ($v0)
.L7F0219E0:
/* 0543D0 7F0219E0 0FC1AE93 */  jal   sub_GAME_7F06B29C
/* 0543D4 7F0219E4 8E040020 */   lw    $a0, 0x20($s0)
/* 0543D8 7F0219E8 0FC1B0B6 */  jal   sub_GAME_7F06BB28
/* 0543DC 7F0219EC 8E040020 */   lw    $a0, 0x20($s0)
/* 0543E0 7F0219F0 10000020 */  b     .L7F021A74
/* 0543E4 7F0219F4 AE020020 */   sw    $v0, 0x20($s0)
/* 0543E8 7F0219F8 8E020160 */  lw    $v0, 0x160($s0)
.L7F0219FC:
/* 0543EC 7F0219FC 50400005 */  beql  $v0, $zero, .L7F021A14
/* 0543F0 7F021A00 8E020164 */   lw    $v0, 0x164($s0)
/* 0543F4 7F021A04 90490001 */  lbu   $t1, 1($v0)
/* 0543F8 7F021A08 312AFFFD */  andi  $t2, $t1, 0xfffd
/* 0543FC 7F021A0C A04A0001 */  sb    $t2, 1($v0)
/* 054400 7F021A10 8E020164 */  lw    $v0, 0x164($s0)
.L7F021A14:
/* 054404 7F021A14 2403FFFD */  li    $v1, -3
/* 054408 7F021A18 50400005 */  beql  $v0, $zero, .L7F021A30
/* 05440C 7F021A1C 8E0201D8 */   lw    $v0, 0x1d8($s0)
/* 054410 7F021A20 904B0001 */  lbu   $t3, 1($v0)
/* 054414 7F021A24 01636824 */  and   $t5, $t3, $v1
/* 054418 7F021A28 A04D0001 */  sb    $t5, 1($v0)
/* 05441C 7F021A2C 8E0201D8 */  lw    $v0, 0x1d8($s0)
.L7F021A30:
/* 054420 7F021A30 50400005 */  beql  $v0, $zero, .L7F021A48
/* 054424 7F021A34 8FAE0158 */   lw    $t6, 0x158($sp)
/* 054428 7F021A38 904C0001 */  lbu   $t4, 1($v0)
/* 05442C 7F021A3C 0183C824 */  and   $t9, $t4, $v1
/* 054430 7F021A40 A0590001 */  sb    $t9, 1($v0)
/* 054434 7F021A44 8FAE0158 */  lw    $t6, 0x158($sp)
.L7F021A48:
/* 054438 7F021A48 91D80001 */  lbu   $t8, 1($t6)
/* 05443C 7F021A4C 03034024 */  and   $t0, $t8, $v1
/* 054440 7F021A50 A1C80001 */  sb    $t0, 1($t6)
/* 054444 7F021A54 920F00F8 */  lbu   $t7, 0xf8($s0)
/* 054448 7F021A58 920900F9 */  lbu   $t1, 0xf9($s0)
/* 05444C 7F021A5C 920A00FA */  lbu   $t2, 0xfa($s0)
/* 054450 7F021A60 920B00FB */  lbu   $t3, 0xfb($s0)
/* 054454 7F021A64 A20F00F4 */  sb    $t7, 0xf4($s0)
/* 054458 7F021A68 A20900F5 */  sb    $t1, 0xf5($s0)
/* 05445C 7F021A6C A20A00F6 */  sb    $t2, 0xf6($s0)
/* 054460 7F021A70 A20B00F7 */  sb    $t3, 0xf7($s0)
.L7F021A74:
/* 054464 7F021A74 8E0D0014 */  lw    $t5, 0x14($s0)
/* 054468 7F021A78 31AC0400 */  andi  $t4, $t5, 0x400
/* 05446C 7F021A7C 55800015 */  bnezl $t4, .L7F021AD4
/* 054470 7F021A80 00001025 */   move  $v0, $zero
/* 054474 7F021A84 96020012 */  lhu   $v0, 0x12($s0)
/* 054478 7F021A88 8FB80158 */  lw    $t8, 0x158($sp)
/* 05447C 7F021A8C 30590001 */  andi  $t9, $v0, 1
/* 054480 7F021A90 1320000D */  beqz  $t9, .L7F021AC8
/* 054484 7F021A94 00000000 */   nop
/* 054488 7F021A98 8F040020 */  lw    $a0, 0x20($t8)
/* 05448C 7F021A9C 50800009 */  beql  $a0, $zero, .L7F021AC4
/* 054490 7F021AA0 3048FFFE */   andi  $t0, $v0, 0xfffe
/* 054494 7F021AA4 8C830024 */  lw    $v1, 0x24($a0)
.L7F021AA8:
/* 054498 7F021AA8 0FC130C9 */  jal   objDrop
/* 05449C 7F021AAC AFA3003C */   sw    $v1, 0x3c($sp)
/* 0544A0 7F021AB0 8FA4003C */  lw    $a0, 0x3c($sp)
/* 0544A4 7F021AB4 5480FFFC */  bnezl $a0, .L7F021AA8
/* 0544A8 7F021AB8 8C830024 */   lw    $v1, 0x24($a0)
/* 0544AC 7F021ABC 96020012 */  lhu   $v0, 0x12($s0)
/* 0544B0 7F021AC0 3048FFFE */  andi  $t0, $v0, 0xfffe
.L7F021AC4:
/* 0544B4 7F021AC4 A6080012 */  sh    $t0, 0x12($s0)
.L7F021AC8:
/* 0544B8 7F021AC8 0FC0B8A8 */  jal   chrlvTriggerFireWeapon
/* 0544BC 7F021ACC 02002025 */   move  $a0, $s0
/* 0544C0 7F021AD0 00001025 */  move  $v0, $zero
.L7F021AD4:
/* 0544C4 7F021AD4 8FBF0024 */  lw    $ra, 0x24($sp)
/* 0544C8 7F021AD8 8FB00020 */  lw    $s0, 0x20($sp)
/* 0544CC 7F021ADC 27BD0158 */  addiu $sp, $sp, 0x158
/* 0544D0 7F021AE0 03E00008 */  jr    $ra
/* 0544D4 7F021AE4 00000000 */   nop

)
#endif

#endif


/**
 * Address 0x7F021B20.
 */
void chrDropItems(ChrRecord *self)
{
    PropRecord *childprop = self->prop->child;
    while (childprop)
    {
        if ((childprop != self->handle_positiondata_hat) &&
            (childprop != self->weapons_held[GUNLEFT]) &&
            (childprop != self->weapons_held[GUNRIGHT]))
        {
            WeaponObjRecord *wep = childprop->weapon;
            if (!(wep->flags & 0x2000))
            {
                propobjSetDropped(childprop, 1);
            }
        }
        childprop = childprop->prev;
    };

    self->hidden |= 1;
}



/**
 * Unreferenced.
 *
 * Sets gBloodColour 3 bytes from paramter.
 *
 * @param colour: rgba_u8.
 *
 * Address 0x7F021BB4.
 */
void chrSetgBloodColour(rgba_u8 *colour)
{
    gBloodColour.r = colour->r;
    gBloodColour.g = colour->g;
    gBloodColour.b = colour->b;
}


/**
 * Unreferenced.
 *
 * Gets gBloodColour 3 bytes and sets them into parameter.
 *
 * @param colour: rgba_u8.
 *
 * Address 0x7F021BD8.
 */
void chrGetgBloodColour(rgba_u8 *colour)
{
    colour->r = gBloodColour.r;
    colour->g = gBloodColour.g;
    colour->b = gBloodColour.b;
}



/**
 * Address 0x7F021BFC.
*/
Gfx *chrRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    ChrRecord *chr;
    Model *chrmodel;
    struct rgba_f32 spC0; // 192
    s32 spBC; // 188
    s32 spB8; // 184
    s32 chrfadealpha; // 180
    rgba_u8 temp_v1_2;
    ModelRenderData mrData; // 112
    struct view4f sp60; // -?? 96
    struct rgba_s32 chrShade;
    s32 sp4C; // 76
    PropRecord *prop_held_right; // 72
    PropRecord *prop_held_left; // 68
    PropRecord *prop_held_hat; // 64
    ObjectRecord *held_right_obj; // 60
    ObjectRecord *held_left_obj; // 56
    ObjectRecord *held_hat_obj; // 52

    //

    chr = prop->chr;
    chrmodel = chr->model;
    chrfadealpha = (s32) chr->fadealpha;

    if (!(chr->chrflags & CHRFLAG_04000000))
    {
        f32 f = chrobjFogVisRangeRelated(prop, getinstsize(chrmodel)); //0-1
        chrfadealpha = (s32) (f * (f32) chrfadealpha);
    }

    if ((chrfadealpha < 0xFF) || (chr->chrflags & CHRFLAG_00020000))
    {
        if (withalpha == 0)
        {
            // nothing to do
            return gdl;
        }
        else
        {
            spB8 = 3;
        }
    }
    else
    {
        if (withalpha == 0)
        {
            spB8 = 1;
        }
        else
        {
            spB8 = 2;
        }
    }

    spBC = fogGetPropDistColor(prop, &spC0);
    if (spBC != 0)
    {
        if (chrfadealpha > 0)
        {
            mrData = D_8002CCBC;


            sp4C = 0x50;

            prop_held_right = chr->weapons_held[GUNRIGHT];
            prop_held_left = chr->weapons_held[GUNLEFT];
            prop_held_hat = chr->handle_positiondata_hat;
            held_right_obj = NULL;
            held_left_obj = NULL;
            held_hat_obj = NULL;

            if (prop_held_right != NULL)
            {
                held_right_obj = prop_held_right->obj;
            }

            if (prop_held_left != NULL)
            {
                held_left_obj = prop_held_left->obj;
            }

            if (prop_held_hat != NULL)
            {
                held_hat_obj = prop_held_hat->obj;
            }

            if ((getPropCombinedRoomsBBox2D(prop, &sp60) > 0) && !(chr->chrflags & CHRFLAG_CULL_USING_HITBOX))
            {
                gdl = bgScissorCurrentPlayerViewF(gdl, sp60.left, sp60.top, sp60.width, sp60.height);
            }
            else
            {
                gdl = bgScissorCurrentPlayerViewDefault(gdl);
            }

            mrData.flags = spB8;
            mrData.zbufferenabled = TRUE;
            mrData.gdl = gdl;

            if ((chr->chrflags & CHRFLAG_NO_SHADOW) != 0)
            {
                sp4C = 0;
            }
            else if (spBC == 1)
            {
                sp4C = ((1.0f - spC0.a) * (f32)(sp4C));
            }

            sub_GAME_7F073FC8(sp4C);

            chrShade.r = chr->shadecol.r;
            chrShade.g = chr->shadecol.g;
            chrShade.b = chr->shadecol.b;
            chrShade.a = chr->shadecol.a;

            lerp_rgba_s32_with_rgba_f32(&chrShade, spBC, &spC0);

            mrData.envcolour.word = ((gBloodColour.rgba[0] << 0x18) | (gBloodColour.rgba[1] << 0x10)) | (gBloodColour.rgba[2] << 0x08);
            mrData.fogcolour.word = (chrShade.rgba[0] << 0x18) | (chrShade.rgba[1] << 0x10) | (chrShade.rgba[2] << 0x08) | (chrShade.rgba[3] << 0x00);

            if (chrfadealpha < 0xFF)
            {
                mrData.PropType = 8;
                mrData.envcolour.word |= (u8)chrfadealpha;
            }
            else
            {
                mrData.PropType = 7;
            }

            g_playerPerm->time_other_players_on_screen += 1;
            drawjointlist(&mrData, chr->field_20);

            gdl = mrData.gdl;

            if ((held_right_obj != NULL) && (( held_right_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = explosionRenderBulletImpactOnProp(gdl, prop_held_right, withalpha);
            }

            if ((held_left_obj != NULL) && (( held_left_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = explosionRenderBulletImpactOnProp(gdl, prop_held_left, withalpha);
            }

            if ((held_hat_obj != NULL) && (( held_hat_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = explosionRenderBulletImpactOnProp(gdl, prop_held_hat, withalpha);
            }

            if (withalpha != 0)
            {
                bondviewTransformManyPosToViewMatrix(chr->model->render_pos, chr->model->obj->numMatrices);

                if ((held_right_obj != NULL) && ((held_right_obj->runtime_bitflags & 0x800) == 0))
                {
                    bondviewTransformManyPosToViewMatrix(held_right_obj->model->render_pos, held_right_obj->model->obj->numMatrices);
                }

                if ((held_left_obj != NULL) && ((held_left_obj->runtime_bitflags & 0x800) == 0))
                {
                    bondviewTransformManyPosToViewMatrix(held_left_obj->model->render_pos, held_left_obj->model->obj->numMatrices);
                }

                if (held_hat_obj != NULL)
                {
                    bondviewTransformManyPosToViewMatrix(held_hat_obj->model->render_pos, held_hat_obj->model->obj->numMatrices);
                }
            }
        }
    }

    if (withalpha != 0)
    {
        sub_GAME_7F06B248(chr->field_20);
        chr->field_20 = NULL;
    }

    return gdl;
}


/**
 * Creates a smoke puff at the front of the character when the character is shot,
 * and also has a ~50% chance of creating a second smoke puff just behind the character.
 */
void chrCreateHitPuffs(PropRecord *prop, s32 anim_id, coord3d *vec, coord3d *pos)
{
    s32 i;
    f32 scale;
    coord3d sp3c;
    s32 index;
    struct animation_something *entry;

    index = 0;
    i = 0;

    if (D_8002C914[0].id != -1) {
        do {
            if (anim_id == D_8002C914[i].id) {
                index = i;
                break;
            }

            i++;
        }
        while (D_8002C914[i].id != -1);
    }

    entry = &D_8002C914[index];

    if (entry->field_10) {
        // True when randomGetNext() bit 2 is 0, so roughly 50% chance.
        if ((randomGetNext() & 4) == 0) {
            scale = (42.0f / sqrtf(vec->z * vec->z + (vec->x * vec->x + vec->y * vec->y))) + 1.0f;

            sp3c.x = vec->x * scale;
            sp3c.y = vec->y * scale;
            sp3c.z = vec->z * scale;

            mtx4TransformVecInPlace(currentPlayerGetViewToWorldMtxf(), &sp3c);

            bullet_spark_create(&sp3c, entry->field_10, entry->field_18, prop->stan->room);
        }
    }

    if (entry->field_4) {
        bullet_spark_create(pos, entry->field_4, entry->field_C, prop->stan->room);
    }
}


void chrCreateBloodStain(Model *model, s32 arg1, ModelNode *root, struct coord3d *pos)
{
    s32 sp_pos[3];
    s32 unused_stack_pad[1];
    s32 bestdist;
    ModelNode *bestnode;
    s32 bestindex;
    ModelNode *node;
    s32 opcode;
    ModelRoData_DisplayList_CollisionRecord *rodata;
    ModelRoData_DisplayList_CollisionRecord *relatedrodata;
    ModelRwData_DisplayList_CollisionRecord *relatedrwdata;
    ModelNode *relatednode;
    ModelNode *rwdata;
    Vertex *vtx;
    Vertex *newvertices;
    s32 i;
    s32 n;
    s32 paintval;
    s32 relatedindex;

    sp_pos[0] = pos->x;
    sp_pos[1] = pos->y;
    sp_pos[2] = pos->z;

    bestnode = NULL;
    bestindex = 0;
    bestdist = 0x7fffffff;
    node = root;

    while (node != NULL)
    {
        opcode = node->Opcode & 0xff;

        if (opcode == MODELNODE_OPCODE_LOD)
        {
            goto apply_distance;
        }

        if (opcode == MODELNODE_OPCODE_SWITCH)
        {
            goto apply_toggle;
        }

        if (opcode == MODELNODE_OPCODE_HEAD)
        {
            goto apply_head;
        }

        if (opcode != MODELNODE_OPCODE_DLCOLLISION)
        {
            goto after_opcode;
        }

        rodata = &node->Data->DisplayListCollisions;
        n = 0;

        if (rodata->numCollisionVertices > 0)
        {
            vtx = rodata->CollisionVertices;

            do
            {
                s32 dx;
                s32 dy;
                s32 dz;
                s32 dist;

                dx = sp_pos[0] - vtx->coord.x;
                dy = sp_pos[1] - vtx->coord.y;
                dz = sp_pos[2] - vtx->coord.z;
                dist = ((((u32) dx) * ((u32) dx)) + (((u32) dy) * ((u32) dy))) + (((u32) dz) * ((u32) dz));

                if (dist < bestdist)
                {
                    bestdist = dist;
                    bestnode = node;
                    bestindex = n;
                }

                n++;
                vtx++;
            }
            while (n < rodata->numCollisionVertices);
        }

        goto after_opcode;

apply_distance:
        modelApplyDistanceRelations(model, node);
        goto after_opcode;

apply_toggle:
        modelApplyToggleRelations(model, node);
        goto after_opcode;

apply_head:
        modelApplyHeadRelations(model, node);

after_opcode:
        if ((node->Child != NULL) && ((node == root) || ((opcode != MODELNODE_OPCODE_BBOX) && (opcode != MODELNODE_OPCODE_OP17))))
        {
            node = node->Child;
        }
        else
        {
            while (node != NULL)
            {
                if (node == root)
                {
                    node = NULL;
                    break;
                }

                if (node->Next != NULL)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    if (bestnode == NULL)
    {
        return;
    }
    
    node = (ModelNode *) bestnode->Data;
    rwdata = (ModelNode *) modelGetNodeRwData(model, bestnode);
    relatedrodata = NULL;
    relatedrwdata = NULL;
    relatedindex = 0;
    paintval = (randomGetNext() % 50) + 20;
    
    if (arg1 == 8)
    {
        paintval += 100;
    }
    
    if (arg1 == 0x0f)
    {
        paintval += 50;
    }
    
    relatednode = (ModelNode *) ((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].CollisionRelatedNode;
    
    if (((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].CollisionRelatedNode != NULL)
    {
        relatedrodata = &relatednode->Data->DisplayListCollisions;
        relatedrwdata = (ModelRwData_DisplayList_CollisionRecord *) modelGetNodeRwData(model, relatednode);
        relatedindex = ((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].CollisionRelatedIndex;
    }
    
    if (((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices == ((ModelRoData_DisplayList_CollisionRecord *) node)->Vertices)
    {
        newvertices = (Vertex *) vtxstore_allocate(((ModelRoData_DisplayList_CollisionRecord *) node)->numVertices, 0xcccc, 0, 0);
    
        if (newvertices != NULL)
        {
            s32 j;
    
            ((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices = newvertices;
    
            for (j = 0; j < ((ModelRoData_DisplayList_CollisionRecord *) node)->numVertices; j++)
            {
                newvertices[j] = ((ModelRoData_DisplayList_CollisionRecord *) node)->Vertices[j];
            }
        }
    }
    
    if ((relatedrwdata != NULL) && (relatedrwdata->Vertices == relatedrodata->Vertices))
    {
        newvertices = (Vertex *) vtxstore_allocate(relatedrodata->numVertices, 0xcccc, 0, 0);
    
        if (newvertices != NULL)
        {
            s32 j;
    
            relatedrwdata->Vertices = newvertices;
    
            for (j = 0; j < relatedrodata->numVertices; j++)
            {
                newvertices[j] = relatedrodata->Vertices[j];
            }
        }
    }
    
    if (((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices != ((ModelRoData_DisplayList_CollisionRecord *) node)->Vertices)
    {
        s32 index;
    
        index = ((ModelRoData_DisplayList_CollisionRecord *) node)->CollisionVertices[bestindex].index;
    
        while (index >= 0)
        {
            ((ModelRwData_DisplayList_CollisionRecord *) rwdata)->Vertices[index].a = paintval;
            index = ((ModelRoData_DisplayList_CollisionRecord *) node)->PointUsage[index];
        }
    }
    
    if ((relatedrwdata != NULL) && (relatedrwdata->Vertices != relatedrodata->Vertices))
    {
        s32 index;
    
        index = relatedrodata->CollisionVertices[relatedindex].index;
    
        while (index >= 0)
        {
            relatedrwdata->Vertices[index].a = paintval;
            index = relatedrodata->PointUsage[index];
        }
    }
}


// PD: chr_test_hit
#ifdef NONMATCHING
void sub_GAME_7F022648(void) {
    //somewhere
    #ifdef DEBUG
    assert(hits && hits->HasHits());
    #endif
}
#else
#ifdef VERSION_US
GLOBAL_ASM(
.late_rodata
/* chrTickBeams late_rodata, hosted here for US (chrTickBeams is C; 2083C, its former host, is now C) */
glabel D_80051D58
.word 0x3ca3d70a
glabel D_80051D5C
.word 0x3ca3d70a
glabel D_80051D60
.word 0x3ca3d70a
glabel D_80051D64
.word 0x3ca3d70a
glabel D_80051D68
.word 0x3ca3d70a
glabel D_80051D6C
.word 0x3ca3d70a
glabel D_80051D70
.word 0x3f7eb852
glabel D_80051D74
.word 0x3f80a3d7
glabel D_80051D78
.word 0x3f7eb852
glabel D_80051D7C
.word 0x3f80a3d7
glabel D_80051D80
.word 0x3f7eb852
glabel D_80051D84
.word 0x3f80a3d7
glabel D_80051D88
.word 0x41aa6666
.text
glabel sub_GAME_7F022648
/* 057178 7F022648 27BDFF10 */  addiu $sp, $sp, -0xf0
/* 05717C 7F02264C AFBF0044 */  sw    $ra, 0x44($sp)
/* 057180 7F022650 AFB20040 */  sw    $s2, 0x40($sp)
/* 057184 7F022654 AFB1003C */  sw    $s1, 0x3c($sp)
/* 057188 7F022658 AFB00038 */  sw    $s0, 0x38($sp)
/* 05718C 7F02265C AFA400F0 */  sw    $a0, 0xf0($sp)
/* 057190 7F022660 8C820004 */  lw    $v0, 4($a0)
/* 057194 7F022664 24010005 */  li    $at, 5
/* 057198 7F022668 00A09025 */  move  $s2, $a1
/* 05719C 7F02266C 804F0007 */  lb    $t7, 7($v0)
/* 0571A0 7F022670 51E100BE */  beql  $t7, $at, .L7F02296C
/* 0571A4 7F022674 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571A8 7F022678 8C50001C */  lw    $s0, 0x1c($v0)
/* 0571AC 7F02267C AFA200EC */  sw    $v0, 0xec($sp)
/* 0571B0 7F022680 0FC1B403 */  jal   getinstsize
/* 0571B4 7F022684 02002025 */   move  $a0, $s0
/* 0571B8 7F022688 8FA200F0 */  lw    $v0, 0xf0($sp)
/* 0571BC 7F02268C E7A000E4 */  swc1  $f0, 0xe4($sp)
/* 0571C0 7F022690 90580001 */  lbu   $t8, 1($v0)
/* 0571C4 7F022694 33190002 */  andi  $t9, $t8, 2
/* 0571C8 7F022698 532000B4 */  beql  $t9, $zero, .L7F02296C
/* 0571CC 7F02269C 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571D0 7F0226A0 C4440018 */  lwc1  $f4, 0x18($v0)
/* 0571D4 7F0226A4 C6480034 */  lwc1  $f8, 0x34($s2)
/* 0571D8 7F0226A8 00008825 */  move  $s1, $zero
/* 0571DC 7F0226AC 46002181 */  sub.s $f6, $f4, $f0
/* 0571E0 7F0226B0 02002025 */  move  $a0, $s0
/* 0571E4 7F0226B4 4608303C */  c.lt.s $f6, $f8
/* 0571E8 7F0226B8 00000000 */  nop
/* 0571EC 7F0226BC 450200AB */  bc1fl .L7F02296C
/* 0571F0 7F0226C0 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571F4 7F0226C4 AFA000DC */  sw    $zero, 0xdc($sp)
/* 0571F8 7F0226C8 AFA000D8 */  sw    $zero, 0xd8($sp)
/* 0571FC 7F0226CC AFA000D4 */  sw    $zero, 0xd4($sp)
/* 057200 7F0226D0 0FC1B1A8 */  jal   getsubmatrix
/* 057204 7F0226D4 AFA0009C */   sw    $zero, 0x9c($sp)
/* 057208 7F0226D8 44806000 */  mtc1  $zero, $f12
/* 05720C 7F0226DC AFA20098 */  sw    $v0, 0x98($sp)
/* 057210 7F0226E0 00002825 */  move  $a1, $zero
/* 057214 7F0226E4 8FA600EC */  lw    $a2, 0xec($sp)
.L7F0226E8:
/* 057218 7F0226E8 8CC30160 */  lw    $v1, 0x160($a2)
/* 05721C 7F0226EC 50600012 */  beql  $v1, $zero, .L7F022738
/* 057220 7F0226F0 24A50004 */   addiu $a1, $a1, 4
/* 057224 7F0226F4 8C620004 */  lw    $v0, 4($v1)
/* 057228 7F0226F8 8C440014 */  lw    $a0, 0x14($v0)
/* 05722C 7F0226FC E7AC0094 */  swc1  $f12, 0x94($sp)
/* 057230 7F022700 AFA60050 */  sw    $a2, 0x50($sp)
/* 057234 7F022704 0FC1B403 */  jal   getinstsize
/* 057238 7F022708 AFA50054 */   sw    $a1, 0x54($sp)
/* 05723C 7F02270C C60A0014 */  lwc1  $f10, 0x14($s0)
/* 057240 7F022710 C7AC0094 */  lwc1  $f12, 0x94($sp)
/* 057244 7F022714 8FA50054 */  lw    $a1, 0x54($sp)
/* 057248 7F022718 460A0082 */  mul.s $f2, $f0, $f10
/* 05724C 7F02271C 8FA60050 */  lw    $a2, 0x50($sp)
/* 057250 7F022720 4602603C */  c.lt.s $f12, $f2
/* 057254 7F022724 00000000 */  nop
/* 057258 7F022728 45020003 */  bc1fl .L7F022738
/* 05725C 7F02272C 24A50004 */   addiu $a1, $a1, 4
/* 057260 7F022730 46001306 */  mov.s $f12, $f2
/* 057264 7F022734 24A50004 */  addiu $a1, $a1, 4
.L7F022738:
/* 057268 7F022738 24010008 */  li    $at, 8
/* 05726C 7F02273C 14A1FFEA */  bne   $a1, $at, .L7F0226E8
/* 057270 7F022740 24C60004 */   addiu $a2, $a2, 4
/* 057274 7F022744 C7A000E4 */  lwc1  $f0, 0xe4($sp)
/* 057278 7F022748 8FA60098 */  lw    $a2, 0x98($sp)
/* 05727C 7F02274C 2650000C */  addiu $s0, $s2, 0xc
/* 057280 7F022750 460C0000 */  add.s $f0, $f0, $f12
/* 057284 7F022754 02002825 */  move  $a1, $s0
/* 057288 7F022758 02402025 */  move  $a0, $s2
/* 05728C 7F02275C 24C60030 */  addiu $a2, $a2, 0x30
/* 057290 7F022760 44070000 */  mfc1  $a3, $f0
/* 057294 7F022764 0FC1041D */  jal   projectileTestPropBoundingSphere
/* 057298 7F022768 00000000 */   nop
/* 05729C 7F02276C 10400003 */  beqz  $v0, .L7F02277C
/* 0572A0 7F022770 24080001 */   li    $t0, 1
/* 0572A4 7F022774 AFA800D4 */  sw    $t0, 0xd4($sp)
/* 0572A8 7F022778 24110001 */  li    $s1, 1
.L7F02277C:
/* 0572AC 7F02277C 12200028 */  beqz  $s1, .L7F022820
/* 0572B0 7F022780 8FA900EC */   lw    $t1, 0xec($sp)
/* 0572B4 7F022784 8D2A0020 */  lw    $t2, 0x20($t1)
/* 0572B8 7F022788 27AB00D8 */  addiu $t3, $sp, 0xd8
/* 0572BC 7F02278C AFAB0010 */  sw    $t3, 0x10($sp)
/* 0572C0 7F022790 27A40080 */  addiu $a0, $sp, 0x80
/* 0572C4 7F022794 02402825 */  move  $a1, $s2
/* 0572C8 7F022798 02003025 */  move  $a2, $s0
/* 0572CC 7F02279C 27A700DC */  addiu $a3, $sp, 0xdc
/* 0572D0 7F0227A0 0FC1B004 */  jal   sub_GAME_7F06C010
/* 0572D4 7F0227A4 AFAA0080 */   sw    $t2, 0x80($sp)
/* 0572D8 7F0227A8 24010064 */  li    $at, 100 /* HITPART_GUN */
/* 0572DC 7F0227AC 10410003 */  beq   $v0, $at, .L7F0227BC
/* 0572E0 7F0227B0 00408825 */   move  $s1, $v0
/* 0572E4 7F0227B4 2401006E */  li    $at, 110 /* HITPART_HAT */
/* 0572E8 7F0227B8 14410019 */  bne   $v0, $at, .L7F022820
.L7F0227BC:
/* 0572EC 7F0227BC 27AC00A4 */   addiu $t4, $sp, 0xa4
.L7F0227C0:
/* 0572F0 7F0227C0 27AD00A0 */  addiu $t5, $sp, 0xa0
/* 0572F4 7F0227C4 27AE009C */  addiu $t6, $sp, 0x9c
/* 0572F8 7F0227C8 AFAE0018 */  sw    $t6, 0x18($sp)
/* 0572FC 7F0227CC AFAD0014 */  sw    $t5, 0x14($sp)
/* 057300 7F0227D0 AFAC0010 */  sw    $t4, 0x10($sp)
/* 057304 7F0227D4 8FA400DC */  lw    $a0, 0xdc($sp)
/* 057308 7F0227D8 8FA500D8 */  lw    $a1, 0xd8($sp)
/* 05730C 7F0227DC 02403025 */  move  $a2, $s2
/* 057310 7F0227E0 0FC1366C */  jal   propobjFindHit
/* 057314 7F0227E4 02003825 */   move  $a3, $s0
/* 057318 7F0227E8 1440000D */  bnez  $v0, .L7F022820
/* 05731C 7F0227EC 27A40080 */   addiu $a0, $sp, 0x80
/* 057320 7F0227F0 27AF00D8 */  addiu $t7, $sp, 0xd8
/* 057324 7F0227F4 AFAF0010 */  sw    $t7, 0x10($sp)
/* 057328 7F0227F8 02402825 */  move  $a1, $s2
/* 05732C 7F0227FC 02003025 */  move  $a2, $s0
/* 057330 7F022800 0FC1AF80 */  jal   probably_damage_detail_blood_effect_related
/* 057334 7F022804 27A700DC */   addiu $a3, $sp, 0xdc
/* 057338 7F022808 24010064 */  li    $at, 100 /* HITPART_GUN */
/* 05733C 7F02280C 1041FFEB */  beq   $v0, $at, .L7F0227BC
/* 057340 7F022810 00408825 */   move  $s1, $v0
/* 057344 7F022814 2401006E */  li    $at, 110 /* HITPART_HAT */
/* 057348 7F022818 5041FFE9 */  beql  $v0, $at, .L7F0227C0
/* 05734C 7F02281C 27AC00A4 */   addiu $t4, $sp, 0xa4
.L7F022820:
/* 057350 7F022820 5A200041 */  blezl $s1, .L7F022928
/* 057354 7F022824 8FAC00D4 */   lw    $t4, 0xd4($sp)
/* 057358 7F022828 C650000C */  lwc1  $f16, 0xc($s2)
/* 05735C 7F02282C E7B00074 */  swc1  $f16, 0x74($sp)
/* 057360 7F022830 C6520010 */  lwc1  $f18, 0x10($s2)
/* 057364 7F022834 E7B20078 */  swc1  $f18, 0x78($sp)
/* 057368 7F022838 C6440014 */  lwc1  $f4, 0x14($s2)
/* 05736C 7F02283C 0FC1E111 */  jal   currentPlayerGetViewToWorldMtxf
/* 057370 7F022840 E7A4007C */   swc1  $f4, 0x7c($sp)
/* 057374 7F022844 00402025 */  move  $a0, $v0
/* 057378 7F022848 0FC160F6 */  jal   mtx4RotateVecInPlace
/* 05737C 7F02284C 27A50074 */   addiu $a1, $sp, 0x74
/* 057380 7F022850 8FA400DC */  lw    $a0, 0xdc($sp)
/* 057384 7F022854 8FA500D8 */  lw    $a1, 0xd8($sp)
/* 057388 7F022858 0FC1B198 */  jal   modelFindNodeMtx
/* 05738C 7F02285C 00003025 */   move  $a2, $zero
/* 057390 7F022860 C4460030 */  lwc1  $f6, 0x30($v0)
/* 057394 7F022864 C6480000 */  lwc1  $f8, ($s2)
/* 057398 7F022868 46083281 */  sub.s $f10, $f6, $f8
/* 05739C 7F02286C E7AA0060 */  swc1  $f10, 0x60($sp)
/* 0573A0 7F022870 C6520004 */  lwc1  $f18, 4($s2)
/* 0573A4 7F022874 C4500034 */  lwc1  $f16, 0x34($v0)
/* 0573A8 7F022878 46128101 */  sub.s $f4, $f16, $f18
/* 0573AC 7F02287C C7B00060 */  lwc1  $f16, 0x60($sp)
/* 0573B0 7F022880 E7A40064 */  swc1  $f4, 0x64($sp)
/* 0573B4 7F022884 C6480008 */  lwc1  $f8, 8($s2)
/* 0573B8 7F022888 C4460038 */  lwc1  $f6, 0x38($v0)
/* 0573BC 7F02288C 46108482 */  mul.s $f18, $f16, $f16
/* 0573C0 7F022890 C7A40064 */  lwc1  $f4, 0x64($sp)
/* 0573C4 7F022894 46083281 */  sub.s $f10, $f6, $f8
/* 0573C8 7F022898 46042182 */  mul.s $f6, $f4, $f4
/* 0573CC 7F02289C E7AA0068 */  swc1  $f10, 0x68($sp)
/* 0573D0 7F0228A0 C7AA0068 */  lwc1  $f10, 0x68($sp)
/* 0573D4 7F0228A4 460A5402 */  mul.s $f16, $f10, $f10
/* 0573D8 7F0228A8 46069200 */  add.s $f8, $f18, $f6
/* 0573DC 7F0228AC 0C007DF8 */  jal   sqrtf
/* 0573E0 7F0228B0 46088300 */   add.s $f12, $f16, $f8
/* 0573E4 7F0228B4 C6440014 */  lwc1  $f4, 0x14($s2)
/* 0573E8 7F0228B8 C6460008 */  lwc1  $f6, 8($s2)
/* 0573EC 7F0228BC C64A0034 */  lwc1  $f10, 0x34($s2)
/* 0573F0 7F0228C0 46002482 */  mul.s $f18, $f4, $f0
/* 0573F4 7F0228C4 02402025 */  move  $a0, $s2
/* 0573F8 7F0228C8 8FA500F0 */  lw    $a1, 0xf0($sp)
/* 0573FC 7F0228CC 02203825 */  move  $a3, $s1
/* 057400 7F0228D0 8FB800D8 */  lw    $t8, 0xd8($sp)
/* 057404 7F0228D4 27B900A4 */  addiu $t9, $sp, 0xa4
/* 057408 7F0228D8 240B0001 */  li    $t3, 1
/* 05740C 7F0228DC 46069080 */  add.s $f2, $f18, $f6
/* 057410 7F0228E0 46001087 */  neg.s $f2, $f2
/* 057414 7F0228E4 460A103C */  c.lt.s $f2, $f10
/* 057418 7F0228E8 00000000 */  nop
/* 05741C 7F0228EC 4502000E */  bc1fl .L7F022928
/* 057420 7F0228F0 8FAC00D4 */   lw    $t4, 0xd4($sp)
/* 057424 7F0228F4 8FA800A0 */  lw    $t0, 0xa0($sp)
/* 057428 7F0228F8 8FA9009C */  lw    $t1, 0x9c($sp)
/* 05742C 7F0228FC 8FAA00DC */  lw    $t2, 0xdc($sp)
/* 057430 7F022900 44061000 */  mfc1  $a2, $f2
/* 057434 7F022904 AFB80010 */  sw    $t8, 0x10($sp)
/* 057438 7F022908 AFB90014 */  sw    $t9, 0x14($sp)
/* 05743C 7F02290C AFAB0024 */  sw    $t3, 0x24($sp)
/* 057440 7F022910 AFA00028 */  sw    $zero, 0x28($sp)
/* 057444 7F022914 AFA80018 */  sw    $t0, 0x18($sp)
/* 057448 7F022918 AFA9001C */  sw    $t1, 0x1c($sp)
/* 05744C 7F02291C 0FC0EE70 */  jal   chrpropAddBulletHit
/* 057450 7F022920 AFAA0020 */   sw    $t2, 0x20($sp)
/* 057454 7F022924 8FAC00D4 */  lw    $t4, 0xd4($sp)
.L7F022928:
/* 057458 7F022928 8FAD00F0 */  lw    $t5, 0xf0($sp)
/* 05745C 7F02292C 5180000F */  beql  $t4, $zero, .L7F02296C
/* 057460 7F022930 8FBF0044 */   lw    $ra, 0x44($sp)
/* 057464 7F022934 C6500034 */  lwc1  $f16, 0x34($s2)
/* 057468 7F022938 C5A80018 */  lwc1  $f8, 0x18($t5)
/* 05746C 7F02293C 8FAE00EC */  lw    $t6, 0xec($sp)
/* 057470 7F022940 4610403E */  c.le.s $f8, $f16
/* 057474 7F022944 00000000 */  nop
/* 057478 7F022948 45020008 */  bc1fl .L7F02296C
/* 05747C 7F02294C 8FBF0044 */   lw    $ra, 0x44($sp)
/* 057480 7F022950 8DCF0014 */  lw    $t7, 0x14($t6)
/* 057484 7F022954 81D9000A */  lb    $t9, 0xa($t6)
/* 057488 7F022958 35F80004 */  ori   $t8, $t7, 4
/* 05748C 7F02295C 27280001 */  addiu $t0, $t9, 1
/* 057490 7F022960 ADD80014 */  sw    $t8, 0x14($t6)
/* 057494 7F022964 A1C8000A */  sb    $t0, 0xa($t6)
/* 057498 7F022968 8FBF0044 */  lw    $ra, 0x44($sp)
.L7F02296C:
/* 05749C 7F02296C 8FB00038 */  lw    $s0, 0x38($sp)
/* 0574A0 7F022970 8FB1003C */  lw    $s1, 0x3c($sp)
/* 0574A4 7F022974 8FB20040 */  lw    $s2, 0x40($sp)
/* 0574A8 7F022978 03E00008 */  jr    $ra
/* 0574AC 7F02297C 27BD00F0 */   addiu $sp, $sp, 0xf0
)
#elif defined(VERSION_JP)
GLOBAL_ASM(
.late_rodata
/* chrTickBeams late_rodata (JP): 0.8 + D_58..88; chrTickBeams is C in JP */
glabel D_jp80051D88
.word 0x3f4ccccd
glabel D_80051D58
.word 0x3ca3d70a
glabel D_80051D5C
.word 0x3ca3d70a
glabel D_80051D60
.word 0x3ca3d70a
glabel D_80051D64
.word 0x3ca3d70a
glabel D_80051D68
.word 0x3ca3d70a
glabel D_80051D6C
.word 0x3ca3d70a
glabel D_80051D70
.word 0x3f7eb852
glabel D_80051D74
.word 0x3f80a3d7
glabel D_80051D78
.word 0x3f7eb852
glabel D_80051D7C
.word 0x3f80a3d7
glabel D_80051D80
.word 0x3f7eb852
glabel D_80051D84
.word 0x3f80a3d7
glabel D_80051D88
.word 0x41aa6666
.text
glabel sub_GAME_7F022648
/* 057178 7F022648 27BDFF10 */  addiu $sp, $sp, -0xf0
/* 05717C 7F02264C AFBF0044 */  sw    $ra, 0x44($sp)
/* 057180 7F022650 AFB20040 */  sw    $s2, 0x40($sp)
/* 057184 7F022654 AFB1003C */  sw    $s1, 0x3c($sp)
/* 057188 7F022658 AFB00038 */  sw    $s0, 0x38($sp)
/* 05718C 7F02265C AFA400F0 */  sw    $a0, 0xf0($sp)
/* 057190 7F022660 8C820004 */  lw    $v0, 4($a0)
/* 057194 7F022664 24010005 */  li    $at, 5
/* 057198 7F022668 00A09025 */  move  $s2, $a1
/* 05719C 7F02266C 804F0007 */  lb    $t7, 7($v0)
/* 0571A0 7F022670 51E100BE */  beql  $t7, $at, .L7F02296C
/* 0571A4 7F022674 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571A8 7F022678 8C50001C */  lw    $s0, 0x1c($v0)
/* 0571AC 7F02267C AFA200EC */  sw    $v0, 0xec($sp)
/* 0571B0 7F022680 0FC1B403 */  jal   getinstsize
/* 0571B4 7F022684 02002025 */   move  $a0, $s0
/* 0571B8 7F022688 8FA200F0 */  lw    $v0, 0xf0($sp)
/* 0571BC 7F02268C E7A000E4 */  swc1  $f0, 0xe4($sp)
/* 0571C0 7F022690 90580001 */  lbu   $t8, 1($v0)
/* 0571C4 7F022694 33190002 */  andi  $t9, $t8, 2
/* 0571C8 7F022698 532000B4 */  beql  $t9, $zero, .L7F02296C
/* 0571CC 7F02269C 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571D0 7F0226A0 C4440018 */  lwc1  $f4, 0x18($v0)
/* 0571D4 7F0226A4 C6480034 */  lwc1  $f8, 0x34($s2)
/* 0571D8 7F0226A8 00008825 */  move  $s1, $zero
/* 0571DC 7F0226AC 46002181 */  sub.s $f6, $f4, $f0
/* 0571E0 7F0226B0 02002025 */  move  $a0, $s0
/* 0571E4 7F0226B4 4608303C */  c.lt.s $f6, $f8
/* 0571E8 7F0226B8 00000000 */  nop
/* 0571EC 7F0226BC 450200AB */  bc1fl .L7F02296C
/* 0571F0 7F0226C0 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571F4 7F0226C4 AFA000DC */  sw    $zero, 0xdc($sp)
/* 0571F8 7F0226C8 AFA000D8 */  sw    $zero, 0xd8($sp)
/* 0571FC 7F0226CC AFA000D4 */  sw    $zero, 0xd4($sp)
/* 057200 7F0226D0 0FC1B1A8 */  jal   getsubmatrix
/* 057204 7F0226D4 AFA0009C */   sw    $zero, 0x9c($sp)
/* 057208 7F0226D8 44806000 */  mtc1  $zero, $f12
/* 05720C 7F0226DC AFA20098 */  sw    $v0, 0x98($sp)
/* 057210 7F0226E0 00002825 */  move  $a1, $zero
/* 057214 7F0226E4 8FA600EC */  lw    $a2, 0xec($sp)
.L7F0226E8:
/* 057218 7F0226E8 8CC30160 */  lw    $v1, 0x160($a2)
/* 05721C 7F0226EC 50600012 */  beql  $v1, $zero, .L7F022738
/* 057220 7F0226F0 24A50004 */   addiu $a1, $a1, 4
/* 057224 7F0226F4 8C620004 */  lw    $v0, 4($v1)
/* 057228 7F0226F8 8C440014 */  lw    $a0, 0x14($v0)
/* 05722C 7F0226FC E7AC0094 */  swc1  $f12, 0x94($sp)
/* 057230 7F022700 AFA60050 */  sw    $a2, 0x50($sp)
/* 057234 7F022704 0FC1B403 */  jal   getinstsize
/* 057238 7F022708 AFA50054 */   sw    $a1, 0x54($sp)
/* 05723C 7F02270C C60A0014 */  lwc1  $f10, 0x14($s0)
/* 057240 7F022710 C7AC0094 */  lwc1  $f12, 0x94($sp)
/* 057244 7F022714 8FA50054 */  lw    $a1, 0x54($sp)
/* 057248 7F022718 460A0082 */  mul.s $f2, $f0, $f10
/* 05724C 7F02271C 8FA60050 */  lw    $a2, 0x50($sp)
/* 057250 7F022720 4602603C */  c.lt.s $f12, $f2
/* 057254 7F022724 00000000 */  nop
/* 057258 7F022728 45020003 */  bc1fl .L7F022738
/* 05725C 7F02272C 24A50004 */   addiu $a1, $a1, 4
/* 057260 7F022730 46001306 */  mov.s $f12, $f2
/* 057264 7F022734 24A50004 */  addiu $a1, $a1, 4
.L7F022738:
/* 057268 7F022738 24010008 */  li    $at, 8
/* 05726C 7F02273C 14A1FFEA */  bne   $a1, $at, .L7F0226E8
/* 057270 7F022740 24C60004 */   addiu $a2, $a2, 4
/* 057274 7F022744 C7A000E4 */  lwc1  $f0, 0xe4($sp)
/* 057278 7F022748 8FA60098 */  lw    $a2, 0x98($sp)
/* 05727C 7F02274C 2650000C */  addiu $s0, $s2, 0xc
/* 057280 7F022750 460C0000 */  add.s $f0, $f0, $f12
/* 057284 7F022754 02002825 */  move  $a1, $s0
/* 057288 7F022758 02402025 */  move  $a0, $s2
/* 05728C 7F02275C 24C60030 */  addiu $a2, $a2, 0x30
/* 057290 7F022760 44070000 */  mfc1  $a3, $f0
/* 057294 7F022764 0FC1041D */  jal   projectileTestPropBoundingSphere
/* 057298 7F022768 00000000 */   nop
/* 05729C 7F02276C 10400003 */  beqz  $v0, .L7F02277C
/* 0572A0 7F022770 24080001 */   li    $t0, 1
/* 0572A4 7F022774 AFA800D4 */  sw    $t0, 0xd4($sp)
/* 0572A8 7F022778 24110001 */  li    $s1, 1
.L7F02277C:
/* 0572AC 7F02277C 12200028 */  beqz  $s1, .L7F022820
/* 0572B0 7F022780 8FA900EC */   lw    $t1, 0xec($sp)
/* 0572B4 7F022784 8D2A0020 */  lw    $t2, 0x20($t1)
/* 0572B8 7F022788 27AB00D8 */  addiu $t3, $sp, 0xd8
/* 0572BC 7F02278C AFAB0010 */  sw    $t3, 0x10($sp)
/* 0572C0 7F022790 27A40080 */  addiu $a0, $sp, 0x80
/* 0572C4 7F022794 02402825 */  move  $a1, $s2
/* 0572C8 7F022798 02003025 */  move  $a2, $s0
/* 0572CC 7F02279C 27A700DC */  addiu $a3, $sp, 0xdc
/* 0572D0 7F0227A0 0FC1B004 */  jal   sub_GAME_7F06C010
/* 0572D4 7F0227A4 AFAA0080 */   sw    $t2, 0x80($sp)
/* 0572D8 7F0227A8 24010064 */  li    $at, 100 /* HITPART_GUN */
/* 0572DC 7F0227AC 10410003 */  beq   $v0, $at, .L7F0227BC
/* 0572E0 7F0227B0 00408825 */   move  $s1, $v0
/* 0572E4 7F0227B4 2401006E */  li    $at, 110 /* HITPART_HAT */
/* 0572E8 7F0227B8 14410019 */  bne   $v0, $at, .L7F022820
.L7F0227BC:
/* 0572EC 7F0227BC 27AC00A4 */   addiu $t4, $sp, 0xa4
.L7F0227C0:
/* 0572F0 7F0227C0 27AD00A0 */  addiu $t5, $sp, 0xa0
/* 0572F4 7F0227C4 27AE009C */  addiu $t6, $sp, 0x9c
/* 0572F8 7F0227C8 AFAE0018 */  sw    $t6, 0x18($sp)
/* 0572FC 7F0227CC AFAD0014 */  sw    $t5, 0x14($sp)
/* 057300 7F0227D0 AFAC0010 */  sw    $t4, 0x10($sp)
/* 057304 7F0227D4 8FA400DC */  lw    $a0, 0xdc($sp)
/* 057308 7F0227D8 8FA500D8 */  lw    $a1, 0xd8($sp)
/* 05730C 7F0227DC 02403025 */  move  $a2, $s2
/* 057310 7F0227E0 0FC1366C */  jal   propobjFindHit
/* 057314 7F0227E4 02003825 */   move  $a3, $s0
/* 057318 7F0227E8 1440000D */  bnez  $v0, .L7F022820
/* 05731C 7F0227EC 27A40080 */   addiu $a0, $sp, 0x80
/* 057320 7F0227F0 27AF00D8 */  addiu $t7, $sp, 0xd8
/* 057324 7F0227F4 AFAF0010 */  sw    $t7, 0x10($sp)
/* 057328 7F0227F8 02402825 */  move  $a1, $s2
/* 05732C 7F0227FC 02003025 */  move  $a2, $s0
/* 057330 7F022800 0FC1AF80 */  jal   probably_damage_detail_blood_effect_related
/* 057334 7F022804 27A700DC */   addiu $a3, $sp, 0xdc
/* 057338 7F022808 24010064 */  li    $at, 100 /* HITPART_GUN */
/* 05733C 7F02280C 1041FFEB */  beq   $v0, $at, .L7F0227BC
/* 057340 7F022810 00408825 */   move  $s1, $v0
/* 057344 7F022814 2401006E */  li    $at, 110 /* HITPART_HAT */
/* 057348 7F022818 5041FFE9 */  beql  $v0, $at, .L7F0227C0
/* 05734C 7F02281C 27AC00A4 */   addiu $t4, $sp, 0xa4
.L7F022820:
/* 057350 7F022820 5A200041 */  blezl $s1, .L7F022928
/* 057354 7F022824 8FAC00D4 */   lw    $t4, 0xd4($sp)
/* 057358 7F022828 C650000C */  lwc1  $f16, 0xc($s2)
/* 05735C 7F02282C E7B00074 */  swc1  $f16, 0x74($sp)
/* 057360 7F022830 C6520010 */  lwc1  $f18, 0x10($s2)
/* 057364 7F022834 E7B20078 */  swc1  $f18, 0x78($sp)
/* 057368 7F022838 C6440014 */  lwc1  $f4, 0x14($s2)
/* 05736C 7F02283C 0FC1E111 */  jal   currentPlayerGetViewToWorldMtxf
/* 057370 7F022840 E7A4007C */   swc1  $f4, 0x7c($sp)
/* 057374 7F022844 00402025 */  move  $a0, $v0
/* 057378 7F022848 0FC160F6 */  jal   mtx4RotateVecInPlace
/* 05737C 7F02284C 27A50074 */   addiu $a1, $sp, 0x74
/* 057380 7F022850 8FA400DC */  lw    $a0, 0xdc($sp)
/* 057384 7F022854 8FA500D8 */  lw    $a1, 0xd8($sp)
/* 057388 7F022858 0FC1B198 */  jal   modelFindNodeMtx
/* 05738C 7F02285C 00003025 */   move  $a2, $zero
/* 057390 7F022860 C4460030 */  lwc1  $f6, 0x30($v0)
/* 057394 7F022864 C6480000 */  lwc1  $f8, ($s2)
/* 057398 7F022868 46083281 */  sub.s $f10, $f6, $f8
/* 05739C 7F02286C E7AA0060 */  swc1  $f10, 0x60($sp)
/* 0573A0 7F022870 C6520004 */  lwc1  $f18, 4($s2)
/* 0573A4 7F022874 C4500034 */  lwc1  $f16, 0x34($v0)
/* 0573A8 7F022878 46128101 */  sub.s $f4, $f16, $f18
/* 0573AC 7F02287C C7B00060 */  lwc1  $f16, 0x60($sp)
/* 0573B0 7F022880 E7A40064 */  swc1  $f4, 0x64($sp)
/* 0573B4 7F022884 C6480008 */  lwc1  $f8, 8($s2)
/* 0573B8 7F022888 C4460038 */  lwc1  $f6, 0x38($v0)
/* 0573BC 7F02288C 46108482 */  mul.s $f18, $f16, $f16
/* 0573C0 7F022890 C7A40064 */  lwc1  $f4, 0x64($sp)
/* 0573C4 7F022894 46083281 */  sub.s $f10, $f6, $f8
/* 0573C8 7F022898 46042182 */  mul.s $f6, $f4, $f4
/* 0573CC 7F02289C E7AA0068 */  swc1  $f10, 0x68($sp)
/* 0573D0 7F0228A0 C7AA0068 */  lwc1  $f10, 0x68($sp)
/* 0573D4 7F0228A4 460A5402 */  mul.s $f16, $f10, $f10
/* 0573D8 7F0228A8 46069200 */  add.s $f8, $f18, $f6
/* 0573DC 7F0228AC 0C007DF8 */  jal   sqrtf
/* 0573E0 7F0228B0 46088300 */   add.s $f12, $f16, $f8
/* 0573E4 7F0228B4 C6440014 */  lwc1  $f4, 0x14($s2)
/* 0573E8 7F0228B8 C6460008 */  lwc1  $f6, 8($s2)
/* 0573EC 7F0228BC C64A0034 */  lwc1  $f10, 0x34($s2)
/* 0573F0 7F0228C0 46002482 */  mul.s $f18, $f4, $f0
/* 0573F4 7F0228C4 02402025 */  move  $a0, $s2
/* 0573F8 7F0228C8 8FA500F0 */  lw    $a1, 0xf0($sp)
/* 0573FC 7F0228CC 02203825 */  move  $a3, $s1
/* 057400 7F0228D0 8FB800D8 */  lw    $t8, 0xd8($sp)
/* 057404 7F0228D4 27B900A4 */  addiu $t9, $sp, 0xa4
/* 057408 7F0228D8 240B0001 */  li    $t3, 1
/* 05740C 7F0228DC 46069080 */  add.s $f2, $f18, $f6
/* 057410 7F0228E0 46001087 */  neg.s $f2, $f2
/* 057414 7F0228E4 460A103C */  c.lt.s $f2, $f10
/* 057418 7F0228E8 00000000 */  nop
/* 05741C 7F0228EC 4502000E */  bc1fl .L7F022928
/* 057420 7F0228F0 8FAC00D4 */   lw    $t4, 0xd4($sp)
/* 057424 7F0228F4 8FA800A0 */  lw    $t0, 0xa0($sp)
/* 057428 7F0228F8 8FA9009C */  lw    $t1, 0x9c($sp)
/* 05742C 7F0228FC 8FAA00DC */  lw    $t2, 0xdc($sp)
/* 057430 7F022900 44061000 */  mfc1  $a2, $f2
/* 057434 7F022904 AFB80010 */  sw    $t8, 0x10($sp)
/* 057438 7F022908 AFB90014 */  sw    $t9, 0x14($sp)
/* 05743C 7F02290C AFAB0024 */  sw    $t3, 0x24($sp)
/* 057440 7F022910 AFA00028 */  sw    $zero, 0x28($sp)
/* 057444 7F022914 AFA80018 */  sw    $t0, 0x18($sp)
/* 057448 7F022918 AFA9001C */  sw    $t1, 0x1c($sp)
/* 05744C 7F02291C 0FC0EE70 */  jal   chrpropAddBulletHit
/* 057450 7F022920 AFAA0020 */   sw    $t2, 0x20($sp)
/* 057454 7F022924 8FAC00D4 */  lw    $t4, 0xd4($sp)
.L7F022928:
/* 057458 7F022928 8FAD00F0 */  lw    $t5, 0xf0($sp)
/* 05745C 7F02292C 5180000F */  beql  $t4, $zero, .L7F02296C
/* 057460 7F022930 8FBF0044 */   lw    $ra, 0x44($sp)
/* 057464 7F022934 C6500034 */  lwc1  $f16, 0x34($s2)
/* 057468 7F022938 C5A80018 */  lwc1  $f8, 0x18($t5)
/* 05746C 7F02293C 8FAE00EC */  lw    $t6, 0xec($sp)
/* 057470 7F022940 4610403E */  c.le.s $f8, $f16
/* 057474 7F022944 00000000 */  nop
/* 057478 7F022948 45020008 */  bc1fl .L7F02296C
/* 05747C 7F02294C 8FBF0044 */   lw    $ra, 0x44($sp)
/* 057480 7F022950 8DCF0014 */  lw    $t7, 0x14($t6)
/* 057484 7F022954 81D9000A */  lb    $t9, 0xa($t6)
/* 057488 7F022958 35F80004 */  ori   $t8, $t7, 4
/* 05748C 7F02295C 27280001 */  addiu $t0, $t9, 1
/* 057490 7F022960 ADD80014 */  sw    $t8, 0x14($t6)
/* 057494 7F022964 A1C8000A */  sb    $t0, 0xa($t6)
/* 057498 7F022968 8FBF0044 */  lw    $ra, 0x44($sp)
.L7F02296C:
/* 05749C 7F02296C 8FB00038 */  lw    $s0, 0x38($sp)
/* 0574A0 7F022970 8FB1003C */  lw    $s1, 0x3c($sp)
/* 0574A4 7F022974 8FB20040 */  lw    $s2, 0x40($sp)
/* 0574A8 7F022978 03E00008 */  jr    $ra
/* 0574AC 7F02297C 27BD00F0 */   addiu $sp, $sp, 0xf0
)
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F022648
/* 057178 7F022648 27BDFF10 */  addiu $sp, $sp, -0xf0
/* 05717C 7F02264C AFBF0044 */  sw    $ra, 0x44($sp)
/* 057180 7F022650 AFB20040 */  sw    $s2, 0x40($sp)
/* 057184 7F022654 AFB1003C */  sw    $s1, 0x3c($sp)
/* 057188 7F022658 AFB00038 */  sw    $s0, 0x38($sp)
/* 05718C 7F02265C AFA400F0 */  sw    $a0, 0xf0($sp)
/* 057190 7F022660 8C820004 */  lw    $v0, 4($a0)
/* 057194 7F022664 24010005 */  li    $at, 5
/* 057198 7F022668 00A09025 */  move  $s2, $a1
/* 05719C 7F02266C 804F0007 */  lb    $t7, 7($v0)
/* 0571A0 7F022670 51E100BE */  beql  $t7, $at, .L7F02296C
/* 0571A4 7F022674 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571A8 7F022678 8C50001C */  lw    $s0, 0x1c($v0)
/* 0571AC 7F02267C AFA200EC */  sw    $v0, 0xec($sp)
/* 0571B0 7F022680 0FC1B403 */  jal   getinstsize
/* 0571B4 7F022684 02002025 */   move  $a0, $s0
/* 0571B8 7F022688 8FA200F0 */  lw    $v0, 0xf0($sp)
/* 0571BC 7F02268C E7A000E4 */  swc1  $f0, 0xe4($sp)
/* 0571C0 7F022690 90580001 */  lbu   $t8, 1($v0)
/* 0571C4 7F022694 33190002 */  andi  $t9, $t8, 2
/* 0571C8 7F022698 532000B4 */  beql  $t9, $zero, .L7F02296C
/* 0571CC 7F02269C 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571D0 7F0226A0 C4440018 */  lwc1  $f4, 0x18($v0)
/* 0571D4 7F0226A4 C6480034 */  lwc1  $f8, 0x34($s2)
/* 0571D8 7F0226A8 00008825 */  move  $s1, $zero
/* 0571DC 7F0226AC 46002181 */  sub.s $f6, $f4, $f0
/* 0571E0 7F0226B0 02002025 */  move  $a0, $s0
/* 0571E4 7F0226B4 4608303C */  c.lt.s $f6, $f8
/* 0571E8 7F0226B8 00000000 */  nop
/* 0571EC 7F0226BC 450200AB */  bc1fl .L7F02296C
/* 0571F0 7F0226C0 8FBF0044 */   lw    $ra, 0x44($sp)
/* 0571F4 7F0226C4 AFA000DC */  sw    $zero, 0xdc($sp)
/* 0571F8 7F0226C8 AFA000D8 */  sw    $zero, 0xd8($sp)
/* 0571FC 7F0226CC AFA000D4 */  sw    $zero, 0xd4($sp)
/* 057200 7F0226D0 0FC1B1A8 */  jal   getsubmatrix
/* 057204 7F0226D4 AFA0009C */   sw    $zero, 0x9c($sp)
/* 057208 7F0226D8 44806000 */  mtc1  $zero, $f12
/* 05720C 7F0226DC AFA20098 */  sw    $v0, 0x98($sp)
/* 057210 7F0226E0 00002825 */  move  $a1, $zero
/* 057214 7F0226E4 8FA600EC */  lw    $a2, 0xec($sp)
.L7F0226E8:
/* 057218 7F0226E8 8CC30160 */  lw    $v1, 0x160($a2)
/* 05721C 7F0226EC 50600012 */  beql  $v1, $zero, .L7F022738
/* 057220 7F0226F0 24A50004 */   addiu $a1, $a1, 4
/* 057224 7F0226F4 8C620004 */  lw    $v0, 4($v1)
/* 057228 7F0226F8 8C440014 */  lw    $a0, 0x14($v0)
/* 05722C 7F0226FC E7AC0094 */  swc1  $f12, 0x94($sp)
/* 057230 7F022700 AFA60050 */  sw    $a2, 0x50($sp)
/* 057234 7F022704 0FC1B403 */  jal   getinstsize
/* 057238 7F022708 AFA50054 */   sw    $a1, 0x54($sp)
/* 05723C 7F02270C C60A0014 */  lwc1  $f10, 0x14($s0)
/* 057240 7F022710 C7AC0094 */  lwc1  $f12, 0x94($sp)
/* 057244 7F022714 8FA50054 */  lw    $a1, 0x54($sp)
/* 057248 7F022718 460A0082 */  mul.s $f2, $f0, $f10
/* 05724C 7F02271C 8FA60050 */  lw    $a2, 0x50($sp)
/* 057250 7F022720 4602603C */  c.lt.s $f12, $f2
/* 057254 7F022724 00000000 */  nop
/* 057258 7F022728 45020003 */  bc1fl .L7F022738
/* 05725C 7F02272C 24A50004 */   addiu $a1, $a1, 4
/* 057260 7F022730 46001306 */  mov.s $f12, $f2
/* 057264 7F022734 24A50004 */  addiu $a1, $a1, 4
.L7F022738:
/* 057268 7F022738 24010008 */  li    $at, 8
/* 05726C 7F02273C 14A1FFEA */  bne   $a1, $at, .L7F0226E8
/* 057270 7F022740 24C60004 */   addiu $a2, $a2, 4
/* 057274 7F022744 C7A000E4 */  lwc1  $f0, 0xe4($sp)
/* 057278 7F022748 8FA60098 */  lw    $a2, 0x98($sp)
/* 05727C 7F02274C 2650000C */  addiu $s0, $s2, 0xc
/* 057280 7F022750 460C0000 */  add.s $f0, $f0, $f12
/* 057284 7F022754 02002825 */  move  $a1, $s0
/* 057288 7F022758 02402025 */  move  $a0, $s2
/* 05728C 7F02275C 24C60030 */  addiu $a2, $a2, 0x30
/* 057290 7F022760 44070000 */  mfc1  $a3, $f0
/* 057294 7F022764 0FC1041D */  jal   projectileTestPropBoundingSphere
/* 057298 7F022768 00000000 */   nop
/* 05729C 7F02276C 10400003 */  beqz  $v0, .L7F02277C
/* 0572A0 7F022770 24080001 */   li    $t0, 1
/* 0572A4 7F022774 AFA800D4 */  sw    $t0, 0xd4($sp)
/* 0572A8 7F022778 24110001 */  li    $s1, 1
.L7F02277C:
/* 0572AC 7F02277C 12200028 */  beqz  $s1, .L7F022820
/* 0572B0 7F022780 8FA900EC */   lw    $t1, 0xec($sp)
/* 0572B4 7F022784 8D2A0020 */  lw    $t2, 0x20($t1)
/* 0572B8 7F022788 27AB00D8 */  addiu $t3, $sp, 0xd8
/* 0572BC 7F02278C AFAB0010 */  sw    $t3, 0x10($sp)
/* 0572C0 7F022790 27A40080 */  addiu $a0, $sp, 0x80
/* 0572C4 7F022794 02402825 */  move  $a1, $s2
/* 0572C8 7F022798 02003025 */  move  $a2, $s0
/* 0572CC 7F02279C 27A700DC */  addiu $a3, $sp, 0xdc
/* 0572D0 7F0227A0 0FC1B004 */  jal   sub_GAME_7F06C010
/* 0572D4 7F0227A4 AFAA0080 */   sw    $t2, 0x80($sp)
/* 0572D8 7F0227A8 24010064 */  li    $at, 100 /* HITPART_GUN */
/* 0572DC 7F0227AC 10410003 */  beq   $v0, $at, .L7F0227BC
/* 0572E0 7F0227B0 00408825 */   move  $s1, $v0
/* 0572E4 7F0227B4 2401006E */  li    $at, 110 /* HITPART_HAT */
/* 0572E8 7F0227B8 14410019 */  bne   $v0, $at, .L7F022820
.L7F0227BC:
/* 0572EC 7F0227BC 27AC00A4 */   addiu $t4, $sp, 0xa4
.L7F0227C0:
/* 0572F0 7F0227C0 27AD00A0 */  addiu $t5, $sp, 0xa0
/* 0572F4 7F0227C4 27AE009C */  addiu $t6, $sp, 0x9c
/* 0572F8 7F0227C8 AFAE0018 */  sw    $t6, 0x18($sp)
/* 0572FC 7F0227CC AFAD0014 */  sw    $t5, 0x14($sp)
/* 057300 7F0227D0 AFAC0010 */  sw    $t4, 0x10($sp)
/* 057304 7F0227D4 8FA400DC */  lw    $a0, 0xdc($sp)
/* 057308 7F0227D8 8FA500D8 */  lw    $a1, 0xd8($sp)
/* 05730C 7F0227DC 02403025 */  move  $a2, $s2
/* 057310 7F0227E0 0FC1366C */  jal   propobjFindHit
/* 057314 7F0227E4 02003825 */   move  $a3, $s0
/* 057318 7F0227E8 1440000D */  bnez  $v0, .L7F022820
/* 05731C 7F0227EC 27A40080 */   addiu $a0, $sp, 0x80
/* 057320 7F0227F0 27AF00D8 */  addiu $t7, $sp, 0xd8
/* 057324 7F0227F4 AFAF0010 */  sw    $t7, 0x10($sp)
/* 057328 7F0227F8 02402825 */  move  $a1, $s2
/* 05732C 7F0227FC 02003025 */  move  $a2, $s0
/* 057330 7F022800 0FC1AF80 */  jal   probably_damage_detail_blood_effect_related
/* 057334 7F022804 27A700DC */   addiu $a3, $sp, 0xdc
/* 057338 7F022808 24010064 */  li    $at, 100 /* HITPART_GUN */
/* 05733C 7F02280C 1041FFEB */  beq   $v0, $at, .L7F0227BC
/* 057340 7F022810 00408825 */   move  $s1, $v0
/* 057344 7F022814 2401006E */  li    $at, 110 /* HITPART_HAT */
/* 057348 7F022818 5041FFE9 */  beql  $v0, $at, .L7F0227C0
/* 05734C 7F02281C 27AC00A4 */   addiu $t4, $sp, 0xa4
.L7F022820:
/* 057350 7F022820 5A200041 */  blezl $s1, .L7F022928
/* 057354 7F022824 8FAC00D4 */   lw    $t4, 0xd4($sp)
/* 057358 7F022828 C650000C */  lwc1  $f16, 0xc($s2)
/* 05735C 7F02282C E7B00074 */  swc1  $f16, 0x74($sp)
/* 057360 7F022830 C6520010 */  lwc1  $f18, 0x10($s2)
/* 057364 7F022834 E7B20078 */  swc1  $f18, 0x78($sp)
/* 057368 7F022838 C6440014 */  lwc1  $f4, 0x14($s2)
/* 05736C 7F02283C 0FC1E111 */  jal   currentPlayerGetViewToWorldMtxf
/* 057370 7F022840 E7A4007C */   swc1  $f4, 0x7c($sp)
/* 057374 7F022844 00402025 */  move  $a0, $v0
/* 057378 7F022848 0FC160F6 */  jal   mtx4RotateVecInPlace
/* 05737C 7F02284C 27A50074 */   addiu $a1, $sp, 0x74
/* 057380 7F022850 8FA400DC */  lw    $a0, 0xdc($sp)
/* 057384 7F022854 8FA500D8 */  lw    $a1, 0xd8($sp)
/* 057388 7F022858 0FC1B198 */  jal   modelFindNodeMtx
/* 05738C 7F02285C 00003025 */   move  $a2, $zero
/* 057390 7F022860 C4460030 */  lwc1  $f6, 0x30($v0)
/* 057394 7F022864 C6480000 */  lwc1  $f8, ($s2)
/* 057398 7F022868 46083281 */  sub.s $f10, $f6, $f8
/* 05739C 7F02286C E7AA0060 */  swc1  $f10, 0x60($sp)
/* 0573A0 7F022870 C6520004 */  lwc1  $f18, 4($s2)
/* 0573A4 7F022874 C4500034 */  lwc1  $f16, 0x34($v0)
/* 0573A8 7F022878 46128101 */  sub.s $f4, $f16, $f18
/* 0573AC 7F02287C C7B00060 */  lwc1  $f16, 0x60($sp)
/* 0573B0 7F022880 E7A40064 */  swc1  $f4, 0x64($sp)
/* 0573B4 7F022884 C6480008 */  lwc1  $f8, 8($s2)
/* 0573B8 7F022888 C4460038 */  lwc1  $f6, 0x38($v0)
/* 0573BC 7F02288C 46108482 */  mul.s $f18, $f16, $f16
/* 0573C0 7F022890 C7A40064 */  lwc1  $f4, 0x64($sp)
/* 0573C4 7F022894 46083281 */  sub.s $f10, $f6, $f8
/* 0573C8 7F022898 46042182 */  mul.s $f6, $f4, $f4
/* 0573CC 7F02289C E7AA0068 */  swc1  $f10, 0x68($sp)
/* 0573D0 7F0228A0 C7AA0068 */  lwc1  $f10, 0x68($sp)
/* 0573D4 7F0228A4 460A5402 */  mul.s $f16, $f10, $f10
/* 0573D8 7F0228A8 46069200 */  add.s $f8, $f18, $f6
/* 0573DC 7F0228AC 0C007DF8 */  jal   sqrtf
/* 0573E0 7F0228B0 46088300 */   add.s $f12, $f16, $f8
/* 0573E4 7F0228B4 C6440014 */  lwc1  $f4, 0x14($s2)
/* 0573E8 7F0228B8 C6460008 */  lwc1  $f6, 8($s2)
/* 0573EC 7F0228BC C64A0034 */  lwc1  $f10, 0x34($s2)
/* 0573F0 7F0228C0 46002482 */  mul.s $f18, $f4, $f0
/* 0573F4 7F0228C4 02402025 */  move  $a0, $s2
/* 0573F8 7F0228C8 8FA500F0 */  lw    $a1, 0xf0($sp)
/* 0573FC 7F0228CC 02203825 */  move  $a3, $s1
/* 057400 7F0228D0 8FB800D8 */  lw    $t8, 0xd8($sp)
/* 057404 7F0228D4 27B900A4 */  addiu $t9, $sp, 0xa4
/* 057408 7F0228D8 240B0001 */  li    $t3, 1
/* 05740C 7F0228DC 46069080 */  add.s $f2, $f18, $f6
/* 057410 7F0228E0 46001087 */  neg.s $f2, $f2
/* 057414 7F0228E4 460A103C */  c.lt.s $f2, $f10
/* 057418 7F0228E8 00000000 */  nop
/* 05741C 7F0228EC 4502000E */  bc1fl .L7F022928
/* 057420 7F0228F0 8FAC00D4 */   lw    $t4, 0xd4($sp)
/* 057424 7F0228F4 8FA800A0 */  lw    $t0, 0xa0($sp)
/* 057428 7F0228F8 8FA9009C */  lw    $t1, 0x9c($sp)
/* 05742C 7F0228FC 8FAA00DC */  lw    $t2, 0xdc($sp)
/* 057430 7F022900 44061000 */  mfc1  $a2, $f2
/* 057434 7F022904 AFB80010 */  sw    $t8, 0x10($sp)
/* 057438 7F022908 AFB90014 */  sw    $t9, 0x14($sp)
/* 05743C 7F02290C AFAB0024 */  sw    $t3, 0x24($sp)
/* 057440 7F022910 AFA00028 */  sw    $zero, 0x28($sp)
/* 057444 7F022914 AFA80018 */  sw    $t0, 0x18($sp)
/* 057448 7F022918 AFA9001C */  sw    $t1, 0x1c($sp)
/* 05744C 7F02291C 0FC0EE70 */  jal   chrpropAddBulletHit
/* 057450 7F022920 AFAA0020 */   sw    $t2, 0x20($sp)
/* 057454 7F022924 8FAC00D4 */  lw    $t4, 0xd4($sp)
.L7F022928:
/* 057458 7F022928 8FAD00F0 */  lw    $t5, 0xf0($sp)
/* 05745C 7F02292C 5180000F */  beql  $t4, $zero, .L7F02296C
/* 057460 7F022930 8FBF0044 */   lw    $ra, 0x44($sp)
/* 057464 7F022934 C6500034 */  lwc1  $f16, 0x34($s2)
/* 057468 7F022938 C5A80018 */  lwc1  $f8, 0x18($t5)
/* 05746C 7F02293C 8FAE00EC */  lw    $t6, 0xec($sp)
/* 057470 7F022940 4610403E */  c.le.s $f8, $f16
/* 057474 7F022944 00000000 */  nop
/* 057478 7F022948 45020008 */  bc1fl .L7F02296C
/* 05747C 7F02294C 8FBF0044 */   lw    $ra, 0x44($sp)
/* 057480 7F022950 8DCF0014 */  lw    $t7, 0x14($t6)
/* 057484 7F022954 81D9000A */  lb    $t9, 0xa($t6)
/* 057488 7F022958 35F80004 */  ori   $t8, $t7, 4
/* 05748C 7F02295C 27280001 */  addiu $t0, $t9, 1
/* 057490 7F022960 ADD80014 */  sw    $t8, 0x14($t6)
/* 057494 7F022964 A1C8000A */  sb    $t0, 0xa($t6)
/* 057498 7F022968 8FBF0044 */  lw    $ra, 0x44($sp)
.L7F02296C:
/* 05749C 7F02296C 8FB00038 */  lw    $s0, 0x38($sp)
/* 0574A0 7F022970 8FB1003C */  lw    $s1, 0x3c($sp)
/* 0574A4 7F022974 8FB20040 */  lw    $s2, 0x40($sp)
/* 0574A8 7F022978 03E00008 */  jr    $ra
/* 0574AC 7F02297C 27BD00F0 */   addiu $sp, $sp, 0xf0
)
#endif
#endif


/**
 * Address: 7F022980
 * 
 * Resolves a known hit against a character. 
 */
void chrHandleBulletHit(struct ShotData *shot, struct BulletHit *bhit)
{
    f32 scale;
    Mtxf invmtx;
    coord3d nearhitpos;
    s32 temp;
    coord3d hitpos;
    Mtxf *mtx;
    ChrRecord *chr;
    PropRecord *prop;
    struct image_sound *sound;
    WeaponObjRecord *weaponobj;
    s32 i;
    s32 temp2;
    coord3d jointpos;
    struct image_sound *sound2;
    
    chr = bhit->prop->chr;

    // Calculate the view space hit position for impact effects.
    hitpos.f[0] = shot->viewOrigin.x - ((bhit->dist * shot->viewDir.x) / shot->viewDir.z);
    hitpos.f[1] = shot->viewOrigin.y - ((bhit->dist * shot->viewDir.y) / shot->viewDir.z);
    hitpos.f[2] = shot->viewOrigin.z - bhit->dist;

    scale = 1.0f - (42.0f / sqrtf(SQ(hitpos.f[0]) + SQ(hitpos.f[1]) + SQ(hitpos.f[2])));

    /** Use scale to create a near hit position closer to the camera.
     *  Probably to prevent the tracer effect and smoke puffs from landing inside the character model.
    */
    nearhitpos.x = hitpos.f[0] * scale;
    nearhitpos.y = hitpos.f[1] * scale;
    nearhitpos.z = hitpos.f[2] * scale;

    mtx4TransformVecInPlace(currentPlayerGetViewToWorldMtxf(), &nearhitpos);

    // Point the tracer effect towards the impact position.
    gunSetTracerTarget(&nearhitpos);

    // Make a fleshy impact sound.
    recall_joy2_hits_edit_detail_edit_flag(shot->weapon, bhit->prop, -1);

    chrCreateHitPuffs(bhit->prop, bhit->hitpart, &hitpos, &nearhitpos);

    // Apply damage to the character.
    if (!handles_shot_actors(chr, bhit->hitpart, &shot->dir, shot->weapon, TRUE)) {
        return;
    }

    if (bhit->hitpart == HIT_GUN) {
        for (i = 0; i != 2; i++) {
            prop = chr->weapons_held[i];

            if (prop != NULL) {
                weaponobj = (WeaponObjRecord *)prop->obj;
                /**
                 * If a character is holding an explosive such as a grenade, and the explosive is shot, detonate it.
                 */
                if (weaponobj->model == bhit->model) {
                    if (
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_GRENADE || 
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_GRENADEROUND || 
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_ROCKETROUND || 
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_TIMEDMINE || 
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_BOMBCASE  || 
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_REMOTEMINE  || 
                        ((WeaponObjRecord *)weaponobj)->weaponnum == ITEM_PROXIMITYMINE
                    )
                    {
                        propobjSetDropped(prop, 1);
                        chr->hidden |= 1;
                    
                        maybe_detonate_object(prop->obj, gunItemGetDestructionAmount(shot->weapon), &hitpos, shot->weapon, get_cur_playernum());
                    // Create a bullet hole on the character's held weapon.
                    } else {
                        if (bhit->hit.texturenum < 0) {
                            sound = g_HitTypeSounds[0];
                        } else {
                            sound = g_HitTypeSounds[g_Textures[bhit->hit.texturenum].hitTexture];
                        }

                        temp = randomGetNext() % (s16)sound->thing2_len;
                        explosionCreateBulletImpact(&bhit->hit.hitpos, &bhit->hit.normal, sound->thing2[temp], 1, prop, bhit->room, 0);
                    }
                }
            }
        }

        return;
    }

    // Create a bullet on hole on a hat or helmet attached to a character's head.
    if (bhit->hitpart == HIT_HAT) {
        if (bhit->hit.texturenum < 0) {
            sound2 = g_HitTypeSounds[0];
        } else {
            sound2 = g_HitTypeSounds[g_Textures[bhit->hit.texturenum].hitTexture];
        }

        temp2 = randomGetNext() % (s16)sound2->thing2_len; 
        explosionCreateBulletImpact(&bhit->hit.hitpos, &bhit->hit.normal, sound2->thing2[temp2], 1, chr->handle_positiondata_hat, bhit->room, 0);

        return;
    }

    mtx = modelFindNodeMtx(bhit->model, bhit->node, 0);

    jointpos.x = hitpos.f[0];
    jointpos.y = hitpos.f[1];
    jointpos.z = hitpos.f[2];

    jointpos.x += (jointpos.x - mtx->m[3][0]) * 0.5f;
    jointpos.y += (jointpos.y - mtx->m[3][1]) * 0.5f;
    jointpos.z += (jointpos.z - mtx->m[3][2]) * 0.5f;

    jointpos.x -= getjointsize(bhit->model, bhit->node) * 0.5f * shot->viewDir.x;
    jointpos.y -= getjointsize(bhit->model, bhit->node) * 0.5f * shot->viewDir.y;
    jointpos.z -= getjointsize(bhit->model, bhit->node) * 0.5f * shot->viewDir.z;

    matrix_4x4_set_inverse_rotation_and_translation(mtx, &invmtx);
    mtx4TransformVecInPlace(&invmtx, &jointpos);

    // Create a blood stain at the impact point.
    chrCreateBloodStain(bhit->model, bhit->hitpart, bhit->node, &jointpos);
}


/**
 * Removed.
 *
 * Address 0x7F022E1C.
 */
void chrRemoved7F022E1C(f32 arg0)
{
    // removed
}


void setanimationdebugflag(s32 param_1)
{
  D_8002C904 = param_1;
  return;
}


/**
 * Decrements g_AnimationTablePointerCountRelated.
 * If less than zero, the variable will then be set to the
 * number of non-zero entries in animation_table_ptrs1.
 *
 * Address 0x7F022E30.
 */
void chrDecrementAnimationTablePointerCount(void)
{
    g_AnimationTablePointerCountRelated--;

    if (g_AnimationTablePointerCountRelated < 0)
    {
        for (
            g_AnimationTablePointerCountRelated = 0;
            animation_table_ptrs1[g_AnimationTablePointerCountRelated+1] != 0;
            g_AnimationTablePointerCountRelated++)
        {
            // nothing to do.
        }
    }
}


/**
 * Decrements g_AnimationTablePointerCountRelated.
 * If the entry in animation_table_ptrs1 at that index is zero
 * then the global variable will be set to zero.
 *
 * Address 0x7F022E90.
 */
void chrIncrementAnimationTablePointerCount(void)
{
    g_AnimationTablePointerCountRelated++;

    if (animation_table_ptrs1[g_AnimationTablePointerCountRelated] == 0)
    {
        g_AnimationTablePointerCountRelated = 0;
    }
}


/**
 * Address 0x7F022EC8.
 */
void chrToggleD_8002C90C(void)
{
    D_8002C90C = !D_8002C90C;
}


void sub_GAME_7F022EE0(s32 param_1){
  D_8002C910 = param_1;
}


/**
 * Iterates all guards and checks if the noise is within the hearing scale distance.
 *
 * @param noise: noise amount to check.
 *
 * Address 0x7F022EEC.
 */
void chrCheckGuardsHeardSound(f32 noise)
{
    s32 i;

    for (i=0; i<g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL)
        {
            if (chrGetDistanceToBond(&g_ChrSlots[i]) < g_ChrSlots[i].hearingscale * (noise * 100.0f))
            {
                chrlvAlertGuardToPlayerPosition(&g_ChrSlots[i]);
            }
        }
    }
}


/**
 * Iterates g_ChrSlots. Returns the first object that (1) model
 * is not null and (2) chrnum matches index.
 *
 * Address 0x7F022FC8.
 * chrFindByLiteralId
 */
ChrRecord* chrFindByLiteralId(s32 index)
{
    s32 i;

    for (i=0; i<g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL && g_ChrSlots[i].chrnum == index)
        {
            return &g_ChrSlots[i];
        }
    }

    return NULL;
}


/**
 * Address 0x7F02302C.
 */
PropRecord *chrGetEquippedWeaponProp(ChrRecord *self, GUNHAND hand)
{
    return self->weapons_held[hand]; //0x160
}


/**
 * Address 0x7F02303C.
 */
PropRecord *chrGetEquippedWeaponPropWithCheck(ChrRecord *self, GUNHAND hand)
{
    PropRecord *gunprop = self->weapons_held[hand];
    if (gunprop)
    {
        WeaponObjRecord *wep = gunprop->weapon;

        if (bondwalkItemCheckBitflags(wep->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0)
        {
            gunprop = NULL;
        }
    }
    return gunprop;
}


/**
 * Updates character collision bounds based on chracter width.
 *
 * @param arg0: prop
 * @param arg1: out parameter, will contain character collision_bounds.
 * @param arg2: out parameter, will contain 0 or 4
 * @param y_out: out parameter, will be character ground + character height
 * @param ground: out parameter, will contain character ground
 *
 * Address 0x7F02308C.
 */
void chrUpdateCollisionBounds(PropRecord *prop, rect4f **polygon, s32 *edges, f32 *y_out, f32 *ground)
{
    ChrRecord *chr;

    chr = prop->chr;

    if (
        (chr->actiontype != ACT_DIE) &&
        (chr->actiontype != ACT_DEAD) &&
        ((chr->chrflags & (CHRFLAG_00010000 | CHRFLAG_HIDDEN)) == 0) &&
        ((chr->hidden & CHRHIDDEN_MOVING) == 0)
        )
    {
        *edges = 4;
        *polygon = &chr->collision_bounds;

        //collision box is a diamond around chr

        chr->collision_bounds.f[0] = prop->pos.x + chr->chrwidth;
        chr->collision_bounds.f[1] = prop->pos.z;

        chr->collision_bounds.f[2] = prop->pos.x;
        chr->collision_bounds.f[3] = prop->pos.z + chr->chrwidth;

        chr->collision_bounds.f[4] = prop->pos.x - chr->chrwidth;
        chr->collision_bounds.f[5] = prop->pos.z;

        chr->collision_bounds.f[6] = prop->pos.x;
        chr->collision_bounds.f[7] = prop->pos.z - chr->chrwidth;

        *ground = chr->ground;
        *y_out = *ground + chr->chrheight;

        return;
    }

    *edges = 0;
}


/**
 * @param arg0: prop
 * @param width: out parameter, will be set to character width
 * @param height: out parameter, will be set to character height - 20
 * @param always_20: out parameter, will be set to 20
 *
 * Address 0x7F023160.
 */
void chrGetChrWidthHeight(PropRecord *arg0, f32 *width, f32 *height, f32 *always_20)
{
    void *temp_v0;

    ChrRecord *c = arg0->chr;

    *width = c->chrwidth;
    *height = c->chrheight - 20.0f;
    *always_20 = 20.0f;
}


/**
 * Address 0x7F023188.
 */
f32 chrGetChrGround(PropRecord *arg0)
{
    ChrRecord *c = arg0->chr;
    return c->ground;
}


/**
 * Calculate auto aim position coordinates.
 *
 * US address 7F023194.
*/
s32 chrGetOnscreenRenderBounds(PropRecord *arg0, struct coord3d *arg1, struct coord2d *arg2, struct coord2d *arg3)
{
    struct ChrRecord *temp_v1;

    temp_v1 = arg0->chr;

    if (arg0->flags & PROPFLAG_ONSCREEN)
    {
        if ((temp_v1->actiontype != ACT_DIE) && (temp_v1->actiontype != ACT_DEAD) && !(temp_v1->chrflags & CHRFLAG_NO_AUTOAIM))
        {
            struct Model *model;
            RenderPosView *model_render_pos_1;
            RenderPosView *model_render_pos_2;

            model = temp_v1->model;
            model_render_pos_1 = &model->render_pos[0];
            model_render_pos_2 = &model->render_pos[1];

            arg1->f[2] = model_render_pos_2->pos.m[3][2] + ((model_render_pos_1->pos.m[3][2] - model_render_pos_2->pos.m[3][2]) * 0.25f);

            if (arg1->f[2] < 0.0f)
            {
                arg1->f[0] = model_render_pos_2->pos.m[3][0] + ((model_render_pos_1->pos.m[3][0] - model_render_pos_2->pos.m[3][0]) * 0.25f);
                arg1->f[1] = model_render_pos_2->pos.m[3][1] + ((model_render_pos_1->pos.m[3][1] - model_render_pos_2->pos.m[3][1]) * 0.25f);

                arg3->y = 0.0f;
                arg3->x = 0.0f;

                arg2->x = 0.0f;
                arg2->y = 0.0f;

                modelGetXYExtents(model, &arg2->y, &arg2->x, &arg3->y, &arg3->x);

                return 1;
            }
        }
    }

    return 0;
}
