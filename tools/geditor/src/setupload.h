#ifndef GEDITOR_SETUPLOAD_H
#define GEDITOR_SETUPLOAD_H

#include "rotation.h"
#include <windows.h>

#include "rom.h"

/* Host-native views of the pad data parsed from the big-endian setup. */
typedef struct SetupPad {
    float pos[3];
    float up[3];
    float look[3];
    /* Authored plink tile name. Resolving it must not move the pad itself. */
    char stanname[16];
} SetupPad;

typedef struct SetupBoundPad {
    SetupPad pad;
    float xmin, xmax;
    float ymin, ymax;
    float zmin, zmax;
} SetupBoundPad;

/* Pad tables have independent indices (doors use bound-pad indices directly). */
typedef struct SetupPadRef {
    DWORD index;
    BOOL bound;
} SetupPadRef;
#define SETUP_PAD_INDEX_NONE ((DWORD)-1)

/* The placement fields shared by setup records which create a
   non-character object. Runtime pointers later overwrite much of the
   source record, so GEditor keeps only the authored values it needs. */
typedef struct SetupObject {
    unsigned char type;
    unsigned short extrascale;  /* unsigned 8.8 fixed point */
    short modelid;              /* index into PitemZ_entries */
    short pad;
    DWORD flags;
    DWORD flags2;
    DWORD sourceoffset;         /* byte offset of the complete raw command */
    BOOL deleted;               /* retained command-index tombstone */
    BOOL nonsolid;              /* authored PROPSTATE_NONSOLID */
} SetupObject;

/* GuardRecord uses a different layout from ObjectRecord. Keep its source
   values separate so prop editing cannot overwrite character commands. */
typedef struct SetupCharacter {
    unsigned short chrnum;
    unsigned short pad;
    unsigned short bodyid;
    short headid;              /* negative: choose a head for this body */
    unsigned short ailistid;
    unsigned short flags;
    DWORD sourceoffset;
} SetupCharacter;

/* The viewport shares picking and selection boxes for placed models. The
   high bit distinguishes character indices from the existing prop indices. */
#define SETUP_CHARACTER_SELECTION_BIT 0x80000000u

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
    SetupObject *objects;
    DWORD objectcount;
    SetupCharacter *characters;
    DWORD charactercount;
    BOOL dirty;
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

/* Deep-copies both the raw resource and its editable host-native views. */
BOOL SetupFileClone(const SetupFile *source, SetupFile *out,
                    const char **reasonout);

/* Logically removes an object without changing setup command indices. */
BOOL SetupFileDeleteObject(SetupFile *setup, DWORD objectindex,
                           const char **reasonout);

/* Used within an edit transaction. Accepts a prop index or a character index
   tagged with SETUP_CHARACTER_SELECTION_BIT. Allocates a private pad without
   changing existing command/pad indices. Props gain explicit placement flags;
   characters retain their flags and the game's stan-grounded placement. */
BOOL SetupFileTranslateModel(SetupFile *setup, DWORD selection,
                              float levelscale, const double offset[3],
                              const char **reasonout);

/* Moves the authored pad in place, retaining all references and bounds.
   Invalidates only this pad's stan link so it resolves at its new position. */
BOOL SetupFileTranslatePad(SetupFile *setup, const SetupPadRef *ref,
                            float levelscale, const double offset[3],
                            BOOL *changedout, const char **reasonout);

/* Overwrites the project copy with the raw setup retained in memory. */
BOOL SetupSaveProjectFile(const char *projectdir, const SetupFile *setup,
                          const char **reasonout);

/* Releases a SetupFile and returns it to the empty state. */
void SetupFileFree(SetupFile *setup);

BOOL SetupFileGetModelPad(const SetupFile *setup, DWORD selection, SetupPadRef *ref);
BOOL SetupFilePadRotation(const SetupFile *setup, const SetupPadRef *ref, Rotation *out);
BOOL SetupFileRotatePad(SetupFile *setup, const SetupPadRef *ref, const Rotation *rotation,
                       BOOL *changed, const char **reasonout);

#endif /* GEDITOR_SETUPLOAD_H */
