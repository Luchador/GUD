/* Exercise the production HUD with a conservative per-glyph command cost. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rcpprofile.h"
typedef struct { u32 words[2]; } Gfx;
static Gfx buffer[4096];
static s32 capacity, rows, fullScissor;
static char labels[16][80];
static int gfx;
static void *ptrFontBankGothicChars, *ptrFontBankGothic;
static u32 g_ProfBgTickCycles, g_ProfLvlTickCycles, g_ProfLvlRenderCycles;
static u32 g_ProfBgRenderCycles, g_ProfChrTickCycles, g_ProfObjTickCycles;
static u32 g_ProfChrActionCycles, g_ProfGfxCommands, g_ProfBgGfxCommands;
OSIntMask osSetIntMask(OSIntMask mask) { (void)mask; return 0; }
u32 osGetCount(void) { return 100; }
s32 viGetX(void) { return 320; }
s32 viGetY(void) { return 240; }
s32 dynGetFreeGfx(Gfx *gdl) { return capacity - (gdl - buffer); }
Gfx *gfxSetup2DTextureMode(Gfx *gdl) { return gdl + 11; }
#define OS_USEC_TO_CYCLES(value) ((u64)(value) * 46875 / 1000)
#define G_SC_NON_INTERLACE 0
#define gDPSetScissor(pkt, mode, x0, y0, x1, y1) do { \
    (void)(pkt); assert((x0) == 0 && (y0) == 0 && (x1) == 320 && (y1) == 240); \
    fullScissor++; \
} while (0)
Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, void *chars,
        void *font, u32 color, s32 width, s32 height, u32 offset, s32 lineheight)
{
    s32 commands = 1 + 12 * strlen(text);
    (void)chars; (void)font; (void)color; (void)offset; (void)lineheight;
    assert(*x == 14 && *y >= 18 && *y + 10 <= height && width == 320);
    assert(rows < 16 && strlen(text) < sizeof(labels[0]));
    strcpy(labels[rows++], text);
    assert(dynGetFreeGfx(gdl) >= commands + 2);
    return gdl + commands;
}
#include "hud_functions.inc"

int main(void)
{
    Gfx *end;
    s32 count;
    rcpProfileReset();
    capacity = 4096;
    end = lvDrawFrameRateDisplay(buffer);
    assert(rows == 10 && !strcmp(labels[9], "RCP: WAITING FOR TASK"));
    assert(fullScissor == 1 && dynGetFreeGfx(end) >= 2);

    rcpProfileRspStart(&gfx, M_GFXTASK, 0, 0);
    rcpProfileRspDone(&gfx, 46875, 0);
    rcpProfileRdpDone(&gfx, 93750, 125000, 62500, 62500, 31250);
    rows = fullScissor = 0;
    end = lvDrawFrameRateDisplay(buffer);
    assert(rows == 16 && fullScissor == 1);
    assert(!strcmp(labels[9], "RCP MS #1"));
    assert(!strcmp(labels[10], "RSP:1.0 AUD:0.0"));
    assert(!strcmp(labels[12], "YLAG:0.0 DPEND:2.0"));
    assert(!strcmp(labels[13], "CLK:2.0 CMD:1.0"));
    assert(!strcmp(labels[14], "PIPE:1.0 TMEM:0.5"));
    assert(!strcmp(labels[15], "END:2.0 AVG:2.0 MAX:2.0"));
    assert(dynGetFreeGfx(end) >= 2);

    /* Exercise every short-buffer boundary: omit rows before overflowing,
     * including the initial 2D setup and the caller's final sync/end. */
    for (count = 2; count <= 4096; count++) {
        capacity = count; rows = fullScissor = 0;
        end = lvDrawFrameRateDisplay(buffer);
        assert(dynGetFreeGfx(end) >= 2);
    }
    puts("RCP HUD values, screen bounds and display-list capacity checks passed");
    return 0;
}
