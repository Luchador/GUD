#include <ultra64.h>
#include <PR/R4300.h>
#include "modellod.h"
#include "bondview.h"
#include "objecthandler.h"
#include "matrixmath.h"

#define MODEL_LOD_CACHE_SIZE 16
#define MODEL_LOD_MATRIX_LIMIT 32
#define MODEL_LOD_JOINT_LIMIT 8
#define MODEL_LOD_GATE_LIMIT 2
#define MODEL_LOD_LIST_LIMIT 32
#define MODEL_LOD_NODE_LIMIT 256
#define MODEL_LOD_INSTANCE_COUNT 64

typedef struct ModelLodGate {
    ModelRoData_LODRecord *lod;
    s32 matrix;
} ModelLodGate;

typedef struct ModelLodJoint {
    ModelNode *node;
    ModelLodGate gates[MODEL_LOD_GATE_LIMIT];
    s32 count;
} ModelLodJoint;

typedef struct ModelLodPlan {
    ModelFileHeader *header;
    ModelNode *head;
    ModelRoData_DisplayList_CollisionRecord *lists[MODEL_LOD_LIST_LIMIT];
    ModelLodJoint joints[MODEL_LOD_JOINT_LIMIT];
    s8 byMatrix[MODEL_LOD_MATRIX_LIMIT];
    s32 numLists;
    s32 numJoints;
    u32 usedMatrices;
    bool valid;
} ModelLodPlan;

typedef struct ModelLodInstance {
    Model *model;
    ModelLodPlan *plan;
    void *datas;
    ModelFileHeader *head;
    void *headDatas;
    u32 headMatrices;
} ModelLodInstance;

/* Metadata only: no per-character allocation and no changes to model files. */
static ModelLodPlan g_ModelLodCache[MODEL_LOD_CACHE_SIZE];
static ModelLodInstance g_ModelLodInstances[MODEL_LOD_INSTANCE_COUNT];
static ModelLodPlan *g_ModelLodPlan;
Model *g_ModelLodModel;
static u32 g_ModelLodHeadMatrices;
static bool g_ModelLodChecked;
static f32 g_ModelLodViewScale;

void modelLodEnd(void)
{
    g_ModelLodPlan = NULL;
    g_ModelLodModel = NULL;
}

void modelLodResetCache(void)
{
    s32 i;
    modelLodEnd();
    for (i = 0; i < MODEL_LOD_CACHE_SIZE; i++) g_ModelLodCache[i].header = NULL;
    for (i = 0; i < MODEL_LOD_INSTANCE_COUNT; i++) g_ModelLodInstances[i].model = NULL;
}

void modelLodInvalidateInstance(Model *model)
{
    ModelLodInstance *entry = &g_ModelLodInstances[((u32)model >> 2) & (MODEL_LOD_INSTANCE_COUNT - 1)];
    if (entry->model == model) entry->model = NULL;
    if (g_ModelLodModel == model) modelLodEnd();
}

/* Refuse indirect lists and matrix/segment edits: their dependencies cannot
 * be proved by this bounded scan. Texture/material commands are unaffected. */
static bool modelLodListMatrices(Gfx *source, void *base, u32 *mask)
{
    u32 address;
    s32 count;
    s32 i;
    if (!source) return TRUE;
    address = (u32)source;
    if ((address >> 24) == SPSEGMENT_MODEL_COL1)
    {
        u32 offset = address & 0x00ffffff;
        if (!IS_KSEG0(base) || K0_TO_PHYS(base) >= osMemSize
                || offset >= osMemSize - K0_TO_PHYS(base)) return FALSE;
        source = (Gfx *)((u8 *)base + offset);
    }
    address = K0_TO_PHYS(source);
    if (!IS_KSEG0(source) || (address & 7) || address >= osMemSize) return FALSE;
    count = (osMemSize - address) / sizeof(Gfx);
    if (count > 8192) count = 8192;
    for (i = 0; i < count; i++)
    {
        u32 op = source[i].words.w0 >> 24;
        u32 arg = source[i].words.w1;
        if (op == (u8)G_ENDDL) return TRUE;
        if (op == (u8)G_DL || op == (u8)G_MOVEMEM) return FALSE;
        if (op == (u8)G_MOVEWORD && ((source[i].words.w0 & 255) == G_MW_SEGMENT
                || (source[i].words.w0 & 255) == G_MW_MATRIX)) return FALSE;
        if (op == (u8)G_MTX)
        {
            u32 offset = arg & 0x00ffffff;
            if ((source[i].words.w0 & 0xffff) != 64
                    || (arg >> 24) != SPSEGMENT_MODEL_MTX || (offset & 63)
                    || offset / 64 >= MODEL_LOD_MATRIX_LIMIT) return FALSE;
            *mask |= 1U << (offset / 64);
        }
    }
    return FALSE;
}

