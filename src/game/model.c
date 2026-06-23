#include <ultra64.h>
#include <memp.h>
#include "model.h"
#include "../rmon.h" /*<PR/rmon.h>*/
#include "bondview.h"
#include "chr.h"
#include "chrobjdata.h"
#include "gbi_extension.h"
#include "initunk_005520.h"
#include "math_asinfacosf.h"
#include "math_floor.h"
#include "math_ceil.h"
#include "math_unk_05A9E0.h"
#include "objecthandler.h"
#include "quaternion.h"
#include "random.h"


extern void (*D_80036090)(s32, s32, s32);

typedef void (*ModelMatrixCallback2)(s32 matrixId, Mtxf *mtx);

typedef struct ModelGroupMtxBuildArg {
    u16 flags;
    u16 pad;
    ModelRoData_GroupRecord *group;
    ModelNode *parentnode;
} ModelGroupMtxBuildArg;

// forward declarations
void modelSetAnimFrame2WithChrStuff(struct Model *model, f32 framea, f32 frameb, f32 frame2a, f32 frame2b);



//newfile per EU
bool modelmgrCanSlotFitRwdata(Model *modelslot, ModelFileHeader *modeldef)
{
    return modeldef->numRecords <= 0
        || (modelslot->datas != NULL && modelslot->rwdatalen >= modeldef->numRecords);
}


/**
 * Address: 7F06C094
 * 
 * Allocates 0x20 bytes for a new model without animations.
 * Models that need animations use modelmgrInstantiateModelWithAnim.
 */
Model *modelmgrInstantiateModel(ModelFileHeader *header)
{
    Model *model;
    u32 *rwdata;
    s16 rwdatalen;

    model = NULL;
    rwdata = NULL;
    rwdatalen = -1;

    if (g_ModelIsLvResetting) 
    {
        s32 i;

        for (i = 0; i < (g_MaxModelSlots - 30); i++) 
        {
            if (g_ModelSlots[i].unk08 == 0) 
            {
                model = (Model *)&g_ModelSlots[i];
                break;
            }
        }

        if (model == NULL) 
        {
            model = mempAllocBytesInBank(0x20, MEMPOOL_STAGE);
        }

        if (header->numRecords > 0) 
        {
            rwdata = mempAllocBytesInBank((((header->numRecords * 4) + 0xf) | 0xf) ^ 0xf, MEMPOOL_STAGE);
            rwdatalen = header->numRecords;
        }
    } 
    else 
    {
        s32 i;

        for (i = 0; i < g_MaxModelSlots; i++) 
        {
            if (g_ModelSlots[i].unk08 == 0 && modelmgrCanSlotFitRwdata((Model *)&g_ModelSlots[i], header)) 
            {
                rwdata = g_ModelSlots[i].unk10;
                rwdatalen = g_ModelSlots[i].unk02;
                model = (Model *)&g_ModelSlots[i];
                break;
            }
        }
    }

    if (model != NULL) 
    {
        modelInit(model, header, rwdata);
        ((struct ModelSlot *)model)->unk02 = rwdatalen;
    }

    return model;
}


void clear_model_obj(Model* model)
{
    model->obj = NULL;
}


/**
 * Allocates 0xc0 bytes for a new model to allow enough memory for animations.
 */
Model *modelmgrInstantiateModelWithAnim(ModelFileHeader *modelFileHeader)
{
    Model *newModel;
    void *rwdatas;
    s16 rwdatalen;
    s32 i;
    s16 requiredRwdatalen;
    s32 i2;

    newModel = NULL;
    rwdatas = NULL;
    rwdatalen = -1;

    if (g_ModelIsLvResetting) 
    {
        for (i = 0; i < (g_MaxAnimModelSlots - 10); i++) 
        {
            if (g_AnimModelSlots[i].unk08 == 0)
            {
                newModel = (Model *)&g_AnimModelSlots[i];
                break;
            }
        }

        if (newModel == NULL) 
        {
            newModel = mempAllocBytesInBank(0xc0, MEMPOOL_STAGE);
        }

        requiredRwdatalen = modelFileHeader->numRecords;

#ifdef DEBUG
        if (modelFileHeader->numRecords > 140) osSyncPrintf("WARNING: increase OISAVESIZE to %d!\n", *(modelFileHeader->numRecords));
#endif

        if (requiredRwdatalen > 0) 
        {
            i = requiredRwdatalen;
            rwdatas = mempAllocBytesInBank((((i * 4) + 0xf) | 0xf) ^ 0xf, MEMPOOL_STAGE);
            rwdatalen = modelFileHeader->numRecords;
        }
    } 
    else 
    {
        requiredRwdatalen = modelFileHeader->numRecords;

        for (i2 = 0; i2 < g_MaxAnimModelSlots; i2++) 
        {
            if ((g_AnimModelSlots[i2].unk08 == 0) && ((requiredRwdatalen <= 0) || ((g_AnimModelSlots[i2].unk10 != NULL) &&(g_AnimModelSlots[i2].unk02 >= requiredRwdatalen)))) 
            {
                newModel = (Model *)&g_AnimModelSlots[i2];
                rwdatas = g_AnimModelSlots[i2].unk10;
                rwdatalen = g_AnimModelSlots[i2].unk02;
                break;
            }
        }
    }

    if (newModel != NULL) 
    {
        animInit(newModel, modelFileHeader, rwdatas);
        newModel->rwdatalen = rwdatalen;
    }

    return newModel;
}


void modelAttachHead(Model *model, ModelNode *node,  ModelFileHeader *head)
{
    modelAttachPart(model,model->obj,node,head);
#ifdef DEBUG
    if (model->numRecords > 140 && g_ModelDistanceScale == 0) osSyncPrintf("WARNING: increase OASAVESIZE to %d!\n", *(model + 0x14));
#endif

    modelInitRwData(model,head->RootNode);
}


void clear_aircraft_model_obj(Model *objinstance)
{
    objinstance->obj = NULL;
    return;
}


void modelSetDistanceDisabled(s32 param_1) {
  g_ModelDistanceDisabled = param_1;
}


// PD: modelSetDistanceScale
void modelSetDistanceScale(f32 param_1) {
  g_ModelDistanceScale = param_1;
}


void sub_GAME_7F06C418(Vew4s32 *src, Vew4s32 *dst) {
    s32 i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            dst[i].v[j] = src[i].v[j];
        }
    }
}


void set_vtxallocator(s32 param_1) {
  vtxallocator = param_1;
}


#if defined(LEFTOVERDEBUG)
// called after a debug print during failed model operation possible "exit()" function in debug
void return_null(void)
{
    // dump something 8 bytes long?

    return;
}
#endif


/**
 * Address: 7F06C474
 */
void modelCalculateScaledRootToOriginDir(Model* model, coord3d* coord)
{
    Mtxf* mtx;
    f32 dist;
    f32 neg_x;
    f32 neg_y;
    f32 neg_z;
    f32 inv_dist;

    mtx = getsubmatrix(model);
    neg_x = -mtx->m[3][0];
    neg_y = -mtx->m[3][1];
    neg_z = -mtx->m[3][2];

    dist = sqrtf((neg_x * neg_x) + (neg_y * neg_y) + (neg_z * neg_z));
    if (dist > 0.0f)
    {
        inv_dist = 1.0f / (model->scale * dist);
        coord->f[0] = neg_x * inv_dist;
        coord->f[1] = neg_y * inv_dist;
        coord->f[2] = neg_z * inv_dist;
        return;
    }

    coord->f[0] = 0.0f;
    coord->f[1] = 0.0f;
    coord->f[2] = 1.0f / model->scale;
}


/**
 * Address: 7F06C550
 */
void modelGetScaledRootToOriginDir(Model* model, coord3d* coord)
{
  modelCalculateScaledRootToOriginDir(model, coord);
}


// PD: model0001a524
s32 modelFindNodeMtxIndex(ModelNode *node, s32 arg1)
{
    s32 index;
    union ModelRoData *rodata1;
    union ModelRoData *rodata2;
    union ModelRoData *rodata3;
    union ModelRoData *rodata4;

    while (node)
    {
        switch (node->Opcode & 0xff)
        {
            case MODELNODE_OPCODE_HEADER:
                rodata1 = node->Data;
                return (s16)rodata1->Header.ModelType;

            case MODELNODE_OPCODE_GROUP:
                rodata2 = node->Data;
                return rodata2->Group.MatrixIDs[arg1 == 0x200 ? 2 : (arg1 == 0x100 ? 1 : 0)];

            case MODELNODE_OPCODE_OP03:
                rodata3 = node->Data;
                return rodata3->Group.MatrixIDs[arg1 == 0x200 ? 2 : (arg1 == 0x100 ? 1 : 0)];

            case MODELNODE_OPCODE_GROUPSIMPLE:
                rodata4 = node->Data;
                return rodata4->GroupSimple.Group1;
                break;
        }

        node = node->Parent;
    }

    return -1;
}


// PD: model0001a5cc
Mtxf *modelFindNodeMtx(struct Model *model, struct ModelNode *node, s32 arg2) {
    s32 index = modelFindNodeMtxIndex(node, arg2);

    if (index >= 0) {
        return &model->render_pos[index].pos;
    }

    return NULL;
}


//rejoined per EU
// PD: model0001a60c
Mtxf *getsubmatrix(Model *objinst)
{
    #if defined(LEFTOVERDEBUG)
    if (!objinst)
    {
        osSyncPrintf("getsubmatrix: no objinst!\n");
        return_null();
    }
    if (!objinst->obj)
    {
        osSyncPrintf("getsubmatrix: objinst has no object!\n");
        return_null();
    }
    #endif
    return modelFindNodeMtx(objinst, objinst->obj->RootNode, 0);
}


// unreferenced
void sub_GAME_7F06C710(Model* model, coord3d* pos)
{
    Mtxf* mtx;

    mtx = getsubmatrix(model);
    if (mtx != NULL)
    {
        pos->f[0] = (f32) mtx->m[3][0];
        pos->f[1] = (f32) mtx->m[3][1];
        pos->f[2] = (f32) mtx->m[3][2];
        return;
    }

    pos->f[0] = 0.0f;
    pos->f[1] = 0.0f;
    pos->f[2] = 0.0f;
}


f32 sub_GAME_7F06C768(Model *objinst)
{
    Mtxf *mtx = getsubmatrix(objinst);
    if (mtx != 0)
    {
        return -mtx->m[3][2];
    }
    return 0.0f;
}


/**
 * Address 0x7F06C79C.
*/
union ModelRwData* modelGetNodeRwData(Model *Objinst, ModelNode *root)
{
    s32 index  = 0;
    union ModelRwData **data = Objinst->datas;

