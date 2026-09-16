#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;
typedef int32_t s32;
typedef struct { uint64_t packet; } Gfx;
#include "profile.inc"
#define G_SC_NON_INTERLACE 0
#define OS_USEC_TO_CYCLES(us) ((uint64_t)(us) * 46875u / 1000u)
static u32 now;
static s32 width = 320, height = 240, rows, scissors;
static Gfx buffer[512], *limit;
static FrameBottleneck result;
static char labels[2][32];
static void *ptrFontBankGothicChars, *ptrFontBankGothic;
static u32 osGetCount(void) { return now; }
static s32 viGetX(void) { return width; }
static s32 viGetY(void) { return height; }
FrameBottleneck frameProfileGetBottleneck(void) { return result; }
static s32 dynGetFreeGfx(Gfx *gdl) { return (s32)(limit - gdl); }
static Gfx *gfxSetup2DTextureMode(Gfx *gdl) { return gdl + 11; }
static Gfx *bgDebugRender(Gfx *gdl) { return gdl; }
static Gfx *bgDebugDrawHud(Gfx *gdl) { return gdl; }
static void Scissor(Gfx *gdl, int mode, int x, int y, int w, int h)
{ assert(gdl < limit && x == 0 && y == 0 && w == width && h == height); scissors++; }
#define gDPSetScissor Scissor
static Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text,
    void *chars, void *font, u32 color, s32 w, s32 h, s32 a, s32 b)
{
    assert(rows < 2 && *x == 14 && *y == (rows ? 32 : 18));
    assert(w == width && h == height);
    snprintf(labels[rows++], sizeof(labels[0]), "%s", text);
    /* Exercise the existing conservative upper bound, leaving final sync/end. */
    gdl += strlen(text) * 12 + 1;
    assert(gdl <= limit - 2);
    return gdl;
}
#include "hud.inc"

int main(void)
{
    const char *expected[] = {"LIMIT: --", "LIMIT: ?", "LIMIT: MIXED", "LIMIT: CPU", "LIMIT: RSP", "LIMIT: RDP"};
    for (unsigned i = 0; i < sizeof(expected) / sizeof(*expected); i++) {
        rows = scissors = 0; limit = buffer + 512; result = (FrameBottleneck)i;
        assert(lvDrawFrameRateDisplay(buffer) <= limit - 2);
        assert(rows == 2 && scissors == 1 && !strcmp(labels[0], "--") && !strcmp(labels[1], expected[i]));
    }
    width = 640; height = 480;
    rows = scissors = 0; limit = buffer + 512;
    lvDrawFrameRateDisplay(buffer); assert(rows == 2 && scissors == 1);
    /* Tight lists may omit the label, then FPS, but must not overrun. */
    for (unsigned capacity = 0; capacity < 210; capacity++) {
        rows = scissors = 0; limit = buffer + capacity;
        Gfx *end = lvDrawFrameRateDisplay(buffer);
        assert(end <= limit);
        if (capacity < 18) { assert(end == buffer && rows == 0 && scissors == 0); }
        else { assert(end <= limit - 2); }
    }
    puts("PASS: compact labels, FPS retention, whole-screen layout and display-list capacity guards.");
    return 0;
}
