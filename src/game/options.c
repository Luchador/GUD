#include <ultra64.h>
#include <bondconstants.h>
#include <boss.h>
#include <fr.h>
#include <music.h>
#include <os_extension.h>
#include <snd.h>
#include <random.h>
#include "options.h"
#include "bondview.h"
#include "dyn.h"
#include "file.h"
#include "front.h"
#include "gbi_extension.h"
#include "language.h"
#include "player.h"
#include "textrelated.h"
#include "glass.h"
#include "frametiming.h"
#include "assets/obseg/text/LoptionE.h"

#define WATCH_BACKGROUND_VERTEX_COUNT 30

#define WATCH_VOL_ADJUST_STEP 1024

#define WATCH_ROTATION_FRAMES speedgraphframes
#define WATCH_PERSPECTIVE_FOVY    50.5f
#define WATCH_PERSPECTIVE_ASPECT  1.3333334f

#define OPTLABELS_ROW1_Y    0x52
#define OPTLABELS_ROW2_Y    0x6b
#define OPTLABELS_ROW_PITCH 0x19
#define OPTLABELS_COL_RET   0x4b
#define OPTLABELS_HINT_Y    0xc3

#define MAX_CONTROL_STYLES_1CONTROLLER  4
#define MAX_CONTROL_STYLES_2CONTROLLERS 8


Mtx gfx_background_8007B0A0;
Mtx gfx_background_8007B0E0;

u32 D_80040990 = 0;
u32 watch_screen_index = 0;
u32 controller_options_index = 0;
u32 g_WatchGameOptionsIndex = 0;

//D:800409A0
s32 mission_brief_index = BRIEF_INDEX_OBJECTIVES;
//D:800409A4
bool g_WatchAbortIsConfirmSelected = FALSE;
//D:800409A8
s32 watch_item_is_actively_selected = 0;
//D:800409AC
s32 D_800409AC = 0;
//D:800409B0
s32 watch_inventory_text_y = 0;
//D:800409B4
s32 watch_inventory_text_target_y = 0;
//D:800409B8
s32 g_curWatchItemIndex = 0;
//D:800409BC
f32 watch_inventory_cursor_pos = 0.0f;
//D:800409C0
bool watch_inventory_text_is_settled = FALSE;
//D:800409C4
s32 D_800409C4 = 0;
//D:800409D0
s32 D_800409D0 = -1;
//D:800409D4
f32 D_800409D4 = 0.0f;

s32 g_MaxControlStyles = MAX_CONTROL_STYLES_2CONTROLLERS;

u16 g_GameControlStyles[] = {
    /*1.1 honey*/    getStringID(LOPTIONS, OPTION_STR_09_11HONEY_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF), /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF),
    /*1.2 solitaire*/getStringID(LOPTIONS, OPTION_STR_0A_12SOLITAIRE_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF), /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF),
    /*1.3 kissy*/    getStringID(LOPTIONS, OPTION_STR_0B_13KISSY_LF),   /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF),  /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF),
    /*1.4 goodnight*/getStringID(LOPTIONS, OPTION_STR_0C_14GOODNIGHT_LF),   /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF),  /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF),
    /*2.1 plenty*/   getStringID(LOPTIONS, OPTION_STR_0D_21PLENTY_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.2 galore*/   getStringID(LOPTIONS, OPTION_STR_0E_22GALORE_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.3 domino*/   getStringID(LOPTIONS, OPTION_STR_0F_23DOMINO_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.4 goodhead*/ getStringID(LOPTIONS, OPTION_STR_10_24GOODHEAD_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF)
};

struct game_options g_GameOptionEntries[] = {
    { {getStringID(LOPTIONS, OPTION_STR_11_LOOKUPDOWN_LF), getStringID(LOPTIONS, OPTION_STR_1C_REVERSE_LF), getStringID(LOPTIONS, OPTION_STR_1B_UPRIGHT_LF), 0}, 0}, //look up/down, reverse, upright
    { {getStringID(LOPTIONS, OPTION_STR_12_AUTOAIM_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //autoaim, off, on
    { {getStringID(LOPTIONS, OPTION_STR_14_AIMCONTROL_LF), getStringID(LOPTIONS, OPTION_STR_1E_HOLD_LF), getStringID(LOPTIONS, OPTION_STR_1D_TOGGLE_LF), 0}, 0}, //aim control, hold, toggle
    { {getStringID(LOPTIONS, OPTION_STR_15_SIGHTONSCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //sight on screen, off, on
    { {getStringID(LOPTIONS, OPTION_STR_13_LOOKAHEAD_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //look ahead, off, on
    { {getStringID(LOPTIONS, OPTION_STR_16_AMMOONSCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //ammo on screen, off, on
    { {getStringID(LOPTIONS, OPTION_STR_17_SCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1F_FULL_LF), getStringID(LOPTIONS, OPTION_STR_20_WIDE_LF), getStringID(LOPTIONS, OPTION_STR_21_CINEMA_LF)}, 0}, //screen, full, wide, cinema
    { {getStringID(LOPTIONS, OPTION_STR_18_RATIO_LF), getStringID(LOPTIONS, OPTION_STR_22_NORMAL_LF), getStringID(LOPTIONS, OPTION_STR_23_169_LF), 0}, 0} //ratio, normal, 16:9
};


bool g_WatchStickXEnabled = FALSE;
bool g_WatchStickYEnabled = FALSE;
//D:80040AE4
u32 watch_stick_y_prev_active = 0;
//D:80040AE8
f32 D_80040AE8 = 0.0f;
//D:80040AEC
f32 D_80040AEC = 0.0f;
//D:80040AF0
f32 D_80040AF0 = 45.0f;
//D:80040AF4
u32 D_80040AF4 = 0xFF00A0;
//D:80040AF8
u32 D_80040AF8 = 10;
//D:80040AFC
u32 D_80040AFC = 0xFF;

//D:80040B00
u32 D_80040B00 = 0xA;

//D:80040B04
s32 g_WatchBackgroundGreen = 0xE0;

//D:80040B08
u32 g_WatchStaticScanlineAlpha = 0;

//D:80040B0C
u32 D_80040B0C = 0xFFA0;
//D:80040B10
u32 D_80040B10 = 0xF800;
//D:80040B14
f32 D_80040B14 = 0.0f;
//D:80040B18
f32 D_80040B18 = 0.0f;
//D:80040B1C
f32 D_80040B1C = 2.5f;
//D:80040B20
f32 g_WatchControllerPitch = 0.0f;
//D:80040B24
f32 g_WatchControllerSpinAngle = 0.0f;
//D:80040B28
f32 g_WatchControllerSpinSpeed = 0.0f;
//D:80040B2C
s32 D_80040B2C = 0;
//D:80040B30
f32 D_80040B30 = 0.0f;
//D:80040B34
f32 D_80040B34 = 0.0f;
//D:80040B38
f32 D_80040B38 = 0.0f;
//D:80040B3C
s32 D_80040B3C = 0;
//D:80040B40
s32 g_WatchStaticScanlineY = 0;
//D:80040B44
u16 D_80040B44 = 0x1;
//D:80040B48
u32 D_80040B48 = 0x32;
//D:80040B4C
u32 D_80040B4C = 0x32;
//D:80040B50
u32 D_80040B50 = 0x32;
//D:80040B54
u32 D_80040B54 = 0x32;
//D:80040B58
u16 mTrack2Vol = 0x7FFF;

//D:80040B5C
coord3d g_ControllerPos = {0.0f, 200.0f, -200.0f};

typedef struct WatchContButtonPositions {
    f32 words[55]; /* 0xdc bytes */
} WatchContButtonPositions;

//D:80040B68
// 3D positions for the buttons at the sides of the screen for the 1x control styles.
WatchContButtonPositions g_1ContButtonPositions[] = {{
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f,
    -900.0f, 200.0f, -45.0f,     // Start
    715.0f, 200.0f, 393.0f,      // Joy Stick
    -875.0f, 200.0f, -210.0f,    // D-Pad
    900.0f, 200.0f, -260.0f,     // C-Up
    900.0f, 200.0f, -160.0f,     // C-Down
    850.0f, 200.0f, -210.0f,     // C-Left
    950.0f, 200.0f, -210.0f,     // C-Right
    900.0f, 200.0f, 128.0f,      // A
    900.0f, 200.0f, -45.0f,      // B
    -820.0f, 200.0f, -389.0f,    // L
    820.0f, 200.0f, -389.0f,     // R
    -830.0f, 200.0f, 78.0f       // Z
}};

//D:80040C44
// 3D positions for the buttons of the left controller for the 2x control styles.
WatchContButtonPositions g_2ContLeftButtonPositions[] = {{
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, -600.0f, 200.0f,
    500.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, -600.0f, 200.0f,
    240.0f, -600.0f, 200.0f,
    110.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f,
    2000.0f, -600.0f, 200.0f,
    320.0f
}};

//D:80040D20
// 3D positions for the buttons of the right controller for the 2x control styles.
WatchContButtonPositions g_2ContRightButtonPositions[] = {{
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    2000.0f, 0.0f, 2000.0f, 2000.0f,
    0.0f, 2000.0f, 600.0f, 200.0f,
    500.0f, 2000.0f, 0.0f, 2000.0f,
    2000.0f, 0.0f, 2000.0f, 2000.0f,
    0.0f, 2000.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f, 2000.0f,
    600.0f, 200.0f, 240.0f, 600.0f,
    200.0f, 110.0f, 2000.0f, 0.0f,
    2000.0f, 2000.0f, 0.0f, 2000.0f,
    600.0f, 200.0f, 320.0f
}};

//D:80058440
const char D_80058440[];
//D:80058444
const char D_80058444[];
//D:80058448
const char aC_2[];
//D:80058450
const char D_80058450[];
//D:80058454
const char D_80058454[];

// forward declarations

void set_page_rectangle_colors(s32 watch_screen_index, struct WatchVertex *vertices);
Gfx *draw_watch_mission_status_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_watch_inventory_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_watch_control_options_page(Gfx *gdl, Mtx *param_2);
Gfx *watchDrawGameOptionsScreen(Gfx *gdl, Mtx *param_2);
Gfx *draw_watch_mission_briefing_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_background_health_and_armor_transitioning(Gfx *gdl, Mtx *param_2);
Gfx *draw_background_health_and_armor(Gfx *gdl, Mtx *arg1, s32 zoom_squish);
void watchSelectGameOption(u32 *param_1, u32 param_2);
void watch_adjust_volume_slider(u16* arg0);
Gfx *watchDrawQuad4Vtx(Gfx *gdl, uintptr_t vtxPhysAddr);
void update_volume_slider_verts(struct WatchVertex *verts, f32 fill_amount, s32 transition_width);
void sub_GAME_7F0A9684(s8 contpadnum, s32 *counter, f32 *value, f32 *step);
Gfx *display_text_buttons_dual_control(Gfx *gdl);
Gfx *sub_GAME_7F0A9AB8(Gfx *gdl);

// end forward declarations


void optionsWatchInit()
{
    watch_screen_index = WATCH_INDEX_MISSION_STATUS;
    controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
    g_WatchGameOptionsIndex = GAME_OPTIONS_INDEX_MUSIC;
    mission_brief_index = BRIEF_INDEX_OBJECTIVES;
    g_WatchAbortIsConfirmSelected = FALSE;
    watch_item_is_actively_selected = 0;
    D_800409AC = 0;
    watch_inventory_text_y = 0;
    watch_inventory_text_target_y = 0;
    g_curWatchItemIndex = 0;
    watch_inventory_cursor_pos = 0.0f;
    watch_inventory_text_is_settled = FALSE;
    D_800409C4 = 0;
    D_800409D0 = -1;
    D_800409D4 = 0.0f;

    g_CurrentPlayer->neg_vspacing_for_control_type_entry = 0;
    g_CurrentPlayer->cur_player_control_type_1 = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->cur_player_control_type_0 = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->cur_player_control_type_2 = 0.0f;
    g_CurrentPlayer->has_set_control_type_data = TRUE;
    g_MaxControlStyles = MAX_CONTROL_STYLES_2CONTROLLERS;

    g_WatchStickXEnabled = FALSE;
    g_WatchStickYEnabled = FALSE;
    watch_stick_y_prev_active = 0;
    D_80040AE8 = 0.0f;
    D_80040AEC = 0.0f;
    D_80040AF0 = 45.0f;
    D_80040AF4 = 0xff00a0;
    D_80040AF8 = 10;
    D_80040AFC = 0xff;
    D_80040B00 = 10;
    g_WatchBackgroundGreen = 0xe0;
    g_WatchStaticScanlineAlpha = 0;
    D_80040B0C = 0xffa0;
    D_80040B10 = 0xf800;
    D_80040B14 = 0.0f;
    D_80040B18 = 0.0f;
    D_80040B1C = 2.5f;
    g_WatchControllerPitch = 0.0f;
    g_WatchControllerSpinAngle = 0.0f;
    g_WatchControllerSpinSpeed = 0.0f;
    D_80040B2C = 0;
    D_80040B30 = 0.0f;
    D_80040B34 = 0.0f;
    D_80040B38 = 0.0f;
    D_80040B3C = 0;
    g_WatchStaticScanlineY = 0;
    D_80040B44 = 1;
    D_80040B48 = 0x32;
    D_80040B4C = 0x32;
    D_80040B50 = 0x32;
    D_80040B54 = 0x32;
    fileLoadSaveSettingsForSelectedFolder();
    g_MissionFailedOrAborted = FALSE;
}


u32 controllerCheckDualControllerTypesAllowed(void)
{
    if (joyGetControllerCount() >= 2)
    {
        if (cur_player_get_control_type() >= 4)
        {
            return 1;
        }
    }
    return 0;
}


s32 cur_player_get_control_type(void)
{
    return g_CurrentPlayer->cur_player_control_type_0;
}


void cur_player_set_control_type(s32 type)
{
    s32 langsize;

    g_CurrentPlayer->cur_player_control_type_0 = type;
    g_CurrentPlayer->cur_player_control_type_1 = type;
    g_CurrentPlayer->cur_player_control_type_2 = (float)type;

    langsize = 10;

    g_CurrentPlayer->neg_vspacing_for_control_type_entry = -(langsize * type);
    g_CurrentPlayer->has_set_control_type_data = TRUE;

}

u32 get_cur_player_look_vertical_inverted(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_LOOK].current_value;
}

void set_cur_player_look_vertical_inverted(u32 param_1)
{
    g_GameOptionEntries[PLAYER_OPTION_LOOK].current_value = param_1;
}

s32 cur_player_get_autoaim(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_AUTOAIM].current_value;
}

void cur_player_set_autoaim(u32 param_1)
{
    g_GameOptionEntries[PLAYER_OPTION_AUTOAIM].current_value = param_1;
}

u32 cur_player_get_lookahead(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_LOOKAHEAD].current_value;
}

void cur_player_set_lookahead(u32 param_1)
{
    g_GameOptionEntries[PLAYER_OPTION_LOOKAHEAD].current_value = param_1;
}

u32 cur_player_get_aim_control(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_AIM].current_value;
}

void cur_player_set_aim_control(u32 param_1)
{
    g_GameOptionEntries[PLAYER_OPTION_AIM].current_value = param_1;
}

u32 cur_player_get_sight_onscreen_control(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_SIGHT].current_value;
}
void cur_player_set_sight_onscreen_control(u32 param_1)
{
    g_GameOptionEntries[PLAYER_OPTION_SIGHT].current_value = param_1;
}

u32 cur_player_get_ammo_onscreen_setting(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_AMMODISPLAY].current_value;
}
void cur_player_set_ammo_onscreen_setting(u32 param_1)
{
    g_GameOptionEntries[PLAYER_OPTION_AMMODISPLAY].current_value = param_1;
}

u32 cur_player_get_screen_setting(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_SCREEN].current_value;
}
void cur_player_set_screen_setting(u32 param_1)
{
    g_GameOptionEntries[PLAYER_OPTION_SCREEN].current_value = param_1;
}

SCREEN_RATIO_OPTION get_screen_ratio(void)
{
    return g_GameOptionEntries[PLAYER_OPTION_RATIO].current_value;
}

void set_screen_ratio(SCREEN_RATIO_OPTION ratio_option)
{
    g_GameOptionEntries[PLAYER_OPTION_RATIO].current_value = ratio_option;
}


void watchPlayBeep(void)
{

    if (watch_item_is_actively_selected == 1)
    {
        watch_item_is_actively_selected = 0;

    } 
    else 
    {
        watch_item_is_actively_selected = 1;
        sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
    }
}


void watchResetItemIsActivelySelected(void)
{
    watch_item_is_actively_selected = 0;
}


bool watchIsLeftStickStrong(void)
{
    return (joyGetStickX(PLAYER_1) < -45);
}


bool watchIsRightStickStrong(void)
{
    return (joyGetStickX(PLAYER_1) > 46);
}


bool watchGetStickXEnabled(void) 
{
    return g_WatchStickXEnabled;
}


void watchSetStickXDisabled(void) 
{
    g_WatchStickXEnabled = FALSE;
}


bool watchShouldNavLeft(void)
{
    return watchIsLeftStickStrong() && watchGetStickXEnabled();
}


bool watchShouldNavRight(void)
{
    return watchIsRightStickStrong() && watchGetStickXEnabled();
}


bool watchIsUpStickStrong(void)
{
    return (joyGetStickY(PLAYER_1) > 46);
}


bool watchIsDownStickStrong(void)
{
    return (joyGetStickY(PLAYER_1) < -45);
}


bool watchGetStickYEnabled(void)
{
    return g_WatchStickYEnabled;
}


void watchSetStickYDisabled(void)
{
    g_WatchStickYEnabled = FALSE;
}


bool watchShouldNavUp(void)
{
    return watchIsUpStickStrong() && watchGetStickYEnabled();
}


bool watchShouldNavDown(void)
{
    return watchIsDownStickStrong() && watchGetStickYEnabled();
}


bool watchIsUpStickHeld(void)
{
    return (joyGetStickY(PLAYER_1) > 16);
}


bool watchIsDownStickHeld(void)
{
    return (joyGetStickY(PLAYER_1) < -15);
}


u32 wasWasStickPrevYActive(void)
{
    return watch_stick_y_prev_active;
}


s32 watchWasStickUpJustActivated(void)
{
    return watchIsUpStickHeld() && !wasWasStickPrevYActive();
}


s32 watchWasStickDownJustActivated(void)
{
    return watchIsDownStickHeld() && !wasWasStickPrevYActive();
}


void watchPlayStaticSound(void)
{
    g_WatchBackgroundGreen = 128;
    sndPlaySfx(g_musicSfxBufferPtr, WATCH_STATIC_SFX, NULL);

    return;
}


void watchChangeScreen(void)
{
    watchSetStickXDisabled();
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, NULL);

    if ((D_80040B10 << 0x10) < randomGetNext()) 
    {
        watchPlayStaticSound();
    }

    return;
}


/**
 * Initial pause screen: WATCH_INDEX_MISSION_STATUS
 */
void watchNavMissionStatus(void)
{
    s32 goto_watch_screen_index_4;
    s32 goto_watch_screen_index_1;

    if (watch_item_is_actively_selected == 0)
    {
        goto_watch_screen_index_4 = FALSE;
        goto_watch_screen_index_1 = FALSE;


        if (joyGetButtonsPressedThisFrame(PLAYER_1, L_TRIG|L_CBUTTONS))
        {
            goto_watch_screen_index_4 = TRUE;
        }

        if (joyGetButtonsPressedThisFrame(PLAYER_1, R_TRIG|R_CBUTTONS))
        {
            goto_watch_screen_index_1 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_JPAD)) || (watchShouldNavLeft()))
        {
            goto_watch_screen_index_4 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_JPAD)) || (watchShouldNavRight()))
        {
            goto_watch_screen_index_1 = TRUE;
        }

        if (goto_watch_screen_index_4)
        {
            watch_screen_index = WATCH_INDEX_MISSION_BRIEFING;
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM1, 15.0f);
        }

        if (goto_watch_screen_index_1)
        {
            watch_screen_index = WATCH_INDEX_INVENTORY;
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM1, 15.0f);
            return;
        }
    }
    else if ((g_WatchAbortIsConfirmSelected) && (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON)))
    {
        g_WatchAbortIsConfirmSelected = FALSE;
        set_missionstate(MISSION_STATE_0);
        bossRunTitleStage();
        g_MissionFailedOrAborted = TRUE;
        deleteCurrentSelectedFolder();
    }
}


