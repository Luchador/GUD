#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef unsigned long long u64;
typedef long long s64;
typedef float f32;
typedef double f64;
typedef s32 bool;
#define TRUE 1
#define FALSE 0
#define _LANGUAGE_C 1
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & (0xffffffffu >> (32 - (w)))) << (s)))
#include <PR/gbi.h>
#include <PR/rcp.h>
#define PLAYER_1 0
#define U_CBUTTONS 0x8
#define D_CBUTTONS 0x4
#define U_JPAD 0x800
#define D_JPAD 0x400
#define L_CBUTTONS 0x2
#define R_CBUTTONS 0x1
#define L_TRIG 0x20
#define R_TRIG 0x10
#define L_JPAD 0x200
#define R_JPAD 0x100
#define XOFFSET_1 64
typedef struct OSViMode { struct { u32 ctrl; } comRegs; } OSViMode;
static u8 g_TestRam[1024 * 1024];
u32 osMemSize = sizeof(g_TestRam);
u8 *g_GfxBuffers[3] = {g_TestRam, g_TestRam + 0x1000, g_TestRam + 0x2000};
u8 *g_VtxBuffers[3] = {g_TestRam + 0x2000, g_TestRam + 0x3000, g_TestRam + 0x4000};
static u32 g_TestButtons;
static s32 g_TestStick;
static s32 g_TestActive;
static s32 g_TestChooseCount;
static u32 joyGetButtonsPressedThisFrame(s32 player, u32 mask) { (void)player; return g_TestButtons & mask; }
static bool watchShouldNavUp(void) { return g_TestStick == 1; }
static bool watchShouldNavDown(void) { return g_TestStick == -1; }
static void watchSetStickYDisabled(void) { g_TestStick = 0; }
static void watchResetItemIsActivelySelected(void) { g_TestActive = 0; }

static bool watchShouldNavLeft(void) { return g_TestStick == -2; }
static bool watchShouldNavRight(void) { return g_TestStick == 2; }
static void watchSelectGameOption(u32 *value, u32 chosen) { *value = chosen; g_TestStick = 0; g_TestChooseCount++; }
/* Use the production measurement and watch-label functions. Only the final
 * rasterizer is replaced, so zero-height clipping cannot pass unnoticed. */
struct fontchar { s32 index, baseline, height, width, kerningindex; u8 *pixeldata; };
struct font { s32 kerning[13 * 13]; struct fontchar chars[94]; };
static struct font g_TestFont;
static struct font *ptrFontBankGothic = &g_TestFont;
static struct fontchar *ptrFontBankGothicChars = g_TestFont.chars;
static s32 text_spacing;
#define SPACE_WIDTH 5
#define YOFFSET_1 80
#define YINC 15
static s32 g_WatchBackgroundGreen = 0xff;
static s32 watch_item_is_actively_selected;
static s32 g_TestDrawCount;
static char g_TestDrawText[40][64];
static struct { s32 x, y, width, outlined; u32 colour; } g_TestDraw[40];
static Gfx *gfxSetup2DTextureMode(Gfx *gdl) { return gdl; }
static char *langGet(u16 id);
static Gfx *gfxDrawTranslucentRect(Gfx *gdl, s32 x, s32 y, s32 right, s32 bottom, u32 colour)
{ (void)x; (void)y; (void)right; (void)bottom; (void)colour; return gdl; }
static Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text,
        struct fontchar *chars, struct font *font, u32 colour,
        s32 width, s32 height, u32 yOffset, s32 lineheight)
{
    (void)chars; (void)font; (void)colour; (void)yOffset; (void)lineheight;
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Watch text has an empty clipping rectangle: %s\n", text);
        abort();
    }
    assert(*x >= 0 && *x + width <= 320 && *y >= 0 && *y + height <= 240);
    assert(g_TestDrawCount < 40 && strlen(text) < 64);
    g_TestDraw[g_TestDrawCount].x = *x;
    g_TestDraw[g_TestDrawCount].y = *y;
    g_TestDraw[g_TestDrawCount].width = width;
    g_TestDraw[g_TestDrawCount].colour = colour;
    g_TestDraw[g_TestDrawCount].outlined = FALSE;
    strcpy(g_TestDrawText[g_TestDrawCount++], text);
    return gdl;
}
static Gfx *textRenderOutlined(Gfx *gdl, s32 *x, s32 *y, char *text,
        struct fontchar *chars, struct font *font, u32 colour, u32 outlinecolour,
        s32 width, s32 height, u32 yOffset, s32 lineheight)
{
    (void)outlinecolour;
    gdl = textRender(gdl, x, y, text, chars, font, colour, width, height, yOffset, lineheight);
    g_TestDraw[g_TestDrawCount - 1].outlined = TRUE;
    return gdl;
}
