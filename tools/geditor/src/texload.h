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

/*
 * One browser thumbnail: a small top-down BGRA image (GDI-native, ready for StretchDIBits) plus its label
 * (the texture ID, i.e. the file stem).
 */
typedef struct TexThumb {
    char label[16];
    int  w, h;                    /* actual thumb size, <= TEX_THUMB_MAX */
    unsigned int pixeloffset;     /* byte offset into the shared block */
} TexThumb;

#define TEX_THUMB_MAX 32

/*
 * Scans <projectdir>\images for the extracted BMPs and builds
 * thumbnails for all of them, sorted by name. On success returns the
 * count and hands out two allocations the CALLER owns: the item array
 * and one shared pixel block the items' offsets point into. Returns 0
 * with *reasonout set when the folder is missing or empty.
 */
DWORD TexLoadProjectThumbnails(const char *projectdir, TexThumb **items,
                               unsigned char **pixelblock,
                               const char **reasonout);

#endif /* GEDITOR_TEXLOAD_H */