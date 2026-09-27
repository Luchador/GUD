#ifndef GEDITOR_STUDIOASSETS_H
#define GEDITOR_STUDIOASSETS_H
#include <windows.h>
#include "texload.h"

typedef struct StudioFileEntry { char filename[MAX_PATH]; } StudioFileEntry;

/* Read one studio subfolder, filtering regular files by extension without case
 * sensitivity. Names are sorted; the caller frees entries. No partial results. */
BOOL StudioFileList(const char *projectdir, const char *folder, const char *extension,
    StudioFileEntry **entries, DWORD *count, const char **why);

/* BrowserSetImages-compatible ownership. Unreadable BMPs retain their filenames
 * with an empty preview, and why reports the problem without hiding other files. */
BOOL StudioLoadImages(const char *projectdir, TexThumb **items,
    unsigned char **pixels, DWORD *count, const char **why);

#endif
