/* Exercise the actual clipping/cache/input functions with recorded commands. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ultra64.h"
typedef s32 bool;
typedef struct { s32 kind, left, top, right, bottom; } Gfx;
enum { SCISSOR = 1, FILL, NOOP, STATE };
static struct player {
    s32 viewleft, viewtop, viewx, viewy;
    s32 watch_animation_state, frozencam;
} player = {0, 20, 320, 200, 0, 0}, *g_CurrentPlayer = &player;
static struct {
    bool enabled, valid;
    s32 left, top, right, bottom;
} g_BgScissorCache;
static bool g_BgScissorTestEnabled, g_LvScissorHalfWidth;
static s32 g_BgScissorTestLeft, g_BgScissorTestRight;
static s32 g_ControlsLockedFlag, paused, buttons, pressed;
#define PLAYER_1 0
#define L_TRIG 0x20
#define R_TRIG 0x10
#define U_JPAD 0x800
#define G_SC_NON_INTERLACE 0
#define G_CYC_FILL 0
#define G_RM_NOOP 0
#define G_RM_NOOP2 0
#define GPACK_RGBA5551(r,g,b,a) (a)
static void record(Gfx *p, s32 kind, s32 left, s32 top, s32 right, s32 bottom)
{
    *p = (Gfx){kind, left, top, right, bottom};
}
#define gDPSetScissor(p,m,l,t,r,b) record(p, SCISSOR, l,t,r,b)
#define gDPFillRectangle(p,l,t,r,b) record(p, FILL, l,t,r,b)
#define gDPNoOp(p) record(p, NOOP, 0,0,0,0)
#define gDPPipeSync(p) record(p, STATE, 0,0,0,0)
#define gDPSetCycleType(p,c) record(p, STATE, 0,0,0,0)
#define gDPSetRenderMode(p,m,n) record(p, STATE, 0,0,0,0)
#define gDPSetFillColor(p,c) record(p, STATE, 0,0,0,0)
static s32 viGetViewLeft(void) { return player.viewleft; }
static s32 viGetViewTop(void) { return player.viewtop; }
static s32 viGetViewWidth(void) { return player.viewx; }
static s32 viGetViewHeight(void) { return player.viewy; }
static s32 checkGamePaused(void) { return paused; }
static s32 joyGetButtons(s32 pad, s32 mask) { (void)pad; return buttons & mask; }
static s32 joyGetButtonsPressedThisFrame(s32 pad, s32 mask)
{ (void)pad; return pressed & mask; }
Gfx *bgScissorCurrentPlayerView(Gfx *, s32, s32, s32, s32);
static Gfx *bgScissorCurrentPlayerViewDefault(Gfx *gdl)
{
    return bgScissorCurrentPlayerView(gdl, player.viewleft, player.viewtop,
            player.viewleft + player.viewx, player.viewtop + player.viewy);
}
#include "scissor_functions.inc"

int main(void)
{
    Gfx full[16], half[16], *end;
    s32 l, r, x, outLeft, outRight;

    /* For every interval in and around a normal viewport, the emitted
     * interval must cover exactly the original pixels inside [80,240). */
    bgSetScissorTest(TRUE);
    for (l = -4; l <= 324; l++) {
        for (r = l; r <= 324; r++) {
            outLeft = l; outRight = r;
            bgClampScissorTest(&outLeft, &outRight);
            assert(outLeft >= 80 && outLeft <= outRight && outRight <= 240);
            for (x = 0; x < 320; x++) {
                assert((x >= outLeft && x < outRight)
                        == (x >= l && x < r && x >= 80 && x < 240));
            }
        }
    }
    /* A non-default viewport gets its own centered half-width interval. */
    player.viewleft = 12; player.viewx = 440;
    bgSetScissorTest(TRUE);
    l = 0; r = 500; bgClampScissorTest(&l, &r);
    assert(l == 122 && r == 342);
    bgSetScissorTest(FALSE);
    l = -4; r = 500; bgClampScissorTest(&l, &r);
    assert(l == -4 && r == 500);
    player.viewleft = 0; player.viewx = 320;

    /* Cache the original bounds: two distinct rooms that both clip to zero
     * must still emit two commands, and repeat calls must still be cached. */
    g_BgScissorCache.enabled = TRUE;
    bgSetScissorTest(TRUE);
    end = bgScissorCurrentPlayerView(half, 0, 20, 40, 220);
    end = bgScissorCurrentPlayerView(end, 10, 20, 50, 220);
    assert(end == half + 2);
    assert(!memcmp(half, half + 1, sizeof(Gfx)));
    end = bgScissorCurrentPlayerView(end, 10, 20, 50, 220);
    assert(end == half + 2);
    bgSetScissorTest(FALSE);
    end = bgScissorCurrentPlayerView(end, 10, 20, 50, 220);
    assert(end == half + 3 && half[2].left == 10 && half[2].right == 50);
    g_BgScissorCache.enabled = FALSE;

    /* Fill commands use inclusive bounds and cannot rely on RDP scissoring.
     * Empty fills become one no-op, never a negative/inverted rectangle. */
    bgSetScissorTest(TRUE);
    end = bgFillScissorTestRectangle(half, 0, 20, 319, 219);
    assert(end == half + 1 && half[0].kind == FILL);
    assert(half[0].left == 80 && half[0].right == 239);
    end = bgFillScissorTestRectangle(half, 0, 20, 79, 219);
    assert(end == half + 1 && half[0].kind == NOOP);
    end = bgFillScissorTestRectangle(half, 240, 20, 319, 219);
    assert(end == half + 1 && half[0].kind == NOOP);
    end = bgFillScissorTestRectangle(half, 79, 20, 80, 219);
    assert(end == half + 1 && half[0].left == 80 && half[0].right == 80);

    /* Both modes clear the same full color area and emit the same number of
     * commands. Only the last scissor's horizontal bounds change. */
    bgSetScissorTest(FALSE);
    end = lvBeginScissorTest(full);
    assert(end == full + 7 && !g_BgScissorTestEnabled);
    buttons = L_TRIG | R_TRIG; pressed = U_JPAD;
    end = lvBeginScissorTest(half);
    assert(end == half + 7 && g_LvScissorHalfWidth && g_BgScissorTestEnabled);
    assert(!memcmp(full, half, 6 * sizeof(Gfx)));
    assert(full[4].kind == FILL && full[4].left == 0 && full[4].right == 319);
    assert(full[6].right == 320 && half[6].left == 80 && half[6].right == 240);
    pressed = 0;
    lvBeginScissorTest(half);
    assert(g_LvScissorHalfWidth); /* Holding Up cannot toggle every frame. */
    pressed = U_JPAD;
    player.watch_animation_state = 1; lvBeginScissorTest(half);
    assert(g_LvScissorHalfWidth);
    player.watch_animation_state = 0; g_ControlsLockedFlag = 1;
    lvBeginScissorTest(half); assert(g_LvScissorHalfWidth);
    g_ControlsLockedFlag = 0; paused = 1;
    lvBeginScissorTest(half); assert(g_LvScissorHalfWidth);
    paused = 0; player.frozencam = 1;
    lvBeginScissorTest(half); assert(g_LvScissorHalfWidth);
    player.frozencam = 0;
    lvBeginScissorTest(full); assert(!g_LvScissorHalfWidth);
    puts("Scissor intersections, cache parity, fill clipping and A/B controls passed");
    return 0;
}