void watchNavInventory(void)
{
    s32 goto_watch_screen_index_0;
    s32 goto_watch_screen_index_2;

    if (watch_item_is_actively_selected == 0)
    {
        goto_watch_screen_index_0 = FALSE;
        goto_watch_screen_index_2 = FALSE;

        if (joyGetButtonsPressedThisFrame(PLAYER_1, L_TRIG|L_CBUTTONS))
        {
            goto_watch_screen_index_0 = TRUE;
        }

        if (joyGetButtonsPressedThisFrame(PLAYER_1, R_TRIG|R_CBUTTONS))
        {
            goto_watch_screen_index_2 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_JPAD)) || (watchShouldNavLeft()))
        {
            goto_watch_screen_index_0 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_JPAD)) || (watchShouldNavRight()))
        {
            goto_watch_screen_index_2 = TRUE;
        }

        if (goto_watch_screen_index_0)
        {
            watch_screen_index = WATCH_INDEX_MISSION_STATUS;
            watchSetAbortIsConfirmSelectedFalse();
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM2, 15.0f);

        }

        if (goto_watch_screen_index_2)
        {
            watch_screen_index = WATCH_INDEX_CONTROL_OPTIONS;
            watchSetStickXDisabled();
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM3, 15.0f);
        }
    }
}


void watchNavControls(void) {

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) || (watchShouldNavLeft()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_INVENTORY;
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM1, 15.0f);
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) || (watchShouldNavRight()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_GAME_OPTIONS;
            watchResetGameOptionsIndex();
            watchSetStickXDisabled();
        }
    }
}


void watchNavOptions(void) {

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) || (watchShouldNavLeft()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_CONTROL_OPTIONS;
            watchResetControllerOptionsIndex();
            watchSetStickXDisabled();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) || (watchShouldNavRight()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_MISSION_BRIEFING;
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM1, 15.0f);
        }
    }
}


void watchNavBriefing(void) {

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) || (watchShouldNavLeft()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            watch_screen_index = WATCH_INDEX_GAME_OPTIONS;
            watchResetGameOptionsIndex();
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM3, 15.0f);
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) || (watchShouldNavRight()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            watch_screen_index = WATCH_INDEX_MISSION_STATUS;
            watchSetAbortIsConfirmSelectedFalse();
            watchChangeScreen();
            watchChangeFOV(WATCHZOOM2, 15.0f);
        }
    }
}


/**
 * Navigate between control styles and the 3D controller on the controller screen.
 */
void watchNavControlTop(void)
{
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (watchShouldNavUp()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_INPUTS;
            watchSetStickYDisabled();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (watchShouldNavDown()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_INPUTS;
            watchSetStickYDisabled();
        }
    }
}


void controller_options_inputs_navigation(void)
{
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (watchShouldNavUp()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
            watchSetStickYDisabled();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (watchShouldNavDown()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
            watchSetStickYDisabled();
        }
    }
}


void watchNavToggleOptions(void)
{
    s32 aux;

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (watchShouldNavUp()))
    {
        g_WatchGameOptionsIndex = g_WatchGameOptionsIndex - 1;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
    else if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (watchShouldNavDown()))
    {
        g_WatchGameOptionsIndex = g_WatchGameOptionsIndex + 1;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }

    aux = g_WatchGameOptionsIndex;

    if (aux >= 10)
    {
        g_WatchGameOptionsIndex = GAME_OPTIONS_INDEX_MUSIC;
        return;
    }

    if (aux < 0)
    {
        g_WatchGameOptionsIndex = GAME_OPTIONS_INDEX_RATIO;
    }
}


void game_options_music_volume_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || watchShouldNavUp())
    {
        g_WatchGameOptionsIndex = GAME_OPTIONS_INDEX_RATIO;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || watchShouldNavDown())
    {
        g_WatchGameOptionsIndex = GAME_OPTIONS_INDEX_FX;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
}


void game_options_fx_volume_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || watchShouldNavUp())
    {
        g_WatchGameOptionsIndex = GAME_OPTIONS_INDEX_MUSIC;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || watchShouldNavDown())
    {
        g_WatchGameOptionsIndex = GAME_OPTIONS_INDEX_LOOK_UPDOWN;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
}


void game_options_inventory_navigation(void)
{
    s32 count;
    s32 item_line_height;
    s32 selected_item_line_height;

    count = bondinvCountTotalItemsInInv();

    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD | U_CBUTTONS) || joyGetStickY(PLAYER_1) >= 0x47)
    {
        if ((s32)watch_inventory_cursor_pos > 0 && !watch_item_is_actively_selected)
        {
            watch_inventory_cursor_pos -= 1.0f;
        }
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD | D_CBUTTONS) || joyGetStickY(PLAYER_1) < -0x46)
    {
        if ((s32)watch_inventory_cursor_pos < count - 1 && !watch_item_is_actively_selected)
        {
            watch_inventory_cursor_pos += 1.0f;
        }
    }

    if (joyGetButtons(PLAYER_1, U_JPAD | U_CBUTTONS))
    {
        if (((s32) watch_inventory_cursor_pos > 0) && !watch_item_is_actively_selected)
        {
            watch_inventory_cursor_pos -= 0.1f;
        }
    }
    else if (joyGetButtons(PLAYER_1, D_JPAD | D_CBUTTONS))
    {
        if (((s32) watch_inventory_cursor_pos < count - 1) && !watch_item_is_actively_selected)
        {
            watch_inventory_cursor_pos += 0.1f;
        }
    }

    if (joyGetStickY(PLAYER_1) >= 0x1f
        && joyGetStickY(PLAYER_1) < 0x46
        && g_curWatchItemIndex > 0
        && !watch_item_is_actively_selected)
    {
        watch_inventory_cursor_pos -= (f32) joyGetStickY(PLAYER_1) / 300.0f;
    }
    else if (joyGetStickY(PLAYER_1) < -0x1e
        && joyGetStickY(PLAYER_1) >= -0x45
        && (s32) watch_inventory_cursor_pos < count - 1
        && !watch_item_is_actively_selected)
    {
        watch_inventory_cursor_pos -= (f32) joyGetStickY(PLAYER_1) / 300.0f;
    }

    if (watchWasStickUpJustActivated() && g_curWatchItemIndex > 0 && !watch_item_is_actively_selected)
    {
        watch_inventory_cursor_pos -= 1.0f;
    }
    else if (watchWasStickDownJustActivated() && g_curWatchItemIndex < count - 1 && !watch_item_is_actively_selected)
    {
        watch_inventory_cursor_pos += 1.0f;
    }

    if (watchIsUpStickHeld() || watchIsDownStickHeld())
    {
        watch_stick_y_prev_active = 1;
    }
    else
    {
        watch_stick_y_prev_active = 0;
    }

    if ((f32) count - 0.5f < watch_inventory_cursor_pos)
    {
        watch_inventory_cursor_pos = (f32) count - 0.5f;
    }

    if (watch_inventory_cursor_pos < -0.5f)
    {
        watch_inventory_cursor_pos = -0.5f;
    }

    // The current item is determined by the integer part of the cursor's position.
    g_curWatchItemIndex = (s32) watch_inventory_cursor_pos;

    item_line_height = 12;

    selected_item_line_height = 12;
    watch_inventory_text_target_y = (2 * selected_item_line_height) + (-g_curWatchItemIndex * item_line_height);

    if (watch_inventory_text_target_y < watch_inventory_text_y)
    {
        watch_inventory_text_y = (watch_inventory_text_y - ((watch_inventory_text_y - watch_inventory_text_target_y) / 3)) - 1;
        watch_inventory_text_is_settled = FALSE;
    }
    else if (watch_inventory_text_y < watch_inventory_text_target_y)
    {
        watch_inventory_text_y = (watch_inventory_text_y + ((watch_inventory_text_target_y - watch_inventory_text_y) / 3)) + ((0, 1));
        watch_inventory_text_is_settled = FALSE;
    }
    else
    {
        watch_inventory_text_is_settled = TRUE;
    }

    if (((f32) g_curWatchItemIndex + 0.55f < watch_inventory_cursor_pos) && !joyGetButtons(PLAYER_1, 0xffff)) // Any button
    {
        watch_inventory_cursor_pos -= 0.1f;
    }
    else if (watch_inventory_cursor_pos <= (f32) g_curWatchItemIndex + 0.45f && !joyGetButtons(PLAYER_1, 0xffff)) // Any button
    {
        watch_inventory_cursor_pos += 0.1f;
    }
}


