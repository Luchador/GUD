#include <ultra64.h>
#include <deb.h>
#include "stan.h"
#include "bg.h"
#include "chrai.h"
#include "chr.h"
#include "stanintersection.h"
#include "assert.h"


struct StanPrefixRecord {
    s32 stanfile;
    StandTile *ptr_firstroom;    // read as offset 4, hence the struct
};

struct StanPrefixRecord *stan_prefix;

/**
 * Each entry contains a pointer to the first stan tile belonging to that room. stanBuildRoomData() initializes entries to NULL and
 * when it encounters the first tile of a new room, it stores that tile's address in the corresponding array entry.
 * Stan files group tiles contiguously by room so code can begin at an entry and advance through until the tile->room changes.
 */
StandTile *g_StanFirstTileByRoom[139];

StanRoomBounds g_StanRoomBounds[139];

/**
 * Stores one more than the highest room ID represented in the loaded stan data.
 * Used as the exclusive upper bound when indexing rooms that have stans.
 */ 
s32 g_StanRoomIndexLimit;

/**
 * Tile containing the stan edge that most recently blocked a collision. Used with g_StanLastCollisionEdgeIndex.
 * This is NULL when the saved edge belongs to a prop rather than a stan.
 */
StandTile *g_StanLastCollisionTile;
s32 g_StanLastCollisionEdgeIndex;

/**
 * Cached world space X/Z end points of a collision edge. These are needed when
 * the colliding edge belongs to a prop and thus cannot be reconstructed from a stan tile and its edge.
 * They may also store the endpoints of a stan edge encountered during line traversal.
 */
struct coord2d g_StanLastCollisionEdgePointA;
struct coord2d g_StanLastCollisionEdgePointB;

/**
 * Position of the nearest collision along the most recently tested line segment, normalized between 0.0 - 1.0.
 * A value of 1.0 indicates an unobstructed line.
 */
f32 g_StanLastLineCollisionFraction;

/**
 * Prop responsible for the most recently detected collision.
 * Set to NULL when no prop collision detected.
 */
PropRecord * g_StanLastCollisionProp;

/**
 * Ladder tile detected by the most recent special tile locus search.
 * Set when the tested circle touches an edge linked to the ladder tile,
 * then consumed by stanGetMoveBondCollisionTiles.
 */
StandTile *g_StanDetectedLadderTile;

StandTile *bfsTileStack[352];

// Indexed by StandTile.mid.headerMid.special.
u8 g_StanTileSpecialFlags[] = {
    0x8D, 0x86, 0x04, 0xC5,
    0x9D, 0xA4, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

f32 level_scale = 1.0;
f32 inv_level_scale = 1.0;

/**
 * Total byte size for variable length StandTile records,
 * indexed by point count. Valid stans have between 3 and 10 points.
 */
u8 g_StanTileSizeByPointCount[] = {
    0x20,0x20,0x20,0x20,
    0x28,0x30,0x38,0x40,
    0x48,0x50,0x58,0x00
};

struct StandTile *standTileStart = NULL;
struct StandTile* stanTileEnd = NULL;

/**
 * Nonzero when g_StanLastCollisionEdgePointA and g_StanLastCollisionEdgePointB contain a valid explicitly cached edge.
 * Required for prop collisions because its edges can't be reconstructed from a stan tile and its edge index.
 */
s32 g_StanLastCollisionEdgePointsValid =  0;

// Begin forward declarations.

void stanGetTileMidPoint(StandTile *tile, coord3d *out);
s32 stanIsSpecialBit1Set(StandTile *arg0, struct StandTileLocusCallbackRecord* arg1);
s32 stanCheckLinkedSpecialTile(StandTile *tile, s32 pointIdx, s32 arg2, s32 arg3, s32 arg4, s32 *outFlags);
f32 getShortest2dDispToInfTripleEdge(StandTile *tile, s32 start3index, f32 p_x, f32 p_z);
s32 stanLocusAddTileRoomIfNew(StandTile *tile, struct StandTileLocusCallbackRecord *rec);
s32 stanGetLocusField0(struct StandTileLocusCallbackRecord *arg0);
s32 stanGetLocusCount(struct StandTileLocusCallbackRecord *arg0);
bool stanLocusEdgeIsAboveY(StandTile *tile, s32 edgeIndex, f32 edgeDist, f32 distToPointA, f32 distToPointB, f32 *yThreshold);

// End forward declarations.


void stanBuildRoomData(void)
{
    StandTile *tile;
    u8 lastRoom;
    s32 i;
    s32 j;
    s32 k;

    lastRoom = 0xff;
    g_StanRoomIndexLimit = 0;

    for (k = 0; k < 139; k++) 
    {
        g_StanFirstTileByRoom[k] = NULL;
    }

    tile = stan_prefix->ptr_firstroom;

    while (*(u32 *)tile)
    {
        if (tile->room != lastRoom)
        {
            lastRoom = tile->room;

            if (g_StanRoomIndexLimit <= lastRoom)
            {
                g_StanRoomIndexLimit = lastRoom + 1;
            }

            g_StanFirstTileByRoom[lastRoom] = tile;

            g_StanRoomBounds[lastRoom].min[0] = g_StanRoomBounds[lastRoom].min[1] = g_StanRoomBounds[lastRoom].min[2] = 0x7fff;
            g_StanRoomBounds[lastRoom].max[0] = g_StanRoomBounds[lastRoom].max[1] = g_StanRoomBounds[lastRoom].max[2] = -0x8000;
        }

        for (i = 0; i < (tile->tail.hdrTail.pointCount & 0xf); i++)
        {
            for (j = 0; j < 3; j++)
            {
                if (tile->points[i].AsArray[j] < g_StanRoomBounds[lastRoom].min[j])
                {
                    g_StanRoomBounds[lastRoom].min[j] = tile->points[i].AsArray[j];
                }

                if (tile->points[i].AsArray[j] > g_StanRoomBounds[lastRoom].max[j])
                {
                    g_StanRoomBounds[lastRoom].max[j] = tile->points[i].AsArray[j];
                }
            }
        }

        tile = (StandTile *)(((u8 *)tile) + g_StanTileSizeByPointCount[tile->tail.hdrTail.pointCount & 0xf]);
    }
}


/**
 * Finds the highest stan file beneath pos. If a tile is found, yRtn is set to the tile
 * height beneath pos.
 * @returns NULL if no suitable tile is found.
 */
StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn)
{
    StandTile *tile;
    StandTile *walkTile;
    StandTile *bestTile;
    StanRoomBounds *roomBounds;
    coord3d midPoint;
    f32 scaledX;
    f32 scaledY;
    f32 scaledZ;
    f32 bestY;
    f32 edgeDist;
    f32 tileY;
    s16 queryX;
    s16 queryY;
    s16 queryZ;
    s32 tailhalf;
    s32 room;
    s32 i;
    bool roomAllowed;
    bool tileContainsPoint;
    bool nearEdge;

    bestY = -3.4028235e38f;
    bestTile = NULL;

    scaledX = pos->x * level_scale;
    scaledY = pos->y * level_scale;
    scaledZ = pos->z * level_scale;

    if (scaledY > 32767.0f)
    {
        scaledY = 32767.0f;
    }
    else if (scaledY < -32767.0f)
    {
        scaledY = -32767.0f;
    }

    queryX = scaledX;
    queryY = scaledY;
    queryZ = scaledZ;

    for (room = 0; room < g_StanRoomIndexLimit; room++)
    {
        tile = g_StanFirstTileByRoom[room];

        if (tile == NULL)
        {
            continue;
        }

        roomBounds = &g_StanRoomBounds[room];

        /* Reject the room before walking any of its variable-length tiles. */
        if (queryX < roomBounds->minX
                || queryX > roomBounds->maxX
                || queryZ < roomBounds->minZ
                || queryZ > roomBounds->maxZ
                || queryY < roomBounds->minY)
        {
            continue;
        }

        roomAllowed = rooms == NULL;

        if (!roomAllowed)
        {
            /* Room lists contain at most four entries and may end with 0xff. */
            for (i = 0; i < 4 && rooms[i] != 0xff; i++)
            {
                if (room == rooms[i])
                {
                    roomAllowed = TRUE;
                    break;
                }
            }
        }

        if (!roomAllowed)
        {
            continue;
        }

        while (*((u32 *) tile) != 0 && tile->room == room)
        {
            tileContainsPoint = TRUE;
            nearEdge = FALSE;

            /* The first three points form the representative triangle. */
            for (i = 0; i < 3; i++)
            {
                edgeDist = getShortest2dDispToInfTripleEdge(tile, i, scaledX, scaledZ);

                if (edgeDist < -2.0f)
                {
                    tileContainsPoint = FALSE;
                    break;
                }

                if (edgeDist < 2.0f)
                {
                    nearEdge = TRUE;
                }
            }

            if (tileContainsPoint && !stanTileHasZeroArea(tile))
            {
                /*
                 * Close to an edge, the representative triangle is ambiguous.
                 * Walk from the tile midpoint to confirm pos belongs to this tile.
                 */
                if (nearEdge)
                {
                    stanGetTileMidPoint(tile, &midPoint);
                    walkTile = tile;

                    if (!walkTilesBetweenPoints_NoCallback(&walkTile, midPoint.x, midPoint.z, pos->x, pos->z) || walkTile != tile)
                    {
                        tileContainsPoint = FALSE;
                    }
                }

                if (tileContainsPoint)
                {
                    tileY = stanGetPositionYValue(tile, pos->x, pos->z);

                    if (tileY <= pos->y && tileY > bestY)
                    {
                        bestTile = tile;
                        bestY = tileY;
                    }
                }
            }

            tailhalf = tile->tail.half;
            tile = (StandTile *) (((u8 *) tile) + g_StanTileSizeByPointCount[(tailhalf >> 12) & 0xf]);
        }
    }

    if ((bestTile != NULL) && (yRtn != NULL))
    {
        *yRtn = bestY;
    }

    return bestTile;
}


