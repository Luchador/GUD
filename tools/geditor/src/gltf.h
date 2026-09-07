#ifndef GEDITOR_GLTF_H
#define GEDITOR_GLTF_H

#include "bgload.h"

/* Writes a self-contained glTF 2.0 model. Geometry is grouped into one
 * primitive per GoldenEye texture tag; the tag is retained in primitive and
 * material extras so GEditor can reconstruct the original render batches. */
BOOL GltfWriteModel(const char *path, const BgVertex *vertices,
                    const unsigned short *tritags, DWORD tricount,
                    const char **reasonout);

/* Loads triangle primitives from a glTF 2.0 JSON file. The loader accepts
 * embedded or external buffers, indexed or non-indexed primitives, and the
 * standard POSITION, COLOR_0, and TEXCOORD_0 accessor encodings used by DCC
 * tools. Caller frees the returned vertex and tag arrays. */
BgVertex *GltfLoadModel(const char *path, DWORD *tricount,
                        unsigned short **tritags,
                        const char **reasonout);

#endif /* GEDITOR_GLTF_H */
