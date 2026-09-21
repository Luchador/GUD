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

#define LEVELID_CUBA 54
#define PLAYER_1 0
#define Z_TRIG 0x2000
#define A_BUTTON 0x8000
#define TRUE 1
#define FALSE 0

static s32 g_CreditsRollTimer, g_CreditsSkipHoldFrames;
static bool g_SkipPostCreditsCast;
static CREDITS_STATE g_CreditsState;
static CreditsEntry entries[256], *credits_pointer;
static void *ptrFontZurichBoldChars, *ptrFontZurichBold;
static s32 stage, screenWidth = 440, screenHeight = 330;
static s32 viewLeft, viewTop, viewWidth = 440, viewHeight = 330;
static s32 prompts, rows, phase, inputReads;
static u16 buttons;
static Gfx buffer[2048];

static s32 bossGetStageNum(void) { return stage; }
static s32 viGetX(void) { return screenWidth; }
static s32 viGetY(void) { return screenHeight; }
static s32 viGetViewLeft(void) { return viewLeft; }
static s32 viGetViewTop(void) { return viewTop; }
static s16 viGetViewHeight(void) { return viewHeight; }
static u16 joyGetButtons(s32 player, u16 mask)
{
    assert(player == PLAYER_1 && mask == Z_TRIG);
    inputReads++;
    return buttons & mask;
}
static char *langGet(u16 id) { assert(id == 1); return "Credit\n"; }
static void textMeasure(s32 *height, s32 *width, char *text, void *chars, void *font, s32 lineheight)
{
    *height = 0;
    *width = 0;
    for (; *text; text++) {
        if (*text == '\n') *height += 16;
        else *width += 8;
    }
}
static Gfx *gfxSetup2DTextureMode(Gfx *gdl) { assert(phase == 0); phase = 1; return gdl + 1; }
static Gfx *gfxRestore3DRenderMode(Gfx *gdl) { assert(phase == 1); phase = 2; return gdl + 1; }
static Gfx *gfxDrawTranslucentRect(Gfx *gdl, s32 x, s32 y, s32 x2, s32 y2, u32 color)
{ assert(phase == 1); return gdl + 1; }
static Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, void *chars, void *font,
                      u32 color, s32 width, s32 height, u32 offset, s32 lineheight)
{ assert(phase == 1); rows++; return gdl + 1; }
static Gfx *textRenderOutlined(Gfx *gdl, s32 *x, s32 *y, char *text, void *chars, void *font,
                              u32 color, u32 outline, s32 width, s32 height, s32 offset, s32 lineheight)
{
    assert(phase == 1 && g_CreditsState == CREDITS_STATE_ROLLING);
    assert(!strcmp(text, "Hold Z to skip.\n"));
    assert(*x >= viewLeft + 12 && *x < viewLeft + viewWidth / 2);
    assert(*y > viewTop + viewHeight / 2 && *y + 16 <= viewTop + viewHeight - 8);
    assert(width == screenWidth && height == screenHeight);
    assert(color == 0xffffffff && outline == 0x000000ff);
    prompts++;
    return gdl + 1;
}

#include "credits.inc"

static bool full_actor_intro;
static s32 intro_character_index, nextMenu, cursorStage, g_CastSkipHoldFrames;
static void frontChangeMenu(s32 menu, s32 reload)
{
    assert(reload == TRUE);
    nextMenu = menu;
}
static void set_cursor_to_stage_solo(s32 stage) { cursorStage = stage; }
#include "front.inc"

static void beginCredits(void)
{
    stage = LEVELID_CUBA;
    g_CreditsState = CREDITS_STATE_ROLLING;
    g_CreditsRollTimer = g_CreditsSkipHoldFrames = 0;
    g_SkipPostCreditsCast = FALSE;
    credits_pointer = entries;
    memset(entries, 0, sizeof(entries));
    for (s32 i = 0; i < 255; i++) {
        entries[i].TextId1 = 1;
        entries[i].TextId2 = 0x5011;
    }
}

static void frame(u16 held)
{
    buttons = held;
    prompts = rows = phase = inputReads = 0;
    Gfx *end = bondviewRenderCredits(buffer);
    assert(end >= buffer && end < buffer + 2048);
    assert(phase == 0 || phase == 2);
}

