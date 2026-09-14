#include <ultra64.h>
#include <PR/rcp.h>
#include "renderconfig.h"

/* Linked into resident .code: renderConfigureViMode runs on scheduler retraces,
 * including those before the main thread initializes game demand paging. */

/* Player-facing preferences survive stage changes. Their applied copies change
 * only with an empty graphics queue, since cached display lists are shared. */
static s32 g_RenderAaStyle = RENDER_AA_FULL;
static s32 g_RenderViFilter = RENDER_VI_SMOOTH;
static s32 g_RenderAppliedAa = RENDER_AA_FULL;
static s32 g_RenderAppliedVi = RENDER_VI_SMOOTH;
static s32 g_RenderColorDither = RENDER_COLOR_DITHER_DEFAULT;
static s32 g_RenderAppliedColorDither = RENDER_COLOR_DITHER_DEFAULT;

#define RENDER_LEAF_CACHE_SIZE 1024
#define RENDER_GDL_STACK_SIZE 16
#define RENDER_COMMAND_LIMIT 262144
#define AA_FIRST_BLENDER_MASK 0xcccc0000u
#define AA_OTHER_BITS_MASK (~AA_FIRST_BLENDER_MASK)
#define AA_COMMAND_WORD 0xb900031du
#define AA_TAG_MASK 0x00ff0000u
#define AA_TAG_PRESENT 0x80
#define AA_TAG_FIRST_BLENDER 0x20
#define COLOR_DITHER_MASK (3u << G_MDSFT_RGBDITHER)
#define COLOR_DITHER_TAG 0x80

/* Only ordinary opaque surfaces are changed. Cutouts, particles, translucent
 * surfaces, and custom blender equations retain their authored render modes. */
static const u32 g_AaOpaqueModes[][2] = {
    {G_RM_AA_ZB_OPA_SURF | G_RM_AA_ZB_OPA_SURF2, G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2},
    {G_RM_AA_ZB_OPA_TERR | G_RM_AA_ZB_OPA_TERR2, G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2},
    {G_RM_AA_ZB_OPA_DECAL | G_RM_AA_ZB_OPA_DECAL2, G_RM_ZB_OPA_DECAL | G_RM_ZB_OPA_DECAL2},
    {G_RM_AA_OPA_SURF | G_RM_AA_OPA_SURF2, G_RM_OPA_SURF | G_RM_OPA_SURF2},
    {G_RM_AA_OPA_TERR | G_RM_AA_OPA_TERR2, G_RM_OPA_SURF | G_RM_OPA_SURF2}
};

static Gfx *g_RenderLeafCache[RENDER_LEAF_CACHE_SIZE];
extern u8 *g_GfxBuffers[3];
extern u8 *g_VtxBuffers[3];

s32 renderGetAaStyle(void) { return g_RenderAaStyle; }
/* Use the applied preference: the watch can request a change partway through
 * building a frame, before the graphics queue has drained. */
bool renderUseOneCycleBackground(void) { return g_RenderAppliedAa == RENDER_AA_OFF; }
s32 renderGetViFilter(void) { return g_RenderViFilter; }
s32 renderGetColorDither(void) { return g_RenderColorDither; }

void renderSetAaStyle(s32 style)
{
    if (style >= 0 && style < RENDER_AA_COUNT) g_RenderAaStyle = style;
}

void renderSetViFilter(s32 filter)
{
    if (filter >= 0 && filter < RENDER_VI_COUNT) g_RenderViFilter = filter;
}

void renderSetColorDither(s32 dither)
{
    if (dither >= 0 && dither < RENDER_COLOR_DITHER_COUNT) g_RenderColorDither = dither;
}

bool renderSettingsPending(void)
{
    return g_RenderAaStyle != g_RenderAppliedAa || g_RenderViFilter != g_RenderAppliedVi
            || g_RenderColorDither != g_RenderAppliedColorDither;
}

