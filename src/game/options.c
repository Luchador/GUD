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
#include "language.h"
#include "player.h"
#include "textrelated.h"
#include "glass.h"
#include "frametiming.h"
#include "assets/obseg/text/LoptionE.h"

#define WATCH_BACKGROUND_VERTEX_COUNT 30

#define WATCH_VOL_ADJUST_STEP 1024

#if defined(VERSION_US)
#define WATCH_ROTATION_FRAMES speedgraphframes
#else
#define WATCH_ROTATION_FRAMES jpD_800484D0
#endif

#if defined(VERSION_EU)
#define WATCH_PERSPECTIVE_FOVY    52.5f
#define WATCH_PERSPECTIVE_ASPECT  1.283847f
#else
#define WATCH_PERSPECTIVE_FOVY    50.5f
#define WATCH_PERSPECTIVE_ASPECT  1.3333334f
#endif

// bss
Mtx gfx_background_8007B0A0;
Mtx gfx_background_8007B0E0;



u32 D_80040990 = 0;
u32 watch_screen_index = 0;
u32 controller_options_index = 0;
u32 game_options_index = 0;
// data
//D:800409A0
s32 mission_brief_index = BRIEF_INDEX_OBJECTIVES;
//D:800409A4
s32 D_800409A4 = 0;
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
//D:800409C8
f32 D_800409C8 = 0.0f;
//D:800409CC
f32 D_800409CC = 0.0f;
//D:800409D0
s32 D_800409D0 = -1;
//D:800409D4
f32 D_800409D4 = 0.0f;
//D:800409D8
s32 D_800409D8 = 8;
//D:800409DC
u16 D_800409DC[] = {
    /*1.1 honey*/    getStringID(LOPTIONS, OPTION_STR_09_11HONEY_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF), /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF),
    /*1.2 solitaire*/getStringID(LOPTIONS, OPTION_STR_0A_12SOLITAIRE_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF), /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF),    /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF),
    /*1.3 kissy*/    getStringID(LOPTIONS, OPTION_STR_0B_13KISSY_LF),   /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF),  /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF),
    /*1.4 goodnight*/getStringID(LOPTIONS, OPTION_STR_0C_14GOODNIGHT_LF),   /*fire*/getStringID(LOPTIONS, OPTION_STR_00_FIRE_LF), /*action*/getStringID(LOPTIONS, OPTION_STR_02_ACTION_LF),  /*aim*/getStringID(LOPTIONS, OPTION_STR_01_AIM_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*weapon*/getStringID(LOPTIONS, OPTION_STR_03_WEAPON_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*move*/getStringID(LOPTIONS, OPTION_STR_05_MOVE_LF), /*pause*/getStringID(LOPTIONS, OPTION_STR_04_PAUSE_LF), /*look*/getStringID(LOPTIONS, OPTION_STR_06_LOOK_LF),
    /*2.1 plenty*/   getStringID(LOPTIONS, OPTION_STR_0D_21PLENTY_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.2 galore*/   getStringID(LOPTIONS, OPTION_STR_0E_22GALORE_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.3 domino*/   getStringID(LOPTIONS, OPTION_STR_0F_23DOMINO_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),
    /*2.4 goodhead*/ getStringID(LOPTIONS, OPTION_STR_10_24GOODHEAD_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),      /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),     /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF),    /*?*/getStringID(LOPTIONS, OPTION_STR_07_QUESTION_LF)
};

struct game_options game_options_entries[] = {
    { {getStringID(LOPTIONS, OPTION_STR_11_LOOKUPDOWN_LF), getStringID(LOPTIONS, OPTION_STR_1C_REVERSE_LF), getStringID(LOPTIONS, OPTION_STR_1B_UPRIGHT_LF), 0}, 0}, //look up/down, reverse, upright
    { {getStringID(LOPTIONS, OPTION_STR_12_AUTOAIM_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //autoaim, off, on
    { {getStringID(LOPTIONS, OPTION_STR_14_AIMCONTROL_LF), getStringID(LOPTIONS, OPTION_STR_1E_HOLD_LF), getStringID(LOPTIONS, OPTION_STR_1D_TOGGLE_LF), 0}, 0}, //aim control, hold, toggle
    { {getStringID(LOPTIONS, OPTION_STR_15_SIGHTONSCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //sight on screen, off, on
    { {getStringID(LOPTIONS, OPTION_STR_13_LOOKAHEAD_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //look ahead, off, on
    { {getStringID(LOPTIONS, OPTION_STR_16_AMMOONSCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1A_OFF_LF), getStringID(LOPTIONS, OPTION_STR_19_ON_LF), 0}, 1}, //ammo on screen, off, on
    { {getStringID(LOPTIONS, OPTION_STR_17_SCREEN_LF), getStringID(LOPTIONS, OPTION_STR_1F_FULL_LF), getStringID(LOPTIONS, OPTION_STR_20_WIDE_LF), getStringID(LOPTIONS, OPTION_STR_21_CINEMA_LF)}, 0}, //screen, full, wide, cinema
    { {getStringID(LOPTIONS, OPTION_STR_18_RATIO_LF), getStringID(LOPTIONS, OPTION_STR_22_NORMAL_LF), getStringID(LOPTIONS, OPTION_STR_23_169_LF), 0}, 0} //ratio, normal, 16:9
};

//D:80040ADC
u32 controlstick_lr_enabled = 0;
//D:80040AE0
u32 watch_stick_y_nav_ready = 0;
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
u32 D_80040AF8 = 0xA;
//D:80040AFC
u32 D_80040AFC = 0xFF;

//D:80040B00
u32 D_80040B00 = 0xA;

//D:80040B04
s32 g_WatchBackgroundGreen = 0xE0;

//D:80040B08
u32 D_80040B08 = 0;

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
s32 D_80040B40 = 0;
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

void set_page_rectangle_colors(s32 watch_screen_index, struct WatchVertex *arg2);
Gfx *draw_watch_mission_status_page(Gfx *gdl, Mtx *param_2);
Gfx *unused_draw_watch_inventory_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_watch_inventory_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_watch_control_options_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_watch_game_options_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_watch_mission_briefing_page(Gfx *gdl, Mtx *param_2);
Gfx *draw_background_health_and_armor_transitioning(Gfx *gdl, Mtx *param_2);
Gfx *draw_background_health_and_armor(Gfx *gdl, Mtx *arg1, s32 zoom_squish);
void sub_GAME_7F0A68D8(s32 *arg0);
void game_option_select_value(u32 *param_1, u32 param_2);
void watch_adjust_volume_slider(u16* arg0);
Gfx *sub_GAME_7F0A3B40(Gfx *gdl, s32 *arg1);
void update_volume_slider_verts(struct WatchVertex *verts, f32 fill_amount, s32 transition_width);
void sub_GAME_7F0A9684(s8 contpadnum, s32 *counter, f32 *value, f32 *step);
Gfx *display_text_buttons_dual_control(Gfx *gdl);
Gfx *sub_GAME_7F0A9AB8(Gfx *gdl);

// end forward declarations



void nullsub_7F0A4860(void)
{

}


void init_watch_at_start_of_stage(int stage)
{
    watch_screen_index = WATCH_INDEX_MISSION_STATUS;
    controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
    game_options_index = GAME_OPTIONS_INDEX_MUSIC;
    mission_brief_index = BRIEF_INDEX_OBJECTIVES;
    D_800409A4 = 0;
    watch_item_is_actively_selected = 0;
    D_800409AC = 0;
    watch_inventory_text_y = 0;
    watch_inventory_text_target_y = 0;
    g_curWatchItemIndex = 0;
    watch_inventory_cursor_pos = 0.0f;
    watch_inventory_text_is_settled = FALSE;
    D_800409C4 = 0;
    D_800409C8 = 0.0f;
    D_800409CC = 0.0f;
    D_800409D0 = -1;
    D_800409D4 = 0.0f;

    if (j_text_trigger ? 1 : 0 && 1)
    {
    }

    g_CurrentPlayer->neg_vspacing_for_control_type_entry = 0;
    g_CurrentPlayer->cur_player_control_type_1 = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->cur_player_control_type_0 = CONTROLLER_CONFIG_HONEY;
    g_CurrentPlayer->cur_player_control_type_2 = 0.0f;
    g_CurrentPlayer->has_set_control_type_data = TRUE;
    D_800409D8 = 8;

    controlstick_lr_enabled = 0;
    watch_stick_y_nav_ready = 0;
    watch_stick_y_prev_active = 0;
    D_80040AE8 = 0.0f;
    D_80040AEC = 0.0f;
    D_80040AF0 = 45.0f;
    D_80040AF4 = 0xff00a0;
    D_80040AF8 = 10;
    D_80040AFC = 0xff;
    D_80040B00 = 10;
    g_WatchBackgroundGreen = 0xe0;
    D_80040B08 = 0;
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
    D_80040B40 = 0;
    D_80040B44 = 1;
    D_80040B48 = 0x32;
    D_80040B4C = 0x32;
    D_80040B50 = 0x32;
    D_80040B54 = 0x32;
    fileLoadSaveSettingsForSelectedFolder(stage);
    mission_failed_or_aborted = FALSE;
}


void controller_deadzone_related(void)
{
    if (10 < joyGetStickX(PLAYER_1))
    {
        D_80040B50 = D_80040B50 + 1;
    }
    if (joyGetStickX(PLAYER_1) < -10)
    {
        D_80040B50 = D_80040B50 + -1;
    }
    if (10 < joyGetStickY(PLAYER_1))
    {
        D_80040B54 = D_80040B54 + -1;
    }
    if (joyGetStickY(PLAYER_1) < -10)
    {
        D_80040B54 = D_80040B54 + 1;
    }
}


Gfx * sub_GAME_7F0A4B40(Gfx *DL)
{
    if (10 < joyGetStickX(PLAYER_1))
    {
        D_80040B48 += 1;
    }

    if (joyGetStickX(PLAYER_1) < -10)
    {
        D_80040B48 -= 1;
    }

    if (10 < joyGetStickY(PLAYER_1))
    {
        D_80040B4C -= 1;
    }

    if (joyGetStickY(PLAYER_1) < -10)
    {
        D_80040B4C += 1;
    }

    gDPSetRenderMode(DL++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(DL++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(DL++, 0, 0, 0xFF, 0x00, 0x00, 0xFF);
    gDPFillRectangle(DL++, D_80040B48, D_80040B4C, D_80040B48+1, D_80040B4C+1);

    {
        u8 buffer [0x12];
        struct font * pFontFile;
        struct fontchar * pFontChars;
        s32 y;
        s32 x;

        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;
        sprintf(buffer,"%d, %d\n",D_80040B48,D_80040B4C);

        DL = microcode_constructor(DL++);

        textMeasure(&x, &y, buffer, pFontChars, pFontFile, 0);
        DL = textRender(DL, &D_80040B48, &D_80040B4C, buffer, pFontChars, pFontFile, 0xff0000ff, y, x, 0, 0);
        // HACK: what is this: ((s32*)pFontChars)[0x224]
        D_80040B4C = (D_80040B4C - ((s32*)pFontChars)[0x224]) + 1;
    }

    return DL;
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


int cur_player_get_control_type(void){
  return g_CurrentPlayer->cur_player_control_type_0;
}


void cur_player_set_control_type(int type)
{
    int langsize;

    g_CurrentPlayer->cur_player_control_type_0 = type;
    g_CurrentPlayer->cur_player_control_type_1 = type;
    g_CurrentPlayer->cur_player_control_type_2 = (float)type;

    langsize = j_text_trigger ? 14 : 10;

    g_CurrentPlayer->neg_vspacing_for_control_type_entry = -(langsize * type);
    g_CurrentPlayer->has_set_control_type_data = TRUE;

}

u32 get_cur_player_look_vertical_inverted(void)
{
    return game_options_entries[PLAYER_OPTION_LOOK].current_value;
}

void set_cur_player_look_vertical_inverted(u32 param_1)
{
    game_options_entries[PLAYER_OPTION_LOOK].current_value = param_1;
}

s32 cur_player_get_autoaim(void)
{
    return game_options_entries[PLAYER_OPTION_AUTOAIM].current_value;
}

void cur_player_set_autoaim(u32 param_1)
{
    game_options_entries[PLAYER_OPTION_AUTOAIM].current_value = param_1;
}

u32 cur_player_get_lookahead(void)
{
    return game_options_entries[PLAYER_OPTION_LOOKAHEAD].current_value;
}

void cur_player_set_lookahead(u32 param_1)
{
    game_options_entries[PLAYER_OPTION_LOOKAHEAD].current_value = param_1;
}

u32 cur_player_get_aim_control(void)
{
    return game_options_entries[PLAYER_OPTION_AIM].current_value;
}

void cur_player_set_aim_control(u32 param_1)
{
    game_options_entries[PLAYER_OPTION_AIM].current_value = param_1;
}

u32 cur_player_get_sight_onscreen_control(void)
{
    return game_options_entries[PLAYER_OPTION_SIGHT].current_value;
}
void cur_player_set_sight_onscreen_control(u32 param_1)
{
    game_options_entries[PLAYER_OPTION_SIGHT].current_value = param_1;
}

u32 cur_player_get_ammo_onscreen_setting(void)
{
    return game_options_entries[PLAYER_OPTION_AMMODISPLAY].current_value;
}
void cur_player_set_ammo_onscreen_setting(u32 param_1)
{
    game_options_entries[PLAYER_OPTION_AMMODISPLAY].current_value = param_1;
}

u32 cur_player_get_screen_setting(void)
{
    return game_options_entries[PLAYER_OPTION_SCREEN].current_value;
}
void cur_player_set_screen_setting(u32 param_1)
{
    game_options_entries[PLAYER_OPTION_SCREEN].current_value = param_1;
}

SCREEN_RATIO_OPTION get_screen_ratio(void)
{
    return game_options_entries[PLAYER_OPTION_RATIO].current_value;
}

void set_screen_ratio(SCREEN_RATIO_OPTION ratio_option)
{
    game_options_entries[PLAYER_OPTION_RATIO].current_value = ratio_option;
}


void watch_play_beep_sound(void) {

    if (watch_item_is_actively_selected == 1) {
        watch_item_is_actively_selected = 0;

    } else {
        watch_item_is_actively_selected = 1;
        sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
    }
}


void reset_watch_item_is_actively_selected(void){
  watch_item_is_actively_selected = 0;
}


u32 is_holding_greater_than_2E_left_on_stick(void)
{
    return (joyGetStickX(PLAYER_1) < -0x2d);
}


u32 is_holding_greater_than_2E_right_on_stick(void)
{
    return ((joyGetStickX(PLAYER_1) < 0x2e) ^ 1);
}


u32 get_controlstick_lr_enabled(void) {
  return controlstick_lr_enabled;
}


void set_controlstick_lr_disabled(void) {
  controlstick_lr_enabled = 0;
}


s32 sub_GAME_7F0A4FB0(void)
{
    return is_holding_greater_than_2E_left_on_stick() && get_controlstick_lr_enabled();
}


s32 sub_GAME_7F0A4FEC(void)
{
    return is_holding_greater_than_2E_right_on_stick() && get_controlstick_lr_enabled();
}


u32 is_holding_greater_than_2E_up_on_stick(void)
{
    return (joyGetStickY(PLAYER_1) < 0x2e) ^ 1;
}


u32 is_holding_greater_than_2E_down_on_stick(void)
{
    return (joyGetStickY(PLAYER_1) < -0x2d);
}


u32 get_watch_stick_y_nav_ready(void)
{
    return watch_stick_y_nav_ready;
}


void disable_watch_stick_y_nav_ready(void)
{
    watch_stick_y_nav_ready = 0;
}


s32 sub_GAME_7F0A5088(void)
{
    return is_holding_greater_than_2E_up_on_stick() && get_watch_stick_y_nav_ready();
}


s32 sub_GAME_7F0A50C4(void)
{
    return is_holding_greater_than_2E_down_on_stick() && get_watch_stick_y_nav_ready();
}


u32 is_holding_less_than_10_up_on_stick(void)
{
    return (joyGetStickY(PLAYER_1) < 0x10) ^ 1;
}


u32 is_holding_less_than_10_down_on_stick(void)
{
    return (joyGetStickY(PLAYER_1) < -0xf);
}


u32 watch_stick_y_was_active(void)
{
    return watch_stick_y_prev_active;
}


void reset_watch_stick_y_latch(void) 
{
    watch_stick_y_prev_active = 0;
}


s32 watch_stick_y_pressed_up(void)
{
    return is_holding_less_than_10_up_on_stick() && !watch_stick_y_was_active();
}


s32 watch_stick_y_pressed_down(void)
{
    return is_holding_less_than_10_down_on_stick() && !watch_stick_y_was_active();
}


void sub_GAME_7F0A51D8(void)
{
    g_WatchBackgroundGreen = 0x80;
    sndPlaySfx(g_musicSfxBufferPtr, WATCH_STATIC_SFX, NULL);
    return;
}


void sub_GAME_7F0A5210(void)
{
    set_controlstick_lr_disabled();
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, NULL);
    if ((D_80040B10 << 0x10) < randomGetNext()) {
        sub_GAME_7F0A51D8();
    }
    return;
}


// initial pause screen: WATCH_INDEX_MISSION_STATUS
void watch_screen0_navigation(void)
{
    s32 goto_watch_screen_index_4;
    s32 goto_watch_screen_index_1;

    if (watch_item_is_actively_selected == 0)
    {
        goto_watch_screen_index_4 = FALSE;
        goto_watch_screen_index_1 = FALSE;

        if (get_debug_gunwatchpos_flag() == 0)
        {
            if (joyGetButtonsPressedThisFrame(PLAYER_1, L_TRIG|L_CBUTTONS))
            {
                goto_watch_screen_index_4 = TRUE;
            }
            if (joyGetButtonsPressedThisFrame(PLAYER_1, R_TRIG|R_CBUTTONS))
            {
                goto_watch_screen_index_1 = TRUE;
            }
        }


        if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_JPAD)) || (sub_GAME_7F0A4FB0()))
        {
            goto_watch_screen_index_4 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_JPAD)) || (sub_GAME_7F0A4FEC()))
        {
            goto_watch_screen_index_1 = TRUE;
        }

        if (goto_watch_screen_index_4)
        {
            watch_screen_index = WATCH_INDEX_MISSION_BRIEFING;
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM1, 15.0f);
        }

        if (goto_watch_screen_index_1)
        {
            watch_screen_index = WATCH_INDEX_INVENTORY;
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM1, 15.0f);
            return;
        }
    }
    else if ((D_800409A4) && (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON)))
    {
        D_800409A4 = 0;
        set_missionstate(MISSION_STATE_0);
        bossRunTitleStage();
        mission_failed_or_aborted = TRUE;
        deleteCurrentSelectedFolder();
    }
}


// pause screen: WATCH_INDEX_INVENTORY
void watch_screen1_navigation(void)
{
    s32 goto_watch_screen_index_0;
    s32 goto_watch_screen_index_2;

    if (watch_item_is_actively_selected == 0)
    {
        goto_watch_screen_index_0 = FALSE;
        goto_watch_screen_index_2 = FALSE;

        if (get_debug_gunwatchpos_flag() == FALSE)
        {
            if (joyGetButtonsPressedThisFrame(PLAYER_1, L_TRIG|L_CBUTTONS))
            {
                goto_watch_screen_index_0 = TRUE;
            }
            if (joyGetButtonsPressedThisFrame(PLAYER_1, R_TRIG|R_CBUTTONS))
            {
                goto_watch_screen_index_2 = TRUE;
            }
        }

        if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_JPAD)) || (sub_GAME_7F0A4FB0()))
        {
            goto_watch_screen_index_0 = TRUE;
        }

        if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_JPAD)) || (sub_GAME_7F0A4FEC()))
        {
            goto_watch_screen_index_2 = TRUE;
        }

        if (goto_watch_screen_index_0)
        {
            watch_screen_index = WATCH_INDEX_MISSION_STATUS;
            zero_D_800409A4();
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM2, 15.0f);

        }

        if (goto_watch_screen_index_2)
        {
            watch_screen_index = WATCH_INDEX_CONTROL_OPTIONS;
            set_controlstick_lr_disabled();
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM3, 15.0f);
        }
    }
}


void unused_watch_screen_navigation(void) {

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) || (sub_GAME_7F0A4FB0()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            watch_screen_index = WATCH_INDEX_INVENTORY;
            set_controlstick_lr_disabled();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) || (sub_GAME_7F0A4FEC()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            watch_screen_index = WATCH_INDEX_CONTROL_OPTIONS;
            reset_controller_options_index();
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM3, 15.0f);
        }
    }
}


// WATCH_INDEX_CONTROL_OPTIONS
void watch_screen2_navigation(void) {

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) || (sub_GAME_7F0A4FB0()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_INVENTORY;
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM1, 15.0f);
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) || (sub_GAME_7F0A4FEC()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_GAME_OPTIONS;
            reset_game_options_index();
            set_controlstick_lr_disabled();
        }
    }
}


// WATCH_INDEX_GAME_OPTIONS
void watch_screen3_navigation(void) {

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) || (sub_GAME_7F0A4FB0()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_CONTROL_OPTIONS;
            reset_controller_options_index();
            set_controlstick_lr_disabled();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) || (sub_GAME_7F0A4FEC()))
    {
        if ((joyGetButtons(PLAYER_1, Z_TRIG) == 0) && (watch_item_is_actively_selected == 0))
        {
            watch_screen_index = WATCH_INDEX_MISSION_BRIEFING;
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM1, 15.0f);
        }
    }
}


// WATCH_INDEX_MISSION_BRIEFING
void watch_screen4_navigation(void) {

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) || (sub_GAME_7F0A4FB0()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            watch_screen_index = WATCH_INDEX_GAME_OPTIONS;
            reset_game_options_index();
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM3, 15.0f);
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) || (sub_GAME_7F0A4FEC()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            watch_screen_index = WATCH_INDEX_MISSION_STATUS;
            zero_D_800409A4();
            sub_GAME_7F0A5210();
            trigger_watch_zoom(WATCHZOOM2, 15.0f);
        }
    }
}


void controller_options_controlstyle_navigation(void)
{
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (sub_GAME_7F0A5088()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_INPUTS;
            disable_watch_stick_y_nav_ready();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (sub_GAME_7F0A50C4()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_INPUTS;
            disable_watch_stick_y_nav_ready();
        }
    }
}


void controller_options_inputs_navigation(void)
{
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (sub_GAME_7F0A5088()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
            disable_watch_stick_y_nav_ready();
            return;
        }
    }
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (sub_GAME_7F0A50C4()))
    {
        if (watch_item_is_actively_selected == 0)
        {
            controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
            disable_watch_stick_y_nav_ready();
        }
    }
}


void sub_GAME_7F0A5998(void)
{
    s32 aux;

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (sub_GAME_7F0A5088()))
    {
        game_options_index = game_options_index - 1;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
    else if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (sub_GAME_7F0A50C4()))
    {
        game_options_index = game_options_index + 1;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }

    aux = game_options_index;

    if (aux >= 10)
    {
        game_options_index = GAME_OPTIONS_INDEX_MUSIC;
        return;
    }

    if (aux < 0)
    {
        game_options_index = GAME_OPTIONS_INDEX_RATIO;
    }
}


void game_options_music_volume_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || sub_GAME_7F0A5088())
    {
        game_options_index = GAME_OPTIONS_INDEX_RATIO;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || sub_GAME_7F0A50C4())
    {
        game_options_index = GAME_OPTIONS_INDEX_FX;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
}


void game_options_fx_volume_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || sub_GAME_7F0A5088())
    {
        game_options_index = GAME_OPTIONS_INDEX_MUSIC;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || sub_GAME_7F0A50C4())
    {
        game_options_index = GAME_OPTIONS_INDEX_LOOK_UPDOWN;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
}


void game_options_inventory_navigation(void)
{
    s32 count;
    s32 item_line_height;
    s32 selected_item_line_height;

    count = bondinvCountTotalItemsInInv();

    if (!get_debug_gunwatchpos_flag())
    {
        if (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD | U_CBUTTONS) || joyGetStickY(PLAYER_1) >= 0x47)
        {
            if (((s32) watch_inventory_cursor_pos > 0) && !watch_item_is_actively_selected)
            {
                watch_inventory_cursor_pos -= 1.0f;
            }
        }
        else
        {
            if (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD | D_CBUTTONS) || joyGetStickY(PLAYER_1) < -0x46)
            {
                goto down_body;
            }

            goto after_updown;

down_body:
            if (((s32) watch_inventory_cursor_pos < count - 1) && !watch_item_is_actively_selected)
            {
                watch_inventory_cursor_pos += 1.0f;
            }

after_updown:
            ;
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

    if (watch_stick_y_pressed_up() && g_curWatchItemIndex > 0 && !watch_item_is_actively_selected)
    {
        watch_inventory_cursor_pos -= 1.0f;
    }
    else if (watch_stick_y_pressed_down() && g_curWatchItemIndex < count - 1 && !watch_item_is_actively_selected)
    {
        watch_inventory_cursor_pos += 1.0f;
    }

    if (is_holding_less_than_10_up_on_stick() || is_holding_less_than_10_down_on_stick())
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

    if (j_text_trigger)
    {
        item_line_height = 14;
    }
    else
    {
        item_line_height = 12;
    }

    selected_item_line_height = j_text_trigger ? 14 : 12;
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

    if (((f32) g_curWatchItemIndex + 0.55f < watch_inventory_cursor_pos)
        && !joyGetButtons(PLAYER_1, 0xffff)) // Any button
    {
        watch_inventory_cursor_pos -= 0.1f;
    }
    else if (watch_inventory_cursor_pos <= (f32) g_curWatchItemIndex + 0.45f
        && !joyGetButtons(PLAYER_1, 0xffff)) // Any button
    {
        watch_inventory_cursor_pos += 0.1f;
    }
}