    switch (root->Opcode & 0xff)
    {
        case MODELNODE_OPCODE_HEADER:
        {
            index = root->Data->Header.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_DLCOLLISION:
        {
            index = root->Data->DisplayListCollisions.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_OP07:
        {
            index = root->Data->Op07.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_LOD:
        {
            index = root->Data->LOD.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_SWITCH:
        {
            index = root->Data->Switch.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_BSP:
        {
            index = root->Data->BSP.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_OP11:
        {
            index = root->Data->Op11.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_GUNFIRE:
        {
            index = root->Data->Gunfire.RwDataIndex;
            break;
        }
        case MODELNODE_OPCODE_HEAD:
        {
            index = root->Data->HeadPlaceholder.RwDataIndex;
            break;
        }
    }

    while (root->Parent)
    {
        root = root->Parent;
        if ((root->Opcode & 0xFF) == MODELNODE_OPCODE_HEAD)
        {
            ModelRwData_HeadPlaceholderRecord *tmp = modelGetNodeRwData(Objinst, root);
            data = tmp->RwDatas;
            break;
        }
    }

    return &data[index];
}



void getpartoffset(Model *objinst, ModelNode *part, coord3d *offset) //#MATCH - however OPCODE 3 needs defining
{
    #if defined(LEFTOVERDEBUG)
    if (!objinst)
    {
        osSyncPrintf("getpartoffset: no objinst!");
        return_null();
    }
    if (!part)
    {
        osSyncPrintf("getpartoffset: no partdesc!");
        return_null();
    }
    #endif
    switch (part->Opcode & 0xFF)
    {
        case MODELNODE_OPCODE_HEADER:
        {
            struct modeldata_root *root = modelGetNodeRwData(objinst, part);
            offset->x                   = root->pos.x;
            offset->y                   = root->pos.y;
            offset->z                   = root->pos.z;
            break;
        }
        case MODELNODE_OPCODE_GROUP:
        {
            ModelRoData_GroupRecord *prt = &part->Data->Group;
            offset->x                  = prt->Origin.x;
            offset->y                  = prt->Origin.y;
            offset->z                  = prt->Origin.z;
            break;
        }
        case MODELNODE_OPCODE_OP03:
        {
            ModelRoData_GroupSimpleRecord *prt = &part->Data->GroupSimple; //UNUSED at this time
            offset->x                        = prt->Origin.x;
            offset->y                        = prt->Origin.y;
            offset->z                        = prt->Origin.z;
            break;
        }
        case MODELNODE_OPCODE_GROUPSIMPLE:
        {
            ModelRoData_GroupSimpleRecord *prt = &part->Data->GroupSimple;
            offset->x                        = prt->Origin.x;
            offset->y                        = prt->Origin.y;
            offset->z                        = prt->Origin.z;
            break;
        }
        default:
        {
            offset->x = 0.0f;
            offset->y = 0.0f;
            offset->z = 0.0f;
            break;
        }
    }
}


void setpartoffset(Model *model, ModelNode *node, coord3d *pos)
{
#if defined(LEFTOVERDEBUG)
    if (!model) {
        osSyncPrintf("setpartoffset: no objinst!");
        return_null();
    }

    if (!node) {
        osSyncPrintf("setpartoffset: no partdesc!");
        return_null();
    }
    else
    {
        // huh?
    }
#endif
    switch (node->Opcode & 0xff)
    {
        case MODELNODE_OPCODE_HEADER:
            {
                ModelRwData_HeaderRecord *rwdata = modelGetNodeRwData(model, node);
                coord3d diff[1];

                diff[0].x = pos->x - rwdata->pos.x;
                diff[0].z = pos->z - rwdata->pos.z;

                rwdata->pos.x = pos->x;
                rwdata->pos.y = pos->y;
                rwdata->pos.z = pos->z;

                rwdata->unk24.x += diff[0].x; rwdata->unk24.z += diff[0].z;
                rwdata->unk34.x += diff[0].x; rwdata->unk34.z += diff[0].z;
                rwdata->unk40.x += diff[0].x; rwdata->unk40.z += diff[0].z;
                rwdata->unk4c.x += diff[0].x; rwdata->unk4c.z += diff[0].z;
            }
            break;
        case MODELNODE_OPCODE_GROUP:
            {
                ModelRoData_GroupRecord *rodata = &node->Data->Group;
                rodata->Origin.x = pos->x;
                rodata->Origin.y = pos->y;
                rodata->Origin.z = pos->z;
            }
            break;
        case MODELNODE_OPCODE_OP03:
            {
                ModelRoData_GroupRecord *rodata = &node->Data->Group;
                rodata->Origin.x = pos->x;
                rodata->Origin.y = pos->y;
                rodata->Origin.z = pos->z;
            }
            break;
        case MODELNODE_OPCODE_GROUPSIMPLE:
            {
                ModelRoData_GroupSimpleRecord *rodata = &node->Data->GroupSimple;
                rodata->Origin.x = pos->x;
                rodata->Origin.y = pos->y;
                rodata->Origin.z = pos->z;
            }
            break;
    }
}


void getsuboffset(Model *objinst, coord3d *offset) //#MATCH
{
    #if defined(LEFTOVERDEBUG )
    if (!objinst)
    {
        osSyncPrintf("getsuboffset: no objinst!");
        return_null();
    }

    if (!objinst->obj)
    {
        osSyncPrintf("getsuboffset: objinst has no object!");
        return_null();
    }
    #endif
    getpartoffset(objinst, objinst->obj->RootNode, offset);
}




void setsuboffset(Model *objinst, coord3d *offset) //#MATCH
{
    #if defined(LEFTOVERDEBUG )
    if (!objinst)
    {
        osSyncPrintf("setsuboffset: no objinst!");
        return_null();
    }
    if (!objinst->obj)
    {
        osSyncPrintf("setsuboffset: objinst has no object!");
        return_null();
    }
    #endif
    setpartoffset(objinst, objinst->obj->RootNode, offset);
}





/**
 * Address 0x7F06CC80.
 */
f32 getsubroty(Model *objinst)
{
    ModelNode *root;

    #if defined(LEFTOVERDEBUG)
    if(0)
    {
        // removed
    }

    if (objinst == NULL)
    {
        osSyncPrintf("getsubroty: no objinst!");
        return_null();
    }

    if(0)
    {
        // removed
    }

    if (objinst->obj == NULL)
    {
        osSyncPrintf("getsubroty: objinst has no object!");
        return_null();
    }

    if(0)
    {
        // removed
    }

    if (objinst->obj->RootNode == NULL)
    {
        osSyncPrintf("getsubroty: objinst has no root part!");
        return_null();
    }

    if(0)
    {
        // removed
    }
    #endif

    root = objinst->obj->RootNode;
    if ((root->Opcode & 0xFF) == MODELNODE_OPCODE_HEADER)
    {
        return ((struct modeldata_root *)modelGetNodeRwData(objinst, root))->subroty;
    }

    return 0.0f;
}


void setsubroty(Model *model, f32 angle)
{
    ModelNode* node;
#if defined(LEFTOVERDEBUG)
    if (!model)
    {
        osSyncPrintf("setsubroty: no objinst!");
        return_null();
    }

    if (!model->obj) //< needs to be v1 not a1
    {
        osSyncPrintf("setsubroty: objinst has no object!");
        return_null();
    }

    if (!model->obj->RootNode)
    {
        osSyncPrintf("setsubroty: objinst has no root part!");
        return_null();
    }
#endif
    node = model->obj->RootNode;
    if ((node->Opcode & 0xff) == MODELNODE_OPCODE_HEADER)
    {
        ModelRwData_HeaderRecord *rwdata = modelGetNodeRwData(model, node);
        f32 diff = angle - rwdata->unk14;

        if (diff < 0) { diff += M_TAU_F; }

        rwdata->unk30 += diff;

        if (rwdata->unk30 >= M_TAU_F) { rwdata->unk30 -= M_TAU_F; }

        rwdata->unk20 += diff;

        if (rwdata->unk20 >= M_TAU_F) { rwdata->unk20 -= M_TAU_F; }

        rwdata->unk14 = angle;
    }
}


void modelSetScale(Model *objinst, f32 scale)
{
    objinst->scale = scale;
}


/**
 * Address: 7F06CE84
 * 
 * Scales only the translation component of the root node of an animation.
 * For example, the animation for the plane flight in Runway's outro doesn't
 * actually move the plane very far. This function is used to scale up the translation
 * ~10x to allow it to fly near the camera.
 */
void modelSetAnimTranslationScale(Model* model, f32 scale)
{
    model->anim_translation_scale = scale;
}


f32 getjointsize(Model *model, ModelNode *node)
{
    Model     *temp_a2;
    ModelNode *temp_a1;
    s32        temp_t7;

#if defined(LEFTOVERDEBUG)
    if (!model)
    {
        osSyncPrintf("getjointsize: no objinst!\n");
        return_null();
    }
#endif

    if (node)
    {
        do
        {
            switch (node->Opcode & 0xFF)
            {
                case MODELNODE_OPCODE_HEADER:
                {
                    ModelRoData_HeaderRecord *rodata = &node->Data->Header;
                    return rodata->GroupsAsF32 * model->scale;
                }
                case MODELNODE_OPCODE_GROUP:
                {
                    ModelRoData_GroupRecord *rodata = &node->Data->Group;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_OP03:
                {
                    ModelRoData_GroupRecord *rodata = &node->Data->Group;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_GROUPSIMPLE:
                {
                    ModelRoData_GroupSimpleRecord *rodata = &node->Data->GroupSimple;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_OP11:
                {
                    ModelRoData_Op11Record *rodata = &node->Data->Op11;
                    return rodata->BoundingVolumeRadius * model->scale;
                }
                case MODELNODE_OPCODE_GUNFIRE:
                {
                    ModelRoData_GunfireRecord *rodata = &node->Data->Gunfire;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_SHADOW:
                {
                    ModelRoData_ShadowRecord *rodata = &node->Data->Shadow;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_OP14:
                {
                    ModelRoData_Op14Record *rodata = &node->Data->Op14;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_INTERLINK:
                {
                    ModelRoData_InterlinkageRecord *rodata = &node->Data->Interlinkage;
                    return rodata->Scale * model->scale;
                }
                case MODELNODE_OPCODE_OP16:
                {
                    ModelNode_Op16Record *rodata = &node->Data->Op16;
                    return rodata->Scale * model->scale;
                }
                default:
                    node = node->Parent;
            }
        } while (node);
    }

    return 0.0f;
}


/**
 * Address 0x7F06D00C.
 * PD: model0001af80
*/
f32 getinstsize(Model *arg0)
{
    #if defined(LEFTOVERDEBUG)
    if (arg0 == NULL)
    {
        osSyncPrintf("getinstsize: no objinst!\n");
        return_null();
    }

    if (arg0->obj == NULL)
    {
        osSyncPrintf("getinstsize: no objdesc!\n");
        return_null();
    }
    #endif

    return arg0->obj->BoundingVolumeRadius * arg0->scale;
}



// PD: model0001af98
void interpolate3dVectors(vec3d *v, vec3d *w, float frac)
{
    v->x += (w->x - v->x) * frac;
    v->y += (w->y - v->y) * frac;
    v->z += (w->z - v->z) * frac;
  return;
}


// PD: model0001afe8
f32 sub_GAME_7F06D0CC(f32 arg0, f32 angle, f32 mult)
{
    f32 value = angle - arg0;

    if (angle < arg0)
    {
        value += M_TAU_F;
    }

    if (value < M_PI_F)
    {
        arg0 += value * mult;

        if (arg0 >= M_TAU_F)
        {
            arg0 -= M_TAU_F;
        }
    }
    else
    {
        arg0 -= (M_TAU_F - value) * mult;

        if (arg0 < 0)
        {
            arg0 += M_TAU_F;
        }
    }

    return arg0;
}


// PD: model0001b07c
void sub_GAME_7F06D160(coord3d *arg0, coord3d *arg1, f32 mult)
{
    arg0->x = sub_GAME_7F06D0CC(arg0->x, arg1->x, mult);
    arg0->y = sub_GAME_7F06D0CC(arg0->y, arg1->y, mult);
    arg0->z = sub_GAME_7F06D0CC(arg0->z, arg1->z, mult);
}


#ifdef NONMATCHING
void sub_GAME_7F06D1CC(int param_1, int param_2, int param_3)

{
    byte   bVar1;
    uint   uVar2;
    dword  local_70;
    byte   local_20;
    dword  local_1c;
    byte  *local_18;
    dword  local_14;
    ushort local_10;
    byte   local_e;
    dword  local_8;

    local_10 = 0;
    local_20 = *(*(param_1 + 8) + param_2 * 6 + 2);
    if (local_20 != 0)
    {
        uVar2    = param_3 + *(*(param_1 + 8) + param_2 * 6);
        local_18 = *(param_1 + 0x10) + (uVar2 >> 3);
        local_e  = 8 - (uVar2 & 7);
        while (local_e <= local_20)
        {
            local_20 = local_20 - local_e;
            local_10 = local_10 | (*local_18 & (1 << (local_e & 0x3f)) - 1U) << (local_20 & 0x3f);
            local_18 = local_18 + 1;
            local_e  = 8;
        }
        if (local_20 != 0)
        {
            local_10 = local_10 | *local_18 >> (local_e - local_20 & 0x3f) & (1 << (local_20 & 0x3f)) - 1U;
        }
        bVar1 = *(*(param_1 + 8) + param_2 * 6 + 2);
        if ((bVar1 < 0x10) && ((local_10 & 1 << (bVar1 - 1 & 0x3f)) != 0))
        {
            local_10 = local_10 | (1 << (0x10 - bVar1 & 0x3f)) + -1 << (bVar1 & 0x3f);
        }
    }
    return local_10 + *(*(param_1 + 8) + param_2 * 6 + 4);
}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F06D1CC
/* 0A1CFC 7F06D1CC 8C8E0008 */  lw    $t6, 8($a0)
/* 0A1D00 7F06D1D0 00057880 */  sll   $t7, $a1, 2
/* 0A1D04 7F06D1D4 01E57823 */  subu  $t7, $t7, $a1
/* 0A1D08 7F06D1D8 000F7840 */  sll   $t7, $t7, 1
/* 0A1D0C 7F06D1DC 01CF3821 */  addu  $a3, $t6, $t7
/* 0A1D10 7F06D1E0 90E80002 */  lbu   $t0, 2($a3)
/* 0A1D14 7F06D1E4 00001825 */  move  $v1, $zero
/* 0A1D18 7F06D1E8 01004825 */  move  $t1, $t0
/* 0A1D1C 7F06D1EC 5920003A */  blezl $t1, .L7F06D2D8
/* 0A1D20 7F06D1F0 94F80004 */   lhu   $t8, 4($a3)
/* 0A1D24 7F06D1F4 94F80000 */  lhu   $t8, ($a3)
/* 0A1D28 7F06D1F8 240E0008 */  li    $t6, 8
/* 0A1D2C 7F06D1FC 8C8D0010 */  lw    $t5, 0x10($a0)
/* 0A1D30 7F06D200 03061021 */  addu  $v0, $t8, $a2
/* 0A1D34 7F06D204 30590007 */  andi  $t9, $v0, 7
/* 0A1D38 7F06D208 01D96023 */  subu  $t4, $t6, $t9
/* 0A1D3C 7F06D20C 318F00FF */  andi  $t7, $t4, 0xff
/* 0A1D40 7F06D210 000228C2 */  srl   $a1, $v0, 3
/* 0A1D44 7F06D214 012F082A */  slt   $at, $t1, $t7
/* 0A1D48 7F06D218 318B00FF */  andi  $t3, $t4, 0xff
/* 0A1D4C 7F06D21C 14200010 */  bnez  $at, .L7F06D260
/* 0A1D50 7F06D220 01A55021 */   addu  $t2, $t5, $a1
.L7F06D224:
/* 0A1D54 7F06D224 91590000 */  lbu   $t9, ($t2)
/* 0A1D58 7F06D228 240D0001 */  li    $t5, 1
/* 0A1D5C 7F06D22C 016D7004 */  sllv  $t6, $t5, $t3
/* 0A1D60 7F06D230 012B1023 */  subu  $v0, $t1, $t3
/* 0A1D64 7F06D234 25CFFFFF */  addiu $t7, $t6, -1
/* 0A1D68 7F06D238 304900FF */  andi  $t1, $v0, 0xff
/* 0A1D6C 7F06D23C 032FC024 */  and   $t8, $t9, $t7
/* 0A1D70 7F06D240 01386804 */  sllv  $t5, $t8, $t1
/* 0A1D74 7F06D244 006D1825 */  or    $v1, $v1, $t5
/* 0A1D78 7F06D248 306EFFFF */  andi  $t6, $v1, 0xffff
/* 0A1D7C 7F06D24C 29210008 */  slti  $at, $t1, 8
/* 0A1D80 7F06D250 01C01825 */  move  $v1, $t6
/* 0A1D84 7F06D254 254A0001 */  addiu $t2, $t2, 1
/* 0A1D88 7F06D258 1020FFF2 */  beqz  $at, .L7F06D224
/* 0A1D8C 7F06D25C 240B0008 */   li    $t3, 8
.L7F06D260:
/* 0A1D90 7F06D260 1920000A */  blez  $t1, .L7F06D28C
/* 0A1D94 7F06D264 01697823 */   subu  $t7, $t3, $t1
/* 0A1D98 7F06D268 91590000 */  lbu   $t9, ($t2)
/* 0A1D9C 7F06D26C 240D0001 */  li    $t5, 1
/* 0A1DA0 7F06D270 012D7004 */  sllv  $t6, $t5, $t1
/* 0A1DA4 7F06D274 01F9C007 */  srav  $t8, $t9, $t7
/* 0A1DA8 7F06D278 25D9FFFF */  addiu $t9, $t6, -1
/* 0A1DAC 7F06D27C 03197824 */  and   $t7, $t8, $t9
/* 0A1DB0 7F06D280 006F1825 */  or    $v1, $v1, $t7
/* 0A1DB4 7F06D284 306DFFFF */  andi  $t5, $v1, 0xffff
/* 0A1DB8 7F06D288 01A01825 */  move  $v1, $t5
.L7F06D28C:
/* 0A1DBC 7F06D28C 310900FF */  andi  $t1, $t0, 0xff
/* 0A1DC0 7F06D290 29210010 */  slti  $at, $t1, 0x10
/* 0A1DC4 7F06D294 1020000F */  beqz  $at, .L7F06D2D4
/* 0A1DC8 7F06D298 252E001F */   addiu $t6, $t1, 0x1f
/* 0A1DCC 7F06D29C 24180001 */  li    $t8, 1
/* 0A1DD0 7F06D2A0 01D8C804 */  sllv  $t9, $t8, $t6
/* 0A1DD4 7F06D2A4 00797824 */  and   $t7, $v1, $t9
/* 0A1DD8 7F06D2A8 11E0000A */  beqz  $t7, .L7F06D2D4
/* 0A1DDC 7F06D2AC 00601025 */   move  $v0, $v1
/* 0A1DE0 7F06D2B0 240D0010 */  li    $t5, 16
/* 0A1DE4 7F06D2B4 01A9C023 */  subu  $t8, $t5, $t1
/* 0A1DE8 7F06D2B8 240E0001 */  li    $t6, 1
/* 0A1DEC 7F06D2BC 030EC804 */  sllv  $t9, $t6, $t8
/* 0A1DF0 7F06D2C0 272FFFFF */  addiu $t7, $t9, -1
/* 0A1DF4 7F06D2C4 012F6804 */  sllv  $t5, $t7, $t1
/* 0A1DF8 7F06D2C8 004D1825 */  or    $v1, $v0, $t5
/* 0A1DFC 7F06D2CC 306EFFFF */  andi  $t6, $v1, 0xffff
/* 0A1E00 7F06D2D0 01C01825 */  move  $v1, $t6
.L7F06D2D4:
/* 0A1E04 7F06D2D4 94F80004 */  lhu   $t8, 4($a3)
.L7F06D2D8:
/* 0A1E08 7F06D2D8 00781821 */  addu  $v1, $v1, $t8
/* 0A1E0C 7F06D2DC 03E00008 */  jr    $ra
/* 0A1E10 7F06D2E0 3062FFFF */   andi  $v0, $v1, 0xffff
)
#endif


/**
 * Address: 7F06D2E4
 */
u16 sub_GAME_7F06D2E4(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, s32 frame, coord16 *out)
{
    u32 scaled;
    s32 base;
    u32 angle_raw;
    u16 angle_ret;
    
    scaled = ((u32) anim->unk0C) * ((u32) frame);
    
    if (flip)
    {
        base = skeleton->Joints[jointnum].mtxB;
    }
    else
    {
        base = skeleton->Joints[jointnum].mtxA;
    }
    
    out->x = sub_GAME_7F06D1CC(anim, base, scaled);
    out->y = sub_GAME_7F06D1CC(anim, base + 1, scaled);
    out->z = sub_GAME_7F06D1CC(anim, base + 2, scaled);
    angle_raw = sub_GAME_7F06D1CC(anim, base + 3, scaled);
    angle_ret = angle_raw;
    
    if (flip)
    {
        out->x = -out->x;
        
        if (angle_raw != 0)
        {
            angle_ret = 0x10000 - (angle_raw & 0xFFFFFFFFu);
        }
    }
    
    return angle_ret;
}


f32 sub_GAME_7F06D3F4(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, s32 frame, coord3d *pos)
{
    s16 tmp[3];
    u16 angle;

    angle = sub_GAME_7F06D2E4(jointnum, flip, skeleton, anim, frame, tmp);

    pos->x = (f32)tmp[0];
    pos->y = (f32)tmp[1];
    pos->z = (f32)tmp[2];

    return ((f32)angle * M_TAU_F) / M_U16_MAX_VALUE_F;
}


/**
 * Address: 7F06D490
 */
void sub_GAME_7F06D490(Model *model, ModelNode *modelNode)
{
    union ModelRwData *rw;
    coord3d sp38;
    coord3d sp2c;
    f32 y;

    rw = modelGetNodeRwData(model, modelNode);

    if (rw->Header.unk00 != 0)
    {
        return;
    }

    sp38.x = rw->Header.unk34.x;
    sp38.y = rw->Header.unk34.y;
    sp38.z = rw->Header.unk34.z;
    rw->Header.unk14 = rw->Header.unk30;

    if (model->unk2c != 0.0f)
    {
        if (rw->Header.unk01 != 0)
        {
            interpolate3dVectors(&sp38, &rw->Header.unk24, model->unk2c);

            // Weird do while loop but needed for matching.
            do
            {
                rw->Header.unk14 = sub_GAME_7F06D0CC(rw->Header.unk30, rw->Header.unk20, model->unk2c);
            }
            while (model->unka0 * 0);
        }
    }

    if ((model->anim2 != 0) || (model->unk84 != 0.0f))
    {
        if (rw->Header.unk02 != 0)
        {
            y = rw->Header.unk4c.y;

            if (model->unk5c != 0.0f)
            {
                y += (rw->Header.unk40.y - y) * model->unk5c;
            }

            sp38.y += (y - sp38.y) * model->unk84;
        }
    }

    sp2c.x = sp38.x;
    sp2c.y = sp38.y;
    sp2c.z = sp38.z;

    if (model->unka0 && !((s32 (*)(Model *, coord3d *, coord3d *, f32 *)) model->unka0)(model, &rw->Header.pos, &sp2c, &rw->Header.ground))
    {
        return;
    }

    sp38.x = sp2c.x - sp38.x;
    sp38.z = sp2c.z - sp38.z;

    rw->Header.pos.x = sp2c.x;
    rw->Header.pos.y = ((f32) sp2c.y) + rw->Header.ground;
    rw->Header.pos.z = sp2c.z;

    rw->Header.unk34.x += sp38.x;
    rw->Header.unk34.z += sp38.z;

    if (rw->Header.unk01 != 0)
    {
        rw->Header.unk24.x += sp38.x;
        rw->Header.unk24.z += sp38.z;
    }

    if (rw->Header.unk02 != 0)
    {
        rw->Header.unk4c.x += sp38.x;
        rw->Header.unk4c.z += sp38.z;
        rw->Header.unk40.x += sp38.x;
        rw->Header.unk40.z += sp38.z;
    }
}


void subcalcpos(Model *arg0)
{
    struct ModelNode *root;

#if defined(LEFTOVERDEBUG)
    if (arg0 == NULL)
    {
        osSyncPrintf("subcalcpos: no objanim!\n");
        return_null();
    }

    if (arg0->obj == 0)
    {
        osSyncPrintf("subcalcpos: no objdesc!\n");
        return_null();
    }
#endif

    root = arg0->obj->RootNode;
    if ((root != NULL) && ((root->Opcode & 0xFF) == 1))
    {
        sub_GAME_7F06D490(arg0, root);
    }
}

void process_01_group_heading(ModelRenderData* renderdata, Model* model, ModelNode* node)
{
    union ModelRoData* rodata;
    union ModelRwData* rwdata;
    f32 scale;
    f32* pos;
    f32 unk14;
    Mtxf* var_a3;
    s32 modeltype;
    RenderPosView* renderpos;
    Mtxf sp20;

    rodata = node->Data;
    rwdata = modelGetNodeRwData(model, node);

    scale = model->scale;
    pos = &rwdata->Header.pos.x;
    unk14 = rwdata->Header.unk14;
    modeltype = (s16)rodata->Header.ModelType;
    renderpos = &model->render_pos[modeltype];

    if (node->Parent != NULL)
    {
        var_a3 = modelFindNodeMtx(model, node->Parent, 0);
    }
    else
    {
        var_a3 = renderdata->basemtx;
    }

    if (rwdata->Header.unk18 != 0.0f)
    {
        unk14 = sub_GAME_7F06D0CC(unk14, rwdata->Header.unk1c, rwdata->Header.unk18);
    }

    if (var_a3 != NULL)
    {
        matrix_4x4_set_position_and_rotation_around_y(pos, unk14, &sp20);

        if (scale != 1.0f)
        {
            matrix_scalar_multiply_2(scale, sp20.m[0]);
        }

        matrix_4x4_multiply_homogeneous(var_a3, &sp20, &renderpos->pos);
        return;
    }

    matrix_4x4_set_position_and_rotation_around_y(pos, unk14, &renderpos->pos);

    if (scale != 1.0f)
    {
        matrix_scalar_multiply_2(scale, renderpos->pos.m[0]);
    }
}


/**
 * Address: 7F06D8B0
 */
void modelBuildGroupMatrices(Mtxf **parentMtx, Model *model, ModelGroupMtxBuildArg *mgm, coord3d *rot)
{
    u32 flags;
    ModelRoData_GroupRecord *group;
    Mtxf *parent;
    Mtxf *matrix0_mtx;
    Mtxf tmp;
    s32 matrix0;
    s32 matrix1;
    s32 matrix2;
    RenderPosView *render_pos;
    Mtxf *parentNodeMtx;
    f32 *origin;
    s32 has_matrix2;
    quatf q;
    quatf q2;
    Mtxf *dst;
    f32 angle;
    Mtxf **parentMtxPtr;
    
    flags = mgm->flags;
    group = mgm->group;
    matrix0 = group->MatrixID0;
    matrix1 = group->MatrixID1;
    parentMtxPtr = parentMtx;
    origin = group->Origin.f;
    matrix2 = group->MatrixID2;
    render_pos = model->render_pos;
    
    if (((Mtxf *) mgm->parentnode) != NULL)
    {
        parentNodeMtx = modelFindNodeMtx(model, (ModelNode *) ((Mtxf *) mgm->parentnode), 0);
        parent = parentNodeMtx;
    }
    else
    {
        parent = parentMtxPtr[0];
    }
    
    has_matrix2 = flags & MODELGROUP_MTX_HAS_MATRIX2;
    matrix0_mtx = (Mtxf *) mgm->parentnode;
    
    if (parent != NULL)
    {
        matrix_4x4_set_position_and_rotation_around_xyz(&group->Origin, rot, &tmp);

        matrix0_mtx = &render_pos[matrix0].pos;
        matrix_4x4_multiply_homogeneous(parent, &tmp, matrix0_mtx);

        if (D_80036090 != NULL)
        {
            ((ModelMatrixCallback2)D_80036090)(matrix0, matrix0_mtx);
        }
    }
    else
    {
        matrix_4x4_set_position_and_rotation_around_xyz(&group->Origin, rot, &render_pos[matrix0].pos);
    }
    
    if (flags & MODELGROUP_MTX_HAS_MATRIX1)
    {
        quaternion_set_rotation_around_xyzf(rot->f, q);
        quaternion_7F05BC68(q, 0.5f, q2);
        
        if (parent != NULL)
        {
            quaternion_to_transform_matrix(origin, q2, tmp.m);
            matrix_4x4_multiply_homogeneous(parent, &tmp, &render_pos[matrix1].pos);
        }
        else
        {
            quaternion_to_transform_matrix(origin, q2, (render_pos + matrix1)->pos.m);
        }
    }

    if (has_matrix2)
    {
        if (parent != NULL)
        {
            dst = &tmp;
        }
        else
        {
            dst = &render_pos[matrix2].pos;
        }
        
        angle = rot->y;
        
        if (angle < M_PI_F)
        {
            angle = angle * 0.5f;
        }
        else
        {
            angle = M_TAU_F - ((M_TAU_F - angle) * 0.5f);
        }
        
        matrix_4x4_set_rotation_around_y(angle, dst);
        
        if (angle >= M_PI_F)
        {
            angle = M_TAU_F - angle;
        }
        if (angle < 0.890118f)
        {
            angle = modelGetBendStretchScale(angle);
        }
        else
        {
            angle = 1.5f;
        }
        
        matrix_column_3_scalar_multiply_2(angle, (f32 *) dst);
        matrix_4x4_set_position(&group->Origin, dst);
        
        if (parent != NULL)
        {
            matrix_4x4_multiply_homogeneous(parent, dst, &render_pos[matrix2].pos);
        }
    }
}


void sub_GAME_7F06DB5C(ModelRenderData *arg0, Model *arg1, ModelNode *arg2, quatf arg3)
{
    s32 spA4;
    ModelRoData_GroupRecord *spA0;
    Mtxf *sp9C;
    s32 _gap98;
    Mtxf sp58;
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    RenderPosView *sp48;
    s32 sp44;
    s32 *new_var;
    s32 sp40;
    quatf sp2C;
    Mtxf *sp28;
    f32 sp24;
    s32 _gap20;
    s32 sp1C;

    spA4 = arg2->Opcode;
    spA0 = (ModelRoData_GroupRecord *)arg2->Data;
    sp54 = spA0->MatrixID0;
    sp50 = spA0->MatrixID1;
    sp4C = spA0->MatrixID2;
    new_var = &sp1C;
    sp48 = arg1->render_pos;
    sp1C = (s32)arg2->Parent;
    if (*new_var != 0) {
        sp9C = arg0->basemtx;
        sp9C = modelFindNodeMtx(arg1, (ModelNode *)sp1C, 0);
    } else {
        sp9C = arg0->basemtx;
    }
    if (sp9C != 0) {
        quaternion_to_transform_matrix(&spA0->Origin, arg3, &sp58);
        sp1C = (s32)&sp48[sp54];
        matrix_4x4_multiply_homogeneous(sp9C, &sp58, (Mtxf *)sp1C);
        if (D_80036090 != NULL) {
            D_80036090(sp54, sp1C, sp1C);
        }
    } else {
        quaternion_to_transform_matrix(&spA0->Origin, arg3, (Mtxf *)&sp48[sp54]);
    }
    if (spA4 & 0x100) {
        quaternion_7F05BC68(arg3, 0.5f, sp2C);
        if (sp9C != 0) {
            quaternion_to_transform_matrix(&spA0->Origin, sp2C, &sp58);
            matrix_4x4_multiply_homogeneous(sp9C, &sp58, (Mtxf *)&sp48[sp50]);
        } else {
            quaternion_to_transform_matrix(&spA0->Origin, sp2C, (Mtxf *)&sp48[sp50]);
        }
    }
    if (spA4 & 0x200) {
        if (sp9C != 0) {
            sp28 = &sp58;
        } else {
            sp28 = (Mtxf *)&sp48[sp4C];
        }
        sp24 = 2.0f * acosf(*arg3);
        if (sp24 < 3.1415927f) {
            sp24 = sp24 * 0.5f;
        } else {
            sp24 = 6.2831855f - ((6.2831855f - sp24) * 0.5f);
        }
        matrix_4x4_set_rotation_around_y(sp24, sp28);
        if (sp24 >= 3.1415927f) {
            sp24 = 6.2831855f - sp24;
        }
        if (sp24 < 0.890118f) {
            sp24 = modelGetBendStretchScale(sp24);
        } else {
            sp24 = 1.5f;
        }
        matrix_column_3_scalar_multiply_2(sp24, (f32 *)sp28);
        matrix_4x4_set_position(&spA0->Origin, sp28);
        if (sp9C != 0) {
            matrix_4x4_multiply_homogeneous(sp9C, sp28, (Mtxf *)&sp48[sp4C]);
        }
    }
}


/**
 * Address: 7F06DE04
 */
u32 modelAnimReadBitsAsU16Angle(u8 *bitstream, u8 width, u32 bitOffset)
{
    u32 value = 0;
    u32 mask;
    u8 numbitsthisbyte;
    u8 remainingbits;

    remainingbits = width;
    value *= bitOffset / 8;

    if(1);

    remainingbits = width;
    bitstream += bitOffset / 8;
    bitOffset %= 8;
    numbitsthisbyte = 8 - bitOffset;

    while (remainingbits >= numbitsthisbyte)
    {
        remainingbits -= numbitsthisbyte;
        mask = (1 << numbitsthisbyte) - 1;
        value |= ((u16)((*bitstream) & mask)) << remainingbits;
        value &= 0xffff;
        bitstream++;
        numbitsthisbyte = 8;
    }

    if (remainingbits > 0)
    {
        mask = (1 << remainingbits) - 1;
        value |= ((*bitstream) >> (numbitsthisbyte - remainingbits)) & mask;
        value &= 0xffff;
    }

    value <<= 16 - width;

    return value & 0xffff;
}


/**
 * Address: 7F06DEC0
 */
void sub_GAME_7F06DEC0(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, u8 *bitstream, coord3d *rot)
{
    u32 bitoffset;
    u8 width;
    u16 rotation[3];

    width = anim->unk06;

    // Mirrored joint rotation?
    if (flip)
    {
        bitoffset = skeleton->Joints[jointnum].mtxB * width;
    }
    else
    {
        bitoffset = skeleton->Joints[jointnum].mtxA * width;
    }

    width = anim->unk06;

    rotation[0] = modelAnimReadBitsAsU16Angle(bitstream, width, bitoffset);
    bitoffset += (unsigned long) width;

    rotation[1] = modelAnimReadBitsAsU16Angle(bitstream, width, bitoffset);
    bitoffset += width;

    rotation[2] = modelAnimReadBitsAsU16Angle(bitstream, width, bitoffset);

    rot->x = (rotation[0] * M_TAU_F) / M_U16_MAX_VALUE_F;

    if (flip)
    {
        if (rotation[1] != 0)
        {
            rot->y = ((0x10000 - rotation[1]) * M_TAU_F) / M_U16_MAX_VALUE_F;
        }
        else
        {
            rot->y = 0.0f;
        }

        if (rotation[2] != 0)
        {
            rot->z = ((0x10000 - rotation[2]) * M_TAU_F) / M_U16_MAX_VALUE_F;
        }
        else
        {
            rot->z = 0.0f;
        }
    }
    else
    {
        rot->y = (rotation[1] * M_TAU_F) / M_U16_MAX_VALUE_F;
        rot->z = (rotation[2] * M_TAU_F) / M_U16_MAX_VALUE_F;
    }
}


void process_02_position(ModelRenderData *arg0, Model *model, ModelNode *node)
{
    union
    {
        s32 v;
        long long int force_structure_alignment;
    } jointnum;

    ModelSkeleton *skeleton;
    coord3d rot1;
    coord3d rot2;
    coord3d rot3;
    quatf q1;
    quatf q2;
    quatf result;
    coord3d rot4;
    ModelRoData_GroupRecord *group;

    group = &node->Data->Group;
    jointnum.v = group->JointID;
    skeleton = model->obj->Skeleton;

    rot1 = D_80036094;
    
    sub_GAME_7F06DEC0(jointnum.v, model->gunhand, skeleton, model->anim, model->unk34, &rot1);

    if (model->unk2c != 0.0f)
    {
        rot2 = D_800360A0;
        sub_GAME_7F06DEC0(jointnum.v, model->gunhand, skeleton, model->anim, model->unk38, &rot2);
        sub_GAME_7F06D160(&rot1, &rot2, model->unk2c);
    }

    if (model->unk84 != 0.0f)
    {
        rot3 = D_800360AC;
        sub_GAME_7F06DEC0(jointnum.v, model->unk25, skeleton, model->anim2, model->unk64, &rot3);

        if (model->unk5c != 0.0f)
        {
            rot4 = D_800360B8;
            sub_GAME_7F06DEC0(jointnum.v, model->unk25, skeleton, model->anim2, model->unk68, &rot4);
            sub_GAME_7F06D160(&rot3, &rot4, model->unk5c);
        }

        quaternion_set_rotation_around_xyzf(&rot1, q1);
        quaternion_set_rotation_around_xyzf(&rot3, q2);
        quaternion_ensure_shortest_path(q1, q2);
        quaternion_slerp(q1, q2, model->unk84, result);
        sub_GAME_7F06DB5C(arg0, model, node, result);
    }
    else
    {
        modelBuildGroupMatrices(arg0, model, node, &rot1);
    }
}


/**
 * Address: 7F06E2B8
 */
void sub_GAME_7F06E2B8(ModelRenderData *renderData, Model *model, ModelNode *node, f32 angle)
{
    s32 opcode;
    union ModelRoData *data;
    Mtxf *mtx;
    RenderPosView *render_pos;
    Mtxf localMtx;
    s32 m0;
    s32 m1;
    s32 m2;
    RenderPosView *renderPosBase;
    f32 *origin;
    s32 matrix2Flag;
    Mtxf *localMtxPtr;
    Mtxf *matrixPtr;
    f32 scalar;

    opcode = node->Opcode;
    data = node->Data;
    m0 = data->Group.MatrixID0;
    m1 = data->Group.MatrixID1;
    m2 = data->Group.MatrixID2;
    renderPosBase = model->render_pos;
    render_pos = renderPosBase;

    if (node->Parent != NULL)
    {
        mtx = modelFindNodeMtx(model, ((0, node))->Parent, 0);
    }
    else
    {
        mtx = renderData->basemtx;
    }

    localMtxPtr = &localMtx;

    if (mtx != NULL)
    {
        matrix_4x4_set_position_and_rotation_around_y((f32 *) &data->Group.Origin, angle, localMtxPtr);
        matrix_4x4_multiply_homogeneous(mtx, &localMtx, &render_pos[m0].pos);
    }
    else
    {
        origin = (f32 *) &data->Group.Origin;
        matrix_4x4_set_position_and_rotation_around_y(origin, angle, &render_pos[m0].pos);
    }

    matrix2Flag = 0x200;
    m0 = opcode & matrix2Flag;

    if ((opcode & 0x100) || m0)
    {
        if (matrixPtr);

        if (angle < M_PI_F)
        {
            angle = angle * 0.5f;
        }
        else
        {
            angle = M_TAU_F - ((M_TAU_F - angle) * 0.5f);
        }
    }

    if (opcode & 0x100)
    {
        if (mtx != NULL)
        {
            matrix_4x4_set_position_and_rotation_around_y((f32 *) &data->Group.Origin, angle, &localMtx);
            matrix_4x4_multiply_homogeneous(mtx, localMtxPtr, &render_pos[m1].pos);
        }
        else
        {
            matrix_4x4_set_position_and_rotation_around_y((f32 *) &data->Group.Origin, angle, &render_pos[m1].pos);
        }
    }

    if (m0)
    {
        if (mtx != NULL)
        {
            matrixPtr = &localMtx;
        }
        else
        {
            matrixPtr = &render_pos[m2].pos;
        }

        matrix_4x4_set_rotation_around_y(angle, matrixPtr);

        if (M_PI_F <= angle)
        {
            angle = M_TAU_F - angle;
        }

        if (angle < 0.890118f)
        {
            scalar = modelGetBendStretchScale(angle);
        }
        else
        {
            scalar = 1.5f;
        }

        matrix_column_3_scalar_multiply_2(scalar, (f32 *) matrixPtr);
        matrix_4x4_set_position(&data->Group.Origin, matrixPtr);

        if (mtx != NULL)
        {
            matrix_4x4_multiply_homogeneous(mtx, matrixPtr, &render_pos[m2].pos);
        }
    }
}


// Decodes a packed joint angle from the animation bitstream using either mtxA or mtxB.
f32 sub_GAME_7F06E540(s32 jointIndex, s32 useMtxB, ModelSkeleton *skeleton, ModelAnimation *anim, u8 *bitstream)
{    
    u32 bitOffset;
    u32 raw;
    u8 width;
    f32 angle;

    angle = 0.0f;
    width = anim->unk06;

    if (useMtxB != 0) {
        bitOffset = skeleton->Joints[jointIndex].mtxB * width;
    } else {
        bitOffset = skeleton->Joints[jointIndex].mtxA * width;
    }

    raw = modelAnimReadBitsAsU16Angle(bitstream, width, bitOffset);

    if (useMtxB != 0) {
        if (raw != 0) {
            angle = ((f32)(s32)(0x10000 - raw) * M_TAU_F) / M_U16_MAX_VALUE_F;
        }
    } else {
        angle = ((f32)raw * M_TAU_F) / M_U16_MAX_VALUE_F;
    }

    return angle;
}


void process_03_unknown(ModelRenderData *renderData, Model *model, ModelNode *node)
{    
    ModelSkeleton *skeleton;
    ModelRoData_GroupRecord *rodata;
    s32 jointIndex;
    f32 angle;
    f32 tmp2;
    f32 tmp;

    rodata = &node->Data->Group;
    jointIndex = rodata->JointID;
    skeleton = model->obj->Skeleton;

    angle = sub_GAME_7F06E540(jointIndex, model->gunhand, skeleton, model->anim, (u8 *)model->unk34);

    if (model->unk2c != 0.0f) {
        tmp = sub_GAME_7F06E540(jointIndex, model->gunhand, skeleton, model->anim, (u8 *)model->unk38);
        angle = sub_GAME_7F06D0CC(angle, tmp, model->unk2c);
    }

    if (model->unk84 != 0.0f) {
        tmp = sub_GAME_7F06E540(jointIndex, model->unk25, skeleton, model->anim2, (u8 *)model->unk64);

        if (model->unk5c != 0.0f) {
            tmp2 = sub_GAME_7F06E540(jointIndex, model->unk25, skeleton, model->anim2, (u8 *)model->unk68);
            tmp = sub_GAME_7F06D0CC(tmp, tmp2, model->unk5c);
        }

        angle = sub_GAME_7F06D0CC(angle, tmp, model->unk84);
    }

    sub_GAME_7F06E2B8(renderData, model, node, angle);
}


void process_15_subposition(ModelRenderData* arg0, Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;
    Mtxf *sp68;
    Mtxf sp28;
    s32 mtxindex = rodata->GroupSimple.Group1;
    RenderPosView *matrices = model->render_pos;

    if (node->Parent)
    {
        sp68 = modelFindNodeMtx(model, node->Parent, 0);
    }
    else
    {
        sp68 = arg0->basemtx;
    }

    if (sp68)
    {
        matrix_4x4_set_identity_and_position(&rodata->GroupSimple.Origin, &sp28);
        matrix_4x4_multiply_homogeneous(sp68, &sp28, &matrices[mtxindex]);
    }
    else
    {
        matrix_4x4_set_identity_and_position(&rodata->GroupSimple.Origin, &matrices[mtxindex]);
    }
}

/*
* Address: 0x7F06E858
*/
void modelUpdateDistanceRelations(Model* model, ModelNode* node)
{
    union ModelRoData *rodata = node->Data;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    Mtxf *mtx = modelFindNodeMtx(model, node, 0);
    f32 distance;

    if (g_ModelDistanceDisabled)
    {
        distance = 0;
    }
    else
    {
        distance = -mtx->m[3][2] * getPlayer_c_lodscalez();

        if (g_ModelDistanceScale != 1)
        {
            distance *= g_ModelDistanceScale;
        }
    }

    if (distance > rodata->LOD.MinDistance * model->scale || rodata->LOD.MinDistance == 0)
    {
        if (distance <= rodata->LOD.MaxDistance * model->scale)
        {
            rwdata->LOD.visible = TRUE;
            node->Child = rodata->LOD.Affects;
            return;
        }
    }

    rwdata->LOD.visible = FALSE;
    node->Child = NULL;
}

/*
* Address: 0x7F06E970
*/
void modelApplyDistanceRelations(Model* model, ModelNode* node)
{
    ModelRoData_LODRecord *rodata = &node->Data->LOD;
    ModelRwData_LODRecord *rwdata = modelGetNodeRwData(model, node);

    if (rwdata->visible)
    {
        node->Child = rodata->Affects;
    }
    else
    {
        node->Child = NULL;
    }
}


void modelApplyToggleRelations(Model* model, ModelNode* node)
{
    ModelRoData_SwitchRecord *rodata = &node->Data->Switch;
    ModelRwData_SwitchRecord *rwdata = modelGetNodeRwData(model, node);

    if (rwdata->visible)
    {
        node->Child = rodata->Controls;
    }
    else
    {
        node->Child = NULL;
    }
}


void modelApplyHeadRelations(Model* model, ModelNode* bodynode)
{
    struct ModelRwData_HeadPlaceholderRecord *rwdata = modelGetNodeRwData(model, bodynode);

    if (rwdata->ModelFileHeader)
    {
        ModelNode *headnode = rwdata->ModelFileHeader->RootNode;

        bodynode->Child = headnode;

        while (headnode)
        {
            headnode->Parent = bodynode;
            headnode = headnode->Next;
        }
    }
}


void modelApplyReorderRelationsByArg(ModelNode *basenode, bool visible)
{
    union ModelRoData *rodata = basenode->Data;
    ModelNode *node1;
    ModelNode *node2;
    ModelNode *loopnode;

    if (visible)
    {
        node1 = rodata->BSP.leftChild;
        node2 = rodata->BSP.rightChild;
    }
    else
    {
        node1 = rodata->BSP.rightChild;
        node2 = rodata->BSP.leftChild;
    }

    if (node1)
    {
        // I think what's happening here is there's two groups of siblings,
        // where node1 and node2 are the head nodes. Either group can be first,
        // and this is ensuring the node1 group is first.
        // Note that node2 might be NULL.

        basenode->Child = node1;
        node1->Prev = NULL;

        // Skip through node1's siblings until node2 is found or the end is
        // reached
        loopnode = node1;

        while (loopnode->Next && loopnode->Next != node2)
        {
            loopnode = loopnode->Next;
        }

        loopnode->Next = node2;

        if (node2)
        {
            // Append node2 and its siblings to node1's siblings
            node2->Prev = loopnode;
            loopnode = node2;

            while (loopnode->Next && loopnode->Next != node1)
            {
                loopnode = loopnode->Next;
            }

            loopnode->Next = NULL;
        }
    }
    else
    {
        basenode->Child = node2;

        if (node2)
        {
            node2->Prev = NULL;
        }
    }
}


void modelApplyReorderRelations(Model* model, ModelNode* node)
{
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    modelApplyReorderRelationsByArg(node, rwdata->BSP.visible);
}


void modelUpdateReorderRelations(Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    Mtxf *mtx = modelFindNodeMtx(model, node, 0);
    coord3d sp38;
    coord3d sp2c;
    f32 tmp;

    if (rodata->BSP.reserved == 0)
    {
        sp38.x = rodata->BSP.Vector.f[0];
        sp38.y = rodata->BSP.Vector.f[1];
        sp38.z = rodata->BSP.Vector.f[2];
        mtx4RotateVecInPlace(mtx, sp38.f);
    }
    else if (rodata->BSP.reserved == 2)
    {
        sp38.x = mtx->m[1][0] * rodata->BSP.Vector.f[1];
        sp38.y = mtx->m[1][1] * rodata->BSP.Vector.f[1];
        sp38.z = mtx->m[1][2] * rodata->BSP.Vector.f[1];
    }
    else if (rodata->BSP.reserved == 3)
    {
        sp38.x = mtx->m[2][0] * rodata->BSP.Vector.f[2];
        sp38.y = mtx->m[2][1] * rodata->BSP.Vector.f[2];
        sp38.z = mtx->m[2][2] * rodata->BSP.Vector.f[2];
    }
    else if (rodata->BSP.reserved == 1)
    {
        sp38.x = mtx->m[0][0] * rodata->BSP.Vector.f[0];
        sp38.y = mtx->m[0][1] * rodata->BSP.Vector.f[0];
        sp38.z = mtx->m[0][2] * rodata->BSP.Vector.f[0];
    }

    sp2c.x = rodata->BSP.Point.f[0];
    sp2c.y = rodata->BSP.Point.f[1];
    sp2c.z = rodata->BSP.Point.f[2];

    mtx4TransformVecInPlace(mtx, &sp2c);

    tmp = sp38.f[0] * sp2c.f[0] + sp38.f[1] * sp2c.f[1] + sp38.f[2] * sp2c.f[2];

    if (tmp < 0)
    {
        rwdata->BSP.visible = TRUE;
    }
    else
    {
        rwdata->BSP.visible = FALSE;
    }

    modelApplyReorderRelations(model, node);
}


void process_07_unknown(Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    Mtxf *mtx = modelFindNodeMtx(model, node, 0);
    f32 ratio;
    f32 coord_multiplied;
    coord3d coord;
    s32 index1;
    f32 theta;
    s32 index2;
    s32 index3;

    modelGetScaledRootToOriginDir(model, &coord);

    theta = acosf(((coord.x * mtx->m[1][0]) + (coord.y * mtx->m[1][1])) + (coord.z * mtx->m[1][2]));
    ratio = acosf((((coord.x * mtx->m[2][0]) + (coord.y * mtx->m[2][1])) + (coord.z * mtx->m[2][2])) / sinf(theta));
    coord_multiplied = ((coord.x * mtx->m[0][0]) + (coord.y * mtx->m[0][1])) + (coord.z * mtx->m[0][2]);

    if ((coord_multiplied < 0.0f) && (ratio > 0.0f))
    {
        ratio = M_TAU_F - ratio;
    }

    index1 = (theta * 64.0f) / M_TAU_F;

    index2 = (s32) ((ratio * M_U16_MAX_VALUE_F) / M_TAU_F);
    index2 += D_800360C4[index1].unk04;
    index2 = index2 >> D_800360C4[index1].unk0C;

    index3 = index2 + D_800360C4[index1].unk00;

    rwdata->Op07.index = rodata->Op07.Data[index3];
}


void modelUpdateRelationsQuick(Model *model, ModelNode *parent)
{
    ModelNode *node = parent->Child;
    ModelNode **unused_parent;

    while (node)
    {
        s32 type = node->Opcode & 0xff;
        bool dochildren = TRUE;

        switch (type)
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
                dochildren = FALSE;
                break;
            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;
            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;
            case MODELNODE_OPCODE_OP07:
                process_07_unknown(model, node);
                break;
            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;
            case MODELNODE_OPCODE_DLCOLLISION:
                break;
        }

        if (dochildren && node->Child)
        {
            node = node->Child;
        }
        else
        {
            unused_parent = &parent;
            while (node)
            {
                if (node == parent->Parent)
                {
                    node = NULL;
                    break;
                }

                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


/*
 * Address: 0x7F06EFC4
*/
void modelUpdateNodeRelations(Model *model)
{
    ModelNode *node = model->obj->RootNode;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;

            case MODELNODE_OPCODE_OP07:
                process_07_unknown(model, node);
                break;

            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEADER:
            case MODELNODE_OPCODE_DLCOLLISION:
            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelUpdateMatrices(ModelRenderData *arg0, Model *model)
{
    ModelNode *node = model->obj->RootNode;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                process_01_group_heading(arg0, model, node);
                break;

            case MODELNODE_OPCODE_GROUP:
                process_02_position(arg0, model, node);
                break;

            case MODELNODE_OPCODE_OP03:
                process_03_unknown(arg0, model, node);
                break;

            case MODELNODE_OPCODE_GROUPSIMPLE:
                process_15_subposition(arg0, model, node);
                break;

            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;

            case MODELNODE_OPCODE_OP07:
                process_07_unknown(model, node);
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

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void instcalcmatrices(ModelRenderData* arg0, Model* arg1)
{
#if defined(LEFTOVERDEBUG)
    if (arg1 == NULL)
    {
        osSyncPrintf("instcalcmatrices: no objinst!\n");
        return_null();
    }

    if (arg0->basemtx == NULL)
    {
        osSyncPrintf("instcalcmatrices: no basemtx!\n");
        return_null();
    }

    if (arg0->mtxlist == NULL)
    {
        osSyncPrintf("instcalcmatrices: no mtxlist!\n");
        return_null();
    }
#endif
    arg1->render_pos = (RenderPosView* ) arg0->mtxlist;
    arg0->mtxlist += arg1->obj->numMatrices;
    modelUpdateMatrices((ModelRenderData* ) arg0, arg1);
}


/**
 * Address 0x7F06F2F8 (VERSION_US, VERSION_JP)
 * Address 0x7F06F670 (VERSION_EU)
*/
void subcalcmatrices(ModelRenderData *arg0, struct Model *arg1)
{
#if defined(LEFTOVERDEBUG)
    if (arg1 == NULL)
    {
        osSyncPrintf("subcalcmatrices: no objanim!\n");
        return_null();
    }

    if (arg0->basemtx == NULL)
    {
        osSyncPrintf("subcalcmatrices: no basemtx!\n");
        return_null();
    }

    if (arg0->mtxlist == NULL)
    {
        osSyncPrintf("subcalcmatrices: no mtxlist!\n");
        return_null();
    }
#endif

    if (arg1->anim != NULL)
    {
#if defined(LEFTOVERDEBUG)
        if ((arg1->attachedto != NULL) && (arg1->attachedto_objinst == NULL))
        {
            osSyncPrintf("subcalcmatrices: no attach for objinst!\n");
            return_null();
        }

        if (((s32) arg1->framea < 0) || ((s32) arg1->framea >= (s32) arg1->anim->unk04))
        {
            osSyncPrintf("subcalcmatrices: framea out of range!\n");
            return_null();
        }

        if (((s32) arg1->frameb < 0) || ((s32) arg1->frameb >= (s32) arg1->anim->unk04))
        {
            osSyncPrintf("subcalcmatrices: frameb out of range!\n");
            return_null();
        }

        if ((arg1->unk84 == 0) || ((arg1->unk84 != 0) && (arg1->anim2 != NULL)))
        {
            //
        }
        else
        {
            osSyncPrintf("subcalcmatrices: no anim2!\n");
            return_null();
        }

        if (
            (arg1->anim2 != NULL)
            && (
                (arg1->anim2 == NULL)
                || (arg1->frame2a < 0)
                || ((s32) arg1->frame2a >= (s32) arg1->anim2->unk04)
                )
            )
        {
            osSyncPrintf("subcalcmatrices: frame2a out of range!\n");
            return_null();
        }

        if (
            (arg1->anim2 == NULL)
            || (
                (arg1->anim2 != NULL)
                 && (arg1->frame2b >= 0)
                 && ((s32) arg1->frame2b < (s32) arg1->anim2->unk04)
                )
            )
        {
            //
        }
        else
        {
            osSyncPrintf("subcalcmatrices: frame2b out of range!\n");
            return_null();
        }
#endif

        arg1->unk34 = loadAnimationFrame(arg1->anim, arg1->framea, arg1->obj->Skeleton);

        if (arg1->unk2c != 0.0f)
        {
            arg1->unk38 = loadAnimationFrame(arg1->anim, arg1->frameb, arg1->obj->Skeleton);
        }

        if (arg1->anim2 != NULL)
        {
            arg1->unk64 = loadAnimationFrame(arg1->anim2, arg1->frame2a, arg1->obj->Skeleton);

            if (arg1->unk5c != 0.0f)
            {
                arg1->unk68 = loadAnimationFrame(arg1->anim2, arg1->frame2b, arg1->obj->Skeleton);
            }
        }

        sub_GAME_7F0755B0();
    }

    instcalcmatrices(arg0, arg1);
}

/**
 * Address 0x7F06F5AC.
*/
struct ModelAnimation * objecthandlerGetModelAnim(struct Model* model) {
    return model->anim;
}

s8 objecthandlerGetModelGunhand(Model *model) {
    return model->gunhand;
}

/**
 * Address 0x7F06F5BC.
*/
f32 objecthandlerGetModelField28(Model *model)
{
    return model->animframe1;
}

f32 sub_GAME_7F06F5C4(Model *model)
{
    f32 unk3c;
    ModelAnimation *modelAnimation;

    unk3c = model->endframe;
    if (unk3c >= 0.0f)
    {
        return unk3c;
    }

    modelAnimation = model->anim;

    if (modelAnimation != NULL)
    {
        return modelAnimation->unk04 - 1;
    }
    return 0.0f;
}


f32 modelGetAnimSpeed(Model *model)
{
    return model->speed;
}


/**
 * Address 0x7F06F618.
 * PD: modelGetAbsAnimSpeed
*/
f32 modelGetAbsAnimSpeed(Model *model)
{
    f32 speed;

    speed = model->speed;

    if (speed < 0.0f)
    {
        speed = -speed;
    }

    return speed;
}

/**
 * Unused Function
 * Unreferenced
*/
f32 modelGetEffectiveAnimSpeed(Model *model) {
    return modelGetAnimSpeed(model) * model->playspeed;
}


s32 modelConstrainOrWrapAnimFrame(s32 frame, ModelAnimation *anim, f32 endframe)
{
    if (frame < 0) {
        if (anim->unk07 & 1) {
            frame = anim->unk04 - ((-frame) % anim->unk04);
        } else {
            frame = 0;
        }
    }
    else if ((0.0f <= endframe) && ((s32)endframe < frame)) {
        frame = ceilFloatToInt(endframe);
    }
    else if (frame >= anim->unk04) {
        if (anim->unk07 & 1) {
            frame %= anim->unk04;
        } else {
            frame = anim->unk04 - 1;
        }
    }
    else {
    }

    return frame;
}


void modelCopyAnimForMerge(Model *model, f32 timemerge)
{
    ModelAnimation *anim;
    ModelNode *root;
    struct modeldata_root *rwdata;
    s32 opcode; 

    if (0.0f < timemerge) {
        anim = model->anim;

        if (anim != NULL) {
            root = model->obj->RootNode;
            opcode = root->Opcode & 0xff;

            model->anim2 = anim;
            model->animframe2 = model->animframe1;
            model->unk5c = model->unk2c;
            model->unk25 = model->gunhand;
            model->frame2a = model->framea;
            model->frame2b = model->frameb;
            model->speed2 = model->speed;
            model->unk74 = model->newspeed;
            model->unk78 = model->oldspeed;
            model->unk7c = model->timespeed;
            model->unk80 = model->elapsespeed;
            model->unk6c = model->endframe;

            if (opcode == MODELNODE_OPCODE_HEADER) {
                rwdata = (struct modeldata_root *)modelGetNodeRwData(model, root);
                rwdata->unk02 = 1;
                rwdata->unk4c.x = rwdata->unk34.x;
                rwdata->unk4c.y = rwdata->unk34.y;
                rwdata->unk4c.z = rwdata->unk34.z;
                rwdata->unk40.x = rwdata->unk24.x;
                rwdata->unk40.y = rwdata->unk24.y;
                rwdata->unk40.z = rwdata->unk24.z;
            }

            return;
        }
    }

    model->anim2 = NULL;
}


#ifdef NONMATCHING
void modelSetAnimation2(Model *model, void *anim, s32 arg2, f32 startframe, f32 half, f32 arg5)
{
    void           *sp80;
    f32             sp70;
    f32             sp6C;
    f32             sp64;
    f32             sp60;
    f32             sp5C;
    f32             sp58;
    f32             sp54;
    f32             sp50;
    f32             sp4C;
    f32             sp48;
    f32             sp44;
    f32             sp40;
    f32             sp30;
    s32             sp2C;
    ModelNode      *temp_a1;
    f32             temp_f0;
    f32             temp_f0_2;
    f32             temp_f0_3;
    f32             temp_f10;
    f32             temp_f12;
    f32             temp_f12_2;
    f32             temp_f14;
    f32             temp_f14_2;
    f32             temp_f2;
    f32             temp_f2_2;
    f32             temp_f2_3;
    f32             temp_f2_4;
    f32             temp_f4;
    f32             temp_f6;
    modeldata_root *temp_v0;
    f32             phi_f14;

    sp2C = model->anim == 0;
    if (model->unk54 != 0)
    {
        model->unk88 = arg5;
        model->unk8C = 0.0f;
        model->unk84 = 1.0f;
    }
    else
    {
        model->unk88 = 0.0f;
        model->unk84 = 0.0f;
    }
    model->anim  = anim;
    model->unk24 = arg2;
    model->endframe = -1.0f;
    model->speed = half;
    model->timespeed = 0.0f;
    modelSetAnimFrame(startframe, model, startframe, anim);
    model->unk26 = 0;
    temp_a1      = model->obj->RootNode;
    if ((temp_a1->Opcode & 0xFF) == 1)
    {
        sp80      = temp_a1->Data;
        temp_v0   = modelGetNodeRwData(model, temp_a1);
        sp5C.unk0 = D_80036244.unk0;
        temp_f2   = model->scale * model->anim_translation_scale;
        sp5C.unk4 = D_80036244.unk4;
        sp5C.unk8 = D_80036244.unk8;
        sp70      = temp_f2;
        sp58      = sub_GAME_7F06D3F4(*sp80, model->unk24, model->obj->Skeleton, model->anim, model->unk32, &sp5C);
        if (temp_f2 != 1.0f)
        {
            sp5C *= temp_f2;
            sp60 *= temp_f2;
            sp64 *= temp_f2;
        }
        sp6C      = cosf(temp_v0->unk14);
        temp_f0   = sinf(temp_v0->unk14);
        temp_f2_2 = model->unk2C;
        if (temp_f2_2 == 0.0f)
        {
            temp_f2_3                 = temp_v0->pos.x;
            temp_v0->unk34.x          = temp_f2_3;
            temp_v0->unk34.AsArray[2] = temp_v0->pos.AsArray[2];
            temp_v0->unk34.AsArray[1] = temp_v0->pos.AsArray[1] - temp_v0->ground;
            temp_v0->unk30            = temp_v0->unk14;
            temp_f4                   = (sp64 * temp_f0) + (temp_f2_3 + (sp5C * sp6C));
            sp4C                      = temp_f4;
            sp50                      = sp60;
            sp54                      = (sp64 * sp6C) + (temp_v0->unk34.AsArray[2] - (sp5C * temp_f0));
            temp_v0->unk24.x          = temp_f4;
            temp_v0->unk24.AsArray[1] = sp50;
            temp_v0->unk24.AsArray[2] = sp54;
            if (temp_v0->unk18 == 0.0f)
            {
                temp_f0_2      = temp_v0->unk30 + sp58;
                temp_v0->unk20 = temp_f0_2;
                if (temp_f0_2 >= M_TAU_F)
                {
                    temp_v0->unk20 -= M_TAU_F;
                }
            }
            temp_v0->unk1 = 1;
        }
        else
        {
            temp_f14                  = (sp64 * temp_f0) + (sp5C * sp6C);
            temp_f10                  = (sp64 * sp6C) + (-sp5C * temp_f0);
            sp30                      = temp_f10;
            sp44                      = sp60;
            temp_f6                   = temp_v0->pos.x + (temp_f14 * (1.0f - temp_f2_2));
            sp40                      = temp_f6;
            sp48                      = temp_v0->pos.AsArray[2] + (temp_f10 * (1.0f - model->unk2C));
            temp_v0->unk24.x          = temp_f6;
            temp_v0->unk24.AsArray[1] = sp44;
            temp_v0->unk24.AsArray[2] = sp48;
            temp_v0->unk34.x          = temp_v0->unk24.x - temp_f14;
            temp_f12                  = temp_v0->pos.AsArray[1] - temp_v0->ground;
            temp_f2_4                 = model->unk2C;
            temp_f12_2                = temp_v0->unk14;
            temp_v0->unk34.AsArray[1] = temp_f12 - (((sp60 - temp_f12) * temp_f2_4) / (1.0f - temp_f2_4));
            temp_v0->unk34.AsArray[2] = temp_v0->unk24.AsArray[2] - sp30;
            temp_f14_2                = temp_f12_2 - sp58;
            phi_f14                   = temp_f14_2;
            if (temp_f14_2 < 0.0f)
            {
                phi_f14 = temp_f14_2 + M_TAU_F;
            }
            temp_v0->unk30 = sub_GAME_7F06D0CC(temp_f12_2, phi_f14, model->unk2C);
            if (temp_v0->unk18 == 0.0f)
            {
                temp_f0_3      = temp_v0->unk30 + sp58;
                temp_v0->unk20 = temp_f0_3;
                if (temp_f0_3 >= M_TAU_F)
                {
                    temp_v0->unk20 -= M_TAU_F;
                }
            }
            temp_v0->unk1 = 1;
        }
        if (sp2C != 0)
        {
            temp_v0->unk34.AsArray[1] = temp_v0->unk24.AsArray[1];
        }
    }
}
#else
GLOBAL_ASM(
.late_rodata
glabel D_80054D40
.word 0x40c90fdb /*6.2831855*/
glabel D_80054D44
.word 0x40c90fdb /*6.2831855*/
glabel D_80054D48
.word 0x40c90fdb /*6.2831855*/
.text
glabel modelSetAnimation2
/* 0A43A8 7F06F878 27BDFF70 */  addiu $sp, $sp, -0x90
/* 0A43AC 7F06F87C AFBF0024 */  sw    $ra, 0x24($sp)
/* 0A43B0 7F06F880 AFB10020 */  sw    $s1, 0x20($sp)
/* 0A43B4 7F06F884 AFB0001C */  sw    $s0, 0x1c($sp)
/* 0A43B8 7F06F888 AFA50094 */  sw    $a1, 0x94($sp)
/* 0A43BC 7F06F88C 8C8E0020 */  lw    $t6, 0x20($a0)
/* 0A43C0 7F06F890 44876000 */  mtc1  $a3, $f12
/* 0A43C4 7F06F894 00A03825 */  move  $a3, $a1
/* 0A43C8 7F06F898 2DCF0001 */  sltiu $t7, $t6, 1
/* 0A43CC 7F06F89C AFAF002C */  sw    $t7, 0x2c($sp)
/* 0A43D0 7F06F8A0 8C980054 */  lw    $t8, 0x54($a0)
/* 0A43D4 7F06F8A4 44056000 */  mfc1  $a1, $f12
/* 0A43D8 7F06F8A8 00808825 */  move  $s1, $a0
/* 0A43DC 7F06F8AC 13000008 */  beqz  $t8, .L7F06F8D0
/* 0A43E0 7F06F8B0 C7A400A4 */   lwc1  $f4, 0xa4($sp)
/* 0A43E4 7F06F8B4 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0A43E8 7F06F8B8 44803000 */  mtc1  $zero, $f6
/* 0A43EC 7F06F8BC 44814000 */  mtc1  $at, $f8
/* 0A43F0 7F06F8C0 E4840088 */  swc1  $f4, 0x88($a0)
/* 0A43F4 7F06F8C4 E486008C */  swc1  $f6, 0x8c($a0)
/* 0A43F8 7F06F8C8 10000005 */  b     .L7F06F8E0
/* 0A43FC 7F06F8CC E4880084 */   swc1  $f8, 0x84($a0)
.L7F06F8D0:
/* 0A4400 7F06F8D0 44805000 */  mtc1  $zero, $f10
/* 0A4404 7F06F8D4 44802000 */  mtc1  $zero, $f4
/* 0A4408 7F06F8D8 E62A0088 */  swc1  $f10, 0x88($s1)
/* 0A440C 7F06F8DC E6240084 */  swc1  $f4, 0x84($s1)
.L7F06F8E0:
/* 0A4410 7F06F8E0 3C01BF80 */  li    $at, 0xBF800000 # -1.000000
/* 0A4414 7F06F8E4 44813000 */  mtc1  $at, $f6
/* 0A4418 7F06F8E8 AE270020 */  sw    $a3, 0x20($s1)
/* 0A441C 7F06F8EC A2260024 */  sb    $a2, 0x24($s1)
/* 0A4420 7F06F8F0 E626003C */  swc1  $f6, 0x3c($s1)
/* 0A4424 7F06F8F4 C7A800A0 */  lwc1  $f8, 0xa0($sp)
/* 0A4428 7F06F8F8 44805000 */  mtc1  $zero, $f10
/* 0A442C 7F06F8FC 02202025 */  move  $a0, $s1
/* 0A4430 7F06F900 E6280040 */  swc1  $f8, 0x40($s1)
/* 0A4434 7F06F904 0FC1BFD9 */  jal   modelSetAnimFrame
/* 0A4438 7F06F908 E62A004C */   swc1  $f10, 0x4c($s1)
/* 0A443C 7F06F90C 8E390008 */  lw    $t9, 8($s1)
/* 0A4440 7F06F910 A2200026 */  sb    $zero, 0x26($s1)
/* 0A4444 7F06F914 24010001 */  li    $at, 1
/* 0A4448 7F06F918 8F250000 */  lw    $a1, ($t9)
/* 0A444C 7F06F91C 94A20000 */  lhu   $v0, ($a1)
/* 0A4450 7F06F920 304800FF */  andi  $t0, $v0, 0xff
/* 0A4454 7F06F924 550100C5 */  bnel  $t0, $at, .L7F06FC3C
/* 0A4458 7F06F928 8FBF0024 */   lw    $ra, 0x24($sp)
/* 0A445C 7F06F92C 8CA90004 */  lw    $t1, 4($a1)
/* 0A4460 7F06F930 02202025 */  move  $a0, $s1
/* 0A4464 7F06F934 0FC1B1E7 */  jal   modelGetNodeRwData
/* 0A4468 7F06F938 AFA90080 */   sw    $t1, 0x80($sp)
/* 0A446C 7F06F93C 3C0C8003 */  lui   $t4, %hi(D_80036244)
/* 0A4470 7F06F940 8FAA0080 */  lw    $t2, 0x80($sp)
/* 0A4474 7F06F944 8E2B0008 */  lw    $t3, 8($s1)
/* 0A4478 7F06F948 258C6244 */  addiu $t4, %lo(D_80036244) # addiu $t4, $t4, 0x6244
/* 0A447C 7F06F94C 8D810000 */  lw    $at, ($t4)
/* 0A4480 7F06F950 C6240014 */  lwc1  $f4, 0x14($s1)
/* 0A4484 7F06F954 C62600B8 */  lwc1  $f6, 0xb8($s1)
/* 0A4488 7F06F958 27A3005C */  addiu $v1, $sp, 0x5c
/* 0A448C 7F06F95C 95440000 */  lhu   $a0, ($t2)
/* 0A4490 7F06F960 8D660004 */  lw    $a2, 4($t3)
/* 0A4494 7F06F964 AC610000 */  sw    $at, ($v1)
/* 0A4498 7F06F968 8D8F0004 */  lw    $t7, 4($t4)
/* 0A449C 7F06F96C 46062082 */  mul.s $f2, $f4, $f6
/* 0A44A0 7F06F970 00408025 */  move  $s0, $v0
/* 0A44A4 7F06F974 AC6F0004 */  sw    $t7, 4($v1)
/* 0A44A8 7F06F978 8D810008 */  lw    $at, 8($t4)
/* 0A44AC 7F06F97C AC610008 */  sw    $at, 8($v1)
/* 0A44B0 7F06F980 86380032 */  lh    $t8, 0x32($s1)
/* 0A44B4 7F06F984 8E270020 */  lw    $a3, 0x20($s1)
/* 0A44B8 7F06F988 82250024 */  lb    $a1, 0x24($s1)
/* 0A44BC 7F06F98C E7A20070 */  swc1  $f2, 0x70($sp)
/* 0A44C0 7F06F990 AFA30014 */  sw    $v1, 0x14($sp)
/* 0A44C4 7F06F994 0FC1B4FD */  jal   sub_GAME_7F06D3F4
/* 0A44C8 7F06F998 AFB80010 */   sw    $t8, 0x10($sp)
/* 0A44CC 7F06F99C 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0A44D0 7F06F9A0 C7A20070 */  lwc1  $f2, 0x70($sp)
/* 0A44D4 7F06F9A4 44814000 */  mtc1  $at, $f8
/* 0A44D8 7F06F9A8 E7A00058 */  swc1  $f0, 0x58($sp)
/* 0A44DC 7F06F9AC C7AA005C */  lwc1  $f10, 0x5c($sp)
/* 0A44E0 7F06F9B0 46081032 */  c.eq.s $f2, $f8
/* 0A44E4 7F06F9B4 00000000 */  nop
/* 0A44E8 7F06F9B8 45010009 */  bc1t  .L7F06F9E0
/* 0A44EC 7F06F9BC 00000000 */   nop
/* 0A44F0 7F06F9C0 46025102 */  mul.s $f4, $f10, $f2
/* 0A44F4 7F06F9C4 C7A60060 */  lwc1  $f6, 0x60($sp)
/* 0A44F8 7F06F9C8 C7AA0064 */  lwc1  $f10, 0x64($sp)
/* 0A44FC 7F06F9CC 46023202 */  mul.s $f8, $f6, $f2
/* 0A4500 7F06F9D0 E7A4005C */  swc1  $f4, 0x5c($sp)
/* 0A4504 7F06F9D4 46025102 */  mul.s $f4, $f10, $f2
/* 0A4508 7F06F9D8 E7A80060 */  swc1  $f8, 0x60($sp)
/* 0A450C 7F06F9DC E7A40064 */  swc1  $f4, 0x64($sp)
.L7F06F9E0:
/* 0A4510 7F06F9E0 0FC15FA8 */  jal   cosf
/* 0A4514 7F06F9E4 C60C0014 */   lwc1  $f12, 0x14($s0)
/* 0A4518 7F06F9E8 C60C0014 */  lwc1  $f12, 0x14($s0)
/* 0A451C 7F06F9EC 0FC15FAB */  jal   sinf
/* 0A4520 7F06F9F0 E7A0006C */   swc1  $f0, 0x6c($sp)
/* 0A4524 7F06F9F4 C622002C */  lwc1  $f2, 0x2c($s1)
/* 0A4528 7F06F9F8 44803000 */  mtc1  $zero, $f6
/* 0A452C 7F06F9FC C7B2006C */  lwc1  $f18, 0x6c($sp)
/* 0A4530 7F06FA00 24190001 */  li    $t9, 1
/* 0A4534 7F06FA04 46023032 */  c.eq.s $f6, $f2
/* 0A4538 7F06FA08 C7A60064 */  lwc1  $f6, 0x64($sp)
/* 0A453C 7F06FA0C C7AA005C */  lwc1  $f10, 0x5c($sp)
/* 0A4540 7F06FA10 45000032 */  bc1f  .L7F06FADC
/* 0A4544 7F06FA14 00000000 */   nop
/* 0A4548 7F06FA18 C608000C */  lwc1  $f8, 0xc($s0)
/* 0A454C 7F06FA1C C60A0004 */  lwc1  $f10, 4($s0)
/* 0A4550 7F06FA20 C6020008 */  lwc1  $f2, 8($s0)
/* 0A4554 7F06FA24 C6060010 */  lwc1  $f6, 0x10($s0)
/* 0A4558 7F06FA28 460A4101 */  sub.s $f4, $f8, $f10
/* 0A455C 7F06FA2C C6080014 */  lwc1  $f8, 0x14($s0)
/* 0A4560 7F06FA30 E6020034 */  swc1  $f2, 0x34($s0)
/* 0A4564 7F06FA34 E606003C */  swc1  $f6, 0x3c($s0)
/* 0A4568 7F06FA38 E6040038 */  swc1  $f4, 0x38($s0)
/* 0A456C 7F06FA3C E6080030 */  swc1  $f8, 0x30($s0)
/* 0A4570 7F06FA40 C7AA0064 */  lwc1  $f10, 0x64($sp)
/* 0A4574 7F06FA44 C7A6005C */  lwc1  $f6, 0x5c($sp)
/* 0A4578 7F06FA48 46005102 */  mul.s $f4, $f10, $f0
/* 0A457C 7F06FA4C 00000000 */  nop
/* 0A4580 7F06FA50 46123202 */  mul.s $f8, $f6, $f18
/* 0A4584 7F06FA54 46081200 */  add.s $f8, $f2, $f8
/* 0A4588 7F06FA58 46003182 */  mul.s $f6, $f6, $f0
/* 0A458C 7F06FA5C 46082100 */  add.s $f4, $f4, $f8
/* 0A4590 7F06FA60 C7A80060 */  lwc1  $f8, 0x60($sp)
/* 0A4594 7F06FA64 E7A4004C */  swc1  $f4, 0x4c($sp)
/* 0A4598 7F06FA68 E7A80050 */  swc1  $f8, 0x50($sp)
/* 0A459C 7F06FA6C C608003C */  lwc1  $f8, 0x3c($s0)
/* 0A45A0 7F06FA70 46064201 */  sub.s $f8, $f8, $f6
/* 0A45A4 7F06FA74 46125182 */  mul.s $f6, $f10, $f18
/* 0A45A8 7F06FA78 46083280 */  add.s $f10, $f6, $f8
/* 0A45AC 7F06FA7C E7AA0054 */  swc1  $f10, 0x54($sp)
/* 0A45B0 7F06FA80 E6040024 */  swc1  $f4, 0x24($s0)
/* 0A45B4 7F06FA84 C6040018 */  lwc1  $f4, 0x18($s0)
/* 0A45B8 7F06FA88 44805000 */  mtc1  $zero, $f10
/* 0A45BC 7F06FA8C C7A60050 */  lwc1  $f6, 0x50($sp)
/* 0A45C0 7F06FA90 46045032 */  c.eq.s $f10, $f4
/* 0A45C4 7F06FA94 E6060028 */  swc1  $f6, 0x28($s0)
/* 0A45C8 7F06FA98 C7A80054 */  lwc1  $f8, 0x54($sp)
/* 0A45CC 7F06FA9C 4500000D */  bc1f  .L7F06FAD4
/* 0A45D0 7F06FAA0 E608002C */   swc1  $f8, 0x2c($s0)
/* 0A45D4 7F06FAA4 C6060030 */  lwc1  $f6, 0x30($s0)
/* 0A45D8 7F06FAA8 C7A80058 */  lwc1  $f8, 0x58($sp)
/* 0A45DC 7F06FAAC 3C018005 */  lui    $at, %hi(D_80054D40)
/* 0A45E0 7F06FAB0 C4224D40 */  lwc1  $f2, %lo(D_80054D40)($at)
/* 0A45E4 7F06FAB4 46083000 */  add.s $f0, $f6, $f8
/* 0A45E8 7F06FAB8 4600103E */  c.le.s $f2, $f0
/* 0A45EC 7F06FABC E6000020 */  swc1  $f0, 0x20($s0)
/* 0A45F0 7F06FAC0 45000004 */  bc1f  .L7F06FAD4
/* 0A45F4 7F06FAC4 00000000 */   nop
/* 0A45F8 7F06FAC8 C60A0020 */  lwc1  $f10, 0x20($s0)
/* 0A45FC 7F06FACC 46025101 */  sub.s $f4, $f10, $f2
/* 0A4600 7F06FAD0 E6040020 */  swc1  $f4, 0x20($s0)
.L7F06FAD4:
/* 0A4604 7F06FAD4 10000053 */  b     .L7F06FC24
/* 0A4608 7F06FAD8 A2190001 */   sb    $t9, 1($s0)
.L7F06FADC:
/* 0A460C 7F06FADC 46003202 */  mul.s $f8, $f6, $f0
/* 0A4610 7F06FAE0 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0A4614 7F06FAE4 C7B00060 */  lwc1  $f16, 0x60($sp)
/* 0A4618 7F06FAE8 46125102 */  mul.s $f4, $f10, $f18
/* 0A461C 7F06FAEC 46044380 */  add.s $f14, $f8, $f4
/* 0A4620 7F06FAF0 46123202 */  mul.s $f8, $f6, $f18
/* 0A4624 7F06FAF4 46005107 */  neg.s $f4, $f10
/* 0A4628 7F06FAF8 46002182 */  mul.s $f6, $f4, $f0
/* 0A462C 7F06FAFC 46064280 */  add.s $f10, $f8, $f6
/* 0A4630 7F06FB00 44814000 */  mtc1  $at, $f8
/* 0A4634 7F06FB04 00000000 */  nop
/* 0A4638 7F06FB08 46024181 */  sub.s $f6, $f8, $f2
/* 0A463C 7F06FB0C E7AA0030 */  swc1  $f10, 0x30($sp)
/* 0A4640 7F06FB10 C6040008 */  lwc1  $f4, 8($s0)
/* 0A4644 7F06FB14 E7B00044 */  swc1  $f16, 0x44($sp)
/* 0A4648 7F06FB18 46067202 */  mul.s $f8, $f14, $f6
/* 0A464C 7F06FB1C 46082180 */  add.s $f6, $f4, $f8
/* 0A4650 7F06FB20 44812000 */  mtc1  $at, $f4
/* 0A4654 7F06FB24 E7A60040 */  swc1  $f6, 0x40($sp)
/* 0A4658 7F06FB28 C628002C */  lwc1  $f8, 0x2c($s1)
/* 0A465C 7F06FB2C 46082181 */  sub.s $f6, $f4, $f8
/* 0A4660 7F06FB30 C6080010 */  lwc1  $f8, 0x10($s0)
/* 0A4664 7F06FB34 46065102 */  mul.s $f4, $f10, $f6
/* 0A4668 7F06FB38 C7A60040 */  lwc1  $f6, 0x40($sp)
/* 0A466C 7F06FB3C 46044280 */  add.s $f10, $f8, $f4
/* 0A4670 7F06FB40 E7AA0048 */  swc1  $f10, 0x48($sp)
/* 0A4674 7F06FB44 E6060024 */  swc1  $f6, 0x24($s0)
/* 0A4678 7F06FB48 C7A80044 */  lwc1  $f8, 0x44($sp)
/* 0A467C 7F06FB4C C60A0024 */  lwc1  $f10, 0x24($s0)
/* 0A4680 7F06FB50 E6080028 */  swc1  $f8, 0x28($s0)
/* 0A4684 7F06FB54 C7A40048 */  lwc1  $f4, 0x48($sp)
/* 0A4688 7F06FB58 C608000C */  lwc1  $f8, 0xc($s0)
/* 0A468C 7F06FB5C 460E5181 */  sub.s $f6, $f10, $f14
/* 0A4690 7F06FB60 E604002C */  swc1  $f4, 0x2c($s0)
/* 0A4694 7F06FB64 C6040004 */  lwc1  $f4, 4($s0)
/* 0A4698 7F06FB68 E6060034 */  swc1  $f6, 0x34($s0)
/* 0A469C 7F06FB6C 46044301 */  sub.s $f12, $f8, $f4
/* 0A46A0 7F06FB70 C622002C */  lwc1  $f2, 0x2c($s1)
/* 0A46A4 7F06FB74 44814000 */  mtc1  $at, $f8
/* 0A46A8 7F06FB78 3C018005 */  lui   $at, %hi(D_80054D44)
/* 0A46AC 7F06FB7C 460C8281 */  sub.s $f10, $f16, $f12
/* 0A46B0 7F06FB80 46024101 */  sub.s $f4, $f8, $f2
/* 0A46B4 7F06FB84 46025182 */  mul.s $f6, $f10, $f2
/* 0A46B8 7F06FB88 46043283 */  div.s $f10, $f6, $f4
/* 0A46BC 7F06FB8C C606002C */  lwc1  $f6, 0x2c($s0)
/* 0A46C0 7F06FB90 460A6201 */  sub.s $f8, $f12, $f10
/* 0A46C4 7F06FB94 C60C0014 */  lwc1  $f12, 0x14($s0)
/* 0A46C8 7F06FB98 E6080038 */  swc1  $f8, 0x38($s0)
/* 0A46CC 7F06FB9C C7A40030 */  lwc1  $f4, 0x30($sp)
/* 0A46D0 7F06FBA0 46043281 */  sub.s $f10, $f6, $f4
/* 0A46D4 7F06FBA4 44803000 */  mtc1  $zero, $f6
/* 0A46D8 7F06FBA8 E60A003C */  swc1  $f10, 0x3c($s0)
/* 0A46DC 7F06FBAC C7A80058 */  lwc1  $f8, 0x58($sp)
/* 0A46E0 7F06FBB0 46086381 */  sub.s $f14, $f12, $f8
/* 0A46E4 7F06FBB4 4606703C */  c.lt.s $f14, $f6
/* 0A46E8 7F06FBB8 00000000 */  nop
/* 0A46EC 7F06FBBC 45000003 */  bc1f  .L7F06FBCC
/* 0A46F0 7F06FBC0 00000000 */   nop
/* 0A46F4 7F06FBC4 C4244D44 */  lwc1  $f4, %lo(D_80054D44)($at)
/* 0A46F8 7F06FBC8 46047380 */  add.s $f14, $f14, $f4
.L7F06FBCC:
/* 0A46FC 7F06FBCC 0FC1B433 */  jal   sub_GAME_7F06D0CC
/* 0A4700 7F06FBD0 8E26002C */   lw    $a2, 0x2c($s1)
/* 0A4704 7F06FBD4 44805000 */  mtc1  $zero, $f10
/* 0A4708 7F06FBD8 C6080018 */  lwc1  $f8, 0x18($s0)
/* 0A470C 7F06FBDC 3C018005 */  lui   $at, %hi(D_80054D48)
/* 0A4710 7F06FBE0 C4224D48 */  lwc1  $f2, %lo(D_80054D48)($at)
/* 0A4714 7F06FBE4 46085032 */  c.eq.s $f10, $f8
/* 0A4718 7F06FBE8 E6000030 */  swc1  $f0, 0x30($s0)
/* 0A471C 7F06FBEC C7A40058 */  lwc1  $f4, 0x58($sp)
/* 0A4720 7F06FBF0 24080001 */  li    $t0, 1
/* 0A4724 7F06FBF4 4502000B */  bc1fl .L7F06FC24
/* 0A4728 7F06FBF8 A2080001 */   sb    $t0, 1($s0)
/* 0A472C 7F06FBFC C6060030 */  lwc1  $f6, 0x30($s0)
/* 0A4730 7F06FC00 46043000 */  add.s $f0, $f6, $f4
/* 0A4734 7F06FC04 4600103E */  c.le.s $f2, $f0
/* 0A4738 7F06FC08 E6000020 */  swc1  $f0, 0x20($s0)
/* 0A473C 7F06FC0C 45020005 */  bc1fl .L7F06FC24
/* 0A4740 7F06FC10 A2080001 */   sb    $t0, 1($s0)
/* 0A4744 7F06FC14 C60A0020 */  lwc1  $f10, 0x20($s0)
/* 0A4748 7F06FC18 46025201 */  sub.s $f8, $f10, $f2
/* 0A474C 7F06FC1C E6080020 */  swc1  $f8, 0x20($s0)
/* 0A4750 7F06FC20 A2080001 */  sb    $t0, 1($s0)
.L7F06FC24:
/* 0A4754 7F06FC24 8FA9002C */  lw    $t1, 0x2c($sp)
/* 0A4758 7F06FC28 51200004 */  beql  $t1, $zero, .L7F06FC3C
/* 0A475C 7F06FC2C 8FBF0024 */   lw    $ra, 0x24($sp)
/* 0A4760 7F06FC30 C6060028 */  lwc1  $f6, 0x28($s0)
/* 0A4764 7F06FC34 E6060038 */  swc1  $f6, 0x38($s0)
/* 0A4768 7F06FC38 8FBF0024 */  lw    $ra, 0x24($sp)
.L7F06FC3C:
/* 0A476C 7F06FC3C 8FB0001C */  lw    $s0, 0x1c($sp)
/* 0A4770 7F06FC40 8FB10020 */  lw    $s1, 0x20($sp)
/* 0A4774 7F06FC44 03E00008 */  jr    $ra
/* 0A4778 7F06FC48 27BD0090 */   addiu $sp, $sp, 0x90
)
#endif


void modelSetAnimationWithMerge(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 timemerge, s32 domerge) {
    if (domerge != 0) {
        modelCopyAnimForMerge(model, timemerge);
    }
    modelSetAnimation2(model, modelAnimation, flip, startframe, speed, timemerge);
}


void modelSetAnimation(Model *model, ModelAnimation *modelAnimation, s32 flip, f32 startframe, f32 speed, f32 merge) {
    modelCopyAnimForMerge(model, merge);
    modelSetAnimation2(model, modelAnimation, flip, startframe, speed, merge);
}


/*
 * Match-only overlay types for sub_GAME_7F06FCFC.
 */
typedef struct ModelCopyHead {
    u32 words[8];      // 0x00-0x1f
} ModelCopyHead;


typedef struct ModelCopyBc {
    u32 words[0x2f];   // 0x00-0xbb
} ModelCopyBc;


/**
 * Unreferenced.
 * 
 * The function copies the Model data through anim_translation_scale (0x00-0xbb),
 * then restores the destination's base/resource fields (0x00-0x1f).
 * Maybe some kind of old/abandoned anim copy function.
 */
void sub_GAME_7F06FCFC(Model *src, Model *dst)
{
    ModelCopyHead tmp;

    tmp = *(ModelCopyHead *)dst;
    *(ModelCopyBc *)dst = *(ModelCopyBc *)src;
    *(ModelCopyHead *)dst = tmp;
}


void modelSetAnimLooping(Model *model, f32 loopframe, f32 loopmerge) {
    model->animlooping = 1;
    model->animloopframe = loopframe;
    model->animloopmerge = loopmerge;
}


void modelSetAnimEndFrame(Model *model, f32 endframe) {
    ModelAnimation *modelAnimation = model->anim;

    if ((modelAnimation != NULL) && (endframe < (modelAnimation->unk04 - 1))) {
        model->endframe = endframe;
    } else {
        model->endframe = -1.0f;
    }
#ifdef DEBUG
    // not too sure why debug wants to call this - must have some significance when most debug has been stripped from this file in XBLA
    modelSetAnimFrame(model, (int)model->animframe1);
#endif
}

void modelSetAnimFlipFunction(Model *model, void *callback) {
    model->animflipfunc = callback;
}


/**
 * Unused Function
*/
void sub_GAME_7F06FE44(Model *model, s32 arg1) {
    model->unk9c = arg1;
}

void modelSetAnimSpeed(Model *model, f32 anim_speed, f32 startframe) {

    if (startframe > 0.0f) {
        model->timespeed = startframe;
        model->newspeed = anim_speed;
        model->elapsespeed = 0.0f;
        model->oldspeed = model->speed;
        return;
    }

    model->speed = anim_speed;
    model->timespeed = 0.0f;
}

/**
 * @param arg0:
 * @param arg1:
 * @param arg2: must be non-zero.
 *
 * Address 0x7F06FE90.
*/
void sub_GAME_7F06FE90(Model *model, f32 arg1, f32 arg2)
{
    f32 temp_f0;
    f32 phi_f2;
    f32 t;

    temp_f0 = model->animframe1;

    if (temp_f0 <= arg1)
    {
        phi_f2 = arg1 - temp_f0;
    }
    else
    {
        phi_f2 = ( (f32)model->anim->unk04 - temp_f0) + arg1;
    }

    t = model->speed + ((2.0f * phi_f2) / arg2);
    modelSetAnimSpeed(model, t, arg2);
}

void modelSetAnimPlaySpeed(Model *model, f32 animation_rate, f32 startframe) {
    if (startframe > 0.0f) {
        model->unkb0 = startframe;
        model->animrate = animation_rate;
        model->unkb4 = 0.0f;
        model->unkac = model->playspeed;
        return;
    }
    model->playspeed = animation_rate;
    model->unkb0 = 0.0f;
}


void sub_GAME_7F06FF5C(Model *model, s32 arg1) {
    model->unka0 = arg1;
}


void modelSetAnimFrame(Model* model, f32 frame)
{
    s32 framea;
    s32 frameb;
    bool forwards;

    framea = floorFloatToInt(frame);

    forwards = (model->speed >= 0);
    frameb = (forwards ? framea + 1 : framea - 1);

    model->framea = modelConstrainOrWrapAnimFrame(framea, model->anim, model->endframe);
    model->frameb = modelConstrainOrWrapAnimFrame(frameb, model->anim, model->endframe);

    if (model->framea == model->frameb)
    {
        model->unk2c = 0.0f;
        model->animframe1 = model->framea;
    }
    else if (forwards)
    {
        f32 tmp = frame - framea;
        model->unk2c = tmp;
        model->animframe1 = model->framea + tmp;
    }
    else
    {
        f32 tmp = 1.0f - (frame - (f32) frameb);
        model->unk2c = tmp;
        model->animframe1 = model->frameb + (1.0f - tmp);
    }
}


void modelSetAnimFrame2(Model* model, f32 frame1, f32 frame2)
{
    s32 framea;
    s32 frameb;
    bool forwards;

    modelSetAnimFrame(model, frame1);

    if (model->anim2 != NULL)
    {
        framea = floorFloatToInt(frame2);

        forwards = (model->speed2 >= 0.0f);
        frameb = forwards ? (framea + 1) : (framea - 1);

        model->frame2a = modelConstrainOrWrapAnimFrame(framea, model->anim2, model->unk6c);
        model->frame2b = modelConstrainOrWrapAnimFrame(frameb, model->anim2, model->unk6c);

        if (model->frame2a == model->frame2b)
        {
            model->unk5c = 0.0f;
            model->animframe2 = model->frame2a;
        }
        else if (forwards != 0)
        {
            f32 tmp = frame2 - framea;
            model->unk5c = tmp;
            model->animframe2 = model->frame2a + tmp;
        }
        else
        {
            f32 tmp = 1.0f - (frame2 - (f32) frameb);
            model->unk5c = tmp;
            model->animframe2 = model->frame2b + (1.0f - tmp);
        }
    }
}


/**
 * Address 0x7F0701D4.
*/
void modelSetAnimMergingEnabled(s32 arg0)
{
    g_ModelAnimMergingEnabled = arg0;
}


/**
 * Address 0x7F0701E0.
*/
u32 modelIsAnimMergingEnabled(void)
{
    return g_ModelAnimMergingEnabled;
}


void modelSetAnimFrame2WithChrStuff(Model *model, f32 framea, f32 frameb, f32 frame2a, f32 frame2b)
{
    Model *modelptr;
    ModelRwData_HeaderRecord *header;
    ModelNode *root;
    ModelNode *node;
    s32 jointnum;
    ModelSkeleton *skeleton;
    f32 scale;
    s32 curframe;
    f32 speed;
    coord3d pos;
    f32 anglecur;
    f32 cosangle;
    s32 vb;
    s32 endframe;
    coord3d pos34;
    s32 forward;
    coord3d pos24;
    f32 angle20;
    s32 flag;
    f32 angledelta;
    f32 sinangle;
    f32 speed2;
    f32 v[3];
    s32 framenum;
    s32 va;
    f32 inv;
    s32 end2;

    modelptr = model;
    root = modelptr->obj->RootNode;
    va = root->Opcode;

    if ((va & 0xff) == 1)
    {
        node = (ModelNode *) root->Data;
        header = (ModelRwData_HeaderRecord *) modelGetNodeRwData(modelptr, root);

        if (header->unk00 == 0)
        {
            jointnum = node->Opcode;
            skeleton = modelptr->obj->Skeleton;
            scale = modelptr->scale * modelptr->anim_translation_scale;
            pos = D_80036254;

            pos34.f[0] = header->unk34.x;
            pos34.f[1] = header->unk34.y;
            pos34.f[2] = header->unk34.z;
            anglecur = header->unk30;
            pos24.f[0] = header->unk24.x;
            pos24.f[1] = header->unk24.y;
            pos24.f[2] = header->unk24.z;
            angle20 = header->unk20;
            flag = header->unk01;

            speed = modelptr->speed;

            if (speed < 0.0f)
            {
                speed = -speed;
            }

            speed2 = modelptr->speed2;

            if (speed2 < 0.0f)
            {
                speed2 = -speed2;
            }

            forward = 0;

            if (framea <= frameb)
            {
                forward = 1;
            }

            if (forward)
            {
                curframe = floorFloatToInt(framea) + 1;
                endframe = floorFloatToInt(frameb);
            }
            else
            {
                curframe = ceilFloatToInt(framea) - 1;
                endframe = ceilFloatToInt(frameb);
            }

            while (1)
            {
                if (forward)
                {
                    if (endframe < curframe)
                    {
                        break;
                    }
                }
                else
                {
                    if (curframe < endframe)
                    {
                        break;
                    }
                }

                framenum = modelConstrainOrWrapAnimFrame(curframe, modelptr->anim, modelptr->endframe);
                modelptr->framea = framenum;

                if (flag)
                {
                    pos34.f[0] = pos24.f[0];
                    pos34.f[1] = pos24.f[1];
                    pos34.f[2] = pos24.f[2];

                    if (header->unk18 == 0.0f)
                    {
                        anglecur = angle20;
                    }
                }
                else
                {
                    angledelta = sub_GAME_7F06D3F4(jointnum, modelptr->gunhand, skeleton, modelptr->anim, framenum, &pos);

                    if (scale != 1.0f)
                    {
                        pos.x *= scale;
                        pos.y *= scale;
                        pos.z *= scale;
                    }

                    if (!forward)
                    {
                        pos.x = -pos.x;
                        pos.z = -pos.z;

                        if (0.0f < angledelta)
                        {
                            angledelta = M_TAU_F - angledelta;
                        }
                    }

                    cosangle = cosf(header->unk14);
                    sinangle = sinf(header->unk14);

                    pos34.f[0] += (pos.x * cosangle) + (pos.z * sinangle);
                    pos34.f[1] = pos.y;
                    pos34.f[2] += (-pos.x * sinangle) + (pos.z * cosangle);

                    if (header->unk18 == 0.0f)
                    {
                        anglecur += angledelta;

                        if (M_TAU_F <= anglecur)
                        {
                            anglecur -= M_TAU_F;
                        }
                    }
                }

                if (forward)
                {
                    curframe += 1;
                }
                else
                {
                    curframe -= 1;
                }

                framenum = modelConstrainOrWrapAnimFrame(curframe, modelptr->anim, modelptr->endframe);
                modelptr->frameb = framenum;

                if (modelptr->frameb != modelptr->framea)
                {
                    angledelta = sub_GAME_7F06D3F4(jointnum, modelptr->gunhand, skeleton, modelptr->anim, framenum, &pos);
                    flag = 1;

                    if (scale != 1.0f)
                    {
                        pos.x *= scale;
                        pos.y *= scale;
                        pos.z *= scale;
                    }

                    if (!forward)
                    {
                        pos.x = -pos.x;
                        pos.z = -pos.z;

                        if (0.0f < angledelta)
                        {
                            angledelta = M_TAU_F - angledelta;
                        }
                    }

                    cosangle = cosf(header->unk30);
                    sinangle = sinf(header->unk30);

                    if (g_ModelAnimMergingEnabled && modelptr->anim2 != NULL)
                    {
                        pos24.f[0] = (pos.z * sinangle) + (pos.x * cosangle);
                        pos24.f[2] = (pos.z * cosangle) + (-pos.x * sinangle);

                        if (0.0f < speed)
                        {
                            f32 t = modelptr->unk84 - (modelptr->playspeed / (speed * modelptr->unk88));

                            if (t < 0.0f)
                            {
                                t = 0.0f;
                            }

                            t = (modelptr->unk84 + t) * 0.5f;

                            v[0] = ((header->unk40.x - header->unk4c.x) * speed2) / speed;
                            v[2] = ((header->unk40.z - header->unk4c.z) * speed2) / speed;

                            pos24.f[0] += (v[0] - pos24.f[0]) * t;
                            pos24.f[2] += (v[2] - pos24.f[2]) * t;
                        }
                        else
                        {
                            pos24.f[0] += (header->unk40.x - header->unk4c.x) * modelptr->unk84;
                            pos24.f[2] += (header->unk40.z - header->unk4c.z) * modelptr->unk84;
                        }

                        pos24.f[0] += pos34.f[0];
                        pos24.f[2] += pos34.f[2];
                        pos24.f[1] = pos.y;
                    }
                    else
                    {
                        pos24.f[0] = (pos34.f[0] + (pos.f[0] * cosangle)) + (pos.f[2] * sinangle);
                        pos24.f[1] = pos.f[1];
                        pos24.f[2] = (pos34.f[2] - (pos.f[0] * sinangle)) + (pos.f[2] * cosangle);
                    }

                    if (0.0f < header->unk5c)
                    {
                        if (0.0f < speed)
                        {
                            inv = 1.0f / speed;

                            if (header->unk5c < inv)
                            {
                                inv = header->unk5c;
                                header->unk5c = 0.0f;
                            }
                            else
                            {
                                header->unk5c -= inv;
                            }

                            angledelta += header->unk58 * inv;

                            if (angledelta < 0.0f)
                            {
                                angledelta += M_TAU_F;
                            }
                            else if (M_TAU_F <= angledelta)
                            {
                                angledelta -= M_TAU_F;
                            }
                        }
                    }

                    if (header->unk18 == 0.0f)
                    {
                        angle20 = anglecur + angledelta;

                        if (M_TAU_F <= angle20)
                        {
                            angle20 -= M_TAU_F;
                        }
                    }
                }
            }

            header->unk34.x = pos34.f[0];
            header->unk34.y = pos34.f[1];
            header->unk34.z = pos34.f[2];
            header->unk30 = anglecur;
            header->unk24.x = pos24.f[0];
            header->unk24.y = pos24.f[1];
            header->unk24.z = pos24.f[2];
            header->unk20 = angle20;

            va = modelptr->framea;
            vb = modelptr->frameb;

            if (vb == va)
            {
                modelptr->unk2c = 0.0f;
                modelptr->animframe1 = va;
            }
            else if (forward)
            {
                modelptr->unk2c = frameb - (f32)endframe;
                modelptr->animframe1 = (f32)va + modelptr->unk2c;
            }
            else
            {
                modelptr->unk2c = (f32)endframe - frameb;
                modelptr->animframe1 = (f32)vb + (1.0f - modelptr->unk2c);
            }

            if (modelptr->anim2 != NULL)
            {
                curframe = floorFloatToInt(frame2a);
                end2 = floorFloatToInt(frame2b);

                if ((forward && (curframe < end2)) || (!forward && (end2 < curframe)))
                {
                    if (header->unk02)
                    {
                        header->unk4c.y = header->unk40.y;
                    }
                    else
                    {
                        header->unk4c.y = header->unk34.y;
                    }

                    modelptr->frame2a = modelConstrainOrWrapAnimFrame(end2, modelptr->anim2, modelptr->unk6c);
                    framenum = modelConstrainOrWrapAnimFrame(end2 + 1, modelptr->anim2, modelptr->unk6c);
                    modelptr->frame2b = framenum;

                    sub_GAME_7F06D3F4(jointnum, modelptr->unk25, skeleton, modelptr->anim2, framenum, &pos);

                    if (scale != 1.0f)
                    {
                        pos.y *= scale;
                    }

                    header->unk40.y = pos.y;
                    header->unk02 = 1;
                }

                if (forward)
                {
                    modelptr->unk5c = frame2b - (f32)end2;
                    modelptr->animframe2 = (f32)modelptr->frame2a + modelptr->unk5c;
                }
                else
                {
                    modelptr->unk5c = 1.0f - (frame2b - (f32)end2);
                    modelptr->animframe2 = (f32)modelptr->frame2b + (1.0f - modelptr->unk5c);
                }
            }
            else
            {
                header->unk02 = 0;
            }
        }
        else
        {
            modelSetAnimFrame2(modelptr, frameb, frame2b);
        }
    }
    else
    {
        modelSetAnimFrame2(modelptr, frameb, frame2b);
    }
}


void modelTickAnim(struct Model *model, s32 numticks, s32 update_chrstuff)
{
    f32 frame;
    f32 frame2;
    f32 animlast;

    frame = model->animframe1;
    frame2 = model->animframe2;

    if (numticks > 0) 
    {
        while (numticks > 0) 
        {
            f32 playspeed;
            f32 speed;
            f32 limit;
            f32 endframe;
            f32 saved_newspeed;
            f32 saved_oldspeed;
            f32 saved_timespeed;
            f32 saved_elapsespeed;
            f32 loopframe;
            
            if (model->unkb0 > 0.0f) 
            {
                model->unkb4 += 1.0f;

                if (model->unkb4 < model->unkb0) 
                {
                    model->playspeed = model->unkac + ((model->animrate - model->unkac) * model->unkb4) / model->unkb0;
                } 
                else 
                {
                    model->unkb0 = 0.0f;
                    model->playspeed = model->animrate;
                }
            }

            playspeed = model->playspeed;

            if (model->unk88 > 0.0f)
            {
                model->unk8c += playspeed;

                if (model->unk8c == 0.0f)
                {
                    model->unk84 = 1.0f;
                    playspeed = model->playspeed;
                } 
                else if (model->unk8c < model->unk88) 
                {
                    model->unk84 = (model->unk88 - model->unk8c) / model->unk88;
                    playspeed = model->playspeed;
                } 
                else 
                {
                    model->unk88 = 0.0f;
                    model->unk84 = 0.0f;
                    model->anim2 = NULL;
                    playspeed = model->playspeed;
                }
            }

            if (model->timespeed > 0.0f) 
            {
                model->elapsespeed += playspeed;

                if (model->elapsespeed < model->timespeed) 
                {
                    model->speed = model->oldspeed
                        + ((model->newspeed - model->oldspeed) * model->elapsespeed)
                        / model->timespeed;
                    playspeed = model->playspeed;
                } 
                else 
                {
                    model->timespeed = 0.0f;
                    playspeed = model->playspeed;
                    model->speed = model->newspeed;
                }
            }

            speed = model->speed;
            frame += playspeed * speed;

            if (model->anim2 != NULL) 
            {
                if (model->unk7c > 0.0f) 
                {
                    model->unk80 += playspeed;

                    if (model->unk80 < model->unk7c) 
                    {
                        model->speed2 = model->unk78 + ((model->unk74 - model->unk78) * model->unk80) / model->unk7c;
                        playspeed = model->playspeed;
                    } 
                    else 
                    {
                        model->unk7c = 0.0f;
                        playspeed = model->playspeed;
                        model->speed2 = model->unk74;
                    }
                }

                if (frame2);

                frame2 += playspeed * model->speed2;
            }

            if (model->animlooping) 
            {
                animlast = model->anim->unk04 - 1;
                endframe = model->endframe;

                if (endframe);

                if (speed >= 0.0f) 
                {
                    limit = animlast;
                    loopframe = model->animloopframe;

                    if (endframe >= 0.0f && endframe < animlast) 
                    {
                        limit = endframe;
                    }
                } 
                else 
                {
                    limit = model->animloopframe;
                    loopframe = animlast;

                    if (endframe >= 0.0f && endframe < animlast) 
                    {
                        loopframe = endframe;
                    }
                }

                if ((speed >= 0.0f && frame >= limit) || (speed < 0.0f && frame <= limit)) 
                {
                    saved_newspeed = model->newspeed;
                    saved_oldspeed = model->oldspeed;
                    saved_timespeed = model->timespeed;
                    saved_elapsespeed = model->elapsespeed;

                    if (update_chrstuff) 
                    {
                        modelSetAnimFrame2WithChrStuff(model, model->animframe1, limit, 0.0f, 0.0f);
                    } 
                    else 
                    {
                        modelSetAnimFrame2(model, limit, 0.0f);
                    }

                    modelSetAnimation(model, model->anim, model->gunhand, loopframe, model->speed, model->animloopmerge);

                    model->animlooping = 1;
                    model->endframe = endframe;
                    model->newspeed = saved_newspeed;
                    model->oldspeed = saved_oldspeed;
                    model->timespeed = saved_timespeed;
                    model->elapsespeed = saved_elapsespeed;

                    frame2 = frame;
                    frame = loopframe + frame - limit;

                    if (model->animflipfunc != 0) 
                    {
                        ((void (*)(void))model->animflipfunc)();
                    }
                }
            }

            numticks--;
        }

        if (update_chrstuff) 
        {
            if (model->anim2 != NULL) 
            {
                modelSetAnimFrame2WithChrStuff(model, model->animframe1, frame, model->animframe2, frame2);
            } 
            else 
            {
                modelSetAnimFrame2WithChrStuff(model, model->animframe1, frame, 0.0f, 0.0f);
            }
        } 
        else 
        {
            if (model->anim2 != NULL) 
            {
                modelSetAnimFrame2(model, frame, frame2);
            }
            else 
            {
                modelSetAnimFrame2(model, frame, 0.0f);
            }
        }
    }
}


/**
 * @brief Model Type 1: 1Cycle No Secondary
 * @param[in,out] renderdata append cycle, CC and RM to display List
 */
void modelApplyRenderModeType1(ModelRenderData *renderdata)
{
    gDPPipeSync(renderdata->gdl++);
    gDPSetCycleType(renderdata->gdl++, G_CYC_1CYCLE);

    if (renderdata->zbufferenabled)
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_AA_ZB_OPA_SURF2, G_RM_AA_ZB_OPA_SURF);
    }
    else
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
    }

    gDPSetCombineMode(renderdata->gdl++, G_CC_MODULATEIA, G_CC_MODULATEIA);
}

/**
 * @brief Model Type 3: GunLighting - Reduced Secondary Commands (guns)
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
 * @param renderdata
 * @param isPrimary
 */
void modelApplyRenderModeType3(ModelRenderData *renderdata, bool isPrimary)
{
    if (renderdata->PropType == PROP_TYPE_VIEWER+1)
    {
        if (isPrimary)
        {
            u8 r, g, b, a;
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

            r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            r = _SHIFTR(renderdata->envcolour.word, 24, 8);
            g = _SHIFTR(renderdata->envcolour.word, 16, 8);
            b = _SHIFTR(renderdata->envcolour.word, 8, 8);
            a = 0xFF;
            gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

            gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, ENVIRONMENT, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_EXPLOSION+1)
    {
        if (isPrimary)
        {
            u8 r, g, b, a;
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

            r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            r = _SHIFTR(renderdata->envcolour.word, 24, 8);
            g = _SHIFTR(renderdata->envcolour.word, 16, 8);
            b = _SHIFTR(renderdata->envcolour.word, 8, 8);
            a = _SHIFTR(renderdata->envcolour.word, 0, 8);
            gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

            gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_SMOKE+1)
    {
        if ((renderdata->envcolour.word & 0xFF) == 0)
        {
            if (isPrimary)
            {
                u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
                u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
                u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
                u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

                gDPPipeSync(renderdata->gdl++);
                gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
                gDPSetFogColor(renderdata->gdl++, r, g, b, a);
                gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, 0xFF);
                gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, (renderdata->envcolour.word >> 8) & 0xFF);

                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, PRIMITIVE);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
                }
            }
            else
            {
                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
                }
            }
        }
        else
        {
            if (isPrimary)
            {
                u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
                u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
                u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
                u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

                gDPPipeSync(renderdata->gdl++);
                gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
                gDPSetFogColor(renderdata->gdl++, r, g, b, a);
                gDPSetEnvColor(renderdata->gdl++, 0, 0, 0, renderdata->envcolour.word & 0xFF);

                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
            else
            {
                gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, (renderdata->envcolour.word >> 8) & 0xFF);
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, SHADE, ENVIRONMENT, TEXEL0, 0, COMBINED, 0, SHADE, 0, 1, 0, PRIMITIVE, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_CHR+1)
    {
        if (isPrimary)
        {
            u8 r = _SHIFTR(renderdata->envcolour.word, 24, 8);
            u8 g = _SHIFTR(renderdata->envcolour.word, 16, 8);
            u8 b = _SHIFTR(renderdata->envcolour.word, 8, 8);
            u8 a = _SHIFTR(renderdata->envcolour.word, 0, 8);

            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_WEAPON+1)
    {
        u8 r, g, b, a;
        if (isPrimary)
        {
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

            r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);

            a = renderdata->envcolour.word & 0xFF;

            if (a < 255)
            {
                gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, a);

                if (renderdata->envcolour.word & 0xFF00)
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, SHADE, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
                else
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
            }
            else
            {
                gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
            }

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
        else
        {
            a = renderdata->envcolour.word & 0xFF;

            if (a < 255)
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
            }
            else
            {
                gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
            }
        }
    }
    else
    {
        if (isPrimary)
        {
            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_XLU_SURF2);
            }
        }
    }
}

/**
 * @brief Model Type 4: Normal Fog/Lighting object
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
 * @param renderdata
 * @param isPrimary Type of DisplayList
 */
void modelApplyRenderModeType4(ModelRenderData *renderdata, bool isPrimary)
{
    if (renderdata->PropType == PROP_TYPE_VIEWER+1)
    {
        u8 r, g, b, a;
        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

        r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
        g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
        b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
        a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        r = _SHIFTR(renderdata->envcolour.word, 24, 8);
        g = _SHIFTR(renderdata->envcolour.word, 16, 8);
        b = _SHIFTR(renderdata->envcolour.word, 8, 8);
        a = 0xFF;
        gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

        gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, ENVIRONMENT, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

        if (isPrimary)
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_EXPLOSION+1)
    {
        u8 r, g, b, a;
        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

        r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
        g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
        b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
        a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        r = _SHIFTR(renderdata->envcolour.word, 24, 8);
        g = _SHIFTR(renderdata->envcolour.word, 16, 8);
        b = _SHIFTR(renderdata->envcolour.word, 8, 8);
        a = _SHIFTR(renderdata->envcolour.word, 0, 8);
        gDPSetEnvColor(renderdata->gdl++, r, g, b, a);

        gDPSetCombineLERP(renderdata->gdl++, TEXEL0, ENVIRONMENT, SHADE_ALPHA, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

        if (renderdata->zbufferenabled)
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
        }
        else
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
        }
    }
    else if (renderdata->PropType == PROP_TYPE_SMOKE+1)
    {
        if ((renderdata->envcolour.word & 0xFF) == 0)
        {
            u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);
            gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, 0xFF);
            gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, ((renderdata->envcolour.word >> 8 ) & 0xFF));

            if (isPrimary)
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, PRIMITIVE);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
                }
            }
            else
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, PRIMITIVE);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
                }
            }
        }
        else
        {
            u8 r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
            u8 g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
            u8 b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
            u8 a = _SHIFTR(renderdata->fogcolour.word, 0, 8);

            gDPPipeSync(renderdata->gdl++);
            gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
            gDPSetFogColor(renderdata->gdl++, r, g, b, a);
            gDPSetEnvColor(renderdata->gdl++, 0, 0, 0, renderdata->envcolour.word & 0xFF);

            if (isPrimary)
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, SHADE, ENVIRONMENT, COMBINED, 0, SHADE, 0, 0, 0, 0, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
            else
            {
                gDPSetPrimColor(renderdata->gdl++, 0, 0, 0, 0, 0, (renderdata->envcolour.word >> 8) & 0xFF);
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, SHADE, ENVIRONMENT, TEXEL0, 0, COMBINED, 0, SHADE, 0, 1, 0, PRIMITIVE, COMBINED);

                if (renderdata->zbufferenabled)
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
                }
                else
                {
                    gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_TEX_EDGE2);
                }
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_CHR+1)
    {
        u8 r = _SHIFTR(renderdata->envcolour.word, 24, 8);
        u8 g = _SHIFTR(renderdata->envcolour.word, 16, 8);
        u8 b = _SHIFTR(renderdata->envcolour.word, 8, 8);
        u8 a = _SHIFTR(renderdata->envcolour.word, 0, 8);

        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

        if (isPrimary)
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
    else if (renderdata->PropType == PROP_TYPE_WEAPON+1)
    {
        u8 r, g, b, a;

        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

        r = _SHIFTR(renderdata->fogcolour.word, 24, 8);
        g = _SHIFTR(renderdata->fogcolour.word, 16, 8);
        b = _SHIFTR(renderdata->fogcolour.word, 8, 8);
        a = _SHIFTR(renderdata->fogcolour.word, 0, 8);
        gDPSetFogColor(renderdata->gdl++, r, g, b, a);

        a = renderdata->envcolour.word & 0xFF;

        if (a < 255)
        {
            gDPSetEnvColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, a);

            if (isPrimary)
            {
                if (renderdata->envcolour.word & 0xFF00) //apply inverse vertex alpha if any
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, SHADE, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
                else
                {
                    gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, 1, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
                }
            }
            else
            {
                gDPSetCombineLERP(renderdata->gdl++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL0, 0, ENVIRONMENT, 0, COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0);
            }
        }
        else
        {
            gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
        }

        if (renderdata->zbufferenabled)
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
        }
        else
        {
            gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
        }
    }
    else
    {
        gDPPipeSync(renderdata->gdl++);
        gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);
        gDPSetFogColor(renderdata->gdl++, 0xFF, 0xFF, 0xFF, 0x00);
        gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);

        if (isPrimary)
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_OPA_SURF2);
            }
        }
        else
        {
            if (renderdata->zbufferenabled)
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
            }
            else
            {
                gDPSetRenderMode(renderdata->gdl++, G_RM_FOG_PRIM_A, G_RM_AA_XLU_SURF2);
            }
        }
    }
}

