#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int s32, bool;
typedef unsigned u32;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint8_t u8;
typedef float f32;
#define TRUE 1
#define FALSE 0
#define PROPSTATE_DESTROYED 0x80
#define PROPRUNTIMEFLAG_ONSCREEN 2
#define PROPFLAG_ORTHOGONAL 1
#define PROPFLAG_FIXED_MONITOR 2
#define PROPFLAG_MONITOR_SECONDARY_SCREENS_DECAL 4
#define PROPFLAG2_DISABLE_ZBUFFER 1
#define DOORFLAG_FLIP 1
#define M_TAU_F 6.283185307179586f
#define M_U16_MAX_VALUE_F 65535.0f
#define MONITOR_TIMER_DELTA g_GlobalTimerDelta
enum { PROPDEF_OBJ, PROPDEF_MONITOR, PROPDEF_MULTI_MONITOR, PROPDEF_DOOR };
enum { CULLMODE_BOTH, CULLMODE_NONE, CULLMODE_FRONT, CULLMODE_BACK };
enum { PROP_TYPE_PLAYER=5, PROP_TYPE_MAX=9 };
typedef struct Gfx { int op; struct Gfx *target; } Gfx;
typedef struct { u32 word; } Mtx, Mtxf;
typedef struct { s16 x,y,z,flag,s,t; u8 r,g,b,a; } Vertex;
typedef struct sImageTableEntry { int width; } sImageTableEntry;
/* MONITOR_RECORD */
union ModelRoData { struct { Vertex *Vertices; s32 RwDataIndex; } DisplayListCollisions; };
union ModelRwData { struct { Vertex *Vertices; Gfx *gdl; } DisplayListCollisions; };
typedef struct ModelNode {
    int Opcode; union ModelRoData *Data; struct ModelNode *Child,*Parent,*Next;
} ModelNode;
typedef struct { ModelNode *Switches[4],*RootNode; int numMatrices; } ModelFileHeader;
typedef struct {
    ModelFileHeader *obj; union ModelRwData rw[5]; void *datas[5]; void *render_pos; ModelNode *body;
} Model;
typedef struct PropRecord PropRecord;
typedef struct { int type,state,obj; u32 flags,flags2; Model *model; float maxdamage; } ObjectRecord;
typedef struct { ObjectRecord obj; MonitorRecord Monitor; } MonitorObjRecord;
typedef struct { ObjectRecord obj; MonitorRecord Monitor[4]; } MultiMonitorObjRecord;
typedef struct { ObjectRecord obj; int doorFlags; } DoorRecord;
struct PropRecord { u32 flags; ObjectRecord *obj; DoorRecord *door; PropRecord *child,*prev; };
typedef struct { Gfx *gdl; u32 flags; s32 PropType,cullmode; struct { u32 word; } envcolour; } ModelRenderData;
static sImageTableEntry monitorimages[100];
static Vertex vertexArena[256];
static int vertexCount, textures, draws, relations, impacts, impactDraws, viewConversions, worldConversions;
static int g_ClockTimer=1;
static float g_GlobalTimerDelta=1;
static u32 rng;
static Mtx projection;
static union ModelRwData *modelGetNodeRwData(Model *m, ModelNode *n)
{ return &m->rw[n->Data->DisplayListCollisions.RwDataIndex]; }
static void modelLodInvalidateInstance(Model *m) {}
static ModelNode *sub_GAME_7F04B478(ObjectRecord *o) { return o->model->body; }
static Mtx *camGetPlayerProjViewMtx(void) { return &projection; }
static Mtx *camGetPlayerProjMtx(void) { return &projection; }
static u32 randomGetNext(void) { rng=rng*1664525U+1013904223U; return rng; }
static Vertex *dynAllocateVertices(int count)
{ Vertex *v=&vertexArena[vertexCount]; vertexCount+=count; assert(vertexCount<=256); return v; }
static void emit(Gfx *g, int op) { g->op=op; g->target=NULL; }
#define gSPClearGeometryMode(g,...) emit(g,1)
#define gSPSetGeometryMode(g,...) emit(g,2)
#define osVirtualToPhysical(p) (p)
#define gSPMatrix(g,m,mode) ((void)(m),emit(g,3))
#define gSPVertex(g,...) emit(g,4)
#define gSP2Triangles(g,...) emit(g,5)
#define gSPEndDisplayList(g) emit(g,6)
#define gSPSegment(g,...) emit(g,7)
static void branch(Gfx *g, Gfx *target) { emit(g,8); g->target=target; }
#define gSPBranchList(g,t) branch(g,t)
static void texSelect(Gfx **g, sImageTableEntry *image, int pass,int mode,int arg)
{ textures++; emit((*g)++,9); }
static void monitorApplyCachedUvs(MonitorRecord *screen,sImageTableEntry *image,Vertex *v) {}
static void modelApplyDistanceRelations(Model *m,ModelNode *n) { relations++; }
static void modelApplyToggleRelations(Model *m,ModelNode *n) { relations++; }
static void modelApplyHeadRelations(Model *m,ModelNode *n) { relations++; }
static void modelApplyReorderRelations(Model *m,ModelNode *n) { relations++; }
static void modelRenderNodeDl(ModelRenderData *d,Model *m,ModelNode *n)
{ draws++; emit(d->gdl++,10); }
static void modelRenderNodeGundl(ModelRenderData *d,ModelNode *n)
{ draws++; emit(d->gdl++,10); }
static void modelRenderShadow(ModelRenderData *d,Model *m,ModelNode *n)
{ draws++; emit(d->gdl++,10); }
/* Actual effect RNG paths are separately exercised by character_occlusion. */
static void modelRenderGunfire(ModelRenderData *d,Model *m,ModelNode *n) {}
static void modelRenderRotatingTexture(ModelRenderData *d,ModelNode *n) {}
static Gfx *explosionRenderBulletImpactOnPropFiltered(Gfx *g,PropRecord *p,bool pass,bool render)
{ impacts++; impactDraws+=render; if(render) emit(g++,10); return g; }
static void bviewTransformManyPosToWorldMatrix(Mtxf *m,int n) { worldConversions++; }
static void bviewTransformManyPosToViewMatrix(void *m,int n) { viewConversions++; }
