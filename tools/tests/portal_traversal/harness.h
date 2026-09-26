#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef int8_t s8;
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
#define BGLOADTYPE_ROOMS 0
#define ARRAYCOUNT(a) (sizeof(a) / sizeof((a)[0]))
#define LEVELID_CONTROL 1
#define LEVELID_JUNGLE 2
typedef int LEVELID;
#define PORTALFLAG_DISABLED 1
#define PORTALFLAG_SPECIAL 2
typedef s32 PORTALFLAGS;
typedef struct { f32 x, y; } coord2d;
typedef struct { union { struct { f32 x, y, z; }; f32 f[3]; }; } coord3d;
typedef struct bbox2d { union { struct { coord2d min, max; }; f32 f[2][2]; }; } bbox2d;
struct rectbbox { float left, up, right, down; };
typedef struct Portal { u8 numPoints, padding[3]; coord3d point; } Portal;
typedef struct PortalData { Portal *portal; u8 connectedRoom1, connectedRoom2, controlbytes1, controlbytes2; } PortalData;
typedef struct { u8 room_rendered, room_loaded_mask, portal_visit_count, unloadAge;
    coord3d minbounds, maxbounds; } RoomInfo;
typedef struct { s32 roomid, draworder; bbox2d bbox; PORTALFLAGS specialPortalFlags; } BgDrawSlot;
struct PortalMetric { coord3d normal; f32 min, max; };
static RoomInfo g_BgRoomInfo[MAXROOMCOUNT];
static BgDrawSlot g_BgDrawSlots[204];
static PortalData portals[PORTMAX + 1], *g_BgPortals = portals;
static struct { u8 numPoints, padding[3]; coord3d points[8]; } geometry[PORTMAX];
static s32 g_MaxNumRooms = MAXROOMCOUNT, g_BgRenderMode = BGLOADTYPE_ROOMS, g_CurrentBgLevelId;
static coord3d D_80044904 = {{{FLT_MAX, FLT_MAX, FLT_MAX}}};
static coord3d D_80044910 = {{{-FLT_MAX, -FLT_MAX, -FLT_MAX}}};
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
