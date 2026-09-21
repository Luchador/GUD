#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "bg.h"
#include "bgdebug.h"
#include "occlusion.h"
#include "bgroomtrans.h"
#include "bondview.h"
#include "cam.h"
#include "dyn.h"
#include "lv.h"
#include "player.h"
#include "textrelated.h"

/* Debug geometry uses spare space in the current frame's master list, not
 * persistent copies of room data or a new allocation from the vertex pool.
 * Leave room for the legend, existing FPS display and task terminators. */
#define BG_DEBUG_GFX_RESERVE 3072
#define BG_DEBUG_STACK_SIZE 16
#define BG_DEBUG_COMMAND_LIMIT 131072
#define BG_DEBUG_LEGEND_ROOMS 18

bool g_BgDebugEnabled = FALSE;
static u8 g_BgDebugSubmitted[MAXROOMCOUNT];
static bool g_BgDebugDrawn;
static bool g_BgDebugIncomplete;
static s32 g_BgDebugRoomCount;

extern u8 *g_BgData;
extern u8 g_ViBackIndex;

typedef struct BgDebugWalk {
    Gfx *out;
    Gfx *end;
    u32 segments[16];
    u32 validVertices;
    s32 remaining;
    bool incomplete;
} BgDebugWalk;

void bgDebugReset(void)
{
    g_BgDebugEnabled = FALSE;
    bgDebugBeginFrame();
}

void bgDebugInput(u16 pressed, bool allowEnable)
{
    if (allowEnable && g_BgDebugEnabled && (pressed & L_JPAD)) { occlusionToggle(); }
    /* Down always wins, including during watch/cutscene transitions. */
    if (pressed & D_JPAD) g_BgDebugEnabled = FALSE;
    else if (allowEnable && (pressed & U_JPAD)) g_BgDebugEnabled = TRUE;
}

void bgDebugBeginFrame(void)
{
    s32 i;
    g_BgDebugDrawn = FALSE;
    g_BgDebugIncomplete = FALSE;
    g_BgDebugRoomCount = 0;
    if (g_BgDebugEnabled) {
        for (i = 0; i < MAXROOMCOUNT; i++) g_BgDebugSubmitted[i] = 0;
    }
}

void bgDebugRecordRoom(s32 room, s32 layer)
{
    if (room > 0 && room < MAXROOMCOUNT) {
        if (!g_BgDebugSubmitted[room]) g_BgDebugRoomCount++;
        g_BgDebugSubmitted[room] |= layer;
    }
}

static u32 bgDebugRoomColor(s32 room)
{
    /* Stable colors as rooms enter/leave the worklist; the legend uses the
     * same RGB at full opacity. Nearby room IDs have contrasting hues. */
    static const u32 colors[] = {
        0xff6060ff, 0x60ffffff, 0xffff60ff, 0xb080ffff,
        0x60ff80ff, 0xffa060ff, 0x6090ffff, 0xff80d0ff,
        0xc0ff80ff, 0x60c0ffff, 0xffc0c0ff, 0xc0c060ff
    };
    return colors[room % (sizeof(colors) / sizeof(colors[0]))];
}

static bool bgDebugResolve(u32 address, const u32 *segments, u32 bytes, u32 *physical)
{
    u32 segment = address >> 24;
    u32 base;
    if (segment == 0x80 || segment == 0xa0) {
        base = address & 0x1fffffff;
    } else {
        if (segment >= 16 || segments[segment] == 0xffffffffu) return FALSE;
        base = segments[segment];
        address &= 0xffffff;
        if (base >= osMemSize || address > osMemSize - base) return FALSE;
        base += address;
    }
    if (base >= osMemSize || bytes > osMemSize - base || (base & 7)) return FALSE;
    *physical = base;
    return TRUE;
}

static bool bgDebugTriangleValid(u32 vertices, s32 a, s32 b, s32 c)
{
    return a < 16 && b < 16 && c < 16
            && (vertices & (1u << a)) && (vertices & (1u << b)) && (vertices & (1u << c));
}

/* Keep only Fast3D vertex loads and triangles. Resolve nested material lists
 * on the CPU so none can re-enable textures, fog, Z or portal scissors. Bounds,
 * stack depth and a shared command budget also cover malformed/cyclic lists.
 * Ignoring culling is intentional: this shows geometry submitted by the CPU,
 * including batches that the RSP or RDP would subsequently reject. */