void sub_GAME_7F0A611C(f32 *arg0, s32 *arg1, s32 arg2, s32 *arg3, s32 *arg4, s32 *arg5, s32 arg6, s32 arg7, s32 arg8)
{

    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD | U_CBUTTONS) || joyGetStickY(PLAYER_1) >= 0x47)
    {
        if ((s32)*arg0 > 0 && arg7)
        {
            *arg0 -= 1.0f;
        }
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD | D_CBUTTONS) || joyGetStickY(PLAYER_1) < -0x46)
    {
        if ((s32)*arg0 < arg2 - 1 && arg7)
        {
            *arg0 += 1.0f;
        }
    }

    if (joyGetButtons(PLAYER_1, U_JPAD | U_CBUTTONS))
    {
        if ((s32)*arg0 > 0 && arg7)
        {
            *arg0 -= 0.1f;
        }
    }
    else if (joyGetButtons(PLAYER_1, D_JPAD | D_CBUTTONS))
    {
        if ((s32)*arg0 < arg2 - 1 && arg7)
        {
            *arg0 += 0.1f;
        }
    }

    if (joyGetStickY(PLAYER_1) >= 0x1f && joyGetStickY(PLAYER_1) < 0x46 && *arg1 > 0 && arg7)
    {
        *arg0 -= (f32)joyGetStickY(PLAYER_1) / 300.0f;
    }
    else if (joyGetStickY(PLAYER_1) < -0x1e && joyGetStickY(PLAYER_1) >= -0x45 && (s32)*arg0 < arg2 - 1 && arg7)
    {
        *arg0 -= (f32)joyGetStickY(PLAYER_1) / 300.0f;
    }

    if (watchWasStickUpJustActivated() && *arg1 > 0 && arg7)
    {
        *arg0 -= 1.0f;
    }
    else if (watchWasStickDownJustActivated() && *arg1 < arg2 - 1 && arg7)
    {
        *arg0 += 1.0f;
    }

    if (watchIsUpStickHeld() || watchIsDownStickHeld())
    {
        watch_stick_y_prev_active = 1;
    }
    else
    {
        watch_stick_y_prev_active = 0;
    }

    if ((f32)arg2 - 0.5f < *arg0)
    {
        *arg0 = (f32)arg2 - 0.5f;
    }

    if (*arg0 < -0.5f)
    {
        *arg0 = -0.5f;
    }

    *arg1 = (s32)*arg0;
    *arg4 = (arg6 * arg8) + (-*arg1 * arg8);

    if (*arg4 < *arg3)
    {
        *arg3 = (*arg3 - ((*arg3 - *arg4) / 3)) - 1;
        *arg5 = 0;
    }
    else if (*arg3 < *arg4)
    {
        *arg3 = (((*arg4 - *arg3) / 3) + *arg3) + 1;
        *arg5 = 0;
    }
    else
    {
        *arg5 = 1;
    }

    if ((f32)*arg1 + 0.55f < *arg0)
    {
        if (!joyGetButtons(PLAYER_1, ANY_BUTTON))
        {
            *arg0 -= 0.1f;
            return;
        }
    }

    if (*arg0 <= (f32)*arg1 + 0.45f)
    {
        if (!joyGetButtons(PLAYER_1, ANY_BUTTON))
        {
            *arg0 += 0.1f;
        }
    }
}


void mission_brief_background_navigation(void)
{
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (watchShouldNavUp()))
    {
        mission_brief_index = BRIEF_INDEX_OBJECTIVES;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (watchShouldNavDown()))
    {
        mission_brief_index = BRIEF_INDEX_M;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
}


void mission_brief_m_briefing_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || watchShouldNavUp())
    {
        mission_brief_index = BRIEF_INDEX_BACKGROUND;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || watchShouldNavDown())
    {
        mission_brief_index = BRIEF_INDEX_Q;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
}


void mission_brief_q_branch_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || watchShouldNavUp())
    {
        mission_brief_index = BRIEF_INDEX_M;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || watchShouldNavDown())
    {
        mission_brief_index = BRIEF_INDEX_MONEYPENNY;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
}

void mission_brief_moneypenny_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || watchShouldNavUp())
    {
        mission_brief_index = BRIEF_INDEX_Q;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || watchShouldNavDown())
    {
        mission_brief_index = BRIEF_INDEX_OBJECTIVES;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
}

void mission_brief_objectives_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || watchShouldNavUp())
    {
        mission_brief_index = BRIEF_INDEX_MONEYPENNY;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || watchShouldNavDown())
    {
        mission_brief_index = BRIEF_INDEX_BACKGROUND;
        watchSetStickYDisabled();
        watchResetItemIsActivelySelected();
    }
}


void build_watch_static_scanline_vertices(Vtx *vertices)
{
    s32 halfWidth; // Half width for the scanline
    Vtx *vertex;
    s32 zoffs;
    s32 greenChannelIndex;
    s32 side;

    // Fit the width of the scanline to the watch's green circle
    halfWidth = sqrtf(213444.0f - ((f32) (g_WatchStaticScanlineY * g_WatchStaticScanlineY)));

    for (zoffs = 0; zoffs != 8; zoffs += 4)
    {
        for (side = -1; side != 3; side += 2)
        {
            vertex = vertices;

            vertex->v.ob[0] = halfWidth * side;
            vertex->v.ob[1] = 0;
            vertex->v.ob[2] = zoffs + g_WatchStaticScanlineY;

            vertex->v.cn[1] = 0xA0;
            vertex->v.flag = 0;
            vertex->v.tc[0] = 0;
            vertex->v.tc[1] = 0;
            vertex->v.cn[0] = 0;

            greenChannelIndex = 1;
            vertex->v.cn[greenChannelIndex] = 0xA0;
            vertex->v.cn[2] = 0;
            vertex->v.cn[3] = g_WatchStaticScanlineAlpha;

            vertices++;
        }
    }
}


void watchReset(void)
{
    if (joyGetControllerCount() < 2)
    {
        g_MaxControlStyles = MAX_CONTROL_STYLES_1CONTROLLER;
    }
    else
    {
        g_MaxControlStyles = MAX_CONTROL_STYLES_2CONTROLLERS;
    }

    watchResetItemIsActivelySelected();

    watch_screen_index = WATCH_INDEX_MISSION_STATUS;
    mission_brief_index = BRIEF_INDEX_OBJECTIVES;

    bondinvDetermineEquippedItem();
}


f32 watchWrapAroundPI(f32 radians)
{
    if (radians > M_PI_F)
    {
        radians = radians - M_TAU_F;
    }
    else if (radians < -M_PI_F)
    {
        radians = radians + M_TAU_F;
    }

    return radians;
}


void watchNavigate(void)
{
    u32 temp_1;
    s32 temp_2;
    s32 temp_3;
    u32 random_value;

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON))
    {
        set_open_close_solo_watch_menu_to1();
    }

    if (!g_WatchStickXEnabled)
    {
        if ((joyGetStickX(PLAYER_1) >= -10) && (joyGetStickX(PLAYER_1) < 11))
        {
            g_WatchStickXEnabled = TRUE;
        }
        else if ((joyGetStickX(PLAYER_1) < 11) && (joyGetPrevStickX(PLAYER_1) >= 11))
        {
            g_WatchStickXEnabled = TRUE;
        }
        else if ((joyGetStickX(PLAYER_1) >= -10) && (joyGetPrevStickX(PLAYER_1) < -10))
        {
            g_WatchStickXEnabled = TRUE;
        }
    }

    if (!g_WatchStickYEnabled)
    {
        if ((joyGetStickY(PLAYER_1) >= -10) && (joyGetStickY(PLAYER_1) < 11))
        {
            g_WatchStickYEnabled = TRUE;
        }
        else if ((joyGetStickY(PLAYER_1) < 11) && (joyGetPrevStickY(PLAYER_1) >= 11))
        {
            g_WatchStickYEnabled = TRUE;
        }
        else if ((joyGetStickY(PLAYER_1) >= -10) && (joyGetPrevStickY(PLAYER_1) < -10))
        {
            g_WatchStickYEnabled = TRUE;
        }
    }

    temp_2 = D_80040AF8;

    if (temp_2 < 0)
    {
        D_80040AF4 = D_80040AF4 + 0xFFF00000;
    }

    D_80040AF8 = temp_2 - 1;

    if (D_80040AF4 < 0x5F00A1U)
    {
        D_80040AF4 = 0xFF00A0U;
        D_80040AF8 = 0xF;
    }

    temp_3 = D_80040B00;

    if (temp_3 < 0)
    {
        D_80040AFC = D_80040AFC - 16;
    }

    D_80040B00 = temp_3 - 1;

    if (D_80040AFC < 0x60U)
    {
        D_80040AFC = 0xFFU;
        D_80040B00 = 0xF;
    }

    D_80040B14 += ((D_80040B1C * speedgraphframes * M_TAU_F) / 360.0f);
    D_80040B14 = watchWrapAroundPI(D_80040B14);

    temp_1 = D_80040B0C << 0x10;

    if (temp_1 < randomGetNext())
    {
        watchPlayStaticSound();
    }

    if (g_WatchBackgroundGreen < 224)
    {
        random_value = randomGetNext();
        g_WatchBackgroundGreen += (random_value >> 0x1E);
    }

    if (g_WatchBackgroundGreen > 224)
    {
        g_WatchBackgroundGreen = 224;
    }

    g_WatchStaticScanlineAlpha = ((-g_WatchBackgroundGreen * 4) + 896);

    g_WatchStaticScanlineY = g_WatchStaticScanlineY - 4;

    if (g_WatchStaticScanlineY >= 343)
    {
        g_WatchStaticScanlineY = -342;
    }

    if (g_WatchStaticScanlineY < -342) 
    {
        g_WatchStaticScanlineY = 342;
    }

    D_80040B44 = (s16)D_80040B44 + 1;
    D_80040B44 = (s16)D_80040B44 & 1;

    switch (watch_screen_index)
    {
        case WATCH_INDEX_MISSION_STATUS:
            watchNavMissionStatus();
            break;

        case WATCH_INDEX_CONTROL_OPTIONS:
            switch (controller_options_index)
            {
                case CONTROLLER_OPTIONS_INDEX_STYLE:
                    watchNavControlTop();
                    break;

                case CONTROLLER_OPTIONS_INDEX_INPUTS:
                    controller_options_inputs_navigation();
            }
            watchNavControls();
            break;

        case WATCH_INDEX_GAME_OPTIONS:
            switch (g_WatchGameOptionsIndex)
            {
                case GAME_OPTIONS_INDEX_MUSIC:
                    game_options_music_volume_navigation();
                    break;

                case GAME_OPTIONS_INDEX_FX:
                    game_options_fx_volume_navigation();
                    break;

                case GAME_OPTIONS_INDEX_LOOK_UPDOWN:
                case GAME_OPTIONS_INDEX_AUTO_AIM:
                case GAME_OPTIONS_INDEX_AIM_CONTROL:
                case GAME_OPTIONS_INDEX_SIGHT_ONSCREEN:
                case GAME_OPTIONS_INDEX_LOOK_AHEAD:
                case GAME_OPTIONS_INDEX_AMMO_ONSCREEN:
                case GAME_OPTIONS_INDEX_SCREEN_SIZE:
                case GAME_OPTIONS_INDEX_RATIO:
                    watchNavToggleOptions();
            }
            watchNavOptions();
            break;

        case WATCH_INDEX_MISSION_BRIEFING:
            watchNavBriefing();
            break;

        case WATCH_INDEX_INVENTORY:
            watchNavInventory();
    }
}


Gfx *sub_GAME_7F0A6EE8(Gfx *DL)
{
    gSPSetGeometryMode(DL++, G_CULL_BACK);
    gDPSetCycleType(DL++, G_CYC_1CYCLE);
    gDPPipelineMode(DL++, G_PM_1PRIMITIVE);
    gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0,viGetX(), viGetY() );
    gDPSetTextureLOD(DL++, G_TL_TILE);
    gDPSetTextureLUT(DL++, G_TT_NONE);
    gDPSetTextureDetail(DL++, G_TD_CLAMP);
    gDPSetTexturePersp(DL++, G_TP_PERSP);
    gDPSetTextureFilter(DL++, G_TF_BILERP);
    gDPSetTextureConvert(DL++, G_TC_FILT);
    gDPSetCombineLERP(DL++, 0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE,  0, 0, 0, SHADE);
    gDPSetCombineKey(DL++, G_CK_NONE);
    gDPSetAlphaCompare(DL++, G_AC_NONE);
    gDPSetRenderMode(DL++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetColorDither(DL++, G_CD_MAGICSQ);

    return DL;
}


void set_page_rectangle_colors(s32 watch_screen_index, struct WatchVertex *vertices)
{
    s32 i;

    // Unselected rectangles.
    for (i = 0; i < 20; i++)
    {
        vertices[i].color.r = 0x20;
        vertices[i].color.g = 0x70;
        vertices[i].color.b = 0x20;
    }

    // Currently selected page rectangle.
    for (i = watch_screen_index * 4; i <= watch_screen_index * 4 + 3; i++)
    {
        vertices[i].color.r = 0x50;
        vertices[i].color.g = 0xF0;
        vertices[i].color.b = 0x50;

        // Currently selected page rectangle, but something else is in focus e.g. toggling options or manipulating the controller on the controller screen.
        if (watch_item_is_actively_selected)
        {
            vertices[i].color.r = 0x30;
            vertices[i].color.g = 0xA0;
            vertices[i].color.b = 0x30;
        }
    }
}


