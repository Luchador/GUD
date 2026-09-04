#include <ultra64.h>
#include <memp.h>
#include "objecthandler.h"
#include "model.h"

/* Fixed scratch pool rebuilt into a doubly linked free list on stage load. */
ModelHitEntry g_ModelHitEntries[MODEL_HIT_ENTRY_POOL_SIZE];

Model *g_AnimatedModelSlots;
ModelSlot *g_ModelSlots;

s32 g_AnimatedModelSlotCount;
s32 g_ModelSlotCount;
s32 g_ModelIsLvResetting;
s32 g_ModelDistanceDisabled;
f32 g_ModelDistanceScale = 1.0;
struct Vertex* (*g_ModelVertexAllocator)(s32 numVertices);
void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx);
coord3d g_ModelZeroVector;
u32 g_ModelAnimMergingEnabled = 1;

Vertex g_GunfireVertexTemplate = {
    { 0, 0, 0 },
    0,
    { 0, 0 },
    0xFF,
    0xFF,
    0xFF,
    0xFF
};

s32 g_ModelShadowAlpha = 0x50;
Vtx g_ShadowVertexTemplate = {
    {
        { 0, 0, 0 },
        0,
        { 0, 0 },
        { 0xFF, 0xFF, 0xFF, 0x50 }
    }
};
struct ModelAnimationScratch *g_ModelAnimationScratch;


/**
 * Appends the model nodes that need independent depth sorting to a hit/render
 * list. Entries come from the fixed stage-lifetime pool above.
 */
ModelHitEntry *modelHitBuildNodeList(ModelHitEntry *head, Model *model)
{
    ModelHitEntry *firstNewEntry = g_ModelHitFreeList;
    ModelHitEntry *freeEntry = firstNewEntry;
    ModelNode *node = model->obj->RootNode;
    ModelNode *nextNode;

    while (node != NULL && freeEntry != NULL)
    {
        switch (node->Opcode & 0xff)
        {
            case MODELNODE_OPCODE_HEADER:
            case MODELNODE_OPCODE_GROUP:
            case MODELNODE_OPCODE_OP03:
            case MODELNODE_OPCODE_OP11:
            case MODELNODE_OPCODE_GUNFIRE:
            case MODELNODE_OPCODE_SHADOW:
            case MODELNODE_OPCODE_OP14:
            case MODELNODE_OPCODE_INTERLINK:
            case MODELNODE_OPCODE_OP16:
            case MODELNODE_OPCODE_GROUPSIMPLE:
                freeEntry->model = model;
                freeEntry->rootnode = node;
                freeEntry = freeEntry->next;
                break;
            default:
                break;
        }

        if (node->Child != NULL)
        {
            node = node->Child;
            continue;
        }

        while (node != NULL)
        {
            nextNode = node->Next;

            if (nextNode != NULL)
            {
                node = nextNode;
                break;
            }

            node = node->Parent;
        }
    }

    if (freeEntry != firstNewEntry)
    {
        if (head != NULL)
        {
            ModelHitEntry *tail = head;

            while (tail->next != NULL)
            {
                tail = tail->next;
            }

            tail->next = firstNewEntry;
            firstNewEntry->prev = tail;
        }
        else
        {
            head = firstNewEntry;
        }

        if (freeEntry != NULL)
        {
            ModelHitEntry *lastNewEntry = freeEntry->prev;

            lastNewEntry->next = NULL;
            freeEntry->prev = NULL;
        }

        g_ModelHitFreeList = freeEntry;
    }

    return head;
}


/**
 * Returns a chain of ModelHitEntry records to the free pool. Walks to
 * the chain's tail, splices the old free-list head after it (fixing the
 * head's prev backlink), and installs the chain as the new head. NULL
 * is a no-op. Cost is O(chain length) for the tail walk.
 */
void modelHitFreeChain(ModelHitEntry *entry)
{
    ModelHitEntry *tail;

    if (entry == NULL)
    {
        return;
    }

    if (g_ModelHitFreeList != NULL)
    {
        tail = entry;

        while (tail->next != NULL)
        {
            tail = tail->next;
        }

        tail->next = g_ModelHitFreeList;
        g_ModelHitFreeList->prev = tail;
    }

    entry->prev = NULL;
    g_ModelHitFreeList = entry;
}