static bool modelLodAddNode(ModelNode **nodes, s32 *count, ModelNode *node)
{
    s32 i;
    if (!node) return TRUE;
    for (i = 0; i < *count; i++) if (nodes[i] == node) return TRUE;
    if (*count == MODEL_LOD_NODE_LIMIT) return FALSE;
    nodes[(*count)++] = node;
    return TRUE;
}

static ModelNode *modelLodMatrixNode(ModelNode *node)
{
    while (node)
    {
        u32 type = node->Opcode & 255;
        if (type == MODELNODE_OPCODE_HEADER || type == MODELNODE_OPCODE_GROUP
                || type == MODELNODE_OPCODE_GROUPSIMPLE) return node;
        node = node->Parent;
    }
    return NULL;
}

/* A dependency is usable only if its depth matrix is this joint's primary
 * matrix or an ancestor's. It has then already been built this frame, even
 * when BSP nodes reorder limbs. Never consult last frame's visibility. */
static void modelLodAddUse(ModelLodJoint *joint, ModelNode *list)
{
    ModelNode *lod = list->Parent;
    ModelNode *parent;
    ModelNode *owner;
    s32 i;
    if (joint->count < 0) return;
    while (lod && (lod->Opcode & 255) != MODELNODE_OPCODE_LOD) lod = lod->Parent;
    if (!lod) { joint->count = -1; return; }
    parent = modelLodMatrixNode(lod);
    owner = joint->node;
    while (owner && owner != parent) owner = owner->Parent;
    if (!owner) { joint->count = -1; return; }
    for (i = 0; i < joint->count; i++) if (joint->gates[i].lod == &lod->Data->LOD) return;
    if (joint->count == MODEL_LOD_GATE_LIMIT) { joint->count = -1; return; }
    joint->gates[joint->count].lod = &lod->Data->LOD;
    joint->gates[joint->count].matrix = modelFindNodeMtxIndex(parent, 0);
    joint->count++;
}

