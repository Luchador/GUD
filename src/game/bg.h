#ifndef _BG_H_
#define _BG_H_
#include <ultra64.h>
#include <bondgame.h>
#include <bondtypes.h>
#include <bondconstants.h>

struct levelentry
{
    s32 levelID;
    void *bg_seg_filename;
    void *bg_stan_filename;
    f32 levelscale;
    f32 visibility;
    f32 unknownfloat;
};
// cannon definition
#define MAXPORTALSPERROOM 20

// cannonical name
#define PORTMAX 200

#define BG_SEG_TO_PTR(base, off) ((void *) (((u32) (base)) + (((u32) (off)) + 0xF1000000)))

typedef struct RoomVtxBatchBounds {
    s16 gdlindex;    // 0x00
    s16 pad02;       // 0x02

    union {
        struct {
            s32 xmin; // 0x04
            s32 ymin; // 0x08
            s32 zmin; // 0x0c
            s32 xmax; // 0x10
            s32 ymax; // 0x14
            s32 zmax; // 0x18
        };

        struct {
            s32 min[3]; // 0x04
            s32 max[3]; // 0x10
        };
    };
} RoomVtxBatchBounds; // size = 0x1c

typedef struct BoundVec { 
    s32 x, y, z; 
} BoundVec;

typedef struct RoomInfo {
    // is room being rendered? boolean
    u8 room_rendered;                       // 0x00

    // is the room a neighbor to a room being rendered? boolean
    u8 room_neighbor_to_rendered;           // 0x01

    /**
     * Acts like a small room age counter.
     * 0 = unloaded
     * 1 = loaded/used recently
     * 2-3 = loaded but aging towards unload
     * 4 = unload on tick
     */
    u8 unloadAge;                          // 0x02

    /**
     * Counts how often this room has been reached during the current portal
     * visibility traversal.
     */
    u8 portal_visit_count;                  // 0x03

    Vtx *vertices;                          // 0x04
    Gfx *primaryGdl;                        // 0x08
    void *secondaryGdl;                     // 0x0c

    s32 verticesRomBlockSize;               // 0x10
    s32 primaryGdlRomBlockSize;             // 0x14
    s32 secondaryGdlRomBlockSize;           // 0x18

    s32 verticesSize;                       // 0x1c
    s32 primaryGdlSize;                     // 0x20
    s32 secondaryGdlSize;                   // 0x24

    s32 cur_room_totalsize;                 // 0x28
    RoomVtxBatchBounds *vtx_batch_bounds;   // 0x2c

    s16 num_vtx_batch_bounds;               // 0x30
    s16 field_32;                           // 0x32

    u8 room_loaded_mask;                    // 0x34
    u8 keepLoaded;                          // 0x35
    s16 mtxid;                              // 0x36

    coord3d minbounds;                      // 0x38
    coord3d maxbounds;                      // 0x44
} RoomInfo; 

typedef struct BgDrawSlot
{
    s32 roomid;
    s32 draworder;
    struct bbox2d bbox;
    PORTALFLAGS specialPortalFlags;
} BgDrawSlot;

typedef struct Portal
{
    u8 numPoints;
    u8 padding[3];
    coord3d point;
} Portal;

typedef struct PortalData
{
    Portal *portal;
    u8 connectedRoom1;
    u8 connectedRoom2;
    u8 controlbytes1;
    u8 controlbytes2;
} PortalData;

typedef struct BgRoomData
{
    void* pPointTableBin;
    void* primaryGraphics;
    void* secondaryGraphics;
    coord3d pos;
} BgRoomData;

typedef struct s_specialportal
{
    u8 levelid;
    u8 portallist[];
} s_specialportal;

typedef struct PortalCache {
    s32 count;
    bbox2d bbox;
} PortalCache;

typedef struct BgQueuedPortal {
    s32 arg0;          // 0x00
    s32 roomnum;       // 0x04
    s32 portalnum;     // 0x08
    s32 arg3;          // 0x0c
    f32 sp10[4];       // 0x10
} BgQueuedPortal;

extern PortalData *g_BgPortals;
extern struct PortalCache g_PortalCameraCache[PORTMAX];

extern s32 g_MaxNumRooms;
extern f32 g_LevelInverseScale;

extern BgRoomData * ptr_bgdata_room_fileposition_list;
extern RoomInfo g_BgRoomInfo[];
extern Gfx *ptrDynamic_CC_RM_LUT[];
extern Gfx DL_LUT_PRIMARY_ADDFOG[];


bool bgRoomsSharePortal(s32 roomA, s32 roomB);
s32 bgCopyVisibleRoomsToList(s32 *rooms, s32 max);
bool bgTestBulletHitBackground(coord3d *from, coord3d *to, s32 roomnum, struct HitThing *hit);
void bgFreeRoomData(s32 roomID);
void bgLoadFile(LEVEL_INDEX stagenum);
s8 bgSwapConnectedRooms(s32 index);
void bgToggleDataPortalsContrlBytes1Bit1(s32 index, s32 toggle);
void bgTick(void);
Gfx* bgSetupAndRender(Gfx *arg0);
Gfx *bgScissorCurrentPlayerView(Gfx *arg0, s32 left, s32 top, s32 width, s32 height);
Gfx* bgScissorCurrentPlayerViewDefault(Gfx* arg0);
Gfx* bgScissorCurrentPlayerViewF(Gfx* arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4);
f32 bgGetRoomScale(void);
u8 bgIsRoomRendered(s32 roomID);
s32 bgGet2dBboxByRoomId(s32 room_id, struct bbox2d *result);
f32 bgGetLevelVisibilityScale(void);
void bgRectOutersect(struct bbox2d *a, struct bbox2d *b);
f32 bgGetRoomInverseScale(void);
s32 bgGetPortalBetweenRooms(s32 arg0, s32 arg1, struct coord3d *arg2, struct coord3d *arg3);
void bgCalcPortalPlane(s32 portalnum, struct PortalMetric *metric);
void bgApplyDynamicCCRMLUT(Gfx *arg0, Gfx *arg1, enum CCRMLUT arg2);
void bgGetRoomsIntersectingBbox(coord3d *, coord3d *, s32 *, s32 *, s32);
void bgFindRoomsAlongSegment(coord3d *pos1, coord3d *pos2, u8 *initialRooms, u8 *outRoomSet, s32 *outRoomNums, s32 *outRoomNumsCount, s32 outRoomNumsMax);
s32 bgFindPortalCrossedByLine(coord3d *arg0, coord3d *arg1);
void bgRoomCalcBB(s32 room);

#endif
