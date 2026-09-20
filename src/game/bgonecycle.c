#include <ultra64.h>
#include "bgonecycle.h"
#include "renderconfig.h"
#include "image.h"
#include "bgtransparency.h"

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
    /* Ordinary props add a constant alpha for their secondary surfaces. */
    {gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,
            COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, PRIMITIVE),
     gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, TEXEL0, 0, SHADE, PRIMITIVE,
            TEXEL0, 0, SHADE, 0, TEXEL0, 0, SHADE, PRIMITIVE)},
    /* Some room textures take alpha from one mip instead of interpolating it. */
    {gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, TEXEL1, 0,
            COMBINED, 0, SHADE, 0, COMBINED, 0, ENVIRONMENT, 0),
     gsDPSetCombineMode(G_CC_MODULATEIFADEA, G_CC_MODULATEIFADEA)},
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
    G_RM_ZB_OPA_SURF2, G_RM_OPA_SURF2,
    /* AA-Off retains TERR's 1MA blender field. It is inactive for these
     * opaque pixels, but later partial translucent writes can inherit it. */
    (G_RM_ZB_OPA_SURF2) & ~(3u << 16),
    (G_RM_ZB_OPA_SURF2) & ~(Z_CMP | Z_UPD),
    (G_RM_ZB_OPA_SURF2) & ~(Z_CMP | Z_UPD | (3u << 16))
};

/* Damaged props use the RDP's nine-bit SHADE_ALPHA + ENV_ALPHA result,
 * including its wrap/clamp behavior, to open holes in deformed geometry.
 * Keep that equation on the RDP; only the RGB mip interpolation is removed. */
static const Gfx g_ModelDamageCombiners[2] = {
    gsDPSetCombineLERP(TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, SHADE, ENVIRONMENT,
            COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 1, 0, SHADE, ENVIRONMENT,
            TEXEL0, 0, SHADE, 0, 1, 0, SHADE, ENVIRONMENT)
};

/* Only used after the model renderer has proved that the part still uses
 * its authored, full-alpha vertices. Blood needs the original first cycle.
 * The opaque blender ignores pixel alpha; fog uses the per-instance fog
 * colour/alpha, not vertex alpha. Keep texture LOD selection for characters:
 * their native equation already samples one mip rather than interpolating. */
static const Gfx g_CharacterOneCycleCombiners[][2] = {
    {gsDPSetCombineLERP(TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT,
            TEXEL0, ENVIRONMENT, SHADE, ENVIRONMENT,
            COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED),
     gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, 1,
            TEXEL0, 0, SHADE, 0, 0, 0, 0, 1)},
    {gsDPSetCombineLERP(1, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT,
            0, 0, 0, 1, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED),
     gsDPSetCombineLERP(0, 0, 0, SHADE, 0, 0, 0, 1,
            0, 0, 0, SHADE, 0, 0, 0, 1)}
};

typedef struct BgOneCycleState {
    u32 high;
    u32 highKnown;
    u32 low;
    u32 lowKnown;
    Gfx combine;
    s32 combineKnown;
    s32 textureEnabled; /* -1 until an explicit gSPTexture */
    s32 baseTile;
    s32 envAlpha;
    s32 blendAlpha;
    u32 tiles[8];
    u32 palettes[8];
    u32 tileKnown;
    u32 image;
    u32 imageDepth;
    u32 loadedImage;
    u32 loadedTexels;
    s32 paletteLoaded;
    u32 surfacePolicy;
    u32 alphaSource;
    struct tex *loadedTexture;
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
    s32 i;
    state->high = state->highKnown = state->low = state->lowKnown = 0;
    state->combine.words.w0 = state->combine.words.w1 = 0;
    state->combineKnown = FALSE;
    state->textureEnabled = -1;
    state->baseTile = state->envAlpha = state->blendAlpha = -1;
    state->tileKnown = state->image = state->imageDepth = 0;
    state->loadedImage = state->loadedTexels = 0;
    state->paletteLoaded = FALSE;
    state->surfacePolicy = BG_SURFACE_AUTO;
    state->alphaSource = BG_ALPHA_AUTO;
    for (i = 0; i < 8; i++) state->tiles[i] = state->palettes[i] = 0;
    state->loadedTexture = NULL;
}

