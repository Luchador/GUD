/* Minimal room/allocator mocks; production lifecycle and selection code follows. */
#define BG_DEBUG_PRIMARY 1
#define BG_DEBUG_SECONDARY 2
static bool g_BgDebugEnabled;
static void bgDebugRecordRoom(s32 room, s32 layer) { (void)room; (void)layer; }
typedef struct { s32 unused[6]; } RoomVtxBatchBounds;
typedef struct {
    Gfx *primaryGdl, *secondaryGdl;
    s32 secondaryGdlSize, primaryGdlSize, cur_room_totalsize, unloadAge, num_vtx_batch_bounds;
    RoomVtxBatchBounds *vtx_batch_bounds;
    Vtx *vertices;
} RoomInfo;
static RoomInfo g_BgRoomInfo[2];
static struct { Gfx *gdl; s32 size; Gfx *secondaryGdl; s32 secondarySize; } g_BgOneCycleRooms[2];
static s32 g_MaxNumRooms = 2, g_RoomLoadBudget;
static s32 g_TestAllocFail, g_TestAllocCount, g_TestFreeCount;
static void *memaAlloc(u32 size)
{
    g_TestAllocCount++;
    assert((size & 15) == 0);
    return g_TestAllocFail ? NULL : malloc(size);
}
static void memaFree(void *p, s32 size)
{
    assert(p && size > 0 && (size & 15) == 0);
    g_TestFreeCount++;
    free(p);
}
/* Room allocation now goes through rendercache.c. Keep this room-lifecycle
 * harness focused on allocation/failure/free dispatch; the model harness
 * exercises the real shared allocator and its drained reclaim. */
static bool renderCacheIsEnabled(void) { return TRUE; }
static void *renderCacheAlloc(s32 size) { return memaAlloc(size); }
static void renderCacheFree(void *p)
{
    assert(p);
    g_TestFreeCount++;
    free(p);
}
static void bgLoadRoomModelData(s32 room) { (void)room; }
static Gfx *applyRoomMatrixToDisplayList(Gfx *gdl, s32 room) { (void)room; return gdl; }
#define SPSEGMENT_BG_VTX 14
#define OS_K0_TO_PHYSICAL(p) ((u32)(uintptr_t)(p) & 0x1fffffffu)