/**
 * @brief Model Type 2: 2Cycle No Secondary
 * @param[in,out] renderdata append cycle, CC and RM to display List
 */
void modelApplyRenderModeType2(ModelRenderData *renderdata)
{
    gDPPipeSync(renderdata->gdl++);
    gDPSetCycleType(renderdata->gdl++, G_CYC_2CYCLE);

    if (renderdata->zbufferenabled)
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
    }
    else
    {
        gDPSetRenderMode(renderdata->gdl++, G_RM_PASS, G_RM_AA_OPA_SURF2);
    }

    gDPSetCombineMode(renderdata->gdl++, G_CC_TRILERP, G_CC_MODULATEIA2);
}


void modelApplyCullMode(ModelRenderData *renderdata)
{
    if (renderdata->cullmode == CULLMODE_NONE)
    {
        gSPClearGeometryMode(renderdata->gdl++, G_CULL_BOTH);
    }
    else if (renderdata->cullmode == CULLMODE_FRONT)
    {
        gSPSetGeometryMode(renderdata->gdl++, G_CULL_FRONT);
    }
    else if (renderdata->cullmode == CULLMODE_BACK)
    {
        gSPSetGeometryMode(renderdata->gdl++, G_CULL_BACK);
    }
}


void modelRenderNodeGundl(ModelRenderData* renderdata, ModelNode* arg1)
{
    ModelRoData_DisplayListRecord* rodata = &arg1->Data->DisplayList;

    if (renderdata->unk18 == 0)
    {
        if ((renderdata->flags & 1) && rodata->Primary)
        {
            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

            if (renderdata->cullmode)
            {
                modelApplyCullMode(renderdata);
            }

            if (rodata->ModelType == 1)
            {
                modelApplyRenderModeType1(renderdata);
            }
            else if (rodata->ModelType == 3)
            {
                modelApplyRenderModeType3(renderdata, 1);
            }
            else if (rodata->ModelType == 4)
            {
                modelApplyRenderModeType4(renderdata, 1);
            }
            else if (rodata->ModelType == 2)
            {
                modelApplyRenderModeType2(renderdata);
            }

            gSPDisplayList(renderdata->gdl++, rodata->Primary);

            if ((rodata->ModelType == 3) && rodata->Secondary)
            {
                modelApplyRenderModeType3(renderdata, 0);
                gSPDisplayList(renderdata->gdl++, rodata->Secondary);
            }
        }

        if ((renderdata->flags & 2) && rodata->Primary && (rodata->ModelType == 4) && rodata->Secondary)
        {
            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

            if (renderdata->cullmode)
            {
                modelApplyCullMode(renderdata);
            }

            modelApplyRenderModeType4(renderdata, 0);
            gSPDisplayList(renderdata->gdl++, rodata->Secondary);
        }
    }
}