#define MODEL_HIT_POINT_DEPTH(position, matrix) \
    (-((position)->x * (matrix)->m[0][2] \
     + (position)->y * (matrix)->m[1][2] \
     + (position)->z * (matrix)->m[2][2] \
     + (matrix)->m[3][2]))

#define MODEL_HIT_DEPTH_BIAS_1 0.0000152587890625f
#define MODEL_HIT_DEPTH_BIAS_2 0.000030517578125f
#define MODEL_HIT_DEPTH_BIAS_4 0.00006103515625f
#define MODEL_HIT_DEPTH_BIAS_6 0.000091552734375f

/** Calculates camera-space depths for sorting the independently rendered nodes. */
void modelHitCalculateNodeDepths(ModelHitEntry *head)
{
    ModelHitEntry *entry = head;
    ModelHitEntry *op16Entry = NULL;

    while (entry != NULL)
    {
        ModelNode *node = entry->rootnode;
        s32 opcode = node->Opcode & 0xff;

        switch (opcode)
        {
            case MODELNODE_OPCODE_HEADER:
            {
                union ModelRoData *data;
                ModelNode *othernode;
                Mtxf *mtx;

                data = node->Data;
                othernode = data->Header.FirstGroupNode;

                mtx = modelFindNodeMtx(entry->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(entry->model, othernode, 0);
                    entry->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    entry->sortvalue = -mtx->m[3][2];
                }
                break;
            }

            case MODELNODE_OPCODE_GROUP:
            {
                union ModelRoData *data;
                ModelNode *othernode;
                Mtxf *mtx;

                data = node->Data;
                othernode = data->Group.ChildGroupNode;

                mtx = modelFindNodeMtx(entry->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(entry->model, othernode, 0);
                    entry->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    entry->sortvalue = -mtx->m[3][2];
                }
                break;
            }

            case MODELNODE_OPCODE_OP03:
            {
                union ModelRoData *data;
                ModelNode *othernode;
                Mtxf *mtx;

                data = node->Data;
                othernode = data->Group.ChildGroupNode;

                mtx = modelFindNodeMtx(entry->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(entry->model, othernode, 0);
                    entry->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    entry->sortvalue = -mtx->m[3][2];
                }
                break;
            }

            case MODELNODE_OPCODE_GROUPSIMPLE:
            {
                Mtxf *mtx;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                entry->sortvalue = -mtx->m[3][2];
                break;
            }

            case MODELNODE_OPCODE_OP14:
            case MODELNODE_OPCODE_OP16:
            case MODELNODE_OPCODE_OP11:
            case MODELNODE_OPCODE_GUNFIRE:
            {
                Mtxf *mtx = modelFindNodeMtx(entry->model, node, 0);
                coord3d *position = (coord3d *)node->Data;

                entry->sortvalue = MODEL_HIT_POINT_DEPTH(position, mtx);

                if (opcode == MODELNODE_OPCODE_OP16)
                {
                    op16Entry = entry;
                }
                break;
            }

            case MODELNODE_OPCODE_INTERLINK:
            {
                Mtxf *mtx;
                ModelRoData_InterlinkageRecord *interlinkage;
                f32 sortvalue1;
                f32 sortvalue2;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                interlinkage = &node->Data->Interlinkage;

                sortvalue1 = MODEL_HIT_POINT_DEPTH(&interlinkage->pos, mtx);
                sortvalue2 = MODEL_HIT_POINT_DEPTH(&interlinkage->pos2, mtx);
                entry->sortvalue = sortvalue1 < sortvalue2 ? sortvalue1 : sortvalue2;
                break;
            }

            case MODELNODE_OPCODE_SHADOW:
            {
                Mtxf *mtx;
                ModelRoData_ShadowRecord *shadow;
                union ModelRwData *rwdata;

                mtx = modelFindNodeMtx(entry->model, node, 0);
                shadow = &node->Data->Shadow;

                rwdata = modelGetNodeRwData(entry->model, shadow->HeaderNode);

                entry->sortvalue = -(shadow->pos.x * mtx->m[0][2]
                                   + (rwdata->Header.ground - rwdata->Header.pos.y) * mtx->m[1][2]
                                   + shadow->pos.y * mtx->m[2][2]
                                   + mtx->m[3][2]);
                break;
            }
        }

        entry = entry->next;
    }

    if (op16Entry != NULL)
    {
        Model *model = op16Entry->model;

        if (model->attachedto != NULL)
        {
            /* Keep the four linked pieces in a stable painter's order as the
             * attached model changes orientation. */
            ModelNode **switches;
            ModelNode *referencedNode0e;
            ModelNode *op16Node;
            volatile ModelNode_Op16Record *op16Data;
            ModelNode *referencedNode0c;
            ModelNode *referencedNode10;
            ModelHitEntry *referencedEntry0e;
            ModelHitEntry *referencedEntry0c;
            ModelHitEntry *referencedEntry10;
            Mtxf *mtx;
            f32 zAxisDot;
            f32 yAxisDot;
            f32 depth0e;
            coord3d zAxis;
            coord3d yAxis;
            f32 depth0c;
            f32 depth10;
            coord3d worldPosition;
            ModelHitEntry *scan;
            f32 op16Depth;

            op16Depth = op16Entry->sortvalue;
            op16Node = op16Entry->rootnode;
            op16Data = (volatile ModelNode_Op16Record *)op16Node->Data;

            switches = model->attachedto->obj->Switches;

            referencedNode0e = switches[op16Data->nodeindex0e];
            referencedNode0c = switches[op16Data->nodeindex0c];
            referencedNode10 = switches[op16Data->nodeindex10];

            referencedEntry10 = NULL;
            referencedEntry0e = NULL;
            referencedEntry0c = NULL;

            mtx = modelFindNodeMtx(model, op16Node, 0);
            scan = head;

            while (scan != NULL)
            {
                if (referencedNode0e == scan->rootnode)
                {
                    referencedEntry0e = scan;
                }

                if (referencedNode0c == scan->rootnode)
                {
                    referencedEntry0c = scan;
                }

                if (referencedNode10 == scan->rootnode)
                {
                    referencedEntry10 = scan;
                }

                scan = scan->next;
            }

            zAxis.x = mtx->m[2][0];
            zAxis.y = mtx->m[2][1];
            zAxis.z = mtx->m[2][2];

            yAxis.x = mtx->m[1][0];
            yAxis.y = mtx->m[1][1];
            yAxis.z = mtx->m[1][2];

            worldPosition.x = op16Data->pos.x * mtx->m[0][0]
                            + op16Data->pos.y * mtx->m[1][0]
                            + op16Data->pos.z * mtx->m[2][0]
                            + mtx->m[3][0];
            worldPosition.y = op16Data->pos.x * mtx->m[0][1]
                            + op16Data->pos.y * mtx->m[1][1]
                            + op16Data->pos.z * mtx->m[2][1]
                            + mtx->m[3][1];
            worldPosition.z = op16Data->pos.x * mtx->m[0][2]
                            + op16Data->pos.y * mtx->m[1][2]
                            + op16Data->pos.z * mtx->m[2][2]
                            + mtx->m[3][2];

            zAxisDot = zAxis.x * worldPosition.x
                     + zAxis.y * worldPosition.y
                     + zAxis.z * worldPosition.z;
            yAxisDot = yAxis.x * worldPosition.x
                     + yAxis.y * worldPosition.y
                     + yAxis.z * worldPosition.z;

            depth0e = referencedEntry0e->sortvalue;
            depth0c = referencedEntry0c->sortvalue;
            depth10 = referencedEntry10->sortvalue;

            if (zAxisDot < 0.0f)
            {
                if (depth10 < depth0e)
                {
                    if (depth10 < op16Depth)
                    {
                        op16Entry->sortvalue = depth10 - MODEL_HIT_DEPTH_BIAS_2;
                    }
                }
                else
                {
                    if (depth0e < op16Depth)
                    {
                        op16Entry->sortvalue = depth0e - MODEL_HIT_DEPTH_BIAS_2;
                    }
                }
            }
            else if (0.0f <= zAxisDot)
            {
                if (op16Depth < depth0e)
                {
                    referencedEntry0e->sortvalue = op16Depth - MODEL_HIT_DEPTH_BIAS_4;
                }

                if (op16Depth < depth10)
                {
                    if (depth10 < depth0e)
                    {
                        referencedEntry10->sortvalue = op16Depth - MODEL_HIT_DEPTH_BIAS_6;
                    }
                    else
                    {
                        referencedEntry10->sortvalue = op16Depth - MODEL_HIT_DEPTH_BIAS_2;
                    }
                }
            }

            if (yAxisDot < 0.0f)
            {
                if (depth0c < op16Entry->sortvalue)
                {
                    if (referencedEntry0e->sortvalue < op16Entry->sortvalue
                            && depth0c < referencedEntry0e->sortvalue)
                    {
                        referencedEntry0e->sortvalue = depth0c - MODEL_HIT_DEPTH_BIAS_4;
                    }

                    if (referencedEntry10->sortvalue < op16Entry->sortvalue
                            && depth0c < referencedEntry10->sortvalue)
                    {
                        if (depth10 < depth0e)
                        {
                            referencedEntry10->sortvalue = depth0c - MODEL_HIT_DEPTH_BIAS_6;
                        }
                        else
                        {
                            referencedEntry10->sortvalue = depth0c - MODEL_HIT_DEPTH_BIAS_2;
                        }
                    }

                    op16Entry->sortvalue = depth0c - MODEL_HIT_DEPTH_BIAS_1;
                }
            }
            else if (0.0f <= yAxisDot)
            {
                if (op16Entry->sortvalue < depth0c)
                {
                    referencedEntry0c->sortvalue = op16Entry->sortvalue - MODEL_HIT_DEPTH_BIAS_1;

                    if (depth0c < depth0e)
                    {
                        if (referencedEntry0e->sortvalue < referencedEntry0c->sortvalue)
                        {
                            referencedEntry0c->sortvalue = referencedEntry0e->sortvalue - MODEL_HIT_DEPTH_BIAS_1;
                        }
                    }

                    if (depth0c < depth10)
                    {
                        if (referencedEntry10->sortvalue < referencedEntry0c->sortvalue)
                        {
                            referencedEntry0c->sortvalue = referencedEntry10->sortvalue - MODEL_HIT_DEPTH_BIAS_1;
                        }
                    }
                }
            }
        }
    }
}

