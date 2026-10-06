#ifndef GEDITOR_MODELCOMPILE_H
#define GEDITOR_MODELCOMPILE_H
#include "modelload.h"
#include "gltf.h"
DWORD ModelDataHash(const unsigned char *data, DWORD size);
/* New character defaults: set explicit backface culling in every native list.
 * Texture/material edits subsequently retain this authored state. */
BOOL ModelCompileDefaultCulling(const unsigned char *data, DWORD size,
    const ModelSource *source, unsigned char **result, DWORD *resultsize, const char **why);
/* Delete source face IDs and prune unused vertex loads. Keep native vertex
 * data, joints, bounds, collision links and all non-triangle render commands. */
BOOL ModelCompileDeleteFaces(const unsigned char *data, DWORD size, const ModelSource *source,
    const DWORD *faces, DWORD count, unsigned char **result, DWORD *resultsize, const char **reasonout);
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
/* Bind arbitrary static head geometry to a single rigid template part.
 * Retains its attachment, bounds and hit/blood-stain structures, rebuilding
 * vertex links. Material names and normalized UVs remain the imported ones. */
BOOL ModelCompileHeadGeometry(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir, ModelMaterials *ordered,
    unsigned char **result, DWORD *resultsize, const char **why);
/* Bind an unrigged, standing arms-down body to the template's closest-LOD
 * surface joints. Keeps imported topology, seams, UVs and colors. The new body
 * uses its own geometry at every distance; stock low LOD meshes are removed. */
BOOL ModelCompileBodyGeometry(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, BOOL fit, ModelMaterials *ordered,
    unsigned char **result, DWORD *resultsize, const char **why);
/* Repair isolated joint outliers in an existing imported body. Preserve face
 * order, native materials and attributes. No change returns NULL/zero bytes. */
BOOL ModelCompileRepairBodyBindings(const unsigned char *data, DWORD size, const ModelSource *source,
    unsigned char **result, DWORD *resultsize, DWORD *fixed, const char **why);
/* Export compatibility: restore live ownership of detached UV/color copies
 * without changing their joints or attributes. No change returns NULL. */
BOOL ModelCompileBodyBloodVertices(const unsigned char *data, DWORD size, const ModelSource *source,
    unsigned char **result, DWORD *resultsize, const char **why);
/* Translate a rigid imported head, including collision points. Preserve every
 * UV, color, material and display-list byte; positions use native integers. */
BOOL ModelCompileOffsetHead(const unsigned char *data, DWORD size, const ModelSource *source,
    const int delta[3], unsigned char **result, const char **why);
/* -1 leaves that property unchanged. Culling: 0=none, 1=back, 2=front.
   Surface: 0=opaque, 1=cutout, 2=alpha blend. U/V wrap: 0=repeat, 1=clamp,
   2=mirror (textured faces only). IDs index ModelSource.faces. */
BOOL ModelCompileProperties(const unsigned char *data, DWORD size, const ModelSource *source,
    const DWORD *faces, DWORD count, int culling, int surface, int wrapu, int wrapv,
    unsigned char **result, DWORD *resultsize, const char **reasonout);
#endif
