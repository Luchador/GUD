#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef struct { uint64_t packet; } Gfx;
#include "declarations.inc"

#define TRUE 1
#define FALSE 0
#define PLAYER_1 0
#define Z_TRIG 0x2000
#define A_BUTTON 0x8000
#define LEVELID_CUBA 54
#define GAMEMODE_MULTI 1
#define SCREEN_SIZE_320x240 0
#define SCREEN_SIZE_440x330 1
#define SCREEN_HEIGHT 240
#define ASPECT_RATIO (4.0f / 3.0f)
#define ASPECT_RATIO_SD ASPECT_RATIO
#define FOV_Y_F 60.0f
#define BODY_Moonraker_Elite_1_Male 100
#define BODY_Moonraker_Elite_2_Female 101
#define BODY_Mayday 102
#define BODY_Jaws 103
#define BODY_Oddjob 104
#define BODY_Baron_Samedi 105

static MENU current_menu, menu_update, maybe_prev_menu;
static s32 screen_size, g_MenuTimer, g_ClockTimer = 1, g_CastSkipHoldFrames;
static s32 intro_character_index, selected_stage, gamemode, cursorStage;
static bool g_HiResSwitchPending, g_LoResSwitchPending, full_actor_intro, g_SkipPostCreditsCast;
static s32 castLoads, castUnloads, missionLoads, demoStarts;
static u16 buttons, pressed, previousButtons;
static struct { s32 body, flag, text1, text2, text3; } intro_char_table[4];
static s32 screenWidth, screenHeight, viewLeft, viewTop, viewHeight;
static s32 prompts;
static s32 selected_difficulty, requestedStage;
static s32 folder_selection_screen_option_icon, folder_selected_for_deletion, folder_selected_for_deletion_choice;
static bool tab_start_highlight, tab_next_highlight, tab_prev_highlight, maybe_is_in_menu, prev_keypresses;
static void *ptrFontZurichBoldChars, *ptrFontZurichBold;
static u16 framebuffer[2], menuBuffer;
static u16 *cfb_16[2] = { &framebuffer[0], &framebuffer[1] };
static u16 *ptr_menu_videobuffer = &menuBuffer;
static u16 *viFrameBuffer = &menuBuffer;
static Gfx buffer[128];

static u16 joyGetButtons(s32 player, u16 mask) { assert(player == PLAYER_1); return buttons & mask; }
static u16 joyGetButtonsPressedThisFrame(s32 player, u16 mask) { return pressed & mask; }
static void viSetFovY(float fov) {}
static void viSetAspect(float aspect) {}
static void viSetZRange(float near, float far) {}
static void viSetUseZBuf(s32 enabled) {}
static u16 *viGetFrameBuf2(void) { return viFrameBuffer; }
static void viSetFrameBuf2(u16 *buffer) { viFrameBuffer = buffer; }
static void viSetXY(s32 width, s32 height) { screenWidth = width; screenHeight = height; }
static void viSetBuf(s32 width, s32 height) {}
static void viSetViewSize(s32 width, s32 height) { viewHeight = height; }
static void viSetViewPosition(s32 x, s32 y) { viewLeft = x; viewTop = y; }
static void set_cur_player_screen_size(s32 width, s32 height) {}
static void set_cur_player_viewport_size(s32 x, s32 y) {}
static s32 viGetX(void) { return screenWidth; }
static s32 viGetY(void) { return screenHeight; }
static s32 viGetViewLeft(void) { return viewLeft; }
static s32 viGetViewTop(void) { return viewTop; }
static s32 viGetViewHeight(void) { return viewHeight; }
static bool check_aztec_completed_any_folder_secret_00(void) { return TRUE; }
static bool fileIsEgyptCompletedOn00AnyFolder(void) { return TRUE; }
static bool ramromIsDemoLaunchPending(void) { return FALSE; }
static void ramromSelectDemoAndPlay(void) { demoStarts++; }
static u32 randomGetNext(void) { return 1; }
static void set_cursor_to_stage_solo(s32 stage) { cursorStage = stage; }
static void bossSetLoadedStage(s32 stage) { requestedStage = stage; }
static void lvSetSelectedDifficulty(s32 difficulty) {}
/* Model allocation/cleanup are mocked; menu transitions and input are production code. */
static void init_menu18_displaycast(void) { g_MenuTimer = 0; castLoads++; }
static void update_menu18_displaycast(void) { castUnloads++; }
static void init_menu07_missionselect(void) { selected_stage = -1; missionLoads++; }

