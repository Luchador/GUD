#ifndef GEDITOR_SETUPLOAD_H
#define GEDITOR_SETUPLOAD_H

#include <windows.h>

#include "rom.h"

/* One setup binary loaded from a project's setup folder. */
typedef struct SetupFile {
    unsigned char *data;
    DWORD size;
    char name[64];
} SetupFile;

/*
 * Copies every single-player and multiplayer setup resource from the
 * ROM into <projectdir>\setup as <resource name>.set. Returns the
 * number written, or 0 with *reasonout set on failure.
 */
DWORD SetupExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout);

/* Loads <projectdir>\setup\<setupname>.set into out. */
BOOL SetupLoadProjectFile(const char *projectdir, const char *setupname,
                          SetupFile *out, const char **reasonout);

/* Releases a SetupFile and returns it to the empty state. */
void SetupFileFree(SetupFile *setup);

#endif /* GEDITOR_SETUPLOAD_H */
