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
typedef struct { struct { u32 w0,w1; } words; } Gfx;
static Gfx ram[1024 * 1024];
static u32 osMemSize = sizeof(ram);
#define IS_KSEG0(p) ((uintptr_t)(p) >= (uintptr_t)ram && (uintptr_t)(p) < (uintptr_t)ram + sizeof(ram))
#define K0_TO_PHYS(p) ((uintptr_t)(p) - (uintptr_t)ram)
#define SPSEGMENT_MODEL_COL1 5
#define SPSEGMENT_MODEL_MTX 3
#define G_ENDDL 0xb8
#define G_DL 6
#define G_MOVEMEM 3
#define G_MOVEWORD 0xbc
#define G_MW_SEGMENT 6
#define G_MW_MATRIX 0
#define G_MTX 1
#define MODELGROUP_MTX_HAS_MATRIX1 0x100
#define MODELGROUP_MTX_HAS_MATRIX2 0x200
#define M_PI_F 3.14159265358979323846f
#define M_TAU_F (2.0f * M_PI_F)
typedef float quatf[4];
typedef float vec3f[3];
typedef struct coord3d { union { struct { float x,y,z; }; float f[3]; }; } coord3d;
typedef struct Mtxf { f32 m[4][4]; } Mtxf;
typedef struct RenderPosView { Mtxf pos; } RenderPosView;
typedef struct ModelNode ModelNode;
typedef struct ModelFileHeader ModelFileHeader;
typedef struct ModelRoData_GroupRecord {
    coord3d Origin; u16 JointID;
    union { s16 MatrixIDs[3]; struct { s16 MatrixID0,MatrixID1,MatrixID2; }; };
} ModelRoData_GroupRecord;
typedef struct ModelRoData_LODRecord { f32 MinDistance,MaxDistance; ModelNode *Affects; u16 RwDataIndex; } ModelRoData_LODRecord;
typedef struct ModelRoData_DisplayList_CollisionRecord { Gfx *Primary,*Secondary; void *BaseAddr; u16 RwDataIndex; } ModelRoData_DisplayList_CollisionRecord;
typedef struct ModelRoData_DisplayListRecord { Gfx *Primary,*Secondary; void *BaseAddr; } ModelRoData_DisplayListRecord;
typedef union ModelRoData {
    struct { s16 MatrixIndex; } Header;
    ModelRoData_GroupRecord Group;
    struct { s16 Group1; } GroupSimple;
    ModelRoData_LODRecord LOD;
    struct { ModelNode *Controls; } Switch;
    struct { ModelNode *leftChild,*rightChild; } BSP;
    struct { u16 RwDataIndex; } HeadPlaceholder;
    ModelRoData_DisplayList_CollisionRecord DisplayListCollisions;
    ModelRoData_DisplayListRecord DisplayList;
} ModelRoData;
typedef struct ModelRwData_DisplayList_CollisionRecord { Gfx *gdl; } ModelRwData_DisplayList_CollisionRecord;
typedef struct ModelRwData_HeadPlaceholderRecord { ModelFileHeader *ModelFileHeader; void *RwDatas; } ModelRwData_HeadPlaceholderRecord;
typedef union ModelRwData {
    ModelRwData_DisplayList_CollisionRecord DisplayListCollisions;
    ModelRwData_HeadPlaceholderRecord HeadPlaceholder;
} ModelRwData;
struct ModelNode { u16 Opcode,pad; ModelRoData *Data; ModelNode *Parent,*Next,*Prev,*Child; };
struct ModelFileHeader { ModelNode *RootNode; s16 numMatrices; };
typedef struct Model { ModelFileHeader *obj; RenderPosView *render_pos; union ModelRwData **datas; f32 scale; struct Model *attachedto; } Model;
typedef struct ModelRenderData { Mtxf *basemtx; } ModelRenderData;
typedef struct ModelGroupMtxBuildArg { u16 flags,pad; ModelRoData_GroupRecord *group; ModelNode *parentnode; } ModelGroupMtxBuildArg;
static s32 g_ModelDistanceDisabled;
static f32 g_ModelDistanceScale = 1, viewscale = 1;
static float getPlayer_c_lodscalez(void) { return viewscale; }
static void *modelGetNodeRwData(Model *model,ModelNode *node) {
    return &model->datas[node->Data->HeadPlaceholder.RwDataIndex];
}
static void (*g_ModelJointPositionedFunc)(s32,Mtxf *);
static unsigned quaternionBuilds;
static f32 modelGetBendStretchScale(f32 angle) { return sqrtf(tanf(angle)+1); }
typedef struct NodeSpec {
    unsigned op; int parent,next,child,matrix,extra,affects,other;
    float min,max; int primary,secondary;
} NodeSpec;
typedef struct Asset { const char *name; const NodeSpec *nodes; int count,root,matrices; } Asset;
typedef struct Fixture {
    ModelNode nodes[256]; ModelRoData ro[256]; ModelRwData rw[256];
    ModelFileHeader header; Model model; RenderPosView matrices[32];
} Fixture;