/**
 * @param gdl:
 * @param arg1: Something about watch view matrix.
 * @param zoom_squish: When set, will "unfold" the interior watch watch area (green + bars)
 * based on view distance during pause animation. This is used when starting to pause and bring
 * the watch up, and exiting pause menu to resume game play. If this is disabled then
 * the interior area will always be the same size as the watch container.
*/
Gfx *draw_background_health_and_armor(Gfx *gdl, Mtx *arg1, s32 zoom_squish)
{
    int i;
    struct WatchVertex *sp48;
    struct WatchVertex *sp44;
    Gfx *sp40;
    Gfx *sp3C;

    s32 stack_pad[6];

    f32 scale;

    sp48 = dynAllocateVertices(WATCH_BACKGROUND_VERTEX_COUNT);
    sp44 = dynAllocateVertices(WATCH_BACKGROUND_VERTEX_COUNT);
    sp40 = dynAllocate(0xF8);
    sp3C = dynAllocate(0xF8);

    /**
     * It seems at this point in rendering the background watch arms (hour,minute,second)
     * have already been drawn. Whatever is about to be drawn next is oriented
     * to the second hand of the watch.
     * The following commands reset the orientation for the background green
     * area and health bars. Or, commenting this out will draw the background
     * and slowly spin it around in sync with the second hand.
    */
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gDPSetPrimColor(gdl++, 0, 0, 0xE6, 0xE6, 0xE6, 0x00);
    gSPMatrix(gdl++, arg1, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    // end initial setup.

    scale = 1;

    if (check_watch_page_transistion_running())
    {
        scale = (g_CurrentPlayer->zoomintime * (g_CurrentPlayer->zoominfovynew - g_CurrentPlayer->zoominfovyold))
            / g_CurrentPlayer->zoomintimemax;

        if (scale < 0.0f)
        {
            scale = -scale;
        }

        if (scale > 1)
        {
            scale = 1;
        }

        scale = scale * scale;
    }

    if (zoom_squish == 1)
    {
        scale = 0.05f;
        g_WatchBackgroundGreen = 0xE0;

        if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x4 || g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x6)
        {
            scale = bondviewWatchAnimationRelated();
        }
    }

    guScale(&gfx_background_8007B0A0, 0.25f, 0.25f, 0.25f);

    gSPMatrix(gdl++, &gfx_background_8007B0A0, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);

    if (zoom_squish == 0)
    {
        gSPClearGeometryMode(gdl++, G_CULL_BOTH);
        // draw body armor bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_body_armor_bar_gdl));
        // draw health bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_health_bar_gdl));
    }

    /**
     * This section renders main background, side health bar & body armor bars while zooming in or out.
    */
    guScale(&gfx_background_8007B0E0, 1, 1, scale);

    gSPMatrix(gdl++, &gfx_background_8007B0E0, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);

    if (zoom_squish == 1)
    {
        gSPClearGeometryMode(gdl++, G_CULL_BOTH);
        // draw body armor bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_body_armor_bar_gdl));
        // draw health bars
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(&g_CurrentPlayer->watch_health_bar_gdl));
    }
    /**
     * End health bar zoom section
    */

    sub_GAME_7F0A33F8(sp44, WATCH_BACKGROUND_VERTEX_COUNT, 0.92f, 0);
    draw_watch_background(sp3C, OS_PHYSICAL_TO_K0(sp44), WATCH_BACKGROUND_VERTEX_COUNT, 0);

    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0x00, 0xFF, 0x00, 0x00);
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(sp3C));
    gDPPipeSync(gdl++);

    /**
     * This section renders the green background area of the watch menu.
    */
    if (g_WatchBackgroundGreen < 0xE0)
    {
        sub_GAME_7F0A33F8(sp48, WATCH_BACKGROUND_VERTEX_COUNT, 0.899999976158f, 0);
        draw_watch_background(sp40, OS_PHYSICAL_TO_K0(sp48), WATCH_BACKGROUND_VERTEX_COUNT, 0);

        gDPSetRenderMode(gdl++, G_RM_AA_PCL_SURF, G_RM_AA_PCL_SURF2);
    }
    else
    {
        sub_GAME_7F0A33F8(sp48, WATCH_BACKGROUND_VERTEX_COUNT, 0.899999976158f, 1);
        draw_watch_background(sp40, OS_PHYSICAL_TO_K0(sp48), WATCH_BACKGROUND_VERTEX_COUNT, 1);

        gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }

    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(sp40));
    /**
     * // end green background area.
    */

    /**
     * This section renders the green rectangles/page select at the bottom of the screen.
     * This is setup in bondview trigger_solo_watch_menu.
    */
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(g_CurrentPlayer->buffer_for_watch_greenbackdrop_DL));
    /**
     * // end green rectangles/page select section
    */

    for (i=0; i<WATCH_BACKGROUND_VERTEX_COUNT; i++)
    {
        sp48[i].color.a = (s8)g_WatchBackgroundGreen;
        sp44[i].color.a = (s8)g_WatchBackgroundGreen;
    }

    if (g_WatchBackgroundGreen < 0xE0)
    {
        // Create the thin green scanline that moves up the screen while the watch does static.
        build_watch_static_scanline_vertices(g_CurrentPlayer->buffer_for_watch_static_vertices);

        gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(g_CurrentPlayer->buffer_for_watch_static_DL));
    }

    return gdl;
}


Gfx *draw_background_health_and_armor_transitioning(Gfx *gdl, Mtx *param_2)
{
    return draw_background_health_and_armor(gdl, param_2, 1);
}


Gfx *draw_abort_cancel_confirm(Gfx *gdl)
{
    s32 sp7C;
    s32 sp78;
    s32 sp74;
    s32 sp70;
    s32 sp6C;
    s32 sp68;
    s32 sp64;
    s32 sp60;

    s32 pFontFile;
    s32 pFontChars;
    s32 sp54;
    s32 sp50;
    s32 sp4C;

    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;
    sp54 = langGet(getStringID(LOPTIONS, OPTION_STR_24_ABORT_LF)); //abort:
    sp50 = langGet(getStringID(LOPTIONS, OPTION_STR_25_CONFIRM_LF)); //confirm
    sp4C = langGet(getStringID(LOPTIONS, OPTION_STR_26_CANCEL_LF)); //cancel
    sp7C = 0x51;

    sp78 = 0xBD;

    sp74 = 0x88;

    sp70 = sp6C = sp68 = 76;

    if (watch_item_is_actively_selected != 0)
    {
        if (g_WatchAbortIsConfirmSelected == FALSE)
        {
            if ((joyGetStickX(PLAYER_1) >= 0x2E) || (joyGetButtons(PLAYER_1, 0x111) != 0))
            {
                g_WatchAbortIsConfirmSelected = TRUE;
            }
        }
        else
        {
            if (g_WatchAbortIsConfirmSelected)
            {
                if ((joyGetStickX(PLAYER_1) < -0x2D) || (joyGetButtons(PLAYER_1, 0x222) != 0))
                {
                    g_WatchAbortIsConfirmSelected = FALSE;
                }
            }
        }
    }

    if (watch_item_is_actively_selected != 0)
    {
        textMeasure(&sp60, &sp64, sp54, pFontChars, pFontFile, 0);

        gdl = textRender(gdl, &sp7C, &sp70, sp54, pFontChars, pFontFile, 0xA0FFA0F0, sp64, sp60, 0, 0);

        if (g_WatchAbortIsConfirmSelected)
        {
            gdl = textRenderOutlined(gdl, &sp78, &sp6C, sp50, pFontChars, pFontFile, -1, 0x7000A0, viGetX(), viGetY(), 0, 0);
            gdl = textRender(gdl, &sp74, &sp68, sp4C, pFontChars, pFontFile, 0xFF00B0, viGetX(), viGetY(), 0, 0);
        }
        else
        {
            if (g_WatchAbortIsConfirmSelected == FALSE)
            {
                gdl = textRender(gdl, &sp78, &sp6C, sp50, pFontChars, pFontFile, 0xFF00B0, viGetX(), viGetY(), 0, 0);
                gdl = textRenderOutlined(gdl, &sp74, &sp68, sp4C, pFontChars, pFontFile, -1, 0x7000A0, viGetX(), viGetY(), 0, 0);
            }
        }
    }
    else
    {
        textMeasure(&sp60, &sp64, sp54, pFontChars, pFontFile, 0);
        gdl = textRender(gdl, &sp7C, &sp70, sp54, pFontChars, pFontFile, 0x800080, sp64, sp60, 0, 0);
        gdl = textRender(gdl, &sp78, &sp6C, sp50, pFontChars, pFontFile, 0x800080, viGetX(), viGetY(), 0, 0);
        gdl = textRender(gdl, &sp74, &sp68, sp4C, pFontChars, pFontFile, 0x800080, viGetX(), viGetY(), 0, 0);
    }

    return gdl;
}


Gfx *draw_text_mission_status(Gfx *gdl)
{
    s32 txtptr_1;
    s32 txtptr_2;
    s32 sp64;
    s32 sp60;
    s32 sp5C;
    s32 sp58;
    s32 pFontFile;
    s32 pFontChars;
    s32 sp4C;

    txtptr_1 = langGet(getStringID(LOPTIONS, OPTION_STR_27_MISSIONSTATUS_LF)); //mission status:
    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;

    if (objectiveIsAllComplete())
    {
        sp4C = 0xFF00B0;
        txtptr_2 = langGet(getStringID(LOPTIONS, OPTION_STR_28_COMPLETE_LF)); //complete
    }
    else
    {
        sp4C = D_80040AF4;
        txtptr_2 = langGet(getStringID(LOPTIONS, OPTION_STR_29_INCOMPLETE_LF)); //incomplete
    }

    gdl = gfxSetup2DTextureMode(gdl);
    textMeasure(&sp5C, &sp58, txtptr_1, pFontChars, pFontFile, 0);
    sp64 = 0x51;
    sp60 = YOFFSET_MISSIONSTATUS;
    gdl = textRender(gdl, &sp64, &sp60, txtptr_1, pFontChars, pFontFile, 0xFF00B0, sp58, sp5C, 0, 0);
    
    sp64 = sp64 + sp58 + 4;
    sp60 = sp60 - sp5C;
    textMeasure(&sp5C, &sp58, txtptr_2, pFontChars, pFontFile, 0);
    gdl = textRender(gdl, &sp64, &sp60, txtptr_2, pFontChars, pFontFile, sp4C, sp58, sp5C, 0, 0);
    gdl = draw_abort_cancel_confirm(gdl);

    return gdl;
}


Gfx *empty_draw_function(Gfx *gdl)
{
    return gdl;
}


Gfx *draw_text_q_watch_v201_beta(Gfx *gdl)
{
    s32 txtptr;
    s32 sp50;
    s32 sp4C;
    s32 sp48;
    s32 sp44;
    s32 pFontFile;
    s32 pFontChars;

    txtptr = langGet(getStringID(LOPTIONS, OPTION_STR_2B_QWATCHVERSION_LF)); //q watch v2.01 beta

    sp50 = 0x65;
    sp4C = YOFFSET_7;
    sp48 = 0;
    sp44 = 0;
    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;
    gdl = gfxSetup2DTextureMode(gdl);
    textMeasure(&sp48, &sp44, txtptr, pFontChars, pFontFile, 0);
    gdl = textRender(gdl, &sp50, &sp4C, txtptr, pFontChars, pFontFile, 0xFF00B0, sp44, sp48, 0, 0);
    return gdl;
}


#ifndef _BONDWALK_H_
#define _BONDWALK_H_
typedef struct GunModelFileRecord {
    void *item_header;
    char *item_file_name;
    s32   has_no_model;
    void *item_weapon_stats;
    u16   upper_watch_text;
    u16   lower_watch_text;
    f32   watch_pos_x, watch_pos_y, watch_pos_z;
    f32   x_rotation,  y_rotation;
    u16   weapon_of_choice_text, watch_equipment_text;
    f32   equip_watch_x, equip_watch_y, equip_watch_z;
} GunModelFileRecord;
#endif
extern GunModelFileRecord gitem_structs[];
/* forward declarations so the compiler uses f32 calling convention ($f0) */
extern f32 bondinvGetVposWatchForIndex(s32 index);
extern f32 bondinvGetHposWatchForIndex(s32 index);
extern f32 bondinvGetDepthWatchForIndex(s32 index);
extern f32 bondinvGetDifferent45AngleForIndex(s32 index);
extern f32 bondinvGetXrotWatchForIndex(s32 index);
extern f32 bondinvGetYrotWatchForIndex(s32 index);

Gfx* draw_current_hand_item_and_ammo(Gfx* gdl)
{
    Mtx* sp114;
    u16 perspNorm;
    Mtxf matrix2;
    Mtxf matrix;
    s32 sp8C;
    s32 sp88;
    s32 sp84;
    s32 sp80;
    s32 sp7C;
    s32 sp78;
    s32 temp_v0;
    s32 sp70;
    f32 sp6C;
    f32 sp68;
    f32 sp64;
    f32 sp60;
    f32 rotx;
    f32 roty;
    s8* text;
    s8* text2;
    struct GunModelFileRecord *gitem;

    sp114 = dynAllocateMatrix();
    sp84 = 0;
    sp80 = 0;

    sp7C = ptrFontBankGothic;
    sp78 = ptrFontBankGothicChars;

    temp_v0 = bondinvGetCurEquippedItem();
    sp70 = bondinvGetTextbyInvIndex(temp_v0);
    sp6C = bondinvGetVposWatchForIndex(temp_v0);
    sp68 = bondinvGetHposWatchForIndex(temp_v0);
    sp64 = bondinvGetDepthWatchForIndex(temp_v0);
    sp60 = bondinvGetDifferent45AngleForIndex(temp_v0);
    rotx = bondinvGetXrotWatchForIndex(temp_v0);
    roty = bondinvGetYrotWatchForIndex(temp_v0);
    text = bondinvGetFirstTitlebyIndex(temp_v0);
    text2 = bondinvGetSecondTitlebyIndex(temp_v0);


    guPerspective(sp114, &perspNorm, sp60, 1.33333337f, 10.0f, 10000.0f, 1.0f);
    gSPMatrix(gdl++, osVirtualToPhysical(sp114), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    matrix_4x4_set_rotation_around_y((roty * 6.2831855f) / 360.0f, &matrix2);
    matrix_4x4_set_rotation_around_z(6.2831855f - ((rotx * 6.2831855f) / 360.0f), &matrix);
    matrix_4x4_multiply_in_place(&matrix, &matrix2);
    matrix_4x4_set_lookat_target(&matrix, sp64, sp6C, sp68, 0.0f, sp6C, sp68, 0.0f, 1.0f, 0.0f);
    matrix_4x4_multiply_in_place(&matrix, &matrix2);

    gdl = sub_GAME_7F0A6EE8(gdl);

    if (g_WatchBackgroundGreen < 0xE0) {
        gdl = set_enviro_fog_for_items_in_solo_watch_menu(gdl, sp70, &matrix2, g_WatchBackgroundGreen + 1, 0x64DC6428);
    } else {
        gdl = set_enviro_fog_for_items_in_solo_watch_menu(gdl, sp70, &matrix2, 0xFF, 0x64DC6428);
    }

    gdl = gfxSetup2DTextureMode(gunDrawWatchAmmoDisplay(gdl));

    sp8C = 0x60;
#if defined(LEFTOVERDEBUG)
    sp88 = 0xA0;
#else
    sp88 = 0xBC;
#endif
    textMeasure(&sp84, &sp80, text, sp78, sp7C, 0);
    gdl = textRender(gdl, &sp8C, &sp88, text, sp78, sp7C, 0xFF00B0, sp80, sp84, 0, 0);

#if defined(LEFTOVERDEBUG)
    sp88 = 0xAA;
#else
    sp88 = 0xC6;
#endif
    textMeasure(&sp84, &sp80, text2, sp78, sp7C, 0);
    gdl = textRender(gdl, &sp8C, &sp88, text2, sp78, sp7C, 0xFF00B0, sp80, sp84, 0, 0);

    return gdl;
}


Gfx *draw_watch_mission_status_page(Gfx *gdl, Mtx *param_2)
{
    gdl = draw_background_health_and_armor(gdl, param_2, 0);

    if (check_watch_page_transistion_running() != 1)
    {
        gdl = draw_text_q_watch_v201_beta(gdl);
        gdl = draw_text_mission_status(gdl);
        gdl = draw_current_hand_item_and_ammo(empty_draw_function(gdl));
    }
    else
    {
        check_watch_page_transistion_running();
    }

    return gdl;
}


void sub_GAME_7F0A8378(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON) == 0) {
        if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON) == 0)
        {
            return;
        }

        if (getCurrentPlayerWeaponId(0) == bondinvGetTextbyInvIndex(g_curWatchItemIndex))
        {
            return;
        }
    }

    currentPlayerUnEquipWeaponWrapper(0, bondinvGetTextbyInvIndex(g_curWatchItemIndex));
    currentPlayerUnEquipWeaponWrapper(1, 0);
    bondinvSetCurEquippedItem(g_curWatchItemIndex);
    D_800409C4 = 10;
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
}


