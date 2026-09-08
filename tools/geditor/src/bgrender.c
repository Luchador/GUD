#include "bgrender.h"

/* Fast3D / RDP encodings from include/PR/gbi.h, without its N64 ABI types. */
#define BG_G_SETENVCOLOR 0xFBu
#define BG_G_SETPRIMCOLOR 0xFAu
#define BG_G_SETOTHERMODE_L 0xB9u
#define BG_G_RDPSETOTHERMODE 0xEFu
#define BG_G_CLEARGEOMETRYMODE 0xB6u
#define BG_G_SETGEOMETRYMODE 0xB7u
#define BG_G_ZBUFFER 1u
#define BG_Z_CMP 0x10u
#define BG_Z_UPD 0x20u
#define BG_ZMODE_MASK 0xC00u
#define BG_ZMODE_DEC 0xC00u
#define BG_ZMODE_XLU 0x800u
#define BG_FORCE_BL 0x4000u
#define BG_CVG_X_ALPHA 0x1000u
#define BG_ALPHA_COMPARE_MASK 3u

void BgRenderStateInit(BgRenderState *state, BOOL secondary)
{
    state->environmentalpha = state->primitivealpha = 255;
    state->zbuffer = TRUE;
    state->othermode = BG_Z_CMP | (secondary ? BG_ZMODE_XLU | BG_FORCE_BL : BG_Z_UPD);
}

void BgRenderStateRead(BgRenderState *state, DWORD word0, DWORD word1)
{
    switch (word0 >> 24)
    {
    case BG_G_SETENVCOLOR:
        state->environmentalpha = (unsigned char)word1;
        break;
    case BG_G_SETPRIMCOLOR:
        state->primitivealpha = (unsigned char)word1;
        break;
    case BG_G_SETOTHERMODE_L:
    {
        /* Fast3D encodes the shift and bit count directly. Alpha/depth-source
         * commands must not erase the render-mode bits inherited by a group. */
        unsigned int shift = (word0 >> 8) & 0xFFu;
        unsigned int count = word0 & 0xFFu;
        DWORD mask;
        if (!count || shift >= 32 || count > 32 - shift)
        {
            break;
        }
        mask = count == 32 ? 0xFFFFFFFFu : ((1u << count) - 1u) << shift;
        state->othermode = (state->othermode & ~mask) | (word1 & mask);
        break;
    }
    case BG_G_RDPSETOTHERMODE:
        state->othermode = word1;
        break;
    case BG_G_CLEARGEOMETRYMODE:
        if (word1 & BG_G_ZBUFFER)
        {
            state->zbuffer = FALSE;
        }
        break;
    case BG_G_SETGEOMETRYMODE:
        if (word1 & BG_G_ZBUFFER)
        {
            state->zbuffer = TRUE;
        }
        break;
    }
}

unsigned char BgRenderStateFlags(const BgRenderState *state)
{
    unsigned char flags = 0;
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
    return flags;
}

unsigned char BgRenderDefaultFlags(BOOL secondary)
{
    BgRenderState state;
    BgRenderStateInit(&state, secondary);
    return BgRenderStateFlags(&state);
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
    BgRenderAlpha alpha = {255, TRUE, TRUE};
    DWORD w0 = material->combineword0, w1 = material->combineword1;
    unsigned int i;
    unsigned int a = (w1 >> 21) & 7, b = (w1 >> 3) & 7;
    unsigned int c = (w1 >> 18) & 7, d = w1 & 7;
    int source = -1;

    for (i = 0; i < sizeof(remapped) / sizeof(remapped[0]); i++)
    {
        if (w0 == remapped[i].word0 && w1 == remapped[i].word1)
        {
            alpha.constant = state->environmentalpha;
            alpha.shade = FALSE;
            alpha.texture = remapped[i].texture;
            return alpha;
        }
    }
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
