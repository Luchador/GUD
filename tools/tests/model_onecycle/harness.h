typedef struct { float unused[16]; } Mtxf;
typedef union { u32 word; u8 rgba[4]; } rgba_u8;
typedef struct ModelRenderData {
    Mtxf *basemtx; bool zbufferenabled; u32 flags; Gfx *gdl; Mtxf *mtxlist;
    s32 PropType; rgba_u8 envcolour, fogcolour; u32 cullmode;
} ModelRenderData;
enum { PROP_TYPE_NUL, PROP_TYPE_OBJ, PROP_TYPE_DOOR, PROP_TYPE_CHR,
       PROP_TYPE_WEAPON, PROP_TYPE_PLAYER, PROP_TYPE_VIEWER, PROP_TYPE_EXPLOSION,
       PROP_TYPE_SMOKE, PROP_TYPE_MAX };
enum { CULLMODE_BOTH, CULLMODE_NONE, CULLMODE_FRONT, CULLMODE_BACK };
#define SPSEGMENT_MODEL_COL1 5
#define SPSEGMENT_MODEL_VTX 4
typedef struct { Gfx *Primary, *Secondary; void *BaseAddr; s32 ModelType; } ModelRoData_DisplayListRecord;
union ModelRoData {
    ModelRoData_DisplayListRecord DisplayList;
    ModelRoData_DisplayListRecord DisplayListCollisions;
};
union ModelRwData { struct { Gfx *gdl; void *Vertices; } DisplayListCollisions; };
typedef struct { union ModelRwData *rwdata; } Model;
typedef struct { union ModelRoData *Data; } ModelNode;
typedef struct { void *colorSegmentBase, *vertexSegmentBase; bool type3PipelineReady; } ModelNodeRenderCache;
static union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node)
{ (void)node; return model->rwdata; }
#undef K0_TO_PHYS
#undef IS_KSEG0
#define _SHIFTR(v, s, w) (((u32)(v) >> (s)) & (0xffffffffu >> (32 - (w))))
#define K0_TO_PHYS(p) ((u32)((const u8 *)(p) - g_TestRam))
#define IS_KSEG0(p) ((uintptr_t)(p) >= (uintptr_t)g_TestRam && (uintptr_t)(p) < (uintptr_t)(g_TestRam + sizeof(g_TestRam)))
#define osVirtualToPhysical(p) K0_TO_PHYS(p)
static u32 testDisplayListAddress(const Gfx *list)
{ return IS_KSEG0(list) ? K0_TO_PHYS(list) : (u32)(uintptr_t)list; }
#undef gSPDisplayList
#define gSPDisplayList(pkt, dl) gDma1p(pkt, G_DL, testDisplayListAddress(dl), 0, G_DL_PUSH)
static s32 allocated, allocations, frees, failAllocation;
static void *memaAlloc(u32 bytes)
{
    void *p;
    allocations++;
    if (failAllocation) return NULL;
    assert(allocated + bytes <= 0x10000);
    p = g_TestRam + 0x80000 + allocated;
    allocated += bytes;
    return p;
}
static void memaFree(void *p, u32 bytes)
{ (void)p; frees++; allocated -= bytes; }
static s32 frameBytes;
static void *dynAllocate(s32 size)
{
    void *p = g_TestRam + 0x40000 + frameBytes;
    frameBytes += (size + 15) & ~15;
    assert(frameBytes <= 0x10000);
    return p;
}