static void modelLodBuildPlan(ModelLodPlan *plan, ModelFileHeader *header)
{
    ModelNode *nodes[MODEL_LOD_NODE_LIMIT];
    u32 assigned = 0;
    s32 count = 0;
    s32 i;
    s32 j;
    bool body = (header->RootNode->Opcode & 255) == MODELNODE_OPCODE_HEADER;

    plan->header = header;
    plan->head = NULL;
    plan->numLists = 0;
    plan->numJoints = 0;
    plan->usedMatrices = 0;
    plan->valid = FALSE;
    for (i = 0; i < MODEL_LOD_MATRIX_LIMIT; i++) plan->byMatrix[i] = -1;
    if (header->numMatrices < 0 || header->numMatrices > MODEL_LOD_MATRIX_LIMIT) return;
    modelLodAddNode(nodes, &count, header->RootNode);

    /* Follow authored branches, including currently hidden LODs and toggles.
     * A head is a separate asset with separate runtime data. */
    for (i = 0; i < count; i++)
    {
        ModelNode *node = nodes[i];
        ModelNode *child = node->Child;
        ModelNode *parent;
        u32 type = node->Opcode & 255;
        s32 matrix = -1;
        if (type == MODELNODE_OPCODE_HEADER || type == MODELNODE_OPCODE_GROUP
                || type == MODELNODE_OPCODE_GROUPSIMPLE)
        {
            if (!body || (node->Opcode & 0x200)) return;
            matrix = modelFindNodeMtxIndex(node, 0);
            if (matrix < 0 || matrix >= header->numMatrices || (assigned & (1U << matrix))) return;
            assigned |= 1U << matrix;
            for (parent = node->Parent; parent; parent = parent->Parent)
            {
                u32 op = parent->Opcode & 255;
                if (op == MODELNODE_OPCODE_LOD || op == MODELNODE_OPCODE_SWITCH) return;
            }
            if (node->Opcode & 0x100)
            {
                ModelLodJoint *joint;
                if (type != MODELNODE_OPCODE_GROUP || plan->numJoints == MODEL_LOD_JOINT_LIMIT) return;
                matrix = node->Data->Group.MatrixID1;
                /* Auto-aim also reads body matrices 0/1 directly. */
                if (matrix < 2 || matrix >= header->numMatrices || (assigned & (1U << matrix))) return;
                assigned |= 1U << matrix;
                plan->byMatrix[matrix] = plan->numJoints;
                joint = &plan->joints[plan->numJoints++];
                joint->node = node;
                joint->count = 0;
            }
        }
        else if (type == MODELNODE_OPCODE_LOD) child = node->Data->LOD.Affects;
        else if (type == MODELNODE_OPCODE_SWITCH) child = node->Data->Switch.Controls;
        else if (type == MODELNODE_OPCODE_BSP)
        {
            child = node->Data->BSP.leftChild;
            if (!modelLodAddNode(nodes, &count, node->Data->BSP.rightChild)) return;
        }
        else if (type == MODELNODE_OPCODE_HEAD)
        {
            if (!body || plan->head) return;
            plan->head = node;
            child = NULL;
        }
        else if (type != MODELNODE_OPCODE_BBOX && type != MODELNODE_OPCODE_SHADOW
                && type != MODELNODE_OPCODE_DL && type != MODELNODE_OPCODE_DLCOLLISION) return;
        if (!modelLodAddNode(nodes, &count, node->Next)
                || !modelLodAddNode(nodes, &count, child)) return;
    }

    for (i = 0; i < count; i++)
    {
        ModelNode *node = nodes[i];
        u32 type = node->Opcode & 255;
        u32 mask = 0;
        if (type == MODELNODE_OPCODE_DLCOLLISION)
        {
            ModelRoData_DisplayList_CollisionRecord *dl = &node->Data->DisplayListCollisions;
            if (plan->numLists == MODEL_LOD_LIST_LIMIT) return;
            plan->lists[plan->numLists++] = dl;
            if (!modelLodListMatrices(dl->Primary, dl->BaseAddr, &mask)
                    || !modelLodListMatrices(dl->Secondary, dl->BaseAddr, &mask)) return;
        }
        else if (type == MODELNODE_OPCODE_DL)
        {
            ModelRoData_DisplayListRecord *dl = &node->Data->DisplayList;
            if (!modelLodListMatrices(dl->Primary, dl->BaseAddr, &mask)
                    || !modelLodListMatrices(dl->Secondary, dl->BaseAddr, &mask)) return;
        }
        plan->usedMatrices |= mask;
        if (body && (mask & ~assigned)) return;
        for (j = 0; j < plan->numJoints; j++)
        {
            if (mask & (1U << plan->joints[j].node->Data->Group.MatrixID1))
                modelLodAddUse(&plan->joints[j], node);
        }
    }
    /* Unreferenced extras are left alone too. Only known LOD consumers qualify. */
    for (i = 0; i < plan->numJoints; i++)
        if (plan->joints[i].count <= 0) plan->byMatrix[plan->joints[i].node->Data->Group.MatrixID1] = -1;
    plan->valid = TRUE;
}

static ModelLodPlan *modelLodGetPlan(ModelFileHeader *header)
{
    s32 i;
    ModelLodPlan *empty = NULL;
    if (!header || !header->RootNode) return NULL;
    for (i = 0; i < MODEL_LOD_CACHE_SIZE; i++)
    {
        ModelLodPlan *plan = &g_ModelLodCache[i];
        if (plan->header == header) return plan->valid ? plan : NULL;
        if (!plan->header && !empty) empty = plan;
    }
    /* A full cache is a performance fallback, never a reason to omit work. */
    if (!empty) return NULL;
    modelLodBuildPlan(empty, header);
    return empty->valid ? empty : NULL;
}

static bool modelLodListsUnchanged(ModelLodPlan *plan, union ModelRwData **datas)
{
    s32 i;
    for (i = 0; i < plan->numLists; i++)
    {
        ModelRoData_DisplayList_CollisionRecord *dl = plan->lists[i];
        ModelRwData_DisplayList_CollisionRecord *rw = (ModelRwData_DisplayList_CollisionRecord *)&datas[dl->RwDataIndex];
        if (rw->gdl != dl->Primary) return FALSE;
    }
    return TRUE;
}

