#ifndef GEDITOR_BGRENDER_H
#define GEDITOR_BGRENDER_H

#include <windows.h>

/* Host preview flags derived from the preserved Fast3D stream. These are
 * presentation data, never written back as replacement N64 commands. */
enum BgRenderFlags
{
    BG_RENDER_DEPTH_TEST = 1,
    BG_RENDER_DEPTH_WRITE = 2,
    BG_RENDER_DECAL = 4,
    BG_RENDER_BLEND = 8,
    BG_RENDER_ALPHA_TEST = 16
};

typedef struct BgRenderState
{
    DWORD othermode;
    BOOL zbuffer;
} BgRenderState;

void BgRenderStateInit(BgRenderState *state, BOOL secondary);
void BgRenderStateRead(BgRenderState *state, DWORD word0, DWORD word1);
unsigned char BgRenderStateFlags(const BgRenderState *state);
unsigned char BgRenderDefaultFlags(BOOL secondary);

#endif