static void bgDebugCopyGeometry(BgDebugWalk *walk, const Gfx *src, s32 size, s32 depth)
{
    const Gfx *end;
    Gfx command;
    u32 opcode;
    u32 physical;
    s32 count;
    s32 first;
    s32 i;
    s32 a;
    s32 b;
    s32 c;

    if (!src || size < sizeof(Gfx) || (size & 7) || depth >= BG_DEBUG_STACK_SIZE) {
        walk->incomplete = TRUE;
        return;
    }
    end = src + size / sizeof(Gfx);
    while (src < end && walk->remaining-- > 0) {
        command = *src++;
        opcode = command.words.w0 >> 24;
        if (opcode == (u8)G_ENDDL) return;
        if (opcode == (u8)G_DL) {
            if (!bgDebugResolve(command.words.w1, walk->segments, sizeof(Gfx), &physical)) {
                walk->incomplete = TRUE;
                return;
            }
            bgDebugCopyGeometry(walk, (const Gfx *)(physical | 0x80000000),
                    (osMemSize - physical) & ~7, depth + 1);
            if (((command.words.w0 >> 16) & 0xff) == G_DL_NOPUSH) return;
            continue;
        }
        if (opcode == (u8)G_MOVEWORD && (command.words.w0 & 0xff) == G_MW_SEGMENT) {
            walk->segments[((command.words.w0 >> 8) & 0xffff) / 4 & 15] = command.words.w1 & 0x1fffffff;
            continue;
        }
        if (opcode == (u8)G_VTX) {
            first = (command.words.w0 >> 16) & 15;
            count = ((command.words.w0 >> 20) & 15) + 1;
            if (first + count > 16 || (command.words.w0 & 0xffff) != count * sizeof(Vtx)
                    || !bgDebugResolve(command.words.w1, walk->segments, count * sizeof(Vtx), &physical)) {
                walk->incomplete = TRUE;
                return;
            }
            command.words.w1 = physical;
            walk->validVertices |= ((1u << count) - 1) << first;
        } else if (opcode == (u8)G_TRI1) {
            a = (command.words.w1 >> 16) & 255;
            b = (command.words.w1 >> 8) & 255;
            c = command.words.w1 & 255;
            if (a % 10 || b % 10 || c % 10 || !bgDebugTriangleValid(walk->validVertices, a / 10, b / 10, c / 10)) {
                walk->incomplete = TRUE;
                return;
            }
        } else if (opcode == 0xb1) { /* GoldenEye's packed TRI4 */
            for (i = 0; i < 4; i++) {
                a = (command.words.w1 >> (i * 8)) & 15;
                b = (command.words.w1 >> (i * 8 + 4)) & 15;
                c = (command.words.w0 >> (i * 4)) & 15;
                if ((a || b || c) && !bgDebugTriangleValid(walk->validVertices, a, b, c)) {
                    walk->incomplete = TRUE;
                    return;
                }
            }
        } else if (opcode == (u8)G_MTX || opcode == (u8)G_POPMTX || opcode == (u8)G_LINE3D
                || opcode == (u8)G_TEXRECT || opcode == (u8)G_TEXRECTFLIP || opcode == (u8)G_FILLRECT
                || opcode == 0xaf || opcode == 0xb0 || (opcode >= 0xc8 && opcode <= 0xcf)
                || (opcode == (u8)G_MOVEWORD && (command.words.w0 & 0xff) == G_MW_POINTS)) {
            /* These need another transform/geometry decoder. Don't draw
             * misleading geometry or pass arbitrary commands to the RSP. */
            walk->incomplete = TRUE;
            return;
        } else {
            continue;
        }
        if (walk->out >= walk->end) {
            walk->incomplete = TRUE;
            return;
        }
        *walk->out++ = command;
    }
    walk->incomplete = TRUE; /* Missing terminator or command budget exhausted. */
}