void stanLoadFile(struct StanPrefixRecord *file)
{
    struct StanPrefixRecord *prefix = &stan_prefix;

    prefix->stanfile = file;

    standTileStart = (StandTile *)(((u8 *)file->ptr_firstroom) - 0x80);

    stanBuildRoomData();
    setLevelScale(1.0f);
}


/**
 * Returns true if x/z coords from the three point indices out of tile->tail.half are colinear i.e. the triangle has zero horizontal area.
 */
bool stanTileHasZeroArea(StandTile *tile)
{
    s32 AB[3];
    s32 AC[3];
    u32 crossStore[2];
    s32 temp1, temp2, temp3;
    

    temp1 = (tile->tail.half >> 8) & 0xf;
    temp2 = (tile->tail.half >> 4) & 0xf;
    temp3 = (tile->tail.half) & 0xf;

    AB[0] = tile->points[temp2].x - tile->points[temp1].x;
    AB[2] = tile->points[temp2].z - tile->points[temp1].z;
    
    AC[0] = tile->points[temp3].x - tile->points[temp1].x;
    AC[2] = tile->points[temp3].z - tile->points[temp1].z;

    crossStore[0] = (AB[2] * AC[0]) - (AB[0] * AC[2]);

    return crossStore[0] == 0;
}


void stanGetTileMidPoint(StandTile *tile, coord3d *out)
{
    u16 tail;
    u8 indexA;
    u32 indexB;
    u32 indexC;
    StandTilePoint *pointA;
    StandTilePoint *pointB;
    unsigned int new_var2;
    StandTilePoint *pointC;
    s16 *new_var3;

    tail = (indexC = tile->tail.half);
    new_var2 = (tail & 0xFFFF) >> 4;
    indexA = (tail >> 8) & 0xf;
    indexB = new_var2 & 0xf;
    indexC = indexC & 0xf;
    new_var3 = &(&tile->points[indexC])->x;
    pointA = &tile->points[indexA];
    pointB = &tile->points[indexB];
    out->x = (((((f32) pointA->x) + ((f32) pointB->x)) + ((f32) (*new_var3))) / 3.0f) * inv_level_scale;
    out->y = (((((f32) (&tile->points[indexA])->y) + ((f32) pointB->y)) + ((f32) (&tile->points[indexC])->y)) / 3.0f) * inv_level_scale;
    out->z = (((((f32) (&tile->points[indexA])->z) + ((f32) pointB->z)) + ((f32) ((float) (&tile->points[indexC])->z))) / 3.0f) * inv_level_scale;
}


void getPointJustInsideOfTileTriple(StandTile *tile, s32 tripleIndex /*canonically c */, coord3d *out)
{
    coord3d midPoint;
    s32 pntIndex;

    #ifdef DEBUG
    assert(c<3);
    #endif

    pntIndex = (tile->tail.half >> (8 - (tripleIndex * 4))) & 0xf;
    
    if (1);
    if (&midPoint);
    
    out->x = ((f32) tile->points[pntIndex].x) * inv_level_scale;
    out->y = ((f32) tile->points[pntIndex].y) * inv_level_scale;
    out->z = ((f32) tile->points[pntIndex].z) * inv_level_scale;
    
    stanGetTileMidPoint(tile, &midPoint);
    
    // 10% of the way from the actual tile point towards the tile's centre.
    out->x = (midPoint.x * 0.1f) + (0.9f * out->x);
    out->y = (midPoint.y * 0.1f) + (0.9f * out->y);
    out->z = (midPoint.z * 0.1f) + (0.9f * out->z);
}


f32 distanceSquaredBetweenPoints3d(coord3d *a,coord3d *b)
{
    f32 dx;
    f32 dy;
    f32 dz;

    dx = b->x - a->x;
    dy = b->y - a->y;
    dz = b->z - a->z;

    return dx * dx + dy * dy + dz * dz;
}


StandTile *stanFindNearestWalkablePosition(f32 *x, f32 *y, f32 *z, f32 clearanceRadius)
{
    StandTile *tile;
    s32 tileTail;
    StandTile *stack[1];
    StandTile *bestTile;
    s32 i;
    s32 midpointIndex;
    coord3d original;
    coord3d candidate;
    f32 bestDist;
    f32 dist;

    bestTile = NULL;
    original.x = *x;
    original.y = *y;
    original.z = *z;
    midpointIndex = 3;
    bestDist = M_U32_MAX_VALUE_F;

    tile = stan_prefix->ptr_firstroom;

    if (*((u32 *) tile))
    {
        do
        {
            if (((((u16 *) tile)[0] >> 15) & 1) != 1)
            {
                if (stanTileHasZeroArea(tile) == FALSE)
                {
                    for (i = 0; i != 4; i++)
                    {
                        if (i == midpointIndex)
                        {
                            stanGetTileMidPoint(tile, &candidate);
                        }
                        else
                        {
                            getPointJustInsideOfTileTriple(tile, i, &candidate);
                        }

                        stack[0] = tile;

                        if (stanTestCircleCollisionNoCallbacks(stack, candidate.x, candidate.z, clearanceRadius) < 0)
                        {
                            dist = distanceSquaredBetweenPoints3d(&candidate, &original);

                            if (dist < bestDist)
                            {
                                bestTile = tile;
                                bestDist = dist;
                                *x = candidate.x;
                                *y = candidate.y;
                                *z = candidate.z;
                            }
                        }
                    }
                }
            }

            tileTail = tile->tail.half;
            tile = (StandTile *) (((u8 *) tile) + g_StanTileSizeByPointCount[(tileTail >> 12) & 0xf]);
            
        } while (*((u32 *) tile));
    }

    return bestTile;
}


// Returns the shortest distance from (p_x,p_z) to the infinite extention of tile's index-th edge, projected into XZ.
// Where the edge is vertical (or degenerate) they just return the distance between the points.
// cannonically tile is sf and index is ei
f32 getShortest2dDispToInfTileEdge(StandTile *tile,s32 index,f32 p_x,f32 p_z)
{
    s32 nextIndex;
    f32 edge_x;
    f32 edge_z;
    f32 edge_len; //canonically d

    f32 v_x;
    f32 v_z;
    f32 crossProduct;

    // 3 unused. We use 2 for the points to make our code cleaner,
    //   though it seems much more likely that the variables were used in the else clause.
    struct StandTilePoint* currPnt;
    struct StandTilePoint* nextPnt;
    f32 UNUSED;

    #ifdef DEBUG
    assert(ei<getsides(sf));
    #endif

    // Omiting the '& 0xF' is equivalent, but keeping it is necessary to match.
    // Perhaps the structure isn't correct but this seems much cleaner than doing an explicit >> 0xC.
    nextIndex = (index + 1) % STAN_TAIL_E(tile);

    nextPnt = &tile->points[nextIndex];
    currPnt = &tile->points[index];
    edge_x = (f32)(nextPnt->x - currPnt->x);
    edge_z = (f32)(nextPnt->z - currPnt->z);

    edge_len = sqrtf(edge_x * edge_x + edge_z * edge_z);

    if (edge_len == 0) {
        // Degenerate case, edge is vertical
        // They just return the distance between the points, which is sensible and the correct value in 3 dimensions.
        v_x = p_x - (f32)tile->points[nextIndex].x;
        v_z = p_z - (f32)tile->points[nextIndex].z;
        return sqrtf(v_x * v_x + v_z * v_z);
    }
    else
    {
        #ifdef DEBUG
        assert(d>0.0f);
        #endif

        // | (AP x AB) / ||AB|| | = ||PA|| sin(a),
        // so we're returning the SIGNED displacement
        crossProduct = (
            edge_z * (p_x - (f32)tile->points[index].x)
            +
            -edge_x * (p_z - (f32)tile->points[index].z)
        );
        return crossProduct / edge_len;
    }

}


f32 getShortest2dDispToInfTripleEdge(StandTile *tile, s32 start3index, f32 p_x, f32 p_z)
{
    f32 dx;
    f32 edgeX;
    f32 edgeZ;
    f32 edgeLen;
    f32 dz;
    f32 crossProduct;
    s32 end3index;
    s32 currPntI;
    s32 nextPntI;
    s32 tail;

    #ifdef DEBUG
    assert(ei<getsides(sf));
    #endif

    nextPntI = 2;

    if (start3index != nextPntI) {
        end3index = start3index + 1;
    } else {
        end3index = 0;
    }

    start3index = (tile->tail.half >> (8 - (start3index << nextPntI))) & 0xf;
    end3index = (tile->tail.half >> (8 - (end3index << nextPntI))) & 0xf;

    edgeX = tile->points[end3index].x - tile->points[start3index].x;
    edgeZ = tile->points[end3index].z - tile->points[start3index].z;
    edgeLen = sqrtf((edgeX * edgeX) + (edgeZ * edgeZ));

    if (edgeLen == 0.0f) {
        dx = p_x - tile->points[end3index].x;
        dz = p_z - tile->points[end3index].z;
        return sqrtf((dx * dx) + (dz * dz));
    }

    #ifdef DEBUG
    assert(d>0.0f);
    #endif

    crossProduct = (edgeZ * (p_x - tile->points[start3index].x)) + (-edgeX * (p_z - tile->points[start3index].z));
    return crossProduct / edgeLen;
}


