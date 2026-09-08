#include "bgrender.h"

/* Fast3D / RDP encodings from include/PR/gbi.h, without its N64 ABI types. */
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
    state->zbuffer = TRUE;
    state->othermode = BG_Z_CMP | (secondary ? BG_ZMODE_XLU | BG_FORCE_BL : BG_Z_UPD);
}

void BgRenderStateRead(BgRenderState *state, DWORD word0, DWORD word1)
{
    switch (word0 >> 24)
    {
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