int main(void)
{
    beginCredits();
    for (s32 i = 0; i < 60; i++) {
        frame(0);
        assert(g_CreditsState == CREDITS_STATE_ROLLING && prompts == 1 && rows > 0);
        assert(g_CreditsSkipHoldFrames == 0 && g_CreditsRollTimer == i + 1);
    }
    for (s32 i = 0; i < 29; i++) frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_ROLLING && prompts == 1);
    assert(!g_SkipPostCreditsCast);
    frame(0);
    assert(g_CreditsSkipHoldFrames == 0);
    for (s32 i = 0; i < 29; i++) frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_ROLLING);
    frame(A_BUTTON);
    assert(g_CreditsSkipHoldFrames == 0);
    for (s32 i = 0; i < 29; i++) frame(Z_TRIG | A_BUTTON);
    assert(g_CreditsState == CREDITS_STATE_ROLLING);
    frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_COMPLETED && g_CreditsSkipHoldFrames == 0);
    assert(g_SkipPostCreditsCast);
    assert(prompts == 0 && rows == 0 && phase == 0);
    frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_COMPLETED && prompts == 0 && inputReads == 0);
    assert(g_SkipPostCreditsCast);

    /* Keep the request through the fade-out and title-stage return, then consume it. */
    stage = 0;
    for (s32 i = 0; i < 60; i++) frame(0);
    assert(g_SkipPostCreditsCast);
    full_actor_intro = TRUE;
    intro_character_index = 12;
    cursorStage = -1;
    frontContinueAfterCredits();
    assert(nextMenu == MENU_MISSION_SELECT && cursorStage == SP_LEVEL_CRADLE);
    assert(!full_actor_intro && intro_character_index == 0 && !g_SkipPostCreditsCast);

    /* Consuming the request must not suppress a later, unskipped showcase. */
    cursorStage = -1;
    frontContinueAfterCredits();
    assert(nextMenu == MENU_DISPLAY_CAST && full_actor_intro && intro_character_index == 0);
    assert(cursorStage == -1);

    /* Inactive stages/states and a missing table must not accumulate a hold. */
    for (s32 inactive = 0; inactive < 4; inactive++) {
        beginCredits();
        if (inactive == 0) stage = 0;
        if (inactive == 1) g_CreditsState = CREDIT_STATE_START;
        if (inactive == 2) g_CreditsState = CREDITS_STATE_COMPLETED;
        if (inactive == 3) credits_pointer = NULL;
        CREDITS_STATE previous = g_CreditsState;
        g_CreditsSkipHoldFrames = 29;
        for (s32 i = 0; i < 40; i++) frame(Z_TRIG);
        assert(g_CreditsState == previous && g_CreditsRollTimer == 0);
        assert(g_CreditsSkipHoldFrames == 0 && prompts == 0 && inputReads == 0);
        assert(!g_SkipPostCreditsCast);
    }

    /* An uninterrupted replay needs a fresh full hold. */
    beginCredits();
    for (s32 i = 0; i < 29; i++) frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_ROLLING);
    frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_COMPLETED);

    /* Normal end-of-table completion also removes the prompt. */
    beginCredits();
    memset(&entries[1], 0, sizeof(entries[1]));
    g_CreditsRollTimer = viewHeight + 31;
    frame(0);
    assert(g_CreditsState == CREDITS_STATE_COMPLETED && prompts == 0 && phase == 2);
    assert(!g_SkipPostCreditsCast);
    cursorStage = -1;
    frontContinueAfterCredits();
    assert(nextMenu == MENU_DISPLAY_CAST && full_actor_intro && cursorStage == -1);

    /* The lower-left prompt follows the active viewport and resolution. */
    beginCredits();
    frame(0);
    screenWidth = viewWidth = 320;
    screenHeight = viewHeight = 240;
    frame(0);
    viewLeft = 20; viewTop = 15; viewWidth = 280; viewHeight = 210;
    frame(0);
    assert(prompts == 1);
    puts("PASS: 30-frame hold, release reset, stage gating, prompt placement and post-credits cast routing.");
    return 0;
}
