#ifndef GEDITOR_MODELCOMPILE_H
#define GEDITOR_MODELCOMPILE_H
#include "modelload.h"
#include "gltf.h"
DWORD ModelDataHash(const unsigned char *data, DWORD size);
BOOL ModelCompileImport(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
#endif