#ifndef _BONDWALK_H_
typedef struct GunModelFileRecord {
    void *item_header;
    char *item_file_name;
    s32   has_no_model;
    void *item_weapon_stats;
    u16   upper_watch_text;
    u16   lower_watch_text;
    f32   watch_pos_x, watch_pos_y, watch_pos_z;
    f32   x_rotation,  y_rotation;
    u16   weapon_of_choice_text, watch_equipment_text;
    f32   equip_watch_x, equip_watch_y, equip_watch_z;
} GunModelFileRecord;
#endif
extern GunModelFileRecord gitem_structs[];
/* forward declarations so the compiler uses f32 calling convention ($f0) */
extern f32 bondinvGet45AngleForIndex(s32 index);
extern f32 bondinvGetHoffsetForIndex(s32 index);
extern f32 bondinvGetVoffsetForIndex(s32 index);
extern f32 bondinvGetDepthForIndex(s32 index);
extern f32 bondinvGetXrotWatchForIndex(s32 index);
extern f32 bondinvGetYrotWatchForIndex(s32 index);
extern u16 *bondinvGetNameByIndex(s32 index);


Gfx *draw_watch_inventory_page(Gfx *gdl, Mtx *param_2)
{
    Mtx *sp924;
    u16 perspNorm;
    Mtxf sp8E0;
    Mtxf sp8A0;
    f32 temp_cos;
    f32 temp_sin;
    f32 sp894;
    f32 sp890;
    f32 sp88C;
    f32 sp888;
    s32 sp884;
    f32 sp880;
    f32 sp87C;
    s32 temp_s0_3;
    GunModelFileRecord *gitem;
    s32 x1;
    s32 y1;

    gdl = draw_background_health_and_armor(gdl, param_2, 0);

    if (check_watch_page_transistion_running() != 1)
    {
        sp924 = dynAllocateMatrix();
        sp894 = bondinvGet45AngleForIndex(g_curWatchItemIndex);
        sp890 = bondinvGetHoffsetForIndex(g_curWatchItemIndex);
        sp88C = bondinvGetVoffsetForIndex(g_curWatchItemIndex);
        sp888 = bondinvGetDepthForIndex(g_curWatchItemIndex);
        sp884 = bondinvGetTextbyInvIndex(g_curWatchItemIndex);
        sp880 = bondinvGetXrotWatchForIndex(g_curWatchItemIndex);
        sp87C = bondinvGetYrotWatchForIndex(g_curWatchItemIndex);

        guPerspective(sp924, &perspNorm, sp894, WATCH_PERSPECTIVE_ASPECT, 10.0f, 10000.0f, 1.0f);

#undef WATCH_INV_ASPECT_RATIO

        gSPMatrix(gdl++, osVirtualToPhysical(sp924), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

        matrix_4x4_set_rotation_around_y((sp87C * M_TAU_F) / 360.0f, &sp8E0);
        matrix_4x4_set_rotation_around_z(M_TAU_F - ((sp880 * M_TAU_F) / 360.0f), &sp8A0);
        matrix_4x4_multiply_in_place(&sp8A0, &sp8E0);

        temp_cos = cosf(D_80040B14) * sp888;
        temp_sin = sinf(D_80040B14) * sp888;

        matrix_4x4_set_lookat_target(&sp8A0, temp_cos, sp88C, temp_sin + sp890, 0.0f, sp88C, sp890, 0.0f, 1.0f, 0.0f);

        matrix_4x4_multiply_in_place(&sp8A0, &sp8E0);

        gdl = set_enviro_fog_for_items_in_solo_watch_menu(sub_GAME_7F0A6EE8(gdl), sp884, &sp8E0, 0x40, 0xA0FFA03C);

        {
            s32 i;
            s32 textheight;
            s32 textwidth;
            s32 pFontFile2;
            s32 pFontChars2;
            char string_builder_allocation[2000];

#define LINEHEIGHT() 12
#define WATCH_INV_BASE_Y() 140

            textheight = 0;
            textwidth = 0;
            string_builder_allocation[0] = 0;
            pFontFile2 = ptrFontBankGothic;
            pFontChars2 = ptrFontBankGothicChars;

            for (i = 0; i < bondinvCountTotalItemsInInv(); i++)
            {
                char *name = bondinvGetNameByIndex(i);

                strcat(string_builder_allocation, name);
            }

            if (D_800409C4 > 0)
            {
                D_800409C4--;
            }

            game_options_inventory_navigation();

            x1 = 78;
            y1 = WATCH_INV_BASE_Y();

            temp_s0_3 = 1;
            temp_s0_3 = (LINEHEIGHT() * 2) + WATCH_INV_BASE_Y() + temp_s0_3;

            gdl = gfxSetup2DTextureMode(gdl);

            textMeasure(&textheight, &textwidth, string_builder_allocation, pFontChars2, pFontFile2, LINEHEIGHT());

            gdl = gfxDrawTranslucentRect(gdl, 0x4E, WATCH_INV_BASE_Y(), textwidth + 0x4E, (LINEHEIGHT() * 5) + WATCH_INV_BASE_Y(), 0);

            gdl = textRender(gdl, &x1, &y1, string_builder_allocation, pFontChars2, pFontFile2, 0xAA00B0, textwidth + 1, LINEHEIGHT() * 5, watch_inventory_text_y, LINEHEIGHT());

            gdl = gfxDrawTranslucentRect(gdl, 0x4B, temp_s0_3, textwidth + 0x52, (LINEHEIGHT() + temp_s0_3) - 2, 0x800050);

            {
                char formattedString[32];
                s32 pFontFile;
                s32 pFontChars;
                s32 x2;
                s32 y2;
                char *invItemName;

                pFontFile = ptrFontBankGothic;
                pFontChars = ptrFontBankGothicChars;
                invItemName = bondinvGetNameByIndex(g_curWatchItemIndex);

                sprintf(formattedString, "%d, %d\n%d %f\n", watch_inventory_text_y, watch_inventory_text_target_y, g_curWatchItemIndex, (f64) watch_inventory_cursor_pos);

                gdl = gfxSetup2DTextureMode(gdl);

                textMeasure(&y2, &x2, formattedString, pFontChars, pFontFile, 0);

                if (watch_inventory_text_is_settled != 0)
                {
                    textMeasure(&y2, &x2, invItemName, pFontChars, pFontFile, LINEHEIGHT());

                    x1 = 0x4E;
                    y1 = (LINEHEIGHT() * 2) + WATCH_INV_BASE_Y();

                    if (D_800409C4 == 0)
                    {
                        gdl = textRender(gdl, &x1, &y1, invItemName, pFontChars, pFontFile, 0xA0FFA0F0, x2, 0x64, 0, LINEHEIGHT());
                    }
                    else
                    {
                        gdl = textRenderOutlined(gdl, &x1, &y1, invItemName, pFontChars, pFontFile, -1, 0x7000A0, x2 + 1, 0x64, 0, LINEHEIGHT());
                    }

                    sub_GAME_7F0A8378();
                }
            }

#undef WATCH_INV_BASE_Y
#undef LINEHEIGHT
        }
    }

    return gdl;
}


void update_volume_slider_verts(struct WatchVertex *verts, f32 fill_amount, s32 transition_width)
{
    s32 i;
    struct WatchVertex *vtx;
    s32 xdiff;
    s32 filledrightx;

    xdiff = verts[2].coord1.x - verts[4].coord1.x;
    transition_width = (s32) (((f32) transition_width) * (1.2f - fill_amount));
    i = 0;
    vtx = verts;

    /**
     * Verts 0-3: unfilled right section.
     * Dark green.
     */
    do
    {
        i++;
        vtx++;
        vtx[-1].color.r = 0x20;
        vtx[-1].color.g = 0x40;
        vtx[-1].color.b = 0x20;
        vtx[-1].color.a = 0xE0;
    } while (i < 4);

    i = 4;
    vtx = &verts[4];

    /**
     * Verts 4-9: filled left section and transition.
     * Verts 10 and 11: moving boundary between transition and unfilled section.
     */
    do
    {
        filledrightx = xdiff + transition_width;

        if (i < 10)
        {
            // The filled section gets brighter as the volume increases.
            s32 rb;
            s32 g;
            rb = ((s32) (48.0f * fill_amount)) + 0x40;
            g = ((s32) (96.0f * fill_amount)) + 0x80;
            vtx->color.r = rb;
            vtx->color.g = g;
            vtx->color.b = rb;

            // Left edge of the transition band.
            if (i >= 6)
            {
                vtx->coord1.x = (s32) ((((f32) verts[4].coord1.x) + ((((f32) xdiff) + ((f32) transition_width)) * fill_amount)) - ((f32) transition_width));
                if (vtx->coord1.x < verts[4].coord1.x)
                {
                    vtx->coord1.x = verts[4].coord1.x;
                }
            }
        }
        else
        {
        // Right edge of the transition band.
        vtx->coord1.x = (s32) ((((f32) verts[4].coord1.x) + (((f32) filledrightx) * fill_amount)) + ((f32) transition_width));
            if (verts[2].coord1.x < vtx->coord1.x)
            {
                vtx->coord1.x = verts[2].coord1.x;
            }
        }

    i++;
    vtx++;

    } while (i != 12);

    // Make the unfilled section begin at the right edge of the transition area.
    filledrightx = verts[10].coord1.x;
    verts[1].coord1.x = filledrightx;
    verts[0].coord1.x = filledrightx;
}


void watch_adjust_volume_slider(u16* outVolume) {
    s32 joy_x;
    s32 adjusted_volume;

    joy_x = joyGetStickX(PLAYER_1);
    adjusted_volume = *outVolume;

    if (joyGetButtons(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD))
    {
        adjusted_volume = adjusted_volume + WATCH_VOL_ADJUST_STEP;
    } 
    else if (joyGetButtons(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD))
    {
        adjusted_volume = adjusted_volume - WATCH_VOL_ADJUST_STEP;
    }

    // Clamp stick deflection
    if (joy_x >= 71)
    {
        joy_x = 70;
    } 
    else if (joy_x < -70)
    {
        joy_x = -70;
    }

    // Increase volume
    if (joy_x >= 8)
    {
        adjusted_volume += (joy_x * 0x800 + -0x3800) / 70;
    // Decrease volume
    } 
    else if (joy_x < -7)
    {
        adjusted_volume += (joy_x * 0x800 + 0x3800) / 70;
    }

    // Clamp volume between min and max allowed volume.
    if (adjusted_volume >= VOLUME_MAX + 1)
    {
        *outVolume = VOLUME_MAX;
    } 
    else if (adjusted_volume < 0)
    {
        *outVolume = 0;
    } 
    else 
    {
        *outVolume = adjusted_volume;
    }
}


Gfx *watchDrawFXVolumeSlider(Gfx *gdl)
{
    u16 volume;
    f32 fvolume;
    struct WatchVertex *vtx1;
    struct WatchVertex *vtx;
    Gfx *cmd;

    vtx1 = (struct WatchVertex *)dynAllocateVertices(12);

    volume = sndGetSfxSlotFirstNaturalVolume();

    if (watch_item_is_actively_selected && g_WatchGameOptionsIndex == 1)
    {
        watch_adjust_volume_slider(&volume);
    }

    fvolume = (f32)(u32)volume / 32767.0f;

    sndApplyVolumeAllSfxSlot(volume);

    cmd = gdl++;
    gDPSetRenderMode(cmd, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gdl = watchDrawQuad4Vtx(gdl, OS_K0_TO_PHYSICAL(vtx1));
    vtx = setup_watch_rectangles(vtx1, 0, 0, 600, 20, -299, -205);

    gdl = watchDrawQuad4Vtx(gdl, OS_K0_TO_PHYSICAL(vtx));
    vtx = setup_watch_rectangles(vtx, 0, 0, 600, 20, -299, -205);

    gdl = watchDrawQuad4Vtx(gdl, OS_K0_TO_PHYSICAL(vtx));
    setup_watch_rectangles(vtx, 0, 0, 600, 20, -299, -205);

    update_volume_slider_verts(vtx1, fvolume, 30);

    return gdl;
}


u16 call_sndGetSfxSlotFirstNaturalVolume(void)
{
    return sndGetSfxSlotFirstNaturalVolume();
}


Gfx *watchDrawMusicVolumeSlider(Gfx *gdl)
{
    u16 volume;
    f32 fvolume;
    struct WatchVertex *vtx1;
    struct WatchVertex *vtx;
    Gfx *cmd;

    vtx1 = (struct WatchVertex *)dynAllocateVertices(12);
    volume = get_mTrack2Vol();

    if (watch_item_is_actively_selected && g_WatchGameOptionsIndex == 0)
    {
        watch_adjust_volume_slider(&volume);
    }

    fvolume = (f32)(u32)volume / 32767.0f;
    set_mTrack2Vol(volume);

    cmd = gdl++;
    gDPSetRenderMode(cmd, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gdl = watchDrawQuad4Vtx(gdl, OS_K0_TO_PHYSICAL(vtx1));
    vtx = setup_watch_rectangles(vtx1, 0, 0, 600, 20, -299, -275);


    gdl = watchDrawQuad4Vtx(gdl, OS_K0_TO_PHYSICAL(vtx));
    vtx = setup_watch_rectangles(vtx, 0, 0, 600, 20, -299, -275);

    gdl = watchDrawQuad4Vtx(gdl, OS_K0_TO_PHYSICAL(vtx));
    setup_watch_rectangles(vtx, 0, 0, 600, 20, -299, -275);

    update_volume_slider_verts(vtx1, fvolume, 30);

    return gdl;
}


u16 get_mTrack2Vol(void)
{
  return mTrack2Vol;
}


void set_mTrack2Vol(u16 param_1)
{
    mTrack2Vol = param_1;
    musicTrack2ApplySeqpVol(mTrack2Vol);
}


/**
 * Address: 7F0A9398
 *
 * This draws the text for the toggle options (both option titles and values).
 * It also draws button names and the actions mapped to them on the controller screen.
 */
Gfx *draw_options_labels(Gfx *gdl, s32 x, s32 y, char *text, u32 colour, s32 outlined, u32 outlinecolour, s32 centre, s32 drawbg, u32 bgcolour, s32 rightalign)
{
    s32 textx;
    s32 textright;
    s32 textbottom;
    s32 textwidth;
    s32 textheight;
    struct font *font;
    struct fontchar *chars;

    font = ptrFontBankGothic;
    chars = ptrFontBankGothicChars;

    textMeasure(&textheight, &textwidth, text, chars, font, 10);

    if (centre)
    {
        textx = x - (textwidth / 2);
    }
    else if (rightalign)
    {
        textx = x - textwidth;
    }
    else
    {
        textx = x;
    }

    textright = textx + textwidth;
    textbottom = y + textheight;

    if (g_WatchBackgroundGreen < 0xe0)
    {
        /**
         * Increases the effect of fuzzy static mode on the text,
         * making text pixels more subject to disappearing or almost disappearing.
         */
        gDPSetRenderMode(gdl++, G_RM_AA_PCL_SURF, G_RM_AA_PCL_SURF2);
    }
    else
    {
        gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }

    if (drawbg)
    {
        gdl = gfxDrawTranslucentRect(gdl, textx - 1, (y + outlined) + 1, textright + 1, textbottom + 1, bgcolour);
    }

    gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

    if (!outlined)
    {
        gdl = textRender(gdl, &textx, &y, text, chars, font, colour, textwidth, textheight, 0, 10);
    }

    if (outlined)
    {
        gdl = textRenderOutlined(gdl, &textx, &y, text, chars, font, colour, outlinecolour, textwidth + 1, textheight, 0, 10);
    }

    return gdl;
}


f32 sub_GAME_7F0A95C4(f32 param_1, f32 param_2, f32 param_3)
{
    if (param_1 < param_2) {
        param_1 += (param_2 - param_1) / param_3;
    } else if (param_2 < param_1) {
        param_1 -= (param_1 - param_2) / param_3;
    }

    return param_1;
}


void sub_GAME_7F0A9684(s8 contpadnum, s32 *counter, f32 *value, f32 *step)
{
    s32 count;

    if ((joyGetStickX(contpadnum) >= 10) || (joyGetStickX(contpadnum) < -9))
    {
        if (watch_item_is_actively_selected)
        {
            count = 0;

            if (controller_options_index == 1)
            {
                goto zero_done;
            }
        }
    }

    count = *counter;

    if (count < 100)
    {
        count = (*counter = count + 1);
        count = *counter;
    }

    goto counter_done;

zero_done:
    *counter = 0;

counter_done:
    if (count >= 100)
    {
        *value = sub_GAME_7F0A95C4(*value, (-(*step)) / 10.0f, 4.0f);
    }
    else if (watch_item_is_actively_selected && (controller_options_index == 1))
    {
        *value = sub_GAME_7F0A95C4(*value, (((-((f32) joyGetStickX(contpadnum))) * 0.2f) * M_TAU_F) / 360.0f, 4.0f);
    }
}


Gfx *draw_controller_style_text(Gfx *gdl)
{
    char pad[12];
    char text[2000];
    s32 x;
    s32 y;
    s32 i;
    u16 *stringids;
    s32 textheight;
    s32 textwidth;
    struct font *font;
    struct fontchar *chars;
    u8 *selectedtext;

    font = ptrFontBankGothic;
    chars = ptrFontBankGothicChars;
    textheight = 0;
    textwidth = 0;
    text[0] = '\0';
    i = 0;

    if (g_MaxControlStyles > 0)
    {
        stringids = g_GameControlStyles;

        do
        {
            strcat(text, langGet(*stringids));
            i++;
            stringids += 10;
            if (i);
        }
        while (i < g_MaxControlStyles);
    }

    if (watch_item_is_actively_selected)
    {
        if (controller_options_index == CONTROLLER_OPTIONS_INDEX_STYLE)
        {
            sub_GAME_7F0A611C(&g_CurrentPlayer->cur_player_control_type_2, &g_CurrentPlayer->cur_player_control_type_0, g_MaxControlStyles, &g_CurrentPlayer->neg_vspacing_for_control_type_entry, &g_CurrentPlayer->cur_player_control_type_1, (s32 *) (&g_CurrentPlayer->has_set_control_type_data), 0, 1, 10);
        }
    }

    x = 0xaa;
    y = 0x1a;

    textMeasure(&textheight, &textwidth, text, chars, font, 10);

    i = 10;

    gdl = textRender(gdl, &x, &y, text, chars, font, 0x00aa00b0, textwidth, i, g_CurrentPlayer->neg_vspacing_for_control_type_entry, 10);

    if (g_CurrentPlayer->has_set_control_type_data != 0)
    {
        selectedtext = langGet(*(u16 *)((u8 *) g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20)));

        textMeasure(&textheight, &textwidth, selectedtext, chars, font, 10);

        x = 0xaa;
        y = 0x1a;

        selectedtext = langGet(*(u16 *)((u8 *) g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20)));

        gdl = textRender(gdl, &x, &y, selectedtext, chars, font, 0xa0ffa0f0, textwidth, 0x64, 0, 10);
    }

    return gdl;
}

 
Gfx *sub_GAME_7F0A9AB8(Gfx *gdl)
{
    u8 *dirtext1;
    u8 *dirtext2;
 
    if (g_GameOptionEntries[0].current_value == 1)
    {
        dirtext1 = langGet(getStringID(LOPTIONS, OPTION_STR_2D_UP_LF));
        dirtext2 = langGet(getStringID(LOPTIONS, OPTION_STR_2C_DOWN_LF));
    }
    else
    {
        dirtext1 = langGet(getStringID(LOPTIONS, OPTION_STR_2C_DOWN_LF));
        dirtext2 = langGet(getStringID(LOPTIONS, OPTION_STR_2D_UP_LF));
    }
 
    {
        char strA[] = "(A)\n";
        char strB[] = "(B)\n";
        char strZ[] = "(Z)\n";
        char strL[] = "(L)\n";
        char strR[] = "(R)\n";
        char strC[] = "(C)\n";
        char strPlus[] = "(+)\n";
        char strS[] = "(S)\n";
        char str3D[] = "(3D)\n";
        u8 *ctext;
        u8 *dpadtext;
        s32 buttons;
        s32 showmovesight;
        volatile unsigned int y;
        u8 pad[8];
 
        showmovesight = 0;
        gdl = gfxSetup2DTextureMode(gdl);
 
        if (joyGetButtons(PLAYER_1, L_TRIG))
        {
            gdl = draw_options_labels(gdl, 0x32, OPTLABELS_ROW1_Y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 8)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
 
            if (*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 8) == getStringID(LOPTIONS, OPTION_STR_01_AIM_LF))
            {
                showmovesight = 1;
            }
        }
        else
        {
            gdl = draw_options_labels(gdl, 0x32, OPTLABELS_ROW1_Y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 8)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        }
 
        y = OPTLABELS_ROW2_Y;
 
        if (controller_options_index != 1 || !watch_item_is_actively_selected || !joyGetButtons(PLAYER_1, U_JPAD | D_JPAD | L_JPAD | R_JPAD))
        {
            gdl = draw_options_labels(gdl, 0x32, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 14)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        }
        else
        {
            if (joyGetButtons(PLAYER_1, U_JPAD))
            {
                if (*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 14) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                {
                    dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_30_FORWARD_LF));
                }
                else
                {
                    dpadtext = dirtext1;
                }
            }
            else if (joyGetButtons(PLAYER_1, D_JPAD))
            {
                if (*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 14) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                {
                    dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_31_BACK_LF));
                }
                else
                {
                    dpadtext = dirtext2;
                }
            }
            else if (joyGetButtons(PLAYER_1, L_JPAD))
            {
                dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_2F_SIDESTEP_LF));
            }
            else if (joyGetButtons(PLAYER_1, R_JPAD))
            {
                dpadtext = langGet(getStringID(LOPTIONS, OPTION_STR_2E_SIDESTEP_LF));
            }
 
            gdl = draw_options_labels(gdl, 0x32, y, dpadtext, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
        }
 
        y += OPTLABELS_ROW_PITCH;
        gdl = draw_options_labels(gdl, 0x32, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 16)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        y += OPTLABELS_ROW_PITCH;
 
        if (joyGetButtons(PLAYER_1, Z_TRIG))
        {
            gdl = draw_options_labels(gdl, 0x32, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 6)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
 
            if (*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 6) == getStringID(LOPTIONS, OPTION_STR_01_AIM_LF))
            {
                showmovesight = 1;
            }
        }
        else
        {
            gdl = draw_options_labels(gdl, 0x32, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 6)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
        }
 
        y -= OPTLABELS_COL_RET;
 
        if (joyGetButtons(PLAYER_1, R_TRIG))
        {
            gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 10)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
 
            if (*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 10) == getStringID(LOPTIONS, OPTION_STR_01_AIM_LF))
            {
                showmovesight = 1;
            }
        }
        else
        {
            gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 10)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        y += OPTLABELS_ROW_PITCH;
 
        if (controller_options_index != 1 || !watch_item_is_actively_selected || !joyGetButtons(PLAYER_1, U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS))
        {
            gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
        else
        {
            buttons = joyGetButtons(PLAYER_1, U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS);
 
            /* power-of-two test: exactly one C button held */
            if ((buttons & (buttons - 1U)) == 0)
            {
                if (joyGetButtons(PLAYER_1, U_CBUTTONS))
                {
                    if (*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                    {
                        ctext = langGet(getStringID(LOPTIONS, OPTION_STR_30_FORWARD_LF));
                    }
                    else
                    {
                        ctext = dirtext1;
                    }
                }
                else if (joyGetButtons(PLAYER_1, D_CBUTTONS))
                {
                    if (*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12) == getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF))
                    {
                        ctext = langGet(getStringID(LOPTIONS, OPTION_STR_31_BACK_LF));
                    }
                    else
                    {
                        ctext = dirtext2;
                    }
                }
                else if (joyGetButtons(PLAYER_1, L_CBUTTONS))
                {
                    ctext = langGet(getStringID(LOPTIONS, OPTION_STR_2F_SIDESTEP_LF));
                }
                else if (joyGetButtons(PLAYER_1, R_CBUTTONS))
                {
                    ctext = langGet(getStringID(LOPTIONS, OPTION_STR_2E_SIDESTEP_LF));
                }
 
                gdl = draw_options_labels(gdl, 0x10e, y, ctext, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
            }
            else
            {
                gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 12)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
            }
        }
 
        y += OPTLABELS_ROW_PITCH;
 
        if (joyGetButtons(PLAYER_1, B_BUTTON))
        {
            gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 4)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
        }
        else
        {
            gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 4)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        y += OPTLABELS_ROW_PITCH;
 
        if (joyGetButtons(PLAYER_1, A_BUTTON))
        {
            gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 2)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
        }
        else
        {
            gdl = draw_options_labels(gdl, 0x10e, y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 2)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        if (showmovesight)
        {
            gdl = draw_options_labels(gdl, 0xfa, OPTLABELS_HINT_Y, langGet(getStringID(LOPTIONS, OPTION_STR_08_MOVESIGHT_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
        }
        else
        {
            gdl = draw_options_labels(gdl, 0xfa, OPTLABELS_HINT_Y, langGet(*(u16 *)((u8 *)g_GameControlStyles + (g_CurrentPlayer->cur_player_control_type_0 * 20) + 18)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
        }
 
        return gdl;
    }
}


Gfx *display_text_buttons_dual_control(Gfx *gdl)
{
    s32 textptr_aux;

    gdl = gfxSetup2DTextureMode(gdl);

    if (joyGetButtons(PLAYER_1, A_BUTTON))
    {
        gdl = draw_options_labels(gdl, 0x5A, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0); //weapon
    }
    else
    {
        gdl = draw_options_labels(gdl, 0x5A, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0); //weapon
    }

    if (joyGetButtons(PLAYER_1, B_BUTTON))
    {
        gdl = draw_options_labels(gdl, 0x5A, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0); //action
    }
    else
    {
        gdl = draw_options_labels(gdl, 0x5A, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0); //action
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_GALORE))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF)); //fire
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_01_AIM_LF)); //aim
    }

    if (joyGetButtons(PLAYER_1, Z_TRIG))
    {
        gdl = draw_options_labels(gdl, 0x5A, YOFFSET_5, textptr_aux, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0);
    }
    else
    {
        gdl = draw_options_labels(gdl, 0x5A, YOFFSET_5, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_DOMINO))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF)); //move
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF)); //look
    }

    gdl = draw_options_labels(gdl, 0x5A, YOFFSET_4, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 0);

    if (joyGetButtons(1, A_BUTTON))
    {
        gdl = draw_options_labels(gdl, 0xE6, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1); //weapon
    }
    else
    {
        gdl = draw_options_labels(gdl, 0xE6, YOFFSET_WEAPTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1); //weapon
    }

    if (joyGetButtons(1, B_BUTTON))
    {
        gdl = draw_options_labels(gdl, 0xE6, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1); //action
    }
    else
    {
        gdl = draw_options_labels(gdl, 0xE6, YOFFSET_ACTIONTEXT, langGet(getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF)), 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1); //action
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_GALORE))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_01_AIM_LF)); //aim
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF)); //fire
    }

    if (joyGetButtons(1, Z_TRIG))
    {
        gdl = draw_options_labels(gdl, 0xE6, YOFFSET_5, textptr_aux, -1, 1, 0x7000A0, 0, 0, 0x3000B0, 1);
    }
    else
    {
        gdl = draw_options_labels(gdl, 0xE6, YOFFSET_5, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
    }

    if ((g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_PLENTY) || (g_CurrentPlayer->cur_player_control_type_0 == CONTROLLER_CONFIG_DOMINO))
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF)); //look
    }
    else
    {
        textptr_aux = langGet(getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF)); //move
    }

    gdl = draw_options_labels(gdl, 0xE6, YOFFSET_4, textptr_aux, 0xAA00B0, 0, -1, 0, 0, 0x3000B0, 1);
    return gdl;
}