/* SETOTHERMODE uses a shift and bit count, including editor-authored partial
 * surface overrides. Never treat a partial write as a complete render mode. */
static s32 bgOneCycleReadState(BgOneCycleState *state, Gfx command, bool cutouts)
{
    u32 opcode = command.words.w0 >> 24;
    u32 shift;
    u32 length;
    u32 mask;
    u32 tile;
    struct tex *tex;
    if (BG_SURFACE_IS_MARKER(command.words.w0, command.words.w1)) {
        state->surfacePolicy = BG_SURFACE_TAG_POLICY(command.words.w1);
    } else if (BG_ALPHA_IS_MARKER(command.words.w0, command.words.w1)) {
        if (BG_ALPHA_TAG_KIND(command.words.w1) <= BG_ALPHA_VERTEX)
            state->alphaSource = BG_ALPHA_TAG_KIND(command.words.w1);
    } else if (opcode == (u8)G_SETOTHERMODE_H || opcode == (u8)G_SETOTHERMODE_L) {
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
        state->baseTile = (command.words.w0 >> 8) & 7;
    } else if (opcode == (u8)G_SETENVCOLOR) {
        state->envAlpha = command.words.w1 & 0xff;
    } else if (opcode == (u8)G_SETBLENDCOLOR) {
        state->blendAlpha = command.words.w1 & 0xff;
    } else if (cutouts && opcode == (u8)G_SETTILE) {
        tile = (command.words.w1 >> 24) & 7;
        state->tiles[tile] = command.words.w0;
        state->palettes[tile] = (command.words.w1 >> 20) & 15;
        state->tileKnown |= 1u << tile;
    } else if (cutouts && opcode == (u8)G_SETTIMG) {
        state->image = command.words.w1;
        state->imageDepth = (command.words.w0 >> 19) & 3;
    } else if (cutouts && (opcode == (u8)G_LOADBLOCK || opcode == (u8)G_LOADTILE)) {
        /* Only a full base-image upload to TMEM zero is understood. A later
         * detail/partial upload invalidates it; TLUT uploads do not replace it. */
        state->loadedTexture = NULL;
        state->paletteLoaded = FALSE;
        tile = (command.words.w1 >> 24) & 7;
        if (opcode == (u8)G_LOADBLOCK && !(command.words.w0 & 0xffffff)
                && (state->tileKnown & (1u << tile)) && !(state->tiles[tile] & 0x1ff)) {
            tex = texFindByData(state->image);
            if (tex
                    && (((((command.words.w1 >> 12) & 0xfff) + 1) << state->imageDepth) / 2
                        >= (tex->depth == G_IM_SIZ_32b ? ((tex->width + 3) & ~3) * 4 * tex->height
                            : ((tex->width * (4 << tex->depth) + 63) / 64) * 8 * tex->height)))
            {
                state->loadedTexture = tex;
                state->loadedImage = state->image;
                state->loadedTexels = ((command.words.w1 >> 12) & 0xfff) + 1;
            }
        }
    } else if (cutouts && opcode == (u8)G_LOADTLUT) {
        tile = (command.words.w1 >> 24) & 7;
        /* Require the ordinary palette load from the same texture record.
         * Partial/rebased TLUTs could give the same indices different alpha. */
        state->paletteLoaded = state->image == state->loadedImage
                && (state->tileKnown & (1u << tile)) && (state->tiles[tile] & 0x1ff) == 0x100
                && state->loadedTexture
                && (((command.words.w0 >> 14) & 0x3ff) + ((command.words.w0 >> 2) & 0x3ff)
                    == state->loadedTexels)
                && ((command.words.w0 ^ command.words.w1) & 0xfff) == 0
                && (((command.words.w1 >> 14) & 0x3ff) - ((command.words.w0 >> 14) & 0x3ff)
                    == state->loadedTexture->unk0a);
    } else if (opcode == (u8)G_RDPSETOTHERMODE) {
        state->high = command.words.w0 & 0xffffff;
        state->highKnown = 0xffffff;
        state->low = command.words.w1;
        state->lowKnown = 0xffffffff;
    }
    return TRUE;
}

