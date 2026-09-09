#ifndef GEDITOR_BGRENDER_H
#define GEDITOR_BGRENDER_H

#include <windows.h>
#include "bgmaterial.h"

/* Host preview flags derived from the preserved Fast3D stream. These are
 * presentation data, never written back as replacement N64 commands. */
typedef unsigned short BgRenderFlags;

enum BgRenderFlag
{
    BG_RENDER_DEPTH_TEST = 1,
    BG_RENDER_DEPTH_WRITE = 2,
    BG_RENDER_DECAL = 4,
    BG_RENDER_BLEND = 8,
    BG_RENDER_ALPHA_TEST = 16,
    BG_RENDER_IGNORE_TEXTURE_ALPHA = 32,
    BG_RENDER_CLAMP_S = 64,
    BG_RENDER_MIRROR_S = 128,
    BG_RENDER_CLAMP_T = 256,
    BG_RENDER_MIRROR_T = 512,
    BG_RENDER_WRAP_MASK = BG_RENDER_CLAMP_S | BG_RENDER_MIRROR_S
                       | BG_RENDER_CLAMP_T | BG_RENDER_MIRROR_T
};

typedef struct BgRenderState
{
    DWORD othermode;
    BOOL zbuffer;
    unsigned char environmentalpha, primitivealpha;
} BgRenderState;

/* Common BG alpha products: optional texture alpha multiplied by a constant
 * and/or vertex alpha. RGB shading is independent of this preview state. */
typedef struct BgRenderAlpha
{
    unsigned char constant;
    BOOL shade, texture;
} BgRenderAlpha;

BgRenderAlpha BgRenderGetMaterialAlpha(const BgRenderState *state, const BgMaterial *material);
BgRenderAlpha BgRenderGetAlpha(const BgRenderState *state, const BgMaterial *material);
unsigned char BgRenderVertexAlpha(BgRenderAlpha alpha, unsigned char vertexalpha);

void BgRenderStateInit(BgRenderState *state, BOOL secondary);
void BgRenderStateRead(BgRenderState *state, DWORD word0, DWORD word1);
BgRenderFlags BgRenderStateFlags(const BgRenderState *state);
BgRenderFlags BgRenderDefaultFlags(BOOL secondary);

/* Matches texModeToGbiMode: mode 3, like mode 0, means ordinary repeat. */
BgRenderFlags BgRenderMaterialWrap(const BgMaterial *material);
/* Fold normalized coordinates before linear filtering. Clamp/mirror use
   clamped edge texels; repeat blends opposite edges at its seam. */
double BgRenderWrapCoordinate(double coordinate, BgRenderFlags flags, BOOL t);
int BgRenderWrapTexel(int texel, int size, BgRenderFlags flags, BOOL t);

#endif
