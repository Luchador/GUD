static struct { void *pPointTableBin, *primaryGraphics, *secondaryGraphics; }
    ptr_bgdata_room_fileposition_list[MAXROOMCOUNT];
static struct { char *bg_seg_filename; } testLevel;
#define g_CurrentBgLevel (&testLevel)
static struct { int FogEnabled; } env;
#define envGetCurrent() (&env)
static s32 g_MaxNumRooms = MAXROOMCOUNT, g_RoomLoadBudget;
static bool g_BgDebugEnabled, oneCycle = TRUE, settingsPending;
static s32 g_MainStageNum = -1;
static int conversions, rejectConversion, modelResets, leafInvalidations;
static int primaryCacheSize = 256, secondaryCacheSize = 128;
static Gfx *lastDrawn;
static void doorShadowClearRenderCaches(void) {}
static void doorShadowFreeRoom(int room) { (void)room; }
static Gfx *doorShadowRenderRoom(Gfx *gdl, int room, int layer, bool oneCycle)
{ (void)room; (void)layer; (void)oneCycle; return gdl; }
static void bgApplyEnvironmentMapping(Gfx *start, Gfx *end, Vtx *vertices, u32 bytes)
{ (void)start; (void)end; (void)vertices; (void)bytes; }
static void modelOneCycleResetCache(void) { modelResets++; }
static void renderInvalidateDisplayListCache(void) { leafInvalidations++; }
static bool renderSettingsPending(void) { return settingsPending; }
static bool renderUseOneCycle(void) { return oneCycle; }
static void bgDebugRecordRoom(int room, int pass) { (void)room; (void)pass; }
static Gfx *applyRoomMatrixToDisplayList(Gfx *gdl, int room) { (void)room; return gdl; }
#define OS_K0_TO_PHYSICAL(p) (p)
#define gSPSegment(p,s,v) ((void)(p))
#define gSPDisplayList(p,v) ((void)(p), lastDrawn = (Gfx *)(v))
#define gDPPipeSync(p) ((void)(p))
#define gDPSetAlphaCompare(p,a) ((void)(p))
#define gDPSetBlendColor(p,r,g,b,a) ((void)(p))
static int convert(Gfx *dst, int cap, int size)
{
    conversions++;
    if (dst) {
        if (rejectConversion) return -1;
        assert(cap >= size);
        memset(dst, 0xcc, size);
    }
    return size;
}
static int bgBuildOneCycleGdl(Gfx *src, int size, Gfx *dst, int cap)
{ (void)src; (void)size; return convert(dst, cap, primaryCacheSize); }
static int bgBuildCutoutGdl(Gfx *src, int size, Gfx *dst, int cap)
{ (void)src; (void)size; return convert(dst, cap, secondaryCacheSize); }
static int streamSizes[3] = {64, 64, 64};
static int bgGetRoomStreamSize(int offset)
{ assert(offset >= 0x100 && offset <= 0x300); return streamSizes[offset / 0x100 - 1]; }
static void obLoadBGFileBytesAtOffset(char *file, u8 *dst, int offset, int size)
{ (void)file; (void)offset; memset(dst, 0x5a, size); }
static int texLoadFromGdl(Gfx *src, int size, Gfx *dst, void *pool)
{ (void)pool; memmove(dst, src, size); return size; }
static void clear_light_fixturetable_in_room(int room) { (void)room; }
static void redarken_lights_in_room(int room) { (void)room; }
static void bgApplyDynamicCCRMLUT(Gfx *a, Gfx *b, int mode)
{ (void)a; (void)b; (void)mode; }
static void bgBuildRoomVtxBounds(int room)
{
    g_BgRoomInfo[room].vtx_batch_bounds = memaAlloc(32);
    if (g_BgRoomInfo[room].vtx_batch_bounds) {
        g_BgRoomInfo[room].num_vtx_batch_bounds = 1;
        memset(g_BgRoomInfo[room].vtx_batch_bounds, 0x33, 32);
    }
}
