#ifndef GEDITOR_BGLOAD_H
#define GEDITOR_BGLOAD_H

#include <windows.h>

/*
 * GoldenEye background geometry loader.
 *
 * Parses a bg .seg file (uncompressed, as GUD ships them) into a flat
 * triangle soup with vertex colors - the untextured preview the
 * viewport can draw today. Primary and secondary (transparent) room
 * geometry are both included.
 */

typedef struct BgVertex {
    float x, y, z;
    float s, t;                 /* texel coordinates (already /32) */
    unsigned char r, g, b, a;
} BgVertex;

/*
 * Per-triangle tag: low 12 bits are the texture ID (0xFFF = none),
 * bit 12 marks secondary (transparent-layer) geometry. Secondary
 * sorts after primary by plain integer order, which is exactly the
 * draw order blending needs.
 */
#define BG_TEX_ID_MASK   0x0FFF
#define BG_TEX_NONE      0x0FFF
#define BG_TRI_SECONDARY 0x1000

#define BG_TEX_ID(tag)        ((tag) & BG_TEX_ID_MASK)
#define BG_TRI_IS_SECONDARY(tag) (((tag) & BG_TRI_SECONDARY) != 0)

/*
 * Parses the bg file at data (maxlen readable bytes) and returns a
 * malloc'd array of tricount*3 BgVertex, or NULL with *reasonout set.
 * Caller frees.
 *
 * texids receives a malloc'd array of tricount entries - the texture
 * ID each triangle was drawn with, or BG_TEX_NONE. Caller frees.
 */
BgVertex *BgLoadGeometry(const unsigned char *data, DWORD maxlen,
                         DWORD *tricount, unsigned short **texids,
                         const char **reasonout);

#endif /* GEDITOR_BGLOAD_H */