f32 getShortest2dDispToInfTripleEdgeUnscaled(StandTile *tile,s32 start3index,f32 p_x,f32 p_z)
{
  f32 disp;

  disp = getShortest2dDispToInfTripleEdge(tile, start3index, p_x * level_scale, p_z * level_scale);
  return disp * inv_level_scale;
}


f32 distToTilePnt2D(StandTile *tile,int pntI,f32 p_x,f32 p_z)
{
  f32 len;

  p_x -= (f32)tile->points[pntI].x;
  p_z -= (f32)tile->points[pntI].z;
  return sqrtf(p_x * p_x + p_z * p_z);
}


/**
 * Address: 7F0B0198
 * 
 * Returns true if the perpendicular projection of the X/Z point onto the
 * tile edge's infinite line falls between the edge endpoints.
 *
 * Example:
 *
 *     A -------- B
 *          |
 *          |
 *          P
 *
 * P is not on the edge, but its projection lands between A and B.
 */
bool stanPointProjectsOntoTileEdge(StandTile *tile, s32 edgeIndex, f32 p_x, f32 p_z)
{
    StandTilePoint *point;
    f32 edgeXCopy;
    f32 startX;
    f32 startZ;
    f32 edgeX;
    f32 edgeZ;
    StandTilePoint *nextPoint;

    point = &tile->points[edgeIndex];

    startX = point->x;
    startZ = point->z;

    edgeIndex = (edgeIndex + 1) % ((tile->tail.half >> 12) & 0xf);

    point = (nextPoint = &tile->points[edgeIndex]);

    edgeX = point->x;
    edgeX = edgeX - startX;

    edgeZ = point->z;
    edgeZ = edgeZ - startZ;

    p_x -= startX;
    p_z -= startZ;

    edgeXCopy = edgeX;

    startZ = (edgeXCopy * edgeXCopy) + (edgeZ * edgeZ);
    startX = (p_x * edgeXCopy) + (p_z * edgeZ);

    edgeZ = startX;

    return ((startZ < edgeZ) && (edgeZ < 0.0f))
        || ((0.0f < edgeZ) && (edgeZ < startZ));
}


s32 isPointInsideTriStandTileUnscaled_Maybe(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 disp;
    s32 i;

    for (i = 0; i != 3; i++)
    {
        disp = getShortest2dDispToInfTripleEdgeUnscaled(tile,i,p_x,p_z);
        if (disp < 0) {
            return 0;
        }
    }

    return 1;
}


/**
 * Returns the signed perpendicular distance from point px, pz to one edge of the tile's triangle.
 * Positve return value = inside the tile.
 */
f32 stanGetSignedDistToTileEdge(StandTile *tile, s32 edgenum, f32 px, f32 pz)
{
    f32 edgeX;
    f32 edgeZ;
    f32 edgeLen;
    s32 nextnum;
    s32 corner;
    s32 nextcorner;
    f32 cross;

    nextnum = (edgenum != 2) ? edgenum + 1 : 0;

    corner     = (tile->tail.half >> (8 - (edgenum << 2))) & 0xF;
    nextcorner = (tile->tail.half >> (8 - (nextnum << 2))) & 0xF;

    edgeX = (f32)(tile->points[nextcorner].x - tile->points[corner].x);
    edgeZ = (f32)(tile->points[nextcorner].z - tile->points[corner].z);

    edgeLen = sqrtf((edgeX * edgeX) + (edgeZ * edgeZ));

    if (edgeLen == 0.0f)
    {
        return 0.0f;
    }

    cross = (edgeZ * (px - tile->points[corner].x)) + ((pz - tile->points[corner].z) * -edgeX);
    return cross / edgeLen;
}


/**
 * Tests if point is inside or very close to being inside a tile. It gives a margin of 2 units
 * for a point to still be considered acceptable.
 */
bool stanIsPointNearTile(StandTile *tile, f32 px, f32 pz)
{
    f32 margin;
    s32 i;

    px *= level_scale;
    pz *= level_scale;

    for (i = 0; i != 3; i++)
    {
        margin = stanGetSignedDistToTileEdge(tile, i, px, pz);

        if (margin < -2.0f)
        {
            return FALSE;
        }
    }

    return TRUE;
}


// A->B ACWS returns 1, CWS (including opposite) returns -1.
// Identical direction and |A| >= |B| returns 0
int getRotationalDirectionBetween(f32 a_x,f32 a_z,f32 b_x,f32 b_z)
{
    // The main 2 cases : return the sign of AxB where it's non-zero
    if (a_z * b_x < a_x * b_z) {
        return 1;
    }
    if (a_x * b_z < a_z * b_x) {
        return -1;
    }

    // [AxB == 0 now]

    // If the vectors are opposite, default to clockwise
    if ((a_x * b_x < 0) || (a_z * b_z < 0)) {
      return -1;
    }

    // If A is shorter, return anti-clockwise
    if (a_x * a_x + a_z * a_z < b_x * b_x + b_z * b_z) {
        return 1;   // ACWS
    }

    // Identical direction, |A| >= |B|
    return 0;
}


/**
 * Address: 7F0B0688
 * 
 * Test if two 2D line segments intersect.
 * 
 * Segment 1 runs from start1 -> end1. Segment 2 runs from start2 -> end2.
 * 
 * This function acts as a gate for calculateSegmentIntersectionFraction,
 * so callers can decide if it's worth computing where along segment 1 the crossing lands.
 */
bool doSegmentsIntersect(f32 start1X, f32 start1Z, f32 end1X, f32 end1Z, f32 start2X, f32 start2Z, f32 end2X, f32 end2Z)
{
    s32 unused1;
    s32 unused2;
    f32 start1RelX;
    f32 start1RelZ;
    f32 seg1Dx;
    f32 seg1Dz;
 
    start1RelX = start1X - start2X;
    start1RelZ = start1Z - start2Z;
    seg1Dx = end1X - start1X;
    seg1Dz = end1Z - start1Z;
 
    return
        (
            (getRotationalDirectionBetween(seg1Dx, seg1Dz, -start1RelX, -start1RelZ)
            * getRotationalDirectionBetween(seg1Dx, seg1Dz, end2X - start1X, end2Z - start1Z)) < 1)
        &&
        (
            (getRotationalDirectionBetween(end2X - start2X, end2Z - start2Z, start1RelX, start1RelZ)
            * getRotationalDirectionBetween(end2X - start2X, end2Z - start2Z, end1X - start2X, end1Z - start2Z)) < 1)
        ;
}


/**
 * Tests whether two X/Z line segments cross.
 *
 * When includeTouching is false, the segments must cross strictly. When it is
 * true, a zero-valued side test is accepted so touching an endpoint or a
 * collinear segment can also count as an intersection.
 */
bool doSegmentsIntersectWithTolerance(f32 start1X, f32 start1Z, f32 end1X, f32 end1Z, f32 start2X, f32 start2Z, f32 end2X, f32 end2Z, s32 includeTouching)
{
    f32 segment1Dx;
    f32 segment1Dz;
    f32 segment2Dx;
    f32 segment2Dz;
    f32 start1FromStart2X;
    f32 start1FromStart2Z;
    s32 start2SideOfSegment1;
    s32 end2SideOfSegment1;
    s32 segment2EndpointSideProduct;
    s32 start1SideOfSegment2;
    s32 end1SideOfSegment2;
    s32 segment1EndpointSideProduct;
    bool intersects;

    segment1Dx = end1X - start1X;
    segment1Dz = end1Z - start1Z;
    segment2Dx = end2X - start2X;
    segment2Dz = end2Z - start2Z;
    start1FromStart2X = start1X - start2X;
    start1FromStart2Z = start1Z - start2Z;

    start2SideOfSegment1 = getRotationalDirectionBetween(segment1Dx, segment1Dz, -start1FromStart2X, -start1FromStart2Z);

    end2SideOfSegment1 = getRotationalDirectionBetween(segment1Dx, segment1Dz, end2X - start1X, end2Z - start1Z);

    segment2EndpointSideProduct = start2SideOfSegment1 * end2SideOfSegment1;

    start1SideOfSegment2 = getRotationalDirectionBetween(segment2Dx, segment2Dz, start1FromStart2X, start1FromStart2Z);

    end1SideOfSegment2 = getRotationalDirectionBetween(segment2Dx, segment2Dz, end1X - start2X, end1Z - start2Z);

    segment1EndpointSideProduct = start1SideOfSegment2 * end1SideOfSegment2;

    intersects = TRUE;

    if (segment2EndpointSideProduct >= includeTouching)
    {
        intersects = FALSE;
    }

    if (segment1EndpointSideProduct >= includeTouching)
    {
        intersects = FALSE;
    }

    return intersects;
}


