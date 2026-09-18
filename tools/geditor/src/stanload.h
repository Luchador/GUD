#ifndef GEDITOR_STANLOAD_H
#define GEDITOR_STANLOAD_H

#include "rotation.h"
#include "scaling.h"
#include <windows.h>

#include "rom.h"

#define STAN_TILE_MAX_POINTS 10
#define STAN_TILE_NONE ((DWORD)-1)
/* The runtime room index has 139 slots, including reserved room zero. */
#define STAN_MAX_ROOM 138u

/* Host-native form of one point from a variable-length stan tile.
   Positions are converted to gameplay world units while the authored
   edge link is retained for future editing and inspection tools. */
typedef struct StanPoint {
    float x, y, z;
    unsigned short link;
} StanPoint;

/* Semantic view of a stan tile. The RGB components are expanded from
   the file's four-bit channels to ordinary eight-bit editor colors. */
typedef struct StanTile {
    DWORD id;
    DWORD editorid; /* stable during this level session, never serialized; native IDs can repeat */
    DWORD sourceoffset; /* byte offset for resolving authored edge links */
    unsigned char room;
    unsigned char special;
    unsigned char red, green, blue;
    unsigned char pointcount;
    unsigned char extreme[3];
    StanPoint points[STAN_TILE_MAX_POINTS];
} StanTile;

/* One raw project stan plus its decoded variable-sided tiles. */
typedef struct StanFile {
    unsigned char *data;
    DWORD size;
    char name[64];
    StanTile *tiles;
    DWORD tilecount;
    float levelscale; /* restores file-space arithmetic for runtime queries */
    BOOL dirty;
} StanFile;

/* Copies every Tbg_*_stanZ resource into <projectdir>\stan as a
   .stan file. Returns the number written, or 0 with a reason. */
DWORD StanExtractAll(const RomFile *rom, const char *projectdir,
                     const char **reasonout);

/* Loads and decodes <projectdir>\stan\<stanname>.stan. Coordinates
   are divided by levelscale, matching the game's stan world scale. */
BOOL StanLoadProjectFile(const char *projectdir, const char *stanname,
                         float levelscale, StanFile *out,
                         const char **reasonout);

/* Overwrites the project copy with the raw stan retained in memory. */
BOOL StanSaveProjectFile(const char *projectdir, const StanFile *stan,
                         const char **reasonout);

void StanFileFree(StanFile *stan);
BOOL StanFileClone(const StanFile *source, StanFile *out, const char **reasonout);

/* A point identity is local to its tile. The point map joins coincident
   endpoints only through authored tile links, never unrelated stacked floors. */
typedef struct StanPointRef { DWORD tile, point; } StanPointRef;
/* Directed perimeter edge: point -> (point + 1) % pointcount. */
typedef struct StanEdgeRef { DWORD tile, point; } StanEdgeRef;
/* Bridge two unshared boundary edges with one triangle at a shared endpoint,
 * or two triangles across a four-endpoint gap, with reciprocal source links.
 * Inherit the first edge's room and tile attributes, even across rooms. Native
 * endpoints stay exact; validation/allocation failure leaves the file intact.
 * countout receives 1 or 2 on success (0 on failure); out holds two tile indices,
 * with out[1] set to STAN_TILE_NONE when only one triangle is created. */
BOOL StanCanBridgeEdges(const StanFile *stan, const StanEdgeRef edges[2], const char **reasonout);
BOOL StanBridgeEdges(StanFile *stan, const StanEdgeRef edges[2], DWORD out[2],
    DWORD *countout, const char **reasonout);
/* Insert the native midpoint, split the tile and all linked incident tiles,
 * and relocate external/internal links atomically. Triangles become two
 * triangles; larger convex tiles become two convex polygons. */
BOOL StanBisectEdge(StanFile *stan, const StanEdgeRef *edge,
    StanEdgeRef *out, const char **reasonout);
/* Like BG Split Edge, detach the incident tiles' endpoints, without adding
 * points. Remove links that would still join those endpoints in either direction. */
BOOL StanSplitEdge(StanFile *stan, const StanEdgeRef *edge,
    BOOL *changedout, const char **reasonout);