/*

A1 is primary = 1, secondary = 0
Inside the T8 or whatever temporary register indicates gun or not gun (0 = gun, or UseZ = 1), for different render mode

Bool UseZ //guns = false
Bool


Model Type 0: NoSetup.
    type 0 Has No DL Setup and will use whaterver is currently set.

Model Type 1: 1Cycle No Sec
    E700000000000000 pipesync()
    BA00140200000000 CycleType(1c)
    if UseZ
      B900031D00552078 SetRendermode(AA_ZB_OPA_1) //cin ain cmem amem
    else
      B900031D00552048 SetRendermode(AA_OPA_1) //cin ain cmem amem
    end if
    FC121824FF33FFFF SetCombine(MODULATERGBA)
    No Secondary

Model Type 2: 2Cycle No Sec
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    if UseZ
      B900031D0C192078 SetRendermode(AA_ZB_OPA_2) // cin 0 cin 1 //colour only
    else
      B900031D0C192048 SetRendermode(AA_OPA_2) // cin 0 cin 1 //colour only
    end if
    FC26A0041F1093FF SetCombine(TRILERP, MODULATERGBA)
    No Secondary
Model Type 3: GunLighting - Reduced Secondary Commands (guns)
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
    Primary
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    F800000000000026 SetFogColor(0,0,0,38)
    if dltype = full
      if guard
        FB0000005A0000FF SetEnvColor(90,0,0,255)
        FC1598045FFEDBF8 SetCombine(((Texel0-Env)*ShadeA+Env)
                         ((Texel0-Env)*Shade+Env),
                         MODULATERGB_DECALA)
      else if prop
        FB000000FFFFFFFF SetEnvColor(255,255,255,255)
        FA00000000000000 SetPrimColor(0,0,0,0)
        FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
      endif
    else
      FC26A0041F1093FF SetCombine(TRILERP, MODULATERGBA)
    endif
    if UseZ
      B900031DC4112078 SetRendermode(AA_ZB_OPA_StanFOG_2)
    else
      B900031DC4112048 SetRendermode(AA_OPA_StanFOG_2) //acvg
    endif

    Secondary
    if UseZ
      B900031DC41049D8 SetRendermode(AA_Zcmp_XLU_StanFOG_2)
    else
      B900031DC41041C8 SetRendermode(AA_OPA_StanFOG_2)//FcBl ClrOnCvg
    endif

Model Type 4: Normal Fog/Lighting object
    This Type Uses Vertex Alpha for Secondary Surfaces and uses the FOG Alpha value for applying Fog/"Lighting".
    Primary
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    F800000000000026 SetFogColor(0,0,0,38)
    if dltype = full
      if guard
        FB0000005A0000FF SetEnvColor(90,0,0,255)
        FC1598045FFEDBF8 SetCombine(((Texel0-Env)*ShadeA+Env)
                         ((Texel0-Env)*Shade+Env),
                         MODULATERGB_DECALA)
      else if prop
        FB000000FFFFFFFF SetEnvColor(255,255,255,255)
        FA00000000000000 SetPrimColor(0,0,0,0)
        FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
      endif
    else
      FC26A0041F1093FF SetCombine(TRILERP, MODULATERGBA)
    endif
    if UseZ
      B900031DC4112078 SetRendermode(AA_ZB_OPA_StanFOG_2)
    else
      B900031DC4112048 SetRendermode(AA_OPA_StanFOG_2) //acvg
    endif

    Secondary
    E700000000000000 pipesync()
    BA00140200100000 CycleType(2c)
    F800000000000026 SetFogColor(0,0,0,38)
    if dltype = full
      if guard
        FB0000005A0000FF SetEnvColor(90,0,0,255)
        FC1598045FFEDBF8 SetCombine(((Texel0-Env)*ShadeA+Env)
                         ((Texel0-Env)*Shade+Env),
                         MODULATERGB_DECALA)
      else if prop
        FB000000FFFFFFFF SetEnvColor(255,255,255,255)
        FA00000000000000 SetPrimColor(0,0,0,0)
        FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
      endif
    else
      FA00000000000000 SetPrimColor(0,0,0,0)
      FC26A0041F1093FB SetCombine(TRILERP, MODULATERGB_ADDPRIM_A)
    endif
    if UseZ
      B900031DC41049D8 SetRendermode(AA_Zcmp_XLU_StanFOG_2)
    else
      B900031DC41041C8 SetRendermode(AA_OPA_StanFOG_2)//FcBl ClrOnCvg
    endif
*/

/**
* 7F072A0C
* DisplayList Setups Depend on Object Type, Prop Guard or Gun.
These are applied to each part of an object at runtime and can be overridden. loading the next part will use these values once more.
GeometryMode is not in setup and is persistent accross parts.
*/
void modelRenderNodeDl(ModelRenderData *renderdata, Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;

    if (!renderdata->unk18)
    {
        if (renderdata->flags & 1)
        {
            union ModelRwData *rwdata = modelGetNodeRwData(model, node);

            if (rwdata->DisplayListCollisions.gdl)
            {
                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->DisplayListCollisions.BaseAddr));

                if (renderdata->cullmode)
                {
                    modelApplyCullMode(renderdata);
                }

                if (rodata->DisplayListCollisions.ModelType == 1)
                {
                    modelApplyRenderModeType1(renderdata);
                }
                else if (rodata->DisplayListCollisions.ModelType == 3)
                {
                    modelApplyRenderModeType3(renderdata, TRUE);
                }
                else if (rodata->DisplayListCollisions.ModelType == 4)
                {
                    modelApplyRenderModeType4(renderdata, TRUE);
                }
                else if (rodata->DisplayListCollisions.ModelType == 2)
                {
                    modelApplyRenderModeType2(renderdata);
                }

                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(rwdata->DisplayListCollisions.Vertices));

                gSPDisplayList(renderdata->gdl++, rwdata->DisplayListCollisions.gdl);

                if (rodata->DisplayListCollisions.ModelType == 3 && rodata->DisplayListCollisions.Secondary)
                {
                    modelApplyRenderModeType3(renderdata, FALSE);
                    gSPDisplayList(renderdata->gdl++, rodata->DisplayListCollisions.Secondary);
                }
            }
        }

        if (renderdata->flags & 2)
        {
            union ModelRwData *rwdata = modelGetNodeRwData(model, node);

            if (rwdata->DisplayListCollisions.gdl && rodata->DisplayListCollisions.ModelType == 4 && rodata->DisplayListCollisions.Secondary)
            {
                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->DisplayListCollisions.BaseAddr));

                if (renderdata->cullmode)
                {
                    modelApplyCullMode(renderdata);
                }

                gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(rwdata->DisplayListCollisions.Vertices));

                modelApplyRenderModeType4(renderdata, FALSE);

                gSPDisplayList(renderdata->gdl++, rodata->DisplayListCollisions.Secondary);
            }
        }
    }
}


void sub_GAME_7F072C10(ModelRenderData *param_1, struct Model *param_2, struct ModelNode *param_3)
{
    return;
}


/**
 * Star gunfire is a muzzle flash in a first person perspective, where the
 * muzzle flash has 3 or 4 "arms" that flare out from the main body.
 *
 * This function reads vertices from the model definition, tweaks them randomly,
 * writes them to a newly allocated vertices table and queues the node's
 * displaylist to the renderdata's DL.
 */
void dorottex(ModelRenderData *renderdata, ModelNode *node)
{
    if (renderdata->unk18 == 0 && (renderdata->flags & 2))
    {

        ModelRoData_DisplayListPrimaryRecord *rodata = &node->Data->DisplayListPrimary;
        s32 i;

        if (rodata->Primary)
        {
            Vertex *src;
            Vertex *dst;

            src = (Vertex *) rodata->Vertices;

#ifndef VERSION_EU
            if (vtxallocator != NULL)
            {
            }
            else
            {
                osSyncPrintf("dorottex: no vtx allocator!\n");
                return_null();
            }
#endif
            dst = vtxallocator(rodata->numVertices * 4);

            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_VTX, osVirtualToPhysical(dst));
            gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

            gDPSetFogColor(renderdata->gdl++, 0x00, 0x00, 0x00, 0x00);
            gSPDisplayList(renderdata->gdl++, rodata->Primary);

            for (i = 0; i < rodata->numVertices; i++)
            {
                u16 rand1 = (randomGetNext() << 10) & 0xffff;
                s32 s4 = ((coss(rand1) << 5) * 181) >> 18;
                s32 s3 = ((sins(rand1) << 5) * 181) >> 18;
                s32 s1 = (u32)randomGetNext() >> 31;
                s32 mult = 0x10000 - (randomGetNext() & 0x3fff);
                s32 corner1 = 0x200 + s3;
                s32 corner2 = 0x200 - s3;
                s32 corner3 = 0x200 - s4;
                s32 corner4 = 0x200 + s4;

                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = src[3];

                dst[0].s = corner3;
                dst[0].t = corner2;
                dst[0].coord.x = (src[(s1 + 0) % 4].coord.x * mult) >> 16;
                dst[0].coord.y = (src[(s1 + 0) % 4].coord.y * mult) >> 16;
                dst[0].coord.z = (src[(s1 + 0) % 4].coord.z * mult) >> 16;

                dst[1].s = corner1;
                dst[1].t = corner3;
                dst[1].coord.x = (src[(s1 + 1) % 4].coord.x * mult) >> 16;
                dst[1].coord.y = (src[(s1 + 1) % 4].coord.y * mult) >> 16;
                dst[1].coord.z = (src[(s1 + 1) % 4].coord.z * mult) >> 16;

                dst[2].s = corner4;
                dst[2].t = corner1;
                dst[2].coord.x = (src[(s1 + 2) % 4].coord.x * mult) >> 16;
                dst[2].coord.y = (src[(s1 + 2) % 4].coord.y * mult) >> 16;
                dst[2].coord.z = (src[(s1 + 2) % 4].coord.z * mult) >> 16;

                dst[3].s = corner2;
                dst[3].t = corner4;
                dst[3].coord.x = (src[(s1 + 3) % 4].coord.x * mult) >> 16;
                dst[3].coord.y = (src[(s1 + 3) % 4].coord.y * mult) >> 16;
                dst[3].coord.z = (src[(s1 + 3) % 4].coord.z * mult) >> 16;

                src += 4;
                dst += 4;
            }
        }
    }
}


void sub_GAME_7F073038(ModelRenderData *renderdata, struct sImageTableEntry *tconfig, s32 arg2)
{
    texSelect(&renderdata->gdl, tconfig, arg2, renderdata->zbufferenabled, 2);
}


void sub_GAME_7F07306C(s32 param_1,struct Model *param_2,struct ModelNode *param_3)
{
    return;
}


void dotube(ModelRenderData* renderdata, Model* model, ModelNode* node)
{
    s32 rw_index_sel;
    s32 rw_index_sel2;
    s32 c_entry_count;
    struct ModelRoData_Op07Record *rodata2;
    s32 c_entry2_count;
    s32 c_entry_index;
    s32 renderpos_index;
    ModelNode *node_from_07;
    struct ModelRoData_Child *c_entry2;
    Vertex *vtx2;
    u8 *entry2_04;
    s32 unused1;
    bool swap_order;
    struct ModelRoData_Op07Record *rodata;
    struct ModelRoData_Child *c_entry;
    struct ModelRwData_Op07Record *rwdata;
    Vertex *vtx_10;
    Vertex *vtx_10_2;
    RenderPosView *render_pos2;
    RenderPosView *render_pos;
    s32 rw_index2;
    s32 rw_index;
    Vertex *vtx1;
    struct ModelRwData_Op07Record *rwdata2;
    u8 *entry_04;
    s32 unused2;
    s32 renderpos_index2;
    s32 unused3;

    rodata = &node->Data->Op07;
    rwdata = &modelGetNodeRwData(model, node)->Op07;

    if (rodata->unk00 != NULL)
    {
        node_from_07 = rodata->unk00;
    }
    else
    {
        node_from_07 = rodata->unk04;
    }

    rodata2 = &node_from_07->Data->Op07;
    rwdata2 = &modelGetNodeRwData(model, node_from_07)->Op07;
    swap_order = 1;

    if (renderdata->flags & 1)
    {
        renderpos_index2 = modelFindNodeMtxIndex(node, 0);
        render_pos2 = &model->render_pos[renderpos_index2];
        rw_index = rwdata->index;
        rw_index2 = rwdata2->index;
        c_entry = &rodata->Children[rw_index];

        if (rodata->unk00 != NULL)
        {
            rw_index_sel = rw_index2;
            rw_index_sel2 = rw_index;
            renderpos_index = modelFindNodeMtxIndex(node, 0x200);
        }
        else
        {
            rw_index_sel = rw_index;
            rw_index_sel2 = rw_index2;
            renderpos_index = modelFindNodeMtxIndex(rodata->unk04, 0x200);
        }

        render_pos = &model->render_pos[renderpos_index];

        c_entry_index = ((rw_index_sel2 - rw_index_sel) + rodata->NumChildren) % rodata->NumChildren;

        if ((c_entry_index >= 2) && (c_entry_index < 7))
        {
            if (c_entry_index < 4)
            {
                c_entry_index = ((c_entry_index / 2) + rw_index_sel + rodata->NumChildren) % rodata->NumChildren;
            }
            else
            {
                c_entry_index = ((rw_index_sel - ((8 - c_entry_index) / 2)) + rodata->NumChildren) % rodata->NumChildren;
                swap_order = 0;
            }
        }
        else if ((c_entry_index >= 0xA) && (c_entry_index < 0xF))
        {
            if (c_entry_index >= 0xD)
            {
                c_entry_index = ((rw_index_sel - ((0x10 - c_entry_index) / 2)) + rodata->NumChildren) % rodata->NumChildren;
            }
            else
            {
                c_entry_index = (((c_entry_index - 8) / 2) + rw_index_sel + rodata->NumChildren) % rodata->NumChildren;
                swap_order = 0;
            }
        }
        else
        {
            if ((c_entry_index >= 7) && (c_entry_index < 0xA))
            {
                swap_order = 0;
            }
            c_entry_index = rw_index_sel;
        }

        entry_04 = c_entry->unk04;

        gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

        for (c_entry_count = c_entry->NumEntries; c_entry_count > 0; c_entry_count--)
        {
            switch (*entry_04)
            {
                case MODELNODE_CHILD_VTX:
                    {
                        struct ModelRoData_Child_Vtx* child_vtx = ((struct ModelRoData_Child_Vtx*)entry_04);
#if defined(LEFTOVERDEBUG)
                        if (vtxallocator == NULL)
                        {
                            osSyncPrintf("dotube: no vtx allocator!\n");
                            return_null();
                        }
#endif
                        vtx1 = vtxallocator(2);
                        vtx2 = &vtx1[1];

                        *vtx1 = rodata->Vertices[child_vtx->VtxIndex];
                        *vtx2 = rodata->Vertices[child_vtx->VtxIndex+1];

                        if (rodata->unk04 != NULL)
                        {
                            c_entry2 = &rodata->Children[c_entry_index];
                            entry2_04 = c_entry2->unk04;

                            for (c_entry2_count = c_entry2->NumEntries; c_entry2_count > 0; c_entry2_count--)
                            {
                                struct ModelRoData_Child_Vtx* entry2_04_child = ((struct ModelRoData_Child_Vtx*)entry2_04);
                                if (entry2_04_child->Type == (u8) 1) {
                                    vtx_10   = &rodata->Vertices[entry2_04_child->VtxIndex];
                                    vtx_10_2 = vtx_10+1;

                                    vtx1->coord.AsArray[0] = vtx_10->coord.AsArray[0];
                                    vtx1->coord.AsArray[1] = vtx_10->coord.AsArray[1];
                                    vtx1->coord.AsArray[2] = vtx_10->coord.AsArray[2];

                                    vtx2->coord.AsArray[0] = vtx_10_2->coord.AsArray[0];
                                    vtx2->coord.AsArray[1] = vtx_10_2->coord.AsArray[1];
                                    vtx2->coord.AsArray[2] = vtx_10_2->coord.AsArray[2];
                                    break;
                                }

                                switch (*entry2_04)
                                {
                                    case MODELNODE_CHILD_VTX:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Vtx);
                                        break;
                                    case MODELNODE_CHILD_IMAGE:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Image);
                                        break;
                                    case MODELNODE_CHILD_TRI:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Tri);
                                        break;
                                }
                            }

                        }
                        else
                        {
                            c_entry2 = &rodata2->Children[c_entry_index];
                            entry2_04 = c_entry2->unk04;

                            for (c_entry2_count = c_entry2->NumEntries; c_entry2_count > 0; c_entry2_count--)
                            {
                                struct ModelRoData_Child_Vtx* entry2_04_child = ((struct ModelRoData_Child_Vtx*)entry2_04);
                                if (entry2_04_child->Type == (u8) 1)
                                {
                                    vtx_10   = &rodata2->Vertices[entry2_04_child->VtxIndex];
                                    vtx_10_2 = vtx_10 + 1;
                                    if (swap_order != 0)
                                    {
#if defined(LEFTOVERDEBUG)
                                        if (vtx_10->coord.AsArray);
#endif
                                        vtx1->coord.AsArray[0] = vtx_10_2->coord.AsArray[0];
                                        vtx1->coord.AsArray[1] = vtx_10_2->coord.AsArray[1];
                                        vtx1->coord.AsArray[2] = vtx_10_2->coord.AsArray[2];

                                        vtx2->coord.AsArray[0] = vtx_10->coord.AsArray[0];
                                        vtx2->coord.AsArray[1] = vtx_10->coord.AsArray[1];
                                        vtx2->coord.AsArray[2] = vtx_10->coord.AsArray[2];
                                    }
                                    else
                                    {
                                        vtx1->coord.AsArray[0] = vtx_10->coord.AsArray[0];
                                        vtx1->coord.AsArray[1] = vtx_10->coord.AsArray[1];
                                        vtx1->coord.AsArray[2] = vtx_10->coord.AsArray[2];

                                        vtx2->coord.AsArray[0] = vtx_10_2->coord.AsArray[0];
                                        vtx2->coord.AsArray[1] = vtx_10_2->coord.AsArray[1];
                                        vtx2->coord.AsArray[2] = vtx_10_2->coord.AsArray[2];
                                    }
                                    break;
                                }

                                switch (*entry2_04)
                                {
                                    case MODELNODE_CHILD_VTX:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Vtx);
                                        break;

                                    case MODELNODE_CHILD_IMAGE:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Image);
                                        break;

                                    case MODELNODE_CHILD_TRI:
                                        entry2_04 += sizeof(struct ModelRoData_Child_Tri);
                                        break;
                                }
                            }
                        }

                        gSPMatrix(renderdata->gdl++, osVirtualToPhysical(render_pos), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        gSPVertex(renderdata->gdl++, osVirtualToPhysical(vtx1), 2, 0);
                        gSPMatrix(renderdata->gdl++, osVirtualToPhysical(render_pos2), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        gSPVertex(renderdata->gdl++, osVirtualToPhysical(&rodata->Vertices[child_vtx->VtxIndex] + 2), (u32)(child_vtx->unk01 - 2), 2);

                        entry_04 += sizeof(struct ModelRoData_Child_Vtx);

                        break;
                    }

                case MODELNODE_CHILD_IMAGE:
                    {
                        struct ModelRoData_Child_Image* child_image = (struct ModelRoData_Child_Image*)entry_04;
                        if (child_image->ImageIndex != 0xFF)
                        {
                            sub_GAME_7F073038(renderdata, &rodata->Images[child_image->ImageIndex], 1);
                            entry_04 += sizeof(struct ModelRoData_Child_Image);
                        }
                        else
                        {
                            sub_GAME_7F073038(renderdata, NULL, 1);
                            entry_04 += sizeof(struct ModelRoData_Child_Image);
                        }
                        break;
                    }

                case MODELNODE_CHILD_TRI:
                    {
                        struct ModelRoData_Child_Tri* child_tri = (struct ModelRoData_Child_Tri*)entry_04;
                        gSP1Triangle(renderdata->gdl++, child_tri->VtxIndex1, child_tri->VtxIndex2, child_tri->VtxIndex3, 0);
                        entry_04 += sizeof(struct ModelRoData_Child_Tri);
                        break;
                    }
            }
        }
    }
}


void sub_GAME_7F0737EC(s32 param_1,struct Model *param_2, struct ModelNode *param_3)
{
    return;
}


void sub_GAME_7F0737FC(s32 param_1,struct Model *param_2,struct ModelNode *param_3)
{
    return;
}