bool stanWalkTilesBetweenPointsWithCallback(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z, standTileWalkCallback_t callback, struct StandTileWalkCallbackRecord *callbackData)
{
    StandTile *tile;
    StandTile *previousTile;
    StandTile *previousPreviousTile;
    StandTile *linkedTile;
    StandTile *nextTile;
    f32 lineNegDz;
    f32 lineDx;
    s32 uninitialized;
    s32 edgeIndex;
    s32 crossings;
    s32 iterationCount;
    s32 savedPointIndex;
    StandTilePoint *nextPoint;
    StandTilePoint *curPoint;
    s32 nextPointIndex;
    s32 hasLink;

    start_x *= level_scale;
    start_z *= level_scale;
    dest_x *= level_scale;
    dest_z *= level_scale;

    tile = *tileStack;
    previousTile = *tileStack;
    previousPreviousTile = *tileStack;
    lineNegDz = -(dest_z - start_z);
    crossings = 0;
    nextTile = NULL;
    iterationCount = 0;
    lineDx = dest_x - start_x;

    savedPointIndex = uninitialized;

    while (1)
    {
        crossings = 0;

        if (callback)
        {
            callback(tile, previousTile, callbackData);
        }

        curPoint = (StandTilePoint *) tile;

        for (edgeIndex = 0; edgeIndex < (tile->tail.hdrTail.pointCount & 0xF); edgeIndex++, curPoint++)
        {
            nextPointIndex = (edgeIndex + 1) % (tile->tail.hdrTail.pointCount & 0xF);
            nextPoint = &((StandTilePoint *) tile)[nextPointIndex];

            if (((lineNegDz * (nextPoint[1].x - curPoint[1].x)) + (lineDx * (nextPoint[1].z - curPoint[1].z))) <= 0.0f)
            {
                hasLink = curPoint[1].link >> 4 != 0;

                if (doSegmentsIntersectWithTolerance(start_x, start_z, dest_x, dest_z, curPoint[1].x, curPoint[1].z, nextPoint[1].x, nextPoint[1].z, hasLink))
                {
                    linkedTile = &standTileStart[curPoint[1].link];
                    crossings++;

                    if (previousTile != linkedTile && previousPreviousTile != linkedTile)
                    {
                        savedPointIndex = edgeIndex;
                        nextTile = curPoint[1].link >> 4 != 0 ? linkedTile : NULL;
                    }
                }
            }
        }

        previousPreviousTile = previousTile;
        previousTile = tile;

        if (tile == nextTile)
        {
            crossings = 0;
        }

        tile = nextTile;

        if (crossings == 0)
        {
            return TRUE;
        }

        if (iterationCount++ > 500 || nextTile == NULL || crossings == 0)
        {
            g_StanLastCollisionTile = previousTile;
            g_StanLastCollisionEdgeIndex = savedPointIndex;
            return FALSE;
        }

        *tileStack = nextTile;
    }
}


s32 walkTilesBetweenPoints_NoCallback(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z)
{
    return stanWalkTilesBetweenPointsWithCallback(tileStack, start_x, start_z, dest_x, dest_z, NULL, NULL);
}


s32 stanWalkTilesBetweenPointsAndCollectRooms(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 *rtnCountSize, s32 maxBufSize)
{
    struct StandTileWalkCallbackRecord callbackData;
    s32 rtn;

    callbackData.roomBuf = roomBuffer;
    callbackData.count = 0;
    callbackData.bufMax = maxBufSize;
    callbackData.lastRoom = -1;

    rtn = stanWalkTilesBetweenPointsWithCallback(tileStack, start_x, start_z, dest_x, dest_z, noteTileRoomIfDifferentToPrev, &callbackData);

    *rtnCountSize = callbackData.count;
    return rtn;
}


void noteTileRoomIfDifferentToPrev(StandTile *tile, StandTile *unused, struct StandTileWalkCallbackRecord *data)
{
    s32 newRoom;

    if (tile->room != data->lastRoom && data->count < data->bufMax)
    {
        newRoom = (s32)tile->room;
        *data->roomBuf = newRoom;
        data->lastRoom = newRoom;
        data->roomBuf += 1;
        data->count += 1;
    }

    return;
}


/**
 * Builds a list of room IDs between a start position and a destination position.
 * Only used for objects on set paths e.g. patrolling guards.
 */
s32 stanGetRoomsBetweenPoints(StandTile *tile, f32 start_x, f32 start_z, StandTile **destTile, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 maxBufSize)
{
    StandTile *savedTile;
    s32 count;

    savedTile = tile;
    count = 0;

    if (*destTile != NULL) {
        u8 roomA; // Source tile's room
        u8 roomB; // Destination tile's room

        roomA = tile->room;
        roomB = (*destTile)->room;

        // Fast path: start and destination tiles are in the same room, return a count of 1.
        if (roomB == roomA) {  
            roomBuffer[0] = roomA;
            return 1;
        }

        // Next fastest case: both rooms directly connected by a portal, write two room IDs and return a count of 2.
        if (bgRoomsSharePortal(roomA & 0xff, roomB & 0xff)) { 
            roomBuffer[0] = tile->room;
            roomBuffer[1] = (*destTile)->room;
            return 2;
        }
    }

    /**
     * Full path check needed. Find the rooms between the points, store them in roomBuffer, and save the number of rooms in count.
     * If the path check fails, return 0.
     */
    if (!stanWalkTilesBetweenPointsAndCollectRooms(&savedTile, start_x, start_z, dest_x, dest_z, roomBuffer, &count, maxBufSize))
    {
        return 0;
    }

    if (maxBufSize < count)
    {
        count = maxBufSize;
    }

    if (*destTile == NULL)
    {
        *destTile = savedTile;
    }

    if (savedTile != *destTile)
    {
        return 0;
    }

    return count;
}


s32 stanTestLineUnobstructed(StandTile **tile, f32 startX, f32 startZ, f32 endX, f32 endZ, s32 collisionTypes, f32 startTop, f32 startBottom, f32 endTop, f32 endBottom)
{
    PropRecord *prop;
    StandTile *reachedTile;
    coord2d lineStart;
    coord2d lineEnd;
    f32 nearestCollisionFraction;
    f32 topSlope;
    f32 bottomSlope;
    f32 startFloorY;
    f32 floorSlope;
    f32 floorEndX;
    f32 floorEndZ;
    f32 intersectionFraction;
    f32 propTop;
    f32 propBottom;
    s32 unobstructed;
    s32 nextEdgePointIndex;
    s32 roomCount;
    s32 roomBuffer[21];
    s32 checkVerticalOverlap;
    s32 verticalExtentsPrepared;
    s32 edgeIndex;
    s32 edgeCount;
    s32 blocksLine;
    s16 *propIndex;
    coord2d *edgeStart;
    coord2d *edgeEnd;
    struct rect4f *polygon;

    nearestCollisionFraction = 1.0f;
    roomCount = 0;
    checkVerticalOverlap = startBottom <= startTop;
    verticalExtentsPrepared = 0;
    reachedTile = *tile;

    lineStart.f[0] = startX;
    lineStart.f[1] = startZ;
    lineEnd.f[0] = endX;
    lineEnd.f[1] = endZ;

    /* The extra room slot is reserved for roomGetProps's -1 terminator. */
    unobstructed = stanWalkTilesBetweenPointsAndCollectRooms(&reachedTile, startX, startZ, endX, endZ, roomBuffer, &roomCount, 20);

    if (!unobstructed)
    {
        nextEdgePointIndex = g_StanLastCollisionEdgeIndex + 1;

        if (nextEdgePointIndex == ((g_StanLastCollisionTile->tail.half >> 12) & 0xf))
        {
            nextEdgePointIndex = 0;
        }

        g_StanLastCollisionEdgePointsValid = 1;
        g_StanLastCollisionEdgePointA.f[0] = (f32)g_StanLastCollisionTile->points[g_StanLastCollisionEdgeIndex].x * inv_level_scale;
        g_StanLastCollisionEdgePointA.f[1] = (f32)g_StanLastCollisionTile->points[g_StanLastCollisionEdgeIndex].z * inv_level_scale;
        g_StanLastCollisionEdgePointB.f[0] = (f32)g_StanLastCollisionTile->points[nextEdgePointIndex].x * inv_level_scale;
        g_StanLastCollisionEdgePointB.f[1] = (f32)g_StanLastCollisionTile->points[nextEdgePointIndex].z * inv_level_scale;

        nearestCollisionFraction = calculateSegmentIntersectionFraction(&lineStart, &lineEnd, &g_StanLastCollisionEdgePointA, &g_StanLastCollisionEdgePointB);
    }

    g_StanLastCollisionProp = NULL;

    if (collisionTypes != 0)
    {
        roomBuffer[roomCount] = -1;
        roomGetProps(roomBuffer);

        for (propIndex = ptr_list_object_lookup_indices; *propIndex >= 0; propIndex++)
        {
            prop = &g_Props[*propIndex];

            if (!propIsOfCdType(prop, collisionTypes))
            {
                continue;
            }

            chraiGetCollisionBounds(prop, &polygon, &edgeCount, &propTop, &propBottom);
            edgeStart = &polygon->points[0];
            edgeEnd = &polygon->points[1];

            for (edgeIndex = 0; edgeIndex < edgeCount; edgeIndex++, edgeStart++, edgeEnd++)
            {
                if (edgeIndex + 1 == edgeCount)
                {
                    edgeEnd = &polygon->points[0];
                }

                if (!doSegmentsIntersect(startX, startZ, endX, endZ,
                        edgeStart->f[0], edgeStart->f[1], edgeEnd->f[0], edgeEnd->f[1]))
                {
                    continue;
                }

                intersectionFraction = calculateSegmentIntersectionFraction(&lineStart, &lineEnd,
                        edgeStart, edgeEnd);

                if (intersectionFraction >= nearestCollisionFraction)
                {
                    continue;
                }

                blocksLine = 1;

                if (checkVerticalOverlap)
                {
                    if (!verticalExtentsPrepared)
                    {
                        verticalExtentsPrepared = 1;

                        if (endBottom <= endTop)
                        {
                            topSlope = endTop - startTop;
                            bottomSlope = endBottom - startBottom;
                        }
                        else
                        {
                            floorEndX = endX;
                            floorEndZ = endZ;

                            if (nearestCollisionFraction < 1.0f)
                            {
                                floorEndX = startX + (endX - startX) * nearestCollisionFraction;
                                floorEndZ = startZ + (endZ - startZ) * nearestCollisionFraction;
                            }

                            startFloorY = stanGetPositionYValue(*tile, startX, startZ);
                            startTop += startFloorY;
                            startBottom += startFloorY;
                            floorSlope = (stanGetPositionYValue(reachedTile, floorEndX, floorEndZ)
                                    - startFloorY) / nearestCollisionFraction;
                            topSlope = floorSlope;
                            bottomSlope = floorSlope;
                        }
                    }

                    if (propTop <= startBottom + bottomSlope * intersectionFraction
                            || startTop + topSlope * intersectionFraction <= propBottom)
                    {
                        blocksLine = 0;
                    }
                }

                if (blocksLine)
                {
                    unobstructed = 0;
                    nearestCollisionFraction = intersectionFraction;
                    g_StanLastCollisionEdgePointsValid = 1;
                    g_StanLastCollisionEdgePointA = *edgeStart;
                    g_StanLastCollisionEdgePointB = *edgeEnd;
                    g_StanLastCollisionTile = NULL;
                    g_StanLastCollisionEdgeIndex = 0;
                    g_StanLastCollisionProp = prop;

                    /* Recover the STAN tile at the prop collision point below. */
                    reachedTile = NULL;
                }
            }
        }
    }

    if (reachedTile == NULL)
    {
        reachedTile = *tile;
        endX = startX + (endX - startX) * nearestCollisionFraction;
        endZ = startZ + (endZ - startZ) * nearestCollisionFraction;
        walkTilesBetweenPoints_NoCallback(&reachedTile, startX, startZ, endX, endZ);
    }

    *tile = reachedTile;
    g_StanLastLineCollisionFraction = nearestCollisionFraction;

    return unobstructed;
}


