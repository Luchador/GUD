#ifndef GEDITOR_SETUPLOAD_H
#define GEDITOR_SETUPLOAD_H

#include "rotation.h"
#include "scaling.h"
#include <windows.h>

#include "rom.h"
#include "../../../src/ammoconstants.h"
#include "../../../src/doorconstants.h"

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
#define SETUP_PAD_HALF_SIZE 5.0f /* ordinary-pad preview half extent, world units */
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

/* Shared inspector values are decoded from the authored record on demand.
 * Health is ObjectRecord.damage (signed 16.16 in the setup), not the runtime
 * damage accumulator named maxdamage. Specialized fields can add property IDs
 * and validated setters without exposing raw offsets to panel controls. */
typedef enum SetupObjectProperty {
    SETUP_OBJECT_HEALTH, SETUP_OBJECT_MODEL, SETUP_OBJECT_KEY_FLAGS,
    SETUP_OBJECT_AMMO_TYPE, SETUP_OBJECT_AMMO_QUANTITY,
    SETUP_OBJECT_DOOR_TRAVEL, SETUP_OBJECT_DOOR_CLEARANCE,
    SETUP_OBJECT_DOOR_ACCEL, SETUP_OBJECT_DOOR_DECEL, SETUP_OBJECT_DOOR_SPEED,
    SETUP_OBJECT_DOOR_CLOSE_DELAY, SETUP_OBJECT_DOOR_TYPE,
    SETUP_OBJECT_DOOR_SOUND, SETUP_OBJECT_DOOR_FLAGS, SETUP_OBJECT_DOOR_KEY_FLAGS,
    SETUP_OBJECT_CCTV_LOOK_PAD, SETUP_OBJECT_CCTV_SWEEP_MIN,
    SETUP_OBJECT_CCTV_SWEEP_MAX, SETUP_OBJECT_CCTV_SPEED, SETUP_OBJECT_CCTV_RANGE
} SetupObjectProperty;
/* Movement uses native fractions/degrees/animation units per 60 Hz tick.
 * The inspector converts these to percentages/degrees and seconds. */
typedef struct SetupDoorProperties {
    double travel, clearance, accel, decel, speed;
    DWORD closeframes, sound;
    unsigned short type, flags;
} SetupDoorProperties;
/* CCTV angles are degrees relative to the look-at pad; speed is degrees/sec.
 * Range is in game world units, with any native nonpositive value shown as 0
 * (unlimited). The look-at pad is separate from the object's placement pad. */
typedef struct SetupCctvProperties {
    LONG lookpad;
    double sweepmin, sweepmax, speed;
    DWORD range;
} SetupCctvProperties;
typedef struct SetupObjectProperties {
    SetupObject object;
    double health;
    DWORD keyflags, ammotype; /* keyflags: supplied by a key, required by a door */
    SetupDoorProperties door;
    SetupCctvProperties cctv;
    struct { unsigned short model, quantity; } ammo[AMMOTYPE_GLOBAL_MAX];
} SetupObjectProperties;
typedef struct SetupObjectPropertyEdit {
    DWORD objectindex, sourceoffset;
    unsigned char type;
    SetupObjectProperty property;
    double value;
    DWORD slot; /* Zero-based multi-ammo slot; ignored by other properties. */
} SetupObjectPropertyEdit;

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
    BOOL deleted;             /* retained command-index tombstone */
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

const char *SetupObjectTypeName(unsigned char type);
BOOL SetupFileGetObjectProperties(const SetupFile *setup, DWORD index,
                                  SetupObjectProperties *out, const char **reasonout);
/* Validates before mutation, preserves all unrelated bytes, reports no-ops.
 * The frame validates model asset availability before committing the edit. */
BOOL SetupFileSetObjectProperty(SetupFile *setup, const SetupObjectPropertyEdit *edit,
                                BOOL *changedout, const char **reasonout);

/* Resolve an object-relative native command reference (not an object-array index). */
BOOL SetupObjectRelativeTarget(const SetupFile *setup, DWORD sourceoffset, LONG relative, DWORD *objectindex);

/* Derived viewport markers. These are never serialized into setup records.
 * Cameras already store gameplay coordinates; spawn pads use levelscale. */
typedef enum SetupMarkerKind {
    SETUP_MARKER_SPAWN, SETUP_MARKER_INTRO, SETUP_MARKER_OUTRO,
    SETUP_MARKER_SWIRL, SETUP_MARKER_KIND_COUNT
} SetupMarkerKind;
typedef struct SetupMarkerRef {
    SetupMarkerKind kind;
    DWORD command; /* index in intro or propDefs; reselect after removing commands */
} SetupMarkerRef;
/* Normal-play starts only. Solo placement replaces its start and preserves
 * heading; multiplayer placement appends (the game supports up to 16).
 * Deletion refuses the last start. Both operations retain other pad indices
 * and leave the setup unchanged on failure. */
