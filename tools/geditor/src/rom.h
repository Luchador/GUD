#ifndef GEDITOR_ROM_H
#define GEDITOR_ROM_H

#include <windows.h>


#define ROM_MAX_ENTRIES 32

typedef struct RomManifestEntry {
    DWORD kind;      /* fourcc, e.g. 'IMGS'  */
    DWORD romstart;
    DWORD romend;
    DWORD flags;
} RomManifestEntry;

typedef struct RomInfo {
    DWORD size;
    char  internalname[21];      /* header title, NUL-terminated */
    DWORD manifestoffset;
    DWORD manifestversion;
    DWORD entrycount;
    RomManifestEntry entries[ROM_MAX_ENTRIES];
} RomInfo;


/**
 * Full ROM validation. Opens and reads the ROM, checks byte-order and manifest internal inconsistency.
 * On success fills info and returns TRUE. On failure returns FALSE with *reasonout explaining why.
 */
BOOL RomValidate(const char *path, RomInfo *info, const char **reasonout);

#endif