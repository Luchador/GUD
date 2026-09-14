#include <ultra64.h>
#include "bgonecycle.h"
#include "renderconfig.h"

#define BG_CYCLE_MASK (3u << G_MDSFT_CYCLETYPE)
#define BG_LOD_MASK (1u << G_MDSFT_TEXTLOD)
#define BG_DETAIL_MASK (3u << G_MDSFT_TEXTDETAIL)
#define BG_FILTER_MASK (3u << G_MDSFT_TEXTFILT)
#define BG_RENDER_MASK 0xfffffff8u
#define BG_FIRST_BLENDER_MASK 0xcccc0000u
#define BG_MAX_ONE_CYCLE_BYTES 0x100000

/* The first four entries cover standard textured rooms, before and after
 * BG's fog/alpha LUTs. The others cover simple single-texture and untextured
 * materials that inherit a two-cycle pipeline from an earlier draw. */
static const Gfx g_BgOneCycleCombiners[][2] = {
    {gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
     gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA)},
    {gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
     gsDPSetCombineMode(G_CC_MODULATEI, G_CC_MODULATEI)},
    {gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,
            COMBINED, 0, SHADE, 0, COMBINED, 0, ENVIRONMENT, 0),
     gsDPSetCombineMode(G_CC_MODULATEIFADEA, G_CC_MODULATEIFADEA)},
    {gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,
            COMBINED, 0, SHADE, 0, 0, 0, 0, ENVIRONMENT),
     gsDPSetCombineMode(G_CC_MODULATEIFADE, G_CC_MODULATEIFADE)},
    {gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA),
     gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA)},
    {gsDPSetCombineMode(G_CC_MODULATEI, G_CC_MODULATEI),
     gsDPSetCombineMode(G_CC_MODULATEI, G_CC_MODULATEI)},
    {gsDPSetCombineMode(G_CC_MODULATEIFADEA, G_CC_MODULATEIFADEA),
     gsDPSetCombineMode(G_CC_MODULATEIFADEA, G_CC_MODULATEIFADEA)},
    {gsDPSetCombineMode(G_CC_MODULATEIFADE, G_CC_MODULATEIFADE),
     gsDPSetCombineMode(G_CC_MODULATEIFADE, G_CC_MODULATEIFADE)},
    {gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
     gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE)},
    {gsDPSetCombineLERP(0, 0, 0, SHADE, 0, 0, 0, ENVIRONMENT,
            0, 0, 0, SHADE, 0, 0, 0, ENVIRONMENT),
     gsDPSetCombineLERP(0, 0, 0, SHADE, 0, 0, 0, ENVIRONMENT,
            0, 0, 0, SHADE, 0, 0, 0, ENVIRONMENT)}
};

static const u32 g_BgOneCycleSurfaces[] = {
    G_RM_AA_ZB_OPA_SURF2, G_RM_AA_ZB_OPA_TERR2,
    G_RM_AA_OPA_SURF2, G_RM_AA_OPA_TERR2,
    G_RM_ZB_OPA_SURF2, G_RM_OPA_SURF2
};

typedef struct BgOneCycleState {
    u32 high;
    u32 highKnown;
    u32 low;
    u32 lowKnown;
    Gfx combine;
    s32 combineKnown;
    s32 textureEnabled; /* -1 until an explicit gSPTexture */
} BgOneCycleState;

typedef struct BgOneCycleOutput {
    Gfx *dst;
    s32 capacity;
    s32 count;
    s32 converted;
    s32 failed;
} BgOneCycleOutput;

static void bgOneCycleEmit(BgOneCycleOutput *out, Gfx command)
{
    if (out->count >= BG_MAX_ONE_CYCLE_BYTES / sizeof(Gfx)
            || (out->dst && out->count >= out->capacity)) {
        out->failed = TRUE;
        return;
    }
    if (out->dst) out->dst[out->count] = command;
    out->count++;
}

static void bgOneCycleResetState(BgOneCycleState *state)
{
    state->high = state->highKnown = state->low = state->lowKnown = 0;
    state->combine.words.w0 = state->combine.words.w1 = 0;
    state->combineKnown = FALSE;
    state->textureEnabled = -1;
}

/* SETOTHERMODE uses a shift and bit count, including editor-authored partial
 * surface overrides. Never treat a partial write as a complete render mode. */