#undef MODEL_HIT_POINT_DEPTH
#undef MODEL_HIT_DEPTH_BIAS_1
#undef MODEL_HIT_DEPTH_BIAS_2
#undef MODEL_HIT_DEPTH_BIAS_4
#undef MODEL_HIT_DEPTH_BIAS_6


/**
 * Address: 7F06BB28
 *
 * Stable descending linked-list merge sort. The original selection sort did
 * roughly n(n+1)/2 depth comparisons each frame; merge sort reduces that to
 * O(n log n) while preserving the order of entries with equal depth.
 */
ModelHitEntry *modelHitSortByDepth(ModelHitEntry *head)
{
    ModelHitEntry *left;
    ModelHitEntry *right;
    ModelHitEntry *node;
    ModelHitEntry *tail;
    s32 leftSize;
    s32 rightSize;
    s32 runSize;
    s32 mergeCount;
    s32 i;

    if (head == NULL || head->next == NULL)
    {
        return head;
    }

    runSize = 1;

    do
    {
        left = head;
        head = NULL;
        tail = NULL;
        mergeCount = 0;

        while (left != NULL)
        {
            mergeCount++;
            right = left;
            leftSize = 0;

            for (i = 0; i < runSize && right != NULL; i++)
            {
                leftSize++;
                right = right->next;
            }

            rightSize = runSize;

            while (leftSize > 0 || (rightSize > 0 && right != NULL))
            {
                if (leftSize == 0)
                {
                    node = right;
                    right = right->next;
                    rightSize--;
                }
                else if (rightSize == 0 || right == NULL)
                {
                    node = left;
                    left = left->next;
                    leftSize--;
                }
                else if (left->sortvalue >= right->sortvalue)
                {
                    node = left;
                    left = left->next;
                    leftSize--;
                }
                else
                {
                    node = right;
                    right = right->next;
                    rightSize--;
                }

                if (tail != NULL)
                {
                    tail->next = node;
                }
                else
                {
                    head = node;
                }

                node->prev = tail;
                tail = node;
            }

            left = right;
        }

        tail->next = NULL;
        runSize *= 2;
    }
    while (mergeCount > 1);

    return head;
}