/**
 * Address: 7F0AADC0
 * 
 * Draw the controller model(s) and the individual buttons.
 */
Gfx *watchDrawController(Gfx *gdl)
{
    Mtx *perspmtx;
    Mtxf identity;
    Mtxf zrotmtx;
    Mtxf xrotmtx;
    Mtxf tmpmtx1;
    Mtxf tmpmtx2;
    Mtxf modelmtx;
    Mtxf finalmtx;
    coord3d pos;
    u16 perspNorm;
    Mtxf lookat1;
    Mtxf lookat2;
    void *watchTable;
    s32 green;
    s8 contpadnum0;
    s8 contpadnum1;
    WatchContButtonPositions table0;
    WatchContButtonPositions table1;
    WatchContButtonPositions table2;
    Gfx *cmd0;
    Gfx *cmd1;

    perspmtx = dynAllocateMatrix();
    pos = g_ControllerPos;
    contpadnum0 = 0;
    contpadnum1 = 1;
    table0 = g_1ContButtonPositions[0];
    table1 = g_2ContLeftButtonPositions[0];
    table2 = g_2ContRightButtonPositions[0];

    sub_GAME_7F0A9684(0, &D_80040B2C, &g_WatchControllerSpinSpeed, &g_WatchControllerSpinAngle);
    matrix_4x4_set_identity(&identity);

    g_WatchControllerSpinAngle += (g_WatchControllerSpinSpeed * ((f32) WATCH_ROTATION_FRAMES)) * 0.5f;
    g_WatchControllerSpinAngle = watchWrapAroundPI(g_WatchControllerSpinAngle);

    matrix_4x4_set_rotation_around_z(g_WatchControllerSpinAngle, &zrotmtx);

    if (watch_item_is_actively_selected && (controller_options_index == CONTROLLER_OPTIONS_INDEX_INPUTS))
    {
        g_WatchControllerPitch = sub_GAME_7F0A95C4(g_WatchControllerPitch, (((f32) joyGetStickY(PLAYER_1)) * M_TAU_F) / 360.0f, 4.0f);
    }
    else
    {
        g_WatchControllerPitch = sub_GAME_7F0A95C4(g_WatchControllerPitch, 0.0f, 4.0f);
    }

    matrix_4x4_set_rotation_around_x((-g_WatchControllerPitch) - 0.78539819f, &xrotmtx);
    matrix_4x4_multiply(&identity, &zrotmtx, &tmpmtx1);
    matrix_4x4_multiply(&tmpmtx1, &xrotmtx, &tmpmtx2);
    matrix_4x4_set_identity_and_position(&pos, &tmpmtx1);
    matrix_4x4_multiply(&tmpmtx1, &tmpmtx2, &modelmtx);

    if (controllerCheckDualControllerTypesAllowed())
    {
        f32 eye = 495.0f;

        watchTable = &table1;

        if (1);

        matrix_4x4_set_lookat_target(&lookat1, eye, 2500.0f, 32.0f, eye, 0.0f, 32.0f, 0.0f, 0.0f, -1.0f);
    }
    else
    {
        watchTable = &table0;
        matrix_4x4_set_lookat_target(&lookat1, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
    }

    matrix_4x4_multiply(&lookat1, &modelmtx, &finalmtx);

    guPerspective(perspmtx, &perspNorm, WATCH_PERSPECTIVE_FOVY, WATCH_PERSPECTIVE_ASPECT, 1000.0f, 3000.0f, 1.0f);

    cmd0 = gdl++; \
    gSPMatrix(cmd0, osVirtualToPhysical(perspmtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    gdl = sub_GAME_7F0A6EE8(gdl);
    green = g_WatchBackgroundGreen;

    if (green < 0xe0)
    {
        gdl = watchRenderController(gdl, &finalmtx, green - 6, 1, watchTable, &contpadnum0);
    }
    else
    {
        gdl = watchRenderControllerOpaque(gdl, &finalmtx, 1, (s32) watchTable, &contpadnum0);
    }

    if (controllerCheckDualControllerTypesAllowed())
    {
        sub_GAME_7F0A9684(1, &D_80040B3C, &D_80040B38, &D_80040B34);
        matrix_4x4_set_identity(&identity);

        D_80040B34 += (D_80040B38 * ((f32) WATCH_ROTATION_FRAMES)) * 0.5f;
        D_80040B34 = watchWrapAroundPI(D_80040B34);

        matrix_4x4_set_rotation_around_z(D_80040B34, &zrotmtx);

        if (watch_item_is_actively_selected && (controller_options_index == CONTROLLER_OPTIONS_INDEX_INPUTS))
        {
            D_80040B30 = sub_GAME_7F0A95C4(D_80040B30, (((f32) joyGetStickY(PLAYER_2)) * M_TAU_F) / 360.0f, 4.0f);
        }
        else
        {
            D_80040B30 = sub_GAME_7F0A95C4(D_80040B30, 0.0f, 4.0f);
        }

        matrix_4x4_set_rotation_around_x((-D_80040B30) - 0.78539819f, &xrotmtx);
        matrix_4x4_multiply(&identity, &zrotmtx, &tmpmtx1);
        matrix_4x4_multiply(&tmpmtx1, &xrotmtx, &tmpmtx2);
        matrix_4x4_set_identity_and_position(&pos, &tmpmtx1);
        matrix_4x4_multiply(&tmpmtx1, &tmpmtx2, &modelmtx);
        matrix_4x4_set_lookat_target(&lookat2, -505.0f, 2500.0f, 32.0f, -505.0f, 0.0f, 32.0f, 0.0f, 0.0f, -1.0f);
        matrix_4x4_multiply(&lookat2, &modelmtx, &finalmtx);

        guPerspective(perspmtx, &perspNorm, WATCH_PERSPECTIVE_FOVY, WATCH_PERSPECTIVE_ASPECT, 1000.0f, 3000.0f, 1.0f);

        /**
         * This should be something like:
         * 
         *   cmd1 = gdl++;
         *   gSPMatrix(cmd1, osVirtualToPhysical(perspmtx),G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
         * 
         * but it doesn't match for some reason.
         */
        cmd1 = gdl++; cmd1->words.w0 = 0x01030040; cmd1->words.w1 = osVirtualToPhysical(perspmtx);

        gdl = sub_GAME_7F0A6EE8(gdl);
        green = g_WatchBackgroundGreen;

        if (green < 0xe0)
        {
            gdl = watchRenderController(gdl, &finalmtx, green - 6, 1, &table2, &contpadnum1);
        }
        else
        {
            gdl = watchRenderControllerOpaque(gdl, &finalmtx, 1, (s32) (&table2), &contpadnum1);
        }
    }

    if (controllerCheckDualControllerTypesAllowed())
    {
        gdl = display_text_buttons_dual_control(gdl);
    }
    else
    {
        gdl = sub_GAME_7F0A9AB8(gdl);
    }

    return gdl;
}


void watchResetControllerOptionsIndex(void)
{
    controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
}


void watchResetGameOptionsIndex(void)
{
    g_WatchGameOptionsIndex = 0;
}


void watchSetAbortIsConfirmSelectedFalse(void)
{
    g_WatchAbortIsConfirmSelected = FALSE;
}


Gfx *draw_watch_control_options_page(Gfx *gdl, Mtx *param_2)
{
    s32 phi_s1;
    u16 *textptr;
    s32 sp5C;
    s32 sp58;
    s32 sp54;
    s32 sp50;
    s32 pFontFile;
    s32 pFontChars;

    gdl = draw_background_health_and_armor(gdl, param_2, 0);

    if (check_watch_page_transistion_running() != 1) {

        gdl = watchDrawController(gdl);
        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;

        gdl = gfxSetup2DTextureMode(gdl);
        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_32_CONTROLSTYLE_LF)); //control style

        sp5C = XOFFSET_1;
        sp58 = 0x1A;
        phi_s1 = 0xFF00B0;

        if (controller_options_index == CONTROLLER_OPTIONS_INDEX_STYLE)
        {
            phi_s1 = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0)
            {
                phi_s1 = -1;
            }
        }

        textMeasure(&sp50, &sp54, textptr, pFontChars, pFontFile, 0);

        if ((watch_item_is_actively_selected != 0) && (controller_options_index == CONTROLLER_OPTIONS_INDEX_STYLE))
        {
            gdl = textRenderOutlined(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, 0x7000A0, sp54 + 1, sp50, 0, 0);
        } else
        {
            gdl = textRender(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, sp54, sp50, 0, 0);
        }

        gdl = draw_controller_style_text(gdl);
        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_33_CONTROLLER_LF)); //controller;

        phi_s1 = 0xFF00B0;

        if (controllerCheckDualControllerTypesAllowed())
        {
            textptr = langGet(getStringID(LOPTIONS, OPTION_STR_34_CONTROLLERS_LF)); //controllers;
        }
    
        sp5C = XOFFSET_1;
        sp58 = 0x2B;

        if (controller_options_index == CONTROLLER_OPTIONS_INDEX_INPUTS)
        {
            phi_s1 = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0)
            {
                phi_s1 = -1;
            }
        }

        textMeasure(&sp50, &sp54, textptr, pFontChars, pFontFile, 0);

        if ((watch_item_is_actively_selected != 0) && (controller_options_index == CONTROLLER_OPTIONS_INDEX_INPUTS))
        {
            gdl = textRenderOutlined(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, 0x7000A0, sp54 + 1, sp50, 0, 0);
        } 
        else
        {
            gdl = textRender(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, sp54, sp50, 0, 0);
        }

    }

    return gdl;
}


