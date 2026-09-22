#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/game/occlusionmath.h"
typedef int bool;
typedef int32_t s32;
typedef uint32_t u32;
typedef int16_t s16;
typedef uint16_t u16;
typedef uint8_t u8;
typedef float f32;
#define TRUE 1
#define FALSE 0
#define GUNRIGHT 0
#define GUNLEFT 1
typedef struct coord3d { union { struct { f32 x,y,z; }; f32 f[3]; }; } coord3d;
typedef struct Mtxf { f32 m[4][4]; } Mtxf;
typedef union RenderPosView { Mtxf pos; } RenderPosView;
typedef int Gfx;
typedef struct Vertex { struct { s16 x,y,z; } coord; s16 flag,s,t; u8 r,g,b,a; } Vertex;
typedef struct sImageTableEntry { int width; } sImageTableEntry;
typedef struct ModelNode ModelNode;
typedef struct ModelRoData_GunfireRecord { coord3d Offset,Size; void *Image; f32 Scale; int RwDataIndex,BaseAddr; } ModelRoData_GunfireRecord;
typedef struct ModelRoData_DisplayListPrimaryRecord { int numVertices; Vertex *Vertices; Gfx *Primary; void *BaseAddr; } ModelRoData_DisplayListPrimaryRecord;
typedef struct ModelRoData_HeaderRecord { s16 MatrixIndex; f32 GroupsAsF32; } ModelRoData_HeaderRecord;
typedef struct ModelRoData_GroupRecord { s16 MatrixIDs[3]; f32 BoundingVolumeRadius; } ModelRoData_GroupRecord;
typedef struct ModelRoData_GroupSimpleRecord { s16 Group1; f32 BoundingVolumeRadius; } ModelRoData_GroupSimpleRecord;
typedef struct ModelRoData_Op11Record { f32 BoundingVolumeRadius; } ModelRoData_Op11Record;
typedef struct { f32 Scale; } ModelRoData_ShadowRecord;
typedef struct { f32 Scale; } ModelRoData_Op14Record;
typedef struct { f32 Scale; } ModelRoData_InterlinkageRecord;
typedef struct { f32 Scale; } ModelNode_Op16Record;
union ModelRoData {
    struct { ModelNode *Controls; } Switch;
    ModelRoData_HeaderRecord Header;
    ModelRoData_GroupRecord Group;
    ModelRoData_GroupSimpleRecord GroupSimple;
    ModelRoData_Op11Record Op11;
    ModelRoData_ShadowRecord Shadow;
    ModelRoData_Op14Record Op14;
    ModelRoData_InterlinkageRecord Interlinkage;
    ModelNode_Op16Record Op16;
    struct { Gfx *Primary,*Secondary; int ModelType; } DisplayList,DisplayListCollisions;
    ModelRoData_DisplayListPrimaryRecord DisplayListPrimary;
    ModelRoData_GunfireRecord Gunfire;
};
union ModelRwData { struct { int visible; } Gunfire,Switch; struct { struct ModelFileHeader *ModelFileHeader; } HeadPlaceholder; };
struct ModelNode { int Opcode; union ModelRoData *Data; ModelNode *Parent,*Next,*Child; union ModelRwData *rwData; int testId; };
typedef struct ModelFileHeader { ModelNode *RootNode; f32 BoundingVolumeRadius; int numMatrices; ModelNode **Switches; int numSwitches; } ModelFileHeader;
typedef struct Model { ModelFileHeader *obj; RenderPosView *render_pos; f32 scale; union ModelRwData rw; } Model;
typedef struct ModelHitEntry { Model *model; ModelNode *rootnode; float sortvalue; struct ModelHitEntry *next,*prev; } ModelHitEntry;
typedef union Colour { u32 word; u8 rgba[4]; } Colour;
typedef struct ModelRenderData { int flags,zbufferenabled,PropType; Gfx *gdl; Colour envcolour,fogcolour; } ModelRenderData;
typedef struct ModelNodeRenderCache { void *colorSegmentBase,*vertexSegmentBase; bool type3PipelineReady; } ModelNodeRenderCache;
struct Scorch { Model *model; int unk02; };
typedef struct ObjectRecord { Model *model; int state,runtime_bitflags; } ObjectRecord;
typedef struct PropRecord PropRecord;
typedef struct rgba_u8 { union { struct { u8 r,g,b,a; }; u8 rgba[4]; }; } rgba_u8;
struct rgba_s32 { union { struct { s32 r,g,b,a; }; s32 rgba[4]; }; };
struct rgba_f32 { float r,g,b,a; };
struct view4f { float left,top,width,height; };
typedef struct ChrRecord { Model *model; u32 chrflags; int fadealpha; rgba_u8 shadecol; PropRecord *weapons_held[2],*handle_positiondata_hat; ModelHitEntry *hitChain; int action,health,animation; } ChrRecord;
struct PropRecord { ChrRecord *chr; ObjectRecord *obj; struct Scorch *scorch; int flags; coord3d pos; };
static Mtxf view;
static int missingCamera;
static Mtxf *currentPlayerGetViewToWorldMtxf(void) { return missingCamera?NULL:&view; }
static union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node) { return node->rwData?node->rwData:&model->rw; }
static int emitted,allocations,relations,conversions,freed,shadowAlpha;
static int nodeDraws[5];
static u32 rng;
static u32 randomGetNext(void) { rng=rng*1664525U+1013904223U; return rng; }
static Vertex allocated[256];
static Vertex *g_ModelVertexAllocator(int count) { assert(count<=256); allocations++; return allocated; }
static Vertex g_GunfireVertexTemplate;
static void command(Gfx *gdl) { emitted++; *gdl=1; }
#define gSPSegment(g,...) command(g)
#define gSPClearGeometryMode(g,...) command(g)
#define gSPSetGeometryMode(g,...) command(g)
#define gSPMatrix(g,...) command(g)
#define gSPVertex(g,...) command(g)
#define gSP2Triangles(g,...) command(g)
#define gDPSetFogColor(g,...) command(g)
#define gSPDisplayList(g,...) command(g)
#define gDPSetColorDither(g,...) command(g)
static s32 coss(u16 a) { return (s32)(cos(a*6.283185307179586/65536)*32767); }
static s32 sins(u16 a) { return (s32)(sin(a*6.283185307179586/65536)*32767); }
static void sub_GAME_7F073038(ModelRenderData *rd,sImageTableEntry *im,s32 arg) { (void)im; (void)arg; command(rd->gdl++); }
static void modelRenderNodeGundl(ModelRenderData *rd,ModelNode *node) { nodeDraws[node->testId]++; command(rd->gdl++); }
static void modelRenderNodeDlWithCache(ModelRenderData *rd,Model *m,ModelNode *n,ModelNodeRenderCache *c) { (void)m;(void)c;nodeDraws[n->testId]++;command(rd->gdl++); }
#define RELATION(name) static void name(Model *m,ModelNode *n) { (void)m; (void)n; relations++; }
RELATION(modelApplyDistanceRelations)
RELATION(modelApplyReorderRelations)
static void modelApplyToggleRelations(Model *m,ModelNode *n) {
    relations++;
    if(n->Data->Switch.Controls) n->Child=modelGetNodeRwData(m,n)->Switch.visible?n->Data->Switch.Controls:NULL;
}
RELATION(modelApplyHeadRelations)
static void modelSetShadowAlpha(int n) { shadowAlpha=n; }
static int chrCalcScreenFadeAlpha(PropRecord *p) { (void)p; return 255; }
static int envColor=2;
static int envGetPropDistColor(PropRecord *p,struct rgba_f32 *c) { (void)p; memset(c,0,sizeof(*c)); return envColor; }
static int getPropCombinedRoomsBBox2D(PropRecord *p,struct view4f *v) { (void)p; memset(v,0,sizeof(*v)); return 1; }
static Gfx *bgScissorCurrentPlayerViewF(Gfx *g,float a,float b,float c,float d) { (void)a;(void)b;(void)c;(void)d;command(g++);return g; }
static Gfx *bgScissorCurrentPlayerViewDefault(Gfx *g) { command(g++);return g; }
static void lerp_rgba_s32_with_rgba_f32(struct rgba_s32 *a,int b,struct rgba_f32 *c) { (void)a;(void)b;(void)c; }
static void bviewTransformManyPosToViewMatrix(RenderPosView *m,int n) { assert(m && n>0); conversions++; }
static void modelHitFreeChain(ModelHitEntry *e) { assert(e);freed++; }
static ModelRenderData D_8002CCBC;
static Colour gBloodColour;
static struct { int time_other_players_on_screen; } playerStats,*g_playerPerm=&playerStats;
#define BULLET_IMPACT_BUFFER_LEN 4
static struct { PropRecord *prop; int room,impact_type,model_render_pos_index; Vertex *vertex_list; } g_BulletImpactBuffer[BULLET_IMPACT_BUFFER_LEN];
static struct { int unk1,unk2; } g_ImpactTypes[2];
static int impactimages[2];
static int bgIsRoomRendered(int room) { (void)room; return 1; }
static Gfx *applyRoomMatrixToDisplayList(Gfx *g,int r) { (void)r; command(g++); return g; }
static void texSelect(Gfx **g,void *im,int a,int b,int c) { (void)im;(void)a;(void)b;(void)c;command((*g)++); }