PropRecord *sub_GAME_7F0B1410(StandTile *t, f32 start_x, f32 start_z, f32 end_x, f32 end_z, s32 cdtypes)
{
    f32 frac;
    PropRecord *prop;
    coord2d lineStart;
    coord2d lineEnd;
    struct coord2d *tmp;
    s32 pad;
    coord2d edgeStart;
    coord2d edgeEnd;
    s32 i;
    StandTile *tile;
    s32 roomCount;
    s32 roomBuffer[21];
    s16 *propIndexPtr;
    struct rect4f *polygon;
    s32 numEdges;
    PropRecord *bestProp;
    s32 next;
    f32 bestFrac;

    bestProp = NULL;
    bestFrac = 1.0f;

    tile = t;
    roomCount = 0;

    stanWalkTilesBetweenPointsAndCollectRooms(&tile, start_x, start_z, end_x, end_z, roomBuffer, &roomCount, 20);

    if (roomCount >= 21)
    {
        // The comment below was in the unmatched function. I have left it where I think it was most likely meant to go.
        //osSyncPrintf("stanLineDoor: %d rooms is more than %d\n");
        roomCount = 20;
    }

    lineStart.f[0] = start_x;
    lineStart.f[1] = start_z;
    lineEnd.f[0] = end_x;
    lineEnd.f[1] = end_z;

    if (cdtypes != 0)
    {
        roomBuffer[roomCount] = -1;
        roomGetProps(roomBuffer);

        propIndexPtr = ptr_list_object_lookup_indices;

        // Fake but needed for matching.
        if (polygon);

        if (*propIndexPtr >= 0)
        {
            do
            {
                prop = &g_Props[*propIndexPtr];

                if (propIsOfCdType(prop, cdtypes))
                {
                    chraiGetCollisionBoundsWithoutY(prop, &polygon, &numEdges);

                    if (numEdges > 0)
                    {
                        i = 0;

                        while (i < numEdges)
                        {
                            next = (i + 1) % numEdges;

                            if (doSegmentsIntersect(start_x, start_z, end_x, end_z, polygon->points[i].f[0], polygon->points[i].f[1], polygon->points[next].f[0], polygon->points[next].f[1]))
                            {
                                edgeStart.f[0] = polygon->points[i].f[0];
                                tmp = &polygon->points[i];
                                edgeStart.f[1] = (*tmp).f[1];

                                edgeEnd.f[0] = polygon->points[next].f[0];
                                edgeEnd.f[1] = polygon->points[next].f[1];

                                frac = calculateSegmentIntersectionFraction(&lineStart, &lineEnd, &edgeStart, &edgeEnd);

                                if (frac < bestFrac)
                                {
                                    bestFrac = frac;
                                    bestProp = prop;
                                }
                            }

                            i++;
                        }
                    }
                }

                propIndexPtr++;
            }
            while (*propIndexPtr >= 0);
        }
    }

    return bestProp;
}


/**
 * Computes the signed perpendicular distance from point P to the infinite
 * line that goes through point A and point B.
 * The sign indicates which side of the line the point is on.
 */
f32 stanGetSignedPointLineDistance(f32 a_x, f32 a_z, f32 b_x, f32 b_z, f32 p_x, f32 p_z)
{
    u32 stack[8];
    f32 result;

    result = sqrtf((b_x - a_x) * (b_x - a_x) + (b_z - a_z) * (b_z - a_z));

    if (result == 0.0f)
    {
        return sqrtf((p_x - b_x) * (p_x - b_x) + (p_z - b_z) * (p_z - b_z));
    }
    #ifdef DEBUG
    assert(d>0.0F);
    #endif
    return ((b_z - a_z) * (p_x - a_x) + -(b_x - a_x) * (p_z - a_z)) / result;
}


f32 distBetweenPoints2d(f32 o_x,f32 o_z,f32 p_x,f32 p_z)
{
    p_x -= o_x;
    p_z -= o_z;

    return sqrtf(p_x * p_x + p_z * p_z);
}


/**
 * Tests whether a point P's perpendicular projection onto the infinite line
 * going through points A and B falls inside the finite edge segment.
 */
bool stanPointProjectsOntoEdge(f32 a_x, f32 a_z, f32 b_x, f32 b_z, f32 p_x, f32 p_z)
{
    f32 f0;
    f32 f2;
    f32 f16;
    f32 f18;

    p_x -= a_x;
    p_z -= a_z;

    f0 = b_x - a_x;
    f2 = b_z - a_z;

    f16 = p_x * f0 + p_z * f2;
    f18 = f0 * f0 + f2 * f2;

    return (f18 < f16 && f16 < 0) || (f16 > 0 && f16 < f18);
}


s32 stanTestVolume(StandTile **arg0, f32 arg1, f32 arg2, f32 arg3, s32 cdtypes, f32 arg5, f32 arg6)
{
    s32 i; // stack ??
    f32 var_f20; // stack ??
    f32 var_f24; // stack ??
    s32 temp_v0; // stack ??
    s32 next; // stack ??

    s32 sp108;
    f32 temp_f0;  // stack ??
    s16 *sp100;
    s32 spFC;
    struct PropRecord *prop; // no stack
    s32 spA8[0x14];
    struct rect4f *polygon;
    s32 numvertices0;  // spa0
    f32 temp_f0_3; // stack ??
    f32 temp_f0_2; // stack ??
    f32 sp94;
    f32 sp90;

    s32 padding1;
    s32 padding2;

    sp108 = (arg6 <= arg5);

    spFC = 0;

    temp_v0 = stanTestCircleAndCollectRooms(arg0, arg1, arg2, arg3, &spA8[0], &spFC, 20);
    if (temp_v0 >= 0)
    {
        return temp_v0;
    }


    if (spFC > 20)
    {
        #ifdef DEBUG
            osSyncPrintf("stanCircleLegalXFObjTypeY: %d rooms is more than %d\n",spFC,20);
        #endif
        spFC = 20;
    }

    g_StanLastCollisionProp = NULL;

    if (cdtypes)
    {
        if (sp108)
        {
            temp_f0 = stanGetPositionYValue(*arg0, arg1, arg2);
            arg5 += temp_f0;
            arg6 += temp_f0;
        }

        spA8[spFC] = -1;
        roomGetProps(&spA8[0]);

        for (sp100 = ptr_list_object_lookup_indices; *sp100 >= 0; sp100++)
        {
            prop = &g_Props[*sp100];

            if (propIsOfCdType(prop, cdtypes) != 0)
            {
                chraiGetCollisionBounds(prop, &polygon, &numvertices0, &sp94, &sp90);
                if ((numvertices0 > 0) && ((sp108 == 0) || ((sp90 <= arg5) && (arg6 <= sp94))))
                {
                    var_f24 = -1.0f;

                    i=0;
                    while(1)
                    {
                        next = (i + 1) % numvertices0;

                        var_f20 = stanGetSignedPointLineDistance(polygon->points[i].f[0], polygon->points[i].f[1], polygon->points[next].f[0], polygon->points[next].f[1], arg1, arg2);

                        if (var_f20 < 0.0f)
                        {
                            var_f20 = -var_f20;
                        }

                        if (var_f24 < var_f20)
                        {
                            temp_f0_2 = distBetweenPoints2d(polygon->points[i].f[0], polygon->points[i].f[1], arg1, arg2);
                            temp_f0_3 = distBetweenPoints2d(polygon->points[next].f[0], polygon->points[next].f[1], arg1, arg2);

                            if ((var_f20 < arg3)
                                && (
                                    (temp_f0_2 < arg3)
                                    || (temp_f0_3 < arg3)
                                    || (stanPointProjectsOntoEdge(polygon->points[i].f[0], polygon->points[i].f[1], polygon->points[next].f[0], polygon->points[next].f[1], arg1, arg2) != 0)))
                            {
                                g_StanLastCollisionEdgePointsValid = 1;
                                var_f24 = var_f20;

                                g_StanLastCollisionEdgePointA.f[0] = polygon->points[i].f[0];
                                g_StanLastCollisionEdgePointA.f[1] = polygon->points[i].f[1];
                                g_StanLastCollisionEdgePointB.f[0] = polygon->points[next].f[0];
                                g_StanLastCollisionEdgePointB.f[1] = polygon->points[next].f[1];
                                g_StanLastCollisionTile = NULL;
                                g_StanLastCollisionEdgeIndex = 0;
                                g_StanLastCollisionProp = prop;
                            }
                        }

                        if (next == 0)
                        {
                            break;
                        }

                        i = next;
                    }

                    if (var_f24 > -1.0f)
                    {
                        return 2;
                    }
                }
            }
        }
    }

    return -2;
}


