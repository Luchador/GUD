#include "bgrender.h"
#include <math.h>

/* Fast3D / RDP encodings from include/PR/gbi.h, without its N64 ABI types. */
#define BG_G_SETENVCOLOR 0xFBu
#define BG_G_SETPRIMCOLOR 0xFAu
#define BG_G_SETOTHERMODE_L 0xB9u
#define BG_G_RDPSETOTHERMODE 0xEFu
#define BG_G_CLEARGEOMETRYMODE 0xB6u
#define BG_G_SETGEOMETRYMODE 0xB7u
#define BG_G_ZBUFFER 1u
#define BG_G_SHADE 0x00000004u
#define BG_G_SHADING_SMOOTH 0x00000200u
#define BG_G_LIGHTING 0x00020000u
#define BG_G_TEXTURE_GEN 0x00040000u
#define BG_G_TEXTURE_GEN_LINEAR 0x00080000u
#define BG_AA_EN 0x8u
#define BG_Z_CMP 0x10u
#define BG_Z_UPD 0x20u
#define BG_ZMODE_MASK 0xC00u
#define BG_ZMODE_DEC 0xC00u
#define BG_ZMODE_XLU 0x800u
#define BG_CVG_DST_MASK 0x300u
#define BG_CVG_DST_WRAP 0x100u
#define BG_FORCE_BL 0x4000u
#define BG_CVG_X_ALPHA 0x1000u
#define BG_ALPHA_COMPARE_MASK 3u

BgRenderFlags BgRenderResolveModelCulling(BgRenderFlags flags, BOOL flippeddoor)
{
    BOOL back = (flags & BG_RENDER_CULL_BACK) != 0;
    BOOL front = (flags & BG_RENDER_CULL_FRONT) != 0;
    /* Older glTF previews only recorded fully explicit culling. A known set
       bit still wins; otherwise apply the ordinary world-model default. New
       exports also retain partial clears via the per-bit inheritance flags. */
    if (!(flags & BG_RENDER_CULL_EXPLICIT))
    {
        if (!back) { flags |= BG_RENDER_CULL_BACK_INHERITED; }
        if (!front) { flags |= BG_RENDER_CULL_FRONT_INHERITED; }
    }
    if (flags & BG_RENDER_CULL_BACK_INHERITED) { back = !flippeddoor; }
    if (flags & BG_RENDER_CULL_FRONT_INHERITED) { front = flippeddoor; }
    /* ObjectPlaceModel mirrors Z and reverses triangle winding. Swap the
       resolved cull sides as well, including explicitly authored overrides. */
    if (flippeddoor) { BOOL swap = back; back = front; front = swap; }
    flags &= ~(BG_RENDER_CULL_BACK | BG_RENDER_CULL_FRONT);
    return flags | BG_RENDER_CULL_EXPLICIT
        | (back ? BG_RENDER_CULL_BACK : 0) | (front ? BG_RENDER_CULL_FRONT : 0);
}

BgRenderFlags BgRenderModelCulling(const BgRenderState *state)
{
    BgRenderFlags flags = BG_RENDER_CULL_EXPLICIT;
    if (state->geometrymode & 0x2000u) { flags |= BG_RENDER_CULL_BACK; }
    if (state->geometrymode & 0x1000u) { flags |= BG_RENDER_CULL_FRONT; }
    if (!(state->geometryknown & 0x2000u)) { flags |= BG_RENDER_CULL_BACK_INHERITED; }
    if (!(state->geometryknown & 0x1000u)) { flags |= BG_RENDER_CULL_FRONT_INHERITED; }
    return BgRenderResolveModelCulling(flags, FALSE);
}

void BgRenderStateInit(BgRenderState *state, BOOL secondary)
{
    state->environmentalpha = state->primitivealpha = 255;
    state->primitiveword0 = 0xFA000000u;
    state->primitiveword1 = 0xFFFFFFFFu;
    state->zbuffer = TRUE;
    /* Match dlZBufferGeometry in assets/font_dl.c. A room which inherits
     * shading must compare equal to one which explicitly enables it. Keep
     * geometryknown separate: these defaults are not authored room commands. */
    state->geometrymode = BG_G_ZBUFFER | BG_G_SHADE | BG_G_SHADING_SMOOTH;
    state->geometryknown = 0;
    state->othermodehigh = 0;
    state->othermodeknown = 0;
    state->othermodehighknown = 0;
    state->surfacepolicy = BG_SURFACE_AUTO;
    state->surfacebasemode = 0;
    state->othermode = BG_Z_CMP | (secondary ? BG_ZMODE_XLU | BG_FORCE_BL : BG_Z_UPD);
}

