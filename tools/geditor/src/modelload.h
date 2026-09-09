#ifndef GEDITOR_MODELLOAD_H
#define GEDITOR_MODELLOAD_H

#include <windows.h>

#include "rom.h"
#include "bgload.h"
#include "bgrender.h"

/*
 * GoldenEye model (P/C/G file) geometry extraction.
 *
 * Parses a raw model file's node tree, collects every mesh node's
 * display lists, and emits the same textured/colored triangle soup
 * bgload produces. The unanimated pose accumulates parent node offsets
 * and applies display-list matrix selection when vertices are loaded.
 * Positions are baked relative to the root; setup placement/scale is separate.
 * Runtime joint animation and attachment to other models are not evaluated.
 */
BgVertex *ModelLoadGeometry(const unsigned char *data, DWORD maxlen,
                            DWORD *tricount, unsigned short **texids,
                            BgRenderFlags **renderflags,
                            const char **reasonout);

/* Character exports retain only the closest LOD of each body part. */
BgVertex *ModelLoadCharacterGeometry(const unsigned char *data, DWORD maxlen,
                                     DWORD *tricount, unsigned short **texids,
                                     BgRenderFlags **renderflags,
                                     const char **reasonout);

/* Model-space position of MODELNODE_OPCODE_HEAD in the unanimated body. */
BOOL ModelReadHeadAttachment(const unsigned char *data, DWORD size,
                             float position[3]);

/* Switch-table node position in the unanimated model. The switch count
   comes from its static ModelFileHeader, not the raw model file. */
BOOL ModelReadSwitchAttachment(const unsigned char *data, DWORD size,
                                int switchcount, int index, float position[3]);

/* Held models use their authored root offset, which flattened project
   geometry omits for ordinary object placement. GROUP roots inherit the
   hand's scale directly; only HEADER roots also apply their own model scale. */
BOOL ModelReadHeldPlacement(const unsigned char *data, DWORD size,
                             float origin[3], BOOL *usesmodelscale);

/* Looks up a setup ObjectRecord's model ID in GoldenEye's canonical
   PitemZ_entries table. The returned name is the extracted P...Z
   resource filename and scale is the model's authored base scale. */
BOOL ModelGetPropDefinition(int modelid, const char **nameout,
                            float *scaleout);

/* Read the authored collision/placement box selected by
   chrobjGetBboxFromObjFile. It can differ from the visible mesh bounds. */
BOOL ModelReadPlacementBounds(const unsigned char *data, DWORD size,
                               float min[3], float max[3]);

/* Loads one previously extracted object-model glTF. The legacy PLY reader is
   retained so projects created by older GEditor builds still open. Caller
   frees the returned vertex, tag and preview render-flag arrays. */
BgVertex *ModelLoadProjectGeometry(const char *projectdir, int modelid,
                                   DWORD *tricount,
                                   unsigned short **tritags,
                                   BgRenderFlags **renderflags,
                                   float *modelscale,
                                   const char **reasonout);

/*
 * Extracts every model in the ROM's file table into
 * <projectdir>\models\{characters,guns,objects,casings}\<name>.gltf.
 * Files are self-contained glTF 2.0 assets with positions, UVs, vertex colors,
 * and GoldenEye texture/depth/blending metadata. Returns models written; 0 with *reasonout set
 * on total failure. Unparseable models are skipped.
 */
DWORD ModelExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout);

#endif /* GEDITOR_MODELLOAD_H */