void stanResetHits(void)
{
    g_StanLastCollisionProp = NULL;
    g_StanLastCollisionTile = 0;
    g_StanLastCollisionEdgeIndex = 0;
    g_StanLastCollisionEdgePointsValid = 0;
}


void getTileEdgePoints(StandTile *tile, s32 pointI, coord3d *currPntRtn, coord3d *nextPointRtn)
{
    f32 scale;

    scale = inv_level_scale;

    currPntRtn->x = tile->points[pointI].x * scale;
    currPntRtn->y = tile->points[pointI].y * scale;
    currPntRtn->z = tile->points[pointI].z * scale;

    /**
     * This line could potentially become:
     * pointI = (pointI + 1) % STAN_POINT_COUNT(tile);
     * 
     * If STAN_POINT_COUNT were redefined as:
     * #define STAN_POINT_COUNT(tile) (((tile)->tail.half >> 12) & 0xf)
     * Something to consider?
     */
    pointI = (pointI + 1) % ((tile->tail.half >> 12) & 0xf);

    nextPointRtn->x = tile->points[pointI].x * scale;
    nextPointRtn->y = tile->points[pointI].y * scale;
    nextPointRtn->z = tile->points[pointI].z * scale;
}


/**
 * Traverses linked stan tiles that intersect a circle at the given X/Z
 * position. The callbacks can observe visited tiles, reject linked edges, or
 * allow traversal to continue after encountering a blocking edge.
 *
 * @return STAN_COLLISION_FOUND when a blocking edge is found,
 * STAN_COLLISION_TRAVERSAL_LIMIT when the tile stack limit is reached, or
 * STAN_COLLISION_NONE when traversal completes without a collision.
 */
StanCollisionResult stanTestCircleCollisionWithCallbacks(StandTile **startTile, f32 x, f32 z, f32 radius, standTileLocusCallback_A_t onVisitTile, standTileLocusCallback_B_t shouldBlockEdge, standTileLocusCallback_C_t onBoundaryOrComplete, struct StandTileLocusCallbackRecord *record)
{
    enum { STAN_CIRCLE_TRAVERSAL_LIMIT = 41 };
    s32 i;
    StandTile *visitedTiles[STAN_CIRCLE_TRAVERSAL_LIMIT];
    StandTile *tile;
    StandTile *linkedTile;
    s32 visitedCount;
    s32 tileCount;
    s32 pointCount;
    s32 edgeIndex;
    s32 nextEdgeIndex;
    f32 edgeDist;
    f32 pointDistA;
    f32 pointDistB;
    f32 firstPointDist;

    x *= level_scale;
    z *= level_scale;
    radius *= level_scale;
    visitedCount = 0;
    tileCount = 1;
    visitedTiles[0] = *startTile;

    while (visitedCount < tileCount)
    {
        tile = visitedTiles[visitedCount++];

        if (onVisitTile != NULL)
        {
            onVisitTile(tile, record);
        }

        pointCount = (tile->tail.half >> 12) & 0xf;

        if (pointCount > 0)
        {
            firstPointDist = distToTilePnt2D(tile, 0, x, z);
            pointDistA = firstPointDist;

            for (edgeIndex = 0; edgeIndex < pointCount; edgeIndex++)
            {
                nextEdgeIndex = edgeIndex + 1;
                edgeDist = getShortest2dDispToInfTileEdge(tile, edgeIndex, x, z);

                if (nextEdgeIndex == pointCount)
                {
                    nextEdgeIndex = 0;
                    pointDistB = firstPointDist;
                }
                else
                {
                    pointDistB = distToTilePnt2D(tile, nextEdgeIndex, x, z);
                }

                if (edgeDist < radius && (pointDistA < radius || pointDistB < radius || stanPointProjectsOntoTileEdge(tile, edgeIndex, x, z)))
                {
                    if ((shouldBlockEdge == NULL || !shouldBlockEdge(tile, edgeIndex, edgeDist, pointDistA, pointDistB, record)) && (tile->points[edgeIndex].link >> 4))
                    {
                        linkedTile = (StandTile *)((u32)standTileStart + (tile->points[edgeIndex].link << 3));

                        for (i = tileCount - 1; i >= 0; i--)
                        {
                            if (linkedTile == visitedTiles[i])
                            {
                                break;
                            }
                        }

                        if (i < 0)
                        {
                            if (tileCount >= STAN_CIRCLE_TRAVERSAL_LIMIT)
                            {
                                return STAN_COLLISION_TRAVERSAL_LIMIT;
                            }

                            visitedTiles[tileCount++] = linkedTile;
                        }
                    }
                    else
                    {
                        g_StanLastCollisionTile = tile;
                        g_StanLastCollisionEdgeIndex = edgeIndex;

                        if (onBoundaryOrComplete == NULL || onBoundaryOrComplete(visitedTiles, tileCount, record) != 1)
                        {
                            return STAN_COLLISION_FOUND;
                        }
                    }
                }

                pointDistA = pointDistB;
            }
        }

        if ((u32)tileCount >= STAN_CIRCLE_TRAVERSAL_LIMIT)
        {
            return STAN_COLLISION_TRAVERSAL_LIMIT;
        }
    }

    if (onBoundaryOrComplete != NULL)
    {
        onBoundaryOrComplete(visitedTiles, tileCount, record);
    }

    return STAN_COLLISION_NONE;
}


s32 stanTestCircleCollisionNoCallbacks(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius)
{
    return stanTestCircleCollisionWithCallbacks(tileStack, target_x, target_z, radius, NULL, NULL, NULL, NULL);
}


/**
 * Callback for stan locus traversal.
 * 
 * Adds the current tile's room ID to the caller-provided room list if it
 * has not already been recorded. The callback always returns 0 so traversal
 * continues.
 */
s32 stanLocusAddTileRoomIfNew(StandTile *tile, struct StandTileLocusCallbackRecord *rec)
{
    s32 roomCount;
    s32 room;
    StandTile *t;
    struct StandTileLocusCallbackRecord *record;
    s32 i;
    s32 *ptr;
    
    record = rec;
    i = 0;
    roomCount = record->count;
    t = tile;
    
    // Only search for duplicates if more than 0 rooms have been collected.
    if (roomCount > 0)
    {
        room = t->room;
        ptr = record->rooms;
        
        //If the tile's room is already in the room list, return immediately.
        do
        {
            if (room == (*ptr))
            {
                return 0;
            }
            
            i++;
            ptr++;
        }
        while (i < record->count);
    }
    
    // The room has not been collected yet so append it.
    if (roomCount < rec->bufMax)
    {
        rec->rooms[roomCount] = tile->room;
        rec->count = rec->count + 1;
    }
    
    return 0;
}


s32 stanLocusCountBoundaryEdge(StandTile **tileStack, s32 stackHeight, struct StandTileLocusCallbackRecord* data)
{
    data->boundaryEdgeCount += 1;
    return 1;
}


StanCollisionResult stanTestCircleAndCollectRooms(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius, s32 *rooms, s32 *count_rtn, s32 bufMax)
{
    struct StandTileLocusCallbackRecord data;
    s32 rtn;

    data.rooms = rooms;
    data.count = 0;
    data.bufMax = bufMax;
    data.boundaryEdgeCount = 0;

    rtn = stanTestCircleCollisionWithCallbacks(tileStack, target_x, target_z, radius, stanLocusAddTileRoomIfNew, NULL, stanLocusCountBoundaryEdge, &data);

    *count_rtn = data.count;

    if (1 < data.boundaryEdgeCount)
    {
        return 2;
    }

    return rtn;
}


s32 stanIsSpecialBit1Set(StandTile *arg0, struct StandTileLocusCallbackRecord *arg1)
{
    s32 val = arg0->mid.half >> 0xC;
    if (g_StanTileSpecialFlags[val] & STANTILEFLAG_FORCECROUCH)
    {
        arg1->rooms = 1;
    }

    return 0;
}


