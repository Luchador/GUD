#include <ultra64.h>
#include <PR/rcp.h>
#include "renderconfig.h"


static bool g_RenderAaEnabled = TRUE;
static bool g_RenderViFilterEnabled = TRUE;
static bool g_RenderAppliedAaEnabled = TRUE;
static bool g_RenderAppliedViFilterEnabled = TRUE; 
static bool g_RenderFpsCounterEnabled = TRUE;

#define RENDER_LEAF_CACHE_SIZE 1024
#define RENDER_GDL_STACK_SIZE 16
#define RENDER_COMMAND_LIMIT 262144
#define AA_FIRST_BLENDER_MASK 0xcccc0000u
#define AA_OTHER_BITS_MASK (~AA_FIRST_BLENDER_MASK)
#define AA_BLENDER_MASK 0xffff0000u
#define AA_COMMAND_WORD 0xb900031du
#define AA_TAG_MASK 0x00ff0000u
#define AA_TAG_PRESENT 0x80

/* Only ordinary opaque surfaces are changed. Cutouts, particles, translucent
 * surfaces, and custom blender equations retain their authored render modes. */
static const u32 g_AaOpaqueModes[][2] = {
    {G_RM_AA_ZB_OPA_SURF | G_RM_AA_ZB_OPA_SURF2, G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2},
    {G_RM_AA_ZB_OPA_TERR | G_RM_AA_ZB_OPA_TERR2, G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2},
    {G_RM_AA_ZB_OPA_DECAL | G_RM_AA_ZB_OPA_DECAL2, G_RM_ZB_OPA_DECAL | G_RM_ZB_OPA_DECAL2},
    /* Use the same unblended opaque flags without Z. G_RM_OPA_SURF uses
     * FORCE_BL and a different mux, so it cannot retain the authored mux. */
    {G_RM_AA_OPA_SURF | G_RM_AA_OPA_SURF2,
        (G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2) & ~(Z_CMP | Z_UPD)},
    {G_RM_AA_OPA_TERR | G_RM_AA_OPA_TERR2,
        (G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2) & ~(Z_CMP | Z_UPD)}
};

static Gfx *g_RenderLeafCache[RENDER_LEAF_CACHE_SIZE];
extern u8 *g_GfxBuffers[3];
extern u8 *g_VtxBuffers[3];

bool renderIsAaEnabled(void) { return g_RenderAaEnabled; }
/* Use the applied preference: the watch can request a change partway through
 * building a frame, before the graphics queue has drained. */
bool renderUseOneCycle(void) 
{ 
    return !g_RenderAppliedAaEnabled; 
}


bool renderIsViFilterEnabled(void) 
{ 
    return g_RenderViFilterEnabled; 
}


void renderSetAaEnabled(bool enabled)
{
    g_RenderAaEnabled = enabled != FALSE;
}


void renderSetViFilterEnabled(bool enabled)
{
    g_RenderViFilterEnabled = enabled != FALSE;
}


bool renderIsFpsCounterEnabled(void)
{
    return g_RenderFpsCounterEnabled;
}


void renderSetFpsCounterEnabled(bool enabled)
{
    g_RenderFpsCounterEnabled = enabled != FALSE;
}


bool renderSettingsPending(void)
{
    return g_RenderAaEnabled != g_RenderAppliedAaEnabled || g_RenderViFilterEnabled != g_RenderAppliedViFilterEnabled;
}


void renderInvalidateDisplayListCache(void)
{
    s32 i;
    for (i = 0; i < RENDER_LEAF_CACHE_SIZE; i++) g_RenderLeafCache[i] = NULL;
}


void renderApplySettings(void)
{
    if (g_RenderAaEnabled != g_RenderAppliedAaEnabled) {
        g_RenderAppliedAaEnabled = g_RenderAaEnabled;
        renderInvalidateDisplayListCache();
    }
    /* VI-only changes do not affect any RDP commands or cached lists. */
    g_RenderAppliedViFilterEnabled = g_RenderViFilterEnabled;
}