static s32 bgOneCycleReadState(BgOneCycleState *state, Gfx command)
{
    u32 opcode = command.words.w0 >> 24;
    u32 shift;
    u32 length;
    u32 mask;
    if (opcode == (u8)G_SETOTHERMODE_H || opcode == (u8)G_SETOTHERMODE_L) {
        shift = (command.words.w0 >> 8) & 0xff;
        length = command.words.w0 & 0xff;
        if (!length || shift >= 32 || length > 32 - shift) return FALSE;
        mask = (0xffffffffu >> (32 - length)) << shift;
        if (opcode == (u8)G_SETOTHERMODE_H) {
            state->high = (state->high & ~mask) | (command.words.w1 & mask);
            state->highKnown |= mask;
        } else {
            state->low = (state->low & ~mask) | (command.words.w1 & mask);
            state->lowKnown |= mask;
        }
    } else if (opcode == (u8)G_SETCOMBINE) {
        state->combine = command;
        state->combineKnown = TRUE;
    } else if (opcode == (u8)G_TEXTURE) {
        state->textureEnabled = (command.words.w0 & 0xff) != 0;
    } else if (opcode == (u8)G_RDPSETOTHERMODE) {
        state->high = command.words.w0 & 0xffffff;
        state->highKnown = 0xffffff;
        state->low = command.words.w1;
        state->lowKnown = 0xffffffff;
    }
    return TRUE;
}

static s32 bgOneCycleChooseState(const BgOneCycleState *source, BgOneCycleState *chosen)
{
    u32 first;
    u32 mode;
    u32 required;
    s32 i;
    s32 combiner;
    *chosen = *source;
    if ((source->highKnown & BG_CYCLE_MASK) != BG_CYCLE_MASK
            || (source->high & BG_CYCLE_MASK) != G_CYC_2CYCLE
            || (source->lowKnown & BG_RENDER_MASK) != BG_RENDER_MASK
            || (source->low & 3) || !source->combineKnown) return FALSE;

    first = source->low & BG_FIRST_BLENDER_MASK;
    if (first != (G_RM_PASS) && first != (G_RM_FOG_SHADE_A)) return FALSE;
    mode = source->low & (BG_RENDER_MASK & ~BG_FIRST_BLENDER_MASK);
    for (i = 0; i < sizeof(g_BgOneCycleSurfaces) / sizeof(g_BgOneCycleSurfaces[0]); i++) {
        if (mode == g_BgOneCycleSurfaces[i]) break;
    }
    if (i == sizeof(g_BgOneCycleSurfaces) / sizeof(g_BgOneCycleSurfaces[0])) return FALSE;

    for (combiner = 0; combiner < sizeof(g_BgOneCycleCombiners) / sizeof(g_BgOneCycleCombiners[0]); combiner++) {
        if (source->combine.words.w0 == g_BgOneCycleCombiners[combiner][0].words.w0
                && source->combine.words.w1 == g_BgOneCycleCombiners[combiner][0].words.w1) break;
    }
    if (combiner == sizeof(g_BgOneCycleCombiners) / sizeof(g_BgOneCycleCombiners[0])) return FALSE;
    if (combiner < 8) {
        required = BG_LOD_MASK | BG_DETAIL_MASK | BG_FILTER_MASK;
        if (source->textureEnabled != TRUE || (source->highKnown & required) != required
                || (source->high & BG_DETAIL_MASK) != G_TD_CLAMP
                || ((source->high & BG_FILTER_MASK) != G_TF_BILERP
                    && (source->high & BG_FILTER_MASK) != G_TF_POINT)) return FALSE;
        chosen->high &= ~BG_LOD_MASK; /* Sample the primitive's base tile. */
    }
    chosen->high &= ~BG_CYCLE_MASK;
    chosen->combine = g_BgOneCycleCombiners[combiner][1];
    if (first == (G_RM_FOG_SHADE_A)) {
        /* One-cycle fog uses the FIRST blender mux. FORCE_BL is essential:
         * AA is off, but the fog operation must still run. No framebuffer
         * colour read is needed; Z compare/update retain their source bits. */
        chosen->low = (source->low & (Z_CMP | Z_UPD | 7))
                | CVG_DST_FULL | ALPHA_CVG_SEL | FORCE_BL
                | G_RM_FOG_SHADE_A | ((G_RM_FOG_SHADE_A) >> 2);
    } else {
        chosen->low = (source->low & 7) | ((source->low & Z_CMP)
                ? G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2
                : G_RM_OPA_SURF | G_RM_OPA_SURF2);
    }
    return TRUE;
}

