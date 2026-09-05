#ifndef GEDITOR_SETUPLOAD_H
#define GEDITOR_SETUPLOAD_H

#include <windows.h>

#include "rom.h"

/* Host-native views of the pad data parsed from the big-endian setup. */
typedef struct SetupPad {
    float pos[3];
    float up[3];
    float look[3];
} SetupPad;

typedef struct SetupBoundPad {
    SetupPad pad;
    float xmin, xmax;
    float ymin, ymax;
    float zmin, zmax;
} SetupBoundPad;

/* Builds an oriented box in gameplay world coordinates. The local
   axes are the same ones GoldenEye uses for bound-pad volume tests. */
void SetupPadGetBoxCorners(const SetupPad *pad,
                           float xmin, float xmax,
                           float ymin, float ymax,
                           float zmin, float zmax,
                           float worldscale,
                           float corners[8][3]);

/* One setup binary loaded from a project's setup folder, plus the pad
   sections decoded into host-native data for editor tools. */
typedef struct SetupFile {
    unsigned char *data;
    DWORD size;
    char name[64];
    SetupPad *pads;
    DWORD padcount;
    SetupBoundPad *boundpads;
    DWORD boundpadcount;
} SetupFile;

/*
 * Copies every single-player and multiplayer setup resource from the
 * ROM into <projectdir>\setup as <resource name>.set. Returns the
 * number written, or 0 with *reasonout set on failure.
 */
DWORD SetupExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout);

/* Loads <projectdir>\setup\<setupname>.set and parses both pad lists. */
BOOL SetupLoadProjectFile(const char *projectdir, const char *setupname,
                          SetupFile *out, const char **reasonout);

/* Releases a SetupFile and returns it to the empty state. */
void SetupFileFree(SetupFile *setup);

#endif /* GEDITOR_SETUPLOAD_H */
