static u8 *heap;

static void reset(int bytes)
{
    memaReset(heap, bytes);
    renderCacheReset();
    modelResets = leafInvalidations = conversions = rejectConversion = 0;
    oneCycle = TRUE; settingsPending = FALSE; g_MainStageNum = -1;
    streamSizes[0] = streamSizes[1] = streamSizes[2] = 64;
    lastDrawn = NULL;
    memset(g_BgRoomInfo, 0, sizeof(g_BgRoomInfo));
    bgClearRoomRenderCaches();
    for (int i = 1; i < MAXROOMCOUNT; i++) {
        g_BgRoomInfo[i].verticesRomBlockSize = 64;
        g_BgRoomInfo[i].primaryGdlRomBlockSize = 64;
        g_BgRoomInfo[i].cur_room_totalsize = 160;
        ptr_bgdata_room_fileposition_list[i].pPointTableBin = (void *)0x0f000100;
        ptr_bgdata_room_fileposition_list[i].primaryGraphics = (void *)0x0f000200;
        ptr_bgdata_room_fileposition_list[i].secondaryGraphics = (void *)0x0f000300;
    }
}

static void assert_full_heap(int size)
{
    /* memaAlloc performs its normal merge passes before declaring failure. */
    void *block = memaAlloc(size);
    assert(block == heap);
    memaFree(block, size);
}

static void check_room_recovery(void)
{
    Gfx out[16], *primary, *secondary;
    Vtx *vertices;
    RoomVtxBatchBounds *bounds;
    u8 saved[192], savedCache[256];
    void *modelCopy;
    int before, conversionCount;
    reset(1024);
    assert(testFrameAllowed(0) && testFrameAllowed(1) && !testFrameAllowed(2));
    g_BgRoomInfo[1].secondaryGdlRomBlockSize = 64;
    g_BgRoomInfo[1].cur_room_totalsize = 224;
    bgLoadRoomModelData(1);
    assert(g_BgRoomInfo[1].unloadAge && g_BgOneCycleRooms[1].gdl
            && g_BgOneCycleRooms[1].secondaryGdl);
    vertices = g_BgRoomInfo[1].vertices;
    primary = g_BgRoomInfo[1].primaryGdl;
    secondary = g_BgRoomInfo[1].secondaryGdl;
    bounds = g_BgRoomInfo[1].vtx_batch_bounds;
    memcpy(saved, vertices, sizeof(saved));
    memcpy(savedCache, g_BgOneCycleRooms[1].gdl, sizeof(savedCache));
    modelCopy = renderCacheAlloc(256); assert(modelCopy); memset(modelCopy, 0x77, 256);
    assert(memaGetLongestFree() < 160);
    before = memaGetLongestFree();
    g_RoomLoadBudget = 3;
    assert(bgRenderRoomPrimary(out, 2) == out);
    assert(!g_BgRoomInfo[2].unloadAge && renderCacheReclaimPending());
    assert(!renderCacheIsEnabled() && memaGetLongestFree() == before);
    assert(!testFrameAllowed(1) && !testFrameAllowed(2) && testFrameAllowed(0));
    assert(!memcmp(savedCache, g_BgOneCycleRooms[1].gdl, sizeof(savedCache)));
    for (int i = 0; i < 256; i++) assert(((u8 *)modelCopy)[i] == 0x77);
    assert(!renderCacheAlloc(16));
    bgRenderRoomPrimary(out, 1); assert(lastDrawn == g_BgOneCycleRooms[1].gdl);
    bgRenderRoomSecondary(out, 1); assert(lastDrawn == g_BgOneCycleRooms[1].secondaryGdl);
    /* Two OS_SC_DONE_MSG completions take the queue from two tasks to zero. */
    for (int pending = 2; pending; pending--) assert(!testFrameAllowed(pending));
    assert(testFrameAllowed(0)); renderCacheReclaim();
    assert(!renderCacheReclaimPending() && !renderCacheIsEnabled());
    assert(modelResets == 1 && !g_BgOneCycleRooms[1].gdl && !g_BgOneCycleRooms[1].secondaryGdl);
    assert(g_BgRoomInfo[1].vertices == vertices && g_BgRoomInfo[1].primaryGdl == primary);
    assert(g_BgRoomInfo[1].secondaryGdl == secondary && g_BgRoomInfo[1].vtx_batch_bounds == bounds);
    assert(!memcmp(saved, vertices, sizeof(saved)));
    for (int i = 0; i < 32; i++) assert(((u8 *)bounds)[i] == 0x33);
    conversionCount = conversions;
    g_RoomLoadBudget = 3;
    assert(bgRenderRoomPrimary(out, 2) > out && g_BgRoomInfo[2].unloadAge);
    assert(lastDrawn == g_BgRoomInfo[2].primaryGdl && conversions == conversionCount);
    bgRenderRoomPrimary(out, 1); assert(lastDrawn == primary);
    bgRenderRoomSecondary(out, 1); assert(lastDrawn == secondary);
    assert(testFrameAllowed(1));
    settingsPending = TRUE; assert(!testFrameAllowed(1) && testFrameAllowed(0));
    settingsPending = FALSE; g_MainStageNum = 1; assert(!testFrameAllowed(0)); g_MainStageNum = -1;
    renderCacheReclaim(); assert(modelResets == 1);
    bgFreeRoomData(1); bgFreeRoomData(2); assert_full_heap(1024);
    puts("PASS: required room loads after drained recovery; source geometry, collision bounds and both fallback passes survive; no rebuild churn.");
}

