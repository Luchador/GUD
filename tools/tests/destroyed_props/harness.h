#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int s32;
typedef unsigned u32;
typedef int bool;
#define TRUE 1
#define FALSE 0
#define PROPSTATE_DESTROYED 0x80
#define PROPRUNTIMEFLAG_ONSCREEN 2
#define PROPFLAG_ORTHOGONAL 1
#define PROPFLAG_FIXED_MONITOR 2
#define PROPFLAG_MONITOR_SECONDARY_SCREENS_DECAL 4
#define PROPFLAG2_DISABLE_ZBUFFER 1
#define DOORFLAG_FLIP 1
enum { PROPDEF_OBJ, PROPDEF_MONITOR, PROPDEF_MULTI_MONITOR, PROPDEF_DOOR };
enum { CULLMODE_BOTH, CULLMODE_NONE, CULLMODE_FRONT, CULLMODE_BACK };
enum { PROP_TYPE_PLAYER = 5, PROP_TYPE_MAX = 9, MODELNODE_OPCODE_DLCOLLISION = 24 };
typedef struct { u32 word; } Gfx, Vertex, Mtx, Mtxf;
union ModelRoData { struct { Vertex *Vertices; s32 RwDataIndex; } DisplayListCollisions; };
union ModelRwData { struct { Vertex *Vertices; Gfx *gdl; } DisplayListCollisions; };
typedef struct ModelNode { int Opcode; union ModelRoData *Data; } ModelNode;
typedef struct { ModelNode *Switches[4]; int numMatrices; } ModelFileHeader;
typedef struct { ModelFileHeader *obj; union ModelRwData rw[5]; void *datas[5]; void *render_pos; ModelNode *body; } Model;
typedef struct PropRecord PropRecord;
typedef struct { int type, state; u32 flags, flags2; Model *model; float maxdamage; } ObjectRecord;
typedef struct { int unused; } MonitorRecord;
typedef struct { ObjectRecord obj; MonitorRecord Monitor; } MonitorObjRecord;
typedef struct { ObjectRecord obj; MonitorRecord Monitor[4]; } MultiMonitorObjRecord;
typedef struct { ObjectRecord obj; int doorFlags; } DoorRecord;
struct PropRecord { u32 flags; ObjectRecord *obj; DoorRecord *door; PropRecord *child, *prev; };
typedef struct { Gfx *gdl; u32 flags; s32 PropType, cullmode; struct { u32 word; } envcolour; } ModelRenderData;
static int screenBuilds, draws;
static int lastCull, lastEnv;
static Gfx screenList;
static union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node)
{ return &model->rw[node->Data->DisplayListCollisions.RwDataIndex]; }
static ModelNode *sub_GAME_7F04B478(ObjectRecord *obj) { return obj->model->body; }
static Mtx *camGetPlayerProjViewMtx(void) { return NULL; }
static Mtx *camGetPlayerProjMtx(void) { return NULL; }
#define gSPClearGeometryMode(p, mode) ((p)->word = 0)
#define gSPMatrix(p, matrix, mode) ((void)(matrix), (p)->word = 1)
static void subdraw(ModelRenderData *data, Model *model)
{ draws++; lastCull = data->cullmode; lastEnv = data->envcolour.word & 255; }
static Gfx *explosionRenderBulletImpactOnProp(Gfx *g, PropRecord *p, bool alpha) { return g; }
static void bviewTransformManyPosToWorldMatrix(Mtxf *m, int n) {}
static void bviewTransformManyPosToViewMatrix(void *m, int n) {}
/* Observe dispatch to the unchanged monitor builder, and emulate its instance
 * list/vertex assignment. Texture generation and rasterization are not tested. */
static Gfx *monitorProcessAndRender(Model *model, ModelNode *node, MonitorRecord *screen, Gfx *g, int z, int pass)
{
    if (node && (node->Opcode & 255) == MODELNODE_OPCODE_DLCOLLISION) {
        union ModelRwData *rw = modelGetNodeRwData(model, node);
        screenBuilds++;
        rw->DisplayListCollisions.gdl = &screenList;
        rw->DisplayListCollisions.Vertices = node->Data->DisplayListCollisions.Vertices;
    }
    return g;
}