void BgRenderStateRead(BgRenderState *state, DWORD word0, DWORD word1)
{
    if (BG_SURFACE_IS_MARKER(word0, word1))
    {
        DWORD policy = BG_SURFACE_TAG_POLICY(word1);
        state->surfacepolicy = policy <= BG_SURFACE_BLEND ? policy : BG_SURFACE_UNKNOWN;
        state->surfacebasemode = policy == BG_SURFACE_AUTO ? 0 : word1 & BG_SURFACE_MODE_MASK;
        return;
    }
    switch (word0 >> 24)
    {
    case BG_G_SETENVCOLOR:
        state->environmentalpha = (unsigned char)word1;
        break;
    case BG_G_SETPRIMCOLOR:
        state->primitivealpha = (unsigned char)word1;
        state->primitiveword0 = word0;
        state->primitiveword1 = word1;
        break;
    case 0xBAu: /* SETOTHERMODE_H, including cycle type. */
    case BG_G_SETOTHERMODE_L:
    {
        /* Fast3D encodes the shift and bit count directly. Alpha/depth-source
         * commands must not erase the render-mode bits inherited by a group. */
        unsigned int shift = (word0 >> 8) & 0xFFu;
        unsigned int count = word0 & 0xFFu;
        DWORD mask;
        DWORD *mode = (word0 >> 24) == 0xBAu ? &state->othermodehigh : &state->othermode;
        if (!count || shift >= 32 || count > 32 - shift)
        {
            break;
        }
        mask = count == 32 ? 0xFFFFFFFFu : ((1u << count) - 1u) << shift;
        *mode = (*mode & ~mask) | (word1 & mask);
        if ((word0 >> 24) == BG_G_SETOTHERMODE_L) { state->othermodeknown |= mask; }
        else { state->othermodehighknown |= mask; }
        break;
    }
    case BG_G_RDPSETOTHERMODE:
        state->othermode = word1;
        state->othermodeknown = 0xFFFFFFFFu;
        state->othermodehigh = word0 & 0x00FFFFFFu;
        state->othermodehighknown = 0x00FFFFFFu;
        break;
    case BG_G_CLEARGEOMETRYMODE:
        state->geometryknown |= word1;
        state->geometrymode &= ~word1;
        if (word1 & BG_G_ZBUFFER)
        {
            state->zbuffer = FALSE;
        }
        break;
    case BG_G_SETGEOMETRYMODE:
        state->geometryknown |= word1;
        state->geometrymode |= word1;
        if (word1 & BG_G_ZBUFFER)
        {
            state->zbuffer = TRUE;
        }
        break;
    }
}

BgRenderFlags BgRenderStateFlags(const BgRenderState *state)
{
    BgRenderFlags flags = 0;
    BOOL decal = (state->othermode & BG_ZMODE_MASK) == BG_ZMODE_DEC;
    if (state->zbuffer && (state->othermode & BG_Z_CMP))
    {
        flags |= BG_RENDER_DEPTH_TEST;
    }
    if (state->zbuffer && (state->othermode & BG_Z_UPD) && !decal)
    {
        flags |= BG_RENDER_DEPTH_WRITE;
    }
    if (decal)
    {
        flags |= BG_RENDER_DECAL;
    }
    if (state->othermode & BG_FORCE_BL)
    {
        flags |= BG_RENDER_BLEND;
    }
    if (state->othermode & (BG_CVG_X_ALPHA | BG_ALPHA_COMPARE_MASK))
    {
        flags |= BG_RENDER_ALPHA_TEST;
    }
    if ((state->geometrymode & (BG_G_LIGHTING | BG_G_TEXTURE_GEN))
        == (BG_G_LIGHTING | BG_G_TEXTURE_GEN))
    {
        flags |= BG_RENDER_ENVIRONMENT;
        if (state->geometrymode & BG_G_TEXTURE_GEN_LINEAR)
        {
            flags |= BG_RENDER_ENVIRONMENT_LINEAR;
        }
    }
    return flags;
}

