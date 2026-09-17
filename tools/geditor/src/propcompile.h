#ifndef GEDITOR_PROPCOMPILE_H
#define GEDITOR_PROPCOMPILE_H
#include "gltf.h"

/* Static glTF metres -> native millimetres, with model placement scale 0.1.
 * One matrix, vertex colors, opaque/alpha passes, generated collision bounds. */
BOOL PropCompile(const BgVertex *vertices, const unsigned short *tags,
    const BgRenderFlags *flags, DWORD count, const char *projectdir,
    unsigned char **data, DWORD *size, float *radius, const char **reasonout);
#endif