void watchSelectGameOption(u32 *new_option, u32 old_option)
{
    *new_option = old_option;
    watchSetStickXDisabled();
    sndPlaySfx(g_musicSfxBufferPtr, OPTION_CHOOSE_SFX, NULL);
}


void game_option_toggle_input(s32 option_index)
{
    if ( (joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD) || watchShouldNavLeft()) && watch_item_is_actively_selected )
    {
        if (g_GameOptionEntries[option_index].current_value == 1)
        {
            watchSelectGameOption(&g_GameOptionEntries[option_index].current_value, 0);
        }
        else if (g_GameOptionEntries[option_index].current_value == 2)
        {
            watchSelectGameOption(&g_GameOptionEntries[option_index].current_value, 1);
        }
    }
    else
    {
        if ( (joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD) || watchShouldNavRight()) && watch_item_is_actively_selected )
        {
            if (g_GameOptionEntries[option_index].current_value == 0)
            {
                watchSelectGameOption(&g_GameOptionEntries[option_index].current_value, 1);
            }
            else if ( (g_GameOptionEntries[option_index].current_value == 1) && g_GameOptionEntries[option_index].text[3] )
            {
                watchSelectGameOption(&g_GameOptionEntries[option_index].current_value, 2);
            }
        }
    }
}


/**
 * Set the color and draw the text for the values of the toggle options.
 * For example, draw the "ON" and "OFF" text for the Auto-Aim option,
 * but not the "AUTO-AIM" text itself.
 *
 * Options are highlighted by using the controller to advance up and down the toggle options list,
 * but options are not selected until the A button is pressed.
 */
Gfx *watchDrawToggleOptionValues(Gfx *gdl, s32 y, s32 option_index, u32 state)
{
    s32 colour1;
    s32 colour2;
    s32 colour3;
    s32 x1;
    s32 x2;
    struct game_options *entry;
    struct game_options *drawentry;

    colour1 = 0x00800080;
    colour2 = 0x00800080;
    colour3 = 0x00800080;

    entry = &g_GameOptionEntries[option_index];

    x1 = 0xB4;
    x2 = 0xE1;

    // Option is unhighlighted
    if (state == 0)
    {
        goto state_unhighlighted;
    }
    // Option is highlighted
    else if (state == 1)
    {
        goto state_highlighted;
    }
    // Option is selected
    else if (state == 2)
    {
        goto state_selected;
    }
    goto after_state;

state_unhighlighted:
    entry = &g_GameOptionEntries[option_index];

    if (entry->current_value == 0)
    {
        colour1 = 0x00FF00B0;
    }
    else if (entry->current_value == 1)
    {
        colour2 = 0x00FF00B0;
    }
    else if (entry->current_value == 2)
    {
        colour3 = 0x00FF00B0;
    }

    goto after_state;

/**
 * Sets color of the active value of the highlighted option.
 * These use the same colors as the active values of the unhighlighted options,
 * so changing the highlighted option has no visual effect.
 */
state_highlighted:
    entry = &g_GameOptionEntries[option_index];
    if (entry->current_value == 0)
    {
        colour1 = 0x00FF00B0;
    }
    else if (entry->current_value == 1)
    {
        colour2 = 0x00FF00B0;
    }
    else if (entry->current_value == 2)
    {
        colour3 = 0x00FF00B0;
    }
    goto after_state;

/**
 * Make the active value of the selected option extra bright.
 */
state_selected:
    game_option_toggle_input(option_index);
    entry = &g_GameOptionEntries[option_index];
    if (entry->current_value == 0)
    {
        colour1 = 0xA0FFA0F0;
    }
    else if (entry->current_value == 1)
    {
        colour2 = 0xA0FFA0F0;
    }
    else if (entry->current_value == 2)
    {
        colour3 = 0xA0FFA0F0;
    }

after_state:
    if (entry->text[3] == 0)
    {
        x1 = 0xC8;
        x2 = 0xFA; 
    }

    drawentry = entry;

    gdl = draw_options_labels(gdl, x1, y, langGet(drawentry->text[1]), colour1, 0, -1, 1, 0, 0x3000B0, 0); // Draw text of option's first value e.g. "Full" for the Screen option.
    gdl = draw_options_labels(gdl, x2, y, langGet(drawentry->text[2]), colour2, 0, -1, 1, 0, 0x3000B0, 0); // Draw text of option's second value e.g. "Wide" for the Screen option.

    if (drawentry->text[3])
    {
        gdl = draw_options_labels(gdl, 0x10E, y, langGet(drawentry->text[3]), colour3, 0, -1, 1, 0, 0x3000B0, 0); // Draw text of option's third value e.g. "Cinema" for the Screen option.
    }

    return gdl;
}


