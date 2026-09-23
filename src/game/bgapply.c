#include "renderconfig.h"
#include "bgtransparency.h"
#include <ultra64.h>
#include <PR/os.h>
#include <PR/gbi.h>
#include <bondconstants.h>
#include <fr.h>
#include <memp.h>
#include "bg.h"
#include "bondview.h"
#include "environment.h"
#include "gmath.h"
#include "lv.h"
#include "matrixmath.h"
#include "player.h"
#include "explosion.h"
#include "bgroomtrans.h"

/* Override only alpha after the material LUT. In two-cycle materials the
 * first alpha cycle samples the base image (including mip interpolation),
 * and the second applies the requested factor. Detail alpha comes from the
 * base tile, never the separate detail image. RGB remains authored. */
static void bgApplyAlphaPreset(Gfx *command, u32 source, u32 high)
{
    u32 factor = BG_ALPHA_USES_VERTEX(source) ? G_ACMUX_SHADE
        : BG_ALPHA_USES_CONSTANT(source) ? G_ACMUX_ENVIRONMENT : G_ACMUX_1;
    u32 a0 = G_ACMUX_0, b0 = G_ACMUX_0, c0 = G_ACMUX_0, d0 = factor;
    u32 a1 = G_ACMUX_0, b1 = G_ACMUX_0, c1 = G_ACMUX_0, d1 = factor;

    if (BG_ALPHA_USES_TEXTURE(source))
    {
        if ((high & (3u << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE)
        {
            d0 = G_ACMUX_TEXEL0;
            if (high & G_TL_LOD)
            {
                if ((high & (3u << G_MDSFT_TEXTDETAIL)) == G_TD_DETAIL)
                { d0 = G_ACMUX_TEXEL1; }
                else
                {
                    a0 = G_ACMUX_TEXEL1; b0 = G_ACMUX_TEXEL0;
                    c0 = G_ACMUX_LOD_FRACTION;
                }
            }
            d1 = G_ACMUX_COMBINED;
            if (source != BG_ALPHA_TEXTURE)
            { a1 = G_ACMUX_COMBINED; c1 = factor; d1 = G_ACMUX_0; }
        }
        else
        {
            d0 = d1 = G_ACMUX_TEXEL0;
            if (source != BG_ALPHA_TEXTURE)
            { a0 = a1 = G_ACMUX_TEXEL0; c0 = c1 = factor; d0 = d1 = G_ACMUX_0; }
        }
    }
    command->words.w0 = (command->words.w0 & ~0x00007e00u) | (a0 << 12) | (c0 << 9);
    command->words.w1 = (command->words.w1 & ~0x00fc7e3fu)
        | (a1 << 21) | (c1 << 18) | (b0 << 12) | (d0 << 9) | (b1 << 3) | d1;
}

/* A supported one-cycle material repeats its active equation in both mux
 * cycles. Keep the first equation and make cycle two pass COMBINED, avoiding
 * the two-cycle TEXEL0/TEXEL1 swap. Fog needs SHADE alpha, so standard authored
 * alpha references use ENVIRONMENT just as the ordinary BG alpha LUT does. */
static void bgPromoteFogCombine(Gfx *command)
{
    u32 w0 = command->words.w0, w1 = command->words.w1;
    u32 a = (w1 >> 21) & 7, c = (w1 >> 18) & 7;
    u32 b = (w1 >> 3) & 7, d = w1 & 7;
    if (a == G_ACMUX_SHADE) { a = G_ACMUX_ENVIRONMENT; }
    if (b == G_ACMUX_SHADE) { b = G_ACMUX_ENVIRONMENT; }
    if (c == G_ACMUX_SHADE) { c = G_ACMUX_ENVIRONMENT; }
    if (d == G_ACMUX_SHADE) { d = G_ACMUX_ENVIRONMENT; }
    command->words.w0 = (w0 & ~0x00007fffu) | (a << 12) | (c << 9) | 0x1ffu;
    command->words.w1 = (w1 & 0xf0038000u) | (b << 12) | (d << 9) | 0x0ffc0038u;
}


/**
 * Scan the Gfx commands in the range starting at 'start'. If 'end' is non-NULL
 * treat it as a one-past-last pointer and scan [start, end). If 'end' is NULL
 * treat 'start' as a null-terminated display list and scan until the sentinel
 * command G_ENDDL is encountered.
 *
 * For each Gfx in the scanned range, look up replacement entries in the
 * runtime-selected LUT: ptrDynamic_CC_RM_LUT[lutIndex]. The LUT is organized as
 * (match,replacement) pairs (two Gfx entries per pair), terminated by an
 * entry whose words.w0 == 0. When a match is found replace the command with
 * the LUT's replacement. Replacements are counted in a static counter for
 * telemetry/debugging.
 *
 * Parameters:
 *   start   - pointer to first Gfx to scan
 *   end     - pointer one-past-last Gfx to scan, or NULL to use G_ENDDL sentinel
 *   lutIndex- index into ptrDynamic_CC_RM_LUT selecting the active LUT
 */
void bgApplyDynamicCCRMLUT(Gfx *start, Gfx *end, enum CCRMLUT lutIndex)
{
    Gfx *curGfx;
    Gfx *lutPair;
    u32 alphaSource = BG_ALPHA_AUTO;
    u32 fogPolicy = BG_FOG_AUTO;
    u32 surfacePolicy = BG_SURFACE_AUTO;
    u32 low = 0;
    u32 high = 0;
    u32 levelFog = envGetCurrent()->FogEnabled;
    u32 fog = levelFog ? G_FOG : 0;
    u32 opcode;
    u32 kind;
    u32 shift;
    u32 length;
    u32 mask;

    static s32 s_bg_lut_replacement_count = 0;

    renderRestoreDisplayListSettings(start, end);
    curGfx = start;

    /* Loop until end pointer or sentinel G_ENDDL (when end==NULL) */
    while (((end != NULL) && (curGfx < end)) || ((end == NULL) && ((curGfx->words.w0 >> 24) != (u8)G_ENDDL)))
    {
        u32 promote = fogPolicy == BG_FOG_ON && levelFog
            && (high & (3u << G_MDSFT_CYCLETYPE)) == G_CYC_1CYCLE;
        if (BG_FOG_IS_MARKER(curGfx->words.w0, curGfx->words.w1))
        {
            kind = curGfx->words.w1 & 255u;
            if (kind <= BG_FOG_OFF) { fogPolicy = kind; }
            else if (kind == BG_FOG_CYCLE)
            { gDPSetCycleType(curGfx, promote ? G_CYC_2CYCLE : high & (3u << G_MDSFT_CYCLETYPE)); }
            else if (kind >= BG_FOG_BLENDER && kind <= BG_FOG_LAST_SLOT)
            {
                u32 blender = promote ? (low & 0xcccc0000u) >> 2 : low;
                shift = 16 + 4 * (kind - BG_FOG_BLENDER);
                gSPSetOtherMode(curGfx, G_SETOTHERMODE_L, shift, 2, blender & (3u << shift));
            }
            curGfx++;
            continue;
        }
        if (BG_ALPHA_IS_MARKER(curGfx->words.w0, curGfx->words.w1))
        {
            kind = BG_ALPHA_TAG_KIND(curGfx->words.w1);
            if (BG_ALPHA_IS_PRESET(kind)) { alphaSource = kind; }
            else if (kind == BG_ALPHA_SYNC) { gDPPipeSync(curGfx); }
            else if (kind == BG_ALPHA_FOG)
            {
                u32 enabled = fogPolicy == BG_FOG_AUTO ? fog
                    : fogPolicy == BG_FOG_ON && levelFog;
                if (!BG_ALPHA_USES_VERTEX(alphaSource) && enabled) { gSPSetGeometryMode(curGfx, G_FOG); }
                else { gSPClearGeometryMode(curGfx, G_FOG); }
            }
            else if (kind >= BG_ALPHA_BLENDER && kind <= BG_ALPHA_LAST_SLOT)
            {
                /* The first blender's four fields interleave the second's.
                 * Write them separately so later AA toggles can still change
                 * the final surface blender. One-cycle draws need only the
                 * geometry fog change; leave their final blender untouched. */
                u32 blender = low;
                if (promote || (high & (3u << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE)
                {
                    if (BG_ALPHA_USES_VERTEX(alphaSource) || fogPolicy == BG_FOG_OFF)
                    { blender = G_RM_PASS; }
                    else if (fogPolicy == BG_FOG_ON)
                    { blender = levelFog ? G_RM_FOG_SHADE_A : G_RM_PASS; }
                }
                shift = 18 + 4 * (kind - BG_ALPHA_BLENDER);
                gSPSetOtherMode(curGfx, G_SETOTHERMODE_L, shift, 2, blender & (3u << shift));
            }
            /* Slots are generated commands, not changes to the authored
             * state. Retain the policy marker for the one-cycle converter. */
            curGfx++;
            continue;
        }
        if (BG_SURFACE_IS_MARKER(curGfx->words.w0, curGfx->words.w1))
        { surfacePolicy = BG_SURFACE_TAG_POLICY(curGfx->words.w1); }
        if (surfacePolicy == BG_SURFACE_CUTOUT && alphaSource == BG_ALPHA_AUTO)
        {
            u32 w0 = curGfx->words.w0, w1 = curGfx->words.w1;
            curGfx->words.w0 = BG_CUTOUT_COMBINE_W0(w0, w1);
            curGfx->words.w1 = BG_CUTOUT_COMBINE_W1(w0, w1);
        }
        for (lutPair = ptrDynamic_CC_RM_LUT[(s32)lutIndex]; lutPair->words.w0 != 0; lutPair += 2)
        {
            if ((lutPair->words.w0 == curGfx->words.w0) && (lutPair->words.w1 == curGfx->words.w1))
            {
                s_bg_lut_replacement_count += 1;
                *curGfx = *(lutPair + 1);
            }
        }

        opcode = curGfx->words.w0 >> 24;
        if (opcode == (u8)G_SETOTHERMODE_L || opcode == (u8)G_SETOTHERMODE_H)
        {
            shift = (curGfx->words.w0 >> 8) & 0xff;
            length = curGfx->words.w0 & 0xff;
            if (length && shift < 32 && length <= 32 - shift)
            {
                mask = (0xffffffffu >> (32 - length)) << shift;
                if (opcode == (u8)G_SETOTHERMODE_L)
                { low = (low & ~mask) | (curGfx->words.w1 & mask); }
                else { high = (high & ~mask) | (curGfx->words.w1 & mask); }
            }
        }
        else if (opcode == (u8)G_RDPSETOTHERMODE)
        { low = curGfx->words.w1; high = curGfx->words.w0 & 0xffffff; }
        else if (opcode == (u8)G_SETGEOMETRYMODE) { fog |= curGfx->words.w1 & G_FOG; }
        else if (opcode == (u8)G_CLEARGEOMETRYMODE) { fog &= ~curGfx->words.w1; }
        else if (opcode == (u8)G_SETCOMBINE)
        {
            /* An explicit fog choice must not turn standard material opacity
             * into the RSP fog factor (or painted A when fog is disabled).
             * Use the ordinary BG alpha LUT for Auto as the preview does.
             * Its entries change alpha only; custom equations stay intact. */
            if (fogPolicy != BG_FOG_AUTO && alphaSource == BG_ALPHA_AUTO)
            {
                for (lutPair = ptrDynamic_CC_RM_LUT[CCRMLUT_PRIMARY]; lutPair->words.w0; lutPair += 2)
                {
                    if (lutPair->words.w0 == curGfx->words.w0 && lutPair->words.w1 == curGfx->words.w1)
                    { *curGfx = *(lutPair + 1); break; }
                }
            }
            if (promote) { bgPromoteFogCombine(curGfx); }
            if (alphaSource != BG_ALPHA_AUTO)
            {
                /* Promoted one-cycle draws still sample only their original
                 * base tile; promotion must not introduce mip interpolation. */
                u32 alphahigh = promote ? (high & ~(G_TL_LOD | (3u << G_MDSFT_TEXTDETAIL))) | G_CYC_2CYCLE : high;
                bgApplyAlphaPreset(curGfx, alphaSource, alphahigh);
            }
        }
        curGfx++;
    }
}