static void check_allocation_lifetime(void)
{
    void *a, *b, *c;
    int freeBefore;
    reset(1024);
    a = renderCacheAlloc(17); b = renderCacheAlloc(32); c = renderCacheAlloc(48);
    assert(a && b && c && !((uintptr_t)a & 15) && !((uintptr_t)b & 15) && !((uintptr_t)c & 15));
    renderCacheFree(b); /* Middle. */
    renderCacheFree(a); /* Tail. */
    renderCacheFree(c); /* Head. */
    renderCacheFree(NULL); assert_full_heap(1024);
    assert(!renderCacheAlloc(0) && !renderCacheAlloc(-1) && !renderCacheAlloc(0x7fffffff));
    assert(!renderCacheAlloc(2048) && renderCacheIsEnabled() && !renderCacheReclaimPending());
    /* Normal room unloading unregisters both copies; recovery must not free them twice. */
    g_BgRoomInfo[1].secondaryGdlRomBlockSize = 64;
    g_BgRoomInfo[1].cur_room_totalsize = 224;
    bgLoadRoomModelData(1); bgFreeRoomData(1); assert_full_heap(1024);
    rejectConversion = TRUE; bgLoadRoomModelData(2);
    assert(!g_BgOneCycleRooms[2].gdl && !g_RenderCacheAllocations);
    bgFreeRoomData(2); assert_full_heap(1024);
    /* Failure caused entirely by required data needs no graphics drain. */
    a = memaAlloc(1024); assert(a); bgLoadRoomModelData(3);
    assert(!g_BgRoomInfo[3].unloadAge && !renderCacheReclaimPending());
    assert(!renderCacheIsEnabled() && testFrameAllowed(1));
    memaFree(a, 1024); bgLoadRoomModelData(3);
    assert(g_BgRoomInfo[3].unloadAge && !g_BgOneCycleRooms[3].gdl);
    bgFreeRoomData(3);
    /* No cached size and no available bytes must not become a zero-byte allocation. */
    a = memaAlloc(1024); g_BgRoomInfo[4].cur_room_totalsize = -1; bgLoadRoomModelData(4);
    assert(!g_BgRoomInfo[4].unloadAge); memaFree(a, 1024);
    /* Stage transition also discards a pending request without touching the old heap. */
    reset(1024); a = renderCacheAlloc(32); assert(a); renderCacheRequestReclaim();
    assert(renderCacheReclaimPending()); reset(1024);
    assert(renderCacheIsEnabled() && !renderCacheReclaimPending());
    freeBefore = memaGetLongestFree(); renderCacheReclaim(); assert(memaGetLongestFree() == freeBefore);
    bgLoadRoomModelData(1); assert(g_BgOneCycleRooms[1].gdl); bgFreeRoomData(1);
    assert_full_heap(1024);
    puts("PASS: aligned allocation, unlink/free, conversion failure, exhaustion, repeated reclaim and stage-reset lifetimes.");
}

