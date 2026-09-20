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

/* Preview-only inputs for N64 normal-based texture generation. The normal is
 * in the same space as the vertex; scale is the generated S/T range in texels. */
typedef struct BgEnvironmentVertex {
    float normal[3];
    float scale[2];
} BgEnvironmentVertex;

typedef struct BgVertex {
    float x, y, z;
    float s, t;                 /* texel coordinates (already /32) */
    unsigned char r, g, b, a;
    BgEnvironmentVertex environment;
} BgVertex;

#define BG_MAX_PORTALS 200

/* Raw project background retained while a level is open. The compiler uses
   it to preserve the header, portals, visibility data, and other data which
   sits outside editable room geometry. */
typedef struct BgFile {
    unsigned char *data;
    DWORD size;
    char name[64];
    /* Session-only addresses for editor-created polygons. Keep retired slots
     * for undo after saving; reloading makes all saved polygons native IDs. */
    DWORD newportaloffsets[BG_MAX_PORTALS];
} BgFile;

#define BG_PORTAL_MAX_POINTS 8
#define BG_PORTAL_INDEX_NONE ((DWORD)-1)
/* Unsaved editor polygons use identities outside the native 24-bit offsets. */
#define BG_PORTAL_NEW_GEOMETRY 0x80000000u

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
    /* Exact native floats also live in history: undo after saving must restore
     * the original bits, even when world/level scaling is not reversible. */
    BgPortalPoint nativepoints[BG_PORTAL_MAX_POINTS];
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
/* Compact unreferenced vertices in a saved copy, remapping G_VTX and triangle
 * indices together. Empty rooms retain their bounds-only vertex records.
 * On success out->data is NULL when no changes are needed. Source is immutable. */
BOOL BgFileRemoveUnusedVertices(const BgFile *source, BgFile *out,
                                const char **reasonout);
void BgFileFree(BgFile *bg);
/* Serialize only reachable metadata and room streams. Output offsets are new;
 * use the copy for disk/ROM, never as the source for a live undo history. */
BOOL BgFileCompact(const BgFile *source, BgFile *out, const char **reasonout);

/* Same native BG-unit margin used by bgGetPortalMargin in the game. */
float BgPortalGetMargin(const BgPortal *portal);
/* Quantize a finite, nonnegative native BG-unit margin to the closest byte
 * encoding. Equidistant values round upward. Reject values above 122880. */
BOOL BgPortalEncodeMargin(double margin, unsigned char *encoded);
void BgPortalFileFree(BgPortalFile *portals);

#endif /* GEDITOR_BGLOAD_H */