BgRenderFlags BgRenderDefaultFlags(BOOL secondary)
{
    BgRenderState state;
    BgRenderStateInit(&state, secondary);
    return BgRenderStateFlags(&state);
}

BgTransparency BgRenderGetTransparency(const BgRenderState *state)
{
    BgRenderFlags flags;
    /* gDPSetRenderMode writes bits 3..31. Alpha compare can be inherited
       separately; an explicit compare command is included in the flags. */
    if (!state || (state->othermodeknown & 0xFFFFFFF8u) != 0xFFFFFFF8u)
    { return BG_TRANSPARENCY_UNKNOWN; }
    flags = BgRenderStateFlags(state);
    if (flags & BG_RENDER_DECAL) { return BG_TRANSPARENCY_DECAL; }
    if ((flags & (BG_RENDER_ALPHA_TEST | BG_RENDER_BLEND)) == (BG_RENDER_ALPHA_TEST | BG_RENDER_BLEND))
    { return BG_TRANSPARENCY_CUTOUT_BLEND; }
    if (flags & BG_RENDER_ALPHA_TEST) { return BG_TRANSPARENCY_CUTOUT; }
    return flags & BG_RENDER_BLEND ? BG_TRANSPARENCY_BLEND : BG_TRANSPARENCY_OPAQUE;
}

BgTransparency BgRenderGetSurfaceTransparency(const BgRenderState *state)
{
    BgRenderState surface;
    if (!state) { return BG_TRANSPARENCY_UNKNOWN; }
    surface = *state;
    surface.othermode &= ~BG_ZMODE_MASK;
    return BgRenderGetTransparency(&surface);
}

static DWORD BgRenderSetDecalMode(DWORD mode, BOOL decal)
{
    BOOL blend = (mode & BG_FORCE_BL) != 0;
    DWORD coverage = !(mode & BG_AA_EN) ? BG_CVG_DST_MASK
        : decal || blend ? BG_CVG_DST_WRAP : 0;
    mode &= ~(BG_ZMODE_MASK | BG_Z_UPD | BG_CVG_DST_MASK);
    /* Decals compare against the supporting surface without replacing its
     * depth. Native decals use wrap coverage with AA, full coverage without it. Retain the
     * alpha test, blender, AA, fog and all unrelated other-mode bits. */
    return mode | coverage | (decal ? BG_ZMODE_DEC : blend ? BG_ZMODE_XLU : BG_Z_UPD);
}

BOOL BgRenderSurfacePreset(const BgRenderState *state, BgTransparency surface, DWORD *modeout)
{
    DWORD bits, blender, fixedmask, fixedvalue;
    BOOL onecycle;
    if (!state || !modeout || ((unsigned int)surface > BG_TRANSPARENCY_BLEND
            && surface != BG_TRANSPARENCY_AUTO) || state->surfacepolicy > BG_SURFACE_BLEND)
    { return FALSE; }
    if ((state->othermodehighknown & 0x00300000u) != 0x00300000u
        || (state->othermodehigh & 0x00200000u)
        || BgRenderGetSurfaceTransparency(state) > BG_TRANSPARENCY_BLEND
        || (state->othermode & 3u)) { return FALSE; }
    onecycle = !(state->othermodehigh & 0x00100000u);
    /* Ordinary final blender: input colour/alpha and framebuffer colour.
       Additive/custom blenders and alpha-compare pipelines stay read-only. */
    fixedmask = onecycle ? 0xCCC00000u : 0x33300000u;
    fixedvalue = onecycle ? 0x00400000u : 0x00100000u;
    if ((state->othermode & fixedmask) != fixedvalue) { return FALSE; }
    blender = onecycle ? 0x000C0000u : 0x00030000u;
    if ((state->othermode & blender) > (onecycle ? 0x00040000u : 0x00010000u)) { return FALSE; }
    if (surface == BG_TRANSPARENCY_AUTO)
    {
        /* Recover the pre-override surface while preserving current fog and
         * the unused cycle's blender. Unmarked old projects are already Auto. */
        *modeout = state->surfacepolicy == BG_SURFACE_AUTO ? state->othermode
            : (state->othermode & ~(0xFFF8u | blender))
                | (state->surfacebasemode & (0xFFF8u | blender));
        /* Auto restores transparency, not the separately edited Decal choice. */
        if ((state->othermode & BG_ZMODE_MASK) == BG_ZMODE_DEC
            && (*modeout & BG_ZMODE_MASK) != BG_ZMODE_DEC)
        { *modeout = BgRenderSetDecalMode(*modeout, TRUE); }
        else if ((state->othermode & BG_ZMODE_MASK) != BG_ZMODE_DEC
            && (*modeout & BG_ZMODE_MASK) == BG_ZMODE_DEC)
        { *modeout = BgRenderSetDecalMode(*modeout, FALSE); }
        return TRUE;
    }
    bits = surface == BG_TRANSPARENCY_OPAQUE ? 0x2078u
        : surface == BG_TRANSPARENCY_CUTOUT ? 0x3078u : 0x49D8u;
    if (!(state->othermode & BG_Z_CMP)) { bits &= ~0x830u; }
    if (surface != BG_TRANSPARENCY_BLEND) { bits |= onecycle ? 0x00040000u : 0x00010000u; }
    *modeout = (state->othermode & ~(0xFFF8u | blender)) | bits;
    if ((state->othermode & BG_ZMODE_MASK) == BG_ZMODE_DEC)
    { *modeout = BgRenderSetDecalMode(*modeout, TRUE); }
    return TRUE;
}

