#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
typedef float f32;
typedef int bool;
#define TRUE 1
#define FALSE 0
#define ARRAYCOUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PORTMAX 200
#define MAXROOMCOUNT 139
#define MAX_PROPS 600
#define BGLOADTYPE_ROOMS 0
#define BGLOADTYPE_SINGLE_DL 1
#define PORTALFLAG_DISABLED 1
#define PORTALFLAG_SPECIAL 2
#define LEVEL_INDEX_CRAD 19
#define LEVELID_CRADLE 41
#define CDTYPE_OBJS 1
#define CDTYPE_DOORS 2
#define CDTYPE_PATHBLOCKER 4

typedef struct coord3d { union { struct { f32 x, y, z; }; f32 f[3]; }; } coord3d;
typedef struct coord2d { union { struct { f32 x, y; }; f32 f[2]; }; } coord2d;
typedef struct bbox2d { union { struct { coord2d min, max; }; f32 f[2][2]; }; } bbox2d;
struct rectbbox { f32 min[2], max[2]; };
typedef struct Mtxf { f32 m[4][4]; } Mtxf;
typedef struct Portal { u8 numPoints, padding[3]; coord3d point; } Portal;
typedef struct PortalData { Portal *portal; u8 connectedRoom1, connectedRoom2, controlbytes1, controlbytes2; } PortalData;
struct PortalMetric { coord3d normal; f32 min, max; };
static struct { u8 numPoints, padding[3]; coord3d points[8]; } portalGeometry[PORTMAX];
static PortalData portalData[PORTMAX + 1];
static PortalData *g_BgPortals = portalData;
static struct PortalMetric g_PortalPlanes[PORTMAX];
static u8 g_PortalIsVertical[PORTMAX], g_PortalTraversalDepths[PORTMAX];
static struct { s32 count; } g_PortalCameraCache[PORTMAX];
static struct { s32 room_rendered, room_loaded_mask; } g_BgRoomInfo[MAXROOMCOUNT];
static coord3d D_80044904 = {{{FLT_MAX, FLT_MAX, FLT_MAX}}};
static coord3d D_80044910 = {{{-FLT_MAX, -FLT_MAX, -FLT_MAX}}};
static s32 g_MaxNumRooms, g_BgRenderMode, g_BgCurrentRoom, g_RoomLoadBudget;
static s32 g_BgGlobalVisAddedRoomCount, levelentry_index, g_CurrentBgLevelId;
static f32 g_LevelScale;
typedef struct GlobalVisCommand { s32 unused; } GlobalVisCommand;
static GlobalVisCommand *g_BgGlobalVisCommands;
enum { CAMERAMODE_INTRO, CAMERAMODE_FADESWIRL, CAMERAMODE_SWIRL, CAMERAMODE_POSEND,
       CAMERAMODE_MP, CAMERAMODE_FP, CAMERAMODE_DEATH_CAM_SP, CAMERAMODE_DEATH_CAM_MP,
       CAMERAMODE_FP_NOINPUT };
typedef struct StandTile { s32 id; } StandTile;
typedef struct PropRecord { coord3d pos; StandTile *stan; } PropRecord;
static struct player {
    f32 c_screentop, c_screenleft, c_screenwidth, c_screenheight;
    f32 c_halfwidth, c_halfheight, c_recipscalex, c_recipscaley;
    bbox2d screensize;
    PropRecord *prop;
} playerData, *g_CurrentPlayer = &playerData;
static coord3d cameraPosition, cameraGround;
static s32 cameraRoom, cameraMode, currentPlayer, playerCount;
static s32 lineHits[PORTMAX], portalVisible[PORTMAX], roomVisible[256], roomFull[256];
static bbox2d portalScreen[PORTMAX];
static f32 portalMargin[PORTMAX];
static s16 RoomPropListBlockIndices[256];
static struct roomproplistblock { s16 propnums[16]; } RoomPropListBlocks[256];
static s16 propResults[MAX_PROPS + 1], *g_RoomPropQueryIndices = propResults;

