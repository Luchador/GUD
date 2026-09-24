#define MAXROOMCOUNT 2
#define SPSEGMENT_BG_VTX 14
#define OS_K0_TO_PHYSICAL(p) ((u32)(uintptr_t)(p) & 0x1fffffffu)
typedef struct { u16 scale; u8 state, type; } PropDefHeaderRecord;
typedef struct { u16 scale; u8 state, type; void *prop; f32 maxFrac, openPosition; } ObjectRecord;
typedef ObjectRecord DoorRecord;
static DoorRecord g_TestDoor;
static ObjectRecord *setupGetPtrToCommandByIndex(s32 index) { return index==1?&g_TestDoor:NULL; }
static s32 sizepropdef(PropDefHeaderRecord *p) { assert(p->type==PROPDEF_DOOR_SHADOW);return DOOR_SHADOW_BYTES/4; }
typedef union { struct { f32 x,y,z; }; f32 f[3]; } coord3d;
typedef struct { struct { s16 x,y,z; } coord; s16 index,s,t; u8 r,g,b,a; } Vertex;
typedef struct HitThing {
    coord3d hitpos,normal;
    Vertex *vtx0,*vtx1,*vtx2;
    Gfx *tricmd;
    s16 unk28,texturenum,tileformat,tilesize;
} HitThing;
typedef struct { s32 xmin,ymin,zmin,xmax,ymax,zmax; } RoomVtxBatchBounds;
typedef union { f32 m[4][4]; s32 unused; } Mtxf;
static struct TestPlayer { coord3d current_model_pos; } testPlayer;
static struct TestPlayer *g_CurrentPlayer=&testPlayer;
static f32 g_LevelInverseScale=1;
static f32 g_LevelScale=1;
static f32 g_MtxConversionScale[2]={65536,65536};
typedef struct {
    coord3d minbounds,maxbounds;
    Vtx *vertices;
    Gfx *primaryGdl;
    RoomVtxBatchBounds *vtx_batch_bounds;
    s16 num_vtx_batch_bounds;
} RoomInfo;
static RoomInfo g_BgRoomInfo[MAXROOMCOUNT];
static struct {coord3d pos;} ptr_bgdata_room_fileposition_list[MAXROOMCOUNT];
static s32 g_MaxNumRooms=MAXROOMCOUNT;
#define SQ(x) ((x)*(x))
static f32 bgGetRoomScale(void) { return g_LevelScale; }
static f32 bgGetRoomInverseScale(void) { return 1/g_LevelScale; }
static s32 bgGetMaxNumRooms(void) { return g_MaxNumRooms; }
static int allocations, frees, failalloc, reclaim, verticesAllocated, matricesAllocated, freeBytes;
static int frameVertexCount,frameMatrixCount,frameBytes;
static void *memaAlloc(s32 bytes) { if(failalloc)return NULL;allocations++;return malloc(bytes); }
static void memaFree(void *ptr,s32 bytes) { assert(ptr&&bytes>0);frees++;free(ptr); }
static void memaRealloc(intptr_t ptr,s32 old,s32 size) { assert(ptr&&size<=old&&size>0); }
static void renderCacheRequestReclaim(void) { reclaim++; }
static Vtx frameVertexBuffers[DOOR_SHADOW_MAX][18];
static Mtx frameMatrixBuffers[DOOR_SHADOW_MAX],roomMatrix;
#define frameVertices frameVertexBuffers[0]
#define frameMatrix frameMatrixBuffers[0]
#define SHADOW_FRAME_BYTES (sizeof(frameVertices)+sizeof(Mtx))
static Vtx *dynAllocateVertices(s32 count) {
    assert(count==18&&frameVertexCount<DOOR_SHADOW_MAX&&frameBytes>=(int)sizeof(frameVertices));
    frameBytes-=sizeof(frameVertices);verticesAllocated++;return frameVertexBuffers[frameVertexCount++];
}
static Mtx *dynAllocateMatrix(void) {
    assert(frameMatrixCount<DOOR_SHADOW_MAX&&frameBytes>=(int)sizeof(Mtx));
    frameBytes-=sizeof(Mtx);matricesAllocated++;return &frameMatrixBuffers[frameMatrixCount++];
}
static s32 dynGetFreeVertexBytes(void) { return frameBytes; }
static Gfx *applyRoomMatrixToDisplayList(Gfx *gdl,s32 room) {
    assert(room==1);
    gSPMatrix(gdl++,OS_K0_TO_PHYSICAL(&roomMatrix),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    return gdl;
}
static double matrixValue(const Mtx *matrix,int row,int column) {
    const u32 *words=(const u32 *)matrix->m;
    int i=row*4+column,pair=i/2;
    u32 value=i&1?(words[pair]<<16)|(words[pair+8]&0xffff)
        :(words[pair]&0xffff0000)|(words[pair+8]>>16);
    return (s32)value/65536.0;
}
static double transformedPosition(const Vtx *vertex,const Mtx *matrix,int axis) {
    double result=matrixValue(matrix,3,axis);
    for(int a=0;a<3;a++)result+=vertex->v.ob[a]*matrixValue(matrix,a,axis);
    return result;
}