#ifdef NONMATCHING
void sub_GAME_7F0A611C(void) {

}
#else
GLOBAL_ASM(
.late_rodata
glabel D_80058494
.word 0x3dcccccd /*0.1*/
glabel D_80058498
.word 0x3dcccccd /*0.1*/
glabel D_8005849C
.word 0x3f0ccccd /*0.55000001*/
glabel D_800584A0
.word 0x3dcccccd /*0.1*/
glabel D_800584A4
.word 0x3ee66666 /*0.44999999*/
glabel D_800584A8
.word 0x3dcccccd /*0.1*/
.text
glabel sub_GAME_7F0A611C
/* 0DAC4C 7F0A611C 27BDFFE0 */  addiu $sp, $sp, -0x20
/* 0DAC50 7F0A6120 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0DAC54 7F0A6124 AFB00018 */  sw    $s0, 0x18($sp)
/* 0DAC58 7F0A6128 00808025 */  move  $s0, $a0
/* 0DAC5C 7F0A612C AFA50024 */  sw    $a1, 0x24($sp)
/* 0DAC60 7F0A6130 AFA60028 */  sw    $a2, 0x28($sp)
/* 0DAC64 7F0A6134 0FC24415 */  jal   get_debug_gunwatchpos_flag
/* 0DAC68 7F0A6138 AFA7002C */   sw    $a3, 0x2c($sp)
/* 0DAC6C 7F0A613C 14400056 */  bnez  $v0, .L7F0A6298
/* 0DAC70 7F0A6140 00002025 */   move  $a0, $zero
/* 0DAC74 7F0A6144 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0DAC78 7F0A6148 24050808 */   li    $a1, 2056
/* 0DAC7C 7F0A614C 54400007 */  bnezl $v0, .L7F0A616C
/* 0DAC80 7F0A6150 C6000000 */   lwc1  $f0, ($s0)
/* 0DAC84 7F0A6154 0C00307F */  jal   joyGetStickY
/* 0DAC88 7F0A6158 00002025 */   move  $a0, $zero
/* 0DAC8C 7F0A615C 28410047 */  slti  $at, $v0, 0x47
/* 0DAC90 7F0A6160 1420000F */  bnez  $at, .L7F0A61A0
/* 0DAC94 7F0A6164 00002025 */   move  $a0, $zero
/* 0DAC98 7F0A6168 C6000000 */  lwc1  $f0, ($s0)
.L7F0A616C:
/* 0DAC9C 7F0A616C 8FB8003C */  lw    $t8, 0x3c($sp)
/* 0DACA0 7F0A6170 4600010D */  trunc.w.s $f4, $f0
/* 0DACA4 7F0A6174 440F2000 */  mfc1  $t7, $f4
/* 0DACA8 7F0A6178 00000000 */  nop
/* 0DACAC 7F0A617C 59E00022 */  blezl $t7, .L7F0A6208
/* 0DACB0 7F0A6180 00002025 */   move  $a0, $zero
/* 0DACB4 7F0A6184 1300001F */  beqz  $t8, .L7F0A6204
/* 0DACB8 7F0A6188 3C013F80 */   li    $at, 0x3F800000 # 1.000000
/* 0DACBC 7F0A618C 44813000 */  mtc1  $at, $f6
/* 0DACC0 7F0A6190 00000000 */  nop
/* 0DACC4 7F0A6194 46060201 */  sub.s $f8, $f0, $f6
/* 0DACC8 7F0A6198 1000001A */  b     .L7F0A6204
/* 0DACCC 7F0A619C E6080000 */   swc1  $f8, ($s0)
.L7F0A61A0:
/* 0DACD0 7F0A61A0 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0DACD4 7F0A61A4 24050404 */   li    $a1, 1028
/* 0DACD8 7F0A61A8 54400007 */  bnezl $v0, .L7F0A61C8
/* 0DACDC 7F0A61AC C6000000 */   lwc1  $f0, ($s0)
/* 0DACE0 7F0A61B0 0C00307F */  jal   joyGetStickY
/* 0DACE4 7F0A61B4 00002025 */   move  $a0, $zero
/* 0DACE8 7F0A61B8 2841FFBA */  slti  $at, $v0, -0x46
/* 0DACEC 7F0A61BC 50200012 */  beql  $at, $zero, .L7F0A6208
/* 0DACF0 7F0A61C0 00002025 */   move  $a0, $zero
/* 0DACF4 7F0A61C4 C6000000 */  lwc1  $f0, ($s0)
.L7F0A61C8:
/* 0DACF8 7F0A61C8 8FA90028 */  lw    $t1, 0x28($sp)
/* 0DACFC 7F0A61CC 8FAB003C */  lw    $t3, 0x3c($sp)
/* 0DAD00 7F0A61D0 4600028D */  trunc.w.s $f10, $f0
/* 0DAD04 7F0A61D4 252AFFFF */  addiu $t2, $t1, -1
/* 0DAD08 7F0A61D8 44085000 */  mfc1  $t0, $f10
/* 0DAD0C 7F0A61DC 00000000 */  nop
/* 0DAD10 7F0A61E0 010A082A */  slt   $at, $t0, $t2
/* 0DAD14 7F0A61E4 50200008 */  beql  $at, $zero, .L7F0A6208
/* 0DAD18 7F0A61E8 00002025 */   move  $a0, $zero
/* 0DAD1C 7F0A61EC 11600005 */  beqz  $t3, .L7F0A6204
/* 0DAD20 7F0A61F0 3C013F80 */   li    $at, 0x3F800000 # 1.000000
/* 0DAD24 7F0A61F4 44818000 */  mtc1  $at, $f16
/* 0DAD28 7F0A61F8 00000000 */  nop
/* 0DAD2C 7F0A61FC 46100480 */  add.s $f18, $f0, $f16
/* 0DAD30 7F0A6200 E6120000 */  swc1  $f18, ($s0)
.L7F0A6204:
/* 0DAD34 7F0A6204 00002025 */  move  $a0, $zero
.L7F0A6208:
/* 0DAD38 7F0A6208 0C0030C3 */  jal   joyGetButtons
/* 0DAD3C 7F0A620C 24050808 */   li    $a1, 2056
/* 0DAD40 7F0A6210 1040000E */  beqz  $v0, .L7F0A624C
/* 0DAD44 7F0A6214 00002025 */   move  $a0, $zero
/* 0DAD48 7F0A6218 C6000000 */  lwc1  $f0, ($s0)
/* 0DAD4C 7F0A621C 8FAE003C */  lw    $t6, 0x3c($sp)
/* 0DAD50 7F0A6220 4600010D */  trunc.w.s $f4, $f0
/* 0DAD54 7F0A6224 440D2000 */  mfc1  $t5, $f4
/* 0DAD58 7F0A6228 00000000 */  nop
/* 0DAD5C 7F0A622C 19A0001A */  blez  $t5, .L7F0A6298
/* 0DAD60 7F0A6230 00000000 */   nop
/* 0DAD64 7F0A6234 11C00018 */  beqz  $t6, .L7F0A6298
/* 0DAD68 7F0A6238 3C018006 */   lui   $at, %hi(D_80058494)
/* 0DAD6C 7F0A623C C4268494 */  lwc1  $f6, %lo(D_80058494)($at)
/* 0DAD70 7F0A6240 46060201 */  sub.s $f8, $f0, $f6
/* 0DAD74 7F0A6244 10000014 */  b     .L7F0A6298
/* 0DAD78 7F0A6248 E6080000 */   swc1  $f8, ($s0)
.L7F0A624C:
/* 0DAD7C 7F0A624C 0C0030C3 */  jal   joyGetButtons
/* 0DAD80 7F0A6250 24050404 */   li    $a1, 1028
/* 0DAD84 7F0A6254 10400010 */  beqz  $v0, .L7F0A6298
/* 0DAD88 7F0A6258 00000000 */   nop
/* 0DAD8C 7F0A625C C6000000 */  lwc1  $f0, ($s0)
/* 0DAD90 7F0A6260 8FB90028 */  lw    $t9, 0x28($sp)
/* 0DAD94 7F0A6264 8FA8003C */  lw    $t0, 0x3c($sp)
/* 0DAD98 7F0A6268 4600028D */  trunc.w.s $f10, $f0
/* 0DAD9C 7F0A626C 2729FFFF */  addiu $t1, $t9, -1
/* 0DADA0 7F0A6270 44185000 */  mfc1  $t8, $f10
/* 0DADA4 7F0A6274 00000000 */  nop
/* 0DADA8 7F0A6278 0309082A */  slt   $at, $t8, $t1
/* 0DADAC 7F0A627C 10200006 */  beqz  $at, .L7F0A6298
/* 0DADB0 7F0A6280 00000000 */   nop
/* 0DADB4 7F0A6284 11000004 */  beqz  $t0, .L7F0A6298
/* 0DADB8 7F0A6288 3C018006 */   lui   $at, %hi(D_80058498)
/* 0DADBC 7F0A628C C4308498 */  lwc1  $f16, %lo(D_80058498)($at)
/* 0DADC0 7F0A6290 46100480 */  add.s $f18, $f0, $f16
/* 0DADC4 7F0A6294 E6120000 */  swc1  $f18, ($s0)
.L7F0A6298:
/* 0DADC8 7F0A6298 0C00307F */  jal   joyGetStickY
/* 0DADCC 7F0A629C 00002025 */   move  $a0, $zero
/* 0DADD0 7F0A62A0 2841001F */  slti  $at, $v0, 0x1f
/* 0DADD4 7F0A62A4 14200017 */  bnez  $at, .L7F0A6304
/* 0DADD8 7F0A62A8 00000000 */   nop
/* 0DADDC 7F0A62AC 0C00307F */  jal   joyGetStickY
/* 0DADE0 7F0A62B0 00002025 */   move  $a0, $zero
/* 0DADE4 7F0A62B4 28410046 */  slti  $at, $v0, 0x46
/* 0DADE8 7F0A62B8 10200012 */  beqz  $at, .L7F0A6304
/* 0DADEC 7F0A62BC 8FAA0024 */   lw    $t2, 0x24($sp)
/* 0DADF0 7F0A62C0 8D4B0000 */  lw    $t3, ($t2)
/* 0DADF4 7F0A62C4 8FAC003C */  lw    $t4, 0x3c($sp)
/* 0DADF8 7F0A62C8 1960000E */  blez  $t3, .L7F0A6304
/* 0DADFC 7F0A62CC 00000000 */   nop
/* 0DAE00 7F0A62D0 1180000C */  beqz  $t4, .L7F0A6304
/* 0DAE04 7F0A62D4 00000000 */   nop
/* 0DAE08 7F0A62D8 0C00307F */  jal   joyGetStickY
/* 0DAE0C 7F0A62DC 00002025 */   move  $a0, $zero
/* 0DAE10 7F0A62E0 44822000 */  mtc1  $v0, $f4
/* 0DAE14 7F0A62E4 3C014396 */  li    $at, 0x43960000 # 300.000000
/* 0DAE18 7F0A62E8 44814000 */  mtc1  $at, $f8
/* 0DAE1C 7F0A62EC 468021A0 */  cvt.s.w $f6, $f4
/* 0DAE20 7F0A62F0 C6100000 */  lwc1  $f16, ($s0)
/* 0DAE24 7F0A62F4 46083283 */  div.s $f10, $f6, $f8
/* 0DAE28 7F0A62F8 460A8481 */  sub.s $f18, $f16, $f10
/* 0DAE2C 7F0A62FC 10000021 */  b     .L7F0A6384
/* 0DAE30 7F0A6300 E6120000 */   swc1  $f18, ($s0)
.L7F0A6304:
/* 0DAE34 7F0A6304 0C00307F */  jal   joyGetStickY
/* 0DAE38 7F0A6308 00002025 */   move  $a0, $zero
/* 0DAE3C 7F0A630C 2841FFE2 */  slti  $at, $v0, -0x1e
/* 0DAE40 7F0A6310 1020001C */  beqz  $at, .L7F0A6384
/* 0DAE44 7F0A6314 00000000 */   nop
/* 0DAE48 7F0A6318 0C00307F */  jal   joyGetStickY
/* 0DAE4C 7F0A631C 00002025 */   move  $a0, $zero
/* 0DAE50 7F0A6320 2841FFBB */  slti  $at, $v0, -0x45
/* 0DAE54 7F0A6324 14200017 */  bnez  $at, .L7F0A6384
/* 0DAE58 7F0A6328 00000000 */   nop
/* 0DAE5C 7F0A632C C6040000 */  lwc1  $f4, ($s0)
/* 0DAE60 7F0A6330 8FAF0028 */  lw    $t7, 0x28($sp)
/* 0DAE64 7F0A6334 8FB8003C */  lw    $t8, 0x3c($sp)
/* 0DAE68 7F0A6338 4600218D */  trunc.w.s $f6, $f4
/* 0DAE6C 7F0A633C 25F9FFFF */  addiu $t9, $t7, -1
/* 0DAE70 7F0A6340 440E3000 */  mfc1  $t6, $f6
/* 0DAE74 7F0A6344 00000000 */  nop
/* 0DAE78 7F0A6348 01D9082A */  slt   $at, $t6, $t9
/* 0DAE7C 7F0A634C 1020000D */  beqz  $at, .L7F0A6384
/* 0DAE80 7F0A6350 00000000 */   nop
/* 0DAE84 7F0A6354 1300000B */  beqz  $t8, .L7F0A6384
/* 0DAE88 7F0A6358 00000000 */   nop
/* 0DAE8C 7F0A635C 0C00307F */  jal   joyGetStickY
/* 0DAE90 7F0A6360 00002025 */   move  $a0, $zero
/* 0DAE94 7F0A6364 44824000 */  mtc1  $v0, $f8
/* 0DAE98 7F0A6368 3C014396 */  li    $at, 0x43960000 # 300.000000
/* 0DAE9C 7F0A636C 44815000 */  mtc1  $at, $f10
/* 0DAEA0 7F0A6370 46804420 */  cvt.s.w $f16, $f8
/* 0DAEA4 7F0A6374 C6040000 */  lwc1  $f4, ($s0)
/* 0DAEA8 7F0A6378 460A8483 */  div.s $f18, $f16, $f10
/* 0DAEAC 7F0A637C 46122181 */  sub.s $f6, $f4, $f18
/* 0DAEB0 7F0A6380 E6060000 */  swc1  $f6, ($s0)
.L7F0A6384:
/* 0DAEB4 7F0A6384 0FC29458 */  jal   watch_stick_y_pressed_up
/* 0DAEB8 7F0A6388 00000000 */   nop
/* 0DAEBC 7F0A638C 1040000D */  beqz  $v0, .L7F0A63C4
/* 0DAEC0 7F0A6390 8FA90024 */   lw    $t1, 0x24($sp)
/* 0DAEC4 7F0A6394 8D280000 */  lw    $t0, ($t1)
/* 0DAEC8 7F0A6398 8FAA003C */  lw    $t2, 0x3c($sp)
/* 0DAECC 7F0A639C 19000009 */  blez  $t0, .L7F0A63C4
/* 0DAED0 7F0A63A0 00000000 */   nop
/* 0DAED4 7F0A63A4 11400007 */  beqz  $t2, .L7F0A63C4
/* 0DAED8 7F0A63A8 3C013F80 */   li    $at, 0x3F800000 # 1.000000
/* 0DAEDC 7F0A63AC C6080000 */  lwc1  $f8, ($s0)
/* 0DAEE0 7F0A63B0 44818000 */  mtc1  $at, $f16
/* 0DAEE4 7F0A63B4 00000000 */  nop
/* 0DAEE8 7F0A63B8 46104281 */  sub.s $f10, $f8, $f16
/* 0DAEEC 7F0A63BC 10000013 */  b     .L7F0A640C
/* 0DAEF0 7F0A63C0 E60A0000 */   swc1  $f10, ($s0)
.L7F0A63C4:
/* 0DAEF4 7F0A63C4 0FC29467 */  jal   watch_stick_y_pressed_down
/* 0DAEF8 7F0A63C8 00000000 */   nop
/* 0DAEFC 7F0A63CC 1040000F */  beqz  $v0, .L7F0A640C
/* 0DAF00 7F0A63D0 8FAB0024 */   lw    $t3, 0x24($sp)
/* 0DAF04 7F0A63D4 8FAD0028 */  lw    $t5, 0x28($sp)
/* 0DAF08 7F0A63D8 8D6C0000 */  lw    $t4, ($t3)
/* 0DAF0C 7F0A63DC 8FAE003C */  lw    $t6, 0x3c($sp)
/* 0DAF10 7F0A63E0 25AFFFFF */  addiu $t7, $t5, -1
/* 0DAF14 7F0A63E4 018F082A */  slt   $at, $t4, $t7
/* 0DAF18 7F0A63E8 10200008 */  beqz  $at, .L7F0A640C
/* 0DAF1C 7F0A63EC 00000000 */   nop
/* 0DAF20 7F0A63F0 11C00006 */  beqz  $t6, .L7F0A640C
/* 0DAF24 7F0A63F4 3C013F80 */   li    $at, 0x3F800000 # 1.000000
/* 0DAF28 7F0A63F8 C6040000 */  lwc1  $f4, ($s0)
/* 0DAF2C 7F0A63FC 44819000 */  mtc1  $at, $f18
/* 0DAF30 7F0A6400 00000000 */  nop
/* 0DAF34 7F0A6404 46122180 */  add.s $f6, $f4, $f18
/* 0DAF38 7F0A6408 E6060000 */  swc1  $f6, ($s0)
.L7F0A640C:
/* 0DAF3C 7F0A640C 0FC29440 */  jal   is_holding_less_than_10_up_on_stick
/* 0DAF40 7F0A6410 00000000 */   nop
/* 0DAF44 7F0A6414 54400006 */  bnezl $v0, .L7F0A6430
/* 0DAF48 7F0A6418 24190001 */   li    $t9, 1
/* 0DAF4C 7F0A641C 0FC29449 */  jal   is_holding_less_than_10_down_on_stick
/* 0DAF50 7F0A6420 00000000 */   nop
/* 0DAF54 7F0A6424 10400005 */  beqz  $v0, .L7F0A643C
/* 0DAF58 7F0A6428 3C018004 */   lui   $at, %hi(watch_stick_y_prev_active)
/* 0DAF5C 7F0A642C 24190001 */  li    $t9, 1
.L7F0A6430:
/* 0DAF60 7F0A6430 3C018004 */  lui    $at, %hi(watch_stick_y_prev_active)
/* 0DAF64 7F0A6434 10000002 */  b     .L7F0A6440
/* 0DAF68 7F0A6438 AC390AE4 */   sw    $t9, %lo(watch_stick_y_prev_active)($at)
.L7F0A643C:
/* 0DAF6C 7F0A643C AC200AE4 */  sw    $zero, %lo(watch_stick_y_prev_active)($at)
.L7F0A6440:
/* 0DAF70 7F0A6440 8FB80028 */  lw    $t8, 0x28($sp)
/* 0DAF74 7F0A6444 3C013F00 */  li    $at, 0x3F000000 # 0.500000
/* 0DAF78 7F0A6448 44815000 */  mtc1  $at, $f10
/* 0DAF7C 7F0A644C 44984000 */  mtc1  $t8, $f8
/* 0DAF80 7F0A6450 C6000000 */  lwc1  $f0, ($s0)
/* 0DAF84 7F0A6454 3C01BF00 */  li    $at, 0xBF000000 # -0.500000
/* 0DAF88 7F0A6458 46804420 */  cvt.s.w $f16, $f8
/* 0DAF8C 7F0A645C 460A8081 */  sub.s $f2, $f16, $f10
/* 0DAF90 7F0A6460 4600103C */  c.lt.s $f2, $f0
/* 0DAF94 7F0A6464 00000000 */  nop
/* 0DAF98 7F0A6468 45020004 */  bc1fl .L7F0A647C
/* 0DAF9C 7F0A646C 44811000 */   mtc1  $at, $f2
/* 0DAFA0 7F0A6470 E6020000 */  swc1  $f2, ($s0)
/* 0DAFA4 7F0A6474 C6000000 */  lwc1  $f0, ($s0)
/* 0DAFA8 7F0A6478 44811000 */  mtc1  $at, $f2
.L7F0A647C:
/* 0DAFAC 7F0A647C 00000000 */  nop
/* 0DAFB0 7F0A6480 4602003C */  c.lt.s $f0, $f2
/* 0DAFB4 7F0A6484 00000000 */  nop
/* 0DAFB8 7F0A6488 45020004 */  bc1fl .L7F0A649C
/* 0DAFBC 7F0A648C 4600010D */   trunc.w.s $f4, $f0
/* 0DAFC0 7F0A6490 E6020000 */  swc1  $f2, ($s0)
/* 0DAFC4 7F0A6494 C6000000 */  lwc1  $f0, ($s0)
/* 0DAFC8 7F0A6498 4600010D */  trunc.w.s $f4, $f0
.L7F0A649C:
/* 0DAFCC 7F0A649C 8FA50040 */  lw    $a1, 0x40($sp)
/* 0DAFD0 7F0A64A0 8FAA0024 */  lw    $t2, 0x24($sp)
/* 0DAFD4 7F0A64A4 8FA40030 */  lw    $a0, 0x30($sp)
/* 0DAFD8 7F0A64A8 44082000 */  mfc1  $t0, $f4
/* 0DAFDC 7F0A64AC 8FA6002C */  lw    $a2, 0x2c($sp)
/* 0DAFE0 7F0A64B0 00085823 */  negu  $t3, $t0
/* 0DAFE4 7F0A64B4 01650019 */  multu $t3, $a1
/* 0DAFE8 7F0A64B8 AD480000 */  sw    $t0, ($t2)
/* 0DAFEC 7F0A64BC 8FAC0038 */  lw    $t4, 0x38($sp)
/* 0DAFF0 7F0A64C0 00006812 */  mflo  $t5
/* 0DAFF4 7F0A64C4 00000000 */  nop
/* 0DAFF8 7F0A64C8 00000000 */  nop
/* 0DAFFC 7F0A64CC 01850019 */  multu $t4, $a1
/* 0DB000 7F0A64D0 00007812 */  mflo  $t7
/* 0DB004 7F0A64D4 01AF7021 */  addu  $t6, $t5, $t7
/* 0DB008 7F0A64D8 AC8E0000 */  sw    $t6, ($a0)
/* 0DB00C 7F0A64DC 8CC20000 */  lw    $v0, ($a2)
/* 0DB010 7F0A64E0 01C01825 */  move  $v1, $t6
/* 0DB014 7F0A64E4 00002025 */  move  $a0, $zero
/* 0DB018 7F0A64E8 01C2082A */  slt   $at, $t6, $v0
/* 0DB01C 7F0A64EC 1020000A */  beqz  $at, .L7F0A6518
/* 0DB020 7F0A64F0 004EC823 */   subu  $t9, $v0, $t6
/* 0DB024 7F0A64F4 24010003 */  li    $at, 3
/* 0DB028 7F0A64F8 0321001A */  div   $zero, $t9, $at
/* 0DB02C 7F0A64FC 0000C012 */  mflo  $t8
/* 0DB030 7F0A6500 00584823 */  subu  $t1, $v0, $t8
/* 0DB034 7F0A6504 252AFFFF */  addiu $t2, $t1, -1
/* 0DB038 7F0A6508 ACCA0000 */  sw    $t2, ($a2)
/* 0DB03C 7F0A650C 8FA80034 */  lw    $t0, 0x34($sp)
/* 0DB040 7F0A6510 10000010 */  b     .L7F0A6554
/* 0DB044 7F0A6514 AD000000 */   sw    $zero, ($t0)
.L7F0A6518:
/* 0DB048 7F0A6518 0043082A */  slt   $at, $v0, $v1
/* 0DB04C 7F0A651C 1020000B */  beqz  $at, .L7F0A654C
/* 0DB050 7F0A6520 8FB80034 */   lw    $t8, 0x34($sp)
/* 0DB054 7F0A6524 00625823 */  subu  $t3, $v1, $v0
/* 0DB058 7F0A6528 24010003 */  li    $at, 3
/* 0DB05C 7F0A652C 0161001A */  div   $zero, $t3, $at
/* 0DB060 7F0A6530 00006012 */  mflo  $t4
/* 0DB064 7F0A6534 004C6821 */  addu  $t5, $v0, $t4
/* 0DB068 7F0A6538 25AF0001 */  addiu $t7, $t5, 1
/* 0DB06C 7F0A653C ACCF0000 */  sw    $t7, ($a2)
/* 0DB070 7F0A6540 8FAE0034 */  lw    $t6, 0x34($sp)
/* 0DB074 7F0A6544 10000003 */  b     .L7F0A6554
/* 0DB078 7F0A6548 ADC00000 */   sw    $zero, ($t6)
.L7F0A654C:
/* 0DB07C 7F0A654C 24190001 */  li    $t9, 1
/* 0DB080 7F0A6550 AF190000 */  sw    $t9, ($t8)
.L7F0A6554:
/* 0DB084 7F0A6554 8FA90024 */  lw    $t1, 0x24($sp)
/* 0DB088 7F0A6558 3C018006 */  lui   $at, %hi(D_8005849C)
/* 0DB08C 7F0A655C C428849C */  lwc1  $f8, %lo(D_8005849C)($at)
/* 0DB090 7F0A6560 8D2A0000 */  lw    $t2, ($t1)
/* 0DB094 7F0A6564 C60A0000 */  lwc1  $f10, ($s0)
/* 0DB098 7F0A6568 448A9000 */  mtc1  $t2, $f18
/* 0DB09C 7F0A656C 00000000 */  nop
/* 0DB0A0 7F0A6570 468091A0 */  cvt.s.w $f6, $f18
/* 0DB0A4 7F0A6574 46083400 */  add.s $f16, $f6, $f8
/* 0DB0A8 7F0A6578 460A803C */  c.lt.s $f16, $f10
/* 0DB0AC 7F0A657C 00000000 */  nop
/* 0DB0B0 7F0A6580 4502000B */  bc1fl .L7F0A65B0
/* 0DB0B4 7F0A6584 8FA80024 */   lw    $t0, 0x24($sp)
/* 0DB0B8 7F0A6588 0C0030C3 */  jal   joyGetButtons
/* 0DB0BC 7F0A658C 3405FFFF */   li    $a1, 65535
/* 0DB0C0 7F0A6590 14400006 */  bnez  $v0, .L7F0A65AC
/* 0DB0C4 7F0A6594 3C018006 */   lui   $at, %hi(D_800584A0)
/* 0DB0C8 7F0A6598 C6040000 */  lwc1  $f4, ($s0)
/* 0DB0CC 7F0A659C C43284A0 */  lwc1  $f18, %lo(D_800584A0)($at)
/* 0DB0D0 7F0A65A0 46122181 */  sub.s $f6, $f4, $f18
/* 0DB0D4 7F0A65A4 10000017 */  b     .L7F0A6604
/* 0DB0D8 7F0A65A8 E6060000 */   swc1  $f6, ($s0)
.L7F0A65AC:
/* 0DB0DC 7F0A65AC 8FA80024 */  lw    $t0, 0x24($sp)
.L7F0A65B0:
/* 0DB0E0 7F0A65B0 3C018006 */  lui   $at, %hi(D_800584A4)
/* 0DB0E4 7F0A65B4 C42A84A4 */  lwc1  $f10, %lo(D_800584A4)($at)
/* 0DB0E8 7F0A65B8 8D0B0000 */  lw    $t3, ($t0)
/* 0DB0EC 7F0A65BC C6120000 */  lwc1  $f18, ($s0)
/* 0DB0F0 7F0A65C0 00002025 */  move  $a0, $zero
/* 0DB0F4 7F0A65C4 448B4000 */  mtc1  $t3, $f8
/* 0DB0F8 7F0A65C8 00000000 */  nop
/* 0DB0FC 7F0A65CC 46804420 */  cvt.s.w $f16, $f8
/* 0DB100 7F0A65D0 460A8100 */  add.s $f4, $f16, $f10
/* 0DB104 7F0A65D4 4604903E */  c.le.s $f18, $f4
/* 0DB108 7F0A65D8 00000000 */  nop
/* 0DB10C 7F0A65DC 4502000A */  bc1fl .L7F0A6608
/* 0DB110 7F0A65E0 8FBF001C */   lw    $ra, 0x1c($sp)
/* 0DB114 7F0A65E4 0C0030C3 */  jal   joyGetButtons
/* 0DB118 7F0A65E8 3405FFFF */   li    $a1, 65535
/* 0DB11C 7F0A65EC 14400005 */  bnez  $v0, .L7F0A6604
/* 0DB120 7F0A65F0 3C018006 */   lui   $at, %hi(D_800584A8)
/* 0DB124 7F0A65F4 C6060000 */  lwc1  $f6, ($s0)
/* 0DB128 7F0A65F8 C42884A8 */  lwc1  $f8, %lo(D_800584A8)($at)
/* 0DB12C 7F0A65FC 46083400 */  add.s $f16, $f6, $f8
/* 0DB130 7F0A6600 E6100000 */  swc1  $f16, ($s0)
.L7F0A6604:
/* 0DB134 7F0A6604 8FBF001C */  lw    $ra, 0x1c($sp)
.L7F0A6608:
/* 0DB138 7F0A6608 8FB00018 */  lw    $s0, 0x18($sp)
/* 0DB13C 7F0A660C 27BD0020 */  addiu $sp, $sp, 0x20
/* 0DB140 7F0A6610 03E00008 */  jr    $ra
/* 0DB144 7F0A6614 00000000 */   nop
)
#endif






void mission_brief_background_navigation(void)
{
    if ((joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD)) || (sub_GAME_7F0A5088()))
    {
        mission_brief_index = BRIEF_INDEX_OBJECTIVES;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }

    if ((joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD)) || (sub_GAME_7F0A50C4()))
    {
        mission_brief_index = BRIEF_INDEX_M;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
}


void mission_brief_m_briefing_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || sub_GAME_7F0A5088())
    {
        mission_brief_index = BRIEF_INDEX_BACKGROUND;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || sub_GAME_7F0A50C4())
    {
        mission_brief_index = BRIEF_INDEX_Q;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
}


void mission_brief_q_branch_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || sub_GAME_7F0A5088())
    {
        mission_brief_index = BRIEF_INDEX_M;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || sub_GAME_7F0A50C4())
    {
        mission_brief_index = BRIEF_INDEX_MONEYPENNY;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
}

void mission_brief_moneypenny_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || sub_GAME_7F0A5088())
    {
        mission_brief_index = BRIEF_INDEX_Q;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || sub_GAME_7F0A50C4())
    {
        mission_brief_index = BRIEF_INDEX_OBJECTIVES;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
}

void mission_brief_objectives_navigation(void)
{
    if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS|U_JPAD) || sub_GAME_7F0A5088())
    {
        mission_brief_index = BRIEF_INDEX_MONEYPENNY;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
        return;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS|D_JPAD) || sub_GAME_7F0A50C4())
    {
        mission_brief_index = BRIEF_INDEX_BACKGROUND;
        disable_watch_stick_y_nav_ready();
        reset_watch_item_is_actively_selected();
    }
}


#ifdef NONMATCHING
void sub_GAME_7F0A68D8(void) {

}
#else
GLOBAL_ASM(
.late_rodata
glabel D_800584AC
.word 0x48507100 /*213444.0*/
.text
glabel sub_GAME_7F0A68D8
/* 0DB408 7F0A68D8 3C098004 */  lui   $t1, %hi(D_80040B40)
/* 0DB40C 7F0A68DC 25290B40 */  addiu $t1, %lo(D_80040B40) # addiu $t1, $t1, 0xb40
/* 0DB410 7F0A68E0 8D220000 */  lw    $v0, ($t1)
/* 0DB414 7F0A68E4 3C018006 */  lui   $at, %hi(D_800584AC)
/* 0DB418 7F0A68E8 C42484AC */  lwc1  $f4, %lo(D_800584AC)($at)
/* 0DB41C 7F0A68EC 00420019 */  multu $v0, $v0
/* 0DB420 7F0A68F0 27BDFFE8 */  addiu $sp, $sp, -0x18
/* 0DB424 7F0A68F4 AFBF0014 */  sw    $ra, 0x14($sp)
/* 0DB428 7F0A68F8 AFA40018 */  sw    $a0, 0x18($sp)
/* 0DB42C 7F0A68FC 00007012 */  mflo  $t6
/* 0DB430 7F0A6900 448E3000 */  mtc1  $t6, $f6
/* 0DB434 7F0A6904 00000000 */  nop
/* 0DB438 7F0A6908 46803220 */  cvt.s.w $f8, $f6
/* 0DB43C 7F0A690C 0C007DF8 */  jal   sqrtf
/* 0DB440 7F0A6910 46082301 */   sub.s $f12, $f4, $f8
/* 0DB444 7F0A6914 4600028D */  trunc.w.s $f10, $f0
/* 0DB448 7F0A6918 3C098004 */  lui   $t1, %hi(D_80040B40)
/* 0DB44C 7F0A691C 3C078004 */  lui   $a3, %hi(D_80040B08)
/* 0DB450 7F0A6920 25290B40 */  addiu $t1, %lo(D_80040B40) # addiu $t1, $t1, 0xb40
/* 0DB454 7F0A6924 44055000 */  mfc1  $a1, $f10
/* 0DB458 7F0A6928 8FA40018 */  lw    $a0, 0x18($sp)
/* 0DB45C 7F0A692C 24E70B08 */  addiu $a3, %lo(D_80040B08) # addiu $a3, $a3, 0xb08
/* 0DB460 7F0A6930 00001825 */  move  $v1, $zero
/* 0DB464 7F0A6934 240A0008 */  li    $t2, 8
/* 0DB468 7F0A6938 24080003 */  li    $t0, 3
/* 0DB46C 7F0A693C 240600A0 */  li    $a2, 160
/* 0DB470 7F0A6940 2402FFFF */  li    $v0, -1
.L7F0A6944:
/* 0DB474 7F0A6944 00A20019 */  multu $a1, $v0
/* 0DB478 7F0A6948 A4800002 */  sh    $zero, 2($a0)
/* 0DB47C 7F0A694C 24420002 */  addiu $v0, $v0, 2
/* 0DB480 7F0A6950 24840010 */  addiu $a0, $a0, 0x10
/* 0DB484 7F0A6954 0000C012 */  mflo  $t8
/* 0DB488 7F0A6958 A498FFF0 */  sh    $t8, -0x10($a0)
/* 0DB48C 7F0A695C 8D390000 */  lw    $t9, ($t1)
/* 0DB490 7F0A6960 A480FFF6 */  sh    $zero, -0xa($a0)
/* 0DB494 7F0A6964 A480FFF8 */  sh    $zero, -8($a0)
/* 0DB498 7F0A6968 00795821 */  addu  $t3, $v1, $t9
/* 0DB49C 7F0A696C A48BFFF4 */  sh    $t3, -0xc($a0)
/* 0DB4A0 7F0A6970 A480FFFA */  sh    $zero, -6($a0)
/* 0DB4A4 7F0A6974 A080FFFC */  sb    $zero, -4($a0)
/* 0DB4A8 7F0A6978 A086FFFD */  sb    $a2, -3($a0)
/* 0DB4AC 7F0A697C A080FFFE */  sb    $zero, -2($a0)
/* 0DB4B0 7F0A6980 8CEC0000 */  lw    $t4, ($a3)
/* 0DB4B4 7F0A6984 1448FFEF */  bne   $v0, $t0, .L7F0A6944
/* 0DB4B8 7F0A6988 A08CFFFF */   sb    $t4, -1($a0)
/* 0DB4BC 7F0A698C 24630004 */  addiu $v1, $v1, 4
/* 0DB4C0 7F0A6990 546AFFEC */  bnel  $v1, $t2, .L7F0A6944
/* 0DB4C4 7F0A6994 2402FFFF */   li    $v0, -1
/* 0DB4C8 7F0A6998 8FBF0014 */  lw    $ra, 0x14($sp)
/* 0DB4CC 7F0A699C 27BD0018 */  addiu $sp, $sp, 0x18
/* 0DB4D0 7F0A69A0 03E00008 */  jr    $ra
/* 0DB4D4 7F0A69A4 00000000 */   nop
)
#endif



void sub_GAME_7F0A69A8(void)
{
    if (joyGetControllerCount() < 2)
    {
        D_800409D8 = 4;
    }
    else
    {
        D_800409D8 = 8;
    }
    reset_watch_item_is_actively_selected();
    watch_screen_index = WATCH_INDEX_MISSION_STATUS;
    mission_brief_index = BRIEF_INDEX_OBJECTIVES;
    D_800409C8 = 0.999f;
    D_800409CC = 0.9999f;
    bondinvDetermineEquippedItem();
}


/**
 * Address 0x7F0A6A2C. (VERSION_US, VERSION_JP)
 * Address 0x7F0A5D78. (VERSION_EU)
*/
f32 watchWrapAroundPI(f32 arg0)
{
    if (arg0 > M_PI_F)
    {
        arg0 = arg0 - M_TAU_F;
    }
    else if (arg0 < M_MINUS_PI_F)
    {
        arg0 = arg0 + M_TAU_F;
    }
    return arg0;
}


