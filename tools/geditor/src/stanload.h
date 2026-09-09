#ifndef GEDITOR_STANLOAD_H
#define GEDITOR_STANLOAD_H

#include "rotation.h"
#include "scaling.h"
#include <windows.h>

#include "rom.h"

#define STAN_TILE_MAX_POINTS 10
#define STAN_TILE_NONE ((DWORD)-1)

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
DWORD StanLinkedTile(const StanFile *stan, unsigned short link);
DWORD *StanBuildPointMap(const StanFile *stan, const char **reasonout);
BOOL StanTranslatePoints(StanFile *stan, const StanPointRef *points, DWORD count,
                          const double offset[3], DWORD *movedout,
                          const char **reasonout);
/* RGB is quantized to the format's 4-bit channels; stan has no stored alpha. */
BOOL StanPaintTile(StanFile *stan, DWORD tile, const unsigned char rgba[4],
                   BOOL *changedout, const char **reasonout);

/* Runtime placement queries. Positions/heights are gameplay world units.
   The named tile takes priority; fallback matches padAssignStanTile's
   nearest-walkable sample search and linked walk, without altering pos. */
DWORD StanResolvePadTile(const StanFile *stan, const char *name,
                         const float pos[3]);
BOOL StanWalkTiles(const StanFile *stan, DWORD *tile,
                   float startx, float startz, float endx, float endz);
BOOL StanGetTileHeight(const StanFile *stan, DWORD tile,
                       float x, float z, float *height);

BOOL StanRotatePoints(StanFile *stan, const StanPointRef *points, DWORD count,
    const Rotation *rotation, const double pivot[3], DWORD *movedout, const char **reasonout);

BOOL StanScalePoints(StanFile *stan, const StanPointRef *points, DWORD count,
    const Scaling *scale, DWORD *movedout, const char **reasonout);

#endif /* GEDITOR_STANLOAD_H */
