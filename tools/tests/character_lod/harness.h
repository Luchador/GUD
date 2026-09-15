#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef float f32;
typedef int bool;
#define TRUE 1
#define FALSE 0
typedef struct Mtxf { f32 m[4][4]; } Mtxf;
typedef struct RenderPosView { Mtxf pos; } RenderPosView;
typedef struct ModelNode ModelNode;
typedef struct ModelFileHeader ModelFileHeader;
typedef struct ModelRoData_LODRecord { f32 MinDistance,MaxDistance; ModelNode *Affects; u16 RwDataIndex,reserved; } ModelRoData_LODRecord;
typedef struct ModelRwData_LODRecord { bool visible; } ModelRwData_LODRecord;
typedef struct ModelRoData_SwitchRecord { ModelNode *Controls; u16 RwDataIndex; } ModelRoData_SwitchRecord;
typedef struct ModelRwData_SwitchRecord { bool visible; } ModelRwData_SwitchRecord;
typedef struct ModelRwData_HeadPlaceholderRecord { ModelFileHeader *ModelFileHeader; void *RwDatas; } ModelRwData_HeadPlaceholderRecord;
typedef union ModelRoData {
    struct { s16 MatrixIndex; u16 RwDataIndex; } Header;
    struct { s16 MatrixIDs[3]; } Group;
    struct { s16 Group1; } GroupSimple;
    struct { u16 RwDataIndex; } DisplayListCollisions,Op07,BSP,Op11,Gunfire,HeadPlaceholder;
    ModelRoData_LODRecord LOD;
    ModelRoData_SwitchRecord Switch;
} ModelRoData;
typedef union ModelRwData {
    struct { bool visible; } Header,DisplayListCollisions,Op07,BSP,Op11,Gunfire;
    ModelRwData_LODRecord LOD;
    ModelRwData_SwitchRecord Switch;
    ModelRwData_HeadPlaceholderRecord HeadPlaceholder;
} ModelRwData;
struct ModelNode { u16 Opcode; ModelRoData *Data; ModelNode *Parent,*Next,*Prev,*Child; int id; };
struct ModelFileHeader { ModelNode *RootNode; };
typedef struct Model { ModelFileHeader *obj; RenderPosView *render_pos; ModelRwData **datas; f32 scale; } Model;
typedef struct ModelRenderData { f32 depth,spread; int mutate; } ModelRenderData;
static bool g_ModelDistanceDisabled;
static f32 g_ModelDistanceScale=1,viewscale=1;
static unsigned rwlookups,matrixlookups,depthlookups,rwparents,matrixparents;
static f32 getPlayer_c_lodscalez(void) { depthlookups++; return viewscale; }
static ModelRwData *modelGetNodeRwData(Model *,ModelNode *);
static s32 modelFindNodeMtxIndex(ModelNode *,s32);
static void BuildMatrix(ModelRenderData *data,Model *model,ModelNode *node)
{
    int index;
    switch (node->Opcode&255)
    {
    case MODELNODE_OPCODE_HEADER: index=node->Data->Header.MatrixIndex; break;
    case MODELNODE_OPCODE_GROUPSIMPLE: index=node->Data->GroupSimple.Group1; break;
    default: index=node->Data->Group.MatrixIDs[0]; break;
    }
    assert(index>=0 && index<256);
    model->render_pos[index].pos.m[3][2]=-(data->depth+node->id*data->spread);
    /* A joint callback can change a matrix used by later siblings, as well
       as the LOD scale settings. The cache must not survive this event. */
    if (data->mutate==node->id)
    { model->render_pos[0].pos.m[3][2]=-data->depth-500; g_ModelDistanceScale=1.7f; viewscale=0.7f; model->scale=0.3f; }
}
#define process_01_group_heading BuildMatrix
#define process_02_position BuildMatrix
#define process_03_unknown BuildMatrix
#define process_15_subposition BuildMatrix
static void modelUpdateReorderRelations(Model *model,ModelNode *node) {}
static ModelNode *trace[2048];
static unsigned tracecount;
static void Trace(ModelNode *node) { assert(tracecount<2048); trace[tracecount++]=node; }
typedef struct NodeSpec { unsigned op; int parent,next,prev,child,matrix,affects; f32 min,max; } NodeSpec;
typedef struct Asset { const char *name; const NodeSpec *nodes; int count,root; } Asset;