// PD: modelRenderNodeChrGunfire
void dogfnegx(ModelRenderData *renderdata, Model *model, ModelNode *node)
{
    u32 unused[3];
    f32 negspc0;
    ModelRoData_GunfireRecord *rodata = &node->Data->Gunfire;
    union ModelRwData *rwdata = modelGetNodeRwData(model, node);
    sImageTableEntry *tconfig;
    f32 spf0;
    f32 spec;
    coord3d spe0;
    f32 spdc;
    f32 spd8;
    f32 spd4;
    f32 spd0;
    f32 spcc;
    f32 spc8;
    f32 spc4;
    f32 spc0;
    f32 spbc;
    f32 negspcc;
    f32 negspc8;
    f32 scale;
    Mtxf *mtx;
    f32 tmp;
    coord3d sp9c;
    coord3d sp90;
    Vertex vtxtemplate = D_800363E0;
    Vertex *vertices;
    f32 distance;

    if ((renderdata->flags & 2) && rwdata->Gunfire.visible)
    {
        s32 index = modelFindNodeMtxIndex(node, 0);
        mtx = &model->render_pos[index].pos;

        spe0.x = -(rodata->Offset.f[0] * mtx->m[0][0] + rodata->Offset.f[1] * mtx->m[1][0] + rodata->Offset.f[2] * mtx->m[2][0] + mtx->m[3][0]);
        spe0.y = -(rodata->Offset.f[0] * mtx->m[0][1] + rodata->Offset.f[1] * mtx->m[1][1] + rodata->Offset.f[2] * mtx->m[2][1] + mtx->m[3][1]);
        spe0.z = -(rodata->Offset.f[0] * mtx->m[0][2] + rodata->Offset.f[1] * mtx->m[1][2] + rodata->Offset.f[2] * mtx->m[2][2] + mtx->m[3][2]);

        distance = sqrtf(spe0.f[0] * spe0.f[0] + spe0.f[1] * spe0.f[1] + spe0.f[2] * spe0.f[2]);

        if (distance > 0)
        {
            f32 tmp = 1 / (model->scale * distance);
            spe0.f[0] *= tmp;
            spe0.f[1] *= tmp;
            spe0.f[2] *= tmp;
        }
        else
        {
            spe0.f[0] = 0;
            spe0.f[1] = 0;
            spe0.f[2] = 1 / model->scale;
        }

        spec = acosf(spe0.f[0] * mtx->m[1][0] + spe0.f[1] * mtx->m[1][1] + spe0.f[2] * mtx->m[1][2]);
        spf0 = acosf(-(spe0.f[0] * mtx->m[2][0] + spe0.f[1] * mtx->m[2][1] + spe0.f[2] * mtx->m[2][2]) / sinf(spec));

        tmp = -(spe0.f[0] * mtx->m[0][0] + spe0.f[1] * mtx->m[0][1] + spe0.f[2] * mtx->m[0][2]);

        if (tmp < 0)
        {
            spf0 = M_TAU_F - spf0;
        }

        spdc = cosf(spf0);
        spd8 = sinf(spf0);
        spd4 = cosf(spec);
        spd0 = sinf(spec);

        scale = 0.75f + (randomGetNext() % 128) * (1.0f / 256.0f); // 0.75 to 1.25

        sp9c.f[0] = rodata->Size.f[0] * scale;
        sp9c.f[1] = rodata->Size.f[1] * scale;
        sp9c.f[2] = rodata->Size.f[2] * scale;

        spcc = sp9c.f[0] * spdc * 0.5f;
        spc8 = sp9c.f[2] * spd8 * 0.5f;
        spc4 = sp9c.f[1] * spd0 * 0.5f;

        spc0 = sp9c.f[0] * spd4 * spd8 * 0.5f;
        spbc = sp9c.f[2] * spd4 * spdc * 0.5f;

        negspcc = -spcc;
        negspc8 = -spc8;
        negspc0 = -spc0;

        sp90.f[0] = rodata->Offset.f[0] - sp9c.f[0] * 0.5f;
        sp90.f[1] = rodata->Offset.f[1];
        sp90.f[2] = rodata->Offset.f[2];

#if defined (LEFTOVERDEBUG)
        if (vtxallocator == NULL) {
            osSyncPrintf("dogfnegx: no vtx allocator!\n");
            return_null();
        }
#endif

        vertices = vtxallocator(4);

        vertices[0] = vtxtemplate;
        vertices[1] = vtxtemplate;
        vertices[2] = vtxtemplate;
        vertices[3] = vtxtemplate;

        vertices[0].coord.x = sp90.f[0] + negspcc + negspc0;
        vertices[0].coord.y = sp90.f[1] - spc4;
        vertices[0].coord.z = sp90.f[2] - negspc8 + -spbc;
        vertices[1].coord.x = sp90.f[0] + negspcc - negspc0;
        vertices[1].coord.y = sp90.f[1] + spc4;
        vertices[1].coord.z = sp90.f[2] - negspc8 - -spbc;
        vertices[2].coord.x = sp90.f[0] - negspcc - negspc0;
        vertices[2].coord.y = sp90.f[1] + spc4;
        vertices[2].coord.z = sp90.f[2] + negspc8 - -spbc;
        vertices[3].coord.x = sp90.f[0] - negspcc + negspc0;
        vertices[3].coord.y = sp90.f[1] - spc4;
        vertices[3].coord.z = sp90.f[2] + negspc8 + -spbc;

        gSPSegment(renderdata->gdl++, SPSEGMENT_MODEL_COL1, osVirtualToPhysical(rodata->BaseAddr));

        if (rodata->Image)
        {
            s32 centre;
            u16 sp62;
            s32 sp5c;
            s32 sp58;

            tconfig = rodata->Image;

            sp62 = (randomGetNext() * 1024) & 0xffff;
            sp5c = (coss(sp62) * tconfig->width * 0xb5) >> 18;
            sp58 = (sins(sp62) * tconfig->width * 0xb5) >> 18;

            centre = tconfig->width << 4;

            vertices[0].s = centre - sp5c;
            vertices[0].t = centre - sp58;
            vertices[1].s = centre + sp58;
            vertices[1].t = centre - sp5c;
            vertices[2].s = centre + sp5c;
            vertices[2].t = centre + sp58;
            vertices[3].s = centre - sp58;
            vertices[3].t = centre + sp5c;

            sub_GAME_7F073038(renderdata, tconfig, 4);
        }
        else
        {
            sub_GAME_7F073038(renderdata, NULL, 1);
        }

        gSPSetGeometryMode(renderdata->gdl++, G_CULL_BACK);
        gSPMatrix(renderdata->gdl++, osVirtualToPhysical(mtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(renderdata->gdl++, osVirtualToPhysical(vertices), 4, 0);
        if (1);
        gSP2Triangles(renderdata->gdl++, 0, 1, 2, 0, 2, 3, 0, 0);
    }
}


void sub_GAME_7F073FC8(s32 arg0)
{
    D_800363F0 = arg0;
}



#ifdef NONMATCHING
void doshadow(void)
{
    s32   sp70;
    void *sp6C;
    s8    sp67;
    ? sp58;
    void *sp54;
    s16   sp52;
    f32   sp48;
    f32   sp44;
    f32   sp40;
    void *sp38;
    void *sp2C;
    void *sp28;
    f32   temp_f0;
    f32   temp_f12;
    f32   temp_f14;
    f32   temp_f2;
    f32   var_f12;
    f32   var_f14;
    s16   var_v1;
    s32   temp_v0_2;
    void *(*temp_v0_3)(f32, f32, ?, void *);
    void *temp_a2;
    void *temp_v0;
    void *temp_v0_4;
    void *temp_v1;
    void *temp_v1_2;
    void *temp_v1_3;
    void *temp_v1_4;
    void *temp_v1_5;
    void *temp_v1_6;

    if (D_800363F0 > 0)
    {
        temp_a2   = arg2->unk4;
        sp58.unk0 = D_800363F8.unk0;
        sp58.unk4 = D_800363F8.unk4;
        sp58.unkC = D_800363F8.unkC;
        sp58.unk8 = D_800363F8.unk8;
        sp6C      = temp_a2;
        temp_v0   = modelGetNodeRwData(arg1, temp_a2->unk14, temp_a2, arg1);
        temp_f12  = temp_a2->unk8;
        temp_f14  = temp_a2->unkC;
        temp_f2   = temp_v0->unkC - temp_v0->unk4;
        if (arg0->unk8 & 2)
        {
            temp_v0_2 = arg0->unk30;
            if ((temp_v0_2 == 3) || (temp_v0_2 == 8))
            {
                sp67 = ((arg0->unk34 & 0xFF) * D_800363F0) / 255;
            }
            else
            {
                sp67 = D_800363F0;
            }
            sp6C = temp_a2;
            sp40 = temp_f2;
            sp48 = temp_f12;
            sp44 = temp_f14;
            sp70 = arg1->unkC + (modelFindNodeMtxIndex(temp_f12, temp_f14, arg2, 0, temp_a2, arg1) << 6);
            if (arg0->unk4 != 0)
            {
                var_v1 = (2.0f - temp_f2) / arg1->unk14;
            }
            else
            {
                var_v1 = -temp_f2 / arg1->unk14;
            }
            if (temp_f2 < 50.0f)
            {
                var_f12 = temp_f12 * 1.25f;
                var_f14 = temp_f14 * 1.25f;
            }
            else if (temp_f2 > 300.0f)
            {
                var_f14 = 0.0f;
                var_f12 = 0.0f;
            }
            else
            {
                temp_f0 = (300.0f - temp_f2) / 200.0f;
                var_f12 = temp_f12 * temp_f0;
                var_f14 = temp_f14 * temp_f0;
            }
            temp_v0_3 = vtxallocator;
            if (temp_v0_3 == NULL)
            {
                sp52 = var_v1;
                sp6C = temp_a2;
                sp48 = var_f12;
                sp44 = var_f14;
                osSyncPrintf("doshadow: no vtx allocator!\n", temp_a2, arg1);
                return_null();
            }
            sp52             = var_v1;
            sp6C             = temp_a2;
            sp48             = var_f12;
            sp44             = var_f14;
            temp_v0_4        = temp_v0_3(var_f12, var_f14, 4, temp_a2);
            temp_v0_4->unk0  = sp58.unk0;
            temp_v0_4->unk4  = sp58.unk4;
            temp_v0_4->unk8  = sp58.unk8;
            temp_v0_4->unkC  = sp58.unkC;
            temp_v0_4->unk10 = sp58.unk0;
            temp_v0_4->unk14 = sp58.unk4;
            temp_v0_4->unk18 = sp58.unk8;
            temp_v0_4->unk1C = sp58.unkC;
            temp_v0_4->unk20 = sp58.unk0;
            temp_v0_4->unk24 = sp58.unk4;
            temp_v0_4->animframe1 = sp58.unk8;
            temp_v0_4->unk2C = sp58.unkC;
            temp_v0_4->unk30 = sp58.unk0;
            temp_v0_4->unk34 = sp58.unk4;
            temp_v0_4->unk38 = sp58.unk8;
            temp_v0_4->unk3C = sp58.unkC;
            temp_v0_4->unk2  = var_v1;
            temp_v0_4->unk0  = temp_a2->unk0 - var_f12;
            temp_v0_4->unk4  = temp_a2->unk4 - var_f14;
            temp_v0_4->unk12 = var_v1;
            temp_v0_4->unk10 = temp_a2->unk0 - var_f12;
            temp_v0_4->unk14 = temp_a2->unk4 + var_f14;
            temp_v0_4->unk22 = var_v1;
            temp_v0_4->unk20 = temp_a2->unk0 + var_f12;
            temp_v0_4->unk24 = temp_a2->unk4 + var_f14;
            temp_v0_4->unk32 = var_v1;
            temp_v0_4->unk30 = temp_a2->unk0 + var_f12;
            temp_v0_4->unk34 = temp_a2->unk4 - var_f14;
            temp_v1          = arg0->unkC;
            arg0->unkC       = temp_v1 + 8;
            temp_v1->unk0    = 0xBC001406;
            sp54             = temp_v0_4;
            sp6C             = temp_a2;
            sp38             = temp_v1;
            sp38->unk4       = osVirtualToPhysical(var_f12, var_f14, temp_a2->unk1C, temp_a2);
            temp_v1_2        = temp_a2->unk10;
            if (temp_v1_2 != NULL)
            {
                sp54->unk8  = 0;
                sp54->unkA  = 0;
                sp54->unk1A = 0;
                sp54->unk18 = (temp_v1_2->unk4 << 5) - 1;
                sp54->animframe1 = (temp_v1_2->unk4 << 5) - 1;
                sp54->unk38 = 0;
                sp54->unk2A = (temp_v1_2->unk5 << 5) - 1;
                sp54->unk3A = (temp_v1_2->unk5 << 5) - 1;
                sub_GAME_7F073038(arg0, temp_v1_2, 4, sp54);
            }
            else
            {
                sub_GAME_7F073038(arg0, NULL, 1, sp54);
            }
            temp_v1_3       = arg0->unkC;
            arg0->unkC      = temp_v1_3 + 8;
            temp_v1_3->unk0 = 0xB7000000;
            temp_v1_3->unk4 = 0x2000;
            temp_v1_4       = arg0->unkC;
            arg0->unkC      = temp_v1_4 + 8;
            temp_v1_4->unk0 = 0x01020040;
            sp2C            = temp_v1_4;
            sp2C->unk4      = osVirtualToPhysical((bitwise f32)sp70);
            temp_v1_5       = arg0->unkC;
            arg0->unkC      = temp_v1_5 + 8;
            temp_v1_5->unk0 = 0x04300040;
            sp28            = temp_v1_5;
            sp28->unk4      = osVirtualToPhysical((bitwise f32)sp54, (bitwise f32)sp2C);
            temp_v1_6       = arg0->unkC;
            arg0->unkC      = temp_v1_6 + 8;
            temp_v1_6->unk4 = 0x3210;
            temp_v1_6->unk0 = 0xB1000002;
        }
    }
}
#else
    #ifndef VERSION_EU
//D:80054A94
const char aDoshadowNoVtxAllocator[] = "doshadow: no vtx allocator!\n";
GLOBAL_ASM(
.text
glabel doshadow
/* 0A8B04 7F073FD4 3C038003 */  lui   $v1, %hi(D_800363F0)
/* 0A8B08 7F073FD8 246363F0 */  addiu $v1, %lo(D_800363F0) # addiu $v1, $v1, 0x63f0
/* 0A8B0C 7F073FDC 8C6E0000 */  lw    $t6, ($v1)
/* 0A8B10 7F073FE0 27BDFF88 */  addiu $sp, $sp, -0x78
/* 0A8B14 7F073FE4 AFB00018 */  sw    $s0, 0x18($sp)
/* 0A8B18 7F073FE8 00808025 */  move  $s0, $a0
/* 0A8B1C 7F073FEC AFBF001C */  sw    $ra, 0x1c($sp)
/* 0A8B20 7F073FF0 AFA60080 */  sw    $a2, 0x80($sp)
/* 0A8B24 7F073FF4 19C00142 */  blez  $t6, .L7F074500
/* 0A8B28 7F073FF8 00A03825 */   move  $a3, $a1
/* 0A8B2C 7F073FFC 3C198003 */  lui   $t9, %hi(D_800363F8)
/* 0A8B30 7F074000 273963F8 */  addiu $t9, %lo(D_800363F8) # addiu $t9, $t9, 0x63f8
/* 0A8B34 7F074004 8F210000 */  lw    $at, ($t9)
/* 0A8B38 7F074008 8F290004 */  lw    $t1, 4($t9)
/* 0A8B3C 7F07400C 8CC60004 */  lw    $a2, 4($a2)
/* 0A8B40 7F074010 27B80058 */  addiu $t8, $sp, 0x58
/* 0A8B44 7F074014 AF010000 */  sw    $at, ($t8)
/* 0A8B48 7F074018 AF090004 */  sw    $t1, 4($t8)
/* 0A8B4C 7F07401C 8F29000C */  lw    $t1, 0xc($t9)
/* 0A8B50 7F074020 8F210008 */  lw    $at, 8($t9)
/* 0A8B54 7F074024 00A02025 */  move  $a0, $a1
/* 0A8B58 7F074028 AF09000C */  sw    $t1, 0xc($t8)
/* 0A8B5C 7F07402C AF010008 */  sw    $at, 8($t8)
/* 0A8B60 7F074030 8CC50014 */  lw    $a1, 0x14($a2)
/* 0A8B64 7F074034 AFA7007C */  sw    $a3, 0x7c($sp)
/* 0A8B68 7F074038 0FC1B1E7 */  jal   modelGetNodeRwData
/* 0A8B6C 7F07403C AFA6006C */   sw    $a2, 0x6c($sp)
/* 0A8B70 7F074040 8E0A0008 */  lw    $t2, 8($s0)
/* 0A8B74 7F074044 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A8B78 7F074048 C444000C */  lwc1  $f4, 0xc($v0)
/* 0A8B7C 7F07404C C4460004 */  lwc1  $f6, 4($v0)
/* 0A8B80 7F074050 3C038003 */  lui   $v1, %hi(D_800363F0)
/* 0A8B84 7F074054 314B0002 */  andi  $t3, $t2, 2
/* 0A8B88 7F074058 246363F0 */  addiu $v1, %lo(D_800363F0) # addiu $v1, $v1, 0x63f0
/* 0A8B8C 7F07405C 8FA7007C */  lw    $a3, 0x7c($sp)
/* 0A8B90 7F074060 C4CC0008 */  lwc1  $f12, 8($a2)
/* 0A8B94 7F074064 C4CE000C */  lwc1  $f14, 0xc($a2)
/* 0A8B98 7F074068 11600125 */  beqz  $t3, .L7F074500
/* 0A8B9C 7F07406C 46062081 */   sub.s $f2, $f4, $f6
/* 0A8BA0 7F074070 8E020030 */  lw    $v0, 0x30($s0)
/* 0A8BA4 7F074074 24010003 */  li    $at, 3
/* 0A8BA8 7F074078 00002825 */  move  $a1, $zero
/* 0A8BAC 7F07407C 10410003 */  beq   $v0, $at, .L7F07408C
/* 0A8BB0 7F074080 24010008 */   li    $at, 8
/* 0A8BB4 7F074084 5441000F */  bnel  $v0, $at, .L7F0740C4
/* 0A8BB8 7F074088 8C780000 */   lw    $t8, ($v1)
.L7F07408C:
/* 0A8BBC 7F07408C 8E0C0034 */  lw    $t4, 0x34($s0)
/* 0A8BC0 7F074090 8C6E0000 */  lw    $t6, ($v1)
/* 0A8BC4 7F074094 240100FF */  li    $at, 255
/* 0A8BC8 7F074098 318D00FF */  andi  $t5, $t4, 0xff
/* 0A8BCC 7F07409C 01AE0019 */  multu $t5, $t6
/* 0A8BD0 7F0740A0 00001012 */  mflo  $v0
/* 0A8BD4 7F0740A4 00000000 */  nop
/* 0A8BD8 7F0740A8 00000000 */  nop
/* 0A8BDC 7F0740AC 0041001B */  divu  $zero, $v0, $at
/* 0A8BE0 7F0740B0 00004012 */  mflo  $t0
/* 0A8BE4 7F0740B4 A3A80067 */  sb    $t0, 0x67($sp)
/* 0A8BE8 7F0740B8 10000004 */  b     .L7F0740CC
/* 0A8BEC 7F0740BC 8FA40080 */   lw    $a0, 0x80($sp)
/* 0A8BF0 7F0740C0 8C780000 */  lw    $t8, ($v1)
.L7F0740C4:
/* 0A8BF4 7F0740C4 A3B80067 */  sb    $t8, 0x67($sp)
/* 0A8BF8 7F0740C8 8FA40080 */  lw    $a0, 0x80($sp)
.L7F0740CC:
/* 0A8BFC 7F0740CC AFA6006C */  sw    $a2, 0x6c($sp)
/* 0A8C00 7F0740D0 AFA7007C */  sw    $a3, 0x7c($sp)
/* 0A8C04 7F0740D4 E7A20040 */  swc1  $f2, 0x40($sp)
/* 0A8C08 7F0740D8 E7AC0048 */  swc1  $f12, 0x48($sp)
/* 0A8C0C 7F0740DC 0FC1B15C */  jal   modelFindNodeMtxIndex
/* 0A8C10 7F0740E0 E7AE0044 */   swc1  $f14, 0x44($sp)
/* 0A8C14 7F0740E4 8FA7007C */  lw    $a3, 0x7c($sp)
/* 0A8C18 7F0740E8 00024980 */  sll   $t1, $v0, 6
/* 0A8C1C 7F0740EC 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A8C20 7F0740F0 8CF9000C */  lw    $t9, 0xc($a3)
/* 0A8C24 7F0740F4 C7A20040 */  lwc1  $f2, 0x40($sp)
/* 0A8C28 7F0740F8 C7AC0048 */  lwc1  $f12, 0x48($sp)
/* 0A8C2C 7F0740FC 03295021 */  addu  $t2, $t9, $t1
/* 0A8C30 7F074100 AFAA0070 */  sw    $t2, 0x70($sp)
/* 0A8C34 7F074104 8E0B0004 */  lw    $t3, 4($s0)
/* 0A8C38 7F074108 C7AE0044 */  lwc1  $f14, 0x44($sp)
/* 0A8C3C 7F07410C 3C014000 */  li    $at, 0x40000000 # 2.000000
/* 0A8C40 7F074110 1160000B */  beqz  $t3, .L7F074140
/* 0A8C44 7F074114 3C028003 */   lui   $v0, %hi(vtxallocator)
/* 0A8C48 7F074118 44814000 */  mtc1  $at, $f8
/* 0A8C4C 7F07411C C4F20014 */  lwc1  $f18, 0x14($a3)
/* 0A8C50 7F074120 46024281 */  sub.s $f10, $f8, $f2
/* 0A8C54 7F074124 46125103 */  div.s $f4, $f10, $f18
/* 0A8C58 7F074128 4600218D */  trunc.w.s $f6, $f4
/* 0A8C5C 7F07412C 44033000 */  mfc1  $v1, $f6
/* 0A8C60 7F074130 00000000 */  nop
/* 0A8C64 7F074134 00036C00 */  sll   $t5, $v1, 0x10
/* 0A8C68 7F074138 10000009 */  b     .L7F074160
/* 0A8C6C 7F07413C 000D1C03 */   sra   $v1, $t5, 0x10
.L7F074140:
/* 0A8C70 7F074140 C4EA0014 */  lwc1  $f10, 0x14($a3)
/* 0A8C74 7F074144 46001207 */  neg.s $f8, $f2
/* 0A8C78 7F074148 460A4483 */  div.s $f18, $f8, $f10
/* 0A8C7C 7F07414C 4600910D */  trunc.w.s $f4, $f18
/* 0A8C80 7F074150 44032000 */  mfc1  $v1, $f4
/* 0A8C84 7F074154 00000000 */  nop
/* 0A8C88 7F074158 00034400 */  sll   $t0, $v1, 0x10
/* 0A8C8C 7F07415C 00081C03 */  sra   $v1, $t0, 0x10
.L7F074160:
/* 0A8C90 7F074160 3C014248 */  li    $at, 0x42480000 # 50.000000
/* 0A8C94 7F074164 44813000 */  mtc1  $at, $f6
/* 0A8C98 7F074168 3C048005 */  lui   $a0, %hi(aDoshadowNoVtxAllocator)
/* 0A8C9C 7F07416C 3C014396 */  li    $at, 0x43960000 # 300.000000
/* 0A8CA0 7F074170 4606103C */  c.lt.s $f2, $f6
/* 0A8CA4 7F074174 24844A94 */  addiu $a0, %lo(aDoshadowNoVtxAllocator) # addiu $a0, $a0, 0x4a94
/* 0A8CA8 7F074178 4502000A */  bc1fl .L7F0741A4
/* 0A8CAC 7F07417C 44818000 */   mtc1  $at, $f16
/* 0A8CB0 7F074180 3C013FA0 */  li    $at, 0x3FA00000 # 1.250000
/* 0A8CB4 7F074184 44810000 */  mtc1  $at, $f0
/* 0A8CB8 7F074188 00000000 */  nop
/* 0A8CBC 7F07418C 46006302 */  mul.s $f12, $f12, $f0
/* 0A8CC0 7F074190 00000000 */  nop
/* 0A8CC4 7F074194 46007382 */  mul.s $f14, $f14, $f0
/* 0A8CC8 7F074198 10000012 */  b     .L7F0741E4
/* 0A8CCC 7F07419C 00000000 */   nop
/* 0A8CD0 7F0741A0 44818000 */  mtc1  $at, $f16
.L7F0741A4:
/* 0A8CD4 7F0741A4 3C014348 */  li    $at, 0x43480000 # 200.000000
/* 0A8CD8 7F0741A8 4602803C */  c.lt.s $f16, $f2
/* 0A8CDC 7F0741AC 00000000 */  nop
/* 0A8CE0 7F0741B0 45020005 */  bc1fl .L7F0741C8
/* 0A8CE4 7F0741B4 46028201 */   sub.s $f8, $f16, $f2
/* 0A8CE8 7F0741B8 44807000 */  mtc1  $zero, $f14
/* 0A8CEC 7F0741BC 10000009 */  b     .L7F0741E4
/* 0A8CF0 7F0741C0 46007306 */   mov.s $f12, $f14
/* 0A8CF4 7F0741C4 46028201 */  sub.s $f8, $f16, $f2
.L7F0741C8:
/* 0A8CF8 7F0741C8 44815000 */  mtc1  $at, $f10
/* 0A8CFC 7F0741CC 00000000 */  nop
/* 0A8D00 7F0741D0 460A4003 */  div.s $f0, $f8, $f10
/* 0A8D04 7F0741D4 46006302 */  mul.s $f12, $f12, $f0
/* 0A8D08 7F0741D8 00000000 */  nop
/* 0A8D0C 7F0741DC 46007382 */  mul.s $f14, $f14, $f0
/* 0A8D10 7F0741E0 00000000 */  nop
.L7F0741E4:
/* 0A8D14 7F0741E4 8C42608C */  lw    $v0, %lo(vtxallocator)($v0)
/* 0A8D18 7F0741E8 5440000F */  bnezl $v0, .L7F074228
/* 0A8D1C 7F0741EC 24040004 */   li    $a0, 4
/* 0A8D20 7F0741F0 A7A30052 */  sh    $v1, 0x52($sp)
/* 0A8D24 7F0741F4 AFA6006C */  sw    $a2, 0x6c($sp)
/* 0A8D28 7F0741F8 E7AC0048 */  swc1  $f12, 0x48($sp)
/* 0A8D2C 7F0741FC 0C0033D1 */  jal   osSyncPrintf
/* 0A8D30 7F074200 E7AE0044 */   swc1  $f14, 0x44($sp)
/* 0A8D34 7F074204 0FC1B11B */  jal   return_null
/* 0A8D38 7F074208 00000000 */   nop
/* 0A8D3C 7F07420C 3C028003 */  lui   $v0, %hi(vtxallocator)
/* 0A8D40 7F074210 8C42608C */  lw    $v0, %lo(vtxallocator)($v0)
/* 0A8D44 7F074214 87A30052 */  lh    $v1, 0x52($sp)
/* 0A8D48 7F074218 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A8D4C 7F07421C C7AC0048 */  lwc1  $f12, 0x48($sp)
/* 0A8D50 7F074220 C7AE0044 */  lwc1  $f14, 0x44($sp)
/* 0A8D54 7F074224 24040004 */  li    $a0, 4
.L7F074228:
/* 0A8D58 7F074228 A7A30052 */  sh    $v1, 0x52($sp)
/* 0A8D5C 7F07422C AFA6006C */  sw    $a2, 0x6c($sp)
/* 0A8D60 7F074230 E7AC0048 */  swc1  $f12, 0x48($sp)
/* 0A8D64 7F074234 0040F809 */  jalr  $v0
/* 0A8D68 7F074238 E7AE0044 */  swc1  $f14, 0x44($sp)
/* 0A8D6C 7F07423C 27A40058 */  addiu $a0, $sp, 0x58
/* 0A8D70 7F074240 8C810000 */  lw    $at, ($a0)
/* 0A8D74 7F074244 87A30052 */  lh    $v1, 0x52($sp)
/* 0A8D78 7F074248 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A8D7C 7F07424C C7AC0048 */  lwc1  $f12, 0x48($sp)
/* 0A8D80 7F074250 C7AE0044 */  lwc1  $f14, 0x44($sp)
/* 0A8D84 7F074254 AC410000 */  sw    $at, ($v0)
/* 0A8D88 7F074258 8C8A0004 */  lw    $t2, 4($a0)
/* 0A8D8C 7F07425C AC4A0004 */  sw    $t2, 4($v0)
/* 0A8D90 7F074260 8C810008 */  lw    $at, 8($a0)
/* 0A8D94 7F074264 AC410008 */  sw    $at, 8($v0)
/* 0A8D98 7F074268 8C8A000C */  lw    $t2, 0xc($a0)
/* 0A8D9C 7F07426C AC4A000C */  sw    $t2, 0xc($v0)
/* 0A8DA0 7F074270 8C810000 */  lw    $at, ($a0)
/* 0A8DA4 7F074274 AC410010 */  sw    $at, 0x10($v0)
/* 0A8DA8 7F074278 8C8D0004 */  lw    $t5, 4($a0)
/* 0A8DAC 7F07427C AC4D0014 */  sw    $t5, 0x14($v0)
/* 0A8DB0 7F074280 8C810008 */  lw    $at, 8($a0)
/* 0A8DB4 7F074284 AC410018 */  sw    $at, 0x18($v0)
/* 0A8DB8 7F074288 8C8D000C */  lw    $t5, 0xc($a0)
/* 0A8DBC 7F07428C AC4D001C */  sw    $t5, 0x1c($v0)
/* 0A8DC0 7F074290 8C810000 */  lw    $at, ($a0)
/* 0A8DC4 7F074294 AC410020 */  sw    $at, 0x20($v0)
/* 0A8DC8 7F074298 8C880004 */  lw    $t0, 4($a0)
/* 0A8DCC 7F07429C AC480024 */  sw    $t0, 0x24($v0)
/* 0A8DD0 7F0742A0 8C810008 */  lw    $at, 8($a0)
/* 0A8DD4 7F0742A4 AC410028 */  sw    $at, 0x28($v0)
/* 0A8DD8 7F0742A8 8C88000C */  lw    $t0, 0xc($a0)
/* 0A8DDC 7F0742AC AC48002C */  sw    $t0, 0x2c($v0)
/* 0A8DE0 7F0742B0 8C810000 */  lw    $at, ($a0)
/* 0A8DE4 7F0742B4 AC410030 */  sw    $at, 0x30($v0)
/* 0A8DE8 7F0742B8 8C990004 */  lw    $t9, 4($a0)
/* 0A8DEC 7F0742BC AC590034 */  sw    $t9, 0x34($v0)
/* 0A8DF0 7F0742C0 8C810008 */  lw    $at, 8($a0)
/* 0A8DF4 7F0742C4 AC410038 */  sw    $at, 0x38($v0)
/* 0A8DF8 7F0742C8 8C99000C */  lw    $t9, 0xc($a0)
/* 0A8DFC 7F0742CC AC59003C */  sw    $t9, 0x3c($v0)
/* 0A8E00 7F0742D0 C4D20000 */  lwc1  $f18, ($a2)
/* 0A8E04 7F0742D4 A4430002 */  sh    $v1, 2($v0)
/* 0A8E08 7F0742D8 460C9101 */  sub.s $f4, $f18, $f12
/* 0A8E0C 7F0742DC 4600218D */  trunc.w.s $f6, $f4
/* 0A8E10 7F0742E0 440C3000 */  mfc1  $t4, $f6
/* 0A8E14 7F0742E4 00000000 */  nop
/* 0A8E18 7F0742E8 A44C0000 */  sh    $t4, ($v0)
/* 0A8E1C 7F0742EC C4C80004 */  lwc1  $f8, 4($a2)
/* 0A8E20 7F0742F0 460E4281 */  sub.s $f10, $f8, $f14
/* 0A8E24 7F0742F4 4600548D */  trunc.w.s $f18, $f10
/* 0A8E28 7F0742F8 440D9000 */  mfc1  $t5, $f18
/* 0A8E2C 7F0742FC 00000000 */  nop
/* 0A8E30 7F074300 A44D0004 */  sh    $t5, 4($v0)
/* 0A8E34 7F074304 C4C40000 */  lwc1  $f4, ($a2)
/* 0A8E38 7F074308 A4430012 */  sh    $v1, 0x12($v0)
/* 0A8E3C 7F07430C 460C2181 */  sub.s $f6, $f4, $f12
/* 0A8E40 7F074310 4600320D */  trunc.w.s $f8, $f6
/* 0A8E44 7F074314 440E4000 */  mfc1  $t6, $f8
/* 0A8E48 7F074318 00000000 */  nop
/* 0A8E4C 7F07431C A44E0010 */  sh    $t6, 0x10($v0)
/* 0A8E50 7F074320 C4CA0004 */  lwc1  $f10, 4($a2)
/* 0A8E54 7F074324 460E5480 */  add.s $f18, $f10, $f14
/* 0A8E58 7F074328 4600910D */  trunc.w.s $f4, $f18
/* 0A8E5C 7F07432C 44092000 */  mfc1  $t1, $f4
/* 0A8E60 7F074330 00000000 */  nop
/* 0A8E64 7F074334 A4490014 */  sh    $t1, 0x14($v0)
/* 0A8E68 7F074338 C4C60000 */  lwc1  $f6, ($a2)
/* 0A8E6C 7F07433C A4430022 */  sh    $v1, 0x22($v0)
/* 0A8E70 7F074340 3C09BC00 */  lui   $t1, (0xBC001406 >> 16) # lui $t1, 0xbc00
/* 0A8E74 7F074344 460C3200 */  add.s $f8, $f6, $f12
/* 0A8E78 7F074348 35291406 */  ori   $t1, (0xBC001406 & 0xFFFF) # ori $t1, $t1, 0x1406
/* 0A8E7C 7F07434C 4600428D */  trunc.w.s $f10, $f8
/* 0A8E80 7F074350 44195000 */  mfc1  $t9, $f10
/* 0A8E84 7F074354 00000000 */  nop
/* 0A8E88 7F074358 A4590020 */  sh    $t9, 0x20($v0)
/* 0A8E8C 7F07435C C4D20004 */  lwc1  $f18, 4($a2)
/* 0A8E90 7F074360 460E9100 */  add.s $f4, $f18, $f14
/* 0A8E94 7F074364 4600218D */  trunc.w.s $f6, $f4
/* 0A8E98 7F074368 440C3000 */  mfc1  $t4, $f6
/* 0A8E9C 7F07436C 00000000 */  nop
/* 0A8EA0 7F074370 A44C0024 */  sh    $t4, 0x24($v0)
/* 0A8EA4 7F074374 C4C80000 */  lwc1  $f8, ($a2)
/* 0A8EA8 7F074378 A4430032 */  sh    $v1, 0x32($v0)
/* 0A8EAC 7F07437C 460C4280 */  add.s $f10, $f8, $f12
/* 0A8EB0 7F074380 4600548D */  trunc.w.s $f18, $f10
/* 0A8EB4 7F074384 440D9000 */  mfc1  $t5, $f18
/* 0A8EB8 7F074388 00000000 */  nop
/* 0A8EBC 7F07438C A44D0030 */  sh    $t5, 0x30($v0)
/* 0A8EC0 7F074390 C4C40004 */  lwc1  $f4, 4($a2)
/* 0A8EC4 7F074394 460E2181 */  sub.s $f6, $f4, $f14
/* 0A8EC8 7F074398 4600320D */  trunc.w.s $f8, $f6
/* 0A8ECC 7F07439C 440E4000 */  mfc1  $t6, $f8
/* 0A8ED0 7F0743A0 00000000 */  nop
/* 0A8ED4 7F0743A4 A44E0034 */  sh    $t6, 0x34($v0)
/* 0A8ED8 7F0743A8 8E03000C */  lw    $v1, 0xc($s0)
/* 0A8EDC 7F0743AC 24680008 */  addiu $t0, $v1, 8
/* 0A8EE0 7F0743B0 AE08000C */  sw    $t0, 0xc($s0)
/* 0A8EE4 7F0743B4 AC690000 */  sw    $t1, ($v1)
/* 0A8EE8 7F0743B8 8CC4001C */  lw    $a0, 0x1c($a2)
/* 0A8EEC 7F0743BC AFA20054 */  sw    $v0, 0x54($sp)
/* 0A8EF0 7F0743C0 AFA6006C */  sw    $a2, 0x6c($sp)
/* 0A8EF4 7F0743C4 0C003A2C */  jal   osVirtualToPhysical
/* 0A8EF8 7F0743C8 AFA30038 */   sw    $v1, 0x38($sp)
/* 0A8EFC 7F0743CC 8FA50038 */  lw    $a1, 0x38($sp)
/* 0A8F00 7F0743D0 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A8F04 7F0743D4 8FA70054 */  lw    $a3, 0x54($sp)
/* 0A8F08 7F0743D8 ACA20004 */  sw    $v0, 4($a1)
/* 0A8F0C 7F0743DC 8CC30010 */  lw    $v1, 0x10($a2)
/* 0A8F10 7F0743E0 24060001 */  li    $a2, 1
/* 0A8F14 7F0743E4 00002825 */  move  $a1, $zero
/* 0A8F18 7F0743E8 1060001C */  beqz  $v1, .L7F07445C
/* 0A8F1C 7F0743EC 02002025 */   move  $a0, $s0
/* 0A8F20 7F0743F0 A4E00008 */  sh    $zero, 8($a3)
/* 0A8F24 7F0743F4 A4E0000A */  sh    $zero, 0xa($a3)
/* 0A8F28 7F0743F8 90780004 */  lbu   $t8, 4($v1)
/* 0A8F2C 7F0743FC A4E0001A */  sh    $zero, 0x1a($a3)
/* 0A8F30 7F074400 00602825 */  move  $a1, $v1
/* 0A8F34 7F074404 0018C940 */  sll   $t9, $t8, 5
/* 0A8F38 7F074408 272AFFFF */  addiu $t2, $t9, -1
/* 0A8F3C 7F07440C A4EA0018 */  sh    $t2, 0x18($a3)
/* 0A8F40 7F074410 906C0004 */  lbu   $t4, 4($v1)
/* 0A8F44 7F074414 02002025 */  move  $a0, $s0
/* 0A8F48 7F074418 24060004 */  li    $a2, 4
/* 0A8F4C 7F07441C 000C5940 */  sll   $t3, $t4, 5
/* 0A8F50 7F074420 256DFFFF */  addiu $t5, $t3, -1
/* 0A8F54 7F074424 A4ED0028 */  sh    $t5, 0x28($a3)
/* 0A8F58 7F074428 906F0005 */  lbu   $t7, 5($v1)
/* 0A8F5C 7F07442C A4E00038 */  sh    $zero, 0x38($a3)
/* 0A8F60 7F074430 000F7140 */  sll   $t6, $t7, 5
/* 0A8F64 7F074434 25C8FFFF */  addiu $t0, $t6, -1
/* 0A8F68 7F074438 A4E8002A */  sh    $t0, 0x2a($a3)
/* 0A8F6C 7F07443C 90690005 */  lbu   $t1, 5($v1)
/* 0A8F70 7F074440 0009C140 */  sll   $t8, $t1, 5
/* 0A8F74 7F074444 2719FFFF */  addiu $t9, $t8, -1
/* 0A8F78 7F074448 A4F9003A */  sh    $t9, 0x3a($a3)
/* 0A8F7C 7F07444C 0FC1CC0E */  jal   sub_GAME_7F073038
/* 0A8F80 7F074450 AFA70054 */   sw    $a3, 0x54($sp)
/* 0A8F84 7F074454 10000004 */  b     .L7F074468
/* 0A8F88 7F074458 8FA70054 */   lw    $a3, 0x54($sp)
.L7F07445C:
/* 0A8F8C 7F07445C 0FC1CC0E */  jal   sub_GAME_7F073038
/* 0A8F90 7F074460 AFA70054 */   sw    $a3, 0x54($sp)
/* 0A8F94 7F074464 8FA70054 */  lw    $a3, 0x54($sp)
.L7F074468:
/* 0A8F98 7F074468 8E03000C */  lw    $v1, 0xc($s0)
/* 0A8F9C 7F07446C 3C0CB700 */  lui   $t4, 0xb700
/* 0A8FA0 7F074470 240B2000 */  li    $t3, 8192
/* 0A8FA4 7F074474 246A0008 */  addiu $t2, $v1, 8
/* 0A8FA8 7F074478 AE0A000C */  sw    $t2, 0xc($s0)
/* 0A8FAC 7F07447C AC6C0000 */  sw    $t4, ($v1)
/* 0A8FB0 7F074480 AC6B0004 */  sw    $t3, 4($v1)
/* 0A8FB4 7F074484 8E03000C */  lw    $v1, 0xc($s0)
/* 0A8FB8 7F074488 3C0F0102 */  lui   $t7, (0x01020040 >> 16) # lui $t7, 0x102
/* 0A8FBC 7F07448C 35EF0040 */  ori   $t7, (0x01020040 & 0xFFFF) # ori $t7, $t7, 0x40
/* 0A8FC0 7F074490 246D0008 */  addiu $t5, $v1, 8
/* 0A8FC4 7F074494 AE0D000C */  sw    $t5, 0xc($s0)
/* 0A8FC8 7F074498 AC6F0000 */  sw    $t7, ($v1)
/* 0A8FCC 7F07449C AFA70054 */  sw    $a3, 0x54($sp)
/* 0A8FD0 7F0744A0 8FA40070 */  lw    $a0, 0x70($sp)
/* 0A8FD4 7F0744A4 0C003A2C */  jal   osVirtualToPhysical
/* 0A8FD8 7F0744A8 AFA3002C */   sw    $v1, 0x2c($sp)
/* 0A8FDC 7F0744AC 8FA5002C */  lw    $a1, 0x2c($sp)
/* 0A8FE0 7F0744B0 8FA40054 */  lw    $a0, 0x54($sp)
/* 0A8FE4 7F0744B4 3C080430 */  lui   $t0, (0x04300040 >> 16) # lui $t0, 0x430
/* 0A8FE8 7F0744B8 ACA20004 */  sw    $v0, 4($a1)
/* 0A8FEC 7F0744BC 8E03000C */  lw    $v1, 0xc($s0)
/* 0A8FF0 7F0744C0 35080040 */  ori   $t0, (0x04300040 & 0xFFFF) # ori $t0, $t0, 0x40
/* 0A8FF4 7F0744C4 246E0008 */  addiu $t6, $v1, 8
/* 0A8FF8 7F0744C8 AE0E000C */  sw    $t6, 0xc($s0)
/* 0A8FFC 7F0744CC AC680000 */  sw    $t0, ($v1)
/* 0A9000 7F0744D0 0C003A2C */  jal   osVirtualToPhysical
/* 0A9004 7F0744D4 AFA30028 */   sw    $v1, 0x28($sp)
/* 0A9008 7F0744D8 8FA60028 */  lw    $a2, 0x28($sp)
/* 0A900C 7F0744DC 3C18B100 */  lui   $t8, (0xB1000002 >> 16) # lui $t8, 0xb100
/* 0A9010 7F0744E0 37180002 */  ori   $t8, (0xB1000002 & 0xFFFF) # ori $t8, $t8, 2
/* 0A9014 7F0744E4 ACC20004 */  sw    $v0, 4($a2)
/* 0A9018 7F0744E8 8E03000C */  lw    $v1, 0xc($s0)
/* 0A901C 7F0744EC 24193210 */  li    $t9, 12816
/* 0A9020 7F0744F0 24690008 */  addiu $t1, $v1, 8
/* 0A9024 7F0744F4 AE09000C */  sw    $t1, 0xc($s0)
/* 0A9028 7F0744F8 AC790004 */  sw    $t9, 4($v1)
/* 0A902C 7F0744FC AC780000 */  sw    $t8, ($v1)
.L7F074500:
/* 0A9030 7F074500 8FBF001C */  lw    $ra, 0x1c($sp)
/* 0A9034 7F074504 8FB00018 */  lw    $s0, 0x18($sp)
/* 0A9038 7F074508 27BD0078 */  addiu $sp, $sp, 0x78
/* 0A903C 7F07450C 03E00008 */  jr    $ra
/* 0A9040 7F074510 00000000 */   nop
)
#endif
#ifdef VERSION_EU
GLOBAL_ASM(
.text
glabel doshadow
/* 0A6AD8 7F0740E8 3C038003 */  lui   $v1, %hi(D_800363F0) # $v1, 0x8003
/* 0A6ADC 7F0740EC 24631940 */  addiu $v1, %lo(D_800363F0) # addiu $v1, $v1, 0x1940
/* 0A6AE0 7F0740F0 8C6E0000 */  lw    $t6, ($v1)
/* 0A6AE4 7F0740F4 27BDFF88 */  addiu $sp, $sp, -0x78
/* 0A6AE8 7F0740F8 AFB00018 */  sw    $s0, 0x18($sp)
/* 0A6AEC 7F0740FC 00808025 */  move  $s0, $a0
/* 0A6AF0 7F074100 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0A6AF4 7F074104 AFA60080 */  sw    $a2, 0x80($sp)
/* 0A6AF8 7F074108 19C00132 */  blez  $t6, .L7F0745D4
/* 0A6AFC 7F07410C 00A03825 */   move  $a3, $a1
/* 0A6B00 7F074110 3C198003 */  lui   $t9, %hi(D_800363F8) # $t9, 0x8003
/* 0A6B04 7F074114 27391948 */  addiu $t9, %lo(D_800363F8) # addiu $t9, $t9, 0x1948
/* 0A6B08 7F074118 8F210000 */  lw    $at, ($t9)
/* 0A6B0C 7F07411C 8F2A0004 */  lw    $t2, 4($t9)
/* 0A6B10 7F074120 8CC60004 */  lw    $a2, 4($a2)
/* 0A6B14 7F074124 27B80058 */  addiu $t8, $sp, 0x58
/* 0A6B18 7F074128 AF010000 */  sw    $at, ($t8)
/* 0A6B1C 7F07412C AF0A0004 */  sw    $t2, 4($t8)
/* 0A6B20 7F074130 8F2A000C */  lw    $t2, 0xc($t9)
/* 0A6B24 7F074134 8F210008 */  lw    $at, 8($t9)
/* 0A6B28 7F074138 00A02025 */  move  $a0, $a1
/* 0A6B2C 7F07413C AF0A000C */  sw    $t2, 0xc($t8)
/* 0A6B30 7F074140 AF010008 */  sw    $at, 8($t8)
/* 0A6B34 7F074144 8CC50014 */  lw    $a1, 0x14($a2)
/* 0A6B38 7F074148 AFA7007C */  sw    $a3, 0x7c($sp)
/* 0A6B3C 7F07414C 0FC1B3A3 */  jal   modelGetNodeRwData
/* 0A6B40 7F074150 AFA6006C */   sw    $a2, 0x6c($sp)
/* 0A6B44 7F074154 8E0B0008 */  lw    $t3, 8($s0)
/* 0A6B48 7F074158 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A6B4C 7F07415C C444000C */  lwc1  $f4, 0xc($v0)
/* 0A6B50 7F074160 C4460004 */  lwc1  $f6, 4($v0)
/* 0A6B54 7F074164 3C038003 */  lui   $v1, %hi(D_800363F0) # $v1, 0x8003
/* 0A6B58 7F074168 316C0002 */  andi  $t4, $t3, 2
/* 0A6B5C 7F07416C 24631940 */  addiu $v1, %lo(D_800363F0) # addiu $v1, $v1, 0x1940
/* 0A6B60 7F074170 8FA7007C */  lw    $a3, 0x7c($sp)
/* 0A6B64 7F074174 C4C20008 */  lwc1  $f2, 8($a2)
/* 0A6B68 7F074178 C4CC000C */  lwc1  $f12, 0xc($a2)
/* 0A6B6C 7F07417C 11800115 */  beqz  $t4, .L7F0745D4
/* 0A6B70 7F074180 46062381 */   sub.s $f14, $f4, $f6
/* 0A6B74 7F074184 8E020030 */  lw    $v0, 0x30($s0)
/* 0A6B78 7F074188 24010003 */  li    $at, 3
/* 0A6B7C 7F07418C 00002825 */  move  $a1, $zero
/* 0A6B80 7F074190 10410003 */  beq   $v0, $at, .L7F0741A0
/* 0A6B84 7F074194 24010008 */   li    $at, 8
/* 0A6B88 7F074198 5441000F */  bnel  $v0, $at, .L7F0741D8
/* 0A6B8C 7F07419C 8C790000 */   lw    $t9, ($v1)
.L7F0741A0:
/* 0A6B90 7F0741A0 8E0D0034 */  lw    $t5, 0x34($s0)
/* 0A6B94 7F0741A4 8C6F0000 */  lw    $t7, ($v1)
/* 0A6B98 7F0741A8 240100FF */  li    $at, 255
/* 0A6B9C 7F0741AC 31AE00FF */  andi  $t6, $t5, 0xff
/* 0A6BA0 7F0741B0 01CF0019 */  multu $t6, $t7
/* 0A6BA4 7F0741B4 00001012 */  mflo  $v0
/* 0A6BA8 7F0741B8 00000000 */  nop
/* 0A6BAC 7F0741BC 00000000 */  nop
/* 0A6BB0 7F0741C0 0041001B */  divu  $zero, $v0, $at
/* 0A6BB4 7F0741C4 0000C012 */  mflo  $t8
/* 0A6BB8 7F0741C8 A3B80067 */  sb    $t8, 0x67($sp)
/* 0A6BBC 7F0741CC 10000004 */  b     .L7F0741E0
/* 0A6BC0 7F0741D0 8FA40080 */   lw    $a0, 0x80($sp)
/* 0A6BC4 7F0741D4 8C790000 */  lw    $t9, ($v1)
.L7F0741D8:
/* 0A6BC8 7F0741D8 A3B90067 */  sb    $t9, 0x67($sp)
/* 0A6BCC 7F0741DC 8FA40080 */  lw    $a0, 0x80($sp)
.L7F0741E0:
/* 0A6BD0 7F0741E0 AFA6006C */  sw    $a2, 0x6c($sp)
/* 0A6BD4 7F0741E4 AFA7007C */  sw    $a3, 0x7c($sp)
/* 0A6BD8 7F0741E8 E7A20048 */  swc1  $f2, 0x48($sp)
/* 0A6BDC 7F0741EC E7AC0044 */  swc1  $f12, 0x44($sp)
/* 0A6BE0 7F0741F0 0FC1B32A */  jal   modelFindNodeMtxIndex
/* 0A6BE4 7F0741F4 E7AE0040 */   swc1  $f14, 0x40($sp)
/* 0A6BE8 7F0741F8 8FA7007C */  lw    $a3, 0x7c($sp)
/* 0A6BEC 7F0741FC 00025980 */  sll   $t3, $v0, 6
/* 0A6BF0 7F074200 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A6BF4 7F074204 8CEA000C */  lw    $t2, 0xc($a3)
/* 0A6BF8 7F074208 C7A20048 */  lwc1  $f2, 0x48($sp)
/* 0A6BFC 7F07420C C7AC0044 */  lwc1  $f12, 0x44($sp)
/* 0A6C00 7F074210 014B6021 */  addu  $t4, $t2, $t3
/* 0A6C04 7F074214 AFAC0070 */  sw    $t4, 0x70($sp)
/* 0A6C08 7F074218 8E0D0004 */  lw    $t5, 4($s0)
/* 0A6C0C 7F07421C C7AE0040 */  lwc1  $f14, 0x40($sp)
/* 0A6C10 7F074220 3C014000 */  li    $at, 0x40000000 # 2.000000
/* 0A6C14 7F074224 11A0000B */  beqz  $t5, .L7F074254
/* 0A6C18 7F074228 24040004 */   li    $a0, 4
/* 0A6C1C 7F07422C 44814000 */  mtc1  $at, $f8
/* 0A6C20 7F074230 C4F20014 */  lwc1  $f18, 0x14($a3)
/* 0A6C24 7F074234 460E4281 */  sub.s $f10, $f8, $f14
/* 0A6C28 7F074238 46125103 */  div.s $f4, $f10, $f18
/* 0A6C2C 7F07423C 4600218D */  trunc.w.s $f6, $f4
/* 0A6C30 7F074240 44033000 */  mfc1  $v1, $f6
/* 0A6C34 7F074244 00000000 */  nop
/* 0A6C38 7F074248 00037C00 */  sll   $t7, $v1, 0x10
/* 0A6C3C 7F07424C 10000009 */  b     .L7F074274
/* 0A6C40 7F074250 000F1C03 */   sra   $v1, $t7, 0x10
.L7F074254:
/* 0A6C44 7F074254 C4EA0014 */  lwc1  $f10, 0x14($a3)
/* 0A6C48 7F074258 46007207 */  neg.s $f8, $f14
/* 0A6C4C 7F07425C 460A4483 */  div.s $f18, $f8, $f10
/* 0A6C50 7F074260 4600910D */  trunc.w.s $f4, $f18
/* 0A6C54 7F074264 44032000 */  mfc1  $v1, $f4
/* 0A6C58 7F074268 00000000 */  nop
/* 0A6C5C 7F07426C 0003CC00 */  sll   $t9, $v1, 0x10
/* 0A6C60 7F074270 00191C03 */  sra   $v1, $t9, 0x10
.L7F074274:
/* 0A6C64 7F074274 3C014248 */  li    $at, 0x42480000 # 50.000000
/* 0A6C68 7F074278 44813000 */  mtc1  $at, $f6
/* 0A6C6C 7F07427C 3C014396 */  li    $at, 0x43960000 # 300.000000
/* 0A6C70 7F074280 3C198003 */  lui   $t9, %hi(vtxallocator) # $t9, 0x8003
/* 0A6C74 7F074284 4606703C */  c.lt.s $f14, $f6
/* 0A6C78 7F074288 00000000 */  nop
/* 0A6C7C 7F07428C 4502000A */  bc1fl .L7F0742B8
/* 0A6C80 7F074290 44818000 */   mtc1  $at, $f16
/* 0A6C84 7F074294 3C013FA0 */  li    $at, 0x3FA00000 # 1.250000
/* 0A6C88 7F074298 44810000 */  mtc1  $at, $f0
/* 0A6C8C 7F07429C 00000000 */  nop
/* 0A6C90 7F0742A0 46001082 */  mul.s $f2, $f2, $f0
/* 0A6C94 7F0742A4 00000000 */  nop
/* 0A6C98 7F0742A8 46006302 */  mul.s $f12, $f12, $f0
/* 0A6C9C 7F0742AC 10000012 */  b     .L7F0742F8
/* 0A6CA0 7F0742B0 00000000 */   nop
/* 0A6CA4 7F0742B4 44818000 */  mtc1  $at, $f16
.L7F0742B8:
/* 0A6CA8 7F0742B8 3C014348 */  li    $at, 0x43480000 # 200.000000
/* 0A6CAC 7F0742BC 460E803C */  c.lt.s $f16, $f14
/* 0A6CB0 7F0742C0 00000000 */  nop
/* 0A6CB4 7F0742C4 45020005 */  bc1fl .L7F0742DC
/* 0A6CB8 7F0742C8 460E8201 */   sub.s $f8, $f16, $f14
/* 0A6CBC 7F0742CC 44806000 */  mtc1  $zero, $f12
/* 0A6CC0 7F0742D0 10000009 */  b     .L7F0742F8
/* 0A6CC4 7F0742D4 46006086 */   mov.s $f2, $f12
/* 0A6CC8 7F0742D8 460E8201 */  sub.s $f8, $f16, $f14
.L7F0742DC:
/* 0A6CCC 7F0742DC 44815000 */  mtc1  $at, $f10
/* 0A6CD0 7F0742E0 00000000 */  nop
/* 0A6CD4 7F0742E4 460A4003 */  div.s $f0, $f8, $f10
/* 0A6CD8 7F0742E8 46001082 */  mul.s $f2, $f2, $f0
/* 0A6CDC 7F0742EC 00000000 */  nop
/* 0A6CE0 7F0742F0 46006302 */  mul.s $f12, $f12, $f0
/* 0A6CE4 7F0742F4 00000000 */  nop
.L7F0742F8:
/* 0A6CE8 7F0742F8 8F3915DC */  lw    $t9, %lo(vtxallocator)($t9)
/* 0A6CEC 7F0742FC A7A30052 */  sh    $v1, 0x52($sp)
/* 0A6CF0 7F074300 AFA6006C */  sw    $a2, 0x6c($sp)
/* 0A6CF4 7F074304 E7A20048 */  swc1  $f2, 0x48($sp)
/* 0A6CF8 7F074308 0320F809 */  jalr  $t9
/* 0A6CFC 7F07430C E7AC0044 */  swc1  $f12, 0x44($sp)
/* 0A6D00 7F074310 27A80058 */  addiu $t0, $sp, 0x58
/* 0A6D04 7F074314 8D010000 */  lw    $at, ($t0)
/* 0A6D08 7F074318 87A30052 */  lh    $v1, 0x52($sp)
/* 0A6D0C 7F07431C 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A6D10 7F074320 C7A20048 */  lwc1  $f2, 0x48($sp)
/* 0A6D14 7F074324 C7AC0044 */  lwc1  $f12, 0x44($sp)
/* 0A6D18 7F074328 AC410000 */  sw    $at, ($v0)
/* 0A6D1C 7F07432C 8D0D0004 */  lw    $t5, 4($t0)
/* 0A6D20 7F074330 AC4D0004 */  sw    $t5, 4($v0)
/* 0A6D24 7F074334 8D010008 */  lw    $at, 8($t0)
/* 0A6D28 7F074338 AC410008 */  sw    $at, 8($v0)
/* 0A6D2C 7F07433C 8D0D000C */  lw    $t5, 0xc($t0)
/* 0A6D30 7F074340 AC4D000C */  sw    $t5, 0xc($v0)
/* 0A6D34 7F074344 8D010000 */  lw    $at, ($t0)
/* 0A6D38 7F074348 AC410010 */  sw    $at, 0x10($v0)
/* 0A6D3C 7F07434C 8D090004 */  lw    $t1, 4($t0)
/* 0A6D40 7F074350 AC490014 */  sw    $t1, 0x14($v0)
/* 0A6D44 7F074354 8D010008 */  lw    $at, 8($t0)
/* 0A6D48 7F074358 AC410018 */  sw    $at, 0x18($v0)
/* 0A6D4C 7F07435C 8D09000C */  lw    $t1, 0xc($t0)
/* 0A6D50 7F074360 AC49001C */  sw    $t1, 0x1c($v0)
/* 0A6D54 7F074364 8D010000 */  lw    $at, ($t0)
/* 0A6D58 7F074368 AC410020 */  sw    $at, 0x20($v0)
/* 0A6D5C 7F07436C 8D190004 */  lw    $t9, 4($t0)
/* 0A6D60 7F074370 AC590024 */  sw    $t9, 0x24($v0)
/* 0A6D64 7F074374 8D010008 */  lw    $at, 8($t0)
/* 0A6D68 7F074378 AC410028 */  sw    $at, 0x28($v0)
/* 0A6D6C 7F07437C 8D19000C */  lw    $t9, 0xc($t0)
/* 0A6D70 7F074380 AC59002C */  sw    $t9, 0x2c($v0)
/* 0A6D74 7F074384 8D010000 */  lw    $at, ($t0)
/* 0A6D78 7F074388 AC410030 */  sw    $at, 0x30($v0)
/* 0A6D7C 7F07438C 8D0D0004 */  lw    $t5, 4($t0)
/* 0A6D80 7F074390 AC4D0034 */  sw    $t5, 0x34($v0)
/* 0A6D84 7F074394 8D010008 */  lw    $at, 8($t0)
/* 0A6D88 7F074398 AC410038 */  sw    $at, 0x38($v0)
/* 0A6D8C 7F07439C 8D0D000C */  lw    $t5, 0xc($t0)
/* 0A6D90 7F0743A0 AC4D003C */  sw    $t5, 0x3c($v0)
/* 0A6D94 7F0743A4 C4D20000 */  lwc1  $f18, ($a2)
/* 0A6D98 7F0743A8 A4430002 */  sh    $v1, 2($v0)
/* 0A6D9C 7F0743AC 3C0DBC00 */  lui   $t5, (0xBC001406 >> 16) # lui $t5, 0xbc00
/* 0A6DA0 7F0743B0 46029101 */  sub.s $f4, $f18, $f2
/* 0A6DA4 7F0743B4 35AD1406 */  ori   $t5, (0xBC001406 & 0xFFFF) # ori $t5, $t5, 0x1406
/* 0A6DA8 7F0743B8 4600218D */  trunc.w.s $f6, $f4
/* 0A6DAC 7F0743BC 440E3000 */  mfc1  $t6, $f6
/* 0A6DB0 7F0743C0 00000000 */  nop
/* 0A6DB4 7F0743C4 A44E0000 */  sh    $t6, ($v0)
/* 0A6DB8 7F0743C8 C4C80004 */  lwc1  $f8, 4($a2)
/* 0A6DBC 7F0743CC 460C4281 */  sub.s $f10, $f8, $f12
/* 0A6DC0 7F0743D0 4600548D */  trunc.w.s $f18, $f10
/* 0A6DC4 7F0743D4 440A9000 */  mfc1  $t2, $f18
/* 0A6DC8 7F0743D8 00000000 */  nop
/* 0A6DCC 7F0743DC A44A0004 */  sh    $t2, 4($v0)
/* 0A6DD0 7F0743E0 C4C40000 */  lwc1  $f4, ($a2)
/* 0A6DD4 7F0743E4 A4430012 */  sh    $v1, 0x12($v0)
/* 0A6DD8 7F0743E8 46022181 */  sub.s $f6, $f4, $f2
/* 0A6DDC 7F0743EC 4600320D */  trunc.w.s $f8, $f6
/* 0A6DE0 7F0743F0 44194000 */  mfc1  $t9, $f8
/* 0A6DE4 7F0743F4 00000000 */  nop
/* 0A6DE8 7F0743F8 A4590010 */  sh    $t9, 0x10($v0)
/* 0A6DEC 7F0743FC C4CA0004 */  lwc1  $f10, 4($a2)
/* 0A6DF0 7F074400 460C5480 */  add.s $f18, $f10, $f12
/* 0A6DF4 7F074404 4600910D */  trunc.w.s $f4, $f18
/* 0A6DF8 7F074408 440C2000 */  mfc1  $t4, $f4
/* 0A6DFC 7F07440C 00000000 */  nop
/* 0A6E00 7F074410 A44C0014 */  sh    $t4, 0x14($v0)
/* 0A6E04 7F074414 C4C60000 */  lwc1  $f6, ($a2)
/* 0A6E08 7F074418 A4430022 */  sh    $v1, 0x22($v0)
/* 0A6E0C 7F07441C 46023200 */  add.s $f8, $f6, $f2
/* 0A6E10 7F074420 4600428D */  trunc.w.s $f10, $f8
/* 0A6E14 7F074424 440F5000 */  mfc1  $t7, $f10
/* 0A6E18 7F074428 00000000 */  nop
/* 0A6E1C 7F07442C A44F0020 */  sh    $t7, 0x20($v0)
/* 0A6E20 7F074430 C4D20004 */  lwc1  $f18, 4($a2)
/* 0A6E24 7F074434 460C9100 */  add.s $f4, $f18, $f12
/* 0A6E28 7F074438 4600218D */  trunc.w.s $f6, $f4
/* 0A6E2C 7F07443C 44093000 */  mfc1  $t1, $f6
/* 0A6E30 7F074440 00000000 */  nop
/* 0A6E34 7F074444 A4490024 */  sh    $t1, 0x24($v0)
/* 0A6E38 7F074448 C4C80000 */  lwc1  $f8, ($a2)
/* 0A6E3C 7F07444C A4430032 */  sh    $v1, 0x32($v0)
/* 0A6E40 7F074450 46024280 */  add.s $f10, $f8, $f2
/* 0A6E44 7F074454 4600548D */  trunc.w.s $f18, $f10
/* 0A6E48 7F074458 44189000 */  mfc1  $t8, $f18
/* 0A6E4C 7F07445C 00000000 */  nop
/* 0A6E50 7F074460 A4580030 */  sh    $t8, 0x30($v0)
/* 0A6E54 7F074464 C4C40004 */  lwc1  $f4, 4($a2)
/* 0A6E58 7F074468 460C2181 */  sub.s $f6, $f4, $f12
/* 0A6E5C 7F07446C 4600320D */  trunc.w.s $f8, $f6
/* 0A6E60 7F074470 440B4000 */  mfc1  $t3, $f8
/* 0A6E64 7F074474 00000000 */  nop
/* 0A6E68 7F074478 A44B0034 */  sh    $t3, 0x34($v0)
/* 0A6E6C 7F07447C 8E03000C */  lw    $v1, 0xc($s0)
/* 0A6E70 7F074480 246C0008 */  addiu $t4, $v1, 8
/* 0A6E74 7F074484 AE0C000C */  sw    $t4, 0xc($s0)
/* 0A6E78 7F074488 AC6D0000 */  sw    $t5, ($v1)
/* 0A6E7C 7F07448C 8CC4001C */  lw    $a0, 0x1c($a2)
/* 0A6E80 7F074490 AFA20054 */  sw    $v0, 0x54($sp)
/* 0A6E84 7F074494 AFA6006C */  sw    $a2, 0x6c($sp)
/* 0A6E88 7F074498 0C003838 */  jal   osVirtualToPhysical
/* 0A6E8C 7F07449C AFA30038 */   sw    $v1, 0x38($sp)
/* 0A6E90 7F0744A0 8FA50038 */  lw    $a1, 0x38($sp)
/* 0A6E94 7F0744A4 8FA6006C */  lw    $a2, 0x6c($sp)
/* 0A6E98 7F0744A8 8FA70054 */  lw    $a3, 0x54($sp)
/* 0A6E9C 7F0744AC ACA20004 */  sw    $v0, 4($a1)
/* 0A6EA0 7F0744B0 8CC30010 */  lw    $v1, 0x10($a2)
/* 0A6EA4 7F0744B4 24060001 */  li    $a2, 1
/* 0A6EA8 7F0744B8 00002825 */  move  $a1, $zero
/* 0A6EAC 7F0744BC 1060001C */  beqz  $v1, .L7F074530
/* 0A6EB0 7F0744C0 02002025 */   move  $a0, $s0
/* 0A6EB4 7F0744C4 A4E00008 */  sh    $zero, 8($a3)
/* 0A6EB8 7F0744C8 A4E0000A */  sh    $zero, 0xa($a3)
/* 0A6EBC 7F0744CC 906F0004 */  lbu   $t7, 4($v1)
/* 0A6EC0 7F0744D0 A4E0001A */  sh    $zero, 0x1a($a3)
/* 0A6EC4 7F0744D4 00602825 */  move  $a1, $v1
/* 0A6EC8 7F0744D8 000F7140 */  sll   $t6, $t7, 5
/* 0A6ECC 7F0744DC 25C9FFFF */  addiu $t1, $t6, -1
/* 0A6ED0 7F0744E0 A4E90018 */  sh    $t1, 0x18($a3)
/* 0A6ED4 7F0744E4 906A0004 */  lbu   $t2, 4($v1)
/* 0A6ED8 7F0744E8 02002025 */  move  $a0, $s0
/* 0A6EDC 7F0744EC 24060004 */  li    $a2, 4
/* 0A6EE0 7F0744F0 000AC140 */  sll   $t8, $t2, 5
/* 0A6EE4 7F0744F4 2719FFFF */  addiu $t9, $t8, -1
/* 0A6EE8 7F0744F8 A4F90028 */  sh    $t9, 0x28($a3)
/* 0A6EEC 7F0744FC 906B0005 */  lbu   $t3, 5($v1)
/* 0A6EF0 7F074500 A4E00038 */  sh    $zero, 0x38($a3)
/* 0A6EF4 7F074504 000B6140 */  sll   $t4, $t3, 5
/* 0A6EF8 7F074508 258DFFFF */  addiu $t5, $t4, -1
/* 0A6EFC 7F07450C A4ED002A */  sh    $t5, 0x2a($a3)
/* 0A6F00 7F074510 906F0005 */  lbu   $t7, 5($v1)
/* 0A6F04 7F074514 000F7140 */  sll   $t6, $t7, 5
/* 0A6F08 7F074518 25C9FFFF */  addiu $t1, $t6, -1
/* 0A6F0C 7F07451C A4E9003A */  sh    $t1, 0x3a($a3)
/* 0A6F10 7F074520 0FC1CC67 */  jal   sub_GAME_7F073038
/* 0A6F14 7F074524 AFA70054 */   sw    $a3, 0x54($sp)
/* 0A6F18 7F074528 10000004 */  b     .L7F07453C
/* 0A6F1C 7F07452C 8FA70054 */   lw    $a3, 0x54($sp)
.L7F074530:
/* 0A6F20 7F074530 0FC1CC67 */  jal   sub_GAME_7F073038
/* 0A6F24 7F074534 AFA70054 */   sw    $a3, 0x54($sp)
/* 0A6F28 7F074538 8FA70054 */  lw    $a3, 0x54($sp)
.L7F07453C:
/* 0A6F2C 7F07453C 8E03000C */  lw    $v1, 0xc($s0)
/* 0A6F30 7F074540 3C18B700 */  lui   $t8, 0xb700
/* 0A6F34 7F074544 24192000 */  li    $t9, 8192
/* 0A6F38 7F074548 246A0008 */  addiu $t2, $v1, 8
/* 0A6F3C 7F07454C AE0A000C */  sw    $t2, 0xc($s0)
/* 0A6F40 7F074550 AC780000 */  sw    $t8, ($v1)
/* 0A6F44 7F074554 AC790004 */  sw    $t9, 4($v1)
/* 0A6F48 7F074558 8E03000C */  lw    $v1, 0xc($s0)
/* 0A6F4C 7F07455C 3C0C0102 */  lui   $t4, (0x01020040 >> 16) # lui $t4, 0x102
/* 0A6F50 7F074560 358C0040 */  ori   $t4, (0x01020040 & 0xFFFF) # ori $t4, $t4, 0x40
/* 0A6F54 7F074564 246B0008 */  addiu $t3, $v1, 8
/* 0A6F58 7F074568 AE0B000C */  sw    $t3, 0xc($s0)
/* 0A6F5C 7F07456C AC6C0000 */  sw    $t4, ($v1)
/* 0A6F60 7F074570 AFA70054 */  sw    $a3, 0x54($sp)
/* 0A6F64 7F074574 8FA40070 */  lw    $a0, 0x70($sp)
/* 0A6F68 7F074578 0C003838 */  jal   osVirtualToPhysical
/* 0A6F6C 7F07457C AFA3002C */   sw    $v1, 0x2c($sp)
/* 0A6F70 7F074580 8FA5002C */  lw    $a1, 0x2c($sp)
/* 0A6F74 7F074584 8FA40054 */  lw    $a0, 0x54($sp)
/* 0A6F78 7F074588 3C0F0430 */  lui   $t7, (0x04300040 >> 16) # lui $t7, 0x430
/* 0A6F7C 7F07458C ACA20004 */  sw    $v0, 4($a1)
/* 0A6F80 7F074590 8E03000C */  lw    $v1, 0xc($s0)
/* 0A6F84 7F074594 35EF0040 */  ori   $t7, (0x04300040 & 0xFFFF) # ori $t7, $t7, 0x40
/* 0A6F88 7F074598 246D0008 */  addiu $t5, $v1, 8
/* 0A6F8C 7F07459C AE0D000C */  sw    $t5, 0xc($s0)
/* 0A6F90 7F0745A0 AC6F0000 */  sw    $t7, ($v1)
/* 0A6F94 7F0745A4 0C003838 */  jal   osVirtualToPhysical
/* 0A6F98 7F0745A8 AFA30028 */   sw    $v1, 0x28($sp)
/* 0A6F9C 7F0745AC 8FA60028 */  lw    $a2, 0x28($sp)
/* 0A6FA0 7F0745B0 3C09B100 */  lui   $t1, (0xB1000002 >> 16) # lui $t1, 0xb100
/* 0A6FA4 7F0745B4 35290002 */  ori   $t1, (0xB1000002 & 0xFFFF) # ori $t1, $t1, 2
/* 0A6FA8 7F0745B8 ACC20004 */  sw    $v0, 4($a2)
/* 0A6FAC 7F0745BC 8E03000C */  lw    $v1, 0xc($s0)
/* 0A6FB0 7F0745C0 240A3210 */  li    $t2, 12816
/* 0A6FB4 7F0745C4 246E0008 */  addiu $t6, $v1, 8
/* 0A6FB8 7F0745C8 AE0E000C */  sw    $t6, 0xc($s0)
/* 0A6FBC 7F0745CC AC6A0004 */  sw    $t2, 4($v1)
/* 0A6FC0 7F0745D0 AC690000 */  sw    $t1, ($v1)
.L7F0745D4:
/* 0A6FC4 7F0745D4 8FBF001C */  lw    $ra, 0x1c($sp)
/* 0A6FC8 7F0745D8 8FB00018 */  lw    $s0, 0x18($sp)
/* 0A6FCC 7F0745DC 27BD0078 */  addiu $sp, $sp, 0x78
/* 0A6FD0 7F0745E0 03E00008 */  jr    $ra
/* 0A6FD4 7F0745E4 00000000 */   nop
)
#endif
#endif


void sub_GAME_7F074514(s32 param_1,struct Model *param_2,struct ModelNode *param_3)
{
    return;
}


void sub_GAME_7F074524(Gfx *param_1,struct Model *param_2, struct ModelNode *param_3)
{
    return;
}


void sub_GAME_7F074534(ModelRenderData* data, Model* model, ModelNode* node) {
    u32 id = node->Opcode & 0xFF;
    switch (id) {
    case MODELNODE_OPCODE_LOD:
        modelApplyDistanceRelations(model, node);
        return;
    case MODELNODE_OPCODE_SWITCH:
        modelApplyToggleRelations(model, node);
        return;
    case MODELNODE_OPCODE_HEAD:
        modelApplyHeadRelations(model, node);
        return;
    case MODELNODE_OPCODE_BSP:
        modelApplyReorderRelations(model, node);
        return;
    case MODELNODE_OPCODE_OP11:
        sub_GAME_7F0737FC(data, model, node);
        return;
    case MODELNODE_OPCODE_GUNFIRE:
        dogfnegx(data, model, node);
        return;
    case MODELNODE_OPCODE_SHADOW:
        doshadow(data, model, node);
        return;
    case MODELNODE_OPCODE_BBOX:
        sub_GAME_7F074514(data, model, node);
        return;
    case MODELNODE_OPCODE_OP17:
        sub_GAME_7F074524(data, model, node);
        return;
    case MODELNODE_OPCODE_DL:
        modelRenderNodeGundl(data, node);
        return;
    case MODELNODE_OPCODE_DLCOLLISION:
        modelRenderNodeDl(data, model, node);
        return;
    case MODELNODE_OPCODE_OP20:
        sub_GAME_7F072C10(data, model, node);
        return;
    case MODELNODE_OPCODE_DLPRIMARY:
        dorottex(data, node);
        return;
    case MODELNODE_OPCODE_OP05:
        sub_GAME_7F07306C(data, model, node);
        return;
    case MODELNODE_OPCODE_OP07:
        dotube(data, model, node);
        return;
    case MODELNODE_OPCODE_OP06:
        sub_GAME_7F0737EC(data,model,node);
        return;
    case MODELNODE_OPCODE_HEADER:
    case MODELNODE_OPCODE_GROUP:
    case MODELNODE_OPCODE_OP03:
    case MODELNODE_OPCODE_OP14:
    case MODELNODE_OPCODE_INTERLINK:
    case MODELNODE_OPCODE_OP16:
    default:
        return;
    }
}


#if 1
void subdraw(ModelRenderData *mrData, Model *mdl) // Matches
{
    ModelNode *root = mdl->obj->RootNode;
    #if defined(LEFTOVERDEBUG)

    if (mrData->gdl == NULL)
    {
        osSyncPrintf("subdraw: no gfxlist!\n");
        return_null();
    }

    if (mdl->obj->isLoaded)
    {
    }
    else
    {
        osSyncPrintf("subdraw: object not initialised! (0x%X)\n", (u32)mdl->obj);
        return_null();
    }
    #endif
    gSPSegment(mrData->gdl++, 3, osVirtualToPhysical(mdl->render_pos));

    while (root != NULL)
    {
        sub_GAME_7F074534(mrData, mdl, root);

        if (root->Child)
        {
            root = root->Child;
        }
        else
        {
            while (root)
            {
                if (root->Next)
                {
                    root = root->Next;
                    break;
                }
                root = root->Parent;
            }
        }
    }
}

#else
#ifndef VERSION_EU
//D:80054AB4
const char aSubdrawNoGfxlist[] = "subdraw: no gfxlist!\n";
//D:80054ACC
const char aSubdrawObjectNotInitialised0xX[] = "subdraw: object not initialised! (0x%X)\n";
GLOBAL_ASM(
.text
glabel subdraw
/* 0A91B4 7F074684 27BDFFD0 */  addiu $sp, $sp, -0x30
/* 0A91B8 7F074688 AFBF0024 */  sw    $ra, 0x24($sp)
/* 0A91BC 7F07468C AFB20020 */  sw    $s2, 0x20($sp)
/* 0A91C0 7F074690 AFB1001C */  sw    $s1, 0x1c($sp)
/* 0A91C4 7F074694 AFB00018 */  sw    $s0, 0x18($sp)
/* 0A91C8 7F074698 8C8E000C */  lw    $t6, 0xc($a0)
/* 0A91CC 7F07469C 8CA20008 */  lw    $v0, 8($a1)
/* 0A91D0 7F0746A0 00808825 */  move  $s1, $a0
/* 0A91D4 7F0746A4 00A09025 */  move  $s2, $a1
/* 0A91D8 7F0746A8 15C00007 */  bnez  $t6, .L7F0746C8
/* 0A91DC 7F0746AC 8C500000 */   lw    $s0, ($v0)
/* 0A91E0 7F0746B0 3C048005 */  lui   $a0, %hi(aSubdrawNoGfxlist)
/* 0A91E4 7F0746B4 0C0033D1 */  jal   osSyncPrintf
/* 0A91E8 7F0746B8 24844AB4 */   addiu $a0, %lo(aSubdrawNoGfxlist) # addiu $a0, $a0, 0x4ab4
/* 0A91EC 7F0746BC 0FC1B11B */  jal   return_null
/* 0A91F0 7F0746C0 00000000 */   nop
/* 0A91F4 7F0746C4 8E420008 */  lw    $v0, 8($s2)
.L7F0746C8:
/* 0A91F8 7F0746C8 8C4F001C */  lw    $t7, 0x1c($v0)
/* 0A91FC 7F0746CC 3C048005 */  lui   $a0, %hi(aSubdrawObjectNotInitialised0xX)
/* 0A9200 7F0746D0 24844ACC */  addiu $a0, %lo(aSubdrawObjectNotInitialised0xX) # addiu $a0, $a0, 0x4acc
/* 0A9204 7F0746D4 11E00003 */  beqz  $t7, .L7F0746E4
/* 0A9208 7F0746D8 00000000 */   nop
/* 0A920C 7F0746DC 10000006 */  b     .L7F0746F8
/* 0A9210 7F0746E0 8E22000C */   lw    $v0, 0xc($s1)
.L7F0746E4:
/* 0A9214 7F0746E4 0C0033D1 */  jal   osSyncPrintf
/* 0A9218 7F0746E8 00402825 */   move  $a1, $v0
/* 0A921C 7F0746EC 0FC1B11B */  jal   return_null
/* 0A9220 7F0746F0 00000000 */   nop
/* 0A9224 7F0746F4 8E22000C */  lw    $v0, 0xc($s1)
.L7F0746F8:
/* 0A9228 7F0746F8 24580008 */  addiu $t8, $v0, 8
/* 0A922C 7F0746FC 3C19BC00 */  lui   $t9, (0xBC000C06 >> 16) # lui $t9, 0xbc00
/* 0A9230 7F074700 37390C06 */  ori   $t9, (0xBC000C06 & 0xFFFF) # ori $t9, $t9, 0xc06
/* 0A9234 7F074704 AE38000C */  sw    $t8, 0xc($s1)
/* 0A9238 7F074708 AC590000 */  sw    $t9, ($v0)
/* 0A923C 7F07470C 8E44000C */  lw    $a0, 0xc($s2)
/* 0A9240 7F074710 0C003A2C */  jal   osVirtualToPhysical
/* 0A9244 7F074714 AFA20028 */   sw    $v0, 0x28($sp)
/* 0A9248 7F074718 8FA30028 */  lw    $v1, 0x28($sp)
/* 0A924C 7F07471C 12000016 */  beqz  $s0, .L7F074778
/* 0A9250 7F074720 AC620004 */   sw    $v0, 4($v1)
/* 0A9254 7F074724 02202025 */  move  $a0, $s1
.L7F074728:
/* 0A9258 7F074728 02402825 */  move  $a1, $s2
/* 0A925C 7F07472C 0FC1D14D */  jal   sub_GAME_7F074534
/* 0A9260 7F074730 02003025 */   move  $a2, $s0
/* 0A9264 7F074734 8E020014 */  lw    $v0, 0x14($s0)
/* 0A9268 7F074738 10400003 */  beqz  $v0, .L7F074748
/* 0A926C 7F07473C 00000000 */   nop
/* 0A9270 7F074740 1000000B */  b     .L7F074770
/* 0A9274 7F074744 00408025 */   move  $s0, $v0
.L7F074748:
/* 0A9278 7F074748 12000009 */  beqz  $s0, .L7F074770
/* 0A927C 7F07474C 00000000 */   nop
/* 0A9280 7F074750 8E02000C */  lw    $v0, 0xc($s0)
.L7F074754:
/* 0A9284 7F074754 50400004 */  beql  $v0, $zero, .L7F074768
/* 0A9288 7F074758 8E100008 */   lw    $s0, 8($s0)
/* 0A928C 7F07475C 10000004 */  b     .L7F074770
/* 0A9290 7F074760 00408025 */   move  $s0, $v0
/* 0A9294 7F074764 8E100008 */  lw    $s0, 8($s0)
.L7F074768:
/* 0A9298 7F074768 5600FFFA */  bnezl $s0, .L7F074754
/* 0A929C 7F07476C 8E02000C */   lw    $v0, 0xc($s0)
.L7F074770:
/* 0A92A0 7F074770 5600FFED */  bnezl $s0, .L7F074728
/* 0A92A4 7F074774 02202025 */   move  $a0, $s1
.L7F074778:
/* 0A92A8 7F074778 8FBF0024 */  lw    $ra, 0x24($sp)
/* 0A92AC 7F07477C 8FB00018 */  lw    $s0, 0x18($sp)
/* 0A92B0 7F074780 8FB1001C */  lw    $s1, 0x1c($sp)
/* 0A92B4 7F074784 8FB20020 */  lw    $s2, 0x20($sp)
/* 0A92B8 7F074788 03E00008 */  jr    $ra
/* 0A92BC 7F07478C 27BD0030 */   addiu $sp, $sp, 0x30
)
#endif
#ifdef VERSION_EU
GLOBAL_ASM(
.text
glabel subdraw
/* 0A7148 7F074758 27BDFFD0 */  addiu $sp, $sp, -0x30
/* 0A714C 7F07475C AFBF0024 */  sw    $ra, 0x24($sp)
/* 0A7150 7F074760 AFB20020 */  sw    $s2, 0x20($sp)
/* 0A7154 7F074764 AFB1001C */  sw    $s1, 0x1c($sp)
/* 0A7158 7F074768 AFB00018 */  sw    $s0, 0x18($sp)
/* 0A715C 7F07476C 8CAE0008 */  lw    $t6, 8($a1)
/* 0A7160 7F074770 8C82000C */  lw    $v0, 0xc($a0)
/* 0A7164 7F074774 3C18BC00 */  lui   $t8, (0xBC000C06 >> 16) # lui $t8, 0xbc00
/* 0A7168 7F074778 8DD00000 */  lw    $s0, ($t6)
/* 0A716C 7F07477C 244F0008 */  addiu $t7, $v0, 8
/* 0A7170 7F074780 AC8F000C */  sw    $t7, 0xc($a0)
/* 0A7174 7F074784 37180C06 */  ori   $t8, (0xBC000C06 & 0xFFFF) # ori $t8, $t8, 0xc06
/* 0A7178 7F074788 00809025 */  move  $s2, $a0
/* 0A717C 7F07478C AC580000 */  sw    $t8, ($v0)
/* 0A7180 7F074790 8CA4000C */  lw    $a0, 0xc($a1)
/* 0A7184 7F074794 00A08825 */  move  $s1, $a1
/* 0A7188 7F074798 0C003838 */  jal   osVirtualToPhysical
/* 0A718C 7F07479C AFA20028 */   sw    $v0, 0x28($sp)
/* 0A7190 7F0747A0 8FA30028 */  lw    $v1, 0x28($sp)
/* 0A7194 7F0747A4 12000016 */  beqz  $s0, .L7F074800
/* 0A7198 7F0747A8 AC620004 */   sw    $v0, 4($v1)
/* 0A719C 7F0747AC 02402025 */  move  $a0, $s2
.L7F0747B0:
/* 0A71A0 7F0747B0 02202825 */  move  $a1, $s1
/* 0A71A4 7F0747B4 0FC1D182 */  jal   sub_GAME_7F074534
/* 0A71A8 7F0747B8 02003025 */   move  $a2, $s0
/* 0A71AC 7F0747BC 8E020014 */  lw    $v0, 0x14($s0)
/* 0A71B0 7F0747C0 10400003 */  beqz  $v0, .L7F0747D0
/* 0A71B4 7F0747C4 00000000 */   nop
/* 0A71B8 7F0747C8 1000000B */  b     .L7F0747F8
/* 0A71BC 7F0747CC 00408025 */   move  $s0, $v0
.L7F0747D0:
/* 0A71C0 7F0747D0 12000009 */  beqz  $s0, .L7F0747F8
/* 0A71C4 7F0747D4 00000000 */   nop
/* 0A71C8 7F0747D8 8E02000C */  lw    $v0, 0xc($s0)
.L7F0747DC:
/* 0A71CC 7F0747DC 50400004 */  beql  $v0, $zero, .L7F0747F0
/* 0A71D0 7F0747E0 8E100008 */   lw    $s0, 8($s0)
/* 0A71D4 7F0747E4 10000004 */  b     .L7F0747F8
/* 0A71D8 7F0747E8 00408025 */   move  $s0, $v0
/* 0A71DC 7F0747EC 8E100008 */  lw    $s0, 8($s0)
.L7F0747F0:
/* 0A71E0 7F0747F0 5600FFFA */  bnezl $s0, .L7F0747DC
/* 0A71E4 7F0747F4 8E02000C */   lw    $v0, 0xc($s0)
.L7F0747F8:
/* 0A71E8 7F0747F8 5600FFED */  bnezl $s0, .L7F0747B0
/* 0A71EC 7F0747FC 02402025 */   move  $a0, $s2
.L7F074800:
/* 0A71F0 7F074800 8FBF0024 */  lw    $ra, 0x24($sp)
/* 0A71F4 7F074804 8FB00018 */  lw    $s0, 0x18($sp)
/* 0A71F8 7F074808 8FB1001C */  lw    $s1, 0x1c($sp)
/* 0A71FC 7F07480C 8FB20020 */  lw    $s2, 0x20($sp)
/* 0A7200 7F074810 03E00008 */  jr    $ra
/* 0A7204 7F074814 27BD0030 */   addiu $sp, $sp, 0x30
)
#endif
#endif


// unreferenced
void sub_GAME_7F074790(ModelRenderData* arg0, Model* arg1)
{
    subcalcpos(arg1);
    subcalcmatrices(arg0, arg1);
    subdraw((s32) arg0, arg1);
}


/**
 * Address: 7F0747D0
 * 
 * Ray vs transformed bounding box test. The bbox is transformed by mtx while the ray
 * is defined by pos and dir.
 * 
 * @returns TRUE if the ray intersects the bbox, otherwise FALSE.
 */
bool modelTestRayIntersectsTransformedBBox(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, coord3d *pos, coord3d *dir)
{
    f32 xthingx;
    f32 xthingy;
    f32 xthingz;
    u32 stack1[1];
    f32 xpmin;
    f32 xpmax;
    f32 xsum1;
    f32 xsum2;
    f32 xsum3;
    f32 negL2;
    f32 pmin;
    f32 ythingx;
    f32 ythingy;
    f32 ythingz;
    f32 pmax;
    f32 ypmin;
    f32 ypmax;
    f32 ysum1;
    f32 ysum2;
    f32 ysum3;
    f32 mult1;
    f32 mult2;
    f32 bestsum2;
    f32 bestsum1;
    f32 anotherbestsum3;
    f32 anotherbestsum1;
    f32 mult3;
    f32 mult4;
    f32 zthingx;
    f32 zthingy;
    f32 zthingz;
    u32 stack3[1];
    f32 zpmin;
    f32 zpmax;
    f32 zsum1;
    f32 zsum2;
    f32 zsum3;

    xthingx = mtx->m[0][0] * mtx->m[0][0];
    xthingy = mtx->m[0][1] * mtx->m[0][1];
    xthingz = mtx->m[0][2] * mtx->m[0][2];

    negL2 = -((xthingx + xthingy) + xthingz);

    xpmin = negL2;
    xpmin *= bbox->Bounds.xmin;

    xpmax = negL2;
    xpmax *= bbox->Bounds.xmax;

    xsum1 = ((mtx->m[0][0] * dir->f[0]) + (mtx->m[0][1] * dir->f[1])) + (mtx->m[0][2] * dir->f[2]);
    xsum2 = ((mtx->m[0][0] * (pos->f[0] - mtx->m[3][0])) + (mtx->m[0][1] * (pos->f[1] - mtx->m[3][1]))) + (mtx->m[0][2] * (pos->f[2] - mtx->m[3][2]));

    xsum3 = -(xsum2 + xpmax);
    xsum2 = -(xsum2 + xpmin);

    if (xsum1 < 0.0f)
    {
        xsum1 = -xsum1;
        xsum2 = -xsum2;
        xsum3 = -xsum3;
    }

    if ((xsum2 < 0.0f) && (xsum3 < 0.0f))
    {
        return FALSE;
    }

    if (xsum3 < xsum2)
    {
        f32 tmp = xsum2;

        xsum2 = xsum3;
        xsum3 = tmp;
    }

    ythingx = mtx->m[1][0] * mtx->m[1][0];
    ythingy = mtx->m[1][1] * mtx->m[1][1];
    ythingz = mtx->m[1][2] * mtx->m[1][2];

    negL2 = -((ythingx + ythingy) + ythingz);

    ypmin = negL2;
    ypmin *= bbox->Bounds.ymin;

    ypmax = negL2;
    ypmax *= bbox->Bounds.ymax;

    ysum1 = ((mtx->m[1][0] * dir->f[0]) + (mtx->m[1][1] * dir->f[1])) + (mtx->m[1][2] * dir->f[2]);
    ysum2 = ((mtx->m[1][0] * (pos->f[0] - mtx->m[3][0])) + (mtx->m[1][1] * (pos->f[1] - mtx->m[3][1]))) + (mtx->m[1][2] * (pos->f[2] - mtx->m[3][2]));

    ysum3 = -(ysum2 + ypmax);
    ysum2 = -(ysum2 + ypmin);

    if (ysum1 < 0.0f)
    {
        ysum1 = -ysum1;
        ysum2 = -ysum2;
        ysum3 = -ysum3;
    }

    if ((ysum2 < 0.0f) && (ysum3 < 0.0f))
    {
        return FALSE;
    }

    if (ysum3 < ysum2)
    {
        f32 tmp = ysum2;

        ysum2 = ysum3;
        ysum3 = tmp;
    }

    mult1 = ysum2 * xsum1;
    mult2 = xsum2 * ysum1;
    mult3 = xsum3 * ysum1;
    mult4 = ysum3 * xsum1;

    if (mult1 < mult2)
    {
        if (mult4 < mult2)
        {
            return FALSE;
        }

        bestsum2 = xsum2;
        bestsum1 = xsum1;
    }
    else
    {
        if (mult3 < mult1)
        {
            return FALSE;
        }

        bestsum2 = ysum2;
        bestsum1 = ysum1;
    }

    if (mult3 < mult4)
    {
        anotherbestsum3 = xsum3;
        anotherbestsum1 = xsum1;
    }
    else
    {
        anotherbestsum3 = ysum3;
        anotherbestsum1 = ysum1;
    }

    zthingx = mtx->m[2][0] * mtx->m[2][0];
    zthingy = mtx->m[2][1] * mtx->m[2][1];
    zthingz = mtx->m[2][2] * mtx->m[2][2];

    negL2 = -((zthingx + zthingy) + zthingz);

    zpmin = negL2;
    zpmin *= bbox->Bounds.zmin;

    zpmax = negL2;
    zpmax *= bbox->Bounds.zmax;

    zsum1 = ((mtx->m[2][0] * dir->f[0]) + (mtx->m[2][1] * dir->f[1])) + (mtx->m[2][2] * dir->f[2]);
    zsum2 = ((mtx->m[2][0] * (pos->f[0] - mtx->m[3][0])) + (mtx->m[2][1] * (pos->f[1] - mtx->m[3][1]))) + (mtx->m[2][2] * (pos->f[2] - mtx->m[3][2]));

    zsum3 = -(zsum2 + zpmax);
    zsum2 = -(zsum2 + zpmin);

    if (zsum1 < 0.0f)
    {
        zsum1 = -zsum1;
        zsum2 = -zsum2;
        zsum3 = -zsum3;
    }

    if ((zsum2 < 0.0f) && (zsum3 < 0.0f))
    {
        return FALSE;
    }

    if (zsum3 < zsum2)
    {
        f32 tmp = zsum2;

        zsum2 = zsum3;
        zsum3 = tmp;
    }

    if ((bestsum2 * zsum1) < (zsum2 * bestsum1))
    {
        if ((anotherbestsum3 * zsum1) < (zsum2 * anotherbestsum1))
        {
            return FALSE;
        }
    }
    else if ((zsum3 * bestsum1) < (bestsum2 * zsum1))
    {
        return FALSE;
    }

    return TRUE;
}


/**
 * Address: 7F074C68
 */
bool modelTestRayIntersectsNodeBBox(Model *model, ModelNode *node, coord3d *pos, coord3d *dir)
{
    ModelRoData_BoundingBoxRecord *bbox = &node->Data->BoundingBox;

    return modelTestRayIntersectsTransformedBBox(bbox, modelFindNodeMtx(model, node, 0), pos, dir);
}


#ifdef NONMATCHING
void sub_GAME_7F074CAC(void) {

}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F074CAC
/* 0A97DC 7F074CAC 27BDFF50 */  addiu $sp, $sp, -0xb0
/* 0A97E0 7F074CB0 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0A97E4 7F074CB4 AFB00018 */  sw    $s0, 0x18($sp)
/* 0A97E8 7F074CB8 AFA400B0 */  sw    $a0, 0xb0($sp)
/* 0A97EC 7F074CBC 8CA80004 */  lw    $t0, 4($a1)
/* 0A97F0 7F074CC0 00C08025 */  move  $s0, $a2
/* 0A97F4 7F074CC4 00003025 */  move  $a2, $zero
/* 0A97F8 7F074CC8 AFA700BC */  sw    $a3, 0xbc($sp)
/* 0A97FC 7F074CCC AFA500B4 */  sw    $a1, 0xb4($sp)
/* 0A9800 7F074CD0 0FC1B198 */  jal   modelFindNodeMtx
/* 0A9804 7F074CD4 AFA800AC */   sw    $t0, 0xac($sp)
/* 0A9808 7F074CD8 3C0B8003 */  lui   $t3, %hi(D_80036408)
/* 0A980C 7F074CDC 256B6408 */  addiu $t3, %lo(D_80036408) # addiu $t3, $t3, 0x6408
/* 0A9810 7F074CE0 8D610000 */  lw    $at, ($t3)
/* 0A9814 7F074CE4 8FA700BC */  lw    $a3, 0xbc($sp)
/* 0A9818 7F074CE8 8FA800AC */  lw    $t0, 0xac($sp)
/* 0A981C 7F074CEC 27AA0090 */  addiu $t2, $sp, 0x90
/* 0A9820 7F074CF0 AD410000 */  sw    $at, ($t2)
/* 0A9824 7F074CF4 8D610008 */  lw    $at, 8($t3)
/* 0A9828 7F074CF8 8D790004 */  lw    $t9, 4($t3)
/* 0A982C 7F074CFC 00401825 */  move  $v1, $v0
/* 0A9830 7F074D00 AD410008 */  sw    $at, 8($t2)
/* 0A9834 7F074D04 AD590004 */  sw    $t9, 4($t2)
/* 0A9838 7F074D08 C5080004 */  lwc1  $f8, 4($t0)
/* 0A983C 7F074D0C 8FB800B4 */  lw    $t8, 0xb4($sp)
/* 0A9840 7F074D10 E7A8008C */  swc1  $f8, 0x8c($sp)
/* 0A9844 7F074D14 C4E60000 */  lwc1  $f6, ($a3)
/* 0A9848 7F074D18 E7A6009C */  swc1  $f6, 0x9c($sp)
/* 0A984C 7F074D1C C4EA0004 */  lwc1  $f10, 4($a3)
/* 0A9850 7F074D20 E7AA00A0 */  swc1  $f10, 0xa0($sp)
/* 0A9854 7F074D24 C4E40008 */  lwc1  $f4, 8($a3)
/* 0A9858 7F074D28 E7A400A4 */  swc1  $f4, 0xa4($sp)
/* 0A985C 7F074D2C 97040000 */  lhu   $a0, ($t8)
/* 0A9860 7F074D30 308E0100 */  andi  $t6, $a0, 0x100
/* 0A9864 7F074D34 11C0003C */  beqz  $t6, .L7F074E28
/* 0A9868 7F074D38 308F0200 */   andi  $t7, $a0, 0x200
/* 0A986C 7F074D3C C5080008 */  lwc1  $f8, 8($t0)
/* 0A9870 7F074D40 E7A80080 */  swc1  $f8, 0x80($sp)
/* 0A9874 7F074D44 C506000C */  lwc1  $f6, 0xc($t0)
/* 0A9878 7F074D48 C7A40080 */  lwc1  $f4, 0x80($sp)
/* 0A987C 7F074D4C E7A60084 */  swc1  $f6, 0x84($sp)
/* 0A9880 7F074D50 C50A0010 */  lwc1  $f10, 0x10($t0)
/* 0A9884 7F074D54 E7AA0088 */  swc1  $f10, 0x88($sp)
/* 0A9888 7F074D58 C4480000 */  lwc1  $f8, ($v0)
/* 0A988C 7F074D5C C7AA0084 */  lwc1  $f10, 0x84($sp)
/* 0A9890 7F074D60 46082182 */  mul.s $f6, $f4, $f8
/* 0A9894 7F074D64 C4480010 */  lwc1  $f8, 0x10($v0)
/* 0A9898 7F074D68 E7A40020 */  swc1  $f4, 0x20($sp)
/* 0A989C 7F074D6C C4440020 */  lwc1  $f4, 0x20($v0)
/* 0A98A0 7F074D70 46085202 */  mul.s $f8, $f10, $f8
/* 0A98A4 7F074D74 46083180 */  add.s $f6, $f6, $f8
/* 0A98A8 7F074D78 C7A80088 */  lwc1  $f8, 0x88($sp)
/* 0A98AC 7F074D7C 46044102 */  mul.s $f4, $f8, $f4
/* 0A98B0 7F074D80 46043180 */  add.s $f6, $f6, $f4
/* 0A98B4 7F074D84 C4440030 */  lwc1  $f4, 0x30($v0)
/* 0A98B8 7F074D88 E7AA0024 */  swc1  $f10, 0x24($sp)
/* 0A98BC 7F074D8C C60A0000 */  lwc1  $f10, ($s0)
/* 0A98C0 7F074D90 460A2101 */  sub.s $f4, $f4, $f10
/* 0A98C4 7F074D94 46062280 */  add.s $f10, $f4, $f6
/* 0A98C8 7F074D98 C7A40020 */  lwc1  $f4, 0x20($sp)
/* 0A98CC 7F074D9C E7AA0090 */  swc1  $f10, 0x90($sp)
/* 0A98D0 7F074DA0 C4460004 */  lwc1  $f6, 4($v0)
/* 0A98D4 7F074DA4 46062282 */  mul.s $f10, $f4, $f6
/* 0A98D8 7F074DA8 C7A60024 */  lwc1  $f6, 0x24($sp)
/* 0A98DC 7F074DAC E7A80024 */  swc1  $f8, 0x24($sp)
/* 0A98E0 7F074DB0 C4480014 */  lwc1  $f8, 0x14($v0)
/* 0A98E4 7F074DB4 46083202 */  mul.s $f8, $f6, $f8
/* 0A98E8 7F074DB8 46085280 */  add.s $f10, $f10, $f8
/* 0A98EC 7F074DBC C7A80024 */  lwc1  $f8, 0x24($sp)
/* 0A98F0 7F074DC0 E7A40024 */  swc1  $f4, 0x24($sp)
/* 0A98F4 7F074DC4 C4440024 */  lwc1  $f4, 0x24($v0)
/* 0A98F8 7F074DC8 46044102 */  mul.s $f4, $f8, $f4
/* 0A98FC 7F074DCC 46045280 */  add.s $f10, $f10, $f4
/* 0A9900 7F074DD0 C4440034 */  lwc1  $f4, 0x34($v0)
/* 0A9904 7F074DD4 E7A60020 */  swc1  $f6, 0x20($sp)
/* 0A9908 7F074DD8 C6060004 */  lwc1  $f6, 4($s0)
/* 0A990C 7F074DDC 46062101 */  sub.s $f4, $f4, $f6
/* 0A9910 7F074DE0 460A2180 */  add.s $f6, $f4, $f10
/* 0A9914 7F074DE4 C7A40024 */  lwc1  $f4, 0x24($sp)
/* 0A9918 7F074DE8 E7A60094 */  swc1  $f6, 0x94($sp)
/* 0A991C 7F074DEC C44A0008 */  lwc1  $f10, 8($v0)
/* 0A9920 7F074DF0 460A2182 */  mul.s $f6, $f4, $f10
/* 0A9924 7F074DF4 C44A0018 */  lwc1  $f10, 0x18($v0)
/* 0A9928 7F074DF8 C7A40020 */  lwc1  $f4, 0x20($sp)
/* 0A992C 7F074DFC 460A2102 */  mul.s $f4, $f4, $f10
/* 0A9930 7F074E00 46043280 */  add.s $f10, $f6, $f4
/* 0A9934 7F074E04 C4460028 */  lwc1  $f6, 0x28($v0)
/* 0A9938 7F074E08 46064102 */  mul.s $f4, $f8, $f6
/* 0A993C 7F074E0C C4460038 */  lwc1  $f6, 0x38($v0)
/* 0A9940 7F074E10 46045200 */  add.s $f8, $f10, $f4
/* 0A9944 7F074E14 C60A0008 */  lwc1  $f10, 8($s0)
/* 0A9948 7F074E18 460A3101 */  sub.s $f4, $f6, $f10
/* 0A994C 7F074E1C 46082180 */  add.s $f6, $f4, $f8
/* 0A9950 7F074E20 1000003E */  b     .L7F074F1C
/* 0A9954 7F074E24 E7A60098 */   swc1  $f6, 0x98($sp)
.L7F074E28:
/* 0A9958 7F074E28 51E00031 */  beql  $t7, $zero, .L7F074EF0
/* 0A995C 7F074E2C C4460030 */   lwc1  $f6, 0x30($v0)
/* 0A9960 7F074E30 8D050014 */  lw    $a1, 0x14($t0)
/* 0A9964 7F074E34 8FA400B0 */  lw    $a0, 0xb0($sp)
/* 0A9968 7F074E38 00003025 */  move  $a2, $zero
/* 0A996C 7F074E3C 50A0001F */  beql  $a1, $zero, .L7F074EBC
/* 0A9970 7F074E40 C4480030 */   lwc1  $f8, 0x30($v0)
/* 0A9974 7F074E44 AFA300A8 */  sw    $v1, 0xa8($sp)
/* 0A9978 7F074E48 0FC1B198 */  jal   modelFindNodeMtx
/* 0A997C 7F074E4C AFA800AC */   sw    $t0, 0xac($sp)
/* 0A9980 7F074E50 8FA300A8 */  lw    $v1, 0xa8($sp)
/* 0A9984 7F074E54 C4440030 */  lwc1  $f4, 0x30($v0)
/* 0A9988 7F074E58 3C013F00 */  li    $at, 0x3F000000 # 0.500000
/* 0A998C 7F074E5C C46A0030 */  lwc1  $f10, 0x30($v1)
/* 0A9990 7F074E60 44810000 */  mtc1  $at, $f0
/* 0A9994 7F074E64 8FA800AC */  lw    $t0, 0xac($sp)
/* 0A9998 7F074E68 46045200 */  add.s $f8, $f10, $f4
/* 0A999C 7F074E6C C60A0000 */  lwc1  $f10, ($s0)
/* 0A99A0 7F074E70 46004182 */  mul.s $f6, $f8, $f0
/* 0A99A4 7F074E74 460A3101 */  sub.s $f4, $f6, $f10
/* 0A99A8 7F074E78 E7A40090 */  swc1  $f4, 0x90($sp)
/* 0A99AC 7F074E7C C4460034 */  lwc1  $f6, 0x34($v0)
/* 0A99B0 7F074E80 C4680034 */  lwc1  $f8, 0x34($v1)
/* 0A99B4 7F074E84 46064280 */  add.s $f10, $f8, $f6
/* 0A99B8 7F074E88 C6080004 */  lwc1  $f8, 4($s0)
/* 0A99BC 7F074E8C 46005102 */  mul.s $f4, $f10, $f0
/* 0A99C0 7F074E90 46082181 */  sub.s $f6, $f4, $f8
/* 0A99C4 7F074E94 E7A60094 */  swc1  $f6, 0x94($sp)
/* 0A99C8 7F074E98 C4440038 */  lwc1  $f4, 0x38($v0)
/* 0A99CC 7F074E9C C46A0038 */  lwc1  $f10, 0x38($v1)
/* 0A99D0 7F074EA0 46045200 */  add.s $f8, $f10, $f4
/* 0A99D4 7F074EA4 C60A0008 */  lwc1  $f10, 8($s0)
/* 0A99D8 7F074EA8 46004182 */  mul.s $f6, $f8, $f0
/* 0A99DC 7F074EAC 460A3101 */  sub.s $f4, $f6, $f10
/* 0A99E0 7F074EB0 1000001A */  b     .L7F074F1C
/* 0A99E4 7F074EB4 E7A40098 */   swc1  $f4, 0x98($sp)
/* 0A99E8 7F074EB8 C4480030 */  lwc1  $f8, 0x30($v0)
.L7F074EBC:
/* 0A99EC 7F074EBC C6060000 */  lwc1  $f6, ($s0)
/* 0A99F0 7F074EC0 46064281 */  sub.s $f10, $f8, $f6
/* 0A99F4 7F074EC4 E7AA0090 */  swc1  $f10, 0x90($sp)
/* 0A99F8 7F074EC8 C6080004 */  lwc1  $f8, 4($s0)
/* 0A99FC 7F074ECC C4440034 */  lwc1  $f4, 0x34($v0)
/* 0A9A00 7F074ED0 46082181 */  sub.s $f6, $f4, $f8
/* 0A9A04 7F074ED4 E7A60094 */  swc1  $f6, 0x94($sp)
/* 0A9A08 7F074ED8 C6040008 */  lwc1  $f4, 8($s0)
/* 0A9A0C 7F074EDC C44A0038 */  lwc1  $f10, 0x38($v0)
/* 0A9A10 7F074EE0 46045201 */  sub.s $f8, $f10, $f4
/* 0A9A14 7F074EE4 1000000D */  b     .L7F074F1C
/* 0A9A18 7F074EE8 E7A80098 */   swc1  $f8, 0x98($sp)
/* 0A9A1C 7F074EEC C4460030 */  lwc1  $f6, 0x30($v0)
.L7F074EF0:
/* 0A9A20 7F074EF0 C60A0000 */  lwc1  $f10, ($s0)
/* 0A9A24 7F074EF4 460A3101 */  sub.s $f4, $f6, $f10
/* 0A9A28 7F074EF8 E7A40090 */  swc1  $f4, 0x90($sp)
/* 0A9A2C 7F074EFC C6060004 */  lwc1  $f6, 4($s0)
/* 0A9A30 7F074F00 C4480034 */  lwc1  $f8, 0x34($v0)
/* 0A9A34 7F074F04 46064281 */  sub.s $f10, $f8, $f6
/* 0A9A38 7F074F08 E7AA0094 */  swc1  $f10, 0x94($sp)
/* 0A9A3C 7F074F0C C6080008 */  lwc1  $f8, 8($s0)
/* 0A9A40 7F074F10 C4440038 */  lwc1  $f4, 0x38($v0)
/* 0A9A44 7F074F14 46082181 */  sub.s $f6, $f4, $f8
/* 0A9A48 7F074F18 E7A60098 */  swc1  $f6, 0x98($sp)
.L7F074F1C:
/* 0A9A4C 7F074F1C 8FA900B4 */  lw    $t1, 0xb4($sp)
/* 0A9A50 7F074F20 C7A400A4 */  lwc1  $f4, 0xa4($sp)
/* 0A9A54 7F074F24 C7A600A0 */  lwc1  $f6, 0xa0($sp)
/* 0A9A58 7F074F28 95240000 */  lhu   $a0, ($t1)
/* 0A9A5C 7F074F2C 308D0400 */  andi  $t5, $a0, 0x400
/* 0A9A60 7F074F30 11A00074 */  beqz  $t5, .L7F075104
/* 0A9A64 7F074F34 30820800 */   andi  $v0, $a0, 0x800
/* 0A9A68 7F074F38 C46A0008 */  lwc1  $f10, 8($v1)
/* 0A9A6C 7F074F3C C50C0018 */  lwc1  $f12, 0x18($t0)
/* 0A9A70 7F074F40 C510001C */  lwc1  $f16, 0x1c($t0)
/* 0A9A74 7F074F44 46045202 */  mul.s $f8, $f10, $f4
/* 0A9A78 7F074F48 C46A0004 */  lwc1  $f10, 4($v1)
/* 0A9A7C 7F074F4C C4600000 */  lwc1  $f0, ($v1)
/* 0A9A80 7F074F50 E7A40024 */  swc1  $f4, 0x24($sp)
/* 0A9A84 7F074F54 460A3282 */  mul.s $f10, $f6, $f10
/* 0A9A88 7F074F58 C7A4009C */  lwc1  $f4, 0x9c($sp)
/* 0A9A8C 7F074F5C E7A80020 */  swc1  $f8, 0x20($sp)
/* 0A9A90 7F074F60 46002202 */  mul.s $f8, $f4, $f0
/* 0A9A94 7F074F64 460A4200 */  add.s $f8, $f8, $f10
/* 0A9A98 7F074F68 C7AA0020 */  lwc1  $f10, 0x20($sp)
/* 0A9A9C 7F074F6C 46085280 */  add.s $f10, $f10, $f8
/* 0A9AA0 7F074F70 460C5082 */  mul.s $f2, $f10, $f12
/* 0A9AA4 7F074F74 00000000 */  nop
/* 0A9AA8 7F074F78 46020202 */  mul.s $f8, $f0, $f2
/* 0A9AAC 7F074F7C 46044280 */  add.s $f10, $f8, $f4
/* 0A9AB0 7F074F80 E7AA009C */  swc1  $f10, 0x9c($sp)
/* 0A9AB4 7F074F84 C4680004 */  lwc1  $f8, 4($v1)
/* 0A9AB8 7F074F88 46024102 */  mul.s $f4, $f8, $f2
/* 0A9ABC 7F074F8C 46062200 */  add.s $f8, $f4, $f6
/* 0A9AC0 7F074F90 E7A800A0 */  swc1  $f8, 0xa0($sp)
/* 0A9AC4 7F074F94 C4640008 */  lwc1  $f4, 8($v1)
/* 0A9AC8 7F074F98 46022182 */  mul.s $f6, $f4, $f2
/* 0A9ACC 7F074F9C C7A40024 */  lwc1  $f4, 0x24($sp)
/* 0A9AD0 7F074FA0 46043180 */  add.s $f6, $f6, $f4
/* 0A9AD4 7F074FA4 E7A600A4 */  swc1  $f6, 0xa4($sp)
/* 0A9AD8 7F074FA8 C4640008 */  lwc1  $f4, 8($v1)
/* 0A9ADC 7F074FAC C4600000 */  lwc1  $f0, ($v1)
/* 0A9AE0 7F074FB0 E7AA0024 */  swc1  $f10, 0x24($sp)
/* 0A9AE4 7F074FB4 C7AA0098 */  lwc1  $f10, 0x98($sp)
/* 0A9AE8 7F074FB8 E7A80020 */  swc1  $f8, 0x20($sp)
/* 0A9AEC 7F074FBC E7A60028 */  swc1  $f6, 0x28($sp)
/* 0A9AF0 7F074FC0 460A2102 */  mul.s $f4, $f4, $f10
/* 0A9AF4 7F074FC4 C4660004 */  lwc1  $f6, 4($v1)
/* 0A9AF8 7F074FC8 C7A80094 */  lwc1  $f8, 0x94($sp)
/* 0A9AFC 7F074FCC E7AA002C */  swc1  $f10, 0x2c($sp)
/* 0A9B00 7F074FD0 C7AA0090 */  lwc1  $f10, 0x90($sp)
/* 0A9B04 7F074FD4 46064182 */  mul.s $f6, $f8, $f6
/* 0A9B08 7F074FD8 E7A40030 */  swc1  $f4, 0x30($sp)
/* 0A9B0C 7F074FDC 46005102 */  mul.s $f4, $f10, $f0
/* 0A9B10 7F074FE0 46062100 */  add.s $f4, $f4, $f6
/* 0A9B14 7F074FE4 C7A60030 */  lwc1  $f6, 0x30($sp)
/* 0A9B18 7F074FE8 46043180 */  add.s $f6, $f6, $f4
/* 0A9B1C 7F074FEC 460C3382 */  mul.s $f14, $f6, $f12
/* 0A9B20 7F074FF0 00000000 */  nop
/* 0A9B24 7F074FF4 460E0102 */  mul.s $f4, $f0, $f14
/* 0A9B28 7F074FF8 460A2180 */  add.s $f6, $f4, $f10
/* 0A9B2C 7F074FFC E7A60090 */  swc1  $f6, 0x90($sp)
/* 0A9B30 7F075000 C4640004 */  lwc1  $f4, 4($v1)
/* 0A9B34 7F075004 460E2282 */  mul.s $f10, $f4, $f14
/* 0A9B38 7F075008 46085100 */  add.s $f4, $f10, $f8
/* 0A9B3C 7F07500C E7A40094 */  swc1  $f4, 0x94($sp)
/* 0A9B40 7F075010 C46A0008 */  lwc1  $f10, 8($v1)
/* 0A9B44 7F075014 460E5202 */  mul.s $f8, $f10, $f14
/* 0A9B48 7F075018 C7AA002C */  lwc1  $f10, 0x2c($sp)
/* 0A9B4C 7F07501C 460A4200 */  add.s $f8, $f8, $f10
/* 0A9B50 7F075020 C7AA0024 */  lwc1  $f10, 0x24($sp)
/* 0A9B54 7F075024 E7A80098 */  swc1  $f8, 0x98($sp)
/* 0A9B58 7F075028 C4600010 */  lwc1  $f0, 0x10($v1)
/* 0A9B5C 7F07502C E7A60030 */  swc1  $f6, 0x30($sp)
/* 0A9B60 7F075030 E7A4002C */  swc1  $f4, 0x2c($sp)
/* 0A9B64 7F075034 E7A80024 */  swc1  $f8, 0x24($sp)
/* 0A9B68 7F075038 46005182 */  mul.s $f6, $f10, $f0
/* 0A9B6C 7F07503C C4680014 */  lwc1  $f8, 0x14($v1)
/* 0A9B70 7F075040 C7A40020 */  lwc1  $f4, 0x20($sp)
/* 0A9B74 7F075044 46082202 */  mul.s $f8, $f4, $f8
/* 0A9B78 7F075048 46083180 */  add.s $f6, $f6, $f8
/* 0A9B7C 7F07504C C7A80028 */  lwc1  $f8, 0x28($sp)
/* 0A9B80 7F075050 E7AA0028 */  swc1  $f10, 0x28($sp)
/* 0A9B84 7F075054 C46A0018 */  lwc1  $f10, 0x18($v1)
/* 0A9B88 7F075058 46085282 */  mul.s $f10, $f10, $f8
/* 0A9B8C 7F07505C 46065280 */  add.s $f10, $f10, $f6
/* 0A9B90 7F075060 C7A60028 */  lwc1  $f6, 0x28($sp)
/* 0A9B94 7F075064 46105082 */  mul.s $f2, $f10, $f16
/* 0A9B98 7F075068 00000000 */  nop
/* 0A9B9C 7F07506C 46020282 */  mul.s $f10, $f0, $f2
/* 0A9BA0 7F075070 46065280 */  add.s $f10, $f10, $f6
/* 0A9BA4 7F075074 E7AA009C */  swc1  $f10, 0x9c($sp)
/* 0A9BA8 7F075078 C4660014 */  lwc1  $f6, 0x14($v1)
/* 0A9BAC 7F07507C 46023282 */  mul.s $f10, $f6, $f2
/* 0A9BB0 7F075080 46045180 */  add.s $f6, $f10, $f4
/* 0A9BB4 7F075084 E7A600A0 */  swc1  $f6, 0xa0($sp)
/* 0A9BB8 7F075088 C46A0018 */  lwc1  $f10, 0x18($v1)
/* 0A9BBC 7F07508C 46025102 */  mul.s $f4, $f10, $f2
/* 0A9BC0 7F075090 C7AA0030 */  lwc1  $f10, 0x30($sp)
/* 0A9BC4 7F075094 46082180 */  add.s $f6, $f4, $f8
/* 0A9BC8 7F075098 C7A8002C */  lwc1  $f8, 0x2c($sp)
/* 0A9BCC 7F07509C E7A600A4 */  swc1  $f6, 0xa4($sp)
/* 0A9BD0 7F0750A0 C4600010 */  lwc1  $f0, 0x10($v1)
/* 0A9BD4 7F0750A4 C4660014 */  lwc1  $f6, 0x14($v1)
/* 0A9BD8 7F0750A8 E7AA0030 */  swc1  $f10, 0x30($sp)
/* 0A9BDC 7F0750AC 46005102 */  mul.s $f4, $f10, $f0
/* 0A9BE0 7F0750B0 C46A0018 */  lwc1  $f10, 0x18($v1)
/* 0A9BE4 7F0750B4 46064182 */  mul.s $f6, $f8, $f6
/* 0A9BE8 7F0750B8 46062100 */  add.s $f4, $f4, $f6
/* 0A9BEC 7F0750BC C7A60024 */  lwc1  $f6, 0x24($sp)
/* 0A9BF0 7F0750C0 46065282 */  mul.s $f10, $f10, $f6
/* 0A9BF4 7F0750C4 46045280 */  add.s $f10, $f10, $f4
/* 0A9BF8 7F0750C8 C7A40030 */  lwc1  $f4, 0x30($sp)
/* 0A9BFC 7F0750CC 46105302 */  mul.s $f12, $f10, $f16
/* 0A9C00 7F0750D0 00000000 */  nop
/* 0A9C04 7F0750D4 460C0282 */  mul.s $f10, $f0, $f12
/* 0A9C08 7F0750D8 46045280 */  add.s $f10, $f10, $f4
/* 0A9C0C 7F0750DC E7AA0090 */  swc1  $f10, 0x90($sp)
/* 0A9C10 7F0750E0 C4640014 */  lwc1  $f4, 0x14($v1)
/* 0A9C14 7F0750E4 460C2282 */  mul.s $f10, $f4, $f12
/* 0A9C18 7F0750E8 46085100 */  add.s $f4, $f10, $f8
/* 0A9C1C 7F0750EC E7A40094 */  swc1  $f4, 0x94($sp)
/* 0A9C20 7F0750F0 C46A0018 */  lwc1  $f10, 0x18($v1)
/* 0A9C24 7F0750F4 460C5202 */  mul.s $f8, $f10, $f12
/* 0A9C28 7F0750F8 46064100 */  add.s $f4, $f8, $f6
/* 0A9C2C 7F0750FC 1000004F */  b     .L7F07523C
/* 0A9C30 7F075100 E7A40098 */   swc1  $f4, 0x98($sp)
.L7F075104:
/* 0A9C34 7F075104 14400005 */  bnez  $v0, .L7F07511C
/* 0A9C38 7F075108 308C1000 */   andi  $t4, $a0, 0x1000
/* 0A9C3C 7F07510C 15800003 */  bnez  $t4, .L7F07511C
/* 0A9C40 7F075110 308A2000 */   andi  $t2, $a0, 0x2000
/* 0A9C44 7F075114 5140004A */  beql  $t2, $zero, .L7F075240
/* 0A9C48 7F075118 C7B2009C */   lwc1  $f18, 0x9c($sp)
.L7F07511C:
/* 0A9C4C 7F07511C C50A0018 */  lwc1  $f10, 0x18($t0)
/* 0A9C50 7F075120 308B1000 */  andi  $t3, $a0, 0x1000
/* 0A9C54 7F075124 10400008 */  beqz  $v0, .L7F075148
/* 0A9C58 7F075128 E7AA0060 */   swc1  $f10, 0x60($sp)
/* 0A9C5C 7F07512C C4680000 */  lwc1  $f8, ($v1)
/* 0A9C60 7F075130 E7A80054 */  swc1  $f8, 0x54($sp)
/* 0A9C64 7F075134 C4660004 */  lwc1  $f6, 4($v1)
/* 0A9C68 7F075138 E7A60058 */  swc1  $f6, 0x58($sp)
/* 0A9C6C 7F07513C C4640008 */  lwc1  $f4, 8($v1)
/* 0A9C70 7F075140 10000012 */  b     .L7F07518C
/* 0A9C74 7F075144 E7A4005C */   swc1  $f4, 0x5c($sp)
.L7F075148:
/* 0A9C78 7F075148 11600008 */  beqz  $t3, .L7F07516C
/* 0A9C7C 7F07514C 30992000 */   andi  $t9, $a0, 0x2000
/* 0A9C80 7F075150 C46A0010 */  lwc1  $f10, 0x10($v1)
/* 0A9C84 7F075154 E7AA0054 */  swc1  $f10, 0x54($sp)
/* 0A9C88 7F075158 C4680014 */  lwc1  $f8, 0x14($v1)
/* 0A9C8C 7F07515C E7A80058 */  swc1  $f8, 0x58($sp)
/* 0A9C90 7F075160 C4660018 */  lwc1  $f6, 0x18($v1)
/* 0A9C94 7F075164 10000009 */  b     .L7F07518C
/* 0A9C98 7F075168 E7A6005C */   swc1  $f6, 0x5c($sp)
.L7F07516C:
/* 0A9C9C 7F07516C 53200008 */  beql  $t9, $zero, .L7F075190
/* 0A9CA0 7F075170 C7AC0054 */   lwc1  $f12, 0x54($sp)
/* 0A9CA4 7F075174 C4640020 */  lwc1  $f4, 0x20($v1)
/* 0A9CA8 7F075178 E7A40054 */  swc1  $f4, 0x54($sp)
/* 0A9CAC 7F07517C C46A0024 */  lwc1  $f10, 0x24($v1)
/* 0A9CB0 7F075180 E7AA0058 */  swc1  $f10, 0x58($sp)
/* 0A9CB4 7F075184 C4680028 */  lwc1  $f8, 0x28($v1)
/* 0A9CB8 7F075188 E7A8005C */  swc1  $f8, 0x5c($sp)
.L7F07518C:
/* 0A9CBC 7F07518C C7AC0054 */  lwc1  $f12, 0x54($sp)
.L7F075190:
/* 0A9CC0 7F075190 C7B2009C */  lwc1  $f18, 0x9c($sp)
/* 0A9CC4 7F075194 C7AE0058 */  lwc1  $f14, 0x58($sp)
/* 0A9CC8 7F075198 C7A400A0 */  lwc1  $f4, 0xa0($sp)
/* 0A9CCC 7F07519C 460C9182 */  mul.s $f6, $f18, $f12
/* 0A9CD0 7F0751A0 C7B0005C */  lwc1  $f16, 0x5c($sp)
/* 0A9CD4 7F0751A4 460E2282 */  mul.s $f10, $f4, $f14
/* 0A9CD8 7F0751A8 460A3200 */  add.s $f8, $f6, $f10
/* 0A9CDC 7F0751AC C7A600A4 */  lwc1  $f6, 0xa4($sp)
/* 0A9CE0 7F0751B0 46068282 */  mul.s $f10, $f16, $f6
/* 0A9CE4 7F0751B4 46085280 */  add.s $f10, $f10, $f8
/* 0A9CE8 7F0751B8 C7A80060 */  lwc1  $f8, 0x60($sp)
/* 0A9CEC 7F0751BC 46085002 */  mul.s $f0, $f10, $f8
/* 0A9CF0 7F0751C0 E7A80030 */  swc1  $f8, 0x30($sp)
/* 0A9CF4 7F0751C4 46006282 */  mul.s $f10, $f12, $f0
/* 0A9CF8 7F0751C8 46125480 */  add.s $f18, $f10, $f18
/* 0A9CFC 7F0751CC 46007282 */  mul.s $f10, $f14, $f0
/* 0A9D00 7F0751D0 E7B2009C */  swc1  $f18, 0x9c($sp)
/* 0A9D04 7F0751D4 46045280 */  add.s $f10, $f10, $f4
/* 0A9D08 7F0751D8 46008102 */  mul.s $f4, $f16, $f0
/* 0A9D0C 7F0751DC E7AA00A0 */  swc1  $f10, 0xa0($sp)
/* 0A9D10 7F0751E0 46062280 */  add.s $f10, $f4, $f6
/* 0A9D14 7F0751E4 C7A40090 */  lwc1  $f4, 0x90($sp)
/* 0A9D18 7F0751E8 E7AA00A4 */  swc1  $f10, 0xa4($sp)
/* 0A9D1C 7F0751EC 460C2182 */  mul.s $f6, $f4, $f12
/* 0A9D20 7F0751F0 C7AA0094 */  lwc1  $f10, 0x94($sp)
/* 0A9D24 7F0751F4 E7A4002C */  swc1  $f4, 0x2c($sp)
/* 0A9D28 7F0751F8 460E5202 */  mul.s $f8, $f10, $f14
/* 0A9D2C 7F0751FC 46083180 */  add.s $f6, $f6, $f8
/* 0A9D30 7F075200 C7A80098 */  lwc1  $f8, 0x98($sp)
/* 0A9D34 7F075204 46088102 */  mul.s $f4, $f16, $f8
/* 0A9D38 7F075208 46062100 */  add.s $f4, $f4, $f6
/* 0A9D3C 7F07520C C7A60030 */  lwc1  $f6, 0x30($sp)
/* 0A9D40 7F075210 46062082 */  mul.s $f2, $f4, $f6
/* 0A9D44 7F075214 C7A6002C */  lwc1  $f6, 0x2c($sp)
/* 0A9D48 7F075218 46026102 */  mul.s $f4, $f12, $f2
/* 0A9D4C 7F07521C 46062100 */  add.s $f4, $f4, $f6
/* 0A9D50 7F075220 46027182 */  mul.s $f6, $f14, $f2
/* 0A9D54 7F075224 E7A40090 */  swc1  $f4, 0x90($sp)
/* 0A9D58 7F075228 460A3100 */  add.s $f4, $f6, $f10
/* 0A9D5C 7F07522C 46028182 */  mul.s $f6, $f16, $f2
/* 0A9D60 7F075230 E7A40094 */  swc1  $f4, 0x94($sp)
/* 0A9D64 7F075234 46083280 */  add.s $f10, $f6, $f8
/* 0A9D68 7F075238 E7AA0098 */  swc1  $f10, 0x98($sp)
.L7F07523C:
/* 0A9D6C 7F07523C C7B2009C */  lwc1  $f18, 0x9c($sp)
.L7F075240:
/* 0A9D70 7F075240 C7A40090 */  lwc1  $f4, 0x90($sp)
/* 0A9D74 7F075244 C7AE00A0 */  lwc1  $f14, 0xa0($sp)
/* 0A9D78 7F075248 C7A80094 */  lwc1  $f8, 0x94($sp)
/* 0A9D7C 7F07524C 46049182 */  mul.s $f6, $f18, $f4
/* 0A9D80 7F075250 C7B000A4 */  lwc1  $f16, 0xa4($sp)
/* 0A9D84 7F075254 E7A40030 */  swc1  $f4, 0x30($sp)
/* 0A9D88 7F075258 46087282 */  mul.s $f10, $f14, $f8
/* 0A9D8C 7F07525C 8FBF001C */  lw    $ra, 0x1c($sp)
/* 0A9D90 7F075260 00001025 */  move  $v0, $zero
/* 0A9D94 7F075264 460A3180 */  add.s $f6, $f6, $f10
/* 0A9D98 7F075268 C7AA0098 */  lwc1  $f10, 0x98($sp)
/* 0A9D9C 7F07526C 46105102 */  mul.s $f4, $f10, $f16
/* 0A9DA0 7F075270 46062080 */  add.s $f2, $f4, $f6
/* 0A9DA4 7F075274 44802000 */  mtc1  $zero, $f4
/* 0A9DA8 7F075278 00000000 */  nop
/* 0A9DAC 7F07527C 4602203C */  c.lt.s $f4, $f2
/* 0A9DB0 7F075280 00000000 */  nop
/* 0A9DB4 7F075284 4500001A */  bc1f  .L7F0752F0
/* 0A9DB8 7F075288 00000000 */   nop
/* 0A9DBC 7F07528C 46129182 */  mul.s $f6, $f18, $f18
/* 0A9DC0 7F075290 00000000 */  nop
/* 0A9DC4 7F075294 460E7102 */  mul.s $f4, $f14, $f14
/* 0A9DC8 7F075298 46043180 */  add.s $f6, $f6, $f4
/* 0A9DCC 7F07529C 46108102 */  mul.s $f4, $f16, $f16
/* 0A9DD0 7F0752A0 46062000 */  add.s $f0, $f4, $f6
/* 0A9DD4 7F0752A4 C7A40030 */  lwc1  $f4, 0x30($sp)
/* 0A9DD8 7F0752A8 46042182 */  mul.s $f6, $f4, $f4
/* 0A9DDC 7F0752AC 00000000 */  nop
/* 0A9DE0 7F0752B0 46084102 */  mul.s $f4, $f8, $f8
/* 0A9DE4 7F0752B4 46043200 */  add.s $f8, $f6, $f4
/* 0A9DE8 7F0752B8 460A5182 */  mul.s $f6, $f10, $f10
/* 0A9DEC 7F0752BC C7AA008C */  lwc1  $f10, 0x8c($sp)
/* 0A9DF0 7F0752C0 46021102 */  mul.s $f4, $f2, $f2
/* 0A9DF4 7F0752C4 46083300 */  add.s $f12, $f6, $f8
/* 0A9DF8 7F0752C8 460A6181 */  sub.s $f6, $f12, $f10
/* 0A9DFC 7F0752CC 46003202 */  mul.s $f8, $f6, $f0
/* 0A9E00 7F0752D0 4604403E */  c.le.s $f8, $f4
/* 0A9E04 7F0752D4 00000000 */  nop
/* 0A9E08 7F0752D8 45000003 */  bc1f  .L7F0752E8
/* 0A9E0C 7F0752DC 00000000 */   nop
/* 0A9E10 7F0752E0 10000003 */  b     .L7F0752F0
/* 0A9E14 7F0752E4 24020001 */   li    $v0, 1
.L7F0752E8:
/* 0A9E18 7F0752E8 10000001 */  b     .L7F0752F0
/* 0A9E1C 7F0752EC 00001025 */   move  $v0, $zero
.L7F0752F0:
/* 0A9E20 7F0752F0 8FB00018 */  lw    $s0, 0x18($sp)
/* 0A9E24 7F0752F4 03E00008 */  jr    $ra
/* 0A9E28 7F0752F8 27BD00B0 */   addiu $sp, $sp, 0xb0
)
#endif


/**
 * Address: 7F0752FC
 */
u32 modelFindNextProjectileHitCandidate(Model *model, coord3d *arg1, coord3d *arg2, ModelNode **nodeptr)
{
    ModelNode *node;
    s32 descend;
    u32 opcode;

    descend = TRUE;

    if (*nodeptr != NULL) {
        node = *nodeptr;
        *nodeptr = NULL;
    } else {
        node = model->obj->RootNode;
    }

    if (node != NULL) {
        do {
            if (descend != 0 && node->Child != NULL) {
                node = node->Child;
            } else {
                if (node != NULL) {
                    do {
                        if (node->Next != NULL) {
                            node = node->Next;
                            break;
                        }

                        node = node->Parent;
                    } while (node != NULL);
                }

                if (node == NULL) {
                    break;
                }
            }

            descend = TRUE;
            opcode = node->Opcode & 0xff;

            /*
            * This switch is written as opcode - 1 to match the compiler's jump-table generation.
            * The real opcodes run from HEADER=1 through DLCOLLISION=24, so the compiler
            * normalizes them to a zero-based table index by subtracting 1.
            */
            switch (opcode - 1) {
            case MODELNODE_OPCODE_BBOX - 1:
                if (modelTestRayIntersectsNodeBBox(model, node, arg1, arg2) != 0) {
                    *nodeptr = node;
                    return *(u32 *)node->Data;
                }
                descend = FALSE;
                break;

            case MODELNODE_OPCODE_OP17 - 1:
                if (sub_GAME_7F074CAC(model, node, arg1, arg2) != 0) {
                    *nodeptr = node;
                    return *(u32 *)node->Data;
                }
                descend = FALSE;
                break;

            case MODELNODE_OPCODE_LOD - 1:
                modelApplyDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_SWITCH - 1:
                modelApplyToggleRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEAD - 1:
                modelApplyHeadRelations(model, node);
                break;
            case MODELNODE_OPCODE_HEADER - 1:
            case MODELNODE_OPCODE_DLCOLLISION - 1:
            default:
                break;
            }
        } while (node != NULL);
    }

    return 0;
}


/**
 * Unreferenced
 */
u32 *sub_GAME_7F07549C(void *arg0, f32 *arg1, f32 *arg2, ModelNode **nodeptr)
{
    *nodeptr = NULL;
    return modelFindNextProjectileHitCandidate(arg0, arg1, arg2, nodeptr);
}


/**
 * Address 7F0754BC.
 * Copy animation from ROM to RAM
*/
s32 loadAnimationFrame(ModelAnimation* anim, s32 frame, ModelSkeleton* unused)
{
    s32 ret;
    s32 source;
    s32 frameSize;
    u32 dest;
    u32 size;

    ret = 0;
    frameSize = anim->unk0E >> 3; // divide by 8

    if (anim->address & 0x80000000) // If animation's address is in RAM
    {
        // Load that frame from RAM
        ret = anim->address + (frame * frameSize);
    }
    else if (D_80036414 != NULL) // should never be NULL after sub_GAME_7F0009E0 is called
    {
        // Get dest from this D_80036414 which points to an array. Align to 16 bytes.
        dest = ((u32) (D_80036414->animBufferPtr2 + 15) >> 4) * 16;
        ret = dest;

        // Get source of this animation in ROM with the offset of the frame we'll load
        source = anim->address + (frame * frameSize);
        if (source & 1)
        {
            source--;
            frameSize++;
            ret++;
        }

        // Size of frame but 16-bytes aligned. Observed to be 80 bytes. Might differ for non-guards.
        size = ((u32) (frameSize + 15) >> 4) * 16;

        // This copies one animation frame from ROM to the destination in RAM
        romCopy((void* ) dest, (void* ) source, size);

        // Increment this which serves nothing
        D_80036414->uselessPointer += 1;

        // Set this to point to the end of the copied frame
        // This allows to copy another frame after this one
        D_80036414->animBufferPtr2 = dest + size;
    }
    return ret;
}


/**
 * Address 7F0755B0.
*/
void sub_GAME_7F0755B0(void)
{
    if (D_80036414 != NULL) // should never be NULL after sub_GAME_7F0009E0 is called
    {
        // Reset the pointer to point to the start of the array
        D_80036414->animBufferPtr2 = D_80036414->animBufferPtr1;
        D_80036414->uselessPointer = NULL;
    }
}


#define PROMOTE(var) \
    if (var) \
        var = (void *)((u32)var + diff)

void modelPromoteNodeOffsetsToPointers(ModelNode *node, u32 vma, u32 fileramaddr)
{
    s32 diff = fileramaddr - vma;
    s32 i;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        PROMOTE(node->Data);
        PROMOTE(node->Parent);
        PROMOTE(node->Next);
        PROMOTE(node->Prev);
        PROMOTE(node->Child);

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                {
                    ModelRoData_HeaderRecord* rodata = &node->Data->Header;
                    PROMOTE(rodata->FirstGroup);
                    break;
                }

            case MODELNODE_OPCODE_GROUP:
                {
                    ModelRoData_GroupRecord* rodata = &node->Data->Group;
                    PROMOTE(rodata->ChildGroup);
                    break;
                }

            case MODELNODE_OPCODE_OP03:
                {
                    ModelRoData_GroupRecord* rodata = &node->Data->Group;
                    PROMOTE(rodata->ChildGroup);
                    break;
                }

            case MODELNODE_OPCODE_DL:
                {
                    ModelRoData_DisplayListRecord* rodata = &node->Data->DisplayList;
                    PROMOTE(rodata->Vertices);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_DLCOLLISION:
                {
                    ModelRoData_DisplayList_CollisionRecord* rodata = &node->Data->DisplayListCollisions;
                    PROMOTE(rodata->Vertices);
                    PROMOTE(rodata->CollisionVertices);
                    PROMOTE(rodata->PointUsage);
                    for (i = 0; i < rodata->numCollisionVertices; i++)
                    {
                        PROMOTE(rodata->CollisionVertices[i].LinkedTo);
                    }
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_OP20:
                {
                    ModelRoData_HeaderRecord* rodata = &node->Data->Header;
                    PROMOTE(rodata->FirstGroup);
                    break;
                }

            case MODELNODE_OPCODE_OP05:
                {
                    ModelRoData_Op05Record* rodata = &node->Data->Op05;

                    // shared with op07
                    PROMOTE(rodata->Children);
                    PROMOTE(rodata->Vertices);
                    PROMOTE(rodata->Images);
                    for (i = 0; i < rodata->NumChildren; i++)
                    {
                        PROMOTE(rodata->Children[i].unk04);
                    }

                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_OP07:
                {
                    ModelRoData_Op07Record* rodata = &node->Data->Op07;
                    PROMOTE(rodata->unk00);
                    PROMOTE(rodata->unk04);

                    // shared with op05
                    PROMOTE(rodata->Children);
                    PROMOTE(rodata->Vertices);
                    PROMOTE(rodata->Images);
                    for (i = 0; i < rodata->NumChildren; i++)
                    {
                        PROMOTE(rodata->Children[i].unk04);
                    }

                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_OP06:
                {
                    ModelRoData_Op06Record* rodata = &node->Data->Op06;
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_LOD:
                {
                    ModelRoData_LODRecord* rodata = &node->Data->LOD;
                    PROMOTE(rodata->Affects);
                    node->Child = rodata->Affects;
                    break;
                }

            case MODELNODE_OPCODE_SWITCH:
                {
                    ModelRoData_SwitchRecord* rodata = &node->Data->Switch;
                    PROMOTE(rodata->Controls);
                    break;
                }

            case MODELNODE_OPCODE_BSP:
                {
                    ModelRoData_BSPRecord* rodata = &node->Data->BSP;
                    PROMOTE(rodata->leftChild);
                    PROMOTE(rodata->rightChild);
                    break;
                }

            case MODELNODE_OPCODE_OP17:
                {
                    ModelRoData_GroupRecord* rodata = &node->Data->Group;
                    PROMOTE(rodata->ChildGroup);
                    break;
                }

            case MODELNODE_OPCODE_OP11:
                {
                    ModelRoData_Op11Record* rodata = &node->Data->Op11;
                    PROMOTE(rodata->unk0c[15]);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_GUNFIRE:
                {
                    ModelRoData_GunfireRecord* rodata = &node->Data->Gunfire;
                    PROMOTE(rodata->Image);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_SHADOW:
                {
                    ModelRoData_ShadowRecord* rodata = &node->Data->Shadow;
                    PROMOTE(rodata->image);
                    PROMOTE(rodata->Header);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            case MODELNODE_OPCODE_DLPRIMARY:
                {
                    ModelRoData_DisplayListPrimaryRecord* rodata = &node->Data->DisplayListPrimary;
                    PROMOTE(rodata->Vertices);
                    rodata->BaseAddr = (void *)fileramaddr;
                    break;
                }

            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}

/**
 * Address 7F075A90.
*/
void sub_GAME_7F075A90(ModelFileHeader *header, s32 vma, u32 addr) {
    s32 diff = addr - vma;
    s32 i;

    for(i = 0;i < header->numSwitches;i++)
    {
        PROMOTE(header->Switches[i]);
    }
    modelPromoteNodeOffsetsToPointers(header->RootNode, vma, addr);
}

/**
 * unreferenced
 * Address 7F075B08.
*/
void REMOVED_sub_GAME_7F075B08(s32 param_1,s32 param_2,s32 param_3,s32 param_4)
{
    return;
}


s32 modelCalculateRwDataIndexes(ModelNode *basenode)
{
    u16 len = 0;
    ModelNode *node = basenode;
    union ModelRoData *rodata;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                if (1)
                {
                    ModelRoData_HeaderRecord *rodata = &node->Data->Header;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_HeaderRecord) / 4;
                    break;
                }
            case MODELNODE_OPCODE_OP07:
                if (1)
                {
                    ModelRoData_Op07Record *rodata = &node->Data->Op07;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_Op07Record) / 4;
                    break;
                }
            case MODELNODE_OPCODE_LOD:
                if (1)
                {
                    ModelRoData_LODRecord *rodata = &node->Data->LOD;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_LODRecord) / 4;
                    node->Child = rodata->Affects;
                    break;
                }
            case MODELNODE_OPCODE_SWITCH:
                if (1)
                {
                    ModelRoData_SwitchRecord *rodata = &node->Data->Switch;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_SwitchRecord) / 4;
                    node->Child = rodata->Controls;
                    break;
                }
            case MODELNODE_OPCODE_HEAD:
                if (1)
                {
                    ModelRoData_HeadPlaceholderRecord *rodata = &node->Data->HeadPlaceholder;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_HeadPlaceholderRecord) / 4;
                    node->Child = NULL;
                    break;
                }
            case MODELNODE_OPCODE_BSP:
                if (1)
                {
                    ModelRoData_BSPRecord *rodata = &node->Data->BSP;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_BSPRecord) / 4;
                    modelApplyReorderRelationsByArg(node, FALSE);
                    break;
                }
            case MODELNODE_OPCODE_OP11:
                if (1)
                {
                    ModelRoData_Op11Record *rodata = &node->Data->Op11;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_Op11Record) / 4;
                    break;
                }
            case MODELNODE_OPCODE_GUNFIRE:
                if (1)
                {
                    ModelRoData_GunfireRecord *rodata = &node->Data->Gunfire;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_GunfireRecord) / 4;
                    break;
                }
            case MODELNODE_OPCODE_DLCOLLISION:
                if (1)
                {
                    ModelRoData_DisplayList_CollisionRecord *rodata = &node->Data->DisplayListCollisions;
                    rodata->RwDataIndex = len;
                    len += sizeof(struct ModelRwData_DisplayList_CollisionRecord) / 4;
                    break;
                }
            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node == basenode->Parent)
                {
                    node = NULL;
                    break;
                }

                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    return len;
}


