#ifndef GEDITOR_BGLOAD_H
#define GEDITOR_BGLOAD_H

#include <windows.h>

#include "rom.h"

/*
 * GoldenEye background geometry loader.
 *
 * Parses a bg .seg file (uncompressed, as GUD ships them) into a flat
 * triangle soup with vertex colors, texture coordinates, and render
 * state tags. Primary and secondary (transparent) room geometry are
 * both included.
 */

typedef struct BgVertex {
    float x, y, z;
    float s, t;                 /* texel coordinates (already /32) */
    unsigned char r, g, b, a;
} BgVertex;

/* Raw project background retained while a level is open. The compiler uses
   it to preserve the header, portals, visibility data, and other data which
   sits outside editable room geometry. */
typedef struct BgFile {
    unsigned char *data;
    DWORD size;
    char name[64];
} BgFile;

#define BG_PORTAL_MAX_POINTS 8

/* Host-native view of one entry in the BG portal table. Portal points
   are absolute (not room-relative) and are converted to gameplay world
   units while the room links and control bytes remain authored data.
   geometryoffset identifies entries which share one polygon. */
typedef struct BgPortalPoint {
    float x, y, z;
} BgPortalPoint;

typedef struct BgPortal {
    DWORD geometryoffset;
    unsigned char connectedroom1;
    unsigned char connectedroom2;
    unsigned char controlbytes1;
    unsigned char controlbytes2;
    unsigned char pointcount;
    BgPortalPoint points[BG_PORTAL_MAX_POINTS];
} BgPortal;

typedef struct BgPortalFile {
    BgPortal *portals;
    DWORD portalcount;
} BgPortalFile;

/*
 * Per-triangle tag: low 12 bits are the texture ID (0xFFF = none),
 * bit 12 marks secondary (transparent-layer) geometry, bit 13 records
 * whether Fast3D backface culling was enabled, and bit 14 identifies
 * setup-object geometry appended to the viewport scene.
 */
#define BG_TEX_ID_MASK   0x0FFF
#define BG_TEX_NONE      0x0FFF
#define BG_TRI_SECONDARY 0x1000
#define BG_TRI_CULL_BACK 0x2000
#define BG_TRI_OBJECT    0x4000

#define BG_TEX_ID(tag)        ((tag) & BG_TEX_ID_MASK)
#define BG_TRI_IS_SECONDARY(tag) (((tag) & BG_TRI_SECONDARY) != 0)
#define BG_TRI_CULLS_BACK(tag)   (((tag) & BG_TRI_CULL_BACK) != 0)
#define BG_TRI_IS_OBJECT(tag)    (((tag) & BG_TRI_OBJECT) != 0)

/*
 * Parses the bg file at data (maxlen readable bytes) and returns a
 * malloc'd array of tricount*3 BgVertex, or NULL with *reasonout set.
 * Positions are converted from the bg's scaled room coordinates to
 * gameplay world coordinates using levelscale. Caller frees.
 *
 * tritags receives a malloc'd array of tricount entries containing the
 * texture, layer, and culling flags described above. Caller frees.
 */
BgVertex *BgLoadGeometry(const unsigned char *data, DWORD maxlen,
                         float levelscale,
                         DWORD *tricount, unsigned short **tritags,
                         const char **reasonout);

/* Decodes the portal table embedded in a room-based BG. Repeated table
   entries are retained because their room links are meaningful; their
   shared geometryoffset lets the viewport draw the polygon only once. */
BOOL BgLoadPortals(const unsigned char *data, DWORD maxlen,
                   float levelscale, BgPortalFile *out,
                   const char **reasonout);

/*
 * Copies every complete .seg resource beneath bg/ in the ROM file table
 * into <projectdir>\bg, preserving its rooms, embedded portals, and
 * filename. Returns the number written, or 0 with *reasonout set if
 * extraction could not complete.
 */
DWORD BgExtractAll(const RomFile *rom, const char *projectdir,
                   const char **reasonout);

/* Loads and saves the editable project copy of one complete BG segment. */
BOOL BgLoadProjectFile(const char *projectdir, const char *bgname,
                       BgFile *out, const char **reasonout);
BOOL BgSaveProjectFile(const char *projectdir, const BgFile *bg,
                       const char **reasonout);
void BgFileFree(BgFile *bg);

/*
 * Loads and parses one background from the project's bg folder. The
 * caller owns the returned vertices and triangle tags, as above.
 */
BgVertex *BgLoadProjectGeometry(const char *projectdir, const char *bgname,
                                float levelscale,
                                DWORD *tricount, unsigned short **tritags,
                                const char **reasonout);

/* Loads the portals from the same saved <projectdir>\bg\*.seg used by
   BgLoadProjectGeometry; no ROM access or duplicate portal asset is
   required when a project is reopened. */
BOOL BgLoadProjectPortals(const char *projectdir, const char *bgname,
                          float levelscale, BgPortalFile *out,
                          const char **reasonout);

void BgPortalFileFree(BgPortalFile *portals);

#endif /* GEDITOR_BGLOAD_H */
