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

/*
 * Extracts every model in the ROM's file table into
 * <projectdir>\models\{characters,guns,objects,casings}\<name>.ply -
 * PLY chosen for native vertex colors. Returns models written; 0 with
 * *reasonout set on total failure. Unparseable models are skipped.
 */
DWORD ModelExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout);

#endif /* GEDITOR_MODELLOAD_H */