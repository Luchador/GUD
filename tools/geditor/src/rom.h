#ifndef GEDITOR_ROM_H
#define GEDITOR_ROM_H

#include <windows.h>


#define ROM_MAX_ENTRIES 32
#define ROM_MAX_LEVELS  64

typedef struct RomManifestEntry {
    DWORD kind;      /* fourcc, e.g. 'IMGS'  */
    DWORD romstart;
    DWORD romend;
    DWORD flags;
} RomManifestEntry;

/*
 * One row of the ROM's level table, strings resolved and copied out.
 * name/world are display stems derived from Rare's filenames:
 * "UsetupsevbunkerZ" -> "sevbunker", "bg/bg_sev_all_p.seg" -> "sev".
 */
typedef struct RomLevel {
    LONG  levelID;
    char  setupname[32];   /* raw, e.g. "UsetupsevbunkerZ" */
    char  bgname[40];      /* raw, e.g. "bg/bg_sev_all_p.seg" */
    char  stanname[40];    /* raw, e.g. "Tbg_sev_all_p_stanZ" */
    char  name[32];        /* display stem */
    char  world[24];       /* display stem of the shared bg/stan pair */
    float levelscale;
    float renderScale;
    short music;
    short bgsound;
    short xtrack;
} RomLevel;

typedef struct RomInfo {
    DWORD size;
    char  internalname[21];      /* header title, NUL-terminated */
    DWORD manifestoffset;
    DWORD manifestversion;
    DWORD entrycount;
    RomManifestEntry entries[ROM_MAX_ENTRIES];
    DWORD levelcount;
    RomLevel levels[ROM_MAX_LEVELS];
} RomInfo;


/**
 * Full ROM validation. Opens and reads the ROM, checks byte-order and manifest internal inconsistency.
 * On success fills info and returns TRUE. On failure returns FALSE with *reasonout explaining why.
 */
BOOL RomValidate(const char *path, RomInfo *info, const char **reasonout);

#endif