#include <ultra64.h>
#include <math.h>
#include <bondtypes.h>
#include <boss.h>
#include <fr.h>
#include <joy.h>
#include <music.h>
#include <snd.h>
#include <str.h>
#include <options.h>
#include "bg.h"
#include "bgroomtrans.h"
#include "blood_animation.h"
#include "bondhead.h"
#include "bondinv.h"
#include "bondview.h"
#include "chr.h"
#include "chr_b.h"
#include "chraction.h"
#include "chrai.h"
#include "debugmenu_handler.h"
#include "explosion.h"
#include "file.h"
#include "frametiming.h"
#include "front.h"
#include "glass.h"
#include "gun.h"
#include "initanitable.h"
#include "language.h"
#include "loadobjectmodel.h"
#include "lv.h"
#include "math_atan2f.h"
#include "matrixmath.h"
#include "model.h"
#include "mp_music.h"
#include "mpmenu.h"
#include "objecthandler.h"
#include "objective_status.h"
#include "os_extension.h"
#include "player.h"
#include "propobj.h"
#include "quaternion.h"
#include "random.h"
#include "stan.h"
#include "stanintersection.h"
#include "textrelated.h"

#ifdef VERSION_EU

    #define BONDVIEW_AUTOAIM_TIME 0x19 /* 25 */

    #define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_A 0x1a
    #define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_B 0x19
    #define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_C 0x64

    #define BONDVIEW_UPPER_TEXT_TIMER_A 0x33
    #define BONDVIEW_UPPER_TEXT_TIMER_B 0x32
    #define BONDVIEW_UPPER_TEXT_TIMER_C 0xc8

    #define EU_CAMERA_8003642C_ASPECT 1.19047617912f

#else

    #define BONDVIEW_AUTOAIM_TIME 0x1e /* 30 */

    #define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_A 0x1f
    #define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_B 0x1e
    #define BONDVIEW_INTRO_CAMERA_BONDMESSCNT_C 0x78

    #define BONDVIEW_UPPER_TEXT_TIMER_A 0x3d
    #define BONDVIEW_UPPER_TEXT_TIMER_B 0x3c
    #define BONDVIEW_UPPER_TEXT_TIMER_C 0xf0

#endif

/*cannonically these are both*/
#define BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH 0x97
#define BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH 0x65
/*these*/
#define MAXTALKMESSLEN 150
#define MAXMESSAGELEN 100



#if defined(VERSION_US)
    #define BONDVIEW_2ND_FONTTABLE(_param) copy_2ndfonttable
    #define BONDVIEW_1ST_FONTTABLE(_param) copy_1stfonttable
#elif defined(VERSION_JP) || defined(VERSION_EU)
    #define BONDVIEW_2ND_FONTTABLE(_param) dword_CODE_bss_jp80079CEC[_param]
    #define BONDVIEW_1ST_FONTTABLE(_param) dword_CODE_bss_jp80079Cd8[_param]
#endif


#if defined(VERSION_US) || defined(VERSION_JP)
    #define BONDVIEW_VIEW_TOP_OFFSET_1 0x0C
    #define BONDVIEW_VIEW_TOP_OFFSET_2 0x28
    #define BONDVIEW_VIEW_TOP_OFFSET_3 0x10
#elif defined(VERSION_EU)
    #define BONDVIEW_VIEW_TOP_OFFSET_1 0x16
    #define BONDVIEW_VIEW_TOP_OFFSET_2 0x32
    #define BONDVIEW_VIEW_TOP_OFFSET_3 0x14
#endif


#if defined(VERSION_EU)
    #define TANKUPDATEROTATION_SCALE 0.904799997807f
    #define TANKTURRETVERTICALANGLERELATED_SCALE 0.928399980068f
    #define TANK_UNKD0_SCALE 0.79960000515f
    #define CHR_OBJ_ACCEL_SPEED_FACTOR 0.6f
    #define CHR_OBJ_MAXSPEED 6.0f
    #define MAX_SPEED_FACTOR 0.8f
    #define TANK_DAMAGE_PENTALTY_TICKS 75

    #define TANK_VERT_ANGLE_FACTOR 0.0716000199318f
    #define TANK_VERT_ANGLE_RAD_FACTOR 0.0952f

    #define MAX_AIMLOCK_SPEED_DEFAULT 0.8344f

    #define THREE_SECOND_TICKS 150
    #define PLAYER_TICKEXPLODE_FACTOR 12

    #define CLIPPING_CLOCK_FACTOR 0.765100002289f
    #define CLIPPING_FIELD88_FACTOR 0.234899997711f
    #define CLIPPING_FIELD8C_VALUE 12
    #define CLIPPING_FIELD90_VALUE -5.625f
#else
    #define TANKUPDATEROTATION_SCALE 0.92f
    #define TANKTURRETVERTICALANGLERELATED_SCALE 0.94f
    #define TANK_UNKD0_SCALE 0.83f
    #define CHR_OBJ_ACCEL_SPEED_FACTOR 0.5f
    #define CHR_OBJ_MAXSPEED 5.0f
    #define MAX_SPEED_FACTOR 0.8f
    #define TANK_DAMAGE_PENTALTY_TICKS 90

    #define TANK_VERT_ANGLE_FACTOR 0.0600000023842f
    #define TANK_VERT_ANGLE_RAD_FACTOR 0.0799999833107f
    #define MAX_AIMLOCK_SPEED_DEFAULT 0.86f

    #define THREE_SECOND_TICKS 180
    #define PLAYER_TICKEXPLODE_FACTOR 15

    #define CLIPPING_CLOCK_FACTOR 0.8f
    #define CLIPPING_FIELD88_FACTOR 0.19999999f
    #define CLIPPING_FIELD8C_VALUE 15
    #define CLIPPING_FIELD90_VALUE -4.5f
#endif

#define FULL_CROUCH_OFFSET -100.0f

#define SPEED_REGULAR_MAX  1.0f
#define SPEED_RUN_MAX      1.25f
#define SPEED_TICK_ADJUST  0.01f
#define TANK_MAX_SPEED     15.0f


#define FLOAT_TEN_A 10.0f
#define FLOAT_TEN_B 10.00f


// bss
coord3d g_CamFrustumTopNormal;
f32 g_CamFrustumTopOffset;
coord3d g_CamFrustumBottomNormal;
f32 g_CamFrustumBottomOffset;
coord3d g_CamFrustumLeftNormal;
f32 g_CamFrustumLeftOffset;
coord3d g_CamFrustumRightNormal;
f32 g_CamFrustumRightOffset;
//CODE.bss:80079980
f32 g_CamFrustumNearOffset;
//CODE.bss:80079984
f32 flt_CODE_bss_80079984;
//CODE.bss:80079988
f32 flt_CODE_bss_80079988;
//CODE.bss:8007998C
f32 flt_CODE_bss_8007998C;

//CODE.bss:80079990
vec3d g_ForceBondMoveOffset;
// //CODE.bss:80079994
// f32 flt_CODE_bss_80079994;
// //CODE.bss:80079998
// f32 flt_CODE_bss_80079998;

//CODE.bss:8007999C
s32 g_SurroundBondWithExplosionsTicks;
//CODE.bss:800799A0
s32 g_PlayerTickExplodeCreatePosition;
//CODE.bss:800799A4
s32 dword_CODE_bss_800799A4;

//CODE.bss:800799A8
struct coord3d g_TankModelPositionOffset;

//CODE.bss:800799B4
s32 g_TankEngineSfxVolume;

/**
 * Address 0x800799B8.
 * State 0: begin.
 * State 1: Finished sitting down/turning, queue audio.
 * State 2: complete
*/
s32 g_EnterTankAudioState;

/**
 * Address 0x800799BC.
*/
f32 g_TankEnteringSitHeight;

/**
 * Address 0x800799C0.
*/
f32 g_TankEnteringSitHeightRemain;

/**
 * Address 0x800799C4.
*/
f32 g_TankEnterBondHorizAngleDeg;

/**
 * Address 0x800799C8.
*/
f32 g_TankEnterBondVertAngleDeg;

//CODE.bss:800799CC
f32 flt_CODE_bss_800799CC;

//CODE.bss:800799D0
struct coord3d g_EnterTankCoord;

//CODE.bss:800799DC
f32 flt_CODE_bss_800799DC;

//CODE.bss:800799E0
//s32 starting_right_weapon;
//s32 starting_left_weapon;
ITEM_IDS starting_weapon[2];

//CODE.bss:800799E8
struct coord3d flt_CODE_bss_800799E8;

//CODE.bss:800799F4
struct PropRecord* dword_CODE_bss_800799F4;

//CODE.bss:800799F8
PadRecord * dword_CODE_bss_800799F8;
//CODE.bss:800799FC
CutsceneRecord *gBondViewCutscene;
//CODE.bss:80079A00
f32 flt_CODE_bss_80079A00;
//CODE.bss:80079A04
f32 flt_CODE_bss_80079A04;
//CODE.bss:80079A08
f32 flt_CODE_bss_80079A08;
//CODE.bss:80079A0C
f32 flt_CODE_bss_80079A0C;
//CODE.bss:80079A10
f32 flt_CODE_bss_80079A10;
//CODE.bss:80079A14
s32 dword_CODE_bss_80079A14;
//CODE.bss:80079A18
enum CAMERAMODE dword_CODE_bss_80079A18;
//CODE.bss:80079A1C
s32 dword_CODE_bss_80079A1C;
//CODE.bss:80079A20
s32 mission_timer;

#if defined(VERSION_JP) || defined(VERSION_EU)
//CODE.bss:80079A24
f32 watch_time_0;
#else
//CODE.bss:80079A24
s32 watch_time_0;
#endif

/**
 * Address 80079A28
 * EU .bss 80068508
*/
char stringbuffer_lowerleft[0x5][BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH];
char dword_CODE_bss_80079c21[0x04];

#if defined(BUGFIX_R1)
//CODE.bss:80079Cd8
s32 dword_CODE_bss_jp80079Cd8[0x05];
s32 dword_CODE_bss_jp80079CEC[0x05];
#endif

/**
 * Address 80079C28
 * EU .bss 80068738
*/
PadRecord *g_Startpad[0x10];

//CODE.bss:80079C68
s32 startpadcount;
//CODE.bss:80079C6C
s32 dword_CODE_bss_80079C6C;

#if defined LEFTOVERDEBUG
//CODE.bss:80079C70
/***/
char stringbuffer_top[0x2][BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH];
u16 dword_CODE_bss_80079d9E;
#endif

//CODE.bss:80079DA0
/**
 * EU .bss 80078780
*/
StandTilePoint *dword_CODE_bss_80079DA0;
//CODE.bss:80079DA4
StandTilePoint *dword_CODE_bss_80079DA4;

//CODE.bss:80079DA8
s32 dword_CODE_bss_80079DA8[BSS_80079DA8_LENGTH];

#ifndef VERSION_EU
//CODE.bss:80079DC8
char dword_CODE_bss_80079DC8[0x3C];
#else
char dword_CODE_bss_80079DC8[0x2][BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH];
char dword_CODE_bss_80079EF6[0x3C];
#endif

//CODE.bss:80079E04
f32 g_MpSwirlRotateSpeed;
//CODE.bss:80079E08
f32 g_MpSwirlAngleDegrees;
//CODE.bss:80079E0C
f32 g_MpSwirlForwardSpeed;
//CODE.bss:80079E10
f32 g_MpSwirlDistance;


// data
//D:80036420
s32 D_80036420 = 0;

/**
 * When set, will increment each tick until reaching a threshold value (4).
 * Then current items will be unequipped from left and run hands.
 * Address 0x80036424.
*/
s32 g_bondviewForceDisarm = 0;

//D:80036428
s32 resolution = 0;
//D:8003642C
s32 cameraBufferToggle = 0;
//D:80036430
s32 cameraFrameCounter1 = 0;
//D:80036434
s32 cameraFrameCounter2 = 0;
//D:80036438
s32 camera_80036438 = 0;
//D:8003643C
s32 credits_state = 0;
//D:80036440
CreditsEntry *credits_pointer = NULL;
//D:80036444
s32 g_SurroundBondWithExplosionsFlag = 0;

//D:80036448
s32 g_PlayerIsInTank = 0;

//D:8003644C
struct PropRecord *g_WorldTankProp = NULL;

//D:80036450 cannonically bondonprop2
struct PropRecord *g_PlayerTankProp = NULL;

/**
 * Related to g_PlayerTankProp.
 * Address 0x80036454.
 */
f32 g_PlayerTankYOffset = 0;

/**
 * US address 80036458.
*/
ALSoundState * g_TankSfxState[2] = { NULL, NULL };

/**
 * min -3.749999, max +3.749999
 * Address 0x80036460.
*/
f32 g_TankTurnSpeed = 0;

/**
 * Address 0x80036464.
*/
f32 g_TankOrientationAngle = 0;

//D:80036468
f32 tank_turret_unused_angle = 0.0f;

/**
 * Argument to sinf,cosf.
 *
 * Address 0x8003646C.
 */
f32 g_TankTurretVerticalAngle = 0;

/**
 * Address 0x80036470.
*/
f32 g_TankTurretVerticalAngleRelated = 0;

/**
 * Address 0x80036474.
*/
f32 g_TankTurretOrientationAngleRad = 0;

//D:80036478
f32 g_TankTurretOrientationAngleDeg = 0;

//D:8003647C
f32 tank_turret_turn_speed = 0;

/**
 * Can enter tank, remains set once Bond is in tank.
 * Address 0x80036480.80036480
*/
s32 g_BondCanEnterTank = 0;

/**
 * Address 0x80036484.
*/
f32 g_TankTurretAngle = 0;

/**
 * Address 0x80036488.
*/
f32 g_TankTurretTurn = 0;

//D:8003648C
s32 g_ExplodeTankOnDeathFlag = 0;
//D:80036490
s32 g_TankDamagePenaltyTicks = 0;
//D:80036494
enum CAMERAMODE g_CameraMode = CAMERAMODE_NONE;
//D:80036498
enum CAMERAMODE g_CameraAfterCinema = CAMERAMODE_NONE;
//D:8003649C
s32 camera_fade_active = 0;
//D:800364A0
s32 stop_time_flag = 0;
//D:800364A4
f32 camera_transition_timer = 0;
//D:800364A8
s32 intro_camera_index = 1;
//D:800364AC
struct SetupIntroSwirl *g_IntroSwirl = NULL;
//D:800364B0
s32 is_timer_active = 1;
//D:800364B4
bool g_PlayerInvincible = FALSE;
//D:800364B8
struct SetupIntroCamera* g_CurrentSetupIntroCamera = NULL;
//D:800364BC
s32 g_SetupIntroCameraCount = 0;
//D:800364C0
struct SetupIntroCamera *ptr_random06cam_entry = NULL;

/**
 * Flag to toggle invisibility cheat.
 * 1 = visible to guards
 * 0 = not visible to guards
 *
 * Address 0x800364C4.
 */
s32 g_VisibleToGuardsFlag = TRUE;

//D:800364C8
s32 obj_collision_flag = TRUE;
//D:800364CC
f32 D_800364CC = 1.0;
//D:800364D0
f32 D_800364D0 = 1.0;
//D:800364D4
f32 D_800364D4 = 1.0;

/**
 * Address 0x800364D8.
*/
s32 g_bondviewBondDeathAnimations[] = {
    PTR_ANIM_death_forward_face_down,
    PTR_ANIM_death_forward_spin_face_up,
    PTR_ANIM_death_backward_fall_face_up1,
    PTR_ANIM_death_backward_spin_face_down_right,
    PTR_ANIM_death_backward_spin_face_up_right,
    PTR_ANIM_death_backward_spin_face_down_left,
    PTR_ANIM_death_backward_spin_face_up_left,
    PTR_ANIM_death_forward_face_down_hard,
    PTR_ANIM_death_forward_face_down_soft,
    PTR_ANIM_death_fetal_position_right,
    PTR_ANIM_death_fetal_position_left,
    PTR_ANIM_death_backward_fall_face_up2,
    0
};

/**
 * Address 0x8003650C.
*/
s32 g_bondviewBondDeathAnimationsCount = 0;

//D:80036510
enum CAMERAMODE camera_mode = CAMERAMODE_NONE;
//D:80036514
s32 g_IntroAnimationIndex = 0;

//D:80036518
struct struct_4 stage_intro_anim_table[] = {
    {PTR_ANIM_extending_left_hand, 95.0, -1.0, 0.02},
    {PTR_ANIM_fire_standing_draw_one_handed_weapon_fast, 7.0, 40.0, 0.5},
    {PTR_ANIM_draw_one_handed_weapon_and_look_around, 0.0, -1.0, 0.5},
    {PTR_ANIM_draw_one_handed_weapon_and_stand_up, 0.0, -1.0, 0.5},
    {PTR_ANIM_aim_one_handed_weapon_left_right, 0.0, -1.0, 0.5},
    {PTR_ANIM_cock_one_handed_weapon_and_turn_around, 0.0, -1.0, 0.5},
    {PTR_ANIM_cock_one_handed_weapon_turn_around_and_stand_up, 0.0, -1.0, 0.5},
    {PTR_ANIM_draw_one_handed_weapon_and_turn_around, 0.0, -1.0, 0.5},
    {PTR_ANIM_bond_eye_fire_alt, 0.0, -1.0, 0.5}
};

//D:800365A8
f32 watch_transition_time = 0.90909088;

//D:800365AC
WeaponObjRecord dummy_08_pp7_obj[] = {
    0x0100,
    0x00,
    0x08,
    PROP_CHRWPPK,
    0x4000,
    0x00000000,
    0x00000000,
    NULL,
    NULL,
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    },
    {0.0f, 0.0f, 0.0f},
    {0x00000000},
    NULL,
    NULL,
    0.0f,
    1000.0f,
    {0xff, 0xff, 0xff, 0x00},
    0xff,
    0xff,
    0xff,
    0x00,
    ITEM_WPPK,
    -1,
    -1,
    NULL
};


//D:80036634
struct DamageType g_DamageTypes[] = {
        {   0,    10.0,    60.0,    0.6,    0,    5.0,    40.0,    1.0,            0xFF,       0xFF,       0xFF}, // 1 bars
        {   0,    10.0,    60.0,    0.6,    0,    5.0,    40.0,    1.0,            0xFF,       0xFF,       0xFF},
        {   0,    10.0,    50.0,    0.6,    0,    5.0,    30.0,    0.800000011921, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    40.0,    0.6,    0,    5.0,    25.0,    0.600000023842, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    35.0,    0.6,    0,    5.0,    22.0,    0.550000011921, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    30.0,    0.6,    0,    5.0,    19.0,    0.5,            0xFF,       0xFF,       0xFF},
        {   0,    10.0,    30.0,    0.6,    0,    5.0,    17.0,    0.449999988079, 0xFF,       0xFF,       0xFF},
        {   0,    10.0,    30.0,    0.6,    0,    5.0,    15.0,    0.40000000596,  0xFF,       0xFF,       0xFF}  // 8 bars
};


//D:80036794
/**
 * The second column is how many frames before the gauge switches from showing the old health to the new health.
 * The third column is how many frames before the health display is hidden.
 */
struct HealthDisplayDuration g_HealthDisplayDurations[8] = {
    { 0, 40, 100 }, // 1 bar of health
    { 0, 30, 80 },
    { 0, 20, 60 },
    { 0, 20, 60 },
    { 0, 20, 60 },
    { 0, 20, 50 },
    { 0, 20, 50 },
    { 0, 20, 50 }  // 8 bars of health
};

/**
 * US Address 0x800367F4.
*/
struct coord3d g_DefaultMoveBondOffset = { 0 };


/**
 * struct player property `pos` .
 * US address 80036800.
 */
struct coord3d g_DefaultFrozenPlayerPos = { 0 };

/**
 * struct player property `pos2`.
 * US address 8003680C.
 */
struct coord3d g_DefaultFrozenPlayerPos2 = { 0, 0, 1.0f };

/**
 * struct player property `offset`.
 * US address 80036818.
 */
struct coord3d g_DefaultFrozenPlayerOffset = { 0, 1.0f, 0 };

/**
 * struct player property `offset`.
 * US address 80036824.
 */
struct coord3d g_DefaultFrozenMoveOffset = { 0 };

//D:80036830
struct coord3d D_80036830 = { 0 };

ModelRenderData D_8003683C = {NULL,
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

//D:8003687C
s32 D_8003687C = 0;

//D:80036880
s32 D_80036880 = 0;
//D:80036884
s32 D_80036884 = 0;
//D:80036888
s32 D_80036888 = 0;

//D:8003688C
coord3d D_8003688C = {0};
//D:80036898
s32 status_bar_text_buffer_index = 0;
//D:8003689C
s32 display_statusbar = 0;
#ifdef BUGFIX_R0
//D:800368A0
s32 copy_1stfonttable = 0;
//D:800368A4
s32 copy_2ndfonttable = 0;
#endif
//D:800368A8
s32 upper_text_buffer_index = 0;
//D:800368AC
s32 display_upper_text_window = 0;
//D:800368B0
s32 upper_text_window_timer = 0xFFFFFFFF;
s32 g_UpperTextDisplayFlag = 0;
//D:800368B8
DirectionLabels D_800368B8 = {{"n", "ne", "e", "se", "s", "sw", "w", "nw", "n"}};
u8 D_800368B8_tail[3] = "";

//D:800368D8
struct firing_anim_struct firing_animation_groups[][6] = {
    {{pistol_firing_animation_group1, 0, 0.1, 79.0, 87.0},
     {&D_80030660[2], 0, 0.5, 0.0, -1.0},
     {&D_80030660[3], 0, 0.5, 0.0, -1.0},
     {&D_80030660[8], 0, 0.5, 0.0, -1.0},
     {&D_80030660[9], 0, 0.5, 0.0, -1.0},
     {crouched_pistol_firing_animation_group1, 0, 0.1, 56.0, 68.0}},
    {{rifle_firing_animation_group1, 0, 0.050000001, 35.0, 40.0},
     {&D_80030660[0], 0, 0.5, 0.0, -1.0},
     {&D_80030660[1], 0, 0.5, 0.0, -1.0},
     {&D_80030660[8], 0, 0.5, 0.0, -1.0},
     {&D_80030660[9], 0, 0.5, 0.0, -1.0},
     {crouched_rifle_firing_animation_groupA, 0, 0.1, 45.0, 55.0}},
    {{NULL, 0x8194, 0.25, 0.0, -1.0},
     {NULL, 0x8204, 0.5, 0.0, -1.0},
     {NULL, 0x777C, 0.5, 0.0, -1.0},
     {&D_80030660[8], 0, 0.5, 0.0, -1.0},
     {&D_80030660[9], 0, 0.5, 0.0, -1.0},
     {NULL, 0x6C18, 0.050000001, 28.0, 29.0}},
    {{doubles_firing_animation_group1, 0, 0.1, 32.0, 42.0},
     {&D_80030660[4], 0, 0.5, 0.0, -1.0},
     {&D_80030660[5], 0, 0.5, 0.0, -1.0},
     {&D_80030660[8], 0, 0.5, 0.0, -1.0},
     {&D_80030660[8], 0, 0.5, 0.0, -1.0},
     {crouched_doubles_firing_animation_group1, 0, 0.1, 37.0, 47.0}}
 };

//D:80036AB8
s32 D_80036AB8 = 2;
//D:80036ABC
s32 D_80036ABC = 0xFFFFFFFF;
//D:80036AC0
f32 D_80036AC0 = 1.0;
//D:80036AC4
f32 D_80036AC4 = 0.1;

// forward declarations

void sub_GAME_7F07DE9C(struct player *player);
s32 chrTickBeams(PropRecord *prop);
void sub_GAME_7F07DE64(struct player *player);
void bondviewUpdateWatchZoomIn(void);
void bondviewSetPauseWatchRelated(f32 arg0);
void bondviewSetPauseWatchRelatedAlt(f32 arg0);
void bondviewStepWatchAnimation(void);
f32 bondviewGetPauseAnimationPercent(void);
void bondviewCurrentPlayerUpdateSpeedVerta(f32 value);
void bondviewCurrentPlayerUpdateSpeedTheta(f32 value);
s16 bondviewGetCurrentPlayerViewportWidth(void);
s16 bondviewGetCurrentPlayerViewportHeight(void);
s16 bondviewGetCurrentPlayerViewportUly(void);
void currentPlayerTickChrFade(void);
void currentPlayerUpdateColourScreenProperties(void);
s16 getWidth320or440(void);
s16 getHeight330or240(void);
void sub_GAME_7F07B1A4(void);
bool currentPlayerIsFadeComplete(void);
s16 get_curplayer_viewport_ulx(void);
void bondviewFrozenMoveBond(s8, s8, u16, u16);
void bondviewMovePlayerUpdateViewport(s8 arg0, s8 arg1, u16 arg2);
void bondviewUpdateCurrentRoomPosition(s32 arg0);
void trigger_solo_watch_menu(s32 arg0);
void bondviewUpdatePlayerCollisionBounds(void);
void bondviewGetTankCollisionBounds(struct rect4f *, coord3d *, f32);
void bondviewIntroCameraTextTick(void);
void bondviewUpperTextWindowTimerTick(void);
void sub_GAME_7F07DEFC(void);
void MoveBond(s8 arg0, s8 arg1, u16 arg2, u16 arg3);
void bondviewProcessInput(s8 arg0, s8 arg1, u16 arg2, u16 arg3);
void bondviewPlayerTickDamageAndHealth(void);
void bondviewPlayerTickExplode(void);
void bondviewPlayerStopAudioForPause(void);
void bondviewWatchAnimationTick(void);
void bondviewUpdatePlayerCollisionPositionFields(void);
void bondviewTankModelRotationRelated(void);
s32 bondviewTankCollisionStatus(struct coord3d *collision_position, StandTile *arg1, f32 tank_orientation_angle, struct coord3d *arg3, struct coord3d *arg4);
s32 bondviewCallTankCollisionStatus(struct coord3d *arg0, struct StandTile *arg1, f32 arg2);
s32 sub_GAME_7F07CDD4(struct coord3d *arg0, f32 arg1, struct StandTile **arg2);
s32 bondviewTryMoveToStan(struct coord3d *arg0, struct StandTile **stan);
s32 bondviewTestLineUnobstructed(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, s32 cdtypes, struct coord3d *coord_p, struct coord3d *coord_dest);

s32 bondviewTryFractionMovePlayerCollision(struct coord3d *next_pos, struct coord3d *collision1_pt0, struct coord3d *collision1_pt1, struct coord3d *collision2_pt0, struct coord3d *collision2_pt1);
s32 bondviewTryEdgeMovePlayerCollision(struct coord3d *prior_next_pos, struct coord3d *collision_pt0, struct coord3d *collision_pt1);
s32 bondviewTryEndHopPlayerCollision(struct coord3d *prior_next_pos, struct coord3d *collision_pt0, struct coord3d *collision_pt1);
void bondviewApplyVertaTheta(void);

f32 bheadGetBreathingValue(void);
void bondviewMoveAnimationTick(f32 speed, f32 speedforwards, f32 speedsideways);
void bondviewCalcUpdatePlayerCollision(struct coord3d *offset, s32 allow_scoot);
f32 bondviewPauseAngleRelated(s32 arg0);
void bondviewStartPauseTransition(f32 duration);
void bondviewStartUnpauseTransition(f32 duration);
bool bondViewIsPauseTransitioning(void);
f32 sub_GAME_7F080228(f32 arg0);
void currentPlayerSetSwayTarget(s32 value);
void currentPlayerAdjustCrouchPos(s32 value);
void bondviewUpdateSpeedSideways(s32 arg0);
void bondviewUpdateSpeedForwards(s32 arg0);
void bondviewFrozenCameraTick(u16 buttons, u16 oldbuttons, struct coord3d *pos, struct coord3d *pos2, struct coord3d *offset, struct StandTile **stan, struct coord3d *arg6);
void sub_GAME_7F07B2A0(s32, f32, struct coord3d *, struct coord3d *);
s32 pickDeathCameraAngles(PropRecord *prop1, coord3d *pos, PropRecord *prop2, coord3d *collision_pos, StandTile *tile, f32 camera_dist);
Gfx* sub_GAME_7F08A5FC(Gfx* arg0);
Gfx *sub_GAME_7F08AAE8(Gfx *gdl);
Gfx *sub_GAME_7F088CD8(Gfx *gdl);
Gfx *sub_GAME_7F087E74(Gfx *gdl);
Gfx *bondviewRenderGaugeBars(Gfx *gdl);

// end forward declarations

void nullsub_75(void)
{
    return;
}

void currentPlayerSetScreenSize(f32 width, f32 height)
{
    g_CurrentPlayer->c_screenwidth = width;
    g_CurrentPlayer->c_screenheight = height;
    g_CurrentPlayer->c_halfwidth = width * 0.5f;
    g_CurrentPlayer->c_halfheight = height * 0.5f;
}

void currentPlayerSetScreenPosition(f32 left, f32 top)
{
    g_CurrentPlayer->c_screenleft = left;
    g_CurrentPlayer->c_screentop = top;
}

void currentPlayerSetPerspective(f32 near, f32 fovy, f32 aspect)
{
    g_CurrentPlayer->c_perspnear = near;
    g_CurrentPlayer->c_perspfovy = fovy;
    g_CurrentPlayer->c_perspaspect = aspect;
}

void currentPlayerSetCameraScale(void)
{
	f32 fVar4;
	f32 tmp;
	f32 fVar5;
	f32 fVar2;

	g_CurrentPlayer->c_scaley = sinf(mDegToHalfRad(g_CurrentPlayer->c_perspfovy)) / (cosf(mDegToHalfRad(g_CurrentPlayer->c_perspfovy)) * g_CurrentPlayer->c_halfheight);
	g_CurrentPlayer->c_scalex = (g_CurrentPlayer->c_scaley * g_CurrentPlayer->c_perspaspect * g_CurrentPlayer->c_halfheight) / g_CurrentPlayer->c_halfwidth;

	g_CurrentPlayer->c_recipscalex = 1.0f / g_CurrentPlayer->c_scalex;
	g_CurrentPlayer->c_recipscaley = 1.0f / g_CurrentPlayer->c_scaley;

    g_CurrentPlayer->c_scalelod = g_CurrentPlayer->c_scaley;
    g_CurrentPlayer->c_scalelod60 = sinf(DegToRad(30)) / (cosf(DegToRad(30)) * 120.0f);
	g_CurrentPlayer->c_lodscalez = g_CurrentPlayer->c_scalelod / g_CurrentPlayer->c_scalelod60;
	tmp = (g_CurrentPlayer->c_lodscalez * M_U16_MAX_VALUE_F);

	if (tmp > M_U32_MAX_VALUE_F) {
		g_CurrentPlayer->c_lodscalezu32 = -1;
	} else {
		g_CurrentPlayer->c_lodscalezu32 = tmp;
	}

	fVar2 = g_CurrentPlayer->c_halfheight * g_CurrentPlayer->c_scaley;
	fVar4 = 1.0f / sqrtf(fVar2 * fVar2 + 1.0f);
	g_CurrentPlayer->c_cameratopnorm.x = 0;
	g_CurrentPlayer->c_cameratopnorm.y = fVar4;
	g_CurrentPlayer->c_cameratopnorm.z = fVar2 * fVar4;

	fVar5 = -g_CurrentPlayer->c_halfwidth * g_CurrentPlayer->c_scalex;
	fVar4 = 1.0f / sqrtf(fVar5 * fVar5 + 1.0f);
	g_CurrentPlayer->c_cameraleftnorm.x = -fVar4;
	g_CurrentPlayer->c_cameraleftnorm.y = 0;
	g_CurrentPlayer->c_cameraleftnorm.z = -fVar5 * fVar4;
}

/**
 * Transforms a 2D screen coordinate to a 3D world coordinate
 *
 * 'out' looks to be a vector which probably has the length 'length'
 * It starts from the middle of the screen.
 *
 * Address 0x7F077EEC.
 */
void transformAndNormalizeByLength2Dto3D(coord2d *in, coord3d *out, f32 length) {
    f32 norm;
    f32 x;
    f32 y;
    f32 z;
    y = (g_CurrentPlayer->c_halfheight - (in->y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    x = ((in->x - g_CurrentPlayer->c_screenleft) - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    z = -1.0f;
    norm = length / sqrtf((x * x) + (y * y) + (z * z));
    out->x = (x * norm);
    out->y = (y * norm);
    out->z = (-1.0f * norm);
}

void scale3DCoordinates(coord3d *in, f32 value, coord3d *out) {
    out->y = ((in->y * value) * g_CurrentPlayer->c_scaley);
    out->x = ((in->x * value) * g_CurrentPlayer->c_scalex);
}

void transform3Dto2DCoords(coord3d *in, coord2d *out) {
    f32 inv_z = (1.0f / in->z);
    out->y = (in->y * inv_z * g_CurrentPlayer->c_recipscaley) + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
    out->x = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - (in->x * inv_z * g_CurrentPlayer->c_recipscalex);
}

void transform3Dto2DWithZScaling(coord3d *in, coord3d *out)
{
	f32 inv_z;

	if (in->z == 0.0f) {
		inv_z = -100000000000000000000.0f;
	} else {
		inv_z = 1.0f / in->z;
	}

	out->y = in->y * inv_z * g_CurrentPlayer->c_recipscaley + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
	out->x = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - in->x * inv_z * g_CurrentPlayer->c_recipscalex;
}

void divide3DCoordinates(coord3d *in, f32 divisor, coord3d *out)
{
	out->y = in->y * (1.0f / divisor) * g_CurrentPlayer->c_recipscaley;
	out->x = in->x * (1.0f / divisor) * g_CurrentPlayer->c_recipscalex;
}

void transform3DCoordinatesWithAngle(coord3d *in, coord3d *out, f32 value1, f32 angle, f32 value2) {
    f32 var1;
    f32 x;
    f32 y;
    f32 z;
    f32 var2 = sinf(mDegToHalfRad(angle)) / (cosf(mDegToHalfRad(angle)) * g_CurrentPlayer->c_halfheight);
    f32 var3 = (var2 * value2 * g_CurrentPlayer->c_halfheight) / g_CurrentPlayer->c_halfwidth;
    y = (g_CurrentPlayer->c_halfheight - (in->y - g_CurrentPlayer->c_screentop)) * var2;
    x = ((in->x - g_CurrentPlayer->c_screenleft) - g_CurrentPlayer->c_halfwidth) * var3;
    z = -1.0f;
    var1 = value1 / sqrtf((x * x) + (y * y) + (z * z));
    out->x = (x * var1);
    out->y = (y * var1);
    out->z = (-1.0f * var1);
}

/**
 * Unreferenced.
 *
 * Address 0x7F078258.
 */
void transform3DCoordinatesWithAngleAndValue(coord3d *in, coord3d *out, f32 angle, f32 value)
{
    f32 var1 = (cosf(mDegToHalfRad(angle)) * g_CurrentPlayer->c_halfheight) / (sinf(mDegToHalfRad(angle)) * in->f[2]);
    f32 var2 = (var1 * g_CurrentPlayer->c_halfwidth) / (value * g_CurrentPlayer->c_halfheight);

    out->f[1] = (in->f[1] * var1) + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
    out->f[0] = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - (in->f[0] * var2);
}

void currentPlayerSetMatrix10C4(Mtx *matrix) {
    g_CurrentPlayer->field_10C4 = matrix;
}

Mtx *currentPlayerGetMatrix10C4(void) {
    return g_CurrentPlayer->field_10C4;
}

void currentPlayerSetMatrix10C8(Mtx *matrix) {
    g_CurrentPlayer->field_10C8 = matrix;
}

Mtx *currentPlayerGetMatrix10C8(void) {
    return g_CurrentPlayer->field_10C8;
}

void currentPlayerSetProjectionMatrix(Mtx *matrix) {
    g_CurrentPlayer->projmatrix = matrix;
}

Mtx *currentPlayerGetProjectionMatrix(void) {
    return g_CurrentPlayer->projmatrix;
}

void set_BONDdata_field_10E0(s32 arg0) {
    g_CurrentPlayer->field_10E0 = arg0;
}

s32 get_BONDdata_field_10E0(void) {
    return g_CurrentPlayer->field_10E0;
}

void *currentPlayerSetMatrix10CC(Mtxf *matrix) {
    g_CurrentPlayer->field_10E8 = g_CurrentPlayer->field_10CC;
    g_CurrentPlayer->field_10CC = matrix;
}

Mtxf *camGetWorldToScreenMtxf(void) {
    return g_CurrentPlayer->field_10CC;
}

void currentPlayerSetProjectionMatrixF(Mtxf *matrix) {
    g_CurrentPlayer->projmatrixf = matrix;
}

Mtxf *currentPlayerGetProjectionMatrixF(void) {
    return g_CurrentPlayer->projmatrixf;
}

Mtxf *currentPlayerGetMatrix10E8(void) {
    return g_CurrentPlayer->field_10E8;
}

void sub_GAME_7F078404(s32 arg0) {
    g_CurrentPlayer->field_10D0 = arg0;
}

s32 sub_GAME_7F078414(void) {
    return g_CurrentPlayer->field_10D0;
}

void currentPlayerSetViewToWorldMtxf(Mtxf *matrix) {
    g_CurrentPlayer->field_10EC = g_CurrentPlayer->viewtoworldmtxf;
    g_CurrentPlayer->viewtoworldmtxf = matrix;
}

Mtxf *currentPlayerGetViewToWorldMtxf(void) {
    return g_CurrentPlayer->viewtoworldmtxf;
}

Mtxf *currentPlayerGetMatrix10EC(void) {
    return g_CurrentPlayer->field_10EC;
}

void sub_GAME_7F078464(s32 arg0) {
    g_CurrentPlayer->field_10E4 = arg0;
}

s32 sub_GAME_7F078474(void)
{
    return g_CurrentPlayer->field_10E4;
}

f32 getPlayer_c_lodscalez(void)
{
    return g_CurrentPlayer->c_lodscalez;
}

u32 getPlayer_c_lodscalezu32(void)
{
    return g_CurrentPlayer->c_lodscalezu32;
}

f32 getPlayer_c_screenwidth(void)
{
    return g_CurrentPlayer->c_screenwidth;
}

f32 getPlayer_c_screenheight(void)
{
    return g_CurrentPlayer->c_screenheight;
}

f32 getPlayer_c_screenleft(void)
{
    return g_CurrentPlayer->c_screenleft;
}

f32 getPlayer_c_screentop(void)
{
    return g_CurrentPlayer->c_screentop;
}

f32 getPlayer_c_perspfovy(void)
{
    return g_CurrentPlayer->c_perspfovy;
}

f32 getPlayer_c_perspaspect(void)
{
    return g_CurrentPlayer->c_perspaspect;
}

void getPlayer_c_cameratopnorm(coord3d *out)
{
    out->x = (g_CurrentPlayer->c_cameratopnorm).x;
    out->y = (g_CurrentPlayer->c_cameratopnorm).y;
    out->z = (g_CurrentPlayer->c_cameratopnorm).z;
}

void getPlayer_c_cameratopnorm_inverted_y(coord3d *out)
{
    out->x = (g_CurrentPlayer->c_cameratopnorm).x;
    out->y = -(g_CurrentPlayer->c_cameratopnorm).y;
    out->z = (g_CurrentPlayer->c_cameratopnorm).z;
}

void getPlayer_c_cameraleftnorm(coord3d *out)
{
    out->x = (g_CurrentPlayer->c_cameraleftnorm).x;
    out->y = (g_CurrentPlayer->c_cameraleftnorm).y;
    out->z = (g_CurrentPlayer->c_cameraleftnorm).z;
}

void getPlayer_c_cameraleftnorm_inverted_x(coord3d *out)
{
    out->x = -(g_CurrentPlayer->c_cameraleftnorm).x;
    out->y = (g_CurrentPlayer->c_cameraleftnorm).y;
    out->z = (g_CurrentPlayer->c_cameraleftnorm).z;
}

f32 getPlayer_c_perspnear(void)
{
    return g_CurrentPlayer->c_perspnear;
}


/**
 * Address: 7F0785DC
 *
 * Update the world space frustum planes used for object visibility tests.
 */
void bondviewUpdateFrustumPlanes()
{
    f32 h_div;
    f32 h2;
    f32 h;
    f32 nh_div;
    f32 nh2_div;
    f32 h2_div;

    h = g_CurrentPlayer->c_halfheight * g_CurrentPlayer->c_scaley;
    h_div = 1.0f / sqrtf((h * h) + 1.0f);
    h *= h_div;
    nh_div = -h_div;

    g_CamFrustumTopNormal.x = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][0]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumTopNormal.y = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][1]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumTopNormal.z = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][2]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumTopOffset = (g_CamFrustumTopNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                          + (g_CamFrustumTopNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                          + (g_CamFrustumTopNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumBottomNormal.x = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][0]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumBottomNormal.y = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][1]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumBottomNormal.z = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][2]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumBottomOffset = (g_CamFrustumBottomNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                             + (g_CamFrustumBottomNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                             + (g_CamFrustumBottomNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    h2 = (-g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    h2_div = 1.0f / sqrtf((h2 * h2) + 1.0f);
    h2 *= h2_div;
    nh2_div = -h2_div;

    g_CamFrustumLeftNormal.x = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][0]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumLeftNormal.y = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][1]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumLeftNormal.z = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][2]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumLeftOffset = (g_CamFrustumLeftNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                           + (g_CamFrustumLeftNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                           + (g_CamFrustumLeftNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumRightNormal.x = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][0]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumRightNormal.y = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][1]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumRightNormal.z = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][2]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumRightOffset = (g_CamFrustumRightNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                            + (g_CamFrustumRightNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                            + (g_CamFrustumRightNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumNearOffset = (g_CurrentPlayer->viewtoworldmtxf->m[2][0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                           + (g_CurrentPlayer->viewtoworldmtxf->m[2][1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                           + (g_CurrentPlayer->viewtoworldmtxf->m[2][2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);
}


/**
 * Address: 7F078950
 *
 * Unreferenced.
 */
void bondviewGetFrustumTopPlane(coord3d *normal, f32 *offset)
{
    normal->x = g_CamFrustumTopNormal.x;
    normal->y = g_CamFrustumTopNormal.y;
    normal->z = g_CamFrustumTopNormal.z;
    *offset = g_CamFrustumTopOffset;
}


/**
 * Address: 7F078980
 *
 * Unreferenced.
 */
void bondviewGetFrustumBottomPlane(coord3d *normal, f32 *offset)
{
    normal->x = g_CamFrustumBottomNormal.x;
    normal->y = g_CamFrustumBottomNormal.y;
    normal->z = g_CamFrustumBottomNormal.z;
    *offset = g_CamFrustumBottomOffset;
}


/**
 * Address: 7F0789B0
 *
 * Unreferenced.
 */
void bondviewGetFrustumLeftPlane(coord3d *normal, f32 *offset)
{
    normal->x = g_CamFrustumLeftNormal.x;
    normal->y = g_CamFrustumLeftNormal.y;
    normal->z = g_CamFrustumLeftNormal.z;
    *offset = g_CamFrustumLeftOffset;
}


/**
 * Address: 7F0789E0
 *
 * Unreferenced.
 */
void bondviewGetFrustumRightPlane(coord3d *normal, f32 *offset)
{
    normal->x = g_CamFrustumRightNormal.x;
    normal->y = g_CamFrustumRightNormal.y;
    normal->z = g_CamFrustumRightNormal.z;
    *offset = g_CamFrustumRightOffset;
}


/**
 * Address: 7F078A10
 *
 * Unreferenced.
 */
void bondviewGetFrustumNearPlane(coord3d *normal, f32 *offset)
{
    normal->x = g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    normal->y = g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    normal->z = g_CurrentPlayer->viewtoworldmtxf->m[2][2];
    *offset = g_CamFrustumNearOffset;
}


/**
 * Check if the 3D coordinate is within the screen
 *
 * Takes dot product of some position and compares each to an associated scalar value.
 * Returns 0 if the dot product exceeds the scalar amount, 1 otherwise.
 *
 * @param pos: Applies dot product of this position against g_CurrentPlayer->viewtoworldmtxf
 * and four coords starting at g_CamFrustumLeftNormal.
 *
 * @param margin: Value added to g_CamFrustumNearOffset to compare g_CurrentPlayer->viewtoworldmtxf,
 * and the four values starting at g_CamFrustumLeftOffset.
 *
 * Address 0x7F078A58.
 */
s32 camIsPosInScreen(coord3d *pos, f32 margin)
{
    if (g_CamFrustumNearOffset + margin < (g_CurrentPlayer->viewtoworldmtxf->m[2][0] * pos->f[0]) + (g_CurrentPlayer->viewtoworldmtxf->m[2][1] * pos->f[1]) + (g_CurrentPlayer->viewtoworldmtxf->m[2][2] * pos->f[2]))
    {
        return 0;
    }

    if (g_CamFrustumLeftOffset + margin < (g_CamFrustumLeftNormal.f[0] * pos->f[0]) + (g_CamFrustumLeftNormal.f[1] * pos->f[1]) + (g_CamFrustumLeftNormal.f[2] * pos->f[2]))
    {
        return 0;
    }

    if (g_CamFrustumRightOffset + margin < (g_CamFrustumRightNormal.f[0] * pos->f[0]) + (g_CamFrustumRightNormal.f[1] * pos->f[1]) + (g_CamFrustumRightNormal.f[2] * pos->f[2]))
    {
        return 0;
    }

    if (g_CamFrustumTopOffset + margin < (g_CamFrustumTopNormal.f[0] * pos->f[0]) + (g_CamFrustumTopNormal.f[1] * pos->f[1]) + (g_CamFrustumTopNormal.f[2] * pos->f[2]))
    {
        return 0;
    }

    if (g_CamFrustumBottomOffset + margin < (g_CamFrustumBottomNormal.f[0] * pos->f[0]) + (g_CamFrustumBottomNormal.f[1] * pos->f[1]) + (g_CamFrustumBottomNormal.f[2] * pos->f[2]))
    {
        return 0;
    }

    return 1;
}


bool camIsPosInScreenBox(coord3d *pos, f32 margin, bbox2d *box)
{
    coord3d sp74;
    f32 sp70;
    coord3d sp64;
    f32 sp60;
    coord3d sp54;
    f32 sp50;
    coord3d sp44;
    f32 sp40;
    f32 sp3c;
    f32 sp38;
    f32 sp34;
    f32 sp30;
    f32 sp2c;
    f32 sp28;
    f32 sp24;
    f32 sp20;
    f32 sp1c;
    f32 sp18;

    if (g_CamFrustumNearOffset + margin < g_CurrentPlayer->viewtoworldmtxf->m[2][0] * pos->f[0] + g_CurrentPlayer->viewtoworldmtxf->m[2][1] * pos->f[1] + g_CurrentPlayer->viewtoworldmtxf->m[2][2] * pos->f[2])
    {
        return FALSE;
    }

    sp38 = (box->min.x - g_CurrentPlayer->c_screenleft - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;

    sp3c = 1.0f / sqrtf(sp38 * sp38 + 1.0f);
    sp38 *= sp3c;
    sp24 = -sp3c;

    sp54.f[0] = sp24 * g_CurrentPlayer->viewtoworldmtxf->m[0][0] - sp38 * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    sp54.f[1] = sp24 * g_CurrentPlayer->viewtoworldmtxf->m[0][1] - sp38 * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    sp54.f[2] = sp24 * g_CurrentPlayer->viewtoworldmtxf->m[0][2] - sp38 * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    sp50 = sp54.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + sp54.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + sp54.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (sp50 + margin < sp54.f[0] * pos->f[0] + sp54.f[1] * pos->f[1] + sp54.f[2] * pos->f[2])
    {
        return FALSE;
    }

    sp38 = -(box->max.x - g_CurrentPlayer->c_screenleft - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    sp30 = 1.0f / sqrtf(sp38 * sp38 + 1.0f);
    sp38 *= sp30;
    sp20 = -sp30;

    sp44.f[0] = -sp20 * g_CurrentPlayer->viewtoworldmtxf->m[0][0] - sp38 * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    sp44.f[1] = -sp20 * g_CurrentPlayer->viewtoworldmtxf->m[0][1] - sp38 * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    sp44.f[2] = -sp20 * g_CurrentPlayer->viewtoworldmtxf->m[0][2] - sp38 * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    sp40 = sp44.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + sp44.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + sp44.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (sp40 + margin < sp44.f[0] * pos->f[0] + sp44.f[1] * pos->f[1] + sp44.f[2] * pos->f[2])
    {
        return FALSE;
    }

    sp34 = (g_CurrentPlayer->c_halfheight - (box->min.y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    sp2c = 1.0f / sqrtf(sp34 * sp34 + 1.0f);
    sp34 *= sp2c;
    sp1c = -sp2c;

    sp74.f[0] = -sp1c * g_CurrentPlayer->viewtoworldmtxf->m[1][0] + sp34 * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    sp74.f[1] = -sp1c * g_CurrentPlayer->viewtoworldmtxf->m[1][1] + sp34 * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    sp74.f[2] = -sp1c * g_CurrentPlayer->viewtoworldmtxf->m[1][2] + sp34 * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    sp70 = sp74.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + sp74.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + sp74.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (sp70 + margin < sp74.f[0] * pos->f[0] + sp74.f[1] * pos->f[1] + sp74.f[2] * pos->f[2])
    {
        return FALSE;
    }

    sp34 = -(g_CurrentPlayer->c_halfheight - (box->max.y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    sp28 = 1.0f / sqrtf(sp34 * sp34 + 1.0f);
    sp34 *= sp28;
    sp18 = -sp28;

    sp64.f[0] = sp18 * g_CurrentPlayer->viewtoworldmtxf->m[1][0] + sp34 * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    sp64.f[1] = sp18 * g_CurrentPlayer->viewtoworldmtxf->m[1][1] + sp34 * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    sp64.f[2] = sp18 * g_CurrentPlayer->viewtoworldmtxf->m[1][2] + sp34 * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    sp60 = sp64.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + sp64.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + sp64.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    if (sp60 + margin < sp64.f[0] * pos->f[0] + sp64.f[1] * pos->f[1] + sp64.f[2] * pos->f[2])
    {
        return FALSE;
    }

    return TRUE;
}


//split here makes sense to have the pd split make sense
s32 bondviewGetRandomSpawnPadIndex(void)
{
    PadRecord *pad;
    PropRecord *player_prop;
    s32 player_count;
    f32 diff_x;
    s32 pad_index;
    s32 player_num;
    s32 player_index;
    s32 enemy_nearby;
    s32 attempt_num;
    f32 dist;
    f32 diff_z;

    // set up initial values
    player_num = get_cur_playernum();
    player_count = getPlayerCount();
    enemy_nearby = TRUE;
#ifdef DEBUG
    osSyncPrintf("choosing a start pad for player %d\n", player_num);
#endif

    // loop pads until no enemy is within 1000 units
    for (attempt_num = 0; enemy_nearby && (attempt_num < startpadcount);)
    {
        attempt_num++;
        enemy_nearby = FALSE;
        g_CurrentPlayer->field_29E0++;
        pad_index = ( g_CurrentPlayer->field_29E0) % (startpadcount);
#ifdef DEBUG
        osSyncPrintf("testing pad %d\n", pad_index);
#endif

        for (player_index = 0; player_index < player_count; player_index++)
        {
            // don't consider yourself as an enemy
            if (player_index == player_num) { continue; }

            // make sure the player prop is valid
            player_prop = g_playerPointers[player_index]->prop;
            if (player_prop == 0)
            {
#ifdef DEBUG
                osSyncPrintf("Player %d has no prop\n", player_index);
#endif
                continue;
            }

            // find distance between enemy and this pad
            pad = g_Startpad[pad_index];
            diff_x = player_prop->pos.x - pad->pos.x;
            diff_z = player_prop->pos.z - pad->pos.z;
            dist = sqrtf((diff_x * diff_x) + (diff_z * diff_z));
#ifdef DEBUG
            osSyncPrintf("Distance from player %d (%f, %f)->(%f, %f)= %f\n", player_index, pad->pos.x, pad->pos.z, player_prop->pos.x, player_prop->pos.z, dist);
#endif

            // if pad is within 1000, don't pick it
            if (dist < 1000)
            {
#ifdef DEBUG
                osSyncPrintf("Too close to player %d (closer than 10m)\n", player_index);
#endif
                enemy_nearby = TRUE;
            }
        }
    }

    do {} while (0); // leftover debug code? - Probably catch Player has no Prop

    // loop pads until no enemy is within 100 units
    for (; enemy_nearby && (attempt_num < startpadcount);)
    {
        attempt_num++;
        enemy_nearby = FALSE;
        g_CurrentPlayer->field_29E0++;
        pad_index = ((s32) g_CurrentPlayer->field_29E0) % ((s32) startpadcount);
#ifdef DEBUG
        osSyncPrintf("testing pad %d (second try)\n", pad_index);
#endif

        for (player_index = 0; player_index < player_count; player_index++)
        {
            // don't consider yourself as an enemy
            if (player_index == player_num) { continue; }

            // make sure the player prop is valid
            player_prop = g_playerPointers[player_index]->prop;
            if (player_prop == 0)
            {
#ifdef DEBUG
                osSyncPrintf("Player %d has no prop\n", player_index);
#endif
                continue;
            }

            // find distance between enemy and this pad
            pad = g_Startpad[pad_index];
            diff_x = player_prop->pos.x - pad->pos.x;
            diff_z = player_prop->pos.z - pad->pos.z;
            dist = sqrtf((diff_x * diff_x) + (diff_z * diff_z));

#ifdef DEBUG
            osSyncPrintf("Distance from player %d (%f, %f)->(%f, %f)= %f\n", player_index, pad->pos.x, pad->pos.z, player_prop->pos.x, player_prop->pos.z, dist);
#endif
            // if pad is within 100, don't pick it
            if (dist < 100.f)
            {
#ifdef DEBUG
                osSyncPrintf("Too close to player %d (closer than 1m)\n", player_index);
#endif
                enemy_nearby = TRUE;
            }
        }
    }

    // if we searched through all pads and failed to find a safe one, just pick one at random
    if (enemy_nearby)
    {
#ifdef DEBUG
        osSyncPrintf("**** No decent start pad found for player %d - picking a random one ****\n", player_index);
#endif
        pad_index = (randomGetNext() % (startpadcount));
    }

    return pad_index;
}


//file split per pd
void init_player_BONDdata(void)
{
    if (getPlayerCount() >= 2)
    {
        g_CurrentPlayer->controldef = get_player_control_style(get_cur_playernum());
        cur_player_set_control_type(get_player_control_style(get_cur_playernum()));
    }
    g_CurrentPlayer->current_model_pos.f[0] = 0.0f;
    g_CurrentPlayer->current_model_pos.f[1] = 0.0f;
    g_CurrentPlayer->current_model_pos.f[2] = 0.0f;
    g_CurrentPlayer->previous_model_pos.f[0] = 0.0f;
    g_CurrentPlayer->previous_model_pos.f[1] = 0.0f;
    g_CurrentPlayer->previous_model_pos.f[2] = 0.0f;
    g_CurrentPlayer->current_room_pos.f[0] = 0.0f;
    g_CurrentPlayer->current_room_pos.f[1] = 0.0f;
    g_CurrentPlayer->current_room_pos.f[2] = 0.0f;
    g_CurrentPlayer->cameramode = 0;
    g_CurrentPlayer->pos.f[0] = 0.0f;
    g_CurrentPlayer->pos.f[1] = 0.0f;
    g_CurrentPlayer->pos.f[2] = 0.0f;
    g_CurrentPlayer->pos2.f[0] = 0.0f;
    g_CurrentPlayer->pos2.f[1] = 0.0f;
    g_CurrentPlayer->pos2.f[2] = 1.0f;
    g_CurrentPlayer->offset.f[0] = 0.0f;
    g_CurrentPlayer->offset.f[1] = 1.0f;
    g_CurrentPlayer->offset.f[2] = 0.0f;
    g_CurrentPlayer->pos3.f[0] = 0.0f;
    g_CurrentPlayer->pos3.f[1] = 0.0f;
    g_CurrentPlayer->pos3.f[2] = 0.0f;
    g_CurrentPlayer->room_pointer = 0;
    g_CurrentPlayer->field_3C4 = 0.0f;
    g_CurrentPlayer->field_3C8 = 0.0f;
    g_CurrentPlayer->field_3CC = 1.0f;
    g_CurrentPlayer->field_84 = 0.0f;
    g_CurrentPlayer->field_88 = 0.0f;
    g_CurrentPlayer->field_8C = 0;
    g_CurrentPlayer->vertical_bounce_adjust = 0.0f;
    g_CurrentPlayer->field_94 = 0;
    g_CurrentPlayer->field_98 = 0.0f;
    g_CurrentPlayer->swaytarget = 0.0f;
    g_CurrentPlayer->swayoffset0 = 0.0f;
    g_CurrentPlayer->swayoffset2 = 0.0f;
    g_CurrentPlayer->crouchpos = CROUCH_STAND;
    g_CurrentPlayer->autocrouchpos = CROUCH_STAND;
    g_CurrentPlayer->ducking_height_offset = 0.0f;
    g_CurrentPlayer->field_A4 = 0.0f;
    g_CurrentPlayer->field_AC = 1;
    g_CurrentPlayer->field_D0 = 0;
    g_CurrentPlayer->bonddead = 0;
    g_CurrentPlayer->bondhealth = 1.0f;
    g_CurrentPlayer->bondarmour = 0.0f;
    g_CurrentPlayer->oldhealth = 1.0f;
    g_CurrentPlayer->oldarmour = 0.0f;
    g_CurrentPlayer->apparenthealth = 1.0f;
    g_CurrentPlayer->apparentarmour = 0.0f;
    g_CurrentPlayer->damageshowtime = -1;
    g_CurrentPlayer->healthshowtime = -1;
    g_CurrentPlayer->watch_pause_time = 0;
    g_CurrentPlayer->timer_1C4 = 0;
    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x0;
    g_CurrentPlayer->outside_watch_menu = TRUE;
    g_CurrentPlayer->open_close_solo_watch_menu = FALSE;
    g_CurrentPlayer->field_1A0 = 0;
    g_CurrentPlayer->bondbreathing = 0.0f;
    g_CurrentPlayer->speedtheta = 0.0f;
    g_CurrentPlayer->vv_costheta = 1.0f;
    g_CurrentPlayer->vv_sintheta = 0.0f;
    g_CurrentPlayer->vv_verta = -4.0f;
    g_CurrentPlayer->vv_verta360 = (f32) g_CurrentPlayer->vv_verta;
    if (g_CurrentPlayer->vv_verta360 < 0.0f)
    {
        g_CurrentPlayer->vv_verta360 = (f32) (g_CurrentPlayer->vv_verta360 + 360.0f);
    }
    g_CurrentPlayer->speedverta = 0.0f;
    g_CurrentPlayer->vv_cosverta = 1.0f;
    g_CurrentPlayer->vv_sinverta = 0.0f;
    g_CurrentPlayer->speedsideways = 0.0f;
    g_CurrentPlayer->speedstrafe = 0.0f;
    g_CurrentPlayer->speedforwards = 0.0f;
    g_CurrentPlayer->speedgo = 0.0f;
    g_CurrentPlayer->speedboost = 1.0f;
    g_CurrentPlayer->speedmaxtime60 = 0;
    g_CurrentPlayer->bondshotspeed.x = 0.0f;
    g_CurrentPlayer->bondshotspeed.y = 0.0f;
    g_CurrentPlayer->bondshotspeed.z = 0.0f;
    g_CurrentPlayer->docentreupdown = FALSE;
    g_CurrentPlayer->lastupdown60 = 0;
    g_CurrentPlayer->prevupdown = FALSE;
    g_CurrentPlayer->movecentrerelease = FALSE;
    g_CurrentPlayer->lookaheadcentreenabled = TRUE;
    g_CurrentPlayer->automovecentreenabled = TRUE;
    g_CurrentPlayer->fastmovecentreenabled = FALSE;
    g_CurrentPlayer->automovecentre = TRUE;
    g_CurrentPlayer->insightaimmode = FALSE;
    g_CurrentPlayer->autoyaimenabled = TRUE;
    g_CurrentPlayer->autoaimy = 0.0f;
    g_CurrentPlayer->autoaim_target_y = NULL;
    g_CurrentPlayer->autoyaimtime60 = -1;
    g_CurrentPlayer->autoxaimenabled = TRUE;
    g_CurrentPlayer->autoaimx = 0.0f;
    g_CurrentPlayer->autoaim_target_x = NULL;
    g_CurrentPlayer->autoxaimtime60 = -1;
    g_CurrentPlayer->colourscreenred = 0xff;
    g_CurrentPlayer->colourscreengreen = 0xff;
    g_CurrentPlayer->colourscreenblue = 0xff;
    g_CurrentPlayer->colourscreenfrac = 0.0f;
    g_CurrentPlayer->colourfadetime60 = -1.0f;
    g_CurrentPlayer->colourfadetimemax60 = -1.0f;
    g_CurrentPlayer->colourfaderedold = 0xff;
    g_CurrentPlayer->colourfaderednew = 0xff;
    g_CurrentPlayer->colourfadegreenold = 0xff;
    g_CurrentPlayer->colourfadegreennew = 0xff;
    g_CurrentPlayer->colourfadeblueold = 0xff;
    g_CurrentPlayer->colourfadebluenew = 0xff;
    g_CurrentPlayer->colourfadefracold = 0.0f;
    g_CurrentPlayer->colourfadefracnew = 0.0f;
    g_CurrentPlayer->bondfadetime60 = -1.0f;
    g_CurrentPlayer->bondfadetimemax60 = -1.0f;
    g_CurrentPlayer->bondfadefracold = 0.0f;
    g_CurrentPlayer->bondfadefracnew = 0.0f;
    g_CurrentPlayer->field_42c = 2;
    g_CurrentPlayer->controldef = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->pause_starting_angle = 0.0f;
    g_CurrentPlayer->pause_saved_verta = 0.0f;
    g_CurrentPlayer->pause_target_verta = 0.0f;
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_transition_duration = 0.0f;
    g_CurrentPlayer->pause_state = 0;
    g_CurrentPlayer->step_in_view_watch_animation = 0;
    g_CurrentPlayer->pause_animation_counter = 0.0f;
    g_CurrentPlayer->pausing_flag = FALSE;
    g_CurrentPlayer->buttons_pressed = (u16)0;
    g_CurrentPlayer->prev_buttons_pressed = (u16)0;
    g_CurrentPlayer->field_29C0 = 15.0f;
    g_CurrentPlayer->field_2A04 = -1;
    g_CurrentPlayer->field_2A08 = 0.0f;
    g_CurrentPlayer->field_2A0C = 0.0f;
    g_CurrentPlayer->field_2A6C = 0;
    g_CurrentPlayer->field_2A70 = 0;
}

//split per pd

void bondviewPlayerSpawnRelated(void)
{
    g_CurrentPlayer->field_29BC = ((g_playerPerm->player_perspective_height * 185.0f * (s32)1) - 10.0f);

    g_CurrentPlayer->kills_this_life = 0;
    g_CurrentPlayer->lifestarttime60 = getMissiontimer();
    g_CurrentPlayer->healthdisplaytime = 0;

    bondinvAddInvItem(ITEM_FIST);

    if (getPlayerCount() >= 2)
    {
        currentPlayerEquipWeaponWrapper(GUNLEFT, starting_weapon[GUNLEFT]);
        currentPlayerEquipWeaponWrapper(GUNRIGHT, starting_weapon[GUNRIGHT]);

        if (g_CurrentPlayer->ptr_char_objectinstance == NULL)
        {
            solo_char_load();
        }
    }
}


void currentPlayerSetSwayTarget(s32 value) {
    g_CurrentPlayer->swaytarget = (value * 75.0f);
}

void currentPlayerAdjustCrouchPos(s32 value) {
    g_CurrentPlayer->crouchpos = g_CurrentPlayer->crouchpos + value;

    if (g_CurrentPlayer->crouchpos < CROUCH_SQUAT) {
        g_CurrentPlayer->crouchpos = CROUCH_SQUAT;
    } else if (g_CurrentPlayer->crouchpos > CROUCH_STAND) {
        g_CurrentPlayer->crouchpos = CROUCH_STAND;
    }
}

s32 currentPlayerGetCrouchPos(void) {
    return ((g_CurrentPlayer->crouchpos < g_CurrentPlayer->autocrouchpos) ? g_CurrentPlayer->crouchpos : g_CurrentPlayer->autocrouchpos);
}

s32 playerGetCrouchPos(s32 playernum) {
	return (g_playerPointers[playernum]->crouchpos < g_playerPointers[playernum]->autocrouchpos)
		? g_playerPointers[playernum]->crouchpos
		: g_playerPointers[playernum]->autocrouchpos;
}


void currentPlayerSetCameraMode(s32 mode)
{
    g_CurrentPlayer->cameramode = mode;
}


/**
 * Compares current player position to parameters. If different, sets current
 * player position values to parameter values.
 * Also updates related room pointer.
 *
 * Address 0x7F079A60.
 */
void bondviewSetCurrentPlayerPosition(coord3d *pos, coord3d *pos2, coord3d *offset, StandTile *tile, coord3d *stan_walk_start)
{
    StandTile *sp34;
    StandTile *sp30;

    if (
        (pos->f[0] != g_CurrentPlayer->pos.f[0])
        || (pos->f[1] != g_CurrentPlayer->pos.f[1])
        || (pos->f[2] != g_CurrentPlayer->pos.f[2])
        || (pos2->f[0] != g_CurrentPlayer->pos2.f[0])
        || (pos2->f[1] != g_CurrentPlayer->pos2.f[1])
        || (pos2->f[2] != g_CurrentPlayer->pos2.f[2])
        || (offset->f[0] != g_CurrentPlayer->offset.f[0])
        || (offset->f[1] != g_CurrentPlayer->offset.f[1])
        || (offset->f[2] != g_CurrentPlayer->offset.f[2])
        || (g_CurrentPlayer->room_pointer == NULL))
    {
        sp34 = tile;
        if (walkTilesBetweenPoints_NoCallback((StandTile **) &sp34, stan_walk_start->f[0], stan_walk_start->f[2], pos->f[0], pos->f[2]))
        {
            // @bug ...? This is either a bug or removed code, this function has no side effects.
            // Return value should used to check if point is safe for stan.
            stanTestPointWithinTileBoundsMaybe(sp34, pos->f[0], pos->f[2]);
            g_CurrentPlayer->room_pointer = sp34;
        }
        else
        {
            if (g_CurrentPlayer->room_pointer != NULL)
            {
                sp30 = g_CurrentPlayer->room_pointer;
                if (walkTilesBetweenPoints_NoCallback((StandTile **) &sp30, g_CurrentPlayer->pos.f[0], g_CurrentPlayer->pos.f[2], pos->f[0], pos->f[2]))
                {
                    g_CurrentPlayer->room_pointer = sp30;
                }
                else
                {
                    g_CurrentPlayer->room_pointer = sp34;
                }
            }
            else
            {
                g_CurrentPlayer->room_pointer = sp34;
            }
        }

        g_CurrentPlayer->pos.f[0] = pos->f[0];
        g_CurrentPlayer->pos.f[1] = pos->f[1];
        g_CurrentPlayer->pos.f[2] = pos->f[2];
        g_CurrentPlayer->pos2.f[0] = pos2->f[0];
        g_CurrentPlayer->pos2.f[1] = pos2->f[1];
        g_CurrentPlayer->pos2.f[2] = pos2->f[2];
        g_CurrentPlayer->offset.f[0] = offset->f[0];
        g_CurrentPlayer->offset.f[1] = offset->f[1];
        g_CurrentPlayer->offset.f[2] = offset->f[2];
        g_CurrentPlayer->pos3.f[0] = g_CurrentPlayer->pos.f[0];
        g_CurrentPlayer->pos3.f[2] = g_CurrentPlayer->pos.f[2];
        g_CurrentPlayer->pos3.f[1] = stanGetPositionYValue(g_CurrentPlayer->room_pointer, g_CurrentPlayer->pos.f[0], g_CurrentPlayer->pos.f[2]);
    }
}





#ifdef NONMATCHING
#define ALIGN64_V3(val) (((val) | 0x3f) ^ 0x3f)
void solo_char_load(void)
{
    f32 hRot;
    ModelFileHeader *pBody;
    ModelFileHeader *pHead;
    ModelFileHeader *p_leftHeader;
    ModelFileHeader *bodyBuffer;
    ModelFileHeader *headBuffer;
    s32 totalsize; //canonically sizer
    s32 bodyBufSize;
    s32 headBufSize;
    WeaponObjRecord *p_rightHeader;
    //? sp54;
    s32 rhandweapID;
    s32 body;
    s32 head;
    Model *model;
    s32 numRecords;
    ChrModelFileRecord *p_modelEntry;                       /* compiler-managed */
    ChrModelFileRecord *p_bodyEntry;
    ChrModelFileRecord *p_headEntry;
    ChrRecord *curChr;
    ModelFileHeader *p_headHeader;
    ModelFileHeader *p_headEntryHeader;
    ModelFileHeader *p_rhandItemHeader;
    ModelFileHeader *p_bodyEntryHeader;
    ModelFileHeader *p_bodyHeader;
    ModelFileHeader *p_lhandItemHeader;
    s32 rhandPropID;
    s32 bodyalignedSizeRemainPlus0x5F;
    s32 bufferSizeRemain;
    s32 folderBond;
    //s32 numRecords;
    s32 cuffId;
    hRot = bondviewGetPlayerYawRadians();
    curChr = g_CurrentPlayer->prop->chr;
    if (curChr == NULL)
    {
        bodyBuffer = getPlayerWeaponBufferForHand(GUNRIGHT);
        headBuffer = getPlayerWeaponBufferForHand(GUNLEFT);
        totalsize = 0;
        bodyBufSize = getSizeBufferWeaponInHand(GUNRIGHT);
        headBufSize = getSizeBufferWeaponInHand(GUNLEFT);
        //M2C_MEMCPY_ALIGNED(&sp54, &dummy_08_pp7_obj, 0x84);
        //*(&sp54 + 0x84) = *(&dummy_08_pp7_obj + 0x84);
        rhandweapID = get_item_in_hand_or_watch_menu(GUNRIGHT);
        body = BODY_Formal_Wear;
        head = HEAD_Male_Brosnan_Default;
        model = NULL;
        sub_GAME_7F07DE64(g_CurrentPlayer);
        if (getPlayerCount() == 1)
        {
            folderBond = fileGetBondForCurrentFolder();
            cuffId = g_CurrentPlayer->bondtype;
            switch (cuffId)                        /* switch 1 */
            {
            case CUFF_BOILER:                                 /* switch 1 */
                body = BODY_Special_Operations_Uniform; break;
            case CUFF_JUNGLE:                                 /* switch 1 */
                body = BODY_Jungle_Fatigues; break;
            case CUFF_SNOW:                                 /* switch 1 */
                body = BODY_Parka; break;
            case CUFF_BROSNAN:                                 /* switch 1 */
                body = BODY_Brosnan_Tuxedo; break;
            case CUFF_CONNERY:                                 /* switch 1 */
                body = BODY_Brosnan_Tuxedo; break;
            case CUFF_DALTON:                                 /* switch 1 */
                body = BODY_Brosnan_Tuxedo; break;
            case CUFF_MOORE:                                 /* switch 1 */
                body = BODY_Brosnan_Tuxedo; break;
            case CUFF_FOLDER:                                 /* switch 1 */
                switch (folderBond)                  /* switch 3; irregular */
                {
                case BOND_BROSNAN:                             /* switch 3 */
                    body = BODY_Brosnan_Tuxedo; break;
                case BOND_CONNERY:                             /* switch 3 */
                    body = BODY_Brosnan_Tuxedo; break;
                case BOND_DALTON:                             /* switch 3 */
                    body = BODY_Brosnan_Tuxedo; break;
                case BOND_MOORE:
                    body = BODY_Brosnan_Tuxedo; break;
                }
                break;
            }
            switch (folderBond)                      /* switch 4; irregular */
            {
            case BOND_BROSNAN:                                 /* switch 4 */
                switch (cuffId)                    /* switch 2 */
                {
                case CUFF_BOILER:                             /* switch 2 */
                    head = HEAD_Male_Brosnan_Boiler; break;
                case CUFF_JUNGLE:                             /* switch 2 */
                    head = HEAD_Male_Brosnan_Jungle; break;
                case CUFF_BROSNAN:                             /* switch 2 */
                    head = HEAD_Male_Brosnan_Tuxedo; break;
                case CUFF_CONNERY:                             /* switch 2 */
                    head = HEAD_Male_Brosnan_Tuxedo; break;
                case CUFF_DALTON:                             /* switch 2 */
                    head = HEAD_Male_Brosnan_Tuxedo; break;
                case CUFF_MOORE:                             /* switch 2 */
                    head = HEAD_Male_Brosnan_Tuxedo; break;
                case CUFF_FOLDER:                             /* switch 2 */
                    head = HEAD_Male_Brosnan_Tuxedo; break;
                }
                break;
            case BOND_CONNERY:                                 /* switch 4 */
                head = HEAD_Male_Brosnan_Tuxedo; break;
            case BOND_DALTON:                                 /* switch 4 */
                head = HEAD_Male_Brosnan_Tuxedo; break;
            case BOND_MOORE:                                 /* switch 4 */
                head = HEAD_Male_Brosnan_Tuxedo; break;
            }
        }
        else
        {
            head = get_player_mp_char_head(get_cur_playernum());
            body = get_player_mp_char_body(get_cur_playernum());
        }
        if (g_CameraMode == CAMERAMODE_SWIRL)
        {
            rhandweapID = starting_right_weapon;
        }
        if (getPlayerCount() == 1)
        {
#ifdef DEBUG
            assert(currentplayer->gunmemused[GUNRIGHT]==0 || currentplayer->gunmemtype[GUNRIGHT]==0);
            assert(currentplayer->gunmemused[GUNLEFT]==0 || currentplayer->gunmemtype[GUNLEFT]==0); //j
#endif

            remove_item_in_hand(GUNLEFT);
            remove_item_in_hand(GUNRIGHT);
            texInitPool(&texPool, headBuffer, headBufSize);
            p_bodyHeader = get_ptr_itemheader_in_hand(GUNRIGHT);

            p_bodyEntry = &c_item_entries[body];
            p_bodyEntryHeader = p_bodyEntry->header;

            pBody = p_bodyHeader;

            p_bodyHeader->RootNode = p_bodyEntryHeader->RootNode;
            p_bodyHeader->Skeleton = p_bodyEntryHeader->Skeleton;
            p_bodyHeader->Switches = p_bodyEntryHeader->Switches;
            p_bodyHeader->unkC = p_bodyEntryHeader->unkC;
            p_bodyHeader->BoundingVolumeRadius = p_bodyEntryHeader->BoundingVolumeRadius;
            p_bodyHeader->unk14 = p_bodyEntryHeader->unk14;
            p_bodyHeader->Textures = p_bodyEntryHeader->Textures;
            p_bodyHeader->isLoaded = p_bodyEntryHeader->isLoaded;

            p_modelEntry = p_bodyEntry;
            load_object_fill_header(p_bodyHeader, p_bodyEntry->filename, bodyBuffer, bodyBufSize, &texPool);
    #ifdef DEBUG
            assert(sizer <= bondmemsizer);
    #endif

            p_headEntry = &c_item_entries[head];
            p_headEntryHeader = p_headEntry->header;

            bufferSizeRemain = ALIGN64_V3(get_pc_buffer_remaining_value(p_modelEntry->filename) + 0x3F);
#ifdef DEBUG
            assert(sizer<=bondmemsizer);
#endif
            p_headHeader = bodyBuffer + bufferSizeRemain;
            bodyalignedSizeRemainPlus0x5F = ALIGN64_V3(bufferSizeRemain + 0x5F);

            p_headHeader->RootNode = p_headEntryHeader->RootNode;
            p_headHeader->Skeleton = p_headEntryHeader->Skeleton;
            p_headHeader->Switches = p_headEntryHeader->Switches;
            p_headHeader->unkC = p_headEntryHeader->unkC;
            p_headHeader->BoundingVolumeRadius = p_headEntryHeader->BoundingVolumeRadius;
            p_headHeader->unk14 = p_headEntryHeader->unk14;
            p_headHeader->Textures = p_headEntryHeader->Textures;
            p_headHeader->isLoaded = p_headEntryHeader->isLoaded;

            totalsize = bodyalignedSizeRemainPlus0x5F;
            pHead = p_headHeader;
            p_modelEntry = p_headEntry;
            load_object_fill_header(p_headHeader, p_headEntry->filename, bodyBuffer + bodyalignedSizeRemainPlus0x5F, bodyBufSize - bodyalignedSizeRemainPlus0x5F, &texPool);

            bufferSizeRemain = ALIGN64_V3(get_pc_buffer_remaining_value(p_modelEntry->filename) + totalsize + 0x3F);
            model = bodyBuffer + bufferSizeRemain;
            totalsize = ALIGN64_V3(bufferSizeRemain + 0xFB);
    #ifdef DEBUG
            assert(sizer <= bondmemsizer); // canonically total is sizer and bodybuff is bond
    #endif

            modelCalculateRwDataLen(pBody);
            modelCalculateRwDataLen(pHead);
            numRecords = pBody->numRecords + pHead->numRecords + 0xA;

            totalsize = ALIGN64_V3((numRecords * 4) + totalsize + 0x3F);
            #ifdef DEBUG
            assert(sizer<=bondmemsizer);
            #endif

            animInit(model, pBody, bodyBuffer + totalsize);
            model->Type = numRecords; //???
            //goto block_46;
            //&c_item_entries[head]->header = pHead;
        }

        pBody = &c_item_entries[body]->header;
        if (&c_item_entries[body]->header->RootNode == NULL)
        {
            fileLoad(&c_item_entries[body]->header, &c_item_entries[body]->filename);
        }

        if (&c_item_entries[head]->header->RootNode == NULL)
        {
            pHead = &c_item_entries[head]->header;
            fileLoad(&c_item_entries[head]->header, &c_item_entries[head]->filename);
//block_46:
//            &c_item_entries[head]->header = pHead;
        }
        g_CurrentPlayer->ptr_char_objectinstance = makeonebody(body, head, pBody, &c_item_entries[head]->header/*pHead maybe?*/, 0, model);
        #ifdef DEBUG
            assert(currentplayer->bondsub);
        #endif

        modelSetScale((Model *) g_CurrentPlayer->ptr_char_objectinstance, g_CurrentPlayer->ptr_char_objectinstance->unk14 * 0.97f);
        init_GUARDdata_with_set_values(g_CurrentPlayer->prop, g_CurrentPlayer->ptr_char_objectinstance, &g_CurrentPlayer->prop->pos, hRot, g_CurrentPlayer->prop->stan, 0);
        g_CurrentPlayer->prop->type = VIEWER;
        g_CurrentPlayer->prop->chr->chrflags |= CHRFLAG_INIT;
        setsuboffset(g_CurrentPlayer->ptr_char_objectinstance, &g_CurrentPlayer->prop->pos);
        setsubroty(g_CurrentPlayer->ptr_char_objectinstance, hRot);
        rhandPropID = getPropForHeldItem(rhandweapID);
        if (rhandPropID >= 0)
        {
            if (getPlayerCount() == 1)
            {
                p_rightHeader = bodyBuffer + totalsize;
                totalsize = ALIGN64_V3(totalsize + 0xC7);
                #ifdef DEBUG
                assert(sizer<=bondmemsizer);
                #endif
                p_lhandItemHeader = get_ptr_itemheader_in_hand(GUNLEFT);
                p_rhandItemHeader = &PitemZ_entries[rhandPropID]->header;
                p_leftHeader = p_lhandItemHeader;
                p_lhandItemHeader->RootNode = p_rhandItemHeader->RootNode;
                p_lhandItemHeader->Skeleton = p_rhandItemHeader->Skeleton;
                p_lhandItemHeader->Switches = p_rhandItemHeader->Switches;
                p_lhandItemHeader->unkC = p_rhandItemHeader->unkC;
                p_lhandItemHeader->BoundingVolumeRadius = p_rhandItemHeader->BoundingVolumeRadius;
                p_lhandItemHeader->unk14 = p_rhandItemHeader->unk14;
                p_lhandItemHeader->Textures = p_rhandItemHeader->Textures;
                p_lhandItemHeader->isLoaded = p_rhandItemHeader->isLoaded;
                p_modelEntry = &PitemZ_entries[rhandPropID];
                load_object_fill_header(p_lhandItemHeader, &PitemZ_entries[rhandPropID]->filename, bodyBuffer + totalsize, bodyBufSize - totalsize, &texPool);
                get_pc_buffer_remaining_value(p_modelEntry->filename);
                modelCalculateRwDataLen(p_leftHeader);
            }
            else
            {
                p_rightHeader = NULL;
                p_leftHeader = NULL;
            }
            something_with_generating_object(g_CurrentPlayer->prop->chr, rhandPropID, rhandweapID, 0, p_rightHeader, p_leftHeader);
        }
        chrlvIdleAnimationRelated7F023A94(g_CurrentPlayer->prop->chr, 0.0f);
        return;
    }
    if (curChr->model->anim == NULL)
    {
        curChr->chrflags |= CHRFLAG_INIT;
        chrlvIdleAnimationRelated7F023A94(curChr, 0.0f);
        setsuboffset(g_CurrentPlayer->ptr_char_objectinstance, &g_CurrentPlayer->prop->pos);
        setsubroty(g_CurrentPlayer->ptr_char_objectinstance, hRot);
    }
}
#else

#ifdef VERSION_US
GLOBAL_ASM(
.late_rodata
glabel D_80054FB8
.word 0
glabel D_80054FBC
.word 0

/*D:80054FC0*/
glabel a8s
/*"%8s"*/
.word 0x25387300

glabel aX4_0f
/*"x %4.0f"*/
.word 0x78202534
.word 0x2E306600

glabel aY4_0f
/*"y %4.0f"*/
.word 0x79202534
.word 0x2E306600

glabel aZ4_0f
/*"z %4.0f"*/
.word 0x7A202534
.word 0x2E306600

glabel aS3d
/*"%s %3d"*/
.word 0x25732025
.word 0x33640000
/*D:80054FE4*/
glabel jpt_bondcuff
.word .L7F079EA4
.word cuff_1_brosnan
.word cuff_2_jungle
.word cuff_3_boiler
.word cuff_4_snowsuit
.word cuff_5_connery
.word cuff_6_moore
.word cuff_7_dalton
.word cuff_8_variable_bond_head
.size jpt_bondcuff, . - jpt_bondcuff

/*D:80055008*/
glabel jpt_bondhead
.word .L7F079F84
.word brosnan_tux_head_set
.word jungle_head_set
.word boiler_bond_head_set
.word .L7F079F84
.word connery_head_set
.word moore_head_set
.word dalton_head_set
.word variable_body_head

glabel D_8005502C
.word 0x3f7851ec /*0.97000003*/
.text
glabel solo_char_load
/* 0AE820 7F079CF0 27BDFEE8 */  addiu $sp, $sp, -0x118
/* 0AE824 7F079CF4 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0AE828 7F079CF8 0FC227B9 */  jal   bondviewGetPlayerYawRadians
/* 0AE82C 7F079CFC 00000000 */   nop
/* 0AE830 7F079D00 3C0E8008 */  lui   $t6, %hi(g_CurrentPlayer)
/* 0AE834 7F079D04 8DCEA0B0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0AE838 7F079D08 E7A00114 */  swc1  $f0, 0x114($sp)
/* 0AE83C 7F079D0C 8DCF00A8 */  lw    $t7, 0xa8($t6)
/* 0AE840 7F079D10 8DE20004 */  lw    $v0, 4($t7)
/* 0AE844 7F079D14 544001CA */  bnezl $v0, .L7F07A440
/* 0AE848 7F079D18 8C4F001C */   lw    $t7, 0x1c($v0)
/* 0AE84C 7F079D1C 0FC173CC */  jal   getPlayerWeaponBufferForHand
/* 0AE850 7F079D20 00002025 */   move  $a0, $zero
/* 0AE854 7F079D24 AFA200F0 */  sw    $v0, 0xf0($sp)
/* 0AE858 7F079D28 0FC173CC */  jal   getPlayerWeaponBufferForHand
/* 0AE85C 7F079D2C 24040001 */   li    $a0, 1
/* 0AE860 7F079D30 AFA200EC */  sw    $v0, 0xec($sp)
/* 0AE864 7F079D34 AFA000E8 */  sw    $zero, 0xe8($sp)
/* 0AE868 7F079D38 0FC173D2 */  jal   getSizeBufferWeaponInHand
/* 0AE86C 7F079D3C 00002025 */   move  $a0, $zero
/* 0AE870 7F079D40 AFA200E4 */  sw    $v0, 0xe4($sp)
/* 0AE874 7F079D44 0FC173D2 */  jal   getSizeBufferWeaponInHand
/* 0AE878 7F079D48 24040001 */   li    $a0, 1
/* 0AE87C 7F079D4C 3C198003 */  lui   $t9, %hi(dummy_08_pp7_obj)
/* 0AE880 7F079D50 273965AC */  addiu $t9, %lo(dummy_08_pp7_obj) # addiu $t9, $t9, 0x65ac
/* 0AE884 7F079D54 AFA200E0 */  sw    $v0, 0xe0($sp)
/* 0AE888 7F079D58 272C0084 */  addiu $t4, $t9, 0x84
/* 0AE88C 7F079D5C 27B80054 */  addiu $t8, $sp, 0x54
.L7F079D60:
/* 0AE890 7F079D60 8F210000 */  lw    $at, ($t9)
/* 0AE894 7F079D64 2739000C */  addiu $t9, $t9, 0xc
/* 0AE898 7F079D68 2718000C */  addiu $t8, $t8, 0xc
/* 0AE89C 7F079D6C AF01FFF4 */  sw    $at, -0xc($t8)
/* 0AE8A0 7F079D70 8F21FFF8 */  lw    $at, -8($t9)
/* 0AE8A4 7F079D74 AF01FFF8 */  sw    $at, -8($t8)
/* 0AE8A8 7F079D78 8F21FFFC */  lw    $at, -4($t9)
/* 0AE8AC 7F079D7C 172CFFF8 */  bne   $t9, $t4, .L7F079D60
/* 0AE8B0 7F079D80 AF01FFFC */   sw    $at, -4($t8)
/* 0AE8B4 7F079D84 8F210000 */  lw    $at, ($t9)
/* 0AE8B8 7F079D88 00002025 */  move  $a0, $zero
/* 0AE8BC 7F079D8C 0FC17691 */  jal   get_item_in_hand_or_watch_menu
/* 0AE8C0 7F079D90 AF010000 */   sw    $at, ($t8)
/* 0AE8C4 7F079D94 240D0017 */  li    $t5, 23
/* 0AE8C8 7F079D98 240E004B */  li    $t6, 75
/* 0AE8CC 7F079D9C 3C048008 */  lui   $a0, %hi(g_CurrentPlayer)
/* 0AE8D0 7F079DA0 AFA20048 */  sw    $v0, 0x48($sp)
/* 0AE8D4 7F079DA4 AFAD0044 */  sw    $t5, 0x44($sp)
/* 0AE8D8 7F079DA8 AFAE0040 */  sw    $t6, 0x40($sp)
/* 0AE8DC 7F079DAC AFA00038 */  sw    $zero, 0x38($sp)
/* 0AE8E0 7F079DB0 0FC1F799 */  jal   sub_GAME_7F07DE64
/* 0AE8E4 7F079DB4 8C84A0B0 */   lw    $a0, %lo(g_CurrentPlayer)($a0)
/* 0AE8E8 7F079DB8 0FC26919 */  jal   getPlayerCount
/* 0AE8EC 7F079DBC 00000000 */   nop
/* 0AE8F0 7F079DC0 24010001 */  li    $at, 1
/* 0AE8F4 7F079DC4 14410066 */  bne   $v0, $at, .L7F079F60
/* 0AE8F8 7F079DC8 00000000 */   nop
/* 0AE8FC 7F079DCC 0FC0755B */  jal   fileGetBondForCurrentFolder
/* 0AE900 7F079DD0 00000000 */   nop
/* 0AE904 7F079DD4 3C0F8008 */  lui   $t7, %hi(g_CurrentPlayer)
/* 0AE908 7F079DD8 8DEFA0B0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0AE90C 7F079DDC 8DE3041C */  lw    $v1, 0x41c($t7)
/* 0AE910 7F079DE0 2C610009 */  sltiu $at, $v1, 9
/* 0AE914 7F079DE4 1020002F */  beqz  $at, .L7F079EA4
/* 0AE918 7F079DE8 00035880 */   sll   $t3, $v1, 2
/* 0AE91C 7F079DEC 3C018005 */  lui   $at, %hi(jpt_bondcuff)
/* 0AE920 7F079DF0 002B0821 */  addu  $at, $at, $t3
/* 0AE924 7F079DF4 8C2B4FE4 */  lw    $t3, %lo(jpt_bondcuff)($at)
/* 0AE928 7F079DF8 01600008 */  jr    $t3
/* 0AE92C 7F079DFC 00000000 */   nop
cuff_3_boiler:
/* 0AE930 7F079E00 240A0016 */  li    $t2, 22
/* 0AE934 7F079E04 10000027 */  b     .L7F079EA4
/* 0AE938 7F079E08 AFAA0044 */   sw    $t2, 0x44($sp)
cuff_2_jungle:
/* 0AE93C 7F079E0C 240C0018 */  li    $t4, 24
/* 0AE940 7F079E10 10000024 */  b     .L7F079EA4
/* 0AE944 7F079E14 AFAC0044 */   sw    $t4, 0x44($sp)
cuff_4_snowsuit:
/* 0AE948 7F079E18 24190019 */  li    $t9, 25
/* 0AE94C 7F079E1C 10000021 */  b     .L7F079EA4
/* 0AE950 7F079E20 AFB90044 */   sw    $t9, 0x44($sp)
cuff_1_brosnan:
/* 0AE954 7F079E24 24180005 */  li    $t8, 5
/* 0AE958 7F079E28 1000001E */  b     .L7F079EA4
/* 0AE95C 7F079E2C AFB80044 */   sw    $t8, 0x44($sp)
cuff_5_connery:
/* 0AE960 7F079E30 240D0005 */  li    $t5, 5
/* 0AE964 7F079E34 1000001B */  b     .L7F079EA4
/* 0AE968 7F079E38 AFAD0044 */   sw    $t5, 0x44($sp)
cuff_6_moore:
/* 0AE96C 7F079E3C 240E0005 */  li    $t6, 5
/* 0AE970 7F079E40 10000018 */  b     .L7F079EA4
/* 0AE974 7F079E44 AFAE0044 */   sw    $t6, 0x44($sp)
cuff_7_dalton:
/* 0AE978 7F079E48 240F0005 */  li    $t7, 5
/* 0AE97C 7F079E4C 10000015 */  b     .L7F079EA4
/* 0AE980 7F079E50 AFAF0044 */   sw    $t7, 0x44($sp)
cuff_8_variable_bond_head:
/* 0AE984 7F079E54 1040000C */  beqz  $v0, .L7F079E88
/* 0AE988 7F079E58 240B0005 */   li    $t3, 5
/* 0AE98C 7F079E5C 24010001 */  li    $at, 1
/* 0AE990 7F079E60 1041000B */  beq   $v0, $at, .L7F079E90
/* 0AE994 7F079E64 240A0005 */   li    $t2, 5
/* 0AE998 7F079E68 24010002 */  li    $at, 2
/* 0AE99C 7F079E6C 1041000A */  beq   $v0, $at, .L7F079E98
/* 0AE9A0 7F079E70 240C0005 */   li    $t4, 5
/* 0AE9A4 7F079E74 24010003 */  li    $at, 3
/* 0AE9A8 7F079E78 10410009 */  beq   $v0, $at, .L7F079EA0
/* 0AE9AC 7F079E7C 24190005 */   li    $t9, 5
/* 0AE9B0 7F079E80 10000008 */  b     .L7F079EA4
/* 0AE9B4 7F079E84 00000000 */   nop
.L7F079E88:
/* 0AE9B8 7F079E88 10000006 */  b     .L7F079EA4
/* 0AE9BC 7F079E8C AFAB0044 */   sw    $t3, 0x44($sp)
.L7F079E90:
/* 0AE9C0 7F079E90 10000004 */  b     .L7F079EA4
/* 0AE9C4 7F079E94 AFAA0044 */   sw    $t2, 0x44($sp)
.L7F079E98:
/* 0AE9C8 7F079E98 10000002 */  b     .L7F079EA4
/* 0AE9CC 7F079E9C AFAC0044 */   sw    $t4, 0x44($sp)
.L7F079EA0:
/* 0AE9D0 7F079EA0 AFB90044 */  sw    $t9, 0x44($sp)
.L7F079EA4:
/* 0AE9D4 7F079EA4 1040000C */  beqz  $v0, .L7F079ED8
/* 0AE9D8 7F079EA8 2C610009 */   sltiu $at, $v1, 9
/* 0AE9DC 7F079EAC 24010001 */  li    $at, 1
/* 0AE9E0 7F079EB0 10410025 */  beq   $v0, $at, .L7F079F48
/* 0AE9E4 7F079EB4 2418004E */   li    $t8, 78
/* 0AE9E8 7F079EB8 24010002 */  li    $at, 2
/* 0AE9EC 7F079EBC 10410024 */  beq   $v0, $at, .L7F079F50
/* 0AE9F0 7F079EC0 240D004E */   li    $t5, 78
/* 0AE9F4 7F079EC4 24010003 */  li    $at, 3
/* 0AE9F8 7F079EC8 10410023 */  beq   $v0, $at, .L7F079F58
/* 0AE9FC 7F079ECC 240E004E */   li    $t6, 78
/* 0AEA00 7F079ED0 1000002C */  b     .L7F079F84
/* 0AEA04 7F079ED4 00000000 */   nop
.L7F079ED8:
/* 0AEA08 7F079ED8 1020002A */  beqz  $at, .L7F079F84
/* 0AEA0C 7F079EDC 0003C080 */   sll   $t8, $v1, 2
/* 0AEA10 7F079EE0 3C018005 */  lui   $at, %hi(jpt_bondhead)
/* 0AEA14 7F079EE4 00380821 */  addu  $at, $at, $t8
/* 0AEA18 7F079EE8 8C385008 */  lw    $t8, %lo(jpt_bondhead)($at)
/* 0AEA1C 7F079EEC 03000008 */  jr    $t8
/* 0AEA20 7F079EF0 00000000 */   nop
boiler_bond_head_set:
/* 0AEA24 7F079EF4 240D004A */  li    $t5, 74
/* 0AEA28 7F079EF8 10000022 */  b     .L7F079F84
/* 0AEA2C 7F079EFC AFAD0040 */   sw    $t5, 0x40($sp)
jungle_head_set:
/* 0AEA30 7F079F00 240E004C */  li    $t6, 76
/* 0AEA34 7F079F04 1000001F */  b     .L7F079F84
/* 0AEA38 7F079F08 AFAE0040 */   sw    $t6, 0x40($sp)
brosnan_tux_head_set:
/* 0AEA3C 7F079F0C 240F004E */  li    $t7, 78
/* 0AEA40 7F079F10 1000001C */  b     .L7F079F84
/* 0AEA44 7F079F14 AFAF0040 */   sw    $t7, 0x40($sp)
connery_head_set:
/* 0AEA48 7F079F18 240B004E */  li    $t3, 78
/* 0AEA4C 7F079F1C 10000019 */  b     .L7F079F84
/* 0AEA50 7F079F20 AFAB0040 */   sw    $t3, 0x40($sp)
moore_head_set:
/* 0AEA54 7F079F24 240A004E */  li    $t2, 78
/* 0AEA58 7F079F28 10000016 */  b     .L7F079F84
/* 0AEA5C 7F079F2C AFAA0040 */   sw    $t2, 0x40($sp)
dalton_head_set:
/* 0AEA60 7F079F30 240C004E */  li    $t4, 78
/* 0AEA64 7F079F34 10000013 */  b     .L7F079F84
/* 0AEA68 7F079F38 AFAC0040 */   sw    $t4, 0x40($sp)
variable_body_head:
/* 0AEA6C 7F079F3C 2419004E */  li    $t9, 78
/* 0AEA70 7F079F40 10000010 */  b     .L7F079F84
/* 0AEA74 7F079F44 AFB90040 */   sw    $t9, 0x40($sp)
.L7F079F48:
/* 0AEA78 7F079F48 1000000E */  b     .L7F079F84
/* 0AEA7C 7F079F4C AFB80040 */   sw    $t8, 0x40($sp)
.L7F079F50:
/* 0AEA80 7F079F50 1000000C */  b     .L7F079F84
/* 0AEA84 7F079F54 AFAD0040 */   sw    $t5, 0x40($sp)
.L7F079F58:
/* 0AEA88 7F079F58 1000000A */  b     .L7F079F84
/* 0AEA8C 7F079F5C AFAE0040 */   sw    $t6, 0x40($sp)
.L7F079F60:
/* 0AEA90 7F079F60 0FC26C54 */  jal   get_cur_playernum
/* 0AEA94 7F079F64 00000000 */   nop
/* 0AEA98 7F079F68 0FC040AF */  jal   get_player_mp_char_head
/* 0AEA9C 7F079F6C 00402025 */   move  $a0, $v0
/* 0AEAA0 7F079F70 0FC26C54 */  jal   get_cur_playernum
/* 0AEAA4 7F079F74 AFA20040 */   sw    $v0, 0x40($sp)
/* 0AEAA8 7F079F78 0FC040D7 */  jal   get_player_mp_char_body
/* 0AEAAC 7F079F7C 00402025 */   move  $a0, $v0
/* 0AEAB0 7F079F80 AFA20044 */  sw    $v0, 0x44($sp)
.L7F079F84:
/* 0AEAB4 7F079F84 3C0F8003 */  lui   $t7, %hi(g_CameraMode)
/* 0AEAB8 7F079F88 8DEF6494 */  lw    $t7, %lo(g_CameraMode)($t7)
/* 0AEABC 7F079F8C 24010003 */  li    $at, 3
/* 0AEAC0 7F079F90 15E10003 */  bne   $t7, $at, .L7F079FA0
/* 0AEAC4 7F079F94 3C0B8008 */   lui   $t3, %hi(starting_weapon)
/* 0AEAC8 7F079F98 8D6B99E0 */  lw    $t3, %lo(starting_weapon)($t3)
/* 0AEACC 7F079F9C AFAB0048 */  sw    $t3, 0x48($sp)
.L7F079FA0:
/* 0AEAD0 7F079FA0 0FC26919 */  jal   getPlayerCount
/* 0AEAD4 7F079FA4 00000000 */   nop
/* 0AEAD8 7F079FA8 24010001 */  li    $at, 1
/* 0AEADC 7F079FAC 14410083 */  bne   $v0, $at, .L7F07A1BC
/* 0AEAE0 7F079FB0 8FAF0044 */   lw    $t7, 0x44($sp)
/* 0AEAE4 7F079FB4 0FC173D7 */  jal   remove_item_in_hand
/* 0AEAE8 7F079FB8 24040001 */   li    $a0, 1
/* 0AEAEC 7F079FBC 0FC173D7 */  jal   remove_item_in_hand
/* 0AEAF0 7F079FC0 00002025 */   move  $a0, $zero
/* 0AEAF4 7F079FC4 27A40100 */  addiu $a0, $sp, 0x100
/* 0AEAF8 7F079FC8 8FA500EC */  lw    $a1, 0xec($sp)
/* 0AEAFC 7F079FCC 0FC32EBD */  jal   texInitPool
/* 0AEB00 7F079FD0 8FA600E0 */   lw    $a2, 0xe0($sp)
/* 0AEB04 7F079FD4 0FC173C6 */  jal   get_ptr_itemheader_in_hand
/* 0AEB08 7F079FD8 00002025 */   move  $a0, $zero
/* 0AEB0C 7F079FDC 8FAA0044 */  lw    $t2, 0x44($sp)
/* 0AEB10 7F079FE0 3C198004 */  lui   $t9, %hi(c_item_entries)
/* 0AEB14 7F079FE4 2739DE10 */  addiu $t9, %lo(c_item_entries) # addiu $t9, $t9, -0x21f0
/* 0AEB18 7F079FE8 000A6080 */  sll   $t4, $t2, 2
/* 0AEB1C 7F079FEC 018A6021 */  addu  $t4, $t4, $t2
/* 0AEB20 7F079FF0 000C6080 */  sll   $t4, $t4, 2
/* 0AEB24 7F079FF4 01991821 */  addu  $v1, $t4, $t9
/* 0AEB28 7F079FF8 8C780000 */  lw    $t8, ($v1)
/* 0AEB2C 7F079FFC AFA200FC */  sw    $v0, 0xfc($sp)
/* 0AEB30 7F07A000 27AB0100 */  addiu $t3, $sp, 0x100
/* 0AEB34 7F07A004 8F010000 */  lw    $at, ($t8)
/* 0AEB38 7F07A008 00402025 */  move  $a0, $v0
/* 0AEB3C 7F07A00C AC410000 */  sw    $at, ($v0)
/* 0AEB40 7F07A010 8F0F0004 */  lw    $t7, 4($t8)
/* 0AEB44 7F07A014 AC4F0004 */  sw    $t7, 4($v0)
/* 0AEB48 7F07A018 8F010008 */  lw    $at, 8($t8)
/* 0AEB4C 7F07A01C AC410008 */  sw    $at, 8($v0)
/* 0AEB50 7F07A020 8F0F000C */  lw    $t7, 0xc($t8)
/* 0AEB54 7F07A024 AC4F000C */  sw    $t7, 0xc($v0)
/* 0AEB58 7F07A028 8F010010 */  lw    $at, 0x10($t8)
/* 0AEB5C 7F07A02C AC410010 */  sw    $at, 0x10($v0)
/* 0AEB60 7F07A030 8F0F0014 */  lw    $t7, 0x14($t8)
/* 0AEB64 7F07A034 AC4F0014 */  sw    $t7, 0x14($v0)
/* 0AEB68 7F07A038 8F010018 */  lw    $at, 0x18($t8)
/* 0AEB6C 7F07A03C AC410018 */  sw    $at, 0x18($v0)
/* 0AEB70 7F07A040 8F0F001C */  lw    $t7, 0x1c($t8)
/* 0AEB74 7F07A044 AC4F001C */  sw    $t7, 0x1c($v0)
/* 0AEB78 7F07A048 AFA30028 */  sw    $v1, 0x28($sp)
/* 0AEB7C 7F07A04C AFAB0010 */  sw    $t3, 0x10($sp)
/* 0AEB80 7F07A050 8FA700E4 */  lw    $a3, 0xe4($sp)
/* 0AEB84 7F07A054 8FA600F0 */  lw    $a2, 0xf0($sp)
/* 0AEB88 7F07A058 0FC1D929 */  jal   load_object_fill_header
/* 0AEB8C 7F07A05C 8C650004 */   lw    $a1, 4($v1)
/* 0AEB90 7F07A060 8FA30028 */  lw    $v1, 0x28($sp)
/* 0AEB94 7F07A064 0FC2F462 */  jal   get_pc_buffer_remaining_value
/* 0AEB98 7F07A068 8C640004 */   lw    $a0, 4($v1)
/* 0AEB9C 7F07A06C 8FAD0040 */  lw    $t5, 0x40($sp)
/* 0AEBA0 7F07A070 3C0F8004 */  lui   $t7, %hi(c_item_entries)
/* 0AEBA4 7F07A074 25EFDE10 */  addiu $t7, %lo(c_item_entries) # addiu $t7, $t7, -0x21f0
/* 0AEBA8 7F07A078 000DC080 */  sll   $t8, $t5, 2
/* 0AEBAC 7F07A07C 030DC021 */  addu  $t8, $t8, $t5
/* 0AEBB0 7F07A080 0018C080 */  sll   $t8, $t8, 2
/* 0AEBB4 7F07A084 030F1821 */  addu  $v1, $t8, $t7
/* 0AEBB8 7F07A088 8C6B0000 */  lw    $t3, ($v1)
/* 0AEBBC 7F07A08C 2448003F */  addiu $t0, $v0, 0x3f
/* 0AEBC0 7F07A090 8FA900F0 */  lw    $t1, 0xf0($sp)
/* 0AEBC4 7F07A094 350A003F */  ori   $t2, $t0, 0x3f
/* 0AEBC8 7F07A098 8D610000 */  lw    $at, ($t3)
/* 0AEBCC 7F07A09C 394C003F */  xori  $t4, $t2, 0x3f
/* 0AEBD0 7F07A0A0 2588005F */  addiu $t0, $t4, 0x5f
/* 0AEBD4 7F07A0A4 3519003F */  ori   $t9, $t0, 0x3f
/* 0AEBD8 7F07A0A8 012C2021 */  addu  $a0, $t1, $t4
/* 0AEBDC 7F07A0AC 3B28003F */  xori  $t0, $t9, 0x3f
/* 0AEBE0 7F07A0B0 AC810000 */  sw    $at, ($a0)
/* 0AEBE4 7F07A0B4 8D790004 */  lw    $t9, 4($t3)
/* 0AEBE8 7F07A0B8 27AD0100 */  addiu $t5, $sp, 0x100
/* 0AEBEC 7F07A0BC 01283021 */  addu  $a2, $t1, $t0
/* 0AEBF0 7F07A0C0 AC990004 */  sw    $t9, 4($a0)
/* 0AEBF4 7F07A0C4 8D610008 */  lw    $at, 8($t3)
/* 0AEBF8 7F07A0C8 AC810008 */  sw    $at, 8($a0)
/* 0AEBFC 7F07A0CC 8D79000C */  lw    $t9, 0xc($t3)
/* 0AEC00 7F07A0D0 AC99000C */  sw    $t9, 0xc($a0)
/* 0AEC04 7F07A0D4 8D610010 */  lw    $at, 0x10($t3)
/* 0AEC08 7F07A0D8 AC810010 */  sw    $at, 0x10($a0)
/* 0AEC0C 7F07A0DC 8D790014 */  lw    $t9, 0x14($t3)
/* 0AEC10 7F07A0E0 AC990014 */  sw    $t9, 0x14($a0)
/* 0AEC14 7F07A0E4 8D610018 */  lw    $at, 0x18($t3)
/* 0AEC18 7F07A0E8 AC810018 */  sw    $at, 0x18($a0)
/* 0AEC1C 7F07A0EC 8D79001C */  lw    $t9, 0x1c($t3)
/* 0AEC20 7F07A0F0 AC99001C */  sw    $t9, 0x1c($a0)
/* 0AEC24 7F07A0F4 8FAE00E4 */  lw    $t6, 0xe4($sp)
/* 0AEC28 7F07A0F8 AFA800E8 */  sw    $t0, 0xe8($sp)
/* 0AEC2C 7F07A0FC AFA400F8 */  sw    $a0, 0xf8($sp)
/* 0AEC30 7F07A100 AFA30028 */  sw    $v1, 0x28($sp)
/* 0AEC34 7F07A104 AFAD0010 */  sw    $t5, 0x10($sp)
/* 0AEC38 7F07A108 8C650004 */  lw    $a1, 4($v1)
/* 0AEC3C 7F07A10C 0FC1D929 */  jal   load_object_fill_header
/* 0AEC40 7F07A110 01C83823 */   subu  $a3, $t6, $t0
/* 0AEC44 7F07A114 8FA30028 */  lw    $v1, 0x28($sp)
/* 0AEC48 7F07A118 0FC2F462 */  jal   get_pc_buffer_remaining_value
/* 0AEC4C 7F07A11C 8C640004 */   lw    $a0, 4($v1)
/* 0AEC50 7F07A120 8FA300E8 */  lw    $v1, 0xe8($sp)
/* 0AEC54 7F07A124 8FAC00F0 */  lw    $t4, 0xf0($sp)
/* 0AEC58 7F07A128 8FA400FC */  lw    $a0, 0xfc($sp)
/* 0AEC5C 7F07A12C 00431821 */  addu  $v1, $v0, $v1
/* 0AEC60 7F07A130 2463003F */  addiu $v1, $v1, 0x3f
/* 0AEC64 7F07A134 3478003F */  ori   $t8, $v1, 0x3f
/* 0AEC68 7F07A138 3B0F003F */  xori  $t7, $t8, 0x3f
/* 0AEC6C 7F07A13C 25E300FB */  addiu $v1, $t7, 0xfb
/* 0AEC70 7F07A140 346B003F */  ori   $t3, $v1, 0x3f
/* 0AEC74 7F07A144 3979003F */  xori  $t9, $t3, 0x3f
/* 0AEC78 7F07A148 018F5021 */  addu  $t2, $t4, $t7
/* 0AEC7C 7F07A14C AFAA0038 */  sw    $t2, 0x38($sp)
/* 0AEC80 7F07A150 0FC1D73D */  jal   modelCalculateRwDataLen
/* 0AEC84 7F07A154 AFB900E8 */   sw    $t9, 0xe8($sp)
/* 0AEC88 7F07A158 0FC1D73D */  jal   modelCalculateRwDataLen
/* 0AEC8C 7F07A15C 8FA400F8 */   lw    $a0, 0xf8($sp)
/* 0AEC90 7F07A160 8FA500FC */  lw    $a1, 0xfc($sp)
/* 0AEC94 7F07A164 8FAF00F8 */  lw    $t7, 0xf8($sp)
/* 0AEC98 7F07A168 8FA300E8 */  lw    $v1, 0xe8($sp)
/* 0AEC9C 7F07A16C 84B80014 */  lh    $t8, 0x14($a1)
/* 0AECA0 7F07A170 85EC0014 */  lh    $t4, 0x14($t7)
/* 0AECA4 7F07A174 8FAE00F0 */  lw    $t6, 0xf0($sp)
/* 0AECA8 7F07A178 8FA40038 */  lw    $a0, 0x38($sp)
/* 0AECAC 7F07A17C 030C1021 */  addu  $v0, $t8, $t4
/* 0AECB0 7F07A180 2442000A */  addiu $v0, $v0, 0xa
/* 0AECB4 7F07A184 00025080 */  sll   $t2, $v0, 2
/* 0AECB8 7F07A188 01C33021 */  addu  $a2, $t6, $v1
/* 0AECBC 7F07A18C 01431821 */  addu  $v1, $t2, $v1
/* 0AECC0 7F07A190 2463003F */  addiu $v1, $v1, 0x3f
/* 0AECC4 7F07A194 346B003F */  ori   $t3, $v1, 0x3f
/* 0AECC8 7F07A198 3979003F */  xori  $t9, $t3, 0x3f
/* 0AECCC 7F07A19C AFB900E8 */  sw    $t9, 0xe8($sp)
/* 0AECD0 7F07A1A0 0FC1D7EB */  jal   animInit
/* 0AECD4 7F07A1A4 AFA20030 */   sw    $v0, 0x30($sp)
/* 0AECD8 7F07A1A8 8FA20030 */  lw    $v0, 0x30($sp)
/* 0AECDC 7F07A1AC 8FAE0038 */  lw    $t6, 0x38($sp)
/* 0AECE0 7F07A1B0 A5C20002 */  sh    $v0, 2($t6)
/* 0AECE4 7F07A1B4 1000001F */  b     .L7F07A234
/* 0AECE8 7F07A1B8 8FA700F8 */   lw    $a3, 0xf8($sp)
.L7F07A1BC:
/* 0AECEC 7F07A1BC 000FC080 */  sll   $t8, $t7, 2
/* 0AECF0 7F07A1C0 030FC021 */  addu  $t8, $t8, $t7
/* 0AECF4 7F07A1C4 3C0C8004 */  lui   $t4, %hi(c_item_entries)
/* 0AECF8 7F07A1C8 258CDE10 */  addiu $t4, %lo(c_item_entries) # addiu $t4, $t4, -0x21f0
/* 0AECFC 7F07A1CC 0018C080 */  sll   $t8, $t8, 2
/* 0AED00 7F07A1D0 030C1821 */  addu  $v1, $t8, $t4
/* 0AED04 7F07A1D4 8C6A0000 */  lw    $t2, ($v1)
/* 0AED08 7F07A1D8 AFAA00FC */  sw    $t2, 0xfc($sp)
/* 0AED0C 7F07A1DC 8D590000 */  lw    $t9, ($t2)
/* 0AED10 7F07A1E0 01402025 */  move  $a0, $t2
/* 0AED14 7F07A1E4 57200004 */  bnezl $t9, .L7F07A1F8
/* 0AED18 7F07A1E8 8FAD0040 */   lw    $t5, 0x40($sp)
/* 0AED1C 7F07A1EC 0FC1D953 */  jal   fileLoad
/* 0AED20 7F07A1F0 8C650004 */   lw    $a1, 4($v1)
/* 0AED24 7F07A1F4 8FAD0040 */  lw    $t5, 0x40($sp)
.L7F07A1F8:
/* 0AED28 7F07A1F8 3C0F8004 */  lui   $t7, %hi(c_item_entries)
/* 0AED2C 7F07A1FC 25EFDE10 */  addiu $t7, %lo(c_item_entries) # addiu $t7, $t7, -0x21f0
/* 0AED30 7F07A200 000D7080 */  sll   $t6, $t5, 2
/* 0AED34 7F07A204 01CD7021 */  addu  $t6, $t6, $t5
/* 0AED38 7F07A208 000E7080 */  sll   $t6, $t6, 2
/* 0AED3C 7F07A20C 01CF1821 */  addu  $v1, $t6, $t7
/* 0AED40 7F07A210 8C670000 */  lw    $a3, ($v1)
/* 0AED44 7F07A214 8CF80000 */  lw    $t8, ($a3)
/* 0AED48 7F07A218 00E02025 */  move  $a0, $a3
/* 0AED4C 7F07A21C 57000006 */  bnezl $t8, .L7F07A238
/* 0AED50 7F07A220 8FAC0038 */   lw    $t4, 0x38($sp)
/* 0AED54 7F07A224 8C650004 */  lw    $a1, 4($v1)
/* 0AED58 7F07A228 0FC1D953 */  jal   fileLoad
/* 0AED5C 7F07A22C AFA700F8 */   sw    $a3, 0xf8($sp)
/* 0AED60 7F07A230 8FA700F8 */  lw    $a3, 0xf8($sp)
.L7F07A234:
/* 0AED64 7F07A234 8FAC0038 */  lw    $t4, 0x38($sp)
.L7F07A238:
/* 0AED68 7F07A238 8FA40044 */  lw    $a0, 0x44($sp)
/* 0AED6C 7F07A23C 8FA50040 */  lw    $a1, 0x40($sp)
/* 0AED70 7F07A240 8FA600FC */  lw    $a2, 0xfc($sp)
/* 0AED74 7F07A244 AFA00010 */  sw    $zero, 0x10($sp)
/* 0AED78 7F07A248 0FC08CBA */  jal   makeonebody
/* 0AED7C 7F07A24C AFAC0014 */   sw    $t4, 0x14($sp)
/* 0AED80 7F07A250 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0AED84 7F07A254 2463A0B0 */  addiu $v1, %lo(g_CurrentPlayer) # addiu $v1, $v1, -0x5f50
/* 0AED88 7F07A258 8C6A0000 */  lw    $t2, ($v1)
/* 0AED8C 7F07A25C 3C018005 */  lui   $at, %hi(D_8005502C)
/* 0AED90 7F07A260 AD4200D4 */  sw    $v0, 0xd4($t2)
/* 0AED94 7F07A264 8C790000 */  lw    $t9, ($v1)
/* 0AED98 7F07A268 C426502C */  lwc1  $f6, %lo(D_8005502C)($at)
/* 0AED9C 7F07A26C 8F2400D4 */  lw    $a0, 0xd4($t9)
/* 0AEDA0 7F07A270 C4840014 */  lwc1  $f4, 0x14($a0)
/* 0AEDA4 7F07A274 46062202 */  mul.s $f8, $f4, $f6
/* 0AEDA8 7F07A278 44054000 */  mfc1  $a1, $f8
/* 0AEDAC 7F07A27C 0FC1B39E */  jal   modelSetScale
/* 0AEDB0 7F07A280 00000000 */   nop
/* 0AEDB4 7F07A284 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0AEDB8 7F07A288 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0AEDBC 7F07A28C 8FA70114 */  lw    $a3, 0x114($sp)
/* 0AEDC0 7F07A290 8C6400A8 */  lw    $a0, 0xa8($v1)
/* 0AEDC4 7F07A294 8C6500D4 */  lw    $a1, 0xd4($v1)
/* 0AEDC8 7F07A298 8C8B0014 */  lw    $t3, 0x14($a0)
/* 0AEDCC 7F07A29C AFA00014 */  sw    $zero, 0x14($sp)
/* 0AEDD0 7F07A2A0 24860008 */  addiu $a2, $a0, 8
/* 0AEDD4 7F07A2A4 0FC0802F */  jal   init_GUARDdata_with_set_values
/* 0AEDD8 7F07A2A8 AFAB0010 */   sw    $t3, 0x10($sp)
/* 0AEDDC 7F07A2AC 3C078008 */  lui   $a3, %hi(g_CurrentPlayer)
/* 0AEDE0 7F07A2B0 24E7A0B0 */  addiu $a3, %lo(g_CurrentPlayer) # addiu $a3, $a3, -0x5f50
/* 0AEDE4 7F07A2B4 8CEE0000 */  lw    $t6, ($a3)
/* 0AEDE8 7F07A2B8 240D0006 */  li    $t5, 6
/* 0AEDEC 7F07A2BC 8DCF00A8 */  lw    $t7, 0xa8($t6)
/* 0AEDF0 7F07A2C0 A1ED0000 */  sb    $t5, ($t7)
/* 0AEDF4 7F07A2C4 8CF80000 */  lw    $t8, ($a3)
/* 0AEDF8 7F07A2C8 8F0C00A8 */  lw    $t4, 0xa8($t8)
/* 0AEDFC 7F07A2CC 8D860004 */  lw    $a2, 4($t4)
/* 0AEE00 7F07A2D0 8CCA0014 */  lw    $t2, 0x14($a2)
/* 0AEE04 7F07A2D4 35590001 */  ori   $t9, $t2, 1
/* 0AEE08 7F07A2D8 ACD90014 */  sw    $t9, 0x14($a2)
/* 0AEE0C 7F07A2DC 8CE30000 */  lw    $v1, ($a3)
/* 0AEE10 7F07A2E0 8C6500A8 */  lw    $a1, 0xa8($v1)
/* 0AEE14 7F07A2E4 8C6400D4 */  lw    $a0, 0xd4($v1)
/* 0AEE18 7F07A2E8 AFA60110 */  sw    $a2, 0x110($sp)
/* 0AEE1C 7F07A2EC 0FC1B303 */  jal   setsuboffset
/* 0AEE20 7F07A2F0 24A50008 */   addiu $a1, $a1, 8
/* 0AEE24 7F07A2F4 3C0B8008 */  lui   $t3, %hi(g_CurrentPlayer)
/* 0AEE28 7F07A2F8 8D6BA0B0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0AEE2C 7F07A2FC 8FA50114 */  lw    $a1, 0x114($sp)
/* 0AEE30 7F07A300 0FC1B34F */  jal   setsubroty
/* 0AEE34 7F07A304 8D6400D4 */   lw    $a0, 0xd4($t3)
/* 0AEE38 7F07A308 0FC26C91 */  jal   getPropForHeldItem
/* 0AEE3C 7F07A30C 8FA40048 */   lw    $a0, 0x48($sp)
/* 0AEE40 7F07A310 04400045 */  bltz  $v0, .L7F07A428
/* 0AEE44 7F07A314 AFA2004C */   sw    $v0, 0x4c($sp)
/* 0AEE48 7F07A318 0FC26919 */  jal   getPlayerCount
/* 0AEE4C 7F07A31C 00000000 */   nop
/* 0AEE50 7F07A320 24010001 */  li    $at, 1
/* 0AEE54 7F07A324 14410035 */  bne   $v0, $at, .L7F07A3FC
/* 0AEE58 7F07A328 8FAE00F0 */   lw    $t6, 0xf0($sp)
/* 0AEE5C 7F07A32C 8FA200E8 */  lw    $v0, 0xe8($sp)
/* 0AEE60 7F07A330 24040001 */  li    $a0, 1
/* 0AEE64 7F07A334 01C26821 */  addu  $t5, $t6, $v0
/* 0AEE68 7F07A338 244200C7 */  addiu $v0, $v0, 0xc7
/* 0AEE6C 7F07A33C 344F003F */  ori   $t7, $v0, 0x3f
/* 0AEE70 7F07A340 39F8003F */  xori  $t8, $t7, 0x3f
/* 0AEE74 7F07A344 AFAD00DC */  sw    $t5, 0xdc($sp)
/* 0AEE78 7F07A348 0FC173C6 */  jal   get_ptr_itemheader_in_hand
/* 0AEE7C 7F07A34C AFB800E8 */   sw    $t8, 0xe8($sp)
/* 0AEE80 7F07A350 8FAC004C */  lw    $t4, 0x4c($sp)
/* 0AEE84 7F07A354 3C198004 */  lui   $t9, %hi(PitemZ_entries)
/* 0AEE88 7F07A358 2739A228 */  addiu $t9, %lo(PitemZ_entries) # addiu $t9, $t9, -0x5dd8
/* 0AEE8C 7F07A35C 000C5080 */  sll   $t2, $t4, 2
/* 0AEE90 7F07A360 014C5023 */  subu  $t2, $t2, $t4
/* 0AEE94 7F07A364 000A5080 */  sll   $t2, $t2, 2
/* 0AEE98 7F07A368 01591821 */  addu  $v1, $t2, $t9
/* 0AEE9C 7F07A36C 8C6B0000 */  lw    $t3, ($v1)
/* 0AEEA0 7F07A370 AFA200F4 */  sw    $v0, 0xf4($sp)
/* 0AEEA4 7F07A374 27AD0100 */  addiu $t5, $sp, 0x100
/* 0AEEA8 7F07A378 8D610000 */  lw    $at, ($t3)
/* 0AEEAC 7F07A37C 00402025 */  move  $a0, $v0
/* 0AEEB0 7F07A380 AC410000 */  sw    $at, ($v0)
/* 0AEEB4 7F07A384 8D6F0004 */  lw    $t7, 4($t3)
/* 0AEEB8 7F07A388 AC4F0004 */  sw    $t7, 4($v0)
/* 0AEEBC 7F07A38C 8D610008 */  lw    $at, 8($t3)
/* 0AEEC0 7F07A390 AC410008 */  sw    $at, 8($v0)
/* 0AEEC4 7F07A394 8D6F000C */  lw    $t7, 0xc($t3)
/* 0AEEC8 7F07A398 AC4F000C */  sw    $t7, 0xc($v0)
/* 0AEECC 7F07A39C 8D610010 */  lw    $at, 0x10($t3)
/* 0AEED0 7F07A3A0 AC410010 */  sw    $at, 0x10($v0)
/* 0AEED4 7F07A3A4 8D6F0014 */  lw    $t7, 0x14($t3)
/* 0AEED8 7F07A3A8 AC4F0014 */  sw    $t7, 0x14($v0)
/* 0AEEDC 7F07A3AC 8D610018 */  lw    $at, 0x18($t3)
/* 0AEEE0 7F07A3B0 AC410018 */  sw    $at, 0x18($v0)
/* 0AEEE4 7F07A3B4 8D6F001C */  lw    $t7, 0x1c($t3)
/* 0AEEE8 7F07A3B8 AC4F001C */  sw    $t7, 0x1c($v0)
/* 0AEEEC 7F07A3BC 8FAC00E8 */  lw    $t4, 0xe8($sp)
/* 0AEEF0 7F07A3C0 8FAA00E4 */  lw    $t2, 0xe4($sp)
/* 0AEEF4 7F07A3C4 8FB800F0 */  lw    $t8, 0xf0($sp)
/* 0AEEF8 7F07A3C8 AFA30028 */  sw    $v1, 0x28($sp)
/* 0AEEFC 7F07A3CC AFAD0010 */  sw    $t5, 0x10($sp)
/* 0AEF00 7F07A3D0 8C650004 */  lw    $a1, 4($v1)
/* 0AEF04 7F07A3D4 014C3823 */  subu  $a3, $t2, $t4
/* 0AEF08 7F07A3D8 0FC1D929 */  jal   load_object_fill_header
/* 0AEF0C 7F07A3DC 030C3021 */   addu  $a2, $t8, $t4
/* 0AEF10 7F07A3E0 8FA30028 */  lw    $v1, 0x28($sp)
/* 0AEF14 7F07A3E4 0FC2F462 */  jal   get_pc_buffer_remaining_value
/* 0AEF18 7F07A3E8 8C640004 */   lw    $a0, 4($v1)
/* 0AEF1C 7F07A3EC 0FC1D73D */  jal   modelCalculateRwDataLen
/* 0AEF20 7F07A3F0 8FA400F4 */   lw    $a0, 0xf4($sp)
/* 0AEF24 7F07A3F4 10000004 */  b     .L7F07A408
/* 0AEF28 7F07A3F8 8FAE00DC */   lw    $t6, 0xdc($sp)
.L7F07A3FC:
/* 0AEF2C 7F07A3FC AFA000DC */  sw    $zero, 0xdc($sp)
/* 0AEF30 7F07A400 AFA000F4 */  sw    $zero, 0xf4($sp)
/* 0AEF34 7F07A404 8FAE00DC */  lw    $t6, 0xdc($sp)
.L7F07A408:
/* 0AEF38 7F07A408 8FAB00F4 */  lw    $t3, 0xf4($sp)
/* 0AEF3C 7F07A40C 8FA40110 */  lw    $a0, 0x110($sp)
/* 0AEF40 7F07A410 8FA5004C */  lw    $a1, 0x4c($sp)
/* 0AEF44 7F07A414 8FA60048 */  lw    $a2, 0x48($sp)
/* 0AEF48 7F07A418 00003825 */  move  $a3, $zero
/* 0AEF4C 7F07A41C AFAE0010 */  sw    $t6, 0x10($sp)
/* 0AEF50 7F07A420 0FC14885 */  jal   something_with_generating_object
/* 0AEF54 7F07A424 AFAB0014 */   sw    $t3, 0x14($sp)
.L7F07A428:
/* 0AEF58 7F07A428 8FA40110 */  lw    $a0, 0x110($sp)
/* 0AEF5C 7F07A42C 0FC08EA5 */  jal   chrlvIdleAnimationRelated7F023A94
/* 0AEF60 7F07A430 24050000 */   li    $a1, 0
/* 0AEF64 7F07A434 10000017 */  b     .L7F07A494
/* 0AEF68 7F07A438 8FBF001C */   lw    $ra, 0x1c($sp)
/* 0AEF6C 7F07A43C 8C4F001C */  lw    $t7, 0x1c($v0)
.L7F07A440:
/* 0AEF70 7F07A440 00402025 */  move  $a0, $v0
/* 0AEF74 7F07A444 8DF80020 */  lw    $t8, 0x20($t7)
/* 0AEF78 7F07A448 57000012 */  bnezl $t8, .L7F07A494
/* 0AEF7C 7F07A44C 8FBF001C */   lw    $ra, 0x1c($sp)
/* 0AEF80 7F07A450 8C4C0014 */  lw    $t4, 0x14($v0)
/* 0AEF84 7F07A454 24050000 */  li    $a1, 0
/* 0AEF88 7F07A458 358A0001 */  ori   $t2, $t4, 1
/* 0AEF8C 7F07A45C 0FC08EA5 */  jal   chrlvIdleAnimationRelated7F023A94
/* 0AEF90 7F07A460 AC4A0014 */   sw    $t2, 0x14($v0)
/* 0AEF94 7F07A464 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0AEF98 7F07A468 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0AEF9C 7F07A46C 8C6500A8 */  lw    $a1, 0xa8($v1)
/* 0AEFA0 7F07A470 8C6400D4 */  lw    $a0, 0xd4($v1)
/* 0AEFA4 7F07A474 0FC1B303 */  jal   setsuboffset
/* 0AEFA8 7F07A478 24A50008 */   addiu $a1, $a1, 8
/* 0AEFAC 7F07A47C 3C198008 */  lui   $t9, %hi(g_CurrentPlayer)
/* 0AEFB0 7F07A480 8F39A0B0 */  lw    $t9, %lo(g_CurrentPlayer)($t9)
/* 0AEFB4 7F07A484 8FA50114 */  lw    $a1, 0x114($sp)
/* 0AEFB8 7F07A488 0FC1B34F */  jal   setsubroty
/* 0AEFBC 7F07A48C 8F2400D4 */   lw    $a0, 0xd4($t9)
/* 0AEFC0 7F07A490 8FBF001C */  lw    $ra, 0x1c($sp)
.L7F07A494:
/* 0AEFC4 7F07A494 27BD0118 */  addiu $sp, $sp, 0x118
/* 0AEFC8 7F07A498 03E00008 */  jr    $ra
/* 0AEFCC 7F07A49C 00000000 */   nop
)
#endif

#ifdef VERSION_JP
GLOBAL_ASM(
.late_rodata
glabel D_80054FB8
.word 0
glabel D_80054FBC
.word 0

/*D:80054FC0*/
glabel a8s
/*"%8s"*/
.word 0x25387300

glabel aX4_0f
/*"x %4.0f"*/
.word 0x78202534
.word 0x2E306600

glabel aY4_0f
/*"y %4.0f"*/
.word 0x79202534
.word 0x2E306600

glabel aZ4_0f
/*"z %4.0f"*/
.word 0x7A202534
.word 0x2E306600

glabel aS3d
/*"%s %3d"*/
.word 0x25732025
.word 0x33640000
/*D:80054FE4*/
glabel jpt_bondcuff
.word .Ljp7F07A494
.word cuff_1_brosnan
.word cuff_2_jungle
.word cuff_3_boiler
.word cuff_4_snowsuit
.word cuff_5_connery
.word cuff_6_moore
.word cuff_7_dalton
.word cuff_8_variable_bond_head
.size jpt_bondcuff, . - jpt_bondcuff

/*D:80055008*/
glabel jpt_bondhead
.word .Ljp7F07A574
.word brosnan_tux_head_set
.word jungle_head_set
.word boiler_bond_head_set
.word .Ljp7F07A574
.word connery_head_set
.word moore_head_set
.word dalton_head_set
.word variable_body_head

glabel D_8005502C
.word 0x3f7851ec /*0.97000003*/
.text
glabel solo_char_load
/* 0AEE50 7F07A2E0 27BDFEE8 */  addiu $sp, $sp, -0x118
/* 0AEE54 7F07A2E4 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0AEE58 7F07A2E8 0FC22987 */  jal   bondviewGetPlayerYawRadians
/* 0AEE5C 7F07A2EC 00000000 */   nop
/* 0AEE60 7F07A2F0 3C0E8008 */  lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8008
/* 0AEE64 7F07A2F4 8DCEA120 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0AEE68 7F07A2F8 E7A00114 */  swc1  $f0, 0x114($sp)
/* 0AEE6C 7F07A2FC 8DCF00A8 */  lw    $t7, 0xa8($t6)
/* 0AEE70 7F07A300 8DE20004 */  lw    $v0, 4($t7)
/* 0AEE74 7F07A304 544001D6 */  bnezl $v0, .Ljp7F07AA60
/* 0AEE78 7F07A308 8C59001C */   lw    $t9, 0x1c($v0)
/* 0AEE7C 7F07A30C 0FC17514 */  jal   getPlayerWeaponBufferForHand
/* 0AEE80 7F07A310 00002025 */   move  $a0, $zero
/* 0AEE84 7F07A314 AFA200F0 */  sw    $v0, 0xf0($sp)
/* 0AEE88 7F07A318 0FC17514 */  jal   getPlayerWeaponBufferForHand
/* 0AEE8C 7F07A31C 24040001 */   li    $a0, 1
/* 0AEE90 7F07A320 AFA200EC */  sw    $v0, 0xec($sp)
/* 0AEE94 7F07A324 AFA000E8 */  sw    $zero, 0xe8($sp)
/* 0AEE98 7F07A328 0FC1751A */  jal   getSizeBufferWeaponInHand
/* 0AEE9C 7F07A32C 00002025 */   move  $a0, $zero
/* 0AEEA0 7F07A330 AFA200E4 */  sw    $v0, 0xe4($sp)
/* 0AEEA4 7F07A334 0FC1751A */  jal   getSizeBufferWeaponInHand
/* 0AEEA8 7F07A338 24040001 */   li    $a0, 1
/* 0AEEAC 7F07A33C 3C198003 */  lui   $t9, %hi(dummy_08_pp7_obj) # $t9, 0x8003
/* 0AEEB0 7F07A340 273965EC */  addiu $t9, %lo(dummy_08_pp7_obj) # addiu $t9, $t9, 0x65ec
/* 0AEEB4 7F07A344 AFA200E0 */  sw    $v0, 0xe0($sp)
/* 0AEEB8 7F07A348 272C0084 */  addiu $t4, $t9, 0x84
/* 0AEEBC 7F07A34C 27B80054 */  addiu $t8, $sp, 0x54
.Ljp7F07A350:
/* 0AEEC0 7F07A350 8F210000 */  lw    $at, ($t9)
/* 0AEEC4 7F07A354 2739000C */  addiu $t9, $t9, 0xc
/* 0AEEC8 7F07A358 2718000C */  addiu $t8, $t8, 0xc
/* 0AEECC 7F07A35C AF01FFF4 */  sw    $at, -0xc($t8)
/* 0AEED0 7F07A360 8F21FFF8 */  lw    $at, -8($t9)
/* 0AEED4 7F07A364 AF01FFF8 */  sw    $at, -8($t8)
/* 0AEED8 7F07A368 8F21FFFC */  lw    $at, -4($t9)
/* 0AEEDC 7F07A36C 172CFFF8 */  bne   $t9, $t4, .Ljp7F07A350
/* 0AEEE0 7F07A370 AF01FFFC */   sw    $at, -4($t8)
/* 0AEEE4 7F07A374 8F210000 */  lw    $at, ($t9)
/* 0AEEE8 7F07A378 00002025 */  move  $a0, $zero
/* 0AEEEC 7F07A37C 0FC177D9 */  jal   get_item_in_hand_or_watch_menu
/* 0AEEF0 7F07A380 AF010000 */   sw    $at, ($t8)
/* 0AEEF4 7F07A384 240D0017 */  li    $t5, 23
/* 0AEEF8 7F07A388 240E004B */  li    $t6, 75
/* 0AEEFC 7F07A38C 3C048008 */  lui   $a0, %hi(g_CurrentPlayer) # $a0, 0x8008
/* 0AEF00 7F07A390 AFA20048 */  sw    $v0, 0x48($sp)
/* 0AEF04 7F07A394 AFAD0044 */  sw    $t5, 0x44($sp)
/* 0AEF08 7F07A398 AFAE0040 */  sw    $t6, 0x40($sp)
/* 0AEF0C 7F07A39C AFA00038 */  sw    $zero, 0x38($sp)
/* 0AEF10 7F07A3A0 0FC1F922 */  jal   sub_GAME_7F07DE64
/* 0AEF14 7F07A3A4 8C84A120 */   lw    $a0, %lo(g_CurrentPlayer)($a0)
/* 0AEF18 7F07A3A8 0FC26C01 */  jal   getPlayerCount
/* 0AEF1C 7F07A3AC 00000000 */   nop
/* 0AEF20 7F07A3B0 24010001 */  li    $at, 1
/* 0AEF24 7F07A3B4 14410066 */  bne   $v0, $at, .Ljp7F07A550
/* 0AEF28 7F07A3B8 00000000 */   nop
/* 0AEF2C 7F07A3BC 0FC075C4 */  jal   fileGetBondForCurrentFolder
/* 0AEF30 7F07A3C0 00000000 */   nop
/* 0AEF34 7F07A3C4 3C0F8008 */  lui   $t7, %hi(g_CurrentPlayer) # $t7, 0x8008
/* 0AEF38 7F07A3C8 8DEFA120 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0AEF3C 7F07A3CC 8DE3041C */  lw    $v1, 0x41c($t7)
/* 0AEF40 7F07A3D0 2C610009 */  sltiu $at, $v1, 9
/* 0AEF44 7F07A3D4 1020002F */  beqz  $at, .Ljp7F07A494
/* 0AEF48 7F07A3D8 00035880 */   sll   $t3, $v1, 2
/* 0AEF4C 7F07A3DC 3C018005 */  lui   $at, %hi(jpt_bondcuff)
/* 0AEF50 7F07A3E0 002B0821 */  addu  $at, $at, $t3
/* 0AEF54 7F07A3E4 8C2B5014 */  lw    $t3, %lo(jpt_bondcuff)($at)
/* 0AEF58 7F07A3E8 01600008 */  jr    $t3
/* 0AEF5C 7F07A3EC 00000000 */   nop
cuff_3_boiler:
/* 0AEF60 7F07A3F0 240A0016 */  li    $t2, 22
/* 0AEF64 7F07A3F4 10000027 */  b     .Ljp7F07A494
/* 0AEF68 7F07A3F8 AFAA0044 */   sw    $t2, 0x44($sp)
cuff_2_jungle:
/* 0AEF6C 7F07A3FC 240C0018 */  li    $t4, 24
/* 0AEF70 7F07A400 10000024 */  b     .Ljp7F07A494
/* 0AEF74 7F07A404 AFAC0044 */   sw    $t4, 0x44($sp)
cuff_4_snowsuit:
/* 0AEF78 7F07A408 24190019 */  li    $t9, 25
/* 0AEF7C 7F07A40C 10000021 */  b     .Ljp7F07A494
/* 0AEF80 7F07A410 AFB90044 */   sw    $t9, 0x44($sp)
cuff_1_brosnan:
/* 0AEF84 7F07A414 24180005 */  li    $t8, 5
/* 0AEF88 7F07A418 1000001E */  b     .Ljp7F07A494
/* 0AEF8C 7F07A41C AFB80044 */   sw    $t8, 0x44($sp)
cuff_5_connery:
/* 0AEF90 7F07A420 240D0005 */  li    $t5, 5
/* 0AEF94 7F07A424 1000001B */  b     .Ljp7F07A494
/* 0AEF98 7F07A428 AFAD0044 */   sw    $t5, 0x44($sp)
cuff_6_moore:
/* 0AEF9C 7F07A42C 240E0005 */  li    $t6, 5
/* 0AEFA0 7F07A430 10000018 */  b     .Ljp7F07A494
/* 0AEFA4 7F07A434 AFAE0044 */   sw    $t6, 0x44($sp)
cuff_7_dalton:
/* 0AEFA8 7F07A438 240F0005 */  li    $t7, 5
/* 0AEFAC 7F07A43C 10000015 */  b     .Ljp7F07A494
/* 0AEFB0 7F07A440 AFAF0044 */   sw    $t7, 0x44($sp)
cuff_8_variable_bond_head:
/* 0AEFB4 7F07A444 1040000C */  beqz  $v0, .Ljp7F07A478
/* 0AEFB8 7F07A448 240B0005 */   li    $t3, 5
/* 0AEFBC 7F07A44C 24010001 */  li    $at, 1
/* 0AEFC0 7F07A450 1041000B */  beq   $v0, $at, .Ljp7F07A480
/* 0AEFC4 7F07A454 240A0005 */   li    $t2, 5
/* 0AEFC8 7F07A458 24010002 */  li    $at, 2
/* 0AEFCC 7F07A45C 1041000A */  beq   $v0, $at, .Ljp7F07A488
/* 0AEFD0 7F07A460 240C0005 */   li    $t4, 5
/* 0AEFD4 7F07A464 24010003 */  li    $at, 3
/* 0AEFD8 7F07A468 10410009 */  beq   $v0, $at, .Ljp7F07A490
/* 0AEFDC 7F07A46C 24190005 */   li    $t9, 5
/* 0AEFE0 7F07A470 10000008 */  b     .Ljp7F07A494
/* 0AEFE4 7F07A474 00000000 */   nop
.Ljp7F07A478:
/* 0AEFE8 7F07A478 10000006 */  b     .Ljp7F07A494
/* 0AEFEC 7F07A47C AFAB0044 */   sw    $t3, 0x44($sp)
.Ljp7F07A480:
/* 0AEFF0 7F07A480 10000004 */  b     .Ljp7F07A494
/* 0AEFF4 7F07A484 AFAA0044 */   sw    $t2, 0x44($sp)
.Ljp7F07A488:
/* 0AEFF8 7F07A488 10000002 */  b     .Ljp7F07A494
/* 0AEFFC 7F07A48C AFAC0044 */   sw    $t4, 0x44($sp)
.Ljp7F07A490:
/* 0AF000 7F07A490 AFB90044 */  sw    $t9, 0x44($sp)
.Ljp7F07A494:
/* 0AF004 7F07A494 1040000C */  beqz  $v0, .Ljp7F07A4C8
/* 0AF008 7F07A498 2C610009 */   sltiu $at, $v1, 9
/* 0AF00C 7F07A49C 24010001 */  li    $at, 1
/* 0AF010 7F07A4A0 10410025 */  beq   $v0, $at, .Ljp7F07A538
/* 0AF014 7F07A4A4 2418004E */   li    $t8, 78
/* 0AF018 7F07A4A8 24010002 */  li    $at, 2
/* 0AF01C 7F07A4AC 10410024 */  beq   $v0, $at, .Ljp7F07A540
/* 0AF020 7F07A4B0 240D004E */   li    $t5, 78
/* 0AF024 7F07A4B4 24010003 */  li    $at, 3
/* 0AF028 7F07A4B8 10410023 */  beq   $v0, $at, .Ljp7F07A548
/* 0AF02C 7F07A4BC 240E004E */   li    $t6, 78
/* 0AF030 7F07A4C0 1000002C */  b     .Ljp7F07A574
/* 0AF034 7F07A4C4 00000000 */   nop
.Ljp7F07A4C8:
/* 0AF038 7F07A4C8 1020002A */  beqz  $at, .Ljp7F07A574
/* 0AF03C 7F07A4CC 0003C080 */   sll   $t8, $v1, 2
/* 0AF040 7F07A4D0 3C018005 */  lui   $at, %hi(jpt_bondhead)
/* 0AF044 7F07A4D4 00380821 */  addu  $at, $at, $t8
/* 0AF048 7F07A4D8 8C385038 */  lw    $t8, %lo(jpt_bondhead)($at)
/* 0AF04C 7F07A4DC 03000008 */  jr    $t8
/* 0AF050 7F07A4E0 00000000 */   nop
boiler_bond_head_set:
/* 0AF054 7F07A4E4 240D004A */  li    $t5, 74
/* 0AF058 7F07A4E8 10000022 */  b     .Ljp7F07A574
/* 0AF05C 7F07A4EC AFAD0040 */   sw    $t5, 0x40($sp)
jungle_head_set:
/* 0AF060 7F07A4F0 240E004C */  li    $t6, 76
/* 0AF064 7F07A4F4 1000001F */  b     .Ljp7F07A574
/* 0AF068 7F07A4F8 AFAE0040 */   sw    $t6, 0x40($sp)
brosnan_tux_head_set:
/* 0AF06C 7F07A4FC 240F004E */  li    $t7, 78
/* 0AF070 7F07A500 1000001C */  b     .Ljp7F07A574
/* 0AF074 7F07A504 AFAF0040 */   sw    $t7, 0x40($sp)
connery_head_set:
/* 0AF078 7F07A508 240B004E */  li    $t3, 78
/* 0AF07C 7F07A50C 10000019 */  b     .Ljp7F07A574
/* 0AF080 7F07A510 AFAB0040 */   sw    $t3, 0x40($sp)
moore_head_set:
/* 0AF084 7F07A514 240A004E */  li    $t2, 78
/* 0AF088 7F07A518 10000016 */  b     .Ljp7F07A574
/* 0AF08C 7F07A51C AFAA0040 */   sw    $t2, 0x40($sp)
dalton_head_set:
/* 0AF090 7F07A520 240C004E */  li    $t4, 78
/* 0AF094 7F07A524 10000013 */  b     .Ljp7F07A574
/* 0AF098 7F07A528 AFAC0040 */   sw    $t4, 0x40($sp)
variable_body_head:
/* 0AF09C 7F07A52C 2419004E */  li    $t9, 78
/* 0AF0A0 7F07A530 10000010 */  b     .Ljp7F07A574
/* 0AF0A4 7F07A534 AFB90040 */   sw    $t9, 0x40($sp)
.Ljp7F07A538:
/* 0AF0A8 7F07A538 1000000E */  b     .Ljp7F07A574
/* 0AF0AC 7F07A53C AFB80040 */   sw    $t8, 0x40($sp)
.Ljp7F07A540:
/* 0AF0B0 7F07A540 1000000C */  b     .Ljp7F07A574
/* 0AF0B4 7F07A544 AFAD0040 */   sw    $t5, 0x40($sp)
.Ljp7F07A548:
/* 0AF0B8 7F07A548 1000000A */  b     .Ljp7F07A574
/* 0AF0BC 7F07A54C AFAE0040 */   sw    $t6, 0x40($sp)
.Ljp7F07A550:
/* 0AF0C0 7F07A550 0FC26F3C */  jal   get_cur_playernum
/* 0AF0C4 7F07A554 00000000 */   nop
/* 0AF0C8 7F07A558 0FC040BF */  jal   get_player_mp_char_head
/* 0AF0CC 7F07A55C 00402025 */   move  $a0, $v0
/* 0AF0D0 7F07A560 0FC26F3C */  jal   get_cur_playernum
/* 0AF0D4 7F07A564 AFA20040 */   sw    $v0, 0x40($sp)
/* 0AF0D8 7F07A568 0FC040E7 */  jal   get_player_mp_char_body
/* 0AF0DC 7F07A56C 00402025 */   move  $a0, $v0
/* 0AF0E0 7F07A570 AFA20044 */  sw    $v0, 0x44($sp)
.Ljp7F07A574:
/* 0AF0E4 7F07A574 3C0F8003 */  lui   $t7, %hi(g_CameraMode) # $t7, 0x8003
/* 0AF0E8 7F07A578 8DEF64D4 */  lw    $t7, %lo(g_CameraMode)($t7)
/* 0AF0EC 7F07A57C 24010003 */  li    $at, 3
/* 0AF0F0 7F07A580 15E10003 */  bne   $t7, $at, .Ljp7F07A590
/* 0AF0F4 7F07A584 3C0B8008 */   lui   $t3, %hi(starting_weapon) # $t3, 0x8008
/* 0AF0F8 7F07A588 8D6B9A20 */  lw    $t3, %lo(starting_weapon)($t3)
/* 0AF0FC 7F07A58C AFAB0048 */  sw    $t3, 0x48($sp)
.Ljp7F07A590:
/* 0AF100 7F07A590 0FC26C01 */  jal   getPlayerCount
/* 0AF104 7F07A594 00000000 */   nop
/* 0AF108 7F07A598 24010001 */  li    $at, 1
/* 0AF10C 7F07A59C 14410082 */  bne   $v0, $at, .Ljp7F07A7A8
/* 0AF110 7F07A5A0 8FAF0044 */   lw    $t7, 0x44($sp)
/* 0AF114 7F07A5A4 0FC1751F */  jal   remove_item_in_hand
/* 0AF118 7F07A5A8 24040001 */   li    $a0, 1
/* 0AF11C 7F07A5AC 0FC1751F */  jal   remove_item_in_hand
/* 0AF120 7F07A5B0 00002025 */   move  $a0, $zero
/* 0AF124 7F07A5B4 27A40100 */  addiu $a0, $sp, 0x100
/* 0AF128 7F07A5B8 8FA500EC */  lw    $a1, 0xec($sp)
/* 0AF12C 7F07A5BC 0FC331F5 */  jal   texInitPool
/* 0AF130 7F07A5C0 8FA600E0 */   lw    $a2, 0xe0($sp)
/* 0AF134 7F07A5C4 0FC1750E */  jal   get_ptr_itemheader_in_hand
/* 0AF138 7F07A5C8 00002025 */   move  $a0, $zero
/* 0AF13C 7F07A5CC 8FAA0044 */  lw    $t2, 0x44($sp)
/* 0AF140 7F07A5D0 3C198004 */  lui   $t9, %hi(c_item_entries) # $t9, 0x8004
/* 0AF144 7F07A5D4 2739DE40 */  addiu $t9, %lo(c_item_entries) # addiu $t9, $t9, -0x21c0
/* 0AF148 7F07A5D8 000A6080 */  sll   $t4, $t2, 2
/* 0AF14C 7F07A5DC 018A6021 */  addu  $t4, $t4, $t2
/* 0AF150 7F07A5E0 000C6080 */  sll   $t4, $t4, 2
/* 0AF154 7F07A5E4 01991821 */  addu  $v1, $t4, $t9
/* 0AF158 7F07A5E8 8C780000 */  lw    $t8, ($v1)
/* 0AF15C 7F07A5EC AFA200FC */  sw    $v0, 0xfc($sp)
/* 0AF160 7F07A5F0 27AB0100 */  addiu $t3, $sp, 0x100
/* 0AF164 7F07A5F4 8F010000 */  lw    $at, ($t8)
/* 0AF168 7F07A5F8 00402025 */  move  $a0, $v0
/* 0AF16C 7F07A5FC AC410000 */  sw    $at, ($v0)
/* 0AF170 7F07A600 8F0F0004 */  lw    $t7, 4($t8)
/* 0AF174 7F07A604 AC4F0004 */  sw    $t7, 4($v0)
/* 0AF178 7F07A608 8F010008 */  lw    $at, 8($t8)
/* 0AF17C 7F07A60C AC410008 */  sw    $at, 8($v0)
/* 0AF180 7F07A610 8F0F000C */  lw    $t7, 0xc($t8)
/* 0AF184 7F07A614 AC4F000C */  sw    $t7, 0xc($v0)
/* 0AF188 7F07A618 8F010010 */  lw    $at, 0x10($t8)
/* 0AF18C 7F07A61C AC410010 */  sw    $at, 0x10($v0)
/* 0AF190 7F07A620 8F0F0014 */  lw    $t7, 0x14($t8)
/* 0AF194 7F07A624 AC4F0014 */  sw    $t7, 0x14($v0)
/* 0AF198 7F07A628 8F010018 */  lw    $at, 0x18($t8)
/* 0AF19C 7F07A62C AC410018 */  sw    $at, 0x18($v0)
/* 0AF1A0 7F07A630 8F0F001C */  lw    $t7, 0x1c($t8)
/* 0AF1A4 7F07A634 AC4F001C */  sw    $t7, 0x1c($v0)
/* 0AF1A8 7F07A638 AFA30028 */  sw    $v1, 0x28($sp)
/* 0AF1AC 7F07A63C AFAB0010 */  sw    $t3, 0x10($sp)
/* 0AF1B0 7F07A640 8FA700E4 */  lw    $a3, 0xe4($sp)
/* 0AF1B4 7F07A644 8FA600F0 */  lw    $a2, 0xf0($sp)
/* 0AF1B8 7F07A648 0FC1DAA5 */  jal   load_object_fill_header
/* 0AF1BC 7F07A64C 8C650004 */   lw    $a1, 4($v1)
/* 0AF1C0 7F07A650 8FA30028 */  lw    $v1, 0x28($sp)
/* 0AF1C4 7F07A654 0FC2F74E */  jal   get_pc_buffer_remaining_value
/* 0AF1C8 7F07A658 8C640004 */   lw    $a0, 4($v1)
/* 0AF1CC 7F07A65C 8FAD0040 */  lw    $t5, 0x40($sp)
/* 0AF1D0 7F07A660 3C0F8004 */  lui   $t7, %hi(c_item_entries) # $t7, 0x8004
/* 0AF1D4 7F07A664 25EFDE40 */  addiu $t7, %lo(c_item_entries) # addiu $t7, $t7, -0x21c0
/* 0AF1D8 7F07A668 000DC080 */  sll   $t8, $t5, 2
/* 0AF1DC 7F07A66C 030DC021 */  addu  $t8, $t8, $t5
/* 0AF1E0 7F07A670 0018C080 */  sll   $t8, $t8, 2
/* 0AF1E4 7F07A674 030F1821 */  addu  $v1, $t8, $t7
/* 0AF1E8 7F07A678 8C6B0000 */  lw    $t3, ($v1)
/* 0AF1EC 7F07A67C 2448003F */  addiu $t0, $v0, 0x3f
/* 0AF1F0 7F07A680 8FA900F0 */  lw    $t1, 0xf0($sp)
/* 0AF1F4 7F07A684 350A003F */  ori   $t2, $t0, 0x3f
/* 0AF1F8 7F07A688 8D610000 */  lw    $at, ($t3)
/* 0AF1FC 7F07A68C 394C003F */  xori  $t4, $t2, 0x3f
/* 0AF200 7F07A690 2588005F */  addiu $t0, $t4, 0x5f
/* 0AF204 7F07A694 3519003F */  ori   $t9, $t0, 0x3f
/* 0AF208 7F07A698 012C2021 */  addu  $a0, $t1, $t4
/* 0AF20C 7F07A69C 3B28003F */  xori  $t0, $t9, 0x3f
/* 0AF210 7F07A6A0 AC810000 */  sw    $at, ($a0)
/* 0AF214 7F07A6A4 8D790004 */  lw    $t9, 4($t3)
/* 0AF218 7F07A6A8 27AD0100 */  addiu $t5, $sp, 0x100
/* 0AF21C 7F07A6AC 01283021 */  addu  $a2, $t1, $t0
/* 0AF220 7F07A6B0 AC990004 */  sw    $t9, 4($a0)
/* 0AF224 7F07A6B4 8D610008 */  lw    $at, 8($t3)
/* 0AF228 7F07A6B8 AC810008 */  sw    $at, 8($a0)
/* 0AF22C 7F07A6BC 8D79000C */  lw    $t9, 0xc($t3)
/* 0AF230 7F07A6C0 AC99000C */  sw    $t9, 0xc($a0)
/* 0AF234 7F07A6C4 8D610010 */  lw    $at, 0x10($t3)
/* 0AF238 7F07A6C8 AC810010 */  sw    $at, 0x10($a0)
/* 0AF23C 7F07A6CC 8D790014 */  lw    $t9, 0x14($t3)
/* 0AF240 7F07A6D0 AC990014 */  sw    $t9, 0x14($a0)
/* 0AF244 7F07A6D4 8D610018 */  lw    $at, 0x18($t3)
/* 0AF248 7F07A6D8 AC810018 */  sw    $at, 0x18($a0)
/* 0AF24C 7F07A6DC 8D79001C */  lw    $t9, 0x1c($t3)
/* 0AF250 7F07A6E0 AC99001C */  sw    $t9, 0x1c($a0)
/* 0AF254 7F07A6E4 8FAE00E4 */  lw    $t6, 0xe4($sp)
/* 0AF258 7F07A6E8 AFA800E8 */  sw    $t0, 0xe8($sp)
/* 0AF25C 7F07A6EC AFA400F8 */  sw    $a0, 0xf8($sp)
/* 0AF260 7F07A6F0 AFA30028 */  sw    $v1, 0x28($sp)
/* 0AF264 7F07A6F4 AFAD0010 */  sw    $t5, 0x10($sp)
/* 0AF268 7F07A6F8 8C650004 */  lw    $a1, 4($v1)
/* 0AF26C 7F07A6FC 0FC1DAA5 */  jal   load_object_fill_header
/* 0AF270 7F07A700 01C83823 */   subu  $a3, $t6, $t0
/* 0AF274 7F07A704 8FA30028 */  lw    $v1, 0x28($sp)
/* 0AF278 7F07A708 0FC2F74E */  jal   get_pc_buffer_remaining_value
/* 0AF27C 7F07A70C 8C640004 */   lw    $a0, 4($v1)
/* 0AF280 7F07A710 8FA300E8 */  lw    $v1, 0xe8($sp)
/* 0AF284 7F07A714 8FAC00F0 */  lw    $t4, 0xf0($sp)
/* 0AF288 7F07A718 8FA400FC */  lw    $a0, 0xfc($sp)
/* 0AF28C 7F07A71C 00431821 */  addu  $v1, $v0, $v1
/* 0AF290 7F07A720 2463003F */  addiu $v1, $v1, 0x3f
/* 0AF294 7F07A724 3478003F */  ori   $t8, $v1, 0x3f
/* 0AF298 7F07A728 3B0F003F */  xori  $t7, $t8, 0x3f
/* 0AF29C 7F07A72C 25E300FB */  addiu $v1, $t7, 0xfb
/* 0AF2A0 7F07A730 346B003F */  ori   $t3, $v1, 0x3f
/* 0AF2A4 7F07A734 3979003F */  xori  $t9, $t3, 0x3f
/* 0AF2A8 7F07A738 018F5021 */  addu  $t2, $t4, $t7
/* 0AF2AC 7F07A73C AFAA0038 */  sw    $t2, 0x38($sp)
/* 0AF2B0 7F07A740 0FC1D8B9 */  jal   modelCalculateRwDataLen
/* 0AF2B4 7F07A744 AFB900E8 */   sw    $t9, 0xe8($sp)
/* 0AF2B8 7F07A748 0FC1D8B9 */  jal   modelCalculateRwDataLen
/* 0AF2BC 7F07A74C 8FA400F8 */   lw    $a0, 0xf8($sp)
/* 0AF2C0 7F07A750 8FA500FC */  lw    $a1, 0xfc($sp)
/* 0AF2C4 7F07A754 8FAF00F8 */  lw    $t7, 0xf8($sp)
/* 0AF2C8 7F07A758 8FA300E8 */  lw    $v1, 0xe8($sp)
/* 0AF2CC 7F07A75C 84B80014 */  lh    $t8, 0x14($a1)
/* 0AF2D0 7F07A760 85EC0014 */  lh    $t4, 0x14($t7)
/* 0AF2D4 7F07A764 8FAE00F0 */  lw    $t6, 0xf0($sp)
/* 0AF2D8 7F07A768 8FA40038 */  lw    $a0, 0x38($sp)
/* 0AF2DC 7F07A76C 030C1021 */  addu  $v0, $t8, $t4
/* 0AF2E0 7F07A770 2442000A */  addiu $v0, $v0, 0xa
/* 0AF2E4 7F07A774 00025080 */  sll   $t2, $v0, 2
/* 0AF2E8 7F07A778 01C33021 */  addu  $a2, $t6, $v1
/* 0AF2EC 7F07A77C 01431821 */  addu  $v1, $t2, $v1
/* 0AF2F0 7F07A780 2463003F */  addiu $v1, $v1, 0x3f
/* 0AF2F4 7F07A784 346B003F */  ori   $t3, $v1, 0x3f
/* 0AF2F8 7F07A788 3979003F */  xori  $t9, $t3, 0x3f
/* 0AF2FC 7F07A78C AFB900E8 */  sw    $t9, 0xe8($sp)
/* 0AF300 7F07A790 0FC1D967 */  jal   animInit
/* 0AF304 7F07A794 AFA20030 */   sw    $v0, 0x30($sp)
/* 0AF308 7F07A798 8FA20030 */  lw    $v0, 0x30($sp)
/* 0AF30C 7F07A79C 8FAE0038 */  lw    $t6, 0x38($sp)
/* 0AF310 7F07A7A0 10000026 */  b     .Ljp7F07A83C
/* 0AF314 7F07A7A4 A5C20002 */   sh    $v0, 2($t6)
.Ljp7F07A7A8:
/* 0AF318 7F07A7A8 000FC080 */  sll   $t8, $t7, 2
/* 0AF31C 7F07A7AC 030FC021 */  addu  $t8, $t8, $t7
/* 0AF320 7F07A7B0 3C0C8004 */  lui   $t4, %hi(c_item_entries) # $t4, 0x8004
/* 0AF324 7F07A7B4 258CDE40 */  addiu $t4, %lo(c_item_entries) # addiu $t4, $t4, -0x21c0
/* 0AF328 7F07A7B8 0018C080 */  sll   $t8, $t8, 2
/* 0AF32C 7F07A7BC 030C1821 */  addu  $v1, $t8, $t4
/* 0AF330 7F07A7C0 8C6A0000 */  lw    $t2, ($v1)
/* 0AF334 7F07A7C4 AFAA00FC */  sw    $t2, 0xfc($sp)
/* 0AF338 7F07A7C8 8D590000 */  lw    $t9, ($t2)
/* 0AF33C 7F07A7CC 01402025 */  move  $a0, $t2
/* 0AF340 7F07A7D0 57200006 */  bnezl $t9, .Ljp7F07A7EC
/* 0AF344 7F07A7D4 906D0011 */   lbu   $t5, 0x11($v1)
/* 0AF348 7F07A7D8 8C650004 */  lw    $a1, 4($v1)
/* 0AF34C 7F07A7DC 0FC1DACF */  jal   fileLoad
/* 0AF350 7F07A7E0 AFA30028 */   sw    $v1, 0x28($sp)
/* 0AF354 7F07A7E4 8FA30028 */  lw    $v1, 0x28($sp)
/* 0AF358 7F07A7E8 906D0011 */  lbu   $t5, 0x11($v1)
.Ljp7F07A7EC:
/* 0AF35C 7F07A7EC 8FAF0040 */  lw    $t7, 0x40($sp)
/* 0AF360 7F07A7F0 3C0C8004 */  lui   $t4, %hi(c_item_entries) # $t4, 0x8004
/* 0AF364 7F07A7F4 11A00005 */  beqz  $t5, .Ljp7F07A80C
/* 0AF368 7F07A7F8 000FC080 */   sll   $t8, $t7, 2
/* 0AF36C 7F07A7FC 240EFFFF */  li    $t6, -1
/* 0AF370 7F07A800 AFAE0040 */  sw    $t6, 0x40($sp)
/* 0AF374 7F07A804 1000000D */  b     .Ljp7F07A83C
/* 0AF378 7F07A808 AFA000F8 */   sw    $zero, 0xf8($sp)
.Ljp7F07A80C:
/* 0AF37C 7F07A80C 030FC021 */  addu  $t8, $t8, $t7
/* 0AF380 7F07A810 0018C080 */  sll   $t8, $t8, 2
/* 0AF384 7F07A814 258CDE40 */  addiu $t4, %lo(c_item_entries) # addiu $t4, $t4, -0x21c0
/* 0AF388 7F07A818 030C1821 */  addu  $v1, $t8, $t4
/* 0AF38C 7F07A81C 8C640000 */  lw    $a0, ($v1)
/* 0AF390 7F07A820 AFA400F8 */  sw    $a0, 0xf8($sp)
/* 0AF394 7F07A824 8C8A0000 */  lw    $t2, ($a0)
/* 0AF398 7F07A828 55400005 */  bnezl $t2, .Ljp7F07A840
/* 0AF39C 7F07A82C 8FB90038 */   lw    $t9, 0x38($sp)
/* 0AF3A0 7F07A830 8C650004 */  lw    $a1, 4($v1)
/* 0AF3A4 7F07A834 0FC1DACF */  jal   fileLoad
/* 0AF3A8 7F07A838 AFA400F8 */   sw    $a0, 0xf8($sp)
.Ljp7F07A83C:
/* 0AF3AC 7F07A83C 8FB90038 */  lw    $t9, 0x38($sp)
.Ljp7F07A840:
/* 0AF3B0 7F07A840 8FA40044 */  lw    $a0, 0x44($sp)
/* 0AF3B4 7F07A844 8FA50040 */  lw    $a1, 0x40($sp)
/* 0AF3B8 7F07A848 8FA600FC */  lw    $a2, 0xfc($sp)
/* 0AF3BC 7F07A84C 8FA700F8 */  lw    $a3, 0xf8($sp)
/* 0AF3C0 7F07A850 AFA00010 */  sw    $zero, 0x10($sp)
/* 0AF3C4 7F07A854 0FC08D76 */  jal   makeonebody
/* 0AF3C8 7F07A858 AFB90014 */   sw    $t9, 0x14($sp)
/* 0AF3CC 7F07A85C 3C038008 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8008
/* 0AF3D0 7F07A860 2463A120 */  addiu $v1, %lo(g_CurrentPlayer) # addiu $v1, $v1, -0x5ee0
/* 0AF3D4 7F07A864 8C6B0000 */  lw    $t3, ($v1)
/* 0AF3D8 7F07A868 3C018005 */  lui   $at, %hi(D_8005502C) # $at, 0x8005
/* 0AF3DC 7F07A86C AD6200D4 */  sw    $v0, 0xd4($t3)
/* 0AF3E0 7F07A870 8C6D0000 */  lw    $t5, ($v1)
/* 0AF3E4 7F07A874 C426505C */  lwc1  $f6, %lo(D_8005502C)($at)
/* 0AF3E8 7F07A878 8DA400D4 */  lw    $a0, 0xd4($t5)
/* 0AF3EC 7F07A87C C4840014 */  lwc1  $f4, 0x14($a0)
/* 0AF3F0 7F07A880 46062202 */  mul.s $f8, $f4, $f6
/* 0AF3F4 7F07A884 44054000 */  mfc1  $a1, $f8
/* 0AF3F8 7F07A888 0FC1B51A */  jal   modelSetScale
/* 0AF3FC 7F07A88C 00000000 */   nop
/* 0AF400 7F07A890 3C038008 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8008
/* 0AF404 7F07A894 8C63A120 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0AF408 7F07A898 8FA70114 */  lw    $a3, 0x114($sp)
/* 0AF40C 7F07A89C 8C6400A8 */  lw    $a0, 0xa8($v1)
/* 0AF410 7F07A8A0 8C6500D4 */  lw    $a1, 0xd4($v1)
/* 0AF414 7F07A8A4 8C8E0014 */  lw    $t6, 0x14($a0)
/* 0AF418 7F07A8A8 AFA00014 */  sw    $zero, 0x14($sp)
/* 0AF41C 7F07A8AC 24860008 */  addiu $a2, $a0, 8
/* 0AF420 7F07A8B0 0FC0809B */  jal   init_GUARDdata_with_set_values
/* 0AF424 7F07A8B4 AFAE0010 */   sw    $t6, 0x10($sp)
/* 0AF428 7F07A8B8 3C078008 */  lui   $a3, %hi(g_CurrentPlayer) # $a3, 0x8008
/* 0AF42C 7F07A8BC 24E7A120 */  addiu $a3, %lo(g_CurrentPlayer) # addiu $a3, $a3, -0x5ee0
/* 0AF430 7F07A8C0 8CF80000 */  lw    $t8, ($a3)
/* 0AF434 7F07A8C4 240F0006 */  li    $t7, 6
/* 0AF438 7F07A8C8 8F0C00A8 */  lw    $t4, 0xa8($t8)
/* 0AF43C 7F07A8CC A18F0000 */  sb    $t7, ($t4)
/* 0AF440 7F07A8D0 8CEA0000 */  lw    $t2, ($a3)
/* 0AF444 7F07A8D4 8D5900A8 */  lw    $t9, 0xa8($t2)
/* 0AF448 7F07A8D8 8F260004 */  lw    $a2, 4($t9)
/* 0AF44C 7F07A8DC 8CCB0014 */  lw    $t3, 0x14($a2)
/* 0AF450 7F07A8E0 356D0001 */  ori   $t5, $t3, 1
/* 0AF454 7F07A8E4 ACCD0014 */  sw    $t5, 0x14($a2)
/* 0AF458 7F07A8E8 8CE30000 */  lw    $v1, ($a3)
/* 0AF45C 7F07A8EC 8C6500A8 */  lw    $a1, 0xa8($v1)
/* 0AF460 7F07A8F0 8C6400D4 */  lw    $a0, 0xd4($v1)
/* 0AF464 7F07A8F4 AFA60110 */  sw    $a2, 0x110($sp)
/* 0AF468 7F07A8F8 0FC1B47F */  jal   setsuboffset
/* 0AF46C 7F07A8FC 24A50008 */   addiu $a1, $a1, 8
/* 0AF470 7F07A900 3C0E8008 */  lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8008
/* 0AF474 7F07A904 8DCEA120 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0AF478 7F07A908 8FA50114 */  lw    $a1, 0x114($sp)
/* 0AF47C 7F07A90C 0FC1B4CB */  jal   setsubroty
/* 0AF480 7F07A910 8DC400D4 */   lw    $a0, 0xd4($t6)
/* 0AF484 7F07A914 8FA20110 */  lw    $v0, 0x110($sp)
/* 0AF488 7F07A918 8FB80040 */  lw    $t8, 0x40($sp)
/* 0AF48C 7F07A91C A0580006 */  sb    $t8, 6($v0)
/* 0AF490 7F07A920 8FAF0044 */  lw    $t7, 0x44($sp)
/* 0AF494 7F07A924 A04F000F */  sb    $t7, 0xf($v0)
/* 0AF498 7F07A928 0FC26F79 */  jal   getPropForHeldItem
/* 0AF49C 7F07A92C 8FA40048 */   lw    $a0, 0x48($sp)
/* 0AF4A0 7F07A930 04400045 */  bltz  $v0, .Ljp7F07AA48
/* 0AF4A4 7F07A934 AFA2004C */   sw    $v0, 0x4c($sp)
/* 0AF4A8 7F07A938 0FC26C01 */  jal   getPlayerCount
/* 0AF4AC 7F07A93C 00000000 */   nop
/* 0AF4B0 7F07A940 24010001 */  li    $at, 1
/* 0AF4B4 7F07A944 14410035 */  bne   $v0, $at, .Ljp7F07AA1C
/* 0AF4B8 7F07A948 8FAC00F0 */   lw    $t4, 0xf0($sp)
/* 0AF4BC 7F07A94C 8FA200E8 */  lw    $v0, 0xe8($sp)
/* 0AF4C0 7F07A950 24040001 */  li    $a0, 1
/* 0AF4C4 7F07A954 01825021 */  addu  $t2, $t4, $v0
/* 0AF4C8 7F07A958 244200C7 */  addiu $v0, $v0, 0xc7
/* 0AF4CC 7F07A95C 3459003F */  ori   $t9, $v0, 0x3f
/* 0AF4D0 7F07A960 3B2B003F */  xori  $t3, $t9, 0x3f
/* 0AF4D4 7F07A964 AFAA00DC */  sw    $t2, 0xdc($sp)
/* 0AF4D8 7F07A968 0FC1750E */  jal   get_ptr_itemheader_in_hand
/* 0AF4DC 7F07A96C AFAB00E8 */   sw    $t3, 0xe8($sp)
/* 0AF4E0 7F07A970 8FAD004C */  lw    $t5, 0x4c($sp)
/* 0AF4E4 7F07A974 3C188004 */  lui   $t8, %hi(PitemZ_entries) # $t8, 0x8004
/* 0AF4E8 7F07A978 2718A258 */  addiu $t8, %lo(PitemZ_entries) # addiu $t8, $t8, -0x5da8
/* 0AF4EC 7F07A97C 000D7080 */  sll   $t6, $t5, 2
/* 0AF4F0 7F07A980 01CD7023 */  subu  $t6, $t6, $t5
/* 0AF4F4 7F07A984 000E7080 */  sll   $t6, $t6, 2
/* 0AF4F8 7F07A988 01D81821 */  addu  $v1, $t6, $t8
/* 0AF4FC 7F07A98C 8C6F0000 */  lw    $t7, ($v1)
/* 0AF500 7F07A990 AFA200F4 */  sw    $v0, 0xf4($sp)
/* 0AF504 7F07A994 27AA0100 */  addiu $t2, $sp, 0x100
/* 0AF508 7F07A998 8DE10000 */  lw    $at, ($t7)
/* 0AF50C 7F07A99C 00402025 */  move  $a0, $v0
/* 0AF510 7F07A9A0 AC410000 */  sw    $at, ($v0)
/* 0AF514 7F07A9A4 8DF90004 */  lw    $t9, 4($t7)
/* 0AF518 7F07A9A8 AC590004 */  sw    $t9, 4($v0)
/* 0AF51C 7F07A9AC 8DE10008 */  lw    $at, 8($t7)
/* 0AF520 7F07A9B0 AC410008 */  sw    $at, 8($v0)
/* 0AF524 7F07A9B4 8DF9000C */  lw    $t9, 0xc($t7)
/* 0AF528 7F07A9B8 AC59000C */  sw    $t9, 0xc($v0)
/* 0AF52C 7F07A9BC 8DE10010 */  lw    $at, 0x10($t7)
/* 0AF530 7F07A9C0 AC410010 */  sw    $at, 0x10($v0)
/* 0AF534 7F07A9C4 8DF90014 */  lw    $t9, 0x14($t7)
/* 0AF538 7F07A9C8 AC590014 */  sw    $t9, 0x14($v0)
/* 0AF53C 7F07A9CC 8DE10018 */  lw    $at, 0x18($t7)
/* 0AF540 7F07A9D0 AC410018 */  sw    $at, 0x18($v0)
/* 0AF544 7F07A9D4 8DF9001C */  lw    $t9, 0x1c($t7)
/* 0AF548 7F07A9D8 AC59001C */  sw    $t9, 0x1c($v0)
/* 0AF54C 7F07A9DC 8FAD00E8 */  lw    $t5, 0xe8($sp)
/* 0AF550 7F07A9E0 8FAE00E4 */  lw    $t6, 0xe4($sp)
/* 0AF554 7F07A9E4 8FAB00F0 */  lw    $t3, 0xf0($sp)
/* 0AF558 7F07A9E8 AFA30028 */  sw    $v1, 0x28($sp)
/* 0AF55C 7F07A9EC AFAA0010 */  sw    $t2, 0x10($sp)
/* 0AF560 7F07A9F0 8C650004 */  lw    $a1, 4($v1)
/* 0AF564 7F07A9F4 01CD3823 */  subu  $a3, $t6, $t5
/* 0AF568 7F07A9F8 0FC1DAA5 */  jal   load_object_fill_header
/* 0AF56C 7F07A9FC 016D3021 */   addu  $a2, $t3, $t5
/* 0AF570 7F07AA00 8FA30028 */  lw    $v1, 0x28($sp)
/* 0AF574 7F07AA04 0FC2F74E */  jal   get_pc_buffer_remaining_value
/* 0AF578 7F07AA08 8C640004 */   lw    $a0, 4($v1)
/* 0AF57C 7F07AA0C 0FC1D8B9 */  jal   modelCalculateRwDataLen
/* 0AF580 7F07AA10 8FA400F4 */   lw    $a0, 0xf4($sp)
/* 0AF584 7F07AA14 10000004 */  b     .Ljp7F07AA28
/* 0AF588 7F07AA18 8FAC00DC */   lw    $t4, 0xdc($sp)
.Ljp7F07AA1C:
/* 0AF58C 7F07AA1C AFA000DC */  sw    $zero, 0xdc($sp)
/* 0AF590 7F07AA20 AFA000F4 */  sw    $zero, 0xf4($sp)
/* 0AF594 7F07AA24 8FAC00DC */  lw    $t4, 0xdc($sp)
.Ljp7F07AA28:
/* 0AF598 7F07AA28 8FAF00F4 */  lw    $t7, 0xf4($sp)
/* 0AF59C 7F07AA2C 8FA40110 */  lw    $a0, 0x110($sp)
/* 0AF5A0 7F07AA30 8FA5004C */  lw    $a1, 0x4c($sp)
/* 0AF5A4 7F07AA34 8FA60048 */  lw    $a2, 0x48($sp)
/* 0AF5A8 7F07AA38 00003825 */  move  $a3, $zero
/* 0AF5AC 7F07AA3C AFAC0010 */  sw    $t4, 0x10($sp)
/* 0AF5B0 7F07AA40 0FC149C4 */  jal   something_with_generating_object
/* 0AF5B4 7F07AA44 AFAF0014 */   sw    $t7, 0x14($sp)
.Ljp7F07AA48:
/* 0AF5B8 7F07AA48 8FA40110 */  lw    $a0, 0x110($sp)
/* 0AF5BC 7F07AA4C 0FC08F65 */  jal   chrlvIdleAnimationRelated7F023A94
/* 0AF5C0 7F07AA50 24050000 */   li    $a1, 0
/* 0AF5C4 7F07AA54 10000017 */  b     .Ljp7F07AAB4
/* 0AF5C8 7F07AA58 8FBF001C */   lw    $ra, 0x1c($sp)
/* 0AF5CC 7F07AA5C 8C59001C */  lw    $t9, 0x1c($v0)
.Ljp7F07AA60:
/* 0AF5D0 7F07AA60 00402025 */  move  $a0, $v0
/* 0AF5D4 7F07AA64 8F2B0020 */  lw    $t3, 0x20($t9)
/* 0AF5D8 7F07AA68 55600012 */  bnezl $t3, .Ljp7F07AAB4
/* 0AF5DC 7F07AA6C 8FBF001C */   lw    $ra, 0x1c($sp)
/* 0AF5E0 7F07AA70 8C4D0014 */  lw    $t5, 0x14($v0)
/* 0AF5E4 7F07AA74 24050000 */  li    $a1, 0
/* 0AF5E8 7F07AA78 35AE0001 */  ori   $t6, $t5, 1
/* 0AF5EC 7F07AA7C 0FC08F65 */  jal   chrlvIdleAnimationRelated7F023A94
/* 0AF5F0 7F07AA80 AC4E0014 */   sw    $t6, 0x14($v0)
/* 0AF5F4 7F07AA84 3C038008 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8008
/* 0AF5F8 7F07AA88 8C63A120 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0AF5FC 7F07AA8C 8C6500A8 */  lw    $a1, 0xa8($v1)
/* 0AF600 7F07AA90 8C6400D4 */  lw    $a0, 0xd4($v1)
/* 0AF604 7F07AA94 0FC1B47F */  jal   setsuboffset
/* 0AF608 7F07AA98 24A50008 */   addiu $a1, $a1, 8
/* 0AF60C 7F07AA9C 3C188008 */  lui   $t8, %hi(g_CurrentPlayer) # $t8, 0x8008
/* 0AF610 7F07AAA0 8F18A120 */  lw    $t8, %lo(g_CurrentPlayer)($t8)
/* 0AF614 7F07AAA4 8FA50114 */  lw    $a1, 0x114($sp)
/* 0AF618 7F07AAA8 0FC1B4CB */  jal   setsubroty
/* 0AF61C 7F07AAAC 8F0400D4 */   lw    $a0, 0xd4($t8)
/* 0AF620 7F07AAB0 8FBF001C */  lw    $ra, 0x1c($sp)
.Ljp7F07AAB4:
/* 0AF624 7F07AAB4 27BD0118 */  addiu $sp, $sp, 0x118
/* 0AF628 7F07AAB8 03E00008 */  jr    $ra
/* 0AF62C 7F07AABC 00000000 */   nop
)
#endif

#ifdef VERSION_EU
GLOBAL_ASM(
.late_rodata
glabel D_80054FB8
.word 0
glabel D_80054FBC
.word 0

/*D:80054FC0*/
glabel a8s
/*"%8s"*/
.word 0x25387300

glabel aX4_0f
/*"x %4.0f"*/
.word 0x78202534
.word 0x2E306600

glabel aY4_0f
/*"y %4.0f"*/
.word 0x79202534
.word 0x2E306600

glabel aZ4_0f
/*"z %4.0f"*/
.word 0x7A202534
.word 0x2E306600

glabel aS3d
/*"%s %3d"*/
.word 0x25732025
.word 0x33640000
/*D:80054FE4*/
glabel jpt_bondcuff
.word .L7F079EA4
.word cuff_1_brosnan
.word cuff_2_jungle
.word cuff_3_boiler
.word cuff_4_snowsuit
.word cuff_5_connery
.word cuff_6_moore
.word cuff_7_dalton
.word cuff_8_variable_bond_head
.size jpt_bondcuff, . - jpt_bondcuff

/*D:80055008*/
glabel jpt_bondhead
.word .L7F079F84
.word brosnan_tux_head_set
.word jungle_head_set
.word boiler_bond_head_set
.word .L7F079F84
.word connery_head_set
.word moore_head_set
.word dalton_head_set
.word variable_body_head

glabel D_8005502C
.word 0x3f7851ec /*0.97000003*/
.text
glabel solo_char_load
/* 0AC760 7F079D70 27BDFEE8 */  addiu $sp, $sp, -0x118
/* 0AC764 7F079D74 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0AC768 7F079D78 0FC2282C */  jal   bondviewGetPlayerYawRadians
/* 0AC76C 7F079D7C 00000000 */   nop
/* 0AC770 7F079D80 3C0E8007 */  lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8007
/* 0AC774 7F079D84 8DCE8BC0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0AC778 7F079D88 E7A00114 */  swc1  $f0, 0x114($sp)
/* 0AC77C 7F079D8C 8DCF00A8 */  lw    $t7, 0xa8($t6)
/* 0AC780 7F079D90 8DE20004 */  lw    $v0, 4($t7)
/* 0AC784 7F079D94 544001D0 */  bnezl $v0, .L7F07A4D8
/* 0AC788 7F079D98 8C59001C */   lw    $t9, 0x1c($v0)
/* 0AC78C 7F079D9C 0FC174FA */  jal   getPlayerWeaponBufferForHand
/* 0AC790 7F079DA0 00002025 */   move  $a0, $zero
/* 0AC794 7F079DA4 AFA200F0 */  sw    $v0, 0xf0($sp)
/* 0AC798 7F079DA8 0FC174FA */  jal   getPlayerWeaponBufferForHand
/* 0AC79C 7F079DAC 24040001 */   li    $a0, 1
/* 0AC7A0 7F079DB0 AFA200EC */  sw    $v0, 0xec($sp)
/* 0AC7A4 7F079DB4 AFA000E8 */  sw    $zero, 0xe8($sp)
/* 0AC7A8 7F079DB8 0FC17500 */  jal   getSizeBufferWeaponInHand
/* 0AC7AC 7F079DBC 00002025 */   move  $a0, $zero
/* 0AC7B0 7F079DC0 AFA200E4 */  sw    $v0, 0xe4($sp)
/* 0AC7B4 7F079DC4 0FC17500 */  jal   getSizeBufferWeaponInHand
/* 0AC7B8 7F079DC8 24040001 */   li    $a0, 1
/* 0AC7BC 7F079DCC 3C198003 */  lui   $t9, %hi(dummy_08_pp7_obj) # $t9, 0x8003
/* 0AC7C0 7F079DD0 27391AFC */  addiu $t9, %lo(dummy_08_pp7_obj) # addiu $t9, $t9, 0x1afc
/* 0AC7C4 7F079DD4 AFA200E0 */  sw    $v0, 0xe0($sp)
/* 0AC7C8 7F079DD8 272C0084 */  addiu $t4, $t9, 0x84
/* 0AC7CC 7F079DDC 27B80054 */  addiu $t8, $sp, 0x54
.L7F079DE0:
/* 0AC7D0 7F079DE0 8F210000 */  lw    $at, ($t9)
/* 0AC7D4 7F079DE4 2739000C */  addiu $t9, $t9, 0xc
/* 0AC7D8 7F079DE8 2718000C */  addiu $t8, $t8, 0xc
/* 0AC7DC 7F079DEC AF01FFF4 */  sw    $at, -0xc($t8)
/* 0AC7E0 7F079DF0 8F21FFF8 */  lw    $at, -8($t9)
/* 0AC7E4 7F079DF4 AF01FFF8 */  sw    $at, -8($t8)
/* 0AC7E8 7F079DF8 8F21FFFC */  lw    $at, -4($t9)
/* 0AC7EC 7F079DFC 172CFFF8 */  bne   $t9, $t4, .L7F079DE0
/* 0AC7F0 7F079E00 AF01FFFC */   sw    $at, -4($t8)
/* 0AC7F4 7F079E04 8F210000 */  lw    $at, ($t9)
/* 0AC7F8 7F079E08 00002025 */  move  $a0, $zero
/* 0AC7FC 7F079E0C 0FC177BF */  jal   get_item_in_hand_or_watch_menu
/* 0AC800 7F079E10 AF010000 */   sw    $at, ($t8)
/* 0AC804 7F079E14 240D0017 */  li    $t5, 23
/* 0AC808 7F079E18 240E004B */  li    $t6, 75
/* 0AC80C 7F079E1C 3C048007 */  lui   $a0, %hi(g_CurrentPlayer) # $a0, 0x8007
/* 0AC810 7F079E20 AFA20048 */  sw    $v0, 0x48($sp)
/* 0AC814 7F079E24 AFAD0044 */  sw    $t5, 0x44($sp)
/* 0AC818 7F079E28 AFAE0040 */  sw    $t6, 0x40($sp)
/* 0AC81C 7F079E2C AFA00038 */  sw    $zero, 0x38($sp)
/* 0AC820 7F079E30 0FC1F7CE */  jal   sub_GAME_7F07DE64
/* 0AC824 7F079E34 8C848BC0 */   lw    $a0, %lo(g_CurrentPlayer)($a0)
/* 0AC828 7F079E38 0FC26669 */  jal   getPlayerCount
/* 0AC82C 7F079E3C 00000000 */   nop
/* 0AC830 7F079E40 24010001 */  li    $at, 1
/* 0AC834 7F079E44 14410066 */  bne   $v0, $at, .L7F079FE0
/* 0AC838 7F079E48 00000000 */   nop
/* 0AC83C 7F079E4C 0FC074F0 */  jal   fileGetBondForCurrentFolder
/* 0AC840 7F079E50 00000000 */   nop
/* 0AC844 7F079E54 3C0F8007 */  lui   $t7, %hi(g_CurrentPlayer) # $t7, 0x8007
/* 0AC848 7F079E58 8DEF8BC0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0AC84C 7F079E5C 8DE3041C */  lw    $v1, 0x41c($t7)
/* 0AC850 7F079E60 2C610009 */  sltiu $at, $v1, 9
/* 0AC854 7F079E64 1020002F */  beqz  $at, .L7F079F24
/* 0AC858 7F079E68 00035880 */   sll   $t3, $v1, 2
/* 0AC85C 7F079E6C 3C018005 */  lui   $at, %hi(jpt_bondcuff)
/* 0AC860 7F079E70 002B0821 */  addu  $at, $at, $t3
/* 0AC864 7F079E74 8C2BABB4 */  lw    $t3, %lo(jpt_bondcuff)($at)
/* 0AC868 7F079E78 01600008 */  jr    $t3
/* 0AC86C 7F079E7C 00000000 */   nop
cuff_3_boiler:
/* 0AC870 7F079E80 240A0016 */  li    $t2, 22
/* 0AC874 7F079E84 10000027 */  b     .L7F079F24
/* 0AC878 7F079E88 AFAA0044 */   sw    $t2, 0x44($sp)
cuff_2_jungle:
/* 0AC87C 7F079E8C 240C0018 */  li    $t4, 24
/* 0AC880 7F079E90 10000024 */  b     .L7F079F24
/* 0AC884 7F079E94 AFAC0044 */   sw    $t4, 0x44($sp)
cuff_4_snowsuit:
/* 0AC888 7F079E98 24190019 */  li    $t9, 25
/* 0AC88C 7F079E9C 10000021 */  b     .L7F079F24
/* 0AC890 7F079EA0 AFB90044 */   sw    $t9, 0x44($sp)
cuff_1_brosnan:
/* 0AC894 7F079EA4 24180005 */  li    $t8, 5
/* 0AC898 7F079EA8 1000001E */  b     .L7F079F24
/* 0AC89C 7F079EAC AFB80044 */   sw    $t8, 0x44($sp)
cuff_5_connery:
/* 0AC8A0 7F079EB0 240D0005 */  li    $t5, 5
/* 0AC8A4 7F079EB4 1000001B */  b     .L7F079F24
/* 0AC8A8 7F079EB8 AFAD0044 */   sw    $t5, 0x44($sp)
cuff_6_moore:
/* 0AC8AC 7F079EBC 240E0005 */  li    $t6, 5
/* 0AC8B0 7F079EC0 10000018 */  b     .L7F079F24
/* 0AC8B4 7F079EC4 AFAE0044 */   sw    $t6, 0x44($sp)
cuff_7_dalton:
/* 0AC8B8 7F079EC8 240F0005 */  li    $t7, 5
/* 0AC8BC 7F079ECC 10000015 */  b     .L7F079F24
/* 0AC8C0 7F079ED0 AFAF0044 */   sw    $t7, 0x44($sp)
cuff_8_variable_bond_head:
/* 0AC8C4 7F079ED4 1040000C */  beqz  $v0, .L7F079F08
/* 0AC8C8 7F079ED8 240B0005 */   li    $t3, 5
/* 0AC8CC 7F079EDC 24010001 */  li    $at, 1
/* 0AC8D0 7F079EE0 1041000B */  beq   $v0, $at, .L7F079F10
/* 0AC8D4 7F079EE4 240A0005 */   li    $t2, 5
/* 0AC8D8 7F079EE8 24010002 */  li    $at, 2
/* 0AC8DC 7F079EEC 1041000A */  beq   $v0, $at, .L7F079F18
/* 0AC8E0 7F079EF0 240C0005 */   li    $t4, 5
/* 0AC8E4 7F079EF4 24010003 */  li    $at, 3
/* 0AC8E8 7F079EF8 10410009 */  beq   $v0, $at, .L7F079F20
/* 0AC8EC 7F079EFC 24190005 */   li    $t9, 5
/* 0AC8F0 7F079F00 10000008 */  b     .L7F079F24
/* 0AC8F4 7F079F04 00000000 */   nop
.L7F079F08:
/* 0AC8F8 7F079F08 10000006 */  b     .L7F079F24
/* 0AC8FC 7F079F0C AFAB0044 */   sw    $t3, 0x44($sp)
.L7F079F10:
/* 0AC900 7F079F10 10000004 */  b     .L7F079F24
/* 0AC904 7F079F14 AFAA0044 */   sw    $t2, 0x44($sp)
.L7F079F18:
/* 0AC908 7F079F18 10000002 */  b     .L7F079F24
/* 0AC90C 7F079F1C AFAC0044 */   sw    $t4, 0x44($sp)
.L7F079F20:
/* 0AC910 7F079F20 AFB90044 */  sw    $t9, 0x44($sp)
.L7F079EA4:
.L7F079F24:
/* 0AC914 7F079F24 1040000C */  beqz  $v0, .L7F079F58
/* 0AC918 7F079F28 2C610009 */   sltiu $at, $v1, 9
/* 0AC91C 7F079F2C 24010001 */  li    $at, 1
/* 0AC920 7F079F30 10410025 */  beq   $v0, $at, .L7F079FC8
/* 0AC924 7F079F34 2418004E */   li    $t8, 78
/* 0AC928 7F079F38 24010002 */  li    $at, 2
/* 0AC92C 7F079F3C 10410024 */  beq   $v0, $at, .L7F079FD0
/* 0AC930 7F079F40 240D004E */   li    $t5, 78
/* 0AC934 7F079F44 24010003 */  li    $at, 3
/* 0AC938 7F079F48 10410023 */  beq   $v0, $at, .L7F079FD8
/* 0AC93C 7F079F4C 240E004E */   li    $t6, 78
/* 0AC940 7F079F50 1000002C */  b     .L7F07A004
/* 0AC944 7F079F54 00000000 */   nop
.L7F079F58:
/* 0AC948 7F079F58 1020002A */  beqz  $at, .L7F07A004
/* 0AC94C 7F079F5C 0003C080 */   sll   $t8, $v1, 2
/* 0AC950 7F079F60 3C018005 */  lui   $at, %hi(jpt_bondhead)
/* 0AC954 7F079F64 00380821 */  addu  $at, $at, $t8
/* 0AC958 7F079F68 8C38ABD8 */  lw    $t8, %lo(jpt_bondhead)($at)
/* 0AC95C 7F079F6C 03000008 */  jr    $t8
/* 0AC960 7F079F70 00000000 */   nop
boiler_bond_head_set:
/* 0AC964 7F079F74 240D004A */  li    $t5, 74
/* 0AC968 7F079F78 10000022 */  b     .L7F07A004
/* 0AC96C 7F079F7C AFAD0040 */   sw    $t5, 0x40($sp)
jungle_head_set:
/* 0AC970 7F079F80 240E004C */  li    $t6, 76
/* 0AC974 7F079F84 1000001F */  b     .L7F07A004
/* 0AC978 7F079F88 AFAE0040 */   sw    $t6, 0x40($sp)
brosnan_tux_head_set:
/* 0AC97C 7F079F8C 240F004E */  li    $t7, 78
/* 0AC980 7F079F90 1000001C */  b     .L7F07A004
/* 0AC984 7F079F94 AFAF0040 */   sw    $t7, 0x40($sp)
connery_head_set:
/* 0AC988 7F079F98 240B004E */  li    $t3, 78
/* 0AC98C 7F079F9C 10000019 */  b     .L7F07A004
/* 0AC990 7F079FA0 AFAB0040 */   sw    $t3, 0x40($sp)
moore_head_set:
/* 0AC994 7F079FA4 240A004E */  li    $t2, 78
/* 0AC998 7F079FA8 10000016 */  b     .L7F07A004
/* 0AC99C 7F079FAC AFAA0040 */   sw    $t2, 0x40($sp)
dalton_head_set:
/* 0AC9A0 7F079FB0 240C004E */  li    $t4, 78
/* 0AC9A4 7F079FB4 10000013 */  b     .L7F07A004
/* 0AC9A8 7F079FB8 AFAC0040 */   sw    $t4, 0x40($sp)
variable_body_head:
/* 0AC9AC 7F079FBC 2419004E */  li    $t9, 78
/* 0AC9B0 7F079FC0 10000010 */  b     .L7F07A004
/* 0AC9B4 7F079FC4 AFB90040 */   sw    $t9, 0x40($sp)
.L7F079FC8:
/* 0AC9B8 7F079FC8 1000000E */  b     .L7F07A004
/* 0AC9BC 7F079FCC AFB80040 */   sw    $t8, 0x40($sp)
.L7F079FD0:
/* 0AC9C0 7F079FD0 1000000C */  b     .L7F07A004
/* 0AC9C4 7F079FD4 AFAD0040 */   sw    $t5, 0x40($sp)
.L7F079FD8:
/* 0AC9C8 7F079FD8 1000000A */  b     .L7F07A004
/* 0AC9CC 7F079FDC AFAE0040 */   sw    $t6, 0x40($sp)
.L7F079FE0:
/* 0AC9D0 7F079FE0 0FC269A4 */  jal   get_cur_playernum
/* 0AC9D4 7F079FE4 00000000 */   nop
/* 0AC9D8 7F079FE8 0FC0406B */  jal   get_player_mp_char_head
/* 0AC9DC 7F079FEC 00402025 */   move  $a0, $v0
/* 0AC9E0 7F079FF0 0FC269A4 */  jal   get_cur_playernum
/* 0AC9E4 7F079FF4 AFA20040 */   sw    $v0, 0x40($sp)
/* 0AC9E8 7F079FF8 0FC04093 */  jal   get_player_mp_char_body
/* 0AC9EC 7F079FFC 00402025 */   move  $a0, $v0
/* 0AC9F0 7F07A000 AFA20044 */  sw    $v0, 0x44($sp)
.L7F079F84:
.L7F07A004:
/* 0AC9F4 7F07A004 3C0F8003 */  lui   $t7, %hi(g_CameraMode) # $t7, 0x8003
/* 0AC9F8 7F07A008 8DEF19E4 */  lw    $t7, %lo(g_CameraMode)($t7)
/* 0AC9FC 7F07A00C 24010003 */  li    $at, 3
/* 0ACA00 7F07A010 15E10003 */  bne   $t7, $at, .L7F07A020
/* 0ACA04 7F07A014 3C0B8007 */   lui   $t3, %hi(starting_weapon) # $t3, 0x8007
/* 0ACA08 7F07A018 8D6B84C0 */  lw    $t3, %lo(starting_weapon)($t3)
/* 0ACA0C 7F07A01C AFAB0048 */  sw    $t3, 0x48($sp)
.L7F07A020:
/* 0ACA10 7F07A020 0FC26669 */  jal   getPlayerCount
/* 0ACA14 7F07A024 00000000 */   nop
/* 0ACA18 7F07A028 24010001 */  li    $at, 1
/* 0ACA1C 7F07A02C 1441007E */  bne   $v0, $at, .L7F07A228
/* 0ACA20 7F07A030 8FAF0044 */   lw    $t7, 0x44($sp)
/* 0ACA24 7F07A034 0FC17505 */  jal   remove_item_in_hand
/* 0ACA28 7F07A038 24040001 */   li    $a0, 1
/* 0ACA2C 7F07A03C 0FC17505 */  jal   remove_item_in_hand
/* 0ACA30 7F07A040 00002025 */   move  $a0, $zero
/* 0ACA34 7F07A044 27A40100 */  addiu $a0, $sp, 0x100
/* 0ACA38 7F07A048 8FA500EC */  lw    $a1, 0xec($sp)
/* 0ACA3C 7F07A04C 0FC32C05 */  jal   texInitPool
/* 0ACA40 7F07A050 8FA600E0 */   lw    $a2, 0xe0($sp)
/* 0ACA44 7F07A054 0FC174F2 */  jal   get_ptr_itemheader_in_hand
/* 0ACA48 7F07A058 00002025 */   move  $a0, $zero
/* 0ACA4C 7F07A05C 8FAA0044 */  lw    $t2, 0x44($sp)
/* 0ACA50 7F07A060 3C198003 */  lui   $t9, %hi(c_item_entries) # $t9, 0x8003
/* 0ACA54 7F07A064 27397A60 */  addiu $t9, %lo(c_item_entries) # addiu $t9, $t9, 0x7a60
/* 0ACA58 7F07A068 000A6080 */  sll   $t4, $t2, 2
/* 0ACA5C 7F07A06C 018A6021 */  addu  $t4, $t4, $t2
/* 0ACA60 7F07A070 000C6080 */  sll   $t4, $t4, 2
/* 0ACA64 7F07A074 01991821 */  addu  $v1, $t4, $t9
/* 0ACA68 7F07A078 8C780000 */  lw    $t8, ($v1)
/* 0ACA6C 7F07A07C AFA200FC */  sw    $v0, 0xfc($sp)
/* 0ACA70 7F07A080 27AB0100 */  addiu $t3, $sp, 0x100
/* 0ACA74 7F07A084 8F010000 */  lw    $at, ($t8)
/* 0ACA78 7F07A088 00402025 */  move  $a0, $v0
/* 0ACA7C 7F07A08C AC410000 */  sw    $at, ($v0)
/* 0ACA80 7F07A090 8F0F0004 */  lw    $t7, 4($t8)
/* 0ACA84 7F07A094 AC4F0004 */  sw    $t7, 4($v0)
/* 0ACA88 7F07A098 8F010008 */  lw    $at, 8($t8)
/* 0ACA8C 7F07A09C AC410008 */  sw    $at, 8($v0)
/* 0ACA90 7F07A0A0 8F0F000C */  lw    $t7, 0xc($t8)
/* 0ACA94 7F07A0A4 AC4F000C */  sw    $t7, 0xc($v0)
/* 0ACA98 7F07A0A8 8F010010 */  lw    $at, 0x10($t8)
/* 0ACA9C 7F07A0AC AC410010 */  sw    $at, 0x10($v0)
/* 0ACAA0 7F07A0B0 8F0F0014 */  lw    $t7, 0x14($t8)
/* 0ACAA4 7F07A0B4 AC4F0014 */  sw    $t7, 0x14($v0)
/* 0ACAA8 7F07A0B8 8F010018 */  lw    $at, 0x18($t8)
/* 0ACAAC 7F07A0BC AC410018 */  sw    $at, 0x18($v0)
/* 0ACAB0 7F07A0C0 AFA30028 */  sw    $v1, 0x28($sp)
/* 0ACAB4 7F07A0C4 AFAB0010 */  sw    $t3, 0x10($sp)
/* 0ACAB8 7F07A0C8 8FA700E4 */  lw    $a3, 0xe4($sp)
/* 0ACABC 7F07A0CC 8FA600F0 */  lw    $a2, 0xf0($sp)
/* 0ACAC0 7F07A0D0 0FC1D949 */  jal   load_object_fill_header
/* 0ACAC4 7F07A0D4 8C650004 */   lw    $a1, 4($v1)
/* 0ACAC8 7F07A0D8 8FA30028 */  lw    $v1, 0x28($sp)
/* 0ACACC 7F07A0DC 0FC2F150 */  jal   get_pc_buffer_remaining_value
/* 0ACAD0 7F07A0E0 8C640004 */   lw    $a0, 4($v1)
/* 0ACAD4 7F07A0E4 8FAD0040 */  lw    $t5, 0x40($sp)
/* 0ACAD8 7F07A0E8 3C0F8003 */  lui   $t7, %hi(c_item_entries) # $t7, 0x8003
/* 0ACADC 7F07A0EC 25EF7A60 */  addiu $t7, %lo(c_item_entries) # addiu $t7, $t7, 0x7a60
/* 0ACAE0 7F07A0F0 000DC080 */  sll   $t8, $t5, 2
/* 0ACAE4 7F07A0F4 030DC021 */  addu  $t8, $t8, $t5
/* 0ACAE8 7F07A0F8 0018C080 */  sll   $t8, $t8, 2
/* 0ACAEC 7F07A0FC 030F1821 */  addu  $v1, $t8, $t7
/* 0ACAF0 7F07A100 8C6B0000 */  lw    $t3, ($v1)
/* 0ACAF4 7F07A104 2448003F */  addiu $t0, $v0, 0x3f
/* 0ACAF8 7F07A108 8FA900F0 */  lw    $t1, 0xf0($sp)
/* 0ACAFC 7F07A10C 350A003F */  ori   $t2, $t0, 0x3f
/* 0ACB00 7F07A110 8D610000 */  lw    $at, ($t3)
/* 0ACB04 7F07A114 394C003F */  xori  $t4, $t2, 0x3f
/* 0ACB08 7F07A118 2588005B */  addiu $t0, $t4, 0x5b
/* 0ACB0C 7F07A11C 3519003F */  ori   $t9, $t0, 0x3f
/* 0ACB10 7F07A120 012C2021 */  addu  $a0, $t1, $t4
/* 0ACB14 7F07A124 3B28003F */  xori  $t0, $t9, 0x3f
/* 0ACB18 7F07A128 AC810000 */  sw    $at, ($a0)
/* 0ACB1C 7F07A12C 8D790004 */  lw    $t9, 4($t3)
/* 0ACB20 7F07A130 27AD0100 */  addiu $t5, $sp, 0x100
/* 0ACB24 7F07A134 01283021 */  addu  $a2, $t1, $t0
/* 0ACB28 7F07A138 AC990004 */  sw    $t9, 4($a0)
/* 0ACB2C 7F07A13C 8D610008 */  lw    $at, 8($t3)
/* 0ACB30 7F07A140 AC810008 */  sw    $at, 8($a0)
/* 0ACB34 7F07A144 8D79000C */  lw    $t9, 0xc($t3)
/* 0ACB38 7F07A148 AC99000C */  sw    $t9, 0xc($a0)
/* 0ACB3C 7F07A14C 8D610010 */  lw    $at, 0x10($t3)
/* 0ACB40 7F07A150 AC810010 */  sw    $at, 0x10($a0)
/* 0ACB44 7F07A154 8D790014 */  lw    $t9, 0x14($t3)
/* 0ACB48 7F07A158 AC990014 */  sw    $t9, 0x14($a0)
/* 0ACB4C 7F07A15C 8D610018 */  lw    $at, 0x18($t3)
/* 0ACB50 7F07A160 AC810018 */  sw    $at, 0x18($a0)
/* 0ACB54 7F07A164 8FAE00E4 */  lw    $t6, 0xe4($sp)
/* 0ACB58 7F07A168 AFA800E8 */  sw    $t0, 0xe8($sp)
/* 0ACB5C 7F07A16C AFA400F8 */  sw    $a0, 0xf8($sp)
/* 0ACB60 7F07A170 AFA30028 */  sw    $v1, 0x28($sp)
/* 0ACB64 7F07A174 AFAD0010 */  sw    $t5, 0x10($sp)
/* 0ACB68 7F07A178 8C650004 */  lw    $a1, 4($v1)
/* 0ACB6C 7F07A17C 0FC1D949 */  jal   load_object_fill_header
/* 0ACB70 7F07A180 01C83823 */   subu  $a3, $t6, $t0
/* 0ACB74 7F07A184 8FA30028 */  lw    $v1, 0x28($sp)
/* 0ACB78 7F07A188 0FC2F150 */  jal   get_pc_buffer_remaining_value
/* 0ACB7C 7F07A18C 8C640004 */   lw    $a0, 4($v1)
/* 0ACB80 7F07A190 8FA300E8 */  lw    $v1, 0xe8($sp)
/* 0ACB84 7F07A194 8FAC00F0 */  lw    $t4, 0xf0($sp)
/* 0ACB88 7F07A198 8FA400FC */  lw    $a0, 0xfc($sp)
/* 0ACB8C 7F07A19C 00431821 */  addu  $v1, $v0, $v1
/* 0ACB90 7F07A1A0 2463003F */  addiu $v1, $v1, 0x3f
/* 0ACB94 7F07A1A4 3478003F */  ori   $t8, $v1, 0x3f
/* 0ACB98 7F07A1A8 3B0F003F */  xori  $t7, $t8, 0x3f
/* 0ACB9C 7F07A1AC 25E300FB */  addiu $v1, $t7, 0xfb
/* 0ACBA0 7F07A1B0 346B003F */  ori   $t3, $v1, 0x3f
/* 0ACBA4 7F07A1B4 3979003F */  xori  $t9, $t3, 0x3f
/* 0ACBA8 7F07A1B8 018F5021 */  addu  $t2, $t4, $t7
/* 0ACBAC 7F07A1BC AFAA0038 */  sw    $t2, 0x38($sp)
/* 0ACBB0 7F07A1C0 0FC1D75F */  jal   modelCalculateRwDataLen
/* 0ACBB4 7F07A1C4 AFB900E8 */   sw    $t9, 0xe8($sp)
/* 0ACBB8 7F07A1C8 0FC1D75F */  jal   modelCalculateRwDataLen
/* 0ACBBC 7F07A1CC 8FA400F8 */   lw    $a0, 0xf8($sp)
/* 0ACBC0 7F07A1D0 8FA500FC */  lw    $a1, 0xfc($sp)
/* 0ACBC4 7F07A1D4 8FAF00F8 */  lw    $t7, 0xf8($sp)
/* 0ACBC8 7F07A1D8 8FA300E8 */  lw    $v1, 0xe8($sp)
/* 0ACBCC 7F07A1DC 84B80014 */  lh    $t8, 0x14($a1)
/* 0ACBD0 7F07A1E0 85EC0014 */  lh    $t4, 0x14($t7)
/* 0ACBD4 7F07A1E4 8FAE00F0 */  lw    $t6, 0xf0($sp)
/* 0ACBD8 7F07A1E8 8FA40038 */  lw    $a0, 0x38($sp)
/* 0ACBDC 7F07A1EC 030C1021 */  addu  $v0, $t8, $t4
/* 0ACBE0 7F07A1F0 2442000A */  addiu $v0, $v0, 0xa
/* 0ACBE4 7F07A1F4 00025080 */  sll   $t2, $v0, 2
/* 0ACBE8 7F07A1F8 01C33021 */  addu  $a2, $t6, $v1
/* 0ACBEC 7F07A1FC 01431821 */  addu  $v1, $t2, $v1
/* 0ACBF0 7F07A200 2463003F */  addiu $v1, $v1, 0x3f
/* 0ACBF4 7F07A204 346B003F */  ori   $t3, $v1, 0x3f
/* 0ACBF8 7F07A208 3979003F */  xori  $t9, $t3, 0x3f
/* 0ACBFC 7F07A20C AFB900E8 */  sw    $t9, 0xe8($sp)
/* 0ACC00 7F07A210 0FC1D80A */  jal   animInit
/* 0ACC04 7F07A214 AFA20030 */   sw    $v0, 0x30($sp)
/* 0ACC08 7F07A218 8FA20030 */  lw    $v0, 0x30($sp)
/* 0ACC0C 7F07A21C 8FAE0038 */  lw    $t6, 0x38($sp)
/* 0ACC10 7F07A220 10000026 */  b     .L7F07A2BC
/* 0ACC14 7F07A224 A5C20002 */   sh    $v0, 2($t6)
.L7F07A228:
/* 0ACC18 7F07A228 000FC080 */  sll   $t8, $t7, 2
/* 0ACC1C 7F07A22C 030FC021 */  addu  $t8, $t8, $t7
/* 0ACC20 7F07A230 3C0C8003 */  lui   $t4, %hi(c_item_entries) # $t4, 0x8003
/* 0ACC24 7F07A234 258C7A60 */  addiu $t4, %lo(c_item_entries) # addiu $t4, $t4, 0x7a60
/* 0ACC28 7F07A238 0018C080 */  sll   $t8, $t8, 2
/* 0ACC2C 7F07A23C 030C1821 */  addu  $v1, $t8, $t4
/* 0ACC30 7F07A240 8C6A0000 */  lw    $t2, ($v1)
/* 0ACC34 7F07A244 AFAA00FC */  sw    $t2, 0xfc($sp)
/* 0ACC38 7F07A248 8D590000 */  lw    $t9, ($t2)
/* 0ACC3C 7F07A24C 01402025 */  move  $a0, $t2
/* 0ACC40 7F07A250 57200006 */  bnezl $t9, .L7F07A26C
/* 0ACC44 7F07A254 906D0011 */   lbu   $t5, 0x11($v1)
/* 0ACC48 7F07A258 8C650004 */  lw    $a1, 4($v1)
/* 0ACC4C 7F07A25C 0FC1D973 */  jal   fileLoad
/* 0ACC50 7F07A260 AFA30028 */   sw    $v1, 0x28($sp)
/* 0ACC54 7F07A264 8FA30028 */  lw    $v1, 0x28($sp)
/* 0ACC58 7F07A268 906D0011 */  lbu   $t5, 0x11($v1)
.L7F07A26C:
/* 0ACC5C 7F07A26C 8FAF0040 */  lw    $t7, 0x40($sp)
/* 0ACC60 7F07A270 3C0C8003 */  lui   $t4, %hi(c_item_entries) # $t4, 0x8003
/* 0ACC64 7F07A274 11A00005 */  beqz  $t5, .L7F07A28C
/* 0ACC68 7F07A278 000FC080 */   sll   $t8, $t7, 2
/* 0ACC6C 7F07A27C 240EFFFF */  li    $t6, -1
/* 0ACC70 7F07A280 AFAE0040 */  sw    $t6, 0x40($sp)
/* 0ACC74 7F07A284 1000000D */  b     .L7F07A2BC
/* 0ACC78 7F07A288 AFA000F8 */   sw    $zero, 0xf8($sp)
.L7F07A28C:
/* 0ACC7C 7F07A28C 030FC021 */  addu  $t8, $t8, $t7
/* 0ACC80 7F07A290 0018C080 */  sll   $t8, $t8, 2
/* 0ACC84 7F07A294 258C7A60 */  addiu $t4, %lo(c_item_entries) # addiu $t4, $t4, 0x7a60
/* 0ACC88 7F07A298 030C1821 */  addu  $v1, $t8, $t4
/* 0ACC8C 7F07A29C 8C640000 */  lw    $a0, ($v1)
/* 0ACC90 7F07A2A0 AFA400F8 */  sw    $a0, 0xf8($sp)
/* 0ACC94 7F07A2A4 8C8A0000 */  lw    $t2, ($a0)
/* 0ACC98 7F07A2A8 55400005 */  bnezl $t2, .L7F07A2C0
/* 0ACC9C 7F07A2AC 8FB90038 */   lw    $t9, 0x38($sp)
/* 0ACCA0 7F07A2B0 8C650004 */  lw    $a1, 4($v1)
/* 0ACCA4 7F07A2B4 0FC1D973 */  jal   fileLoad
/* 0ACCA8 7F07A2B8 AFA400F8 */   sw    $a0, 0xf8($sp)
.L7F07A2BC:
/* 0ACCAC 7F07A2BC 8FB90038 */  lw    $t9, 0x38($sp)
.L7F07A2C0:
/* 0ACCB0 7F07A2C0 8FA40044 */  lw    $a0, 0x44($sp)
/* 0ACCB4 7F07A2C4 8FA50040 */  lw    $a1, 0x40($sp)
/* 0ACCB8 7F07A2C8 8FA600FC */  lw    $a2, 0xfc($sp)
/* 0ACCBC 7F07A2CC 8FA700F8 */  lw    $a3, 0xf8($sp)
/* 0ACCC0 7F07A2D0 AFA00010 */  sw    $zero, 0x10($sp)
/* 0ACCC4 7F07A2D4 0FC08CAE */  jal   makeonebody
/* 0ACCC8 7F07A2D8 AFB90014 */   sw    $t9, 0x14($sp)
/* 0ACCCC 7F07A2DC 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0ACCD0 7F07A2E0 24638BC0 */  addiu $v1, %lo(g_CurrentPlayer) # addiu $v1, $v1, -0x7440
/* 0ACCD4 7F07A2E4 8C6B0000 */  lw    $t3, ($v1)
/* 0ACCD8 7F07A2E8 3C018005 */  lui   $at, %hi(D_8005502C) # $at, 0x8005
/* 0ACCDC 7F07A2EC AD6200D4 */  sw    $v0, 0xd4($t3)
/* 0ACCE0 7F07A2F0 8C6D0000 */  lw    $t5, ($v1)
/* 0ACCE4 7F07A2F4 C426ABFC */  lwc1  $f6, %lo(D_8005502C)($at)
/* 0ACCE8 7F07A2F8 8DA400D4 */  lw    $a0, 0xd4($t5)
/* 0ACCEC 7F07A2FC C4840014 */  lwc1  $f4, 0x14($a0)
/* 0ACCF0 7F07A300 46062202 */  mul.s $f8, $f4, $f6
/* 0ACCF4 7F07A304 44054000 */  mfc1  $a1, $f8
/* 0ACCF8 7F07A308 0FC1B4CF */  jal   modelSetScale
/* 0ACCFC 7F07A30C 00000000 */   nop
/* 0ACD00 7F07A310 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0ACD04 7F07A314 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0ACD08 7F07A318 8FA70114 */  lw    $a3, 0x114($sp)
/* 0ACD0C 7F07A31C 8C6400A8 */  lw    $a0, 0xa8($v1)
/* 0ACD10 7F07A320 8C6500D4 */  lw    $a1, 0xd4($v1)
/* 0ACD14 7F07A324 8C8E0014 */  lw    $t6, 0x14($a0)
/* 0ACD18 7F07A328 AFA00014 */  sw    $zero, 0x14($sp)
/* 0ACD1C 7F07A32C 24860008 */  addiu $a2, $a0, 8
/* 0ACD20 7F07A330 0FC07FC7 */  jal   init_GUARDdata_with_set_values
/* 0ACD24 7F07A334 AFAE0010 */   sw    $t6, 0x10($sp)
/* 0ACD28 7F07A338 3C078007 */  lui   $a3, %hi(g_CurrentPlayer) # $a3, 0x8007
/* 0ACD2C 7F07A33C 24E78BC0 */  addiu $a3, %lo(g_CurrentPlayer) # addiu $a3, $a3, -0x7440
/* 0ACD30 7F07A340 8CF80000 */  lw    $t8, ($a3)
/* 0ACD34 7F07A344 240F0006 */  li    $t7, 6
/* 0ACD38 7F07A348 8F0C00A8 */  lw    $t4, 0xa8($t8)
/* 0ACD3C 7F07A34C A18F0000 */  sb    $t7, ($t4)
/* 0ACD40 7F07A350 8CEA0000 */  lw    $t2, ($a3)
/* 0ACD44 7F07A354 8D5900A8 */  lw    $t9, 0xa8($t2)
/* 0ACD48 7F07A358 8F260004 */  lw    $a2, 4($t9)
/* 0ACD4C 7F07A35C 8CCB0014 */  lw    $t3, 0x14($a2)
/* 0ACD50 7F07A360 356D0001 */  ori   $t5, $t3, 1
/* 0ACD54 7F07A364 ACCD0014 */  sw    $t5, 0x14($a2)
/* 0ACD58 7F07A368 8CE30000 */  lw    $v1, ($a3)
/* 0ACD5C 7F07A36C 8C6500A8 */  lw    $a1, 0xa8($v1)
/* 0ACD60 7F07A370 8C6400D4 */  lw    $a0, 0xd4($v1)
/* 0ACD64 7F07A374 AFA60110 */  sw    $a2, 0x110($sp)
/* 0ACD68 7F07A378 0FC1B480 */  jal   setsuboffset
/* 0ACD6C 7F07A37C 24A50008 */   addiu $a1, $a1, 8
/* 0ACD70 7F07A380 3C0E8007 */  lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8007
/* 0ACD74 7F07A384 8DCE8BC0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0ACD78 7F07A388 8FA50114 */  lw    $a1, 0x114($sp)
/* 0ACD7C 7F07A38C 0FC1B49D */  jal   setsubroty
/* 0ACD80 7F07A390 8DC400D4 */   lw    $a0, 0xd4($t6)
/* 0ACD84 7F07A394 8FA20110 */  lw    $v0, 0x110($sp)
/* 0ACD88 7F07A398 8FB80040 */  lw    $t8, 0x40($sp)
/* 0ACD8C 7F07A39C A0580006 */  sb    $t8, 6($v0)
/* 0ACD90 7F07A3A0 8FAF0044 */  lw    $t7, 0x44($sp)
/* 0ACD94 7F07A3A4 A04F000F */  sb    $t7, 0xf($v0)
/* 0ACD98 7F07A3A8 0FC269E1 */  jal   getPropForHeldItem
/* 0ACD9C 7F07A3AC 8FA40048 */   lw    $a0, 0x48($sp)
/* 0ACDA0 7F07A3B0 04400043 */  bltz  $v0, .L7F07A4C0
/* 0ACDA4 7F07A3B4 AFA2004C */   sw    $v0, 0x4c($sp)
/* 0ACDA8 7F07A3B8 0FC26669 */  jal   getPlayerCount
/* 0ACDAC 7F07A3BC 00000000 */   nop
/* 0ACDB0 7F07A3C0 24010001 */  li    $at, 1
/* 0ACDB4 7F07A3C4 14410033 */  bne   $v0, $at, .L7F07A494
/* 0ACDB8 7F07A3C8 8FAC00F0 */   lw    $t4, 0xf0($sp)
/* 0ACDBC 7F07A3CC 8FA200E8 */  lw    $v0, 0xe8($sp)
/* 0ACDC0 7F07A3D0 24040001 */  li    $a0, 1
/* 0ACDC4 7F07A3D4 01825021 */  addu  $t2, $t4, $v0
/* 0ACDC8 7F07A3D8 244200C7 */  addiu $v0, $v0, 0xc7
/* 0ACDCC 7F07A3DC 3459003F */  ori   $t9, $v0, 0x3f
/* 0ACDD0 7F07A3E0 3B2B003F */  xori  $t3, $t9, 0x3f
/* 0ACDD4 7F07A3E4 AFAA00DC */  sw    $t2, 0xdc($sp)
/* 0ACDD8 7F07A3E8 0FC174F2 */  jal   get_ptr_itemheader_in_hand
/* 0ACDDC 7F07A3EC AFAB00E8 */   sw    $t3, 0xe8($sp)
/* 0ACDE0 7F07A3F0 8FAD004C */  lw    $t5, 0x4c($sp)
/* 0ACDE4 7F07A3F4 3C188003 */  lui   $t8, %hi(PitemZ_entries) # $t8, 0x8003
/* 0ACDE8 7F07A3F8 27184D88 */  addiu $t8, %lo(PitemZ_entries) # addiu $t8, $t8, 0x4d88
/* 0ACDEC 7F07A3FC 000D7080 */  sll   $t6, $t5, 2
/* 0ACDF0 7F07A400 01CD7023 */  subu  $t6, $t6, $t5
/* 0ACDF4 7F07A404 000E7080 */  sll   $t6, $t6, 2
/* 0ACDF8 7F07A408 01D81821 */  addu  $v1, $t6, $t8
/* 0ACDFC 7F07A40C 8C6F0000 */  lw    $t7, ($v1)
/* 0ACE00 7F07A410 AFA200F4 */  sw    $v0, 0xf4($sp)
/* 0ACE04 7F07A414 27AA0100 */  addiu $t2, $sp, 0x100
/* 0ACE08 7F07A418 8DE10000 */  lw    $at, ($t7)
/* 0ACE0C 7F07A41C 00402025 */  move  $a0, $v0
/* 0ACE10 7F07A420 AC410000 */  sw    $at, ($v0)
/* 0ACE14 7F07A424 8DF90004 */  lw    $t9, 4($t7)
/* 0ACE18 7F07A428 AC590004 */  sw    $t9, 4($v0)
/* 0ACE1C 7F07A42C 8DE10008 */  lw    $at, 8($t7)
/* 0ACE20 7F07A430 AC410008 */  sw    $at, 8($v0)
/* 0ACE24 7F07A434 8DF9000C */  lw    $t9, 0xc($t7)
/* 0ACE28 7F07A438 AC59000C */  sw    $t9, 0xc($v0)
/* 0ACE2C 7F07A43C 8DE10010 */  lw    $at, 0x10($t7)
/* 0ACE30 7F07A440 AC410010 */  sw    $at, 0x10($v0)
/* 0ACE34 7F07A444 8DF90014 */  lw    $t9, 0x14($t7)
/* 0ACE38 7F07A448 AC590014 */  sw    $t9, 0x14($v0)
/* 0ACE3C 7F07A44C 8DE10018 */  lw    $at, 0x18($t7)
/* 0ACE40 7F07A450 AC410018 */  sw    $at, 0x18($v0)
/* 0ACE44 7F07A454 8FAD00E8 */  lw    $t5, 0xe8($sp)
/* 0ACE48 7F07A458 8FAE00E4 */  lw    $t6, 0xe4($sp)
/* 0ACE4C 7F07A45C 8FAB00F0 */  lw    $t3, 0xf0($sp)
/* 0ACE50 7F07A460 AFA30028 */  sw    $v1, 0x28($sp)
/* 0ACE54 7F07A464 AFAA0010 */  sw    $t2, 0x10($sp)
/* 0ACE58 7F07A468 8C650004 */  lw    $a1, 4($v1)
/* 0ACE5C 7F07A46C 01CD3823 */  subu  $a3, $t6, $t5
/* 0ACE60 7F07A470 0FC1D949 */  jal   load_object_fill_header
/* 0ACE64 7F07A474 016D3021 */   addu  $a2, $t3, $t5
/* 0ACE68 7F07A478 8FA30028 */  lw    $v1, 0x28($sp)
/* 0ACE6C 7F07A47C 0FC2F150 */  jal   get_pc_buffer_remaining_value
/* 0ACE70 7F07A480 8C640004 */   lw    $a0, 4($v1)
/* 0ACE74 7F07A484 0FC1D75F */  jal   modelCalculateRwDataLen
/* 0ACE78 7F07A488 8FA400F4 */   lw    $a0, 0xf4($sp)
/* 0ACE7C 7F07A48C 10000004 */  b     .L7F07A4A0
/* 0ACE80 7F07A490 8FAC00DC */   lw    $t4, 0xdc($sp)
.L7F07A494:
/* 0ACE84 7F07A494 AFA000DC */  sw    $zero, 0xdc($sp)
/* 0ACE88 7F07A498 AFA000F4 */  sw    $zero, 0xf4($sp)
/* 0ACE8C 7F07A49C 8FAC00DC */  lw    $t4, 0xdc($sp)
.L7F07A4A0:
/* 0ACE90 7F07A4A0 8FAF00F4 */  lw    $t7, 0xf4($sp)
/* 0ACE94 7F07A4A4 8FA40110 */  lw    $a0, 0x110($sp)
/* 0ACE98 7F07A4A8 8FA5004C */  lw    $a1, 0x4c($sp)
/* 0ACE9C 7F07A4AC 8FA60048 */  lw    $a2, 0x48($sp)
/* 0ACEA0 7F07A4B0 00003825 */  move  $a3, $zero
/* 0ACEA4 7F07A4B4 AFAC0010 */  sw    $t4, 0x10($sp)
/* 0ACEA8 7F07A4B8 0FC1493D */  jal   something_with_generating_object
/* 0ACEAC 7F07A4BC AFAF0014 */   sw    $t7, 0x14($sp)
.L7F07A4C0:
/* 0ACEB0 7F07A4C0 8FA40110 */  lw    $a0, 0x110($sp)
/* 0ACEB4 7F07A4C4 0FC08E9D */  jal   chrlvIdleAnimationRelated7F023A94
/* 0ACEB8 7F07A4C8 24050000 */   li    $a1, 0
/* 0ACEBC 7F07A4CC 10000017 */  b     .L7F07A52C
/* 0ACEC0 7F07A4D0 8FBF001C */   lw    $ra, 0x1c($sp)
/* 0ACEC4 7F07A4D4 8C59001C */  lw    $t9, 0x1c($v0)
.L7F07A4D8:
/* 0ACEC8 7F07A4D8 00402025 */  move  $a0, $v0
/* 0ACECC 7F07A4DC 8F2B0020 */  lw    $t3, 0x20($t9)
/* 0ACED0 7F07A4E0 55600012 */  bnezl $t3, .L7F07A52C
/* 0ACED4 7F07A4E4 8FBF001C */   lw    $ra, 0x1c($sp)
/* 0ACED8 7F07A4E8 8C4D0014 */  lw    $t5, 0x14($v0)
/* 0ACEDC 7F07A4EC 24050000 */  li    $a1, 0
/* 0ACEE0 7F07A4F0 35AE0001 */  ori   $t6, $t5, 1
/* 0ACEE4 7F07A4F4 0FC08E9D */  jal   chrlvIdleAnimationRelated7F023A94
/* 0ACEE8 7F07A4F8 AC4E0014 */   sw    $t6, 0x14($v0)
/* 0ACEEC 7F07A4FC 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0ACEF0 7F07A500 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0ACEF4 7F07A504 8C6500A8 */  lw    $a1, 0xa8($v1)
/* 0ACEF8 7F07A508 8C6400D4 */  lw    $a0, 0xd4($v1)
/* 0ACEFC 7F07A50C 0FC1B480 */  jal   setsuboffset
/* 0ACF00 7F07A510 24A50008 */   addiu $a1, $a1, 8
/* 0ACF04 7F07A514 3C188007 */  lui   $t8, %hi(g_CurrentPlayer) # $t8, 0x8007
/* 0ACF08 7F07A518 8F188BC0 */  lw    $t8, %lo(g_CurrentPlayer)($t8)
/* 0ACF0C 7F07A51C 8FA50114 */  lw    $a1, 0x114($sp)
/* 0ACF10 7F07A520 0FC1B49D */  jal   setsubroty
/* 0ACF14 7F07A524 8F0400D4 */   lw    $a0, 0xd4($t8)
/* 0ACF18 7F07A528 8FBF001C */  lw    $ra, 0x1c($sp)
.L7F07A52C:
/* 0ACF1C 7F07A52C 27BD0118 */  addiu $sp, $sp, 0x118
/* 0ACF20 7F07A530 03E00008 */  jr    $ra
/* 0ACF24 7F07A534 00000000 */   nop
)
#endif


#endif

/**
 * Address 0x7F07A4A0.
 */
void maybe_solo_intro_camera_handler(void)
{
    if ((g_CurrentPlayer->prop->chr != 0) && (getPlayerCount() == 1))
    {
        chrpropCleanupForRemoval(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop->chr = NULL;
        g_CurrentPlayer->ptr_char_objectinstance = 0;
        g_bondviewForceDisarm = 1;
        sub_GAME_7F07DE9C(g_CurrentPlayer);
    }
}


u32 bondviewGetCameraMode(void) {
    return g_CameraMode;
}


s32 pickDeathCameraAngles(PropRecord *prop1, coord3d *pos, PropRecord *prop2, coord3d *collision_pos, StandTile *tile, f32 camera_dist)
{
    s32 found;
    s32 outertries;
    f32 camclearance;
    f32 spD0;
    f32 angle;
    StandTile *spC8;
    coord3d spBC;
    coord3d spB0;
    f32 angleRange;
    f32 frac;
    f32 floorY;
    s32 lineok;
    s32 angletries;

    dword_CODE_bss_800799F4 = prop1;

    found = 0;
    outertries = 0;
    angleRange = M_TAU_F;

    while ((outertries <= 0x80) && (!found))
    {
        camclearance = g_CurrentPlayer->field_488.collision_radius;
        spD0 = 1500.0f + camclearance;

        angle = ((f32) randomGetNext()) * 2.3283064e-10f;
        angle = angle * angleRange;

        angletries = 0;

        while ((angletries < 0x10) && (!found))
        {
            angleRange = M_TAU_F;

            angle += 0.39269909f;

            if (angleRange <= angle)
            {
                angle -= angleRange;
            }

            spBC.x = sinf(angle);
            spBC.y = 0.0f;
            spBC.z = cosf(angle);

            spB0.x = (((f32 *) (&spBC))[0] * spD0) + pos->x;
            spB0.y = pos->y;
            spB0.z = (((f32 *) (&spBC))[2] * spD0) + pos->z;

            stanResetHits();

            spC8 = tile;

            sub_GAME_7F03D058(prop2, 0);

            lineok = stanTestLineUnobstructed(&spC8, collision_pos->x, collision_pos->z, spB0.x, spB0.z, 0x13, 0.0f, 1.0f, 0.0f, 1.0f);

            sub_GAME_7F03D058(prop2, 1);

            if (!lineok)
            {
                chrlvStanPointPointIntersection(collision_pos, &spBC, &spB0);

                {
                    f32 dx = spB0.x - collision_pos->x;
                    f32 dz = spB0.z - collision_pos->z;

                    spD0 = sqrtf((dx * dx) + (dz * dz));
                }
            }

            spD0 -= camclearance;

            if (camera_dist <= spD0)
            {
                frac = 1.0f;

                while ((0.0f < frac) && (!found))
                {
                    f32 sp90;
                    f32 candidateDist;

                    sp90 = spD0 - camera_dist;

                    candidateDist = ((f32) randomGetNext()) * 2.3283064e-10f;
                    candidateDist *= sp90;
                    candidateDist *= frac;
                    candidateDist += camera_dist;

                    flt_CODE_bss_800799E8.x = (spBC.x * candidateDist) + pos->x;
                    flt_CODE_bss_800799E8.y = pos->y;
                    flt_CODE_bss_800799E8.z = (spBC.z * candidateDist) + pos->z;

                    spC8 = tile;

                    sub_GAME_7F03D058(prop2, 0);

                    lineok = stanTestLineUnobstructed(&spC8, collision_pos->x, collision_pos->z, flt_CODE_bss_800799E8.x, flt_CODE_bss_800799E8.z, 0x13, 0.0f, 1.0f, 0.0f, 1.0f);

                    sub_GAME_7F03D058(prop2, 1);

                    if (lineok)
                    {
                        lineok = stanTestVolume(&spC8, flt_CODE_bss_800799E8.x, flt_CODE_bss_800799E8.z, camclearance, 0x1f, 0.0f, 1.0f);

                        if (lineok < 0)
                        {
                            floorY = stanGetPositionYValue(spC8, flt_CODE_bss_800799E8.x, flt_CODE_bss_800799E8.z);

                            flt_CODE_bss_800799E8.y = (floorY + camclearance) + ((((f32) randomGetNext()) * 2.3283064e-10f) * (185.0f - camclearance));

                            candidateDist = flt_CODE_bss_800799E8.y - pos->y;

                            if (((-1000.0f) < candidateDist) && (candidateDist < 1000.0f))
                            {
                                found = 1;
                            }
                        }
                    }

                    frac -= 0.25f;

                    if (pos);
                }
            }

            angletries++;
        }

        angleRange = M_TAU_F;

        outertries++;
    }

    return found;
}


// Address 0x7F07A9B8 NTSC.
void bondviewSetCameraMode(s32 arg0)
{
    s32 padding;
    s32 padding2;

    g_CameraMode = arg0;
    g_CameraAfterCinema = 0;

    if (g_CameraMode == CAMERAMODE_INTRO)
    {
        if ((ptr_random06cam_entry != NULL) && (get_recording_ramrom_flag() == 0) && (get_is_ramrom_flag() == 0))
        {
            camera_transition_timer = 0.0f;
            currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            currentPlayerSetFadeFrac(60.0f, 0.0f);
            fogLoadLevelEnvironment(bossGetStageNum(), 1);
            g_CurrentPlayer->room_pointer = NULL;
        }
        else
        {
            bondviewSetCameraMode(CAMERAMODE_SWIRL);
        }
    }
    else if (g_CameraMode == CAMERAMODE_FADESWIRL)
    {
        currentPlayerSetFadeColour(0, 0, 0, 0.0f);
        currentPlayerSetFadeFrac(60.0f, 1.0f);
    }
    else if (g_CameraMode == CAMERAMODE_MP)
    {
        g_MpSwirlRotateSpeed = 0.0f;
        g_MpSwirlAngleDegrees = -90.0f;
        g_MpSwirlForwardSpeed = 0.0f;
        g_MpSwirlDistance = 80.0f;
        fogLoadLevelEnvironment(bossGetStageNum(), 0);
    }
    else if (g_CameraMode == CAMERAMODE_SWIRL)
    {
        struct ModelAnimation *sp38;
        f32 sp78;
        f32 ftemp_3;
        f32 ftemp_1;
        struct ChrRecord *temp_v1;

        camera_fade_active = 0;
        currentPlayerSetFadeColour(0, 0, 0, 1.0f);
        currentPlayerSetFadeFrac(60.0f, 0.0f);
        fogLoadLevelEnvironment(bossGetStageNum(), 0);

        if ((g_IntroSwirl != 0) && (get_recording_ramrom_flag() == 0) && (get_is_ramrom_flag() == 0))
        {
            camera_transition_timer = 0.0f;
            intro_camera_index = CAMERAMODE_INTRO;
            currentPlayerStartChrFade(0.0f, 1.0f);
            solo_char_load();

            // HACK: ptr_animation_table->data regalloc is backwards
            sp38 = (struct ModelAnimation *)((s32)stage_intro_anim_table[g_IntroAnimationIndex].anonymous_0 + (s32)&ptr_animation_table->data);
            sp78 = stage_intro_anim_table[g_IntroAnimationIndex].anonymous_2;
            ftemp_1 = stage_intro_anim_table[g_IntroAnimationIndex].anonymous_1;
            ftemp_3 = stage_intro_anim_table[g_IntroAnimationIndex].anonymous_3;

            modelSetAnimation(
                g_CurrentPlayer->ptr_char_objectinstance,
                sp38,
                0,
                ftemp_1,
                ftemp_3,
                0.0f);

            if (sp78 > 0.0f)
            {
                modelSetAnimEndFrame(g_CurrentPlayer->ptr_char_objectinstance, sp78);
            }

            temp_v1 = g_CurrentPlayer->prop->chr;
            temp_v1->actiontype = ACT_BONDINTRO;
            temp_v1->sleep = 0;
            g_CurrentPlayer->room_pointer = NULL;
        }
        else
        {
            bondviewSetCameraMode(CAMERAMODE_FP);
        }
    }
    else if (g_CameraMode == CAMERAMODE_FP)
    {
        if (bossGetStageNum() == LEVELID_CUBA)
        {
            currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            currentPlayerSetFadeFrac(0.0f, 1.0f);
        }
        else if (camera_fade_active != 0)
        {
            currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            currentPlayerSetFadeFrac(60.0f, 0.0f);
        }

        if (getPlayerCount() >= 2)
        {
            fogLoadLevelEnvironment(bossGetStageNum(), 0);
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
        {
            currentPlayerEquipWeaponWrapper(GUNLEFT, starting_weapon[GUNLEFT]);
            currentPlayerEquipWeaponWrapper(GUNRIGHT, starting_weapon[GUNRIGHT]);
        }

        stop_time_flag = 0;
    }
    else if (g_CameraMode == CAMERAMODE_DEATH_CAM_SP)
    {
        f32 var_f0;
        PropRecord *sp64;
        struct coord3d sp58;
        StandTile *var_v1;
        struct coord3d sp48;
        PropRecord *var_a2;
        struct ChrRecord *temp_v1_2;

        camera_transition_timer = 0.0f;
        intro_camera_index = CAMERAMODE_INTRO;
        currentPlayerSetFadeColour(0, 0, 0, 1.0f);
        currentPlayerSetFadeFrac(60.0f, 0.0f);

        if (g_ExplodeTankOnDeathFlag && (g_PlayerTankProp != NULL))
        {
            // removed
        }
        else
        {
            // This branch restarts Bond's death animation for his death replay
            g_PlayerIsInTank = 0;

            // struct copy
            g_CurrentPlayer->field_488 = g_CurrentPlayer->previous_collision_info;

            g_CurrentPlayer->vv_theta = g_CurrentPlayer->thetadie;
            g_CurrentPlayer->vv_verta = g_CurrentPlayer->vertadie;
            g_CurrentPlayer->prop->pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
            g_CurrentPlayer->prop->pos.f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
            g_CurrentPlayer->prop->pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2];
            g_CurrentPlayer->prop->stan = g_CurrentPlayer->field_488.current_tile_ptr;

            bondviewApplyVertaTheta();
            bondviewMoveAnimationTick(0, 0, 0);
            bondviewUpdatePlayerCollisionPositionFields();
            currentPlayerStartChrFade(0.0f, 1.0f);
            solo_char_load();

            modelSetAnimation(
                g_CurrentPlayer->ptr_char_objectinstance,
                objecthandlerGetModelAnim((Model *) &g_CurrentPlayer->model),
                objecthandlerGetModelGunhand(&g_CurrentPlayer->model),
                0.0f,
                0.5f,
                0.0f);

            temp_v1_2 = g_CurrentPlayer->prop->chr;
            temp_v1_2->actiontype = ACT_BONDDIE;
            temp_v1_2->sleep = 0;
            temp_v1_2->chrflags |= CHRFLAG_INIT;

            setsuboffset(g_CurrentPlayer->ptr_char_objectinstance, &g_CurrentPlayer->prop->pos);
            var_f0 = bondviewGetPlayerYawRadians();
            setsubroty(g_CurrentPlayer->ptr_char_objectinstance, var_f0);
        }

        if (g_ExplodeTankOnDeathFlag && (g_PlayerTankProp != NULL))
        {
            sp64 = g_PlayerTankProp;
            var_f0 = 500.0f; // distance to place the camera
            sp58.f[0] = g_PlayerTankProp->pos.f[0];
            sp58.f[1] = g_PlayerTankProp->pos.f[1];
            sp58.f[2] = g_PlayerTankProp->pos.f[2];
            var_a2 = g_PlayerTankProp;
            sp48.f[0] = g_PlayerTankProp->pos.f[0];
            sp48.f[1] = g_PlayerTankProp->pos.f[1];
            sp48.f[2] = g_PlayerTankProp->pos.f[2];
            var_v1 = g_PlayerTankProp->stan;
        }
        else
        {
            var_f0 = 200.0f; // distance to place the camera
            sp64 = g_CurrentPlayer->prop;
            sp58.f[0] = g_CurrentPlayer->field_3C4;
            sp58.f[1] = g_CurrentPlayer->field_3C8;
            sp58.f[2] = g_CurrentPlayer->field_3CC;
            var_a2 = g_CurrentPlayer->prop;
            sp48.f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
            sp48.f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
            sp48.f[2] = g_CurrentPlayer->field_488.collision_position.f[2];
            var_v1 = g_CurrentPlayer->field_488.current_tile_ptr;
        }

        if (pickDeathCameraAngles(sp64, &sp58, var_a2, &sp48, var_v1, var_f0) != 0)
        {
            if (camera_mode == 0)
            {
#ifdef DEBUG
                osSyncPrintf("mute\n");
#endif

                musicTrack1Play(M_INTROSWOOSH);
                sndSetScalerApplyVolumeAllSfxSlot(0.5f);
            }

            if ((g_ExplodeTankOnDeathFlag != 0) && (g_PlayerTankProp != NULL))
            {
                explosionCreate(g_PlayerTankProp, &g_PlayerTankProp->pos, g_PlayerTankProp->stan, 0xD, 0, get_cur_playernum(), g_PlayerTankProp->rooms, 0);
            }
        }
        else
        {
            // pickDeathCameraAngles has returned 0. This happens when no possible angles were found
            // to place the camera at the requested distance or when the three death replays are finished.
            bossRunTitleStage();
        }
    }
    else if (g_CameraMode == CAMERAMODE_DEATH_CAM_MP)
    {
        currentPlayerSetFadeColour(0, 0, 0, 0.0f);
        currentPlayerSetFadeFrac(60.0f, 1.0f);
    }
    else if (g_CameraMode == CAMERAMODE_POSEND)
    {
        solo_char_load();
        g_CurrentPlayer->room_pointer = NULL;
    }
    else if (g_CameraMode == CAMERAMODE_FP_NOINPUT)
    {
        maybe_solo_intro_camera_handler();
        g_CameraMode = CAMERAMODE_FP;
    }
    else if (g_CameraMode == CAMERAMODE_FADE_TO_TITLE)
    {
        s32 var_s0;

        for (var_s0 = 0; var_s0 < getPlayerCount(); var_s0++)
        {
            set_cur_player(var_s0);
            currentPlayerSetFadeColour(0, 0, 0, 0.0f);
            currentPlayerSetFadeFrac(60.0f, 1.0f);
        }

        set_cur_player(0);
    }
}


void sub_GAME_7F07B1A4(void)
{
    enum CAMERAMODE mode = g_CameraMode;

    g_CameraMode = CAMERAMODE_NONE;
    g_CameraAfterCinema = CAMERAMODE_NONE;

    if (mode == CAMERAMODE_INTRO)
    {
        bondviewSetCameraMode(CAMERAMODE_FADESWIRL);
    }
    else if (mode == CAMERAMODE_FADESWIRL)
    {
        bondviewResetIntroCameraMessageDialogs();
        bondviewSetCameraMode(CAMERAMODE_SWIRL);
    }
    else if (mode != CAMERAMODE_MP)
    {
        if (mode == CAMERAMODE_SWIRL)
        {
            maybe_solo_intro_camera_handler();
            currentPlayerStartChrFade(0.0f, 1.0f);
            bondviewSetCameraMode(CAMERAMODE_FP);
        }
        else if (mode != CAMERAMODE_FP)
        {
            if (mode == CAMERAMODE_DEATH_CAM_SP)
            {
                bondviewSetCameraMode(CAMERAMODE_DEATH_CAM_MP);
            }
            else if (mode == CAMERAMODE_DEATH_CAM_MP)
            {
                camera_mode++;

                if (camera_mode < CAMERAMODE_SWIRL)
                {
                    bondviewSetCameraMode(CAMERAMODE_DEATH_CAM_SP);
                }
            }
        }
    }
}


#ifdef NONMATCHING
void sub_GAME_7F07B2A0(void)
{
    ? sp84;
    ? sp78;
    ? sp6C;
    ? sp60;
    f32 sp58;
    ? sp54;
    s32 sp40;
    ? *var_a1;
    f32   temp_f0;
    f32   var_f0;
    f32   var_f2;
    s32   temp_t4;
    s32   var_a2;
    u32   temp_a0;
    u32   temp_a3;
    u32   var_v0;
    void *temp_t3;
    void *temp_t3_2;

    temp_t4 = arg0 << 5;
    temp_t3 = g_IntroSwirl + temp_t4;
    var_f2  = 0.0f;
    temp_f0 = temp_t3->unk18;
    var_a1  = &sp54;
    var_a2  = -1;
    temp_a3 = (arg0 << 5) + g_IntroSwirl;
    if (temp_f0 > 0.0f)
    {
        var_f2 = arg1 / temp_f0;
    }
    do
    {
        temp_a0 = (var_a2 << 5) + temp_a3;
        var_v0  = temp_a3;
        if (var_a2 < 0)
        {
            if (temp_a0 < g_IntroSwirl)
            {
                var_v0 = g_IntroSwirl;
            }
            else
            {
                var_v0 = temp_a0;
            }
        }
        else if (var_v0 < temp_a0)
        {
loop_8:
            if (!(var_v0->unk24 & 1))
            {
                var_v0 += 0x20;
                if (var_v0 < temp_a0)
                {
                    goto loop_8;
                }
            }
        }
        var_a2 += 1;
        if (var_v0->unk4 & 2)
        {
            // pre-scaled offset eg 2812 = 646 RCP units
            var_a1->unkC  = (g_CurrentPlayer->field_488.theta_transform.f[1] * var_v0->unk8) + (var_v0->unk10 * g_CurrentPlayer->unk498);
            var_a1->unk10 = var_v0->unkC;
            var_a1->unk14 = (var_v0->unk10 * g_CurrentPlayer->unk4A0) - (g_CurrentPlayer->unk498 * var_v0->unk8);
        }
        else
        {
            var_a1->unkC  = var_v0->unk8;
            var_a1->unk10 = var_v0->unkC;
            var_a1->unk14 = var_v0->unk10;
        }
        var_a1 += 0xC;
    } while (var_a2 != 3);
    sp58 = var_f2;
    sp40 = temp_t4;
    coord3dCubicSplineInterp(arg1, &sp60, &sp6C, &sp78, &sp84, var_f2, temp_t3->unk14, arg2);
    arg2->unk0 = arg2->unk0 + g_CurrentPlayer->unk3C4;
    arg2->unk4 = arg2->unk4 + g_CurrentPlayer->unk3C8;
    arg2->unk8 = arg2->unk8 + g_CurrentPlayer->unk3CC;
    arg3->unk0 = g_CurrentPlayer->unk3C4;
    arg3->unk4 = g_CurrentPlayer->unk3C8;
    arg3->unk8 = g_CurrentPlayer->unk3CC;
    temp_t3_2  = g_IntroSwirl + sp40;
    if (!(temp_t3_2->unk4 & 4))
    {
        if (!(temp_t3_2->unk24 & 4))
        {
            var_f0 = 1.0f;
        }
        else
        {
            var_f0 = 1.0f - sp58;
        }
    }
    else if (temp_t3_2->unk24 & 4)
    {
        var_f0 = 0.0f;
    }
    else
    {
        var_f0 = sp58;
    }
    arg3->unk0 = arg3->unk0 + (g_CurrentPlayer->unk4C0 * 40.0f * var_f0);
    arg3->unk4 = arg3->unk4 + (g_CurrentPlayer->unk4C4 * 40.0f * var_f0);
    arg3->unk8 = arg3->unk8 + (g_CurrentPlayer->unk4C8 * 40.0f * var_f0);
}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F07B2A0
/* 0AFDD0 7F07B2A0 3C088003 */  lui   $t0, %hi(g_IntroSwirl)
/* 0AFDD4 7F07B2A4 8D0864AC */  lw    $t0, %lo(g_IntroSwirl)($t0)
/* 0AFDD8 7F07B2A8 27BDFF70 */  addiu $sp, $sp, -0x90
/* 0AFDDC 7F07B2AC 00046140 */  sll   $t4, $a0, 5
/* 0AFDE0 7F07B2B0 AFBF002C */  sw    $ra, 0x2c($sp)
/* 0AFDE4 7F07B2B4 AFB00028 */  sw    $s0, 0x28($sp)
/* 0AFDE8 7F07B2B8 AFA60098 */  sw    $a2, 0x98($sp)
/* 0AFDEC 7F07B2BC 010C5821 */  addu  $t3, $t0, $t4
/* 0AFDF0 7F07B2C0 44801000 */  mtc1  $zero, $f2
/* 0AFDF4 7F07B2C4 C5600018 */  lwc1  $f0, 0x18($t3)
/* 0AFDF8 7F07B2C8 00E08025 */  move  $s0, $a3
/* 0AFDFC 7F07B2CC 44856000 */  mtc1  $a1, $f12
/* 0AFE00 7F07B2D0 4600103C */  c.lt.s $f2, $f0
/* 0AFE04 7F07B2D4 00047140 */  sll   $t6, $a0, 5
/* 0AFE08 7F07B2D8 27A50054 */  addiu $a1, $sp, 0x54
/* 0AFE0C 7F07B2DC 2406FFFF */  li    $a2, -1
/* 0AFE10 7F07B2E0 45000002 */  bc1f  .L7F07B2EC
/* 0AFE14 7F07B2E4 01C83821 */   addu  $a3, $t6, $t0
/* 0AFE18 7F07B2E8 46006083 */  div.s $f2, $f12, $f0
.L7F07B2EC:
/* 0AFE1C 7F07B2EC 3C098008 */  lui   $t1, %hi(g_CurrentPlayer)
/* 0AFE20 7F07B2F0 2529A0B0 */  addiu $t1, %lo(g_CurrentPlayer) # addiu $t1, $t1, -0x5f50
/* 0AFE24 7F07B2F4 240A0003 */  li    $t2, 3
.L7F07B2F8:
/* 0AFE28 7F07B2F8 00067940 */  sll   $t7, $a2, 5
/* 0AFE2C 7F07B2FC 01E72021 */  addu  $a0, $t7, $a3
/* 0AFE30 7F07B300 00E01025 */  move  $v0, $a3
/* 0AFE34 7F07B304 04C10008 */  bgez  $a2, .L7F07B328
/* 0AFE38 7F07B308 00801825 */   move  $v1, $a0
/* 0AFE3C 7F07B30C 0088082B */  sltu  $at, $a0, $t0
/* 0AFE40 7F07B310 10200003 */  beqz  $at, .L7F07B320
/* 0AFE44 7F07B314 00000000 */   nop
/* 0AFE48 7F07B318 1000000E */  b     .L7F07B354
/* 0AFE4C 7F07B31C 01001025 */   move  $v0, $t0
.L7F07B320:
/* 0AFE50 7F07B320 1000000C */  b     .L7F07B354
/* 0AFE54 7F07B324 00601025 */   move  $v0, $v1
.L7F07B328:
/* 0AFE58 7F07B328 0044082B */  sltu  $at, $v0, $a0
/* 0AFE5C 7F07B32C 5020000A */  beql  $at, $zero, .L7F07B358
/* 0AFE60 7F07B330 8C4D0004 */   lw    $t5, 4($v0)
/* 0AFE64 7F07B334 8C580024 */  lw    $t8, 0x24($v0)
.L7F07B338:
/* 0AFE68 7F07B338 33190001 */  andi  $t9, $t8, 1
/* 0AFE6C 7F07B33C 57200006 */  bnezl $t9, .L7F07B358
/* 0AFE70 7F07B340 8C4D0004 */   lw    $t5, 4($v0)
/* 0AFE74 7F07B344 24420020 */  addiu $v0, $v0, 0x20
/* 0AFE78 7F07B348 0043082B */  sltu  $at, $v0, $v1
/* 0AFE7C 7F07B34C 5420FFFA */  bnezl $at, .L7F07B338
/* 0AFE80 7F07B350 8C580024 */   lw    $t8, 0x24($v0)
.L7F07B354:
/* 0AFE84 7F07B354 8C4D0004 */  lw    $t5, 4($v0)
.L7F07B358:
/* 0AFE88 7F07B358 24C60001 */  addiu $a2, $a2, 1
/* 0AFE8C 7F07B35C 31AE0002 */  andi  $t6, $t5, 2
/* 0AFE90 7F07B360 51C00018 */  beql  $t6, $zero, .L7F07B3C4
/* 0AFE94 7F07B364 C4500008 */   lwc1  $f16, 8($v0)
/* 0AFE98 7F07B368 8D230000 */  lw    $v1, ($t1)
/* 0AFE9C 7F07B36C C4460008 */  lwc1  $f6, 8($v0)
/* 0AFEA0 7F07B370 C44A0010 */  lwc1  $f10, 0x10($v0)
/* 0AFEA4 7F07B374 C46404A0 */  lwc1  $f4, 0x4a0($v1)
/* 0AFEA8 7F07B378 C4700498 */  lwc1  $f16, 0x498($v1)
/* 0AFEAC 7F07B37C 46062202 */  mul.s $f8, $f4, $f6
/* 0AFEB0 7F07B380 00000000 */  nop
/* 0AFEB4 7F07B384 46105482 */  mul.s $f18, $f10, $f16
/* 0AFEB8 7F07B388 46124100 */  add.s $f4, $f8, $f18
/* 0AFEBC 7F07B38C E4A4000C */  swc1  $f4, 0xc($a1)
/* 0AFEC0 7F07B390 C446000C */  lwc1  $f6, 0xc($v0)
/* 0AFEC4 7F07B394 E4A60010 */  swc1  $f6, 0x10($a1)
/* 0AFEC8 7F07B398 C44A0010 */  lwc1  $f10, 0x10($v0)
/* 0AFECC 7F07B39C C47004A0 */  lwc1  $f16, 0x4a0($v1)
/* 0AFED0 7F07B3A0 C4440008 */  lwc1  $f4, 8($v0)
/* 0AFED4 7F07B3A4 C4720498 */  lwc1  $f18, 0x498($v1)
/* 0AFED8 7F07B3A8 46105202 */  mul.s $f8, $f10, $f16
/* 0AFEDC 7F07B3AC 00000000 */  nop
/* 0AFEE0 7F07B3B0 46049182 */  mul.s $f6, $f18, $f4
/* 0AFEE4 7F07B3B4 46064281 */  sub.s $f10, $f8, $f6
/* 0AFEE8 7F07B3B8 10000007 */  b     .L7F07B3D8
/* 0AFEEC 7F07B3BC E4AA0014 */   swc1  $f10, 0x14($a1)
/* 0AFEF0 7F07B3C0 C4500008 */  lwc1  $f16, 8($v0)
.L7F07B3C4:
/* 0AFEF4 7F07B3C4 E4B0000C */  swc1  $f16, 0xc($a1)
/* 0AFEF8 7F07B3C8 C452000C */  lwc1  $f18, 0xc($v0)
/* 0AFEFC 7F07B3CC E4B20010 */  swc1  $f18, 0x10($a1)
/* 0AFF00 7F07B3D0 C4440010 */  lwc1  $f4, 0x10($v0)
/* 0AFF04 7F07B3D4 E4A40014 */  swc1  $f4, 0x14($a1)
.L7F07B3D8:
/* 0AFF08 7F07B3D8 14CAFFC7 */  bne   $a2, $t2, .L7F07B2F8
/* 0AFF0C 7F07B3DC 24A5000C */   addiu $a1, $a1, 0xc
/* 0AFF10 7F07B3E0 C5600014 */  lwc1  $f0, 0x14($t3)
/* 0AFF14 7F07B3E4 8FAF0098 */  lw    $t7, 0x98($sp)
/* 0AFF18 7F07B3E8 E7A20058 */  swc1  $f2, 0x58($sp)
/* 0AFF1C 7F07B3EC AFAC0040 */  sw    $t4, 0x40($sp)
/* 0AFF20 7F07B3F0 E7A20010 */  swc1  $f2, 0x10($sp)
/* 0AFF24 7F07B3F4 27A40060 */  addiu $a0, $sp, 0x60
/* 0AFF28 7F07B3F8 27A5006C */  addiu $a1, $sp, 0x6c
/* 0AFF2C 7F07B3FC 27A60078 */  addiu $a2, $sp, 0x78
/* 0AFF30 7F07B400 27A70084 */  addiu $a3, $sp, 0x84
/* 0AFF34 7F07B404 E7A00014 */  swc1  $f0, 0x14($sp)
/* 0AFF38 7F07B408 0FC16C09 */  jal   coord3dCubicSplineInterp
/* 0AFF3C 7F07B40C AFAF0018 */   sw    $t7, 0x18($sp)
/* 0AFF40 7F07B410 3C098008 */  lui   $t1, %hi(g_CurrentPlayer)
/* 0AFF44 7F07B414 2529A0B0 */  addiu $t1, %lo(g_CurrentPlayer) # addiu $t1, $t1, -0x5f50
/* 0AFF48 7F07B418 8FA20098 */  lw    $v0, 0x98($sp)
/* 0AFF4C 7F07B41C 8D380000 */  lw    $t8, ($t1)
/* 0AFF50 7F07B420 8FAC0040 */  lw    $t4, 0x40($sp)
/* 0AFF54 7F07B424 C4480000 */  lwc1  $f8, ($v0)
/* 0AFF58 7F07B428 C70603C4 */  lwc1  $f6, 0x3c4($t8)
/* 0AFF5C 7F07B42C C7A20058 */  lwc1  $f2, 0x58($sp)
/* 0AFF60 7F07B430 C4500004 */  lwc1  $f16, 4($v0)
/* 0AFF64 7F07B434 46064280 */  add.s $f10, $f8, $f6
/* 0AFF68 7F07B438 C4480008 */  lwc1  $f8, 8($v0)
/* 0AFF6C 7F07B43C 44807000 */  mtc1  $zero, $f14
/* 0AFF70 7F07B440 E44A0000 */  swc1  $f10, ($v0)
/* 0AFF74 7F07B444 8D390000 */  lw    $t9, ($t1)
/* 0AFF78 7F07B448 C73203C8 */  lwc1  $f18, 0x3c8($t9)
/* 0AFF7C 7F07B44C 3C198003 */  lui   $t9, %hi(g_IntroSwirl)
/* 0AFF80 7F07B450 46128100 */  add.s $f4, $f16, $f18
/* 0AFF84 7F07B454 E4440004 */  swc1  $f4, 4($v0)
/* 0AFF88 7F07B458 8D2D0000 */  lw    $t5, ($t1)
/* 0AFF8C 7F07B45C C5A603CC */  lwc1  $f6, 0x3cc($t5)
/* 0AFF90 7F07B460 46064280 */  add.s $f10, $f8, $f6
/* 0AFF94 7F07B464 E44A0008 */  swc1  $f10, 8($v0)
/* 0AFF98 7F07B468 8D2E0000 */  lw    $t6, ($t1)
/* 0AFF9C 7F07B46C C5D003C4 */  lwc1  $f16, 0x3c4($t6)
/* 0AFFA0 7F07B470 E6100000 */  swc1  $f16, ($s0)
/* 0AFFA4 7F07B474 8D2F0000 */  lw    $t7, ($t1)
/* 0AFFA8 7F07B478 C5F203C8 */  lwc1  $f18, 0x3c8($t7)
/* 0AFFAC 7F07B47C E6120004 */  swc1  $f18, 4($s0)
/* 0AFFB0 7F07B480 8D380000 */  lw    $t8, ($t1)
/* 0AFFB4 7F07B484 C70403CC */  lwc1  $f4, 0x3cc($t8)
/* 0AFFB8 7F07B488 E6040008 */  swc1  $f4, 8($s0)
/* 0AFFBC 7F07B48C 8F3964AC */  lw    $t9, %lo(g_IntroSwirl)($t9)
/* 0AFFC0 7F07B490 032C5821 */  addu  $t3, $t9, $t4
/* 0AFFC4 7F07B494 8D6D0004 */  lw    $t5, 4($t3)
/* 0AFFC8 7F07B498 31AE0004 */  andi  $t6, $t5, 4
/* 0AFFCC 7F07B49C 55C0000E */  bnezl $t6, .L7F07B4D8
/* 0AFFD0 7F07B4A0 8D790024 */   lw    $t9, 0x24($t3)
/* 0AFFD4 7F07B4A4 8D6F0024 */  lw    $t7, 0x24($t3)
/* 0AFFD8 7F07B4A8 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0AFFDC 7F07B4AC 31F80004 */  andi  $t8, $t7, 4
/* 0AFFE0 7F07B4B0 57000006 */  bnezl $t8, .L7F07B4CC
/* 0AFFE4 7F07B4B4 44814000 */   mtc1  $at, $f8
/* 0AFFE8 7F07B4B8 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0AFFEC 7F07B4BC 44810000 */  mtc1  $at, $f0
/* 0AFFF0 7F07B4C0 1000000C */  b     .L7F07B4F4
/* 0AFFF4 7F07B4C4 8D2E0000 */   lw    $t6, ($t1)
/* 0AFFF8 7F07B4C8 44814000 */  mtc1  $at, $f8
.L7F07B4CC:
/* 0AFFFC 7F07B4CC 10000008 */  b     .L7F07B4F0
/* 0B0000 7F07B4D0 46024001 */   sub.s $f0, $f8, $f2
/* 0B0004 7F07B4D4 8D790024 */  lw    $t9, 0x24($t3)
.L7F07B4D8:
/* 0B0008 7F07B4D8 332D0004 */  andi  $t5, $t9, 4
/* 0B000C 7F07B4DC 51A00004 */  beql  $t5, $zero, .L7F07B4F0
/* 0B0010 7F07B4E0 46001006 */   mov.s $f0, $f2
/* 0B0014 7F07B4E4 10000002 */  b     .L7F07B4F0
/* 0B0018 7F07B4E8 46007006 */   mov.s $f0, $f14
/* 0B001C 7F07B4EC 46001006 */  mov.s $f0, $f2
.L7F07B4F0:
/* 0B0020 7F07B4F0 8D2E0000 */  lw    $t6, ($t1)
.L7F07B4F4:
/* 0B0024 7F07B4F4 3C014220 */  li    $at, 0x42200000 # 40.000000
/* 0B0028 7F07B4F8 44811000 */  mtc1  $at, $f2
/* 0B002C 7F07B4FC C5CA04C0 */  lwc1  $f10, 0x4c0($t6)
/* 0B0030 7F07B500 C6060000 */  lwc1  $f6, ($s0)
/* 0B0034 7F07B504 C6080004 */  lwc1  $f8, 4($s0)
/* 0B0038 7F07B508 46025402 */  mul.s $f16, $f10, $f2
/* 0B003C 7F07B50C 00000000 */  nop
/* 0B0040 7F07B510 46008482 */  mul.s $f18, $f16, $f0
/* 0B0044 7F07B514 46123100 */  add.s $f4, $f6, $f18
/* 0B0048 7F07B518 E6040000 */  swc1  $f4, ($s0)
/* 0B004C 7F07B51C 8D2F0000 */  lw    $t7, ($t1)
/* 0B0050 7F07B520 C6040008 */  lwc1  $f4, 8($s0)
/* 0B0054 7F07B524 C5EA04C4 */  lwc1  $f10, 0x4c4($t7)
/* 0B0058 7F07B528 46025402 */  mul.s $f16, $f10, $f2
/* 0B005C 7F07B52C 00000000 */  nop
/* 0B0060 7F07B530 46008182 */  mul.s $f6, $f16, $f0
/* 0B0064 7F07B534 46064480 */  add.s $f18, $f8, $f6
/* 0B0068 7F07B538 E6120004 */  swc1  $f18, 4($s0)
/* 0B006C 7F07B53C 8D380000 */  lw    $t8, ($t1)
/* 0B0070 7F07B540 C70A04C8 */  lwc1  $f10, 0x4c8($t8)
/* 0B0074 7F07B544 46025402 */  mul.s $f16, $f10, $f2
/* 0B0078 7F07B548 00000000 */  nop
/* 0B007C 7F07B54C 46008202 */  mul.s $f8, $f16, $f0
/* 0B0080 7F07B550 46082180 */  add.s $f6, $f4, $f8
/* 0B0084 7F07B554 E6060008 */  swc1  $f6, 8($s0)
/* 0B0088 7F07B558 8FBF002C */  lw    $ra, 0x2c($sp)
/* 0B008C 7F07B55C 8FB00028 */  lw    $s0, 0x28($sp)
/* 0B0090 7F07B560 27BD0090 */  addiu $sp, $sp, 0x90
/* 0B0094 7F07B564 03E00008 */  jr    $ra
/* 0B0098 7F07B568 00000000 */   nop
)
#endif






/**
 * US address 7F07B56C.
 * JP address 7F07BB8C.
 * EU address 7F07B604.
*/
void bondviewFrozenCameraTick(u16 buttons, u16 oldbuttons, struct coord3d *pos, struct coord3d *pos2, struct coord3d *offset, StandTile **stan, struct coord3d *arg6)
{
    s32 i;
    f32 sp38;
    s32 i2;
    f32 sp30 = 0.0f;
    s32 padding2;
    f32 zero = 0.0f;
    void *p;
    struct PadRecord *setupPad;

    if ((g_CameraMode == CAMERAMODE_INTRO) || (g_CameraMode == CAMERAMODE_FADESWIRL))
    {
        if (g_CameraMode == CAMERAMODE_INTRO)
        {
            if ((camera_transition_timer < 120.0f) && ((camera_transition_timer + g_GlobalTimerDelta) >= 120.0f))
            {
#if defined(VERSION_US)
                setFontTables(ptrFontZurichBoldChars, ptrFontZurichBold);
                hudmsgBottomShow(ptr_random06cam_entry->lang1c.lang_ptr);
#else
                hudmsgBottomShow(ptr_random06cam_entry->lang1c.lang_ptr, ptrFontZurichBoldChars, ptrFontZurichBold);
#endif
            }

            if (ptr_random06cam_entry->lang20.lang_ptr != NULL)
            {
                if ((camera_transition_timer < 300.0f) && ((camera_transition_timer + g_GlobalTimerDelta) >= 300.0f))
                {
#if defined(VERSION_US)
                    hudmsgBottomShow(ptr_random06cam_entry->lang20.lang_ptr);
#else
                    hudmsgBottomShow(ptr_random06cam_entry->lang20.lang_ptr, ptrFontZurichBoldChars, ptrFontZurichBold);
#endif
                }

                if (camera_transition_timer > 480.0f)
                {
                    g_CameraAfterCinema = CAMERAMODE_INTRO;
                }
            }
            else if (camera_transition_timer > 300.0f)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }

            camera_transition_timer += g_GlobalTimerDelta;

            if ((lvlGetControlsLockedFlag() == 0)
                && (buttons & ~oldbuttons & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON | CONT_R | CONT_L)))
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }
        }
        else if (g_CurrentPlayer->colourfadetimemax60 < 0.0f)
        {
            g_CameraAfterCinema = CAMERAMODE_INTRO;
        }

        pos->f[0] = ptr_random06cam_entry->unk04.fval;
        pos->f[1] = ptr_random06cam_entry->unk08.fval;
        pos->f[2] = ptr_random06cam_entry->unk0C.fval;

        pos2->f[0] = pos->f[0] + (cosf(ptr_random06cam_entry->unk14.fval) * sinf(ptr_random06cam_entry->unk10.fval));
        pos2->f[1] = pos->f[1] + sinf(ptr_random06cam_entry->unk14.fval);
        pos2->f[2] = pos->f[2] - (cosf(ptr_random06cam_entry->unk14.fval) * cosf(ptr_random06cam_entry->unk10.fval));

        p = &g_CurrentSetup.pads[ptr_random06cam_entry->unk18];
        setupPad = p;
        *stan = setupPad->stan;

        arg6->f[0] = setupPad->pos.f[0];
        arg6->f[1] = setupPad->pos.f[1];
        arg6->f[2] = setupPad->pos.f[2];
    }
    else if (g_CameraMode == CAMERAMODE_MP)
    {
        /**
        * CAMERAMODE_MP: Perfect Dark method playerTickMpSwirl
        */
        if (get_player_position_in_shuffled(get_cur_playernum()) == 0)
        {
            for (i2=0; i2<g_ClockTimer; i2++)
            {
                if (g_MpSwirlAngleDegrees < 179.5f)
                {
                    if (g_MpSwirlAngleDegrees < -20.0f)
                    {
                        g_MpSwirlRotateSpeed += 0.1f;
                    }

                    if (g_MpSwirlAngleDegrees > 110.0f)
                    {
                        g_MpSwirlRotateSpeed -= 0.1f;
                    }

                    g_MpSwirlAngleDegrees += g_MpSwirlRotateSpeed;
                }
#if defined(VERSION_EU)
                if (g_MpSwirlAngleDegrees >= 179.5f)
#else
                else
#endif
                {
                    g_MpSwirlAngleDegrees = 180.0f;
                }

                if (g_MpSwirlAngleDegrees > 80.0f)
                {
                    if (g_MpSwirlDistance > 60.0f)
                    {
                        g_MpSwirlForwardSpeed -= 0.1f;
                    }
                    else
                    {
                        g_MpSwirlForwardSpeed += 0.015f;
                    }

                    g_MpSwirlDistance += g_MpSwirlForwardSpeed;

                    if (g_MpSwirlDistance < 1.0f)
                    {
                        g_MpSwirlDistance = 1.0f;
                    }
                }
            }
        }

        sp38 = ((g_MpSwirlAngleDegrees - g_CurrentPlayer->vv_theta) * M_PI_F) / 180.0f;

        pos->f[0] = g_CurrentPlayer->field_488.pos.f[0] + (sinf(sp38) * g_MpSwirlDistance);
        pos->f[1] = g_CurrentPlayer->field_488.pos.f[1] + (g_MpSwirlDistance * 0.08f);
        pos->f[2] = g_CurrentPlayer->field_488.pos.f[2] + (cosf(sp38) * g_MpSwirlDistance);

        pos2->f[0] = g_CurrentPlayer->field_488.pos.f[0];
        pos2->f[1] = g_CurrentPlayer->field_488.pos.f[1];
        pos2->f[2] = g_CurrentPlayer->field_488.pos.f[2];

        *stan = g_CurrentPlayer->prop->stan;

        arg6->f[0] = g_CurrentPlayer->field_488.pos.f[0];
        arg6->f[1] = g_CurrentPlayer->field_488.pos.f[1] + (g_MpSwirlDistance * 0.08f);
        arg6->f[2] = g_CurrentPlayer->field_488.pos.f[2];

#if defined(VERSION_EU)
        if (((get_player_position_in_shuffled(get_cur_playernum()) + 1) == getPlayerCount()) && (g_MpSwirlDistance < 5.0f))
        {
            g_CameraAfterCinema = CAMERAMODE_INTRO;
        }
#else
        if (g_MpSwirlDistance < 5.0f)
        {
            g_CameraAfterCinema = CAMERAMODE_INTRO;
        }
#endif
    }
    else if (g_CameraMode == CAMERAMODE_SWIRL)
    {
        camera_transition_timer += g_GlobalTimerDelta;

        while (g_IntroSwirl[intro_camera_index].unk18.fval <= camera_transition_timer)
        {
            if (!(g_IntroSwirl[intro_camera_index + 3].unk04 & 1))
            {
                camera_transition_timer -= g_IntroSwirl[intro_camera_index].unk18.fval;
                intro_camera_index++;
            }
            else
            {
                camera_transition_timer = g_IntroSwirl[intro_camera_index].unk18.fval;
                g_CameraAfterCinema = CAMERAMODE_INTRO;
                break;
            }
        }

        sp30 += (g_IntroSwirl[intro_camera_index].unk18.fval - camera_transition_timer);

        for (i = intro_camera_index + 1; !(g_IntroSwirl[i+2].unk04 & 1); i++)
        {
            sp30 += g_IntroSwirl[i].unk18.fval;
        }

        if ((sp30 < 30.0f) && ((sp30 + g_GlobalTimerDelta) >= 30.0f))
        {
            currentPlayerStartChrFade(30.0f, 0.0f);
        }

        if (camera_fade_active != 0)
        {
            if (currentPlayerIsFadeComplete() != 0)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }
        }

        if ((sp30 > 60.0f) && (camera_fade_active == 0))
        {
            if ((lvlGetControlsLockedFlag() == 0)
                && (buttons & ~oldbuttons & (A_BUTTON | B_BUTTON | Z_TRIG | START_BUTTON | L_TRIG | R_TRIG)))
            {
                camera_fade_active = 1;
                currentPlayerSetFadeColour(0, 0, 0, g_CurrentPlayer->colourscreenfrac);

                if (currentPlayerIsFadeComplete() != 0)
                {
                    currentPlayerSetFadeFrac(60.0f, 1.0f);
                }
                else
                {
                    currentPlayerSetFadeFrac(g_CurrentPlayer->colourfadetime60, 1.0f);
                }
            }
        }

        sub_GAME_7F07B2A0(intro_camera_index, camera_transition_timer, pos, pos2);

        if (g_IntroSwirl[intro_camera_index].unk1C >= 0)
        {
            p = &g_CurrentSetup.pads[g_IntroSwirl[intro_camera_index].unk1C];
            setupPad = p;
            *stan = setupPad->stan;

            arg6->f[0] = setupPad->pos.f[0];
            arg6->f[1] = setupPad->pos.f[1];
            arg6->f[2] = setupPad->pos.f[2];
        }
        else
        {
            *stan = g_CurrentPlayer->field_488.current_tile_ptr;
            arg6->f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
            arg6->f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
            arg6->f[2] = g_CurrentPlayer->field_488.collision_position.f[2];
        }
    }
    else if ((g_CameraMode == CAMERAMODE_DEATH_CAM_SP) || (g_CameraMode == CAMERAMODE_DEATH_CAM_MP))
    {
        if (g_CameraMode == CAMERAMODE_DEATH_CAM_SP)
        {
            camera_transition_timer += g_GlobalTimerDelta;

            if (g_CurrentPlayer->ptr_char_objectinstance != NULL)
            {
                if (objecthandlerGetModelField28((Model *) g_CurrentPlayer->ptr_char_objectinstance)
                    >= sub_GAME_7F06F5C4((Model *) g_CurrentPlayer->ptr_char_objectinstance))
                {
                    g_CameraAfterCinema = CAMERAMODE_INTRO;
                }
            }
            else if (camera_transition_timer >= 180.0f)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }

            if ((buttons & ~oldbuttons & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON))
                && (g_CurrentPlayer->bonddead)
                && (g_CurrentPlayer->redbloodfinished)
                && (g_CurrentPlayer->deathanimfinished))
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
                camera_mode = CAMERAMODE_FADESWIRL;
            }
        }
        else if (g_CameraMode == CAMERAMODE_DEATH_CAM_MP)
        {
            camera_transition_timer += g_GlobalTimerDelta;

            if (g_CurrentPlayer->colourfadetimemax60 < 0.0f)
            {
                g_CameraAfterCinema = CAMERAMODE_INTRO;
            }

            if ((buttons & ~oldbuttons & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON))
                && (g_CurrentPlayer->bonddead)
                && (g_CurrentPlayer->redbloodfinished)
                && (g_CurrentPlayer->deathanimfinished))
            {
                camera_mode = CAMERAMODE_FADESWIRL;
            }
        }

        pos->f[0] = flt_CODE_bss_800799E8.f[0];
        pos->f[1] = flt_CODE_bss_800799E8.f[1];
        pos->f[2] = flt_CODE_bss_800799E8.f[2];

        if (dword_CODE_bss_800799F4 == g_CurrentPlayer->prop)
        {
            pos2->f[0] = g_CurrentPlayer->field_3C4;
            pos2->f[1] = g_CurrentPlayer->field_3C8;
            pos2->f[2] = g_CurrentPlayer->field_3CC;
        }
        else
        {
            pos2->f[0] = dword_CODE_bss_800799F4->pos.f[0];
            pos2->f[1] = dword_CODE_bss_800799F4->pos.f[1];
            pos2->f[2] = dword_CODE_bss_800799F4->pos.f[2];
        }

        *stan = g_CurrentPlayer->field_488.current_tile_ptr;

        arg6->f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
        arg6->f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
        arg6->f[2] = g_CurrentPlayer->field_488.collision_position.f[2];
    }
    else if (g_CameraMode == CAMERAMODE_POSEND)
    {
        if (dword_CODE_bss_800799F8 != NULL)
        {
            pos->f[0] = dword_CODE_bss_800799F8->pos.f[0];
            pos->f[1] = dword_CODE_bss_800799F8->pos.f[1];
            pos->f[2] = dword_CODE_bss_800799F8->pos.f[2];

            pos2->f[0] = g_CurrentPlayer->field_3C4;
            pos2->f[1] = g_CurrentPlayer->field_3C8;
            pos2->f[2] = g_CurrentPlayer->field_3CC;

            *stan = dword_CODE_bss_800799F8->stan;

            arg6->f[0] = dword_CODE_bss_800799F8->pos.f[0];
            arg6->f[1] = dword_CODE_bss_800799F8->pos.f[1];
            arg6->f[2] = dword_CODE_bss_800799F8->pos.f[2];

            return;
        }

        if (gBondViewCutscene != NULL)
        {
            if (isNotBoundPad(gBondViewCutscene->pad))
            {
                p = &g_CurrentSetup.pads[gBondViewCutscene->pad];
            }
            else
            {
                p = &g_CurrentSetup.boundpads[getBoundPadNum(gBondViewCutscene->pad)];
            }

            pos->f[0] = gBondViewCutscene->pos.f[0];
            pos->f[1] = gBondViewCutscene->pos.f[1];
            pos->f[2] = gBondViewCutscene->pos.f[2];

            setupPad = p;
            *stan = setupPad->stan;

            arg6->f[0] = setupPad->pos.f[0];
            arg6->f[1] = setupPad->pos.f[1];
            arg6->f[2] = setupPad->pos.f[2];

            if (dword_CODE_bss_80079A18 == CAMERAMODE_INTRO)
            {
                pos2->f[0] = g_CurrentPlayer->field_3C4;
                pos2->f[1] = g_CurrentPlayer->field_3C8;
                pos2->f[2] = g_CurrentPlayer->field_3CC;
            }
            else
            {
                pos2->f[0] = pos->f[0] + (cosf(gBondViewCutscene->verta) * sinf(gBondViewCutscene->theta));
                pos2->f[1] = pos->f[1] + sinf(gBondViewCutscene->verta);
                pos2->f[2] = pos->f[2] - (cosf(gBondViewCutscene->verta) * cosf(gBondViewCutscene->theta));
            }

            return;
        }

        if (isNotBoundPad(dword_CODE_bss_80079A14))
        {
            setupPad = &g_CurrentSetup.pads[dword_CODE_bss_80079A14];
        }
        else
        {
            setupPad = (struct PadRecord*)&g_CurrentSetup.boundpads[getBoundPadNum(dword_CODE_bss_80079A14)];
        }

        *stan = setupPad->stan;

        arg6->f[0] = setupPad->pos.f[0];
        arg6->f[1] = setupPad->pos.f[1];
        arg6->f[2] = setupPad->pos.f[2];

        pos2->f[0] = setupPad->pos.f[0] + cosf(flt_CODE_bss_80079A00) * 0.0f;
        pos2->f[1] = setupPad->pos.f[1] + flt_CODE_bss_80079A10;
        pos2->f[2] = setupPad->pos.f[2] + sinf(flt_CODE_bss_80079A00) * 0.0f;

        pos->f[0] = setupPad->pos.f[0] + (sinf(flt_CODE_bss_80079A00) * flt_CODE_bss_80079A08) + cosf(flt_CODE_bss_80079A00) * 0.0f;
        pos->f[1] = setupPad->pos.f[1] + flt_CODE_bss_80079A10 + flt_CODE_bss_80079A0C;
        pos->f[2] = setupPad->pos.f[2] + (cosf(flt_CODE_bss_80079A00) * flt_CODE_bss_80079A08) + sinf(flt_CODE_bss_80079A00) * 0.0f;

        flt_CODE_bss_80079A00 += flt_CODE_bss_80079A04 * g_GlobalTimerDelta;

        while (flt_CODE_bss_80079A00 >= M_TAU_F)
        {
            flt_CODE_bss_80079A00 -= M_TAU_F;
        }

        while (flt_CODE_bss_80079A00 < 0.0f)
        {
            flt_CODE_bss_80079A00 += M_TAU_F;
        }
    }
}



//begin bondmove.c per pd

void sub_GAME_7F07C540(s32 arg0)
{
    g_CurrentPlayer->field_42c = arg0;
}


void currentPlayerSetLookAheadSetting(bool enabled)
{
    g_CurrentPlayer->automovecentreenabled = enabled;
}


/**
 * Unreferenced
 */
bool currentPlayerGetLookAheadSetting(void)
{
    return g_CurrentPlayer->automovecentreenabled;
}


void currentPlayerSetYAutoAimEnabled(bool enabled)
{
  g_CurrentPlayer->autoyaimenabled = enabled;
}


/**
 * Address 0x7F07C580.
 */
bool currentPlayerGetYAutoAimEnabled(void)
{
    if (getPlayerCount() == 1)
    {
        return g_CurrentPlayer->autoyaimenabled;
    }

    return (bool) g_playerPerm->autoaim;
}


bool currentPlayerGetYAutoAimEnabledRedirect(void)
{
    return currentPlayerGetYAutoAimEnabled();
}


bool currentPlayerGetIsAiming(void)
{
  return g_CurrentPlayer->insightaimmode;
}


/**
 * Updates autoyaimtime60 by g_ClockTimer.
 * Will update player->autoaimy if new autoyaimtime60 < 0 or autoaim_target != g_CurrentPlayer->autoaim_target_y.
 *
 * Address 0x7F07C5F0.
 */
void bondviewUpdateYAutoAimTime(struct PropRecord *autoaim_target, f32 auto_aim_y)
{
    if (g_CurrentPlayer->autoyaimtime60 >= 0)
    {
        g_CurrentPlayer->autoyaimtime60 = g_CurrentPlayer->autoyaimtime60 - g_ClockTimer;
    }

    if (autoaim_target != g_CurrentPlayer->autoaim_target_y)
    {
        if (g_CurrentPlayer->autoyaimtime60 < 0)
        {
            g_CurrentPlayer->autoyaimtime60 = BONDVIEW_AUTOAIM_TIME;
            g_CurrentPlayer->autoaim_target_y = autoaim_target;
        }
        else
        {
            return;
        }
    }

    g_CurrentPlayer->autoaimy = auto_aim_y;
}


void currentPlayerSetXAutoAimEnabled(bool enabled)
{
  g_CurrentPlayer->autoxaimenabled = enabled;

  return;
}


/**
 * Address 0x7F07C668.
 */
bool currentPlayerGetXAutoAimEnabled(void)
{
    if (getPlayerCount() == 1)
    {
        return g_CurrentPlayer->autoxaimenabled;
    }

    return (bool) g_playerPerm->autoaim;
}


bool currentPlayerGetXAutoAimEnabledRedirect(void)
{
    return currentPlayerGetXAutoAimEnabled();
}


/**
 * Updates autoxaimtime60 by g_ClockTimer.
 * Will update player->autoaimx if new autoxaimtime60 < 0 or autoaim_target_x != g_CurrentPlayer->autoaim_target_x.
 *
 * Address 0x7F07C6C8.
 */
void bondviewUpdateXAutoAimTime(struct PropRecord *autoaim_target, f32 auto_aim_x)
{
    if (g_CurrentPlayer->autoxaimtime60 >= 0)
    {
        g_CurrentPlayer->autoxaimtime60 = g_CurrentPlayer->autoxaimtime60 - g_ClockTimer;
    }

    if (autoaim_target != g_CurrentPlayer->autoaim_target_x)
    {
        if (g_CurrentPlayer->autoxaimtime60 < 0)
        {
            g_CurrentPlayer->autoxaimtime60 = BONDVIEW_AUTOAIM_TIME;
            g_CurrentPlayer->autoaim_target_x = autoaim_target;
        }
        else
        {
            return;
        }
    }

    g_CurrentPlayer->autoaimx = auto_aim_x;

}


void change_player_pos_to_target(struct collision434 *col, coord3d *pos, StandTile *stan)
{
    f32 store_x;
    f32 store_x2;
    f32 store_z;
    f32 store_y;
    col->collision_position.x = pos->x;
    store_x = col->collision_position.x;
    col->collision_position.y = pos->y;
    store_y = col->collision_position.y;
    col->collision_position.z = pos->z;
    store_z = col->collision_position.z;
    store_x2 = pos->x;
    col->current_tile_ptr = stan;
    col->current_tile_ptr_for_portals = stan;
    col->applied_view.y = 0.0f;
    col->applied_view.z = 0.0f;
    col->applied_view2.x = 0.0f;
    col->applied_view2.z = store_x2 * 0.0f;
    col->theta_transform.x = 0.0f;
    col->theta_transform.y = 0.0f;
    col->pos.x = store_x;
    col->pos3.x = store_x;
    col->applied_view.x = 1.0f;
    col->applied_view2.y = 1.0f;
    col->theta_transform.z = 1.0f;
    col->pos.y = store_y;
    col->pos3.y = store_y;
    col->pos.z = store_z;
    col->pos3.z = store_z;
    col->collision_radius = 30;
}


/**
 * US address 7F07C7B4.
*/
void bondviewTankModelRotationRelated(void) {
    struct ObjectRecord *obj;
    struct coord3d *sp68;
    struct coord3d *sp64;
    Mtxf sp24;
    ModelNode **temp_v0;

    if (g_PlayerTankProp != NULL)
    {
        obj = g_PlayerTankProp->obj;

        /// TODO: Fix Model struct Data type.
        temp_v0 = obj->model->obj->Switches;
        sp68 = (struct coord3d *)temp_v0[2]->Data;
        sp64 = (struct coord3d *)temp_v0[1]->Data;
        matrix_4x4_set_rotation_around_y(M_TAU_F - g_TankTurretOrientationAngleRad, &sp24);
        g_TankModelPositionOffset.f[0] = sp68->f[0];
        g_TankModelPositionOffset.f[1] = sp68->f[1];
        g_TankModelPositionOffset.f[2] = sp68->f[2];
        mtx4RotateVecInPlace(&sp24, g_TankModelPositionOffset.f);
        g_TankModelPositionOffset.f[0] += sp64->f[0];
        g_TankModelPositionOffset.f[1] += sp64->f[1];
        g_TankModelPositionOffset.f[2] += sp64->f[2];
    }
}



/**
 * Address 0x7F07C888.
*/
void bondviewGetTankCollisionBounds(struct rect4f *tank_collision_bounds, struct coord3d *collision_position, f32 tank_orientation_angle)
{
    ObjectRecord *sp4C;
    f32 sp48;
    f32 sp44;
    f32 sp40;
    f32 sp3C;
    f32 sp38;
    f32 sp34;
    struct ModelRoData_BoundingBoxRecord *bbox;

    #ifdef DEBUG
    assert(bondonprop2); // canonically g_PlayerTankProp is bondonprop2 - presumably because it was also motorbike code
#endif

    sp4C = g_PlayerTankProp->obj;

    bbox = chrobjGetBboxFromObjectRecord(sp4C);

    sp44 = bbox->Bounds.xmin - g_TankModelPositionOffset.f[0];
    sp40 = bbox->Bounds.xmax - g_TankModelPositionOffset.f[0];

    sp3C = bbox->Bounds.zmin - g_TankModelPositionOffset.f[2];
    sp38 = bbox->Bounds.zmax - g_TankModelPositionOffset.f[2];

    sp34 = sp4C->model->scale * cosf(tank_orientation_angle);
    sp48 = sp4C->model->scale * sinf(tank_orientation_angle);

    tank_collision_bounds->points[0].f[0] = collision_position->f[0] + (-sp3C * sp48) + (sp44 * sp34);
    tank_collision_bounds->points[0].f[1] = collision_position->f[2] + (sp3C * sp34) + (sp44 * sp48);

    tank_collision_bounds->points[1].f[0] = collision_position->f[0] + (-sp3C * sp48) + (sp40 * sp34);
    tank_collision_bounds->points[1].f[1] = collision_position->f[2] + (sp3C * sp34) + (sp40 * sp48);

    tank_collision_bounds->points[2].f[0] = collision_position->f[0] + (-sp38 * sp48) + (sp40 * sp34);
    tank_collision_bounds->points[2].f[1] = collision_position->f[2] + (sp38 * sp34) + (sp40 * sp48);

    tank_collision_bounds->points[3].f[0] = collision_position->f[0] + (-sp38 * sp48) + (sp44 * sp34);
    tank_collision_bounds->points[3].f[1] = collision_position->f[2] + (sp38 * sp34) + (sp44 * sp48);
}



/**
 * Address 0x7F07CA2C.
*/
s32 bondviewTestLineUnobstructed(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, s32 cdtypes, struct coord3d *coord_p, struct coord3d *coord_dest)
{
    s32 temp_v0;

    temp_v0 = stanTestLineUnobstructed(pTile, p_x, p_z, dest_x, dest_z, cdtypes, 0.0f, 1.0f, 0.0f, 1.0f);
    if ((temp_v0 == 0) && (coord_p != NULL))
    {
        coord_p->f[0] = p_x;
        coord_p->f[1] = 0.0f;
        coord_p->f[2] = p_z;
        coord_dest->f[0] = dest_x;
        coord_dest->f[1] = 0.0f;
        coord_dest->f[2] = dest_z;
    }

    return temp_v0;
}





/**
 * Address 0x7F07CAC8.
*/
s32 bondviewTankCollisionStatus(struct coord3d *collision_position, StandTile *arg1, f32 tank_orientation_angle, struct coord3d *arg3, struct coord3d *arg4)
{
    StandTile *spBC;
    s32 stack_padding;
    struct rect4f tank_collision_bounds;
    s32 sp94;
    f32 temp_f0;

    Model *sp8C;

    ModelNode **switches;
    struct coord3d *temp_a1;
    struct coord3d *temp_a2;
    struct coord3d sp74;
    Mtxf sp34;
    struct coord3d *temp_v1;

    spBC = arg1;
    sp94 = 0;

    bondviewGetTankCollisionBounds(&tank_collision_bounds, collision_position, tank_orientation_angle);

    if (g_PlayerTankProp != NULL)
    {
        sub_GAME_7F03D058(g_PlayerTankProp, 0);
    }

    if ((bondviewTestLineUnobstructed(&spBC, collision_position->f[0], collision_position->f[2], tank_collision_bounds.points[0].f[0], tank_collision_bounds.points[0].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, arg3, arg4) != 0)
        && (bondviewTestLineUnobstructed(&spBC, tank_collision_bounds.points[0].f[0], tank_collision_bounds.points[0].f[1], tank_collision_bounds.points[1].f[0], tank_collision_bounds.points[1].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, arg3, arg4) != 0)
        && (bondviewTestLineUnobstructed(&spBC, tank_collision_bounds.points[1].f[0], tank_collision_bounds.points[1].f[1], tank_collision_bounds.points[2].f[0], tank_collision_bounds.points[2].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, arg3, arg4) != 0)
        && (bondviewTestLineUnobstructed(&spBC, tank_collision_bounds.points[2].f[0], tank_collision_bounds.points[2].f[1], tank_collision_bounds.points[3].f[0], tank_collision_bounds.points[3].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, arg3, arg4) != 0)
        && (bondviewTestLineUnobstructed(&spBC, tank_collision_bounds.points[3].f[0], tank_collision_bounds.points[3].f[1], tank_collision_bounds.points[0].f[0], tank_collision_bounds.points[0].f[1], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, arg3, arg4) != 0))
    {
        sp94 = 1;

        if (g_PlayerTankProp != NULL)
        {
            ObjectRecord *obj = g_PlayerTankProp->obj;
            sp8C = obj->model;
            switches = sp8C->obj->Switches;

            temp_v1 = switches[3]->Data;
            temp_a1 = switches[4]->Data;
            temp_a2 = switches[2]->Data;

            sp74.f[0] = temp_a1->f[0] + temp_v1->f[0] - temp_a2->f[0];
            sp74.f[1] = 0.0f;
            sp74.f[2] = temp_a1->f[2] + temp_v1->f[2] - temp_a2->f[2];

            temp_f0 = tank_orientation_angle + g_TankTurretOrientationAngleRad;

            if (temp_f0 >= M_TAU_F)
            {
                temp_f0 -= M_TAU_F;
            }

            if (temp_f0 < 0.0f)
            {
                temp_f0 += M_TAU_F;
            }

            matrix_4x4_set_rotation_around_y(M_TAU_F - temp_f0, &sp34);
            mtx4RotateVecInPlace(&sp34, (f32*)&sp74);

            sp74.f[0] *= sp8C->scale;
            sp74.f[2] *= sp8C->scale;

            sp74.f[0] += collision_position->f[0];
            sp74.f[2] += collision_position->f[2];

            spBC = arg1;

            if (bondviewTestLineUnobstructed(&spBC, collision_position->f[0], collision_position->f[2], sp74.f[0], sp74.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_OBJSIMMUNETOEXPLOSIONS, arg3, arg4) == 0)
            {
                sp94 = 0;
            }

        }
    }

    if (g_PlayerTankProp != NULL)
    {
        sub_GAME_7F03D058(g_PlayerTankProp, 1);
    }

    return sp94;
}




/**
 * Address 0x7F07CDA8.
*/
s32 bondviewCallTankCollisionStatus(struct coord3d *collision_position, StandTile *arg1, f32 tank_orientation_angle)
{
    return bondviewTankCollisionStatus(collision_position, arg1, tank_orientation_angle, NULL, NULL);
}





/**
 * Address 0x7F07CDD4.
*/
s32 sub_GAME_7F07CDD4(struct coord3d *arg0, f32 arg1, StandTile **arg2)
{
    StandTile *sp3C;
    s32 unused_padding[2];

    sp3C = g_CurrentPlayer->field_488.current_tile_ptr;

    if ((
        stanTestLineUnobstructed(
            &sp3C,
            g_CurrentPlayer->field_488.collision_position.f[0],
            g_CurrentPlayer->field_488.collision_position.f[2],
            arg0->f[0],
            arg0->f[2],
            0,
            0.0f,
            1.0f,
            0.0f,
            1.0f) != 0)
        && (bondviewCallTankCollisionStatus(arg0, sp3C, arg1) != 0))
    {
        *arg2 = sp3C;
        return 1;
    }

    return 0;
}





bool isBondInTank(void)
{
    return g_PlayerIsInTank;
}






struct PropRecord *get_ptr_for_players_tank(void)
{
    if (g_PlayerIsInTank == 1)
    {
        return g_PlayerTankProp;
    }

    return 0;
}





/**
 * Sets paraameter position based on global variables g_TankOrientationAngle, g_TankTurretOrientationAngleRad, g_TankTurretVerticalAngle.
 *
 * Address 0x7F07CEB0.
 */
void bondviewSet3dCoord7F07CEB0(coord3d *arg0)
{
    f32 f;

    f = g_TankOrientationAngle + g_TankTurretOrientationAngleRad;

    if (f >= M_TAU_F)
    {
        f = f - M_TAU_F;
    }

    if (f < 0.0f)
    {
        f = f + M_TAU_F;
    }

    arg0->f[0] = -sinf(f) * cosf(g_TankTurretVerticalAngle);
    arg0->f[1] = sinf(g_TankTurretVerticalAngle);
    arg0->f[2] = cosf(f) * cosf(g_TankTurretVerticalAngle);
}






/**
 * Unreferenced.
 *
 * Returns global variable g_TankTurretVerticalAngle, which is in radians.
 *
 * Address 0x7F07CF80.
 */
f32 bondviewGet8003646CRad(void)
{
    return g_TankTurretVerticalAngle;
}



/**
 * Address 0x7F07CF8C.
*/
s32 bondviewTryMoveToStan(struct coord3d *arg0, StandTile **stan)
{
    s32 sp94;
    StandTile *sp90;
    s32 cdtypes;
    f32 height;
    f32 always_30;
    f32 collision_radius;
    s32 sp7C;
    struct TankRecord *tank;
    s32 stack_padding[11];
    struct StandTileLocusCallbackRecord sp3C;

    sp94 = 0;

    if ((g_PlayerIsInTank == 1) && (g_EnterTankAudioState != TANK_RUN_STATE_NOT_RUNNING))
    {
        sp94 = sub_GAME_7F07CDD4(arg0, g_TankOrientationAngle, stan);
    }
    else
    {
        sp90 = g_CurrentPlayer->field_488.current_tile_ptr;

        if (obj_collision_flag)
        {
            cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
        }
        else
        {
            cdtypes = 0;
        }

        bondviewGetCollisionRadius(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

        if (g_WorldTankProp != NULL)
        {
            sub_GAME_7F03D058(g_WorldTankProp, 0);
        }

        sub_GAME_7F03D058(g_CurrentPlayer->prop, 0);
        sp7C = stanTileDistanceRelated(&sp90, arg0->f[0], arg0->f[2], collision_radius, &sp3C);

        if (stanGetLocusField0(&sp3C) != 0)
        {
            g_CurrentPlayer->autocrouchpos = CROUCH_SQUAT;
        }

        if ((stanTestLineUnobstructed(
                &sp90,
                g_CurrentPlayer->field_488.collision_position.f[0],
                g_CurrentPlayer->field_488.collision_position.f[2],
                arg0->f[0],
                arg0->f[2],
                cdtypes,
                height,
                always_30,
                0.0f,
                1.0f) != 0)
            && stanTestVolume(&sp90, arg0->f[0], arg0->f[2], collision_radius, cdtypes, height, always_30) < 0)
        {
            if (g_CurrentPlayer->ducking_height_offset == FULL_CROUCH_OFFSET || sp7C < 0)
            {
                if (stanGetLocusCount(&sp3C) == 0 && stanTestLocusEdgeAboveY(&sp90, arg0->f[0], arg0->f[2], collision_radius, g_CurrentPlayer->field_488.collision_position.f[1] + 175.0f) >= 0)
                {
                    goto block_20;
                }
                else
                {
                    *stan = sp90;
                    sp94 = 1;
                }
            }
            else
            {
                goto block_20;
            }
        }
        else
        {
block_20:
            /* I'm sorry, this is the only way I could make it match. */
            if (g_PlayerTankProp == NULL
                && (stanSavedColl_posData != NULL)
                && (stanSavedColl_posData->type == PROP_TYPE_OBJ))
            {
                tank = (struct TankRecord *)stanSavedColl_posData->obj;
                if (tank->type == PROPDEF_TANK)
                {
                    g_WorldTankProp = stanSavedColl_posData;
                }
            }
        }

        sub_GAME_7F03D058(g_CurrentPlayer->prop, 1);

        if (g_WorldTankProp != NULL)
        {
            sub_GAME_7F03D058(g_WorldTankProp, 1);
        }
    }

    return sp94;
}



/**
 * Calculates collision with current player.
 *
 * @param next_pos: 3d coordinate to attempt to move to.
 * @param collision_pt0: Out parameter. Will set {x,0,z} position of first point (from line edge) if Bond is in collision, otherwise {0}.
 * @param collision_pt1: Out parameter. Will set {x,0,z} position of second point (from line edge) if Bond is in collision, otherwise ... Bond's look angle?
 *
 * @return 1 if able to update stan and collision position, zero otherwise.
 *
 * Address 0x7F07D234.
 */
s32 bondviewTrySimpleMovePlayerCollision(coord3d *next_pos, coord3d *collision_pt0, coord3d *collision_pt1)
{
    struct StandTile *stan;

    // resets stan global collision variables
    stanResetHits();

    if (bondviewTryMoveToStan(next_pos, &stan) != 0)
    {
        g_CurrentPlayer->field_488.current_tile_ptr = stan;
        g_CurrentPlayer->field_488.collision_position.f[0] = next_pos->f[0];
        g_CurrentPlayer->field_488.collision_position.f[2] = next_pos->f[2];

        return 1;
    }

    getCollisionEdge_maybe(collision_pt0, collision_pt1);

    return 0;
}


/**
 * This is a fallback method used when bondviewTrySimpleMovePlayerCollision fails.
 * Instead of moving to the full coordinate specified by `next_pos`, it will
 * scale the position using `calculateRayToSegmentIntersectionNormalized` and try to move to that position.
 *
 * @param next_pos: 3d coordinate to attempt to move to.
 * @param collision1_pt0: Prior collision point 0.
 * @param collision1_pt1: Prior collision point 1.
 * @param collision2_pt0: Out parameter. Will set {x,0,z} position of first point (from line edge) if Bond is in collision, otherwise {0}.
 * @param collision2_pt1: Out parameter. Will set {x,0,z} position of second point (from line edge) if Bond is in collision, otherwise ... Bond's look angle?
 *
 * @return 1 if able to update stan and collision position, zero if still unable to move by failing on the same collision edge, -1 otherwise (still unable to move).
 *
 * US address 7F07D2B4.
 * Perfect Dark, see bondwalk.c bwalk0f0c47d0, bondbike.c bbike0f0d36d4.
*/
s32 bondviewTryFractionMovePlayerCollision(
    struct coord3d *next_pos,
    struct coord3d *collision1_pt0,
    struct coord3d *collision1_pt1,
    struct coord3d *collision2_pt0,
    struct coord3d *collision2_pt1)
{
    StandTile *stan;
    f32 height;
    f32 always_30;
    struct coord3d try_next_pos;
    struct coord3d delta_pos;
    struct coord3d sp50;
    struct coord2d sp48;
    struct coord2d sp40;
    struct coord2d sp38;
    f32 temp_f0;
    f32 collision_radius;

    bondviewGetCollisionRadius(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

    delta_pos.f[0] = next_pos->f[0] - g_CurrentPlayer->field_488.collision_position.f[0];
    delta_pos.f[2] = next_pos->f[2] - g_CurrentPlayer->field_488.collision_position.f[2];

    sp50.f[0] = collision_radius;
    sp50.f[1] = g_CurrentPlayer->field_488.collision_position.f[0];
    sp50.f[2] = g_CurrentPlayer->field_488.collision_position.f[2];

    sp48.f[0] = collision1_pt0->f[0];
    sp48.f[1] = collision1_pt0->f[2];

    sp40.f[0] = collision1_pt1->f[0];
    sp40.f[1] = collision1_pt1->f[2];

    sp38.f[0] = delta_pos.f[0];
    sp38.f[1] = delta_pos.f[2];

    temp_f0 = calculateRayToSegmentIntersectionNormalized(&sp50, &sp48, &sp40, &sp38);

    try_next_pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0] + (delta_pos.f[0] * temp_f0 * 0.25f);
    try_next_pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2] + (delta_pos.f[2] * temp_f0 * 0.25f);

    if (bondviewTryMoveToStan(&try_next_pos, &stan) != 0)
    {
        g_CurrentPlayer->field_488.current_tile_ptr = stan;
        g_CurrentPlayer->field_488.collision_position.f[0] = try_next_pos.f[0];
        g_CurrentPlayer->field_488.collision_position.f[2] = try_next_pos.f[2];

        return 1;
    }

    getCollisionEdge_maybe(collision2_pt0, collision2_pt1);

    if (collision2_pt0->f[0] != collision1_pt0->f[0]
        || collision2_pt0->f[1] != collision1_pt0->f[1]
        || collision2_pt0->f[2] != collision1_pt0->f[2]
        || collision2_pt1->f[0] != collision1_pt1->f[0]
        || collision2_pt1->f[1] != collision1_pt1->f[1]
        || collision2_pt1->f[2] != collision1_pt1->f[2])
    {
        return 0;
    }

    return -1;
}




/**
 * This is a fallback method used when bondviewTrySimpleMovePlayerCollision fails.
 * Instead of moving to the full coordinate specified by `next_pos`, it will
 * attempt to move along the collision edge.
 *
 * @param prior_next_pos: Prior 3d coordinate that Bond failed to move to.
 * @param collision1_pt0: Prior collision point 0.
 * @param collision1_pt1: Prior collision point 1.
 *
 * @return -1 if either x or z coordinates are the same for the collision points, 1 if able to update stan and collision position, zero otherwise.
 *
 * US address 7F07D4C0.
 */
s32 bondviewTryEdgeMovePlayerCollision(struct coord3d *prior_next_pos, struct coord3d *collision_pt0, struct coord3d *collision_pt1)
{
    struct coord3d delta_pos;
    f32 tempf;
    struct coord3d norm_collision_edge;
    struct coord3d try_next_pos;
    StandTile *stan;

    delta_pos.f[0] = prior_next_pos->f[0] - g_CurrentPlayer->field_488.collision_position.f[0];
    delta_pos.f[2] = prior_next_pos->f[2] - g_CurrentPlayer->field_488.collision_position.f[2];

    if (collision_pt0->f[0] != collision_pt1->f[0] || collision_pt0->f[2] != collision_pt1->f[2])
    {
        norm_collision_edge.f[0] = collision_pt1->f[0] - collision_pt0->f[0];
        norm_collision_edge.f[2] = collision_pt1->f[2] - collision_pt0->f[2];

        tempf = (norm_collision_edge.f[0] * norm_collision_edge.f[0]) + (norm_collision_edge.f[2] * norm_collision_edge.f[2]);
        tempf =  1.0f / sqrtf(tempf);
        norm_collision_edge.f[0] *= tempf;
        norm_collision_edge.f[2] *= tempf;

        /**
         * Normalizing gives you the direction vector of the wall, so the dot product in the assignment to
         * tempf gives you the distance moved along the direction of the wall.
         * Then try_next_pos is simply the point of the collision with the wall, plus the
         * length moved along the wall times the direction vector of the wall.
         **/
        tempf = (delta_pos.f[0] * norm_collision_edge.f[0]) + (delta_pos.f[2] * norm_collision_edge.f[2]);
        try_next_pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0] + (tempf * norm_collision_edge.f[0]);
        try_next_pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2] + (tempf * norm_collision_edge.f[2]);

        if (bondviewTryMoveToStan(&try_next_pos, &stan))
        {
            g_CurrentPlayer->field_488.current_tile_ptr = stan;
            g_CurrentPlayer->field_488.collision_position.f[0] = try_next_pos.f[0];
            g_CurrentPlayer->field_488.collision_position.f[2] = try_next_pos.f[2];

            return 1;
        }

        return 0;
    }

    return -1;
}



/**
 * This is a fallback method used when bondviewTrySimpleMovePlayerCollision fails.
 * If Bond previously failed to move because of a collision, this will check
 * if Bond is near the end point of the collision edge. If within the collision
 * radius of Bond to the edge endpoint, then allow movement.
 *
 * @param prior_next_pos: Prior 3d coordinate that Bond failed to move to.
 * @param collision1_pt0: Prior collision point 0.
 * @param collision1_pt1: Prior collision point 1.
 *
 * @return 1 if able to update stan and collision position, zero otherwise.
 *
 * US address 7F07D61C.
 *
 * Perfect Dark bwalk0f0c4a5c.
*/
s32 bondviewTryEndHopPlayerCollision(struct coord3d *prior_next_pos, struct coord3d *collision_pt0, struct coord3d *collision_pt1)
{
    struct coord3d delta_pos;
    struct coord3d sp50;
    struct coord3d try_next_pos;
    f32 height;
    f32 always_30;
    f32 tempf;
    StandTile *stan;
    f32 collision_radius;

    bondviewGetCollisionRadius(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

    delta_pos.f[0] = prior_next_pos->f[0] - g_CurrentPlayer->field_488.collision_position.f[0];
    delta_pos.f[2] = prior_next_pos->f[2] - g_CurrentPlayer->field_488.collision_position.f[2];

    sp50.f[0] = collision_pt0->f[0] - prior_next_pos->f[0];
    sp50.f[2] = collision_pt0->f[2] - prior_next_pos->f[2];

    if (((sp50.f[0] * sp50.f[0]) + (sp50.f[2] * sp50.f[2])) <= (collision_radius * collision_radius))
    {
        if (collision_pt0->f[0] != g_CurrentPlayer->field_488.collision_position.f[0] || collision_pt0->f[2] != g_CurrentPlayer->field_488.collision_position.f[2])
        {
            sp50.f[0] = -(collision_pt0->f[2] - g_CurrentPlayer->field_488.collision_position.f[2]);
            sp50.f[2] = collision_pt0->f[0] - g_CurrentPlayer->field_488.collision_position.f[0];

            tempf = (sp50.f[0] * sp50.f[0]) + (sp50.f[2] * sp50.f[2]);
            tempf =  1.0f / sqrtf(tempf);
            sp50.f[0] *= tempf;
            sp50.f[2] *= tempf;

            tempf = (delta_pos.f[0] * sp50.f[0]) + (delta_pos.f[2] * sp50.f[2]);
            sp50.f[0] *= tempf;
            sp50.f[2] *= tempf;
            try_next_pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0] + (sp50.f[0]);
            try_next_pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2] + (sp50.f[2]);

            if (bondviewTryMoveToStan(&try_next_pos, &stan))
            {
                g_CurrentPlayer->field_488.current_tile_ptr = stan;
                g_CurrentPlayer->field_488.collision_position.f[0] = try_next_pos.f[0];
                g_CurrentPlayer->field_488.collision_position.f[2] = try_next_pos.f[2];

                return 1;
            }
        }
    }
    else
    {
        sp50.f[0] = collision_pt1->f[0] - prior_next_pos->f[0];
        sp50.f[2] = collision_pt1->f[2] - prior_next_pos->f[2];

        if (((sp50.f[0] * sp50.f[0]) + (sp50.f[2] * sp50.f[2])) <= (collision_radius * collision_radius))
        {
            if (collision_pt1->f[0] != g_CurrentPlayer->field_488.collision_position.f[0] || collision_pt1->f[2] != g_CurrentPlayer->field_488.collision_position.f[2])
            {
                sp50.f[0] = -(collision_pt1->f[2] - g_CurrentPlayer->field_488.collision_position.f[2]);
                sp50.f[2] = collision_pt1->f[0] - g_CurrentPlayer->field_488.collision_position.f[0];

                tempf = (sp50.f[0] * sp50.f[0]) + (sp50.f[2] * sp50.f[2]);
                tempf =  1.0f / sqrtf(tempf);
                sp50.f[0] *= tempf;
                sp50.f[2] *= tempf;

                tempf = (delta_pos.f[0] * sp50.f[0]) + (delta_pos.f[2] * sp50.f[2]);
                sp50.f[0] *= tempf;
                sp50.f[2] *= tempf;
                try_next_pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0] + (sp50.f[0]);
                try_next_pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2] + (sp50.f[2]);

                if (bondviewTryMoveToStan(&try_next_pos, &stan))
                {
                    g_CurrentPlayer->field_488.current_tile_ptr = stan;
                    g_CurrentPlayer->field_488.collision_position.f[0] = try_next_pos.f[0];
                    g_CurrentPlayer->field_488.collision_position.f[2] = try_next_pos.f[2];

                    return 1;
                }
            }
        }
    }

    return 0;
}


/**
 * Unreferenced.
 *
 * Bitwise convert 32bit int to float.
 *
 * Address 0x7F07D954.
 */
f32 bondviewBitconvertIntToFloat(s32 arg0)
{
    return *(f32*)&arg0;
}




struct dummy_struct {
    s32 unk00;
    s32 unk04;
};

/**
 * Sets Bond bondprevpos, attempts to move by `offset`.
 *
 * @param offset: Attempt to move Bond by {x,0,z} amount.
 * @param allow_scoot: If movement causes collision, when set will allow Bond to scoot along the collision edge and to bump around corner edges. Otherwise, any collision will stop movement.
 *
 * US address 7F07D960.
 * JP address 7F07DA34 (maybe).
 */
void bondviewCalcUpdatePlayerCollision(struct coord3d *offset, s32 allow_scoot)
{
    struct coord3d next_pos; // spb4
    struct coord3d collision1_pt0; // spa8
    struct coord3d collision1_pt1; // sp9c
    struct rect4f *polygon; // sp98
    s32 edges; // sp94
    struct TankRecord *tank_objrecord; // no stack
    struct ObjectRecord *obj;
    f32 *farr5;
    f32 *farr6;
    f32 temp_f2; // sp80
    struct coord3d collision2_pt0;  // sp74
    struct coord3d collision2_pt1; // sp68
    StandTile *stan; // no stack
    struct coord3d collision3_pt0; // sp58
    struct coord3d collision3_pt1; // sp4c
    s32 tile_count; // sp48
    s32 i; // sp44
    s32 temp_a3; // no stack
    s32 phi_a0_3; // sp3c
    s32 temp_v0_7; // no stack


    g_CurrentPlayer->bondprevpos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
    g_CurrentPlayer->bondprevpos.f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
    g_CurrentPlayer->bondprevpos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2];

    next_pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0] + offset->f[0];
    next_pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2] + offset->f[2];

    g_BondCanEnterTank = 0;

    g_CurrentPlayer->autocrouchpos = CROUCH_STAND;

    if (g_WorldTankProp != NULL)
    {
        chraiGetCollisionBoundsWithoutY(g_WorldTankProp, &polygon, &edges);

        if ((g_PlayerIsInTank == 1)
            || (chrpropTestPointInPolygon(&g_CurrentPlayer->field_488.collision_position, polygon, edges) != 0)
            || ((chrobjTestPointPolygonCollision(&g_CurrentPlayer->field_488.collision_position, g_CurrentPlayer->field_488.collision_radius, polygon, edges) != 0)))
        {

            obj = g_WorldTankProp->obj;
            tank_objrecord = (struct TankRecord *)g_WorldTankProp->obj;

            /// TODO: replace with ModelNode structs
            farr5 = (f32*)obj->model->obj->Switches[5]->Data;
            farr6 = (f32*)obj->model->obj->Switches[6]->Data;
            g_PlayerTankProp = g_WorldTankProp;

            temp_f2 = (farr5[4] - farr5[3]) * obj->model->scale;

            if (g_PlayerIsInTank == 1
                || (chrpropTestPointInPolygon(&g_CurrentPlayer->field_488.collision_position, &tank_objrecord->rect, (s32)tank_objrecord->collision) != 0))
            {
                temp_f2 += (farr6[4] - farr6[3]) * obj->model->scale;
                g_BondCanEnterTank = 1;
            }

            if ((g_PlayerIsInTank == 0) && (g_PlayerTankYOffset < temp_f2))
            {
                g_PlayerTankYOffset += (20.0f * g_GlobalTimerDelta);
                if ((temp_f2 < g_PlayerTankYOffset))
                {
                    //empty;
                }
                else
                {
                    return;
                }

                g_PlayerTankYOffset = temp_f2;
            }
            else
            {
                g_PlayerTankYOffset = temp_f2;
            }

            if (g_PlayerIsInTank == 1)
            {
                if (g_EnterTankAudioState == TANK_RUN_STATE_NOT_RUNNING)
                {
                    g_PlayerTankYOffset += -37.0f * (1.0f - g_TankEnteringSitHeightRemain);
                }
                else
                {
                    g_PlayerTankYOffset += -37.0f;
                }
            }
        }
        else
        {
            if (g_PlayerTankProp != NULL)
            {
                g_WorldTankProp = NULL;
                g_PlayerTankProp = NULL;
                g_PlayerTankYOffset = 0.0f;
            }
        }
    }

    // This `if` block looks like Perfect Dark bbike0f0d3c60
    if (bondviewTrySimpleMovePlayerCollision(&next_pos, &collision1_pt0, &collision1_pt1) == 0)
    {
        // return values are:
        //   1 if able to update stan and collision position
        //   zero if still unable to move by failing on the same collision edge
        //   -1 otherwise (still unable to move).
        temp_v0_7 = bondviewTryFractionMovePlayerCollision(&next_pos, &collision1_pt0, &collision1_pt1, &collision2_pt0, &collision2_pt1);

        if ((temp_v0_7 > 0) || (temp_v0_7 < 0))
        {
            if ((allow_scoot != 0)
                && (bondviewTryEdgeMovePlayerCollision(&next_pos, &collision1_pt0, &collision1_pt1) <= 0)
                && (bondviewTryEndHopPlayerCollision(&next_pos, &collision1_pt0, &collision1_pt1) == 0))
            {
                // empty
            }
        }
        else if (temp_v0_7 == 0)
        {
            bondviewTryFractionMovePlayerCollision(&next_pos, &collision2_pt0, &collision2_pt1, &collision3_pt0, &collision3_pt1);

            if ((allow_scoot != 0)
                && (bondviewTryEdgeMovePlayerCollision(&next_pos, &collision2_pt0, &collision2_pt1) <= 0)
                && (bondviewTryEdgeMovePlayerCollision(&next_pos, &collision1_pt0, &collision1_pt1) <= 0)
                && (bondviewTryEndHopPlayerCollision(&next_pos, &collision2_pt0, &collision2_pt1) == 0))
            {
                bondviewTryEndHopPlayerCollision(&next_pos, &collision1_pt0, &collision1_pt1);
            }
        }
    }

    /**
     * This block seems to be some error checking code, this will only occur when Bond
     * goes out of bounds.
    */
    if (stanTestPointWithinTileBoundsMaybe(
            g_CurrentPlayer->field_488.current_tile_ptr,
            g_CurrentPlayer->field_488.collision_position.f[0],
            g_CurrentPlayer->field_488.collision_position.f[2]) == 0)
    {
        if(1);

        stan = g_CurrentPlayer->field_488.current_tile_ptr;

        for (tile_count=0; tile_count<5; tile_count++)
        {
            /// TODO: fix the horrible casts below.

            for (i=0, phi_a0_3 = 0; i<((stan->tail.half >> 0xC) & 0xF); i++)
            {
                // maybe: if (( stan->points[i+1].link >> 4) != 0)
                if (( ((struct StandTilePoint*)stan) [i+1].link >> 4) != 0)
                {
                    if(1);
                    phi_a0_3++;
                }
            }

            temp_a3 = randomGetNext() % (u32)phi_a0_3;

            for (i=0, phi_a0_3 = 0; i<(((s16) stan->tail.half >> 0xC) & 0xF); i++)
            {
                // maybe: if (( stan->points[i+1].link >> 4) != 0)
                if (( ((struct StandTilePoint*)stan) [i+1].link >> 4) != 0)
                {
                    if (phi_a0_3 == temp_a3)
                    {
                        // note: no `>> 4`
                        // maybe: stan = &(standTileStart)[stan->points[i+1].link];
                        stan = (struct StandTile*)&((struct dummy_struct*)standTileStart)[( ((struct StandTilePoint*)stan)[i+1].link)];
                        break;
                    }

                    phi_a0_3++;
                }
            }

            if(1);

            if (stanTestPointWithinTileBoundsMaybe(
                stan,
                g_CurrentPlayer->field_488.collision_position.f[0],
                g_CurrentPlayer->field_488.collision_position.f[2]))
            {
                g_CurrentPlayer->field_488.current_tile_ptr = stan;
                break;
            }
        }
    }

    sub_GAME_7F07DE9C(g_CurrentPlayer);

    if (g_CurrentPlayer->field_488.current_tile_ptr != NULL)
    {
        objectivestatusCheckRoomEntered(g_CurrentPlayer->field_488.current_tile_ptr->room);
    }
}



void sub_GAME_7F07DE64(struct player *player) {
    chrpropDeregisterRoom(player->prop, player->field_2A04, player);
    player->field_2A04 = -1;
}


 /* extern */

/**
 * Address 0x7F07DE9C.
 */
void sub_GAME_7F07DE9C(struct player *player)
{
    sub_GAME_7F07DE64(player);

    if (player->prop->chr)
    {
        chrPositionRelated7F020D94(player->prop->chr);
        return;
    }

    if (player->field_488.current_tile_ptr)
    {
        player->field_2A04 = (s16) player->field_488.current_tile_ptr->room;

        chrpropRegisterRoom(player->prop, player->field_2A04);
    }
}


void sub_GAME_7F07DEFC(void)
{
    g_CurrentPlayer->pause_starting_angle = g_CurrentPlayer->vv_verta;
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_state = 0;
}

/**
 * Pause angle related.
 *
 * @param arg0: When set, pause_target_verta will be -40.0f, otherwise g_CurrentPlayer->vv_verta.
 *
 * Address 0x7F07DF28.
 */
f32 bondviewPauseAngleRelated(s32 arg0)
{
    f32 f;
    f32 ret;

    if (arg0)
    {
        g_CurrentPlayer->pause_saved_verta = g_CurrentPlayer->vv_verta;
        g_CurrentPlayer->pause_target_verta = -40.0f;
    }
    else
    {
        g_CurrentPlayer->pause_saved_verta = g_CurrentPlayer->pause_starting_angle;
        g_CurrentPlayer->pause_target_verta = g_CurrentPlayer->vv_verta;
    }

    f = g_CurrentPlayer->pause_saved_verta - g_CurrentPlayer->pause_target_verta;

    if (f < 0.0f)
    {
        f = -f;
    }

    if (f >= 60.0f)
    {
        ret = (((f - 60.0f) * 60.0f * 0.5f) / 60.0f) + 60.0f;
    }
    else if (f <= 0.0f)
    {
        ret = 0.0f;
    }
    else
    {
        // this is a different `60` than the other values above!
        ret = (f * 60.f) / 60.f;
    }

    return ret;
}


void bondviewStartPauseTransition(f32 duration) {
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_transition_duration = duration;
    g_CurrentPlayer->pause_state = 1;
}


void bondviewStartUnpauseTransition(f32 duration) {
    g_CurrentPlayer->pause_transition_time = 0.0f;
    g_CurrentPlayer->pause_transition_duration = duration;
    g_CurrentPlayer->pause_state = 2;
}


bool bondViewIsPauseTransitioning(void) {
    return (g_CurrentPlayer->pause_state != 0 && g_CurrentPlayer->pause_state != 3);
}


/**
 * Transition camera pitch from playing to watch menu (-40.0f degrees).
 * Then when the game is unpaused, transition the camera pitch back to its pitch before pausing began.
 */
void bondviewUpdatePauseTransition(void) {
    f32 prevverta;
    f32 frac;
    f32 weight;

    prevverta = g_CurrentPlayer->vv_verta;

    // Entering pause.
    if (g_CurrentPlayer->pause_state == 1) {
        g_CurrentPlayer->pause_transition_time += g_GlobalTimerDelta * watch_transition_time;

        if (g_CurrentPlayer->pause_transition_time < g_CurrentPlayer->pause_transition_duration) {
            // Cosine ease-in-out
            frac = g_CurrentPlayer->pause_transition_time / g_CurrentPlayer->pause_transition_duration;
            weight = (1.0f - cosf((frac * M_TAU_F) * 0.5f)) * 0.5f;

            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_saved_verta
                + ((g_CurrentPlayer->pause_target_verta - g_CurrentPlayer->pause_saved_verta) * weight);
        } else {
            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_target_verta;
            // Set pause state to paused.
            g_CurrentPlayer->pause_state = 3;
        }
    // Leaving pause.
    } else if (g_CurrentPlayer->pause_state == 2) {
        g_CurrentPlayer->pause_transition_time += g_GlobalTimerDelta * watch_transition_time;

        if (g_CurrentPlayer->pause_transition_time < g_CurrentPlayer->pause_transition_duration) {
            // Cosine ease-in-out
            frac = g_CurrentPlayer->pause_transition_time / g_CurrentPlayer->pause_transition_duration;
            weight = (1.0f - cosf((frac * M_TAU_F) * 0.5f)) * 0.5f;

            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_target_verta
                + ((g_CurrentPlayer->pause_saved_verta - g_CurrentPlayer->pause_target_verta) * weight);
        } else {
            g_CurrentPlayer->vv_verta = g_CurrentPlayer->pause_saved_verta;
            // Set pause state to unpaused.
            g_CurrentPlayer->pause_state = 0;
        }
    }

    // Wrap vv_verta into [-180, 180)
    if (g_CurrentPlayer->vv_verta < -180.0f) {
        g_CurrentPlayer->vv_verta += 360.0f;
    } else if (g_CurrentPlayer->vv_verta >= 180.0f) {
        g_CurrentPlayer->vv_verta -= 360.0f;
    }

    /**
     * Calculate shortest angular velocity from previous frame,
     * scale it by g_GlobalTimerDelta,
     * clamp it to [-0.7, 0.7] so the pitch change is never too fast.
     */
    if (g_ClockTimer > 0) {
        g_CurrentPlayer->speedverta = g_CurrentPlayer->vv_verta - prevverta;

        if (g_CurrentPlayer->speedverta < 0.0f) {
            g_CurrentPlayer->speedverta += 360.0f;
        }

        if (g_CurrentPlayer->speedverta > 180.0f) {
            g_CurrentPlayer->speedverta -= 360.0f;
        }

        g_CurrentPlayer->speedverta /= g_GlobalTimerDelta + g_GlobalTimerDelta;

        if (g_CurrentPlayer->speedverta < -0.7f) {
            g_CurrentPlayer->speedverta = -0.7f;
        } else if (g_CurrentPlayer->speedverta > 0.7f) {
            g_CurrentPlayer->speedverta = 0.7f;
        }
    }
}


f32 bondViewGetPauseTransitionFrac(void) {

    // Entering pause
    if (g_CurrentPlayer->pause_state == 1) {
        return g_CurrentPlayer->pause_transition_time / g_CurrentPlayer->pause_transition_duration;
    }
    // Leaving pause
    if (g_CurrentPlayer->pause_state == 2) {
        return 1.0f - (g_CurrentPlayer->pause_transition_time / g_CurrentPlayer->pause_transition_duration);
    }
    // Fully paused
    if (g_CurrentPlayer->pause_state == 3) {
        return 1.0f;
    }
    // Unpaused
    return 0.0f;
}

void trigger_watch_zoom(f32 final,f32 time)
{
  g_CurrentPlayer->zoomintime = 0.00000000;
  g_CurrentPlayer->zoomintimemax = time;
  g_CurrentPlayer->zoominfovyold = g_CurrentPlayer->zoominfovy;
  g_CurrentPlayer->zoominfovynew = final;
}


f32 bondviewGetWatchZoomFovy(void) {

    if (g_CurrentPlayer->zoomintime < g_CurrentPlayer->zoomintimemax)
    {
        return g_CurrentPlayer->zoominfovynew;
    }

    return g_CurrentPlayer->zoominfovy;
}

/**
 * Triggers watch zoom if new value.
 *
 * @param zoominfovy: watch zoom fovy.
 *
 * Address 0x7F07E46C.
 */
void bondviewTriggerWatchZoom(f32 zoominfovy)
{
    if (bondviewGetWatchZoomFovy() != zoominfovy)
    {
        if (zoominfovy < g_CurrentPlayer->zoominfovy)
        {
            trigger_watch_zoom(zoominfovy, ((g_CurrentPlayer->zoominfovy - zoominfovy) * 15.0f) / 30.0f);

            return;
        }

        trigger_watch_zoom(zoominfovy, ((zoominfovy - g_CurrentPlayer->zoominfovy) * 15.0f) / 30.0f);
    }
}


/**
 * Trigger watch zoom with default angle.
 *
 * Address 0x7F07E504.
 */
void bondviewTriggerWatchZoomDefault(void)
{
    bondviewTriggerWatchZoom(60.0f);
}


/**
 * Address 0x7F07E52C.
 */
void bondviewZoomToWatchOnOpen(void)
{
    f32 f;

#if defined(VERSION_EU)
    f = ((6.09999990463f - g_CurrentPlayer->zoominfovy) * 45.0f) / -53.9000015259f;
#else
    f = ((5.9f - g_CurrentPlayer->zoominfovy) * 45.0f) / -54.1f;
#endif

    if (f < 0.0f)
    {
        f = -f;
    }

#if defined(VERSION_EU)
    trigger_watch_zoom(6.09999990463f, f);
#else
    trigger_watch_zoom(5.9f, f);
#endif

}



/**
 * Address 0x7F07E594.
 */
void bondviewZoomFromWatchOnExit(void)
{
    f32 f;

#if defined(VERSION_EU)
    f = ((60.0f - g_CurrentPlayer->zoominfovy) * 45.0f) / -53.9000015259f;
#else
    f = ((60.0f - g_CurrentPlayer->zoominfovy) * 45.0f) / -54.1f;
#endif

    if (f < 0.0f)
    {
        f = -f;
    }

    trigger_watch_zoom(60.0f, f);
}



s32 check_watch_page_transistion_running(void)
{
    return (g_CurrentPlayer->zoomintime < g_CurrentPlayer->zoomintimemax);
}


/**
 * Address 0x7F07E62C.
 */
void bondviewUpdateWatchZoomIn(void)
{
    if (g_CurrentPlayer->zoomintime < g_CurrentPlayer->zoomintimemax)
    {
        if ((g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5) || (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc))
        {
#if defined(BUGFIX_R1)
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (f32) jpD_800484D0;
#else
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (f32) speedgraphframes;
#endif
        }
        else
        {
#if defined(BUGFIX_R1)
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (jpD_800484D0 * watch_transition_time);
#else
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintime + (speedgraphframes * watch_transition_time);
#endif
        }

        if (g_CurrentPlayer->zoomintimemax < g_CurrentPlayer->zoomintime)
        {
            g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintimemax;
        }

        g_CurrentPlayer->zoominfovy =
            g_CurrentPlayer->zoominfovyold +
            (
                (g_CurrentPlayer->zoomintime *
                    (g_CurrentPlayer->zoominfovynew - g_CurrentPlayer->zoominfovyold)
                )
                / g_CurrentPlayer->zoomintimemax
            );
    }
    else
    {
        g_CurrentPlayer->zoomintime = g_CurrentPlayer->zoomintimemax;
        g_CurrentPlayer->zoominfovy = g_CurrentPlayer->zoominfovynew;
    }

    set_cur_player_fovy(g_CurrentPlayer->zoominfovy);
    viSetFovY(g_CurrentPlayer->zoominfovy);
}




/**
 * Address 0x7F07E740.
 */
f32 bondviewWatchAnimationRelated(void)
{
    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4)
    {
        return ((45.0f - g_CurrentPlayer->zoomintimemax) + g_CurrentPlayer->zoomintime) / 45.0f;
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
    {
        return (g_CurrentPlayer->zoomintimemax - g_CurrentPlayer->zoomintime) / 45.0f;
    }

    if ((g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5) || (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc))
    {
        return 1.0f;
    }

    return 0.0f;
}





void sub_GAME_7F07E7CC(void) {
    ModelFileHeader *itemheader;

    itemheader = get_ptr_itemheader_in_hand(1);
    modelCalculateRwDataLen(itemheader);
#ifndef VERSION_EU
    if (0x32 < itemheader->numRecords) {
        return_null();
    }
#endif
    animInit((Model *)((u8 *)g_CurrentPlayer + 0x230), itemheader, (u32 *)((u8 *)g_CurrentPlayer + 0x2ec));
    modelSetScale((Model *)((u8 *)g_CurrentPlayer + 0x230), c_item_entries[41].scale * 0.10000001f);
    modelSetAnimation((Model *)((u8 *)g_CurrentPlayer + 0x230), (ModelAnimation *)&ptr_animation_table->data[(s32)&ANIM_DATA_bond_watch], 0, 0.0f, 0.5f * watch_transition_time, 0.0f);
    *(s32 *)((u8 *)g_CurrentPlayer + 0x220) = 0;
}





/**
 * Address 0x7F07E8B0.
 */
void bondviewSetPauseWatchRelated(f32 arg0)
{
    if (g_CurrentPlayer->step_in_view_watch_animation == 0)
    {
        g_CurrentPlayer->pause_watch_related_scaled = 20.0f / arg0;
    }
    else
    {
        g_CurrentPlayer->pause_watch_related_scaled = (20.0f - g_CurrentPlayer->pause_watch_related_adjust) / arg0;
    }

    g_CurrentPlayer->step_in_view_watch_animation = 1;
    g_CurrentPlayer->pause_watch_related = arg0;
}




/**
 * Address 0x7F07E910.
 */
void bondviewSetPauseWatchRelatedAlt(f32 arg0)
{
    if (g_CurrentPlayer->step_in_view_watch_animation == 3)
    {
        g_CurrentPlayer->pause_watch_related_scaled = 20.0f / arg0;
    }
    else
    {
        g_CurrentPlayer->pause_watch_related_scaled = g_CurrentPlayer->pause_watch_related_adjust / arg0;
    }

    g_CurrentPlayer->step_in_view_watch_animation = 2;
    g_CurrentPlayer->pause_watch_related = arg0;
}





/**
 * Address 0x7F07E964.
 */
void bondviewStepWatchAnimation(void)
{
    if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
    {
        if (g_CurrentPlayer->step_in_view_watch_animation == 1)
        {
            g_CurrentPlayer->pause_animation_counter += g_GlobalTimerDelta * watch_transition_time * g_CurrentPlayer->pause_watch_related_scaled;

            if (g_CurrentPlayer->pause_animation_counter > 20.0f)
            {
                g_CurrentPlayer->pause_animation_counter = 20.0f;
                g_CurrentPlayer->step_in_view_watch_animation = 3;
            }
        }
        else if (g_CurrentPlayer->step_in_view_watch_animation == 2)
        {
            g_CurrentPlayer->pause_animation_counter -= g_GlobalTimerDelta * watch_transition_time * g_CurrentPlayer->pause_watch_related_scaled;

            if (g_CurrentPlayer->pause_animation_counter < 0.0f)
            {
                g_CurrentPlayer->pause_animation_counter = 0.0f;
                g_CurrentPlayer->step_in_view_watch_animation = 0;
            }
        }

        modelSetAnimFrame2((void*)&g_CurrentPlayer->something_with_watch_object_instance, g_CurrentPlayer->pause_animation_counter, 0.0f);
    }
}






/**
 * Address 0x7F07EA78.
 */
f32 bondviewGetPauseAnimationPercent(void)
{
    if ((g_CurrentPlayer->step_in_view_watch_animation == 1) || (g_CurrentPlayer->step_in_view_watch_animation == 2))
    {
        return g_CurrentPlayer->pause_animation_counter / 20.0f;
    }

    if (g_CurrentPlayer->step_in_view_watch_animation == 3)
    {
        return 1.0f;
    }

    return 0.0f;
}




void set_BONDdata_outside_watch_menu_flag(s32 arg0) {
    g_CurrentPlayer->outside_watch_menu = arg0;
}

s32 get_BONDdata_outside_watch_menu_flag(void) {
    return g_CurrentPlayer->outside_watch_menu;
}





void bondviewPlayerStopAudioForPause(void)
{
    struct hand *hand;
	s32 i;
    ObjectRecord *obj;
    PropRecord *prop;

    deactivate_alarm_sound_effect();
    check_deactivate_gas_sound();

    for (i = 0; i < 2; i++)
    {
        hand = &g_CurrentPlayer->hands[i];

        if (hand->audioHandle && sndGetPlayingState(hand->audioHandle) != AL_STOPPED) {
			sndDeactivate(hand->audioHandle);
		}
    }

    for (i = 0; i < 2; i++)
    {
        if (g_TankSfxState[i] && sndGetPlayingState(g_TankSfxState[i]) != AL_STOPPED) {
			sndDeactivate(g_TankSfxState[i]);
		}
    }

    for (prop = get_ptr_obj_pos_list_current_entry(); prop; prop = prop->prev)
    {
        if (prop->type != PROP_TYPE_DOOR && prop->type == PROP_TYPE_OBJ)
        {
            obj = prop->obj;

            if (obj->type == PROPDEF_VEHICHLE)
            {
                VehichleRecord *vehicle = (VehichleRecord *)prop->obj;
                if (vehicle->Sound && sndGetPlayingState(vehicle->Sound) != AL_STOPPED) {
                    sndDeactivate(vehicle->Sound);
                }
            }
            else if (obj->type == PROPDEF_AIRCRAFT)
            {
                AircraftRecord *aircraft = (AircraftRecord *)prop->obj;
                if (aircraft->Sound && sndGetPlayingState(aircraft->Sound) != AL_STOPPED) {
                    sndDeactivate(aircraft->Sound);
                }
            }

            if(1);
        }
    }
}





/**
 * US address 7F07EC54.
 * JP address 7F07F260.
 * EU address 7F07ECF4.
*/
void bondviewWatchAnimationTick(void)
{
#if defined(VERSION_EU)
    #define WATCH_VAR_LOWER 14
    #define WATCH_VAR_UPPER 29
#else
    #define WATCH_VAR_LOWER 17
    #define WATCH_VAR_UPPER 35
#endif

    s32 sp3c;
    s32 sp38;
    s32 sp34;
    f32 sp30;
    f32 sp2c;
    f32 sp28;
    f32 sp24;
    f32 sp20;

    if (g_CurrentPlayer->watch_animation_state)
    {
        sp34 = getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_TRIGGER || getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_WATCHLASER;
        sp3c = sp34;

#if defined (VERSION_US)
        sp38 = WATCH_VAR_LOWER;

        if (sp34)
        {
            sp38 = WATCH_VAR_UPPER;
        }
#else
        sp38 = (sp34) ? WATCH_VAR_UPPER : WATCH_VAR_LOWER;
#endif

        g_CurrentPlayer->timer_1C4 += g_ClockTimer;
        g_CurrentPlayer->watch_pause_time += 1;
        g_CurrentPlayer->pausing_flag = FALSE;

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x1)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                draw_item_in_hand(GUNLEFT, ITEM_SUIT_LF_HAND);
            }
            else if (g_CurrentPlayer->watch_pause_time == 2)
            {
                if (sp34)
                {
                    draw_item_in_hand(GUNRIGHT, ITEM_UNARMED);
                }
            }
            else
            {
                if (
                    (get_item_in_hand_or_watch_menu(1) == ITEM_SUIT_LF_HAND)
                    && (Gun_hand_without_item(GUNLEFT) != ITEM_UNARMED)
                    && (
                        (sp34 == 0)
                        || (
                            (get_item_in_hand_or_watch_menu(0) == ITEM_UNARMED)
                            && (Gun_hand_without_item(GUNRIGHT) != ITEM_UNARMED)))
                    && (g_CurrentPlayer->timer_1C4 >= sp38))
                {
                    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x2;
                    g_CurrentPlayer->watch_pause_time = 1;
                    g_CurrentPlayer->timer_1C4 = 0;
                }
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x2)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                gunSetSightVisible(GUNAMMOREASON_DAMAGE, FALSE);
                gunSetGunAmmoVisible(GUNSIGHTREASON_NOCONTROL, FALSE);
                hudmsgsSetOff(PLAYERFLAG_LOCKCONTROLS);
                bondviewSetUpperTextDisplayFlag(PLAYERFLAG_LOCKCONTROLS);
                countdownTimerSetVisible(4, 0);

                if ((g_CurrentPlayer->pause_state == 0) || (g_CurrentPlayer->pause_state == 2) || (g_CurrentPlayer->pause_state == 3))
                {
                    sp20 = bondviewPauseAngleRelated(1);

                    if (sp20 < 30.0f)
                    {
                        sp20 = 30.0f;
                    }

                    bondviewStartPauseTransition(sp20);
                }
            }

            if ((g_CurrentPlayer->pause_transition_duration - g_CurrentPlayer->pause_transition_time) < 30.0f)
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x3;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;

                sub_GAME_7F07E7CC();
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x3)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                sp30 = 40.0f;

                if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
                {
                    sp30 = ((20.0f - g_CurrentPlayer->pause_watch_related_adjust) * 40.0f) / 20.0f;
                }

                if ((g_CurrentPlayer->pause_state == 0) || (g_CurrentPlayer->pause_state == 2) || (g_CurrentPlayer->pause_state == 3))
                {
                    sp2c = bondviewPauseAngleRelated(1);
                    sp20 = sp30 - 10.0f;

                    if (sp2c < sp20)
                    {
                        sp2c = sp20;
                    }

                    bondviewStartPauseTransition(sp2c);

                    sp20 = sp2c + 10.0f;

                    if (sp30 < sp20)
                    {
                        sp30 = sp20;
                    }
                }

                bondviewSetPauseWatchRelated(sp30);
            }

            if (
                ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
                ||
                bondViewIsPauseTransitioning()
                )
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x4;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4)
        {
            if ((g_CurrentPlayer->watch_pause_time == 1) && (g_CurrentPlayer->field_21C != 0))
            {
                sndPlaySfx(g_musicSfxBufferPtr, WATCH_ON_SFX, NULL);
                g_CurrentPlayer->field_21C = 0;
            }

            bondviewZoomToWatchOnOpen();

            if (check_watch_page_transistion_running() != 0)
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x5;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
                g_CurrentPlayer->field_21C = 1;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                sub_GAME_7F0C1310();
            }

            g_CurrentPlayer->pausing_flag = TRUE;
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc)
        {
            g_CurrentPlayer->pausing_flag = TRUE;

            if (g_CurrentPlayer->watch_pause_time >= 3)
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x6;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
                sndPlaySfx(g_musicSfxBufferPtr, WATCH_OFF_SFX, NULL);
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                bondviewZoomFromWatchOnExit();
                if (sp34)
                {
                    draw_item_in_hand(GUNRIGHT, ITEM_UNARMED);
                }
                else
                {
                    sub_GAME_7F05DAE4(0);
                }
            }
            if (check_watch_page_transistion_running())
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x7;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
                g_CurrentPlayer->field_21C = 1;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x7)
        {
            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                sp28 = 40.0f;
                sp24 = bondviewPauseAngleRelated(0);

                if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
                {
                    sp28 = (g_CurrentPlayer->pause_watch_related_adjust * 40.0f) / 20.0f;
                }

                sp20 = sp28 + 20.0f;

                if (sp24 < sp20)
                {
                    sp24 = sp20;
                }

                bondviewStartUnpauseTransition(sp24);
                bondviewSetPauseWatchRelatedAlt(sp28);
            }

            if ((g_CurrentPlayer->step_in_view_watch_animation != 0) && (g_CurrentPlayer->step_in_view_watch_animation != 3))
            {
                g_CurrentPlayer->pausing_flag = TRUE;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
                g_CurrentPlayer->watch_pause_time = 1;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x8)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {

                if ((get_item_in_hand_or_watch_menu(GUNLEFT) != getCurrentPlayerWeaponId(GUNLEFT)) && (Gun_hand_without_item(GUNLEFT) != ITEM_UNARMED))
                {
                    draw_item_in_hand(GUNLEFT, getCurrentPlayerWeaponId(GUNLEFT));
                }

                if (sp3c)
                {
                    if ((get_item_in_hand_or_watch_menu(GUNRIGHT) != getCurrentPlayerWeaponId(GUNRIGHT)) && (Gun_hand_without_item(GUNRIGHT) != ITEM_UNARMED))
                    {
                        draw_item_in_hand(GUNRIGHT, getCurrentPlayerWeaponId(GUNRIGHT));
                    }
                }
            }
            else if (!bondViewIsPauseTransitioning())
            {
                if (
                    (get_item_in_hand_or_watch_menu(GUNLEFT) == getCurrentPlayerWeaponId(GUNLEFT))
                    && (Gun_hand_without_item(GUNLEFT) != ITEM_UNARMED)
                    && (
                        (sp3c == 0)
                        || (((get_item_in_hand_or_watch_menu(GUNRIGHT) == getCurrentPlayerWeaponId(GUNRIGHT))) && (Gun_hand_without_item(GUNRIGHT) != ITEM_UNARMED))))
                {
                    sub_GAME_7F05DAE4(1);

                    if (sp3c)
                    {
                        sub_GAME_7F05DAE4(0);
                    }

                    gunSetSightVisible(GUNAMMOREASON_DAMAGE, TRUE);
                    gunSetGunAmmoVisible(GUNSIGHTREASON_NOCONTROL, TRUE);
                    hudmsgsSetOn(1);
                    bondviewClearUpperTextDisplayFlag(1);
                    countdownTimerSetVisible(4, 1);

                    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x0;
                    g_CurrentPlayer->watch_pause_time = 0;
                    g_CurrentPlayer->timer_1C4 = 0;
                }
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x9)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (
                (getCurrentPlayerWeaponId(GUNLEFT) != get_item_in_hand_or_watch_menu(GUNLEFT))
                && (Gun_hand_without_item(GUNLEFT) != ITEM_UNARMED)
                && (
                    (sp3c == 0)
                    || (((getCurrentPlayerWeaponId(GUNRIGHT) != get_item_in_hand_or_watch_menu(GUNRIGHT))) && (Gun_hand_without_item(GUNRIGHT) != ITEM_UNARMED))))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
                g_CurrentPlayer->watch_pause_time = 0;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xa)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (g_CurrentPlayer->watch_pause_time == 1)
            {
                bondviewStartUnpauseTransition(bondviewPauseAngleRelated(0));
            }

            if (
                (getCurrentPlayerWeaponId(GUNLEFT) != get_item_in_hand_or_watch_menu(GUNLEFT))
                && (Gun_hand_without_item(GUNLEFT) != ITEM_UNARMED)
                && (
                    (sp3c == 0)
                    || (((getCurrentPlayerWeaponId(GUNRIGHT) != get_item_in_hand_or_watch_menu(GUNRIGHT))) && (Gun_hand_without_item(GUNRIGHT) != ITEM_UNARMED))))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
                g_CurrentPlayer->watch_pause_time = 0;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xb)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (
                (getCurrentPlayerWeaponId(GUNLEFT) == get_item_in_hand_or_watch_menu(GUNLEFT))
                && (Gun_hand_without_item(GUNLEFT) != ITEM_UNARMED)
                && (
                    (sp3c == 0)
                    || (
                        ( (getCurrentPlayerWeaponId(GUNRIGHT) == get_item_in_hand_or_watch_menu(GUNRIGHT)))
                        && (Gun_hand_without_item(GUNRIGHT) != ITEM_UNARMED))))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
                g_CurrentPlayer->watch_pause_time = 0;
                g_CurrentPlayer->timer_1C4 = 0;
            }
        }

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xd)
        {
            g_CurrentPlayer->pausing_flag = FALSE;

            if (Gun_hand_without_item(GUNLEFT)
                && Gun_hand_without_item(GUNRIGHT))
            {
                if ((g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 5)
                    && (g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 6)
                    && (g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 7)
                    && (g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 8))
                {
                    g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
                    g_CurrentPlayer->watch_pause_time = 0;
                    g_CurrentPlayer->timer_1C4 = 0;
                }
            }
        }

        bondviewUpdatePauseTransition();
        bondviewStepWatchAnimation();
        bondviewUpdateWatchZoomIn();
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5)
    {
        lvlSetControlsLockedFlag(TRUE);
        sub_GAME_7F0A6A80();
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc)
    {
        lvlSetControlsLockedFlag(TRUE);
    }
    else
    {
        lvlSetControlsLockedFlag(FALSE);
    }

#undef WATCH_VAR_LOWER
#undef WATCH_VAR_UPPER
}



void set_open_close_solo_watch_menu_to1(void) {
    g_CurrentPlayer->open_close_solo_watch_menu = TRUE;
}





/**
 * US address 7F07F874.
 * EU address 7F07F918.
*/
void trigger_solo_watch_menu(s32 arg0)
{
    struct WatchVertex *ptr_a;
    Gfx *ptr_b;
    struct WatchVertex *next;
    struct WatchVertex *ptr_copy;
    s32 i;

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        if (arg0 == 0)
        {
            watch_transition_time *= 1.1f;
            if (watch_transition_time > 1.7f)
            {
                watch_transition_time = 1.7f;
            }

            if ((Gun_hand_without_item(GUNLEFT) != ITEM_UNARMED)
                && (Gun_hand_without_item(GUNRIGHT) != ITEM_UNARMED)
                && (g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 5)
                && (g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 6)
                && (g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 7)
                && (g_CurrentPlayer->hands[1].when_detonating_mines_is_0 != 8))
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
            }
            else
            {
                g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xd;
            }

            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;

            sub_GAME_7F07DEFC();
            bondviewTriggerWatchZoomDefault();

            hudMakeDamageSegments(&g_CurrentPlayer->armor_display_values, 23*2, 1, currentPlayerGetArmor());
            buildGaugeBarDL(&g_CurrentPlayer->watch_body_armor_bar_gdl, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->armor_display_values), 0x2E);

            hudMakeDamageSegments(&g_CurrentPlayer->health_display_values, 23*2, -1, currentPlayerGetHealth());
            buildGaugeBarDL(&g_CurrentPlayer->watch_health_bar_gdl, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->health_display_values), 0x2E);

            sub_GAME_7F0A69A8();

            /**
             * This section is for rendering the selected screen rectangles.
            */
            ptr_b = g_CurrentPlayer->buffer_for_watch_greenbackdrop_DL; // Gfx
            ptr_a = &g_CurrentPlayer->buffer_for_watch_greenbackdrop_vertices->vtx[0]; // struct WatchRectangle

            for (i=0;
                i<(WATCH_NUMBER_SCREENS * WATCH_SCREEN_SELECT_RECTANGLE_HSTEP);
                i+=WATCH_SCREEN_SELECT_RECTANGLE_HSTEP)
            {
                // Note: colors are set here but overwritten in watch.c set_page_rectangle_colors
                ptr_copy = ptr_a;
                ptr_a = setup_watch_rectangles(ptr_a, i, 0, 0x64, 0x14, -0x12B, 0x136);
                ptr_b = sub_GAME_7F0A3B40(ptr_b, OS_K0_TO_PHYSICAL(ptr_copy));
            }

            gSPEndDisplayList(ptr_b);
            /**
             * End watch screen select rectangles.
            */

            /**
             * This section is related to rendering static on the watch menu.
             * Static is defined by a horizontal bar in the middle of the screen.
            */
            ptr_a = &g_CurrentPlayer->buffer_for_watch_static_vertices->vtx[0]; // struct WatchRectangle
            ptr_b = g_CurrentPlayer->buffer_for_watch_static_DL; // Gfx

            ptr_copy = &g_CurrentPlayer->buffer_for_watch_static_vertices->vtx[0];
            next = setup_watch_rectangles(ptr_a, 0, 0, 0x398, 0x14, -0x1CC, 0);
            ptr_b = sub_GAME_7F0A3B40(ptr_b, OS_K0_TO_PHYSICAL(ptr_copy));

            gSPEndDisplayList(ptr_b);
            /**
             * End watch static section.
            */
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x1)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x9;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x2)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xa;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x3)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x7;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x6;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5)
    {
        deleteCurrentSelectedFolder();
        sub_GAME_7F0C1340();
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xc;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
        g_CurrentPlayer->open_close_solo_watch_menu = FALSE;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xc)
    {
        // removed
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
    {
        if (arg0 == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x4;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
            sub_GAME_7F0A69A8();
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x7)
    {
        if (arg0 == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x3;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
            sub_GAME_7F0A69A8();
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x8)
    {
        if (arg0 == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0xb;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
            sub_GAME_7F0A69A8();
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x9)
    {
        if (arg0 == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xa)
    {
        if (arg0 == 0)
        {
            g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1;
            g_CurrentPlayer->watch_pause_time = 0;
            g_CurrentPlayer->timer_1C4 = 0;
        }
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xb)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x8;
        g_CurrentPlayer->watch_pause_time = 0;
        g_CurrentPlayer->timer_1C4 = 0;
    }
    else if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0xd)
    {
        g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x0;
    }
}

/**
 * US address 7F07FCC4.
 * Perfect Dark bwalkUpdateSpeedSideways.
*/
void bondviewUpdateSpeedSideways(s32 arg0) {
    if (arg0 == -1) {
        g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe - g_GlobalTimerDelta);
        if (g_CurrentPlayer->speedstrafe < -1.0f) {
            g_CurrentPlayer->speedstrafe = -1.0f;
        }
    }
    else {
        if (arg0 == 1) {
            g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe + g_GlobalTimerDelta);
            if (1.0f < g_CurrentPlayer->speedstrafe) {
                g_CurrentPlayer->speedstrafe = 1.0f;
            }
        } else {
            if (0.0f < g_CurrentPlayer->speedstrafe) {
                g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe - g_GlobalTimerDelta);
                if (g_CurrentPlayer->speedstrafe < 0.0f) {
                    g_CurrentPlayer->speedstrafe = 0.0f;
                }
            } else {
                g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe + g_GlobalTimerDelta);
                if (0.0f < g_CurrentPlayer->speedstrafe) {
                    g_CurrentPlayer->speedstrafe = 0.0f;
                }
            }
        }
    }
    g_CurrentPlayer->speedsideways = g_CurrentPlayer->speedstrafe;
}

/**
 * US address 7F07FE1C.
 * Perfect Dark bwalkUpdateSpeedForwards.
*/
void bondviewUpdateSpeedForwards(s32 arg0) {
    if (arg0 == 1) {
        g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo + g_GlobalTimerDelta);
        if (1.0f < g_CurrentPlayer->speedgo) {
            g_CurrentPlayer->speedgo = 1.0f;
        }
    } else {
        if (arg0 == -1) {
            g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo - g_GlobalTimerDelta);
            if (g_CurrentPlayer->speedgo < -1.0f) {
                g_CurrentPlayer->speedgo = -1.0f;
            }
        } else {
            if (0.0f < g_CurrentPlayer->speedgo) {
                g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo - g_GlobalTimerDelta);
                if (g_CurrentPlayer->speedgo < 0.0f) {
                    g_CurrentPlayer->speedgo = 0.0f;
                }
            } else {
                g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo + g_GlobalTimerDelta);
                if (0.0f < g_CurrentPlayer->speedgo) {
                    g_CurrentPlayer->speedgo = 0.0f;
                }
            }
        }
    }
    g_CurrentPlayer->speedforwards = g_CurrentPlayer->speedgo;
}

/**
 * US address 7F07FF74.
 * Duplicate of sub_GAME_7F080228.
*/
f32 sub_GAME_7F07FF74(f32 value) {
    if (value > 0) {
        return (viGetFovY() * value * -0.7f) / FOV_Y_F;
    }

    if (value < 0) {
        return (viGetFovY() * -value * 0.7f) / FOV_Y_F;
    }

    return 0;
}


/**
 * Address 0x7F080010.
 */
void bondviewCurrentPlayerUpdateSpeedVerta(f32 value)
{
    f32 mult = viGetFovY() / FOV_Y_F;
    f32 limit = sub_GAME_7F07FF74(value);

    if (value > 0.0f)
    {
        if (g_CurrentPlayer->speedverta > 0.0f)
        {
            g_CurrentPlayer->speedverta -= (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedverta -= (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedverta < limit)
        {
            g_CurrentPlayer->speedverta = limit;
        }
    }
    else if (value < 0.0f)
    {
        if (g_CurrentPlayer->speedverta < 0.0f)
        {
            g_CurrentPlayer->speedverta += (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedverta += (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedverta > limit)
        {
            g_CurrentPlayer->speedverta = limit;
        }
    }
    else
    {
        if (g_CurrentPlayer->speedverta > limit)
        {
            g_CurrentPlayer->speedverta -= (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedverta < limit)
            {
                g_CurrentPlayer->speedverta = limit;
            }
        }
        else
        {
            g_CurrentPlayer->speedverta += (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedverta > limit)
            {
                g_CurrentPlayer->speedverta = limit;
            }
        }
    }
}



/**
 * US address 7F080228.
 * Duplicate of sub_GAME_7F07FF74.
*/
f32 sub_GAME_7F080228(f32 arg0) {
    if (0.0f < arg0) {
        return (viGetFovY() * arg0 * -0.7f) / FOV_Y_F;
    } else if (arg0 < 0.0f) {
        return (viGetFovY() * -arg0 * 0.7f) / FOV_Y_F;
    } else {
        return 0.0f;
    }
}

/**
 * Address 0x7F0802C4.
 */
void bondviewCurrentPlayerUpdateSpeedTheta(f32 value)
{
    f32 mult = viGetFovY() / FOV_Y_F;
    f32 limit = sub_GAME_7F080228(value);

    if (value > 0.0f)
    {
        if (g_CurrentPlayer->speedtheta > 0.0f)
        {
            g_CurrentPlayer->speedtheta -= (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedtheta -= (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedtheta < limit)
        {
            g_CurrentPlayer->speedtheta = limit;
        }
    }
    else if (value < 0.0f)
    {
        if (g_CurrentPlayer->speedtheta < 0)
        {
            g_CurrentPlayer->speedtheta += (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedtheta += (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (limit < g_CurrentPlayer->speedtheta)
        {
            g_CurrentPlayer->speedtheta = limit;
        }
    }
    else
    {
        if (limit < g_CurrentPlayer->speedtheta)
        {
            g_CurrentPlayer->speedtheta -= (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedtheta < limit)
            {
                g_CurrentPlayer->speedtheta = limit;
            }
        }
        else
        {
            g_CurrentPlayer->speedtheta += (0.05f * g_GlobalTimerDelta * mult);

            if (limit < g_CurrentPlayer->speedtheta)
            {
                g_CurrentPlayer->speedtheta = limit;
            }
        }
    }
}


Gfx *currentPlayerDrawFade(Gfx *gdl)
{
    f32 frac = g_CurrentPlayer->colourscreenfrac;
    s32 r = g_CurrentPlayer->colourscreenred;
    s32 g = g_CurrentPlayer->colourscreengreen;
    s32 b = g_CurrentPlayer->colourscreenblue;
    if ((cameraFrameCounter1 != 0) || (cameraFrameCounter2 != 0)) {
        frac = 1.0f;
        b = 0;
        g = 0;
        r = 0;
    }
    if (frac > 0) {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetColorDither(gdl++, G_CD_DISABLE);
        gDPSetTexturePersp(gdl++, G_TP_NONE);
        gDPSetAlphaCompare(gdl++, G_AC_NONE);
        gDPSetTextureLOD(gdl++, G_TL_TILE);
        gDPSetTextureFilter(gdl++, G_TF_BILERP);
        gDPSetTextureConvert(gdl++, G_TC_FILT);
        gDPSetTextureLUT(gdl++, G_TT_NONE);
        gDPSetRenderMode(gdl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gdl++, 0, 0, r, g, b, (s32)(frac * 255.0f));
        gDPFillRectangle(gdl++, viGetViewLeft(), viGetViewTop(), (viGetViewLeft() + viGetViewWidth()), (viGetViewTop() + viGetViewHeight()));
        gDPPipeSync(gdl++);
        gDPSetColorDither(gdl++, G_CD_BAYER);
        gDPSetTexturePersp(gdl++, G_TP_PERSP);
        gDPSetTextureLOD(gdl++, G_TL_LOD);
    }

    return gdl;
}

void currentPlayerSetFadeColour(s32 r, s32 g, s32 b, f32 frac) {
    g_CurrentPlayer->colourscreenred = r;
    g_CurrentPlayer->colourscreengreen = g;
    g_CurrentPlayer->colourscreenblue = b;
    g_CurrentPlayer->colourscreenfrac = frac;
}

void currentPlayerAdjustFade(f32 maxfadetime, s32 r, s32 g, s32 b, f32 frac)
{
    g_CurrentPlayer->colourfadetime60    = 0;
    g_CurrentPlayer->colourfadetimemax60 = maxfadetime;
    g_CurrentPlayer->colourfaderedold    = g_CurrentPlayer->colourscreenred;
    g_CurrentPlayer->colourfaderednew    = r;
    g_CurrentPlayer->colourfadegreenold  = g_CurrentPlayer->colourscreengreen;
    g_CurrentPlayer->colourfadegreennew  = g;
    g_CurrentPlayer->colourfadeblueold   = g_CurrentPlayer->colourscreenblue;
    g_CurrentPlayer->colourfadebluenew   = b;
    g_CurrentPlayer->colourfadefracold   = g_CurrentPlayer->colourscreenfrac;
    g_CurrentPlayer->colourfadefracnew   = frac;
}

void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac)
{
    currentPlayerAdjustFade(maxfadetime, g_CurrentPlayer->colourscreenred, g_CurrentPlayer->colourscreengreen, g_CurrentPlayer->colourscreenblue, frac);
}

bool currentPlayerIsFadeComplete(void)
{
	return g_CurrentPlayer->colourfadetimemax60 < 0;
}

void currentPlayerUpdateColourScreenProperties(void)
{
    if (g_CurrentPlayer->colourfadetimemax60 >= 0)
    {
        g_CurrentPlayer->colourfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->colourfadetime60 < g_CurrentPlayer->colourfadetimemax60)
        {
            f32 mult                           = g_CurrentPlayer->colourfadetime60 / g_CurrentPlayer->colourfadetimemax60;
            g_CurrentPlayer->colourscreenfrac  = g_CurrentPlayer->colourfadefracold + (g_CurrentPlayer->colourfadefracnew - g_CurrentPlayer->colourfadefracold) * mult;
            g_CurrentPlayer->colourscreenred   = g_CurrentPlayer->colourfaderedold + (s32)((g_CurrentPlayer->colourfaderednew - g_CurrentPlayer->colourfaderedold) * mult);
            g_CurrentPlayer->colourscreengreen = g_CurrentPlayer->colourfadegreenold + (s32)((g_CurrentPlayer->colourfadegreennew - g_CurrentPlayer->colourfadegreenold) * mult);
            g_CurrentPlayer->colourscreenblue  = g_CurrentPlayer->colourfadeblueold + (s32)((g_CurrentPlayer->colourfadebluenew - g_CurrentPlayer->colourfadeblueold) * mult);
            return;
        }

        g_CurrentPlayer->colourscreenfrac    = g_CurrentPlayer->colourfadefracnew;
        g_CurrentPlayer->colourscreenred     = g_CurrentPlayer->colourfaderednew;
        g_CurrentPlayer->colourscreengreen   = g_CurrentPlayer->colourfadegreennew;
        g_CurrentPlayer->colourscreenblue    = g_CurrentPlayer->colourfadebluenew;
        g_CurrentPlayer->colourfadetimemax60 = -1;
    }
}

void currentPlayerStartChrFade(f32 duration60, f32 targetfrac)
{
    ChrRecord *chr = g_CurrentPlayer->prop->chr;

    if (chr)
    {
        g_CurrentPlayer->bondfadetime60    = 0;
        g_CurrentPlayer->bondfadetimemax60 = duration60;
        g_CurrentPlayer->bondfadefracold   = chr->fadealpha / 255.0f;
        g_CurrentPlayer->bondfadefracnew   = targetfrac;
    }
}

void currentPlayerTickChrFade(void)
{
    if (g_CurrentPlayer->bondfadetimemax60 >= 0)
    {
        ChrRecord *chr = g_CurrentPlayer->prop->chr;
        f32        frac;

        g_CurrentPlayer->bondfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->bondfadetime60 < g_CurrentPlayer->bondfadetimemax60)
        {
            frac = g_CurrentPlayer->bondfadefracold + (g_CurrentPlayer->bondfadefracnew - g_CurrentPlayer->bondfadefracold) * g_CurrentPlayer->bondfadetime60 / g_CurrentPlayer->bondfadetimemax60;
        }
        else
        {
            frac = g_CurrentPlayer->bondfadefracnew;
            g_CurrentPlayer->bondfadetimemax60 = -1;
        }

        if (chr)
        {
            chr->fadealpha = (s8)(frac * 255);
        }
    }
}

/**
 * Will apply a move animation update. The pass through call to bheadUpdate is
 * what allows Bond to move. This will also trigger the death animation once
 * Bond dies. This chooses a random death animation from g_bondviewBondDeathAnimations.
 * Address 0x7F080B34.
*/
void bondviewMoveAnimationTick(f32 speed, f32 speedforwards, f32 speedsideways)
{
    f32 percent_speed;
    Mtxf sp8C;
    Mtxf sp4C;

    percent_speed = 0.0f;

    if (g_CurrentPlayer->bonddead == 0)
    {
        bheadAdjustAnimation(speed);

        if (speed != 0.0f)
        {
            percent_speed = speedforwards / speed;
        }
        else if (speedforwards == 0.0f)
        {
            //
        }
    }
    else
    {
        if (g_CurrentPlayer->startnewbonddie)
        {
            // HACK: ptr_animation_table dereference addition is backwards.
            // this should be:
            // ptr_animation_table->data[g_bondviewBondDeathAnimations[((u32) randomGetNext() % (u32) g_bondviewBondDeathAnimationsCount)]]
            bheadStartDeathAnimation((struct ModelAnimation *) ((s32)g_bondviewBondDeathAnimations[((u32) randomGetNext() % (u32) g_bondviewBondDeathAnimationsCount)] + (s32)&ptr_animation_table->data[0]), randomGetNext() & 1, 0.0f, 1.0f);
            g_CurrentPlayer->startnewbonddie = FALSE;
        }

        bheadSetSpeed(0.5f);
        speedsideways = 0.0f;
    }

    bheadUpdate(percent_speed, speedsideways);

    matrix_4x4_set_rotation_around_x((360.0f - g_CurrentPlayer->vv_verta360) * DegToRad1Fact(1), &sp8C);
    matrix_4x4_set_basis_and_position_target(&sp4C, 0.0f, 0.0f, 0.0f, -g_CurrentPlayer->headlook.f[0], -g_CurrentPlayer->headlook.f[1], -g_CurrentPlayer->headlook.f[2], g_CurrentPlayer->headup.f[0], g_CurrentPlayer->headup.f[1], g_CurrentPlayer->headup.f[2]);
    matrix_4x4_multiply_in_place(&sp4C, &sp8C);
    matrix_4x4_set_rotation_around_y((360.0f - g_CurrentPlayer->vv_theta) * DegToRad1Fact(1), &sp4C);
    matrix_4x4_multiply_in_place(&sp4C, &sp8C);

    g_CurrentPlayer->field_488.applied_view.f[0] = sp8C.m[2][0];
    g_CurrentPlayer->field_488.applied_view.f[1] = sp8C.m[2][1];
    g_CurrentPlayer->field_488.applied_view.f[2] = sp8C.m[2][2];

    g_CurrentPlayer->field_488.applied_view2.f[0] = sp8C.m[1][0];
    g_CurrentPlayer->field_488.applied_view2.f[1] = sp8C.m[1][1];
    g_CurrentPlayer->field_488.applied_view2.f[2] = sp8C.m[1][2];

}


/**
 * Address 0x7F080D60.
 */
f32 bondviewYPositionRelated(StandTile *arg0, f32 arg1, f32 arg2)
{
    f32 ret;

    if (g_PlayerTankProp != NULL)
    {
        ObjectRecord * obj = ((PropRecord *)g_PlayerTankProp)->obj;
        PropRecord *p = obj->prop;

        ret = stanGetPositionYValue(p->stan, p->pos.x, p->pos.z);

        ret += g_PlayerTankYOffset;
    }
    else
    {
        if (g_CurrentPlayer->field_2A6C)
        {
            ret = stanGetPositionYValue(g_CurrentPlayer->field_2A70, arg1, arg2);
        }
        else
        {
            ret = stanGetPositionYValue(arg0, arg1, arg2);
        }
    }

    return ret;
}



/**
 * US Address 0x7F080DF8.
 * EU Address 0x7F080E9C.
 */
void bondviewUpdatePlayerY(s32 use_stanHeight, f32 stanHeight_offset)
{
    s32 i; // sp6c
    f32 unused;
    f32 sp64;
    StandTile *stan; //sp60
    f32 collision_radius; //sp5c
    f32 height; //sp58
    f32 always_30; //sp54
    f32 temp_f0; // no stack
    f32 new_field_70; // sp4c
    f32 new_field_7c; //sp48
    f32 ftemp2;
    f32 sp40;

    if (1);

    if (g_PlayerIsInTank == 1)
    {
        g_CurrentPlayer->stanHeight = bondviewYPositionRelated(
            g_CurrentPlayer->field_488.current_tile_ptr,
            g_CurrentPlayer->field_488.collision_position.f[0],
            g_CurrentPlayer->field_488.collision_position.f[2]);

        g_CurrentPlayer->field_6C = g_CurrentPlayer->field_70 / (1.0f - TANK_UNKD0_SCALE);

        for (i=0; i<g_ClockTimer; i++)
        {
            g_CurrentPlayer->field_6C = (g_CurrentPlayer->field_6C * TANK_UNKD0_SCALE) + g_CurrentPlayer->stanHeight;
        }

        g_CurrentPlayer->field_70 = g_CurrentPlayer->field_6C * (1.0f - TANK_UNKD0_SCALE);
    }
    else
    {
        if (use_stanHeight != 0)
        {
            g_CurrentPlayer->stanHeight = g_CurrentPlayer->stanHeight + stanHeight_offset;

            temp_f0 = bondviewYPositionRelated(
                g_CurrentPlayer->field_488.current_tile_ptr,
                g_CurrentPlayer->field_488.collision_position.f[0],
                g_CurrentPlayer->field_488.collision_position.f[2]);

            if (g_CurrentPlayer->stanHeight < temp_f0)
            {
                g_CurrentPlayer->stanHeight = temp_f0;
            }
        }
        else
        {
            stan = g_CurrentPlayer->field_488.current_tile_ptr;

            bondviewGetCollisionRadius(g_CurrentPlayer->prop, &collision_radius, &height, &always_30);

            sp64 = bondviewYPositionRelated(
                g_CurrentPlayer->field_488.current_tile_ptr,
                g_CurrentPlayer->field_488.collision_position.f[0],
                g_CurrentPlayer->field_488.collision_position.f[2]);

            // Another error checking block, it seems this condition is almost never triggered in the game.
            if (stanTestLocusEdgeAboveY(
                &stan,
                g_CurrentPlayer->field_488.collision_position.f[0],
                g_CurrentPlayer->field_488.collision_position.f[2],
                collision_radius,
                bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer) + sp64) >= 0)
            {
                if (sp64 < g_CurrentPlayer->stanHeight)
                {
                    sp64 = g_CurrentPlayer->stanHeight;
                }
            }

            g_CurrentPlayer->stanHeight = sp64;
        }

        if ((g_CurrentPlayer->field_2A6C != 0) && (g_CurrentPlayer->field_70 < g_CurrentPlayer->stanHeight))
        {
            g_CurrentPlayer->field_2A6C = 0;
            g_CurrentPlayer->field_488.current_tile_ptr = g_CurrentPlayer->field_2A70;
            g_CurrentPlayer->field_2A70 = NULL;
        }

        if ((g_CurrentPlayer->field_7C >= 0.0f) || (g_CurrentPlayer->field_70 < g_CurrentPlayer->stanHeight))
        {
            g_CurrentPlayer->field_6C = g_CurrentPlayer->field_70 / (1.0f - TANK_UNKD0_SCALE);

            for (i=0; i<g_ClockTimer; i++)
            {
                g_CurrentPlayer->field_6C = (g_CurrentPlayer->field_6C * TANK_UNKD0_SCALE) + g_CurrentPlayer->stanHeight;
            }

            if (g_CurrentPlayer->field_70 < g_CurrentPlayer->stanHeight)
            {
                g_CurrentPlayer->field_70 = g_CurrentPlayer->field_6C * (1.0f - TANK_UNKD0_SCALE);
            }
        }

        if (g_CurrentPlayer->stanHeight < g_CurrentPlayer->field_70)
        {
            new_field_7c = g_CurrentPlayer->field_7C;
            new_field_70 = g_CurrentPlayer->field_70;

            if ((get_debug_fast_bond_flag() != 0) && (g_ForceBondMoveOffset.f[0] == 0.0f) && (g_ForceBondMoveOffset.f[2] == 0.0f))
            {
                sp40 = 1.388889f;
            }
            else
            {
                sp40 = 0.27777779f;
            }

            ftemp2 = new_field_7c - (g_GlobalTimerDelta * sp40);
            new_field_70 += (g_GlobalTimerDelta * (new_field_7c + ftemp2) * 0.5f);
            new_field_7c = ftemp2;

            if (new_field_70 < g_CurrentPlayer->stanHeight)
            {
                new_field_70 = g_CurrentPlayer->stanHeight;
                new_field_7c = -sqrtf((g_CurrentPlayer->field_7C * g_CurrentPlayer->field_7C) + (((2.0f * (g_CurrentPlayer->field_70 - g_CurrentPlayer->stanHeight) * 0.27777779f) / 60.0f) * 60.0f));


                if (g_CurrentPlayer->field_2A6C != 0)
                {
                    g_CurrentPlayer->field_2A6C = 0;
                    g_CurrentPlayer->field_488.current_tile_ptr = g_CurrentPlayer->field_2A70;
                    g_CurrentPlayer->field_2A70 = NULL;
                }
            }

            g_CurrentPlayer->field_70 = new_field_70;
            g_CurrentPlayer->field_7C = new_field_7c;
        }

        if ((g_CurrentPlayer->field_7C < 0.0f) && (g_CurrentPlayer->field_70 <= g_CurrentPlayer->stanHeight))
        {
            if (g_CurrentPlayer->field_7C < -13.333333f)
            {
                g_CurrentPlayer->field_8C = CLIPPING_FIELD8C_VALUE;
                g_CurrentPlayer->vertical_bounce_adjust = -90.0f;
            }
            else if (g_CurrentPlayer->field_7C < -5.0f)
            {
                g_CurrentPlayer->field_8C = CLIPPING_FIELD8C_VALUE;
                g_CurrentPlayer->vertical_bounce_adjust = ((-5.0f - g_CurrentPlayer->field_7C) * -90.0f) / 8.333333f;
            }

            g_CurrentPlayer->field_7C = 0.0f;
        }

        if (g_CurrentPlayer->field_2A6C != 0)
        {
            if (g_CurrentPlayer->field_70 + bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer)
                < stanGetPositionYValue(
                    g_CurrentPlayer->field_488.current_tile_ptr,
                    g_CurrentPlayer->field_488.collision_position.f[0],
                    g_CurrentPlayer->field_488.collision_position.f[2]))
            {
                g_CurrentPlayer->field_2A6C = 0;
                g_CurrentPlayer->field_488.current_tile_ptr = g_CurrentPlayer->field_2A70;
                g_CurrentPlayer->field_2A70 = NULL;
            }
        }
    }

    for (i=0; i<g_ClockTimer; i++)
    {
        if (g_CurrentPlayer->field_8C > 0)
        {
            g_CurrentPlayer->field_84 = (g_CurrentPlayer->field_84 * CLIPPING_CLOCK_FACTOR) + g_CurrentPlayer->vertical_bounce_adjust;
            g_CurrentPlayer->field_8C += -1;
        }
        else
        {
            if (g_CurrentPlayer->vertical_bounce_adjust < 0.0f)
            {
                g_CurrentPlayer->vertical_bounce_adjust -= CLIPPING_FIELD90_VALUE;

                if (0.0f <= g_CurrentPlayer->vertical_bounce_adjust)
                {
                    g_CurrentPlayer->vertical_bounce_adjust = 0.0f;
                }
            }

            g_CurrentPlayer->field_84 = (g_CurrentPlayer->field_84 * CLIPPING_CLOCK_FACTOR) + g_CurrentPlayer->vertical_bounce_adjust;
        }
    }

    g_CurrentPlayer->field_88 = g_CurrentPlayer->field_84 * CLIPPING_FIELD88_FACTOR;
}





/**
 * Address 0x7F081478 (NTSC).
 * Address 0x7F08151C (PAL).
*/
void bondviewUpdatePlayerCollisionPositionFields(void)
{
    f32 phi_f0;
    s32 i;
    StandTile *sp2C;
    s32 sp28;

    g_CurrentPlayer->field_29BC = (g_CurrentPlayer->headpos.f[1] * g_playerPerm->player_perspective_height) + 7.0f;

    phi_f0 = g_CurrentPlayer->field_29BC +
        ((g_CurrentPlayer->field_88 + g_CurrentPlayer->ducking_height_offset) * g_playerPerm->player_perspective_height);

    if (phi_f0 < 30.0f)
    {
        phi_f0 = 30.0f;
    }

    g_CurrentPlayer->field_488.collision_position.f[1] = g_CurrentPlayer->field_70 + phi_f0;

    if (((g_CameraMode != CAMERAMODE_DEATH_CAM_SP) && (g_CameraMode != CAMERAMODE_DEATH_CAM_MP) && (g_CameraMode != CAMERAMODE_POSEND))
        || (g_CurrentPlayer->ptr_char_objectinstance == 0))
    {
        g_CurrentPlayer->field_488.pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
        g_CurrentPlayer->field_488.pos.f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
        g_CurrentPlayer->field_488.pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2];
    }

    if (g_CurrentPlayer->bonddead != FALSE)
    {
        if (g_CurrentPlayer->field_29C0 > 0.0f)
        {
            g_CurrentPlayer->field_29C0 -= 0.25f;

            if (g_CurrentPlayer->field_29C0 < 0.0f)
            {
                g_CurrentPlayer->field_29C0 = 0.0f;
            }
        }
    }

    if (g_CurrentPlayer->vv_verta < 0.0f)
    {
        g_CurrentPlayer->field_488.pos.f[1] += -(1.0f - g_CurrentPlayer->vv_cosverta) * g_CurrentPlayer->field_29C0;
    }

    sp2C = g_CurrentPlayer->field_488.current_tile_ptr;
    sp28 = stanlinelog_flag;
    stanlinelog_flag = 0;

    walkTilesBetweenPoints_NoCallback(
        &sp2C,
        g_CurrentPlayer->field_488.collision_position.f[0],
        g_CurrentPlayer->field_488.collision_position.f[2],
        g_CurrentPlayer->field_488.pos.f[0],
        g_CurrentPlayer->field_488.pos.f[2]);

    stanlinelog_flag = sp28;

    g_CurrentPlayer->field_488.current_tile_ptr_for_portals = sp2C;

    g_CurrentPlayer->field_488.pos3.f[0] = g_CurrentPlayer->field_488.pos.f[0];
    g_CurrentPlayer->field_488.pos3.f[2] = g_CurrentPlayer->field_488.pos.f[2];

    g_CurrentPlayer->field_488.pos3.f[1] = bondviewYPositionRelated(
        g_CurrentPlayer->field_488.current_tile_ptr_for_portals,
        g_CurrentPlayer->field_488.pos.f[0],
        g_CurrentPlayer->field_488.pos.f[2]);

    g_CurrentPlayer->prop->stan = g_CurrentPlayer->field_488.current_tile_ptr;

    g_CurrentPlayer->prop->pos.f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
    g_CurrentPlayer->prop->pos.f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
    g_CurrentPlayer->prop->pos.f[2] = g_CurrentPlayer->field_488.collision_position.f[2];

#if defined(VERSION_EU)
#define S7F081478_FACTOR_1 0.881200015545f
#else
#define S7F081478_FACTOR_1 0.9f
#endif
    for (i=0; i<g_ClockTimer; i++)
    {
        g_CurrentPlayer->field_3B8.f[0] = (S7F081478_FACTOR_1 * g_CurrentPlayer->field_3B8.f[0]) + g_CurrentPlayer->field_488.pos.f[0];
        g_CurrentPlayer->field_3B8.f[1] = (S7F081478_FACTOR_1 * g_CurrentPlayer->field_3B8.f[1]) + g_CurrentPlayer->field_488.pos.f[1];
        g_CurrentPlayer->field_3B8.f[2] = (S7F081478_FACTOR_1 * g_CurrentPlayer->field_3B8.f[2]) + g_CurrentPlayer->field_488.pos.f[2];
    }

#if defined(VERSION_EU)
#define S7F081478_FACTOR_2 0.118799984455f
#else
#define S7F081478_FACTOR_2 0.100000024f
#endif
    g_CurrentPlayer->field_3C4 = g_CurrentPlayer->field_3B8.f[0] * S7F081478_FACTOR_2;
    g_CurrentPlayer->field_3C8 = g_CurrentPlayer->field_3B8.f[1] * S7F081478_FACTOR_2;
    g_CurrentPlayer->field_3CC = g_CurrentPlayer->field_3B8.f[2] * S7F081478_FACTOR_2;
}





/**
 * Fixes vv_verta within -90 and +90.
 * Updates vv_costheta, vv_sintheta, vv_verta360, vv_cosverta, vv_sinverta, field_488.theta_transform.
 * Address 0x7F081790.
 *
 * Perfect Dark function bmoveUpdateVerta.
*/
void bondviewApplyVertaTheta(void)
{
    while (g_CurrentPlayer->vv_verta < -180.0f)
    {
        g_CurrentPlayer->vv_verta += 360.0f;
    }

    while (g_CurrentPlayer->vv_verta >= 180.0f)
    {
        g_CurrentPlayer->vv_verta -= 360.0f;
    }

    if (g_CurrentPlayer->vv_verta > 90.0f)
    {
        g_CurrentPlayer->vv_verta = 90.0f;
    }
    else if (g_CurrentPlayer->vv_verta < -90.0f)
    {
        g_CurrentPlayer->vv_verta = -90.0f;
    }

    g_CurrentPlayer->vv_costheta = cosf(g_CurrentPlayer->vv_theta * DegToRad1Fact(1));
    g_CurrentPlayer->vv_sintheta = sinf(g_CurrentPlayer->vv_theta * DegToRad1Fact(1));

    g_CurrentPlayer->vv_verta360 = g_CurrentPlayer->vv_verta;
    if (g_CurrentPlayer->vv_verta360 < 0.0f)
    {
        g_CurrentPlayer->vv_verta360 += 360.0f;
    }

    g_CurrentPlayer->vv_cosverta = cosf(g_CurrentPlayer->vv_verta360 * DegToRad1Fact(1));
    g_CurrentPlayer->vv_sinverta = sinf(g_CurrentPlayer->vv_verta360 * DegToRad1Fact(1));

    g_CurrentPlayer->field_488.theta_transform.f[0] = -g_CurrentPlayer->vv_sintheta;
    g_CurrentPlayer->field_488.theta_transform.f[1] = 0;
    g_CurrentPlayer->field_488.theta_transform.f[2] = g_CurrentPlayer->vv_costheta;
}


/**
 * US address 7F081974.
 * EU address 7F081A18.
 * Perfect Dark method bmoveProcessInput.
*/
void bondviewProcessInput(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    struct MoveData moveData; // sp120

    s8 player_joyGetStickX; // sp11F
    s8 player_joyGetStickY; // sp11E
    u16 player_joyGetButtons; // sp11C
    u16 copy_prev_buttons_pressed; // sp11A
    s32 adjustedStickX;
    s32 tmpc2sticky;
    s32 sp10C;
    s32 sp108;
    s32 sp104;
    s32 sp100;
    u16 shootButtons; // spFE
    u16 aimButtons; // spFC
    u16 invButtons; // spFA
    // missing spF8
    TankRecord *spF4;
    s32 i_0; // spF0
    TankRecord *spEC;
    f32 ftemp_nostack_spE8; // unused
    f32 noiseRadius; // spE4
    f32 ftemp_nostack_spE0;
    f32 targetSpeed;
    f32 ftemp_nostack_spD8;
    f32 unadjustedTargetSpeed;
    f32 ftemp_nostack_spD0;
    f32 ftemp_nostack_spCC;
    f32 ftemp_nostack_spC8;
    f32 targetPitch;
    StandTile *spC0;
    f32 spBC;
    f32 ftemp_nostack_spB8;
    struct coord3d spAC;
    struct coord3d spA0;
    s32 stack_padding_sp9C; // unused
    f32 ftemp_nostack_sp98;
    f32 ftemp_nostack_sp94;
    f32 ftemp_nostack_sp90; // unused
    f32 ftemp_nostack_sp8C;
    f32 ftemp_nostack_sp88;
    f32 ftemp_nostack_sp84;
    f32 ftemp_nostack_sp80;
    s32 i_1;
    f32 ftemp_nostack_sp78;
    s32 canCycleWeapons; // sp74
    f32 sp70;


    moveData.canSwivelGun = 0;
    moveData.canManualAim = 0;
    moveData.triggerOn = 0;
    moveData.btap = 0;
    moveData.canLookAhead = 0;
    moveData.canTurnTank = 0;
    moveData.canNaturalTurn = 0;
    moveData.canNaturalPitch = 0;
    moveData.digitalStepForward = 0;
    moveData.digitalStepBack = 0;
    moveData.digitalStepLeft = 0;
    moveData.digitalStepRight = 0;
    moveData.tankTurnLeftSpeed = 0;
    moveData.tankTurnRightSpeed = 0;
    moveData.speedVertaDown = 0;
    moveData.speedVertaUp = 0;
    moveData.aimTurnLeftSpeed = 0;
    moveData.aimTurnRightSpeed = 0;
    moveData.weaponBackOffset = 0;
    moveData.weaponForwardOffset = 0;
    moveData.aiming = 0;
    moveData.zooming = 0;
    moveData.zoomOutFovPersec = 0;
    moveData.zoomInFovPersec = 0;
    moveData.crouchDown = 0;
    moveData.crouchUp = 0;
    moveData.rLeanLeft = 0;
    moveData.rLeanRight = 0;
    moveData.detonating = 0;
    moveData.canAutoAim = 0;
    moveData.invertPitch = get_cur_player_look_vertical_inverted() == 0;
    moveData.disableLookAhead = 0;

    if (stick_x < -5) {
		moveData.controlStickXSafe = stick_x + 5;
	} else if (stick_x > 5) {
		moveData.controlStickXSafe = stick_x - 5;
	} else {
		moveData.controlStickXSafe = 0;
	}

	if (stick_y < -5) {
		moveData.controlStickYSafe = stick_y + 5;
	} else if (stick_y > 5) {
		moveData.controlStickYSafe = stick_y - 5;
	} else {
		moveData.controlStickYSafe = 0;
	}

    moveData.controlStickXRaw = (s32)stick_x;
    moveData.controlStickYRaw = (s32)stick_y;

    moveData.analogTurn = moveData.controlStickXSafe;
    moveData.analogPitch = moveData.controlStickYSafe;
    moveData.analogStrafe = moveData.controlStickXSafe;
    moveData.analogWalk = moveData.controlStickYSafe;

    if (g_CurrentPlayer->bonddead == FALSE
        && g_bondviewForceDisarm <= 0
        && (
            (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x5
                && ((buttons & ~oldbuttons) & START_BUTTON)
            )
            ||
            (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5
                && g_CurrentPlayer->open_close_solo_watch_menu)
        )
        && (getPlayerCount() == 1))
    {
        trigger_solo_watch_menu(0);
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0
        && g_CurrentPlayer->bonddead == FALSE
        && (
            getPlayerCount() == 1
            || (
                g_stopPlayFlag == 0
                && g_gameOverFlag == 0)))
    {
        if (cur_player_get_control_type() == CONTROLLER_CONFIG_DOMINO /* 2.3 */
            || cur_player_get_control_type() == CONTROLLER_CONFIG_GOODHEAD /* 2.4 */
            || cur_player_get_control_type() == CONTROLLER_CONFIG_GALORE /* 2.2 */
            || cur_player_get_control_type() == CONTROLLER_CONFIG_PLENTY /* 2.1 */
            )
        {
            player_joyGetStickX = joyGetStickX(get_cur_playernum() + getPlayerCount());
            player_joyGetStickY = joyGetStickY(get_cur_playernum() + getPlayerCount());
            player_joyGetButtons = joyGetButtons(get_cur_playernum() + getPlayerCount() , (u32)ANY_BUTTON);

            copy_prev_buttons_pressed = g_CurrentPlayer->prev_buttons_pressed;

            if (player_joyGetStickX < -5)
            {
                adjustedStickX = player_joyGetStickX + 5;
            }
            else if (player_joyGetStickX > 5)
            {
                adjustedStickX = player_joyGetStickX - 5;
            }
            else
            {
                adjustedStickX = 0;
            }

            if (player_joyGetStickY < -5)
            {
                tmpc2sticky = player_joyGetStickY + 5;
            }
            else if (player_joyGetStickY >= 6)
            {
                tmpc2sticky = player_joyGetStickY - 5;
            }
            else
            {
                tmpc2sticky = 0;
            }

            /* 2.1 and 2.3 */
            if (cur_player_get_control_type() == CONTROLLER_CONFIG_PLENTY || (cur_player_get_control_type() == CONTROLLER_CONFIG_DOMINO))
            {
                moveData.analogStrafe = adjustedStickX;
                moveData.analogPitch = tmpc2sticky;
            }
            else
            {
                if (g_PlayerIsInTank == 1 && !g_CurrentPlayer->insightaimmode)
                {
                    moveData.analogTurn = adjustedStickX;
                }
                else
                {
                    moveData.analogStrafe = adjustedStickX;
                }

                moveData.analogWalk = tmpc2sticky;
            }

            /* 2.1 and 2.2 */
            if (cur_player_get_control_type() == CONTROLLER_CONFIG_PLENTY || cur_player_get_control_type() == CONTROLLER_CONFIG_GALORE)
            {
                sp104 = (player_joyGetButtons & Z_TRIG) != 0;
                sp100 = ((player_joyGetButtons & ~copy_prev_buttons_pressed) & Z_TRIG) != 0;
                sp10C = (buttons & Z_TRIG) != 0;
                sp108 = ((buttons & ~oldbuttons) & Z_TRIG) != 0;
            }
            else
            {
                sp104 = (buttons & Z_TRIG) != 0;
                sp100 = ((buttons & ~oldbuttons) & Z_TRIG) != 0;
                sp10C = (player_joyGetButtons & Z_TRIG) != 0;
                sp108 = ((player_joyGetButtons & ~copy_prev_buttons_pressed) & Z_TRIG) != 0;
            }

            if (lvlGetControlsLockedFlag() == 0 && disablePlayerActionsWhenPausedOrInMpMenu())
            {
                if (cur_player_get_aim_control() == 0)
                {
                    g_CurrentPlayer->insightaimmode = sp104;
                }
                else if (sp100)
                {
                    g_CurrentPlayer->insightaimmode = !g_CurrentPlayer->insightaimmode;
                }

                moveData.canSwivelGun = !g_CurrentPlayer->insightaimmode;
                moveData.canAutoAim = !g_CurrentPlayer->insightaimmode;

                moveData.canManualAim = g_CurrentPlayer->insightaimmode;

                moveData.btap = (
                    (((buttons & ~oldbuttons) & B_BUTTON) != 0)
                    ||
                    ((((player_joyGetButtons & ~copy_prev_buttons_pressed) & B_BUTTON)) != 0)
                    );

                moveData.canLookAhead = !g_CurrentPlayer->insightaimmode;
                moveData.canTurnTank = 1;
                moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                moveData.canNaturalPitch = !g_CurrentPlayer->insightaimmode;

                if (g_CurrentPlayer->insightaimmode && (stick_y > 60))
                {
                    moveData.speedVertaDown = (f32) (stick_y - 60) / FLOAT_TEN_B;
                    if (moveData.speedVertaDown > 1.0f)
                    {
                        moveData.speedVertaDown = 1.0f;
                    }
                }
                else
                {
                    //moveData.speedVertaDown = 0;
                }

                if (g_CurrentPlayer->insightaimmode && (stick_y < -60))
                {
                    moveData.speedVertaUp = (f32) (-60 - stick_y) / FLOAT_TEN_B;
                    if (moveData.speedVertaUp > 1.0f)
                    {
                        moveData.speedVertaUp = 1.0f;
                    }
                }
                else
                {
                    //moveData.speedVertaUp = 0;
                }


                if (g_CurrentPlayer->insightaimmode && (stick_x < -60))
                {
                    moveData.aimTurnLeftSpeed = (f32) (-60 - stick_x) / FLOAT_TEN_B;
                    if (moveData.aimTurnLeftSpeed > 1.0f)
                    {
                        moveData.aimTurnLeftSpeed = 1.0f;
                    }
                }
                else
                {
                    //moveData.aimTurnLeftSpeed = 0;
                }

                if (g_CurrentPlayer->insightaimmode && (stick_x > 60) )
                {
                    moveData.aimTurnRightSpeed = (f32) (stick_x - 60) / FLOAT_TEN_B;
                    if (moveData.aimTurnRightSpeed > 1.0f)
                    {
                        moveData.aimTurnRightSpeed = 1.0f;
                    }
                }
                else
                {
                    //moveData.aimTurnRightSpeed = 0;
                }

                moveData.weaponBackOffset = (
                        ((buttons & A_BUTTON) != 0)
                        ||
                        ((player_joyGetButtons & A_BUTTON) != 0)
                    )
                    && (sp108);

               moveData.weaponForwardOffset = (
                    (
                       (((buttons & ~oldbuttons) & A_BUTTON) != 0)
                       ||
                       (((player_joyGetButtons & ~copy_prev_buttons_pressed) & A_BUTTON) != 0)
                    ))
                    && (!sp10C);

                moveData.aiming = g_CurrentPlayer->insightaimmode;
                moveData.zooming = g_CurrentPlayer->insightaimmode;

                if ((bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH))
                    && g_CurrentPlayer->insightaimmode)
                {
                    if (tmpc2sticky < 0)
                    {
                        moveData.zoomOutFovPersec = (f32) -tmpc2sticky / 70.0f;
                        if (moveData.zoomOutFovPersec > 1.0f)
                        {
                            moveData.zoomOutFovPersec = 1.0f;
                        }

                        moveData.zoomOutFovPersec *= 2.0f;
                    }

                    if (tmpc2sticky > 0)
                    {
                        moveData.zoomInFovPersec = (f32) tmpc2sticky / 70.0f;
                        if (moveData.zoomInFovPersec > 1.0f)
                        {
                            moveData.zoomInFovPersec = 1.0f;
                        }

                        moveData.zoomInFovPersec *= 2.0f;
                    }
                }

                moveData.crouchDown = (bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                    && (g_CurrentPlayer->insightaimmode)
                    && (player_joyGetStickY < -30);

                moveData.crouchUp = (bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                    && (g_CurrentPlayer->insightaimmode)
                    && (player_joyGetStickY > 30);

                if ((
                           (((buttons & A_BUTTON) != 0) && (((buttons & ~oldbuttons) & B_BUTTON) != 0))
                        || (((buttons & B_BUTTON) != 0) && (((buttons & ~oldbuttons) & A_BUTTON) != 0))
                        || ((player_joyGetButtons & A_BUTTON) && ((player_joyGetButtons & ~copy_prev_buttons_pressed) & B_BUTTON))
                        || ((player_joyGetButtons & B_BUTTON) && ((player_joyGetButtons & ~copy_prev_buttons_pressed) & A_BUTTON)))
                    && (getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_REMOTEMINE))
                {
                    moveData.detonating = 1;
                    moveData.weaponBackOffset = 0;
                    moveData.weaponForwardOffset = 0;
                    moveData.btap = 0;
                }

                if (g_PlayerIsInTank == 1 && g_CurrentPlayer->insightaimmode)
                {
                    if (getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_TANKSHELLS)
                    {
                        moveData.controlStickXRaw = 0;

                        if (moveData.analogStrafe == 0)
                        {
                            moveData.analogStrafe = moveData.analogTurn;
                        }
                    }
                    else if (moveData.analogStrafe == 0)
                    {
                        if (moveData.aimTurnLeftSpeed > 0)
                        {
                            moveData.tankTurnLeftSpeed = moveData.aimTurnLeftSpeed;
                        }

                        if (moveData.aimTurnRightSpeed > 0)
                        {
                            moveData.tankTurnRightSpeed = moveData.aimTurnRightSpeed;
                        }
                    }

                    moveData.aimTurnLeftSpeed = 0;
                    moveData.aimTurnRightSpeed = 0;
                }
            }

            moveData.triggerOn = (sp10C)
                && (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
                && ((buttons & A_BUTTON) == 0)
                && ((player_joyGetButtons & A_BUTTON) == 0);

            moveData.disableLookAhead = 1;
            g_CurrentPlayer->prev_buttons_pressed = player_joyGetButtons;
        }
        else
        {
            /* 1.3 and 1.4 */
            if (cur_player_get_control_type() == CONTROLLER_CONFIG_KISSY
                || cur_player_get_control_type() == CONTROLLER_CONFIG_GOODNIGHT)
            {
                shootButtons = A_BUTTON;
                aimButtons = Z_TRIG;
                invButtons = L_TRIG | R_TRIG;
            }
            else
            {
                shootButtons = Z_TRIG;
                aimButtons = L_TRIG | R_TRIG;
                invButtons = A_BUTTON;
            }

            if (lvlGetControlsLockedFlag() == 0)
            {
                if (disablePlayerActionsWhenPausedOrInMpMenu())
                {
                    if (cur_player_get_aim_control() == 0)
                    {
                        g_CurrentPlayer->insightaimmode = (buttons & aimButtons) != 0;
                    }
                    else if ((buttons & ~oldbuttons) & aimButtons)
                    {
                        g_CurrentPlayer->insightaimmode = !g_CurrentPlayer->insightaimmode;
                    }

                    moveData.canSwivelGun = !g_CurrentPlayer->insightaimmode;
                    moveData.canAutoAim = !g_CurrentPlayer->insightaimmode;
                    moveData.btap = ((buttons & ~oldbuttons) & B_BUTTON) != 0;
                    moveData.canManualAim = g_CurrentPlayer->insightaimmode;

                    /* 1.2 and 1.4 */
                    if (cur_player_get_control_type() == CONTROLLER_CONFIG_SOLITARE
                        || cur_player_get_control_type() == CONTROLLER_CONFIG_GOODNIGHT)
                    {
                        if ((buttons & (L_JPAD | L_CBUTTONS)) != 0)
                        {
                            if (!g_CurrentPlayer->insightaimmode)
                            {
                                if (g_PlayerIsInTank == 1)
                                {
                                    moveData.aimTurnLeftSpeed = 1.0f;
                                }
                                else
                                {
                                    moveData.digitalStepLeft = 1;
                                }
                            }
                            else
                            {
                                moveData.tankTurnLeftSpeed = 1.0f;
                            }
                        }

                        if ((buttons & (R_JPAD | R_CBUTTONS)) != 0)
                        {
                            if (!g_CurrentPlayer->insightaimmode)
                            {
                                if (g_PlayerIsInTank == 1)
                                {
                                    moveData.aimTurnRightSpeed = 1.0f;
                                }
                                else
                                {
                                    moveData.digitalStepRight = 1;
                                }
                            }
                            else
                            {
                                moveData.tankTurnRightSpeed = 1.0f;
                            }
                        }

                        moveData.digitalStepForward = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (U_JPAD | U_CBUTTONS)) );

                        moveData.digitalStepBack = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (D_JPAD | D_CBUTTONS)));

                        moveData.canNaturalPitch = !g_CurrentPlayer->insightaimmode;

                        if (g_PlayerIsInTank == 1)
                        {
                            moveData.canTurnTank = !g_CurrentPlayer->insightaimmode;
                        }
                        else
                        {
                            moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                        }
                    }
                    else
                    {
                        if ((buttons & (s32)(L_JPAD | L_CBUTTONS)) != 0)
                        {
                            moveData.tankTurnLeftSpeed = 1.0f;
                        }
                        /* optional else statement, matches with or without. */
                        else
                        {
                            moveData.tankTurnLeftSpeed = 0;
                        }

                        if ((buttons & (s32)(R_JPAD | R_CBUTTONS)) != 0)
                        {
                            moveData.tankTurnRightSpeed = 1.0f;
                        }
                        /* optional else statement, matches with or without. */
                        else
                        {
                            moveData.tankTurnRightSpeed = 0;
                        }

                        moveData.digitalStepLeft = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (s32)(L_JPAD | L_CBUTTONS)) );

                        moveData.digitalStepRight = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (s32)(R_JPAD | R_CBUTTONS)));

                        moveData.canLookAhead = !g_CurrentPlayer->insightaimmode;

                        if ((!g_CurrentPlayer->insightaimmode) && (buttons & (U_JPAD | U_CBUTTONS)) )
                        {
                            moveData.speedVertaDown = 1.0f;
                        }

                        if ((!g_CurrentPlayer->insightaimmode) && (buttons & (D_JPAD | D_CBUTTONS)))
                        {
                            moveData.speedVertaUp = 1.0f;
                        }

                        moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_y > 60))
                    {
                        moveData.speedVertaDown = (f32) (stick_y - 60) / FLOAT_TEN_B;
                        if (moveData.speedVertaDown > 1.0f)
                        {
                            moveData.speedVertaDown = 1.0f;
                        }
                    }
                    else if ((g_CurrentPlayer->insightaimmode) && (stick_y < -60))
                    {
                        moveData.speedVertaUp = (f32) (-60 - stick_y) / FLOAT_TEN_B;
                        if (moveData.speedVertaUp > 1.0f)
                        {
                            moveData.speedVertaUp = 1.0f;
                        }
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_x < -60))
                    {
                        moveData.aimTurnLeftSpeed = (f32) (-60 - stick_x) / FLOAT_TEN_B;
                        if (moveData.aimTurnLeftSpeed > 1.0f)
                        {
                            moveData.aimTurnLeftSpeed = 1.0f;
                        }
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_x > 60))
                    {
                        moveData.aimTurnRightSpeed = (f32) (stick_x - 60) / FLOAT_TEN_B;
                        if (moveData.aimTurnRightSpeed > 1.0f)
                        {
                            moveData.aimTurnRightSpeed = 1.0f;
                        }
                    }

                    moveData.weaponBackOffset =
                        ((buttons & invButtons) != 0)
                        &&
                        (((buttons & ~oldbuttons) & shootButtons) != 0)
                        ;

                    moveData.weaponForwardOffset =
                        (((buttons & ~oldbuttons) & invButtons) != 0)
                        &&
                        ((buttons & shootButtons) == 0)
                        ;

                    moveData.aiming = g_CurrentPlayer->insightaimmode;
                    moveData.zooming = g_CurrentPlayer->insightaimmode;

                    if ((bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH))
                        && g_CurrentPlayer->insightaimmode
                        )
                    {
                        /* down = 0x404 */
                        if ((buttons & (D_JPAD | D_CBUTTONS)) != 0)
                        {
                            moveData.zoomOutFovPersec = 1.0f;
                        }

                        if ((buttons & (U_JPAD | U_CBUTTONS)) != 0)
                        {
                            moveData.zoomInFovPersec = 1.0f;
                        }
                    }

                    moveData.crouchDown = (bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                        && (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (D_JPAD | D_CBUTTONS)));

                    moveData.crouchUp = (bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                        && (g_CurrentPlayer->insightaimmode)
                        && ((~buttons & (U_JPAD | U_CBUTTONS)));

                    moveData.rLeanLeft = (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (L_JPAD | L_CBUTTONS)));

                    moveData.rLeanRight = (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (R_JPAD | R_CBUTTONS)));

                    if (
                        ((((buttons & invButtons) != 0) && (((buttons & ~oldbuttons) & B_BUTTON) != 0))
                            || ((buttons & B_BUTTON) && (((buttons & ~oldbuttons) & invButtons) != 0)))
                        && (getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_REMOTEMINE))
                    {
                        moveData.detonating = 1;
                        moveData.weaponBackOffset = 0;
                        moveData.weaponForwardOffset = 0;
                        moveData.btap = 0;
                    }

                    if ((g_PlayerIsInTank == 1) && (g_CurrentPlayer->insightaimmode))
                    {
                        if (getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_TANKSHELLS)
                        {
                            moveData.controlStickXRaw = 0;
                            moveData.canTurnTank = 1;
                        }
                        else if ((moveData.tankTurnLeftSpeed == 0) && (moveData.tankTurnRightSpeed == 0))
                        {
                            if (moveData.aimTurnLeftSpeed > 0)
                            {
                                moveData.tankTurnLeftSpeed = moveData.aimTurnLeftSpeed;
                            }

                            if (moveData.aimTurnRightSpeed > 0)
                            {
                                moveData.tankTurnRightSpeed = moveData.aimTurnRightSpeed;
                            }
                        }

                        moveData.aimTurnLeftSpeed = 0;
                        moveData.aimTurnRightSpeed = 0;
                    }
                }
            }

            moveData.triggerOn = ((buttons & shootButtons)  != 0)
                && (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
                && ((buttons & invButtons) == 0);

            /* 1.2 and 1.4 */
            if (cur_player_get_control_type() == CONTROLLER_CONFIG_SOLITARE || cur_player_get_control_type() == CONTROLLER_CONFIG_GOODNIGHT)
            {
                moveData.disableLookAhead = 1;
            }
        }
    }

    g_CurrentPlayer->field_D0 = 0;

    if (moveData.btap)
    {
        /* If Bond is in the tank and pressed B, then exit. */
        if (g_PlayerIsInTank == 1)
        {
            spF4 = (struct TankRecord *)g_PlayerTankProp->obj;
            spF4->unkD8 = get_ammo_count_for_weapon(ITEM_TANKSHELLS);

            add_ammo_to_weapon(ITEM_TANKSHELLS, 0);
            bondinvRemoveItemByID(ITEM_TANKSHELLS);

            if (getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_TANKSHELLS)
            {
                spF4->unkD8 += get_ammo_in_hands_magazine(GUNRIGHT);
                autoadvance_on_deplete_all_ammo();
            }

            spF4->is_firing_tank = 0;
            g_PlayerIsInTank = 0;
            g_CurrentPlayer->speedsideways = 0;
            g_CurrentPlayer->speedforwards = 0;
            g_CurrentPlayer->speedtheta = 0;

            for (i_0=0; i_0<3; i_0++)
            {
                g_CurrentPlayer->bondshotspeed.f[i_0] = 0;
            }

            g_CurrentPlayer->crouchpos = CROUCH_STAND;
        }
        /* If Bond is standing on the tank and pressed B, enter the tank. */
        else if (g_PlayerTankProp != NULL
            && g_PlayerTankProp->type == PROP_TYPE_OBJ
            && g_PlayerTankProp->obj->type == PROPDEF_TANK
            && g_BondCanEnterTank)
        {
            spEC = (struct TankRecord *)g_PlayerTankProp->obj;

            bondinvAddInvItem(ITEM_TANKSHELLS);
            add_ammo_to_weapon(ITEM_TANKSHELLS, spEC->unkD8);
            spEC->unkD8 = 0;
            g_TankTurretVerticalAngle = spEC->turret_vertical_angle;
            g_TankTurretVerticalAngleRelated = g_TankTurretVerticalAngle / TANK_VERT_ANGLE_FACTOR;
            g_TankTurretAngle = spEC->turret_orientation_angle;
            g_TankTurretOrientationAngleRad = spEC->turret_orientation_angle;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / TANK_VERT_ANGLE_RAD_FACTOR;
            tank_turret_turn_speed = 0;
            g_TankOrientationAngle = spEC->tank_orientation_angle;
            g_TankTurnSpeed = 0;
            g_PlayerIsInTank = 1;
            g_EnterTankAudioState = TANK_RUN_STATE_NOT_RUNNING;
            g_CurrentPlayer->speedsideways = 0;
            g_CurrentPlayer->speedforwards = 0;
            g_CurrentPlayer->speedtheta = 0;
            g_CurrentPlayer->crouchpos = CROUCH_HALF;
            g_TankEnteringSitHeight = 0;
            g_TankEnteringSitHeightRemain = 1.0f;
            g_TankEnterBondHorizAngleDeg = g_CurrentPlayer->vv_theta;
            g_TankEnterBondVertAngleDeg = g_CurrentPlayer->vv_verta;
            g_EnterTankCoord.f[0] = g_CurrentPlayer->field_488.collision_position.f[0];
            g_EnterTankCoord.f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
            g_EnterTankCoord.f[2] = g_CurrentPlayer->field_488.collision_position.f[2];
            g_TankDamagePenaltyTicks = 0;

            bondviewTankModelRotationRelated();
        }
        else
        {
            g_CurrentPlayer->field_D0 = 1;
        }
    }

    if (moveData.invertPitch == 0)
    {
        f32 ftemp_nostack_spE8;

        moveData.controlStickYRaw = (s32) -stick_y;
        moveData.analogPitch = -moveData.analogPitch;
        ftemp_nostack_spE8 = moveData.speedVertaDown;
        moveData.speedVertaDown = moveData.speedVertaUp;
        moveData.speedVertaUp = ftemp_nostack_spE8;
    }

    if (bondviewGetIfCurrentPlayerDamageShowTime() && getPlayerCount() == 1)
    {
        moveData.triggerOn = 0;
    }

    gunTickGameplay(moveData.triggerOn);

    if (bondviewGetVisibleToGuardsFlag()
        && (get_hands_firing_status(GUNRIGHT)
            || get_hands_firing_status(GUNLEFT)))
    {
        noiseRadius = 0;

        if (get_hands_firing_status(GUNRIGHT) && getCurrentPlayerNoise(GUNRIGHT) > noiseRadius)
        {
            noiseRadius = getCurrentPlayerNoise(GUNRIGHT);
        }

        if (get_hands_firing_status(GUNLEFT) && noiseRadius < getCurrentPlayerNoise(GUNLEFT))
        {
            noiseRadius = getCurrentPlayerNoise(GUNLEFT);
        }

        chrCheckGuardsHeardSound(noiseRadius);
    }

    gunSetSightVisible(GUNSIGHTREASON_NOTAIMING, moveData.aiming);

    if (moveData.zoomOutFovPersec > 0)
    {
        camera_sniper_zoom_out(moveData.zoomOutFovPersec);
    }

    if (moveData.zoomInFovPersec > 0)
    {
        camera_sniper_zoom_in(moveData.zoomInFovPersec);
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        ftemp_nostack_spE0 = 60.0f;

        if (moveData.zooming)
        {
            ftemp_nostack_spE0 = get_item_in_hand_zoom();

            if (ftemp_nostack_spE0 <= 0)
            {
                ftemp_nostack_spE0 = 60.0f;
            }
        }

        bondviewTriggerWatchZoom(ftemp_nostack_spE0);
        bondviewUpdateWatchZoomIn();
    }

    if (g_PlayerIsInTank == 1)
    {
        g_TankTurretTurn = 0;

        if (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING)
        {
            if (moveData.tankTurnRightSpeed > 0)
            {
                g_TankTurretTurn += g_GlobalTimerDelta * moveData.tankTurnRightSpeed * DegToRad1Fact(1);
            }
            else if (moveData.tankTurnLeftSpeed > 0)
            {
                g_TankTurretTurn -= g_GlobalTimerDelta * moveData.tankTurnLeftSpeed * DegToRad1Fact(1);
            }
            else if (moveData.canTurnTank)
            {
                targetSpeed = (f32) moveData.analogStrafe / 70.0f;

                if (targetSpeed > 1.0f)
                {
                    targetSpeed = 1.0f;
                }

                if (targetSpeed < -1.0f)
                {
                    targetSpeed = -1.0f;
                }

                g_TankTurretTurn += DegToRad1Fact(1) * targetSpeed * g_GlobalTimerDelta;
            }

            if (!g_CurrentPlayer->insightaimmode)
            {
                ftemp_nostack_spD8 = 0;
                targetSpeed = 1.0f;
                ftemp_nostack_spE8 = 1.0f;

                if (moveData.canLookAhead)
                {
                    ftemp_nostack_spD8 = (f32) moveData.analogWalk / 70.0f;
                }
                else if (moveData.digitalStepForward)
                {
                    ftemp_nostack_spD8 = 1.0f;
                }
                else if (moveData.digitalStepBack)
                {
                    ftemp_nostack_spD8 = -1.0f;
                }

                if (ftemp_nostack_spD8 > 1.0f)
                {
                    ftemp_nostack_spD8 = 1.0f;
                }
                else if (ftemp_nostack_spD8 < -1.0f)
                {
                    ftemp_nostack_spD8 = -1.0f;
                }

                unadjustedTargetSpeed = ftemp_nostack_spD8 * TANK_MAX_SPEED;
                targetSpeed = unadjustedTargetSpeed;

                if (g_TankDamagePenaltyTicks > 0)
                {
                    targetSpeed = unadjustedTargetSpeed * 0.5f;
                    ftemp_nostack_spE8 = 4.0f;
                    g_TankDamagePenaltyTicks -= g_ClockTimer;
                }

                if (targetSpeed != g_CurrentPlayer->speedforwards)
                {
                    if (g_CurrentPlayer->speedforwards < targetSpeed)
                    {
                        unadjustedTargetSpeed = ((((((targetSpeed - g_CurrentPlayer->speedforwards) / 4.0f) / TANK_MAX_SPEED) + 0.5f) * ftemp_nostack_spE8 * FLOAT_TEN_A) / 60.0f);

                        g_CurrentPlayer->speedforwards += (unadjustedTargetSpeed) * g_GlobalTimerDelta;

                        if (targetSpeed < g_CurrentPlayer->speedforwards)
                        {
                            g_CurrentPlayer->speedforwards = targetSpeed;
                        }
                    }
                    else if (targetSpeed < g_CurrentPlayer->speedforwards)
                    {
                        unadjustedTargetSpeed = ((((((g_CurrentPlayer->speedforwards - targetSpeed) / 4.0f) / TANK_MAX_SPEED) + 0.5f) * ftemp_nostack_spE8 * -FLOAT_TEN_A) / 60.0f);

                        g_CurrentPlayer->speedforwards += (unadjustedTargetSpeed) * g_GlobalTimerDelta;

                        if (g_CurrentPlayer->speedforwards < targetSpeed)
                        {
                            g_CurrentPlayer->speedforwards = targetSpeed;
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (moveData.digitalStepLeft)
        {
            bondviewUpdateSpeedSideways(-1);
        }
        else if (moveData.digitalStepRight)
        {
            bondviewUpdateSpeedSideways(1);
        }
        else
        {
            bondviewUpdateSpeedSideways(0);
        }

        if (moveData.canTurnTank) // ?? not sure why this tank property is used here. Is the name wrong?
        {
            g_CurrentPlayer->speedsideways = (f32) moveData.analogStrafe / 70.0f;
        }

        if (moveData.digitalStepForward)
        {
            bondviewUpdateSpeedForwards(1);
            g_CurrentPlayer->speedmaxtime60 += g_ClockTimer;
        }
        else if (moveData.digitalStepBack)
        {
            bondviewUpdateSpeedForwards(-1);
        }
        else
        {
            bondviewUpdateSpeedForwards(0);
        }

        if (moveData.canLookAhead)
        {
            g_CurrentPlayer->speedforwards = (f32) moveData.analogWalk / 70.0f;

            if (moveData.analogWalk > 60)
            {
                g_CurrentPlayer->speedmaxtime60 += g_ClockTimer;
            }
            else
            {
                g_CurrentPlayer->speedmaxtime60 = 0;
            }
        }

        if (g_CurrentPlayer->speedforwards > 1.0f)
        {
            g_CurrentPlayer->speedforwards = 1;
        }

        if (g_CurrentPlayer->speedforwards < -1.0f)
        {
            g_CurrentPlayer->speedforwards = -1.0f;
        }

        if (g_CurrentPlayer->speedsideways > 1)
        {
            g_CurrentPlayer->speedsideways = 1;
        }

        if (g_CurrentPlayer->speedsideways < -1)
        {
            g_CurrentPlayer->speedsideways = -1;
        }

        g_CurrentPlayer->speedforwards *= 1.08f;
        g_CurrentPlayer->speedforwards *= g_CurrentPlayer->speedboost;

        if ((moveData.canLookAhead == 0) && (moveData.digitalStepForward == 0))
        {
            g_CurrentPlayer->speedmaxtime60 = 0;
        }

        if (moveData.rLeanLeft)
        {
            currentPlayerSetSwayTarget(-1);
        }
        else if (moveData.rLeanRight)
        {
            currentPlayerSetSwayTarget(1);
        }
        else
        {
            currentPlayerSetSwayTarget(0);
        }

        if (moveData.crouchDown)
        {
            currentPlayerAdjustCrouchPos(-2);
        }
        else if (moveData.crouchUp)
        {
            currentPlayerAdjustCrouchPos(2);
        }
    }

    if (g_CurrentPlayer->speedmaxtime60 >= THREE_SECOND_TICKS)
    {
        if (g_CurrentPlayer->speedboost < SPEED_RUN_MAX)
        {
            g_CurrentPlayer->speedboost += (SPEED_TICK_ADJUST * g_GlobalTimerDelta);
        }

        if (g_CurrentPlayer->speedboost > SPEED_RUN_MAX)
        {
            g_CurrentPlayer->speedboost = SPEED_RUN_MAX;
        }
    }
    else
    {
        if (g_CurrentPlayer->speedboost > SPEED_REGULAR_MAX)
        {
            g_CurrentPlayer->speedboost -= (SPEED_TICK_ADJUST * g_GlobalTimerDelta);
        }

        if (g_CurrentPlayer->speedboost < SPEED_REGULAR_MAX)
        {
            g_CurrentPlayer->speedboost = SPEED_REGULAR_MAX;
        }
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        // By default the camera is pitched slightly below the horizon
        targetPitch = -4.0f;

        // lookaheadcentreenabled is always true, so this block always executes.
        if (g_CurrentPlayer->lookaheadcentreenabled)
        {
            spC0 = g_CurrentPlayer->field_488.current_tile_ptr;
            spBC = 300.0f;

            // prop, f32 *collision_radius, f32 *height, f32 *always_30
            bondviewGetCollisionRadius(g_CurrentPlayer->prop, &spA0.f[0], &spA0.f[2], &spA0.f[1]);

            spAC.f[0] = g_CurrentPlayer->field_488.collision_position.f[0] + (g_CurrentPlayer->field_488.theta_transform.f[0] * 300.0f);
            spAC.f[1] = g_CurrentPlayer->field_488.collision_position.f[1];
            spAC.f[2] = g_CurrentPlayer->field_488.collision_position.f[2] + (g_CurrentPlayer->field_488.theta_transform.f[2] * 300.0f);

            stanResetHits();

            if (stanTestLineUnobstructed(&spC0, g_CurrentPlayer->field_488.collision_position.f[0], g_CurrentPlayer->field_488.collision_position.f[2], spAC.f[0], spAC.f[2], CDTYPE_CLOSEDDOORS, spA0.f[2], spA0.f[1], 0, 1.0f))
            {
                spAC.f[1] = bondviewYPositionRelated(spC0, spAC.f[0], spAC.f[2]);
            }
            else
            {
                chrlvStanPointPointIntersection(&g_CurrentPlayer->field_488.collision_position, &g_CurrentPlayer->field_488.theta_transform, (struct coord3d *) &spAC);
                ftemp_nostack_spD0 = spAC.f[0] - g_CurrentPlayer->field_488.collision_position.f[0];
                ftemp_nostack_spCC = spAC.f[2] - g_CurrentPlayer->field_488.collision_position.f[2];
                spBC = sqrtf((ftemp_nostack_spD0 * ftemp_nostack_spD0) + (ftemp_nostack_spCC * ftemp_nostack_spCC));
                spAC.f[1] = bondviewYPositionRelated(spC0, spAC.f[0], spAC.f[2]);
            }

            if (spBC > 0)
            {
                ftemp_nostack_spC8 = spAC.f[1] - g_CurrentPlayer->stanHeight;

                if ((ftemp_nostack_spC8 > -300.0f) && (ftemp_nostack_spC8 < 500.0f))
                {
                    targetPitch = ((atan2f(ftemp_nostack_spC8, spBC) * 360.0f) / M_TAU_F) + -4.0f;

                    if (targetPitch >= 180.0f)
                    {
                        targetPitch -= 360.0f;
                    }

                    if (targetPitch > 0)
                    {
                        targetPitch *= 0.8666667f;
                    }
                }
            }
        }

        if ((g_CurrentPlayer->movecentrerelease) && (moveData.analogWalk < 40) && (moveData.analogWalk > -40))
        {
            g_CurrentPlayer->movecentrerelease = FALSE;
        }


        if (g_PlayerIsInTank == 0)
        {
            /**
             * If the player is giving manual pitch inputs, stop the automatic look ahead pitch adjust.
             */
            if ((moveData.speedVertaDown > 0) || (moveData.speedVertaUp > 0))
            {
                g_CurrentPlayer->docentreupdown = FALSE;
                g_CurrentPlayer->prevupdown = TRUE;
                g_CurrentPlayer->automovecentre = FALSE;
            }
            else
            {
                if (moveData.disableLookAhead)
                {
                    g_CurrentPlayer->automovecentre = FALSE;
                }
                else if (g_CurrentPlayer->automovecentreenabled)
                {
                    if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < -60)))
                    {
                        g_CurrentPlayer->automovecentre = TRUE;
                    }

                    /**
                     * If the player's camera pitch is 5 degrees above the target pitch or 10 degrees below the target pitch,
                     * and move centre is allowed, look ahead (docentreupdown) can be activated.
                     */
                    if ((g_CurrentPlayer->automovecentre)
                        && (( ((targetPitch + 5.0f) < g_CurrentPlayer->vv_verta)) || (g_CurrentPlayer->vv_verta < (targetPitch + -FLOAT_TEN_A)))
                        && (g_CurrentPlayer->movecentrerelease == FALSE))
                    {
                        g_CurrentPlayer->docentreupdown = TRUE;
                    }
                }
                /**
                 * fastmovecentreenabled is never set to true, so this block can never execute. Cut option?
                 */
                else if ((g_CurrentPlayer->fastmovecentreenabled)
                    && (moveData.canLookAhead)
                    && ((moveData.analogWalk > 60) || (moveData.analogWalk < -60))
                    && (( ((targetPitch + 5.0f) < g_CurrentPlayer->vv_verta)) || (g_CurrentPlayer->vv_verta < (targetPitch + -FLOAT_TEN_A)))
                    && (g_CurrentPlayer->movecentrerelease == FALSE))
                {
                    g_CurrentPlayer->docentreupdown = TRUE;
                }

                g_CurrentPlayer->prevupdown = FALSE;
            }
        }

        /**
         * If look ahead (docentreupdown) is active, adjust the player's camera pitch to the target pitch.
         */
        if (g_CurrentPlayer->docentreupdown)
        {
            if (g_PlayerIsInTank == 0)
            {
                ftemp_nostack_spB8 = (g_CurrentPlayer->speedverta * g_CurrentPlayer->speedverta * 0.5f) / 0.05f;

                if ((targetPitch + ftemp_nostack_spB8) < g_CurrentPlayer->vv_verta)
                {
                    bondviewCurrentPlayerUpdateSpeedVerta(1.0f);
                }
                else if (g_CurrentPlayer->vv_verta < (targetPitch - ftemp_nostack_spB8))
                {
                    bondviewCurrentPlayerUpdateSpeedVerta(-1.0f);
                }
                else
                {
                    bondviewCurrentPlayerUpdateSpeedVerta(0);
                }

                ftemp_nostack_spB8 = g_CurrentPlayer->vv_verta + (2.0f * (g_CurrentPlayer->speedverta * g_GlobalTimerDelta));

                if ((targetPitch < g_CurrentPlayer->vv_verta) && (targetPitch < ftemp_nostack_spB8))
                {
                    g_CurrentPlayer->vv_verta = ftemp_nostack_spB8;
                }
                else if ((g_CurrentPlayer->vv_verta < targetPitch) && (ftemp_nostack_spB8 < targetPitch))
                {
                    g_CurrentPlayer->vv_verta = ftemp_nostack_spB8;
                }
                else
                {
                    g_CurrentPlayer->vv_verta = targetPitch;
                    g_CurrentPlayer->speedverta = 0;

                    if (g_CurrentPlayer->prevupdown == FALSE)
                    {
                        g_CurrentPlayer->docentreupdown = FALSE;
                    }
                }
            }
        }
        else
        {
            if (moveData.canNaturalPitch)
            {
                ftemp_nostack_sp98 = viGetFovY() / 60.0f;
                ftemp_nostack_sp94 = (f32) moveData.analogPitch / 70.0f;

                if (ftemp_nostack_sp94 > 1)
                {
                    ftemp_nostack_sp94 = 1;
                }
                else if (ftemp_nostack_sp94 < -1)
                {
                    ftemp_nostack_sp94 = -1;
                }

                if (ftemp_nostack_sp94 >= 0)
                {
                    ftemp_nostack_sp94 *= ftemp_nostack_sp94;
                }
                else
                {
                    ftemp_nostack_sp94 *= -ftemp_nostack_sp94;
                }

                g_CurrentPlayer->speedverta = -ftemp_nostack_sp94 * ftemp_nostack_sp98;
            }
            else if (moveData.speedVertaDown > 0)
            {
                bondviewCurrentPlayerUpdateSpeedVerta(moveData.speedVertaDown);

                // Bug? This is true for every value except exactly 60.
                if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < 60)))
                {
                    g_CurrentPlayer->movecentrerelease = TRUE;
                }
            }
            else if (moveData.speedVertaUp > 0)
            {
                bondviewCurrentPlayerUpdateSpeedVerta(-moveData.speedVertaUp);

                if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < 60)))
                {
                    g_CurrentPlayer->movecentrerelease = TRUE;
                }
            }
            else
            {
                bondviewCurrentPlayerUpdateSpeedVerta(0);
            }

            g_CurrentPlayer->vv_verta += g_CurrentPlayer->speedverta * g_GlobalTimerDelta * 3.5f;

            if ((g_PlayerIsInTank == 1) && (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING) && (g_CurrentPlayer->vv_verta < -20.0f))
            {
                g_CurrentPlayer->vv_verta = -20.0f;
            }
        }
    }

    if (moveData.canNaturalTurn)
    {
        ftemp_nostack_sp8C = viGetFovY();
        ftemp_nostack_sp84 = (f32) moveData.analogTurn / 70.0f;

        if (ftemp_nostack_sp84 > 1)
        {
            ftemp_nostack_sp84 = 1;
        }
        else if (ftemp_nostack_sp84 < -1)
        {
            ftemp_nostack_sp84 = -1;
        }

        if (ftemp_nostack_sp84 >= 0)
        {
            ftemp_nostack_sp84 *= ftemp_nostack_sp84;
        }
        else
        {
            ftemp_nostack_sp84 *= -ftemp_nostack_sp84;
        }

        ftemp_nostack_sp88 = (ftemp_nostack_sp8C / FOV_Y_F);
        g_CurrentPlayer->speedtheta = ftemp_nostack_sp84 * ftemp_nostack_sp88;
    }
    else if (moveData.aimTurnLeftSpeed > 0)
    {
        bondviewCurrentPlayerUpdateSpeedTheta(moveData.aimTurnLeftSpeed);
    }
    else if (moveData.aimTurnRightSpeed > 0)
    {
        bondviewCurrentPlayerUpdateSpeedTheta(-moveData.aimTurnRightSpeed);
    }
    else
    {
        bondviewCurrentPlayerUpdateSpeedTheta(0);
    }

    if (g_PlayerIsInTank == 1)
    {
        if (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING)
        {
            ftemp_nostack_sp80 = 0;

            if (moveData.canNaturalTurn)
            {
                ftemp_nostack_sp80 = g_CurrentPlayer->speedtheta * 0.3f;
            }
            else if (moveData.aimTurnLeftSpeed > 0)
            {
                ftemp_nostack_sp80 = sub_GAME_7F080228(1) * 0.3f;
            }
            else if (moveData.aimTurnRightSpeed > 0)
            {
                ftemp_nostack_sp80 = sub_GAME_7F080228(-1) * 0.3f;
            }

            for (i_1=0; i_1<g_ClockTimer; i_1++)
            {
                g_TankTurnSpeed = (TANKUPDATEROTATION_SCALE * g_TankTurnSpeed) + ftemp_nostack_sp80;
            }

            g_CurrentPlayer->speedtheta = g_TankTurnSpeed * TANK_VERT_ANGLE_RAD_FACTOR;
        }
        else
        {
            g_CurrentPlayer->speedtheta = 0;
        }
    }

    if (moveData.detonating)
    {
        g_CurrentPlayer->hands[GUNRIGHT].when_detonating_mines_is_0 = 0;
        g_CurrentPlayer->hands[GUNRIGHT].weapon_current_animation = 0;
        trigger_remote_mine_detonation();
    }

    canCycleWeapons = 1;

    if ((getPlayerCount() >= 2) && (get_scenario() == 2) && (bondinvIsAliveWithFlag()))
    {
        canCycleWeapons = 0;
    }

    if (canCycleWeapons)
    {
        if (moveData.weaponBackOffset)
        {
            backstep_through_inventory();
        }

        if (moveData.weaponForwardOffset)
        {
            advance_through_inventory();
        }
    }

    if (moveData.canSwivelGun)
    {
        g_CurrentPlayer->controldef = CONTROLLER_CONFIG_HONEY;
    }
    else if (moveData.canManualAim)
    {
        g_CurrentPlayer->controldef = CONTROLLER_CONFIG_KISSY;
    }

    if (g_CurrentPlayer->controldef == CONTROLLER_CONFIG_HONEY)
    {
        gunSetAimType(0);

        if (moveData.canAutoAim
            && currentPlayerGetXAutoAimEnabledRedirect()
            && g_CurrentPlayer->autoaim_target_x
            && bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_HAS_AUTO_AIM))
        {
            sp70 = g_CurrentPlayer->autoaimx;
        }
        else
        {
            sp70 = g_CurrentPlayer->speedtheta * 0.3f;
        }

        if (moveData.canAutoAim
            && currentPlayerGetYAutoAimEnabledRedirect()
            && g_CurrentPlayer->autoaim_target_y
            && bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT), WEAPONSTATBITFLAG_HAS_AUTO_AIM))
        {
            ftemp_nostack_sp78 = g_CurrentPlayer->autoaimy;
        }
        else
        {
            ftemp_nostack_sp78 = -g_CurrentPlayer->speedverta * 0.1f;
        }

        sub_GAME_7F067F58(sp70, ftemp_nostack_sp78, MAX_AIMLOCK_SPEED_DEFAULT);
    }
    else if (g_CurrentPlayer->controldef == CONTROLLER_CONFIG_KISSY)
    {
        gunSetAimType(0);
        sub_GAME_7F067FBC(((f32) moveData.controlStickXRaw * 0.65f) / 80.0f, ((f32) moveData.controlStickYRaw * 0.65f) / 80.0f);
    }
}


/**
 * Perfect Dark playerTickDamageAndHealth
 *
 * NTSC address 7F083FC8.
 * NTSC-J address 7F0845D8.
 * EU address 7F08406C.
*/
void bondviewPlayerTickDamageAndHealth(void)
{
    // update damage showtime
    if (g_CurrentPlayer->damageshowtime >= 0)
    {
        // 0: This is the first frame of damage
        if (g_CurrentPlayer->damageshowtime == 0)
        {
            gunSetGunAmmoVisible(GUNAMMOREASON_DAMAGE, FALSE);
            gunSetSightVisible(GUNSIGHTREASON_DAMAGE, FALSE);
            hudmsgsSetOff(4);
            bondviewSetUpperTextDisplayFlag(PLAYERFLAG_NOTIMER);
            countdownTimerSetVisible(8, 0);

            g_CurrentPlayer->damagetype = (s32)(currentPlayerGetHealth() * 8.0f);

            if (g_CurrentPlayer->damagetype >= 8)
            {
                g_CurrentPlayer->damagetype = 7;
            }

// Ensure we don't read out of bounds of the g_DamageTypes array.
#if defined(VERSION_EU) || defined(VERSION_JP)
            if (g_CurrentPlayer->damagetype < 0)
            {
                g_CurrentPlayer->damagetype = 0;
            }
#endif
        }

#ifdef VERSION_US
        if (
            (g_DamageTypes[g_CurrentPlayer->damagetype].field_0x8 >= g_CurrentPlayer->damageshowtime)
            || (g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame >= g_CurrentPlayer->damageshowtime))
        {
            if (!g_CurrentPlayer->bonddead)
            {
#else
        if (!g_CurrentPlayer->bonddead
            && (
                (g_DamageTypes[g_CurrentPlayer->damagetype].field_0x8 >= g_CurrentPlayer->damageshowtime)
                || (g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame >= g_CurrentPlayer->damageshowtime)))
        {
#endif
            if (g_CurrentPlayer->damageshowtime >= g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame
                && g_CurrentPlayer->damageshowtime <= g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame)
            {
                f32 frac;
#ifdef VERSION_US
                s32 flashdoneframes;
                s32 totalframes;
                s32 flashfullframe;
#else
                f32 flashdoneframes;
                f32 totalframes;
                f32 flashfullframe;
#endif

                flashdoneframes = g_CurrentPlayer->damageshowtime - g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame;
                flashfullframe = g_DamageTypes[g_CurrentPlayer->damagetype].flashFullFrame;
                totalframes = g_DamageTypes[g_CurrentPlayer->damagetype].flashEndFrame - g_DamageTypes[g_CurrentPlayer->damagetype].flashStartFrame;

                if (flashdoneframes < flashfullframe)
                {
                    frac = (g_DamageTypes[g_CurrentPlayer->damagetype].maxAlpha * (f32)flashdoneframes) / (f32)flashfullframe;
                }
                else
                {
                    frac = (g_DamageTypes[g_CurrentPlayer->damagetype].maxAlpha * (f32)(totalframes - flashdoneframes)) / (f32)(totalframes - flashfullframe);
                }

                currentPlayerSetFadeColour(
                    g_DamageTypes[g_CurrentPlayer->damagetype].red,
                    g_DamageTypes[g_CurrentPlayer->damagetype].green,
                    g_DamageTypes[g_CurrentPlayer->damagetype].blue,
                    frac);
            }
#ifdef VERSION_US
            }
#endif

            if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
            {
#if defined(VERSION_US)
                g_CurrentPlayer->damageshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->damageshowtime += g_GlobalTimerDelta;
#endif
            }
            else
            {
#if defined(VERSION_US)
                g_CurrentPlayer->damageshowtime += speedgraphframes;
#else
                g_CurrentPlayer->damageshowtime += jpD_800484D0;
#endif
            }
        }
        else /* (damage showtime is over) */
        {
            g_CurrentPlayer->damageshowtime = -1;
            currentPlayerSetFadeColour(0xFF, 0xFF, 0xFF, 0);

            if (!g_CurrentPlayer->bonddead)
            {
                gunSetGunAmmoVisible(GUNAMMOREASON_DAMAGE, TRUE);
                gunSetSightVisible(GUNSIGHTREASON_DAMAGE, TRUE);
                hudmsgsSetOn(4);
                bondviewClearUpperTextDisplayFlag(PLAYERFLAG_NOTIMER);
                countdownTimerSetVisible(8, 1);
            }
        }
    }

    // update health showtime
    if (g_CurrentPlayer->healthshowtime >= 0)
    {
        // 0: This is the first frame of damage
        if (g_CurrentPlayer->healthshowtime == 0)
        {
            g_CurrentPlayer->healthdamagetype = (s32)(currentPlayerGetHealth() * 8.0f);

            if (g_CurrentPlayer->healthdamagetype >= 8)
            {
                g_CurrentPlayer->healthdamagetype = 7;
            }

#if defined(VERSION_EU) || defined(VERSION_JP)
            if (g_CurrentPlayer->healthdamagetype < 0)
            {
                g_CurrentPlayer->healthdamagetype = 0;
            }
#endif
        }

        if (!g_CurrentPlayer->bonddead)
        {
            if ((g_CurrentPlayer->healthshowtime >= g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].validStartFrame)
                && (g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].updateToRealHealthFrame >= g_CurrentPlayer->healthshowtime))
            {
                g_CurrentPlayer->apparenthealth = g_CurrentPlayer->oldhealth;
                g_CurrentPlayer->apparentarmour = g_CurrentPlayer->oldarmour;
#if defined(VERSION_US)
                g_CurrentPlayer->healthshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->healthshowtime += g_GlobalTimerDelta;
#endif
            }
            else if ((g_CurrentPlayer->healthshowtime >= g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].validStartFrame)
                && (g_HealthDisplayDurations[g_CurrentPlayer->healthdamagetype].hideHealthFrame >= g_CurrentPlayer->healthshowtime))
            {
                g_CurrentPlayer->apparenthealth = g_CurrentPlayer->bondhealth;
                g_CurrentPlayer->apparentarmour = g_CurrentPlayer->bondarmour;
#if defined(VERSION_US)
                g_CurrentPlayer->healthshowtime += g_ClockTimer;
#else
                g_CurrentPlayer->healthshowtime += g_GlobalTimerDelta;
#endif
            }
            else
            {
                g_CurrentPlayer->healthshowtime = -1;
            }
        }
        else
        {
            g_CurrentPlayer->healthshowtime = -1;
        }
    }
}

/**
 * If global flag g_SurroundBondWithExplosionsFlag is set then explosions
 * will be randomly created around Bond.
 * Perfect Dark method playerTickExplode.
 * NTSC address 7F084360.
 * EU address 7F0844A4.
*/
void bondviewPlayerTickExplode(void)
{
    g_PlayerTickExplodeCreatePosition++;

    if (g_SurroundBondWithExplosionsFlag
        && (g_PlayerInvincible == FALSE)
        && g_SurroundBondWithExplosionsTicks < g_GlobalTimer)
    {
        struct coord3d pos;

        pos.f[0] = g_CurrentPlayer->prop->pos.f[0];
        pos.f[1] = g_CurrentPlayer->prop->pos.f[1];
        pos.f[2] = g_CurrentPlayer->prop->pos.f[2];

        switch (g_PlayerTickExplodeCreatePosition % 4)
        {
            case 0: pos.x += 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 1: pos.x -= 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 2: pos.z += 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
    		case 3: pos.z -= 250.0f + 150.0f * RANDOMGETNEXT_F32(); break;
        }

        pos.y += 200.0f * RANDOMGETNEXT_F32() - 100.0f;

        explosionCreate(0, &pos, g_CurrentPlayer->prop->stan, EXPLOSION_DEF_PLAYER, 0, 0, g_CurrentPlayer->prop->rooms, 0);

        g_SurroundBondWithExplosionsTicks = (randomGetNext() % (u32)PLAYER_TICKEXPLODE_FACTOR) + g_GlobalTimer + PLAYER_TICKEXPLODE_FACTOR;
    }
}


/**
 * NTSC Address 0x7F084648.
 * NTSC-J address 0x7F084CF8.
 *
 * Met by Saint Jon The Archangel in the writhing pits of hell, the beast was
 * pinned, prone on the floor. He had taken many forms throughout the ages.
 * From the creator of man to the far reaches of the universe, his perpetual
 * reincarnations reigned throughout space and time... until now. His form was
 * weak, the opposition strong. The time for atonement had finally come.
 *
 * Battered, torn, exposed, the beast slacked his gaping maw.
 *
 * "I'm sorry, Jon" he whispered.
 *
 * The archangel raised his flaming blade.
 *
 * "You are forgiven, Garfield"
 *
 * And the beast exhaled his last.
 *
 * 10 months of off and on work to match.
 * Thanks Trevor.
 * - Bethany Burns
 */
void MoveBond(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    struct coord3d move_offset;
    f32 ftemp;
    f32 stack_padding_9;
    f32 sp3A0;
    s32 i;
    f32 maxspeed;
    s32 use_stanHeight;
    f32 sp390;

    move_offset = g_DefaultMoveBondOffset;

    use_stanHeight = 0;
    maxspeed = 0.0f;
    sp390 = 0.0f;

    #if defined(VERSION_US) || defined(VERSION_JP)
    if (stick_x >= 100 || stick_x <= -100) return_null(); // __LINE__ __FILE__ (#6414 bondview.c) "joystick x has value %d!\n"
    if (stick_y >= 100 || stick_y <= -100) return_null(); // __LINE__ __FILE__ (#6415 bondview.c) "joystick y has value %d!\n"
    #endif

    if (g_bondviewForceDisarm > 0)
    {
        g_bondviewForceDisarm++;
        if (g_bondviewForceDisarm >= 4)
        {
            g_bondviewForceDisarm = 0;
            g_CurrentPlayer->lock_hand_model[GUNLEFT] = 0;
            g_CurrentPlayer->lock_hand_model[GUNRIGHT] = 0;
            currentPlayerUnEquipWeaponWrapper(GUNLEFT, getCurrentPlayerWeaponId(GUNLEFT));
            currentPlayerUnEquipWeaponWrapper(GUNRIGHT, getCurrentPlayerWeaponId(GUNRIGHT));
        }
    }

    currentPlayerSetCameraMode(0);
    bondviewPlayerTickDamageAndHealth();
    bondviewPlayerTickExplode();
    bondviewProcessInput(stick_x, stick_y, buttons, oldbuttons);

    if (lvlGetControlsLockedFlag())
    {
        bondviewPlayerStopAudioForPause();
    }

    if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
    {
        bondviewWatchAnimationTick();
    }

    /*
        Apply bondshotspeed vector to speedforwards scalar and speedsideways scalar.
        Crouching applies a 50% base speed reduction before applying boost.
        Bond can't be boosted while in the tank.
    */
    if (g_PlayerIsInTank == 0)
    {
        // This `if` block is Perfect Dark bwalkApplyCrouchSpeed.
        if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            g_CurrentPlayer->speedforwards *= 0.5f;
            g_CurrentPlayer->speedsideways *= 0.5f;
        }

        if ((g_CurrentPlayer->bondshotspeed.f[0] != 0.0f) || (g_CurrentPlayer->bondshotspeed.f[2] != 0.0f))
        {
            // boost forwards
            f32 shotboost_forward; // sp38C
            // boost sideways
            f32 shotboost_sideways; // sp388
            f32 shotboost_norm; // sp384

            // Assigning these two variables is done in Perfect Dark bmove0f0cba88.
            shotboost_forward =
                (-g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->vv_sintheta)
                + (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->vv_costheta);
            shotboost_sideways =
                (-g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->vv_costheta)
                - (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->vv_sintheta);

            shotboost_norm = sqrtf(
                (g_CurrentPlayer->bondshotspeed.f[0] * g_CurrentPlayer->bondshotspeed.f[0]) +
                (g_CurrentPlayer->bondshotspeed.f[2] * g_CurrentPlayer->bondshotspeed.f[2]));

            g_CurrentPlayer->speedforwards += shotboost_forward;
            g_CurrentPlayer->speedsideways += shotboost_sideways;

            // 3: x,y,z components of bondshotspeed
            for (i=0; i<3; i++)
            {
                if (g_CurrentPlayer->bondshotspeed.f[i] != 0.0f)
                {
                    if (g_CurrentPlayer->bondshotspeed.f[i] > 0.0f)
                    {
                        g_CurrentPlayer->bondshotspeed.f[i] -=
                            ((0.06666667f * g_GlobalTimerDelta * g_CurrentPlayer->bondshotspeed.f[i]) / shotboost_norm);

                        if (g_CurrentPlayer->bondshotspeed.f[i] < 0.0f)
                        {
                            g_CurrentPlayer->bondshotspeed.f[i] = 0.0f;
                        }
                    }
                    else if (g_CurrentPlayer->bondshotspeed.f[i] < 0.0f)
                    {
                        g_CurrentPlayer->bondshotspeed.f[i] -=
                            ((0.06666667f * g_GlobalTimerDelta * g_CurrentPlayer->bondshotspeed.f[i]) / shotboost_norm);

                        if (g_CurrentPlayer->bondshotspeed.f[i] > 0.0f)
                        {
                            g_CurrentPlayer->bondshotspeed.f[i] = 0.0f;
                        }
                    }
                }
            }
        }
    }

    /**
     * This section updates the tank turret horizontal position (turning left and right),
     * as well as turning the tank left and right.
    */
    if (g_PlayerIsInTank == 1)
    {
        f32 ftemp2;
        struct coord3d check_collision_p1;
        struct coord3d check_collision_p2;
        f32 stack_padding_1;
        s32 i_1;
        f32 curTankAngleRad;
        f32 tankChangeInAngle;
        f32 sp354;
        struct coord3d tank_collision_pt1;
        struct coord3d tank_collision_pt2;
        f32 tank_collision_dx;
        f32 tank_collision_dz;

        tankChangeInAngle = DegToRad1Fact(g_CurrentPlayer->speedtheta * g_GlobalTimerDelta) * 3.5f;
        curTankAngleRad = g_TankOrientationAngle + tankChangeInAngle;

        if (curTankAngleRad >= M_TAU_F)
        {
            curTankAngleRad -= M_TAU_F;
        }

        if (curTankAngleRad < 0.0f)
        {
            curTankAngleRad += M_TAU_F;
        }

        if (bondviewTankCollisionStatus(
            &g_CurrentPlayer->field_488.collision_position,
            g_CurrentPlayer->field_488.current_tile_ptr,
            curTankAngleRad,
            &check_collision_p1,
            &check_collision_p2))
        {
            g_TankOrientationAngle = curTankAngleRad;
        }
        else
        {
            f32 sp1E4; //x
            s32 stack_padding_1; //x
            f32 sp1FC; //x
            f32 sp324;
            f32 sp320;
            f32 sp31C;
            f32 sp20C; //x
            f32 tank_collision_norm; //x
            f32 sp210; //x
            f32 sp30C;
            f32 sp308;
            f32 sp304;

            sp31C = -1; //sp31C: scope within this block, used throughout
            sp304 = -1; //sp304: scope within this block, used throughout

            getCollisionEdge_maybe(&tank_collision_pt1, &tank_collision_pt2);

            tank_collision_dx = (tank_collision_pt2.f[0] - tank_collision_pt1.f[0]);
            tank_collision_dz = (tank_collision_pt2.f[2] - tank_collision_pt1.f[2]);

            tank_collision_norm = 1.0f / sqrtf((tank_collision_dx * tank_collision_dx) + (tank_collision_dz * tank_collision_dz));

            // sp320: scoped within this block, used throughout
            // sp324: scoped within this block, used throughout
            tank_collision_dx *= tank_collision_norm;
            tank_collision_dz *= tank_collision_norm;
            sp324 = tank_collision_dz;
            sp320 = -tank_collision_dx;

            // sp210: short lived variable
            sp210 =
                ((g_CurrentPlayer->field_488.collision_position.f[0] - check_collision_p2.f[0]) * sp324) +
                ((g_CurrentPlayer->field_488.collision_position.f[2] - check_collision_p2.f[2]) * sp320);

            if (sp210 < 0.0f)
            {
                sp210 = -sp210;
                sp324 = -sp324;
                sp320 = -sp320;
            }

            // sp20C: very short lived variable
            sp20C =
                ((g_CurrentPlayer->field_488.collision_position.f[0] - tank_collision_pt1.f[0]) * sp324) +
                ((g_CurrentPlayer->field_488.collision_position.f[2] - tank_collision_pt1.f[2]) * sp320);

            if (sp20C < sp210)
            {
                sp31C = sp210 - sp20C;
            }

            tank_collision_dx = check_collision_p2.f[0] - check_collision_p1.f[0];
            tank_collision_dz = check_collision_p2.f[2] - check_collision_p1.f[2];

            tank_collision_norm = 1.0f / sqrtf((tank_collision_dx * tank_collision_dx) + (tank_collision_dz * tank_collision_dz));

            // sp308: scoped within this block, used throughout
            // sp30C: scoped within this block, used throughout
            tank_collision_dx *= tank_collision_norm;
            tank_collision_dz *= tank_collision_norm;
            sp30C = tank_collision_dz;
            sp308 = -tank_collision_dx;

            // sp1F8 -> sp210: short lived variable
            sp210 =
                ((g_CurrentPlayer->field_488.collision_position.f[0] - check_collision_p2.f[0]) * sp30C) +
                ((g_CurrentPlayer->field_488.collision_position.f[2] - check_collision_p2.f[2]) * sp308);

            if (sp210 < 0.0f)
            {
                sp210 = -sp210;
                sp30C = -sp30C;
                sp308 = -sp308;
            }

            // sp1FC: very short lived variable
            // sp1E4: very short lived variable
            sp1FC =
                ((g_CurrentPlayer->field_488.collision_position.f[0] - tank_collision_pt1.f[0]) * sp30C) +
                ((g_CurrentPlayer->field_488.collision_position.f[2] - tank_collision_pt1.f[2]) * sp308);
            sp1E4 =
                ((g_CurrentPlayer->field_488.collision_position.f[0] - tank_collision_pt2.f[0]) * sp30C) +
                ((g_CurrentPlayer->field_488.collision_position.f[2] - tank_collision_pt2.f[2]) * sp308);

            if (sp1E4 < sp1FC)
            {
                sp1FC = sp1E4;
            }

            if (sp1FC < sp210)
            {
                sp304 = sp210 - sp1FC;
            }

            if ((sp304 >= 0.0f) && ((sp304 < sp31C) || (sp31C < 0.0f)))
            {
                sp324 = sp30C;
                sp320 = sp308;
                sp31C = sp304;
            }

            if (sp31C >= 0.0f)
            {
                move_offset.f[0] = sp31C * sp324 * 1.01f;
                move_offset.f[2] = sp31C * sp320 * 1.01f;

                bondviewCalcUpdatePlayerCollision(&move_offset, 1);

                move_offset.f[0] = 0.0f;
                move_offset.f[2] = 0.0f;

                if (bondviewTankCollisionStatus(
                    &g_CurrentPlayer->field_488.collision_position,
                    g_CurrentPlayer->field_488.current_tile_ptr,
                    curTankAngleRad,
                    &check_collision_p1,
                    &check_collision_p2))
                {
                    g_TankOrientationAngle = curTankAngleRad;
                }
                else
                {
                    tankChangeInAngle = 0.0f;
                }
            }
            else
            {
                tankChangeInAngle = 0.0f;
            }
        }

        sp354 = g_TankTurretOrientationAngleRad;
        g_TankTurretAngle += g_TankTurretTurn;
        if (g_TankTurretAngle >= M_TAU_F)
        {
            g_TankTurretAngle -= M_TAU_F;
        }

        if (g_TankTurretAngle < 0.0f)
        {
            g_TankTurretAngle += M_TAU_F;
        }

        ftemp = (DegToRad1Fact(g_CurrentPlayer->speedtheta * 3.5f) * 4.0f) + g_TankTurretAngle;

        if (ftemp < 0.0f)
        {
            ftemp += M_TAU_F;
        }

        if (ftemp >= M_TAU_F)
        {
            ftemp -= M_TAU_F;
        }

        if ((ftemp - g_TankTurretOrientationAngleRad) >= M_PI_F)
        {
            ftemp -= M_TAU_F;
        }
        else if ((ftemp - g_TankTurretOrientationAngleRad) < -M_PI_F)
        {
            ftemp += M_TAU_F;
        }

        for (i_1=0; i_1<g_ClockTimer; i_1++)
        {
            g_TankTurretOrientationAngleDeg = ((TANKUPDATEROTATION_SCALE) * g_TankTurretOrientationAngleDeg) + ftemp;
        }

        g_TankTurretOrientationAngleRad = g_TankTurretOrientationAngleDeg * (1.0f - TANKUPDATEROTATION_SCALE);

        if (g_TankTurretOrientationAngleRad >= M_TAU_F)
        {
            g_TankTurretOrientationAngleRad -= M_TAU_F;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / (1.0f - TANKUPDATEROTATION_SCALE);
        }

        if (g_TankTurretOrientationAngleRad < 0.0f)
        {
            g_TankTurretOrientationAngleRad += M_TAU_F;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / (1.0f - TANKUPDATEROTATION_SCALE);
        }

        if (bondviewCallTankCollisionStatus(
            &g_CurrentPlayer->field_488.collision_position,
            g_CurrentPlayer->field_488.current_tile_ptr,
            g_TankOrientationAngle) == 0)
        {
            g_TankTurretOrientationAngleRad = sp354;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / (1.0f - TANKUPDATEROTATION_SCALE);
            g_TankTurretAngle = sp354;
        }

        if (g_PlayerTankProp != NULL)
        {
            // sp 0x300
            struct TankRecord *temp_tank;
            struct coord3d tank_move_offset;
            Mtxf sp2B4;
            f32 stack_padding_4;

            temp_tank = (struct TankRecord *)g_PlayerTankProp->obj;

            tank_move_offset.f[1] = 0.0f;
            tank_move_offset.f[0] = g_TankModelPositionOffset.f[0];
            tank_move_offset.f[2] = g_TankModelPositionOffset.f[2];

            matrix_4x4_set_rotation_around_y(tankChangeInAngle, &sp2B4);
            mtx4RotateVecInPlace(&sp2B4, &tank_move_offset);
            bondviewTankModelRotationRelated();

            if (0) { }

            tank_move_offset.f[1] = 0.0f;
            tank_move_offset.f[0] = g_TankModelPositionOffset.f[0] - tank_move_offset.f[0];
            tank_move_offset.f[2] = g_TankModelPositionOffset.f[2] - tank_move_offset.f[2];

            matrix_4x4_set_rotation_around_y(M_TAU_F - g_TankOrientationAngle, &sp2B4);
            matrix_scalar_multiply(temp_tank->model->scale, &sp2B4);
            mtx4RotateVecInPlace(&sp2B4, &tank_move_offset);
            bondviewCalcUpdatePlayerCollision(&tank_move_offset, 1);
        }

        if (g_ClockTimer > 0) {
            for (i=0; i<g_ClockTimer; i++)
            {
                tank_turret_turn_speed = (TANKUPDATEROTATION_SCALE * tank_turret_turn_speed) + (g_TankTurretTurn / g_GlobalTimerDelta);
            }
        }

        ftemp = tank_turret_turn_speed * (1.0f - TANKUPDATEROTATION_SCALE);

        g_CurrentPlayer->vv_theta = (
            g_TankOrientationAngle +
            g_TankTurretOrientationAngleRad +
            ((DegToRad1Fact(g_CurrentPlayer->speedtheta * 3.5f)) * (4.0f)) +
            (ftemp * 4.0f)
            ) * 360.0f / M_TAU_F;

        while (g_CurrentPlayer->vv_theta < 0.0f)
        {
            g_CurrentPlayer->vv_theta += 360.0f;
        }
        while (g_CurrentPlayer->vv_theta >= 360.0f)
        {
            g_CurrentPlayer->vv_theta -= 360.0f;
        }
    }
    else
    {
        stack_padding_9 = g_CurrentPlayer->vv_theta + (g_CurrentPlayer->speedtheta * g_GlobalTimerDelta * 3.5f);

        while (stack_padding_9 < 0.0f)
        {
            stack_padding_9 += 360.0f;
        }
        while (stack_padding_9 >= 360.0f)
        {
            stack_padding_9 -= 360.0f;
        }

        g_CurrentPlayer->vv_theta = stack_padding_9;
    }

    bondviewApplyVertaTheta();

    // Handle crouching, and animation between standing and crouching.
    // Add basic block to declare local variables at the correct stack position.
    {
        f32 sp2AC;
        f32 stack_padding_15;

        sp2AC = 0.0f;
        if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            sp2AC = FULL_CROUCH_OFFSET;
        }
        else if (currentPlayerGetCrouchPos() == CROUCH_HALF)
        {
            sp2AC = -60.0f;
        }
        else
        {
            // removed?
            currentPlayerGetCrouchPos();
        }

        if (sp2AC != g_CurrentPlayer->ducking_height_offset)
        {
            chrobjApplySpeed(
                &g_CurrentPlayer->ducking_height_offset,
                sp2AC,
                &g_CurrentPlayer->field_A4,
                CHR_OBJ_ACCEL_SPEED_FACTOR,
                CHR_OBJ_ACCEL_SPEED_FACTOR,
                CHR_OBJ_MAXSPEED);
        }

        if (sp2AC == g_CurrentPlayer->ducking_height_offset)
        {
            g_CurrentPlayer->field_A4 = 0.0f;
        }
    }

    /**
     * Update forwards/backwards movement.
    */
    if (g_PlayerIsInTank == 1)
    {
        /**
         * This section handles the forward/backwards movement of the tank.
        */

        Mtxf sp268;
        struct coord3d sp25C;
        f32 sp258;
        f32 sp254;
        s32 stack_padding_14;
        s32 i_3;
        f32 ftemp_5;
        f32 tank_engine_utilization_percent;
        struct TankRecord *tank_obj;

        /**
         * Check to see if Bond is just now entering the tank.
         * If so, initialize the tank prop.
         * This also handles spinning Bond around (if required) to face the same direction as the turret.
        */
        if (g_EnterTankAudioState == TANK_RUN_STATE_NOT_RUNNING)
        {
            if (g_PlayerTankProp != NULL)
            {
                tank_obj = g_PlayerTankProp->obj;
                matrix_4x4_set_rotation_around_y(M_TAU_F - g_TankOrientationAngle, &sp268);
                matrix_scalar_multiply(tank_obj->model->scale, &sp268);

                sp25C.f[0] = g_TankModelPositionOffset.f[0];
                sp25C.f[1] = g_TankModelPositionOffset.f[1];
                sp25C.f[2] = g_TankModelPositionOffset.f[2];
                mtx4RotateVecInPlace(&sp268, (f32*)&sp25C);

                sp25C.f[0] += tank_obj->runtime_pos.f[0];
                sp25C.f[1] += tank_obj->runtime_pos.f[1];
                sp25C.f[2] += tank_obj->runtime_pos.f[2];

                sp258 = ((g_TankOrientationAngle + g_TankTurretOrientationAngleRad) * 360.0f) / M_TAU_F;
                sp254 = g_CurrentPlayer->vv_verta;
                if (sp254 < -20.0f)
                {
                    sp254 = -20.0f;
                }

                g_TankEnteringSitHeight += g_GlobalTimerDelta / 45.0f;
                if (g_TankEnteringSitHeight >= 1.0f)
                {
                    g_TankEnteringSitHeight = 1.0f;
                }

                g_TankEnteringSitHeightRemain = (cosf(g_TankEnteringSitHeight * M_TAU_F * 0.5f) + 1.0f) * 0.5f;

                g_CurrentPlayer->vv_verta =
                    (g_TankEnteringSitHeightRemain * g_TankEnterBondVertAngleDeg)
                    + ((1.0f - g_TankEnteringSitHeightRemain) * sp254);

                ftemp_5 = sp258 - g_TankEnterBondHorizAngleDeg;
                if (ftemp_5 > 180.0f)
                {
                    sp258 -= 360.0f;
                }
                if (ftemp_5 < -180.0f)
                {
                    sp258 += 360.0f;
                }

                g_CurrentPlayer->vv_theta =
                    (g_TankEnteringSitHeightRemain * g_TankEnterBondHorizAngleDeg)
                    + ((1.0f - g_TankEnteringSitHeightRemain) * sp258);

                if (g_CurrentPlayer->vv_theta >= 360.0f)
                {
                    g_CurrentPlayer->vv_theta -= 360.0f;
                }

                if (g_CurrentPlayer->vv_theta < 0.0f)
                {
                    g_CurrentPlayer->vv_theta += 360.0f;
                }

                move_offset.f[0] = (
                        (g_TankEnteringSitHeightRemain * g_EnterTankCoord.f[0]) +
                        ((1.0f - g_TankEnteringSitHeightRemain) * sp25C.f[0])
                    ) -
                    g_CurrentPlayer->field_488.collision_position.f[0];

                move_offset.f[1] = 0.0f;

                move_offset.f[2] = (
                    (g_TankEnteringSitHeightRemain * g_EnterTankCoord.f[2]) +
                    ((1.0f - g_TankEnteringSitHeightRemain) * sp25C.f[2])
                    ) -
                    g_CurrentPlayer->field_488.collision_position.f[2];
            }

            if (!(g_TankEnteringSitHeight >= 1.0f))
            {
            }
            else
            {
                g_EnterTankAudioState = TANK_RUN_STATE_STARTING;
            }
        }
        /**
         * Else, Bond has already entered the tank.
        */
        else
        {
            /**
             * There's an initial "starting" step.
            */
            if (g_EnterTankAudioState == TANK_RUN_STATE_STARTING)
            {
                g_EnterTankAudioState = TANK_RUN_STATE_RUNNING;
                if ((g_TankSfxState[0] == NULL) && (lvlGetControlsLockedFlag() == 0))
                {
                    sndPlaySfx(g_musicSfxBufferPtr, TRUCK_START_SFX, &g_TankSfxState[0]);
                }

                sndCreatePostEvent(g_TankSfxState[0], 8, 0x61A8);
                g_TankEngineSfxVolume = 0x61A8;
            }
            /**
             * Else Bond has fully entered the tank, and the engine is running.
             * Update turret vertical angle.
             * Update engine sound effect volume based on current tank speed.
            */
            else
            {
                f32 tank_scaled_speedforwards;
                f32 tank_scaled_speedtheta;
                f32 tank_vertical_angle;

                tank_scaled_speedforwards = g_CurrentPlayer->speedforwards / TANK_MAX_SPEED;
                tank_scaled_speedtheta = g_CurrentPlayer->speedtheta / 0.3f;

                if (tank_scaled_speedforwards < 0.0f)
                {
                    tank_scaled_speedforwards = -tank_scaled_speedforwards;
                }
                if (tank_scaled_speedtheta < 0.0f)
                {
                    tank_scaled_speedtheta = -tank_scaled_speedtheta;
                }

                tank_engine_utilization_percent = tank_scaled_speedforwards;
                if (tank_scaled_speedforwards < tank_scaled_speedtheta)
                {
                    tank_engine_utilization_percent = tank_scaled_speedtheta;
                }

                if (tank_engine_utilization_percent > 0.0f)
                {
                    if (tank_engine_utilization_percent > 1.0f)
                    {
                        tank_engine_utilization_percent = 1.0f;
                    }

                    if (g_TankSfxState[1] == NULL)
                    {
                        if (lvlGetControlsLockedFlag() == 0)
                        {
                            sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, TANK_SFX, &g_TankSfxState[1]);
                        }
                    }

                    if (g_TankSfxState[1] != NULL)
                    {
                        s32 phi_a2;

                        phi_a2 = 0x7FFF;
                        if (tank_engine_utilization_percent < 0.15f)
                        {
                            phi_a2 = (s32) ((tank_engine_utilization_percent * 20000.0f) / 0.15f);
                        }
                        else if (tank_engine_utilization_percent < 0.9f)
                        {
                            phi_a2 = (s32) ((((tank_engine_utilization_percent - 0.15f) * 12767.0f) / 0.75f) + 20000.0f);
                        }

                        sndCreatePostEvent(g_TankSfxState[1], 8, phi_a2);
                    }
                }
                else
                {
                    if (g_TankSfxState[1] != NULL)
                    {
                        if (sndGetPlayingState(g_TankSfxState[1]) != 0)
                        {
                            sndDeactivate(g_TankSfxState[1]);
                        }
                    }
                }

                if (g_TankSfxState[0] == NULL)
                {
                    if (lvlGetControlsLockedFlag() == 0)
                    {
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, TRUCK_RUN_SFX, &g_TankSfxState[0]);
                    }
                }

                if (g_TankSfxState[0] != NULL)
                {
                    g_TankEngineSfxVolume = 0x7FFF;
                    if (tank_engine_utilization_percent < 0.9f)
                    {
                        g_TankEngineSfxVolume = (s32) (((tank_engine_utilization_percent * 7767.0f) / 0.9f) + 25000.0f);
                    }

                    sndCreatePostEvent(g_TankSfxState[0], 8, g_TankEngineSfxVolume);
                }

                if (getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_TANKSHELLS)
                {
                    tank_vertical_angle = g_CurrentPlayer->field_2A08;
                    tank_vertical_angle += 0.17453294f; /* should be DegToRad1Fact(10), but that yields 0.17453293f */
                }
                else
                {
                    tank_vertical_angle = g_TankTurretVerticalAngle;
                }

                if (tank_vertical_angle > DegToRad1Fact(25))
                {
                    tank_vertical_angle = DegToRad1Fact(25);
                }

                /* -0.087266468f should be DegToRad1Fact(-5), but that yields -0.0872664600611 */
                if (tank_vertical_angle < -0.087266468f)
                {
                    tank_vertical_angle = -0.087266468f;
                }

                for (i_3=0; i_3<g_ClockTimer; i_3++)
                {
                    g_TankTurretVerticalAngleRelated = (TANKTURRETVERTICALANGLERELATED_SCALE * g_TankTurretVerticalAngleRelated) + tank_vertical_angle;
                }

                g_TankTurretVerticalAngle = g_TankTurretVerticalAngleRelated * (1.0f - TANKTURRETVERTICALANGLERELATED_SCALE);
            }

        }

        g_CurrentPlayer->bondbreathing -= (0.750f * g_GlobalTimerDelta) / 2700.0f;

        if (g_CurrentPlayer->bondbreathing < 0.0f)
        {
            g_CurrentPlayer->bondbreathing = 0.0f;
        }

        bondviewMoveAnimationTick(0.0f, 0.0f, 0.0f);

        move_offset.f[0] += g_CurrentPlayer->speedforwards * sinf(M_TAU_F - g_TankOrientationAngle) * g_GlobalTimerDelta;
        move_offset.f[2] += g_CurrentPlayer->speedforwards * cosf(M_TAU_F - g_TankOrientationAngle) * g_GlobalTimerDelta;

        bondviewCalcUpdatePlayerCollision(&move_offset, 1);

        if ((g_EnterTankAudioState == TANK_RUN_STATE_RUNNING) && (g_ClockTimer > 0))
        {
            f32 calc_x;
            f32 calc_z;
            f32 calc_speedforwards;

#if defined(VERSION_EU)
            // Divide by zero check.
            if (g_GlobalTimerDelta == 0)
            {
                #if DEBUG
                    // unknown what went here.
                    return_null();
                #endif
            }
#endif

            calc_x = (g_CurrentPlayer->field_488.collision_position.f[0] - g_CurrentPlayer->bondprevpos.f[0]) / g_GlobalTimerDelta;
            calc_z = (g_CurrentPlayer->field_488.collision_position.f[2] - g_CurrentPlayer->bondprevpos.f[2]) / g_GlobalTimerDelta;
            calc_speedforwards = sqrtf((calc_x * calc_x) + (calc_z * calc_z));

            if (g_CurrentPlayer->speedforwards < 0.0f)
            {
                calc_speedforwards = -calc_speedforwards;
            }

            g_CurrentPlayer->speedforwards = calc_speedforwards;
        }
    }
    else // not in tank: g_PlayerIsInTank != 1
    {
        f32 sp220;
        f32 sp21C;
        f32 dist;
        f32 ftemp_col_x;
        f32 ftemp_col_z;
        f32 stack_padding_12;
        f32 start_collision_pos_x;
        f32 start_collision_pos_z;
        struct StandTile *sp200;
        f32 stack_padding_2;
        s32 stack_padding_11;
        f32 speedforwards;
        f32 ftemp_11;
        f32 speedsideways;
        f32 speedtheta;
        f32 stack_padding_25;
        f32 stack_padding_5;
        s32 stack_padding_6;
        f32 sp164;
        f32 sp2B0;
        f32 stack_padding_3;
        f32 stack_padding_111;
        f32 ftemp_26;
        f32 nd; // canonical name
        f32 ftemp_7;
        f32 sp240;
        struct move_bond_temp_struct curLocus;
        struct move_bond_collision bondCollision;
        f32 shorten; // canonical name
        f32 headpos_x;
        f32 headpos_z;
        struct StandTile *sp174;
        struct StandTile *sp170;
        f32 sp16C;
        f32 sp168;

        if ((g_TankSfxState[0] != NULL) && (sndGetPlayingState(g_TankSfxState[0]) != 0))
        {
            #if defined(VERSION_US)
            g_TankEngineSfxVolume -= (g_ClockTimer * 1000);
            #endif

            #if defined(VERSION_JP) || defined(VERSION_EU)
            g_TankEngineSfxVolume -= (s32)(1000.0f * g_GlobalTimerDelta);
            #endif

            if (g_TankEngineSfxVolume > 0)
            {
                sndCreatePostEvent(g_TankSfxState[0], 8, g_TankEngineSfxVolume);
            }
            else
            {
                sndDeactivate(g_TankSfxState[0]);
            }
        }

        if ((g_TankSfxState[1] != NULL) && (sndGetPlayingState(g_TankSfxState[1]) != 0))
        {
            sndDeactivate(g_TankSfxState[1]);
        }


        ftemp_7 = (g_BondMoveAnimationSetup[1].speedMultiplier * 0.5f  * g_GlobalTimerDelta);
        sp3A0  = g_CurrentPlayer->speedsideways * ftemp_7;

        /*
            The following is similar to a block of Perfect Dark bwalk0f0c69b8.
        */

        ftemp_26 = -g_CurrentPlayer->swaytarget * g_CurrentPlayer->field_488.theta_transform.f[2];
        ftemp_11 = g_CurrentPlayer->swaytarget * g_CurrentPlayer->field_488.theta_transform.f[0];

        sp220 = (ftemp_26) - g_CurrentPlayer->swayoffset0;
        sp21C = (ftemp_11) - g_CurrentPlayer->swayoffset2;

        dist = (sp220 * sp220) + (sp21C * sp21C);

        if (dist >= 100.0f)
        {
            sp220 *= 0.6f * 1.0f;
            sp21C *= 0.6f * 1.0f;
        }

        speedsideways = g_CurrentPlayer->speedsideways * MAX_SPEED_FACTOR;
        speedforwards = g_CurrentPlayer->speedforwards;
        speedtheta = g_CurrentPlayer->speedtheta * MAX_SPEED_FACTOR;

        if (speedsideways < 0.0f)
        {
            speedsideways = -speedsideways;
        }
        if (speedforwards < 0.0f)
        {
            speedforwards = -speedforwards;
        }
        if (speedtheta < 0.0f)
        {
            speedtheta = -speedtheta;
        }

        maxspeed = speedforwards;

#if defined(VERSION_EU)
        if (maxspeed < speedsideways && 1)
#else
        if (maxspeed < speedsideways)
#endif
        {
            maxspeed = speedsideways;
        }
        if (maxspeed < speedtheta)
        {
            maxspeed = speedtheta;
        }

        if (dist >= 0.1f && maxspeed < MAX_SPEED_FACTOR)
        {
            maxspeed = MAX_SPEED_FACTOR;
        }

        if (maxspeed >= 0.750f)
        {
            g_CurrentPlayer->bondbreathing += (maxspeed - 0.750f) * g_GlobalTimerDelta / 900.0f ;
        }
        else
        {
            g_CurrentPlayer->bondbreathing -= (0.750f - maxspeed) * g_GlobalTimerDelta / 2700.0f;
        }

        if (g_CurrentPlayer->bondbreathing < 0.0f)
        {
            g_CurrentPlayer->bondbreathing = 0.0f;
        }
        else if (g_CurrentPlayer->bondbreathing > 1.0f)
        {
            g_CurrentPlayer->bondbreathing = 1.0f;
        }

        // perfect dark call: bmove0f0cc654
        bondviewMoveAnimationTick(maxspeed, g_CurrentPlayer->speedforwards, sp3A0);

        headpos_x = g_CurrentPlayer->headpos.f[0];
        headpos_z = g_CurrentPlayer->headpos.f[2];

        move_offset.f[0] +=
            (
                (headpos_z * g_CurrentPlayer->field_488.theta_transform.f[0]) -
                (headpos_x * g_CurrentPlayer->field_488.theta_transform.f[2])
            ) * g_GlobalTimerDelta;

        move_offset.f[2] +=
            (
                (headpos_z * g_CurrentPlayer->field_488.theta_transform.f[2]) +
                (headpos_x * g_CurrentPlayer->field_488.theta_transform.f[0])
            ) * g_GlobalTimerDelta;


        move_offset.f[0] += sp220;
        move_offset.f[2] += sp21C;

        start_collision_pos_x = g_CurrentPlayer->field_488.collision_position.f[0];
        start_collision_pos_z = g_CurrentPlayer->field_488.collision_position.f[2];
        sp200 = g_CurrentPlayer->field_488.current_tile_ptr;

        if (get_debug_fast_bond_flag())
        {
            move_offset.f[0] +=
                (
                    (g_CurrentPlayer->field_488.theta_transform.f[0] * g_CurrentPlayer->speedforwards) -
                    (g_CurrentPlayer->field_488.theta_transform.f[2] * g_CurrentPlayer->speedsideways)
                ) * g_GlobalTimerDelta * 10.0f;

            move_offset.f[2] +=
                (
                    (g_CurrentPlayer->field_488.theta_transform.f[2] * g_CurrentPlayer->speedforwards) +
                    (g_CurrentPlayer->field_488.theta_transform.f[0] * g_CurrentPlayer->speedsideways)
                ) * g_GlobalTimerDelta * 10.0f;
        }

        bondviewCalcUpdatePlayerCollision(&move_offset, (g_CurrentPlayer->swaytarget == 0.0f));

        stanTileDistanceRelated(
            &sp200,
            start_collision_pos_x,
            start_collision_pos_z,
            g_CurrentPlayer->field_488.collision_radius * 1.16f,
            &curLocus);

        /* almost never true */
        if (stanGetLocusCount(&curLocus) != 0)
        {
            use_stanHeight = 1;
        }

        stanTileDistanceRelated(
            &g_CurrentPlayer->field_488.current_tile_ptr,
            g_CurrentPlayer->field_488.collision_position.f[0],
            g_CurrentPlayer->field_488.collision_position.f[2],
            g_CurrentPlayer->field_488.collision_radius * 1.01f,
            &curLocus);

        /* almost never true */
        if (stanGetLocusCount(&curLocus) != 0)
        {
            use_stanHeight = 1;
        }

        stanTileDistanceRelated(
            &g_CurrentPlayer->field_488.current_tile_ptr,
            g_CurrentPlayer->field_488.collision_position.f[0],
            g_CurrentPlayer->field_488.collision_position.f[2],
            g_CurrentPlayer->field_488.collision_radius,
            &curLocus);

        /* almost always true */
        if (stanGetLocusCount(&curLocus) == 0)
        {
            stanTileDistanceRelated(
                &sp200,
                start_collision_pos_x,
                start_collision_pos_z,
                g_CurrentPlayer->field_488.collision_radius * 0.990099f,
                &curLocus);
        }

        /* almost never true */
        if (stanGetLocusCount(&curLocus))
        {
            use_stanHeight = 1;
            stanGetMoveBondCollisionTiles(&sp174, &sp170, &bondCollision);

            if (g_CurrentPlayer->stanHeight <= bondCollision.sp19C.f[1])
            {
                f32 sp2A8;
                f32 sp24C;
                f32 sp250;
                f32 sp310;
                f32 sp314;

                sp168 = bondCollision.bondCollision.f[0] - bondCollision.sp190.f[0];
                sp16C = bondCollision.sp190.f[2] - bondCollision.bondCollision.f[2];

                nd = sqrtf((sp16C * sp16C) + (sp168 * sp168));

                sp168 = sp168 / nd;
                sp16C = sp16C / nd;

                sp164 =
                    ((start_collision_pos_x - bondCollision.bondCollision.f[0]) * sp16C) +
                    ((start_collision_pos_z - bondCollision.bondCollision.f[2]) * sp168);

                sp2B0 =
                    (((start_collision_pos_x + move_offset.f[0]) - bondCollision.bondCollision.f[0]) * sp16C) +
                    (((start_collision_pos_z + move_offset.f[2]) - bondCollision.bondCollision.f[2]) * sp168);

                if ((sp164 * sp2B0) <= 0.0f)
                {
                    shorten = 0.0f;
                }
                else
                {
                    if (sp2B0 < 0.0f)
                    {
                        sp164 = -sp164;
                        sp2B0 = -sp2B0;
                    }

                    if (sp164 <= sp2B0)
                    {
                        shorten = 0.0f;
                    }
                    else if (g_CurrentPlayer->field_488.collision_radius < sp2B0)
                    {
                        shorten = 0.0f;
                    }
                    else if (sp164 < g_CurrentPlayer->field_488.collision_radius)
                    {
                        shorten = 0.0f;
                    }
                    else
                    {
                        shorten = (sp164 - g_CurrentPlayer->field_488.collision_radius) / (sp164 - sp2B0);
                    }
                }

                sp2A8 = sqrtf((move_offset.f[0] * move_offset.f[0]) + (move_offset.f[2] * move_offset.f[2]));
                if (sp2A8 > 0.0f)
                {
                    f32 sp318;

                    sp318 = (1.0f - shorten) * sp2A8;
                    sp390 = sp318 * 0.25f;

                    if (bondCollision.sp19C.f[1] <= (g_CurrentPlayer->stanHeight + sp390))
                    {
                        sp390 = (bondCollision.sp19C.f[1] - g_CurrentPlayer->stanHeight);
                        sp318 -= (sp390 / 0.25f);
                        shorten += (sp318 / sp2A8);
                    }
                }

                sp314 = bondCollision.sp19C.f[0] - bondCollision.bondCollision.f[0];
                sp310 = bondCollision.sp19C.f[2] - bondCollision.bondCollision.f[2];
                sp250 = bondCollision.sp1A8.f[0] - bondCollision.bondCollision.f[0];
                sp24C = bondCollision.sp1A8.f[2] - bondCollision.bondCollision.f[2];

                if (((sp250 * sp250) + (sp24C * sp24C)) < ((sp314 * sp314) + (sp310 * sp310)))
                {
                    sp314 = sp250;
                    sp310 = sp24C;
                }

                move_offset.f[0] = (move_offset.f[0] * shorten) + (sp314 * (sp390 / (bondCollision.sp19C.f[1] - bondCollision.bondCollision.f[1])));
                move_offset.f[2] = (move_offset.f[2] * shorten) + (sp310 * (sp390 / (bondCollision.sp19C.f[1] - bondCollision.bondCollision.f[1])));

                g_CurrentPlayer->field_488.collision_position.f[0] = start_collision_pos_x;
                g_CurrentPlayer->field_488.collision_position.f[2] = start_collision_pos_z;
                g_CurrentPlayer->field_488.current_tile_ptr = sp200;

                bondviewCalcUpdatePlayerCollision(&move_offset, (g_CurrentPlayer->swaytarget == 0.0f));
            }
        }

        ftemp_col_x = g_CurrentPlayer->field_488.collision_position.f[0] - start_collision_pos_x;
        ftemp_col_z = g_CurrentPlayer->field_488.collision_position.f[2] - start_collision_pos_z;
        sp240 = (move_offset.f[0] * move_offset.f[0]) + (move_offset.f[2] * move_offset.f[2]);
        if (sp240 != 0.0f)
        {
            sp240 = ((ftemp_col_x * ftemp_col_x) + (ftemp_col_z * ftemp_col_z)) / sp240;
        }
        sp240 = sqrtf(sp240);
        g_CurrentPlayer->swayoffset0 += sp240 * sp220;
        g_CurrentPlayer->swayoffset2 += sp240 * sp21C;
    }

    // add basic block
    {
        f32 breathing;
        f32 sp14C_temp;
        f32 weapon_speed_verta;

        sp14C_temp = g_CurrentPlayer->speedtheta;
        weapon_speed_verta =
            (g_CurrentPlayer->speedverta / 0.7f) +
            (g_CurrentPlayer->field_A4 / CHR_OBJ_MAXSPEED);

        /*
        Following matches the end of Perfect Dark bwalk0f0c69b8
        */
        breathing = bheadGetBreathingValue();

        if (weapon_speed_verta > 1.0f)
        {
            weapon_speed_verta = 1.0f;
        }
        else if (weapon_speed_verta < -1.0f)
        {
            weapon_speed_verta = -1.0f;
        }

        if (g_CurrentPlayer->headanim == 1)
        {
            breathing *= 1.2f;
        }

        // Perfect Dark call bgun0f09d8dc
        gunSetBondWeaponSway(breathing, maxspeed, weapon_speed_verta, sp14C_temp);

        // Perfect Dark call bgunSetAdjustPos
        gunSetOffsetRelated(DegToRad1Fact(g_CurrentPlayer->vv_verta360));
    }

    // end perfect dark `void bwalk0f0c69b8(void)`

    /**
     * The following section updates the TankRecord fields, and handles prop collision detection
     * with the tank. If colliding with character, play the "arrrhghhg" sound effect, or if
     * colliding with prop then set tank movement penalty and create an explosion.
    */
    if ((g_PlayerTankProp != NULL) && (g_PlayerIsInTank == 1) && (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING))
    {
        struct PropRecord *prop;
        struct TankRecord *sp140_tank_as_TankRecord;
        struct ObjectRecord *sp138_tank_as_ObjectRecord;
        f32 ftemp_12;
        struct ModelNode_BoundingBoxRecord *sp130;
        Mtxf spF0;
        struct coord3d spE4;
        s32 stack_padding_13;
        s32 i_4;
        s32 stemp;
        void *stack_padding_8;
        struct rect4f spB4_tank_collision_bounds;
        // roomids
        s32 sp94[8];
        s32 stanlineret;
        s16 *lookup_index;

        sp140_tank_as_TankRecord = ((struct TankRecord *)g_PlayerTankProp->obj);
        sp138_tank_as_ObjectRecord = (struct  ObjectRecord*)g_PlayerTankProp->obj;
        sp130 = (struct ModelNode_BoundingBoxRecord *)((struct ModelNode *)sp138_tank_as_ObjectRecord->model->obj->Switches)->Child->Data;

        sp140_tank_as_TankRecord->is_firing_tank = (getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_TANKSHELLS)
            && get_hands_firing_status(GUNRIGHT);

        sp140_tank_as_TankRecord->turret_vertical_angle = g_TankTurretVerticalAngle;
        sp140_tank_as_TankRecord->turret_orientation_angle = g_TankTurretOrientationAngleRad;
        sp140_tank_as_TankRecord->tank_orientation_angle = g_TankOrientationAngle;

        matrix_4x4_set_rotation_around_y(M_TAU_F - g_TankOrientationAngle, &spF0);
        matrix_scalar_multiply(sp138_tank_as_ObjectRecord->model->scale, &spF0);

        spE4.f[0] = -g_TankModelPositionOffset.f[0];
        spE4.f[1] = -g_TankModelPositionOffset.f[1];
        spE4.f[2] = -g_TankModelPositionOffset.f[2];

        mtx4RotateVecInPlace(&spF0, &spE4);

        spE4.f[0] += g_CurrentPlayer->field_488.collision_position.f[0];
        spE4.f[2] += g_CurrentPlayer->field_488.collision_position.f[2];

        sp138_tank_as_ObjectRecord->prop->stan = g_CurrentPlayer->field_488.current_tile_ptr;

        stanlineret = walkTilesBetweenPoints_NoCallback(
            &sp138_tank_as_ObjectRecord->prop->stan,
            g_CurrentPlayer->field_488.collision_position.f[0],
            g_CurrentPlayer->field_488.collision_position.f[2],
            spE4.f[0],
            spE4.f[2]);
        #ifdef DEBUG
        assert(stanlineret); // #7362
        #endif

        sp140_tank_as_TankRecord->stan_y = stanGetPositionYValue(sp138_tank_as_ObjectRecord->prop->stan, spE4.f[0], spE4.f[2]);

        for (i_4=0; i_4<g_ClockTimer; i_4++)
        {
            sp140_tank_as_TankRecord->unkD0 = (sp140_tank_as_TankRecord->unkD0 * TANK_UNKD0_SCALE) + sp140_tank_as_TankRecord->stan_y;
        }

        ftemp_12 = (sp140_tank_as_TankRecord->unkD0 * (1.0f - TANK_UNKD0_SCALE));
        spE4.f[1] = ftemp_12
            - (chrpropBBOXGetYmin(sp130) * sp138_tank_as_ObjectRecord->model->scale)
            + 4.0f;

        matrix_4x4_copy(&spF0,  &sp138_tank_as_ObjectRecord->mtx);

        sp138_tank_as_ObjectRecord->runtime_pos.f[0] = sp138_tank_as_ObjectRecord->prop->pos.f[0] = spE4.f[0];
        sp138_tank_as_ObjectRecord->runtime_pos.f[1] = sp138_tank_as_ObjectRecord->prop->pos.f[1] = spE4.f[1];
        sp138_tank_as_ObjectRecord->runtime_pos.f[2] = sp138_tank_as_ObjectRecord->prop->pos.f[2] = spE4.f[2];

        setupUpdateObjectRoomPosition(sp138_tank_as_ObjectRecord);
        chrobjCollisionRelated(sp138_tank_as_ObjectRecord);
        bondviewGetTankCollisionBounds(&spB4_tank_collision_bounds, &g_CurrentPlayer->field_488.collision_position, g_TankOrientationAngle);
        chraiGetPropRoomIds(sp138_tank_as_ObjectRecord->prop, &sp94);

        // update num_obj_position_data_entries
        roomGetProps(&sp94);

        for (lookup_index=ptr_list_object_lookup_indices; *lookup_index>=0; lookup_index++)
        {
            prop = &pos_data_entry[*lookup_index];
            if (prop != sp138_tank_as_ObjectRecord->prop)
            {
                if (prop->type == PROP_TYPE_CHR)
                {
                    s32 sp88_collision_bound_height;
                    s32 sp84_collision_bound_z;
                    f32 sp80_collision_radius;
                    s32 sp7C;
                    struct coord3d sp70;
                    struct ChrRecord *sp6C;
                    f32 stack_padding_28;

                    sp7C = 1;
                    sp6C = prop->chr;
                    chrpropGetCollisionBounds(prop, &sp80_collision_radius, &sp88_collision_bound_height, &sp84_collision_bound_z);

                    if (chrpropTestPointInPolygon(&prop->pos, &spB4_tank_collision_bounds, 4))
                    {
                        sp7C = 0;

                        if (sp6C->actiontype == ACT_DIE)
                        {
#if defined(VERSION_US)
                            if ((sp6C->chrflags << 7) >= 0)
#endif
#if defined(VERSION_JP) || defined(VERSION_EU)
                            if ((sp6C->chrflags << 7) >= 0 && lvlGetControlsLockedFlag() == 0)
#endif
                            {
                                sp6C->chrflags |= CHRFLAG_01000000;
                                if ((D_80048380 % 3) < 2)
                                {
                                    chrobjSndCreatePostEventDefault(sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, CRUSHED_YELL_SFX, NULL), &prop->pos);
                                }
                                if ((D_80048380 % 3) > 0)
                                {
                                    chrobjSndCreatePostEventDefault(sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, TANK_CRUSH_MAN_SFX, NULL), &prop->pos);
                                }
                            }
                        }
                    }

                    if (sp7C && (chrobjTestPointPolygonCollision(&prop->pos, sp80_collision_radius, &spB4_tank_collision_bounds, 4)))
                    {
                        sp7C = 0;
                    }

                    if (sp7C == 0)
                    {
                        sp70.f[0] = sp138_tank_as_ObjectRecord->runtime_pos.f[0];
                        sp70.f[1] = prop->pos.f[1];
                        sp70.f[2] = sp138_tank_as_ObjectRecord->runtime_pos.f[2];

                        chrlvExplosionDamage(prop->chr, &sp70, 3.0f, 1);
                    }
                }
                else if (prop->type == PROP_TYPE_OBJ)
                {
                    struct rect4f *polygon;
                    s32 edges;

                    chraiGetCollisionBoundsWithoutY(prop, &polygon, &edges);
                    if ((edges > 0) && chrobjTestPolygonsTouchingOrOverlap2D(polygon, edges, &spB4_tank_collision_bounds, 4))
                    {
                        // Explode destroyable props when the tank touches them
                        maybe_detonate_object_and_its_children(prop, 10000.0f, &prop->obj->runtime_pos, 0x20, get_cur_playernum());
                        g_TankDamagePenaltyTicks = TANK_DAMAGE_PENTALTY_TICKS;
                    }
                }
            }
        }
    }

    bondviewUpdatePlayerY(use_stanHeight, sp390);
    bondviewUpdatePlayerCollisionPositionFields();
    bondviewUpdatePlayerCollisionBounds();

    if (get_debug_man_pos_flag() != 0)
    {
        f32 sp5C_out_unused;

        copy_tile_RGB_as_24bit(
            g_CurrentPlayer->field_488.current_tile_ptr,
            g_CurrentPlayer->field_488.collision_position.f[0],
            g_CurrentPlayer->field_488.collision_position.f[2],
            &sp5C_out_unused);
    }
}




/**
 * US address 7F086990.
 * EU address 7F086AB0.
*/
void bondviewFrozenMoveBond(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    struct coord3d property_pos;
    struct coord3d property_pos2;
    struct coord3d property_offset;
    struct coord3d offset;
    struct StandTile *room_pointer_tile;
    struct coord3d stan_walk_start;

    property_pos = g_DefaultFrozenPlayerPos;
    property_pos2 = g_DefaultFrozenPlayerPos2;
    property_offset = g_DefaultFrozenPlayerOffset;
    offset = g_DefaultFrozenMoveOffset;

    bondviewPlayerTickDamageAndHealth();
    bondviewPlayerTickExplode();
    bondviewProcessInput(0, 0, 0, 0);
    bondviewApplyVertaTheta();
    bondviewMoveAnimationTick(0, 0, 0);

    if ((g_ForceBondMoveOffset.f[0] != 0.0f) || (g_ForceBondMoveOffset.f[2] != 0.0f))
    {
        offset.f[0] += g_ForceBondMoveOffset.f[0] * g_GlobalTimerDelta;
        offset.f[2] += g_ForceBondMoveOffset.f[2] * g_GlobalTimerDelta;
    }

    offset.f[0] += ((g_CurrentPlayer->headpos.f[2] * g_CurrentPlayer->field_488.theta_transform.f[0]) - (g_CurrentPlayer->headpos.f[0] * g_CurrentPlayer->field_488.theta_transform.f[2])) * g_GlobalTimerDelta;
    offset.f[2] += ((g_CurrentPlayer->headpos.f[2] * g_CurrentPlayer->field_488.theta_transform.f[2]) + (g_CurrentPlayer->headpos.f[0] * g_CurrentPlayer->field_488.theta_transform.f[0])) * g_GlobalTimerDelta;

    bondviewCalcUpdatePlayerCollision(&offset, 1);
    bondviewUpdatePlayerY(0, 0.0f);
    bondviewUpdatePlayerCollisionPositionFields();

    if ((g_CameraMode == CAMERAMODE_FP_NOINPUT) || (g_CameraMode == CAMERAMODE_FP) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))
    {
        currentPlayerSetCameraMode(0);
        return;
    }

    bondviewFrozenCameraTick(buttons, oldbuttons, &property_pos, &property_pos2, &property_offset, &room_pointer_tile, &stan_walk_start);
    currentPlayerSetCameraMode(1);
    bondviewSetCurrentPlayerPosition(&property_pos, &property_pos2, &property_offset, room_pointer_tile, &stan_walk_start);
}



s16 getWidth320or440(void)
{
    if (cameraBufferToggle != 0)
    {
        return SCREEN_WIDTH_440;
    }

    return SCREEN_WIDTH_320;
}

s16 getHeight330or240(void)
{
    if (cameraBufferToggle != 0)
    {
        return SCREEN_HEIGHT_330;
    }

    return SCREEN_HEIGHT;
}

s16 bondviewGetCurrentPlayerViewportWidth(void)
{
    if (getPlayerCount() >= 3)
    {
        return VIEWPORT_WIDTH_4P;
    }

    if (cameraBufferToggle != 0)
    {
        return SCREEN_WIDTH_440;
    }

    if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
    {
        return VIEWPORT_WIDTH_WIDESCREEN;
    }

    if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
    {
        return VIEWPORT_WIDTH_CINEMA;
    }

    return VIEWPORT_WIDTH_FULLSCREEN;
}

s16 get_curplayer_viewport_ulx(void)
{
    if (2 < getPlayerCount())
    {
        if ((get_cur_playernum() == 1) || (get_cur_playernum() == 3))
        {
                return 0xa1;
        }
    }

    return 0;
}




/**
 * Address 0x7F086D24.
 */
s16 bondviewGetCurrentPlayerViewportHeight(void)
{
    f32 t;

    if (getPlayerCount() >= 2)
    {
        return VIEWPORT_HEIGHT_4P;
    }

    if (cameraBufferToggle != 0)
    {
        if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
        {
            return VIEWPORT_HEIGHT_WIDESCREEN;
        }
        else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
        {
            return VIEWPORT_HEIGHT_CINEMA;
        }
        else
        {
            return VIEWPORT_HEIGHT_FULLSCREEN;
        }
    }

    if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
    {
        t = bondviewGetPauseAnimationPercent();
        return (s16) ((s32) (WIDESCREEN_SCALE_FACTOR * t) + VIEWPORT_OFFSET_HEIGHT_WIDESCREEN);
    }
    else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
    {
        t = bondviewGetPauseAnimationPercent();
        return (s16) ((s32) (CINEMA_SCALE_FACTOR * t) + VIEWPORT_OFFSET_HEIGHT_CINEMA);
    }
    else
    {
        return VIEWPORT_HEIGHT_DEFAULT;
    }
}



/**
 * Address 0x7F086E38.
 */
s16 bondviewGetCurrentPlayerViewportUly(void)
{
    f32 t;

    if (getPlayerCount() == 2)
    {
        if (get_cur_playernum() == 0)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_2P_PLAYER_1;
#endif
        }

        return VIEWPORT_ULY_2P_PLAYER_2;
    }

    if (getPlayerCount() >= 3)
    {
        if (get_cur_playernum() < 2)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_4P_PLAYER_12;
#endif
        }

        return VIEWPORT_ULY_4P_PLAYER_34;
    }

    if (cameraBufferToggle != 0)
    {
        if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
        {
            return VIEWPORT_ULY_CAM_WIDESCREEN;
        }
        else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
        {
            return VIEWPORT_ULY_CAM_CINEMA;
        }
        else
        {
            return VIEWPORT_ULY_CAM_FULLSCREEN;
        }
    }

    if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
    {
        t = bondviewGetPauseAnimationPercent();
        return (s16) ((s32) (WIDESCREEN_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_WIDESCREEN_OFFSET);
    }
    else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
    {
        t = bondviewGetPauseAnimationPercent();
        return (s16) ((s32) (CINEMA_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_CINEMA_OFFSET);
    }
    else
    {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_DEFAULT;
#endif
    }
}

/**
 * Sets/updates viewport for player.
 * Refreshes autoaim setting.
 * Arguments are passed into MoveBond or bondviewFrozenMoveBond.
 * Checks if necessary to call bossReturnTitleStage.
 * Set player->buttons_pressed to arg2.
 *
 * Address 0x7F086F9C (VERSION_US).
 * Address 0x7F0870BC (VERSION_EU).
 * Address 0x7F087668 (VERSION_JP).
 */
void bondviewMovePlayerUpdateViewport(s8 stick_x, s8 stick_y, u16 buttons)
{
#ifdef VERSION_EU
    f32 faspect;
#endif

    set_cur_player_fovy(FOV_Y_F);

    // This call doesn't do anything, the call viSetFovY(g_CurrentPlayer->fovy); in lvlRender
    // will actually change the field of view.
    // The call above should set g_CurrentPlayer->fovy, but it doesn't seem to affect
    // the fov....
    viSetFovY(FOV_Y_F);

    if (cameraFrameCounter1 != 0)
    {
        if ((cameraFrameCounter1 >= 4) && (resolution != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
        {
            cameraBufferToggle = 1;
            cameraFrameCounter1 = 0;
        }
        else
        {
            cameraFrameCounter1 += 1;
        }
    }
    else
    {
        if (cameraFrameCounter2 != 0)
        {
            if ((cameraFrameCounter2 >= 4) && (viGetFrameBuf2() == (u8*)(cfb_16[0])))
            {
                cameraBufferToggle = 0;
                cameraFrameCounter2 = 0;
            }
            else
            {
                cameraFrameCounter2 += 1;
            }
        }
    }

    if ((cameraBufferToggle != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
    {
        viSetFrameBuf2((u8 *) resolution);
    }

#ifdef VERSION_EU
    if (get_screen_ratio() == SCREEN_RATIO_16_9)
    {
        faspect = ((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight()) * 0.75f * WIDESCREEN_ASPECT;
    }
    else
    {
        faspect = (f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight();
    }

    if (cameraBufferToggle == 0)
    {
        faspect *= EU_CAMERA_8003642C_ASPECT;
    }

    set_cur_player_aspect(faspect);
    viSetAspect(faspect);

#else

    if (get_screen_ratio() == SCREEN_RATIO_16_9)
    {
        set_cur_player_aspect(((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
        viSetAspect(((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
    }
    else
    {
        set_cur_player_aspect((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight());
        viSetAspect((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight());
    }
#endif

    set_cur_player_screen_size( bondviewGetCurrentPlayerViewportWidth(), bondviewGetCurrentPlayerViewportHeight());
    set_cur_player_viewport_size( get_curplayer_viewport_ulx(), bondviewGetCurrentPlayerViewportUly());
    viSetXY(getWidth320or440(), getHeight330or240());
    viSetBuf(getWidth320or440(), getHeight330or240());
    viSetViewSize(bondviewGetCurrentPlayerViewportWidth(), bondviewGetCurrentPlayerViewportHeight());
    viSetViewPosition(get_curplayer_viewport_ulx(), bondviewGetCurrentPlayerViewportUly());
    currentPlayerUpdateColourScreenProperties();
    currentPlayerTickChrFade();
    currentPlayerSetYAutoAimEnabled(cur_player_get_autoaim());
    currentPlayerSetXAutoAimEnabled(cur_player_get_autoaim());
    currentPlayerSetLookAheadSetting(cur_player_get_lookahead());
    gunSetGunAmmoVisible(GUNAMMOREASON_OPTION, cur_player_get_ammo_onscreen_setting());

    gunSetSightVisible(
        GUNSIGHTREASON_1,
        (getPlayerCount() == 1 && cur_player_get_sight_onscreen_control())
            || (getPlayerCount() >= 2 && g_playerPerm->sight)
    );

#if defined(VERSION_EU)
    if (1);
#endif

    if ((g_CameraMode == CAMERAMODE_NONE) || ((g_CameraMode == CAMERAMODE_FP) && (is_timer_active != 0)) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))
    {
        if (get_cur_playernum() == 0)
        {
            mission_timer += g_ClockTimer;
        }

        MoveBond(stick_x, stick_y, buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }
    else
    {
        bondviewFrozenMoveBond(stick_x, stick_y, buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }

#if defined(BUGFIX_R1)
    watch_time_0 += jpD_800484D0;
#else
    // VERSION_US
    watch_time_0 += speedgraphframes;
#endif

    if (stop_time_flag != 0)
    {
        if ((lvlGetControlsLockedFlag() == 0) && ((buttons & ~(g_CurrentPlayer->buttons_pressed) & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON | CONT_R | CONT_L))))
        {
            stop_time_flag = 2;

            if (currentPlayerIsFadeComplete())
            {
                if (g_CurrentPlayer->colourscreenfrac == 0.0f)
                {
                    currentPlayerSetFadeColour(0, 0, 0, 0.0f);
                    currentPlayerSetFadeFrac(60.0f, 1.0f);
                }
            }
            else
            {
                if (g_CurrentPlayer->colourfadefracnew == 0.0f)
                {
                    currentPlayerSetFadeFrac(g_CurrentPlayer->colourfadetime60, 1.0f);
                }
            }
        }

        if ((stop_time_flag == 2) && currentPlayerIsFadeComplete() && (g_CurrentPlayer->colourscreenfrac == 1.0f))
        {
            bossReturnTitleStage();
        }
    }

    if (g_CameraAfterCinema)
    {
        sub_GAME_7F07B1A4();
    }

    if (g_CurrentPlayer->bonddead)
    {
        if (g_CurrentPlayer->redbloodfinished == FALSE)
        {
            currentPlayerEquipWeaponWrapper(GUNLEFT, 0);
            currentPlayerEquipWeaponWrapper(GUNRIGHT, 0);

            if (0)
            {
                // removed?
            };
        }

        if (g_CurrentPlayer->redbloodfinished && g_CurrentPlayer->deathanimfinished && (camera_mode >= CAMERAMODE_SWIRL))
        {
            bossRunTitleStage();
        }
    }

    if ((g_CameraMode == CAMERAMODE_FADE_TO_TITLE) && currentPlayerIsFadeComplete())
    {
        bossRunTitleStage();
    }

    g_CurrentPlayer->buttons_pressed = buttons;
}


/**
 * Address 0x7F0875E4.
 */
void bondviewUpdateCurrentRoomPosition(s32 arg0)
{
    getRoomPositionScaledByIndex(arg0, &g_CurrentPlayer->current_model_pos);
    g_CurrentPlayer->current_room_pos.f[0] = g_CurrentPlayer->current_model_pos.f[0] * get_room_data_float1();
    g_CurrentPlayer->current_room_pos.f[1] = g_CurrentPlayer->current_model_pos.f[1] * get_room_data_float1();
    g_CurrentPlayer->current_room_pos.f[2] = g_CurrentPlayer->current_model_pos.f[2] * get_room_data_float1();
    setPlayerRoomField(arg0);
}


void store_BONDdata_curpos_to_previous(void) {
    g_CurrentPlayer->previous_model_pos.f[0] = g_CurrentPlayer->current_model_pos.f[0];
    g_CurrentPlayer->previous_model_pos.f[1] = g_CurrentPlayer->current_model_pos.f[1];
    g_CurrentPlayer->previous_model_pos.f[2] = g_CurrentPlayer->current_model_pos.f[2];
    mtx4RotateVecInPlace(camGetWorldToScreenMtxf(), &g_CurrentPlayer->previous_model_pos);
}


/**
 * Address: 7F0876C4
 */
void bondviewUpdateCameraMatrices(coord3d* cam_pos, coord3d* cam_look_dir, coord3d* cam_up)
{
    Mtx sp108;
    LookAt *lookat;
    Mtxf spC4;
    coord3d clpos;
    coord3d scaledpos;
    f32 scale;
    Mtx *temp_s0;
    Mtxf *projmtx;
    Mtxf sp60;
    s32 j;
    s32 i;

    i = bondviewGetCurrentPlayersRoom();
    bondviewUpdateCurrentRoomPosition(i);

    g_CurrentPlayer->field_5C = dynAllocateMatrix();
    g_CurrentPlayer->field_60 = dynAllocateMatrix();
    g_CurrentPlayer->field_64 = dynAllocateMatrix();
    g_CurrentPlayer->field_68 = dynAllocateMatrix();

    lookat = dynAllocateLights(2);

    scale = D_800364CC;

    scaledpos.x = (cam_pos->x - g_CurrentPlayer->current_model_pos.x) * scale;
    scaledpos.y = (cam_pos->y - g_CurrentPlayer->current_model_pos.y) * scale;
    scaledpos.z = (cam_pos->z - g_CurrentPlayer->current_model_pos.z) * scale;

    clpos.f[0] = scaledpos.f[0] + cam_look_dir->f[0];
    clpos.f[1] = scaledpos.f[1] + cam_look_dir->f[1];
    clpos.f[2] = scaledpos.f[2] + cam_look_dir->f[2];

    matrix_4x4_set_lookat(&spC4,
        scaledpos.x, scaledpos.y, scaledpos.z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    guLookAtReflect(&sp108, lookat,
        scaledpos.x, scaledpos.y, scaledpos.z,
        clpos.x, clpos.y, clpos.z,
        cam_up->x, cam_up->y, cam_up->z);

    matrix_4x4_set_lookat((Mtxf*) g_CurrentPlayer->field_64,
        cam_pos->x, cam_pos->y, cam_pos->z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    matrix_4x4_set_basis_and_position((Mtxf*) g_CurrentPlayer->field_68,
        cam_pos->x, cam_pos->y, cam_pos->z,
        cam_look_dir->x, cam_look_dir->y, cam_look_dir->z,
        cam_up->x, cam_up->y, cam_up->z);

    temp_s0 = dynAllocateMatrix();

    projmtx = currentPlayerGetProjectionMatrixF();
    matrix_4x4_multiply(projmtx, &spC4, &sp60);

	for (i = 0; i < 4; i++)
    {
		for (j = 0; j < 4; j++)
        {
			if (sp60.m[i][j] > 32000.0f)
            {
				sp60.m[i][j] = 32000.0f;
			}
            else if (sp60.m[i][j] < -32000.0f)
            {
				sp60.m[i][j] = -32000.0f;
			}
		}
	}

    guMtxF2L((f32 (*)[4]) &sp60, temp_s0);
    set_BONDdata_field_10E0((s32) temp_s0);

    scale = bgGetLevelVisibilityScale();

    matrix_scalar_multiply(scale, spC4.m[0]);
    guMtxF2L((f32 (*)[4]) &spC4, (Mtx* ) g_CurrentPlayer->field_5C);
    sub_GAME_7F059334((s32* ) g_CurrentPlayer->field_5C, (s32* ) g_CurrentPlayer->field_60);

    currentPlayerSetMatrix10C8((Mtx* ) g_CurrentPlayer->field_5C);
    currentPlayerSetMatrix10C4((Mtx* ) g_CurrentPlayer->field_60);
    currentPlayerSetMatrix10CC((Mtxf* ) g_CurrentPlayer->field_64);
    currentPlayerSetViewToWorldMtxf((Mtxf* ) g_CurrentPlayer->field_68);

    sub_GAME_7F078464((s32) lookat);
    bondviewUpdateFrustumPlanes();
    store_BONDdata_curpos_to_previous();
}


/**
 * Address: 7F087A08
 */
Gfx *bondviewRenderDebugBondView(Gfx *gdl)
{
    coord3d cam_pos;
    coord3d cam_look;
    coord3d cam_up;
    struct collision434 *collision;
    coord3d shake;
    coord3d vec;
    coord3d zeropos;
    f32 vec_y;
    f32 horizontal_len;
    struct player *player;
    f32 angle;
    f32 vertical_rot;
    f32 ft4;

#if defined(VERSION_EU)
    if (bossGetStageNum() == LEVELID_CUBA)
    {
        if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
        {
            gdl = insert_imageDL(gdl);
            gdl = insert_imageDL(gdl);
            gdl = insert_imageDL(gdl);
            gdl = insert_imageDL(gdl);
        }
        else if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
        {
            gdl = insert_imageDL(gdl);
            gdl = insert_imageDL(gdl);
            gdl = insert_imageDL(gdl);
        }
        else
        {
            gdl = insert_imageDL(gdl);
            gdl = insert_imageDL(gdl);
        }
    }
#endif

    if (g_CurrentPlayer->cameramode == 1) {
        cam_pos.x = g_CurrentPlayer->pos.x;
        cam_pos.y = g_CurrentPlayer->pos.y;
        cam_pos.z = g_CurrentPlayer->pos.z;

        cam_look.x = g_CurrentPlayer->pos2.x - g_CurrentPlayer->pos.x;
        cam_look.y = g_CurrentPlayer->pos2.y - g_CurrentPlayer->pos.y;
        cam_look.z = g_CurrentPlayer->pos2.z - g_CurrentPlayer->pos.z;

        cam_up.x = g_CurrentPlayer->offset.x;
        cam_up.y = g_CurrentPlayer->offset.y;
        cam_up.z = g_CurrentPlayer->offset.z;
    } else {
        collision = &g_CurrentPlayer->field_488;

        shake = D_80036830;

        if (!g_CurrentPlayer->bonddead) {
            explosionScreenShake(
                &collision->pos,
                &collision->applied_view,
                &shake
            );
        } else {
            viShake(0.0f);
        }

        cam_pos.x = collision->pos.x;
        cam_pos.y = collision->pos.y;
        cam_pos.z = collision->pos.z;

        cam_look.x = collision->applied_view.x;
        cam_look.y = collision->applied_view.y;
        cam_look.z = collision->applied_view.z;

        cam_up.x = collision->applied_view2.x;
        cam_up.y = collision->applied_view2.y;
        cam_up.z = collision->applied_view2.z;
    }

    bondviewUpdateCameraMatrices(&cam_pos, &cam_look, &cam_up);
    sub_GAME_7F068190(&zeropos, &vec);


    vec_y = vec.y;
    horizontal_len = sqrtf((vec.z * vec.z) + (vec.x * vec.x));
    vertical_rot = bondviewGetPlayerPitchRadians();
    ft4 = atan2f(vec_y, horizontal_len) + vertical_rot;

    if (ft4 >= M_PI_F) {
        ft4 -= M_TAU_F;
    }
    g_CurrentPlayer->field_2A08 = ft4;

    angle = atan2f(-vec.x, -vec.z);
    if (angle >= M_PI_F) {
        angle -= M_TAU_F;
    }
    g_CurrentPlayer->field_2A0C = angle;

    return gdl;
}


void bondviewSelectCuff(Model *model, ModelFileHeader *header, s32 switchindex)
{
    s32 pad;
    s32 local;
    ModelNode **switches;
    ModelNode **base;
    ModelNode *node;
    s32 offset;
    s32 *rwdata;
    s32 index;
    s32 visible;
    s32 pad2;

    local = fileGetBondForCurrentFolder();
    switches = header->Switches;
    offset = switchindex << 2;

    if (1);

    // byte-indexed on purpose: offset = switchindex * 4. &switches[i] won't match.
    base = (ModelNode **) (((u8 *) switches) + offset);

    if (base[0] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, base[0]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_BOILER;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 1;

    if (((void *) (index * 0)) != base[1])
    {
        node = switches[index];
        rwdata = (s32 *) modelGetNodeRwData(model, node);
        visible = g_CurrentPlayer->bondtype == CUFF_BROSNAN;
        if (visible == 0)
        {
            visible = g_CurrentPlayer->bondtype == CUFF_DALTON;
            if (visible == 0)
            {
                visible = g_CurrentPlayer->bondtype == CUFF_MOORE;
                if (visible == 0)
                {
                    visible = g_CurrentPlayer->bondtype == CUFF_FOLDER;
                    if (visible != 0)
                    {
                        visible = local != 1;
                    }
                }
            }
        }
        *rwdata = visible;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 2;

    if (base[2] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        visible = g_CurrentPlayer->bondtype == CUFF_CONNERY;

        if (visible == 0)
        {
            visible = g_CurrentPlayer->bondtype == CUFF_FOLDER;
            if (visible != 0)
            {
                visible = local == 1;
            }
        }

        *rwdata = visible;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 3;

    if (base[3] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_BLUE;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = (switchindex + 4) ^ (((switchindex + 4) ^ 0) * 0);

    if (base[4])
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_JUNGLE;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 5;

    if (base[5] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_SNOW;
    }
}


/**
 * Address: 7F087E74
 */
Gfx *sub_GAME_7F087E74(Gfx *gdl)
{
    ModelRenderData renderdata;
    Mtxf watchmtx;
    coord3d watchpos;
    f32 t;
    Mtxf *matrices;
    ModelFileHeader *objheader;
    f32 *nodepos;
    union ModelRwData *rwdata;
    Mtx *perspmtx;
    u16 perspNorm;
 
    if (g_CurrentPlayer->watch_animation_state == 0)
    {
        goto end;
    }
 
    if (g_CurrentPlayer->pausing_flag == FALSE)
    {
        goto end;
    }
 
    renderdata = D_8003683C;
    watchpos = *((coord3d *) (&D_8003687C));
    objheader = get_ptr_itemheader_in_hand(GUNLEFT);
    nodepos = (f32 *) objheader->Switches[3];
    rwdata = modelGetNodeRwData((Model *) (&g_CurrentPlayer->something_with_watch_object_instance), (ModelNode *) nodepos);
    perspmtx = dynAllocateMatrix();
#if defined(VERSION_EU)
    guPerspective(perspmtx, &perspNorm, g_CurrentPlayer->zoominfovy, 1.4005603f, 10.0f, 300.0f, 1.0f);
#else
    guPerspective(perspmtx, &perspNorm, g_CurrentPlayer->zoominfovy, 1.4545455f, 10.0f, 300.0f, 1.0f);
#endif
 
    gSPMatrix(gdl++, OS_PHYSICAL_TO_K0((u32) perspmtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPPerspNormalize(gdl++, perspNorm);

    // Keep this nested block for matching.
    {
        Mtxf targetmtx;
        coord3d currot;
        coord3d targetrot;
        quatf quat1;
        quatf quat2;
        quatf quat3;
        coord3d targetpos;
        s32 total_seconds;
        s32 seconds;
        s32 total_minutes;
        s32 minutes;
        f32 framesfrac;
        f32 secondsAngle;
        f32 minutesAngle;
        f32 hoursAngle;
        s32 time;
        f32 *nodepos2;
        Mtxf handmtx;
        Mtx *finalmtx;
 
        rwdata->Switch.visible = g_CurrentPlayer->outside_watch_menu;
    
        watchpos.x = (g_CurrentPlayer->field_488.theta_transform.x * (g_CurrentPlayer->headbodyoffset.z + (-12.0f))) + (g_CurrentPlayer->field_488.collision_position.x + (g_CurrentPlayer->headbodyoffset.x * (-g_CurrentPlayer->field_488.theta_transform.z)));
        watchpos.y = g_CurrentPlayer->headbodyoffset.y + g_CurrentPlayer->field_488.collision_position.y;
        watchpos.z = (g_CurrentPlayer->field_488.theta_transform.z * (g_CurrentPlayer->headbodyoffset.z + (-12.0f))) + (g_CurrentPlayer->field_488.collision_position.z + (g_CurrentPlayer->headbodyoffset.x * g_CurrentPlayer->field_488.theta_transform.x));
    
        matrix_4x4_set_position_and_rotation_around_y(watchpos.f, (360.0f - g_CurrentPlayer->vv_theta) * 0.017453292f, &watchmtx);
        matrix_4x4_multiply_homogeneous_in_place(camGetWorldToScreenMtxf(), &watchmtx);
        matrices = dynAllocate(objheader->numMatrices << 6);
        bondviewSelectCuff((Model *) (&g_CurrentPlayer->something_with_watch_object_instance), objheader, 4);
        renderdata.basemtx = &watchmtx;
        renderdata.mtxlist = matrices;
        subcalcmatrices(&renderdata, (Model *) (&g_CurrentPlayer->something_with_watch_object_instance));
        nodepos = (f32 *) objheader->Switches[0]->Data;
        time = watch_time_0;
        t = g_CurrentPlayer->pause_watch_related_adjust / 20.0f;
    
        if (t > 1.0f)
        {
            t = 1.0f;
        }
    
        targetpos.x = matrices->m[3][0] + (((g_CurrentPlayer->field_1D4 - (nodepos[0] * g_CurrentPlayer->watch_scale_destination)) - matrices->m[3][0]) * t);
        targetpos.y = matrices->m[3][1] + (((g_CurrentPlayer->field_1D8 + (nodepos[2] * g_CurrentPlayer->watch_scale_destination)) - matrices->m[3][1]) * t);
        targetpos.z = matrices->m[3][2] + (((g_CurrentPlayer->pause_watch_position - (nodepos[1] * g_CurrentPlayer->watch_scale_destination)) - matrices->m[3][2]) * t);
    
        matrix_4x4_set_basis_and_position_target(&targetmtx, 0.0f, 0.0f, 0.0f, g_CurrentPlayer->field_1E0, g_CurrentPlayer->field_1E4, g_CurrentPlayer->field_1E8, g_CurrentPlayer->field_1EC, g_CurrentPlayer->field_1F0, g_CurrentPlayer->field_1F4);
        matrix_4x4_get_rotation_around_xyz(matrices, &currot);
        matrix_4x4_get_rotation_around_xyz(&targetmtx, &targetrot);
        quaternion_set_rotation_around_xyzf(currot.f, quat1);
        quaternion_set_rotation_around_xyzf(targetrot.f, quat2);
        quaternion_ensure_shortest_path(quat1, quat2);
        quaternion_slerp(quat1, quat2, t, quat3);
        quaternion_to_matrix(quat3, matrices->m);
        matrix_4x4_set_position(&targetpos, matrices);
        matrix_scalar_multiply(g_CurrentPlayer->watch_scale_destination, (f32 *) matrices);
        total_seconds = time / 60;
        seconds = total_seconds % 60;
        total_minutes = total_seconds / 60;
        minutes = total_minutes % 60;
        framesfrac = ((f32) (time % 60)) / 60.0f;
        secondsAngle = ((-(((f32) seconds) + framesfrac)) * M_TAU_F) / 60.0f;
        minutesAngle = ((((-((f32) minutes)) * M_TAU_F) / 60.0f) * 1.0f) + (secondsAngle / 60.0f);
        hoursAngle = ((((-((f32) ((total_seconds / 3600) % 12))) * M_TAU_F) / 12.0f) + (minutesAngle / 12.0f)) + (secondsAngle / 720.0f);
    
        while (secondsAngle < 0.0f)
        {
            secondsAngle += M_TAU_F;
        }
    
        while (minutesAngle < 0.0f)
        {
            minutesAngle += M_TAU_F;
        }
    
        while (hoursAngle < 0.0f)
        {
            hoursAngle += M_TAU_F;
        }
    
        matrix_4x4_set_position_and_rotation_around_y((f32 *) objheader->Switches[0]->Data, hoursAngle, &matrices[1]);
        matrix_4x4_multiply_in_place(matrices, &matrices[1]);
        matrix_4x4_set_position_and_rotation_around_y((f32 *) objheader->Switches[1]->Data, minutesAngle, &matrices[2]);
        matrix_4x4_multiply_in_place(matrices, &matrices[2]);
        matrix_4x4_set_position_and_rotation_around_y((f32 *) objheader->Switches[2]->Data, secondsAngle, &matrices[3]);
        matrix_4x4_multiply_in_place(matrices, &matrices[3]);
        
        renderdata.flags = 3;
        renderdata.zbufferenabled = 0;
        renderdata.gdl = gdl;
        renderdata.PropType = PROP_TYPE_WEAPON;
    
        if ((g_CurrentPlayer->watch_animation_state == 5) || (g_CurrentPlayer->watch_animation_state == 12))
        {
            renderdata.envcolour.word = 0xcd;
        }
        else
        {
            renderdata.envcolour.word = g_CurrentPlayer->tileColor.a | (((g_CurrentPlayer->tileColor.r << 24) | (g_CurrentPlayer->tileColor.g << 16)) | (g_CurrentPlayer->tileColor.b << 8));
        }
    
        subdraw(&renderdata, (Model *) (&g_CurrentPlayer->something_with_watch_object_instance));
        gdl = renderdata.gdl;
        nodepos2 = (f32 *) objheader->Switches[2]->Data;
        finalmtx = dynAllocateMatrix();
        matrix_4x4_set_identity_and_position((coord3d *) nodepos2, &handmtx);
        matrix_4x4_multiply_in_place(matrices, &handmtx);
        matrix_4x4_7F058C64();
        matrix_4x4_f32_to_s32(&handmtx, finalmtx);
        matrix_4x4_7F058C88();
        gdl = draw_watch_current_page(gdl, finalmtx, (g_CurrentPlayer->watch_animation_state == 5) || (g_CurrentPlayer->watch_animation_state == 12));
        matrix_4x4_7F058C64();
        bondviewTransformManyPosToViewMatrix(g_CurrentPlayer->field_23C, objheader->numMatrices);
        matrix_4x4_7F058C88();
    }
 
    end:
    return gdl;
}


/**
 * Address: 7F088618
 *
 * Renders the in-game health and armor gauges.
 * The watch menu gauges are handled by trigger_solo_watch_menu().
 */
Gfx *bondviewRenderGaugeBars(Gfx *gdl)
{
    Mtx *lookatmtx;
    Mtx *orthomtx;
    Mtxf lookatmtxf;

    //Set up armor bars.
    hudMakeDamageSegments(&g_CurrentPlayer->armor_display_values[0].items[0], 0x2e, 1, g_CurrentPlayer->apparentarmour);
    buildGaugeBarDL((Gfx *)&g_CurrentPlayer->watch_body_armor_bar_gdl, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->armor_display_values[0].items[0]), 0x2e);

    // Set up health bars.
    hudMakeDamageSegments(&g_CurrentPlayer->health_display_values[0].items[0], 0x2e, -1, g_CurrentPlayer->apparenthealth);
    buildGaugeBarDL((Gfx *)&g_CurrentPlayer->watch_health_bar_gdl, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->health_display_values[0].items[0]), 0x2e);

    // Create an orthographic render state for the gauge.
    lookatmtx = dynAllocateMatrix();
    orthomtx = dynAllocateMatrix();

    guOrtho(orthomtx, -800.0f * D_800364CC, 800.0f * D_800364CC, -600.0f * D_800364CC, 600.0f * D_800364CC, -100.0f, 1000.0f, 1.0f);

    gSPMatrix(gdl++, osVirtualToPhysical(orthomtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    matrix_4x4_set_lookat_target(
        &lookatmtxf,
        0.0f, 500.0f, 0.0f,
        0.0f, 0.0f,   0.0f,
        0.0f, 0.0f,  -1.0f
    );

    matrix_4x4_f32_to_s32(&lookatmtxf, (Mtxf *)lookatmtx);

    gSPMatrix(gdl++, osVirtualToPhysical(lookatmtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gDPSetPrimColor(gdl++, 0, 0, 0xe6, 0xe6, 0xe6, 0x00);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH);

    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_body_armor_bar_gdl));
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_health_bar_gdl));

    gSPMatrix(gdl++, osVirtualToPhysical(currentPlayerGetProjectionMatrix()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    return gdl;
}


void mp_respawn_handler(void) 
{
    coord3d start_pos = D_8003688C;
    f32 start_look_angle;
    s32 start_stan;
    s32 pad;
    f32 stan_height;
    s32 var_v0;
    s32 var_v1;
    u32 var_v0_2;
    struct SetupIntroEmpty* intro_record;

    intro_record = g_CurrentSetup.intro;

    init_player_BONDdata();
    bondviewPlayerSpawnRelated();

    g_CurrentPlayer->bonddead = 0;
    g_CurrentPlayer->deathanimfinished = 0;
    g_CurrentPlayer->redbloodfinished = 0;
    g_CurrentPlayer->startnewbonddie = 1;
    g_CurrentPlayer->healthdamagetype = 7;
    g_CurrentPlayer->damagetype = 7;
    g_CurrentPlayer->gunammooff = 0;
    g_CurrentPlayer->gunsightmode = 2;

    hudmsgsSetOn(-1);
    bondviewClearUpperTextDisplayFlag(-1);


    if ((getPlayerCount() >= 2) && (startpadcount > 0))
    {
        var_v1 = bondviewGetRandomSpawnPadIndex();
    }
    else
    {
        var_v1 = 0;
    }

    #ifdef DEBUG
    assert(g_Startpad[var_v1]->stan);
    #endif

    start_pos.x = g_Startpad[var_v1]->pos.x;
    start_pos.z = g_Startpad[var_v1]->pos.z;
    start_stan = g_Startpad[var_v1]->stan;

    stan_height = bondviewYPositionRelated(start_stan, start_pos.x, start_pos.z);

    start_pos.y = g_CurrentPlayer->field_29BC + stan_height;
    g_CurrentPlayer->field_70 = stan_height;

    start_look_angle = randomGetNext() * 2.3283064e-10f * 6.2831855f;

    g_CurrentPlayer->vv_theta = (f32) ((start_look_angle * 360.0f) / 6.2831855f);
    g_CurrentPlayer->stanHeight = stan_height;
#if defined(VERSION_EU)
    g_CurrentPlayer->field_6C = (f32) (stan_height / 0.2004f);
#else
    g_CurrentPlayer->field_6C = (f32) (stan_height / 0.17000002f);
#endif

    change_player_pos_to_target(&g_CurrentPlayer->field_488, &start_pos, start_stan);

    g_CurrentPlayer->field_488.theta_transform.x = -sinf(start_look_angle);
    g_CurrentPlayer->field_488.theta_transform.y = 0.0f;
    g_CurrentPlayer->field_488.theta_transform.z = cosf(start_look_angle);
    g_CurrentPlayer->prop->pos.x = g_CurrentPlayer->bondprevpos.x = start_pos.f[0];
    g_CurrentPlayer->prop->pos.y = g_CurrentPlayer->bondprevpos.y = start_pos.f[1];
    g_CurrentPlayer->prop->pos.z = g_CurrentPlayer->bondprevpos.z = start_pos.f[2];
    g_CurrentPlayer->prop->stan = start_stan;
#if defined(VERSION_EU)
    g_CurrentPlayer->field_3B8.x = (f32) (g_CurrentPlayer->field_488.pos.x / 0.118799984f);
    g_CurrentPlayer->field_3B8.y = (f32) (g_CurrentPlayer->field_488.pos.y / 0.118799984f);
    g_CurrentPlayer->field_3B8.z = (f32) (g_CurrentPlayer->field_488.pos.z / 0.118799984f);
#else
    g_CurrentPlayer->field_3B8.x = (f32) (g_CurrentPlayer->field_488.pos.x / 0.100000024f);
    g_CurrentPlayer->field_3B8.y = (f32) (g_CurrentPlayer->field_488.pos.y / 0.100000024f);
    g_CurrentPlayer->field_3B8.z = (f32) (g_CurrentPlayer->field_488.pos.z / 0.100000024f);
#endif

    bondinvReinitInv();
    var_v0 = 0;

    while (var_v0 != 30)
    {
        g_CurrentPlayer->ammoheldarr[var_v0++] = 0;
    }

    if (intro_record != NULL) 
    {
        while (intro_record->type != 9) // INTROTYPE_END
        { 
            switch (intro_record->type) 
            {
                case 0: // INTROTYPE_SPAWN
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSpawn));
                    break;
                case 1: // INTROTYPE_ITEM
                    if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->is_demo_playback) {
                        if ( ((struct SetupIntroItem*)intro_record)->item_left >= 0) {
                            bondinvAddDoublesInvItem(((struct SetupIntroItem*)intro_record)->item_right, ((struct SetupIntroItem*)intro_record)->item_left);
                        } else {
                            bondinvAddInvItem(((struct SetupIntroItem*)intro_record)->item_right);
                        }
                    }
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroItem));
                    break;
                case 2: // INTROTYPE_AMMO
                    if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->is_demo_playback) {
                        give_cur_player_ammo(((struct SetupIntroAmmo*)intro_record)->ammo_type, ((struct SetupIntroAmmo*)intro_record)->ammo_amount);
                    }
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAmmo));
                    break;
                case 3: // INTROTYPE_SWIRL
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSwirl));
                    break;
                case 4: // INTROTYPE_ANIM
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAnim));
                    break;
                case 5: // INTROTYPE_CUFF
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCuff));
                    break;
                case 6: // INTROTYPE_CAMERA
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCamera));
                    break;
                default: // INTROTYPE_WATCH, INTROTYPE_CREDITS
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroEmpty));
                    break;
            }
    #ifdef DEBUG
            ossyncprintf("unknown bondstart type %d!\n", var_v0_2);
    #endif
        }
    }

    g_CurrentPlayer->field_78 = 0.0f;
    g_CurrentPlayer->field_7C = -0.0001f;
    g_CurrentPlayer->field_80 = 0.0f;
    currentPlayerStartChrFade(120.0f, 1.0f);
}


#ifdef NONMATCHING
Gfx *sub_GAME_7F088CD8(Gfx *gdl) {

}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F088CD8
/* 0BD808 7F088CD8 27BDFF48 */  addiu $sp, $sp, -0xb8
/* 0BD80C 7F088CDC AFBF005C */  sw    $ra, 0x5c($sp)
/* 0BD810 7F088CE0 AFB5004C */  sw    $s5, 0x4c($sp)
/* 0BD814 7F088CE4 0080A825 */  move  $s5, $a0
/* 0BD818 7F088CE8 AFBE0058 */  sw    $fp, 0x58($sp)
/* 0BD81C 7F088CEC AFB70054 */  sw    $s7, 0x54($sp)
/* 0BD820 7F088CF0 AFB60050 */  sw    $s6, 0x50($sp)
/* 0BD824 7F088CF4 AFB40048 */  sw    $s4, 0x48($sp)
/* 0BD828 7F088CF8 AFB30044 */  sw    $s3, 0x44($sp)
/* 0BD82C 7F088CFC AFB20040 */  sw    $s2, 0x40($sp)
/* 0BD830 7F088D00 AFB1003C */  sw    $s1, 0x3c($sp)
/* 0BD834 7F088D04 0C001A57 */  jal   bossGetStageNum
/* 0BD838 7F088D08 AFB00038 */   sw    $s0, 0x38($sp)
/* 0BD83C 7F088D0C 24010036 */  li    $at, 54
/* 0BD840 7F088D10 14410130 */  bne   $v0, $at, .L7F0891D4
/* 0BD844 7F088D14 3C108003 */   lui   $s0, %hi(credits_state)
/* 0BD848 7F088D18 2610643C */  addiu $s0, %lo(credits_state) # addiu $s0, $s0, 0x643c
/* 0BD84C 7F088D1C 8E0E0000 */  lw    $t6, ($s0)
/* 0BD850 7F088D20 24010001 */  li    $at, 1
/* 0BD854 7F088D24 3C0F8003 */  lui   $t7, %hi(credits_pointer)
/* 0BD858 7F088D28 55C1012B */  bnel  $t6, $at, .L7F0891D8
/* 0BD85C 7F088D2C 8FBF005C */   lw    $ra, 0x5c($sp)
/* 0BD860 7F088D30 8DEF6440 */  lw    $t7, %lo(credits_pointer)($t7)
/* 0BD864 7F088D34 3C028003 */  lui   $v0, %hi(camera_80036438)
/* 0BD868 7F088D38 24426438 */  addiu $v0, %lo(camera_80036438) # addiu $v0, $v0, 0x6438
/* 0BD86C 7F088D3C 11E00125 */  beqz  $t7, .L7F0891D4
/* 0BD870 7F088D40 241600DC */   li    $s6, 220
/* 0BD874 7F088D44 8C580000 */  lw    $t8, ($v0)
/* 0BD878 7F088D48 241700DC */  li    $s7, 220
/* 0BD87C 7F088D4C AFA00084 */  sw    $zero, 0x84($sp)
/* 0BD880 7F088D50 27190001 */  addiu $t9, $t8, 1
/* 0BD884 7F088D54 AFA00080 */  sw    $zero, 0x80($sp)
/* 0BD888 7F088D58 AC590000 */  sw    $t9, ($v0)
/* 0BD88C 7F088D5C AFB900B4 */  sw    $t9, 0xb4($sp)
/* 0BD890 7F088D60 0FC2B366 */  jal   microcode_constructor
/* 0BD894 7F088D64 02A02025 */   move  $a0, $s5
/* 0BD898 7F088D68 0C00112B */  jal   viGetViewHeight
/* 0BD89C 7F088D6C 0040A825 */   move  $s5, $v0
/* 0BD8A0 7F088D70 8FAA00B4 */  lw    $t2, 0xb4($sp)
/* 0BD8A4 7F088D74 3C048003 */  lui   $a0, %hi(credits_pointer)
/* 0BD8A8 7F088D78 01421823 */  subu  $v1, $t2, $v0
/* 0BD8AC 7F088D7C 04610003 */  bgez  $v1, .L7F088D8C
/* 0BD8B0 7F088D80 00035903 */   sra   $t3, $v1, 4
/* 0BD8B4 7F088D84 2461000F */  addiu $at, $v1, 0xf
/* 0BD8B8 7F088D88 00015903 */  sra   $t3, $at, 4
.L7F088D8C:
/* 0BD8BC 7F088D8C 01601825 */  move  $v1, $t3
/* 0BD8C0 7F088D90 01603025 */  move  $a2, $t3
/* 0BD8C4 7F088D94 05410003 */  bgez  $t2, .L7F088DA4
/* 0BD8C8 7F088D98 000A6103 */   sra   $t4, $t2, 4
/* 0BD8CC 7F088D9C 2541000F */  addiu $at, $t2, 0xf
/* 0BD8D0 7F088DA0 00016103 */  sra   $t4, $at, 4
.L7F088DA4:
/* 0BD8D4 7F088DA4 258D0001 */  addiu $t5, $t4, 1
/* 0BD8D8 7F088DA8 04610002 */  bgez  $v1, .L7F088DB4
/* 0BD8DC 7F088DAC AFAD0090 */   sw    $t5, 0x90($sp)
/* 0BD8E0 7F088DB0 00003025 */  move  $a2, $zero
.L7F088DB4:
/* 0BD8E4 7F088DB4 18C00024 */  blez  $a2, .L7F088E48
/* 0BD8E8 7F088DB8 00009825 */   move  $s3, $zero
/* 0BD8EC 7F088DBC 8C846440 */  lw    $a0, %lo(credits_pointer)($a0)
.L7F088DC0:
/* 0BD8F0 7F088DC0 94820000 */  lhu   $v0, ($a0)
/* 0BD8F4 7F088DC4 24015011 */  li    $at, 20497
/* 0BD8F8 7F088DC8 14400008 */  bnez  $v0, .L7F088DEC
/* 0BD8FC 7F088DCC 00000000 */   nop
/* 0BD900 7F088DD0 948E0002 */  lhu   $t6, 2($a0)
/* 0BD904 7F088DD4 15C00005 */  bnez  $t6, .L7F088DEC
/* 0BD908 7F088DD8 240F0002 */   li    $t7, 2
/* 0BD90C 7F088DDC AFB30090 */  sw    $s3, 0x90($sp)
/* 0BD910 7F088DE0 02603025 */  move  $a2, $s3
/* 0BD914 7F088DE4 10000018 */  b     .L7F088E48
/* 0BD918 7F088DE8 AE0F0000 */   sw    $t7, ($s0)
.L7F088DEC:
/* 0BD91C 7F088DEC 10410009 */  beq   $v0, $at, .L7F088E14
/* 0BD920 7F088DF0 94850002 */   lhu   $a1, 2($a0)
/* 0BD924 7F088DF4 84830004 */  lh    $v1, 4($a0)
/* 0BD928 7F088DF8 04620003 */  bltzl $v1, .L7F088E08
/* 0BD92C 7F088DFC 84820006 */   lh    $v0, 6($a0)
/* 0BD930 7F088E00 0060B025 */  move  $s6, $v1
/* 0BD934 7F088E04 84820006 */  lh    $v0, 6($a0)
.L7F088E08:
/* 0BD938 7F088E08 04420003 */  bltzl $v0, .L7F088E18
/* 0BD93C 7F088E0C 24015011 */   li    $at, 20497
/* 0BD940 7F088E10 AFA20084 */  sw    $v0, 0x84($sp)
.L7F088E14:
/* 0BD944 7F088E14 24015011 */  li    $at, 20497
.L7F088E18:
/* 0BD948 7F088E18 10A10009 */  beq   $a1, $at, .L7F088E40
/* 0BD94C 7F088E1C 26730001 */   addiu $s3, $s3, 1
/* 0BD950 7F088E20 84830008 */  lh    $v1, 8($a0)
/* 0BD954 7F088E24 04620003 */  bltzl $v1, .L7F088E34
/* 0BD958 7F088E28 8482000A */   lh    $v0, 0xa($a0)
/* 0BD95C 7F088E2C 0060B825 */  move  $s7, $v1
/* 0BD960 7F088E30 8482000A */  lh    $v0, 0xa($a0)
.L7F088E34:
/* 0BD964 7F088E34 04400002 */  bltz  $v0, .L7F088E40
/* 0BD968 7F088E38 00000000 */   nop
/* 0BD96C 7F088E3C AFA20080 */  sw    $v0, 0x80($sp)
.L7F088E40:
/* 0BD970 7F088E40 1666FFDF */  bne   $s3, $a2, .L7F088DC0
/* 0BD974 7F088E44 2484000C */   addiu $a0, $a0, 0xc
.L7F088E48:
/* 0BD978 7F088E48 8FB80090 */  lw    $t8, 0x90($sp)
/* 0BD97C 7F088E4C 00C09825 */  move  $s3, $a2
/* 0BD980 7F088E50 2405000C */  li    $a1, 12
/* 0BD984 7F088E54 00D8082A */  slt   $at, $a2, $t8
/* 0BD988 7F088E58 102000DB */  beqz  $at, .L7F0891C8
/* 0BD98C 7F088E5C 00000000 */   nop
/* 0BD990 7F088E60 00C50019 */  multu $a2, $a1
/* 0BD994 7F088E64 3C038003 */  lui   $v1, %hi(credits_pointer)
/* 0BD998 7F088E68 8C636440 */  lw    $v1, %lo(credits_pointer)($v1)
/* 0BD99C 7F088E6C 0000A012 */  mflo  $s4
/* 0BD9A0 7F088E70 00742021 */  addu  $a0, $v1, $s4
/* 0BD9A4 7F088E74 94820000 */  lhu   $v0, ($a0)
/* 0BD9A8 7F088E78 14400007 */  bnez  $v0, .L7F088E98
/* 0BD9AC 7F088E7C 00000000 */   nop
/* 0BD9B0 7F088E80 00C50019 */  multu $a2, $a1
/* 0BD9B4 7F088E84 0000C812 */  mflo  $t9
/* 0BD9B8 7F088E88 00794821 */  addu  $t1, $v1, $t9
/* 0BD9BC 7F088E8C 952B0002 */  lhu   $t3, 2($t1)
/* 0BD9C0 7F088E90 116000CD */  beqz  $t3, .L7F0891C8
/* 0BD9C4 7F088E94 00000000 */   nop
.L7F088E98:
/* 0BD9C8 7F088E98 3C1E8004 */  lui   $fp, %hi(ptrFontZurichBoldChars)
/* 0BD9CC 7F088E9C 24015011 */  li    $at, 20497
/* 0BD9D0 7F088EA0 1041005E */  beq   $v0, $at, .L7F08901C
/* 0BD9D4 7F088EA4 27DE0EB8 */   addiu $fp, %lo(ptrFontZurichBoldChars) # addiu $fp, $fp, 0xeb8
/* 0BD9D8 7F088EA8 00402025 */  move  $a0, $v0
/* 0BD9DC 7F088EAC 0FC30776 */  jal   langGet
/* 0BD9E0 7F088EB0 00138900 */   sll   $s1, $s3, 4
/* 0BD9E4 7F088EB4 3C0A8003 */  lui   $t2, %hi(credits_pointer)
/* 0BD9E8 7F088EB8 8D4A6440 */  lw    $t2, %lo(credits_pointer)($t2)
/* 0BD9EC 7F088EBC 00409025 */  move  $s2, $v0
/* 0BD9F0 7F088EC0 01542021 */  addu  $a0, $t2, $s4
/* 0BD9F4 7F088EC4 84830004 */  lh    $v1, 4($a0)
/* 0BD9F8 7F088EC8 04620003 */  bltzl $v1, .L7F088ED8
/* 0BD9FC 7F088ECC 84820006 */   lh    $v0, 6($a0)
/* 0BDA00 7F088ED0 0060B025 */  move  $s6, $v1
/* 0BDA04 7F088ED4 84820006 */  lh    $v0, 6($a0)
.L7F088ED8:
/* 0BDA08 7F088ED8 04400002 */  bltz  $v0, .L7F088EE4
/* 0BDA0C 7F088EDC 00000000 */   nop
/* 0BDA10 7F088EE0 AFA20084 */  sw    $v0, 0x84($sp)
.L7F088EE4:
/* 0BDA14 7F088EE4 0C00112B */  jal   viGetViewHeight
/* 0BDA18 7F088EE8 00000000 */   nop
/* 0BDA1C 7F088EEC 00028400 */  sll   $s0, $v0, 0x10
/* 0BDA20 7F088EF0 00106403 */  sra   $t4, $s0, 0x10
/* 0BDA24 7F088EF4 0C001149 */  jal   viGetViewTop
/* 0BDA28 7F088EF8 01808025 */   move  $s0, $t4
/* 0BDA2C 7F088EFC 8FAE00B4 */  lw    $t6, 0xb4($sp)
/* 0BDA30 7F088F00 00516821 */  addu  $t5, $v0, $s1
/* 0BDA34 7F088F04 3C198004 */  lui   $t9, %hi(ptrFontZurichBold)
/* 0BDA38 7F088F08 8F390EB4 */  lw    $t9, %lo(ptrFontZurichBold)($t9)
/* 0BDA3C 7F088F0C 01AE7823 */  subu  $t7, $t5, $t6
/* 0BDA40 7F088F10 01F0C021 */  addu  $t8, $t7, $s0
/* 0BDA44 7F088F14 AFB800A8 */  sw    $t8, 0xa8($sp)
/* 0BDA48 7F088F18 AFA0009C */  sw    $zero, 0x9c($sp)
/* 0BDA4C 7F088F1C 27A4009C */  addiu $a0, $sp, 0x9c
/* 0BDA50 7F088F20 27A50098 */  addiu $a1, $sp, 0x98
/* 0BDA54 7F088F24 02403025 */  move  $a2, $s2
/* 0BDA58 7F088F28 8FC70000 */  lw    $a3, ($fp)
/* 0BDA5C 7F088F2C AFA00014 */  sw    $zero, 0x14($sp)
/* 0BDA60 7F088F30 AFA00098 */  sw    $zero, 0x98($sp)
/* 0BDA64 7F088F34 0FC2BA63 */  jal   textMeasure
/* 0BDA68 7F088F38 AFB90010 */   sw    $t9, 0x10($sp)
/* 0BDA6C 7F088F3C 8FAA0084 */  lw    $t2, 0x84($sp)
/* 0BDA70 7F088F40 8FA900A8 */  lw    $t1, 0xa8($sp)
/* 0BDA74 7F088F44 8FAB009C */  lw    $t3, 0x9c($sp)
/* 0BDA78 7F088F48 24010001 */  li    $at, 1
/* 0BDA7C 7F088F4C 15410005 */  bne   $t2, $at, .L7F088F64
/* 0BDA80 7F088F50 012B1821 */   addu  $v1, $t1, $t3
/* 0BDA84 7F088F54 8FA80098 */  lw    $t0, 0x98($sp)
/* 0BDA88 7F088F58 02C01025 */  move  $v0, $s6
/* 0BDA8C 7F088F5C 1000000C */  b     .L7F088F90
/* 0BDA90 7F088F60 02C82823 */   subu  $a1, $s6, $t0
.L7F088F64:
/* 0BDA94 7F088F64 8FAC0084 */  lw    $t4, 0x84($sp)
/* 0BDA98 7F088F68 24010002 */  li    $at, 2
/* 0BDA9C 7F088F6C 8FA80098 */  lw    $t0, 0x98($sp)
/* 0BDAA0 7F088F70 15810006 */  bne   $t4, $at, .L7F088F8C
/* 0BDAA4 7F088F74 02C02825 */   move  $a1, $s6
/* 0BDAA8 7F088F78 8FA80098 */  lw    $t0, 0x98($sp)
/* 0BDAAC 7F088F7C 00086843 */  sra   $t5, $t0, 1
/* 0BDAB0 7F088F80 02CD2823 */  subu  $a1, $s6, $t5
/* 0BDAB4 7F088F84 10000002 */  b     .L7F088F90
/* 0BDAB8 7F088F88 00A81021 */   addu  $v0, $a1, $t0
.L7F088F8C:
/* 0BDABC 7F088F8C 02C81021 */  addu  $v0, $s6, $t0
.L7F088F90:
/* 0BDAC0 7F088F90 8FA600A8 */  lw    $a2, 0xa8($sp)
/* 0BDAC4 7F088F94 246E0001 */  addiu $t6, $v1, 1
/* 0BDAC8 7F088F98 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0BDACC 7F088F9C 02A02025 */  move  $a0, $s5
/* 0BDAD0 7F088FA0 24470001 */  addiu $a3, $v0, 1
/* 0BDAD4 7F088FA4 AFA00014 */  sw    $zero, 0x14($sp)
/* 0BDAD8 7F088FA8 AFA500AC */  sw    $a1, 0xac($sp)
/* 0BDADC 7F088FAC 0FC2B405 */  jal   microcode_constructor_related_to_menus
/* 0BDAE0 7F088FB0 24C6FFFF */   addiu $a2, $a2, -1
/* 0BDAE4 7F088FB4 0C001107 */  jal   viGetX
/* 0BDAE8 7F088FB8 0040A825 */   move  $s5, $v0
/* 0BDAEC 7F088FBC 00028400 */  sll   $s0, $v0, 0x10
/* 0BDAF0 7F088FC0 00107C03 */  sra   $t7, $s0, 0x10
/* 0BDAF4 7F088FC4 0C00110B */  jal   viGetY
/* 0BDAF8 7F088FC8 01E08025 */   move  $s0, $t7
/* 0BDAFC 7F088FCC 3C198004 */  lui   $t9, %hi(ptrFontZurichBold)
/* 0BDB00 7F088FD0 8F390EB4 */  lw    $t9, %lo(ptrFontZurichBold)($t9)
/* 0BDB04 7F088FD4 8FD80000 */  lw    $t8, ($fp)
/* 0BDB08 7F088FD8 2409FFFF */  li    $t1, -1
/* 0BDB0C 7F088FDC AFA90018 */  sw    $t1, 0x18($sp)
/* 0BDB10 7F088FE0 02A02025 */  move  $a0, $s5
/* 0BDB14 7F088FE4 27A500AC */  addiu $a1, $sp, 0xac
/* 0BDB18 7F088FE8 27A600A8 */  addiu $a2, $sp, 0xa8
/* 0BDB1C 7F088FEC 02403825 */  move  $a3, $s2
/* 0BDB20 7F088FF0 AFB0001C */  sw    $s0, 0x1c($sp)
/* 0BDB24 7F088FF4 AFA20020 */  sw    $v0, 0x20($sp)
/* 0BDB28 7F088FF8 AFA00024 */  sw    $zero, 0x24($sp)
/* 0BDB2C 7F088FFC AFA00028 */  sw    $zero, 0x28($sp)
/* 0BDB30 7F089000 AFB90014 */  sw    $t9, 0x14($sp)
/* 0BDB34 7F089004 0FC2B6AF */  jal   textRender
/* 0BDB38 7F089008 AFB80010 */   sw    $t8, 0x10($sp)
/* 0BDB3C 7F08900C 3C0B8003 */  lui   $t3, %hi(credits_pointer)
/* 0BDB40 7F089010 8D6B6440 */  lw    $t3, %lo(credits_pointer)($t3)
/* 0BDB44 7F089014 0040A825 */  move  $s5, $v0
/* 0BDB48 7F089018 01742021 */  addu  $a0, $t3, $s4
.L7F08901C:
/* 0BDB4C 7F08901C 94850002 */  lhu   $a1, 2($a0)
/* 0BDB50 7F089020 24015011 */  li    $at, 20497
/* 0BDB54 7F089024 10A1005A */  beq   $a1, $at, .L7F089190
/* 0BDB58 7F089028 00A02025 */   move  $a0, $a1
/* 0BDB5C 7F08902C 0FC30776 */  jal   langGet
/* 0BDB60 7F089030 00138900 */   sll   $s1, $s3, 4
/* 0BDB64 7F089034 3C0A8003 */  lui   $t2, %hi(credits_pointer)
/* 0BDB68 7F089038 8D4A6440 */  lw    $t2, %lo(credits_pointer)($t2)
/* 0BDB6C 7F08903C 00409025 */  move  $s2, $v0
/* 0BDB70 7F089040 01542021 */  addu  $a0, $t2, $s4
/* 0BDB74 7F089044 84830008 */  lh    $v1, 8($a0)
/* 0BDB78 7F089048 04620003 */  bltzl $v1, .L7F089058
/* 0BDB7C 7F08904C 8482000A */   lh    $v0, 0xa($a0)
/* 0BDB80 7F089050 0060B825 */  move  $s7, $v1
/* 0BDB84 7F089054 8482000A */  lh    $v0, 0xa($a0)
.L7F089058:
/* 0BDB88 7F089058 04400002 */  bltz  $v0, .L7F089064
/* 0BDB8C 7F08905C 00000000 */   nop
/* 0BDB90 7F089060 AFA20080 */  sw    $v0, 0x80($sp)
.L7F089064:
/* 0BDB94 7F089064 0C00112B */  jal   viGetViewHeight
/* 0BDB98 7F089068 00000000 */   nop
/* 0BDB9C 7F08906C 00028400 */  sll   $s0, $v0, 0x10
/* 0BDBA0 7F089070 00106403 */  sra   $t4, $s0, 0x10
/* 0BDBA4 7F089074 0C001149 */  jal   viGetViewTop
/* 0BDBA8 7F089078 01808025 */   move  $s0, $t4
/* 0BDBAC 7F08907C 8FAE00B4 */  lw    $t6, 0xb4($sp)
/* 0BDBB0 7F089080 00516821 */  addu  $t5, $v0, $s1
/* 0BDBB4 7F089084 3C198004 */  lui   $t9, %hi(ptrFontZurichBold)
/* 0BDBB8 7F089088 8F390EB4 */  lw    $t9, %lo(ptrFontZurichBold)($t9)
/* 0BDBBC 7F08908C 01AE7823 */  subu  $t7, $t5, $t6
/* 0BDBC0 7F089090 01F0C021 */  addu  $t8, $t7, $s0
/* 0BDBC4 7F089094 AFB800A8 */  sw    $t8, 0xa8($sp)
/* 0BDBC8 7F089098 AFA0009C */  sw    $zero, 0x9c($sp)
/* 0BDBCC 7F08909C 27A4009C */  addiu $a0, $sp, 0x9c
/* 0BDBD0 7F0890A0 27A50098 */  addiu $a1, $sp, 0x98
/* 0BDBD4 7F0890A4 02403025 */  move  $a2, $s2
/* 0BDBD8 7F0890A8 8FC70000 */  lw    $a3, ($fp)
/* 0BDBDC 7F0890AC AFA00014 */  sw    $zero, 0x14($sp)
/* 0BDBE0 7F0890B0 AFA00098 */  sw    $zero, 0x98($sp)
/* 0BDBE4 7F0890B4 0FC2BA63 */  jal   textMeasure
/* 0BDBE8 7F0890B8 AFB90010 */   sw    $t9, 0x10($sp)
/* 0BDBEC 7F0890BC 8FAA0080 */  lw    $t2, 0x80($sp)
/* 0BDBF0 7F0890C0 8FA900A8 */  lw    $t1, 0xa8($sp)
/* 0BDBF4 7F0890C4 8FAB009C */  lw    $t3, 0x9c($sp)
/* 0BDBF8 7F0890C8 24010001 */  li    $at, 1
/* 0BDBFC 7F0890CC 15410005 */  bne   $t2, $at, .L7F0890E4
/* 0BDC00 7F0890D0 012B1821 */   addu  $v1, $t1, $t3
/* 0BDC04 7F0890D4 8FA80098 */  lw    $t0, 0x98($sp)
/* 0BDC08 7F0890D8 02E01025 */  move  $v0, $s7
/* 0BDC0C 7F0890DC 1000000C */  b     .L7F089110
/* 0BDC10 7F0890E0 02E82823 */   subu  $a1, $s7, $t0
.L7F0890E4:
/* 0BDC14 7F0890E4 8FAC0080 */  lw    $t4, 0x80($sp)
/* 0BDC18 7F0890E8 24010002 */  li    $at, 2
/* 0BDC1C 7F0890EC 8FA80098 */  lw    $t0, 0x98($sp)
/* 0BDC20 7F0890F0 15810006 */  bne   $t4, $at, .L7F08910C
/* 0BDC24 7F0890F4 02E02825 */   move  $a1, $s7
/* 0BDC28 7F0890F8 8FA80098 */  lw    $t0, 0x98($sp)
/* 0BDC2C 7F0890FC 00086843 */  sra   $t5, $t0, 1
/* 0BDC30 7F089100 02ED2823 */  subu  $a1, $s7, $t5
/* 0BDC34 7F089104 10000002 */  b     .L7F089110
/* 0BDC38 7F089108 00A81021 */   addu  $v0, $a1, $t0
.L7F08910C:
/* 0BDC3C 7F08910C 02E81021 */  addu  $v0, $s7, $t0
.L7F089110:
/* 0BDC40 7F089110 8FA600A8 */  lw    $a2, 0xa8($sp)
/* 0BDC44 7F089114 246E0001 */  addiu $t6, $v1, 1
/* 0BDC48 7F089118 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0BDC4C 7F08911C 02A02025 */  move  $a0, $s5
/* 0BDC50 7F089120 24470001 */  addiu $a3, $v0, 1
/* 0BDC54 7F089124 AFA00014 */  sw    $zero, 0x14($sp)
/* 0BDC58 7F089128 AFA500AC */  sw    $a1, 0xac($sp)
/* 0BDC5C 7F08912C 0FC2B405 */  jal   microcode_constructor_related_to_menus
/* 0BDC60 7F089130 24C6FFFF */   addiu $a2, $a2, -1
/* 0BDC64 7F089134 0C001107 */  jal   viGetX
/* 0BDC68 7F089138 0040A825 */   move  $s5, $v0
/* 0BDC6C 7F08913C 00028400 */  sll   $s0, $v0, 0x10
/* 0BDC70 7F089140 00107C03 */  sra   $t7, $s0, 0x10
/* 0BDC74 7F089144 0C00110B */  jal   viGetY
/* 0BDC78 7F089148 01E08025 */   move  $s0, $t7
/* 0BDC7C 7F08914C 3C198004 */  lui   $t9, %hi(ptrFontZurichBold)
/* 0BDC80 7F089150 8F390EB4 */  lw    $t9, %lo(ptrFontZurichBold)($t9)
/* 0BDC84 7F089154 8FD80000 */  lw    $t8, ($fp)
/* 0BDC88 7F089158 2409FFFF */  li    $t1, -1
/* 0BDC8C 7F08915C AFA90018 */  sw    $t1, 0x18($sp)
/* 0BDC90 7F089160 02A02025 */  move  $a0, $s5
/* 0BDC94 7F089164 27A500AC */  addiu $a1, $sp, 0xac
/* 0BDC98 7F089168 27A600A8 */  addiu $a2, $sp, 0xa8
/* 0BDC9C 7F08916C 02403825 */  move  $a3, $s2
/* 0BDCA0 7F089170 AFB0001C */  sw    $s0, 0x1c($sp)
/* 0BDCA4 7F089174 AFA20020 */  sw    $v0, 0x20($sp)
/* 0BDCA8 7F089178 AFA00024 */  sw    $zero, 0x24($sp)
/* 0BDCAC 7F08917C AFA00028 */  sw    $zero, 0x28($sp)
/* 0BDCB0 7F089180 AFB90014 */  sw    $t9, 0x14($sp)
/* 0BDCB4 7F089184 0FC2B6AF */  jal   textRender
/* 0BDCB8 7F089188 AFB80010 */   sw    $t8, 0x10($sp)
/* 0BDCBC 7F08918C 0040A825 */  move  $s5, $v0
.L7F089190:
/* 0BDCC0 7F089190 8FAB0090 */  lw    $t3, 0x90($sp)
/* 0BDCC4 7F089194 26730001 */  addiu $s3, $s3, 1
/* 0BDCC8 7F089198 2694000C */  addiu $s4, $s4, 0xc
/* 0BDCCC 7F08919C 026B082A */  slt   $at, $s3, $t3
/* 0BDCD0 7F0891A0 10200009 */  beqz  $at, .L7F0891C8
/* 0BDCD4 7F0891A4 3C0A8003 */   lui   $t2, %hi(credits_pointer)
/* 0BDCD8 7F0891A8 8D4A6440 */  lw    $t2, %lo(credits_pointer)($t2)
/* 0BDCDC 7F0891AC 01542021 */  addu  $a0, $t2, $s4
/* 0BDCE0 7F0891B0 94820000 */  lhu   $v0, ($a0)
/* 0BDCE4 7F0891B4 1440FF38 */  bnez  $v0, .L7F088E98
/* 0BDCE8 7F0891B8 00000000 */   nop
/* 0BDCEC 7F0891BC 948C0002 */  lhu   $t4, 2($a0)
/* 0BDCF0 7F0891C0 1580FF35 */  bnez  $t4, .L7F088E98
/* 0BDCF4 7F0891C4 00000000 */   nop
.L7F0891C8:
/* 0BDCF8 7F0891C8 0FC2B3BC */  jal   combiner_bayer_lod_perspective
/* 0BDCFC 7F0891CC 02A02025 */   move  $a0, $s5
/* 0BDD00 7F0891D0 0040A825 */  move  $s5, $v0
.L7F0891D4:
/* 0BDD04 7F0891D4 8FBF005C */  lw    $ra, 0x5c($sp)
.L7F0891D8:
/* 0BDD08 7F0891D8 02A01025 */  move  $v0, $s5
/* 0BDD0C 7F0891DC 8FB5004C */  lw    $s5, 0x4c($sp)
/* 0BDD10 7F0891E0 8FB00038 */  lw    $s0, 0x38($sp)
/* 0BDD14 7F0891E4 8FB1003C */  lw    $s1, 0x3c($sp)
/* 0BDD18 7F0891E8 8FB20040 */  lw    $s2, 0x40($sp)
/* 0BDD1C 7F0891EC 8FB30044 */  lw    $s3, 0x44($sp)
/* 0BDD20 7F0891F0 8FB40048 */  lw    $s4, 0x48($sp)
/* 0BDD24 7F0891F4 8FB60050 */  lw    $s6, 0x50($sp)
/* 0BDD28 7F0891F8 8FB70054 */  lw    $s7, 0x54($sp)
/* 0BDD2C 7F0891FC 8FBE0058 */  lw    $fp, 0x58($sp)
/* 0BDD30 7F089200 03E00008 */  jr    $ra
/* 0BDD34 7F089204 27BD00B8 */   addiu $sp, $sp, 0xb8
)
#endif





#ifdef NONMATCHING
Gfx * maybe_mp_interface(Gfx *arg0) {

}
#else
#if defined(LEFTOVERDEBUG)
GLOBAL_ASM(
.text
glabel maybe_mp_interface
/* 0BDD38 7F089208 3C0E8008 */  lui   $t6, %hi(g_CurrentPlayer)
/* 0BDD3C 7F08920C 8DCEA0B0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0BDD40 7F089210 27BDFFA8 */  addiu $sp, $sp, -0x58
/* 0BDD44 7F089214 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0BDD48 7F089218 AFA40058 */  sw    $a0, 0x58($sp)
/* 0BDD4C 7F08921C 8DCF0000 */  lw    $t7, ($t6)
/* 0BDD50 7F089220 24010001 */  li    $at, 1
/* 0BDD54 7F089224 15E10014 */  bne   $t7, $at, .L7F089278
/* 0BDD58 7F089228 00000000 */   nop
/* 0BDD5C 7F08922C 0FC22939 */  jal   bondviewIntroCameraTextTick
/* 0BDD60 7F089230 00000000 */   nop
/* 0BDD64 7F089234 0FC2297F */  jal   sub_GAME_7F08A5FC
/* 0BDD68 7F089238 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BDD6C 7F08923C 0FC22A7E */  jal   bondviewUpperTextWindowTimerTick
/* 0BDD70 7F089240 AFA20058 */   sw    $v0, 0x58($sp)
/* 0BDD74 7F089244 0FC22ABA */  jal   sub_GAME_7F08AAE8
/* 0BDD78 7F089248 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BDD7C 7F08924C AFA20058 */  sw    $v0, 0x58($sp)
/* 0BDD80 7F089250 0FC15884 */  jal   countdownTimerRender
/* 0BDD84 7F089254 00402025 */   move  $a0, $v0
/* 0BDD88 7F089258 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BDD8C 7F08925C 0FC20138 */  jal   currentPlayerDrawFade
/* 0BDD90 7F089260 00402025 */   move  $a0, $v0
/* 0BDD94 7F089264 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BDD98 7F089268 0FC22336 */  jal   sub_GAME_7F088CD8
/* 0BDD9C 7F08926C 00402025 */   move  $a0, $v0
/* 0BDDA0 7F089270 10000110 */  b     .L7F0896B4
/* 0BDDA4 7F089274 8FBF001C */   lw    $ra, 0x1c($sp)
.L7F089278:
/* 0BDDA8 7F089278 0FC18648 */  jal   bondwalkFireBothHands
/* 0BDDAC 7F08927C 00000000 */   nop
/* 0BDDB0 7F089280 0FC1A423 */  jal   sub_GAME_7F06908C
/* 0BDDB4 7F089284 27A40058 */   addiu $a0, $sp, 0x58
/* 0BDDB8 7F089288 0FC18AF9 */  jal   sub_GAME_7F062BE4
/* 0BDDBC 7F08928C 27A40058 */   addiu $a0, $sp, 0x58
/* 0BDDC0 7F089290 0FC21F9D */  jal   sub_GAME_7F087E74
/* 0BDDC4 7F089294 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BDDC8 7F089298 3C188008 */  lui   $t8, %hi(g_CurrentPlayer)
/* 0BDDCC 7F08929C 8F18A0B0 */  lw    $t8, %lo(g_CurrentPlayer)($t8)
/* 0BDDD0 7F0892A0 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BDDD4 7F0892A4 8F1929C4 */  lw    $t9, 0x29c4($t8)
/* 0BDDD8 7F0892A8 1320001F */  beqz  $t9, .L7F089328
/* 0BDDDC 7F0892AC 00000000 */   nop
/* 0BDDE0 7F0892B0 0C001145 */  jal   viGetViewLeft
/* 0BDDE4 7F0892B4 00000000 */   nop
/* 0BDDE8 7F0892B8 0C001149 */  jal   viGetViewTop
/* 0BDDEC 7F0892BC AFA20054 */   sw    $v0, 0x54($sp)
/* 0BDDF0 7F0892C0 0C001145 */  jal   viGetViewLeft
/* 0BDDF4 7F0892C4 AFA20050 */   sw    $v0, 0x50($sp)
/* 0BDDF8 7F0892C8 0C001127 */  jal   viGetViewWidth
/* 0BDDFC 7F0892CC A7A2002E */   sh    $v0, 0x2e($sp)
/* 0BDE00 7F0892D0 87A8002E */  lh    $t0, 0x2e($sp)
/* 0BDE04 7F0892D4 00484821 */  addu  $t1, $v0, $t0
/* 0BDE08 7F0892D8 0C001149 */  jal   viGetViewTop
/* 0BDE0C 7F0892DC AFA9004C */   sw    $t1, 0x4c($sp)
/* 0BDE10 7F0892E0 0C00112B */  jal   viGetViewHeight
/* 0BDE14 7F0892E4 A7A2002E */   sh    $v0, 0x2e($sp)
/* 0BDE18 7F0892E8 87AA002E */  lh    $t2, 0x2e($sp)
/* 0BDE1C 7F0892EC 8FA40058 */  lw    $a0, 0x58($sp)
/* 0BDE20 7F0892F0 004A5821 */  addu  $t3, $v0, $t2
/* 0BDE24 7F0892F4 0FC2B366 */  jal   microcode_constructor
/* 0BDE28 7F0892F8 AFAB0048 */   sw    $t3, 0x48($sp)
/* 0BDE2C 7F0892FC 8FAC0048 */  lw    $t4, 0x48($sp)
/* 0BDE30 7F089300 240D00A0 */  li    $t5, 160
/* 0BDE34 7F089304 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BDE38 7F089308 AFAD0014 */  sw    $t5, 0x14($sp)
/* 0BDE3C 7F08930C 00402025 */  move  $a0, $v0
/* 0BDE40 7F089310 8FA50054 */  lw    $a1, 0x54($sp)
/* 0BDE44 7F089314 8FA60050 */  lw    $a2, 0x50($sp)
/* 0BDE48 7F089318 8FA7004C */  lw    $a3, 0x4c($sp)
/* 0BDE4C 7F08931C 0FC2B405 */  jal   microcode_constructor_related_to_menus
/* 0BDE50 7F089320 AFAC0010 */   sw    $t4, 0x10($sp)
/* 0BDE54 7F089324 AFA20058 */  sw    $v0, 0x58($sp)
.L7F089328:
/* 0BDE58 7F089328 0FC227AF */  jal   bondviewGetIfCurrentPlayerHealthShowTime
/* 0BDE5C 7F08932C 00000000 */   nop
/* 0BDE60 7F089330 10400009 */  beqz  $v0, .L7F089358
/* 0BDE64 7F089334 3C0E8008 */   lui   $t6, %hi(g_CurrentPlayer)
/* 0BDE68 7F089338 8DCEA0B0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0BDE6C 7F08933C 8DCF01C8 */  lw    $t7, 0x1c8($t6)
/* 0BDE70 7F089340 15E00005 */  bnez  $t7, .L7F089358
/* 0BDE74 7F089344 00000000 */   nop
/* 0BDE78 7F089348 0FC22186 */  jal   bondviewRenderGaugeBars
/* 0BDE7C 7F08934C 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BDE80 7F089350 10000017 */  b     .L7F0893B0
/* 0BDE84 7F089354 AFA20058 */   sw    $v0, 0x58($sp)
.L7F089358:
/* 0BDE88 7F089358 0FC31812 */  jal   sub_GAME_7F0C6048
/* 0BDE8C 7F08935C 00000000 */   nop
/* 0BDE90 7F089360 10400013 */  beqz  $v0, .L7F0893B0
/* 0BDE94 7F089364 00000000 */   nop
/* 0BDE98 7F089368 0FC22186 */  jal   bondviewRenderGaugeBars
/* 0BDE9C 7F08936C 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BDEA0 7F089370 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0BDEA4 7F089374 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BDEA8 7F089378 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BDEAC 7F08937C 3C188005 */  lui   $t8, %hi(g_ClockTimer)
/* 0BDEB0 7F089380 8C642A00 */  lw    $a0, 0x2a00($v1)
/* 0BDEB4 7F089384 18800007 */  blez  $a0, .L7F0893A4
/* 0BDEB8 7F089388 00000000 */   nop
/* 0BDEBC 7F08938C 8F188374 */  lw    $t8, %lo(g_ClockTimer)($t8)
/* 0BDEC0 7F089390 0098C823 */  subu  $t9, $a0, $t8
/* 0BDEC4 7F089394 AC792A00 */  sw    $t9, 0x2a00($v1)
/* 0BDEC8 7F089398 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0BDECC 7F08939C 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BDED0 7F0893A0 8C642A00 */  lw    $a0, 0x2a00($v1)
.L7F0893A4:
/* 0BDED4 7F0893A4 04810002 */  bgez  $a0, .L7F0893B0
/* 0BDED8 7F0893A8 00000000 */   nop
/* 0BDEDC 7F0893AC AC602A00 */  sw    $zero, 0x2a00($v1)
.L7F0893B0:
/* 0BDEE0 7F0893B0 0FC26919 */  jal   getPlayerCount
/* 0BDEE4 7F0893B4 00000000 */   nop
/* 0BDEE8 7F0893B8 24010001 */  li    $at, 1
/* 0BDEEC 7F0893BC 14410003 */  bne   $v0, $at, .L7F0893CC
/* 0BDEF0 7F0893C0 00000000 */   nop
/* 0BDEF4 7F0893C4 0FC15D50 */  jal   display_objective_status_text_on_status_change
/* 0BDEF8 7F0893C8 00000000 */   nop
.L7F0893CC:
/* 0BDEFC 7F0893CC 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0BDF00 7F0893D0 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BDF04 7F0893D4 8C6200D8 */  lw    $v0, 0xd8($v1)
/* 0BDF08 7F0893D8 1040009F */  beqz  $v0, .L7F089658
/* 0BDF0C 7F0893DC 00000000 */   nop
/* 0BDF10 7F0893E0 8C680428 */  lw    $t0, 0x428($v1)
/* 0BDF14 7F0893E4 24010001 */  li    $at, 1
/* 0BDF18 7F0893E8 15000045 */  bnez  $t0, .L7F089500
/* 0BDF1C 7F0893EC 00000000 */   nop
/* 0BDF20 7F0893F0 14410004 */  bne   $v0, $at, .L7F089404
/* 0BDF24 7F0893F4 00002025 */   move  $a0, $zero
/* 0BDF28 7F0893F8 24090002 */  li    $t1, 2
/* 0BDF2C 7F0893FC 24040001 */  li    $a0, 1
/* 0BDF30 7F089400 AC6900D8 */  sw    $t1, 0xd8($v1)
.L7F089404:
/* 0BDF34 7F089404 1080001F */  beqz  $a0, .L7F089484
/* 0BDF38 7F089408 3C0A8008 */   lui   $t2, %hi(g_CurrentPlayer)
/* 0BDF3C 7F08940C 0FC070A1 */  jal   die_blood_image_routine
/* 0BDF40 7F089410 00002025 */   move  $a0, $zero
/* 0BDF44 7F089414 0FC26919 */  jal   getPlayerCount
/* 0BDF48 7F089418 00000000 */   nop
/* 0BDF4C 7F08941C 24010001 */  li    $at, 1
/* 0BDF50 7F089420 14410013 */  bne   $v0, $at, .L7F089470
/* 0BDF54 7F089424 00000000 */   nop
/* 0BDF58 7F089428 0FC30556 */  jal   musicStopSlot
/* 0BDF5C 7F08942C 2404FFFF */   li    $a0, -1
/* 0BDF60 7F089430 0FC3030F */  jal   set_missionstate
/* 0BDF64 7F089434 00002025 */   move  $a0, $zero
/* 0BDF68 7F089438 0FC302FC */  jal   sub_GAME_7F0C0BF0
/* 0BDF6C 7F08943C 00000000 */   nop
/* 0BDF70 7F089440 0C001C0F */  jal   musicTrack1ApplySeqpVol
/* 0BDF74 7F089444 3044FFFF */   andi  $a0, $v0, 0xffff
/* 0BDF78 7F089448 3C018002 */  lui   $at, %hi(g_musicXTrack1Fade)
/* 0BDF7C 7F08944C AC20434C */  sw    $zero, %lo(g_musicXTrack1Fade)($at)
/* 0BDF80 7F089450 0C001CF1 */  jal   musicTrack2ApplySeqpVol
/* 0BDF84 7F089454 00002025 */   move  $a0, $zero
/* 0BDF88 7F089458 3C018002 */  lui   $at, %hi(g_musicXTrack2Fade)
/* 0BDF8C 7F08945C AC204350 */  sw    $zero, %lo(g_musicXTrack2Fade)($at)
/* 0BDF90 7F089460 0C001B9F */  jal   musicTrack1Play
/* 0BDF94 7F089464 2404001B */   li    $a0, 27
/* 0BDF98 7F089468 10000003 */  b     .L7F089478
/* 0BDF9C 7F08946C 00000000 */   nop
.L7F089470:
/* 0BDFA0 7F089470 0FC3030F */  jal   set_missionstate
/* 0BDFA4 7F089474 24040006 */   li    $a0, 6
.L7F089478:
/* 0BDFA8 7F089478 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0BDFAC 7F08947C 10000020 */  b     .L7F089500
/* 0BDFB0 7F089480 8C63A0B0 */   lw    $v1, %lo(g_CurrentPlayer)($v1)
.L7F089484:
/* 0BDFB4 7F089484 8D4AA0B0 */  lw    $t2, %lo(g_CurrentPlayer)($t2)
/* 0BDFB8 7F089488 24040096 */  li    $a0, 150
/* 0BDFBC 7F08948C 00002825 */  move  $a1, $zero
/* 0BDFC0 7F089490 8D4B0424 */  lw    $t3, 0x424($t2)
/* 0BDFC4 7F089494 00003025 */  move  $a2, $zero
/* 0BDFC8 7F089498 3C073F34 */  lui   $a3, (0x3F34B4B5 >> 16) # lui $a3, 0x3f34
/* 0BDFCC 7F08949C 11600006 */  beqz  $t3, .L7F0894B8
/* 0BDFD0 7F0894A0 3C0C8005 */   lui   $t4, %hi(g_ClockTimer)
/* 0BDFD4 7F0894A4 0FC201EC */  jal   currentPlayerSetFadeColour
/* 0BDFD8 7F0894A8 34E7B4B5 */   ori   $a3, (0x3F34B4B5 & 0xFFFF) # ori $a3, $a3, 0xb4b5
/* 0BDFDC 7F0894AC 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0BDFE0 7F0894B0 10000013 */  b     .L7F089500
/* 0BDFE4 7F0894B4 8C63A0B0 */   lw    $v1, %lo(g_CurrentPlayer)($v1)
.L7F0894B8:
/* 0BDFE8 7F0894B8 8D8C8374 */  lw    $t4, %lo(g_ClockTimer)($t4)
/* 0BDFEC 7F0894BC 24040002 */  li    $a0, 2
/* 0BDFF0 7F0894C0 19800003 */  blez  $t4, .L7F0894D0
/* 0BDFF4 7F0894C4 00000000 */   nop
/* 0BDFF8 7F0894C8 10000001 */  b     .L7F0894D0
/* 0BDFFC 7F0894CC 24040001 */   li    $a0, 1
.L7F0894D0:
/* 0BE000 7F0894D0 0FC070A1 */  jal   die_blood_image_routine
/* 0BE004 7F0894D4 00000000 */   nop
/* 0BE008 7F0894D8 10400004 */  beqz  $v0, .L7F0894EC
/* 0BE00C 7F0894DC 3C0E8008 */   lui   $t6, %hi(g_CurrentPlayer)
/* 0BE010 7F0894E0 8DCEA0B0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0BE014 7F0894E4 240D0001 */  li    $t5, 1
/* 0BE018 7F0894E8 ADCD0424 */  sw    $t5, 0x424($t6)
.L7F0894EC:
/* 0BE01C 7F0894EC 0FC0719C */  jal   gameplayBloodOverlayDL
/* 0BE020 7F0894F0 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BE024 7F0894F4 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0BE028 7F0894F8 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BE02C 7F0894FC AFA20058 */  sw    $v0, 0x58($sp)
.L7F089500:
/* 0BE030 7F089500 0FC1BD6F */  jal   objecthandlerGetModelField28
/* 0BE034 7F089504 24640598 */   addiu $a0, $v1, 0x598
/* 0BE038 7F089508 3C048008 */  lui   $a0, %hi(g_CurrentPlayer)
/* 0BE03C 7F08950C 8C84A0B0 */  lw    $a0, %lo(g_CurrentPlayer)($a0)
/* 0BE040 7F089510 E7A00024 */  swc1  $f0, 0x24($sp)
/* 0BE044 7F089514 0FC1BD71 */  jal   sub_GAME_7F06F5C4
/* 0BE048 7F089518 24840598 */   addiu $a0, $a0, 0x598
/* 0BE04C 7F08951C C7A40024 */  lwc1  $f4, 0x24($sp)
/* 0BE050 7F089520 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0BE054 7F089524 4604003E */  c.le.s $f0, $f4
/* 0BE058 7F089528 00000000 */  nop
/* 0BE05C 7F08952C 4500004A */  bc1f  .L7F089658
/* 0BE060 7F089530 00000000 */   nop
/* 0BE064 7F089534 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BE068 7F089538 8C6F0424 */  lw    $t7, 0x424($v1)
/* 0BE06C 7F08953C 11E00046 */  beqz  $t7, .L7F089658
/* 0BE070 7F089540 00000000 */   nop
/* 0BE074 7F089544 8C780428 */  lw    $t8, 0x428($v1)
/* 0BE078 7F089548 24190001 */  li    $t9, 1
/* 0BE07C 7F08954C 3C014270 */  li    $at, 0x42700000 # 60.000000
/* 0BE080 7F089550 1700000E */  bnez  $t8, .L7F08958C
/* 0BE084 7F089554 00002825 */   move  $a1, $zero
/* 0BE088 7F089558 44816000 */  mtc1  $at, $f12
/* 0BE08C 7F08955C 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0BE090 7F089560 44813000 */  mtc1  $at, $f6
/* 0BE094 7F089564 AC790428 */  sw    $t9, 0x428($v1)
/* 0BE098 7F089568 00003025 */  move  $a2, $zero
/* 0BE09C 7F08956C 00003825 */  move  $a3, $zero
/* 0BE0A0 7F089570 0FC201F8 */  jal   currentPlayerAdjustFade
/* 0BE0A4 7F089574 E7A60010 */   swc1  $f6, 0x10($sp)
/* 0BE0A8 7F089578 3C0142F0 */  li    $at, 0x42F00000 # 120.000000
/* 0BE0AC 7F08957C 44816000 */  mtc1  $at, $f12
/* 0BE0B0 7F089580 44807000 */  mtc1  $zero, $f14
/* 0BE0B4 7F089584 0FC20284 */  jal   currentPlayerStartChrFade
/* 0BE0B8 7F089588 00000000 */   nop
.L7F08958C:
/* 0BE0BC 7F08958C 0FC20223 */  jal   currentPlayerIsFadeComplete
/* 0BE0C0 7F089590 00000000 */   nop
/* 0BE0C4 7F089594 10400030 */  beqz  $v0, .L7F089658
/* 0BE0C8 7F089598 00000000 */   nop
/* 0BE0CC 7F08959C 0FC26919 */  jal   getPlayerCount
/* 0BE0D0 7F0895A0 00000000 */   nop
/* 0BE0D4 7F0895A4 24010001 */  li    $at, 1
/* 0BE0D8 7F0895A8 14410005 */  bne   $v0, $at, .L7F0895C0
/* 0BE0DC 7F0895AC 00000000 */   nop
/* 0BE0E0 7F0895B0 0FC1EA6E */  jal   bondviewSetCameraMode
/* 0BE0E4 7F0895B4 24040005 */   li    $a0, 5
/* 0BE0E8 7F0895B8 10000027 */  b     .L7F089658
/* 0BE0EC 7F0895BC 00000000 */   nop
.L7F0895C0:
/* 0BE0F0 7F0895C0 0FC051D6 */  jal   get_scenario
/* 0BE0F4 7F0895C4 00000000 */   nop
/* 0BE0F8 7F0895C8 0FC26C54 */  jal   get_cur_playernum
/* 0BE0FC 7F0895CC AFA20040 */   sw    $v0, 0x40($sp)
/* 0BE100 7F0895D0 0FC26919 */  jal   getPlayerCount
/* 0BE104 7F0895D4 AFA2003C */   sw    $v0, 0x3c($sp)
/* 0BE108 7F0895D8 00402825 */  move  $a1, $v0
/* 0BE10C 7F0895DC 00002025 */  move  $a0, $zero
/* 0BE110 7F0895E0 1840000C */  blez  $v0, .L7F089614
/* 0BE114 7F0895E4 00001825 */   move  $v1, $zero
/* 0BE118 7F0895E8 8FA8003C */  lw    $t0, 0x3c($sp)
/* 0BE11C 7F0895EC 3C0A8008 */  lui   $t2, %hi(g_playerPlayerData)
/* 0BE120 7F0895F0 254A9EF0 */  addiu $t2, %lo(g_playerPlayerData) # addiu $t2, $t2, -0x6110
/* 0BE124 7F0895F4 00084880 */  sll   $t1, $t0, 2
/* 0BE128 7F0895F8 012A1021 */  addu  $v0, $t1, $t2
.L7F0895FC:
/* 0BE12C 7F0895FC 8C4B0024 */  lw    $t3, 0x24($v0)
/* 0BE130 7F089600 24630001 */  addiu $v1, $v1, 1
/* 0BE134 7F089604 0065082A */  slt   $at, $v1, $a1
/* 0BE138 7F089608 24420070 */  addiu $v0, $v0, 0x70
/* 0BE13C 7F08960C 1420FFFB */  bnez  $at, .L7F0895FC
/* 0BE140 7F089610 008B2021 */   addu  $a0, $a0, $t3
.L7F089614:
/* 0BE144 7F089614 8FAC0040 */  lw    $t4, 0x40($sp)
/* 0BE148 7F089618 24010001 */  li    $at, 1
/* 0BE14C 7F08961C 15810003 */  bne   $t4, $at, .L7F08962C
/* 0BE150 7F089620 28810002 */   slti  $at, $a0, 2
/* 0BE154 7F089624 1020000C */  beqz  $at, .L7F089658
/* 0BE158 7F089628 00000000 */   nop
.L7F08962C:
/* 0BE15C 7F08962C 0FC26C54 */  jal   get_cur_playernum
/* 0BE160 7F089630 00000000 */   nop
/* 0BE164 7F089634 00022600 */  sll   $a0, $v0, 0x18
/* 0BE168 7F089638 00046E03 */  sra   $t5, $a0, 0x18
/* 0BE16C 7F08963C 01A02025 */  move  $a0, $t5
/* 0BE170 7F089640 0C0030C3 */  jal   joyGetButtons
/* 0BE174 7F089644 3405B000 */   li    $a1, 45056
/* 0BE178 7F089648 10400003 */  beqz  $v0, .L7F089658
/* 0BE17C 7F08964C 00000000 */   nop
/* 0BE180 7F089650 0FC2223A */  jal   mp_respawn_handler
/* 0BE184 7F089654 00000000 */   nop
.L7F089658:
/* 0BE188 7F089658 0FC22939 */  jal   bondviewIntroCameraTextTick
/* 0BE18C 7F08965C 00000000 */   nop
/* 0BE190 7F089660 0FC2297F */  jal   sub_GAME_7F08A5FC
/* 0BE194 7F089664 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BE198 7F089668 0FC22A7E */  jal   bondviewUpperTextWindowTimerTick
/* 0BE19C 7F08966C AFA20058 */   sw    $v0, 0x58($sp)
/* 0BE1A0 7F089670 0FC22ABA */  jal   sub_GAME_7F08AAE8
/* 0BE1A4 7F089674 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BE1A8 7F089678 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BE1AC 7F08967C 0FC1A979 */  jal   gunDrawSight
/* 0BE1B0 7F089680 27A40058 */   addiu $a0, $sp, 0x58
/* 0BE1B4 7F089684 0FC1A73D */  jal   generate_ammo_total_microcode
/* 0BE1B8 7F089688 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BE1BC 7F08968C AFA20058 */  sw    $v0, 0x58($sp)
/* 0BE1C0 7F089690 0FC15884 */  jal   countdownTimerRender
/* 0BE1C4 7F089694 00402025 */   move  $a0, $v0
/* 0BE1C8 7F089698 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BE1CC 7F08969C 0FC31824 */  jal   display_red_blue_on_radar
/* 0BE1D0 7F0896A0 00402025 */   move  $a0, $v0
/* 0BE1D4 7F0896A4 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BE1D8 7F0896A8 0FC20138 */  jal   currentPlayerDrawFade
/* 0BE1DC 7F0896AC 00402025 */   move  $a0, $v0
/* 0BE1E0 7F0896B0 8FBF001C */  lw    $ra, 0x1c($sp)
.L7F0896B4:
/* 0BE1E4 7F0896B4 27BD0058 */  addiu $sp, $sp, 0x58
/* 0BE1E8 7F0896B8 03E00008 */  jr    $ra
/* 0BE1EC 7F0896BC 00000000 */   nop
)
#endif

#if !defined(LEFTOVERDEBUG)
GLOBAL_ASM(
.text
glabel maybe_mp_interface
/* 0BBD60 7F089370 3C0E8007 */  lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8007
/* 0BBD64 7F089374 8DCE8BC0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0BBD68 7F089378 27BDFFA8 */  addiu $sp, $sp, -0x58
/* 0BBD6C 7F08937C AFBF001C */  sw    $ra, 0x1c($sp)
/* 0BBD70 7F089380 AFA40058 */  sw    $a0, 0x58($sp)
/* 0BBD74 7F089384 8DCF0000 */  lw    $t7, ($t6)
/* 0BBD78 7F089388 24010001 */  li    $at, 1
/* 0BBD7C 7F08938C 15E10014 */  bne   $t7, $at, .L7F0893E0
/* 0BBD80 7F089390 00000000 */   nop
/* 0BBD84 7F089394 0FC229C0 */  jal   bondviewIntroCameraTextTick
/* 0BBD88 7F089398 00000000 */   nop
/* 0BBD8C 7F08939C 0FC22A06 */  jal   sub_GAME_7F08A5FC
/* 0BBD90 7F0893A0 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BBD94 7F0893A4 0FC22B10 */  jal   bondviewUpperTextWindowTimerTick
/* 0BBD98 7F0893A8 AFA20058 */   sw    $v0, 0x58($sp)
/* 0BBD9C 7F0893AC 0FC22B4C */  jal   sub_GAME_7F08AAE8
/* 0BBDA0 7F0893B0 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BBDA4 7F0893B4 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BBDA8 7F0893B8 0FC1593C */  jal   countdownTimerRender
/* 0BBDAC 7F0893BC 00402025 */   move  $a0, $v0
/* 0BBDB0 7F0893C0 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BBDB4 7F0893C4 0FC20161 */  jal   currentPlayerDrawFade
/* 0BBDB8 7F0893C8 00402025 */   move  $a0, $v0
/* 0BBDBC 7F0893CC AFA20058 */  sw    $v0, 0x58($sp)
/* 0BBDC0 7F0893D0 0FC22390 */  jal   sub_GAME_7F088CD8
/* 0BBDC4 7F0893D4 00402025 */   move  $a0, $v0
/* 0BBDC8 7F0893D8 10000110 */  b     .L7F08981C
/* 0BBDCC 7F0893DC 8FBF001C */   lw    $ra, 0x1c($sp)
.L7F0893E0:
/* 0BBDD0 7F0893E0 0FC18779 */  jal   bondwalkFireBothHands
/* 0BBDD4 7F0893E4 00000000 */   nop
/* 0BBDD8 7F0893E8 0FC1A608 */  jal   sub_GAME_7F06908C
/* 0BBDDC 7F0893EC 27A40058 */   addiu $a0, $sp, 0x58
/* 0BBDE0 7F0893F0 0FC18C2A */  jal   sub_GAME_7F062BE4
/* 0BBDE4 7F0893F4 27A40058 */   addiu $a0, $sp, 0x58
/* 0BBDE8 7F0893F8 0FC21FF5 */  jal   sub_GAME_7F087E74
/* 0BBDEC 7F0893FC 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BBDF0 7F089400 3C188007 */  lui   $t8, %hi(g_CurrentPlayer) # $t8, 0x8007
/* 0BBDF4 7F089404 8F188BC0 */  lw    $t8, %lo(g_CurrentPlayer)($t8)
/* 0BBDF8 7F089408 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BBDFC 7F08940C 8F1929BC */  lw    $t9, 0x29bc($t8)
/* 0BBE00 7F089410 1320001F */  beqz  $t9, .L7F089490
/* 0BBE04 7F089414 00000000 */   nop
/* 0BBE08 7F089418 0C000FD9 */  jal   viGetViewLeft
/* 0BBE0C 7F08941C 00000000 */   nop
/* 0BBE10 7F089420 0C000FDD */  jal   viGetViewTop
/* 0BBE14 7F089424 AFA20054 */   sw    $v0, 0x54($sp)
/* 0BBE18 7F089428 0C000FD9 */  jal   viGetViewLeft
/* 0BBE1C 7F08942C AFA20050 */   sw    $v0, 0x50($sp)
/* 0BBE20 7F089430 0C000FBB */  jal   viGetViewWidth
/* 0BBE24 7F089434 A7A2002E */   sh    $v0, 0x2e($sp)
/* 0BBE28 7F089438 87A8002E */  lh    $t0, 0x2e($sp)
/* 0BBE2C 7F08943C 00484821 */  addu  $t1, $v0, $t0
/* 0BBE30 7F089440 0C000FDD */  jal   viGetViewTop
/* 0BBE34 7F089444 AFA9004C */   sw    $t1, 0x4c($sp)
/* 0BBE38 7F089448 0C000FBF */  jal   viGetViewHeight
/* 0BBE3C 7F08944C A7A2002E */   sh    $v0, 0x2e($sp)
/* 0BBE40 7F089450 87AA002E */  lh    $t2, 0x2e($sp)
/* 0BBE44 7F089454 8FA40058 */  lw    $a0, 0x58($sp)
/* 0BBE48 7F089458 004A5821 */  addu  $t3, $v0, $t2
/* 0BBE4C 7F08945C 0FC2B016 */  jal   microcode_constructor
/* 0BBE50 7F089460 AFAB0048 */   sw    $t3, 0x48($sp)
/* 0BBE54 7F089464 8FAC0048 */  lw    $t4, 0x48($sp)
/* 0BBE58 7F089468 240D00A0 */  li    $t5, 160
/* 0BBE5C 7F08946C AFA20058 */  sw    $v0, 0x58($sp)
/* 0BBE60 7F089470 AFAD0014 */  sw    $t5, 0x14($sp)
/* 0BBE64 7F089474 00402025 */  move  $a0, $v0
/* 0BBE68 7F089478 8FA50054 */  lw    $a1, 0x54($sp)
/* 0BBE6C 7F08947C 8FA60050 */  lw    $a2, 0x50($sp)
/* 0BBE70 7F089480 8FA7004C */  lw    $a3, 0x4c($sp)
/* 0BBE74 7F089484 0FC2B0B5 */  jal   microcode_constructor_related_to_menus
/* 0BBE78 7F089488 AFAC0010 */   sw    $t4, 0x10($sp)
/* 0BBE7C 7F08948C AFA20058 */  sw    $v0, 0x58($sp)
.L7F089490:
/* 0BBE80 7F089490 0FC2281C */  jal   bondviewGetIfCurrentPlayerHealthShowTime
/* 0BBE84 7F089494 00000000 */   nop
/* 0BBE88 7F089498 10400009 */  beqz  $v0, .L7F0894C0
/* 0BBE8C 7F08949C 3C0E8007 */   lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8007
/* 0BBE90 7F0894A0 8DCE8BC0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0BBE94 7F0894A4 8DCF01C8 */  lw    $t7, 0x1c8($t6)
/* 0BBE98 7F0894A8 15E00005 */  bnez  $t7, .L7F0894C0
/* 0BBE9C 7F0894AC 00000000 */   nop
/* 0BBEA0 7F0894B0 0FC221E0 */  jal   bondviewRenderGaugeBars
/* 0BBEA4 7F0894B4 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BBEA8 7F0894B8 10000017 */  b     .L7F089518
/* 0BBEAC 7F0894BC AFA20058 */   sw    $v0, 0x58($sp)
.L7F0894C0:
/* 0BBEB0 7F0894C0 0FC31557 */  jal   sub_GAME_7F0C6048
/* 0BBEB4 7F0894C4 00000000 */   nop
/* 0BBEB8 7F0894C8 10400013 */  beqz  $v0, .L7F089518
/* 0BBEBC 7F0894CC 00000000 */   nop
/* 0BBEC0 7F0894D0 0FC221E0 */  jal   bondviewRenderGaugeBars
/* 0BBEC4 7F0894D4 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BBEC8 7F0894D8 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0BBECC 7F0894DC 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BBED0 7F0894E0 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BBED4 7F0894E4 3C188004 */  lui   $t8, %hi(g_ClockTimer) # $t8, 0x8004
/* 0BBED8 7F0894E8 8C6429F8 */  lw    $a0, 0x29f8($v1)
/* 0BBEDC 7F0894EC 18800007 */  blez  $a0, .L7F08950C
/* 0BBEE0 7F0894F0 00000000 */   nop
/* 0BBEE4 7F0894F4 8F180FF4 */  lw    $t8, %lo(g_ClockTimer)($t8)
/* 0BBEE8 7F0894F8 0098C823 */  subu  $t9, $a0, $t8
/* 0BBEEC 7F0894FC AC7929F8 */  sw    $t9, 0x29f8($v1)
/* 0BBEF0 7F089500 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0BBEF4 7F089504 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BBEF8 7F089508 8C6429F8 */  lw    $a0, 0x29f8($v1)
.L7F08950C:
/* 0BBEFC 7F08950C 04810002 */  bgez  $a0, .L7F089518
/* 0BBF00 7F089510 00000000 */   nop
/* 0BBF04 7F089514 AC6029F8 */  sw    $zero, 0x29f8($v1)
.L7F089518:
/* 0BBF08 7F089518 0FC26669 */  jal   getPlayerCount
/* 0BBF0C 7F08951C 00000000 */   nop
/* 0BBF10 7F089520 24010001 */  li    $at, 1
/* 0BBF14 7F089524 14410003 */  bne   $v0, $at, .L7F089534
/* 0BBF18 7F089528 00000000 */   nop
/* 0BBF1C 7F08952C 0FC15E0C */  jal   display_objective_status_text_on_status_change
/* 0BBF20 7F089530 00000000 */   nop
.L7F089534:
/* 0BBF24 7F089534 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0BBF28 7F089538 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BBF2C 7F08953C 8C6200D8 */  lw    $v0, 0xd8($v1)
/* 0BBF30 7F089540 1040009F */  beqz  $v0, .L7F0897C0
/* 0BBF34 7F089544 00000000 */   nop
/* 0BBF38 7F089548 8C680428 */  lw    $t0, 0x428($v1)
/* 0BBF3C 7F08954C 24010001 */  li    $at, 1
/* 0BBF40 7F089550 15000045 */  bnez  $t0, .L7F089668
/* 0BBF44 7F089554 00000000 */   nop
/* 0BBF48 7F089558 14410004 */  bne   $v0, $at, .L7F08956C
/* 0BBF4C 7F08955C 00002025 */   move  $a0, $zero
/* 0BBF50 7F089560 24090002 */  li    $t1, 2
/* 0BBF54 7F089564 24040001 */  li    $a0, 1
/* 0BBF58 7F089568 AC6900D8 */  sw    $t1, 0xd8($v1)
.L7F08956C:
/* 0BBF5C 7F08956C 1080001F */  beqz  $a0, .L7F0895EC
/* 0BBF60 7F089570 3C0A8007 */   lui   $t2, %hi(g_CurrentPlayer) # $t2, 0x8007
/* 0BBF64 7F089574 0FC07039 */  jal   die_blood_image_routine
/* 0BBF68 7F089578 00002025 */   move  $a0, $zero
/* 0BBF6C 7F08957C 0FC26669 */  jal   getPlayerCount
/* 0BBF70 7F089580 00000000 */   nop
/* 0BBF74 7F089584 24010001 */  li    $at, 1
/* 0BBF78 7F089588 14410013 */  bne   $v0, $at, .L7F0895D8
/* 0BBF7C 7F08958C 00000000 */   nop
/* 0BBF80 7F089590 0FC3028E */  jal   musicStopSlot
/* 0BBF84 7F089594 2404FFFF */   li    $a0, -1
/* 0BBF88 7F089598 0FC3003F */  jal   set_missionstate
/* 0BBF8C 7F08959C 00002025 */   move  $a0, $zero
/* 0BBF90 7F0895A0 0FC3002C */  jal   sub_GAME_7F0C0BF0
/* 0BBF94 7F0895A4 00000000 */   nop
/* 0BBF98 7F0895A8 0C001927 */  jal   musicTrack1ApplySeqpVol
/* 0BBF9C 7F0895AC 3044FFFF */   andi  $a0, $v0, 0xffff
/* 0BBFA0 7F0895B0 3C018002 */  lui   $at, %hi(g_musicXTrack1Fade) # $at, 0x8002
/* 0BBFA4 7F0895B4 AC2021DC */  sw    $zero, %lo(g_musicXTrack1Fade)($at)
/* 0BBFA8 7F0895B8 0C001A09 */  jal   musicTrack2ApplySeqpVol
/* 0BBFAC 7F0895BC 00002025 */   move  $a0, $zero
/* 0BBFB0 7F0895C0 3C018002 */  lui   $at, %hi(g_musicXTrack2Fade) # $at, 0x8002
/* 0BBFB4 7F0895C4 AC2021E0 */  sw    $zero, %lo(g_musicXTrack2Fade)($at)
/* 0BBFB8 7F0895C8 0C0018B7 */  jal   musicTrack1Play
/* 0BBFBC 7F0895CC 2404001B */   li    $a0, 27
/* 0BBFC0 7F0895D0 10000003 */  b     .L7F0895E0
/* 0BBFC4 7F0895D4 00000000 */   nop
.L7F0895D8:
/* 0BBFC8 7F0895D8 0FC3003F */  jal   set_missionstate
/* 0BBFCC 7F0895DC 24040006 */   li    $a0, 6
.L7F0895E0:
/* 0BBFD0 7F0895E0 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0BBFD4 7F0895E4 10000020 */  b     .L7F089668
/* 0BBFD8 7F0895E8 8C638BC0 */   lw    $v1, %lo(g_CurrentPlayer)($v1)
.L7F0895EC:
/* 0BBFDC 7F0895EC 8D4A8BC0 */  lw    $t2, %lo(g_CurrentPlayer)($t2)
/* 0BBFE0 7F0895F0 24040096 */  li    $a0, 150
/* 0BBFE4 7F0895F4 00002825 */  move  $a1, $zero
/* 0BBFE8 7F0895F8 8D4B0424 */  lw    $t3, 0x424($t2)
/* 0BBFEC 7F0895FC 00003025 */  move  $a2, $zero
/* 0BBFF0 7F089600 3C073F34 */  lui   $a3, (0x3F34B4B5 >> 16) # lui $a3, 0x3f34
/* 0BBFF4 7F089604 11600006 */  beqz  $t3, .L7F089620
/* 0BBFF8 7F089608 3C0C8004 */   lui   $t4, %hi(g_ClockTimer) # $t4, 0x8004
/* 0BBFFC 7F08960C 0FC20215 */  jal   currentPlayerSetFadeColour
/* 0BC000 7F089610 34E7B4B5 */   ori   $a3, (0x3F34B4B5 & 0xFFFF) # ori $a3, $a3, 0xb4b5
/* 0BC004 7F089614 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0BC008 7F089618 10000013 */  b     .L7F089668
/* 0BC00C 7F08961C 8C638BC0 */   lw    $v1, %lo(g_CurrentPlayer)($v1)
.L7F089620:
/* 0BC010 7F089620 8D8C0FF4 */  lw    $t4, %lo(g_ClockTimer)($t4)
/* 0BC014 7F089624 24040002 */  li    $a0, 2
/* 0BC018 7F089628 19800003 */  blez  $t4, .L7F089638
/* 0BC01C 7F08962C 00000000 */   nop
/* 0BC020 7F089630 10000001 */  b     .L7F089638
/* 0BC024 7F089634 24040001 */   li    $a0, 1
.L7F089638:
/* 0BC028 7F089638 0FC07039 */  jal   die_blood_image_routine
/* 0BC02C 7F08963C 00000000 */   nop
/* 0BC030 7F089640 10400004 */  beqz  $v0, .L7F089654
/* 0BC034 7F089644 3C0E8007 */   lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8007
/* 0BC038 7F089648 8DCE8BC0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0BC03C 7F08964C 240D0001 */  li    $t5, 1
/* 0BC040 7F089650 ADCD0424 */  sw    $t5, 0x424($t6)
.L7F089654:
/* 0BC044 7F089654 0FC07134 */  jal   gameplayBloodOverlayDL
/* 0BC048 7F089658 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BC04C 7F08965C 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0BC050 7F089660 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BC054 7F089664 AFA20058 */  sw    $v0, 0x58($sp)
.L7F089668:
/* 0BC058 7F089668 0FC1BDD7 */  jal   objecthandlerGetModelField28
/* 0BC05C 7F08966C 24640598 */   addiu $a0, $v1, 0x598
/* 0BC060 7F089670 3C048007 */  lui   $a0, %hi(g_CurrentPlayer) # $a0, 0x8007
/* 0BC064 7F089674 8C848BC0 */  lw    $a0, %lo(g_CurrentPlayer)($a0)
/* 0BC068 7F089678 E7A00024 */  swc1  $f0, 0x24($sp)
/* 0BC06C 7F08967C 0FC1BDD9 */  jal   sub_GAME_7F06F5C4
/* 0BC070 7F089680 24840598 */   addiu $a0, $a0, 0x598
/* 0BC074 7F089684 C7A40024 */  lwc1  $f4, 0x24($sp)
/* 0BC078 7F089688 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0BC07C 7F08968C 4604003E */  c.le.s $f0, $f4
/* 0BC080 7F089690 00000000 */  nop
/* 0BC084 7F089694 4500004A */  bc1f  .L7F0897C0
/* 0BC088 7F089698 00000000 */   nop
/* 0BC08C 7F08969C 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0BC090 7F0896A0 8C6F0424 */  lw    $t7, 0x424($v1)
/* 0BC094 7F0896A4 11E00046 */  beqz  $t7, .L7F0897C0
/* 0BC098 7F0896A8 00000000 */   nop
/* 0BC09C 7F0896AC 8C780428 */  lw    $t8, 0x428($v1)
/* 0BC0A0 7F0896B0 24190001 */  li    $t9, 1
/* 0BC0A4 7F0896B4 3C014270 */  li    $at, 0x42700000 # 60.000000
/* 0BC0A8 7F0896B8 1700000E */  bnez  $t8, .L7F0896F4
/* 0BC0AC 7F0896BC 00002825 */   move  $a1, $zero
/* 0BC0B0 7F0896C0 44816000 */  mtc1  $at, $f12
/* 0BC0B4 7F0896C4 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0BC0B8 7F0896C8 44813000 */  mtc1  $at, $f6
/* 0BC0BC 7F0896CC AC790428 */  sw    $t9, 0x428($v1)
/* 0BC0C0 7F0896D0 00003025 */  move  $a2, $zero
/* 0BC0C4 7F0896D4 00003825 */  move  $a3, $zero
/* 0BC0C8 7F0896D8 0FC20221 */  jal   currentPlayerAdjustFade
/* 0BC0CC 7F0896DC E7A60010 */   swc1  $f6, 0x10($sp)
/* 0BC0D0 7F0896E0 3C0142F0 */  li    $at, 0x42F00000 # 120.000000
/* 0BC0D4 7F0896E4 44816000 */  mtc1  $at, $f12
/* 0BC0D8 7F0896E8 44807000 */  mtc1  $zero, $f14
/* 0BC0DC 7F0896EC 0FC202AD */  jal   currentPlayerStartChrFade
/* 0BC0E0 7F0896F0 00000000 */   nop
.L7F0896F4:
/* 0BC0E4 7F0896F4 0FC2024C */  jal   currentPlayerIsFadeComplete
/* 0BC0E8 7F0896F8 00000000 */   nop
/* 0BC0EC 7F0896FC 10400030 */  beqz  $v0, .L7F0897C0
/* 0BC0F0 7F089700 00000000 */   nop
/* 0BC0F4 7F089704 0FC26669 */  jal   getPlayerCount
/* 0BC0F8 7F089708 00000000 */   nop
/* 0BC0FC 7F08970C 24010001 */  li    $at, 1
/* 0BC100 7F089710 14410005 */  bne   $v0, $at, .L7F089728
/* 0BC104 7F089714 00000000 */   nop
/* 0BC108 7F089718 0FC1EA94 */  jal   bondviewSetCameraMode
/* 0BC10C 7F08971C 24040005 */   li    $a0, 5
/* 0BC110 7F089720 10000027 */  b     .L7F0897C0
/* 0BC114 7F089724 00000000 */   nop
.L7F089728:
/* 0BC118 7F089728 0FC051B2 */  jal   get_scenario
/* 0BC11C 7F08972C 00000000 */   nop
/* 0BC120 7F089730 0FC269A4 */  jal   get_cur_playernum
/* 0BC124 7F089734 AFA20040 */   sw    $v0, 0x40($sp)
/* 0BC128 7F089738 0FC26669 */  jal   getPlayerCount
/* 0BC12C 7F08973C AFA2003C */   sw    $v0, 0x3c($sp)
/* 0BC130 7F089740 00402825 */  move  $a1, $v0
/* 0BC134 7F089744 00002025 */  move  $a0, $zero
/* 0BC138 7F089748 1840000C */  blez  $v0, .L7F08977C
/* 0BC13C 7F08974C 00001825 */   move  $v1, $zero
/* 0BC140 7F089750 8FA8003C */  lw    $t0, 0x3c($sp)
/* 0BC144 7F089754 3C0A8007 */  lui   $t2, %hi(g_playerPlayerData) # $t2, 0x8007
/* 0BC148 7F089758 254A8A00 */  addiu $t2, %lo(g_playerPlayerData) # addiu $t2, $t2, -0x7600
/* 0BC14C 7F08975C 00084880 */  sll   $t1, $t0, 2
/* 0BC150 7F089760 012A1021 */  addu  $v0, $t1, $t2
.L7F089764:
/* 0BC154 7F089764 8C4B0024 */  lw    $t3, 0x24($v0)
/* 0BC158 7F089768 24630001 */  addiu $v1, $v1, 1
/* 0BC15C 7F08976C 0065082A */  slt   $at, $v1, $a1
/* 0BC160 7F089770 24420070 */  addiu $v0, $v0, 0x70
/* 0BC164 7F089774 1420FFFB */  bnez  $at, .L7F089764
/* 0BC168 7F089778 008B2021 */   addu  $a0, $a0, $t3
.L7F08977C:
/* 0BC16C 7F08977C 8FAC0040 */  lw    $t4, 0x40($sp)
/* 0BC170 7F089780 24010001 */  li    $at, 1
/* 0BC174 7F089784 15810003 */  bne   $t4, $at, .L7F089794
/* 0BC178 7F089788 28810002 */   slti  $at, $a0, 2
/* 0BC17C 7F08978C 1020000C */  beqz  $at, .L7F0897C0
/* 0BC180 7F089790 00000000 */   nop
.L7F089794:
/* 0BC184 7F089794 0FC269A4 */  jal   get_cur_playernum
/* 0BC188 7F089798 00000000 */   nop
/* 0BC18C 7F08979C 00022600 */  sll   $a0, $v0, 0x18
/* 0BC190 7F0897A0 00046E03 */  sra   $t5, $a0, 0x18
/* 0BC194 7F0897A4 01A02025 */  move  $a0, $t5
/* 0BC198 7F0897A8 0C002C20 */  jal   joyGetButtons
/* 0BC19C 7F0897AC 3405B000 */   li    $a1, 45056
/* 0BC1A0 7F0897B0 10400003 */  beqz  $v0, .L7F0897C0
/* 0BC1A4 7F0897B4 00000000 */   nop
/* 0BC1A8 7F0897B8 0FC22294 */  jal   mp_respawn_handler
/* 0BC1AC 7F0897BC 00000000 */   nop
.L7F0897C0:
/* 0BC1B0 7F0897C0 0FC229C0 */  jal   bondviewIntroCameraTextTick
/* 0BC1B4 7F0897C4 00000000 */   nop
/* 0BC1B8 7F0897C8 0FC22A06 */  jal   sub_GAME_7F08A5FC
/* 0BC1BC 7F0897CC 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BC1C0 7F0897D0 0FC22B10 */  jal   bondviewUpperTextWindowTimerTick
/* 0BC1C4 7F0897D4 AFA20058 */   sw    $v0, 0x58($sp)
/* 0BC1C8 7F0897D8 0FC22B4C */  jal   sub_GAME_7F08AAE8
/* 0BC1CC 7F0897DC 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BC1D0 7F0897E0 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BC1D4 7F0897E4 0FC1AB5E */  jal   gunDrawSight
/* 0BC1D8 7F0897E8 27A40058 */   addiu $a0, $sp, 0x58
/* 0BC1DC 7F0897EC 0FC1A922 */  jal   generate_ammo_total_microcode
/* 0BC1E0 7F0897F0 8FA40058 */   lw    $a0, 0x58($sp)
/* 0BC1E4 7F0897F4 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BC1E8 7F0897F8 0FC1593C */  jal   countdownTimerRender
/* 0BC1EC 7F0897FC 00402025 */   move  $a0, $v0
/* 0BC1F0 7F089800 AFA20058 */  sw    $v0, 0x58($sp)
/* 0BC1F4 7F089804 0FC31568 */  jal   display_red_blue_on_radar
/* 0BC1F8 7F089808 00402025 */   move  $a0, $v0
/* 0BC1FC 7F08980C AFA20058 */  sw    $v0, 0x58($sp)
/* 0BC200 7F089810 0FC20161 */  jal   currentPlayerDrawFade
/* 0BC204 7F089814 00402025 */   move  $a0, $v0
/* 0BC208 7F089818 8FBF001C */  lw    $ra, 0x1c($sp)
.L7F08981C:
/* 0BC20C 7F08981C 27BD0058 */  addiu $sp, $sp, 0x58
/* 0BC210 7F089820 03E00008 */  jr    $ra
/* 0BC214 7F089824 00000000 */   nop
)
#endif
#endif





/**
 * Address 0x7F0896C0.
 */
Gfx *write_stan_tiles_in_yellow(Gfx *gdl)
{
    if (dword_CODE_bss_80079DA0 != NULL)
    {
        gdl = sub_GAME_7F0B3024(gdl, dword_CODE_bss_80079DA0, 0xFF00FF80U);
    }

    if (dword_CODE_bss_80079DA4 != NULL)
    {
        gdl = sub_GAME_7F0B3024(gdl, dword_CODE_bss_80079DA4, 0xFF00FF80U);
    }

    return gdl;
}


void sub_GAME_7F089718(f32 arg0)
{
    f32 scalar;
    struct collision434* col;

    scalar = D_800364D0 / arg0;

    col = &g_CurrentPlayer->field_488;
    col->collision_position.x *= scalar;
    col->collision_position.z *= scalar;

    D_800364D0 = arg0;
    D_800364D4 = 1.0f / arg0;
}


void sub_GAME_7F08976C(f32 param_1) {
  D_800364CC = param_1;
}


/**
 * Address 0x7F089778.
 */
f32 bondviewGetPlayerStanHeight(struct player *player)
{
    return player->stanHeight;
}


/**
 * Address 0x7F089780.
 */
f32 bondviewGetPlayerDuckingHeightRelated(struct player *player)
{
    return player->field_29BC + player->field_88 + player->ducking_height_offset;
}


PropRecord* getCurrentPlayerProp(void) {
    return g_CurrentPlayer->prop;
}


/**
 * Address 0x7F0897A8.
 */
void bondviewKillCurrentPlayer(void)
{
    if ((g_CurrentPlayer->cheatBondInvincible == 0) && (g_CurrentPlayer->bonddead == FALSE))
    {
        if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
        {
            trigger_solo_watch_menu(1);
        }

        g_isBondKIA = 1;
        g_CurrentPlayer->bonddead = 1;

        g_CurrentPlayer->previous_collision_info = g_CurrentPlayer->field_488;

        g_CurrentPlayer->thetadie = g_CurrentPlayer->vv_theta;
        g_CurrentPlayer->vertadie = g_CurrentPlayer->vv_verta;

        if (g_PlayerTankProp != NULL)
        {
            g_ExplodeTankOnDeathFlag = 1;
        }

        currentPlayerEquipWeaponWrapper(GUNLEFT, 0);
        currentPlayerEquipWeaponWrapper(GUNRIGHT, 0);

        if ((getMissiontimer() - g_CurrentPlayer->lifestarttime60) < g_playerPerm->shortest_inning)
        {
            g_playerPerm->shortest_inning = getMissiontimer() - g_CurrentPlayer->lifestarttime60;
        }

        g_CurrentPlayer->lifestarttime60 = getMissiontimer();
    }
}


/**
 * Unreferenced.
 *
 * Address 0x7F0898E8.
 */
s32 sub_GAME_7F0898E8(void)
{
    return (s32) ((joyGetStickY(0) * 8) + 0x280) / 0xA0;
}


/**
 * @param damage_amount: damage amount
 * @param vectorx: damage source x coordinate
 * @param vectorz: damage source y coordinate
 * @param playerid: player index of player causing the damage
 * @param arg4: boolean, does the damage apply to body armor (e.g. false when gas)
 *
 * Address US 7F08991C.
 * Address EU 7F089A84.
 * Address JP 7F089FF0.
 */
void record_damage_kills(f32 damage_amount, f32 vectorx, f32 vectorz, s32 playerid, s32 affects_armor) {
    f32 damage_dealt = g_playerPerm->handicap * damage_amount;
    s32 cur_player_num;
    f32 angle;
    s32 padding;
    s32 sp2C;
    s32 sp28;

    if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0)
    {
        hudMakeDamageSegments(g_CurrentPlayer->armor_display_values, 0x2E, 1, currentPlayerGetArmor());
        hudMakeDamageSegments(g_CurrentPlayer->health_display_values, 0x2E, -1, currentPlayerGetHealth());
    }

    if (getPlayerCount() < 2 || (g_stopPlayFlag == 0 && g_gameOverFlag == 0))
    {
        if (g_PlayerIsInTank == 1)
        {
            damage_dealt *= 0.25f;
        }

        if (g_CurrentPlayer->bonddead == FALSE && g_CurrentPlayer->cheatBondInvincible == FALSE)
        {
            joyRumblePakStart(get_cur_playernum(), 0.25);
            if (cur_player_get_control_type() >= 4)
            {
                // rumble second controller in 2.x
                joyRumblePakStart(get_cur_playernum() + getPlayerCount(), 0.25);
            }
        }

        if (getPlayerCount() >= 2 && get_scenario() == SCENARIO_LTK)
        {
            // the damage dealt is always equivalent to how much health and armor the player has
            // the result of this is to always kill the player regardless of how much damage he can sustain
            damage_dealt = (g_CurrentPlayer->bondhealth * g_CurrentPlayer->actual_health) + (g_CurrentPlayer->bondarmour * g_CurrentPlayer->actual_armor);
        }

        if (g_CurrentPlayer->cheatBondInvincible == FALSE && g_CurrentPlayer->bonddead == FALSE && g_PlayerInvincible == FALSE &&
            (g_CurrentPlayer->damageshowtime < 0 || (getPlayerCount() >= 2 && g_CurrentPlayer->damageshowtime == 0)))
        {
            if (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x5 && g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0xc)
            {
                g_CurrentPlayer->oldhealth = g_CurrentPlayer->bondhealth;
                g_CurrentPlayer->oldarmour = g_CurrentPlayer->bondarmour;

                if (getPlayerCount() >= 2)
                {
                    cur_player_num = get_cur_playernum();
                    angle = g_playerPointers[cur_player_num]->vv_theta - (360.0f - ((atan2f(vectorx, vectorz) * 180.0f) / 3.1415927f));

                    if (angle < 0.0f)
                    {
                        angle = -angle;
                    }

                    if (angle < 90.0f || angle > 270.0f)
                    {
                        // danger: if Bond could be damaged by toxic gas in multiplayer, playerid would be -1
                        // thus causing an out of bounds access
                        g_playerPlayerData[playerid].damage_to_backside++;
                    }
                }

                if (affects_armor && damage_dealt <= g_CurrentPlayer->bondarmour * g_CurrentPlayer->actual_armor)
                {
                    g_CurrentPlayer->bondarmour = g_CurrentPlayer->bondarmour - (damage_dealt / g_CurrentPlayer->actual_armor);
                }
                else
                {
                    if (affects_armor)
                    {
                        damage_dealt -= g_CurrentPlayer->bondarmour / g_CurrentPlayer->actual_armor;
                        g_CurrentPlayer->bondarmour = 0.0f;
                        g_CurrentPlayer->actual_armor = 1.0f;
                    }

                    g_CurrentPlayer->bondhealth = g_CurrentPlayer->bondhealth - (damage_dealt / g_CurrentPlayer->actual_health);

                    if (g_CurrentPlayer->bondhealth <= 0.0f)
                    {
                        if (getPlayerCount() >= 2)
                        {
                            sp2C = get_cur_playernum();
                            sp28 = 0;

                            if (get_scenario() == 3 && bondinvHasGoldenGun())
                            {
                                sp28 = 1;
                            }

#if defined(VERSION_EU) || defined(VERSION_JP)
                            drop_inventory();
#endif
                            if (sp2C != playerid)
                            {
#if defined(VERSION_US)
                                drop_inventory();
#endif
                                increment_num_deaths();
                            }

                            set_cur_player(playerid);

                            if (sp2C == playerid)
                            {
                                increment_num_suicides_display_MP();
                            }
                            else
                            {
                                increment_num_kills_display_text_in_MP();

                                if (sp28 != 0)
                                {
                                    increment_num_times_killed_MwtGC();
                                }
                            }

                            set_cur_player(sp2C);

                            if(1);

                            g_playerPlayerData[playerid].kill_counts[sp2C]++;
                        }

                        bondviewKillCurrentPlayer();
                    }
                }

#if defined(VERSION_EU) || defined(VERSION_JP)
    #define ZERO_7F08991C 0.0f
#else
    #define ZERO_7F08991C 0
#endif
                if (g_CurrentPlayer->damageshowtime < ZERO_7F08991C)
                {
                    g_CurrentPlayer->bondshotspeed.x = g_CurrentPlayer->bondshotspeed.x + 2.0f * vectorx;
                    g_CurrentPlayer->bondshotspeed.z = g_CurrentPlayer->bondshotspeed.z + 2.0f * vectorz;
                }

                g_CurrentPlayer->damageshowtime = ZERO_7F08991C;
                g_CurrentPlayer->healthshowtime = ZERO_7F08991C;

#undef ZERO_7F08991C

#if defined(VERSION_EU) || defined(VERSION_JP)
                if (!lvlGetControlsLockedFlag())
                {
                    sndPlaySfx(g_musicSfxBufferPtr, BOND_GET_HIT1_SFX, 0);
                }
#else
                sndPlaySfx(g_musicSfxBufferPtr, BOND_GET_HIT1_SFX, 0);
#endif
            }
        }
    }
}


/**
 * @param damage_amount: damage amount
 * @param rad:  damage source angle
 * @param player_id: player index of player causing the damage
 * @param affects_armor: boolean, does the damage apply to body armor (e.g. false when gas)
 *
 * Address 0x7F089E4C.
 */
void bondviewCallRecordDamageKills(f32 damage_amount, f32 angle, s32 playerid, s32 affects_armor)
{
    record_damage_kills(damage_amount, sinf(angle), cosf(angle), playerid, affects_armor);
}


int bondviewGetIfCurrentPlayerDamageShowTime(void)
{
    return (g_CurrentPlayer->damageshowtime >= (s32)0);
}


int bondviewGetIfCurrentPlayerHealthShowTime(void)
{
    return (g_CurrentPlayer->healthshowtime > (s32)0);
}


f32 bondviewGetBondBreathing(void)
{
    return g_CurrentPlayer->bondbreathing;
}


/**
 * Gets the current player's heading angle, converted from degrees to radians.
 * @return Heading (Yaw) in Radians
*/
f32 bondviewGetPlayerYawRadians(void)
{
    return DegToRad(360.0f - g_CurrentPlayer->vv_theta);
}


/**
 * Gets the current player's vertical look angle, converted from degrees to radians.
 * @return Pitch in radians.
 */
f32 bondviewGetPlayerPitchRadians(void)
{
    return DegToRad(g_CurrentPlayer->vv_verta);
}


s32 bond_pressed_reload_activate(void) {
    return g_CurrentPlayer->field_D0;
}


void set_bondata_invincible_flag(u32 arg0) {
    g_CurrentPlayer->cheatBondInvincible = arg0;
}


u8 get_bondata_invincible_flag(void) {
    return g_CurrentPlayer->cheatBondInvincible;
}


/**
 * Sets g_VisibleToGuardsFlag.
 * 1 = visible, 0 = not visible.
 */
void bondviewSetVisibleToGuardsFlag(s32 param_1)
{
  g_VisibleToGuardsFlag = param_1;
}

/**
 * Gets g_VisibleToGuardsFlag.
 * 1 = visible, 0 = not visible
 */
s32 bondviewGetVisibleToGuardsFlag(void)
{
    return g_VisibleToGuardsFlag;
}

void set_obj_collision_flag(s32 flag) {
  obj_collision_flag = flag;
}

s32 get_obj_collision_flag(void) {
    return obj_collision_flag;
}






/**
 * Address 0x7F089F98.
 */
u8 bondviewGetCurrentPlayersRoom(void)
{
    if ((g_CurrentPlayer->cameramode == 1) && (g_CurrentPlayer->room_pointer != 0))
    {
        return g_CurrentPlayer->room_pointer->room;
    }

    return g_CurrentPlayer->field_488.current_tile_ptr_for_portals->room;
}




/**
 * Address 0x7F089FD4.
 */
coord3d *bondviewGetCurrentPlayersPosition(void)
{
    if (g_CurrentPlayer->cameramode == 1)
    {
        return &g_CurrentPlayer->pos;
    }

    return &g_CurrentPlayer->field_488.pos;
}



coord3d * bondviewGetCurrentPlayersPosition3(void)
{

    if (g_CurrentPlayer->cameramode == 1)
    {
        return &g_CurrentPlayer->pos3;
    }

    return &g_CurrentPlayer->field_488.pos3;
}

struct coord3d *getCurrentPlayerPrevPos(void)
{
    return &g_CurrentPlayer->bondprevpos;
}


/**
 * Address 0x7F08A03C.
 */
void bondviewUpdateGuardTankFlagsRelated(PropRecord *prop, s32 flag)
{
    s32 playerIndex;

    playerIndex = getPlayerPointerIndex(prop);

    if (prop->chr != NULL)
    {
        chrSetMoving(prop->chr, flag);
    }

    if (g_PlayerTankProp != NULL)
    {
        // When commented out tank shells fired from the tank detonate immediately.
        sub_GAME_7F04F218(g_PlayerTankProp, flag);
    }

    g_playerPointers[playerIndex]->field_AC = flag;
}





/**
 * Address 0x7F08A0B0.
 */
void bondviewGetPropHeightRelatedValues(PropRecord *arg0, struct rect4f **field_B0, s32 *arg2, f32 *height_related, f32 *collision)
{
    s32 temp_v0;

    temp_v0 = getPlayerPointerIndex(arg0);
    if (g_playerPointers[temp_v0]->field_AC != 0)
    {
        // What is this doing and why is it 1 player only?
        if (getPlayerCount() == 1 || g_playerPointers[temp_v0]->bonddead == FALSE)
        {
            if (g_playerPointers[temp_v0]->cameramode != 1)
            {
                *arg2 = 4;
                *field_B0 = &g_playerPointers[temp_v0]->collision_bounds;
                *collision = g_playerPointers[temp_v0]->field_70;
                *height_related = *collision + bondviewGetPlayerDuckingHeightRelated(g_playerPointers[temp_v0]) + 10.0f;

                return;
            }
        }
    }

    *arg2 = 0;
}




/**
 * Address 0x7F08A19C.
 */
void bondviewUpdatePlayerCollisionBounds(void)
{

    if (g_PlayerIsInTank == 1)
    {
        bondviewGetTankCollisionBounds(&g_CurrentPlayer->collision_bounds, &g_CurrentPlayer->field_488.collision_position, g_TankOrientationAngle);

        return;
    }

    g_CurrentPlayer->collision_bounds.f[0] = (g_CurrentPlayer->field_488.collision_position.f[0] + g_CurrentPlayer->field_488.collision_radius);
    g_CurrentPlayer->collision_bounds.f[1] = g_CurrentPlayer->field_488.collision_position.f[2];
    g_CurrentPlayer->collision_bounds.f[2] = g_CurrentPlayer->field_488.collision_position.f[0];
    g_CurrentPlayer->collision_bounds.f[3] = (g_CurrentPlayer->field_488.collision_position.f[2] + g_CurrentPlayer->field_488.collision_radius);
    g_CurrentPlayer->collision_bounds.f[4] = (g_CurrentPlayer->field_488.collision_position.f[0] - g_CurrentPlayer->field_488.collision_radius);
    g_CurrentPlayer->collision_bounds.f[5] = g_CurrentPlayer->field_488.collision_position.f[2];
    g_CurrentPlayer->collision_bounds.f[6] = g_CurrentPlayer->field_488.collision_position.f[0];
    g_CurrentPlayer->collision_bounds.f[7] = (g_CurrentPlayer->field_488.collision_position.f[2] - g_CurrentPlayer->field_488.collision_radius);
}





/**
 * @param arg0: prop
 * @param width: out parameter, will be set to field_488.collision_radius
 * @param height: out parameter, will be set to character height - 30
 * @param always_30: out parameter, will be set to 30
 *
 * Address 0x7F08A274.
 */
void bondviewGetCollisionRadius(PropRecord* arg0, f32 *collision_radius, f32 *height, f32 *always_30)
{
    struct player **temp_v1;

    temp_v1 = &g_playerPointers[getPlayerPointerIndex(arg0)];
    *collision_radius = (*temp_v1)->field_488.collision_radius;
    *height = (bondviewGetPlayerDuckingHeightRelated(*temp_v1) + 10.0f) - 30.0f;
    *always_30 = 30.0f;
}





/**
 * Address 0x7F08A2EC.
 */
f32 currentPlayerGetHealth(void)
{
    return g_CurrentPlayer->bondhealth;
}


f32 currentPlayerGetArmor(void)
{
  return g_CurrentPlayer->bondarmour;
}





/**
 * Address 0x7F08A30C.
 */
void bondviewAddCurrentPlayerArmor(f32 arg0)
{
    g_playerPerm->body_armor_pickups += arg0;
    g_CurrentPlayer->bondarmour = arg0;
}





/**
 * Address 0x7F08A330.
 */
void bondviewResetIntroCameraMessageDialogs(void)
{
    g_CurrentPlayer->hudmessoff = FALSE;
    g_CurrentPlayer->bondmesscnt = -1;
    display_statusbar = 0;
    status_bar_text_buffer_index = 0;

#ifdef BUGFIX_R0
    copy_1stfonttable = ptrFontBankGothic;
    copy_2ndfonttable = ptrFontBankGothicChars;
#endif
}



void hudmsgsSetOn(s32 flag)
{
    g_CurrentPlayer->hudmessoff &= ~flag;
}





/**
 * Address 0x7F08A39C.
 */
void hudmsgsSetOff(s32 flags)
{
    g_CurrentPlayer->hudmessoff |= flags;
}


#ifdef VERSION_US
void setFontTables(s32 arg0, s32 arg1)
{
    copy_2ndfonttable = arg0;
    copy_1stfonttable = arg1;
}
#endif


#ifdef BUGFIX_R1
void hudmsgBottomShow(char *string, s32 font, s32 arg2)
{
    s32 abs_index;
    s32 index;
    if (getPlayerCount() == 1)
    {
        if (display_statusbar < 5)
        {
            abs_index = status_bar_text_buffer_index + display_statusbar;
            index = abs_index % 5;
            abs_index = index;
            strncpy(stringbuffer_lowerleft[abs_index], string, (BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH-1));
            stringbuffer_lowerleft[abs_index][(BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH-1)] = 0;
            dword_CODE_bss_jp80079CEC[abs_index] = font;
            dword_CODE_bss_jp80079Cd8[abs_index] = arg2;
            display_statusbar++;
        }
    }
    else
    {
        index = get_cur_playernum();
        strncpy(stringbuffer_lowerleft[index], string, (BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH-1));
        stringbuffer_lowerleft[index][(BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH-1)] = 0;
        dword_CODE_bss_jp80079CEC[index] = font;
        dword_CODE_bss_jp80079Cd8[index] = arg2;
#if defined(VERSION_EU)
        g_CurrentPlayer->bondmesscnt = 0x64;
#elif defined(VERSION_JP)
        g_CurrentPlayer->bondmesscnt = 0x78;
#endif
    }
}

#else
#ifdef DEBUG
void hudmsgBottomShow(char *mess, void *font)
#else
void hudmsgBottomShow(char *mess)
#endif
{
    s32 abs_index;
    s32 index;
    #ifdef DEBUG
        assert(font);
        assert(strlen(mess)<=MAXMESSAGELEN);
    #endif
    if (getPlayerCount() == 1)
    {
        if (display_statusbar < 5)
        {
            abs_index = status_bar_text_buffer_index + display_statusbar;
            index = abs_index % 5;
            strncpy(stringbuffer_lowerleft[index], mess, MAXMESSAGELEN);
            display_statusbar++;
            stringbuffer_lowerleft[index][MAXMESSAGELEN] = 0;
        }
    }
    else
    {
        index = get_cur_playernum();
        strncpy(stringbuffer_lowerleft[index], mess, MAXMESSAGELEN);
        stringbuffer_lowerleft[index][MAXMESSAGELEN] = 0;
        g_CurrentPlayer->bondmesscnt = 0x78;
    }
}

#endif


#if defined(BUGFIX_R1)
void jp_hudmsgBottomShow(char *string)
{
    hudmsgBottomShow(string, ptrFontBankGothicChars, ptrFontBankGothic);
}
#endif


/**
 * Address 0x7F08A4E4.
 */
void bondviewIntroCameraTextTick(void)
{
    if ((g_CurrentPlayer->hudmessoff == FALSE) && (g_CurrentPlayer->mpmenuon == FALSE))
    {
        if (g_CurrentPlayer->bondmesscnt >= 0)
        {
            g_CurrentPlayer->bondmesscnt -= g_ClockTimer;

            if (getPlayerCount() == 1)
            {
                if (g_CurrentPlayer->bondmesscnt < 0)
                {
                    status_bar_text_buffer_index = (s32) (status_bar_text_buffer_index + 1) % 5;
                    display_statusbar = display_statusbar - 1;
                }
                else if ((display_statusbar >= 2) && (g_CurrentPlayer->bondmesscnt >= BONDVIEW_INTRO_CAMERA_BONDMESSCNT_A))
                {
                    g_CurrentPlayer->bondmesscnt = BONDVIEW_INTRO_CAMERA_BONDMESSCNT_B;
                }
            }
        }

        if ((getPlayerCount() == 1) && (g_CurrentPlayer->bondmesscnt < 0) && (display_statusbar > 0))
        {
            if (display_statusbar >= 2)
            {
                g_CurrentPlayer->bondmesscnt = BONDVIEW_INTRO_CAMERA_BONDMESSCNT_B;
            }
            else
            {
                g_CurrentPlayer->bondmesscnt = BONDVIEW_INTRO_CAMERA_BONDMESSCNT_C;
            }
        }
    }
}

Gfx* sub_GAME_7F08A5FC(Gfx* arg0)
{
    s32 var_v1;
    s32 view_left;
    s32 view_vert;
    s32 view_horiz;
    s32 view_top;
    s32 view_top_offset;
    s32 view_left_offset;

    if ((g_CurrentPlayer->hudmessoff == FALSE) && (g_CurrentPlayer->bondmesscnt >= 0) && (g_CurrentPlayer->mpmenuon == FALSE))
    {
        var_v1 = 0;
        if (getPlayerCount() == 1)
        {
            if ((u8) *stringbuffer_lowerleft[status_bar_text_buffer_index] != 0)
            {
                var_v1 = 1;
            }
        }
        else if (g_CurrentPlayer->bondmesscnt >= 0)
        {
            status_bar_text_buffer_index = get_cur_playernum();
            var_v1 = 1;
        }

        if (var_v1 != 0)
        {
            arg0 = microcode_constructor(arg0);
            view_left_offset = 0;
            view_top_offset = 0;
            textMeasure(&view_top_offset, &view_left_offset ,(u8* ) stringbuffer_lowerleft[status_bar_text_buffer_index], BONDVIEW_2ND_FONTTABLE(status_bar_text_buffer_index), BONDVIEW_1ST_FONTTABLE(status_bar_text_buffer_index), 0);

            if (getPlayerCount() < 3)
            {
                view_left = viGetViewLeft() + 0x1E;
            }
            else if (get_cur_playernum() & 1)
            {
                view_left = viGetViewLeft() + 0xA;
            }
            else
            {
                view_left = viGetViewLeft() + 0x1E;
            }

            view_horiz = view_left + view_left_offset;

            if (getPlayerCount() < 3)
            {
                if ((get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNLEFT)) == 0) && (is_clock_drawn_onscreen() == 0))
                {
                    view_top = (viGetViewTop() + viGetViewHeight()) - BONDVIEW_VIEW_TOP_OFFSET_1;
                }
                else
                {
                    view_top = (viGetViewTop() + viGetViewHeight()) - BONDVIEW_VIEW_TOP_OFFSET_2;
                }
#if !defined(VERSION_EU)
                if (get_cur_playernum() == 1)
                {
                    view_top -= 8;
                }
#endif
            }
            else
            {
                view_top = viGetViewTop()
#if defined(VERSION_JP) || defined(VERSION_EU)
                         + (((j_text_trigger != 0) && (get_cur_playernum() < 2)) ? 8 : 0)
#endif
                         + BONDVIEW_VIEW_TOP_OFFSET_3;
            }

            view_vert = view_top - view_top_offset;
            arg0 = draw_blackbox_to_screen(arg0, (s32) &view_left, (s32) &view_vert, (s32) &view_horiz, (s32) &view_top);
            arg0 = combiner_bayer_lod_perspective(textRenderOutlined(arg0, &view_left, &view_vert, stringbuffer_lowerleft[status_bar_text_buffer_index], BONDVIEW_2ND_FONTTABLE(status_bar_text_buffer_index), BONDVIEW_1ST_FONTTABLE(status_bar_text_buffer_index), -1, 0x646464FFU, (s16) (s32) viGetX(), (s16) viGetY(), 0, 0));
        }
    }

    return arg0;
}


void bondviewResetUpperTextDisplay(void)
{
    upper_text_window_timer = -1;
    display_upper_text_window = 0;
    upper_text_buffer_index = 0;
    g_UpperTextDisplayFlag = 0;
}


void bondviewClearUpperTextDisplayFlag(int param_1)
{
  int new_var;
  new_var = ~param_1;
  g_UpperTextDisplayFlag = g_UpperTextDisplayFlag & new_var;
}


void bondviewSetUpperTextDisplayFlag(PLAYERFLAG flag)
{
    g_UpperTextDisplayFlag |= flag;
}


void hudmsgTopShow(char* mess)
{
    s32 index;
    #ifdef DEBUG
        assert(strlen(mess)<=MAXTALKMESSLEN);
    #endif
    if (display_upper_text_window >= 2) { return; }

    index = (upper_text_buffer_index + display_upper_text_window) % 2;
#if defined(LEFTOVERDEBUG)
    strncpy(stringbuffer_top[index], mess, (BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH-1));
    display_upper_text_window += 1;
    stringbuffer_top[index][(BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH-1)] = 0;
#else
    strncpy(dword_CODE_bss_80079DC8[index], mess, (BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH-1));
    display_upper_text_window += 1;
    dword_CODE_bss_80079DC8[index][(BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH-1)] = 0;
#endif
}


/**
 * Address 0x7F08A9F8.
 */
void bondviewUpperTextWindowTimerTick(void)
{
    if ((g_UpperTextDisplayFlag == FALSE) && (g_CurrentPlayer->mpmenuon == FALSE))
    {
        if (upper_text_window_timer >= 0)
        {
            upper_text_window_timer -= g_ClockTimer;

            if (upper_text_window_timer < 0)
            {
                upper_text_buffer_index = (s32) (upper_text_buffer_index + 1) % 2;
                display_upper_text_window += -1;
            }
            else if ((display_upper_text_window >= 2) && (upper_text_window_timer >= BONDVIEW_UPPER_TEXT_TIMER_A))
            {
                upper_text_window_timer = BONDVIEW_UPPER_TEXT_TIMER_B;
            }
        }

        if ((upper_text_window_timer < 0) && (display_upper_text_window > 0))
        {
            if (display_upper_text_window >= 2)
            {
                upper_text_window_timer = BONDVIEW_UPPER_TEXT_TIMER_B;
            }
            else
            {
                upper_text_window_timer = BONDVIEW_UPPER_TEXT_TIMER_C;
            }
        }
    }

}


extern char a8s[];    /* "%8s"     */
extern char aX4_0f[]; /* "x %4.0f" */
extern char aY4_0f[]; /* "y %4.0f" */
extern char aZ4_0f[]; /* "z %4.0f" */
extern char aS3d[];   /* "%s %3d"  */


Gfx *sub_GAME_7F08AAE8(Gfx *gdl)
{
    TopMessageLocals msg;
    DebugTextBuffers debugtext;
    s32 debug_x;
    s32 debug_y;
    f32 theta_x;
    s32 debug_boxbottom;
    s32 pad;
    s32 *roomid;
    s32 debug_angle;
    DirectionLabels directions;

    struct
    {
        s16 screenwidth;
        s16 pad;
    } sw;

    if (g_UpperTextDisplayFlag == 0)
    {
        if (upper_text_window_timer >= 0)
        {
#if defined(LEFTOVERDEBUG)
            if (stringbuffer_top[upper_text_buffer_index][0] != '\0')
#else
            if (dword_CODE_bss_80079DC8[upper_text_buffer_index][0] != '\0')
#endif
            {
                if (g_CurrentPlayer->mpmenuon == 0)
                {
                    gdl = microcode_constructor(gdl);
                    msg.textwidth = 0;
                    msg.textheight = 0;
#if defined(LEFTOVERDEBUG)
                    textMeasure(&msg.textheight, &msg.textwidth, stringbuffer_top[upper_text_buffer_index], ptrFontZurichBoldChars, ptrFontZurichBold, 0);
#else
                    textMeasure(&msg.textheight, &msg.textwidth, dword_CODE_bss_80079DC8[upper_text_buffer_index], ptrFontZurichBoldChars, ptrFontZurichBold, 0);
#endif
                    if (cameraBufferToggle != 0)
                    {
                        msg.x = viGetViewLeft() + 0x46;
                        msg.y = viGetViewTop() + 0x10;
                        msg.y += 0x10;
                        msg.y = msg.y / 11;
                        msg.y *= 11;
                        msg.y -= 2;
                    }
                    else
                    {
                        msg.x = viGetViewLeft() + 0x1e;
#ifdef VERSION_EU
                        msg.y = viGetViewTop() + 0x10;
#else
                        msg.y = viGetViewTop() + 0xd;
#endif
                    }

                    msg.bottom = msg.y + msg.textheight;
                    gdl = microcode_constructor_related_to_menus(gdl, 0, msg.y - 2, viGetX(), msg.bottom, 0x64);
#ifdef VERSION_US
                    sw.screenwidth = viGetX();
                    gdl = textRender(gdl, &msg.x, &msg.y, stringbuffer_top[upper_text_buffer_index], ptrFontZurichBoldChars, ptrFontZurichBold, -1, sw.screenwidth, viGetY(), 0, 0);
#else
                    if (j_text_trigger != 0)
                    {
                        sw.screenwidth = viGetX();
#if defined(LEFTOVERDEBUG)
                        gdl = textRenderOutlined(gdl, &msg.x, &msg.y, stringbuffer_top[upper_text_buffer_index], ptrFontZurichBoldChars, ptrFontZurichBold, -1, 0x646464FF, sw.screenwidth, viGetY(), 0, 0);
#else
                        gdl = textRenderOutlined(gdl, &msg.x, &msg.y, dword_CODE_bss_80079DC8[upper_text_buffer_index], ptrFontZurichBoldChars, ptrFontZurichBold, -1, 0x646464FF, sw.screenwidth, viGetY(), 0, 0);
#endif
                    }
                    else
                    {
                        sw.screenwidth = viGetX();
#if defined(LEFTOVERDEBUG)
                        gdl = textRender(gdl, &msg.x, &msg.y, stringbuffer_top[upper_text_buffer_index], ptrFontZurichBoldChars, ptrFontZurichBold, -1, sw.screenwidth, viGetY(), 0, 0);
#else
                        gdl = textRender(gdl, &msg.x, &msg.y, dword_CODE_bss_80079DC8[upper_text_buffer_index], ptrFontZurichBoldChars, ptrFontZurichBold, -1, sw.screenwidth, viGetY(), 0, 0);
#endif
                    }
#endif
                    gdl = combiner_bayer_lod_perspective(gdl);
                    goto end;
                }
            }
        }
    }

    if (get_debug_testingmanpos_flag() != 0)
    {
        theta_x = g_CurrentPlayer->field_488.theta_transform.x;
        debug_angle = (s32) ((atan2f(-theta_x, g_CurrentPlayer->field_488.theta_transform.z) * 180.0f) / M_PI_F);
        directions = D_800368B8;
        roomid = bgDebPrintROOMID(g_CurrentPlayer->field_488.current_tile_ptr->room);

        sprintf(debugtext.room, a8s, roomid);
        sprintf(debugtext.x, aX4_0f, g_CurrentPlayer->field_488.collision_position.x);
        sprintf(debugtext.y, aY4_0f, g_CurrentPlayer->field_488.collision_position.y);
        sprintf(debugtext.z, aZ4_0f, g_CurrentPlayer->field_488.collision_position.z);
        sprintf(debugtext.angle, aS3d, ((char *) (&directions)) + (((debug_angle + 0x16) / 0x2d) * 3), debug_angle);

        debug_x = viGetViewLeft() + 0x11;
        debug_y = viGetViewTop() + 0x11;
        debug_boxbottom = debug_y + 0xa;
        gdl = microcode_constructor(gdl);
        gdl = microcode_constructor_related_to_menus(gdl, 0, debug_y, viGetX(), debug_boxbottom + 1, 0x64);
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.room, ptrFontBankGothicChars, ptrFontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0x57;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.x, ptrFontBankGothicChars, ptrFontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0x8d;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.y, ptrFontBankGothicChars, ptrFontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0xc3;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.z, ptrFontBankGothicChars, ptrFontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0xf9;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.angle, ptrFontBankGothicChars, ptrFontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        gdl = combiner_bayer_lod_perspective(gdl);
    }

    end:
    return gdl;
}


/*
* Address: 0x7F08B0F0
*/
s32 playerTickBeams(PropRecord *prop)
{
    s32 index;
    ChrRecord *chr;
    s32 group;
    s32 ret;
    s32 sub;
    PropRecord *leftprop;
    f32 mtx[15];
    s32 tailret;
    s32 anim;
    f32 angle;
    f32 frame;
    f32 local90;
    f32 local8c;
    f32 local88;
    f32 fwd;
    f32 startframe;
    struct weapon_firing_animation_table *firingtable;
    coord3d off;
    s32 i;
    s32 found;
    s32 cur;
    PropRecord *rightprop;
    struct WeaponObjRecord *leftobj;
    struct WeaponObjRecord *rightobj;
    s32 setanim;
    struct player **ppointers;
 
    index = getPlayerPointerIndex(prop);
    chr = prop->chr;
 
    if (chr != NULL)
    {
        if (get_player_position_in_shuffled(get_cur_playernum()) == 0)
        {
            chr->hidden &= ~CHRHIDDEN_FREEZE;
        }
    }
 
    if (chr != NULL)
    {
        if ((g_playerPointers[index]->ptr_char_objectinstance != NULL) && (!(get_debug_render_raster() && (g_playerPointers[index]->cameramode != 1))))
        {
            g_playerPointers[index]->field_AC = 0;
            ret = chrTickBeams(prop);
            g_playerPointers[index]->field_AC = 1;
 
            g_playerPointers[index]->field_488.collision_position.x = g_playerPointers[index]->prop->pos.x;
            g_playerPointers[index]->field_488.collision_position.y = g_playerPointers[index]->prop->pos.y;
            g_playerPointers[index]->field_488.collision_position.z = g_playerPointers[index]->prop->pos.z;
            g_playerPointers[index]->field_488.current_tile_ptr = g_playerPointers[index]->prop->stan;
            sub_GAME_7F07DE9C(g_playerPointers[index]);
 
            if (prop->flags & PROPFLAG_ONSCREEN)
            {
                RenderPosView *rp = g_playerPointers[index]->ptr_char_objectinstance->render_pos;
                matrix_4x4_multiply_homogeneous(currentPlayerGetViewToWorldMtxf(), (Mtxf *) rp, (Mtxf *) mtx);
                g_playerPointers[index]->field_488.pos.x = mtx[12] + (mtx[4] * 7.0f);
                g_playerPointers[index]->field_488.pos.y = mtx[13] + (mtx[5] * 7.0f);
                g_playerPointers[index]->field_488.pos.z = mtx[14] + (mtx[6] * 7.0f);
            }
 
            return ret;
        }
    }
 
    if (chr == NULL)
    {
        goto clear_and_return;
    }

    if (g_playerPointers[index]->ptr_char_objectinstance == NULL)
    {
        goto clear_and_return;
    }

    if (getPlayerCount() < 2)
    {
        goto clear_and_return;
    }

    if (get_cur_playernum() == index)
    {
        goto clear_and_return;
    }
 
    anim = 0;
    firingtable = NULL;
    local90 = -1.0f;
    frame = -1;
    leftprop = chrGetEquippedWeaponProp(chr, GUNLEFT);
    rightprop = chrGetEquippedWeaponProp(chr, GUNRIGHT);
    leftobj = NULL;
    rightobj = NULL;
    setanim = 0;
 
    if (leftprop != NULL)
    {
        leftobj = leftprop->weapon;
    }
    
    if (rightprop != NULL)
    {
        rightobj = rightprop->weapon;
    }
 
    ppointers = g_playerPointers;
 
    if (get_player_position_in_shuffled(get_cur_playernum()) == 0)
    {
        *((s32 *) D_800368B8_tail) = (*((s32 *) D_800368B8_tail)) + 1;
    }
 
    if ((*((s32 *) D_800368B8_tail)) >= 2)
    {
        local8c = ((0, ppointers[index]))->field_2A08;
        local88 = ppointers[index]->field_2A0C;
 
        if (ppointers[index]->bonddead != FALSE)
        {
            found = 0;
 
            for (i = 0; i < g_bondviewBondDeathAnimationsCount; i++)
            {
                cur = ppointers[index]->players_cur_animation;
 
                if (cur == (g_bondviewBondDeathAnimations[i] + ((s32) ptr_animation_table)))
                {
                    found = 1;
                }
            }
 
            if (found)
            {
                anim = ppointers[index]->players_cur_animation;
                angle = 0.5f;
            }
            else
            {
                anim = g_bondviewBondDeathAnimations[randomGetNext() % g_bondviewBondDeathAnimationsCount] + ((s32) ptr_animation_table);
                angle = 0.5f;
            }
 
            cur = ppointers[index]->players_cur_animation;
            local8c = 0.0f;
            local88 = 0.0f;
            goto join_768;
        }
 
        if ((leftprop != NULL) && (rightprop != NULL))
        {
            group = 3;
        }
        else if ((leftprop == NULL) && (rightprop == NULL))
        {
            group = 2;
        }
        else if ((leftobj != NULL) && (bondwalkItemCheckBitflags(leftobj->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0))
        {
            group = 2;
        }
        else if ((rightobj != NULL) && (bondwalkItemCheckBitflags(rightobj->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0))
        {
            group = 2;
        }
        else if ((leftobj != NULL) && (bondwalkItemCheckBitflags(leftobj->weaponnum, WEAPONSTATBITFLAG_ONLY_1_HANDED) != 0))
        {
            group = 0;
        }
        else if ((rightobj != NULL) && (bondwalkItemCheckBitflags(rightobj->weaponnum, WEAPONSTATBITFLAG_ONLY_1_HANDED) != 0))
        {
            group = 0;
        }
        else
        {
            group = 1;
        }
 
        if (playerGetCrouchPos(index) == 1)
        {
            goto set_crouch_lean;
        }
        else if (playerGetCrouchPos(index) == 0)
        {
set_crouch_lean:
            angle = 1.0f;
            sub = 5;
        }
        else if ((ppointers[index]->speedsideways < 0.0f) && (firing_animation_groups[group][4].pointer != NULL))
        {
            sub = 4;
            angle = -ppointers[index]->speedsideways;
 
            if (ppointers[index]->field_1280 < 90.0f)
            {
                ppointers[index]->field_1280 = ppointers[index]->field_1280 + 15.0f;
            }
        }
        else if ((ppointers[index]->speedsideways > 0.0f) && (firing_animation_groups[group][3].pointer != NULL))
        {
            sub = 3;
            angle = ppointers[index]->speedsideways;
 
            if (ppointers[index]->field_1280 > (-90.0f))
            {
                ppointers[index]->field_1280 = ppointers[index]->field_1280 - 15.0f;
            }
        }
        else
        {
            frame = ppointers[index]->speedtheta;
 
            if (frame < 0.0f)
            {
                frame = -frame;
            }
 
            fwd = ppointers[index]->speedforwards;
 
            if (fwd < -0.050000001f)
            {
                frame = -frame;
 
                if (fwd < frame)
                {
                    frame = fwd;
                }
 
                if (ppointers[index]->headanim == 0)
                {
                    goto shared_double_neg;
                }
 
                sub = 2;
 
                if (-0.40000001f < fwd)
                {
shared_double_neg:
                    angle = frame + frame;
                    sub = 1;
 
                    if (angle < (-1.0f))
                    {
                        angle = -1.0f;
                    }
                }
                else
                {
                    angle = frame;
                    goto lean_return_to_centre;
                }
            }
            else
            {
                if (0.050000001f < fwd)
                {
                    goto shared_framefwd;
                }
                else if (0.050000001f < frame)
                {
shared_framefwd:
                    if (frame < fwd)
                    {
                        frame = fwd;
                    }
                }
                else
                {
                    goto set_full_lean;
                }
 
                if (ppointers[index]->headanim == 0)
                {
                    goto shared_double_pos;
                }
 
                sub = 2;
 
                if (fwd < 0.40000001f)
                {
shared_double_pos:
                    angle = frame + frame;
                    sub = 1;
 
                    if (1.0f < angle)
                    {
                        angle = 1.0f;
                    }
                }
                else
                {
                    angle = frame;
                    goto lean_return_to_centre;
                }
            }
 
            goto lean_return_to_centre;
 
set_full_lean:
            angle = 1.0f;
            sub = 0;
 
lean_return_to_centre:
            if (0.0f < ppointers[index]->field_1280)
            {
                ppointers[index]->field_1280 = ppointers[index]->field_1280 - 15.0f;
            }
 
            if (ppointers[index]->field_1280 < 0.0f)
            {
                ppointers[index]->field_1280 = ppointers[index]->field_1280 + 15.0f;
            }
        }
 
        {
            struct firing_anim_struct *fa = &firing_animation_groups[group][sub];
 
            firingtable = fa->pointer;
 
            if (fa->anim != 0)
            {
                anim = fa->anim + (s32) ptr_animation_table;
            }
 
            angle *= fa->x;
            local90 = fa->z;
            frame = fa->y;
        }
 
        cur = ppointers[index]->players_cur_animation;
 
join_768:
        if ((firingtable != NULL) && (anim == 0))
        {
            anim = *((s32 *) firingtable);
        }
 
        if (anim != cur)
        {
            setanim = 1;
        }
 
        if (0.0f <= frame)
        {
            if ((ppointers[index]->ptr_char_objectinstance->animlooping == 0) || (frame != ppointers[index]->ptr_char_objectinstance->animloopframe))
            {
                setanim = 1;
            }
        }
 
        if (frame < 0.0f)
        {
            if (ppointers[index]->ptr_char_objectinstance->animlooping)
            {
                setanim = 1;
            }
        }
 
        if (setanim != 0)
        {
            if (ppointers[index]->ptr_char_objectinstance->anim2 == NULL)
            {
                startframe = (0.0f <= frame) ? (frame) : (0.0f);
                modelSetAnimation(ppointers[index]->ptr_char_objectinstance, (ModelAnimation *) anim, 0, startframe, angle, 16.0f);
                ppointers[index]->players_cur_animation = anim;
                ppointers[index]->field_1288 = angle;
 
                if (0.0f <= frame)
                {
                    modelSetAnimLooping(ppointers[index]->ptr_char_objectinstance, frame, 16.0f);
                }
 
                if (0.0f <= local90)
                {
                    modelSetAnimEndFrame(ppointers[index]->ptr_char_objectinstance, local90);
                }
            }
 
            cur = ppointers[index]->players_cur_animation;
        }
        else
        {
            if (angle != ppointers[index]->field_1288)
            {
                modelSetAnimSpeed(ppointers[index]->ptr_char_objectinstance, angle, 1.0f);
                ppointers[index]->field_1288 = angle;
            }
 
            cur = ppointers[index]->players_cur_animation;
        }
 
        if (anim == cur)
        {
            if (firingtable != NULL)
            {
                chr->hidden &= ~CHRHIDDEN_0400;
                chrlvUpdateAimendbackShoulders(chr, firingtable, 0, 1, local8c);
            }
            else
            {
                chr->aimendrshoulder = 0.0f;
                chr->aimendlshoulder = 0.0f;
                chr->hidden |= CHRHIDDEN_0400;
                chr->aimendback = local8c;
            }
        }
 
        chr->aimendsideback = local88;
        chr->aimendcount = 10;
    }
 
    prop->pos.x = ppointers[index]->field_488.collision_position.x;
    prop->pos.y = ppointers[index]->field_488.collision_position.y;
    prop->pos.z = ppointers[index]->field_488.collision_position.z;
    prop->stan = ppointers[index]->field_488.current_tile_ptr;
 
    getsuboffset(chr->model, &off);
    off.x = prop->pos.x;
    off.z = prop->pos.z;
    setsuboffset(chr->model, &off);
    setsubroty(chr->model, (((360.0f - ppointers[index]->vv_theta) + ppointers[index]->field_1280) * M_TAU_F) / 360.0f);
 
    chr->chrflags |= CHRFLAG_INIT;
    chr->actiontype = ACT_BONDMULTI;
    chr->act_bondmulti.unk2c = (f32 *) firingtable;
 
    chrSetFiring(chr, GUNRIGHT, ppointers[index]->hands[GUNRIGHT].field_87D);
    chrSetFiring(chr, GUNLEFT, ppointers[index]->hands[GUNLEFT].field_87D);
 
    tailret = chrTickBeams(prop);
 
    for (i = 0; i != 2; i++)
    {
        if (sub_GAME_7F02D630(chr, i, &ppointers[index]->field_2A18[i]) != 0)
        {
            (&ppointers[index]->field_2A30)[i] = D_80048380;
        }
        else if ((&ppointers[index]->field_2A30)[i] < (D_80048380 - 1))
        {
            ppointers[index]->field_2A18[i].x = ppointers[index]->hands[i].field_B58.x;
            ppointers[index]->field_2A18[i].y = ppointers[index]->hands[i].field_B58.y;
            ppointers[index]->field_2A18[i].z = ppointers[index]->hands[i].field_B58.z;
        }
    }
 
    chr->hidden |= CHRHIDDEN_FREEZE;
 
    prop->pos.x = ppointers[index]->field_488.collision_position.x;
    prop->pos.y = ppointers[index]->field_488.collision_position.y;
    prop->pos.z = ppointers[index]->field_488.collision_position.z;
    prop->stan = ppointers[index]->field_488.current_tile_ptr;
 
    return tailret;
 
clear_and_return:
    prop->flags &= ~PROPFLAG_ONSCREEN;
    return 0;
}


/**
 * Address 0x7F08BCB8.
 */
Gfx * bondviewRemoved7F08BCB8(Gfx *arg0)
{
    #ifdef DEBUG
    // removed
    #endif

    return arg0;
}





/**
 * Address 0x7F08BCC0.
 */
Gfx *bondviewRenderProp(PropRecord *arg0, Gfx *arg1, s32 arg2)
{
    if (arg0->chr != NULL)
    {
        arg1 = chrRenderProp(arg0, arg1, arg2);
    }

    return arg1;
}





/**
 * Address 0x7F08BCF4.
 */
Gfx* bondviewGfxPlayerField5cMatrix(Gfx* gdl)
{
    gSPMatrix(gdl++, g_CurrentPlayer->field_5C, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    return gdl;
}





/**
 * Unreferenced.
 *
 * Address 0x7F08BD18.
 */
void bondviewTransformPosToViewMatrix(RenderPosView *arg0)
{
    Mtxf sp18;

    matrix_4x4_copy(&arg0->pos, (Mtxf *) &sp18);
    matrix_4x4_f32_to_s32((Mtxf *) &sp18, &arg0->view);
}



/**
 * Address 0x7F08BD48.
 *
 * Notes: Similar to sub_GAME_7F08BE2C.
 *
 */
void bondviewTransformManyPosToViewMatrix(RenderPosView * arg0, s32 arg1)
{
    Mtxf mtx;
    RenderPosView* rpv_entry;
    s32 i;

    i = 0;
    if (arg1 <= 0) { return; }

    // Couldn't find a better matching loop
    rpv_entry = arg0;
    do
    {
        matrix_4x4_copy(&rpv_entry->pos, &mtx);
        matrix_4x4_f32_to_s32(&mtx, &arg0[i].pos);
        i++;
        rpv_entry++;
    } while (i != arg1);
}



/**
 * Unreferenced.
 *
 * Address 0x7F08BDC4.
 */
void sub_GAME_7F08BDC4(Mtxf *arg0)
{
    Mtxf sp20;

    matrix_4x4_copy(arg0, (Mtxf *) &sp20);
    sp20.m[3][0] -= g_CurrentPlayer->previous_model_pos.f[0];
    sp20.m[3][1] -= g_CurrentPlayer->previous_model_pos.f[1];
    sp20.m[3][2] -= g_CurrentPlayer->previous_model_pos.f[2];
    matrix_4x4_f32_to_s32((Mtxf *) &sp20, arg0);
}


/**
 * Unreferenced.
 *
 * Address 0x7F08BE2C.
 */
void sub_GAME_7F08BE2C(Mtxf *matrices, s32 count)
{
    Mtxf copy;
    s32 i;

    for (i = 0; i < count; i++)
    {
        matrix_4x4_copy((Mtxf *)((uintptr_t)matrices + i * sizeof(Mtxf)), &copy);

        copy.m[3][0] -= g_CurrentPlayer->previous_model_pos.x;
        copy.m[3][1] -= g_CurrentPlayer->previous_model_pos.y;
        copy.m[3][2] -= g_CurrentPlayer->previous_model_pos.z;

        matrix_4x4_f32_to_s32(&copy, matrices + i);
    }
}


void sub_GAME_7F08BEEC(Mtxf *matrices, s32 count)
{
    Mtxf sp40;
    s32 i;
    s32 j;

    for (i = 0, j = 0; i < count; i++, j += sizeof(Mtxf))
    {
        matrix_4x4_multiply_homogeneous(currentPlayerGetViewToWorldMtxf(), (Mtxf *)((u32)matrices + j), &sp40);

        sp40.m[3][0] -= g_CurrentPlayer->current_model_pos.f[0];
        sp40.m[3][1] -= g_CurrentPlayer->current_model_pos.f[1];
        sp40.m[3][2] -= g_CurrentPlayer->current_model_pos.f[2];

        matrix_4x4_f32_to_s32(&sp40, matrices + i);
    }
}


s32 getMissiontimer(void) {
#ifdef VERSION_EU
    return (mission_timer * 60) / 50;
#else
    return mission_timer;
#endif
}


void SurroundWithExplosions(int delay)
{
    g_SurroundBondWithExplosionsFlag = 1;
    g_SurroundBondWithExplosionsTicks = delay + g_GlobalTimer;
    g_PlayerTickExplodeCreatePosition = 0;
}