BOOL SetupFilePlaceSpawn(SetupFile *setup, float levelscale, const double position[3],
                        SetupMarkerRef *out, const char **reasonout);
BOOL SetupFileDeleteSpawn(SetupFile *setup, const SetupMarkerRef *ref, const char **reasonout);
/* Single-player cameras. Intro placement appends a random-intro candidate;
 * outro placement replaces all scripted outro shots with one editable camera.
 * Existing prop command indices/tags are retained. look is a world direction. */
BOOL SetupFilePlaceCamera(SetupFile *setup, SetupMarkerKind kind, float levelscale,
                         const double position[3], const double look[3],
                         SetupMarkerRef *out, const char **reasonout);
BOOL SetupFileDeleteIntroCamera(SetupFile *setup, const SetupMarkerRef *ref, const char **reasonout);
typedef struct SetupMarker {
    SetupMarkerKind kind;
    DWORD command;
    DWORD pad; /* ordinary spawn pad; camera pads are room references only */
    float position[3], look[3], up[3];
} SetupMarker;
BOOL SetupFileBuildMarkers(const SetupFile *setup, float levelscale,
                           SetupMarker **markers, DWORD *count, const char **reasonout);

typedef struct SetupSwirlPoint {
    DWORD command;
    float position[3];
    float look[3], up[3]; /* model +X follows the spline; +Y stays upright */
    float tangentscale;
} SetupSwirlPoint;
typedef struct SetupSwirlPath {
    SetupSwirlPoint *points; /* includes the first/last tangent controls, not the terminator */
    DWORD pointcount;
    float (*curve)[3];      /* sampled camera travel from point 1 to pointcount - 2 */
    DWORD curvecount;
} SetupSwirlPath;
/* spawn is the solo marker after grounding on the stan floor. Offsets are
 * relative to Bond's default standing eye position, without level scaling. */
BOOL SetupFileBuildSwirlPath(const SetupFile *setup, const SetupMarker *spawn,
                            SetupSwirlPath *path, const char **reasonout);
void SetupSwirlPathFree(SetupSwirlPath *path);
/* Exactly one of offset/rotation is supplied. spawn is the grounded solo
 * anchor used to display swirl points. Changes update native setup bytes. */
BOOL SetupFileTransformMarker(SetupFile *setup, const SetupMarkerRef *ref,
                             const SetupMarker *spawn, float levelscale,
                             const double offset[3], const Rotation *rotation,
                             BOOL *changed, const char **reasonout);

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
/* Edit one bit in the object's native PROPFLAG (bank 0) or PROPFLAG2 (bank 1)
   word. Aliases share a bit. No other setup fields or indices are rewritten. */
BOOL SetupFileSetObjectFlag(SetupFile *setup, DWORD objectindex,
                            unsigned int bank, DWORD mask, BOOL enabled,
                            BOOL *changedout, const char **reasonout);
BOOL SetupFileDeleteCharacter(SetupFile *setup, DWORD characterindex,
                              const char **reasonout);

/* Appends an ordinary prop or an unarmed, idle character and its private pad.
   Position is in world units; placement follows the normal stan-grounded rules.
   Existing command indices, pad indices and file-relative links are retained.
   On failure the setup is unchanged. Returns the new viewport selection ID. */
BOOL SetupFileAddModel(SetupFile *setup, BOOL character, int modelid,
                      float levelscale, const double position[3],
                      DWORD *selectionout, const char **reasonout);

#define SETUP_DEFAULT_DOOR_MODEL "Psteel_door1Z"
/* Add an unlocked slider and its private bound pad, preserving all existing
 * command/pad indices. position is the floor anchor in gameplay world units;
 * facing is the viewing direction. The upright door faces back towards the
 * viewer. Default size: 100 wide, 200 high, 12 deep, independent of levelscale. */
BOOL SetupFileAddDoor(SetupFile *setup, int modelid, float levelscale,
                      const double position[3], const double facing[3],
                      DWORD *selectionout, const char **reasonout);

#define SETUP_DEFAULT_GLASS_MODEL "PwindowZ"
/* Add a breakable free-standing pane with a private bound pad. Floor anchor
 * and facing use the same convention as AddDoor. Default size: 100 wide,
 * 200 high, zero thickness. Supports solo and multiplayer setups. */
BOOL SetupFileAddGlass(SetupFile *setup, int modelid, float levelscale,
                       const double position[3], const double facing[3],
                       DWORD *selectionout, const char **reasonout);


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

/* Transactions own rollback when promotion or geometry rebuilding fails. */
BOOL SetupFileScalePad(SetupFile *setup, SetupPadRef *ref, float levelscale, const Scaling *scale,
                       const char **reasonout);
BOOL SetupFileSetModelBounds(SetupFile *setup, DWORD selection, float levelscale,
                             const double bounds[6], const char **reasonout);

#endif /* GEDITOR_SETUPLOAD_H */
