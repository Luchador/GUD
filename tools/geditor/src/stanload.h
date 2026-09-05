#ifndef GEDITOR_STANLOAD_H
#define GEDITOR_STANLOAD_H

#include <windows.h>

#include "rom.h"

#define STAN_TILE_MAX_POINTS 10

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

void StanFileFree(StanFile *stan);

#endif /* GEDITOR_STANLOAD_H */