#include "cast_menu.inc"
#include "menu_reset.inc"

static char *langGet(u16 id) { return "Cast name\n"; }
static void textMeasure(s32 *height, s32 *width, char *text, void *chars, void *font, s32 lineheight)
{ *height = strchr(text, '\n') ? 16 : 0; *width = 100; }
static Gfx *gfxSetup2DTextureMode(Gfx *gdl) { return gdl + 1; }
static Gfx *gfxDrawTranslucentRect(Gfx *gdl, s32 x, s32 y, s32 x2, s32 y2, u32 color) { return gdl + 1; }
static Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, void *chars, void *font,
                      u32 color, s32 width, s32 height, u32 offset, s32 lineheight) { return gdl + 1; }
static Gfx *textRenderOutlined(Gfx *gdl, s32 *x, s32 *y, char *text, void *chars, void *font,
                              u32 color, u32 outline, s32 width, s32 height, s32 offset, s32 lineheight)
{
    assert(full_actor_intro && !strcmp(text, "Hold Z to skip.\n"));
    assert(*x == viewLeft + 12 && *y + 16 == viewTop + viewHeight - 8);
    assert(color == 0xffffffff && outline == 0xff);
    assert(width == screenWidth && height == screenHeight);
    prompts++;
    return gdl + 1;
}
#include "cast_text.inc"

static void beginCast(bool extended)
{
    current_menu = MENU_DISPLAY_CAST;
    menu_update = maybe_prev_menu = MENU_INVALID;
    screen_size = SCREEN_SIZE_440x330;
    screenWidth = 440; screenHeight = viewHeight = 330; viewLeft = viewTop = 0;
    selected_stage = LEVELID_CUBA;
    gamemode = 0;
    g_HiResSwitchPending = g_LoResSwitchPending = g_SkipPostCreditsCast = FALSE;
    g_MenuTimer = 0;
    castLoads = castUnloads = missionLoads = demoStarts = 0;
    cursorStage = -1;
    previousButtons = 0;
    viFrameBuffer = ptr_menu_videobuffer;
    prev_keypresses = FALSE;
    requestedStage = -1;
    memset(intro_char_table, 0, sizeof(intro_char_table));
    intro_char_table[3].body = -1;
    do_extended_cast_display(extended);
}

static void menuFrame(u16 held)
{
    buttons = held;
    pressed = held & ~previousButtons;
    previousButtons = held;
    menu_init();
}

static void finishMenuTransition(void)
{
    for (s32 i = 0; i < 6 && current_menu != MENU_MISSION_SELECT; i++) menuFrame(0);
    assert(current_menu == MENU_MISSION_SELECT && cursorStage == SP_LEVEL_CRADLE);
    assert(missionLoads == 1 && !full_actor_intro && g_CastSkipHoldFrames == 0);
}

static void cubaRoundTrip(bool skip)
{
    beginCast(FALSE);
    current_menu = MENU_MISSION_COMPLETE;
    maybe_prev_menu = MENU_RUN_STAGE;
    menuFrame(0);
    assert(requestedStage == LEVELID_CUBA);
    /* The return destination is selected on launch, before a skip can be requested. */
    assert(current_menu == MENU_RUN_STAGE && menu_update == MENU_DISPLAY_CAST && castLoads == 0);

    g_SkipPostCreditsCast = skip; /* Accepted in Cuba by the separately tested hold/fade path. */
    init_menus_or_reset();
    assert(current_menu == MENU_INVALID && menu_update == MENU_DISPLAY_CAST);
    viFrameBuffer = cfb_16[1];
    for (s32 i = 0; i < 6; i++) {
        menuFrame(0); /* Z has already been released; the accepted request must suffice. */
        if (skip) assert(castLoads == 0);
    }
    if (skip) {
        assert(current_menu == MENU_MISSION_SELECT && cursorStage == SP_LEVEL_CRADLE);
        assert(missionLoads == 1 && !full_actor_intro && !g_SkipPostCreditsCast);
    } else {
        assert(current_menu == MENU_DISPLAY_CAST && castLoads == 1 && full_actor_intro);
        assert(missionLoads == 0);
    }
}