u8 renderEncodeSettings(void)
{
    return 0xc0 | (g_RenderAaEnabled ? 0 : 2) | (g_RenderViFilterEnabled ? 0 : 8) | (g_RenderFpsCounterEnabled ? 0 : 0x10);
}


void renderDecodeSettings(u8 settings)
{
    u8 format = settings & 0xe0;

    renderSetAaEnabled(TRUE);
    renderSetViFilterEnabled(TRUE);
    renderSetFpsCounterEnabled(TRUE);

    if ((format == 0xa0 || format == 0xc0) && (settings & 3) < 3 && ((settings >> 2) & 3) < 3) 
    {
        renderSetAaEnabled((settings & 3) != 2);
        renderSetViFilterEnabled(((settings >> 2) & 3) != 2);

        if (format == 0xc0) 
        {
            renderSetFpsCounterEnabled((settings & 0x10) == 0);
        }
    }
}


void renderConfigureViMode(OSViMode *mode)
{
    u32 control = mode->comRegs.ctrl;
    control &= ~(VI_CTRL_ANTIALIAS_MASK | VI_CTRL_DITHER_FILTER_ON | VI_CTRL_DIVOT_ON);
    if (g_RenderAppliedViFilterEnabled) {
        control |= VI_CTRL_DITHER_FILTER_ON | VI_CTRL_DIVOT_ON;
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
    cmd->words.w1 = (cmd->words.w1 & AA_BLENDER_MASK) | (original & ~AA_BLENDER_MASK);
    cmd->words.w0 = AA_COMMAND_WORD;
}


static void renderDisableAaCommand(Gfx *cmd)
{
    u32 original;
    u32 replacement;
    u32 tag;
    s32 i;

    /* Tagged commands already contain their Off value. Leave them untouched,
     * including when a previous graphics task still references this list. */
    if (cmd->words.w0 != AA_COMMAND_WORD) 
    {
        return;
    }

    original = cmd->words.w1;
    for (i = 0; i < 5; i++) 
    {
        if ((original & AA_OTHER_BITS_MASK) == (g_AaOpaqueModes[i][0] & AA_OTHER_BITS_MASK)) 
        {
            tag = AA_TAG_PRESENT | i;
            replacement = g_AaOpaqueModes[i][1];
            /* Opaque AA-Off pixels do not need a different blender mux.
             * Preserve BOTH cycles: later partial surface writes can enable
             * translucency without rewriting the inherited mux. In particular,
             * TERR's 1MA must not become SURF's framebuffer-coverage factor. */
            cmd->words.w1 = (original & AA_BLENDER_MASK) | (replacement & ~AA_BLENDER_MASK);
            cmd->words.w0 |= tag << 16;
            return;
        }
    }
}


Gfx renderGetAaOffCommand(Gfx command)
{
    renderDisableAaCommand(&command);
    return command;
}


static void renderApplyAaCommand(Gfx *cmd)
{
    if (g_RenderAppliedAaEnabled) renderRestoreAaCommand(cmd);
    else renderDisableAaCommand(cmd);
}


void renderRestoreDisplayListSettings(Gfx *start, Gfx *end)
{
    Gfx *cmd;
    renderInvalidateDisplayListCache();
    if (!start) return;
    for (cmd = start; end ? cmd < end : (cmd->words.w0 >> 24) != (u8)G_ENDDL; cmd++) {
        renderRestoreAaCommand(cmd);
    }
}


bool renderListIsDynamic(Gfx *gdl)
{
    u32 address = (u32)gdl;
    return (address >= (u32)g_GfxBuffers[0] && address < (u32)g_GfxBuffers[2])
            || (address >= (u32)g_VtxBuffers[0] && address < (u32)g_VtxBuffers[2]);
}


static Gfx *renderResolveDisplayListAddress(u32 address, u32 *segments)
{
    u32 physical;

    if (address & 0x80000000) 
    {
        physical = address & 0x1fffffff;
    } 
    else 
    {
        if ((address >> 24) >= 16) 
        {
            return NULL;
        }
    
        physical = segments[address >> 24] + (address & 0xffffff);
    }

    if ((physical & 7) || physical >= osMemSize) 
    {
        return NULL;
    }

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
        }
    }
    return depth < 0;
}
