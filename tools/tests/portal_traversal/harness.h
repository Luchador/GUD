#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef float f32;
typedef int bool;
#define TRUE 1
#define FALSE 0
#define PORTMAX 200
#define MAXROOMCOUNT 139
#define BG_PORTAL_ROOM_COUNT 256
#define PORTALFLAG_DISABLED 1
#define PORTALFLAG_SPECIAL 2
typedef s32 PORTALFLAGS;
typedef struct { f32 x, y; } coord2d;
typedef struct { f32 x, y, z; } coord3d;
typedef struct bbox2d { union { struct { coord2d min, max; }; f32 f[2][2]; }; } bbox2d;
struct rectbbox { float left, up, right, down; };
typedef struct PortalData { void *portal; u8 connectedRoom1, connectedRoom2, controlbytes1, controlbytes2; } PortalData;
typedef struct { u8 room_rendered, room_loaded_mask, portal_visit_count, unloadAge; } RoomInfo;
typedef struct { s32 roomid, draworder; bbox2d bbox; PORTALFLAGS specialPortalFlags; } BgDrawSlot;
struct PortalMetric { coord3d normal; f32 min, max; };
static RoomInfo g_BgRoomInfo[MAXROOMCOUNT];
static BgDrawSlot g_BgDrawSlots[204];
static PortalData portals[PORTMAX], *g_BgPortals = portals;
static struct PortalMetric g_PortalPlanes[PORTMAX];
static u8 g_PortalTraversalDepths[PORTMAX];
static u16 g_BgRoomPortalOffsets[BG_PORTAL_ROOM_COUNT + 1];
static u8 g_BgRoomPortalIndices[PORTMAX * 2];
static s32 g_BgRoomsScheduledToBeDrawn;
static f32 g_LevelScale = 1;
static coord3d camera;
static struct { bbox2d screensize; } player;
#define g_CurrentPlayer (&player)
static bbox2d portalBoxes[PORTMAX];
static int portalVisible[PORTMAX], roomVisible[MAXROOMCOUNT];
static int numPortals, rootRoom, dispatched, peak, maximumDepth;
static int traceOnly, requeueSelf;
static bbox2d lastParent;
static coord3d *bondviewGetPlayerPosition(void) { return &camera; }
static f32 bgGetPortalMargin(s32 portal) { (void)portal; return 0; }
static bool bgGetPortalScreenBbox(s32 portal, bbox2d *box)
{ *box = portalBoxes[portal]; return portalVisible[portal]; }
static bool bgIsRoomOnScreen(s32 room, struct rectbbox *box)
{ (void)box; return roomVisible[room]; }

static s32 g_MaxNumRooms = MAXROOMCOUNT;
static bool cacheEnabled;
static bool renderCacheIsEnabled(void) { return cacheEnabled; }