int main(void)
{
    cubaRoundTrip(TRUE);
    cubaRoundTrip(FALSE);

    beginCast(TRUE);
    for (s32 i = 0; i < 29; i++) menuFrame(Z_TRIG);
    assert(full_actor_intro && menu_update == MENU_INVALID && g_CastSkipHoldFrames == 29);
    menuFrame(Z_TRIG);
    assert(menu_update == MENU_MISSION_SELECT && !full_actor_intro);
    finishMenuTransition();
    assert(castUnloads == 1);

    beginCast(TRUE);
    for (s32 i = 0; i < 29; i++) menuFrame(Z_TRIG);
    menuFrame(0);
    assert(g_CastSkipHoldFrames == 0);
    for (s32 i = 0; i < 29; i++) menuFrame(Z_TRIG);
    assert(full_actor_intro);
    menuFrame(Z_TRIG);
    finishMenuTransition();

    /* The hold spans a normal character change without being reset by its menu reload. */
    beginCast(TRUE);
    g_MenuTimer = 180;
    for (s32 i = 0; i < 29; i++) menuFrame(Z_TRIG);
    assert(full_actor_intro && castLoads == 1 && g_CastSkipHoldFrames == 29);
    menuFrame(Z_TRIG);
    finishMenuTransition();

    /* Reaching 30 during the black transition latches the request until it is safe to exit. */
    beginCast(TRUE);
    for (s32 i = 0; i < 25; i++) menuFrame(Z_TRIG);
    g_MenuTimer = 180;
    for (s32 i = 0; i < 5; i++) menuFrame(Z_TRIG);
    assert(current_menu == MENU_SWITCH_SCREENS && g_CastSkipHoldFrames == 30);
    menuFrame(0);
    assert(menu_update == MENU_MISSION_SELECT && !full_actor_intro);
    finishMenuTransition();

    /* A release during that same transition must reset an incomplete hold. */
    beginCast(TRUE);
    for (s32 i = 0; i < 25; i++) menuFrame(Z_TRIG);
    g_MenuTimer = 180;
    for (s32 i = 0; i < 3; i++) menuFrame(Z_TRIG);
    menuFrame(0);
    assert(g_CastSkipHoldFrames == 0);
    for (s32 i = 0; i < 29; i++) menuFrame(Z_TRIG);
    assert(full_actor_intro && g_CastSkipHoldFrames == 29);
    menuFrame(Z_TRIG);
    finishMenuTransition();

    beginCast(TRUE);
    for (s32 i = 0; i < 35; i++) menuFrame(A_BUTTON);
    assert(full_actor_intro && g_CastSkipHoldFrames == 0 && menu_update == MENU_INVALID);
    prompts = 0;
    assert(drawCastText(buffer) < buffer + 128 && prompts == 1);

    /* The startup attract-mode roster keeps its existing any-button exit and has no prompt. */
    beginCast(FALSE);
    prompts = 0;
    assert(drawCastText(buffer) < buffer + 128 && prompts == 0);
    menuFrame(A_BUTTON);
    assert(menu_update == MENU_FILE_SELECT && g_CastSkipHoldFrames == 0);

    beginCast(TRUE);
    intro_character_index = 2;
    g_MenuTimer = 180;
    menuFrame(0);
    finishMenuTransition();

    puts("PASS: Cuba launch/return without loading cast after a skip, normal ending, cast input and cleanup.");
    return 0;
}