void renderInvalidateDisplayListCache(void)
{
    s32 i;
    for (i = 0; i < RENDER_LEAF_CACHE_SIZE; i++) g_RenderLeafCache[i] = NULL;
}

void renderApplySettings(void)
{
    if (renderSettingsPending()) {
        g_RenderAppliedAa = g_RenderAaStyle;
        g_RenderAppliedVi = g_RenderViFilter;
        g_RenderAppliedColorDither = g_RenderColorDither;
        renderInvalidateDisplayListCache();
    }
}

u8 renderEncodeSettings(void)
{
    /* Bits 7..5 are the format marker; bit 4 stores dither Off. Previous
     * AA/VI saves have bit 4 clear and therefore retain Default dithering. */
    return 0xa0 | g_RenderAaStyle | (g_RenderViFilter << 2) | (g_RenderColorDither << 4);
}

void renderDecodeSettings(u8 settings)
{
    renderSetAaStyle(RENDER_AA_FULL);
    renderSetViFilter(RENDER_VI_SMOOTH);
    renderSetColorDither(RENDER_COLOR_DITHER_DEFAULT);
    if ((settings & 0xe0) == 0xa0 && (settings & 3) < RENDER_AA_COUNT
            && ((settings >> 2) & 3) < RENDER_VI_COUNT) {
        renderSetAaStyle(settings & 3);
        renderSetViFilter((settings >> 2) & 3);
        renderSetColorDither((settings >> 4) & 1);
    }
}

void renderConfigureViMode(OSViMode *mode)
{
    u32 control = mode->comRegs.ctrl;
    control &= ~(VI_CTRL_ANTIALIAS_MASK | VI_CTRL_DITHER_FILTER_ON | VI_CTRL_DIVOT_ON);
    if (g_RenderAppliedVi == RENDER_VI_SMOOTH) {
        control |= VI_CTRL_DITHER_FILTER_ON | VI_CTRL_DIVOT_ON;
    } else if (g_RenderAppliedVi == RENDER_VI_EDGES) {
        control |= 0x100 | VI_CTRL_DIVOT_ON; /* AA, fetch extra lines only when needed. */
    } else {
        /* Point-sampled VI modes: keep the resampling appropriate to pixel size. */
        control |= (control & 3) == VI_CTRL_TYPE_16 ? 0x200 : 0x300;
    }
    mode->comRegs.ctrl = control;
}

/* Fast3D's SetOtherMode command ignores bits 16..23 of word 0. Keep the
 * original opaque mode's table index there while a command is modified. This
 * makes switching back exact without retaining pointers to unloaded assets.
 * This metadata is RAM-only, consumes no extra GBI commands, and is removed
 * before the game's exact-command LUTs examine a display list. */
static void renderRestoreAaCommand(Gfx *cmd)
{
    u32 tag = (cmd->words.w0 >> 16) & 0xff;
    u32 index = tag & 0x1f;
    u32 original;
    if ((cmd->words.w0 & ~AA_TAG_MASK) != AA_COMMAND_WORD
            || !(tag & AA_TAG_PRESENT) || index >= 5) return;
    original = g_AaOpaqueModes[index][0];
    cmd->words.w1 = (cmd->words.w1 & AA_FIRST_BLENDER_MASK) | (original & AA_OTHER_BITS_MASK);
    if (tag & AA_TAG_FIRST_BLENDER) {
        cmd->words.w1 = (cmd->words.w1 & AA_OTHER_BITS_MASK) | (original & AA_FIRST_BLENDER_MASK);
    }
    cmd->words.w0 = AA_COMMAND_WORD;
}

