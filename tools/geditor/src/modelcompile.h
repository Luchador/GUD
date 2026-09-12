#ifndef GEDITOR_MODELCOMPILE_H
#define GEDITOR_MODELCOMPILE_H
#include "modelload.h"
#include "gltf.h"
DWORD ModelDataHash(const unsigned char *data, DWORD size);
BOOL ModelCompileImport(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
/* -1 leaves that property unchanged. Culling: 0=none, 1=back, 2=front.
   Surface: 0=opaque, 1=cutout, 2=alpha blend. IDs index ModelSource.faces. */
BOOL ModelCompileProperties(const unsigned char *data, DWORD size, const ModelSource *source,
    const DWORD *faces, DWORD count, int culling, int surface,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
#endif