void modelLodBegin(Model *model)
{
    ModelLodPlan *plan;
    modelLodEnd();
    if (!model || !model->obj || model->attachedto || g_ModelDistanceDisabled) return;
    plan = modelLodGetPlan(model->obj);
    if (!plan || !plan->numJoints) return;
    g_ModelLodModel = model;
    g_ModelLodPlan = plan;
    g_ModelLodChecked = FALSE;
    g_ModelLodViewScale = getPlayer_c_lodscalez();
}

static bool modelLodCheckInstance(Model *model)
{
    ModelLodPlan *plan = g_ModelLodPlan;
    ModelLodInstance *entry = &g_ModelLodInstances[((u32)model >> 2) & (MODEL_LOD_INSTANCE_COUNT - 1)];
    ModelRwData_HeadPlaceholderRecord *rw = NULL;
    ModelFileHeader *headHeader = NULL;
    void *headDatas = NULL;
    if (plan->head)
    {
        /* The plan excludes nested head placeholders. This index belongs
         * to the body, regardless of which head last used the shared tree. */
        rw = (ModelRwData_HeadPlaceholderRecord *)&model->datas[plan->head->Data->HeadPlaceholder.RwDataIndex];
        headHeader = rw->ModelFileHeader;
        headDatas = rw->RwDatas;
    }
    if (entry->model == model && entry->plan == plan && entry->datas == model->datas
            && entry->head == headHeader && entry->headDatas == headDatas)
    {
        g_ModelLodHeadMatrices = entry->headMatrices;
        g_ModelLodChecked = TRUE;
        return TRUE;
    }
    if (!modelLodListsUnchanged(plan, model->datas)) return FALSE;
    g_ModelLodHeadMatrices = 0;
    if (headHeader)
    {
        ModelLodPlan *head = modelLodGetPlan(headHeader);
        if (!head || (head->header->RootNode->Opcode & 255) == MODELNODE_OPCODE_HEADER
                || !modelLodListsUnchanged(head, headDatas)) return FALSE;
        g_ModelLodHeadMatrices = head->usedMatrices;
    }
    entry->model = model;
    entry->plan = plan;
    entry->datas = model->datas;
    entry->head = headHeader;
    entry->headDatas = headDatas;
    entry->headMatrices = g_ModelLodHeadMatrices;
    g_ModelLodChecked = TRUE;
    return TRUE;
}

bool modelLodSkipJoint(Model *model, ModelRoData_GroupRecord *group)
{
    ModelLodJoint *joint;
    s32 index;
    s32 i;
    if (model != g_ModelLodModel || !g_ModelLodPlan) return FALSE;
    index = group->MatrixID1;
    if (index < 0 || index >= MODEL_LOD_MATRIX_LIMIT) return FALSE;
    index = g_ModelLodPlan->byMatrix[index];
    if (index < 0) return FALSE;
    joint = &g_ModelLodPlan->joints[index];
    if (&joint->node->Data->Group != group) return FALSE;
    for (i = 0; i < joint->count; i++)
    {
        ModelLodGate *gate = &joint->gates[i];
        f32 distance = 0.0f;
        if (!g_ModelDistanceDisabled)
        {
            distance = -model->render_pos[gate->matrix].pos.m[3][2] * g_ModelLodViewScale;
            if (g_ModelDistanceScale != 1.0f) distance *= g_ModelDistanceScale;
        }
        /* Match modelUpdateDistanceRelations exactly, including zoom, scale,
         * disabled LODs, boundary equality, gaps and overlapping ranges. */
        if ((gate->lod->MinDistance == 0 || distance > gate->lod->MinDistance * model->scale)
                && distance <= gate->lod->MaxDistance * model->scale)
        {
            /* Keep the common near pose cheap: once a near branch is needed,
             * use the ordinary path for the rest of this body's joints. */
            modelLodEnd();
            return FALSE;
        }
    }
    /* Most near poses never need this instance/head validation. */
    if (!g_ModelLodChecked && !modelLodCheckInstance(model))
    {
        modelLodEnd();
        return FALSE;
    }
    if (g_ModelLodHeadMatrices & (1U << group->MatrixID1)) return FALSE;
    /* Later code converts the whole array to fixed point. Keep unused slots
     * initialized without computing an unreferenced smoothing transform. */
    matrix_4x4_set_identity(&model->render_pos[group->MatrixID1].pos);
    return TRUE;
}
