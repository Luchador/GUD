#ifndef GEDITOR_TEXLOAD_H
#define GEDITOR_TEXLOAD_H

#include <windows.h>

#include "rom.h"

/*
 * GUTX texture extraction.
 *
 * The ROM's IMGS segment is a run of GUTX records - GUD's raw texture
 * container - one per game texture ID, in ID order. Each record holds
 * up to 7 mip images plus an optional palette; image 0 is full size.
 *
 * TexExtractImages decodes every record's image 0 to RGBA and writes
 * <projectdir>\images\<ID hex>.bmp. Returns the number written, or 0
 * with *reasonout set. Individual undecodable records are skipped,
 * not fatal.
 */
DWORD TexExtractImages(const RomFile *rom, const char *projectdir,
                       const char **reasonout);

#endif /* GEDITOR_TEXLOAD_H */