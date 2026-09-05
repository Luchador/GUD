#ifndef GEDITOR_BGLOAD_H
#define GEDITOR_BGLOAD_H

#include <windows.h>

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

/*
 * Per-triangle tag: low 12 bits are the texture ID (0xFFF = none),
 * bit 12 marks secondary (transparent-layer) geometry, and bit 13
 * records whether Fast3D backface culling was enabled when the
 * triangle was emitted.
 */
#define BG_TEX_ID_MASK   0x0FFF
#define BG_TEX_NONE      0x0FFF
#define BG_TRI_SECONDARY 0x1000
#define BG_TRI_CULL_BACK 0x2000

#define BG_TEX_ID(tag)        ((tag) & BG_TEX_ID_MASK)
#define BG_TRI_IS_SECONDARY(tag) (((tag) & BG_TRI_SECONDARY) != 0)
#define BG_TRI_CULLS_BACK(tag)   (((tag) & BG_TRI_CULL_BACK) != 0)

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

#endif /* GEDITOR_BGLOAD_H */