BOOL BgRenderDecalPreset(const BgRenderState *state, BOOL decal, DWORD *modeout)
{
    DWORD unused;
    if (!state || !modeout || (decal != FALSE && decal != TRUE)
        || !state->zbuffer || !(state->othermode & BG_Z_CMP)
        || !BgRenderSurfacePreset(state, BG_TRANSPARENCY_AUTO, &unused)) { return FALSE; }
    *modeout = ((state->othermode & BG_ZMODE_MASK) == BG_ZMODE_DEC) == decal
        ? state->othermode : BgRenderSetDecalMode(state->othermode, decal);
    return TRUE;
}

BOOL BgRenderSupportsVertexAlpha(const BgRenderState *state)
{
    DWORD first = state->othermode & 0xcccc0000u;
    return (state->othermodehighknown & 0x00300000u) == 0x00300000u
        && !(state->othermodehigh & 0x00200000u)
        && (state->othermodeknown & 0xffff0000u) == 0xffff0000u
        && (!(state->othermodehigh & 0x00100000u)
            || first == 0x0c080000u || first == 0xc8080000u);
}

/* Alpha combiner mux values from gbi.h. The multiplier slot uses 0 and 6
 * for LOD fractions instead of COMBINED and ONE. */
enum BgAlphaInput
{
    BG_ALPHA_COMBINED = 0,
    BG_ALPHA_TEXEL0 = 1,
    BG_ALPHA_TEXEL1 = 2,
    BG_ALPHA_PRIMITIVE = 3,
    BG_ALPHA_SHADE = 4,
    BG_ALPHA_ENVIRONMENT = 5,
    BG_ALPHA_ONE = 6,
    BG_ALPHA_ZERO = 7
};

/* bg.c's DL_LUT_PRIMARY and DL_LUT_SECONDARY replace SHADE alpha with
 * ENVIRONMENT for these exact authored combiners. Applying the substitution
 * only to matching words avoids changing custom vertex-alpha combiners. */
