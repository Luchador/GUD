#ifndef GEDITOR_GLTF_H
#define GEDITOR_GLTF_H

#include "bgload.h"

/* Writes a self-contained glTF 2.0 model. Adjacent triangles with matching
 * texture tags and preview render flags share a primitive. Ordered material
 * extras preserve native depth, decal and alpha behavior in GEditor.
 * Project image dimensions convert GoldenEye texel UVs to normalized UVs. */
BOOL GltfWriteModel(const char *path, const char *projectdir,
                    const BgVertex *vertices,
                    const unsigned short *tritags,
                    const unsigned char *renderflags, DWORD tricount,
                    const char **reasonout);

/* Loads triangle primitives from a glTF 2.0 JSON file. The loader accepts
 * embedded or external buffers, indexed or non-indexed primitives, and the
 * standard POSITION, COLOR_0, and TEXCOORD_0 accessor encodings used by DCC
 * tools. Normalized UVs are restored to GEditor's texel units using project
 * image dimensions. Caller frees the returned vertex, tag and render-flag arrays. */
BgVertex *GltfLoadModel(const char *path, const char *projectdir,
                        DWORD *tricount,
                        unsigned short **tritags,
                        unsigned char **renderflags,
                        const char **reasonout);

/* Loads an embedded GLB scene, applying its node transforms. */
BgVertex *GltfLoadGlbMesh(const unsigned char *data, DWORD size,
                         DWORD *tricount, const char **reasonout);

#endif /* GEDITOR_GLTF_H */
