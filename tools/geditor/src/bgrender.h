#ifndef GEDITOR_BGRENDER_H
#define GEDITOR_BGRENDER_H

#include <windows.h>
#include "bgmaterial.h"

/* Host preview flags derived from the preserved Fast3D stream. These are
 * presentation data, never written back as replacement N64 commands. */
typedef unsigned int BgRenderFlags;

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
    BG_RENDER_ENVIRONMENT = 1024,
    BG_RENDER_ENVIRONMENT_LINEAR = 2048,
    BG_RENDER_MONITOR = 4096, /* Derived setup screen; drawn by monitor preview. */
    BG_RENDER_CULL_BACK = 8192,
    BG_RENDER_CULL_FRONT = 16384,
    BG_RENDER_CULL_EXPLICIT = 32768, /* Resolved preview sides, including two-sided. */
    BG_RENDER_NO_FOG = 65536, /* Vertex alpha needs the shade alpha channel. */
    /* Model preview metadata: retain each unwritten native bit so a flipped
       door can resolve the caller's FRONT default before reversing winding. */
    BG_RENDER_CULL_BACK_INHERITED = 131072,
    BG_RENDER_CULL_FRONT_INHERITED = 262144,
    BG_RENDER_ENVIRONMENT_FACE = 524288, /* Generated flat normal, updated during BG drags. */
    BG_RENDER_CULL_MASK = BG_RENDER_CULL_BACK | BG_RENDER_CULL_FRONT | BG_RENDER_CULL_EXPLICIT
                       | BG_RENDER_CULL_BACK_INHERITED | BG_RENDER_CULL_FRONT_INHERITED,
    BG_RENDER_ENVIRONMENT_MASK = BG_RENDER_ENVIRONMENT | BG_RENDER_ENVIRONMENT_LINEAR | BG_RENDER_ENVIRONMENT_FACE,
    BG_RENDER_WRAP_MASK = BG_RENDER_CLAMP_S | BG_RENDER_MIRROR_S
                       | BG_RENDER_CLAMP_T | BG_RENDER_MIRROR_T
};

typedef struct BgRenderState
{
    DWORD othermode, othermodehigh;
    DWORD fogothermode; /* Low mode after the fog LUT, including partial writes. */
    DWORD othermodeknown; /* Low-mode bits explicitly written by the asset. */
    DWORD othermodehighknown;
    BOOL zbuffer;
    DWORD geometryknown; /* Bits explicitly set/cleared by the native stream. */
    DWORD geometrymode; /* lighting and texture generation, including partial clears */
    unsigned char environmentalpha, primitivealpha;
    DWORD environmentword1; /* Preserve RGB when editing constant opacity. */
    DWORD primitiveword0, primitiveword1; /* Full color/LOD state for detail-tile restoration. */
    DWORD surfacepolicy; /* Auto for streams without an editor override. */
    DWORD surfacebasemode; /* Native mode to restore when returning to Auto. */
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

/* World model previews use backface culling unless the native stream changes
   it. Keep draw defaults separate from geometryknown, which editing uses to
   distinguish authored commands from caller-dependent state. */
BgRenderFlags BgRenderModelCulling(const BgRenderState *state);
BgRenderFlags BgRenderResolveModelCulling(BgRenderFlags flags, BOOL flippeddoor);

typedef enum BgTransparency {
    BG_TRANSPARENCY_OPAQUE, BG_TRANSPARENCY_CUTOUT, BG_TRANSPARENCY_BLEND,
    BG_TRANSPARENCY_DECAL, BG_TRANSPARENCY_CUTOUT_BLEND, BG_TRANSPARENCY_UNKNOWN,
    BG_TRANSPARENCY_AUTO /* Editor choice; not a native render-mode category. */
} BgTransparency;
/* Classify the authored render mode using the preview decoder. Inherited
   layer defaults alone do not establish a known asset transparency type. */
BgTransparency BgRenderGetTransparency(const BgRenderState *state);
/* Transparency and coplanar depth handling are independent face properties. */
BgTransparency BgRenderGetSurfaceTransparency(const BgRenderState *state);
/* Safe surface presets for explicit, ordinary one/two-cycle BG pipelines.
   Preserve the combiner, first-cycle fog, alpha/depth source and cycle type. */
BOOL BgRenderSurfacePreset(const BgRenderState *state, BgTransparency surface, DWORD *modeout);
BOOL BgRenderDecalPreset(const BgRenderState *state, BOOL decal, DWORD *modeout);
BOOL BgRenderSupportsVertexAlpha(const BgRenderState *state);
BOOL BgRenderSupportsAlphaPreset(const BgRenderState *state, const BgMaterial *material, DWORD source);
BOOL BgRenderSupportsFog(const BgRenderState *state, const BgMaterial *material);
BOOL BgRenderUsesFog(const BgRenderState *state, const BgMaterial *material);

/* Matches texModeToGbiMode: mode 3, like mode 0, means ordinary repeat. */
BgRenderFlags BgRenderMaterialWrap(const BgMaterial *material);
/* Fold normalized coordinates before linear filtering. Clamp/mirror use
   clamped edge texels; repeat blends opposite edges at its seam. */
double BgRenderWrapCoordinate(double coordinate, BgRenderFlags flags, BOOL t);
int BgRenderWrapTexel(int texel, int size, BgRenderFlags flags, BOOL t);

/* Decode signed normal bytes without changing the editable/source vertex.
 * Generated surfaces use neutral RGB instead of treating normals as colors. */
DWORD BgRenderTriangleNormal(const double a[3], const double b[3], const double c[3]);
void BgRenderPrepareEnvironment(BgVertex *vertex, BgRenderFlags flags,
                                 const BgMaterial *material);
void BgRenderEnvironmentCoordinates(const BgEnvironmentVertex *vertex, BgRenderFlags flags,
                                     const float right[3], const float up[3], float uv[2]);

#endif