BgRenderAlpha BgRenderGetAlpha(const BgRenderState *state, const BgMaterial *material)
{
    static const struct
    {
        DWORD word0, word1;
        BOOL texture;
    } remapped[] = {
        {0xFC26A004u, 0x1F1093FFu, TRUE},  /* TRILERP, MODULATEIA2 */
        {0xFC121824u, 0xFF33FFFFu, TRUE},  /* MODULATEIA, MODULATEIA */
        {0xFC26A004u, 0x1FFC93FCu, FALSE}, /* TRILERP, MODULATEI2 */
        {0xFC127E24u, 0xFFFFF9FCu, FALSE}, /* MODULATEI, MODULATEI */
        {0xFCFFFFFFu, 0xFFFE7838u, FALSE}, /* SHADE, PASS2 */
        {0xFCFFFFFFu, 0xFFFE793Cu, FALSE}  /* SHADE, SHADE */
    };
    DWORD w0 = material->combineword0, w1 = material->combineword1;
    unsigned int i;

    if (material->alphasource == BG_ALPHA_VERTEX)
    {
        BgRenderAlpha alpha = {255, TRUE, FALSE};
        return alpha;
    }
    if (state->surfacepolicy == BG_SURFACE_CUTOUT)
    {
        w0 = BG_CUTOUT_COMBINE_W0(material->combineword0, material->combineword1);
        w1 = BG_CUTOUT_COMBINE_W1(material->combineword0, material->combineword1);
    }
    for (i = 0; i < sizeof(remapped) / sizeof(remapped[0]); i++)
    {
        if (w0 == remapped[i].word0 && w1 == remapped[i].word1)
        {
            BgRenderAlpha alpha = {state->environmentalpha, FALSE, remapped[i].texture};
            return alpha;
        }
    }
    return BgRenderGetMaterialAlpha(state, material);
}

/* Models use their authored alpha combiner, without bg.c's LUT rewrite. */
BgRenderAlpha BgRenderGetMaterialAlpha(const BgRenderState *state, const BgMaterial *material)
{
    BgRenderAlpha alpha = {255, TRUE, TRUE};
    DWORD w0 = material->combineword0, w1 = material->combineword1;
    unsigned int a = (w1 >> 21) & 7, b = (w1 >> 3) & 7;
    unsigned int c = (w1 >> 18) & 7, d = w1 & 7;
    int source = -1;

    /* Also accept explicit one-source and texture-product alpha equations.
     * TEXEL0/TEXEL1 use the same base image in this non-mipmapped preview.
     * Unrecognized custom equations retain the previous shade*texture preview. */
    if (a == b || c == BG_ALPHA_ZERO)
    {
        source = d;
        if (source != BG_ALPHA_COMBINED)
        {
            alpha.texture = FALSE;
        }
    }
    else if (b == BG_ALPHA_ZERO && d == BG_ALPHA_ZERO &&
             (a == BG_ALPHA_TEXEL0 || a == BG_ALPHA_TEXEL1 ||
              (a == BG_ALPHA_COMBINED && ((w0 >> 12) & 7) == BG_ALPHA_TEXEL1 &&
               ((w1 >> 12) & 7) == BG_ALPHA_TEXEL0 && ((w0 >> 9) & 7) == 0 &&
               ((w1 >> 9) & 7) == BG_ALPHA_TEXEL0)))
    {
        /* C=0/6 denotes LOD, and C=1/2 would square texture alpha. */
        source = c >= BG_ALPHA_PRIMITIVE && c <= BG_ALPHA_ENVIRONMENT ? (int)c : -1;
    }
    switch (source)
    {
    case BG_ALPHA_TEXEL0:
    case BG_ALPHA_TEXEL1:
        alpha.shade = FALSE;
        alpha.texture = TRUE;
        break;
    case BG_ALPHA_PRIMITIVE:
        alpha.shade = FALSE;
        alpha.constant = state->primitivealpha;
        break;
    case BG_ALPHA_SHADE:
        break;
    case BG_ALPHA_ENVIRONMENT:
        alpha.shade = FALSE;
        alpha.constant = state->environmentalpha;
        break;
    case BG_ALPHA_ONE:
        alpha.shade = FALSE;
        break;
    case BG_ALPHA_ZERO:
        alpha.shade = FALSE;
        alpha.constant = 0;
        break;
    }
    return alpha;
}

unsigned char BgRenderVertexAlpha(BgRenderAlpha alpha, unsigned char vertexalpha)
{
    return (unsigned char)(((alpha.shade ? vertexalpha : 255u) * alpha.constant + 127u) / 255u);
}

/* C0 is GoldenEye's texture marker. Its mode numbers differ from the raw
   RDP G_TX_* bits: 1 clamps, 2 mirrors, and 0/3 repeat. */