void modelCalculateRwDataLen(struct ModelFileHeader *objheader)
{
  #if defined(LEFTOVERDEBUG)
    objheader->isLoaded = 1;
  #endif
    objheader->numRecords = modelCalculateRwDataIndexes(objheader->RootNode);
}


void modelInitRwData(Model *model, ModelNode *startnode)
{
    ModelNode *node = startnode;

    while (node)
    {
        u32 type = node->Opcode & 0xFF;

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                if (1)
                {
                    ModelRwData_HeaderRecord* rwdata = &modelGetNodeRwData(model, node)->Header;

                    rwdata->unk00 = 0;
                    rwdata->ground = 0;
                    rwdata->pos.x = 0;
                    rwdata->pos.y = 0;
                    rwdata->pos.z = 0;
                    rwdata->unk14 = 0;
                    rwdata->unk18 = 0;
                    rwdata->unk1c = 0;

                    rwdata->unk01 = 0;
                    rwdata->unk34.x = 0;
                    rwdata->unk34.y = 0;
                    rwdata->unk34.z = 0;
                    rwdata->unk30 = 0;
                    rwdata->unk24.x = 0;
                    rwdata->unk24.y = 0;
                    rwdata->unk24.z = 0;
                    rwdata->unk20 = 0;

                    rwdata->unk02 = 0;
                    rwdata->unk4c.x = 0;
                    rwdata->unk4c.y = 0;
                    rwdata->unk4c.z = 0;
                    rwdata->unk40.x = 0;
                    rwdata->unk40.y = 0;
                    rwdata->unk40.z = 0;
                    rwdata->unk5c = 0;
                    break;
                }

            case MODELNODE_OPCODE_OP07:
                if (1)
                {
                    ModelRwData_Op07Record* rwdata = &modelGetNodeRwData(model, node)->Op07;
                    rwdata->index = 0;
                    break;
                }


            case MODELNODE_OPCODE_LOD:
                if (1)
                {
                    ModelRoData_LODRecord* rodata = &node->Data->LOD;
                    ModelRwData_LODRecord* rwdata = &modelGetNodeRwData(model, node)->LOD;
                    rwdata->visible = FALSE;
                    node->Child = rodata->Affects;
                    break;
                }

            case MODELNODE_OPCODE_SWITCH:
                if (1)
                {
                    ModelRoData_SwitchRecord* rodata = &node->Data->Switch;
                    ModelRwData_SwitchRecord* rwdata = &modelGetNodeRwData(model, node)->Switch;
                    rwdata->visible = TRUE;
                    node->Child = rodata->Controls;
                    break;
                }

            case MODELNODE_OPCODE_HEAD:
                if (1)
                {
                    ModelRwData_HeadPlaceholderRecord* rwdata = &modelGetNodeRwData(model, node)->HeadPlaceholder;
                    rwdata->ModelFileHeader = NULL;
                    rwdata->RwDatas = NULL;
                    break;
                }

            case MODELNODE_OPCODE_BSP:
                if (1)
                {
                    ModelRwData_BSPRecord* rwdata = &modelGetNodeRwData(model, node)->BSP;
                    rwdata->visible = FALSE;
                    modelApplyReorderRelations(model, node);
                    break;
                }

            case MODELNODE_OPCODE_OP11:
                if (1)
                {
                    ModelRwData_Op11Record* rwdata = &modelGetNodeRwData(model, node)->Op11;
                    rwdata->unk00 = FALSE;
                    break;
                }

            case MODELNODE_OPCODE_GUNFIRE:
                if (1)
                {
                    ModelRwData_GunfireRecord* rwdata = &modelGetNodeRwData(model, node)->Gunfire;
                    rwdata->visible = FALSE;
                    break;
                }

            case MODELNODE_OPCODE_DLCOLLISION:
                if (1)
                {
                    ModelRoData_DisplayList_CollisionRecord* rodata = &node->Data->DisplayListCollisions;
                    ModelRwData_DisplayList_CollisionRecord* rwdata = &modelGetNodeRwData(model, node)->DisplayListCollisions;
                    rwdata->Vertices = rodata->Vertices;
                    rwdata->gdl = rodata->Primary;
                    break;
                }

            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node == startnode->Parent)
                {
                    node = NULL;
                    break;
                }

                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data)
{
  objinst->obj = header;
  objinst->datas = data;
  objinst->rwdatalen = -1;
  objinst->attachedto = NULL;
  objinst->attachedto_objinst = NULL;
  objinst->scale = 1.0;
  modelInitRwData(objinst, header->RootNode);
}