static void renderApplyAaStyleToCommand(Gfx *cmd, s32 style)
{
    u32 original;
    u32 replacement;
    u32 tag;
    s32 i;
    Gfx canonical = *cmd;
    renderRestoreAaCommand(&canonical);
    if (style == RENDER_AA_FULL) {
        if (cmd->words.w0 != canonical.words.w0) *cmd = canonical;
        return;
    }
    if (canonical.words.w0 != AA_COMMAND_WORD) return;
    original = canonical.words.w1;
    for (i = 0; i < 5; i++) {
        if ((original & AA_OTHER_BITS_MASK) == (g_AaOpaqueModes[i][0] & AA_OTHER_BITS_MASK)) {
            tag = AA_TAG_PRESENT | i;
            replacement = style == RENDER_AA_REDUCED
                    ? g_AaOpaqueModes[i][0] & ~IM_RD : g_AaOpaqueModes[i][1];
            if ((original & AA_FIRST_BLENDER_MASK) == (g_AaOpaqueModes[i][0] & AA_FIRST_BLENDER_MASK)) {
                tag |= AA_TAG_FIRST_BLENDER;
                canonical.words.w1 = replacement;
            } else {
                /* Retain the first cycle's fog/pass blender. */
                canonical.words.w1 = (original & AA_FIRST_BLENDER_MASK) | (replacement & AA_OTHER_BITS_MASK);
            }
            canonical.words.w0 |= tag << 16;
            if (cmd->words.w0 != canonical.words.w0 || cmd->words.w1 != canonical.words.w1) {
                *cmd = canonical;
            }
            return;
        }
    }
}

Gfx renderGetAaOffCommand(Gfx command)
{
    renderApplyAaStyleToCommand(&command, RENDER_AA_OFF);
    return command;
}

static void renderApplyAaCommand(Gfx *cmd)
{
    renderApplyAaStyleToCommand(cmd, g_RenderAppliedAa);
}

/* RGB dithering lives in SetOtherMode H, independently of AA's L command.
 * As with AA, its unused command byte remembers the authored choice. Handle
 * both gDPSetColorDither and H writes covering the complete RGB-dither field;
 * leave alpha dithering, texture filtering and all other state bits untouched. */
static bool renderCommandSetsColorDither(Gfx *cmd)
{
    u32 shift = (cmd->words.w0 >> 8) & 0xff;
    u32 length = cmd->words.w0 & 0xff;
    return (cmd->words.w0 >> 24) == (u8)G_SETOTHERMODE_H
            && shift <= G_MDSFT_RGBDITHER && length <= 32
            && shift + length >= G_MDSFT_RGBDITHER + 2;
}

static void renderRestoreColorDitherCommand(Gfx *cmd)
{
    u32 tag = (cmd->words.w0 >> 16) & 0xff;
    if (!renderCommandSetsColorDither(cmd) || (tag & ~3u) != COLOR_DITHER_TAG) return;
    cmd->words.w1 = (cmd->words.w1 & ~COLOR_DITHER_MASK) | ((tag & 3) << G_MDSFT_RGBDITHER);
    cmd->words.w0 &= ~AA_TAG_MASK;
}

static void renderApplyColorDitherCommand(Gfx *cmd)
{
    u32 original;
    Gfx canonical;
    if (!renderCommandSetsColorDither(cmd)) return;
    canonical = *cmd;
    renderRestoreColorDitherCommand(&canonical);
    if (canonical.words.w0 & AA_TAG_MASK) return; /* Unrecognized metadata. */
    original = canonical.words.w1 & COLOR_DITHER_MASK;
    if (g_RenderAppliedColorDither == RENDER_COLOR_DITHER_OFF && original != G_CD_DISABLE) {
        canonical.words.w0 |= (COLOR_DITHER_TAG | (original >> G_MDSFT_RGBDITHER)) << 16;
        canonical.words.w1 = (canonical.words.w1 & ~COLOR_DITHER_MASK) | G_CD_DISABLE;
    }
    /* Static lists may also be used by the previous task. Never transiently
     * restore/rewrite an already-correct command during steady-state frames. */
    if (cmd->words.w0 != canonical.words.w0 || cmd->words.w1 != canonical.words.w1) *cmd = canonical;
}