Gfx *watchDrawToggleOptions(Gfx *gdl)
{
    s32 y_offset;
    s32 i;

    gdl = gfxSetup2DTextureMode(gdl);

    for (i = 0, y_offset = YOFFSET_1; i < 8; i = i + 1, y_offset = y_offset + YINC) {

        if ( i == g_WatchGameOptionsIndex - 2)
        {
            // Draw option that is highlighted and selected, if there is one.
            if (watch_item_is_actively_selected)
            {
                gdl = watchDrawToggleOptionValues(draw_options_labels(gdl, XOFFSET_1, y_offset, langGet(g_GameOptionEntries[i].text[0]), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0), y_offset, i, 2);
            }
            // Draw option that is highlighted but not selected, if there is one.
            else
            {
                gdl = watchDrawToggleOptionValues(draw_options_labels(gdl, XOFFSET_1, y_offset, langGet(g_GameOptionEntries[i].text[0]), 0xA0FFA0F0, 0, -1, 0, 0, 0x3000B0, 0), y_offset, i, 1);
            }
        }
        // Draw the options that are neither highlighted nor selected.
        else
        {
            gdl = watchDrawToggleOptionValues(draw_options_labels(gdl, XOFFSET_1, y_offset, langGet(g_GameOptionEntries[i].text[0]), 0xFF00B0, 0, -1, 0, 0, 0x3000B0, 0), y_offset, i, 0);
        }

    }

    return gdl;
}


Gfx *watchDrawGameOptionsScreen(Gfx *gdl, Mtx *param_2)
{
    s32 sp5C;
    u16 *textptr;
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    s32 sp48;

    s32 pFontFile;
    s32 pFontChars;

    gdl = draw_background_health_and_armor(gdl, param_2, 0);

    if (check_watch_page_transistion_running() != 1)
    {
        gdl = watchDrawMusicVolumeSlider(gdl);
        gdl = watchDrawFXVolumeSlider(gdl);
        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;
        gdl = gfxSetup2DTextureMode(gdl);

        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_35_MUSIC_LF)); /* music */

        sp54 = XOFFSET_1;
        sp50 = YOFFSET_8;

        sp5C = 0xFF00B0;

        if (g_WatchGameOptionsIndex == 0)
        {
            sp5C = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0)
            {
                sp5C = -1;
            }
        }

        textMeasure(&sp48, &sp4C, textptr, pFontChars, pFontFile, 0);

        if ((watch_item_is_actively_selected != 0) && (g_WatchGameOptionsIndex == 0))
        {
            gdl = textRenderOutlined(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, 0x7000A0, sp4C + 1, sp48, 0, 0);
        }
        else
        {
            gdl = textRender(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, sp4C, sp48, 0, 0);
        }

        sp5C = 0xFF00B0;
        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_36_FX_LF)); /* fx */

        sp54 = XOFFSET_1;
        sp50 = YOFFSET_9;


        if (g_WatchGameOptionsIndex == 1)
        {
            sp5C = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0)
            {
                sp5C = -1;
            }
        }

        textMeasure(&sp48, &sp4C, textptr, pFontChars, pFontFile, 0);

        if ((watch_item_is_actively_selected != 0) && (g_WatchGameOptionsIndex == 1))
        {
            gdl = textRenderOutlined(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, 0x7000A0, sp4C + 1, sp48, 0, 0);
        }
        else
        {
            gdl = textRender(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, sp4C, sp48, 0, 0);
        }

        gdl = watchDrawToggleOptions(gdl);
    }

    return gdl;
}


s32 sub_GAME_7F0AC0E8(u8 *arg)
{
    u8 cVar1;
    s32 count;

    cVar1 = *arg;
    count = 0;

    while (cVar1)
    {
        if (cVar1 == 10)
        {
            count = count + 1;
        }

        cVar1 = arg[1];
        arg += 1;
    }

    return count;
}


u8 *sub_GAME_7F0AC120(u8 *arg)
{
    u8 *ret;

    ret = arg;

    while (*arg != 0)
    {
        if (*arg == 0xa)
        {
            if (*++arg != 0)
            {
                ret = arg;
            }
        }
#if defined(VERSION_EU)
        else
        {
            arg++;
        }
#else
            arg++;
#endif
    }

    return ret;
}


#if defined(VERSION_EU)
//D:800577C0
const char D_800577C0[] = "\n";
#endif


Gfx *draw_watch_mission_briefing_page(Gfx *gdl, Mtx *param_2)
{
    gdl = draw_background_health_and_armor(gdl, param_2, 0);

    if (check_watch_page_transistion_running() != 1)
    {
        s32 boxLeft;
        s32 boxTop;
        s32 boxRight;
        s32 boxBottom;
        s32 textHeight = 0;
        s32 textWidth = 0;

        struct font *font = ptrFontBankGothic;
        struct fontchar *chars = ptrFontBankGothicChars;

#if defined(VERSION_EU)
            char wrappedText[3000];
#else
            char wrappedText[3000] = "\n";
#endif

        char pageTitle[0x20];
        char *completeText;
        char *incompleteText;
        char *failedText;
        char *titleText;

        s32 lineCount;
        s32 objectiveRow;
        s32 objY;
        s32 objX;

        completeText = langGet(0xac28);
        incompleteText = langGet(0xac29);
        failedText = langGet(0xac37);
        titleText = get_ptr_text_for_watch_breifing_page(BRIEFING_TITLE);

#if defined(VERSION_EU)
        strcpy(wrappedText, D_800577C0);
#endif

        gdl = gfxSetup2DTextureMode(gdl);
        textMeasure(&textHeight, &textWidth, titleText, chars, font, 0);

        boxLeft = ((0xaa - textWidth) / 2) + 0x4b;
        boxTop = 0x1e;
        boxRight = boxLeft + textWidth;
        boxBottom = boxTop + textHeight;

        gdl = draw_blackbox_to_screen(gdl, &boxLeft, &boxTop, &boxRight, &boxBottom);
        gdl = textRender(gdl, &boxLeft, &boxTop, titleText, chars, font, 0xa0ffa0f0, textWidth, textHeight, 0, 0);

        boxLeft = 0x41;

        switch (mission_brief_index)
        {
            case BRIEF_INDEX_BACKGROUND:
                sprintf(pageTitle, langGet(0xac38));
                textWrap(0xd2, get_ptr_text_for_watch_breifing_page(BRIEFING_OVERVIEW), wrappedText, chars, font);
                mission_brief_background_navigation();
                break;

            case BRIEF_INDEX_M:
                sprintf(pageTitle, langGet(0xac39));
                textWrap(0xd2, get_ptr_text_for_watch_breifing_page(BRIEFING_M), wrappedText, chars, font);
                mission_brief_m_briefing_navigation();
                break;

            case BRIEF_INDEX_Q:
                sprintf(pageTitle, langGet(0xac3a));
                textWrap(0xd2, get_ptr_text_for_watch_breifing_page(BRIEFING_Q), wrappedText, chars, font);
                mission_brief_q_branch_navigation();
                break;

            case BRIEF_INDEX_MONEYPENNY:
                sprintf(pageTitle, langGet(0xac3b));
                textWrap(0xd2, get_ptr_text_for_watch_breifing_page(BRIEFING_MONEYPENNY), wrappedText, chars, font);
                mission_brief_moneypenny_navigation();
                break;

            case BRIEF_INDEX_OBJECTIVES:
            {
                char objectiveBuffer[200] = "";
                u32 colour;
                s32 i;
                s32 j;
                char *objectiveText;
                s32 visibleObjectiveIndex;
                s32 pad;
                char *objectiveLetterPtr;

                lineCount = 1;
                objectiveRow = 0;
                visibleObjectiveIndex = 0;

                setTextOverlapCorrection(5);
                sprintf(pageTitle, langGet(0xac3c));

                for (i = 0; i < objectiveGetCount(); i++)
                {
                    if (get_difficulty_for_objective(i) <= lvGetSelectedDifficulty())
                    {
                        objectiveText = get_text_for_objective(i);
                        objectiveBuffer[0] = '\0';

                        for (j = 0; j < lineCount; j++)
                        {
                            strcat(objectiveBuffer, D_80058440);
                        }

                        for (j = 0; j < objectiveRow; j++)
                        {
                            strcat(objectiveBuffer, D_80058444);
                        }

                        objectiveLetterPtr = objectiveBuffer + strlen(objectiveBuffer);

                        sprintf(objectiveLetterPtr, aC_2, visibleObjectiveIndex + 'a');
                        strcat(objectiveBuffer, objectiveText);

                        objY = boxTop + (5);
                        objX = 0x3c;

                        gdl = textRender(gdl, &objX, &objY, objectiveBuffer, chars, font, 0x00ff00b0, viGetX(), viGetY(), 0, 10);
                        lineCount += sub_GAME_7F0AC0E8(objectiveLetterPtr);
                        textMeasure(&textHeight, &textWidth, sub_GAME_7F0AC120(objectiveLetterPtr), chars, font, 10);

                        if ((textWidth + 0x3c) < (viGetX() / 2))
                        {
                            lineCount--;
                        }

                        objectiveBuffer[0] = '\0';

                        for (j = 0; j < lineCount; j++)
                        {
                            strcat(objectiveBuffer, D_80058450);
                        }
                        for (j = 0; j < objectiveRow; j++)
                        {
                            strcat(objectiveBuffer, D_80058454);
                        }

                        switch (get_status_of_objective(i))
                        {
                            case OBJECTIVESTATUS_INCOMPLETE:
                                strcat(objectiveBuffer, incompleteText);
                                colour = (D_80040AFC << 16) | 0x400040ff;
                                break;

                            case OBJECTIVESTATUS_COMPLETE:
                                strcat(objectiveBuffer, completeText);
                                colour = 0xa0ffa0f0;
                                break;

                            case OBJECTIVESTATUS_FAILED:
                                strcat(objectiveBuffer, failedText);
                                colour = 0x00ff00b0;
                                break;

                            default:
                                break;
                        }

                        textMeasure(&textHeight, &textWidth, objectiveBuffer, chars, font, 10);
                        objY = boxTop + 5;
                        objX = 0xaf;

                        gdl = textRender(gdl, &objX, &objY, objectiveBuffer, chars, font, colour, 0xd2, viGetY(), 0, 10);

                        objectiveRow++;
                        visibleObjectiveIndex++;
                    }
                }

                setTextOverlapCorrection(-1);
                mission_brief_objectives_navigation();

                break;

            }
        }

        textMeasure(&textHeight, &textWidth, pageTitle, chars, font, 10);
        gdl = textRender(gdl, &boxLeft, &boxTop, pageTitle, chars, font, 0xa0ffa0f0, 0xd2, 0x82, 0, 10);

        boxTop += 5;
        boxLeft = 0x3c;

        textMeasure(&textHeight, &textWidth, wrappedText, chars, font, 10);
        gdl = textRender(gdl, &boxLeft, &boxTop, wrappedText, chars, font, 0x00ff00b0, viGetX(), viGetY(), 0, 10);
    }

    return gdl;
}


//D:80058440
const char D_80058440[] = " \n";
//D:80058444
const char D_80058444[] = " \n\n";
//D:80058448
const char aC_2[] = "%c: ";
//D:80058450
const char D_80058450[] = " \n";
//D:80058454
const char D_80058454[] = " \n\n";


Gfx *optionsDrawCurrentWatchPage(Gfx *gdl, Mtx *arg1, s32 watch_transitioning)
{
    set_page_rectangle_colors(watch_screen_index, (struct WatchVertex *)g_CurrentPlayer->buffer_for_watch_greenbackdrop_vertices);

    if (watch_transitioning == TRUE)
    {
        set_BONDdata_outside_watch_menu_flag(FALSE);
        lvSetBgRenderEnabled(FALSE);

        // Handle A or Z button click when in any page but inventory page
        if ((watch_screen_index != WATCH_INDEX_INVENTORY) && (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON)))
        {
            watchPlayBeep();
        }

        switch (watch_screen_index)
        {
            case WATCH_INDEX_MISSION_STATUS:
                gdl = draw_watch_mission_status_page(gdl, arg1);
                break;
            case WATCH_INDEX_INVENTORY:
                gdl = draw_watch_inventory_page(gdl, arg1);
                break;
            case WATCH_INDEX_CONTROL_OPTIONS:
                gdl = draw_watch_control_options_page(gdl, arg1);
                break;
            case WATCH_INDEX_GAME_OPTIONS:
                gdl = watchDrawGameOptionsScreen(gdl, arg1);
                break;
            case WATCH_INDEX_MISSION_BRIEFING:
                gdl = draw_watch_mission_briefing_page(gdl, arg1);
        }
    }
    else if (watch_transitioning == FALSE)
    {
        lvSetBgRenderEnabled(TRUE);
        set_BONDdata_outside_watch_menu_flag(TRUE);
        gdl = draw_background_health_and_armor_transitioning(gdl, arg1);
    }

    return gdl;
}


Gfx *watchDrawQuad4Vtx(Gfx *gdl, uintptr_t vtxPhysAddr)
{
    gSPVertex(gdl++, vtxPhysAddr, 4, 0);
    gSP4Triangles(gdl++, 0,1,2,  1,2,3,  0,0,0,  0,0,0);   /* quad over the 4 verts */

    return gdl;
}