void modelHitRenderNodeList(ModelRenderData *renderData, ModelHitEntry *entry)
{
    ModelNode *root;
    ModelNode *node;
    Model *model;
    RenderPosView *matrixSegment = NULL;
    ModelNodeRenderCache renderCache = {NULL, NULL, FALSE};
    s32 renderPrimary = renderData->flags & 1;
    s32 renderSecondary = renderData->flags & 2;
    s32 descend;
    s32 opcode;

    while (entry != NULL)
    {
        model = entry->model;
        root = entry->rootnode;
        node = root;

        if (matrixSegment != model->render_pos)
        {
            matrixSegment = model->render_pos;
            gSPSegment(renderData->gdl++, SPSEGMENT_MODEL_MTX, osVirtualToPhysical(matrixSegment));
        }

        if (node != NULL)
        {
            do
            {
                descend = 1;
                opcode = node->Opcode & 0xff;

                switch (opcode)
                {
                case MODELNODE_OPCODE_HEADER:
                case MODELNODE_OPCODE_GROUP:
                case MODELNODE_OPCODE_OP03:
                case MODELNODE_OPCODE_OP11:
                case MODELNODE_OPCODE_OP14:
                case MODELNODE_OPCODE_INTERLINK:
                case MODELNODE_OPCODE_OP16:
                case MODELNODE_OPCODE_GROUPSIMPLE:
                    if (node != root)
                    {
                        descend = 0;
                    }
                    break;
                case MODELNODE_OPCODE_GUNFIRE:
                    if (node == root)
                    {
                        if (renderSecondary)
                        {
                            renderCache.colorSegmentBase = NULL;
                            renderCache.vertexSegmentBase = NULL;
                            renderCache.type3PipelineReady = FALSE;
                            modelRenderGunfire(renderData, model, node);
                        }
                    }
                    else
                    {
                        descend = 0;
                    }
                    break;
                case MODELNODE_OPCODE_SHADOW:
                    if (node == root)
                    {
                        if (renderSecondary)
                        {
                            renderCache.colorSegmentBase = NULL;
                            renderCache.vertexSegmentBase = NULL;
                            renderCache.type3PipelineReady = FALSE;
                            modelRenderShadow(renderData, model, node);
                        }
                    }
                    else
                    {
                        descend = 0;
                    }
                    break;
                case MODELNODE_OPCODE_DL:
                    if ((renderPrimary && node->Data->DisplayList.Primary)
                            || (renderSecondary
                                && node->Data->DisplayList.Primary
                                && node->Data->DisplayList.ModelType == 4
                                && node->Data->DisplayList.Secondary))
                    {
                        renderCache.colorSegmentBase = NULL;
                        renderCache.vertexSegmentBase = NULL;
                        renderCache.type3PipelineReady = FALSE;
                        modelRenderNodeGundl(renderData, node);
                    }
                    break;
                case MODELNODE_OPCODE_DLCOLLISION:
                    if (renderPrimary
                            || (renderSecondary
                                && node->Data->DisplayListCollisions.ModelType == 4
                                && node->Data->DisplayListCollisions.Secondary))
                    {
                        modelRenderNodeDlWithCache(renderData, model, node, &renderCache);
                    }
                    break;
                case MODELNODE_OPCODE_DLPRIMARY:
                    if (renderSecondary && node->Data->DisplayListPrimary.Primary)
                    {
                        renderCache.colorSegmentBase = NULL;
                        renderCache.vertexSegmentBase = NULL;
                        renderCache.type3PipelineReady = FALSE;
                        modelRenderRotatingTexture(renderData, node);
                    }
                    break;
                case MODELNODE_OPCODE_LOD:
                    modelApplyDistanceRelations(model, node);
                    break;
                case MODELNODE_OPCODE_BSP:
                    modelApplyReorderRelations(model, node);
                    break;
                case MODELNODE_OPCODE_SWITCH:
                    modelApplyToggleRelations(model, node);
                    break;
                case MODELNODE_OPCODE_HEAD:
                    modelApplyHeadRelations(model, node);
                    break;
                case MODELNODE_OPCODE_OP05:
                case MODELNODE_OPCODE_OP06:
                case MODELNODE_OPCODE_OP07:
                case MODELNODE_OPCODE_BBOX:
                case MODELNODE_OPCODE_OP19:
                case MODELNODE_OPCODE_OP20:
                default:
                    break;
                }

                if (descend && node->Child != NULL)
                {
                    node = node->Child;
                }
                else if (node != NULL)
                {
                    do
                    {
                        if (node == root)
                        {
                            node = NULL;
                            break;
                        }

                        if (node->Next != NULL)
                        {
                            node = node->Next;
                            break;
                        }

                        node = node->Parent;
                    }
                    while (node != NULL);
                }
            }
            while (node != NULL);
        }

        entry = entry->next;
    }
}


