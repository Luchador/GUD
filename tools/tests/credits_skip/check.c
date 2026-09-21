#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef float f32;
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
static s32 g_HiResModeActive, g_HiResExitDelay, titleReturns;
static s32 g_UpperTextMsgQueued, g_UpperTextTopSlot;
static char stringbuffer_top[2][BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH];
static f32 g_GlobalTimerDelta = 2.0f;
static struct {
    f32 colourfadetime60, colourfadetimemax60, colourscreenfrac;
    f32 colourfadefracold, colourfadefracnew;
    s32 colourscreenred, colourscreengreen, colourscreenblue;
    s32 colourfaderedold, colourfaderednew, colourfadegreenold, colourfadegreennew;
    s32 colourfadeblueold, colourfadebluenew;
} player, *g_CurrentPlayer = &player;
static void bossReturnTitleStage(void) { assert(!g_HiResModeActive); titleReturns++; }

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
    assert(phase == 1 && (g_CreditsState == CREDITS_STATE_ROLLING || g_CreditsState == CREDITS_STATE_DIALOGUE));
    assert(!strcmp(text, "Hold Z to skip.\n"));
    assert(*x >= viewLeft + 12 && *x < viewLeft + viewWidth / 2);
    assert(*y > viewTop + viewHeight / 2 && *y + 16 <= viewTop + viewHeight - 8);
    assert(width == screenWidth && height == screenHeight);
    assert(color == 0xffffffff && outline == 0x000000ff);
    prompts++;
    return gdl + 1;
}

#include "credits.inc"

static void beginCredits(void)
{
    stage = LEVELID_CUBA;
    g_CreditsState = CREDITS_STATE_ROLLING;
    g_CreditsRollTimer = g_CreditsSkipHoldFrames = 0;
    g_SkipPostCreditsCast = FALSE;
    g_HiResModeActive = 1;
    g_HiResExitDelay = titleReturns = 0;
    g_UpperTextMsgQueued = g_UpperTextTopSlot = 0;
    memset(&player, 0, sizeof(player));
    player.colourfadetimemax60 = -1.0f;
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
    currentPlayerUpdateColourScreenProperties();
    Gfx *end = bondviewRenderCredits(buffer);
    assert(end >= buffer && end < buffer + 2048);
    assert(phase == 0 || phase == 2);
}

static void finishSkip(void)
{
    assert(g_CreditsState == CREDITS_STATE_SKIPPING && g_SkipPostCreditsCast);
    assert(player.colourfadefracnew == 1.0f);
    for (s32 i = 0; i < 40 && !currentPlayerIsFadeComplete(); i++) {
        frame(0);
        assert(titleReturns == 0 && prompts == 0 && inputReads == 0);
    }
    assert(currentPlayerIsFadeComplete() && g_HiResExitDelay == 1);
    assert(player.colourscreenfrac == 1.0f);
    /* The existing viewport/VI code owns the switch; do not unload or restart its delay. */
    g_HiResExitDelay = 3;
    frame(0);
    assert(g_HiResExitDelay == 3 && titleReturns == 0);
    g_HiResModeActive = g_HiResExitDelay = 0;
    frame(0);
    assert(g_CreditsState == CREDITS_STATE_COMPLETED && titleReturns == 1 && g_SkipPostCreditsCast);
    frame(0);
    assert(titleReturns == 1);
}

int main(void)
{
    beginCredits();
    for (s32 i = 0; i < 60; i++) {
        frame(0);
        assert(g_CreditsState == CREDITS_STATE_ROLLING && prompts == 0 && rows > 0);
        assert(g_CreditsSkipHoldFrames == 0 && g_CreditsRollTimer == i + 1);
    }
    for (s32 i = 0; i < 29; i++) frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_ROLLING && prompts == 0);
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
    assert(g_CreditsState == CREDITS_STATE_SKIPPING && g_CreditsSkipHoldFrames == 0);
    assert(g_SkipPostCreditsCast);
    assert(prompts == 0 && rows == 0 && phase == 0);
    frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_SKIPPING && prompts == 0 && inputReads == 0);
    assert(g_SkipPostCreditsCast);
    finishSkip();

    /* Keep the request through the fade-out and title-stage return for menu re-entry. */
    stage = 0;
    for (s32 i = 0; i < 60; i++) frame(0);
    assert(g_SkipPostCreditsCast);

    /* Inactive stages/states must not accumulate a hold. */
    for (s32 inactive = 0; inactive < 3; inactive++) {
        beginCredits();
        if (inactive == 0) stage = 0;
        if (inactive == 1) g_CreditsState = CREDIT_STATE_START;
        if (inactive == 2) g_CreditsState = CREDITS_STATE_COMPLETED;
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
    assert(g_CreditsState == CREDITS_STATE_SKIPPING);
    finishSkip();

    /* The first dialogue line enables the prompt before a single credits row has scrolled. */
    beginCredits();
    g_CreditsState = CREDIT_STATE_START;
    frame(0);
    assert(prompts == 0);
    bviewShowUpperMessage("Bond and Natalya\n");
    frame(0);
    assert(g_CreditsState == CREDITS_STATE_DIALOGUE && prompts == 1 && rows == 0);
    assert(g_CreditsRollTimer == 0);
    g_UpperTextMsgQueued = 0; /* A gap between dialogue lines must retain the prompt. */
    for (s32 i = 0; i < 29; i++) frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_DIALOGUE && prompts == 1);
    frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_SKIPPING && g_CreditsRollTimer == 0);
    /* A late AI credits_roll command must not cancel the already accepted skip. */
    runCreditsRoll();
    assert(g_CreditsState == CREDITS_STATE_SKIPPING);
    finishSkip();

    /* One uninterrupted hold can span the dialogue-to-credits transition. */
    beginCredits();
    g_CreditsState = CREDIT_STATE_START;
    bviewShowUpperMessage("Dialogue\n");
    for (s32 i = 0; i < 15; i++) frame(Z_TRIG);
    runCreditsRoll();
    for (s32 i = 0; i < 14; i++) frame(Z_TRIG);
    assert(g_CreditsState == CREDITS_STATE_ROLLING && g_CreditsSkipHoldFrames == 29);
    frame(Z_TRIG);
    finishSkip();

    /* An absent credits table must not prevent skipping the ending. */
    beginCredits();
    credits_pointer = NULL;
    frame(0);
    assert(prompts == 1 && rows == 0);
    for (s32 i = 0; i < 30; i++) frame(Z_TRIG);
    finishSkip();

    beginCredits();
    stage = 0;
    g_CreditsState = CREDIT_STATE_START;
    bviewShowUpperMessage("Other stage\n");
    assert(g_CreditsState == CREDIT_STATE_START);

    /* Normal end-of-table completion also removes the prompt. */
    beginCredits();
    memset(&entries[1], 0, sizeof(entries[1]));
    g_CreditsRollTimer = viewHeight + 31;
    frame(0);
    assert(g_CreditsState == CREDITS_STATE_COMPLETED && prompts == 0 && phase == 2);
    assert(!g_SkipPostCreditsCast);

    /* The lower-left prompt follows the active viewport and resolution. */
    beginCredits();
    g_CreditsState = CREDITS_STATE_DIALOGUE;
    frame(0);
    screenWidth = viewWidth = 320;
    screenHeight = viewHeight = 240;
    frame(0);
    viewLeft = 20; viewTop = 15; viewWidth = 280; viewHeight = 210;
    frame(0);
    assert(prompts == 1);
    puts("PASS: dialogue prompt, one hold through credits, fade/VI exit and late AI command.");
    return 0;
}