s32 stanCheckLinkedSpecialTile(StandTile *tile, s32 pointIdx, s32 arg2, s32 arg3, s32 arg4, s32 *outFlags)
{
    u16 link;
    StandTile *target;
    s32 mid;

    link = tile->points[pointIdx].link;

    if ((link >> 4) != 0) {
        target = (StandTile *)(link + (StandTile *)standTileStart);

        mid = target->mid.half;

        if (g_StanTileSpecialFlags[mid >> 0xc] & STANTILEFLAG_FORCECROUCH) {
            outFlags[0] = 1;
            return 1;
        }

        mid = target->mid.half;

        if (g_StanTileSpecialFlags[mid >> 0xc] & STANTILEFLAG_LADDER) {
            g_StanDetectedLadderTile = target;
            outFlags[1] = 1;
            return 0;
        }
    }

    return 0;
}


s32 stanTileDistanceRelated(StandTile **arg0, f32 arg1, f32 arg2, f32 arg3, struct StandTileLocusCallbackRecord *arg4)
{
    s32 i;

    // HACK:
    for(i=0;;)
    {
        ((s32*)arg4)[i+0] = 0;
        ((s32*)arg4)[i+1] = 0;
        ((s32*)arg4)[i+2] = 0;
        ((s32*)arg4)[i+3] = 0;
        i+=4;
        if (i>15) break;
    }

    // maybe something like:
    /*
    for(i=0;i<3;i++)
    {
        arg4[i].unk00 = 0;
        arg4[i].count = 0;
        arg4[i].bufMax = 0;
        arg4[i].boundaryEdgeCount = 0;
    }
    */

    return stanTestCircleCollisionWithCallbacks(arg0, arg1, arg2, arg3, stanIsSpecialBit1Set, stanCheckLinkedSpecialTile, NULL, arg4);
}


s32 stanGetLocusField0(struct StandTileLocusCallbackRecord *arg0)
{
    return arg0->rooms;
}


s32 stanGetLocusCount(struct StandTileLocusCallbackRecord *arg0)
{
    return arg0->count;
}


void stanGetTileOrderedPointWorldPos(StandTile *tile, s32 pointnum, coord3d *out)
{
    StandTilePoint *point;
    f32 scale;

    pointnum = tile->tail.half >> (8 - (pointnum * 4));

    point = &tile->points[pointnum & 0xf];

    scale = inv_level_scale;

    out->x = point->x * scale;
    out->y = point->y * scale;
    out->z = point->z * scale;
}


void stanGetMoveBondCollisionTiles(StandTile **tile1, StandTile **tile2, coord3d *coords)
{
    StandTile *curtileStore;
    StandTile *baseTile;
    StandTile *linktile;
    s32 curtilePointI;
    s32 i;
    s32 j;
    s32 k;
    s32 target;

    baseTile = g_StanDetectedLadderTile;

    target = baseTile->tail.hdrTail.pointCount & 0xf;

    i = 0;

    if (i < target)
    {
        do
        {
            linktile = (StandTile *)((u8 *)standTileStart + (baseTile->points[i].link << 3));

            if ((baseTile->points[i].link >> 4) != 0)
            {
                s32 linkTileMid;
            
                linkTileMid = linktile->mid.half;
            
                if (g_StanTileSpecialFlags[linkTileMid >> 12] & STANTILEFLAG_LADDER)
                {
#ifdef DEBUG
                    assert(getsides(linktile) == 3);
#endif
                    curtilePointI = (i + 2) % 3;
            
                    *tile1 = baseTile;
                    *tile2 = linktile;
            
                    j = 0;
                    curtileStore = baseTile;

                    while (1)
                    {
                        for (k = 0; k < 3; k++)
                        {
                            stanGetTileOrderedPointWorldPos(
                                linktile,
                                ((j >> 2) + k) % 3,
                                (coord3d *)((s32)coords + (((j + k) & 3) * 0xc)));
                        }

                        stanGetTileOrderedPointWorldPos(
                            curtileStore,
                            curtilePointI,
                            (coord3d *)((s32)coords + (((j + 3) & 3) * 0xc)));

                        j++;

                        if (j == 12)
                        {
#ifdef DEBUG
                            osSyncPrintf("rotate==12\n");
#endif
                            break;
                        }

                        if (!(coords[2].y < coords[0].y)
                                && !(coords[2].y < coords[1].y)
                                && !(coords[3].y < coords[0].y)
                                && !(coords[3].y < coords[1].y))
                        {
                            break;
                        }
                    }

                    return;
                }
            }

            i++;

            if (i < target)
            {
                continue;
            }

            break;
        }
        while (TRUE);
    }
#ifdef DEBUG
    osSyncPrintf("Ladder %s has no neighbouring ladder stan\n", GetStanRoomID(baseTile));
#endif
}


/**
 * Callback function.
 * 
 * For a given edge, return true if the edge is vertically above yThreshold.
 */
bool stanLocusEdgeIsAboveY(StandTile *tile, s32 edgeIndex, f32 edgeDist, f32 distToPointA, f32 distToPointB, f32 *yThreshold)
{
    s32 nextIndex;
    s32 pointCount;
    f32 *threshold;
    s32 pointCountReload;

    threshold = yThreshold;

    if (*yThreshold < (f32)tile->points[edgeIndex].y)
    {
        /**  
         * The duplicated point count calculation is required for matching.
         * This is really just nextIndex = (edgeIndex + 1) % pointCount;
         */
        pointCount = (tile->tail.half >> 12) & 0xf;
        pointCountReload = (tile->tail.half >> 12) & 0xf;

        nextIndex = (edgeIndex + 1) % pointCount;

        pointCount = pointCountReload;

        if (*threshold < (f32)tile->points[nextIndex].y)
        {
            return TRUE;
        }
    }

    return FALSE;
}


s32 stanTestLocusEdgeAboveY(StandTile **tile, f32 target_x, f32 target_z, f32 radius, f32 yThreshold)
{
    f32 data;

    data = yThreshold * level_scale;

    /// TODO: Why is this cast wrong?

    return stanTestCircleCollisionWithCallbacks(tile, target_x, target_z, radius, NULL, stanLocusEdgeIsAboveY, NULL, (struct StandTileLocusCallbackRecord*)&data);
}


typedef struct BfsSearchLocals {
    StandTile **stackptr;
    s32 pad48;
    s32 pad4c;
    s32 pad50;
    s32 pad54;
    s32 lastnumtiles;
} BfsSearchLocals;


// Horrifc BFS on tiles
//
// Four things look like they could be improved
// 1. The outer loop always restarts at bfsTileStack[0].
//    The next 'wave' will process tiles it already scanned in the second wave
//    Neighbors are checked again needlessly
//    This has a high cost if the waypoint the game is trying to find
//    is far away on the stans
//    Should reset to the previous value of loc.lastnumtiles
// 2. 'seenCount' can become really big because it's all the stans discovered so far
//    Neighbor checks thus become exponentially expensive
//    There could be a faster way to check that a stan was already visited
// 3. The closest pad to a tile is something that could be precomputed
//    because it's static and never changes at run-time
// 4. This function is called at one single location and the second arg
//    is always the same. It could return the result from tilePred already.
StandTile *stanFillSearch(StandTile *starttile, tilePredicate_t predicate) // stanFillSearch is the canonical name for this function
{
    StandTile **stackbase;
    BfsSearchLocals loc;
    StandTile **tileStartAddr;
    StandTilePoint *point;
    StandTile *linkedtile;
    s32 seenCount;
    s32 pointindex;
    s32 pointcount;
    s32 link;
    s32 i;
    s32 stackindex;
    StandTile **tileStack;

    if (predicate(starttile))
    {
        return starttile;
    }

    tileStartAddr = &standTileStart;

    // Add the starting tile to the discovered tile stack.
    bfsTileStack[0] = starttile;
    seenCount = 1;

    // Process the discovered tiles in waves. loc.lastnumtiles is the number of tiles that existed at the beginning of the current wave.
    for (loc.lastnumtiles = 1, tileStack = bfsTileStack, stackbase = tileStack;; loc.lastnumtiles = seenCount)
    {
        stackindex = 0;

        if (seenCount > 0)
        {
            // This restarts at the beginning of the stack for every wave rather than beginning at the new frontier.
            loc.stackptr = tileStack;

            do
            {
                pointindex = 0;
                starttile = *loc.stackptr;
                point = (StandTilePoint *)starttile;
                pointcount = (starttile->tail.half >> 12) & 0xf;

                // Loop over the tile's linked points.
                if (pointcount > 0)
                {
                    do
                    {
                        link = point[1].link;

                        // This assignment is logically redundant but needed for matching.
                        tileStartAddr = &standTileStart;

                        // Keep this expression as written. Simplifying it to "link << 3" changes register usage.
                        linkedtile = (StandTile *)(((u8 *)(*tileStartAddr)) + (link << ((0, 3))));

                        // A zero high portion indicates that there is no valid linked tile to visit.
                        if ((link >> 4) != 0)
                        {
                            // Loop to see if this is a new tile (disgusting)
                            for (i = 0; i < seenCount; i++)
                            {
                                if (linkedtile == tileStack[i])
                                {
                                    goto nextpoint;
                                }
                            }

                            // Return as soon as a matching tile is found.
                            if (predicate(linkedtile))
                            {
                                return linkedtile;
                            }

                            // Add the newly discovered tile to the stack.
                            stackbase[seenCount] = linkedtile;
                            seenCount++;

                            if ((u32)seenCount >= 351)
                            {
                                #ifdef DEBUG
                                    osSyncPrintf("Out of confs[] in stanFillSearch()\n");
                                #endif
                                return 0;
                            }

                            pointcount = (starttile->tail.half >> 12) & 0xf;
                        }

nextpoint:
                        pointindex++;
                        point++;
                    }
                    while (pointindex < pointcount);
                }

                stackindex++;
                loc.stackptr++;
            }
            while (stackindex < loc.lastnumtiles);
        }

        if (predicate || tileStartAddr);

        // We only continue if we made progress with this iteration
        if (seenCount != loc.lastnumtiles)
        {
            // This is logically redundant because both pointers refer to bfsTileStack, but it's still needed for matching.
            stackbase = tileStack;

            continue;
        }

        // No new tiles were discovered so the search is exhausted.
        return 0;
    }
}


