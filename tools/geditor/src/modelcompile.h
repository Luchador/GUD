#ifndef GEDITOR_MODELCOMPILE_H
#define GEDITOR_MODELCOMPILE_H
#include "modelload.h"
#include "gltf.h"
DWORD ModelDataHash(const unsigned char *data, DWORD size);
/* Paint one native vertex, including every corner sharing its storage.
   Only RGBA bytes change; normals, dynamic effects and material alpha remain
   subject to the same constraints as Blender edits. Result retains size. */
BOOL ModelCompileVertexColor(const unsigned char *data, DWORD size, const ModelSource *source,
    DWORD corner, const unsigned char rgba[4], unsigned char **result, const char **reasonout);
BOOL ModelCompileImport(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
/* 1 = original faces, 0 = topology changed, -1 = allocation failure. */
int ModelImportKeepsTopology(const ModelSource *source, const GltfModelImport *imported);
/* Rebuild rigid, vertex-colored native parts using exported material slots.
 * Retain the model tree/bounds/render state; regenerate per-vertex lookup
 * tables. ordered receives material/UV records in the emitted face order. */
BOOL ModelCompileRetopology(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir, ModelMaterials *ordered,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
/* -1 leaves that property unchanged. Culling: 0=none, 1=back, 2=front.
   Surface: 0=opaque, 1=cutout, 2=alpha blend. IDs index ModelSource.faces. */
BOOL ModelCompileProperties(const unsigned char *data, DWORD size, const ModelSource *source,
    const DWORD *faces, DWORD count, int culling, int surface,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
#endif
