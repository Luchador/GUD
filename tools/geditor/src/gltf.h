#ifndef GEDITOR_GLTF_H
#define GEDITOR_GLTF_H

#include "bgload.h"
#include "bgrender.h"

/* Writes a self-contained glTF 2.0 model. Adjacent triangles with matching
 * texture tags and preview render flags share a primitive. Ordered material
 * extras preserve native depth, decal, alpha, wrapping and texture generation.
 * Reflective primitives carry NORMAL and _GUD_ENV_SCALE (VEC2, texel ranges)
 * attributes so camera-dependent UVs can be regenerated after reopening.
 * Project image dimensions convert GoldenEye texel UVs to normalized UVs.
 * Embedded PNG images and standard unlit base-color materials make textures
 * available to Blender without the project folder or manual relinking. */
BOOL GltfWriteModel(const char *path, const char *projectdir,
                    const BgVertex *vertices,
                    const unsigned short *tritags,
                    const BgRenderFlags *renderflags, DWORD tricount,
                    const char **reasonout);

struct ModelSource;
BOOL GltfWriteEditableModel(const char *path, const char *projectdir,
                            const struct ModelSource *source, DWORD sourcehash,
                            const char **reasonout);

/* Import source identities through Blender's generic underscore attributes.
 * Positions are transformed through the selected scene; UVs stay normalized. */
typedef struct GltfModelImport {
    BgVertex *vertices;
    unsigned short *tags;
    DWORD *sourcevertices;
    DWORD count;
} GltfModelImport;
BOOL GltfReadModelImport(const char *path, DWORD sourcehash,
                         GltfModelImport *model, const char **reasonout);
void GltfFreeModelImport(GltfModelImport *model);

/* Loads triangle primitives from a glTF 2.0 JSON file. Standard base-color
 * texture samplers override native wrapping extras when present. The loader accepts
 * embedded or external buffers, indexed or non-indexed primitives, and the
 * standard POSITION, COLOR_0, and TEXCOORD_0 accessor encodings used by DCC
 * tools. Normalized UVs are restored to GEditor's texel units using the
 * asset's recorded texture size, falling back to project image dimensions. Caller frees the returned vertex, tag and render-flag arrays. */
BgVertex *GltfLoadModel(const char *path, const char *projectdir,
                        DWORD *tricount,
                        unsigned short **tritags,
                        BgRenderFlags **renderflags,
                        const char **reasonout);

/* Loads an embedded GLB scene, applying its node transforms. */
BgVertex *GltfLoadGlbMesh(const unsigned char *data, DWORD size,
                         DWORD *tricount, const char **reasonout);

/* Lit editor resources require NORMAL. Transformed unit normals are returned
 * in BgVertex.environment.normal. NULL nodename loads the full scene; a name
 * selects only that mesh node, retaining its ancestors' transforms. */
BgVertex *GltfLoadGlbLitMesh(const unsigned char *data, DWORD size, const char *nodename,
                            DWORD *tricount, const char **reasonout);

#endif /* GEDITOR_GLTF_H */
