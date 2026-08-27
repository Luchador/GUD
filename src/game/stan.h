#ifndef _STAN_H_
#define _STAN_H_
#include <ultra64.h>

#include <bondtypes.h>

// RGB? I've called them 'triple' because I don't really know what RGB is
// No parens around params
#define STAN_TRIPLE_TO_PNT_INDEX(tile, tripleIndex) (tile->hdrTail >> (8 - 4*tripleIndex) & 0xF)
#define STAN_POINT_COUNT(tile) (tile->tail.half & 0xF)

#define STAN_MID_SPECIAL(tile) (tile->mid.half & 0xF)
#define STAN_MID_R(tile) ((tile->mid.half >> 0x04) & 0xF)
#define STAN_MID_G(tile) ((tile->mid.half >> 0x08) & 0xF)
#define STAN_MID_B(tile) ((tile->mid.half >> 0x0C) & 0xF)

#define STAN_TAIL_POINT_COUNT(tile) (tile->tail.half & 0xF)/*canonically getsides()*/
#define STAN_TAIL_C(tile) ((tile->tail.half >> 0x04) & 0xF)
#define STAN_TAIL_D(tile) ((tile->tail.half >> 0x08) & 0xF)
#define STAN_TAIL_E(tile) ((tile->tail.half >> 0x0C) & 0xF)

struct move_bond_collision {
    struct coord3d bondCollision;
    struct coord3d sp190;
    struct coord3d sp19C;
    struct coord3d sp1A8;
};

typedef enum StanCollisionResult {
    STAN_COLLISION_NONE = -2,
    STAN_COLLISION_FOUND = 2,
    STAN_COLLISION_TRAVERSAL_LIMIT = 5
} StanCollisionResult;

typedef struct StanRoomBounds {
    union {
        struct {
            s16 minX;
            s16 minY;
            s16 minZ;
            s16 maxX;
            s16 maxY;
            s16 maxZ;
        };
        struct {
            s16 min[3];
            s16 max[3];
        };
    };
} StanRoomBounds;

extern f32 g_StanLastLineCollisionFraction;
extern PropRecord *g_StanLastCollisionProp;
extern struct StandTile *standTileStart;
extern StandTile *g_StanFirstTileByRoom[139];
extern StanRoomBounds g_StanRoomBounds[139];
extern s32 g_StanRoomIndexLimit;

void noteTileRoomIfDifferentToPrev( StandTile *tile,  StandTile *unused,  struct StandTileWalkCallbackRecord *data);
void setLevelScale(f32 ls);
void stanResetHits(void);
s32 walkTilesBetweenPoints_NoCallback(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z);
bool stanIsPointNearTile(StandTile *tile, f32 px, f32 pz);
f32 stanGetPositionYValue(StandTile* tile, f32 p_x, f32 p_z);
s32 stanGetLastCollisionEdge(coord3d *pntA, coord3d *pntB);
s32 stanTestLocusEdgeAboveY(StandTile **tile, f32 target_x, f32 target_z, f32 radius, f32 yThreshold);
s32 stanTestCircleCollisionNoCallbacks(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius);
s32 stanTestLineUnobstructed(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, int cdtypes, f32 unkHeight, f32 unkA, f32 unkB, f32 unkC);
StandTile* stanFillSearch(StandTile* srcTile, tilePredicate_t tilePred);
s32 stanGetRoomsBetweenPoints(StandTile *tile, f32 start_x, f32 start_z, StandTile **destTile, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 maxBufSize);
s32 stanWalkTilesBetweenPointsAndCollectRooms(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 *rtnCountSize, s32 maxBufSize);
StanCollisionResult stanTestVolume(StandTile **tileStack, f32 p_x, f32 p_z, f32 radius, CDTYPE cdtypes, f32 height, f32 width);
s32 getTileRoom(StandTile* tile);
PropRecord *sub_GAME_7F0B1410(StandTile *arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5);
s32 stanTileDistanceRelated(struct StandTile **arg0, f32 arg1, f32 arg2, f32 arg3, struct StandTileLocusCallbackRecord *arg4);
s32 stanGetLocusField0(struct StandTileLocusCallbackRecord *arg0);
s32 stanGetLocusCount(struct StandTileLocusCallbackRecord *arg0);
f32 distBetweenPoints2d(f32 o_x,f32 o_z,f32 p_x,f32 p_z);
bool stanPointProjectsOntoEdge(f32 x1, f32 z1, f32 x2, f32 z2, f32 x3, f32 z3);
f32 stanGetSignedPointLineDistance(f32 x1, f32 z1, f32 x2, f32 z2, f32 x3, f32 z3);
void stanGetMoveBondCollisionTiles(StandTile **tile1, StandTile **tile2, coord3d *coords);
StandTile *stanFindNearestWalkablePosition(f32 *x, f32 *y, f32 *z, f32 clearanceRadius);
bool doSegmentsIntersect(f32 start1X, f32 start1Z, f32 end1X, f32 end1Z, f32 start2X, f32 start2Z, f32 end2X, f32 end2Z);
struct StandTilePoint *stanMatchTileName(char *id);
s32 isPointInsideTriStandTileUnscaled_Maybe(struct StandTile *tile, f32 p_x, f32 p_z);
StanCollisionResult stanTestCircleAndCollectRooms(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius, s32 *rooms, s32 *count_rtn, s32 bufMax);
StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn);

#endif