Gfx *bgDebugRender(Gfx *gdl)
{
    BgDebugWalk walk;
    RoomInfo *room;
    u32 color;
    s32 i;
    s32 layer;
    s32 freeGfx;

    /* Room submission and the camera here belong to the solo viewport. Watch
     * and cinema views are left alone. No new rooms are loaded by this pass. */
    if (!g_BgDebugEnabled || getPlayerCount() != 1 || lvlGetCurrentStageToLoad() == LEVELID_TITLE
            || !lvGetBgRenderEnabled()
            || g_CameraMode != CAMERAMODE_FP || !g_CurrentPlayer->outside_watch_menu
            || g_CurrentPlayer->pausing_flag || g_CurrentPlayer->pause_state) return gdl;
    g_BgDebugDrawn = TRUE;
    freeGfx = dynGetFreeGfx(gdl);
    if (freeGfx < BG_DEBUG_GFX_RESERVE + 32) {
        g_BgDebugIncomplete = TRUE;
        return gdl;
    }
    walk.end = gdl + freeGfx - BG_DEBUG_GFX_RESERVE;
    walk.remaining = BG_DEBUG_COMMAND_LIMIT;
    walk.incomplete = FALSE;

    gDPPipeSync(gdl++);
    gSPViewport(gdl++, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->viewports[g_ViBackIndex]));
    gSPMatrix(gdl++, OS_K0_TO_PHYSICAL(camGetPlayerProjViewMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPPerspNormalize(gdl++, viGetPerspNorm());
    gSPClipRatio(gdl++, FRUSTRATIO_2);
    gSPClearGeometryMode(gdl++, 0xffffffff);
    gSPSetGeometryMode(gdl++, G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(gdl++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetAlphaDither(gdl++, G_AD_DISABLE);
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, g_CurrentPlayer->viewleft, g_CurrentPlayer->viewtop,
            g_CurrentPlayer->viewleft + g_CurrentPlayer->viewx, g_CurrentPlayer->viewtop + g_CurrentPlayer->viewy);

    for (i = 1; i < MAXROOMCOUNT; i++) {
        if (!g_BgDebugSubmitted[i]) continue;
        room = &g_BgRoomInfo[i];
        if (!room->unloadAge || !room->vertices || walk.end - gdl < 8) {
            walk.incomplete = TRUE;
            continue;
        }
        color = bgDebugRoomColor(i);
        gDPPipeSync(gdl++);
        /* Keep the player's own room faint so its enclosing walls don't
         * dominate the rooms we're trying to inspect through them. */
        gDPSetPrimColor(gdl++, 0, 0, color >> 24, color >> 16, color >> 8,
                i == g_CurrentPlayer->curRoomIndex ? 24 : 72);
        gdl = applyRoomMatrixToDisplayList(gdl, i);
        for (layer = 0; layer < 2; layer++) {
            s32 segment;
            if (!(g_BgDebugSubmitted[i] & (1 << layer))) continue;
            for (segment = 0; segment < 16; segment++) walk.segments[segment] = 0xffffffffu;
            walk.segments[0] = 0;
            walk.segments[SPSEGMENT_BG_VTX] = OS_K0_TO_PHYSICAL(room->vertices);
            walk.segments[SPSEGMENT_BG_DL] = OS_K0_TO_PHYSICAL(g_BgData);
            walk.validVertices = 0;
            walk.out = gdl;
            bgDebugCopyGeometry(&walk, layer ? room->secondaryGdl : room->primaryGdl,
                    layer ? room->secondaryGdlSize : room->primaryGdlSize, 0);
            gdl = walk.out;
        }
    }
    g_BgDebugIncomplete = walk.incomplete;
    gDPPipeSync(gdl++);
    return gdl;
}

static Gfx *bgDebugText(Gfx *gdl, s32 x, s32 y, char *text, u32 color)
{
    s32 n = 0;
    while (text[n]) n++;
    if (dynGetFreeGfx(gdl) < n * 12 + 32) return gdl;
    return textRender(gdl, &x, &y, text, ptrFontBankGothicChars,
            ptrFontBankGothic, color, viGetX(), viGetY(), 0, 0);
}

/* Called after the FPS display establishes text state. */
Gfx *bgDebugDrawHud(Gfx *gdl)
{
    char text[48];
    s32 i;
    s32 shown = 0;
    if (!g_BgDebugDrawn) return gdl;
    sprintf(text, "XRAY BG: %d ROOMS  HERE: %d", g_BgDebugRoomCount, g_CurrentPlayer->curRoomIndex);
    gdl = bgDebugText(gdl, 14, 46, text, 0xffffffff);
    for (i = 1; i < MAXROOMCOUNT && shown < BG_DEBUG_LEGEND_ROOMS; i++) {
        if (!g_BgDebugSubmitted[i]) continue;
        sprintf(text, "%d", i);
        gdl = bgDebugText(gdl, 14 + (shown % 9) * 32, 60 + (shown / 9) * 12, text, bgDebugRoomColor(i));
        shown++;
    }
    if (g_BgDebugIncomplete) {
        gdl = bgDebugText(gdl, 14, 86, "XRAY INCOMPLETE", 0xff6060ff);
    } else if (shown < g_BgDebugRoomCount) {
        sprintf(text, "+%d ROOMS (LEGEND)", g_BgDebugRoomCount - shown);
        gdl = bgDebugText(gdl, 14, 86, text, 0xffffffff);
    }
    if (occlusionCount()) {
        sprintf(text, "OCCL %s: %d BOXES  %d SKIPS", occlusionEnabled() ? "ON" : "OFF",
            occlusionCount(), occlusionRejected());
        gdl = bgDebugText(gdl, 14, 100, text, 0x60ffffff);
        gdl = bgDebugText(gdl, 14, 112, "D-LEFT: TOGGLE OCCLUSION", 0xffffffff);
    }
    return gdl;
}