static void check_short_list_reload(void)
{
    /* Depot room 38 contains 5808 vertex bytes and two 104-byte state-only
     * lists. Room 88 has 256 vertex bytes and a 72-byte primary list.
     * No texture expansion hides the DMA workspace requirement. */
    const int sizes[][3] = {{5808, 104, 104}, {256, 72, 0},
                            {64, 64, 104}, {64, 104, 64}};
    for (unsigned int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        reset(0x10000);
        oneCycle = FALSE;
        memcpy(streamSizes, sizes[i], sizeof(streamSizes));
        g_BgRoomInfo[1].cur_room_totalsize = -1;
        g_BgRoomInfo[1].secondaryGdlRomBlockSize = sizes[i][2] != 0;
        int allocation = 0;
        for (int reload = 0; reload < 4; reload++) {
            bgLoadRoomModelData(1);
            RoomInfo *room = &g_BgRoomInfo[1];
            assert(room->unloadAge && room->vertices && room->primaryGdl);
            assert(room->verticesSize == sizes[i][0] && room->primaryGdlSize == sizes[i][1]);
            if (sizes[i][2]) assert(room->secondaryGdl && room->secondaryGdlSize == sizes[i][2]);
            if (reload) assert(room->cur_room_totalsize == allocation);
            allocation = room->cur_room_totalsize;
            bgFreeRoomData(1);
            assert_full_heap(0x10000);
        }
    }
    puts("PASS: short state-only primary/secondary lists survive repeated room reloads without losing streams or shrinking the cached allocation.");
}

static void check_partial_room_failure(void)
{
    /* Leave just enough space to fail the vertex, primary or secondary
     * loader in turn. Releasing the other allocation must allow a retry. */
    for (int stream = 0; stream < 3; stream++) {
        reset(4096);
        g_BgRoomInfo[1].cur_room_totalsize = -1;
        g_BgRoomInfo[1].secondaryGdlRomBlockSize = 64;
        int occupied = 4096 - (stream + 1) * 64;
        void *otherRoom = memaAlloc(occupied);
        assert(otherRoom);
        bgLoadRoomModelData(1);
        RoomInfo *room = &g_BgRoomInfo[1];
        assert(!room->unloadAge && room->cur_room_totalsize == -1);
        assert(!room->vertices && !room->primaryGdl && !room->secondaryGdl);
        assert(!room->verticesSize && !room->primaryGdlSize && !room->secondaryGdlSize);
        assert(!room->vtx_batch_bounds && !conversions);
        assert(!renderCacheIsEnabled());
        memaFree(otherRoom, occupied);
        assert_full_heap(4096);
        bgLoadRoomModelData(1);
        assert(room->unloadAge && room->vertices && room->primaryGdl && room->secondaryGdl);
        bgFreeRoomData(1);
        assert_full_heap(4096);
    }
    puts("PASS: failure at each room stream rolls back the allocation, leaves no published partial room and recovers on retry.");
}

int main(void)
{
    /* Keep production mema's N64 32-bit addresses intact on the host. */
    heap = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    assert(heap != MAP_FAILED && (uintptr_t)heap + 0x10000 < 0x80000000u);
    check_room_recovery();
    check_allocation_lifetime();
    check_short_list_reload();
    check_partial_room_failure();
    assert(munmap(heap, 0x10000) == 0);
    return 0;
}