// PD: animInit
void animInit(struct Model *objinst, struct ModelFileHeader *header, u32 *data)
{
    modelInit(objinst, header, data);
    objinst->anim = NULL;
    objinst->anim2 = NULL;
    objinst->animlooping = 0;
    objinst->animflipfunc = 0;
    objinst->unk9c = 0;
    objinst->unka0 = 0;
    objinst->unk2c = 0.0f;
    objinst->timespeed = 0.0f;
    objinst->unk5c = 0.0f;
    objinst->unk7c = 0.0f;
    objinst->unk84 = 0.0f;
    objinst->unk88 = 0.0f;
    objinst->unkb0 = 0.0f;
    objinst->speed = 1.0f;
    objinst->speed2 = 1.0f;
    objinst->playspeed = 1.0f;
    objinst->anim_translation_scale = 1.0f;
    objinst->endframe = -1.0f;
    objinst->unk6c = -1.0f;
}


// PD: model00023108
void modelAttachPart(Model *pmodel, ModelFileHeader *pmodeldef, ModelNode *pnode, ModelFileHeader *cmodeldef)
{
    ModelRwData_HeadPlaceholderRecord *rwdata = modelGetNodeRwData(pmodel, pnode);
    ModelNode *node;

    rwdata->ModelFileHeader = cmodeldef;
    rwdata->RwDatas = &pmodel->datas[pmodeldef->numRecords];

    pnode->Child = cmodeldef->RootNode;

    node = pnode->Child;

    while (node)
    {
        node->Parent = pnode;
        node = node->Next;
    }

    pmodeldef->numRecords += modelCalculateRwDataIndexes(pnode->Child);
}


