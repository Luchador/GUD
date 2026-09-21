#include <ultra64.h>
#include <memp.h>
#include <math.h>
#include <bondtypes.h>
#include "occlusion.h"
#include "occlusionmath.h"
#include "cam.h"
#include "bg.h"
#include "player.h"
#include "model.h"

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

/* Use the full authored model radius at each posed joint, rather than a
 * standing-height box or the smaller blood/depth-sorting joint sizes. This
 * deliberately overestimates animated limbs and skinned seams. The existing
 * hit/render chain also includes the current head, held weapons and hat.
 * Only primary joint matrices are read: LOD may omit secondary seam matrices.
 * No cache/allocation, onscreen flag changes or changes to the hit chain. */
int occlusionTestCharacter(struct ModelHitEntry *entry)
{
    Mtxf *view;
    u32 candidates;
    s32 tested = 0;
    s32 visits = 0;
    if (!g_OcclusionEnabled || !g_OccluderCount || !entry) { return FALSE; }
    view = currentPlayerGetViewToWorldMtxf();
    if (!view) { return FALSE; }
    candidates = g_OccluderCount == 32 ? 0xffffffffU : (1U << g_OccluderCount) - 1;
    for (; entry; entry = entry->next) {
        Model *model = entry->model;
        ModelNode *node = entry->rootnode;
        Mtxf *mtx;
        f32 local[3] = {0, 0, 0};
        f32 position[3], camera[3], radius, norm = 0, joint;
        s32 i, j, k, index;
        if (++visits > 256 || !model || !model->obj || !model->render_pos
                || !node || !node->Data || !(model->scale > 0)) { return FALSE; }
        radius = model->obj->BoundingVolumeRadius;
        switch (node->Opcode & 255) {
        case MODELNODE_OPCODE_HEADER:
        case MODELNODE_OPCODE_GROUP:
        case MODELNODE_OPCODE_OP03:
        case MODELNODE_OPCODE_GROUPSIMPLE:
            /* Uncommon bend/stretch joints need their extra matrix bounded
             * separately; keep them visible until that path is supported. */
            if ((node->Opcode & 255) != MODELNODE_OPCODE_HEADER
                    && (node->Opcode & 255) != MODELNODE_OPCODE_GROUPSIMPLE
                    && (node->Opcode & 0x200)) { return FALSE; }
            /* A separately authored head can have a larger local radius. */
            joint = getjointsize(model, node) / model->scale;
            if (!(joint >= 0)) { return FALSE; }
            if (joint > radius) { radius = joint; }
            break;
        case MODELNODE_OPCODE_GUNFIRE:
            if (!modelGetNodeRwData(model, node)->Gunfire.visible) { continue; }
            for (i = 0; i < 3; i++) { local[i] = node->Data->Gunfire.Offset.f[i]; }
            /* Maximum random size, billboard rotation, X offset and s16
             * vertex rounding. Deliberately includes the flash's full extent. */
            radius = 2.0f;
            for (i = 0; i < 3; i++) {
                f32 size = node->Data->Gunfire.Size.f[i];
                if (size < 0.0f) { size = -size; }
                radius += 2.0f * size;
            }
            break;
        case MODELNODE_OPCODE_SHADOW:
            /* modelHitRenderNodeList does not render these nodes. */
            continue;
        default:
            /* Special/procedural node bounds have not been proved. */
            return FALSE;
        }
        if (!(radius > 0) || !(radius < 100000000.0f)) { return FALSE; }
        index = modelFindNodeMtxIndex(node, 0);
        if (index < 0 || index >= model->obj->numMatrices) { return FALSE; }
        mtx = &model->render_pos[index].pos;
        for (i = 0; i < 3; i++) {
            camera[i] = mtx->m[3][i];
            for (j = 0; j < 3; j++) { camera[i] += local[j]*mtx->m[j][i]; }
        }
        for (i = 0; i < 3; i++) {
            position[i] = view->m[3][i];
            for (j = 0; j < 3; j++) { position[i] += camera[j]*view->m[j][i]; }
            for (j = 0; j < 3; j++) {
                f32 value = 0;
                for (k = 0; k < 3; k++) { value += mtx->m[i][k]*view->m[k][j]; }
                norm += value*value;
            }
        }
        radius *= sqrtf(norm);
        for (i = 0; i < g_OccluderCount; i++) {
            if ((candidates & (1U << i))
                    && !occlusionBoxContainsSphere(&g_Occluders[i], position, radius)) {
                candidates &= ~(1U << i);
            }
        }
        if (!candidates) { return FALSE; }
        tested++;
    }
    if (!tested) { return FALSE; }
    g_OcclusionRejected++;
    return TRUE;
}

void occlusionToggle(void) { g_OcclusionEnabled = !g_OcclusionEnabled; }
int occlusionEnabled(void) { return g_OcclusionEnabled; }
int occlusionCount(void) { return g_OccluderCount; }
/* Render calls skipped, not unique entities (opaque/alpha passes may both visit). */
int occlusionRejected(void) { return g_OcclusionRejected; }
