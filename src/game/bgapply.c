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
    u32 low = 0;
    u32 high = 0;
    u32 fog = envGetCurrent()->FogEnabled ? G_FOG : 0;
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
        if (BG_ALPHA_IS_MARKER(curGfx->words.w0, curGfx->words.w1))
        {
            kind = BG_ALPHA_TAG_KIND(curGfx->words.w1);
            if (kind <= BG_ALPHA_VERTEX) { alphaSource = kind; }
            else if (kind == BG_ALPHA_SYNC) { gDPPipeSync(curGfx); }
            else if (kind == BG_ALPHA_FOG)
            {
                if (alphaSource == BG_ALPHA_AUTO && fog) { gSPSetGeometryMode(curGfx, G_FOG); }
                else { gSPClearGeometryMode(curGfx, G_FOG); }
            }
            else if (kind >= BG_ALPHA_BLENDER && kind <= BG_ALPHA_LAST_SLOT)
            {
                /* The first blender's four fields interleave the second's.
                 * Write them separately so later AA toggles can still change
                 * the final surface blender. One-cycle draws need only the
                 * geometry fog change; leave their final blender untouched. */
                if ((high & (3u << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE)
                {
                    u32 blender = alphaSource == BG_ALPHA_VERTEX ? G_RM_PASS : low;
                    shift = 18 + 4 * (kind - BG_ALPHA_BLENDER);
                    gSPSetOtherMode(curGfx, G_SETOTHERMODE_L, shift, 2, blender & (3u << shift));
                }
            }
            /* Slots are generated commands, not changes to the authored
             * state. Retain the policy marker for the one-cycle converter. */
            curGfx++;
            continue;
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
        else if (opcode == (u8)G_SETCOMBINE && alphaSource == BG_ALPHA_VERTEX)
        {
            curGfx->words.w0 = BG_ALPHA_COMBINE_W0(curGfx->words.w0);
            curGfx->words.w1 = BG_ALPHA_COMBINE_W1(curGfx->words.w1);
        }
        curGfx++;
    }
}
