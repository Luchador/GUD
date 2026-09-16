#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;
typedef int32_t s32;
typedef struct { uint64_t packet; } Gfx;
#define G_SC_NON_INTERLACE 0
#define OS_USEC_TO_CYCLES(us) ((uint64_t)(us) * 46875u / 1000u)
static u32 now;
static s32 width = 320, height = 240, rows, scissors;
static Gfx buffer[512], *limit;
static char labels[1][32];
static u32 lastColor;
static s32 phase;
static void *ptrFontBankGothicChars, *ptrFontBankGothic;
static u32 osGetCount(void) { return now; }
static s32 viGetX(void) { return width; }
static s32 viGetY(void) { return height; }
static s32 dynGetFreeGfx(Gfx *gdl) { return (s32)(limit - gdl); }
static Gfx *gfxSetup2DTextureMode(Gfx *gdl) { assert(phase++ == 1); return gdl + 11; }
static Gfx *bgDebugRender(Gfx *gdl) { assert(phase++ == 0); return gdl; }
static Gfx *bgDebugDrawHud(Gfx *gdl) { assert(phase++ == 2); return gdl; }
static void Scissor(Gfx *gdl, int mode, int x, int y, int w, int h)
{ assert(gdl < limit && x == 0 && y == 0 && w == width && h == height); scissors++; }
#define gDPSetScissor Scissor
static Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text,
    void *chars, void *font, u32 color, s32 w, s32 h, s32 a, s32 b)
{
    assert(rows == 0 && *x == 14 && *y == 18);
    lastColor = color;
    assert(w == width && h == height);
    snprintf(labels[rows++], sizeof(labels[0]), "%s", text);
    /* Exercise the existing conservative upper bound, leaving final sync/end. */
    gdl += strlen(text) * 12 + 1;
    assert(gdl <= limit - 2);
    return gdl;
}
#include "hud.inc"

static void drawFrame(void)
{
    rows = scissors = phase = 0;
    limit = buffer + 512;
    assert(lvDrawFrameRateDisplay(buffer) <= limit - 2);
    assert(rows == 1 && scissors == 1 && phase == 3);
}

static void sampleWindow(unsigned frames, const char *expected, u32 color)
{
    u32 start = now;
    for (unsigned i = 1; i <= frames; i++) {
        now = start + (u32)(OS_USEC_TO_CYCLES(1000000) * i / frames);
        drawFrame();
    }
    assert(!strcmp(labels[0], expected) && lastColor == color);
}

int main(void)
{
    now = 1;
    drawFrame();
    assert(!strcmp(labels[0], "--"));
    /* Preserve GUD's existing first-window count and color thresholds. */
    sampleWindow(60, "61", 0x22ff22ff);
    sampleWindow(30, "30", 0xffff22ff);
    sampleWindow(25, "25", 0xff0000ff);
    sampleWindow(26, "26", 0xffff22ff);
    sampleWindow(50, "50", 0xffff22ff);
    sampleWindow(51, "51", 0x22ff22ff);
    width = 640; height = 480;
    sampleWindow(30, "30", 0xffff22ff);
    /* CP0 Count wraps during this sampling window. */
    now = 0xffff0000u;
    drawFrame();
    sampleWindow(60, "60", 0x22ff22ff);
    /* Tight lists may omit FPS, but must preserve final sync/end space. */
    for (unsigned capacity = 0; capacity < 210; capacity++) {
        rows = scissors = phase = 0; limit = buffer + capacity;
        Gfx *end = lvDrawFrameRateDisplay(buffer);
        assert(end <= limit);
        if (capacity < 18) { assert(end == buffer && rows == 0 && scissors == 0 && phase == 0); }
        else { assert(end <= limit - 2 && phase == 3); }
    }
    puts("PASS: FPS sampling/colors, clock wrap, one text row, X-ray ordering, whole-screen layout and capacity guards.");
    return 0;
}