void renderRestoreDisplayListSettings(Gfx *start, Gfx *end)
{
    Gfx *cmd;
    renderInvalidateDisplayListCache();
    if (!start) return;
    for (cmd = start; end ? cmd < end : (cmd->words.w0 >> 24) != (u8)G_ENDDL; cmd++) {
        renderRestoreAaCommand(cmd);
        renderRestoreColorDitherCommand(cmd);
    }
}

static bool renderListIsDynamic(Gfx *gdl)
{
    u32 address = (u32)gdl;
    return (address >= (u32)g_GfxBuffers[0] && address < (u32)g_GfxBuffers[2])
            || (address >= (u32)g_VtxBuffers[0] && address < (u32)g_VtxBuffers[2]);
}

static Gfx *renderResolveDisplayListAddress(u32 address, u32 *segments)
{
    u32 physical;
    if (address & 0x80000000) {
        physical = address & 0x1fffffff;
    } else {
        if ((address >> 24) >= 16) return NULL;
        physical = segments[address >> 24] + (address & 0xffffff);
    }
    if ((physical & 7) || physical >= osMemSize) return NULL;
    return (Gfx *)(physical | 0x80000000);
}

bool renderApplyDisplayListSettings(Gfx *start, Gfx *end)
{
    struct RenderListState {
        Gfx *start;
        Gfx *cmd;
        Gfx *end;
        bool cacheable;
    } stack[RENDER_GDL_STACK_SIZE];
    struct RenderListState *state;
    Gfx *cmd;
    Gfx *child;
    u32 segments[16] = {0};
    u32 opcode;
    u32 slot;
    s32 depth = 0;
    s32 remaining = RENDER_COMMAND_LIMIT;

    stack[0].start = start;
    stack[0].cmd = start;
    stack[0].end = end;
    stack[0].cacheable = FALSE;
    while (depth >= 0 && remaining-- > 0) {
        state = &stack[depth];
        cmd = state->cmd;
        if (state->end && cmd >= state->end) { depth--; continue; }
        if (((u32)cmd & 0x1fffffff) + sizeof(Gfx) > osMemSize) return FALSE;
        state->cmd++;
        opcode = cmd->words.w0 >> 24;
        if (opcode == (u8)G_ENDDL) {
            if (state->cacheable) {
                slot = ((u32)state->start >> 3) & (RENDER_LEAF_CACHE_SIZE - 1);
                g_RenderLeafCache[slot] = state->start;
            }
            depth--;
        } else if (opcode == (u8)G_MOVEWORD && (cmd->words.w0 & 0xff) == G_MW_SEGMENT) {
            state->cacheable = FALSE;
            segments[((cmd->words.w0 >> 8) & 0xffff) / 4 & 15] = cmd->words.w1 & 0x1fffffff;
        } else if (opcode == (u8)G_DL) {
            state->cacheable = FALSE;
            child = renderResolveDisplayListAddress(cmd->words.w1, segments);
            if (!child) return FALSE;
            slot = ((u32)child >> 3) & (RENDER_LEAF_CACHE_SIZE - 1);
            if (!renderListIsDynamic(child) && g_RenderLeafCache[slot] == child) {
                if (((cmd->words.w0 >> 16) & 0xff) == G_DL_NOPUSH) depth--;
                continue;
            }
            if (((cmd->words.w0 >> 16) & 0xff) != G_DL_NOPUSH) {
                if (++depth == RENDER_GDL_STACK_SIZE) return FALSE;
                state = &stack[depth];
            }
            state->start = child;
            state->cmd = child;
            state->end = NULL;
            state->cacheable = !renderListIsDynamic(child);
        } else if (opcode == (u8)G_SETOTHERMODE_L) {
            renderApplyAaCommand(cmd);
        } else if (opcode == (u8)G_SETOTHERMODE_H) {
            renderApplyColorDitherCommand(cmd);
        }
    }
    return depth < 0;
}
