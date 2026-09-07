#ifndef GEDITOR_MODELLOAD_H
#define GEDITOR_MODELLOAD_H

#include <windows.h>

#include "rom.h"
#include "bgload.h"

/*
 * GoldenEye model (P/C/G file) geometry extraction.
 *
 * Parses a raw model file's node tree, collects every mesh node's
 * display lists, and emits the same textured/colored triangle soup
 * bgload produces. Part transforms and skeletons are NOT applied yet:
 * multi-part models (characters especially) come out with each part
 * in its own local space.
 */
BgVertex *ModelLoadGeometry(const unsigned char *data, DWORD maxlen,
                            DWORD *tricount, unsigned short **texids,
                            const char **reasonout);

/* Looks up a setup ObjectRecord's model ID in GoldenEye's canonical
   PitemZ_entries table. The returned name is the extracted P...Z
   resource filename and scale is the model's authored base scale. */
BOOL ModelGetPropDefinition(int modelid, const char **nameout,
                            float *scaleout);

/* Loads one previously extracted object-model glTF. The legacy PLY reader is
   retained so projects created by older GEditor builds still open. Caller
   frees both returned arrays. */
BgVertex *ModelLoadProjectGeometry(const char *projectdir, int modelid,
                                   DWORD *tricount,
                                   unsigned short **tritags,
                                   float *modelscale,
                                   const char **reasonout);

/*
 * Extracts every model in the ROM's file table into
 * <projectdir>\models\{characters,guns,objects,casings}\<name>.gltf.
 * Files are self-contained glTF 2.0 assets with positions, UVs, vertex colors,
 * and GoldenEye texture tags. Returns models written; 0 with *reasonout set
 * on total failure. Unparseable models are skipped.
 */
DWORD ModelExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout);

#endif /* GEDITOR_MODELLOAD_H */