static s32 bgOneCycleChooseState(const BgOneCycleState *source, BgOneCycleState *chosen,
        bool model, bool cutouts, bool character)
{
    u32 first;
    u32 mode;
    u32 required;
    s32 i;
    s32 combiner;
    s32 characterCombiner = -1;
    bool cutout = FALSE;
    bool damaged;
    struct tex *tex;
    Gfx alphaCombine = gsDPSetCombineMode(G_CC_MODULATEIFADEA, G_CC_MODULATEIFADEA);
    *chosen = *source;
    /* A hand-authored blend must survive even when its pixels are binary.
     * Invalid/unknown policies also retain the original pipeline. */
    if (source->surfacePolicy == BG_SURFACE_BLEND || source->surfacePolicy > BG_SURFACE_BLEND) return FALSE;
    if (source->alphaSource != BG_ALPHA_AUTO) return FALSE;
    if ((source->highKnown & BG_CYCLE_MASK) != BG_CYCLE_MASK
            || (source->high & BG_CYCLE_MASK) != G_CYC_2CYCLE
            || (source->lowKnown & BG_RENDER_MASK) != BG_RENDER_MASK
            || (source->low & 3) || !source->combineKnown) return FALSE;

    first = source->low & BG_FIRST_BLENDER_MASK;
    if (first != (G_RM_PASS) && first != (G_RM_FOG_SHADE_A)
            && (!model || first != (G_RM_FOG_PRIM_A))) return FALSE;
    mode = source->low & (BG_RENDER_MASK & ~BG_FIRST_BLENDER_MASK);
    /* Only the primary world-prop damage material is supported. At the
     * native damage alphas (150/200/250/255), every nonzero combined alpha
     * is >= 128. Thresholding therefore retains its holes while replacing
     * partial AA coverage with opaque pixels. modelApplyRenderModeType3/4
     * establish the compare baseline and threshold before calling the list. */
    damaged = model && first == (G_RM_FOG_PRIM_A)
            && (mode == (G_RM_AA_ZB_TEX_EDGE2) || mode == (G_RM_AA_TEX_EDGE2))
            && source->envAlpha >= BG_CUTOUT_THRESHOLD
            && source->blendAlpha == BG_CUTOUT_THRESHOLD && (source->lowKnown & 3) == 3
            && source->combine.words.w0 == g_ModelDamageCombiners[0].words.w0
            && source->combine.words.w1 == g_ModelDamageCombiners[0].words.w1;
    for (i = 0; i < sizeof(g_BgOneCycleSurfaces) / sizeof(g_BgOneCycleSurfaces[0]); i++) {
        if (mode == g_BgOneCycleSurfaces[i]) break;
    }
    if (!damaged && i == sizeof(g_BgOneCycleSurfaces) / sizeof(g_BgOneCycleSurfaces[0])) {
        if (!cutouts || source->surfacePolicy == BG_SURFACE_OPAQUE
                || (mode != (G_RM_AA_ZB_XLU_SURF2) && mode != (G_RM_ZB_XLU_SURF2)
                    && mode != (G_RM_AA_ZB_TEX_EDGE2))) return FALSE;
        /* Auto requires binary alpha and no fade. An explicit Cutout already
         * requests thresholding, including for soft-alpha textures. The room
         * LUT still needs a supported texture-times-environment combiner. */
        tex = source->loadedTexture;
        if (!tex || source->envAlpha < 0
                || (source->surfacePolicy != BG_SURFACE_CUTOUT && (!tex->hasBinaryAlpha || source->envAlpha != 255))
                || (source->surfacePolicy == BG_SURFACE_CUTOUT && mode != (G_RM_AA_ZB_TEX_EDGE2))
                || source->blendAlpha != BG_CUTOUT_THRESHOLD
                || (source->lowKnown & 3) != 3 || source->baseTile != 0
                || !(source->tileKnown & 1) || (source->tiles[0] & 0x1ff)
                || ((source->tiles[0] >> 21) & 7) != tex->gbiformat
                || ((source->tiles[0] >> 19) & 3) != tex->depth
                || (tex->gbiformat == G_IM_FMT_CI && (source->palettes[0] || !source->paletteLoaded))
                || (source->highKnown & (3u << G_MDSFT_TEXTLUT)) != (3u << G_MDSFT_TEXTLUT)
                || ((source->high >> G_MDSFT_TEXTLUT) & 3) != tex->lutmodeindex) return FALSE;
        cutout = TRUE;
    }

    for (combiner = 0; combiner < sizeof(g_BgOneCycleCombiners) / sizeof(g_BgOneCycleCombiners[0]); combiner++) {
        if (source->combine.words.w0 == g_BgOneCycleCombiners[combiner][0].words.w0
                && source->combine.words.w1 == g_BgOneCycleCombiners[combiner][0].words.w1) break;
    }
    if (character) {
        for (i = 0; i < sizeof(g_CharacterOneCycleCombiners) / sizeof(g_CharacterOneCycleCombiners[0]); i++) {
            if (source->combine.words.w0 == g_CharacterOneCycleCombiners[i][0].words.w0
                    && source->combine.words.w1 == g_CharacterOneCycleCombiners[i][0].words.w1) {
                characterCombiner = i;
                break;
            }
        }
        if (characterCombiner < 0 || first != (G_RM_FOG_PRIM_A)) return FALSE;
    } else if (!damaged && combiner == sizeof(g_BgOneCycleCombiners) / sizeof(g_BgOneCycleCombiners[0])) return FALSE;
    if (cutout && (g_BgOneCycleCombiners[combiner][1].words.w0 != alphaCombine.words.w0
                || g_BgOneCycleCombiners[combiner][1].words.w1 != alphaCombine.words.w1)) return FALSE;
    /* The final two entries are the untextured combiners. */
    if (character ? characterCombiner == 0
            : damaged || combiner < sizeof(g_BgOneCycleCombiners) / sizeof(g_BgOneCycleCombiners[0]) - 2) {
        required = BG_LOD_MASK | BG_DETAIL_MASK | BG_FILTER_MASK;
        if (source->textureEnabled != TRUE || (source->highKnown & required) != required
                || (source->high & BG_DETAIL_MASK) != G_TD_CLAMP
                || ((source->high & BG_FILTER_MASK) != G_TF_BILERP
                    && (source->high & BG_FILTER_MASK) != G_TF_POINT)) return FALSE;
        if (!character) chosen->high &= ~BG_LOD_MASK; /* Sample the primitive's base tile. */
    }
    chosen->high &= ~BG_CYCLE_MASK;
    chosen->combine = character ? g_CharacterOneCycleCombiners[characterCombiner][1]
            : damaged ? g_ModelDamageCombiners[1] : g_BgOneCycleCombiners[combiner][1];
    if (first == (G_RM_FOG_SHADE_A) || first == (G_RM_FOG_PRIM_A)) {
        /* One-cycle fog uses the FIRST blender mux. FORCE_BL is essential:
         * AA is off, but the fog operation must still run. No framebuffer
         * colour read is needed; Z compare/update retain their source bits. */
        chosen->low = (source->low & (Z_CMP | Z_UPD | 7))
                | CVG_DST_FULL | ALPHA_CVG_SEL | FORCE_BL
                | first | (first >> 2);
    } else {
        chosen->low = (source->low & 7) | ((source->low & Z_CMP)
                ? G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2
                : G_RM_OPA_SURF | G_RM_OPA_SURF2);
    }
    if (cutout || damaged) {
        /* Threshold uses combiner alpha, not coverage. Surviving texels are
         * opaque, retaining damaged models' original Z flags; room cutouts
         * enable Z writes. Holes write neither colour nor depth. The chosen
         * fog blender never reads the framebuffer colour. */
        chosen->low = (chosen->low & ~(ALPHA_CVG_SEL | CVG_X_ALPHA | 3))
                | (cutout ? Z_UPD : 0) | G_AC_THRESHOLD;
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
    s32 alphaDiff = ((actual->low ^ wanted->low) & 3) != 0;
    s32 combineDiff = actual->combine.words.w0 != wanted->combine.words.w0
            || actual->combine.words.w1 != wanted->combine.words.w1;
    if (!highDiff && !modeDiff && !combineDiff && !alphaDiff) return;
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
    if (alphaDiff) {
        gDPSetAlphaCompare(&command, wanted->low & 3);
        bgOneCycleEmit(out, command);
    }
    if (combineDiff) bgOneCycleEmit(out, wanted->combine);
    *actual = *wanted;
}

/* Model lists inherit their initial material from modelApplyRenderModeType*.
 * Interpret that setup without copying its per-instance colours into the list. */
static s32 bgOneCycleBuild(const Gfx *src, s32 size, Gfx *dst, s32 capacity,
        const Gfx *initial, s32 initialSize, bool cutouts, bool character)
{
    BgOneCycleState source;
    BgOneCycleState actual;
    BgOneCycleState wanted;
    BgOneCycleOutput out;
    Gfx command;
    u32 opcode;
    u32 policy;
    u32 alphaSource;
    s32 i;
    if (!src || size <= 0 || (size & 7) || size > BG_MAX_ONE_CYCLE_BYTES
            || capacity < 0 || dst == src || initialSize < 0 || (initialSize & 7)
            || (initialSize && !initial)) return -1;
    bgOneCycleResetState(&source);
    if (cutouts) {
        /* bgRenderRoomSecondary establishes these before calling an alternate. */
        source.lowKnown |= 3;
        source.blendAlpha = BG_CUTOUT_THRESHOLD;
    }
    for (i = 0; i < initialSize / sizeof(Gfx); i++) {
        command = initial[i];
        if (command.words.w0 >> 24 == (u8)G_SETOTHERMODE_L) command = renderGetAaOffCommand(command);
        if (!bgOneCycleReadState(&source, command, cutouts)) return -1;
    }
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
            if (bgOneCycleChooseState(&source, &wanted, initial != NULL, cutouts, character)) out.converted++;
            bgOneCycleFlush(&out, &actual, &wanted);
        } else if (opcode == (u8)G_ENDDL || opcode == (u8)G_DL
                || opcode == (u8)G_CULLDL || opcode == (u8)G_LINE3D
                || opcode == (u8)G_TEXRECT || opcode == (u8)G_TEXRECTFLIP
                || opcode == (u8)G_FILLRECT) {
            bgOneCycleFlush(&out, &actual, &source);
        }
        bgOneCycleEmit(&out, command);
        if (!bgOneCycleReadState(&source, command, cutouts) || !bgOneCycleReadState(&actual, command, cutouts)
                || out.failed) return -1;
        if (opcode == (u8)G_ENDDL) return out.converted ? out.count * sizeof(Gfx) : 0;
        if (opcode == (u8)G_DL) {
            /* Nested lists (notably animated water) retain their own pipeline.
             * Nothing after a call is assumed until explicitly established. */
            if ((command.words.w0 >> 16) & 0xff) return out.converted ? out.count * sizeof(Gfx) : 0;
            /* The editor policy belongs to this list's faces. Nested lists
             * have their own metadata; only their hardware state is unknown. */
            policy = source.surfacePolicy;
            alphaSource = source.alphaSource;
            bgOneCycleResetState(&source);
            source.surfacePolicy = policy;
            source.alphaSource = alphaSource;
            actual = source;
        }
    }
    return -1; /* No ENDDL/terminal branch within the supplied room stream. */
}

s32 gfxBuildOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity,
        const Gfx *initial, s32 initialSize)
{
    return bgOneCycleBuild(src, size, dst, capacity, initial, initialSize, FALSE, FALSE);
}

s32 gfxBuildCharacterOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity,
        const Gfx *initial, s32 initialSize)
{
    return bgOneCycleBuild(src, size, dst, capacity, initial, initialSize, FALSE, TRUE);
}

s32 bgBuildCutoutGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity)
{
    return bgOneCycleBuild(src, size, dst, capacity, NULL, 0, TRUE, FALSE);
}

s32 bgBuildOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity)
{
    return gfxBuildOneCycleGdl(src, size, dst, capacity, NULL, 0);
}