/**
 * This function can be called repeatedly to iterate a model's display lists.
 *
 * On the first call, the value passed as nodeptr should point to a NULL value.
 * Each time the function is called, it will update *gdlptr to point to the next
 * display list, and will update *nodeptr to point to the current node. On
 * subsequent calls, the same values should be passed as nodeptr and gdlptr so
 * the function can continue correctly.
 *
 * Note that some node types support multiple display lists, so the function
 * may return the same node while it iterates the display lists for that node.
 */
void modelIterateDisplayLists(ModelFileHeader *fileheader, ModelNode **nodeptr, Gfx **gdlptr)
{
    ModelNode *node = *nodeptr;
    union ModelRoData *rodata;
    Gfx *gdl = NULL;

    if (node == NULL)
    {
        node = fileheader->RootNode;
    }

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_DL:
                rodata = node->Data;

                if (node != *nodeptr)
                {
                    gdl = rodata->DisplayList.Primary;
                }
                else if (rodata->DisplayList.Secondary != *gdlptr)
                {
                    gdl = rodata->DisplayList.Secondary;
                }
                break;

            case MODELNODE_OPCODE_DLCOLLISION:
                rodata = node->Data;

                if (node != *nodeptr)
                {
                    gdl = rodata->DisplayListCollisions.Primary;
                }
                else if (rodata->DisplayListCollisions.Secondary != *gdlptr)
                {
                    gdl = rodata->DisplayListCollisions.Secondary;
                }
                break;

            case MODELNODE_OPCODE_DLPRIMARY:
                rodata = node->Data;

                if (node != *nodeptr)
                {
                    gdl = rodata->DisplayListPrimary.Primary;
                }
                break;

            case MODELNODE_OPCODE_LOD:
                rodata = node->Data;
                node->Child = rodata->LOD.Affects;
                break;

            case MODELNODE_OPCODE_SWITCH:
                rodata = node->Data;
                node->Child = rodata->Switch.Controls;
                break;

            case MODELNODE_OPCODE_BSP:
                modelApplyReorderRelationsByArg(node, TRUE);
                break;
        }

        if (gdl) { break; }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }

    *gdlptr = gdl;
    *nodeptr = node;
}


void modelNodeReplaceGdl(u32 arg0, ModelNode *node, Gfx *find, Gfx *replacement)
{
    union ModelRoData *rodata;
    u32 type = node->Opcode & 0xff;

    switch (type) {
        case MODELNODE_OPCODE_DL:
            rodata = node->Data;

            if (rodata->DisplayList.Primary == find)
            {
                rodata->DisplayList.Primary = replacement;
                return;
            }

            if (rodata->DisplayList.Secondary == find)
            {
                rodata->DisplayList.Secondary = replacement;
                return;
            }
            break;

        case MODELNODE_OPCODE_DLCOLLISION:
            rodata = node->Data;

            if (rodata->DisplayListCollisions.Primary == find)
            {
                rodata->DisplayListCollisions.Primary = replacement;
                return;
            }

            if (rodata->DisplayListCollisions.Secondary == find)
            {
                rodata->DisplayListCollisions.Secondary = replacement;
                return;
            }
            break;

        case MODELNODE_OPCODE_DLPRIMARY:
            rodata = node->Data;

            if (rodata->DisplayListPrimary.Primary == find)
            {
                rodata->DisplayListPrimary.Primary = replacement;
                return;
            }
            break;
    }
}
