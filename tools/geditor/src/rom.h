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
/*
 * A loaded ROM: the raw bytes plus the parsed manifest info. Owns the
 * data buffer; release with RomFree.
 */
typedef struct RomFile {
    unsigned char *data;
    DWORD size;
    RomInfo info;
} RomFile;

/*
 * Loads and validates in one step, keeping the buffer for asset
 * access. TRUE on success; FALSE with *reasonout set otherwise.
 */
BOOL RomLoad(const char *path, RomFile *rom, const char **reasonout);
void RomFree(RomFile *rom);

/*
 * Looks a file up by its resource name ("bg/bg_sev_all_p.seg") in the
 * ROM's file table. On success *offset is the file's position in the
 * ROM and *maxlen the readable bytes from there (bounded by the obseg
 * segment). FALSE with *reasonout set when the table is missing or
 * the name is not present.
 */
BOOL RomFindFile(const RomFile *rom, const char *name,
                 DWORD *offset, DWORD *maxlen, const char **reasonout);

BOOL RomValidate(const char *path, RomInfo *info, const char **reasonout);

#endif