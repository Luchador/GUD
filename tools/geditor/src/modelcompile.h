#ifndef GEDITOR_MODELCOMPILE_H
#define GEDITOR_MODELCOMPILE_H
#include "modelload.h"
#include "gltf.h"
DWORD ModelDataHash(const unsigned char *data, DWORD size);
/* Give shared leaf meshes independent near/far branches, vertex buffers and
 * material slots. Keep native joints, collision links and existing offsets.
 * A successful no-op returns NULL, zero bytes and zero separated faces. */
BOOL ModelCompileSeparateLods(const unsigned char *data, DWORD size, const ModelSource *source,
    unsigned char **result, DWORD *resultsize, DWORD *separated, const char **reasonout);
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
   Surface: 0=opaque, 1=cutout, 2=alpha blend. U/V wrap: 0=repeat, 1=clamp,
   2=mirror (textured faces only). IDs index ModelSource.faces. */
BOOL ModelCompileProperties(const unsigned char *data, DWORD size, const ModelSource *source,
    const DWORD *faces, DWORD count, int culling, int surface, int wrapu, int wrapv,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
#endif
