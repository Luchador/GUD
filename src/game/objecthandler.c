#include <ultra64.h>
#include <memp.h>
#include "objecthandler.h"
#include "model.h"

// bss
//CODE.bss:80076A50
char dword_CODE_bss_80076A50[0xC];
//CODE.bss:80076A5C
u32 dword_CODE_bss_80076A5C;
//CODE.bss:80076A60
u32 dword_CODE_bss_80076A60;
//CODE.bss:80076A64;
u32 dword_CODE_bss_80076A64;
//CODE.bss:80076A68;
u32 dword_CODE_bss_80076A68;
//CODE.bss:80076A6C;
u32 dword_CODE_bss_80076A6C;
//CODE.bss:80076A70
u32 dword_CODE_bss_80076A70;
//CODE.bss:80076A74
u32 dword_CODE_bss_80076A74;
//CODE.bss:80076A78
char dword_CODE_bss_80076A78[0xC];
//CODE.bss:80076A84
u32 dword_CODE_bss_80076A84;
//CODE.bss:80076A88
u32 dword_CODE_bss_80076A88;
//CODE.bss:80076A8C
char dword_CODE_bss_80076A8C;
char dword_CODE_bss_80076A8D;
char dword_CODE_bss_80076A8E;
char dword_CODE_bss_80076A8F;
char dword_CODE_bss_80076A90[0x10];
//CODE.bss:80076AA0
char dword_CODE_bss_80076AA0[0x14];
//CODE.bss:80076AB4
char dword_CODE_bss_80076AB4;
char dword_CODE_bss_80076AB5;
char dword_CODE_bss_80076AB6;
char dword_CODE_bss_80076AB7;
char dword_CODE_bss_80076AB8[0x10];
//CODE.bss:80076AC8
char dword_CODE_bss_80076AC8[0x14];
//CODE.bss:80076ADC
char dword_CODE_bss_80076ADC;
char dword_CODE_bss_80076ADD;
char dword_CODE_bss_80076ADE;
char dword_CODE_bss_80076ADF;
char dword_CODE_bss_80076AE0[0x2E28];
//CODE.bss:80079908
char dword_CODE_bss_80079908[0x28];

//CODE.bss:80079930
struct ptr_0_s *ptr_allocation_0;
//CODE.bss:80079934
struct ptr_1_s *ptr_allocation_1;


// data
//D:80036070
u32 D_80036070 = 0;
//D:80036074
u32 D_80036074 = 0;
//D:80036078
u32 D_80036078 = 0;
//D:8003607C
u32 D_8003607C = 0;
//D:80036080
u32 D_80036080 = 0;
//D:80036084
s32 g_ModelDistanceDisabled = 0;
//D:80036088
f32 g_ModelDistanceScale = 1.0;
//D:8003608C
struct Vertex* (*vtxallocator)(s32 numvertices) = NULL;
//D:80036090
u32 D_80036090 = 0;
//D:80036094
u32 D_80036094 = 0;
//D:80036098
u32 D_80036098 = 0;
//D:8003609C
u32 D_8003609C = 0;
//D:800360A0
u32 D_800360A0 = 0;
//D:800360A4
u32 D_800360A4 = 0;
//D:800360A8
u32 D_800360A8 = 0;
//D:800360AC
u32 D_800360AC = 0;
//D:800360B0
u32 D_800360B0 = 0;
//D:800360B4
u32 D_800360B4 = 0;
//D:800360B8
u32 D_800360B8 = 0;
//D:800360BC
u32 D_800360BC = 0;
//D:800360C0
u32 D_800360C0 = 0;
//D:800360C4
struct bondstruct_unk_op07_related D_800360C4[32] = {
    { 0, 0, 0x10 },
    { 1, 0x1000, 0xD },
    { 1, 0x1000, 0xD },
    { 9, 0x800, 0xC },
    { 9, 0x800, 0xC },
    { 0x19, 0x400, 0xB },
    { 0x19, 0x400, 0xB },
    { 0x39, 0x400, 0xB },
    { 0x39, 0x400, 0xB },
    { 0x59, 0x400, 0xB },
    { 0x59, 0x400, 0xB },
    { 0x79, 0x400, 0xB },
    { 0x79, 0x400, 0xB },
    { 0x99, 0x400, 0xB },
    { 0x99, 0x400, 0xB, },
    { 0xB9, 0x400, 0xB, },
    { 0xB9, 0x400, 0xB, },
    { 0xD9, 0x400, 0xB, },
    { 0xD9, 0x400, 0xB, },
    { 0xF9, 0x400, 0xB, },
    { 0xF9, 0x400, 0xB, },
    { 0x119, 0x400, 0xB, },
    { 0x119, 0x400, 0xB, },
    { 0x139, 0x400, 0xB, },
    { 0x139, 0x400, 0xB, },
    { 0x159, 0x400, 0xB, },
    { 0x159, 0x400, 0xB, },
    { 0x179, 0x800, 0xC, },
    { 0x179, 0x800, 0xC, },
    { 0x189, 0x1000, 0xD },
    { 0x189, 0x1000, 0xD },
    { 0x191, 0, 0x10 },
};