/**
 * @param pntA: out parameter, will contain g_StanLastCollisionEdgePointA (x,z)
 * @param pntB: out parameter, will contain g_StanLastCollisionEdgePointB (x,z)
 */
bool getCollisionEdge_maybe(coord3d *pntA, coord3d *pntB)
{
    if (g_StanLastCollisionTile)
    {
        getTileEdgePoints(g_StanLastCollisionTile, g_StanLastCollisionEdgeIndex, pntA, pntB);

        return TRUE;
    }
    else
    {
        if (g_StanLastCollisionEdgePointsValid)
        {
            pntA->x = g_StanLastCollisionEdgePointA.f[0];
            pntA->y = 0;
            pntA->z = g_StanLastCollisionEdgePointA.f[1];

            pntB->x = g_StanLastCollisionEdgePointB.f[0];
            pntB->y = 0;
            pntB->z = g_StanLastCollisionEdgePointB.f[1];

            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
}


void setLevelScale(f32 ls)
{
    level_scale = ls;
    inv_level_scale = (1.0f / ls);

    return;
}


/**
 * Calculates y value on a tile, according to (x,z) position.
 */
f32 stanGetPositionYValue(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 a[3]; // sp 132, vector a
    f32 b[3]; // sp 120, vector b
    s64 cp[3]; // sp 96, cross product vector (a x b)
    s64 rsum;
    s32 temp_a3;
    s32 temp_t6;
    s32 temp_t7;

    p_x *= level_scale;
    temp_t6 = STAN_TAIL_D(tile);
    temp_t7 = STAN_TAIL_C(tile);
    temp_a3 = STAN_TAIL_POINT_COUNT(tile);
    p_z *= level_scale;

    a[0] = (f32) (tile->points[temp_t7].x - tile->points[temp_t6].x);
    a[1] = (f32) (tile->points[temp_t7].y - tile->points[temp_t6].y);
    a[2] = (f32) (tile->points[temp_t7].z - tile->points[temp_t6].z);

    b[0] = (f32) (tile->points[temp_a3].x - tile->points[temp_t6].x);
    b[1] = (f32) (tile->points[temp_a3].y - tile->points[temp_t6].y);
    b[2] = (f32) (tile->points[temp_a3].z - tile->points[temp_t6].z);

    // implicit call to __f_to_ll
    // This is the cross product, a x b
    cp[0] = (s64)((a[1] * b[2]) - (a[2] * b[1]));
    cp[1] = (s64)((a[2] * b[0]) - (a[0] * b[2]));
    cp[2] = (s64)((a[0] * b[1]) - (a[1] * b[0]));

    // implicit call to __ll_mul
    rsum = ((s64)cp[0] * (s64)tile->points[temp_t6].x)
        + ((s64)cp[1] * (s64)tile->points[temp_t6].y)
        + ((s64)cp[2] * (s64)tile->points[temp_t6].z);

    // don't divide by zero
    if (cp[1] == 0)
    {
        return (f32) tile->points[temp_t6].y * inv_level_scale;
    }

    return (f32) ((((f64)(rsum) - ((f64) p_x * (f64)cp[0])) - ((f64) p_z * (f64)cp[2])) / (f64)(cp[1])) * inv_level_scale;
}


 /**
 * Get 24bit id stanIdHi from id string
 * @param stanIdHi: 1bit Type, 15bit Integer ID.
 * @param stanIdLo: 5bit stanIdLo File (a-z) and 3bit subtri 0-7
 */
void stanPackId(char *id, u16 *stanIdHi, u8 *stanIdLo)
{
    u32   bitsnumber; // sp3c
    char *str_end;    // sp38
    s32   y;          // sp34
    s32   bitsletter; // sp30
    s32   bitsfile;   // sp2c
    s32   bitssubtri; // sp28
    s32   var1;       // sp24

    var1 = id[0] - 'p';

    if (var1 < 0 || var1 > 1) // is p or q the first char (q never used?)
    {
        #ifdef ENABLE_LOG
            osSyncPrintf("stanPackId(): Bad letter chr \'%c\' in \"%s\"\n", id[0], id);
        #endif
    }
    else
    {
        bitsletter = id[0] - 'p'; // yes this is right, Im duplicated.
        bitsnumber = strtol(id + 1, &str_end, 10);

        if (id == str_end - 1)
        {
            #ifdef ENABLE_LOG
                osSyncPrintf("stanPackId(): Bad integer in \"%s\"\n", id);
            #endif
        }
        else if (bitsnumber <= 32767)
        {
            if (str_end[0] - 'a' < 0 || str_end[0] - 'a' >= 26)
            {
                #ifdef ENABLE_LOG
                    osSyncPrintf("stanPackId(): Bad file chr \'%c\' in \"%s\"\n", str_end[0], id);
                #endif
            }
            else
            {
                bitsfile = str_end[0] - 'a';
                if (1)
                    ;
                bitssubtri = str_end[1];

                if (bitssubtri != 0 && bitssubtri != '0')
                {
                    bitssubtri -= '0';
                }

                if (bitssubtri < 0 || bitssubtri >= 8)
                {
                    #ifdef ENABLE_LOG
                        osSyncPrintf("stanPackId(): Bad subtri chr \'%c\' in \"%s\"\n", str_end[1], id);
                    #endif
                }
                else
                {
                    if (str_end[1] == 0 || str_end[2] == 0)
                    {
                        #ifdef DEBUG
                        assert(bitsletter>=0&&bitsletter<=1);     // # 1094 "stan.c"
                        assert(bitsnumber>=0&&bitsnumber<=32767); // # 1095 "stan.c"
                        assert(bitsfile >=0&&bitsfile <=31);      // # 1096 "stan.c"
                        assert(bitssubtri>=0&&bitssubtri<=7);     // # 1097 "stan.c"
                        #endif
                        *stanIdHi = bitsletter << 0xf | bitsnumber;
                        *stanIdLo = bitsfile << 3 | bitssubtri;
                        return;
                    }
                    else
                    {
                        #ifdef ENABLE_LOG
                            osSyncPrintf("stanPackId(): Stan id too long \"%s\"\n", id);
                        #endif
                    }
                }
            }
        }
        else
        {
            #ifdef ENABLE_LOG
                osSyncPrintf("stanPackId(): Integer %d out of range in \"%s\"\n", bitsnumber, id);
            #endif
        }
    }
    *stanIdHi = -1;
    *stanIdLo = -1;
    return;
}


struct StandTilePoint *stanMatchTileName(char *id)
{
    StandTilePoint *tile;
    u16 stanIdHi;
    u8 stanIdLo;
    s16 tmp;

    if (*id == '\0') {
        return NULL;
    }

    stanPackId(id, &stanIdHi, &stanIdLo);

    tile = stan_prefix->ptr_firstroom;

    while (*(u32 *)tile != 0) {
        if ((u16)tile->x == stanIdHi) {
            if (*((u8 *)&tile->y) == stanIdLo) {
                return tile;
            }
        }

        tmp = tile->link;
        tile = (StandTilePoint *)((u8 *)tile +
            g_StanTileSizeByPointCount[(tmp >> 12) & 0xf]);
    }

    return NULL;
}


void stanDetermineEOF(struct StanPrefixRecord *file /* canonically r */, s32 origBase, u8 *newBase)
{
    s32 delta;
    void **roomPtr;
    StandTile *tile;
    u8 *tileSizes;
    
    delta = ((s32) newBase) - origBase;
    stan_prefix = file;
  
    standTileStart = (StandTile *)(((s32)file->ptr_firstroom + delta) - 0x80);
    
    newBase = g_StanTileSizeByPointCount;
    roomPtr = (void **)&file->ptr_firstroom;
    
    if (file->ptr_firstroom != NULL)
    {
        do
        {
            *roomPtr = (void *) ((s32) (*roomPtr) + delta);
            roomPtr++;
        }
        while (*roomPtr != NULL);
    }
    
    tile = (StandTile *) (roomPtr + ((0, 1)));
    
    if ((*(s32 *) tile) != 0)
    {
        do
        {
            stanTileEnd = tile;

            // Fake but required for matching.
            if (tile->tail.half);
            
            tile = (StandTile *)((s32)tile
                + (tileSizes = newBase)[(tile->tail.half >> 0xc) & 0xf]);
        } 
        while (*(s32 *) tile != 0);
    }
    
    stan_prefix = file;
}


/**
 Get the room the tile belongs to
 @param tile: Tile to quiry
 @return the room number the tile is located in
 @exception Although room is u8, this needs to be s32 for matching ai.
 */
s32 getTileRoom(StandTile *tile)
{
    return tile->room;
}
