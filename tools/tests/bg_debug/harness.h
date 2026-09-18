#define G_OFF 0
#define MAXROOMCOUNT 139
#define SPSEGMENT_BG_VTX 14
#define SPSEGMENT_BG_DL 15
#define CAMERAMODE_FP 4
#define LEVELID_TITLE 90
#define BG_CUTOUT_THRESHOLD 128
#define OS_K0_TO_PHYSICAL(p) ((u32)((uintptr_t)(p) - (uintptr_t)g_TestRam))
typedef struct {
    Vtx *vertices;
    Gfx *primaryGdl, *secondaryGdl;
    s32 primaryGdlSize, secondaryGdlSize, unloadAge;
} RoomInfo;
static RoomInfo g_BgRoomInfo[MAXROOMCOUNT];
static struct {
    Vp viewports[2];
    s32 viewleft, viewtop, viewx, viewy, outside_watch_menu, pausing_flag, pause_state, curRoomIndex;
} g_TestPlayer;
#define g_CurrentPlayer (&g_TestPlayer)
static s32 g_CameraMode = CAMERAMODE_FP;
static s32 g_TestStage, g_TestPlayerCount = 1, g_TestBgEnabled = 1;
static s32 g_TestLoads, g_TestMatrices, g_MaxNumRooms = MAXROOMCOUNT, g_RoomLoadBudget;
static bool g_TestOneCycle;
static struct { Gfx *gdl, *secondaryGdl; } g_BgOneCycleRooms[MAXROOMCOUNT];
static Gfx *g_TestGfxEnd;
static Mtx g_TestMatrix;
u8 *g_BgData;
u8 g_ViBackIndex;
static void *ptrFontBankGothicChars, *ptrFontBankGothic;
static char g_TestLabels[32][64];
static s32 g_TestLabelCount;
static s32 lvlGetCurrentStageToLoad(void) { return g_TestStage; }
static s32 getPlayerCount(void) { return g_TestPlayerCount; }
static bool lvGetBgRenderEnabled(void) { return g_TestBgEnabled; }
static s32 dynGetFreeGfx(Gfx *p) { return (s32)(g_TestGfxEnd - p); }
static Mtx *camGetPlayerProjViewMtx(void) { return &g_TestMatrix; }
static u16 viGetPerspNorm(void) { return 0x8000; }
static Gfx *applyRoomMatrixToDisplayList(Gfx *p, s32 room)
{
    assert(room > 0 && room < MAXROOMCOUNT);
    g_TestMatrices++;
    gSPMatrix(p++, 0x8000, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    return p;
}
static bool renderUseOneCycle(void) { return g_TestOneCycle; }
static void bgLoadRoomModelData(s32 room) { (void)room; g_TestLoads++; }
static s32 viGetX(void) { return 320; }
static s32 viGetY(void) { return 240; }
static Gfx *textRender(Gfx *p, s32 *x, s32 *y, char *text, void *chars,
        void *font, u32 color, s32 width, s32 height, s32 offset, s32 lineheight)
{
    (void)chars; (void)font; (void)color; (void)offset; (void)lineheight;
    assert(*x >= 0 && *x < width && *y >= 0 && *y < height);
    assert(g_TestLabelCount < 32);
    strcpy(g_TestLabels[g_TestLabelCount++], text);
    p += strlen(text) * 12 + 1;
    assert(p + 2 < g_TestGfxEnd);
    return p;
}

static BgVisibilityStats g_TestVisibilityStats;
static void bgGetVisibilityStats(BgVisibilityStats *stats) { *stats = g_TestVisibilityStats; }