/* Merge canonical selected points at their average native position. Remove
 * tiles collapsed to a line/point. mergedout.tile is NONE if none survives. */
BOOL StanMergeVertices(StanFile *stan, const StanPointRef *points, DWORD count,
    StanPointRef *mergedout, const char **reasonout);
/* Exact 3D area test for native, eight-byte perimeter point records. */
BOOL StanPointsHaveArea(const unsigned char *points, DWORD count);
/* Recompute the native height-query triangle after topology/position edits. */
void StanUpdateRepresentativeTriangle(StanFile *stan, DWORD index);
DWORD StanLinkedTile(const StanFile *stan, unsigned short link);
DWORD *StanBuildPointMap(const StanFile *stan, const char **reasonout);
BOOL StanTranslatePoints(StanFile *stan, const StanPointRef *points, DWORD count,
                          const double offset[3], DWORD *movedout,
                          const char **reasonout);
/* Atomic tile removal: compact records and relocate edge/header pointers.
 * Links to deleted tiles become boundaries. At least one tile must remain. */
BOOL StanDeleteTiles(StanFile *stan, const DWORD *selected, DWORD count,
    DWORD *deletedout, const char **reasonout);
/* Remove the incident tiles of selected canonical edges, not the entire
 * connected mesh. Coincident but unlinked floors remain independent. */
BOOL StanDeleteEdgeTiles(StanFile *stan, const StanEdgeRef *edges, DWORD count,
    DWORD *deletedout, const char **reasonout);
/* After a position edit, remove only changed tiles that are now lines/points.
 * Call inside the transform's undo transaction, after preview is committed. */
BOOL StanDeleteCollapsedTiles(const StanFile *before, StanFile *stan,
    DWORD *deletedout, const char **reasonout);
/* Connect the unique shared edge of two tiles in both directions. Native
 * endpoints must match in reverse order. Existing third-party links are kept. */
BOOL StanLinkTiles(StanFile *stan, DWORD first, DWORD second,
    BOOL *changedout, const char **reasonout);
/* Find the unique opposite edge of another tile and link it reciprocally.
 * Matching uses exact native XYZ endpoints, not proximity or screen overlap. */
BOOL StanLinkEdgeTiles(StanFile *stan, const StanEdgeRef *edge,
    BOOL *changedout, const char **reasonout);
/* Keep live tile indices stable. Saving groups tiles by room and relocates
 * native links in a copy for the runtime's contiguous room scans. */
BOOL StanSetTileRooms(StanFile *stan, const DWORD *selected, DWORD count,
    DWORD room, DWORD roomcount, DWORD *changedout, const char **reasonout);
/* RGB is quantized to the format's 4-bit channels; stan has no stored alpha. */
BOOL StanPaintTile(StanFile *stan, DWORD tile, const unsigned char rgba[4],
                   BOOL *changedout, const char **reasonout);

/* Runtime placement queries. Positions/heights are gameplay world units.
   The named tile takes priority; fallback matches padAssignStanTile's
   nearest-walkable sample search and linked walk, without altering pos. */
DWORD StanResolvePadTile(const StanFile *stan, const char *name,
                         const float pos[3]);
/* Preserve a moved prop's connected floor when below its destination. Otherwise
 * prefer the highest containing floor below, then legacy destination lookup.
 * For new props, pass an empty name and the pad position as both endpoints.
 * Returns a native setup tile name; loading existing assets stays unchanged. */
BOOL StanResolveMovedPadName(const StanFile *stan, const char *name,
    const float from[3], const float to[3], char resolved[16]);
BOOL StanWalkTiles(const StanFile *stan, DWORD *tile,
                   float startx, float startz, float endx, float endz);
BOOL StanGetTileHeight(const StanFile *stan, DWORD tile,
                       float x, float z, float *height);

BOOL StanRotatePoints(StanFile *stan, const StanPointRef *points, DWORD count,
    const Rotation *rotation, const double pivot[3], DWORD *movedout, const char **reasonout);

BOOL StanScalePoints(StanFile *stan, const StanPointRef *points, DWORD count,
    const Scaling *scale, DWORD *movedout, const char **reasonout);

#endif /* GEDITOR_STANLOAD_H */