/* Only these fields are overridden. All other RDP/RSP state and every texture
 * upload, vertex load and triangle packet remain byte-for-byte source data. */
static void bgOneCycleFlush(BgOneCycleOutput *out, BgOneCycleState *actual,
        const BgOneCycleState *wanted)
{
    Gfx command;
    u32 highDiff = (actual->high ^ wanted->high) & (BG_CYCLE_MASK | BG_LOD_MASK);
    s32 modeDiff = ((actual->low ^ wanted->low) & BG_RENDER_MASK) != 0;
    s32 combineDiff = actual->combine.words.w0 != wanted->combine.words.w0
            || actual->combine.words.w1 != wanted->combine.words.w1;
    if (!highDiff && !modeDiff && !combineDiff) return;
    gDPPipeSync(&command);
    bgOneCycleEmit(out, command);
    if (highDiff & BG_CYCLE_MASK) {
        gDPSetCycleType(&command, wanted->high & BG_CYCLE_MASK);
        bgOneCycleEmit(out, command);
    }
    if (highDiff & BG_LOD_MASK) {
        gDPSetTextureLOD(&command, wanted->high & BG_LOD_MASK);
        bgOneCycleEmit(out, command);
    }
    if (modeDiff) {
        gDPSetRenderMode(&command, wanted->low & BG_RENDER_MASK, 0);
        bgOneCycleEmit(out, command);
    }
    if (combineDiff) bgOneCycleEmit(out, wanted->combine);
    *actual = *wanted;
}

s32 bgBuildOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity)
{
    BgOneCycleState source;
    BgOneCycleState actual;
    BgOneCycleState wanted;
    BgOneCycleOutput out;
    Gfx command;
    u32 opcode;
    s32 i;
    if (!src || size <= 0 || (size & 7) || size > BG_MAX_ONE_CYCLE_BYTES
            || capacity < 0 || dst == src) return -1;
    bgOneCycleResetState(&source);
    actual = source;
    out.dst = dst;
    out.capacity = capacity / sizeof(Gfx);
    out.count = out.converted = out.failed = 0;
    for (i = 0; i < size / sizeof(Gfx); i++) {
        command = src[i];
        opcode = command.words.w0 >> 24;
        /* Track the effective AA-Off baseline, including partial overrides
         * following a rewritten full mode. Restoring an assembled authored
         * mode here could resurrect AA that the submission walker removed. */
        if (opcode == (u8)G_SETOTHERMODE_L) command = renderGetAaOffCommand(command);
        /* Different microcode/control-flow formats and raw RDP triangles need
         * their own decoder. Refuse the list instead of guessing their state. */
        if (opcode == 0xaf || opcode == 0xb0 || (opcode >= 0xc8 && opcode <= 0xcf)) return -1;
        if (opcode == (u8)G_TRI1 || opcode == 0xb1) {
            if (bgOneCycleChooseState(&source, &wanted)) out.converted++;
            bgOneCycleFlush(&out, &actual, &wanted);
        } else if (opcode == (u8)G_ENDDL || opcode == (u8)G_DL
                || opcode == (u8)G_CULLDL || opcode == (u8)G_LINE3D
                || opcode == (u8)G_TEXRECT || opcode == (u8)G_TEXRECTFLIP
                || opcode == (u8)G_FILLRECT) {
            bgOneCycleFlush(&out, &actual, &source);
        }
        bgOneCycleEmit(&out, command);
        if (!bgOneCycleReadState(&source, command) || !bgOneCycleReadState(&actual, command)
                || out.failed) return -1;
        if (opcode == (u8)G_ENDDL) return out.converted ? out.count * sizeof(Gfx) : 0;
        if (opcode == (u8)G_DL) {
            /* Nested lists (notably animated water) retain their own pipeline.
             * Nothing after a call is assumed until explicitly established. */
            if ((command.words.w0 >> 16) & 0xff) return out.converted ? out.count * sizeof(Gfx) : 0;
            bgOneCycleResetState(&source);
            actual = source;
        }
    }
    return -1; /* No ENDDL/terminal branch within the supplied room stream. */
}
