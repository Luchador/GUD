#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef float f32;
typedef int bool;
#define TRUE 1
#define FALSE 0
typedef struct coord3d { union { struct { f32 x, y, z; }; f32 f[3]; }; } coord3d;
typedef struct Mtxf { f32 m[4][4]; } Mtxf;
typedef struct RenderPosView { Mtxf pos; } RenderPosView;
typedef struct ModelNode ModelNode;
typedef union ModelRoData {
    struct { coord3d Origin; s16 Group1, Group2; float radius; } GroupSimple;
    struct { s16 unused, MatrixIndex; } Header;
    struct { coord3d Origin; s16 JointID, MatrixIDs[3]; } Group;
} ModelRoData;
struct ModelNode { u16 Opcode; ModelRoData *Data; ModelNode *Parent, *Next, *Prev, *Child; };
typedef struct ModelFileHeader { ModelNode *RootNode; s16 numMatrices; } ModelFileHeader;
typedef struct Model { ModelFileHeader *obj; RenderPosView *render_pos; struct Model *attachedto; ModelNode *attachedto_objinst; } Model;
typedef struct PropRecord PropRecord;
typedef struct ObjectRecord { s16 obj; Model *model; u32 runtime_bitflags, flags2; } ObjectRecord;
struct PropRecord { ObjectRecord *obj; u8 flags; };
typedef struct ModelRenderData { Mtxf *basemtx; Mtxf *mtxlist; } ModelRenderData;
static RenderPosView frameMatrices[128];
static int frameCount, genericUpdates, typeLookups, matrixLookups;
static void *dynAllocate(s32 bytes)
{
    RenderPosView *p = frameMatrices + frameCount;
    assert(bytes >= 0 && bytes % sizeof(Mtxf) == 0);
    frameCount += bytes / sizeof(Mtxf); assert(frameCount <= 128);
    return p;
}
static void modelUpdateMatrices(ModelRenderData *data, Model *model);
static void matrix_4x4_multiply_translation(Mtxf *, coord3d *, Mtxf *);
static void matrix_4x4_set_identity_and_position(coord3d *, Mtxf *);
static void matrix_4x4_apply_scale_and_translation(Mtxf *, coord3d *, coord3d *);
static Mtxf *modelFindNodeMtx(Model *, ModelNode *, s32);
/* Generic fallback simulates relation updates and extra joint work. The stock
 * path uses the real process_15_subposition and matrix routines below. */
static void process_15_subposition(ModelRenderData *, Model *, ModelNode *);
static void modelUpdateMatrices(ModelRenderData *data, Model *model)
{
    ModelNode *node = model->obj->RootNode;
    genericUpdates++;
    while (node) {
        if (node->Opcode == MODELNODE_OPCODE_GROUPSIMPLE) process_15_subposition(data, model, node);
        if (node->Child) node = node->Child;
        else {
            while (node && !node->Next) node = node->Parent;
            if (node) node = node->Next;
        }
    }
}
