/* Minimal host context for the actual camera functions extracted by run.py.
 * Room queries are controlled; scale, frustum, screen-box and fog math are real. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>

typedef s32 bool;
#define TRUE 1
#define FALSE 0
#define MAX_PLAYER_COUNT 4
#define PROP_TYPE_VIEWER 1
#define M_PI_F 3.1415927f
#define M_TAU_F 6.2831855f
#define mDegToHalfRad(x) ((x * M_PI_F) / 360.0f)
#define DegToRad(x) (float)((x) * M_TAU_F / 360.0f)
#define M_U16_MAX_VALUE_F 65536.0f
#define M_U32_MAX_VALUE_F 4294967296.0f

typedef union { struct { f32 x, y, z; }; f32 f[3]; } coord3d;
typedef struct { f32 x, y; } coord2d;
typedef struct { coord2d min, max; } bbox2d;
typedef struct { f32 m[4][4]; } Mtxf;
typedef struct { void *stan; s32 type; void *obj; u8 rooms[8]; } PropRecord;

struct player {
    f32 c_screenwidth, c_screenheight, c_halfwidth, c_halfheight;
    f32 c_screenleft, c_screentop, c_perspnear, c_perspfovy, c_perspaspect;
    f32 c_scalex, c_scaley, c_recipscalex, c_recipscaley;
    f32 c_scalelod60, c_scalelod, c_lodscalez;
    u32 c_lodscalezu32;
    coord3d c_cameratopnorm, c_cameraleftnorm;
    Mtxf *viewtoworldmtxf;
};

static struct player players[MAX_PLAYER_COUNT];
static struct player *g_CurrentPlayer;
static s32 player_num;
static coord3d cameraPos;
static Mtxf worldToView;
static struct { bool FogEnabled; } g_CurrentEnvironment;
static f32 g_ScaledFarFogIntensity;
static bool roomRendered, useScreenBox;
static bbox2d roomBox;
static s32 roomQueries, boxQueries, matrixQueries;

/* Count actual expensive work in the host test, without game instrumentation.
 * run.py redirects only the production camera's sinf call through this wrapper. */
static u32 scaleSineCalls;
static f32 countScaleSinf(f32 angle) { scaleSineCalls++; return sinf(angle); }
static coord3d *bondviewGetPlayerPosition(void) { return &cameraPos; }
static Mtxf *camGetWorldToViewMtxf(void) { matrixQueries++; return &worldToView; }
static void chraiGetPropRoomIds(PropRecord *prop, s32 *rooms)
{
    s32 i;
    for (i = 0; i < 7 && prop->rooms[i] != 0xff; i++) rooms[i] = prop->rooms[i];
    rooms[i] = -1;
}
static bool bgIsRoomRendered(s32 room) { roomQueries++; return roomRendered && room == 1; }
static bool bgGet2dBboxByRoomId(s32 room, bbox2d *box)
{
    (void)room; boxQueries++; *box = roomBox; return useScreenBox;
}
static bool getPropCombinedRoomsBBox2D(PropRecord *prop, bbox2d *box)
{
    (void)prop; boxQueries++; *box = roomBox; return useScreenBox;
}

bool envIsPropVisibleThroughFog(coord3d *offset, f32 radius);
