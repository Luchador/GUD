#define MAXROOMCOUNT 2
#define SPSEGMENT_BG_VTX 14
#define OS_K0_TO_PHYSICAL(p) ((u32)(uintptr_t)(p) & 0x1fffffffu)
typedef struct { u16 scale; u8 state, type; } PropDefHeaderRecord;
typedef struct { u16 scale; u8 state, type; void *prop; f32 maxFrac, openPosition; } ObjectRecord;
typedef ObjectRecord DoorRecord;
static DoorRecord g_TestDoor;
static ObjectRecord *setupGetPtrToCommandByIndex(s32 index) { return index==1?&g_TestDoor:NULL; }
static s32 sizepropdef(PropDefHeaderRecord *p) { assert(p->type==PROPDEF_DOOR_SHADOW);return DOOR_SHADOW_BYTES/4; }
typedef union { f32 f[3]; } coord3d;
static struct {coord3d minbounds,maxbounds;Vtx *vertices;} g_BgRoomInfo[MAXROOMCOUNT];
static struct {coord3d pos;} ptr_bgdata_room_fileposition_list[MAXROOMCOUNT];
static s32 g_MaxNumRooms=MAXROOMCOUNT;
static int allocations, frees, failalloc, reclaim, verticesAllocated, freeBytes;
static void *memaAlloc(s32 bytes) { if(failalloc)return NULL;allocations++;return malloc(bytes); }
static void memaFree(void *ptr,s32 bytes) { assert(ptr&&bytes>0);frees++;free(ptr); }
static void memaRealloc(intptr_t ptr,s32 old,s32 size) { assert(ptr&&size<=old&&size>0); }
static void renderCacheRequestReclaim(void) { reclaim++; }
static Vtx frameVertices[18];
static Vtx *dynAllocateVertices(s32 count) { assert(count==18);verticesAllocated++;return frameVertices; }
static s32 dynGetFreeVertexBytes(void) { return freeBytes; }