/** Continues a back-to-front bounding-box ray query from the previous hit. */
s32 modelHitFindNextBBoxHit(ModelHitEntry **entryPtr, coord3d *rayOrigin, coord3d *rayDirection, Model **hitModel, ModelNode **hitNode)
{
    ModelHitEntry *entry = *entryPtr;

    while (entry != NULL)
    {
        Model *model = entry->model;
        ModelNode *root = entry->rootnode;
        ModelNode *node = *hitNode != NULL ? *hitNode : root;
        s32 descend = TRUE;

        *hitNode = NULL;

        while (node != NULL)
        {
            if (descend && node->Child != NULL)
            {
                node = node->Child;
            }
            else
            {
                while (node != NULL)
                {
                    if (node == root)
                    {
                        node = NULL;
                        break;
                    }

                    if (node->Next != NULL)
                    {
                        node = node->Next;
                        break;
                    }

                    node = node->Parent;
                }

                if (node == NULL)
                {
                    break;
                }
            }

            descend = TRUE;

            switch (node->Opcode & 0xff)
            {
                case MODELNODE_OPCODE_HEADER:
                case MODELNODE_OPCODE_GROUP:
                case MODELNODE_OPCODE_OP03:
                case MODELNODE_OPCODE_OP11:
                case MODELNODE_OPCODE_GUNFIRE:
                case MODELNODE_OPCODE_SHADOW:
                case MODELNODE_OPCODE_OP14:
                case MODELNODE_OPCODE_INTERLINK:
                case MODELNODE_OPCODE_OP16:
                case MODELNODE_OPCODE_GROUPSIMPLE:
                    descend = FALSE;
                    break;
                case MODELNODE_OPCODE_BBOX:
                    if (modelTestRayIntersectsNodeBBox(model, node, rayOrigin, rayDirection))
                    {
                        *hitModel = model;
                        *hitNode = node;
                        *entryPtr = entry;

                        return *(s32 *)node->Data;
                    }

                    descend = FALSE;
                    break;
                case MODELNODE_OPCODE_LOD:
                    modelApplyDistanceRelations(model, node);
                    break;
                case MODELNODE_OPCODE_SWITCH:
                    modelApplyToggleRelations(model, node);
                    break;
                case MODELNODE_OPCODE_HEAD:
                    modelApplyHeadRelations(model, node);
                    break;
                case MODELNODE_OPCODE_DLCOLLISION:
                default:
                    break;
            }
        }

        entry = entry->prev;
    }

    *entryPtr = NULL;
    return 0;
}


/** Starts a back-to-front bounding-box ray query at the list tail. */
s32 modelHitFindFirstBBoxHit(ModelHitEntry **entryPtr, coord3d *rayOrigin, coord3d *rayDirection, Model **hitModel, ModelNode **hitNode)
{
    ModelHitEntry *entry = *entryPtr;

    *hitModel = NULL;
    *hitNode = NULL;

    if (entry == NULL)
    {
        return 0;
    }

    while (entry->next != NULL)
    {
        entry = entry->next;
    }

    *entryPtr = entry;

    return modelHitFindNextBBoxHit(entryPtr, rayOrigin, rayDirection, hitModel, hitNode);
}
