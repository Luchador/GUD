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
    unsigned char r, g, b, a;
} BgVertex;

/*
 * Parses the bg file at data (maxlen readable bytes) and returns a
 * malloc'd array of tricount*3 BgVertex, or NULL with *reasonout set.
 * Caller frees.
 */
BgVertex *BgLoadGeometry(const unsigned char *data, DWORD maxlen,
                         DWORD *tricount, const char **reasonout);

#endif /* GEDITOR_BGLOAD_H */