extern f32 jpD_800484D0;
void sub_GAME_7F0A6A80(void)
{
    u32 temp_1;
    s32 temp_2;
    s32 temp_3;
    u32 random_value;

    if (joyGetButtonsPressedThisFrame(PLAYER_1, START_BUTTON))
    {
        set_open_close_solo_watch_menu_to1();
    }

    if (controlstick_lr_enabled == 0)
    {
        if ((joyGetStickX(PLAYER_1) >= -0xA) && (joyGetStickX(PLAYER_1) < 0xB))
        {
            controlstick_lr_enabled = 1;
        }
        else if ((joyGetStickX(PLAYER_1) < 0xB) && (joy7000C174(PLAYER_1) >= 0xB))
        {
            controlstick_lr_enabled = 1;
        }
        else if ((joyGetStickX(PLAYER_1) >= -0xA) && (joy7000C174(PLAYER_1) < -0xA))
        {
            controlstick_lr_enabled = 1;
        }
    }

    if (watch_stick_y_nav_ready == 0)
    {
        if ((joyGetStickY(PLAYER_1) >= -0xA) && (joyGetStickY(PLAYER_1) < 0xB))
        {
            watch_stick_y_nav_ready = 1;
        }
        else if ((joyGetStickY(PLAYER_1) < 0xB) && (joy7000C284(PLAYER_1) >= 0xB))
        {
            watch_stick_y_nav_ready = 1;
        }
        else if ((joyGetStickY(PLAYER_1) >= -0xA) && (joy7000C284(PLAYER_1) < -0xA))
        {
            watch_stick_y_nav_ready = 1;
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
        D_80040AFC = D_80040AFC - 0x10;
    }
    D_80040B00 = temp_3 - 1;

    if (D_80040AFC < 0x60U)
    {
        D_80040AFC = 0xFFU;
        D_80040B00 = 0xF;
    }
    #ifdef VERSION_US
    D_80040B14 += ((D_80040B1C * speedgraphframes * M_TAU_F) / 360.0f);
    #else
    D_80040B14 += ((D_80040B1C * jpD_800484D0 * M_TAU_F) / 360.0f);
    #endif

    D_80040B14 = watchWrapAroundPI(D_80040B14);

    temp_1 = D_80040B0C << 0x10;
    if (temp_1 < randomGetNext())
    {
        sub_GAME_7F0A51D8();
    }

    if (g_WatchBackgroundGreen < 0xE0)
    {
        random_value = randomGetNext();
        g_WatchBackgroundGreen += (random_value >> 0x1E);
    }

    if (g_WatchBackgroundGreen > 0xe0) {
        g_WatchBackgroundGreen = 0xe0;
    }

    D_80040B08 = ((-g_WatchBackgroundGreen * 4) + 0x380);
    D_80040B40 = D_80040B40 - 4;

    if (D_80040B40 >= 0x157) {
        D_80040B40 = -0x156;
    }

    if (D_80040B40 < -0x156) {
        D_80040B40 = 0x156;
    }

    D_80040B44 = (s16)D_80040B44 + 1;
    D_80040B44 = (s16)D_80040B44 & 1;

    switch (watch_screen_index)
    {
        case WATCH_INDEX_MISSION_STATUS:
            watch_screen0_navigation();
            break;

        case WATCH_INDEX_CONTROL_OPTIONS:
            switch (controller_options_index)
            {
                case CONTROLLER_OPTIONS_INDEX_STYLE:
                    controller_options_controlstyle_navigation();
                    break;

                case CONTROLLER_OPTIONS_INDEX_INPUTS:
                    controller_options_inputs_navigation();
            }
            watch_screen2_navigation();
            break;

        case WATCH_INDEX_GAME_OPTIONS:
            switch (game_options_index)
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
                    sub_GAME_7F0A5998();
            }
            watch_screen3_navigation();
            break;

        case WATCH_INDEX_MISSION_BRIEFING:
            watch_screen4_navigation();
            break;

        case WATCH_INDEX_INVENTORY:
            watch_screen1_navigation();
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


#ifdef NONMATCHING
/**
 * Decomp notes: match down to regalloc.
*/
void set_page_rectangle_colors(s32 watch_screen_index, struct WatchVertex *arg2)
{
    s32 i;
    s32 limit;

    // Default color for rectangle.
    for (i=0; i<(4 * WATCH_NUMBER_SCREENS); i++)
    {
        arg2[i].color.r = 0x20;
        arg2[i].color.g = 0x70;
        arg2[i].color.b = 0x20;
    }

    i = watch_screen_index * 4;
    limit = i + 3;
    for ( ; i <= limit; i++)
    {
        // Color of currently selected screen
        arg2[i].color.r = 0x50;
        arg2[i].color.g = 0xF0;
        arg2[i].color.b = 0x50;

        if (watch_item_is_actively_selected)
        {
            // Color of currently selected screen when a menu option is selected.
            // This applies on main screen, game options, controller options, objective status, but not inventory.
            arg2[i].color.r = 0x30;
            arg2[i].color.g = 0xA0;
            arg2[i].color.b = 0x30;
        }
    }
}
#else
GLOBAL_ASM(
.text
glabel set_page_rectangle_colors
/* 0DBBDC 7F0A70AC 00001025 */  move  $v0, $zero
/* 0DBBE0 7F0A70B0 00A01825 */  move  $v1, $a1
/* 0DBBE4 7F0A70B4 24070070 */  li    $a3, 112
/* 0DBBE8 7F0A70B8 24060020 */  li    $a2, 32
.L7F0A70BC:
/* 0DBBEC 7F0A70BC 24420001 */  addiu $v0, $v0, 1
/* 0DBBF0 7F0A70C0 28410014 */  slti  $at, $v0, 0x14
/* 0DBBF4 7F0A70C4 24630010 */  addiu $v1, $v1, 0x10
/* 0DBBF8 7F0A70C8 A066FFFC */  sb    $a2, -4($v1)
/* 0DBBFC 7F0A70CC A067FFFD */  sb    $a3, -3($v1)
/* 0DBC00 7F0A70D0 1420FFFA */  bnez  $at, .L7F0A70BC
/* 0DBC04 7F0A70D4 A066FFFE */   sb    $a2, -2($v1)
/* 0DBC08 7F0A70D8 00041880 */  sll   $v1, $a0, 2
/* 0DBC0C 7F0A70DC 24660003 */  addiu $a2, $v1, 3
/* 0DBC10 7F0A70E0 00C3082A */  slt   $at, $a2, $v1
/* 0DBC14 7F0A70E4 14200017 */  bnez  $at, .L7F0A7144
/* 0DBC18 7F0A70E8 00037100 */   sll   $t6, $v1, 4
/* 0DBC1C 7F0A70EC 00067900 */  sll   $t7, $a2, 4
/* 0DBC20 7F0A70F0 01E52021 */  addu  $a0, $t7, $a1
/* 0DBC24 7F0A70F4 3C068004 */  lui   $a2, %hi(watch_item_is_actively_selected)
/* 0DBC28 7F0A70F8 00AE1821 */  addu  $v1, $a1, $t6
/* 0DBC2C 7F0A70FC 240500F0 */  li    $a1, 240
/* 0DBC30 7F0A7100 24C609A8 */  addiu $a2, %lo(watch_item_is_actively_selected) # addiu $a2, $a2, 0x9a8
/* 0DBC34 7F0A7104 240800A0 */  li    $t0, 160
/* 0DBC38 7F0A7108 24070030 */  li    $a3, 48
/* 0DBC3C 7F0A710C 24020050 */  li    $v0, 80
/* 0DBC40 7F0A7110 A062000C */  sb    $v0, 0xc($v1)
.L7F0A7114:
/* 0DBC44 7F0A7114 A065000D */  sb    $a1, 0xd($v1)
/* 0DBC48 7F0A7118 A062000E */  sb    $v0, 0xe($v1)
/* 0DBC4C 7F0A711C 8CD80000 */  lw    $t8, ($a2)
/* 0DBC50 7F0A7120 53000005 */  beql  $t8, $zero, .L7F0A7138
/* 0DBC54 7F0A7124 24630010 */   addiu $v1, $v1, 0x10
/* 0DBC58 7F0A7128 A067000C */  sb    $a3, 0xc($v1)
/* 0DBC5C 7F0A712C A068000D */  sb    $t0, 0xd($v1)
/* 0DBC60 7F0A7130 A067000E */  sb    $a3, 0xe($v1)
/* 0DBC64 7F0A7134 24630010 */  addiu $v1, $v1, 0x10
.L7F0A7138:
/* 0DBC68 7F0A7138 0083082B */  sltu  $at, $a0, $v1
/* 0DBC6C 7F0A713C 5020FFF5 */  beql  $at, $zero, .L7F0A7114
/* 0DBC70 7F0A7140 A062000C */   sb    $v0, 0xc($v1)
.L7F0A7144:
/* 0DBC74 7F0A7144 03E00008 */  jr    $ra
/* 0DBC78 7F0A7148 00000000 */   nop
)
#endif





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
    /*sp3C = */sub_GAME_7F0A3978(sp3C, OS_PHYSICAL_TO_K0(sp44), WATCH_BACKGROUND_VERTEX_COUNT, 0);

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
        /*sp40 = */sub_GAME_7F0A3978(sp40, OS_PHYSICAL_TO_K0(sp48), WATCH_BACKGROUND_VERTEX_COUNT, 0);

        gDPSetRenderMode(gdl++, G_RM_AA_PCL_SURF, G_RM_AA_PCL_SURF2);
    }
    else
    {
        sub_GAME_7F0A33F8(sp48, WATCH_BACKGROUND_VERTEX_COUNT, 0.899999976158f, 1);
        /*sp40 = */sub_GAME_7F0A3978(sp40, OS_PHYSICAL_TO_K0(sp48), WATCH_BACKGROUND_VERTEX_COUNT, 1);

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
        sub_GAME_7F0A68D8(g_CurrentPlayer->buffer_for_watch_static_vertices);

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

    sp78 = (j_text_trigger ? 0xF : 0) + 0xBD;

    sp74 = (j_text_trigger ? 0xA : 0) + 0x88;

    sp70 = sp6C = sp68 = (j_text_trigger ? 3 : 0) + (PAL ? 0x4E : 0x4C);

    if (watch_item_is_actively_selected != 0)
    {
        if (D_800409A4 == 0)
        {
            if ((joyGetStickX(PLAYER_1) >= 0x2E) || (joyGetButtons(PLAYER_1, 0x111) != 0))
            {
                D_800409A4 = 1;
            }
        }
        else
        {
            if (D_800409A4 != 0)
            {
                if ((joyGetStickX(PLAYER_1) < -0x2D) || (joyGetButtons(PLAYER_1, 0x222) != 0))
                {
                    D_800409A4 = 0;
                }
            }
        }
    }

    if (watch_item_is_actively_selected != 0)
    {
        textMeasure(&sp60, &sp64, sp54, pFontChars, pFontFile, 0);

        gdl = textRender(gdl, &sp7C, &sp70, sp54, pFontChars, pFontFile, 0xA0FFA0F0, sp64, sp60, 0, 0);

        if (D_800409A4 != 0)
        {
            gdl = textRenderOutlined(gdl, &sp78, &sp6C, sp50, pFontChars, pFontFile, -1, 0x7000A0, viGetX(), viGetY(), 0, 0);
            gdl = textRender(gdl, &sp74, &sp68, sp4C, pFontChars, pFontFile, 0xFF00B0, viGetX(), viGetY(), 0, 0);
        }
        else
        {
            if (D_800409A4 == 0)
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
    s32 joffset;

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

    gdl = microcode_constructor(gdl);
    textMeasure(&sp5C, &sp58, txtptr_1, pFontChars, pFontFile, 0);
    sp64 = 0x51;
    sp60 = YOFFSET_MISSIONSTATUS;
    gdl = textRender(gdl, &sp64, &sp60, txtptr_1, pFontChars, pFontFile, 0xFF00B0, sp58, sp5C, 0, 0);

    if (j_text_trigger)
    {
        joffset = 0x22;
    }
    else
    {
        joffset = 0;
    }

    sp64 = sp64 + sp58 + joffset + 4;
    sp60 = sp60 - sp5C;
    textMeasure(&sp5C, &sp58, txtptr_2, pFontChars, pFontFile, 0);
    gdl = textRender(gdl, &sp64, &sp60, txtptr_2, pFontChars, pFontFile, sp4C, sp58, sp5C, 0, 0);
    gdl = draw_abort_cancel_confirm(gdl);

    return gdl;
}


Gfx *empty_draw_function(Gfx *gdl) {
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
    s32 joffset;

    txtptr = langGet(getStringID(LOPTIONS, OPTION_STR_2B_QWATCHVERSION_LF)); //q watch v2.01 beta

    if (j_text_trigger)
    {
        joffset = -5;
    }
    else
    {
        joffset = 0;
    }
    sp50 = joffset + 0x65;
    sp4C = YOFFSET_7;
    sp48 = 0;
    sp44 = 0;
    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;
    gdl = microcode_constructor(gdl);
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
Gfx* draw_current_hand_item_and_ammo(Gfx* gdl) {
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

    if (get_debug_gunwatchpos_flag() != 0) {
        gitem = &gitem_structs[getCurrentPlayerWeaponId(0)];

        if (joyGetButtons(0, 2) != 0) {
           gitem->watch_pos_y -= 2.0f;
        }

        if (joyGetButtons(0, 1) != 0) {
            gitem->watch_pos_y += 2.0f;
        }

        if (joyGetButtons(0, 4) != 0) {
           gitem->watch_pos_x += 2.0f;
        }

        if (joyGetButtons(0, 8) != 0) {
            gitem->watch_pos_x -= 2.0f;
        }

        if (joyGetButtons(0, 0x20) != 0) {
            gitem->watch_pos_z *= 0.98000002f;
        }

        if (joyGetButtons(0, 0x10) != 0) {
             gitem->watch_pos_z *= 1.0204082f;
        }
#ifdef DEBUG

            osSyncPrintf("gun watch pos x=%f[CL,CR] y=%f[CD,CU] z=%f[TL,TR] ", gitem->watch_pos_x, gitem->watch_pos_y, gitem->watch_pos_z);

#endif
    }


#if defined(LEFTOVERDEBUG)
    guPerspective(sp114, &perspNorm, sp60, 1.33333337f, 10.0f, 10000.0f, 1.0f);
#else
    guPerspective(sp114, &perspNorm, sp60, 1.2838470f, 10.0f, 10000.0f, 1.0f);
#endif

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

    gdl = microcode_constructor(gunDrawWatchAmmoDisplay(gdl));

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

        if (get_debug_gunwatchpos_flag() != 0)
        {
            gitem = &gitem_structs[getCurrentPlayerWeaponId(0)];

            if (joyGetButtons(0, L_CBUTTONS) != 0)
            {
                gitem->equip_watch_x -= 2.0f;
            }

            if (joyGetButtons(0, R_CBUTTONS) != 0)
            {
                gitem->equip_watch_x += 2.0f;
            }

            if (joyGetButtons(0, D_CBUTTONS) != 0)
            {
                gitem->equip_watch_y += 2.0f;
            }

            if (joyGetButtons(0, U_CBUTTONS) != 0)
            {
                gitem->equip_watch_y -= 2.0f;
            }

            if (joyGetButtons(0, L_TRIG) != 0)
            {
                gitem->equip_watch_z *= 0.98000002f;
            }

            if (joyGetButtons(0, R_TRIG) != 0)
            {
                gitem->equip_watch_z *= 1.0204082f;
            }

#if defined(VERSION_US) && defined(DEBUG)
            osSyncPrintf(
                "gun list pos x=%f[CL,CR] y=%f[CD,CU] z=%f[TL,TR] ",
                gitem->equip_watch_x,
                gitem->equip_watch_y,
                gitem->equip_watch_z);
#endif
        }

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

#if defined(VERSION_JP) || defined(VERSION_EU)
            s32 pFontFile;
            s32 base_y;
            char formattedString[32];
#endif

#define LINEHEIGHT() (j_text_trigger ? 14 : 12)

#if defined(VERSION_JP) || defined(VERSION_EU)
#define WATCH_INV_BASE_Y() base_y
#else
#define WATCH_INV_BASE_Y() 0x8C
#endif

            textheight = 0;
            textwidth = 0;

#if defined(VERSION_EU)
            pFontFile2 = ptrFontBankGothic;
            pFontChars2 = ptrFontBankGothicChars;
            base_y = (j_text_trigger) ? (0x82) : (0xAA);
            string_builder_allocation[0] = 0;
#elif defined(VERSION_JP)
            pFontFile2 = ptrFontBankGothic;
            pFontChars2 = ptrFontBankGothicChars;
            base_y = (j_text_trigger) ? (0x82) : (0x8C);
            string_builder_allocation[0] = 0;
#else
            string_builder_allocation[0] = 0;
            pFontFile2 = ptrFontBankGothic;
            pFontChars2 = ptrFontBankGothicChars;
#endif

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

            x1 = 0x4E;
            y1 = WATCH_INV_BASE_Y();

            temp_s0_3 = 1;
            temp_s0_3 = (LINEHEIGHT() * 2) + WATCH_INV_BASE_Y() + temp_s0_3;

            gdl = microcode_constructor(gdl);

            textMeasure(&textheight, &textwidth, string_builder_allocation, pFontChars2, pFontFile2, LINEHEIGHT());

            gdl = microcode_constructor_related_to_menus(gdl, 0x4E, WATCH_INV_BASE_Y(), textwidth + 0x4E, (LINEHEIGHT() * 5) + WATCH_INV_BASE_Y(), 0);

            gdl = textRender(gdl, &x1, &y1, string_builder_allocation, pFontChars2, pFontFile2, 0xAA00B0, textwidth + 1, LINEHEIGHT() * 5, watch_inventory_text_y, LINEHEIGHT());

            gdl = microcode_constructor_related_to_menus(gdl, 0x4B, temp_s0_3, textwidth + 0x52, (LINEHEIGHT() + temp_s0_3) - 2, 0x800050);

            {
#if !defined(VERSION_JP) && !defined(VERSION_EU)
                char formattedString[32];
                s32 pFontFile;
#endif
                s32 pFontChars;
                s32 x2;
                s32 y2;
                char *invItemName;

                pFontFile = ptrFontBankGothic;
                pFontChars = ptrFontBankGothicChars;
                invItemName = bondinvGetNameByIndex(g_curWatchItemIndex);

                sprintf(formattedString, "%d, %d\n%d %f\n", watch_inventory_text_y, watch_inventory_text_target_y, g_curWatchItemIndex, (f64) watch_inventory_cursor_pos);

                gdl = microcode_constructor(gdl);

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


Gfx *unused_draw_watch_inventory_page(Gfx *gdl, Mtx *param_2) {
    s32 temp_1;
    s32 sp70;
    s32 sp6C;
    s32 sp64,sp68; //unused?

    s32 sp60;
    s32 sp5C;
    s32 sp58;
    s32 sp54;
    s32 pFontFile;
    s32 pFontChars;

    u16 *long_name;
    s32 temp_2;

    sp58 = 0;
    sp54 = 0;

    pFontFile = ptrFontBankGothic;
    pFontChars = ptrFontBankGothicChars;

    long_name = bondinvGetLongNameByIndex(g_curWatchItemIndex);
    gdl = draw_background_health_and_armor(gdl, param_2, 0);

    if (check_watch_page_transistion_running() != 1)
    {
        temp_1 = D_800409C4;
        if (temp_1 > 0)
        {
            D_800409C4 = temp_1 - 1;
        }

        game_options_inventory_navigation();
        gdl = microcode_constructor(gdl);

        textMeasure(&sp58, &sp54, long_name, pFontChars, pFontFile, 0);

        sp70 = ((s32) (0xAA - sp54) / 2) + 0x4B;
        temp_2 = sp70;

        sp6C = 0x1E;
        gdl = microcode_constructor_related_to_menus(gdl, temp_2, 0x1E, sp60, sp5C, 0x800050);

        if (watch_inventory_text_is_settled)
        {
            sub_GAME_7F0A8378();
            if (D_800409C4 == 0)
            {
                gdl = textRender(gdl, &sp70, &sp6C, long_name, pFontChars, pFontFile, 0xA0FFA0F0, sp54, 0x64, 0, 0);
            }
            else
            {
                gdl = textRenderOutlined(gdl, &sp70, &sp6C, long_name, pFontChars, pFontFile, -1, 0x7000A0, sp54 + 1, 0x64, 0, 0);
            }
        }
        else
        {
            gdl = textRender(gdl, &sp70, &sp6C, long_name, pFontChars, pFontFile, 0xAA00B0, sp54, 0x64, 0, 0);
        }

    }

    return gdl;
}


/**
 * Address: 7F0A8D40
 */
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



/**
 * Address: 7F0A8ED0
 */
void watch_adjust_volume_slider(u16* outVolume) {
    s32 joy_x;
    s32 adjusted_volume;

    joy_x = joyGetStickX(PLAYER_1);
    adjusted_volume = *outVolume;

    if (joyGetButtons(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD)) {
        adjusted_volume = adjusted_volume + WATCH_VOL_ADJUST_STEP;
    } else if (joyGetButtons(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD)) {
        adjusted_volume = adjusted_volume - WATCH_VOL_ADJUST_STEP;
    }

    // Clamp stick deflection
    if (joy_x >= 0x47) {
        joy_x = 0x46;
    } else if (joy_x < -0x46) {
        joy_x = -0x46;
    }

    // Increase volume
    if (joy_x >= 8) {
        adjusted_volume += (joy_x * 0x800 + -0x3800) / 0x46;
    // Decrease volume
    } else if (joy_x < -7) {
        adjusted_volume += (joy_x * 0x800 + 0x3800) / 0x46;
    }

    // Clamp volume between min and max allowed volume.
    if (adjusted_volume >= VOLUME_MAX + 1) {
        *outVolume = VOLUME_MAX;
    } else if (adjusted_volume < 0) {
        *outVolume = 0;
    } else {
        *outVolume = adjusted_volume;
    }
}


/**
 * Address: 7F0A8FEC
 */
Gfx *draw_fx_volume_slider(Gfx *gdl)
{
    u16 volume;
    f32 fvolume;
    struct WatchVertex *vtx1;
    struct WatchVertex *vtx;
    Gfx *cmd;

    vtx1 = (struct WatchVertex *)dynAllocateVertices(12);

    volume = sndGetSfxSlotFirstNaturalVolume();

    if (watch_item_is_actively_selected && game_options_index == 1)
    {
        watch_adjust_volume_slider(&volume);
    }

    fvolume = (f32)(u32)volume / 32767.0f;

    sndApplyVolumeAllSfxSlot(volume);

    if (1);

    cmd = gdl++;
    gDPSetRenderMode(cmd, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gdl = sub_GAME_7F0A3B40(gdl, OS_K0_TO_PHYSICAL(vtx1));
    vtx = setup_watch_rectangles(vtx1, 0, 0, 600, 20, -299, -205);

    gdl = sub_GAME_7F0A3B40(gdl, OS_K0_TO_PHYSICAL(vtx));
    vtx = setup_watch_rectangles(vtx, 0, 0, 600, 20, -299, -205);

    gdl = sub_GAME_7F0A3B40(gdl, OS_K0_TO_PHYSICAL(vtx));
    setup_watch_rectangles(vtx, 0, 0, 600, 20, -299, -205);

    update_volume_slider_verts(vtx1, fvolume, 30);

    return gdl;
}


u16 call_sndGetSfxSlotFirstNaturalVolume(void) {
    return sndGetSfxSlotFirstNaturalVolume();
}


void sub_GAME_7F0A91A0(u16 arg0) {
    sndApplyVolumeAllSfxSlot(arg0);
}


/**
 * Address: 7F0A91C8
 */
Gfx *draw_music_volume_slider(Gfx *gdl)
{
    u16 volume;
    f32 fvolume;
    struct WatchVertex *vtx1;
    struct WatchVertex *vtx;
    Gfx *cmd;

    vtx1 = (struct WatchVertex *)dynAllocateVertices(12);
    volume = get_mTrack2Vol();

    if (watch_item_is_actively_selected && game_options_index == 0) {
        watch_adjust_volume_slider(&volume);
    }

    fvolume = (f32)(u32)volume / 32767.0f;
    set_mTrack2Vol(volume);

    if(1);

    cmd = gdl++;
    gDPSetRenderMode(cmd, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gdl = sub_GAME_7F0A3B40(gdl, OS_K0_TO_PHYSICAL(vtx1));
    vtx = setup_watch_rectangles(vtx1, 0, 0, 600, 20, -299, -275);


    gdl = sub_GAME_7F0A3B40(gdl, OS_K0_TO_PHYSICAL(vtx));
    vtx = setup_watch_rectangles(vtx, 0, 0, 600, 20, -299, -275);

    gdl = sub_GAME_7F0A3B40(gdl, OS_K0_TO_PHYSICAL(vtx));
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
        gdl = microcode_constructor_related_to_menus(gdl, textx - 1, (y + outlined) + 1, textright + 1, textbottom + 1, bgcolour);
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


s32 sub_GAME_7F0A9610(void) {

    if ((g_WatchControllerSpinAngle < 0.1f) &&
        (g_WatchControllerSpinAngle > -0.1f) &&
        (g_WatchControllerPitch < 0.1f) &&
        (g_WatchControllerPitch > -0.1f))
    {

        return 1;

    }
    return 0;
}

#ifdef NONMATCHING
// just can't quite crack it, current score: 350 95.78%
// https://decomp.me/scratch/qoOjf
void sub_GAME_7F0A9684(s8 arg0, s32* arg1, f32* arg2, f32* arg3) {
    s32 temp_v0;

    if ((joyGetStickX(arg0) >= 10 || joyGetStickX(arg0) < -9)
        && (watch_item_is_actively_selected != 0)) {
        temp_v0 = 0;
        if (controller_options_index != 1) {
            *arg1 = 0;
        }
    } else {
        temp_v0 = *arg1;
    }
    if (temp_v0 < 100) {
        *arg1 = temp_v0 + 1;
        temp_v0 = temp_v0 + 1;
    } else {
        *arg1 = 0;
    }
    if (temp_v0 >= 100) {
        *arg2 = sub_GAME_7F0A95C4(*arg2, -*arg3 / 10.0f, 4.0f);
    } else if ((watch_item_is_actively_selected != 0) && (controller_options_index == 1)) {
        *arg2 = sub_GAME_7F0A95C4(*arg2, -(f32)joyGetStickX(arg0) * 0.2f * 6.2831855f / 360.0f, 4.0f);
    }
}
#else
GLOBAL_ASM(
.late_rodata
glabel D_80058548
.word 0x3e4ccccd /*0.2*/
glabel D_8005854C
.word 0x40c90fdb /*6.2831855*/
.text
glabel sub_GAME_7F0A9684
/* 0DE1B4 7F0A9684 27BDFFE8 */  addiu $sp, $sp, -0x18
/* 0DE1B8 7F0A9688 AFBF0014 */  sw    $ra, 0x14($sp)
/* 0DE1BC 7F0A968C AFA40018 */  sw    $a0, 0x18($sp)
/* 0DE1C0 7F0A9690 AFA60020 */  sw    $a2, 0x20($sp)
/* 0DE1C4 7F0A9694 AFA70024 */  sw    $a3, 0x24($sp)
/* 0DE1C8 7F0A9698 83A4001B */  lb    $a0, 0x1b($sp)
/* 0DE1CC 7F0A969C 0C00303B */  jal   joyGetStickX
/* 0DE1D0 7F0A96A0 AFA5001C */   sw    $a1, 0x1c($sp)
/* 0DE1D4 7F0A96A4 2841000A */  slti  $at, $v0, 0xa
/* 0DE1D8 7F0A96A8 10200007 */  beqz  $at, .L7F0A96C8
/* 0DE1DC 7F0A96AC 8FA5001C */   lw    $a1, 0x1c($sp)
/* 0DE1E0 7F0A96B0 83A4001B */  lb    $a0, 0x1b($sp)
/* 0DE1E4 7F0A96B4 0C00303B */  jal   joyGetStickX
/* 0DE1E8 7F0A96B8 AFA5001C */   sw    $a1, 0x1c($sp)
/* 0DE1EC 7F0A96BC 2841FFF7 */  slti  $at, $v0, -9
/* 0DE1F0 7F0A96C0 1020000B */  beqz  $at, .L7F0A96F0
/* 0DE1F4 7F0A96C4 8FA5001C */   lw    $a1, 0x1c($sp)
.L7F0A96C8:
/* 0DE1F8 7F0A96C8 3C0E8004 */  lui   $t6, %hi(watch_item_is_actively_selected)
/* 0DE1FC 7F0A96CC 8DCE09A8 */  lw    $t6, %lo(watch_item_is_actively_selected)($t6)
/* 0DE200 7F0A96D0 3C0F8004 */  lui   $t7, %hi(controller_options_index)
/* 0DE204 7F0A96D4 51C00007 */  beql  $t6, $zero, .L7F0A96F4
/* 0DE208 7F0A96D8 8CA20000 */   lw    $v0, ($a1)
/* 0DE20C 7F0A96DC 8DEF0998 */  lw    $t7, %lo(controller_options_index)($t7)
/* 0DE210 7F0A96E0 24010001 */  li    $at, 1
/* 0DE214 7F0A96E4 00001025 */  move  $v0, $zero
/* 0DE218 7F0A96E8 51E10009 */  beql  $t7, $at, .L7F0A9710
/* 0DE21C 7F0A96EC ACA00000 */   sw    $zero, ($a1)
.L7F0A96F0:
/* 0DE220 7F0A96F0 8CA20000 */  lw    $v0, ($a1)
.L7F0A96F4:
/* 0DE224 7F0A96F4 28410064 */  slti  $at, $v0, 0x64
/* 0DE228 7F0A96F8 10200005 */  beqz  $at, .L7F0A9710
/* 0DE22C 7F0A96FC 24580001 */   addiu $t8, $v0, 1
/* 0DE230 7F0A9700 ACB80000 */  sw    $t8, ($a1)
/* 0DE234 7F0A9704 10000002 */  b     .L7F0A9710
/* 0DE238 7F0A9708 03001025 */   move  $v0, $t8
/* 0DE23C 7F0A970C ACA00000 */  sw    $zero, ($a1)
.L7F0A9710:
/* 0DE240 7F0A9710 28410064 */  slti  $at, $v0, 0x64
/* 0DE244 7F0A9714 1420000E */  bnez  $at, .L7F0A9750
/* 0DE248 7F0A9718 3C0A8004 */   lui   $t2, %hi(watch_item_is_actively_selected)
/* 0DE24C 7F0A971C 8FA80024 */  lw    $t0, 0x24($sp)
/* 0DE250 7F0A9720 3C014120 */  li    $at, 0x41200000 # 10.000000
/* 0DE254 7F0A9724 44814000 */  mtc1  $at, $f8
/* 0DE258 7F0A9728 C5040000 */  lwc1  $f4, ($t0)
/* 0DE25C 7F0A972C 8FB90020 */  lw    $t9, 0x20($sp)
/* 0DE260 7F0A9730 3C064080 */  lui   $a2, 0x4080
/* 0DE264 7F0A9734 46002187 */  neg.s $f6, $f4
/* 0DE268 7F0A9738 C72C0000 */  lwc1  $f12, ($t9)
/* 0DE26C 7F0A973C 0FC2A571 */  jal   sub_GAME_7F0A95C4
/* 0DE270 7F0A9740 46083383 */   div.s $f14, $f6, $f8
/* 0DE274 7F0A9744 8FA90020 */  lw    $t1, 0x20($sp)
/* 0DE278 7F0A9748 1000001D */  b     .L7F0A97C0
/* 0DE27C 7F0A974C E5200000 */   swc1  $f0, ($t1)
.L7F0A9750:
/* 0DE280 7F0A9750 8D4A09A8 */  lw    $t2, %lo(watch_item_is_actively_selected)($t2)
/* 0DE284 7F0A9754 3C0B8004 */  lui   $t3, %hi(controller_options_index)
/* 0DE288 7F0A9758 5140001A */  beql  $t2, $zero, .L7F0A97C4
/* 0DE28C 7F0A975C 8FBF0014 */   lw    $ra, 0x14($sp)
/* 0DE290 7F0A9760 8D6B0998 */  lw    $t3, %lo(controller_options_index)($t3)
/* 0DE294 7F0A9764 24010001 */  li    $at, 1
/* 0DE298 7F0A9768 55610016 */  bnel  $t3, $at, .L7F0A97C4
/* 0DE29C 7F0A976C 8FBF0014 */   lw    $ra, 0x14($sp)
/* 0DE2A0 7F0A9770 0C00303B */  jal   joyGetStickX
/* 0DE2A4 7F0A9774 83A4001B */   lb    $a0, 0x1b($sp)
/* 0DE2A8 7F0A9778 44825000 */  mtc1  $v0, $f10
/* 0DE2AC 7F0A977C 3C018006 */  lui   $at, %hi(D_80058548)
/* 0DE2B0 7F0A9780 C4248548 */  lwc1  $f4, %lo(D_80058548)($at)
/* 0DE2B4 7F0A9784 46805420 */  cvt.s.w $f16, $f10
/* 0DE2B8 7F0A9788 3C018006 */  lui   $at, %hi(D_8005854C)
/* 0DE2BC 7F0A978C C428854C */  lwc1  $f8, %lo(D_8005854C)($at)
/* 0DE2C0 7F0A9790 3C0143B4 */  li    $at, 0x43B40000 # 360.000000
/* 0DE2C4 7F0A9794 8FAC0020 */  lw    $t4, 0x20($sp)
/* 0DE2C8 7F0A9798 3C064080 */  lui   $a2, 0x4080
/* 0DE2CC 7F0A979C 46008487 */  neg.s $f18, $f16
/* 0DE2D0 7F0A97A0 44818000 */  mtc1  $at, $f16
/* 0DE2D4 7F0A97A4 46049182 */  mul.s $f6, $f18, $f4
/* 0DE2D8 7F0A97A8 C58C0000 */  lwc1  $f12, ($t4)
/* 0DE2DC 7F0A97AC 46083282 */  mul.s $f10, $f6, $f8
/* 0DE2E0 7F0A97B0 0FC2A571 */  jal   sub_GAME_7F0A95C4
/* 0DE2E4 7F0A97B4 46105383 */   div.s $f14, $f10, $f16
/* 0DE2E8 7F0A97B8 8FAD0020 */  lw    $t5, 0x20($sp)
/* 0DE2EC 7F0A97BC E5A00000 */  swc1  $f0, ($t5)
.L7F0A97C0:
/* 0DE2F0 7F0A97C0 8FBF0014 */  lw    $ra, 0x14($sp)
.L7F0A97C4:
/* 0DE2F4 7F0A97C4 27BD0018 */  addiu $sp, $sp, 0x18
/* 0DE2F8 7F0A97C8 03E00008 */  jr    $ra
/* 0DE2FC 7F0A97CC 00000000 */   nop
)
#endif





#ifdef NONMATCHING
void sub_GAME_7F0A97D0(void) {

}
#else
#if defined(LEFTOVERDEBUG)
GLOBAL_ASM(
.text
glabel sub_GAME_7F0A97D0
/* 0DE300 7F0A97D0 27BDF7B0 */  addiu $sp, $sp, -0x850
/* 0DE304 7F0A97D4 3C068004 */  lui   $a2, %hi(D_800409D8)
/* 0DE308 7F0A97D8 8CC609D8 */  lw    $a2, %lo(D_800409D8)($a2)
/* 0DE30C 7F0A97DC 3C0E8004 */  lui   $t6, %hi(ptrFontBankGothic)
/* 0DE310 7F0A97E0 3C0F8004 */  lui   $t7, %hi(ptrFontBankGothicChars)
/* 0DE314 7F0A97E4 8DCE0EAC */  lw    $t6, %lo(ptrFontBankGothic)($t6)
/* 0DE318 7F0A97E8 8DEF0EB0 */  lw    $t7, %lo(ptrFontBankGothicChars)($t7)
/* 0DE31C 7F0A97EC AFB00034 */  sw    $s0, 0x34($sp)
/* 0DE320 7F0A97F0 AFBF003C */  sw    $ra, 0x3c($sp)
/* 0DE324 7F0A97F4 AFB10038 */  sw    $s1, 0x38($sp)
/* 0DE328 7F0A97F8 AFA40850 */  sw    $a0, 0x850($sp)
/* 0DE32C 7F0A97FC AFA00060 */  sw    $zero, 0x60($sp)
/* 0DE330 7F0A9800 AFA0005C */  sw    $zero, 0x5c($sp)
/* 0DE334 7F0A9804 A3A00074 */  sb    $zero, 0x74($sp)
/* 0DE338 7F0A9808 00008025 */  move  $s0, $zero
/* 0DE33C 7F0A980C AFAE0058 */  sw    $t6, 0x58($sp)
/* 0DE340 7F0A9810 18C0000F */  blez  $a2, .L7F0A9850
/* 0DE344 7F0A9814 AFAF0054 */   sw    $t7, 0x54($sp)
/* 0DE348 7F0A9818 3C118004 */  lui   $s1, %hi(D_800409DC)
/* 0DE34C 7F0A981C 263109DC */  addiu $s1, %lo(D_800409DC) # addiu $s1, $s1, 0x9dc
.L7F0A9820:
/* 0DE350 7F0A9820 0FC30776 */  jal   langGet
/* 0DE354 7F0A9824 96240000 */   lhu   $a0, ($s1)
/* 0DE358 7F0A9828 27A40074 */  addiu $a0, $sp, 0x74
/* 0DE35C 7F0A982C 0C0029FF */  jal   strcat
/* 0DE360 7F0A9830 00402825 */   move  $a1, $v0
/* 0DE364 7F0A9834 3C068004 */  lui   $a2, %hi(D_800409D8)
/* 0DE368 7F0A9838 8CC609D8 */  lw    $a2, %lo(D_800409D8)($a2)
/* 0DE36C 7F0A983C 26100001 */  addiu $s0, $s0, 1
/* 0DE370 7F0A9840 26310014 */  addiu $s1, $s1, 0x14
/* 0DE374 7F0A9844 0206082A */  slt   $at, $s0, $a2
/* 0DE378 7F0A9848 1420FFF5 */  bnez  $at, .L7F0A9820
/* 0DE37C 7F0A984C 00000000 */   nop
.L7F0A9850:
/* 0DE380 7F0A9850 3C188004 */  lui   $t8, %hi(watch_item_is_actively_selected)
/* 0DE384 7F0A9854 8F1809A8 */  lw    $t8, %lo(watch_item_is_actively_selected)($t8)
/* 0DE388 7F0A9858 3C198004 */  lui   $t9, %hi(controller_options_index)
/* 0DE38C 7F0A985C 13000019 */  beqz  $t8, .L7F0A98C4
/* 0DE390 7F0A9860 00000000 */   nop
/* 0DE394 7F0A9864 8F390998 */  lw    $t9, %lo(controller_options_index)($t9)
/* 0DE398 7F0A9868 3C118005 */  lui   $s1, %hi(j_text_trigger)
/* 0DE39C 7F0A986C 263184D0 */  addiu $s1, %lo(j_text_trigger) # addiu $s1, $s1, -0x7b30
/* 0DE3A0 7F0A9870 17200014 */  bnez  $t9, .L7F0A98C4
/* 0DE3A4 7F0A9874 3C038008 */   lui   $v1, %hi(g_CurrentPlayer)
/* 0DE3A8 7F0A9878 8E280000 */  lw    $t0, ($s1)
/* 0DE3AC 7F0A987C 240B0001 */  li    $t3, 1
/* 0DE3B0 7F0A9880 2402000A */  li    $v0, 10
/* 0DE3B4 7F0A9884 11000003 */  beqz  $t0, .L7F0A9894
/* 0DE3B8 7F0A9888 00000000 */   nop
/* 0DE3BC 7F0A988C 10000001 */  b     .L7F0A9894
/* 0DE3C0 7F0A9890 2402000E */   li    $v0, 14
.L7F0A9894:
/* 0DE3C4 7F0A9894 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0DE3C8 7F0A9898 AFA00018 */  sw    $zero, 0x18($sp)
/* 0DE3CC 7F0A989C AFAB001C */  sw    $t3, 0x1c($sp)
/* 0DE3D0 7F0A98A0 24692A5C */  addiu $t1, $v1, 0x2a5c
/* 0DE3D4 7F0A98A4 246A2A68 */  addiu $t2, $v1, 0x2a68
/* 0DE3D8 7F0A98A8 AFAA0014 */  sw    $t2, 0x14($sp)
/* 0DE3DC 7F0A98AC AFA90010 */  sw    $t1, 0x10($sp)
/* 0DE3E0 7F0A98B0 AFA20020 */  sw    $v0, 0x20($sp)
/* 0DE3E4 7F0A98B4 24642A60 */  addiu $a0, $v1, 0x2a60
/* 0DE3E8 7F0A98B8 24652A58 */  addiu $a1, $v1, 0x2a58
/* 0DE3EC 7F0A98BC 0FC29847 */  jal   sub_GAME_7F0A611C
/* 0DE3F0 7F0A98C0 24672A64 */   addiu $a3, $v1, 0x2a64
.L7F0A98C4:
/* 0DE3F4 7F0A98C4 3C118005 */  lui   $s1, %hi(j_text_trigger)
/* 0DE3F8 7F0A98C8 263184D0 */  addiu $s1, %lo(j_text_trigger) # addiu $s1, $s1, -0x7b30
/* 0DE3FC 7F0A98CC 8E220000 */  lw    $v0, ($s1)
/* 0DE400 7F0A98D0 240C00AA */  li    $t4, 170
/* 0DE404 7F0A98D4 240D001A */  li    $t5, 26
/* 0DE408 7F0A98D8 AFAC0070 */  sw    $t4, 0x70($sp)
/* 0DE40C 7F0A98DC 10400003 */  beqz  $v0, .L7F0A98EC
/* 0DE410 7F0A98E0 AFAD006C */   sw    $t5, 0x6c($sp)
/* 0DE414 7F0A98E4 10000002 */  b     .L7F0A98F0
/* 0DE418 7F0A98E8 2402000E */   li    $v0, 14
.L7F0A98EC:
/* 0DE41C 7F0A98EC 2402000A */  li    $v0, 10
.L7F0A98F0:
/* 0DE420 7F0A98F0 8FAE0058 */  lw    $t6, 0x58($sp)
/* 0DE424 7F0A98F4 27A40060 */  addiu $a0, $sp, 0x60
/* 0DE428 7F0A98F8 27A5005C */  addiu $a1, $sp, 0x5c
/* 0DE42C 7F0A98FC 27A60074 */  addiu $a2, $sp, 0x74
/* 0DE430 7F0A9900 8FA70054 */  lw    $a3, 0x54($sp)
/* 0DE434 7F0A9904 AFA20014 */  sw    $v0, 0x14($sp)
/* 0DE438 7F0A9908 0FC2BA63 */  jal   textMeasure
/* 0DE43C 7F0A990C AFAE0010 */   sw    $t6, 0x10($sp)
/* 0DE440 7F0A9910 8E220000 */  lw    $v0, ($s1)
/* 0DE444 7F0A9914 8FA40850 */  lw    $a0, 0x850($sp)
/* 0DE448 7F0A9918 27A50070 */  addiu $a1, $sp, 0x70
/* 0DE44C 7F0A991C 10400003 */  beqz  $v0, .L7F0A992C
/* 0DE450 7F0A9920 27A6006C */   addiu $a2, $sp, 0x6c
/* 0DE454 7F0A9924 10000002 */  b     .L7F0A9930
/* 0DE458 7F0A9928 2410000E */   li    $s0, 14
.L7F0A992C:
/* 0DE45C 7F0A992C 2410000A */  li    $s0, 10
.L7F0A9930:
/* 0DE460 7F0A9930 10400003 */  beqz  $v0, .L7F0A9940
/* 0DE464 7F0A9934 27A70074 */   addiu $a3, $sp, 0x74
/* 0DE468 7F0A9938 10000002 */  b     .L7F0A9944
/* 0DE46C 7F0A993C 2402000E */   li    $v0, 14
.L7F0A9940:
/* 0DE470 7F0A9940 2402000A */  li    $v0, 10
.L7F0A9944:
/* 0DE474 7F0A9944 8FAF0054 */  lw    $t7, 0x54($sp)
/* 0DE478 7F0A9948 8FB80058 */  lw    $t8, 0x58($sp)
/* 0DE47C 7F0A994C 8FA8005C */  lw    $t0, 0x5c($sp)
/* 0DE480 7F0A9950 3C098008 */  lui   $t1, %hi(g_CurrentPlayer)
/* 0DE484 7F0A9954 8D29A0B0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DE488 7F0A9958 3C1900AA */  lui   $t9, (0x00AA00B0 >> 16) # lui $t9, 0xaa
/* 0DE48C 7F0A995C 373900B0 */  ori   $t9, (0x00AA00B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DE490 7F0A9960 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DE494 7F0A9964 AFB00020 */  sw    $s0, 0x20($sp)
/* 0DE498 7F0A9968 AFAF0010 */  sw    $t7, 0x10($sp)
/* 0DE49C 7F0A996C AFB80014 */  sw    $t8, 0x14($sp)
/* 0DE4A0 7F0A9970 AFA8001C */  sw    $t0, 0x1c($sp)
/* 0DE4A4 7F0A9974 8D2A2A64 */  lw    $t2, 0x2a64($t1)
/* 0DE4A8 7F0A9978 AFA20028 */  sw    $v0, 0x28($sp)
/* 0DE4AC 7F0A997C 0FC2B6AF */  jal   textRender
/* 0DE4B0 7F0A9980 AFAA0024 */   sw    $t2, 0x24($sp)
/* 0DE4B4 7F0A9984 3C038008 */  lui   $v1, %hi(g_CurrentPlayer)
/* 0DE4B8 7F0A9988 8C63A0B0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0DE4BC 7F0A998C AFA20850 */  sw    $v0, 0x850($sp)
/* 0DE4C0 7F0A9990 8C6B2A68 */  lw    $t3, 0x2a68($v1)
/* 0DE4C4 7F0A9994 51600043 */  beql  $t3, $zero, .L7F0A9AA4
/* 0DE4C8 7F0A9998 8FBF003C */   lw    $ra, 0x3c($sp)
/* 0DE4CC 7F0A999C 8C6C2A58 */  lw    $t4, 0x2a58($v1)
/* 0DE4D0 7F0A99A0 3C048004 */  lui   $a0, %hi(D_800409DC)
/* 0DE4D4 7F0A99A4 000C6880 */  sll   $t5, $t4, 2
/* 0DE4D8 7F0A99A8 01AC6821 */  addu  $t5, $t5, $t4
/* 0DE4DC 7F0A99AC 000D6880 */  sll   $t5, $t5, 2
/* 0DE4E0 7F0A99B0 008D2021 */  addu  $a0, $a0, $t5
/* 0DE4E4 7F0A99B4 0FC30776 */  jal   langGet
/* 0DE4E8 7F0A99B8 948409DC */   lhu   $a0, %lo(D_800409DC)($a0)
/* 0DE4EC 7F0A99BC 8E2E0000 */  lw    $t6, ($s1)
/* 0DE4F0 7F0A99C0 00408025 */  move  $s0, $v0
/* 0DE4F4 7F0A99C4 27A40060 */  addiu $a0, $sp, 0x60
/* 0DE4F8 7F0A99C8 11C00003 */  beqz  $t6, .L7F0A99D8
/* 0DE4FC 7F0A99CC 27A5005C */   addiu $a1, $sp, 0x5c
/* 0DE500 7F0A99D0 10000002 */  b     .L7F0A99DC
/* 0DE504 7F0A99D4 2402000E */   li    $v0, 14
.L7F0A99D8:
/* 0DE508 7F0A99D8 2402000A */  li    $v0, 10
.L7F0A99DC:
/* 0DE50C 7F0A99DC 8FAF0058 */  lw    $t7, 0x58($sp)
/* 0DE510 7F0A99E0 02003025 */  move  $a2, $s0
/* 0DE514 7F0A99E4 8FA70054 */  lw    $a3, 0x54($sp)
/* 0DE518 7F0A99E8 AFA20014 */  sw    $v0, 0x14($sp)
/* 0DE51C 7F0A99EC 0FC2BA63 */  jal   textMeasure
/* 0DE520 7F0A99F0 AFAF0010 */   sw    $t7, 0x10($sp)
/* 0DE524 7F0A99F4 8E390000 */  lw    $t9, ($s1)
/* 0DE528 7F0A99F8 241800AA */  li    $t8, 170
/* 0DE52C 7F0A99FC AFB80070 */  sw    $t8, 0x70($sp)
/* 0DE530 7F0A9A00 13200003 */  beqz  $t9, .L7F0A9A10
/* 0DE534 7F0A9A04 3C098008 */   lui   $t1, %hi(g_CurrentPlayer)
/* 0DE538 7F0A9A08 10000001 */  b     .L7F0A9A10
/* 0DE53C 7F0A9A0C 00000000 */   nop
.L7F0A9A10:
/* 0DE540 7F0A9A10 8D29A0B0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DE544 7F0A9A14 2408001A */  li    $t0, 26
/* 0DE548 7F0A9A18 AFA8006C */  sw    $t0, 0x6c($sp)
/* 0DE54C 7F0A9A1C 8D2A2A58 */  lw    $t2, 0x2a58($t1)
/* 0DE550 7F0A9A20 3C048004 */  lui   $a0, %hi(D_800409DC)
/* 0DE554 7F0A9A24 000A5880 */  sll   $t3, $t2, 2
/* 0DE558 7F0A9A28 016A5821 */  addu  $t3, $t3, $t2
/* 0DE55C 7F0A9A2C 000B5880 */  sll   $t3, $t3, 2
/* 0DE560 7F0A9A30 008B2021 */  addu  $a0, $a0, $t3
/* 0DE564 7F0A9A34 0FC30776 */  jal   langGet
/* 0DE568 7F0A9A38 948409DC */   lhu   $a0, %lo(D_800409DC)($a0)
/* 0DE56C 7F0A9A3C 8E2C0000 */  lw    $t4, ($s1)
/* 0DE570 7F0A9A40 00408025 */  move  $s0, $v0
/* 0DE574 7F0A9A44 8FA40850 */  lw    $a0, 0x850($sp)
/* 0DE578 7F0A9A48 11800003 */  beqz  $t4, .L7F0A9A58
/* 0DE57C 7F0A9A4C 27A50070 */   addiu $a1, $sp, 0x70
/* 0DE580 7F0A9A50 10000002 */  b     .L7F0A9A5C
/* 0DE584 7F0A9A54 2402000E */   li    $v0, 14
.L7F0A9A58:
/* 0DE588 7F0A9A58 2402000A */  li    $v0, 10
.L7F0A9A5C:
/* 0DE58C 7F0A9A5C 8FAD0054 */  lw    $t5, 0x54($sp)
/* 0DE590 7F0A9A60 8FAE0058 */  lw    $t6, 0x58($sp)
/* 0DE594 7F0A9A64 8FB8005C */  lw    $t8, 0x5c($sp)
/* 0DE598 7F0A9A68 3C0FA0FF */  lui   $t7, (0xA0FFA0F0 >> 16) # lui $t7, 0xa0ff
/* 0DE59C 7F0A9A6C 35EFA0F0 */  ori   $t7, (0xA0FFA0F0 & 0xFFFF) # ori $t7, $t7, 0xa0f0
/* 0DE5A0 7F0A9A70 24190064 */  li    $t9, 100
/* 0DE5A4 7F0A9A74 AFB90020 */  sw    $t9, 0x20($sp)
/* 0DE5A8 7F0A9A78 AFAF0018 */  sw    $t7, 0x18($sp)
/* 0DE5AC 7F0A9A7C 27A6006C */  addiu $a2, $sp, 0x6c
/* 0DE5B0 7F0A9A80 02003825 */  move  $a3, $s0
/* 0DE5B4 7F0A9A84 AFA00024 */  sw    $zero, 0x24($sp)
/* 0DE5B8 7F0A9A88 AFA20028 */  sw    $v0, 0x28($sp)
/* 0DE5BC 7F0A9A8C AFAD0010 */  sw    $t5, 0x10($sp)
/* 0DE5C0 7F0A9A90 AFAE0014 */  sw    $t6, 0x14($sp)
/* 0DE5C4 7F0A9A94 0FC2B6AF */  jal   textRender
/* 0DE5C8 7F0A9A98 AFB8001C */   sw    $t8, 0x1c($sp)
/* 0DE5CC 7F0A9A9C AFA20850 */  sw    $v0, 0x850($sp)
/* 0DE5D0 7F0A9AA0 8FBF003C */  lw    $ra, 0x3c($sp)
.L7F0A9AA4:
/* 0DE5D4 7F0A9AA4 8FA20850 */  lw    $v0, 0x850($sp)
/* 0DE5D8 7F0A9AA8 8FB00034 */  lw    $s0, 0x34($sp)
/* 0DE5DC 7F0A9AAC 8FB10038 */  lw    $s1, 0x38($sp)
/* 0DE5E0 7F0A9AB0 03E00008 */  jr    $ra
/* 0DE5E4 7F0A9AB4 27BD0850 */   addiu $sp, $sp, 0x850
)
#endif

#if !defined(LEFTOVERDEBUG)
GLOBAL_ASM(
.text
glabel sub_GAME_7F0A97D0
/* 0DB500 7F0A8B10 27BDF7B0 */  addiu $sp, $sp, -0x850
/* 0DB504 7F0A8B14 3C068004 */  lui   $a2, %hi(D_800409D8) # $a2, 0x8004
/* 0DB508 7F0A8B18 8CC6A628 */  lw    $a2, %lo(D_800409D8)($a2)
/* 0DB50C 7F0A8B1C 3C0E8004 */  lui   $t6, %hi(ptrFontBankGothic) # $t6, 0x8004
/* 0DB510 7F0A8B20 3C0F8004 */  lui   $t7, %hi(ptrFontBankGothicChars) # $t7, 0x8004
/* 0DB514 7F0A8B24 8DCEAAFC */  lw    $t6, %lo(ptrFontBankGothic)($t6)
/* 0DB518 7F0A8B28 8DEFAB00 */  lw    $t7, %lo(ptrFontBankGothicChars)($t7)
/* 0DB51C 7F0A8B2C AFB00034 */  sw    $s0, 0x34($sp)
/* 0DB520 7F0A8B30 AFBF003C */  sw    $ra, 0x3c($sp)
/* 0DB524 7F0A8B34 AFB10038 */  sw    $s1, 0x38($sp)
/* 0DB528 7F0A8B38 AFA40850 */  sw    $a0, 0x850($sp)
/* 0DB52C 7F0A8B3C AFA00060 */  sw    $zero, 0x60($sp)
/* 0DB530 7F0A8B40 AFA0005C */  sw    $zero, 0x5c($sp)
/* 0DB534 7F0A8B44 A3A00074 */  sb    $zero, 0x74($sp)
/* 0DB538 7F0A8B48 00008025 */  move  $s0, $zero
/* 0DB53C 7F0A8B4C AFAE0058 */  sw    $t6, 0x58($sp)
/* 0DB540 7F0A8B50 18C0000F */  blez  $a2, .L7F0A8B90
/* 0DB544 7F0A8B54 AFAF0054 */   sw    $t7, 0x54($sp)
/* 0DB548 7F0A8B58 3C118004 */  lui   $s1, %hi(D_800409DC) # $s1, 0x8004
/* 0DB54C 7F0A8B5C 2631A62C */  addiu $s1, %lo(D_800409DC) # addiu $s1, $s1, -0x59d4
.L7F0A8B60:
/* 0DB550 7F0A8B60 0FC304AE */  jal   langGet
/* 0DB554 7F0A8B64 96240000 */   lhu   $a0, ($s1)
/* 0DB558 7F0A8B68 27A40074 */  addiu $a0, $sp, 0x74
/* 0DB55C 7F0A8B6C 0C002717 */  jal   strcat
/* 0DB560 7F0A8B70 00402825 */   move  $a1, $v0
/* 0DB564 7F0A8B74 3C068004 */  lui   $a2, %hi(D_800409D8) # $a2, 0x8004
/* 0DB568 7F0A8B78 8CC6A628 */  lw    $a2, %lo(D_800409D8)($a2)
/* 0DB56C 7F0A8B7C 26100001 */  addiu $s0, $s0, 1
/* 0DB570 7F0A8B80 26310014 */  addiu $s1, $s1, 0x14
/* 0DB574 7F0A8B84 0206082A */  slt   $at, $s0, $a2
/* 0DB578 7F0A8B88 1420FFF5 */  bnez  $at, .L7F0A8B60
/* 0DB57C 7F0A8B8C 00000000 */   nop
.L7F0A8B90:
/* 0DB580 7F0A8B90 3C188004 */  lui   $t8, %hi(watch_item_is_actively_selected) # $t8, 0x8004
/* 0DB584 7F0A8B94 8F18A5F8 */  lw    $t8, %lo(watch_item_is_actively_selected)($t8)
/* 0DB588 7F0A8B98 3C198004 */  lui   $t9, %hi(controller_options_index) # $t9, 0x8004
/* 0DB58C 7F0A8B9C 13000019 */  beqz  $t8, .L7F0A8C04
/* 0DB590 7F0A8BA0 00000000 */   nop
/* 0DB594 7F0A8BA4 8F39A5E8 */  lw    $t9, %lo(controller_options_index)($t9)
/* 0DB598 7F0A8BA8 3C118004 */  lui   $s1, %hi(j_text_trigger) # $s1, 0x8004
/* 0DB59C 7F0A8BAC 26311150 */  addiu $s1, %lo(j_text_trigger) # addiu $s1, $s1, 0x1150
/* 0DB5A0 7F0A8BB0 17200014 */  bnez  $t9, .L7F0A8C04
/* 0DB5A4 7F0A8BB4 3C038007 */   lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0DB5A8 7F0A8BB8 8E280000 */  lw    $t0, ($s1)
/* 0DB5AC 7F0A8BBC 240B0001 */  li    $t3, 1
/* 0DB5B0 7F0A8BC0 2402000A */  li    $v0, 10
/* 0DB5B4 7F0A8BC4 11000003 */  beqz  $t0, .L7F0A8BD4
/* 0DB5B8 7F0A8BC8 00000000 */   nop
/* 0DB5BC 7F0A8BCC 10000001 */  b     .L7F0A8BD4
/* 0DB5C0 7F0A8BD0 2402000E */   li    $v0, 14
.L7F0A8BD4:
/* 0DB5C4 7F0A8BD4 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0DB5C8 7F0A8BD8 AFA00018 */  sw    $zero, 0x18($sp)
/* 0DB5CC 7F0A8BDC AFAB001C */  sw    $t3, 0x1c($sp)
/* 0DB5D0 7F0A8BE0 24692A54 */  addiu $t1, $v1, 0x2a54
/* 0DB5D4 7F0A8BE4 246A2A60 */  addiu $t2, $v1, 0x2a60
/* 0DB5D8 7F0A8BE8 AFAA0014 */  sw    $t2, 0x14($sp)
/* 0DB5DC 7F0A8BEC AFA90010 */  sw    $t1, 0x10($sp)
/* 0DB5E0 7F0A8BF0 AFA20020 */  sw    $v0, 0x20($sp)
/* 0DB5E4 7F0A8BF4 24642A58 */  addiu $a0, $v1, 0x2a58
/* 0DB5E8 7F0A8BF8 24652A50 */  addiu $a1, $v1, 0x2a50
/* 0DB5EC 7F0A8BFC 0FC2950F */  jal   sub_GAME_7F0A611C
/* 0DB5F0 7F0A8C00 24672A5C */   addiu $a3, $v1, 0x2a5c
.L7F0A8C04:
/* 0DB5F4 7F0A8C04 3C118004 */  lui   $s1, %hi(j_text_trigger) # $s1, 0x8004
/* 0DB5F8 7F0A8C08 26311150 */  addiu $s1, %lo(j_text_trigger) # addiu $s1, $s1, 0x1150
/* 0DB5FC 7F0A8C0C 8E220000 */  lw    $v0, ($s1)
/* 0DB600 7F0A8C10 240C00AA */  li    $t4, 170
/* 0DB604 7F0A8C14 240D001A */  li    $t5, 26
/* 0DB608 7F0A8C18 AFAC0070 */  sw    $t4, 0x70($sp)
/* 0DB60C 7F0A8C1C 10400003 */  beqz  $v0, .L7F0A8C2C
/* 0DB610 7F0A8C20 AFAD006C */   sw    $t5, 0x6c($sp)
/* 0DB614 7F0A8C24 10000002 */  b     .L7F0A8C30
/* 0DB618 7F0A8C28 2402000E */   li    $v0, 14
.L7F0A8C2C:
/* 0DB61C 7F0A8C2C 2402000A */  li    $v0, 10
.L7F0A8C30:
/* 0DB620 7F0A8C30 8FAE0058 */  lw    $t6, 0x58($sp)
/* 0DB624 7F0A8C34 27A40060 */  addiu $a0, $sp, 0x60
/* 0DB628 7F0A8C38 27A5005C */  addiu $a1, $sp, 0x5c
/* 0DB62C 7F0A8C3C 27A60074 */  addiu $a2, $sp, 0x74
/* 0DB630 7F0A8C40 8FA70054 */  lw    $a3, 0x54($sp)
/* 0DB634 7F0A8C44 AFA20014 */  sw    $v0, 0x14($sp)
/* 0DB638 7F0A8C48 0FC2B713 */  jal   textMeasure
/* 0DB63C 7F0A8C4C AFAE0010 */   sw    $t6, 0x10($sp)
/* 0DB640 7F0A8C50 8E220000 */  lw    $v0, ($s1)
/* 0DB644 7F0A8C54 8FA40850 */  lw    $a0, 0x850($sp)
/* 0DB648 7F0A8C58 27A50070 */  addiu $a1, $sp, 0x70
/* 0DB64C 7F0A8C5C 10400003 */  beqz  $v0, .L7F0A8C6C
/* 0DB650 7F0A8C60 27A6006C */   addiu $a2, $sp, 0x6c
/* 0DB654 7F0A8C64 10000002 */  b     .L7F0A8C70
/* 0DB658 7F0A8C68 2410000E */   li    $s0, 14
.L7F0A8C6C:
/* 0DB65C 7F0A8C6C 2410000A */  li    $s0, 10
.L7F0A8C70:
/* 0DB660 7F0A8C70 10400003 */  beqz  $v0, .L7F0A8C80
/* 0DB664 7F0A8C74 27A70074 */   addiu $a3, $sp, 0x74
/* 0DB668 7F0A8C78 10000002 */  b     .L7F0A8C84
/* 0DB66C 7F0A8C7C 2402000E */   li    $v0, 14
.L7F0A8C80:
/* 0DB670 7F0A8C80 2402000A */  li    $v0, 10
.L7F0A8C84:
/* 0DB674 7F0A8C84 8FAF0054 */  lw    $t7, 0x54($sp)
/* 0DB678 7F0A8C88 8FB80058 */  lw    $t8, 0x58($sp)
/* 0DB67C 7F0A8C8C 8FA8005C */  lw    $t0, 0x5c($sp)
/* 0DB680 7F0A8C90 3C098007 */  lui   $t1, %hi(g_CurrentPlayer) # $t1, 0x8007
/* 0DB684 7F0A8C94 8D298BC0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DB688 7F0A8C98 3C1900AA */  lui   $t9, (0x00AA00B0 >> 16) # lui $t9, 0xaa
/* 0DB68C 7F0A8C9C 373900B0 */  ori   $t9, (0x00AA00B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DB690 7F0A8CA0 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DB694 7F0A8CA4 AFB00020 */  sw    $s0, 0x20($sp)
/* 0DB698 7F0A8CA8 AFAF0010 */  sw    $t7, 0x10($sp)
/* 0DB69C 7F0A8CAC AFB80014 */  sw    $t8, 0x14($sp)
/* 0DB6A0 7F0A8CB0 AFA8001C */  sw    $t0, 0x1c($sp)
/* 0DB6A4 7F0A8CB4 8D2A2A5C */  lw    $t2, 0x2a5c($t1)
/* 0DB6A8 7F0A8CB8 AFA20028 */  sw    $v0, 0x28($sp)
/* 0DB6AC 7F0A8CBC 0FC2B35F */  jal   textRender
/* 0DB6B0 7F0A8CC0 AFAA0024 */   sw    $t2, 0x24($sp)
/* 0DB6B4 7F0A8CC4 3C038007 */  lui   $v1, %hi(g_CurrentPlayer) # $v1, 0x8007
/* 0DB6B8 7F0A8CC8 8C638BC0 */  lw    $v1, %lo(g_CurrentPlayer)($v1)
/* 0DB6BC 7F0A8CCC AFA20850 */  sw    $v0, 0x850($sp)
/* 0DB6C0 7F0A8CD0 8C6B2A60 */  lw    $t3, 0x2a60($v1)
/* 0DB6C4 7F0A8CD4 51600043 */  beql  $t3, $zero, .L7F0A8DE4
/* 0DB6C8 7F0A8CD8 8FBF003C */   lw    $ra, 0x3c($sp)
/* 0DB6CC 7F0A8CDC 8C6C2A50 */  lw    $t4, 0x2a50($v1)
/* 0DB6D0 7F0A8CE0 3C048004 */  lui   $a0, %hi(D_800409DC)
/* 0DB6D4 7F0A8CE4 000C6880 */  sll   $t5, $t4, 2
/* 0DB6D8 7F0A8CE8 01AC6821 */  addu  $t5, $t5, $t4
/* 0DB6DC 7F0A8CEC 000D6880 */  sll   $t5, $t5, 2
/* 0DB6E0 7F0A8CF0 008D2021 */  addu  $a0, $a0, $t5
/* 0DB6E4 7F0A8CF4 0FC304AE */  jal   langGet
/* 0DB6E8 7F0A8CF8 9484A62C */   lhu   $a0, %lo(D_800409DC)($a0)
/* 0DB6EC 7F0A8CFC 8E2E0000 */  lw    $t6, ($s1)
/* 0DB6F0 7F0A8D00 00408025 */  move  $s0, $v0
/* 0DB6F4 7F0A8D04 27A40060 */  addiu $a0, $sp, 0x60
/* 0DB6F8 7F0A8D08 11C00003 */  beqz  $t6, .L7F0A8D18
/* 0DB6FC 7F0A8D0C 27A5005C */   addiu $a1, $sp, 0x5c
/* 0DB700 7F0A8D10 10000002 */  b     .L7F0A8D1C
/* 0DB704 7F0A8D14 2402000E */   li    $v0, 14
.L7F0A8D18:
/* 0DB708 7F0A8D18 2402000A */  li    $v0, 10
.L7F0A8D1C:
/* 0DB70C 7F0A8D1C 8FAF0058 */  lw    $t7, 0x58($sp)
/* 0DB710 7F0A8D20 02003025 */  move  $a2, $s0
/* 0DB714 7F0A8D24 8FA70054 */  lw    $a3, 0x54($sp)
/* 0DB718 7F0A8D28 AFA20014 */  sw    $v0, 0x14($sp)
/* 0DB71C 7F0A8D2C 0FC2B713 */  jal   textMeasure
/* 0DB720 7F0A8D30 AFAF0010 */   sw    $t7, 0x10($sp)
/* 0DB724 7F0A8D34 8E390000 */  lw    $t9, ($s1)
/* 0DB728 7F0A8D38 241800AA */  li    $t8, 170
/* 0DB72C 7F0A8D3C AFB80070 */  sw    $t8, 0x70($sp)
/* 0DB730 7F0A8D40 13200003 */  beqz  $t9, .L7F0A8D50
/* 0DB734 7F0A8D44 3C098007 */   lui   $t1, %hi(g_CurrentPlayer) # $t1, 0x8007
/* 0DB738 7F0A8D48 10000001 */  b     .L7F0A8D50
/* 0DB73C 7F0A8D4C 00000000 */   nop
.L7F0A8D50:
/* 0DB740 7F0A8D50 8D298BC0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DB744 7F0A8D54 2408001A */  li    $t0, 26
/* 0DB748 7F0A8D58 AFA8006C */  sw    $t0, 0x6c($sp)
/* 0DB74C 7F0A8D5C 8D2A2A50 */  lw    $t2, 0x2a50($t1)
/* 0DB750 7F0A8D60 3C048004 */  lui   $a0, %hi(D_800409DC)
/* 0DB754 7F0A8D64 000A5880 */  sll   $t3, $t2, 2
/* 0DB758 7F0A8D68 016A5821 */  addu  $t3, $t3, $t2
/* 0DB75C 7F0A8D6C 000B5880 */  sll   $t3, $t3, 2
/* 0DB760 7F0A8D70 008B2021 */  addu  $a0, $a0, $t3
/* 0DB764 7F0A8D74 0FC304AE */  jal   langGet
/* 0DB768 7F0A8D78 9484A62C */   lhu   $a0, %lo(D_800409DC)($a0)
/* 0DB76C 7F0A8D7C 8E2C0000 */  lw    $t4, ($s1)
/* 0DB770 7F0A8D80 00408025 */  move  $s0, $v0
/* 0DB774 7F0A8D84 8FA40850 */  lw    $a0, 0x850($sp)
/* 0DB778 7F0A8D88 11800003 */  beqz  $t4, .L7F0A8D98
/* 0DB77C 7F0A8D8C 27A50070 */   addiu $a1, $sp, 0x70
/* 0DB780 7F0A8D90 10000002 */  b     .L7F0A8D9C
/* 0DB784 7F0A8D94 2402000E */   li    $v0, 14
.L7F0A8D98:
/* 0DB788 7F0A8D98 2402000A */  li    $v0, 10
.L7F0A8D9C:
/* 0DB78C 7F0A8D9C 8FAD0054 */  lw    $t5, 0x54($sp)
/* 0DB790 7F0A8DA0 8FAE0058 */  lw    $t6, 0x58($sp)
/* 0DB794 7F0A8DA4 8FB8005C */  lw    $t8, 0x5c($sp)
/* 0DB798 7F0A8DA8 3C0FA0FF */  lui   $t7, (0xA0FFA0F0 >> 16) # lui $t7, 0xa0ff
/* 0DB79C 7F0A8DAC 35EFA0F0 */  ori   $t7, (0xA0FFA0F0 & 0xFFFF) # ori $t7, $t7, 0xa0f0
/* 0DB7A0 7F0A8DB0 24190064 */  li    $t9, 100
/* 0DB7A4 7F0A8DB4 AFB90020 */  sw    $t9, 0x20($sp)
/* 0DB7A8 7F0A8DB8 AFAF0018 */  sw    $t7, 0x18($sp)
/* 0DB7AC 7F0A8DBC 27A6006C */  addiu $a2, $sp, 0x6c
/* 0DB7B0 7F0A8DC0 02003825 */  move  $a3, $s0
/* 0DB7B4 7F0A8DC4 AFA00024 */  sw    $zero, 0x24($sp)
/* 0DB7B8 7F0A8DC8 AFA20028 */  sw    $v0, 0x28($sp)
/* 0DB7BC 7F0A8DCC AFAD0010 */  sw    $t5, 0x10($sp)
/* 0DB7C0 7F0A8DD0 AFAE0014 */  sw    $t6, 0x14($sp)
/* 0DB7C4 7F0A8DD4 0FC2B35F */  jal   textRender
/* 0DB7C8 7F0A8DD8 AFB8001C */   sw    $t8, 0x1c($sp)
/* 0DB7CC 7F0A8DDC AFA20850 */  sw    $v0, 0x850($sp)
/* 0DB7D0 7F0A8DE0 8FBF003C */  lw    $ra, 0x3c($sp)
.L7F0A8DE4:
/* 0DB7D4 7F0A8DE4 8FA20850 */  lw    $v0, 0x850($sp)
/* 0DB7D8 7F0A8DE8 8FB00034 */  lw    $s0, 0x34($sp)
/* 0DB7DC 7F0A8DEC 8FB10038 */  lw    $s1, 0x38($sp)
/* 0DB7E0 7F0A8DF0 03E00008 */  jr    $ra
/* 0DB7E4 7F0A8DF4 27BD0850 */   addiu $sp, $sp, 0x850
)
#endif
#endif





#ifdef NONMATCHING
void sub_GAME_7F0A9AB8(void) {

}
#else
//D:80057778
const char aA[] =  "(A)\n";
//D:80057780
const char aB[] =  "(B)\n";
//D:80057788
const char aZ[] =  "(Z)\n";
//D:80057790
const char aL[] =  "(L)\n";
//D:80057798
const char aR[] =  "(R)\n";
//D:800577A0
const char aC[] =  "(C)\n";
//D:800577A8
const char D_800577A8[] =  "(+)\n";
//D:800577B0
const char aS_0[] =  "(S)\n";
//D:800577B8
const char a3d[] =  "(3D)\n";

#if defined(LEFTOVERDEBUG)
GLOBAL_ASM(

.text
glabel sub_GAME_7F0A9AB8
/* 0DE5E8 7F0A9AB8 3C0E8004 */  lui   $t6, %hi(game_options_entries)
/* 0DE5EC 7F0A9ABC 8DCE0A84 */  lw    $t6, %lo(game_options_entries + 0x8)($t6)
/* 0DE5F0 7F0A9AC0 27BDFF50 */  addiu $sp, $sp, -0xb0
/* 0DE5F4 7F0A9AC4 AFB00038 */  sw    $s0, 0x38($sp)
/* 0DE5F8 7F0A9AC8 24010001 */  li    $at, 1
/* 0DE5FC 7F0A9ACC 00808025 */  move  $s0, $a0
/* 0DE600 7F0A9AD0 15C10008 */  bne   $t6, $at, .L7F0A9AF4
/* 0DE604 7F0A9AD4 AFBF003C */   sw    $ra, 0x3c($sp)
/* 0DE608 7F0A9AD8 0FC30776 */  jal   langGet
/* 0DE60C 7F0A9ADC 3404AC2D */   li    $a0, 44077
/* 0DE610 7F0A9AE0 AFA200AC */  sw    $v0, 0xac($sp)
/* 0DE614 7F0A9AE4 0FC30776 */  jal   langGet
/* 0DE618 7F0A9AE8 3404AC2C */   li    $a0, 44076
/* 0DE61C 7F0A9AEC 10000007 */  b     .L7F0A9B0C
/* 0DE620 7F0A9AF0 AFA200A8 */   sw    $v0, 0xa8($sp)
.L7F0A9AF4:
/* 0DE624 7F0A9AF4 0FC30776 */  jal   langGet
/* 0DE628 7F0A9AF8 3404AC2C */   li    $a0, 44076
/* 0DE62C 7F0A9AFC AFA200AC */  sw    $v0, 0xac($sp)
/* 0DE630 7F0A9B00 0FC30776 */  jal   langGet
/* 0DE634 7F0A9B04 3404AC2D */   li    $a0, 44077
/* 0DE638 7F0A9B08 AFA200A8 */  sw    $v0, 0xa8($sp)
.L7F0A9B0C:
/* 0DE63C 7F0A9B0C 3C188005 */  lui   $t8, %hi(aA)
/* 0DE640 7F0A9B10 27187778 */  addiu $t8, %lo(aA) # addiu $t8, $t8, 0x7778
/* 0DE644 7F0A9B14 8F010000 */  lw    $at, ($t8)
/* 0DE648 7F0A9B18 27AF00A0 */  addiu $t7, $sp, 0xa0
/* 0DE64C 7F0A9B1C 3C0B8005 */  lui   $t3, %hi(aB)
/* 0DE650 7F0A9B20 ADE10000 */  sw    $at, ($t7)
/* 0DE654 7F0A9B24 93010004 */  lbu   $at, 4($t8)
/* 0DE658 7F0A9B28 256B7780 */  addiu $t3, %lo(aB) # addiu $t3, $t3, 0x7780
/* 0DE65C 7F0A9B2C 27AA0098 */  addiu $t2, $sp, 0x98
/* 0DE660 7F0A9B30 A1E10004 */  sb    $at, 4($t7)
/* 0DE664 7F0A9B34 8D610000 */  lw    $at, ($t3)
/* 0DE668 7F0A9B38 3C198005 */  lui   $t9, %hi(aZ)
/* 0DE66C 7F0A9B3C 27397788 */  addiu $t9, %lo(aZ) # addiu $t9, $t9, 0x7788
/* 0DE670 7F0A9B40 AD410000 */  sw    $at, ($t2)
/* 0DE674 7F0A9B44 91610004 */  lbu   $at, 4($t3)
/* 0DE678 7F0A9B48 27A80090 */  addiu $t0, $sp, 0x90
/* 0DE67C 7F0A9B4C 3C0C8005 */  lui   $t4, %hi(aL)
/* 0DE680 7F0A9B50 A1410004 */  sb    $at, 4($t2)
/* 0DE684 7F0A9B54 8F210000 */  lw    $at, ($t9)
/* 0DE688 7F0A9B58 258C7790 */  addiu $t4, %lo(aL) # addiu $t4, $t4, 0x7790
/* 0DE68C 7F0A9B5C 27AD0088 */  addiu $t5, $sp, 0x88
/* 0DE690 7F0A9B60 AD010000 */  sw    $at, ($t0)
/* 0DE694 7F0A9B64 93210004 */  lbu   $at, 4($t9)
/* 0DE698 7F0A9B68 3C0F8005 */  lui   $t7, %hi(aR)
/* 0DE69C 7F0A9B6C 25EF7798 */  addiu $t7, %lo(aR) # addiu $t7, $t7, 0x7798
/* 0DE6A0 7F0A9B70 A1010004 */  sb    $at, 4($t0)
/* 0DE6A4 7F0A9B74 8D810000 */  lw    $at, ($t4)
/* 0DE6A8 7F0A9B78 27B80080 */  addiu $t8, $sp, 0x80
/* 0DE6AC 7F0A9B7C 3C0A8005 */  lui   $t2, %hi(aC)
/* 0DE6B0 7F0A9B80 ADA10000 */  sw    $at, ($t5)
/* 0DE6B4 7F0A9B84 91810004 */  lbu   $at, 4($t4)
/* 0DE6B8 7F0A9B88 254A77A0 */  addiu $t2, %lo(aC) # addiu $t2, $t2, 0x77a0
/* 0DE6BC 7F0A9B8C 27AB0078 */  addiu $t3, $sp, 0x78
/* 0DE6C0 7F0A9B90 A1A10004 */  sb    $at, 4($t5)
/* 0DE6C4 7F0A9B94 8DE10000 */  lw    $at, ($t7)
/* 0DE6C8 7F0A9B98 3C088005 */  lui   $t0, %hi(D_800577A8)
/* 0DE6CC 7F0A9B9C 250877A8 */  addiu $t0, %lo(D_800577A8) # addiu $t0, $t0, 0x77a8
/* 0DE6D0 7F0A9BA0 AF010000 */  sw    $at, ($t8)
/* 0DE6D4 7F0A9BA4 91E10004 */  lbu   $at, 4($t7)
/* 0DE6D8 7F0A9BA8 27B90070 */  addiu $t9, $sp, 0x70
/* 0DE6DC 7F0A9BAC 3C0D8005 */  lui   $t5, %hi(aS_0)
/* 0DE6E0 7F0A9BB0 A3010004 */  sb    $at, 4($t8)
/* 0DE6E4 7F0A9BB4 8D410000 */  lw    $at, ($t2)
/* 0DE6E8 7F0A9BB8 25AD77B0 */  addiu $t5, %lo(aS_0) # addiu $t5, $t5, 0x77b0
/* 0DE6EC 7F0A9BBC 27AC0068 */  addiu $t4, $sp, 0x68
/* 0DE6F0 7F0A9BC0 AD610000 */  sw    $at, ($t3)
/* 0DE6F4 7F0A9BC4 91410004 */  lbu   $at, 4($t2)
/* 0DE6F8 7F0A9BC8 3C188005 */  lui   $t8, %hi(a3d)
/* 0DE6FC 7F0A9BCC 271877B8 */  addiu $t8, %lo(a3d) # addiu $t8, $t8, 0x77b8
/* 0DE700 7F0A9BD0 A1610004 */  sb    $at, 4($t3)
/* 0DE704 7F0A9BD4 8D010000 */  lw    $at, ($t0)
/* 0DE708 7F0A9BD8 27AF0060 */  addiu $t7, $sp, 0x60
/* 0DE70C 7F0A9BDC 02002025 */  move  $a0, $s0
/* 0DE710 7F0A9BE0 AF210000 */  sw    $at, ($t9)
/* 0DE714 7F0A9BE4 91010004 */  lbu   $at, 4($t0)
/* 0DE718 7F0A9BE8 A3210004 */  sb    $at, 4($t9)
/* 0DE71C 7F0A9BEC 8DA10000 */  lw    $at, ($t5)
/* 0DE720 7F0A9BF0 AD810000 */  sw    $at, ($t4)
/* 0DE724 7F0A9BF4 91A10004 */  lbu   $at, 4($t5)
/* 0DE728 7F0A9BF8 A1810004 */  sb    $at, 4($t4)
/* 0DE72C 7F0A9BFC 8F010000 */  lw    $at, ($t8)
/* 0DE730 7F0A9C00 ADE10000 */  sw    $at, ($t7)
/* 0DE734 7F0A9C04 97010004 */  lhu   $at, 4($t8)
/* 0DE738 7F0A9C08 A5E10004 */  sh    $at, 4($t7)
/* 0DE73C 7F0A9C0C 0FC2B366 */  jal   microcode_constructor
/* 0DE740 7F0A9C10 AFA00050 */   sw    $zero, 0x50($sp)
/* 0DE744 7F0A9C14 00408025 */  move  $s0, $v0
/* 0DE748 7F0A9C18 00002025 */  move  $a0, $zero
/* 0DE74C 7F0A9C1C 0C0030C3 */  jal   joyGetButtons
/* 0DE750 7F0A9C20 24050020 */   li    $a1, 32
/* 0DE754 7F0A9C24 1040002C */  beqz  $v0, .L7F0A9CD8
/* 0DE758 7F0A9C28 3C0A8008 */   lui   $t2, %hi(g_CurrentPlayer)
/* 0DE75C 7F0A9C2C 8D4AA0B0 */  lw    $t2, %lo(g_CurrentPlayer)($t2)
/* 0DE760 7F0A9C30 3C048004 */  lui   $a0, %hi(D_800409DC+8)
/* 0DE764 7F0A9C34 8D4B2A58 */  lw    $t3, 0x2a58($t2)
/* 0DE768 7F0A9C38 000B6080 */  sll   $t4, $t3, 2
/* 0DE76C 7F0A9C3C 018B6021 */  addu  $t4, $t4, $t3
/* 0DE770 7F0A9C40 000C6080 */  sll   $t4, $t4, 2
/* 0DE774 7F0A9C44 008C2021 */  addu  $a0, $a0, $t4
/* 0DE778 7F0A9C48 0FC30776 */  jal   langGet
/* 0DE77C 7F0A9C4C 948409E4 */   lhu   $a0, %lo(D_800409DC+8)($a0)
/* 0DE780 7F0A9C50 3C080070 */  lui   $t0, (0x007000A0 >> 16) # lui $t0, 0x70
/* 0DE784 7F0A9C54 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DE788 7F0A9C58 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DE78C 7F0A9C5C 350800A0 */  ori   $t0, (0x007000A0 & 0xFFFF) # ori $t0, $t0, 0xa0
/* 0DE790 7F0A9C60 240DFFFF */  li    $t5, -1
/* 0DE794 7F0A9C64 240E0001 */  li    $t6, 1
/* 0DE798 7F0A9C68 AFAE0014 */  sw    $t6, 0x14($sp)
/* 0DE79C 7F0A9C6C AFAD0010 */  sw    $t5, 0x10($sp)
/* 0DE7A0 7F0A9C70 AFA80018 */  sw    $t0, 0x18($sp)
/* 0DE7A4 7F0A9C74 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DE7A8 7F0A9C78 02002025 */  move  $a0, $s0
/* 0DE7AC 7F0A9C7C 24050032 */  li    $a1, 50
/* 0DE7B0 7F0A9C80 24060052 */  li    $a2, 82
/* 0DE7B4 7F0A9C84 00403825 */  move  $a3, $v0
/* 0DE7B8 7F0A9C88 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DE7BC 7F0A9C8C AFA00020 */  sw    $zero, 0x20($sp)
/* 0DE7C0 7F0A9C90 0FC2A4E6 */  jal   draw_options_labels
/* 0DE7C4 7F0A9C94 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DE7C8 7F0A9C98 3C0F8008 */  lui   $t7, %hi(g_CurrentPlayer)
/* 0DE7CC 7F0A9C9C 8DEFA0B0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0DE7D0 7F0A9CA0 3C0A8004 */  lui   $t2, %hi(D_800409DC+9)
/* 0DE7D4 7F0A9CA4 3401AC01 */  li    $at, 44033
/* 0DE7D8 7F0A9CA8 8DF82A58 */  lw    $t8, 0x2a58($t7)
/* 0DE7DC 7F0A9CAC 00408025 */  move  $s0, $v0
/* 0DE7E0 7F0A9CB0 240B0001 */  li    $t3, 1
/* 0DE7E4 7F0A9CB4 00184880 */  sll   $t1, $t8, 2
/* 0DE7E8 7F0A9CB8 01384821 */  addu  $t1, $t1, $t8
/* 0DE7EC 7F0A9CBC 00094880 */  sll   $t1, $t1, 2
/* 0DE7F0 7F0A9CC0 01495021 */  addu  $t2, $t2, $t1
/* 0DE7F4 7F0A9CC4 954A09E4 */  lhu   $t2, %lo(D_800409DC+8)($t2)
/* 0DE7F8 7F0A9CC8 1541001F */  bne   $t2, $at, .L7F0A9D48
/* 0DE7FC 7F0A9CCC 00000000 */   nop
/* 0DE800 7F0A9CD0 1000001D */  b     .L7F0A9D48
/* 0DE804 7F0A9CD4 AFAB0050 */   sw    $t3, 0x50($sp)
.L7F0A9CD8:
/* 0DE808 7F0A9CD8 3C0C8008 */  lui   $t4, %hi(g_CurrentPlayer)
/* 0DE80C 7F0A9CDC 8D8CA0B0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DE810 7F0A9CE0 3C048004 */  lui   $a0, %hi(D_800409DC+8)
/* 0DE814 7F0A9CE4 8D8D2A58 */  lw    $t5, 0x2a58($t4)
/* 0DE818 7F0A9CE8 000D7080 */  sll   $t6, $t5, 2
/* 0DE81C 7F0A9CEC 01CD7021 */  addu  $t6, $t6, $t5
/* 0DE820 7F0A9CF0 000E7080 */  sll   $t6, $t6, 2
/* 0DE824 7F0A9CF4 008E2021 */  addu  $a0, $a0, $t6
/* 0DE828 7F0A9CF8 0FC30776 */  jal   langGet
/* 0DE82C 7F0A9CFC 948409E4 */   lhu   $a0, %lo(D_800409DC+8)($a0)
/* 0DE830 7F0A9D00 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DE834 7F0A9D04 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DE838 7F0A9D08 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DE83C 7F0A9D0C 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DE840 7F0A9D10 2419FFFF */  li    $t9, -1
/* 0DE844 7F0A9D14 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DE848 7F0A9D18 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DE84C 7F0A9D1C AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DE850 7F0A9D20 02002025 */  move  $a0, $s0
/* 0DE854 7F0A9D24 24050032 */  li    $a1, 50
/* 0DE858 7F0A9D28 24060052 */  li    $a2, 82
/* 0DE85C 7F0A9D2C 00403825 */  move  $a3, $v0
/* 0DE860 7F0A9D30 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DE864 7F0A9D34 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DE868 7F0A9D38 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DE86C 7F0A9D3C 0FC2A4E6 */  jal   draw_options_labels
/* 0DE870 7F0A9D40 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DE874 7F0A9D44 00408025 */  move  $s0, $v0
.L7F0A9D48:
/* 0DE878 7F0A9D48 3C098004 */  lui   $t1, %hi(controller_options_index)
/* 0DE87C 7F0A9D4C 8D290998 */  lw    $t1, %lo(controller_options_index)($t1)
/* 0DE880 7F0A9D50 2418006B */  li    $t8, 107
/* 0DE884 7F0A9D54 24010001 */  li    $at, 1
/* 0DE888 7F0A9D58 1521000A */  bne   $t1, $at, .L7F0A9D84
/* 0DE88C 7F0A9D5C AFB8004C */   sw    $t8, 0x4c($sp)
/* 0DE890 7F0A9D60 3C0A8004 */  lui   $t2, %hi(watch_item_is_actively_selected)
/* 0DE894 7F0A9D64 8D4A09A8 */  lw    $t2, %lo(watch_item_is_actively_selected)($t2)
/* 0DE898 7F0A9D68 00002025 */  move  $a0, $zero
/* 0DE89C 7F0A9D6C 11400005 */  beqz  $t2, .L7F0A9D84
/* 0DE8A0 7F0A9D70 00000000 */   nop
/* 0DE8A4 7F0A9D74 0C0030C3 */  jal   joyGetButtons
/* 0DE8A8 7F0A9D78 24050F00 */   li    $a1, 3840
/* 0DE8AC 7F0A9D7C 1440001E */  bnez  $v0, .L7F0A9DF8
/* 0DE8B0 7F0A9D80 00002025 */   move  $a0, $zero
.L7F0A9D84:
/* 0DE8B4 7F0A9D84 3C0B8008 */  lui   $t3, %hi(g_CurrentPlayer)
/* 0DE8B8 7F0A9D88 8D6BA0B0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DE8BC 7F0A9D8C 3C048004 */  lui   $a0, %hi(D_800409DC+14)
/* 0DE8C0 7F0A9D90 8D6C2A58 */  lw    $t4, 0x2a58($t3)
/* 0DE8C4 7F0A9D94 000C6880 */  sll   $t5, $t4, 2
/* 0DE8C8 7F0A9D98 01AC6821 */  addu  $t5, $t5, $t4
/* 0DE8CC 7F0A9D9C 000D6880 */  sll   $t5, $t5, 2
/* 0DE8D0 7F0A9DA0 008D2021 */  addu  $a0, $a0, $t5
/* 0DE8D4 7F0A9DA4 0FC30776 */  jal   langGet
/* 0DE8D8 7F0A9DA8 948409EA */   lhu   $a0, %lo(D_800409DC+14)($a0)
/* 0DE8DC 7F0A9DAC 3C0E00AA */  lui   $t6, (0x00AA00B0 >> 16) # lui $t6, 0xaa
/* 0DE8E0 7F0A9DB0 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DE8E4 7F0A9DB4 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DE8E8 7F0A9DB8 35CE00B0 */  ori   $t6, (0x00AA00B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DE8EC 7F0A9DBC 2408FFFF */  li    $t0, -1
/* 0DE8F0 7F0A9DC0 AFA80018 */  sw    $t0, 0x18($sp)
/* 0DE8F4 7F0A9DC4 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DE8F8 7F0A9DC8 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DE8FC 7F0A9DCC 02002025 */  move  $a0, $s0
/* 0DE900 7F0A9DD0 24050032 */  li    $a1, 50
/* 0DE904 7F0A9DD4 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DE908 7F0A9DD8 00403825 */  move  $a3, $v0
/* 0DE90C 7F0A9DDC AFA00014 */  sw    $zero, 0x14($sp)
/* 0DE910 7F0A9DE0 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DE914 7F0A9DE4 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DE918 7F0A9DE8 0FC2A4E6 */  jal   draw_options_labels
/* 0DE91C 7F0A9DEC AFA00028 */   sw    $zero, 0x28($sp)
/* 0DE920 7F0A9DF0 10000051 */  b     .L7F0A9F38
/* 0DE924 7F0A9DF4 00408025 */   move  $s0, $v0
.L7F0A9DF8:
/* 0DE928 7F0A9DF8 0C0030C3 */  jal   joyGetButtons
/* 0DE92C 7F0A9DFC 24050800 */   li    $a1, 2048
/* 0DE930 7F0A9E00 10400014 */  beqz  $v0, .L7F0A9E54
/* 0DE934 7F0A9E04 00002025 */   move  $a0, $zero
/* 0DE938 7F0A9E08 3C0F8008 */  lui   $t7, %hi(g_CurrentPlayer)
/* 0DE93C 7F0A9E0C 8DEFA0B0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0DE940 7F0A9E10 3C0A8004 */  lui   $t2, %hi(D_800409DC+14)
/* 0DE944 7F0A9E14 3401AC05 */  li    $at, 44037
/* 0DE948 7F0A9E18 8DF82A58 */  lw    $t8, 0x2a58($t7)
/* 0DE94C 7F0A9E1C 8FA700AC */  lw    $a3, 0xac($sp)
/* 0DE950 7F0A9E20 00184880 */  sll   $t1, $t8, 2
/* 0DE954 7F0A9E24 01384821 */  addu  $t1, $t1, $t8
/* 0DE958 7F0A9E28 00094880 */  sll   $t1, $t1, 2
/* 0DE95C 7F0A9E2C 01495021 */  addu  $t2, $t2, $t1
/* 0DE960 7F0A9E30 954A09EA */  lhu   $t2, %lo(D_800409DC+14)($t2)
/* 0DE964 7F0A9E34 15410005 */  bne   $t2, $at, .L7F0A9E4C
/* 0DE968 7F0A9E38 00000000 */   nop
/* 0DE96C 7F0A9E3C 0FC30776 */  jal   langGet
/* 0DE970 7F0A9E40 3404AC30 */   li    $a0, 44080
/* 0DE974 7F0A9E44 10000001 */  b     .L7F0A9E4C
/* 0DE978 7F0A9E48 00403825 */   move  $a3, $v0
.L7F0A9E4C:
/* 0DE97C 7F0A9E4C 10000027 */  b     .L7F0A9EEC
/* 0DE980 7F0A9E50 AFA70058 */   sw    $a3, 0x58($sp)
.L7F0A9E54:
/* 0DE984 7F0A9E54 0C0030C3 */  jal   joyGetButtons
/* 0DE988 7F0A9E58 24050400 */   li    $a1, 1024
/* 0DE98C 7F0A9E5C 10400014 */  beqz  $v0, .L7F0A9EB0
/* 0DE990 7F0A9E60 00002025 */   move  $a0, $zero
/* 0DE994 7F0A9E64 3C0B8008 */  lui   $t3, %hi(g_CurrentPlayer)
/* 0DE998 7F0A9E68 8D6BA0B0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DE99C 7F0A9E6C 3C0E8004 */  lui   $t6, %hi(D_800409DC+14)
/* 0DE9A0 7F0A9E70 3401AC05 */  li    $at, 44037
/* 0DE9A4 7F0A9E74 8D6C2A58 */  lw    $t4, 0x2a58($t3)
/* 0DE9A8 7F0A9E78 8FA700A8 */  lw    $a3, 0xa8($sp)
/* 0DE9AC 7F0A9E7C 000C6880 */  sll   $t5, $t4, 2
/* 0DE9B0 7F0A9E80 01AC6821 */  addu  $t5, $t5, $t4
/* 0DE9B4 7F0A9E84 000D6880 */  sll   $t5, $t5, 2
/* 0DE9B8 7F0A9E88 01CD7021 */  addu  $t6, $t6, $t5
/* 0DE9BC 7F0A9E8C 95CE09EA */  lhu   $t6, %lo(D_800409DC+14)($t6)
/* 0DE9C0 7F0A9E90 15C10005 */  bne   $t6, $at, .L7F0A9EA8
/* 0DE9C4 7F0A9E94 00000000 */   nop
/* 0DE9C8 7F0A9E98 0FC30776 */  jal   langGet
/* 0DE9CC 7F0A9E9C 3404AC31 */   li    $a0, 44081
/* 0DE9D0 7F0A9EA0 10000001 */  b     .L7F0A9EA8
/* 0DE9D4 7F0A9EA4 00403825 */   move  $a3, $v0
.L7F0A9EA8:
/* 0DE9D8 7F0A9EA8 10000010 */  b     .L7F0A9EEC
/* 0DE9DC 7F0A9EAC AFA70058 */   sw    $a3, 0x58($sp)
.L7F0A9EB0:
/* 0DE9E0 7F0A9EB0 0C0030C3 */  jal   joyGetButtons
/* 0DE9E4 7F0A9EB4 24050200 */   li    $a1, 512
/* 0DE9E8 7F0A9EB8 10400005 */  beqz  $v0, .L7F0A9ED0
/* 0DE9EC 7F0A9EBC 00002025 */   move  $a0, $zero
/* 0DE9F0 7F0A9EC0 0FC30776 */  jal   langGet
/* 0DE9F4 7F0A9EC4 3404AC2F */   li    $a0, 44079
/* 0DE9F8 7F0A9EC8 10000008 */  b     .L7F0A9EEC
/* 0DE9FC 7F0A9ECC AFA20058 */   sw    $v0, 0x58($sp)
.L7F0A9ED0:
/* 0DEA00 7F0A9ED0 0C0030C3 */  jal   joyGetButtons
/* 0DEA04 7F0A9ED4 24050100 */   li    $a1, 256
/* 0DEA08 7F0A9ED8 50400005 */  beql  $v0, $zero, .L7F0A9EF0
/* 0DEA0C 7F0A9EDC 3C0F0070 */   lui   $t7, 0x70
/* 0DEA10 7F0A9EE0 0FC30776 */  jal   langGet
/* 0DEA14 7F0A9EE4 3404AC2E */   li    $a0, 44078
/* 0DEA18 7F0A9EE8 AFA20058 */  sw    $v0, 0x58($sp)
.L7F0A9EEC:
/* 0DEA1C 7F0A9EEC 3C0F0070 */  lui   $t7, (0x007000A0 >> 16) # lui $t7, 0x70
.L7F0A9EF0:
/* 0DEA20 7F0A9EF0 3C180030 */  lui   $t8, (0x003000B0 >> 16) # lui $t8, 0x30
/* 0DEA24 7F0A9EF4 371800B0 */  ori   $t8, (0x003000B0 & 0xFFFF) # ori $t8, $t8, 0xb0
/* 0DEA28 7F0A9EF8 35EF00A0 */  ori   $t7, (0x007000A0 & 0xFFFF) # ori $t7, $t7, 0xa0
/* 0DEA2C 7F0A9EFC 2408FFFF */  li    $t0, -1
/* 0DEA30 7F0A9F00 24190001 */  li    $t9, 1
/* 0DEA34 7F0A9F04 8FA70058 */  lw    $a3, 0x58($sp)
/* 0DEA38 7F0A9F08 AFB90014 */  sw    $t9, 0x14($sp)
/* 0DEA3C 7F0A9F0C AFA80010 */  sw    $t0, 0x10($sp)
/* 0DEA40 7F0A9F10 AFAF0018 */  sw    $t7, 0x18($sp)
/* 0DEA44 7F0A9F14 AFB80024 */  sw    $t8, 0x24($sp)
/* 0DEA48 7F0A9F18 02002025 */  move  $a0, $s0
/* 0DEA4C 7F0A9F1C 24050032 */  li    $a1, 50
/* 0DEA50 7F0A9F20 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DEA54 7F0A9F24 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DEA58 7F0A9F28 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DEA5C 7F0A9F2C 0FC2A4E6 */  jal   draw_options_labels
/* 0DEA60 7F0A9F30 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DEA64 7F0A9F34 00408025 */  move  $s0, $v0
.L7F0A9F38:
/* 0DEA68 7F0A9F38 8FA9004C */  lw    $t1, 0x4c($sp)
/* 0DEA6C 7F0A9F3C 3C0B8008 */  lui   $t3, %hi(g_CurrentPlayer)
/* 0DEA70 7F0A9F40 8D6BA0B0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DEA74 7F0A9F44 252A0019 */  addiu $t2, $t1, 0x19
/* 0DEA78 7F0A9F48 AFAA004C */  sw    $t2, 0x4c($sp)
/* 0DEA7C 7F0A9F4C 8D6C2A58 */  lw    $t4, 0x2a58($t3)
/* 0DEA80 7F0A9F50 3C048004 */  lui   $a0, %hi(D_800409DC+16)
/* 0DEA84 7F0A9F54 000C6880 */  sll   $t5, $t4, 2
/* 0DEA88 7F0A9F58 01AC6821 */  addu  $t5, $t5, $t4
/* 0DEA8C 7F0A9F5C 000D6880 */  sll   $t5, $t5, 2
/* 0DEA90 7F0A9F60 008D2021 */  addu  $a0, $a0, $t5
/* 0DEA94 7F0A9F64 0FC30776 */  jal   langGet
/* 0DEA98 7F0A9F68 948409EC */   lhu   $a0, %lo(D_800409DC+16)($a0)
/* 0DEA9C 7F0A9F6C 3C0E00AA */  lui   $t6, (0x00AA00B0 >> 16) # lui $t6, 0xaa
/* 0DEAA0 7F0A9F70 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DEAA4 7F0A9F74 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DEAA8 7F0A9F78 35CE00B0 */  ori   $t6, (0x00AA00B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DEAAC 7F0A9F7C 2408FFFF */  li    $t0, -1
/* 0DEAB0 7F0A9F80 AFA80018 */  sw    $t0, 0x18($sp)
/* 0DEAB4 7F0A9F84 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DEAB8 7F0A9F88 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DEABC 7F0A9F8C 02002025 */  move  $a0, $s0
/* 0DEAC0 7F0A9F90 24050032 */  li    $a1, 50
/* 0DEAC4 7F0A9F94 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DEAC8 7F0A9F98 00403825 */  move  $a3, $v0
/* 0DEACC 7F0A9F9C AFA00014 */  sw    $zero, 0x14($sp)
/* 0DEAD0 7F0A9FA0 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DEAD4 7F0A9FA4 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DEAD8 7F0A9FA8 0FC2A4E6 */  jal   draw_options_labels
/* 0DEADC 7F0A9FAC AFA00028 */   sw    $zero, 0x28($sp)
/* 0DEAE0 7F0A9FB0 8FAF004C */  lw    $t7, 0x4c($sp)
/* 0DEAE4 7F0A9FB4 00408025 */  move  $s0, $v0
/* 0DEAE8 7F0A9FB8 00002025 */  move  $a0, $zero
/* 0DEAEC 7F0A9FBC 25F80019 */  addiu $t8, $t7, 0x19
/* 0DEAF0 7F0A9FC0 AFB8004C */  sw    $t8, 0x4c($sp)
/* 0DEAF4 7F0A9FC4 0C0030C3 */  jal   joyGetButtons
/* 0DEAF8 7F0A9FC8 24052000 */   li    $a1, 8192
/* 0DEAFC 7F0A9FCC 1040002D */  beqz  $v0, .L7F0AA084
/* 0DEB00 7F0A9FD0 3C0B8008 */   lui   $t3, %hi(g_CurrentPlayer)
/* 0DEB04 7F0A9FD4 3C098008 */  lui   $t1, %hi(g_CurrentPlayer)
/* 0DEB08 7F0A9FD8 8D29A0B0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DEB0C 7F0A9FDC 3C048004 */  lui   $a0, %hi(D_800409DC+0x6)
/* 0DEB10 7F0A9FE0 8D2A2A58 */  lw    $t2, 0x2a58($t1)
/* 0DEB14 7F0A9FE4 000A5880 */  sll   $t3, $t2, 2
/* 0DEB18 7F0A9FE8 016A5821 */  addu  $t3, $t3, $t2
/* 0DEB1C 7F0A9FEC 000B5880 */  sll   $t3, $t3, 2
/* 0DEB20 7F0A9FF0 008B2021 */  addu  $a0, $a0, $t3
/* 0DEB24 7F0A9FF4 0FC30776 */  jal   langGet
/* 0DEB28 7F0A9FF8 948409E2 */   lhu   $a0, %lo(D_800409DC+0x6)($a0)
/* 0DEB2C 7F0A9FFC 3C0E0070 */  lui   $t6, (0x007000A0 >> 16) # lui $t6, 0x70
/* 0DEB30 7F0AA000 3C080030 */  lui   $t0, (0x003000B0 >> 16) # lui $t0, 0x30
/* 0DEB34 7F0AA004 350800B0 */  ori   $t0, (0x003000B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DEB38 7F0AA008 35CE00A0 */  ori   $t6, (0x007000A0 & 0xFFFF) # ori $t6, $t6, 0xa0
/* 0DEB3C 7F0AA00C 240CFFFF */  li    $t4, -1
/* 0DEB40 7F0AA010 240D0001 */  li    $t5, 1
/* 0DEB44 7F0AA014 AFAD0014 */  sw    $t5, 0x14($sp)
/* 0DEB48 7F0AA018 AFAC0010 */  sw    $t4, 0x10($sp)
/* 0DEB4C 7F0AA01C AFAE0018 */  sw    $t6, 0x18($sp)
/* 0DEB50 7F0AA020 AFA80024 */  sw    $t0, 0x24($sp)
/* 0DEB54 7F0AA024 02002025 */  move  $a0, $s0
/* 0DEB58 7F0AA028 24050032 */  li    $a1, 50
/* 0DEB5C 7F0AA02C 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DEB60 7F0AA030 00403825 */  move  $a3, $v0
/* 0DEB64 7F0AA034 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DEB68 7F0AA038 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DEB6C 7F0AA03C 0FC2A4E6 */  jal   draw_options_labels
/* 0DEB70 7F0AA040 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DEB74 7F0AA044 3C198008 */  lui   $t9, %hi(g_CurrentPlayer)
/* 0DEB78 7F0AA048 8F39A0B0 */  lw    $t9, %lo(g_CurrentPlayer)($t9)
/* 0DEB7C 7F0AA04C 3C098004 */  lui   $t1, %hi(D_800409DC+6)
/* 0DEB80 7F0AA050 3401AC01 */  li    $at, 44033
/* 0DEB84 7F0AA054 8F2F2A58 */  lw    $t7, 0x2a58($t9)
/* 0DEB88 7F0AA058 00408025 */  move  $s0, $v0
/* 0DEB8C 7F0AA05C 240A0001 */  li    $t2, 1
/* 0DEB90 7F0AA060 000FC080 */  sll   $t8, $t7, 2
/* 0DEB94 7F0AA064 030FC021 */  addu  $t8, $t8, $t7
/* 0DEB98 7F0AA068 0018C080 */  sll   $t8, $t8, 2
/* 0DEB9C 7F0AA06C 01384821 */  addu  $t1, $t1, $t8
/* 0DEBA0 7F0AA070 952909E2 */  lhu   $t1, %lo(D_800409DC+6)($t1)
/* 0DEBA4 7F0AA074 5521001F */  bnel  $t1, $at, .L7F0AA0F4
/* 0DEBA8 7F0AA078 8FAF004C */   lw    $t7, 0x4c($sp)
/* 0DEBAC 7F0AA07C 1000001C */  b     .L7F0AA0F0
/* 0DEBB0 7F0AA080 AFAA0050 */   sw    $t2, 0x50($sp)
.L7F0AA084:
/* 0DEBB4 7F0AA084 8D6BA0B0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DEBB8 7F0AA088 3C048004 */  lui   $a0, %hi(D_800409DC+6)
/* 0DEBBC 7F0AA08C 8D6C2A58 */  lw    $t4, 0x2a58($t3)
/* 0DEBC0 7F0AA090 000C6880 */  sll   $t5, $t4, 2
/* 0DEBC4 7F0AA094 01AC6821 */  addu  $t5, $t5, $t4
/* 0DEBC8 7F0AA098 000D6880 */  sll   $t5, $t5, 2
/* 0DEBCC 7F0AA09C 008D2021 */  addu  $a0, $a0, $t5
/* 0DEBD0 7F0AA0A0 0FC30776 */  jal   langGet
/* 0DEBD4 7F0AA0A4 948409E2 */   lhu   $a0, %lo(D_800409DC+6)($a0)
/* 0DEBD8 7F0AA0A8 3C0E00AA */  lui   $t6, (0x00AA00B0 >> 16) # lui $t6, 0xaa
/* 0DEBDC 7F0AA0AC 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DEBE0 7F0AA0B0 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DEBE4 7F0AA0B4 35CE00B0 */  ori   $t6, (0x00AA00B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DEBE8 7F0AA0B8 2408FFFF */  li    $t0, -1
/* 0DEBEC 7F0AA0BC AFA80018 */  sw    $t0, 0x18($sp)
/* 0DEBF0 7F0AA0C0 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DEBF4 7F0AA0C4 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DEBF8 7F0AA0C8 02002025 */  move  $a0, $s0
/* 0DEBFC 7F0AA0CC 24050032 */  li    $a1, 50
/* 0DEC00 7F0AA0D0 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DEC04 7F0AA0D4 00403825 */  move  $a3, $v0
/* 0DEC08 7F0AA0D8 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DEC0C 7F0AA0DC AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DEC10 7F0AA0E0 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DEC14 7F0AA0E4 0FC2A4E6 */  jal   draw_options_labels
/* 0DEC18 7F0AA0E8 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DEC1C 7F0AA0EC 00408025 */  move  $s0, $v0
.L7F0AA0F0:
/* 0DEC20 7F0AA0F0 8FAF004C */  lw    $t7, 0x4c($sp)
.L7F0AA0F4:
/* 0DEC24 7F0AA0F4 00002025 */  move  $a0, $zero
/* 0DEC28 7F0AA0F8 24050010 */  li    $a1, 16
/* 0DEC2C 7F0AA0FC 25F8FFB5 */  addiu $t8, $t7, -0x4b
/* 0DEC30 7F0AA100 0C0030C3 */  jal   joyGetButtons
/* 0DEC34 7F0AA104 AFB8004C */   sw    $t8, 0x4c($sp)
/* 0DEC38 7F0AA108 1040002E */  beqz  $v0, .L7F0AA1C4
/* 0DEC3C 7F0AA10C 3C0C8008 */   lui   $t4, %hi(g_CurrentPlayer)
/* 0DEC40 7F0AA110 3C098008 */  lui   $t1, %hi(g_CurrentPlayer)
/* 0DEC44 7F0AA114 8D29A0B0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DEC48 7F0AA118 3C048004 */  lui   $a0, %hi(D_800409DC+10)
/* 0DEC4C 7F0AA11C 8D2A2A58 */  lw    $t2, 0x2a58($t1)
/* 0DEC50 7F0AA120 000A5880 */  sll   $t3, $t2, 2
/* 0DEC54 7F0AA124 016A5821 */  addu  $t3, $t3, $t2
/* 0DEC58 7F0AA128 000B5880 */  sll   $t3, $t3, 2
/* 0DEC5C 7F0AA12C 008B2021 */  addu  $a0, $a0, $t3
/* 0DEC60 7F0AA130 0FC30776 */  jal   langGet
/* 0DEC64 7F0AA134 948409E6 */   lhu   $a0, %lo(D_800409DC+10)($a0)
/* 0DEC68 7F0AA138 3C0E0070 */  lui   $t6, (0x007000A0 >> 16) # lui $t6, 0x70
/* 0DEC6C 7F0AA13C 3C080030 */  lui   $t0, (0x003000B0 >> 16) # lui $t0, 0x30
/* 0DEC70 7F0AA140 350800B0 */  ori   $t0, (0x003000B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DEC74 7F0AA144 35CE00A0 */  ori   $t6, (0x007000A0 & 0xFFFF) # ori $t6, $t6, 0xa0
/* 0DEC78 7F0AA148 240CFFFF */  li    $t4, -1
/* 0DEC7C 7F0AA14C 240D0001 */  li    $t5, 1
/* 0DEC80 7F0AA150 24190001 */  li    $t9, 1
/* 0DEC84 7F0AA154 AFB90028 */  sw    $t9, 0x28($sp)
/* 0DEC88 7F0AA158 AFAD0014 */  sw    $t5, 0x14($sp)
/* 0DEC8C 7F0AA15C AFAC0010 */  sw    $t4, 0x10($sp)
/* 0DEC90 7F0AA160 AFAE0018 */  sw    $t6, 0x18($sp)
/* 0DEC94 7F0AA164 AFA80024 */  sw    $t0, 0x24($sp)
/* 0DEC98 7F0AA168 02002025 */  move  $a0, $s0
/* 0DEC9C 7F0AA16C 2405010E */  li    $a1, 270
/* 0DECA0 7F0AA170 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DECA4 7F0AA174 00403825 */  move  $a3, $v0
/* 0DECA8 7F0AA178 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DECAC 7F0AA17C 0FC2A4E6 */  jal   draw_options_labels
/* 0DECB0 7F0AA180 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DECB4 7F0AA184 3C0F8008 */  lui   $t7, %hi(g_CurrentPlayer)
/* 0DECB8 7F0AA188 8DEFA0B0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0DECBC 7F0AA18C 3C0A8004 */  lui   $t2, %hi(D_800409DC+10)
/* 0DECC0 7F0AA190 3401AC01 */  li    $at, 44033
/* 0DECC4 7F0AA194 8DF82A58 */  lw    $t8, 0x2a58($t7)
/* 0DECC8 7F0AA198 00408025 */  move  $s0, $v0
/* 0DECCC 7F0AA19C 240B0001 */  li    $t3, 1
/* 0DECD0 7F0AA1A0 00184880 */  sll   $t1, $t8, 2
/* 0DECD4 7F0AA1A4 01384821 */  addu  $t1, $t1, $t8
/* 0DECD8 7F0AA1A8 00094880 */  sll   $t1, $t1, 2
/* 0DECDC 7F0AA1AC 01495021 */  addu  $t2, $t2, $t1
/* 0DECE0 7F0AA1B0 954A09E6 */  lhu   $t2, %lo(D_800409DC+10)($t2)
/* 0DECE4 7F0AA1B4 55410020 */  bnel  $t2, $at, .L7F0AA238
/* 0DECE8 7F0AA1B8 8FA9004C */   lw    $t1, 0x4c($sp)
/* 0DECEC 7F0AA1BC 1000001D */  b     .L7F0AA234
/* 0DECF0 7F0AA1C0 AFAB0050 */   sw    $t3, 0x50($sp)
.L7F0AA1C4:
/* 0DECF4 7F0AA1C4 8D8CA0B0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DECF8 7F0AA1C8 3C048004 */  lui   $a0, %hi(D_800409DC+10)
/* 0DECFC 7F0AA1CC 8D8D2A58 */  lw    $t5, 0x2a58($t4)
/* 0DED00 7F0AA1D0 000D7080 */  sll   $t6, $t5, 2
/* 0DED04 7F0AA1D4 01CD7021 */  addu  $t6, $t6, $t5
/* 0DED08 7F0AA1D8 000E7080 */  sll   $t6, $t6, 2
/* 0DED0C 7F0AA1DC 008E2021 */  addu  $a0, $a0, $t6
/* 0DED10 7F0AA1E0 0FC30776 */  jal   langGet
/* 0DED14 7F0AA1E4 948409E6 */   lhu   $a0, %lo(D_800409DC+10)($a0)
/* 0DED18 7F0AA1E8 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DED1C 7F0AA1EC 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DED20 7F0AA1F0 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DED24 7F0AA1F4 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DED28 7F0AA1F8 2419FFFF */  li    $t9, -1
/* 0DED2C 7F0AA1FC 24180001 */  li    $t8, 1
/* 0DED30 7F0AA200 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DED34 7F0AA204 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DED38 7F0AA208 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DED3C 7F0AA20C AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DED40 7F0AA210 02002025 */  move  $a0, $s0
/* 0DED44 7F0AA214 2405010E */  li    $a1, 270
/* 0DED48 7F0AA218 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DED4C 7F0AA21C 00403825 */  move  $a3, $v0
/* 0DED50 7F0AA220 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DED54 7F0AA224 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DED58 7F0AA228 0FC2A4E6 */  jal   draw_options_labels
/* 0DED5C 7F0AA22C AFA00020 */   sw    $zero, 0x20($sp)
/* 0DED60 7F0AA230 00408025 */  move  $s0, $v0
.L7F0AA234:
/* 0DED64 7F0AA234 8FA9004C */  lw    $t1, 0x4c($sp)
.L7F0AA238:
/* 0DED68 7F0AA238 3C0B8004 */  lui   $t3, %hi(controller_options_index)
/* 0DED6C 7F0AA23C 8D6B0998 */  lw    $t3, %lo(controller_options_index)($t3)
/* 0DED70 7F0AA240 24010001 */  li    $at, 1
/* 0DED74 7F0AA244 252A0019 */  addiu $t2, $t1, 0x19
/* 0DED78 7F0AA248 1561000A */  bne   $t3, $at, .L7F0AA274
/* 0DED7C 7F0AA24C AFAA004C */   sw    $t2, 0x4c($sp)
/* 0DED80 7F0AA250 3C0C8004 */  lui   $t4, %hi(watch_item_is_actively_selected)
/* 0DED84 7F0AA254 8D8C09A8 */  lw    $t4, %lo(watch_item_is_actively_selected)($t4)
/* 0DED88 7F0AA258 00002025 */  move  $a0, $zero
/* 0DED8C 7F0AA25C 11800005 */  beqz  $t4, .L7F0AA274
/* 0DED90 7F0AA260 00000000 */   nop
/* 0DED94 7F0AA264 0C0030C3 */  jal   joyGetButtons
/* 0DED98 7F0AA268 2405000F */   li    $a1, 15
/* 0DED9C 7F0AA26C 1440001F */  bnez  $v0, .L7F0AA2EC
/* 0DEDA0 7F0AA270 00002025 */   move  $a0, $zero
.L7F0AA274:
/* 0DEDA4 7F0AA274 3C0D8008 */  lui   $t5, %hi(g_CurrentPlayer)
/* 0DEDA8 7F0AA278 8DADA0B0 */  lw    $t5, %lo(g_CurrentPlayer)($t5)
/* 0DEDAC 7F0AA27C 3C048004 */  lui   $a0, %hi(D_800409DC+12)
/* 0DEDB0 7F0AA280 8DAE2A58 */  lw    $t6, 0x2a58($t5)
/* 0DEDB4 7F0AA284 000E4080 */  sll   $t0, $t6, 2
/* 0DEDB8 7F0AA288 010E4021 */  addu  $t0, $t0, $t6
/* 0DEDBC 7F0AA28C 00084080 */  sll   $t0, $t0, 2
/* 0DEDC0 7F0AA290 00882021 */  addu  $a0, $a0, $t0
/* 0DEDC4 7F0AA294 0FC30776 */  jal   langGet
/* 0DEDC8 7F0AA298 948409E8 */   lhu   $a0, %lo(D_800409DC+12)($a0)
/* 0DEDCC 7F0AA29C 3C1900AA */  lui   $t9, (0x00AA00B0 >> 16) # lui $t9, 0xaa
/* 0DEDD0 7F0AA2A0 3C180030 */  lui   $t8, (0x003000B0 >> 16) # lui $t8, 0x30
/* 0DEDD4 7F0AA2A4 371800B0 */  ori   $t8, (0x003000B0 & 0xFFFF) # ori $t8, $t8, 0xb0
/* 0DEDD8 7F0AA2A8 373900B0 */  ori   $t9, (0x00AA00B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DEDDC 7F0AA2AC 240FFFFF */  li    $t7, -1
/* 0DEDE0 7F0AA2B0 24090001 */  li    $t1, 1
/* 0DEDE4 7F0AA2B4 AFA90028 */  sw    $t1, 0x28($sp)
/* 0DEDE8 7F0AA2B8 AFAF0018 */  sw    $t7, 0x18($sp)
/* 0DEDEC 7F0AA2BC AFB90010 */  sw    $t9, 0x10($sp)
/* 0DEDF0 7F0AA2C0 AFB80024 */  sw    $t8, 0x24($sp)
/* 0DEDF4 7F0AA2C4 02002025 */  move  $a0, $s0
/* 0DEDF8 7F0AA2C8 2405010E */  li    $a1, 270
/* 0DEDFC 7F0AA2CC 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DEE00 7F0AA2D0 00403825 */  move  $a3, $v0
/* 0DEE04 7F0AA2D4 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DEE08 7F0AA2D8 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DEE0C 7F0AA2DC 0FC2A4E6 */  jal   draw_options_labels
/* 0DEE10 7F0AA2E0 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DEE14 7F0AA2E4 10000076 */  b     .L7F0AA4C0
/* 0DEE18 7F0AA2E8 00408025 */   move  $s0, $v0
.L7F0AA2EC:
/* 0DEE1C 7F0AA2EC 0C0030C3 */  jal   joyGetButtons
/* 0DEE20 7F0AA2F0 2405000F */   li    $a1, 15
/* 0DEE24 7F0AA2F4 244AFFFF */  addiu $t2, $v0, -1
/* 0DEE28 7F0AA2F8 004A5824 */  and   $t3, $v0, $t2
/* 0DEE2C 7F0AA2FC 15600054 */  bnez  $t3, .L7F0AA450
/* 0DEE30 7F0AA300 3C088008 */   lui   $t0, %hi(g_CurrentPlayer)
/* 0DEE34 7F0AA304 00002025 */  move  $a0, $zero
/* 0DEE38 7F0AA308 0C0030C3 */  jal   joyGetButtons
/* 0DEE3C 7F0AA30C 24050008 */   li    $a1, 8
/* 0DEE40 7F0AA310 10400014 */  beqz  $v0, .L7F0AA364
/* 0DEE44 7F0AA314 00002025 */   move  $a0, $zero
/* 0DEE48 7F0AA318 3C0C8008 */  lui   $t4, %hi(g_CurrentPlayer)
/* 0DEE4C 7F0AA31C 8D8CA0B0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DEE50 7F0AA320 3C088004 */  lui   $t0, %hi(D_800409DC+12)
/* 0DEE54 7F0AA324 3401AC05 */  li    $at, 44037
/* 0DEE58 7F0AA328 8D8D2A58 */  lw    $t5, 0x2a58($t4)
/* 0DEE5C 7F0AA32C 8FA700AC */  lw    $a3, 0xac($sp)
/* 0DEE60 7F0AA330 000D7080 */  sll   $t6, $t5, 2
/* 0DEE64 7F0AA334 01CD7021 */  addu  $t6, $t6, $t5
/* 0DEE68 7F0AA338 000E7080 */  sll   $t6, $t6, 2
/* 0DEE6C 7F0AA33C 010E4021 */  addu  $t0, $t0, $t6
/* 0DEE70 7F0AA340 950809E8 */  lhu   $t0, %lo(D_800409DC+12)($t0)
/* 0DEE74 7F0AA344 15010005 */  bne   $t0, $at, .L7F0AA35C
/* 0DEE78 7F0AA348 00000000 */   nop
/* 0DEE7C 7F0AA34C 0FC30776 */  jal   langGet
/* 0DEE80 7F0AA350 3404AC30 */   li    $a0, 44080
/* 0DEE84 7F0AA354 10000001 */  b     .L7F0AA35C
/* 0DEE88 7F0AA358 00403825 */   move  $a3, $v0
.L7F0AA35C:
/* 0DEE8C 7F0AA35C 10000027 */  b     .L7F0AA3FC
/* 0DEE90 7F0AA360 AFA7005C */   sw    $a3, 0x5c($sp)
.L7F0AA364:
/* 0DEE94 7F0AA364 0C0030C3 */  jal   joyGetButtons
/* 0DEE98 7F0AA368 24050004 */   li    $a1, 4
/* 0DEE9C 7F0AA36C 10400014 */  beqz  $v0, .L7F0AA3C0
/* 0DEEA0 7F0AA370 00002025 */   move  $a0, $zero
/* 0DEEA4 7F0AA374 3C198008 */  lui   $t9, %hi(g_CurrentPlayer)
/* 0DEEA8 7F0AA378 8F39A0B0 */  lw    $t9, %lo(g_CurrentPlayer)($t9)
/* 0DEEAC 7F0AA37C 3C098004 */  lui   $t1, %hi(D_800409DC+12)
/* 0DEEB0 7F0AA380 3401AC05 */  li    $at, 44037
/* 0DEEB4 7F0AA384 8F2F2A58 */  lw    $t7, 0x2a58($t9)
/* 0DEEB8 7F0AA388 8FA700A8 */  lw    $a3, 0xa8($sp)
/* 0DEEBC 7F0AA38C 000FC080 */  sll   $t8, $t7, 2
/* 0DEEC0 7F0AA390 030FC021 */  addu  $t8, $t8, $t7
/* 0DEEC4 7F0AA394 0018C080 */  sll   $t8, $t8, 2
/* 0DEEC8 7F0AA398 01384821 */  addu  $t1, $t1, $t8
/* 0DEECC 7F0AA39C 952909E8 */  lhu   $t1, %lo(D_800409DC+12)($t1)
/* 0DEED0 7F0AA3A0 15210005 */  bne   $t1, $at, .L7F0AA3B8
/* 0DEED4 7F0AA3A4 00000000 */   nop
/* 0DEED8 7F0AA3A8 0FC30776 */  jal   langGet
/* 0DEEDC 7F0AA3AC 3404AC31 */   li    $a0, 44081
/* 0DEEE0 7F0AA3B0 10000001 */  b     .L7F0AA3B8
/* 0DEEE4 7F0AA3B4 00403825 */   move  $a3, $v0
.L7F0AA3B8:
/* 0DEEE8 7F0AA3B8 10000010 */  b     .L7F0AA3FC
/* 0DEEEC 7F0AA3BC AFA7005C */   sw    $a3, 0x5c($sp)
.L7F0AA3C0:
/* 0DEEF0 7F0AA3C0 0C0030C3 */  jal   joyGetButtons
/* 0DEEF4 7F0AA3C4 24050002 */   li    $a1, 2
/* 0DEEF8 7F0AA3C8 10400005 */  beqz  $v0, .L7F0AA3E0
/* 0DEEFC 7F0AA3CC 00002025 */   move  $a0, $zero
/* 0DEF00 7F0AA3D0 0FC30776 */  jal   langGet
/* 0DEF04 7F0AA3D4 3404AC2F */   li    $a0, 44079
/* 0DEF08 7F0AA3D8 10000008 */  b     .L7F0AA3FC
/* 0DEF0C 7F0AA3DC AFA2005C */   sw    $v0, 0x5c($sp)
.L7F0AA3E0:
/* 0DEF10 7F0AA3E0 0C0030C3 */  jal   joyGetButtons
/* 0DEF14 7F0AA3E4 24050001 */   li    $a1, 1
/* 0DEF18 7F0AA3E8 50400005 */  beql  $v0, $zero, .L7F0AA400
/* 0DEF1C 7F0AA3EC 3C0C0070 */   lui   $t4, 0x70
/* 0DEF20 7F0AA3F0 0FC30776 */  jal   langGet
/* 0DEF24 7F0AA3F4 3404AC2E */   li    $a0, 44078
/* 0DEF28 7F0AA3F8 AFA2005C */  sw    $v0, 0x5c($sp)
.L7F0AA3FC:
/* 0DEF2C 7F0AA3FC 3C0C0070 */  lui   $t4, (0x007000A0 >> 16) # lui $t4, 0x70
.L7F0AA400:
/* 0DEF30 7F0AA400 3C0D0030 */  lui   $t5, (0x003000B0 >> 16) # lui $t5, 0x30
/* 0DEF34 7F0AA404 35AD00B0 */  ori   $t5, (0x003000B0 & 0xFFFF) # ori $t5, $t5, 0xb0
/* 0DEF38 7F0AA408 358C00A0 */  ori   $t4, (0x007000A0 & 0xFFFF) # ori $t4, $t4, 0xa0
/* 0DEF3C 7F0AA40C 240AFFFF */  li    $t2, -1
/* 0DEF40 7F0AA410 240B0001 */  li    $t3, 1
/* 0DEF44 7F0AA414 240E0001 */  li    $t6, 1
/* 0DEF48 7F0AA418 8FA7005C */  lw    $a3, 0x5c($sp)
/* 0DEF4C 7F0AA41C AFAE0028 */  sw    $t6, 0x28($sp)
/* 0DEF50 7F0AA420 AFAB0014 */  sw    $t3, 0x14($sp)
/* 0DEF54 7F0AA424 AFAA0010 */  sw    $t2, 0x10($sp)
/* 0DEF58 7F0AA428 AFAC0018 */  sw    $t4, 0x18($sp)
/* 0DEF5C 7F0AA42C AFAD0024 */  sw    $t5, 0x24($sp)
/* 0DEF60 7F0AA430 02002025 */  move  $a0, $s0
/* 0DEF64 7F0AA434 2405010E */  li    $a1, 270
/* 0DEF68 7F0AA438 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DEF6C 7F0AA43C AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DEF70 7F0AA440 0FC2A4E6 */  jal   draw_options_labels
/* 0DEF74 7F0AA444 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DEF78 7F0AA448 1000001D */  b     .L7F0AA4C0
/* 0DEF7C 7F0AA44C 00408025 */   move  $s0, $v0
.L7F0AA450:
/* 0DEF80 7F0AA450 8D08A0B0 */  lw    $t0, %lo(g_CurrentPlayer)($t0)
/* 0DEF84 7F0AA454 3C048004 */  lui   $a0, %hi(D_800409DC+12)
/* 0DEF88 7F0AA458 8D192A58 */  lw    $t9, 0x2a58($t0)
/* 0DEF8C 7F0AA45C 00197880 */  sll   $t7, $t9, 2
/* 0DEF90 7F0AA460 01F97821 */  addu  $t7, $t7, $t9
/* 0DEF94 7F0AA464 000F7880 */  sll   $t7, $t7, 2
/* 0DEF98 7F0AA468 008F2021 */  addu  $a0, $a0, $t7
/* 0DEF9C 7F0AA46C 0FC30776 */  jal   langGet
/* 0DEFA0 7F0AA470 948409E8 */   lhu   $a0, %lo(D_800409DC+12)($a0)
/* 0DEFA4 7F0AA474 3C1800AA */  lui   $t8, (0x00AA00B0 >> 16) # lui $t8, 0xaa
/* 0DEFA8 7F0AA478 3C0A0030 */  lui   $t2, (0x003000B0 >> 16) # lui $t2, 0x30
/* 0DEFAC 7F0AA47C 354A00B0 */  ori   $t2, (0x003000B0 & 0xFFFF) # ori $t2, $t2, 0xb0
/* 0DEFB0 7F0AA480 371800B0 */  ori   $t8, (0x00AA00B0 & 0xFFFF) # ori $t8, $t8, 0xb0
/* 0DEFB4 7F0AA484 2409FFFF */  li    $t1, -1
/* 0DEFB8 7F0AA488 240B0001 */  li    $t3, 1
/* 0DEFBC 7F0AA48C AFAB0028 */  sw    $t3, 0x28($sp)
/* 0DEFC0 7F0AA490 AFA90018 */  sw    $t1, 0x18($sp)
/* 0DEFC4 7F0AA494 AFB80010 */  sw    $t8, 0x10($sp)
/* 0DEFC8 7F0AA498 AFAA0024 */  sw    $t2, 0x24($sp)
/* 0DEFCC 7F0AA49C 02002025 */  move  $a0, $s0
/* 0DEFD0 7F0AA4A0 2405010E */  li    $a1, 270
/* 0DEFD4 7F0AA4A4 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DEFD8 7F0AA4A8 00403825 */  move  $a3, $v0
/* 0DEFDC 7F0AA4AC AFA00014 */  sw    $zero, 0x14($sp)
/* 0DEFE0 7F0AA4B0 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DEFE4 7F0AA4B4 0FC2A4E6 */  jal   draw_options_labels
/* 0DEFE8 7F0AA4B8 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DEFEC 7F0AA4BC 00408025 */  move  $s0, $v0
.L7F0AA4C0:
/* 0DEFF0 7F0AA4C0 8FAC004C */  lw    $t4, 0x4c($sp)
/* 0DEFF4 7F0AA4C4 00002025 */  move  $a0, $zero
/* 0DEFF8 7F0AA4C8 24054000 */  li    $a1, 16384
/* 0DEFFC 7F0AA4CC 258D0019 */  addiu $t5, $t4, 0x19
/* 0DF000 7F0AA4D0 0C0030C3 */  jal   joyGetButtons
/* 0DF004 7F0AA4D4 AFAD004C */   sw    $t5, 0x4c($sp)
/* 0DF008 7F0AA4D8 10400020 */  beqz  $v0, .L7F0AA55C
/* 0DF00C 7F0AA4DC 3C0C8008 */   lui   $t4, %hi(g_CurrentPlayer)
/* 0DF010 7F0AA4E0 3C0E8008 */  lui   $t6, %hi(g_CurrentPlayer)
/* 0DF014 7F0AA4E4 8DCEA0B0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0DF018 7F0AA4E8 3C048004 */  lui   $a0, %hi(D_800409DC+4)
/* 0DF01C 7F0AA4EC 8DC82A58 */  lw    $t0, 0x2a58($t6)
/* 0DF020 7F0AA4F0 0008C880 */  sll   $t9, $t0, 2
/* 0DF024 7F0AA4F4 0328C821 */  addu  $t9, $t9, $t0
/* 0DF028 7F0AA4F8 0019C880 */  sll   $t9, $t9, 2
/* 0DF02C 7F0AA4FC 00992021 */  addu  $a0, $a0, $t9
/* 0DF030 7F0AA500 0FC30776 */  jal   langGet
/* 0DF034 7F0AA504 948409E0 */   lhu   $a0, %lo(D_800409DC+4)($a0)
/* 0DF038 7F0AA508 3C090070 */  lui   $t1, (0x007000A0 >> 16) # lui $t1, 0x70
/* 0DF03C 7F0AA50C 3C0A0030 */  lui   $t2, (0x003000B0 >> 16) # lui $t2, 0x30
/* 0DF040 7F0AA510 354A00B0 */  ori   $t2, (0x003000B0 & 0xFFFF) # ori $t2, $t2, 0xb0
/* 0DF044 7F0AA514 352900A0 */  ori   $t1, (0x007000A0 & 0xFFFF) # ori $t1, $t1, 0xa0
/* 0DF048 7F0AA518 240FFFFF */  li    $t7, -1
/* 0DF04C 7F0AA51C 24180001 */  li    $t8, 1
/* 0DF050 7F0AA520 240B0001 */  li    $t3, 1
/* 0DF054 7F0AA524 AFAB0028 */  sw    $t3, 0x28($sp)
/* 0DF058 7F0AA528 AFB80014 */  sw    $t8, 0x14($sp)
/* 0DF05C 7F0AA52C AFAF0010 */  sw    $t7, 0x10($sp)
/* 0DF060 7F0AA530 AFA90018 */  sw    $t1, 0x18($sp)
/* 0DF064 7F0AA534 AFAA0024 */  sw    $t2, 0x24($sp)
/* 0DF068 7F0AA538 02002025 */  move  $a0, $s0
/* 0DF06C 7F0AA53C 2405010E */  li    $a1, 270
/* 0DF070 7F0AA540 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DF074 7F0AA544 00403825 */  move  $a3, $v0
/* 0DF078 7F0AA548 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DF07C 7F0AA54C 0FC2A4E6 */  jal   draw_options_labels
/* 0DF080 7F0AA550 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DF084 7F0AA554 1000001D */  b     .L7F0AA5CC
/* 0DF088 7F0AA558 00408025 */   move  $s0, $v0
.L7F0AA55C:
/* 0DF08C 7F0AA55C 8D8CA0B0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DF090 7F0AA560 3C048004 */  lui   $a0, %hi(D_800409DC+4)
/* 0DF094 7F0AA564 8D8D2A58 */  lw    $t5, 0x2a58($t4)
/* 0DF098 7F0AA568 000D7080 */  sll   $t6, $t5, 2
/* 0DF09C 7F0AA56C 01CD7021 */  addu  $t6, $t6, $t5
/* 0DF0A0 7F0AA570 000E7080 */  sll   $t6, $t6, 2
/* 0DF0A4 7F0AA574 008E2021 */  addu  $a0, $a0, $t6
/* 0DF0A8 7F0AA578 0FC30776 */  jal   langGet
/* 0DF0AC 7F0AA57C 948409E0 */   lhu   $a0, %lo(D_800409DC+4)($a0)
/* 0DF0B0 7F0AA580 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DF0B4 7F0AA584 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DF0B8 7F0AA588 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DF0BC 7F0AA58C 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DF0C0 7F0AA590 2419FFFF */  li    $t9, -1
/* 0DF0C4 7F0AA594 24180001 */  li    $t8, 1
/* 0DF0C8 7F0AA598 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DF0CC 7F0AA59C AFB90018 */  sw    $t9, 0x18($sp)
/* 0DF0D0 7F0AA5A0 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DF0D4 7F0AA5A4 AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DF0D8 7F0AA5A8 02002025 */  move  $a0, $s0
/* 0DF0DC 7F0AA5AC 2405010E */  li    $a1, 270
/* 0DF0E0 7F0AA5B0 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DF0E4 7F0AA5B4 00403825 */  move  $a3, $v0
/* 0DF0E8 7F0AA5B8 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DF0EC 7F0AA5BC AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DF0F0 7F0AA5C0 0FC2A4E6 */  jal   draw_options_labels
/* 0DF0F4 7F0AA5C4 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DF0F8 7F0AA5C8 00408025 */  move  $s0, $v0
.L7F0AA5CC:
/* 0DF0FC 7F0AA5CC 8FA9004C */  lw    $t1, 0x4c($sp)
/* 0DF100 7F0AA5D0 00002025 */  move  $a0, $zero
/* 0DF104 7F0AA5D4 34058000 */  li    $a1, 32768
/* 0DF108 7F0AA5D8 252A0019 */  addiu $t2, $t1, 0x19
/* 0DF10C 7F0AA5DC 0C0030C3 */  jal   joyGetButtons
/* 0DF110 7F0AA5E0 AFAA004C */   sw    $t2, 0x4c($sp)
/* 0DF114 7F0AA5E4 10400020 */  beqz  $v0, .L7F0AA668
/* 0DF118 7F0AA5E8 3C098008 */   lui   $t1, %hi(g_CurrentPlayer)
/* 0DF11C 7F0AA5EC 3C0B8008 */  lui   $t3, %hi(g_CurrentPlayer)
/* 0DF120 7F0AA5F0 8D6BA0B0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DF124 7F0AA5F4 3C048004 */  lui   $a0, %hi(D_800409DC+2)
/* 0DF128 7F0AA5F8 8D6C2A58 */  lw    $t4, 0x2a58($t3)
/* 0DF12C 7F0AA5FC 000C6880 */  sll   $t5, $t4, 2
/* 0DF130 7F0AA600 01AC6821 */  addu  $t5, $t5, $t4
/* 0DF134 7F0AA604 000D6880 */  sll   $t5, $t5, 2
/* 0DF138 7F0AA608 008D2021 */  addu  $a0, $a0, $t5
/* 0DF13C 7F0AA60C 0FC30776 */  jal   langGet
/* 0DF140 7F0AA610 948409DE */   lhu   $a0, %lo(D_800409DC+2)($a0)
/* 0DF144 7F0AA614 3C190070 */  lui   $t9, (0x007000A0 >> 16) # lui $t9, 0x70
/* 0DF148 7F0AA618 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DF14C 7F0AA61C 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DF150 7F0AA620 373900A0 */  ori   $t9, (0x007000A0 & 0xFFFF) # ori $t9, $t9, 0xa0
/* 0DF154 7F0AA624 240EFFFF */  li    $t6, -1
/* 0DF158 7F0AA628 24080001 */  li    $t0, 1
/* 0DF15C 7F0AA62C 24180001 */  li    $t8, 1
/* 0DF160 7F0AA630 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DF164 7F0AA634 AFA80014 */  sw    $t0, 0x14($sp)
/* 0DF168 7F0AA638 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DF16C 7F0AA63C AFB90018 */  sw    $t9, 0x18($sp)
/* 0DF170 7F0AA640 AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DF174 7F0AA644 02002025 */  move  $a0, $s0
/* 0DF178 7F0AA648 2405010E */  li    $a1, 270
/* 0DF17C 7F0AA64C 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DF180 7F0AA650 00403825 */  move  $a3, $v0
/* 0DF184 7F0AA654 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DF188 7F0AA658 0FC2A4E6 */  jal   draw_options_labels
/* 0DF18C 7F0AA65C AFA00020 */   sw    $zero, 0x20($sp)
/* 0DF190 7F0AA660 1000001D */  b     .L7F0AA6D8
/* 0DF194 7F0AA664 00408025 */   move  $s0, $v0
.L7F0AA668:
/* 0DF198 7F0AA668 8D29A0B0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DF19C 7F0AA66C 3C048004 */  lui   $a0, %hi(D_800409DC+2)
/* 0DF1A0 7F0AA670 8D2A2A58 */  lw    $t2, 0x2a58($t1)
/* 0DF1A4 7F0AA674 000A5880 */  sll   $t3, $t2, 2
/* 0DF1A8 7F0AA678 016A5821 */  addu  $t3, $t3, $t2
/* 0DF1AC 7F0AA67C 000B5880 */  sll   $t3, $t3, 2
/* 0DF1B0 7F0AA680 008B2021 */  addu  $a0, $a0, $t3
/* 0DF1B4 7F0AA684 0FC30776 */  jal   langGet
/* 0DF1B8 7F0AA688 948409DE */   lhu   $a0, %lo(D_800409DC+2)($a0)
/* 0DF1BC 7F0AA68C 3C0C00AA */  lui   $t4, (0x00AA00B0 >> 16) # lui $t4, 0xaa
/* 0DF1C0 7F0AA690 3C0E0030 */  lui   $t6, (0x003000B0 >> 16) # lui $t6, 0x30
/* 0DF1C4 7F0AA694 35CE00B0 */  ori   $t6, (0x003000B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DF1C8 7F0AA698 358C00B0 */  ori   $t4, (0x00AA00B0 & 0xFFFF) # ori $t4, $t4, 0xb0
/* 0DF1CC 7F0AA69C 240DFFFF */  li    $t5, -1
/* 0DF1D0 7F0AA6A0 24080001 */  li    $t0, 1
/* 0DF1D4 7F0AA6A4 AFA80028 */  sw    $t0, 0x28($sp)
/* 0DF1D8 7F0AA6A8 AFAD0018 */  sw    $t5, 0x18($sp)
/* 0DF1DC 7F0AA6AC AFAC0010 */  sw    $t4, 0x10($sp)
/* 0DF1E0 7F0AA6B0 AFAE0024 */  sw    $t6, 0x24($sp)
/* 0DF1E4 7F0AA6B4 02002025 */  move  $a0, $s0
/* 0DF1E8 7F0AA6B8 2405010E */  li    $a1, 270
/* 0DF1EC 7F0AA6BC 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DF1F0 7F0AA6C0 00403825 */  move  $a3, $v0
/* 0DF1F4 7F0AA6C4 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DF1F8 7F0AA6C8 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DF1FC 7F0AA6CC 0FC2A4E6 */  jal   draw_options_labels
/* 0DF200 7F0AA6D0 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DF204 7F0AA6D4 00408025 */  move  $s0, $v0
.L7F0AA6D8:
/* 0DF208 7F0AA6D8 8FB90050 */  lw    $t9, 0x50($sp)
/* 0DF20C 7F0AA6DC 3C0C8008 */  lui   $t4, %hi(g_CurrentPlayer)
/* 0DF210 7F0AA6E0 13200018 */  beqz  $t9, .L7F0AA744
/* 0DF214 7F0AA6E4 00000000 */   nop
/* 0DF218 7F0AA6E8 0FC30776 */  jal   langGet
/* 0DF21C 7F0AA6EC 3404AC08 */   li    $a0, 44040
/* 0DF220 7F0AA6F0 3C090070 */  lui   $t1, (0x007000A0 >> 16) # lui $t1, 0x70
/* 0DF224 7F0AA6F4 3C0A0030 */  lui   $t2, (0x003000B0 >> 16) # lui $t2, 0x30
/* 0DF228 7F0AA6F8 354A00B0 */  ori   $t2, (0x003000B0 & 0xFFFF) # ori $t2, $t2, 0xb0
/* 0DF22C 7F0AA6FC 352900A0 */  ori   $t1, (0x007000A0 & 0xFFFF) # ori $t1, $t1, 0xa0
/* 0DF230 7F0AA700 240FFFFF */  li    $t7, -1
/* 0DF234 7F0AA704 24180001 */  li    $t8, 1
/* 0DF238 7F0AA708 240B0001 */  li    $t3, 1
/* 0DF23C 7F0AA70C AFAB0028 */  sw    $t3, 0x28($sp)
/* 0DF240 7F0AA710 AFB80014 */  sw    $t8, 0x14($sp)
/* 0DF244 7F0AA714 AFAF0010 */  sw    $t7, 0x10($sp)
/* 0DF248 7F0AA718 AFA90018 */  sw    $t1, 0x18($sp)
/* 0DF24C 7F0AA71C AFAA0024 */  sw    $t2, 0x24($sp)
/* 0DF250 7F0AA720 02002025 */  move  $a0, $s0
/* 0DF254 7F0AA724 240500FA */  li    $a1, 250
/* 0DF258 7F0AA728 240600C3 */  li    $a2, 195
/* 0DF25C 7F0AA72C 00403825 */  move  $a3, $v0
/* 0DF260 7F0AA730 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DF264 7F0AA734 0FC2A4E6 */  jal   draw_options_labels
/* 0DF268 7F0AA738 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DF26C 7F0AA73C 1000001D */  b     .L7F0AA7B4
/* 0DF270 7F0AA740 00408025 */   move  $s0, $v0
.L7F0AA744:
/* 0DF274 7F0AA744 8D8CA0B0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DF278 7F0AA748 3C048004 */  lui   $a0, %hi(D_800409DC+18)
/* 0DF27C 7F0AA74C 8D8D2A58 */  lw    $t5, 0x2a58($t4)
/* 0DF280 7F0AA750 000D7080 */  sll   $t6, $t5, 2
/* 0DF284 7F0AA754 01CD7021 */  addu  $t6, $t6, $t5
/* 0DF288 7F0AA758 000E7080 */  sll   $t6, $t6, 2
/* 0DF28C 7F0AA75C 008E2021 */  addu  $a0, $a0, $t6
/* 0DF290 7F0AA760 0FC30776 */  jal   langGet
/* 0DF294 7F0AA764 948409EE */   lhu   $a0, %lo(D_800409DC+18)($a0)
/* 0DF298 7F0AA768 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DF29C 7F0AA76C 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DF2A0 7F0AA770 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DF2A4 7F0AA774 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DF2A8 7F0AA778 2419FFFF */  li    $t9, -1
/* 0DF2AC 7F0AA77C 24180001 */  li    $t8, 1
/* 0DF2B0 7F0AA780 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DF2B4 7F0AA784 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DF2B8 7F0AA788 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DF2BC 7F0AA78C AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DF2C0 7F0AA790 02002025 */  move  $a0, $s0
/* 0DF2C4 7F0AA794 240500FA */  li    $a1, 250
/* 0DF2C8 7F0AA798 240600C3 */  li    $a2, 195
/* 0DF2CC 7F0AA79C 00403825 */  move  $a3, $v0
/* 0DF2D0 7F0AA7A0 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DF2D4 7F0AA7A4 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DF2D8 7F0AA7A8 0FC2A4E6 */  jal   draw_options_labels
/* 0DF2DC 7F0AA7AC AFA00020 */   sw    $zero, 0x20($sp)
/* 0DF2E0 7F0AA7B0 00408025 */  move  $s0, $v0
.L7F0AA7B4:
/* 0DF2E4 7F0AA7B4 8FBF003C */  lw    $ra, 0x3c($sp)
/* 0DF2E8 7F0AA7B8 02001025 */  move  $v0, $s0
/* 0DF2EC 7F0AA7BC 8FB00038 */  lw    $s0, 0x38($sp)
/* 0DF2F0 7F0AA7C0 03E00008 */  jr    $ra
/* 0DF2F4 7F0AA7C4 27BD00B0 */   addiu $sp, $sp, 0xb0
)
#endif

#if !defined(LEFTOVERDEBUG)
GLOBAL_ASM(
.text
glabel sub_GAME_7F0A9AB8
/* 0DB7E8 7F0A8DF8 3C0E8004 */  lui   $t6, %hi(game_options_entries) # $t6, 0x8004
/* 0DB7EC 7F0A8DFC 8DCEA6D4 */  lw    $t6, %lo(game_options_entries + 0x8)($t6)
/* 0DB7F0 7F0A8E00 27BDFF50 */  addiu $sp, $sp, -0xb0
/* 0DB7F4 7F0A8E04 AFB00038 */  sw    $s0, 0x38($sp)
/* 0DB7F8 7F0A8E08 24010001 */  li    $at, 1
/* 0DB7FC 7F0A8E0C 00808025 */  move  $s0, $a0
/* 0DB800 7F0A8E10 15C10008 */  bne   $t6, $at, .L7F0A8E34
/* 0DB804 7F0A8E14 AFBF003C */   sw    $ra, 0x3c($sp)
/* 0DB808 7F0A8E18 0FC304AE */  jal   langGet
/* 0DB80C 7F0A8E1C 3404AC2D */   li    $a0, 44077
/* 0DB810 7F0A8E20 AFA200AC */  sw    $v0, 0xac($sp)
/* 0DB814 7F0A8E24 0FC304AE */  jal   langGet
/* 0DB818 7F0A8E28 3404AC2C */   li    $a0, 44076
/* 0DB81C 7F0A8E2C 10000007 */  b     .L7F0A8E4C
/* 0DB820 7F0A8E30 AFA200A8 */   sw    $v0, 0xa8($sp)
.L7F0A8E34:
/* 0DB824 7F0A8E34 0FC304AE */  jal   langGet
/* 0DB828 7F0A8E38 3404AC2C */   li    $a0, 44076
/* 0DB82C 7F0A8E3C AFA200AC */  sw    $v0, 0xac($sp)
/* 0DB830 7F0A8E40 0FC304AE */  jal   langGet
/* 0DB834 7F0A8E44 3404AC2D */   li    $a0, 44077
/* 0DB838 7F0A8E48 AFA200A8 */  sw    $v0, 0xa8($sp)
.L7F0A8E4C:
/* 0DB83C 7F0A8E4C 3C188005 */  lui   $t8, %hi(aA) # $t8, 0x8005
/* 0DB840 7F0A8E50 2718CE48 */  addiu $t8, %lo(aA) # addiu $t8, $t8, -0x31b8
/* 0DB844 7F0A8E54 8F010000 */  lw    $at, ($t8)
/* 0DB848 7F0A8E58 27AF00A0 */  addiu $t7, $sp, 0xa0
/* 0DB84C 7F0A8E5C 3C0B8005 */  lui   $t3, %hi(aB) # $t3, 0x8005
/* 0DB850 7F0A8E60 ADE10000 */  sw    $at, ($t7)
/* 0DB854 7F0A8E64 93010004 */  lbu   $at, 4($t8)
/* 0DB858 7F0A8E68 256BCE50 */  addiu $t3, %lo(aB) # addiu $t3, $t3, -0x31b0
/* 0DB85C 7F0A8E6C 27AA0098 */  addiu $t2, $sp, 0x98
/* 0DB860 7F0A8E70 A1E10004 */  sb    $at, 4($t7)
/* 0DB864 7F0A8E74 8D610000 */  lw    $at, ($t3)
/* 0DB868 7F0A8E78 3C198005 */  lui   $t9, %hi(aZ) # $t9, 0x8005
/* 0DB86C 7F0A8E7C 2739CE58 */  addiu $t9, %lo(aZ) # addiu $t9, $t9, -0x31a8
/* 0DB870 7F0A8E80 AD410000 */  sw    $at, ($t2)
/* 0DB874 7F0A8E84 91610004 */  lbu   $at, 4($t3)
/* 0DB878 7F0A8E88 27A80090 */  addiu $t0, $sp, 0x90
/* 0DB87C 7F0A8E8C 3C0C8005 */  lui   $t4, %hi(aL) # $t4, 0x8005
/* 0DB880 7F0A8E90 A1410004 */  sb    $at, 4($t2)
/* 0DB884 7F0A8E94 8F210000 */  lw    $at, ($t9)
/* 0DB888 7F0A8E98 258CCE60 */  addiu $t4, %lo(aL) # addiu $t4, $t4, -0x31a0
/* 0DB88C 7F0A8E9C 27AD0088 */  addiu $t5, $sp, 0x88
/* 0DB890 7F0A8EA0 AD010000 */  sw    $at, ($t0)
/* 0DB894 7F0A8EA4 93210004 */  lbu   $at, 4($t9)
/* 0DB898 7F0A8EA8 3C0F8005 */  lui   $t7, %hi(aR) # $t7, 0x8005
/* 0DB89C 7F0A8EAC 25EFCE68 */  addiu $t7, %lo(aR) # addiu $t7, $t7, -0x3198
/* 0DB8A0 7F0A8EB0 A1010004 */  sb    $at, 4($t0)
/* 0DB8A4 7F0A8EB4 8D810000 */  lw    $at, ($t4)
/* 0DB8A8 7F0A8EB8 27B80080 */  addiu $t8, $sp, 0x80
/* 0DB8AC 7F0A8EBC 3C0A8005 */  lui   $t2, %hi(aC) # $t2, 0x8005
/* 0DB8B0 7F0A8EC0 ADA10000 */  sw    $at, ($t5)
/* 0DB8B4 7F0A8EC4 91810004 */  lbu   $at, 4($t4)
/* 0DB8B8 7F0A8EC8 254ACE70 */  addiu $t2, %lo(aC) # addiu $t2, $t2, -0x3190
/* 0DB8BC 7F0A8ECC 27AB0078 */  addiu $t3, $sp, 0x78
/* 0DB8C0 7F0A8ED0 A1A10004 */  sb    $at, 4($t5)
/* 0DB8C4 7F0A8ED4 8DE10000 */  lw    $at, ($t7)
/* 0DB8C8 7F0A8ED8 3C088005 */  lui   $t0, %hi(D_800577A8) # $t0, 0x8005
/* 0DB8CC 7F0A8EDC 2508CE78 */  addiu $t0, %lo(D_800577A8) # addiu $t0, $t0, -0x3188
/* 0DB8D0 7F0A8EE0 AF010000 */  sw    $at, ($t8)
/* 0DB8D4 7F0A8EE4 91E10004 */  lbu   $at, 4($t7)
/* 0DB8D8 7F0A8EE8 27B90070 */  addiu $t9, $sp, 0x70
/* 0DB8DC 7F0A8EEC 3C0D8005 */  lui   $t5, %hi(aS_0) # $t5, 0x8005
/* 0DB8E0 7F0A8EF0 A3010004 */  sb    $at, 4($t8)
/* 0DB8E4 7F0A8EF4 8D410000 */  lw    $at, ($t2)
/* 0DB8E8 7F0A8EF8 25ADCE80 */  addiu $t5, %lo(aS_0) # addiu $t5, $t5, -0x3180
/* 0DB8EC 7F0A8EFC 27AC0068 */  addiu $t4, $sp, 0x68
/* 0DB8F0 7F0A8F00 AD610000 */  sw    $at, ($t3)
/* 0DB8F4 7F0A8F04 91410004 */  lbu   $at, 4($t2)
/* 0DB8F8 7F0A8F08 3C188005 */  lui   $t8, %hi(a3d) # $t8, 0x8005
/* 0DB8FC 7F0A8F0C 2718CE88 */  addiu $t8, %lo(a3d) # addiu $t8, $t8, -0x3178
/* 0DB900 7F0A8F10 A1610004 */  sb    $at, 4($t3)
/* 0DB904 7F0A8F14 8D010000 */  lw    $at, ($t0)
/* 0DB908 7F0A8F18 27AF0060 */  addiu $t7, $sp, 0x60
/* 0DB90C 7F0A8F1C 02002025 */  move  $a0, $s0
/* 0DB910 7F0A8F20 AF210000 */  sw    $at, ($t9)
/* 0DB914 7F0A8F24 91010004 */  lbu   $at, 4($t0)
/* 0DB918 7F0A8F28 A3210004 */  sb    $at, 4($t9)
/* 0DB91C 7F0A8F2C 8DA10000 */  lw    $at, ($t5)
/* 0DB920 7F0A8F30 AD810000 */  sw    $at, ($t4)
/* 0DB924 7F0A8F34 91A10004 */  lbu   $at, 4($t5)
/* 0DB928 7F0A8F38 A1810004 */  sb    $at, 4($t4)
/* 0DB92C 7F0A8F3C 8F010000 */  lw    $at, ($t8)
/* 0DB930 7F0A8F40 ADE10000 */  sw    $at, ($t7)
/* 0DB934 7F0A8F44 97010004 */  lhu   $at, 4($t8)
/* 0DB938 7F0A8F48 A5E10004 */  sh    $at, 4($t7)
/* 0DB93C 7F0A8F4C 0FC2B016 */  jal   microcode_constructor
/* 0DB940 7F0A8F50 AFA00050 */   sw    $zero, 0x50($sp)
/* 0DB944 7F0A8F54 00408025 */  move  $s0, $v0
/* 0DB948 7F0A8F58 00002025 */  move  $a0, $zero
/* 0DB94C 7F0A8F5C 0C002C20 */  jal   joyGetButtons
/* 0DB950 7F0A8F60 24050020 */   li    $a1, 32
/* 0DB954 7F0A8F64 1040002C */  beqz  $v0, .L7F0A9018
/* 0DB958 7F0A8F68 3C0A8007 */   lui   $t2, %hi(g_CurrentPlayer) # $t2, 0x8007
/* 0DB95C 7F0A8F6C 8D4A8BC0 */  lw    $t2, %lo(g_CurrentPlayer)($t2)
/* 0DB960 7F0A8F70 3C048004 */  lui   $a0, %hi(D_800409DC+8)
/* 0DB964 7F0A8F74 8D4B2A50 */  lw    $t3, 0x2a50($t2)
/* 0DB968 7F0A8F78 000B6080 */  sll   $t4, $t3, 2
/* 0DB96C 7F0A8F7C 018B6021 */  addu  $t4, $t4, $t3
/* 0DB970 7F0A8F80 000C6080 */  sll   $t4, $t4, 2
/* 0DB974 7F0A8F84 008C2021 */  addu  $a0, $a0, $t4
/* 0DB978 7F0A8F88 0FC304AE */  jal   langGet
/* 0DB97C 7F0A8F8C 9484A634 */   lhu   $a0, %lo(D_800409DC+8)($a0)
/* 0DB980 7F0A8F90 3C080070 */  lui   $t0, (0x007000A0 >> 16) # lui $t0, 0x70
/* 0DB984 7F0A8F94 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DB988 7F0A8F98 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DB98C 7F0A8F9C 350800A0 */  ori   $t0, (0x007000A0 & 0xFFFF) # ori $t0, $t0, 0xa0
/* 0DB990 7F0A8FA0 240DFFFF */  li    $t5, -1
/* 0DB994 7F0A8FA4 240E0001 */  li    $t6, 1
/* 0DB998 7F0A8FA8 AFAE0014 */  sw    $t6, 0x14($sp)
/* 0DB99C 7F0A8FAC AFAD0010 */  sw    $t5, 0x10($sp)
/* 0DB9A0 7F0A8FB0 AFA80018 */  sw    $t0, 0x18($sp)
/* 0DB9A4 7F0A8FB4 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DB9A8 7F0A8FB8 02002025 */  move  $a0, $s0
/* 0DB9AC 7F0A8FBC 24050032 */  li    $a1, 50
/* 0DB9B0 7F0A8FC0 2406005C */  li    $a2, 92
/* 0DB9B4 7F0A8FC4 00403825 */  move  $a3, $v0
/* 0DB9B8 7F0A8FC8 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DB9BC 7F0A8FCC AFA00020 */  sw    $zero, 0x20($sp)
/* 0DB9C0 7F0A8FD0 0FC2A1B6 */  jal   draw_options_labels
/* 0DB9C4 7F0A8FD4 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DB9C8 7F0A8FD8 3C0F8007 */  lui   $t7, %hi(g_CurrentPlayer) # $t7, 0x8007
/* 0DB9CC 7F0A8FDC 8DEF8BC0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0DB9D0 7F0A8FE0 3C0A8004 */  lui   $t2, %hi(D_800409DC+9)
/* 0DB9D4 7F0A8FE4 3401AC01 */  li    $at, 44033
/* 0DB9D8 7F0A8FE8 8DF82A50 */  lw    $t8, 0x2a50($t7)
/* 0DB9DC 7F0A8FEC 00408025 */  move  $s0, $v0
/* 0DB9E0 7F0A8FF0 240B0001 */  li    $t3, 1
/* 0DB9E4 7F0A8FF4 00184880 */  sll   $t1, $t8, 2
/* 0DB9E8 7F0A8FF8 01384821 */  addu  $t1, $t1, $t8
/* 0DB9EC 7F0A8FFC 00094880 */  sll   $t1, $t1, 2
/* 0DB9F0 7F0A9000 01495021 */  addu  $t2, $t2, $t1
/* 0DB9F4 7F0A9004 954AA634 */  lhu   $t2, %lo(D_800409DC+8)($t2)
/* 0DB9F8 7F0A9008 1541001F */  bne   $t2, $at, .L7F0A9088
/* 0DB9FC 7F0A900C 00000000 */   nop
/* 0DBA00 7F0A9010 1000001D */  b     .L7F0A9088
/* 0DBA04 7F0A9014 AFAB0050 */   sw    $t3, 0x50($sp)
.L7F0A9018:
/* 0DBA08 7F0A9018 3C0C8007 */  lui   $t4, %hi(g_CurrentPlayer) # $t4, 0x8007
/* 0DBA0C 7F0A901C 8D8C8BC0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DBA10 7F0A9020 3C048004 */  lui   $a0, %hi(D_800409DC+8)
/* 0DBA14 7F0A9024 8D8D2A50 */  lw    $t5, 0x2a50($t4)
/* 0DBA18 7F0A9028 000D7080 */  sll   $t6, $t5, 2
/* 0DBA1C 7F0A902C 01CD7021 */  addu  $t6, $t6, $t5
/* 0DBA20 7F0A9030 000E7080 */  sll   $t6, $t6, 2
/* 0DBA24 7F0A9034 008E2021 */  addu  $a0, $a0, $t6
/* 0DBA28 7F0A9038 0FC304AE */  jal   langGet
/* 0DBA2C 7F0A903C 9484A634 */   lhu   $a0, %lo(D_800409DC+8)($a0)
/* 0DBA30 7F0A9040 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DBA34 7F0A9044 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DBA38 7F0A9048 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DBA3C 7F0A904C 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DBA40 7F0A9050 2419FFFF */  li    $t9, -1
/* 0DBA44 7F0A9054 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DBA48 7F0A9058 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DBA4C 7F0A905C AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DBA50 7F0A9060 02002025 */  move  $a0, $s0
/* 0DBA54 7F0A9064 24050032 */  li    $a1, 50
/* 0DBA58 7F0A9068 2406005C */  li    $a2, 92
/* 0DBA5C 7F0A906C 00403825 */  move  $a3, $v0
/* 0DBA60 7F0A9070 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DBA64 7F0A9074 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBA68 7F0A9078 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DBA6C 7F0A907C 0FC2A1B6 */  jal   draw_options_labels
/* 0DBA70 7F0A9080 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DBA74 7F0A9084 00408025 */  move  $s0, $v0
.L7F0A9088:
/* 0DBA78 7F0A9088 3C098004 */  lui   $t1, %hi(controller_options_index) # $t1, 0x8004
/* 0DBA7C 7F0A908C 8D29A5E8 */  lw    $t1, %lo(controller_options_index)($t1)
/* 0DBA80 7F0A9090 2418007A */  li    $t8, 122
/* 0DBA84 7F0A9094 24010001 */  li    $at, 1
/* 0DBA88 7F0A9098 1521000A */  bne   $t1, $at, .L7F0A90C4
/* 0DBA8C 7F0A909C AFB8004C */   sw    $t8, 0x4c($sp)
/* 0DBA90 7F0A90A0 3C0A8004 */  lui   $t2, %hi(watch_item_is_actively_selected) # $t2, 0x8004
/* 0DBA94 7F0A90A4 8D4AA5F8 */  lw    $t2, %lo(watch_item_is_actively_selected)($t2)
/* 0DBA98 7F0A90A8 00002025 */  move  $a0, $zero
/* 0DBA9C 7F0A90AC 11400005 */  beqz  $t2, .L7F0A90C4
/* 0DBAA0 7F0A90B0 00000000 */   nop
/* 0DBAA4 7F0A90B4 0C002C20 */  jal   joyGetButtons
/* 0DBAA8 7F0A90B8 24050F00 */   li    $a1, 3840
/* 0DBAAC 7F0A90BC 1440001E */  bnez  $v0, .L7F0A9138
/* 0DBAB0 7F0A90C0 00002025 */   move  $a0, $zero
.L7F0A90C4:
/* 0DBAB4 7F0A90C4 3C0B8007 */  lui   $t3, %hi(g_CurrentPlayer) # $t3, 0x8007
/* 0DBAB8 7F0A90C8 8D6B8BC0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DBABC 7F0A90CC 3C048004 */  lui   $a0, %hi(D_800409DC+14)
/* 0DBAC0 7F0A90D0 8D6C2A50 */  lw    $t4, 0x2a50($t3)
/* 0DBAC4 7F0A90D4 000C6880 */  sll   $t5, $t4, 2
/* 0DBAC8 7F0A90D8 01AC6821 */  addu  $t5, $t5, $t4
/* 0DBACC 7F0A90DC 000D6880 */  sll   $t5, $t5, 2
/* 0DBAD0 7F0A90E0 008D2021 */  addu  $a0, $a0, $t5
/* 0DBAD4 7F0A90E4 0FC304AE */  jal   langGet
/* 0DBAD8 7F0A90E8 9484A63A */   lhu   $a0, %lo(D_800409DC+14)($a0)
/* 0DBADC 7F0A90EC 3C0E00AA */  lui   $t6, (0x00AA00B0 >> 16) # lui $t6, 0xaa
/* 0DBAE0 7F0A90F0 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DBAE4 7F0A90F4 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DBAE8 7F0A90F8 35CE00B0 */  ori   $t6, (0x00AA00B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DBAEC 7F0A90FC 2408FFFF */  li    $t0, -1
/* 0DBAF0 7F0A9100 AFA80018 */  sw    $t0, 0x18($sp)
/* 0DBAF4 7F0A9104 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DBAF8 7F0A9108 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DBAFC 7F0A910C 02002025 */  move  $a0, $s0
/* 0DBB00 7F0A9110 24050032 */  li    $a1, 50
/* 0DBB04 7F0A9114 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DBB08 7F0A9118 00403825 */  move  $a3, $v0
/* 0DBB0C 7F0A911C AFA00014 */  sw    $zero, 0x14($sp)
/* 0DBB10 7F0A9120 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBB14 7F0A9124 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DBB18 7F0A9128 0FC2A1B6 */  jal   draw_options_labels
/* 0DBB1C 7F0A912C AFA00028 */   sw    $zero, 0x28($sp)
/* 0DBB20 7F0A9130 10000051 */  b     .L7F0A9278
/* 0DBB24 7F0A9134 00408025 */   move  $s0, $v0
.L7F0A9138:
/* 0DBB28 7F0A9138 0C002C20 */  jal   joyGetButtons
/* 0DBB2C 7F0A913C 24050800 */   li    $a1, 2048
/* 0DBB30 7F0A9140 10400014 */  beqz  $v0, .L7F0A9194
/* 0DBB34 7F0A9144 00002025 */   move  $a0, $zero
/* 0DBB38 7F0A9148 3C0F8007 */  lui   $t7, %hi(g_CurrentPlayer) # $t7, 0x8007
/* 0DBB3C 7F0A914C 8DEF8BC0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0DBB40 7F0A9150 3C0A8004 */  lui   $t2, %hi(D_800409DC+14)
/* 0DBB44 7F0A9154 3401AC05 */  li    $at, 44037
/* 0DBB48 7F0A9158 8DF82A50 */  lw    $t8, 0x2a50($t7)
/* 0DBB4C 7F0A915C 8FA700AC */  lw    $a3, 0xac($sp)
/* 0DBB50 7F0A9160 00184880 */  sll   $t1, $t8, 2
/* 0DBB54 7F0A9164 01384821 */  addu  $t1, $t1, $t8
/* 0DBB58 7F0A9168 00094880 */  sll   $t1, $t1, 2
/* 0DBB5C 7F0A916C 01495021 */  addu  $t2, $t2, $t1
/* 0DBB60 7F0A9170 954AA63A */  lhu   $t2, %lo(D_800409DC+14)($t2)
/* 0DBB64 7F0A9174 15410005 */  bne   $t2, $at, .L7F0A918C
/* 0DBB68 7F0A9178 00000000 */   nop
/* 0DBB6C 7F0A917C 0FC304AE */  jal   langGet
/* 0DBB70 7F0A9180 3404AC30 */   li    $a0, 44080
/* 0DBB74 7F0A9184 10000001 */  b     .L7F0A918C
/* 0DBB78 7F0A9188 00403825 */   move  $a3, $v0
.L7F0A918C:
/* 0DBB7C 7F0A918C 10000027 */  b     .L7F0A922C
/* 0DBB80 7F0A9190 AFA70058 */   sw    $a3, 0x58($sp)
.L7F0A9194:
/* 0DBB84 7F0A9194 0C002C20 */  jal   joyGetButtons
/* 0DBB88 7F0A9198 24050400 */   li    $a1, 1024
/* 0DBB8C 7F0A919C 10400014 */  beqz  $v0, .L7F0A91F0
/* 0DBB90 7F0A91A0 00002025 */   move  $a0, $zero
/* 0DBB94 7F0A91A4 3C0B8007 */  lui   $t3, %hi(g_CurrentPlayer) # $t3, 0x8007
/* 0DBB98 7F0A91A8 8D6B8BC0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DBB9C 7F0A91AC 3C0E8004 */  lui   $t6, %hi(D_800409DC+14)
/* 0DBBA0 7F0A91B0 3401AC05 */  li    $at, 44037
/* 0DBBA4 7F0A91B4 8D6C2A50 */  lw    $t4, 0x2a50($t3)
/* 0DBBA8 7F0A91B8 8FA700A8 */  lw    $a3, 0xa8($sp)
/* 0DBBAC 7F0A91BC 000C6880 */  sll   $t5, $t4, 2
/* 0DBBB0 7F0A91C0 01AC6821 */  addu  $t5, $t5, $t4
/* 0DBBB4 7F0A91C4 000D6880 */  sll   $t5, $t5, 2
/* 0DBBB8 7F0A91C8 01CD7021 */  addu  $t6, $t6, $t5
/* 0DBBBC 7F0A91CC 95CEA63A */  lhu   $t6, %lo(D_800409DC+14)($t6)
/* 0DBBC0 7F0A91D0 15C10005 */  bne   $t6, $at, .L7F0A91E8
/* 0DBBC4 7F0A91D4 00000000 */   nop
/* 0DBBC8 7F0A91D8 0FC304AE */  jal   langGet
/* 0DBBCC 7F0A91DC 3404AC31 */   li    $a0, 44081
/* 0DBBD0 7F0A91E0 10000001 */  b     .L7F0A91E8
/* 0DBBD4 7F0A91E4 00403825 */   move  $a3, $v0
.L7F0A91E8:
/* 0DBBD8 7F0A91E8 10000010 */  b     .L7F0A922C
/* 0DBBDC 7F0A91EC AFA70058 */   sw    $a3, 0x58($sp)
.L7F0A91F0:
/* 0DBBE0 7F0A91F0 0C002C20 */  jal   joyGetButtons
/* 0DBBE4 7F0A91F4 24050200 */   li    $a1, 512
/* 0DBBE8 7F0A91F8 10400005 */  beqz  $v0, .L7F0A9210
/* 0DBBEC 7F0A91FC 00002025 */   move  $a0, $zero
/* 0DBBF0 7F0A9200 0FC304AE */  jal   langGet
/* 0DBBF4 7F0A9204 3404AC2F */   li    $a0, 44079
/* 0DBBF8 7F0A9208 10000008 */  b     .L7F0A922C
/* 0DBBFC 7F0A920C AFA20058 */   sw    $v0, 0x58($sp)
.L7F0A9210:
/* 0DBC00 7F0A9210 0C002C20 */  jal   joyGetButtons
/* 0DBC04 7F0A9214 24050100 */   li    $a1, 256
/* 0DBC08 7F0A9218 50400005 */  beql  $v0, $zero, .L7F0A9230
/* 0DBC0C 7F0A921C 3C0F0070 */   lui   $t7, 0x70
/* 0DBC10 7F0A9220 0FC304AE */  jal   langGet
/* 0DBC14 7F0A9224 3404AC2E */   li    $a0, 44078
/* 0DBC18 7F0A9228 AFA20058 */  sw    $v0, 0x58($sp)
.L7F0A922C:
/* 0DBC1C 7F0A922C 3C0F0070 */  lui   $t7, (0x007000A0 >> 16) # lui $t7, 0x70
.L7F0A9230:
/* 0DBC20 7F0A9230 3C180030 */  lui   $t8, (0x003000B0 >> 16) # lui $t8, 0x30
/* 0DBC24 7F0A9234 371800B0 */  ori   $t8, (0x003000B0 & 0xFFFF) # ori $t8, $t8, 0xb0
/* 0DBC28 7F0A9238 35EF00A0 */  ori   $t7, (0x007000A0 & 0xFFFF) # ori $t7, $t7, 0xa0
/* 0DBC2C 7F0A923C 2408FFFF */  li    $t0, -1
/* 0DBC30 7F0A9240 24190001 */  li    $t9, 1
/* 0DBC34 7F0A9244 8FA70058 */  lw    $a3, 0x58($sp)
/* 0DBC38 7F0A9248 AFB90014 */  sw    $t9, 0x14($sp)
/* 0DBC3C 7F0A924C AFA80010 */  sw    $t0, 0x10($sp)
/* 0DBC40 7F0A9250 AFAF0018 */  sw    $t7, 0x18($sp)
/* 0DBC44 7F0A9254 AFB80024 */  sw    $t8, 0x24($sp)
/* 0DBC48 7F0A9258 02002025 */  move  $a0, $s0
/* 0DBC4C 7F0A925C 24050032 */  li    $a1, 50
/* 0DBC50 7F0A9260 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DBC54 7F0A9264 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBC58 7F0A9268 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DBC5C 7F0A926C 0FC2A1B6 */  jal   draw_options_labels
/* 0DBC60 7F0A9270 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DBC64 7F0A9274 00408025 */  move  $s0, $v0
.L7F0A9278:
/* 0DBC68 7F0A9278 8FA9004C */  lw    $t1, 0x4c($sp)
/* 0DBC6C 7F0A927C 3C0B8007 */  lui   $t3, %hi(g_CurrentPlayer) # $t3, 0x8007
/* 0DBC70 7F0A9280 8D6B8BC0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DBC74 7F0A9284 252A001E */  addiu $t2, $t1, 0x1e
/* 0DBC78 7F0A9288 AFAA004C */  sw    $t2, 0x4c($sp)
/* 0DBC7C 7F0A928C 8D6C2A50 */  lw    $t4, 0x2a50($t3)
/* 0DBC80 7F0A9290 3C048004 */  lui   $a0, %hi(D_800409DC+16)
/* 0DBC84 7F0A9294 000C6880 */  sll   $t5, $t4, 2
/* 0DBC88 7F0A9298 01AC6821 */  addu  $t5, $t5, $t4
/* 0DBC8C 7F0A929C 000D6880 */  sll   $t5, $t5, 2
/* 0DBC90 7F0A92A0 008D2021 */  addu  $a0, $a0, $t5
/* 0DBC94 7F0A92A4 0FC304AE */  jal   langGet
/* 0DBC98 7F0A92A8 9484A63C */   lhu   $a0, %lo(D_800409DC+16)($a0)
/* 0DBC9C 7F0A92AC 3C0E00AA */  lui   $t6, (0x00AA00B0 >> 16) # lui $t6, 0xaa
/* 0DBCA0 7F0A92B0 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DBCA4 7F0A92B4 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DBCA8 7F0A92B8 35CE00B0 */  ori   $t6, (0x00AA00B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DBCAC 7F0A92BC 2408FFFF */  li    $t0, -1
/* 0DBCB0 7F0A92C0 AFA80018 */  sw    $t0, 0x18($sp)
/* 0DBCB4 7F0A92C4 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DBCB8 7F0A92C8 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DBCBC 7F0A92CC 02002025 */  move  $a0, $s0
/* 0DBCC0 7F0A92D0 24050032 */  li    $a1, 50
/* 0DBCC4 7F0A92D4 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DBCC8 7F0A92D8 00403825 */  move  $a3, $v0
/* 0DBCCC 7F0A92DC AFA00014 */  sw    $zero, 0x14($sp)
/* 0DBCD0 7F0A92E0 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBCD4 7F0A92E4 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DBCD8 7F0A92E8 0FC2A1B6 */  jal   draw_options_labels
/* 0DBCDC 7F0A92EC AFA00028 */   sw    $zero, 0x28($sp)
/* 0DBCE0 7F0A92F0 8FAF004C */  lw    $t7, 0x4c($sp)
/* 0DBCE4 7F0A92F4 00408025 */  move  $s0, $v0
/* 0DBCE8 7F0A92F8 00002025 */  move  $a0, $zero
/* 0DBCEC 7F0A92FC 25F8001E */  addiu $t8, $t7, 0x1e
/* 0DBCF0 7F0A9300 AFB8004C */  sw    $t8, 0x4c($sp)
/* 0DBCF4 7F0A9304 0C002C20 */  jal   joyGetButtons
/* 0DBCF8 7F0A9308 24052000 */   li    $a1, 8192
/* 0DBCFC 7F0A930C 1040002D */  beqz  $v0, .L7F0A93C4
/* 0DBD00 7F0A9310 3C0B8007 */   lui   $t3, %hi(g_CurrentPlayer)
/* 0DBD04 7F0A9314 3C098007 */  lui   $t1, %hi(g_CurrentPlayer) # $t1, 0x8007
/* 0DBD08 7F0A9318 8D298BC0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DBD0C 7F0A931C 3C048004 */  lui   $a0, %hi(D_800409DC+0x6)
/* 0DBD10 7F0A9320 8D2A2A50 */  lw    $t2, 0x2a50($t1)
/* 0DBD14 7F0A9324 000A5880 */  sll   $t3, $t2, 2
/* 0DBD18 7F0A9328 016A5821 */  addu  $t3, $t3, $t2
/* 0DBD1C 7F0A932C 000B5880 */  sll   $t3, $t3, 2
/* 0DBD20 7F0A9330 008B2021 */  addu  $a0, $a0, $t3
/* 0DBD24 7F0A9334 0FC304AE */  jal   langGet
/* 0DBD28 7F0A9338 9484A632 */   lhu   $a0, %lo(D_800409DC+0x6)($a0)
/* 0DBD2C 7F0A933C 3C0E0070 */  lui   $t6, (0x007000A0 >> 16) # lui $t6, 0x70
/* 0DBD30 7F0A9340 3C080030 */  lui   $t0, (0x003000B0 >> 16) # lui $t0, 0x30
/* 0DBD34 7F0A9344 350800B0 */  ori   $t0, (0x003000B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DBD38 7F0A9348 35CE00A0 */  ori   $t6, (0x007000A0 & 0xFFFF) # ori $t6, $t6, 0xa0
/* 0DBD3C 7F0A934C 240CFFFF */  li    $t4, -1
/* 0DBD40 7F0A9350 240D0001 */  li    $t5, 1
/* 0DBD44 7F0A9354 AFAD0014 */  sw    $t5, 0x14($sp)
/* 0DBD48 7F0A9358 AFAC0010 */  sw    $t4, 0x10($sp)
/* 0DBD4C 7F0A935C AFAE0018 */  sw    $t6, 0x18($sp)
/* 0DBD50 7F0A9360 AFA80024 */  sw    $t0, 0x24($sp)
/* 0DBD54 7F0A9364 02002025 */  move  $a0, $s0
/* 0DBD58 7F0A9368 24050032 */  li    $a1, 50
/* 0DBD5C 7F0A936C 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DBD60 7F0A9370 00403825 */  move  $a3, $v0
/* 0DBD64 7F0A9374 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBD68 7F0A9378 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DBD6C 7F0A937C 0FC2A1B6 */  jal   draw_options_labels
/* 0DBD70 7F0A9380 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DBD74 7F0A9384 3C198007 */  lui   $t9, %hi(g_CurrentPlayer) # $t9, 0x8007
/* 0DBD78 7F0A9388 8F398BC0 */  lw    $t9, %lo(g_CurrentPlayer)($t9)
/* 0DBD7C 7F0A938C 3C098004 */  lui   $t1, %hi(D_800409DC+6)
/* 0DBD80 7F0A9390 3401AC01 */  li    $at, 44033
/* 0DBD84 7F0A9394 8F2F2A50 */  lw    $t7, 0x2a50($t9)
/* 0DBD88 7F0A9398 00408025 */  move  $s0, $v0
/* 0DBD8C 7F0A939C 240A0001 */  li    $t2, 1
/* 0DBD90 7F0A93A0 000FC080 */  sll   $t8, $t7, 2
/* 0DBD94 7F0A93A4 030FC021 */  addu  $t8, $t8, $t7
/* 0DBD98 7F0A93A8 0018C080 */  sll   $t8, $t8, 2
/* 0DBD9C 7F0A93AC 01384821 */  addu  $t1, $t1, $t8
/* 0DBDA0 7F0A93B0 9529A632 */  lhu   $t1, %lo(D_800409DC+6)($t1)
/* 0DBDA4 7F0A93B4 5521001F */  bnel  $t1, $at, .Leu7F0A9434
/* 0DBDA8 7F0A93B8 8FAF004C */   lw    $t7, 0x4c($sp)
/* 0DBDAC 7F0A93BC 1000001C */  b     .L7F0A9430
/* 0DBDB0 7F0A93C0 AFAA0050 */   sw    $t2, 0x50($sp)
.L7F0A93C4:
/* 0DBDB4 7F0A93C4 8D6B8BC0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DBDB8 7F0A93C8 3C048004 */  lui   $a0, %hi(D_800409DC+6)
/* 0DBDBC 7F0A93CC 8D6C2A50 */  lw    $t4, 0x2a50($t3)
/* 0DBDC0 7F0A93D0 000C6880 */  sll   $t5, $t4, 2
/* 0DBDC4 7F0A93D4 01AC6821 */  addu  $t5, $t5, $t4
/* 0DBDC8 7F0A93D8 000D6880 */  sll   $t5, $t5, 2
/* 0DBDCC 7F0A93DC 008D2021 */  addu  $a0, $a0, $t5
/* 0DBDD0 7F0A93E0 0FC304AE */  jal   langGet
/* 0DBDD4 7F0A93E4 9484A632 */   lhu   $a0, %lo(D_800409DC+6)($a0)
/* 0DBDD8 7F0A93E8 3C0E00AA */  lui   $t6, (0x00AA00B0 >> 16) # lui $t6, 0xaa
/* 0DBDDC 7F0A93EC 3C190030 */  lui   $t9, (0x003000B0 >> 16) # lui $t9, 0x30
/* 0DBDE0 7F0A93F0 373900B0 */  ori   $t9, (0x003000B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DBDE4 7F0A93F4 35CE00B0 */  ori   $t6, (0x00AA00B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DBDE8 7F0A93F8 2408FFFF */  li    $t0, -1
/* 0DBDEC 7F0A93FC AFA80018 */  sw    $t0, 0x18($sp)
/* 0DBDF0 7F0A9400 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DBDF4 7F0A9404 AFB90024 */  sw    $t9, 0x24($sp)
/* 0DBDF8 7F0A9408 02002025 */  move  $a0, $s0
/* 0DBDFC 7F0A940C 24050032 */  li    $a1, 50
/* 0DBE00 7F0A9410 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DBE04 7F0A9414 00403825 */  move  $a3, $v0
/* 0DBE08 7F0A9418 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DBE0C 7F0A941C AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBE10 7F0A9420 AFA00020 */  sw    $zero, 0x20($sp)
/* 0DBE14 7F0A9424 0FC2A1B6 */  jal   draw_options_labels
/* 0DBE18 7F0A9428 AFA00028 */   sw    $zero, 0x28($sp)
/* 0DBE1C 7F0A942C 00408025 */  move  $s0, $v0
.L7F0A9430:
/* 0DBE20 7F0A9430 8FAF004C */  lw    $t7, 0x4c($sp)
.Leu7F0A9434:
/* 0DBE24 7F0A9434 00002025 */  move  $a0, $zero
/* 0DBE28 7F0A9438 24050010 */  li    $a1, 16
/* 0DBE2C 7F0A943C 25F8FFA6 */  addiu $t8, $t7, -0x5a
/* 0DBE30 7F0A9440 0C002C20 */  jal   joyGetButtons
/* 0DBE34 7F0A9444 AFB8004C */   sw    $t8, 0x4c($sp)
/* 0DBE38 7F0A9448 1040002E */  beqz  $v0, .L7F0A9504
/* 0DBE3C 7F0A944C 3C0C8007 */   lui   $t4, %hi(g_CurrentPlayer) # $t4, 0x8007
/* 0DBE40 7F0A9450 3C098007 */  lui   $t1, %hi(g_CurrentPlayer) # $t1, 0x8007
/* 0DBE44 7F0A9454 8D298BC0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DBE48 7F0A9458 3C048004 */  lui   $a0, %hi(D_800409DC+10)
/* 0DBE4C 7F0A945C 8D2A2A50 */  lw    $t2, 0x2a50($t1)
/* 0DBE50 7F0A9460 000A5880 */  sll   $t3, $t2, 2
/* 0DBE54 7F0A9464 016A5821 */  addu  $t3, $t3, $t2
/* 0DBE58 7F0A9468 000B5880 */  sll   $t3, $t3, 2
/* 0DBE5C 7F0A946C 008B2021 */  addu  $a0, $a0, $t3
/* 0DBE60 7F0A9470 0FC304AE */  jal   langGet
/* 0DBE64 7F0A9474 9484A636 */   lhu   $a0, %lo(D_800409DC+10)($a0)
/* 0DBE68 7F0A9478 3C0E0070 */  lui   $t6, (0x007000A0 >> 16) # lui $t6, 0x70
/* 0DBE6C 7F0A947C 3C080030 */  lui   $t0, (0x003000B0 >> 16) # lui $t0, 0x30
/* 0DBE70 7F0A9480 350800B0 */  ori   $t0, (0x003000B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DBE74 7F0A9484 35CE00A0 */  ori   $t6, (0x007000A0 & 0xFFFF) # ori $t6, $t6, 0xa0
/* 0DBE78 7F0A9488 240CFFFF */  li    $t4, -1
/* 0DBE7C 7F0A948C 240D0001 */  li    $t5, 1
/* 0DBE80 7F0A9490 24190001 */  li    $t9, 1
/* 0DBE84 7F0A9494 AFB90028 */  sw    $t9, 0x28($sp)
/* 0DBE88 7F0A9498 AFAD0014 */  sw    $t5, 0x14($sp)
/* 0DBE8C 7F0A949C AFAC0010 */  sw    $t4, 0x10($sp)
/* 0DBE90 7F0A94A0 AFAE0018 */  sw    $t6, 0x18($sp)
/* 0DBE94 7F0A94A4 AFA80024 */  sw    $t0, 0x24($sp)
/* 0DBE98 7F0A94A8 02002025 */  move  $a0, $s0
/* 0DBE9C 7F0A94AC 2405010E */  li    $a1, 270
/* 0DBEA0 7F0A94B0 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DBEA4 7F0A94B4 00403825 */  move  $a3, $v0
/* 0DBEA8 7F0A94B8 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBEAC 7F0A94BC 0FC2A1B6 */  jal   draw_options_labels
/* 0DBEB0 7F0A94C0 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DBEB4 7F0A94C4 3C0F8007 */  lui   $t7, %hi(g_CurrentPlayer) # $t7, 0x8007
/* 0DBEB8 7F0A94C8 8DEF8BC0 */  lw    $t7, %lo(g_CurrentPlayer)($t7)
/* 0DBEBC 7F0A94CC 3C0A8004 */  lui   $t2, %hi(D_800409DC+10)
/* 0DBEC0 7F0A94D0 3401AC01 */  li    $at, 44033
/* 0DBEC4 7F0A94D4 8DF82A50 */  lw    $t8, 0x2a50($t7)
/* 0DBEC8 7F0A94D8 00408025 */  move  $s0, $v0
/* 0DBECC 7F0A94DC 240B0001 */  li    $t3, 1
/* 0DBED0 7F0A94E0 00184880 */  sll   $t1, $t8, 2
/* 0DBED4 7F0A94E4 01384821 */  addu  $t1, $t1, $t8
/* 0DBED8 7F0A94E8 00094880 */  sll   $t1, $t1, 2
/* 0DBEDC 7F0A94EC 01495021 */  addu  $t2, $t2, $t1
/* 0DBEE0 7F0A94F0 954AA636 */  lhu   $t2, %lo(D_800409DC+10)($t2)
/* 0DBEE4 7F0A94F4 55410020 */  bnel  $t2, $at, .L7F0A9578
/* 0DBEE8 7F0A94F8 8FA9004C */   lw    $t1, 0x4c($sp)
/* 0DBEEC 7F0A94FC 1000001D */  b     .L7F0A9574
/* 0DBEF0 7F0A9500 AFAB0050 */   sw    $t3, 0x50($sp)
.L7F0A9504:
/* 0DBEF4 7F0A9504 8D8C8BC0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DBEF8 7F0A9508 3C048004 */  lui   $a0, %hi(D_800409DC+10)
/* 0DBEFC 7F0A950C 8D8D2A50 */  lw    $t5, 0x2a50($t4)
/* 0DBF00 7F0A9510 000D7080 */  sll   $t6, $t5, 2
/* 0DBF04 7F0A9514 01CD7021 */  addu  $t6, $t6, $t5
/* 0DBF08 7F0A9518 000E7080 */  sll   $t6, $t6, 2
/* 0DBF0C 7F0A951C 008E2021 */  addu  $a0, $a0, $t6
/* 0DBF10 7F0A9520 0FC304AE */  jal   langGet
/* 0DBF14 7F0A9524 9484A636 */   lhu   $a0, %lo(D_800409DC+10)($a0)
/* 0DBF18 7F0A9528 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DBF1C 7F0A952C 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DBF20 7F0A9530 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DBF24 7F0A9534 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DBF28 7F0A9538 2419FFFF */  li    $t9, -1
/* 0DBF2C 7F0A953C 24180001 */  li    $t8, 1
/* 0DBF30 7F0A9540 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DBF34 7F0A9544 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DBF38 7F0A9548 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DBF3C 7F0A954C AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DBF40 7F0A9550 02002025 */  move  $a0, $s0
/* 0DBF44 7F0A9554 2405010E */  li    $a1, 270
/* 0DBF48 7F0A9558 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DBF4C 7F0A955C 00403825 */  move  $a3, $v0
/* 0DBF50 7F0A9560 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DBF54 7F0A9564 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DBF58 7F0A9568 0FC2A1B6 */  jal   draw_options_labels
/* 0DBF5C 7F0A956C AFA00020 */   sw    $zero, 0x20($sp)
/* 0DBF60 7F0A9570 00408025 */  move  $s0, $v0
.L7F0A9574:
/* 0DBF64 7F0A9574 8FA9004C */  lw    $t1, 0x4c($sp)
.L7F0A9578:
/* 0DBF68 7F0A9578 3C0B8004 */  lui   $t3, %hi(controller_options_index) # $t3, 0x8004
/* 0DBF6C 7F0A957C 8D6BA5E8 */  lw    $t3, %lo(controller_options_index)($t3)
/* 0DBF70 7F0A9580 24010001 */  li    $at, 1
/* 0DBF74 7F0A9584 252A001E */  addiu $t2, $t1, 0x1e
/* 0DBF78 7F0A9588 1561000A */  bne   $t3, $at, .L7F0A95B4
/* 0DBF7C 7F0A958C AFAA004C */   sw    $t2, 0x4c($sp)
/* 0DBF80 7F0A9590 3C0C8004 */  lui   $t4, %hi(watch_item_is_actively_selected) # $t4, 0x8004
/* 0DBF84 7F0A9594 8D8CA5F8 */  lw    $t4, %lo(watch_item_is_actively_selected)($t4)
/* 0DBF88 7F0A9598 00002025 */  move  $a0, $zero
/* 0DBF8C 7F0A959C 11800005 */  beqz  $t4, .L7F0A95B4
/* 0DBF90 7F0A95A0 00000000 */   nop
/* 0DBF94 7F0A95A4 0C002C20 */  jal   joyGetButtons
/* 0DBF98 7F0A95A8 2405000F */   li    $a1, 15
/* 0DBF9C 7F0A95AC 1440001F */  bnez  $v0, .L7F0A962C
/* 0DBFA0 7F0A95B0 00002025 */   move  $a0, $zero
.L7F0A95B4:
/* 0DBFA4 7F0A95B4 3C0D8007 */  lui   $t5, %hi(g_CurrentPlayer) # $t5, 0x8007
/* 0DBFA8 7F0A95B8 8DAD8BC0 */  lw    $t5, %lo(g_CurrentPlayer)($t5)
/* 0DBFAC 7F0A95BC 3C048004 */  lui   $a0, %hi(D_800409DC+12)
/* 0DBFB0 7F0A95C0 8DAE2A50 */  lw    $t6, 0x2a50($t5)
/* 0DBFB4 7F0A95C4 000E4080 */  sll   $t0, $t6, 2
/* 0DBFB8 7F0A95C8 010E4021 */  addu  $t0, $t0, $t6
/* 0DBFBC 7F0A95CC 00084080 */  sll   $t0, $t0, 2
/* 0DBFC0 7F0A95D0 00882021 */  addu  $a0, $a0, $t0
/* 0DBFC4 7F0A95D4 0FC304AE */  jal   langGet
/* 0DBFC8 7F0A95D8 9484A638 */   lhu   $a0, %lo(D_800409DC+12)($a0)
/* 0DBFCC 7F0A95DC 3C1900AA */  lui   $t9, (0x00AA00B0 >> 16) # lui $t9, 0xaa
/* 0DBFD0 7F0A95E0 3C180030 */  lui   $t8, (0x003000B0 >> 16) # lui $t8, 0x30
/* 0DBFD4 7F0A95E4 371800B0 */  ori   $t8, (0x003000B0 & 0xFFFF) # ori $t8, $t8, 0xb0
/* 0DBFD8 7F0A95E8 373900B0 */  ori   $t9, (0x00AA00B0 & 0xFFFF) # ori $t9, $t9, 0xb0
/* 0DBFDC 7F0A95EC 240FFFFF */  li    $t7, -1
/* 0DBFE0 7F0A95F0 24090001 */  li    $t1, 1
/* 0DBFE4 7F0A95F4 AFA90028 */  sw    $t1, 0x28($sp)
/* 0DBFE8 7F0A95F8 AFAF0018 */  sw    $t7, 0x18($sp)
/* 0DBFEC 7F0A95FC AFB90010 */  sw    $t9, 0x10($sp)
/* 0DBFF0 7F0A9600 AFB80024 */  sw    $t8, 0x24($sp)
/* 0DBFF4 7F0A9604 02002025 */  move  $a0, $s0
/* 0DBFF8 7F0A9608 2405010E */  li    $a1, 270
/* 0DBFFC 7F0A960C 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DC000 7F0A9610 00403825 */  move  $a3, $v0
/* 0DC004 7F0A9614 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DC008 7F0A9618 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC00C 7F0A961C 0FC2A1B6 */  jal   draw_options_labels
/* 0DC010 7F0A9620 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC014 7F0A9624 10000076 */  b     .L7F0A9800
/* 0DC018 7F0A9628 00408025 */   move  $s0, $v0
.L7F0A962C:
/* 0DC01C 7F0A962C 0C002C20 */  jal   joyGetButtons
/* 0DC020 7F0A9630 2405000F */   li    $a1, 15
/* 0DC024 7F0A9634 244AFFFF */  addiu $t2, $v0, -1
/* 0DC028 7F0A9638 004A5824 */  and   $t3, $v0, $t2
/* 0DC02C 7F0A963C 15600054 */  bnez  $t3, .L7F0A9790
/* 0DC030 7F0A9640 3C088007 */   lui   $t0, %hi(g_CurrentPlayer)
/* 0DC034 7F0A9644 00002025 */  move  $a0, $zero
/* 0DC038 7F0A9648 0C002C20 */  jal   joyGetButtons
/* 0DC03C 7F0A964C 24050008 */   li    $a1, 8
/* 0DC040 7F0A9650 10400014 */  beqz  $v0, .L7F0A96A4
/* 0DC044 7F0A9654 00002025 */   move  $a0, $zero
/* 0DC048 7F0A9658 3C0C8007 */  lui   $t4, %hi(g_CurrentPlayer) # $t4, 0x8007
/* 0DC04C 7F0A965C 8D8C8BC0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DC050 7F0A9660 3C088004 */  lui   $t0, %hi(D_800409DC+12)
/* 0DC054 7F0A9664 3401AC05 */  li    $at, 44037
/* 0DC058 7F0A9668 8D8D2A50 */  lw    $t5, 0x2a50($t4)
/* 0DC05C 7F0A966C 8FA700AC */  lw    $a3, 0xac($sp)
/* 0DC060 7F0A9670 000D7080 */  sll   $t6, $t5, 2
/* 0DC064 7F0A9674 01CD7021 */  addu  $t6, $t6, $t5
/* 0DC068 7F0A9678 000E7080 */  sll   $t6, $t6, 2
/* 0DC06C 7F0A967C 010E4021 */  addu  $t0, $t0, $t6
/* 0DC070 7F0A9680 9508A638 */  lhu   $t0, %lo(D_800409DC+12)($t0)
/* 0DC074 7F0A9684 15010005 */  bne   $t0, $at, .L7F0A969C
/* 0DC078 7F0A9688 00000000 */   nop
/* 0DC07C 7F0A968C 0FC304AE */  jal   langGet
/* 0DC080 7F0A9690 3404AC30 */   li    $a0, 44080
/* 0DC084 7F0A9694 10000001 */  b     .L7F0A969C
/* 0DC088 7F0A9698 00403825 */   move  $a3, $v0
.L7F0A969C:
/* 0DC08C 7F0A969C 10000027 */  b     .L7F0A973C
/* 0DC090 7F0A96A0 AFA7005C */   sw    $a3, 0x5c($sp)
.L7F0A96A4:
/* 0DC094 7F0A96A4 0C002C20 */  jal   joyGetButtons
/* 0DC098 7F0A96A8 24050004 */   li    $a1, 4
/* 0DC09C 7F0A96AC 10400014 */  beqz  $v0, .L7F0A9700
/* 0DC0A0 7F0A96B0 00002025 */   move  $a0, $zero
/* 0DC0A4 7F0A96B4 3C198007 */  lui   $t9, %hi(g_CurrentPlayer) # $t9, 0x8007
/* 0DC0A8 7F0A96B8 8F398BC0 */  lw    $t9, %lo(g_CurrentPlayer)($t9)
/* 0DC0AC 7F0A96BC 3C098004 */  lui   $t1, %hi(D_800409DC+12)
/* 0DC0B0 7F0A96C0 3401AC05 */  li    $at, 44037
/* 0DC0B4 7F0A96C4 8F2F2A50 */  lw    $t7, 0x2a50($t9)
/* 0DC0B8 7F0A96C8 8FA700A8 */  lw    $a3, 0xa8($sp)
/* 0DC0BC 7F0A96CC 000FC080 */  sll   $t8, $t7, 2
/* 0DC0C0 7F0A96D0 030FC021 */  addu  $t8, $t8, $t7
/* 0DC0C4 7F0A96D4 0018C080 */  sll   $t8, $t8, 2
/* 0DC0C8 7F0A96D8 01384821 */  addu  $t1, $t1, $t8
/* 0DC0CC 7F0A96DC 9529A638 */  lhu   $t1, %lo(D_800409DC+12)($t1)
/* 0DC0D0 7F0A96E0 15210005 */  bne   $t1, $at, .L7F0A96F8
/* 0DC0D4 7F0A96E4 00000000 */   nop
/* 0DC0D8 7F0A96E8 0FC304AE */  jal   langGet
/* 0DC0DC 7F0A96EC 3404AC31 */   li    $a0, 44081
/* 0DC0E0 7F0A96F0 10000001 */  b     .L7F0A96F8
/* 0DC0E4 7F0A96F4 00403825 */   move  $a3, $v0
.L7F0A96F8:
/* 0DC0E8 7F0A96F8 10000010 */  b     .L7F0A973C
/* 0DC0EC 7F0A96FC AFA7005C */   sw    $a3, 0x5c($sp)
.L7F0A9700:
/* 0DC0F0 7F0A9700 0C002C20 */  jal   joyGetButtons
/* 0DC0F4 7F0A9704 24050002 */   li    $a1, 2
/* 0DC0F8 7F0A9708 10400005 */  beqz  $v0, .L7F0A9720
/* 0DC0FC 7F0A970C 00002025 */   move  $a0, $zero
/* 0DC100 7F0A9710 0FC304AE */  jal   langGet
/* 0DC104 7F0A9714 3404AC2F */   li    $a0, 44079
/* 0DC108 7F0A9718 10000008 */  b     .L7F0A973C
/* 0DC10C 7F0A971C AFA2005C */   sw    $v0, 0x5c($sp)
.L7F0A9720:
/* 0DC110 7F0A9720 0C002C20 */  jal   joyGetButtons
/* 0DC114 7F0A9724 24050001 */   li    $a1, 1
/* 0DC118 7F0A9728 50400005 */  beql  $v0, $zero, .L7F0A9740
/* 0DC11C 7F0A972C 3C0C0070 */   lui   $t4, 0x70
/* 0DC120 7F0A9730 0FC304AE */  jal   langGet
/* 0DC124 7F0A9734 3404AC2E */   li    $a0, 44078
/* 0DC128 7F0A9738 AFA2005C */  sw    $v0, 0x5c($sp)
.L7F0A973C:
/* 0DC12C 7F0A973C 3C0C0070 */  lui   $t4, (0x007000A0 >> 16) # lui $t4, 0x70
.L7F0A9740:
/* 0DC130 7F0A9740 3C0D0030 */  lui   $t5, (0x003000B0 >> 16) # lui $t5, 0x30
/* 0DC134 7F0A9744 35AD00B0 */  ori   $t5, (0x003000B0 & 0xFFFF) # ori $t5, $t5, 0xb0
/* 0DC138 7F0A9748 358C00A0 */  ori   $t4, (0x007000A0 & 0xFFFF) # ori $t4, $t4, 0xa0
/* 0DC13C 7F0A974C 240AFFFF */  li    $t2, -1
/* 0DC140 7F0A9750 240B0001 */  li    $t3, 1
/* 0DC144 7F0A9754 240E0001 */  li    $t6, 1
/* 0DC148 7F0A9758 8FA7005C */  lw    $a3, 0x5c($sp)
/* 0DC14C 7F0A975C AFAE0028 */  sw    $t6, 0x28($sp)
/* 0DC150 7F0A9760 AFAB0014 */  sw    $t3, 0x14($sp)
/* 0DC154 7F0A9764 AFAA0010 */  sw    $t2, 0x10($sp)
/* 0DC158 7F0A9768 AFAC0018 */  sw    $t4, 0x18($sp)
/* 0DC15C 7F0A976C AFAD0024 */  sw    $t5, 0x24($sp)
/* 0DC160 7F0A9770 02002025 */  move  $a0, $s0
/* 0DC164 7F0A9774 2405010E */  li    $a1, 270
/* 0DC168 7F0A9778 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DC16C 7F0A977C AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC170 7F0A9780 0FC2A1B6 */  jal   draw_options_labels
/* 0DC174 7F0A9784 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC178 7F0A9788 1000001D */  b     .L7F0A9800
/* 0DC17C 7F0A978C 00408025 */   move  $s0, $v0
.L7F0A9790:
/* 0DC180 7F0A9790 8D088BC0 */  lw    $t0, %lo(g_CurrentPlayer)($t0)
/* 0DC184 7F0A9794 3C048004 */  lui   $a0, %hi(D_800409DC+12)
/* 0DC188 7F0A9798 8D192A50 */  lw    $t9, 0x2a50($t0)
/* 0DC18C 7F0A979C 00197880 */  sll   $t7, $t9, 2
/* 0DC190 7F0A97A0 01F97821 */  addu  $t7, $t7, $t9
/* 0DC194 7F0A97A4 000F7880 */  sll   $t7, $t7, 2
/* 0DC198 7F0A97A8 008F2021 */  addu  $a0, $a0, $t7
/* 0DC19C 7F0A97AC 0FC304AE */  jal   langGet
/* 0DC1A0 7F0A97B0 9484A638 */   lhu   $a0, %lo(D_800409DC+12)($a0)
/* 0DC1A4 7F0A97B4 3C1800AA */  lui   $t8, (0x00AA00B0 >> 16) # lui $t8, 0xaa
/* 0DC1A8 7F0A97B8 3C0A0030 */  lui   $t2, (0x003000B0 >> 16) # lui $t2, 0x30
/* 0DC1AC 7F0A97BC 354A00B0 */  ori   $t2, (0x003000B0 & 0xFFFF) # ori $t2, $t2, 0xb0
/* 0DC1B0 7F0A97C0 371800B0 */  ori   $t8, (0x00AA00B0 & 0xFFFF) # ori $t8, $t8, 0xb0
/* 0DC1B4 7F0A97C4 2409FFFF */  li    $t1, -1
/* 0DC1B8 7F0A97C8 240B0001 */  li    $t3, 1
/* 0DC1BC 7F0A97CC AFAB0028 */  sw    $t3, 0x28($sp)
/* 0DC1C0 7F0A97D0 AFA90018 */  sw    $t1, 0x18($sp)
/* 0DC1C4 7F0A97D4 AFB80010 */  sw    $t8, 0x10($sp)
/* 0DC1C8 7F0A97D8 AFAA0024 */  sw    $t2, 0x24($sp)
/* 0DC1CC 7F0A97DC 02002025 */  move  $a0, $s0
/* 0DC1D0 7F0A97E0 2405010E */  li    $a1, 270
/* 0DC1D4 7F0A97E4 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DC1D8 7F0A97E8 00403825 */  move  $a3, $v0
/* 0DC1DC 7F0A97EC AFA00014 */  sw    $zero, 0x14($sp)
/* 0DC1E0 7F0A97F0 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC1E4 7F0A97F4 0FC2A1B6 */  jal   draw_options_labels
/* 0DC1E8 7F0A97F8 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC1EC 7F0A97FC 00408025 */  move  $s0, $v0
.L7F0A9800:
/* 0DC1F0 7F0A9800 8FAC004C */  lw    $t4, 0x4c($sp)
/* 0DC1F4 7F0A9804 00002025 */  move  $a0, $zero
/* 0DC1F8 7F0A9808 24054000 */  li    $a1, 16384
/* 0DC1FC 7F0A980C 258D001E */  addiu $t5, $t4, 0x1e
/* 0DC200 7F0A9810 0C002C20 */  jal   joyGetButtons
/* 0DC204 7F0A9814 AFAD004C */   sw    $t5, 0x4c($sp)
/* 0DC208 7F0A9818 10400020 */  beqz  $v0, .L7F0A989C
/* 0DC20C 7F0A981C 3C0C8007 */   lui   $t4, %hi(g_CurrentPlayer) # $t4, 0x8007
/* 0DC210 7F0A9820 3C0E8007 */  lui   $t6, %hi(g_CurrentPlayer) # $t6, 0x8007
/* 0DC214 7F0A9824 8DCE8BC0 */  lw    $t6, %lo(g_CurrentPlayer)($t6)
/* 0DC218 7F0A9828 3C048004 */  lui   $a0, %hi(D_800409DC+4)
/* 0DC21C 7F0A982C 8DC82A50 */  lw    $t0, 0x2a50($t6)
/* 0DC220 7F0A9830 0008C880 */  sll   $t9, $t0, 2
/* 0DC224 7F0A9834 0328C821 */  addu  $t9, $t9, $t0
/* 0DC228 7F0A9838 0019C880 */  sll   $t9, $t9, 2
/* 0DC22C 7F0A983C 00992021 */  addu  $a0, $a0, $t9
/* 0DC230 7F0A9840 0FC304AE */  jal   langGet
/* 0DC234 7F0A9844 9484A630 */   lhu   $a0, %lo(D_800409DC+4)($a0)
/* 0DC238 7F0A9848 3C090070 */  lui   $t1, (0x007000A0 >> 16) # lui $t1, 0x70
/* 0DC23C 7F0A984C 3C0A0030 */  lui   $t2, (0x003000B0 >> 16) # lui $t2, 0x30
/* 0DC240 7F0A9850 354A00B0 */  ori   $t2, (0x003000B0 & 0xFFFF) # ori $t2, $t2, 0xb0
/* 0DC244 7F0A9854 352900A0 */  ori   $t1, (0x007000A0 & 0xFFFF) # ori $t1, $t1, 0xa0
/* 0DC248 7F0A9858 240FFFFF */  li    $t7, -1
/* 0DC24C 7F0A985C 24180001 */  li    $t8, 1
/* 0DC250 7F0A9860 240B0001 */  li    $t3, 1
/* 0DC254 7F0A9864 AFAB0028 */  sw    $t3, 0x28($sp)
/* 0DC258 7F0A9868 AFB80014 */  sw    $t8, 0x14($sp)
/* 0DC25C 7F0A986C AFAF0010 */  sw    $t7, 0x10($sp)
/* 0DC260 7F0A9870 AFA90018 */  sw    $t1, 0x18($sp)
/* 0DC264 7F0A9874 AFAA0024 */  sw    $t2, 0x24($sp)
/* 0DC268 7F0A9878 02002025 */  move  $a0, $s0
/* 0DC26C 7F0A987C 2405010E */  li    $a1, 270
/* 0DC270 7F0A9880 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DC274 7F0A9884 00403825 */  move  $a3, $v0
/* 0DC278 7F0A9888 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC27C 7F0A988C 0FC2A1B6 */  jal   draw_options_labels
/* 0DC280 7F0A9890 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC284 7F0A9894 1000001D */  b     .L7F0A990C
/* 0DC288 7F0A9898 00408025 */   move  $s0, $v0
.L7F0A989C:
/* 0DC28C 7F0A989C 8D8C8BC0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DC290 7F0A98A0 3C048004 */  lui   $a0, %hi(D_800409DC+4)
/* 0DC294 7F0A98A4 8D8D2A50 */  lw    $t5, 0x2a50($t4)
/* 0DC298 7F0A98A8 000D7080 */  sll   $t6, $t5, 2
/* 0DC29C 7F0A98AC 01CD7021 */  addu  $t6, $t6, $t5
/* 0DC2A0 7F0A98B0 000E7080 */  sll   $t6, $t6, 2
/* 0DC2A4 7F0A98B4 008E2021 */  addu  $a0, $a0, $t6
/* 0DC2A8 7F0A98B8 0FC304AE */  jal   langGet
/* 0DC2AC 7F0A98BC 9484A630 */   lhu   $a0, %lo(D_800409DC+4)($a0)
/* 0DC2B0 7F0A98C0 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DC2B4 7F0A98C4 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DC2B8 7F0A98C8 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DC2BC 7F0A98CC 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DC2C0 7F0A98D0 2419FFFF */  li    $t9, -1
/* 0DC2C4 7F0A98D4 24180001 */  li    $t8, 1
/* 0DC2C8 7F0A98D8 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DC2CC 7F0A98DC AFB90018 */  sw    $t9, 0x18($sp)
/* 0DC2D0 7F0A98E0 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DC2D4 7F0A98E4 AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DC2D8 7F0A98E8 02002025 */  move  $a0, $s0
/* 0DC2DC 7F0A98EC 2405010E */  li    $a1, 270
/* 0DC2E0 7F0A98F0 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DC2E4 7F0A98F4 00403825 */  move  $a3, $v0
/* 0DC2E8 7F0A98F8 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DC2EC 7F0A98FC AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC2F0 7F0A9900 0FC2A1B6 */  jal   draw_options_labels
/* 0DC2F4 7F0A9904 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC2F8 7F0A9908 00408025 */  move  $s0, $v0
.L7F0A990C:
/* 0DC2FC 7F0A990C 8FA9004C */  lw    $t1, 0x4c($sp)
/* 0DC300 7F0A9910 00002025 */  move  $a0, $zero
/* 0DC304 7F0A9914 34058000 */  li    $a1, 32768
/* 0DC308 7F0A9918 252A001E */  addiu $t2, $t1, 0x1e
/* 0DC30C 7F0A991C 0C002C20 */  jal   joyGetButtons
/* 0DC310 7F0A9920 AFAA004C */   sw    $t2, 0x4c($sp)
/* 0DC314 7F0A9924 10400020 */  beqz  $v0, .L7F0A99A8
/* 0DC318 7F0A9928 3C098007 */   lui   $t1, %hi(g_CurrentPlayer) # $t1, 0x8007
/* 0DC31C 7F0A992C 3C0B8007 */  lui   $t3, %hi(g_CurrentPlayer) # $t3, 0x8007
/* 0DC320 7F0A9930 8D6B8BC0 */  lw    $t3, %lo(g_CurrentPlayer)($t3)
/* 0DC324 7F0A9934 3C048004 */  lui   $a0, %hi(D_800409DC+2)
/* 0DC328 7F0A9938 8D6C2A50 */  lw    $t4, 0x2a50($t3)
/* 0DC32C 7F0A993C 000C6880 */  sll   $t5, $t4, 2
/* 0DC330 7F0A9940 01AC6821 */  addu  $t5, $t5, $t4
/* 0DC334 7F0A9944 000D6880 */  sll   $t5, $t5, 2
/* 0DC338 7F0A9948 008D2021 */  addu  $a0, $a0, $t5
/* 0DC33C 7F0A994C 0FC304AE */  jal   langGet
/* 0DC340 7F0A9950 9484A62E */   lhu   $a0, %lo(D_800409DC+2)($a0)
/* 0DC344 7F0A9954 3C190070 */  lui   $t9, (0x007000A0 >> 16) # lui $t9, 0x70
/* 0DC348 7F0A9958 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DC34C 7F0A995C 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DC350 7F0A9960 373900A0 */  ori   $t9, (0x007000A0 & 0xFFFF) # ori $t9, $t9, 0xa0
/* 0DC354 7F0A9964 240EFFFF */  li    $t6, -1
/* 0DC358 7F0A9968 24080001 */  li    $t0, 1
/* 0DC35C 7F0A996C 24180001 */  li    $t8, 1
/* 0DC360 7F0A9970 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DC364 7F0A9974 AFA80014 */  sw    $t0, 0x14($sp)
/* 0DC368 7F0A9978 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0DC36C 7F0A997C AFB90018 */  sw    $t9, 0x18($sp)
/* 0DC370 7F0A9980 AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DC374 7F0A9984 02002025 */  move  $a0, $s0
/* 0DC378 7F0A9988 2405010E */  li    $a1, 270
/* 0DC37C 7F0A998C 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DC380 7F0A9990 00403825 */  move  $a3, $v0
/* 0DC384 7F0A9994 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC388 7F0A9998 0FC2A1B6 */  jal   draw_options_labels
/* 0DC38C 7F0A999C AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC390 7F0A99A0 1000001D */  b     .L7F0A9A18
/* 0DC394 7F0A99A4 00408025 */   move  $s0, $v0
.L7F0A99A8:
/* 0DC398 7F0A99A8 8D298BC0 */  lw    $t1, %lo(g_CurrentPlayer)($t1)
/* 0DC39C 7F0A99AC 3C048004 */  lui   $a0, %hi(D_800409DC+2)
/* 0DC3A0 7F0A99B0 8D2A2A50 */  lw    $t2, 0x2a50($t1)
/* 0DC3A4 7F0A99B4 000A5880 */  sll   $t3, $t2, 2
/* 0DC3A8 7F0A99B8 016A5821 */  addu  $t3, $t3, $t2
/* 0DC3AC 7F0A99BC 000B5880 */  sll   $t3, $t3, 2
/* 0DC3B0 7F0A99C0 008B2021 */  addu  $a0, $a0, $t3
/* 0DC3B4 7F0A99C4 0FC304AE */  jal   langGet
/* 0DC3B8 7F0A99C8 9484A62E */   lhu   $a0, %lo(D_800409DC+2)($a0)
/* 0DC3BC 7F0A99CC 3C0C00AA */  lui   $t4, (0x00AA00B0 >> 16) # lui $t4, 0xaa
/* 0DC3C0 7F0A99D0 3C0E0030 */  lui   $t6, (0x003000B0 >> 16) # lui $t6, 0x30
/* 0DC3C4 7F0A99D4 35CE00B0 */  ori   $t6, (0x003000B0 & 0xFFFF) # ori $t6, $t6, 0xb0
/* 0DC3C8 7F0A99D8 358C00B0 */  ori   $t4, (0x00AA00B0 & 0xFFFF) # ori $t4, $t4, 0xb0
/* 0DC3CC 7F0A99DC 240DFFFF */  li    $t5, -1
/* 0DC3D0 7F0A99E0 24080001 */  li    $t0, 1
/* 0DC3D4 7F0A99E4 AFA80028 */  sw    $t0, 0x28($sp)
/* 0DC3D8 7F0A99E8 AFAD0018 */  sw    $t5, 0x18($sp)
/* 0DC3DC 7F0A99EC AFAC0010 */  sw    $t4, 0x10($sp)
/* 0DC3E0 7F0A99F0 AFAE0024 */  sw    $t6, 0x24($sp)
/* 0DC3E4 7F0A99F4 02002025 */  move  $a0, $s0
/* 0DC3E8 7F0A99F8 2405010E */  li    $a1, 270
/* 0DC3EC 7F0A99FC 8FA6004C */  lw    $a2, 0x4c($sp)
/* 0DC3F0 7F0A9A00 00403825 */  move  $a3, $v0
/* 0DC3F4 7F0A9A04 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DC3F8 7F0A9A08 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC3FC 7F0A9A0C 0FC2A1B6 */  jal   draw_options_labels
/* 0DC400 7F0A9A10 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC404 7F0A9A14 00408025 */  move  $s0, $v0
.L7F0A9A18:
/* 0DC408 7F0A9A18 8FB90050 */  lw    $t9, 0x50($sp)
/* 0DC40C 7F0A9A1C 3C0C8007 */  lui   $t4, %hi(g_CurrentPlayer) # $t4, 0x8007
/* 0DC410 7F0A9A20 13200018 */  beqz  $t9, .L7F0A9A84
/* 0DC414 7F0A9A24 00000000 */   nop
/* 0DC418 7F0A9A28 0FC304AE */  jal   langGet
/* 0DC41C 7F0A9A2C 3404AC08 */   li    $a0, 44040
/* 0DC420 7F0A9A30 3C090070 */  lui   $t1, (0x007000A0 >> 16) # lui $t1, 0x70
/* 0DC424 7F0A9A34 3C0A0030 */  lui   $t2, (0x003000B0 >> 16) # lui $t2, 0x30
/* 0DC428 7F0A9A38 354A00B0 */  ori   $t2, (0x003000B0 & 0xFFFF) # ori $t2, $t2, 0xb0
/* 0DC42C 7F0A9A3C 352900A0 */  ori   $t1, (0x007000A0 & 0xFFFF) # ori $t1, $t1, 0xa0
/* 0DC430 7F0A9A40 240FFFFF */  li    $t7, -1
/* 0DC434 7F0A9A44 24180001 */  li    $t8, 1
/* 0DC438 7F0A9A48 240B0001 */  li    $t3, 1
/* 0DC43C 7F0A9A4C AFAB0028 */  sw    $t3, 0x28($sp)
/* 0DC440 7F0A9A50 AFB80014 */  sw    $t8, 0x14($sp)
/* 0DC444 7F0A9A54 AFAF0010 */  sw    $t7, 0x10($sp)
/* 0DC448 7F0A9A58 AFA90018 */  sw    $t1, 0x18($sp)
/* 0DC44C 7F0A9A5C AFAA0024 */  sw    $t2, 0x24($sp)
/* 0DC450 7F0A9A60 02002025 */  move  $a0, $s0
/* 0DC454 7F0A9A64 240500FA */  li    $a1, 250
/* 0DC458 7F0A9A68 240600E1 */  li    $a2, 225
/* 0DC45C 7F0A9A6C 00403825 */  move  $a3, $v0
/* 0DC460 7F0A9A70 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC464 7F0A9A74 0FC2A1B6 */  jal   draw_options_labels
/* 0DC468 7F0A9A78 AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC46C 7F0A9A7C 1000001D */  b     .L7F0A9AF4
/* 0DC470 7F0A9A80 00408025 */   move  $s0, $v0
.L7F0A9A84:
/* 0DC474 7F0A9A84 8D8C8BC0 */  lw    $t4, %lo(g_CurrentPlayer)($t4)
/* 0DC478 7F0A9A88 3C048004 */  lui   $a0, %hi(D_800409DC+18)
/* 0DC47C 7F0A9A8C 8D8D2A50 */  lw    $t5, 0x2a50($t4)
/* 0DC480 7F0A9A90 000D7080 */  sll   $t6, $t5, 2
/* 0DC484 7F0A9A94 01CD7021 */  addu  $t6, $t6, $t5
/* 0DC488 7F0A9A98 000E7080 */  sll   $t6, $t6, 2
/* 0DC48C 7F0A9A9C 008E2021 */  addu  $a0, $a0, $t6
/* 0DC490 7F0A9AA0 0FC304AE */  jal   langGet
/* 0DC494 7F0A9AA4 9484A63E */   lhu   $a0, %lo(D_800409DC+18)($a0)
/* 0DC498 7F0A9AA8 3C0800AA */  lui   $t0, (0x00AA00B0 >> 16) # lui $t0, 0xaa
/* 0DC49C 7F0A9AAC 3C0F0030 */  lui   $t7, (0x003000B0 >> 16) # lui $t7, 0x30
/* 0DC4A0 7F0A9AB0 35EF00B0 */  ori   $t7, (0x003000B0 & 0xFFFF) # ori $t7, $t7, 0xb0
/* 0DC4A4 7F0A9AB4 350800B0 */  ori   $t0, (0x00AA00B0 & 0xFFFF) # ori $t0, $t0, 0xb0
/* 0DC4A8 7F0A9AB8 2419FFFF */  li    $t9, -1
/* 0DC4AC 7F0A9ABC 24180001 */  li    $t8, 1
/* 0DC4B0 7F0A9AC0 AFB80028 */  sw    $t8, 0x28($sp)
/* 0DC4B4 7F0A9AC4 AFB90018 */  sw    $t9, 0x18($sp)
/* 0DC4B8 7F0A9AC8 AFA80010 */  sw    $t0, 0x10($sp)
/* 0DC4BC 7F0A9ACC AFAF0024 */  sw    $t7, 0x24($sp)
/* 0DC4C0 7F0A9AD0 02002025 */  move  $a0, $s0
/* 0DC4C4 7F0A9AD4 240500FA */  li    $a1, 250
/* 0DC4C8 7F0A9AD8 240600E1 */  li    $a2, 225
/* 0DC4CC 7F0A9ADC 00403825 */  move  $a3, $v0
/* 0DC4D0 7F0A9AE0 AFA00014 */  sw    $zero, 0x14($sp)
/* 0DC4D4 7F0A9AE4 AFA0001C */  sw    $zero, 0x1c($sp)
/* 0DC4D8 7F0A9AE8 0FC2A1B6 */  jal   draw_options_labels
/* 0DC4DC 7F0A9AEC AFA00020 */   sw    $zero, 0x20($sp)
/* 0DC4E0 7F0A9AF0 00408025 */  move  $s0, $v0
.L7F0A9AF4:
/* 0DC4E4 7F0A9AF4 8FBF003C */  lw    $ra, 0x3c($sp)
/* 0DC4E8 7F0A9AF8 02001025 */  move  $v0, $s0
/* 0DC4EC 7F0A9AFC 8FB00038 */  lw    $s0, 0x38($sp)
/* 0DC4F0 7F0A9B00 03E00008 */  jr    $ra
/* 0DC4F4 7F0A9B04 27BD00B0 */   addiu $sp, $sp, 0xb0
)
#endif
#endif



Gfx *display_text_buttons_dual_control(Gfx *gdl)
{
    s32 textptr_aux;

    gdl = microcode_constructor(gdl);

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
Gfx *draw_watch_controller(Gfx *gdl)
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
        gdl = sub_GAME_7F06359C(gdl, &finalmtx, green - 6, 1, watchTable, &contpadnum0);
    }
    else
    {
        gdl = sub_GAME_7F064364(gdl, &finalmtx, 1, (s32) watchTable, &contpadnum0);
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
            gdl = sub_GAME_7F06359C(gdl, &finalmtx, green - 6, 1, &table2, &contpadnum1);
        }
        else
        {
            gdl = sub_GAME_7F064364(gdl, &finalmtx, 1, (s32) (&table2), &contpadnum1);
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


void reset_controller_options_index(void) {
    controller_options_index = CONTROLLER_OPTIONS_INDEX_STYLE;
}


void reset_game_options_index(void) {
    game_options_index = 0;
}


void zero_D_800409A4(void) {
    D_800409A4 = 0;
}


u32 return_arg0_7F0AB4B0(u32 uParm1) {
    return uParm1;
}


Gfx *draw_watch_control_options_page(Gfx *gdl, Mtx *param_2) {
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

        gdl = draw_watch_controller(gdl);
        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;

        gdl = microcode_constructor(gdl);
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

        gdl = sub_GAME_7F0A97D0(gdl);
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
        } else
        {
            gdl = textRender(gdl, &sp5C, &sp58, textptr, pFontChars, pFontFile, phi_s1, sp54, sp50, 0, 0);
        }

    }
    return gdl;
}


void game_option_select_value(u32 *param_1, u32 param_2)
{
    *param_1 = param_2;
    set_controlstick_lr_disabled();
    sndPlaySfx(g_musicSfxBufferPtr, OPTION_CHOOSE_SFX, NULL);
}


void game_option_toggle_input(s32 option_index)
{
    if ( (joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS|L_TRIG|L_JPAD) || sub_GAME_7F0A4FB0()) && watch_item_is_actively_selected )
    {
        if (game_options_entries[option_index].current_value == 1)
        {
            game_option_select_value(&game_options_entries[option_index].current_value, 0);
        }
        else if (game_options_entries[option_index].current_value == 2)
        {
            game_option_select_value(&game_options_entries[option_index].current_value, 1);
        }
    }
    else
    {
        if ( (joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS|R_TRIG|R_JPAD) || sub_GAME_7F0A4FEC()) && watch_item_is_actively_selected )
        {
            if (game_options_entries[option_index].current_value == 0)
            {
                game_option_select_value(&game_options_entries[option_index].current_value, 1);
            }
            else if ( (game_options_entries[option_index].current_value == 1) && game_options_entries[option_index].text[3] )
            {
                game_option_select_value(&game_options_entries[option_index].current_value, 2);
            }
        }
    }
}


/**
 * Address: 7F0AB908
 *
 * Set the color and draw the text for the values of the toggle options.
 * For example, draw the "ON" and "OFF" text for the Auto-Aim option,
 * but not the "AUTO-AIM" text itself.
 *
 * Options are highlighted by using the controller to advance up and down the toggle options list,
 * but options are not selected until the A button is pressed.
 */
Gfx *draw_toggle_option_values(Gfx *gdl, s32 y, s32 option_index, u32 state)
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

    entry = &game_options_entries[option_index];

    if (j_text_trigger)
    {
        x1 = 0xAA;
        if (1);
    }
    else
    {
        x1 = 0xB4;
    }

    if (j_text_trigger)
    {
        x2 = 0xDC;
    }
    else
    {
        x2 = 0xE1;
    }

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
    entry = &game_options_entries[option_index];
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
    entry = &game_options_entries[option_index];
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
    entry = &game_options_entries[option_index];
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
        if (j_text_trigger)
        {
            x1 = 0xBE;
        }
        else
        {
            x1 = 0xC8;
        }

        if (j_text_trigger)
        {
            // This weird code must be kept on one line for matching.
            x2 = 0xFA; } else { x2 = 0xFA; }
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


Gfx *draw_toggle_options(Gfx *gdl)
{
    s32 y_offset;
    s32 i;

    gdl = microcode_constructor(gdl);

    for (i = 0, y_offset = YOFFSET_1; i < 8; i = i + 1, y_offset = y_offset + YINC) {

        if ( i == game_options_index - 2)
        {
            // Draw option that is highlighted and selected, if there is one.
            if (watch_item_is_actively_selected)
            {
                gdl = draw_toggle_option_values(draw_options_labels(gdl, XOFFSET_1, y_offset, langGet(game_options_entries[i].text[0]), -1, 1, 0x7000A0, 0, 0, 0x3000B0, 0), y_offset, i, 2);
            }
            // Draw option that is highlighted but not selected, if there is one.
            else
            {
                gdl = draw_toggle_option_values(draw_options_labels(gdl, XOFFSET_1, y_offset, langGet(game_options_entries[i].text[0]), 0xA0FFA0F0, 0, -1, 0, 0, 0x3000B0, 0), y_offset, i, 1);
            }
        }
        // Draw the options that are neither highlighted nor selected.
        else
        {
            gdl = draw_toggle_option_values(draw_options_labels(gdl, XOFFSET_1, y_offset, langGet(game_options_entries[i].text[0]), 0xFF00B0, 0, -1, 0, 0, 0x3000B0, 0), y_offset, i, 0);
        }

    }

    return gdl;
}


Gfx *draw_watch_game_options_page(Gfx *gdl, Mtx *param_2) {
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
        gdl = draw_music_volume_slider(gdl);
        gdl = draw_fx_volume_slider(gdl);
        pFontFile = ptrFontBankGothic;
        pFontChars = ptrFontBankGothicChars;
        gdl = microcode_constructor(gdl);

        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_35_MUSIC_LF)); //music

        sp54 = XOFFSET_1;
        sp50 = YOFFSET_8;

        sp5C = 0xFF00B0;

        if (game_options_index == 0)
        {
            sp5C = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0)
            {
                sp5C = -1;
            }
        }

        textMeasure(&sp48, &sp4C, textptr, pFontChars, pFontFile, 0);

        if ((watch_item_is_actively_selected != 0) && (game_options_index == 0))
        {
            gdl = textRenderOutlined(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, 0x7000A0, sp4C + 1, sp48, 0, 0);
        }
        else
        {
            gdl = textRender(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, sp4C, sp48, 0, 0);
        }

        sp5C = 0xFF00B0;
        textptr = langGet(getStringID(LOPTIONS, OPTION_STR_36_FX_LF)); //fx

        sp54 = XOFFSET_1;
        sp50 = YOFFSET_9;


        if (game_options_index == 1)
        {
            sp5C = 0xA0FFA0F0;
            if (watch_item_is_actively_selected != 0)
            {
                sp5C = -1;
            }
        }

        textMeasure(&sp48, &sp4C, textptr, pFontChars, pFontFile, 0);

        if ((watch_item_is_actively_selected != 0) && (game_options_index == 1))
        {
            gdl = textRenderOutlined(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, 0x7000A0, sp4C + 1, sp48, 0, 0);
        }
        else
        {
            gdl = textRender(gdl, &sp54, &sp50, textptr, pFontChars, pFontFile, sp5C, sp4C, sp48, 0, 0);
        }

        gdl = draw_toggle_options(gdl);
    }

    return gdl;
}


int sub_GAME_7F0AC0E8(u8 *arg) {
    u8 cVar1;
    int count;

    cVar1 = *arg;
    count = 0;

    while (cVar1) {

        if (cVar1 == 0xA) {
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
        /**
         * spDAC, spDA4, spD68 are unused.
         * Maybe vestigial tables for formatting the briefings.
         */
        s32 spDAC[15] = {0x34, 0x2f, 0x2d, 0x2a, 0x28, 0x25, 0x25, 0x28, 0x2a, 0x2d, 0x2f, 0x34, 0x37, 0x40, -1};
        s32 spDA4[2] = {0x4b, -1};
        s32 spD68[15] = {0x10e, 0x113, 0x116, 0x119, 0x11a, 0x11b, 0x11b, 0x11a, 0x119, 0x116, 0x113, 0x10e, 0x108, 0xfe, -1};

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

        gdl = microcode_constructor(gdl);
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
#if defined(VERSION_EU)
                char objectiveBuffer[200];
#else
                char objectiveBuffer[200] = "";
#endif
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

                setTextOverlapCorrection((j_text_trigger) ? (1) : (5));
                sprintf(pageTitle, langGet(0xac3c));

                for (i = 0; i < objectiveGetCount(); i++)
                {
                    if (get_difficulty_for_objective(i) <= lvlGetSelectedDifficulty())
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

                        objY = boxTop + ((j_text_trigger) ? (1) : (5));
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

                                if (j_text_trigger)
                                {
                                    colour = 0xa0ffa0f0;
                                }
                                else
                                {
                                    colour = 0x00ff00b0;
                                }
                                break;

                            default:
                                break;
                        }

                        textMeasure(&textHeight, &textWidth, objectiveBuffer, chars, font, 10);
                        objY = boxTop + ((j_text_trigger) ? (1) : (5));

                        if (j_text_trigger)
                        {
                            objX = 0xf5 - textWidth;
                        }
                        else
                        {
                            objX = 0xaf;
                        }

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


/**
 * Address: 7F0ACA28
 */
Gfx *draw_watch_current_page(Gfx *gdl, Mtx *arg1, s32 watch_transitioning)
{
    set_page_rectangle_colors(watch_screen_index, (struct WatchVertex *)g_CurrentPlayer->buffer_for_watch_greenbackdrop_vertices);

    if (watch_transitioning == TRUE)
    {
        set_BONDdata_outside_watch_menu_flag(FALSE);
        sub_GAME_7F0BD8FC(0);

        // Handle A or Z button click when in any page but inventory page
        if ((watch_screen_index != WATCH_INDEX_INVENTORY) && (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON)))
        {
            watch_play_beep_sound();
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
                gdl = draw_watch_game_options_page(gdl, arg1);
                break;
            case WATCH_INDEX_MISSION_BRIEFING:
                gdl = draw_watch_mission_briefing_page(gdl, arg1);
        }
    }
    else if (watch_transitioning == FALSE)
    {
        sub_GAME_7F0BD8FC(1);
        set_BONDdata_outside_watch_menu_flag(TRUE);
        gdl = draw_background_health_and_armor_transitioning(gdl, arg1);
    }

    return gdl;
}