/* Trace the unchanged engine operations around the optimized searches. */
static struct Trace { s32 kind, value, room, portal, depth; bbox2d box; } trace[8192];
static s32 traceCount;
static void record(s32 kind, s32 value, s32 room, s32 portal, s32 depth, f32 *box)
{
    struct Trace *entry;
    assert(traceCount < (s32)ARRAYCOUNT(trace));
    entry = &trace[traceCount++];
    memset(entry, 0, sizeof(*entry));
    entry->kind = kind; entry->value = value; entry->room = room;
    entry->portal = portal; entry->depth = depth;
    if (box) memcpy(&entry->box, box, sizeof(entry->box));
}
static void bgQueuePortalTraversal(s32 value, s32 room, s32 portal, s32 depth, f32 *box)
{ record(1, value, room, portal, depth, box); }
static s32 bgTestLineIntersectsPortal(s32 portal, coord3d *a, coord3d *b)
{ record(2, 0, 0, portal, 0, NULL); return lineHits[portal]; }
static s32 bgSetRoomOnScreen(s32 room, s32 depth, bbox2d *box, s32 special)
{ record(3, special, room, 0, depth, box->f[0]); return roomFull[room]; }
static void bgResetPortalVisitCounts(void) {}
static void bgResetPortalQueue(void) {}
static s32 bgProcessNextQueuedPortal(void) { return FALSE; }
static void bgExecuteGlobalVisCommands(GlobalVisCommand *commands) {}
static void bgUpdateCurrentPlayerScreenMinMax(void) {}
static s32 bgIsRoomOnScreen(s32 room, struct rectbbox *box) { return roomVisible[room]; }
static s32 bgGetPortalScreenBbox(s32 portal, bbox2d *box)
{ *box = portalScreen[portal]; return portalVisible[portal]; }
static f32 bgGetPortalMargin(s32 portal) { return portalMargin[portal]; }
static void bgRectIntersect(bbox2d *box, bbox2d *other)
{
    if (box->min.x < other->min.x) box->min.x = other->min.x;
    if (box->min.y < other->min.y) box->min.y = other->min.y;
    if (box->max.x > other->max.x) box->max.x = other->max.x;
    if (box->max.y > other->max.y) box->max.y = other->max.y;
}
static s32 get_cur_playernum(void) { return currentPlayer; }
static s32 get_player_position_in_shuffled(s32 player) { return player; }
static void bgRoomsTickUnload(void) { record(4, 0, 0, 0, 0, NULL); }
static s32 bviewGetCameraMode(void) { return cameraMode; }
static s32 bondviewGetPlayerRoom(void) { return cameraRoom; }
static coord3d *bondviewGetPlayerPosition(void) { return &cameraPosition; }
static coord3d *bondviewGetPlayerGroundPosition(void) { return &cameraGround; }
static void bgDetermineVisibleRooms(void);
static void reference_bgDetermineVisibleRooms(void);

static f32 difficulty, crosshairX, crosshairY;
static s32 autoAimX, losResult, losCalls, solidTrace;
static StandTile *losTile;
static f32 getPlayer_c_screentop(void) { return playerData.c_screentop; }
static f32 getPlayer_c_screenleft(void) { return playerData.c_screenleft; }
static f32 getPlayer_c_screenwidth(void) { return playerData.c_screenwidth; }
static f32 getPlayer_c_screenheight(void) { return playerData.c_screenheight; }
static void get_bullet_angle(f32 *x, f32 *y) { *x = crosshairX; *y = crosshairY; }
static s32 currentPlayerGetXAutoAimEnabledRedirect(void) { return autoAimX; }
static s32 getPlayerCount(void) { return playerCount; }
static PropRecord *getCurrentPlayerProp(void) { return playerData.prop; }
static f32 bondviewGetPlayerDuckingHeightRelated(struct player *player) { return 159.0f; }
static void bviewSetPlayerSolid(PropRecord *prop, s32 enabled) { solidTrace = solidTrace * 3 + enabled + 1; }
static s32 stanTestLineUnobstructed(StandTile **tile, f32 sx, f32 sz, f32 ex, f32 ez,
                                  s32 flags, f32 st, f32 sb, f32 et, f32 eb)
{ losCalls++; *tile = losTile; return losResult; }
static f32 floorFloat(f32 x) { return floorf(x); }
static f32 ceilFloat(f32 x) { return ceilf(x); }
static void transform3Dto2DCoords(coord3d *in, coord2d *out);

typedef union RenderPosView { Mtxf pos; } RenderPosView;
union ModelRoData { struct { coord3d Origin; s32 Group1; } GroupSimple; };
typedef struct ModelNode { union ModelRoData *Data; struct ModelNode *Parent; } ModelNode;
typedef struct Model { RenderPosView *render_pos; } Model;
typedef struct ModelRenderData { Mtxf *basemtx; } ModelRenderData;
static Mtxf *nodeParentMatrix;
static Mtxf *modelFindNodeMtx(Model *model, ModelNode *node, s32 index) { return nodeParentMatrix; }