//D:80036244
u32 D_80036244 = 0;
//D:80036248
u32 D_80036248 = 0;
//D:8003624C
u32 D_8003624C = 0;
//D:80036250
u32 g_ModelAnimMergingEnabled = 1;
//D:80036254
u32 D_80036254 = 0;
//D:80036258
u32 D_80036258 = 0;
//D:8003625C
u32 D_8003625C = 0;
//D:80036260
u32 D_80036260 = 0;
//D:80036264
u32 D_80036264 = 0;
//D:80036268
u32 D_80036268 = 0x10;
//D:8003626C
u32 D_8003626C = 1;
//D:80036270
u32 D_80036270 = 0x1000;
//D:80036274
u32 D_80036274 = 0xD;
//D:80036278
u32 D_80036278 = 1;
//D:8003627C
u32 D_8003627C = 0x1000;
//D:80036280
u32 D_80036280 = 0xD;
//D:80036284
u32 D_80036284 = 9;
//D:80036288
u32 D_80036288 = 0x800;
//D:8003628C
u32 D_8003628C = 0xC;
//D:80036290
u32 D_80036290 = 9;
//D:80036294
u32 D_80036294 = 0x800;
//D:80036298
u32 D_80036298 = 0xC;
//D:8003629C
u32 D_8003629C = 0x19;
//D:800362A0
u32 D_800362A0 = 0x800;
//D:800362A4
u32 D_800362A4 = 0xC;
//D:800362A8
u32 D_800362A8 = 0x19;
//D:800362AC
u32 D_800362AC = 0x800;
//D:800362B0
u32 D_800362B0 = 0xC;
//D:800362B4
u32 D_800362B4 = 0x29;
//D:800362B8
u32 D_800362B8 = 0x800;
//D:800362BC
u32 D_800362BC = 0xC;
//D:800362C0
u32 D_800362C0 = 0x29;
//D:800362C4
u32 D_800362C4 = 0x800;
//D:800362C8
u32 D_800362C8 = 0xC;
//D:800362CC
u32 D_800362CC = 0x39;
//D:800362D0
u32 D_800362D0 = 0x800;
//D:800362D4
u32 D_800362D4 = 0xC;
//D:800362D8
u32 D_800362D8 = 0x39;
//D:800362DC
u32 D_800362DC = 0x800;
//D:800362E0
u32 D_800362E0 = 0xC;
//D:800362E4
u32 D_800362E4 = 0x49;
//D:800362E8
u32 D_800362E8 = 0x800;
//D:800362EC
u32 D_800362EC = 0xC;
//D:800362F0
u32 D_800362F0 = 0x49;
//D:800362F4
u32 D_800362F4 = 0x800;
//D:800362F8
u32 D_800362F8 = 0xC;
//D:800362FC
u32 D_800362FC = 0x59;
//D:80036300
u32 D_80036300 = 0x1000;
//D:80036304
u32 D_80036304 = 0xD;
//D:80036308
u32 D_80036308 = 0x59;
//D:8003630C
u32 D_8003630C = 0x1000;
//D:80036310
u32 D_80036310 = 0xD;
//D:80036314
u32 D_80036314 = 0x61;
//D:80036318
u32 D_80036318 = 0;
//D:8003631C
u32 D_8003631C[] = {
  0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//D:800363EC0
Vertex D_800363E0 = {
    { 0, 0, 0 },
    0,
    { 0, 0 },
    0xFF,
    0xFF,
    0xFF,
    0xFF
};

//D:800363F0
u32 D_800363F0 = 0x50;
//D:800363F4
u32 D_800363F4 = 0;
//D:800363F8
u32 D_800363F8 =  0;
//D:800363FC
u32 D_800363FC =  0;
//D:80036400
u32 D_80036400 =  0;
//D:80036404
u32 D_80036404 =  0xFFFFFF50;
//D:80036408
f32 D_80036408 =  1.0f;
//D:8003640C
u32 D_8003640C =  0;
//D:80036410
s32 D_80036410 =  0;
//D:80036414
struct bondstruct_unk_animation_related* D_80036414 =  0;
//D:80036418
s32 D_80036418 =  0;
//D:8003641C
s32 D_8003641C =  0;




/*
*/

ModelHitEntry* sub_GAME_7F06B120(ModelHitEntry* head, Model* context) {
    ModelHitEntry* freeListCursor;
    ModelNode* sceneCursor;
    ModelNode* childPtr;
    s32 nodeType;

    sceneCursor = context->obj->RootNode;
    freeListCursor = D_80036060;

    while ((sceneCursor != NULL) && (freeListCursor != NULL)) {
        nodeType = sceneCursor->Opcode & 0xFF;

        switch (nodeType) {
        case 1:
        case 2:
        case 3:
        case 0xb:
        case 0xc:
        case 0xd:
        case 0xe:
        case 0xf:
        case 0x10:
        case 0x15:
            freeListCursor->model = context;
            freeListCursor->rootnode = sceneCursor;
            freeListCursor = freeListCursor->next;
            break;

        default:
            break;
        }

        childPtr = sceneCursor->Child;
        if (childPtr != NULL) {
            sceneCursor = childPtr;
            continue;
        }

        while (sceneCursor != NULL) {
            childPtr = sceneCursor->Next;
            if (childPtr != NULL) {
                sceneCursor = childPtr;
                break;
            }
            sceneCursor = sceneCursor->Parent;
        }
    }

    if (freeListCursor != D_80036060) {
        if (head != NULL) {
            ModelHitEntry *tail = head;

            while (tail->next != NULL) {
                tail = tail->next;
            }

            tail->next = D_80036060;
            D_80036060->prev = tail;
        } else {
            head = D_80036060;
        }

        if (freeListCursor != NULL) {
            ModelHitEntry* prevNode = freeListCursor->prev;
            if (prevNode != NULL) {
                prevNode->next = NULL;
                freeListCursor->prev = NULL;
            }
        }

        D_80036060 = freeListCursor;
    }

    return head;
}


void sub_GAME_7F06B248(ModelHitEntry *entry)
{
    ModelHitEntry *oldhead;
    ModelHitEntry *tail;
    
    if (entry != NULL) {
        oldhead = D_80036060;
        if (oldhead != NULL) {
            tail = entry;
            while (tail->next != NULL) {
                tail = tail->next;
            }
            tail->next = oldhead;
            D_80036060->prev = tail;
        }
        D_80036060 = entry;
    }
}


#define OP16_NODEINDEX_0C(data) (((ModelNode_Op16Record *)(data))->nodeindex0c)
#define OP16_NODEINDEX_0E(data) (((ModelNode_Op16Record *)(data))->nodeindex0e)
#define OP16_NODEINDEX_10(data) (((ModelNode_Op16Record *)(data))->nodeindex10)

#define OP16_POS_X_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[0])
#define OP16_POS_Y_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[1])
#define OP16_POS_Z_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[2])

/**
 * Address: 7F06B29C
 */
void sub_GAME_7F06B29C(ModelHitEntry *arg0)
{
    ModelHitEntry *entry;
    ModelHitEntry *special;

    entry = arg0;
    special = NULL;

    while (arg0 != NULL) {
        ModelNode *node = arg0->rootnode;
        u16 opcode = node->Opcode;

        switch (opcode & 0xFF)
        {
            case MODELNODE_OPCODE_HEADER:
            {
                union ModelRoData *data;
                ModelNode *othernode;
                Mtxf *mtx;

                data = node->Data;
                othernode = data->Header.FirstGroupNode;

                mtx = modelFindNodeMtx(arg0->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(arg0->model, othernode, 0);
                    arg0->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    arg0->sortvalue = -mtx->m[3][2];
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

                mtx = modelFindNodeMtx(arg0->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(arg0->model, othernode, 0);
                    arg0->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    arg0->sortvalue = -mtx->m[3][2];
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

                mtx = modelFindNodeMtx(arg0->model, node, 0);

                if (othernode != NULL)
                {
                    Mtxf *othermtx;

                    othermtx = modelFindNodeMtx(arg0->model, othernode, 0);
                    arg0->sortvalue = -(mtx->m[3][2] + othermtx->m[3][2]) * 0.5f;
                }
                else
                {
                    arg0->sortvalue = -mtx->m[3][2];
                }
                break;
            }

            case MODELNODE_OPCODE_GROUPSIMPLE:
            {
                Mtxf *mtx;

                mtx = modelFindNodeMtx(arg0->model, node, 0);
                arg0->sortvalue = -mtx->m[3][2];
                break;
            }

            case MODELNODE_OPCODE_OP14:
            {
                Mtxf *mtx;
                f32 *data;

                mtx = modelFindNodeMtx(arg0->model, node, 0);
                data = (f32 *)node->Data;

                arg0->sortvalue = -(data[0] * mtx->m[0][2]
                                + data[1] * mtx->m[1][2]
                                + data[2] * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }

            case MODELNODE_OPCODE_INTERLINK:
            {
                Mtxf *mtx;
                ModelRoData_InterlinkageRecord *interlinkage;
                f32 sortvalue1;
                f32 sortvalue2;

                mtx = modelFindNodeMtx(arg0->model, node, 0);
                interlinkage = &node->Data->Interlinkage;

                sortvalue1 = -(interlinkage->pos.x * mtx->m[0][2]
                            + interlinkage->pos.y * mtx->m[1][2]
                            + interlinkage->pos.z * mtx->m[2][2]
                            + mtx->m[3][2]);

                sortvalue2 = -(interlinkage->pos2.x * mtx->m[0][2]
                            + interlinkage->pos2.y * mtx->m[1][2]
                            + interlinkage->pos2.z * mtx->m[2][2]
                            + mtx->m[3][2]);

                if (sortvalue1 < sortvalue2)
                {
                    arg0->sortvalue = sortvalue1;
                }
                else
                {
                    arg0->sortvalue = sortvalue2;
                }
                break;
            }

            case MODELNODE_OPCODE_OP16:
            {
                Mtxf *mtx;
                f32 *data;

                mtx = modelFindNodeMtx(arg0->model, node, 0);
                data = (f32 *)node->Data;

                arg0->sortvalue = -(data[0] * mtx->m[0][2]
                                + data[1] * mtx->m[1][2]
                                + data[2] * mtx->m[2][2]
                                + mtx->m[3][2]);

                special = arg0;
                break;
            }

            case MODELNODE_OPCODE_OP11:
            {
                Mtxf *mtx;
                ModelRoData_Op11Record *op11;

                mtx = modelFindNodeMtx(arg0->model, node, 0);
                op11 = &node->Data->Op11;

                arg0->sortvalue = -(op11->pos.x * mtx->m[0][2]
                                + op11->pos.y * mtx->m[1][2]
                                + op11->pos.z * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }

            case MODELNODE_OPCODE_GUNFIRE:
            {
                Mtxf *mtx;
                f32 *data;

                mtx = modelFindNodeMtx(arg0->model, node, 0);
                data = (f32 *)node->Data;

                arg0->sortvalue = -(data[0] * mtx->m[0][2]
                                + data[1] * mtx->m[1][2]
                                + data[2] * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }

            case MODELNODE_OPCODE_SHADOW:
            {
                Mtxf *mtx;
                ModelRoData_ShadowRecord *shadow;
                union ModelRwData *rwdata;

                mtx = modelFindNodeMtx(arg0->model, node, 0);
                shadow = &node->Data->Shadow;

                rwdata = modelGetNodeRwData(arg0->model, shadow->HeaderNode);

                arg0->sortvalue = -(shadow->pos.x * mtx->m[0][2]
                                + (rwdata->Header.ground - rwdata->Header.pos.y) * mtx->m[1][2]
                                + shadow->pos.y * mtx->m[2][2]
                                + mtx->m[3][2]);
                break;
            }
        }

        arg0 = arg0->next;
    }

    if (special != NULL)
    {
        Model *model = special->model;

        if (model->attachedto != NULL)
        {
            ModelNode **switches;
            ModelNode *node0e;
            ModelNode *specialnode;
            void *op16data;
            ModelNode *node0c;
            ModelNode *node10;
            ModelHitEntry *entry0e;
            ModelHitEntry *entry0c;
            ModelHitEntry *entry10;
            Mtxf *mtx;
            f32 axis2dot;
            f32 axis1dot;
            f32 sort0e;
            coord3d axis2;
            coord3d axis1;
            f32 sort0c;
            f32 sort10;
            coord3d transformed;
            ModelHitEntry *scan;
            f32 tempf;

            tempf = special->sortvalue;
            specialnode = special->rootnode;
            op16data = specialnode->Data;

            switches = model->attachedto->obj->Switches;

            node0e = switches[OP16_NODEINDEX_0E(op16data)];
            node0c = switches[OP16_NODEINDEX_0C(op16data)];
            node10 = switches[OP16_NODEINDEX_10(op16data)];

            entry10 = NULL;
            entry0e = NULL;
            entry0c = NULL;


            mtx = modelFindNodeMtx(model, specialnode, 0);
            scan = entry;

            while (scan != NULL)
            {
                if (node0e == scan->rootnode)
                {
                    entry0e = scan;
                }

                if (node0c == scan->rootnode)
                {
                    entry0c = scan;
                }

                if (node10 == scan->rootnode)
                {
                    entry10 = scan;
                }

                scan = scan->next;
            }

            axis2.f[0] = mtx->m[2][0];
            axis2.f[1] = mtx->m[2][1];
            axis2.f[2] = mtx->m[2][2];

            axis1.f[0] = mtx->m[1][0];
            axis1.f[1] = mtx->m[1][1];
            axis1.f[2] = mtx->m[1][2];

            transformed.f[0] = OP16_POS_X_VOL(op16data) * mtx->m[0][0]
                          + OP16_POS_Y_VOL(op16data) * mtx->m[1][0]
                          + OP16_POS_Z_VOL(op16data) * mtx->m[2][0]
                          + mtx->m[3][0];
            
            transformed.f[1] = OP16_POS_X_VOL(op16data) * mtx->m[0][1]
                          + OP16_POS_Y_VOL(op16data) * mtx->m[1][1]
                          + OP16_POS_Z_VOL(op16data) * mtx->m[2][1]
                          + mtx->m[3][1];
            
            transformed.f[2] = OP16_POS_X_VOL(op16data) * mtx->m[0][2]
                          + OP16_POS_Y_VOL(op16data) * mtx->m[1][2]
                          + OP16_POS_Z_VOL(op16data) * mtx->m[2][2]
                          + mtx->m[3][2];

            axis2dot = axis2.f[0] * transformed.f[0]
                     + axis2.f[1] * transformed.f[1]
                     + axis2.f[2] * transformed.f[2];
            axis1dot = axis1.f[0] * transformed.f[0]
                     + axis1.f[1] * transformed.f[1]
                     + axis1.f[2] * transformed.f[2];
            
            
            sort0e = entry0e->sortvalue;
            sort0c = entry0c->sortvalue;
            sort10 = entry10->sortvalue;
            if (axis2dot < 0.0f)
            {
                if (sort10 < sort0e)
                {
                    if (sort10 < tempf)
                    {
                        special->sortvalue = sort10 - 0.000030517578125f; // 2/65536
                    }
                }
                else
                {
                    if (sort0e < tempf)
                    {
                        special->sortvalue = sort0e - 0.000030517578125f; // 2/65536
                    }
                }
            }
            else if (0.0f <= axis2dot)
            {
                if (tempf < sort0e)
                {
                    entry0e->sortvalue = tempf - 0.00006103515625f; // 4/65536
                }

                if (tempf < sort10)
                {
                    if (sort10 < sort0e)
                    {
                        entry10->sortvalue = tempf - 0.000091552734375f; // 6/65536
                    }
                    else
                    {
                        entry10->sortvalue = tempf - 0.000030517578125f; // 2/65536
                    }
                }
            }

            if (axis1dot < 0.0f)
            {
                if (sort0c < special->sortvalue)
                {
                    if (entry0e->sortvalue < special->sortvalue && sort0c < entry0e->sortvalue)
                    {
                        entry0e->sortvalue = sort0c - 0.00006103515625f; // 4/65536
                    }

                    if (entry10->sortvalue < special->sortvalue && sort0c < entry10->sortvalue)
                    {
                        if (sort10 < sort0e)
                        {
                            entry10->sortvalue = sort0c - 0.000091552734375f; // 6/65536
                        }
                        else
                        {
                            entry10->sortvalue = sort0c - 0.000030517578125f; // 2/65536
                        }
                    }

                    special->sortvalue = sort0c - 0.0000152587890625f; // 1/65536
                }
            }
            else if (0.0f <= axis1dot)
            {
                if (special->sortvalue < sort0c)
                {
                    entry0c->sortvalue = special->sortvalue - 0.0000152587890625f; // 1/65536

                    if (sort0c < sort0e)
                    {
                        if (entry0e->sortvalue < entry0c->sortvalue)
                        {
                            entry0c->sortvalue = entry0e->sortvalue - 0.0000152587890625f; // 1/65536
                        }
                    }

                    if (sort0c < sort10)
                    {
                        if (entry10->sortvalue < entry0c->sortvalue)
                        {
                            entry0c->sortvalue = entry10->sortvalue - 0.0000152587890625f; // 1/65536
                        }
                    }
                }
            }
        }
    }
}

#undef OP16_NODEINDEX_0C
#undef OP16_NODEINDEX_0E
#undef OP16_NODEINDEX_10
#undef OP16_POS_X_VOL
#undef OP16_POS_Y_VOL
#undef OP16_POS_Z_VOL


/**
 * Address: 7F06BB28
 */
ModelHitEntry *sub_GAME_7F06BB28(ModelHitEntry *modelhit)
{
    ModelHitEntry stacknodes[2];
    ModelHitEntry *last;
    ModelHitEntry *current;
    ModelHitEntry *next;
    ModelHitEntry *scan;
    ModelHitEntry *best;
    f32 bestvalue;

    if (modelhit != NULL)
    {
        last = modelhit;

        if (last->next != NULL)
        {
            do
            {
                last = last->next;

                if (next);
            }
            while (last->next != NULL);
        }

        stacknodes[1].next = modelhit;
        modelhit->prev = &stacknodes[1];

        stacknodes[0].prev = last;
        last->next = &stacknodes[0];

        current = &stacknodes[1];

        do
        {
            next = current->next;
            best = NULL;
            bestvalue = -M_U32_MAX_VALUE_F;

            if (next != &stacknodes[0])
            {
                scan = next;

                do
                {
                    if (bestvalue < scan->sortvalue)
                    {
                        bestvalue = scan->sortvalue;
                        best = scan;
                    }

                    scan = scan->next;
                }
                while (scan != &stacknodes[0]);
            }

            if (best != NULL)
            {
                best->next->prev = best->prev;
                best->prev->next = best->next;

                best->prev = current;
                best->next = current->next;

                current->next->prev = best;
                current->next = best;

                next = best;
            }

            current = next;
        }
        while (next != &stacknodes[0]);

        modelhit = stacknodes[1].next;
        stacknodes[1].next->prev = NULL;
        stacknodes[0].prev->next = NULL;
    }

    return modelhit;
}


#if defined(LEFTOVERDEBUG)
// rodata
//D:80054420
const char aDrawjointlistNoGfxlist[] = "drawjointlist: no gfxlist!\n";
//D:8005443C
const char aDrawjointlistNoObject0xX[] = "drawjointlist: no object! (0x%X)\n";
//D:80054460
const char aDrawjointlistObjectNotInitialised0[] = "drawjointlist: object not initialised! (0x%X)\n";
#endif

#ifdef NONMATCHING
void drawjointlist(ModelRenderData *mrData, ModelHitEntry *entry)
{
    Model           *temp_a2;
    ModelFileHeader *mfhModel;
    ModelNode       *temp_s1;
    ModelNode       *temp_v0_2;
    ModelNode       *temp_v0_3;
    ModelNode       *var_s0;
    s32              temp_t3;
    s32              var_s2;
    u64             *temp_v0;



    if (mrData->gdl == NULL)
    {
        osSyncPrintf(&aDrawjointlistNoGfxlist);
        exit();
    }
    if (field_20 != NULL)
    {
        do
        {
            temp_a2 = field_20->unk0;
            temp_s1 = field_20->unk4;
            mfhModel  = temp_a2->obj;
            var_s0  = temp_s1;
            if (mfhModel == NULL)
            {
                osSyncPrintf(&aDrawjointlistNoObject0xX, temp_a2, temp_a2);
                exit();
                mfhModel = field_20->unk0->obj;
            }
            if (mfhModel->isLoaded == 0)
            {
                osSyncPrintf(&aDrawjointlistObjectNotInitialised0, mfhModel);
                exit();
            }
            if (mrData->unk18 != 0)
            {
                if (field_20->unk8 < getjointsize(field_20->unk0, temp_s1))
                {
                    var_s0 = NULL;
                }
            }
            else
            {
                temp_v0       = mrData->gdl;
                mrData->gdl    = temp_v0 + 8;
                temp_v0->unk0 = 0xBC000C06;
                temp_v0->unk4 = osVirtualToPhysical(field_20->unk0->unk0c, temp_s1);
            }
            if (var_s0 != NULL)
            {
                do
                {
                    var_s2  = 1;
                    temp_t3 = var_s0->Opcode & 0xFF;
                    switch (temp_t3)
                    {
                        case 1:
                        case 2:
                        case 3:
                        case 11:
                        case 12:
                        case 13:
                        case 14:
                        case 15:
                        case 16:
                        case 21:
                            if (var_s0 == temp_s1)
                            {
                                sub_GAME_7F074534(mrData, field_20->unk0, var_s0);
                            }
                            else
                            {
                                var_s2 = 0;
                            }
                            break;
                        default:
                            sub_GAME_7F074534(mrData, field_20->unk0, var_s0);
                            break;
                    }
                    if ((var_s2 != 0) && (temp_v0_2 = var_s0->Child, (temp_v0_2 != NULL)))
                    {
                        var_s0 = temp_v0_2;
                    }
                    else if (var_s0 != NULL)
                    {
loop_22:
                        if (var_s0 == temp_s1)
                        {
                            var_s0 = NULL;
                        }
                        else
                        {
                            temp_v0_3 = var_s0->Next;
                            if (temp_v0_3 != NULL)
                            {
                                var_s0 = temp_v0_3;
                            }
                            else
                            {
                                var_s0 = var_s0->Parent;
                                if (var_s0 != NULL)
                                {
                                    goto loop_22;
                                }
                            }
                        }
                    }
                } while (var_s0 != NULL);
            }
            field_20 = field_20->unkC;
        } while (field_20 != NULL);
    }
}

#else
#ifndef VERSION_EU
GLOBAL_ASM(
.late_rodata
/*D:80054538*/
glabel obj_table_jointlist
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD44
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68

.text
glabel drawjointlist
/* 0A073C 7F06BC0C 27BDFFD0 */  addiu $sp, $sp, -0x30
/* 0A0740 7F06BC10 AFBF002C */  sw    $ra, 0x2c($sp)
/* 0A0744 7F06BC14 AFB40028 */  sw    $s4, 0x28($sp)
/* 0A0748 7F06BC18 AFB30024 */  sw    $s3, 0x24($sp)
/* 0A074C 7F06BC1C AFB20020 */  sw    $s2, 0x20($sp)
/* 0A0750 7F06BC20 AFB1001C */  sw    $s1, 0x1c($sp)
/* 0A0754 7F06BC24 AFB00018 */  sw    $s0, 0x18($sp)
/* 0A0758 7F06BC28 8C8E000C */  lw    $t6, 0xc($a0)
/* 0A075C 7F06BC2C 0080A025 */  move  $s4, $a0
/* 0A0760 7F06BC30 00A09825 */  move  $s3, $a1
/* 0A0764 7F06BC34 15C00005 */  bnez  $t6, .L7F06BC4C
/* 0A0768 7F06BC38 3C048005 */   lui   $a0, %hi(aDrawjointlistNoGfxlist)
/* 0A076C 7F06BC3C 0C0033D1 */  jal   osSyncPrintf
/* 0A0770 7F06BC40 24844420 */   addiu $a0, %lo(aDrawjointlistNoGfxlist) # addiu $a0, $a0, 0x4420
/* 0A0774 7F06BC44 0FC1B11B */  jal   return_null
/* 0A0778 7F06BC48 00000000 */   nop
.L7F06BC4C:
/* 0A077C 7F06BC4C 52600065 */  beql  $s3, $zero, .L7F06BDE4
/* 0A0780 7F06BC50 8FBF002C */   lw    $ra, 0x2c($sp)
/* 0A0784 7F06BC54 8E660000 */  lw    $a2, ($s3)
.L7F06BC58:
/* 0A0788 7F06BC58 8E710004 */  lw    $s1, 4($s3)
/* 0A078C 7F06BC5C 3C048005 */  lui   $a0, %hi(aDrawjointlistNoObject0xX)
/* 0A0790 7F06BC60 8CC50008 */  lw    $a1, 8($a2)
/* 0A0794 7F06BC64 2484443C */  addiu $a0, %lo(aDrawjointlistNoObject0xX) # addiu $a0, $a0, 0x443c
/* 0A0798 7F06BC68 02208025 */  move  $s0, $s1
/* 0A079C 7F06BC6C 54A00008 */  bnezl $a1, .L7F06BC90
/* 0A07A0 7F06BC70 8CB8001C */   lw    $t8, 0x1c($a1)
/* 0A07A4 7F06BC74 0C0033D1 */  jal   osSyncPrintf
/* 0A07A8 7F06BC78 00C02825 */   move  $a1, $a2
/* 0A07AC 7F06BC7C 0FC1B11B */  jal   return_null
/* 0A07B0 7F06BC80 00000000 */   nop
/* 0A07B4 7F06BC84 8E6F0000 */  lw    $t7, ($s3)
/* 0A07B8 7F06BC88 8DE50008 */  lw    $a1, 8($t7)
/* 0A07BC 7F06BC8C 8CB8001C */  lw    $t8, 0x1c($a1)
.L7F06BC90:
/* 0A07C0 7F06BC90 3C048005 */  lui   $a0, %hi(aDrawjointlistObjectNotInitialised0)
/* 0A07C4 7F06BC94 57000006 */  bnezl $t8, .L7F06BCB0
/* 0A07C8 7F06BC98 8E990018 */   lw    $t9, 0x18($s4)
/* 0A07CC 7F06BC9C 0C0033D1 */  jal   osSyncPrintf
/* 0A07D0 7F06BCA0 24844460 */   addiu $a0, %lo(aDrawjointlistObjectNotInitialised0) # addiu $a0, $a0, 0x4460
/* 0A07D4 7F06BCA4 0FC1B11B */  jal   return_null
/* 0A07D8 7F06BCA8 00000000 */   nop
/* 0A07DC 7F06BCAC 8E990018 */  lw    $t9, 0x18($s4)
.L7F06BCB0:
/* 0A07E0 7F06BCB0 02202825 */  move  $a1, $s1
/* 0A07E4 7F06BCB4 5320000B */  beql  $t9, $zero, .L7F06BCE4
/* 0A07E8 7F06BCB8 8E82000C */   lw    $v0, 0xc($s4)
/* 0A07EC 7F06BCBC 0FC1B3A4 */  jal   getjointsize
/* 0A07F0 7F06BCC0 8E640000 */   lw    $a0, ($s3)
/* 0A07F4 7F06BCC4 C6640008 */  lwc1  $f4, 8($s3)
/* 0A07F8 7F06BCC8 4600203C */  c.lt.s $f4, $f0
/* 0A07FC 7F06BCCC 00000000 */  nop
/* 0A0800 7F06BCD0 4500000E */  bc1f  .L7F06BD0C
/* 0A0804 7F06BCD4 00000000 */   nop
/* 0A0808 7F06BCD8 1000000C */  b     .L7F06BD0C
/* 0A080C 7F06BCDC 00008025 */   move  $s0, $zero
/* 0A0810 7F06BCE0 8E82000C */  lw    $v0, 0xc($s4)
.L7F06BCE4:
/* 0A0814 7F06BCE4 3C09BC00 */  lui   $t1, (0xBC000C06 >> 16) # lui $t1, 0xbc00
/* 0A0818 7F06BCE8 35290C06 */  ori   $t1, (0xBC000C06 & 0xFFFF) # ori $t1, $t1, 0xc06
/* 0A081C 7F06BCEC 24480008 */  addiu $t0, $v0, 8
/* 0A0820 7F06BCF0 AE88000C */  sw    $t0, 0xc($s4)
/* 0A0824 7F06BCF4 AC490000 */  sw    $t1, ($v0)
/* 0A0828 7F06BCF8 8E6A0000 */  lw    $t2, ($s3)
/* 0A082C 7F06BCFC 00409025 */  move  $s2, $v0
/* 0A0830 7F06BD00 0C003A2C */  jal   osVirtualToPhysical
/* 0A0834 7F06BD04 8D44000C */   lw    $a0, 0xc($t2)
/* 0A0838 7F06BD08 AE420004 */  sw    $v0, 4($s2)
.L7F06BD0C:
/* 0A083C 7F06BD0C 52000032 */  beql  $s0, $zero, .L7F06BDD8
/* 0A0840 7F06BD10 8E73000C */   lw    $s3, 0xc($s3)
/* 0A0844 7F06BD14 96020000 */  lhu   $v0, ($s0)
.L7F06BD18:
/* 0A0848 7F06BD18 24120001 */  li    $s2, 1
/* 0A084C 7F06BD1C 304B00FF */  andi  $t3, $v0, 0xff
/* 0A0850 7F06BD20 256CFFFF */  addiu $t4, $t3, -1
/* 0A0854 7F06BD24 2D810018 */  sltiu $at, $t4, 0x18
/* 0A0858 7F06BD28 1020000F */  beqz  $at, .L7F06BD68
/* 0A085C 7F06BD2C 000C6080 */   sll   $t4, $t4, 2
/* 0A0860 7F06BD30 3C018005 */  lui   $at, %hi(obj_table_jointlist)
/* 0A0864 7F06BD34 002C0821 */  addu  $at, $at, $t4
/* 0A0868 7F06BD38 8C2C4538 */  lw    $t4, %lo(obj_table_jointlist)($at)
.L7F06BD3C:
/* 0A086C 7F06BD3C 01800008 */  jr    $t4
/* 0A0870 7F06BD40 00000000 */   nop
.L7F06BD44:
/* 0A0874 7F06BD44 16110006 */  bne   $s0, $s1, .L7F06BD60
/* 0A0878 7F06BD48 02802025 */   move  $a0, $s4
/* 0A087C 7F06BD4C 8E650000 */  lw    $a1, ($s3)
/* 0A0880 7F06BD50 0FC1D14D */  jal   sub_GAME_7F074534
/* 0A0884 7F06BD54 02003025 */   move  $a2, $s0
/* 0A0888 7F06BD58 10000007 */  b     .L7F06BD78
/* 0A088C 7F06BD5C 00000000 */   nop
.L7F06BD60:
/* 0A0890 7F06BD60 10000005 */  b     .L7F06BD78
/* 0A0894 7F06BD64 00009025 */   move  $s2, $zero
def_7F06BD3C:
.L7F06BD68:
/* 0A0898 7F06BD68 02802025 */  move  $a0, $s4
/* 0A089C 7F06BD6C 8E650000 */  lw    $a1, ($s3)
/* 0A08A0 7F06BD70 0FC1D14D */  jal   sub_GAME_7F074534
/* 0A08A4 7F06BD74 02003025 */   move  $a2, $s0
.L7F06BD78:
/* 0A08A8 7F06BD78 12400006 */  beqz  $s2, .L7F06BD94
/* 0A08AC 7F06BD7C 00000000 */   nop
/* 0A08B0 7F06BD80 8E020014 */  lw    $v0, 0x14($s0)
/* 0A08B4 7F06BD84 10400003 */  beqz  $v0, .L7F06BD94
/* 0A08B8 7F06BD88 00000000 */   nop
/* 0A08BC 7F06BD8C 1000000F */  b     .L7F06BDCC
/* 0A08C0 7F06BD90 00408025 */   move  $s0, $v0
.L7F06BD94:
/* 0A08C4 7F06BD94 1200000D */  beqz  $s0, .L7F06BDCC
/* 0A08C8 7F06BD98 00000000 */   nop
.L7F06BD9C:
/* 0A08CC 7F06BD9C 56110004 */  bnel  $s0, $s1, .L7F06BDB0
/* 0A08D0 7F06BDA0 8E02000C */   lw    $v0, 0xc($s0)
/* 0A08D4 7F06BDA4 10000009 */  b     .L7F06BDCC
/* 0A08D8 7F06BDA8 00008025 */   move  $s0, $zero
/* 0A08DC 7F06BDAC 8E02000C */  lw    $v0, 0xc($s0)
.L7F06BDB0:
/* 0A08E0 7F06BDB0 50400004 */  beql  $v0, $zero, .L7F06BDC4
/* 0A08E4 7F06BDB4 8E100008 */   lw    $s0, 8($s0)
/* 0A08E8 7F06BDB8 10000004 */  b     .L7F06BDCC
/* 0A08EC 7F06BDBC 00408025 */   move  $s0, $v0
/* 0A08F0 7F06BDC0 8E100008 */  lw    $s0, 8($s0)
.L7F06BDC4:
/* 0A08F4 7F06BDC4 1600FFF5 */  bnez  $s0, .L7F06BD9C
/* 0A08F8 7F06BDC8 00000000 */   nop
.L7F06BDCC:
/* 0A08FC 7F06BDCC 5600FFD2 */  bnezl $s0, .L7F06BD18
/* 0A0900 7F06BDD0 96020000 */   lhu   $v0, ($s0)
/* 0A0904 7F06BDD4 8E73000C */  lw    $s3, 0xc($s3)
.L7F06BDD8:
/* 0A0908 7F06BDD8 5660FF9F */  bnezl $s3, .L7F06BC58
/* 0A090C 7F06BDDC 8E660000 */   lw    $a2, ($s3)
/* 0A0910 7F06BDE0 8FBF002C */  lw    $ra, 0x2c($sp)
.L7F06BDE4:
/* 0A0914 7F06BDE4 8FB00018 */  lw    $s0, 0x18($sp)
/* 0A0918 7F06BDE8 8FB1001C */  lw    $s1, 0x1c($sp)
/* 0A091C 7F06BDEC 8FB20020 */  lw    $s2, 0x20($sp)
/* 0A0920 7F06BDF0 8FB30024 */  lw    $s3, 0x24($sp)
/* 0A0924 7F06BDF4 8FB40028 */  lw    $s4, 0x28($sp)
/* 0A0928 7F06BDF8 03E00008 */  jr    $ra
/* 0A092C 7F06BDFC 27BD0030 */   addiu $sp, $sp, 0x30
)
#endif

#ifdef VERSION_EU
GLOBAL_ASM(
.late_rodata
/*D:80054538*/
glabel obj_table_jointlist
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD44
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD44
.word .L7F06BD68
.word .L7F06BD68
.word .L7F06BD68

.text
glabel drawjointlist
/* 09EDAC 7F06C3BC 27BDFFD0 */  addiu $sp, $sp, -0x30
/* 09EDB0 7F06C3C0 AFB40028 */  sw    $s4, 0x28($sp)
/* 09EDB4 7F06C3C4 AFB30024 */  sw    $s3, 0x24($sp)
/* 09EDB8 7F06C3C8 00A09825 */  move  $s3, $a1
/* 09EDBC 7F06C3CC 0080A025 */  move  $s4, $a0
/* 09EDC0 7F06C3D0 AFBF002C */  sw    $ra, 0x2c($sp)
/* 09EDC4 7F06C3D4 AFB20020 */  sw    $s2, 0x20($sp)
/* 09EDC8 7F06C3D8 AFB1001C */  sw    $s1, 0x1c($sp)
/* 09EDCC 7F06C3DC 10A0004F */  beqz  $a1, .L7F06C51C
/* 09EDD0 7F06C3E0 AFB00018 */   sw    $s0, 0x18($sp)
/* 09EDD4 7F06C3E4 8E8E0018 */  lw    $t6, 0x18($s4)
.L7F06C3E8:
/* 09EDD8 7F06C3E8 8E710004 */  lw    $s1, 4($s3)
/* 09EDDC 7F06C3EC 11C0000B */  beqz  $t6, .L7F06C41C
/* 09EDE0 7F06C3F0 02208025 */   move  $s0, $s1
/* 09EDE4 7F06C3F4 8E640000 */  lw    $a0, ($s3)
/* 09EDE8 7F06C3F8 0FC1B4D5 */  jal   getjointsize
/* 09EDEC 7F06C3FC 02202825 */   move  $a1, $s1
/* 09EDF0 7F06C400 C6640008 */  lwc1  $f4, 8($s3)
/* 09EDF4 7F06C404 4600203C */  c.lt.s $f4, $f0
/* 09EDF8 7F06C408 00000000 */  nop
/* 09EDFC 7F06C40C 4500000E */  bc1f  .L7F06C448
/* 09EE00 7F06C410 00000000 */   nop
/* 09EE04 7F06C414 1000000C */  b     .L7F06C448
/* 09EE08 7F06C418 00008025 */   move  $s0, $zero
.L7F06C41C:
/* 09EE0C 7F06C41C 8E82000C */  lw    $v0, 0xc($s4)
/* 09EE10 7F06C420 3C18BC00 */  lui   $t8, (0xBC000C06 >> 16) # lui $t8, 0xbc00
/* 09EE14 7F06C424 37180C06 */  ori   $t8, (0xBC000C06 & 0xFFFF) # ori $t8, $t8, 0xc06
/* 09EE18 7F06C428 244F0008 */  addiu $t7, $v0, 8
/* 09EE1C 7F06C42C AE8F000C */  sw    $t7, 0xc($s4)
/* 09EE20 7F06C430 AC580000 */  sw    $t8, ($v0)
/* 09EE24 7F06C434 8E790000 */  lw    $t9, ($s3)
/* 09EE28 7F06C438 00409025 */  move  $s2, $v0
/* 09EE2C 7F06C43C 0C003838 */  jal   osVirtualToPhysical
/* 09EE30 7F06C440 8F24000C */   lw    $a0, 0xc($t9)
/* 09EE34 7F06C444 AE420004 */  sw    $v0, 4($s2)
.L7F06C448:
/* 09EE38 7F06C448 52000032 */  beql  $s0, $zero, .L7F06C514
/* 09EE3C 7F06C44C 8E73000C */   lw    $s3, 0xc($s3)
/* 09EE40 7F06C450 96020000 */  lhu   $v0, ($s0)
.L7F06C454:
/* 09EE44 7F06C454 24120001 */  li    $s2, 1
/* 09EE48 7F06C458 304800FF */  andi  $t0, $v0, 0xff
/* 09EE4C 7F06C45C 2509FFFF */  addiu $t1, $t0, -1
/* 09EE50 7F06C460 2D210018 */  sltiu $at, $t1, 0x18
/* 09EE54 7F06C464 1020000F */  beqz  $at, .L7F06C4A4
/* 09EE58 7F06C468 00094880 */   sll   $t1, $t1, 2
/* 09EE5C 7F06C46C 3C018005 */  lui   $at, %hi(obj_table_jointlist)
/* 09EE60 7F06C470 00290821 */  addu  $at, $at, $t1
/* 09EE64 7F06C474 8C29A608 */  lw    $t1, %lo(obj_table_jointlist)($at)
.L7F06BD3C:
/* 09EE68 7F06C478 01200008 */  jr    $t1
/* 09EE6C 7F06C47C 00000000 */   nop
.L7F06BD44:
/* 09EE70 7F06C480 16110006 */  bne   $s0, $s1, .L7F06C49C
/* 09EE74 7F06C484 02802025 */   move  $a0, $s4
/* 09EE78 7F06C488 8E650000 */  lw    $a1, ($s3)
/* 09EE7C 7F06C48C 0FC1D182 */  jal   sub_GAME_7F074534
/* 09EE80 7F06C490 02003025 */   move  $a2, $s0
/* 09EE84 7F06C494 10000007 */  b     .L7F06C4B4
/* 09EE88 7F06C498 00000000 */   nop
.L7F06C49C:
/* 09EE8C 7F06C49C 10000005 */  b     .L7F06C4B4
/* 09EE90 7F06C4A0 00009025 */   move  $s2, $zero
.L7F06BD68:
.L7F06C4A4:
/* 09EE94 7F06C4A4 02802025 */  move  $a0, $s4
/* 09EE98 7F06C4A8 8E650000 */  lw    $a1, ($s3)
/* 09EE9C 7F06C4AC 0FC1D182 */  jal   sub_GAME_7F074534
/* 09EEA0 7F06C4B0 02003025 */   move  $a2, $s0
.L7F06C4B4:
/* 09EEA4 7F06C4B4 12400006 */  beqz  $s2, .L7F06C4D0
/* 09EEA8 7F06C4B8 00000000 */   nop
/* 09EEAC 7F06C4BC 8E020014 */  lw    $v0, 0x14($s0)
/* 09EEB0 7F06C4C0 10400003 */  beqz  $v0, .L7F06C4D0
/* 09EEB4 7F06C4C4 00000000 */   nop
/* 09EEB8 7F06C4C8 1000000F */  b     .L7F06C508
/* 09EEBC 7F06C4CC 00408025 */   move  $s0, $v0
.L7F06C4D0:
/* 09EEC0 7F06C4D0 1200000D */  beqz  $s0, .L7F06C508
/* 09EEC4 7F06C4D4 00000000 */   nop
.L7F06C4D8:
/* 09EEC8 7F06C4D8 56110004 */  bnel  $s0, $s1, .L7F06C4EC
/* 09EECC 7F06C4DC 8E02000C */   lw    $v0, 0xc($s0)
/* 09EED0 7F06C4E0 10000009 */  b     .L7F06C508
/* 09EED4 7F06C4E4 00008025 */   move  $s0, $zero
/* 09EED8 7F06C4E8 8E02000C */  lw    $v0, 0xc($s0)
.L7F06C4EC:
/* 09EEDC 7F06C4EC 50400004 */  beql  $v0, $zero, .L7F06C500
/* 09EEE0 7F06C4F0 8E100008 */   lw    $s0, 8($s0)
/* 09EEE4 7F06C4F4 10000004 */  b     .L7F06C508
/* 09EEE8 7F06C4F8 00408025 */   move  $s0, $v0
/* 09EEEC 7F06C4FC 8E100008 */  lw    $s0, 8($s0)
.L7F06C500:
/* 09EEF0 7F06C500 1600FFF5 */  bnez  $s0, .L7F06C4D8
/* 09EEF4 7F06C504 00000000 */   nop
.L7F06C508:
/* 09EEF8 7F06C508 5600FFD2 */  bnezl $s0, .L7F06C454
/* 09EEFC 7F06C50C 96020000 */   lhu   $v0, ($s0)
/* 09EF00 7F06C510 8E73000C */  lw    $s3, 0xc($s3)
.L7F06C514:
/* 09EF04 7F06C514 5660FFB4 */  bnezl $s3, .L7F06C3E8
/* 09EF08 7F06C518 8E8E0018 */   lw    $t6, 0x18($s4)
.L7F06C51C:
/* 09EF0C 7F06C51C 8FBF002C */  lw    $ra, 0x2c($sp)
/* 09EF10 7F06C520 8FB00018 */  lw    $s0, 0x18($sp)
/* 09EF14 7F06C524 8FB1001C */  lw    $s1, 0x1c($sp)
/* 09EF18 7F06C528 8FB20020 */  lw    $s2, 0x20($sp)
/* 09EF1C 7F06C52C 8FB30024 */  lw    $s3, 0x24($sp)
/* 09EF20 7F06C530 8FB40028 */  lw    $s4, 0x28($sp)
/* 09EF24 7F06C534 03E00008 */  jr    $ra
/* 09EF28 7F06C538 27BD0030 */   addiu $sp, $sp, 0x30
)
#endif
#endif


s32 probably_damage_detail_blood_effect_related(ModelHitEntry **entryptr, coord3d *raypos, coord3d *raydir, Model **outModel, ModelNode **inoutNode)
{
    ModelHitEntry *entry = *entryptr;
    ModelNode *node;
    s32 descend;
    ModelNode *root;
    ModelNode *next;

    while (entry != NULL) {
        ModelNode *resume = *inoutNode;

        root = entry->rootnode;
        descend = TRUE;

        if (resume != NULL) {
            node = resume;
            *inoutNode = NULL;
        } else {
            node = root;
        }

        while (node != NULL) {
            if (descend && node->Child != NULL) {
                node = node->Child;
            } else {
                if (node != NULL) {
walk_node:
                    if (node == root) {
                        node = NULL;
                    } else {
                        next = node->Next;

                        if (next != NULL) {
                            node = next;
                        } else {
                            node = node->Parent;

                            if (node != NULL) {
                                goto walk_node;
                            }
                        }
                    }
                }

                if (node == NULL) {
                    break;
                }
            }

            descend = TRUE;
            
            {
                u16 opcode = node->Opcode;
            
                descend = TRUE;
            
                switch (opcode & 0xff) {
                case MODELNODE_OPCODE_HEADER:
                case MODELNODE_OPCODE_GROUP:
                case MODELNODE_OPCODE_OP03:
                case MODELNODE_OPCODE_GROUPSIMPLE:
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_OP11:
                case MODELNODE_OPCODE_GUNFIRE:
                case MODELNODE_OPCODE_SHADOW:
                case MODELNODE_OPCODE_OP14:
                case MODELNODE_OPCODE_INTERLINK:
                case MODELNODE_OPCODE_OP16:
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_BBOX:
                    if (sub_GAME_7F074C68(entry->model, node, raypos, raydir)) {
                        *outModel = entry->model;
                        *inoutNode = node;
                        *entryptr = entry;
            
                        return *(s32 *)node->Data;
                    }
            
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_OP17:
                    if (sub_GAME_7F074CAC(entry->model, node, raypos, raydir)) {
                        *outModel = entry->model;
                        *inoutNode = node;
                        *entryptr = entry;
            
                        return *(s32 *)node->Data;
                    }
            
                    descend = FALSE;
                    break;
            
                case MODELNODE_OPCODE_LOD:
                    modelApplyDistanceRelations(entry->model, node);
                    break;
            
                case MODELNODE_OPCODE_SWITCH:
                    modelApplyToggleRelations(entry->model, node);
                    break;
            
                case MODELNODE_OPCODE_HEAD:
                    modelApplyHeadRelations(entry->model, node);
                    break;
            
                case MODELNODE_OPCODE_DLCOLLISION:
                default:
                    break;
                }
            }
        }

        entry = entry->prev;
    }

    *entryptr = NULL;
    return 0;
}


/**
 * Address: 0x7F06C010
* https://decomp.me/scratch/IDiXU
 * #MATCH! Unlikley match, not sure why we are setting the root node to the last node.
 
f32 sub_GAME_7F06C010(ModelFileHeader *head, s32 unused, s32 unused2, s32 *arg3, s32 *arg4)
{
    ModelNode *lastnode = head->RootNode;

    while (lastnode->Next != NULL)
    {
        lastnode = lastnode->Next;
    }

    head->RootNode = lastnode;
    *arg3          = 0;
    *arg4          = 0;
    return probably_damage_detail_blood_effect_related(head, unused, unused2, arg3, arg4);
}*/


/**
 * Address: 7F06C010
 * 
 * Update: decompiling probably_damage_detail_blood_effect_related revealed that the first argument for this function is a ModelHitEntry.
 * The previous function matched because both ModelNode and ModelHitEntry happen to have a normal 32 bit field at offset 0x0c.
 */
s32 sub_GAME_7F06C010(ModelHitEntry **entryptr, coord3d *modelRayStart, coord3d *modelRayDir, Model **outModel, ModelNode **outNode)
{
    ModelHitEntry *entry = *entryptr;

    while (entry->next != NULL) 
    {
        entry = entry->next;
    }

    *entryptr = entry;
    *outModel = NULL;
    *outNode = NULL;

    return probably_damage_detail_blood_effect_related( entryptr, modelRayStart, modelRayDir, outModel, outNode);
}