BgRenderFlags BgRenderMaterialWrap(const BgMaterial *material)
{
    DWORD s, t;
    BgRenderFlags flags = 0;
    if (BgMaterialTextureId(material) == BG_TEX_NONE)
    {
        return 0;
    }
    s = (material->textureword0 >> 22) & 3;
    t = (material->textureword0 >> 20) & 3;
    if (s == 1)
    {
        flags |= BG_RENDER_CLAMP_S;
    }
    if (s == 2)
    {
        flags |= BG_RENDER_MIRROR_S;
    }
    if (t == 1)
    {
        flags |= BG_RENDER_CLAMP_T;
    }
    if (t == 2)
    {
        flags |= BG_RENDER_MIRROR_T;
    }
    return flags;
}

double BgRenderWrapCoordinate(double coordinate, BgRenderFlags flags, BOOL t)
{
    if (flags & (t ? BG_RENDER_CLAMP_T : BG_RENDER_CLAMP_S))
    {
        return coordinate < 0 ? 0 : coordinate > 1 ? 1 : coordinate;
    }
    if (flags & (t ? BG_RENDER_MIRROR_T : BG_RENDER_MIRROR_S))
    {
        coordinate = fmod(coordinate, 2.0);
        if (coordinate < 0)
        {
            coordinate += 2.0;
        }
        return coordinate <= 1.0 ? coordinate : 2.0 - coordinate;
    }
    return coordinate - floor(coordinate);
}

int BgRenderWrapTexel(int texel, int size, BgRenderFlags flags, BOOL t)
{
    if (flags &
        (t ? BG_RENDER_CLAMP_T | BG_RENDER_MIRROR_T : BG_RENDER_CLAMP_S | BG_RENDER_MIRROR_S))
    {
        return texel < 0 ? 0 : texel >= size ? size - 1 : texel;
    }
    return (texel % size + size) % size;
}

void BgRenderPrepareEnvironment(BgVertex *vertex, BgRenderFlags flags, const BgMaterial *material)
{
    unsigned char rgb[3] = {vertex->r, vertex->g, vertex->b};
    double length = 0;
    int axis;
    ZeroMemory(&vertex->environment, sizeof(vertex->environment));
    if (!(flags & BG_RENDER_ENVIRONMENT))
    {
        return;
    }
    for (axis = 0; axis < 3; axis++)
    {
        int value = rgb[axis] < 128 ? rgb[axis] : (int)rgb[axis] - 256;
        vertex->environment.normal[axis] = (float)value;
        length += value * value;
    }
    if (length > 0)
    {
        float inverse = (float)(1.0 / sqrt(length));
        for (axis = 0; axis < 3; axis++)
        {
            vertex->environment.normal[axis] *= inverse;
        }
    }
    else
    {
        vertex->environment.normal[2] = 1;
    }
    /* gSPTexture's unsigned 0.16 scale maps generated 0..1024 coordinates
       into texels. texWriteTextureCmd preserves the authored scale word. */
    vertex->environment.scale[0] = (float)(material->modeword1 >> 16) / 64.0f;
    vertex->environment.scale[1] = (float)(material->modeword1 & 0xFFFFu) / 64.0f;
    vertex->r = vertex->g = vertex->b = 255;
}

void BgRenderEnvironmentCoordinates(const BgEnvironmentVertex *vertex, BgRenderFlags flags,
                                    const float right[3], const float up[3], float uv[2])
{
    double length = 0, inverse;
    int axis, component;
    for (component = 0; component < 3; component++)
    {
        length += (double)vertex->normal[component] * vertex->normal[component];
    }
    inverse = length > 0 ? 1.0 / sqrt(length) : 0;
    for (axis = 0; axis < 2; axis++)
    {
        const float *direction = axis ? up : right;
        double projection = 0;
        for (component = 0; component < 3; component++)
        {
            projection += vertex->normal[component] * direction[component];
        }
        projection *= inverse;
        if (projection < -1)
        {
            projection = -1;
        }
        if (projection > 1)
        {
            projection = 1;
        }
        /* N64 projects normals onto guLookAtReflect's right/up vectors;
           this is not OpenGL's eye-vector-based GL_SPHERE_MAP equation. */
        uv[axis] = (float)((flags & BG_RENDER_ENVIRONMENT_LINEAR)
                               ? acos(projection) / 3.14159265358979323846
                               : (projection + 1) * 0.5) *
                   vertex->scale[axis];
    }
}
