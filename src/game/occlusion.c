#include <ultra64.h>
#include <memp.h>
#include <bondtypes.h>
#include "occlusion.h"
#include "occlusionmath.h"
#include "cam.h"
#include "bg.h"
#include "player.h"

static OcclusionBox *g_Occluders;
static s32 g_OccluderCount;
static s32 g_OcclusionRejected;
static s32 g_OcclusionEnabled = TRUE;

void occlusionReset(void)
{
    g_Occluders = NULL;
    g_OccluderCount = 0;
    g_OcclusionRejected = 0;
    g_OcclusionEnabled = TRUE;
}

void occlusionLoad(struct BoundPadRecord *pads)
{
    struct BoundPadRecord *pad;
    s32 count = 0;
    if (!pads) { return; }
    for (pad = pads; pad->plink; pad++) {
        if ((u32)pad->stan == OCCLUDER_PAD_TAG) { count++; }
    }
    /* Invalid/excess input fails open; the editor rejects it before export. */
    if (count > 0 && count <= OCCLUDER_MAX) {
        g_Occluders = mempAllocBytesInBank((count*sizeof(*g_Occluders)+15)&~15, MEMPOOL_STAGE);
    }
    for (pad = pads; pad->plink; pad++) {
        if ((u32)pad->stan == OCCLUDER_PAD_TAG) {
            float bounds[6];
            pad->stan = NULL;
            bounds[0] = pad->bbox.xmin; bounds[1] = pad->bbox.xmax;
            bounds[2] = pad->bbox.ymin; bounds[3] = pad->bbox.ymax;
            bounds[4] = pad->bbox.zmin; bounds[5] = pad->bbox.zmax;
            if (g_Occluders && occlusionBoxInit(&g_Occluders[g_OccluderCount],
                pad->pos.f, pad->up.f, pad->look.f, bounds)) { g_OccluderCount++; }
        }
    }
}

void occlusionBeginView(void)
{
    Mtxf *view = currentPlayerGetViewToWorldMtxf();
    f32 clip[4];
    s32 i;
    g_OcclusionRejected = 0;
    if (!g_OccluderCount) { return; }
    /* Use the rendered eye, including cinema/frozen cameras and split-screen. */
    if (view) {
        clip[3] = -g_CurrentPlayer->c_perspnear / bgGetLevelRenderScale();
        for (i = 0; i < 3; i++) {
            clip[i] = -view->m[2][i];
            clip[3] -= clip[i]*view->m[3][i];
        }
    }
    for (i = 0; i < g_OccluderCount; i++) {
        g_Occluders[i].count = 0;
        if (g_OcclusionEnabled && view) { occlusionBoxView(&g_Occluders[i], view->m[3], clip); }
    }
}

int occlusionTestSphere(const float position[3], float radius)
{
    s32 i;
    if (!g_OcclusionEnabled) { return FALSE; }
    for (i = 0; i < g_OccluderCount; i++) {
        if (occlusionBoxContainsSphere(&g_Occluders[i], position, radius)) {
            g_OcclusionRejected++;
            return TRUE;
        }
    }
    return FALSE;
}

void occlusionToggle(void) { g_OcclusionEnabled = !g_OcclusionEnabled; }
int occlusionEnabled(void) { return g_OcclusionEnabled; }
int occlusionCount(void) { return g_OccluderCount; }
/* Render calls skipped, not unique props (opaque/alpha passes may both visit). */
int occlusionRejected(void) { return g_OcclusionRejected; }
