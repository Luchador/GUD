#ifndef GEDITOR_SETUPLOAD_H
#define GEDITOR_SETUPLOAD_H

#include "rotation.h"
#include "scaling.h"
#include <windows.h>

#include "rom.h"
#include "../../../src/ammoconstants.h"
#include "../../../src/doorconstants.h"
#include "../../../src/occluderformat.h"
#include "../../../src/objectfadeformat.h"

/* Host-native views of the pad data parsed from the big-endian setup. */
typedef struct SetupPad {
    float pos[3];
    float up[3];
    float look[3];
    /* Authored plink tile name. Resolving it must not move the pad itself. */
    char stanname[16];
    BOOL deleted; /* Stable-index tombstone; hidden by the editor. */
    BOOL occluder; /* Dedicated render-only bound pad, never floor-snapped. */
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
    SETUP_OBJECT_CCTV_SWEEP_MAX, SETUP_OBJECT_CCTV_SPEED, SETUP_OBJECT_CCTV_RANGE,
    SETUP_OBJECT_DRONE_AIM_PAD, SETUP_OBJECT_DRONE_YAW_MIN,
    SETUP_OBJECT_DRONE_YAW_MAX, SETUP_OBJECT_DRONE_SPEED, SETUP_OBJECT_DRONE_RANGE,
    SETUP_OBJECT_ARMOR_STRENGTH, SETUP_OBJECT_FADE_DISTANCES
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
/* Effective yaw limits in degrees, tracking speed in degrees/sec, range in
 * world units. Stock +/-720-degree limits mean unrestricted yaw; present
 * those as +/-180 while preserving their original words on unchanged edits.
 * Unlike CCTV, zero range is NOT unlimited. Negative aim pads use +Z, level. */
typedef struct SetupDroneProperties {
    LONG aimpad;
    double yawmin, yawmax, speed, range;
} SetupDroneProperties;
typedef struct SetupObjectProperties {
    SetupObject object;
    double health;
    BOOL customfade;
    double fadestart, fadeend; /* Camera distances in metres; native values use centimetres. */
    double armorstrength; /* Percentage, decoded from BodyArmourRecord.initialamount. */
    DWORD keyflags, ammotype; /* keyflags: supplied by a key, required by a door */
    SetupDoorProperties door;
    SetupCctvProperties cctv;
    SetupDroneProperties drone;
    struct { unsigned short model, quantity; } ammo[AMMOTYPE_GLOBAL_MAX];
} SetupObjectProperties;
typedef struct SetupObjectPropertyEdit {
    DWORD objectindex, sourceoffset;
    unsigned char type;
    SetupObjectProperty property;
    double value;
    double value2; /* Fade end distance for SETUP_OBJECT_FADE_DISTANCES; otherwise ignored. */
    DWORD slot; /* Zero-based multi-ammo slot; ignored by other properties. */
} SetupObjectPropertyEdit;

/* Validate tagged object distances and destination runtime support on export
 * and rebase. Untagged setup records remain compatible with older runtimes. */
BOOL SetupValidateObjectFadeNative(const unsigned char *data, DWORD size,
                                 const RomFile *rom, const char **reasonout);

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
    unsigned char *actionmeta; /* Project-only Action Block names and notes. */
    DWORD actionmetasize;
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
    /* Immutable shared-script reference catalog, shared by undo snapshots.
     * Rebuilt from the project's base ROM; never serialized. */
    struct SetupScriptReferences *globalrefs;
} SetupFile;

/* Normal-play starting equipment. Demo-specific intro records stay untouched.
 * Weapon values are right/left item IDs (-1 means no left weapon); ammo values
 * are type/quantity. The first weapon record is equipped on level entry. */
enum { SETUP_INTRO_WEAPON = 1, SETUP_INTRO_AMMO = 2 };
typedef struct SetupIntroEntry { DWORD command, type; LONG value[2]; } SetupIntroEntry;
typedef enum SetupIntroAction {
    SETUP_INTRO_ADD, SETUP_INTRO_UPDATE, SETUP_INTRO_REMOVE,
    SETUP_INTRO_UP, SETUP_INTRO_DOWN
} SetupIntroAction;
typedef struct SetupIntroEdit { SetupIntroAction action; SetupIntroEntry entry; } SetupIntroEdit;
typedef struct SetupIntroChoice { int id; const char *name; } SetupIntroChoice;
const SetupIntroChoice *SetupIntroItemChoices(DWORD *count);
const char *SetupIntroItemName(int item);
/* Caller frees the returned array. Commands are stable until a row is removed. */
BOOL SetupFileGetIntroEquipment(const SetupFile *setup, SetupIntroEntry **entries,
    DWORD *count, const char **reasonout);
/* Atomic, compacted native edits. Moving a row swaps same-type normal-play
 * entries only, preserving cameras, spawns, demo equipment and other commands. */
BOOL SetupFileEditIntroEquipment(SetupFile *setup, const SetupIntroEdit *edit,
    BOOL *changedout, const char **reasonout);

/* Starting visible hand equipment. Right is 0, left is 1. None is -1;
 * -2 denotes differing item types among authored difficulty variants. */
#define SETUP_WEAPON_NONE (-1)
#define SETUP_WEAPON_MIXED (-2)
typedef struct SetupWeaponChoice { int item, model; const char *name; } SetupWeaponChoice;
typedef struct SetupCharacterWeapons { int item[2]; DWORD count[2]; } SetupCharacterWeapons;
typedef struct SetupCharacterWeaponEdit {
    DWORD characterindex, sourceoffset;
    unsigned short chrnum;
    int hand, item;
} SetupCharacterWeaponEdit;
const SetupWeaponChoice *SetupWeaponChoices(DWORD *count);
const SetupWeaponChoice *SetupWeaponChoiceForItem(int item);
BOOL SetupFileGetCharacterWeapons(const SetupFile *setup, DWORD index, SetupCharacterWeapons *out);
void SetupFileGetCharacterHeldWeapons(const SetupFile *setup, DWORD index, const SetupObject *held[2]);
/* Atomic; existing variants retain flags, scale and command identities.
 * None disables all visible records for that hand. Concealed inventory is untouched. */
BOOL SetupFileSetCharacterWeapon(SetupFile *setup, const SetupCharacterWeaponEdit *edit,
    BOOL *changedout, const char **reasonout);

#define SETUP_HAT_NONE (-1)
#define SETUP_HAT_MIXED (-2)
typedef struct SetupHatChoice { int model, fitting; const char *name; } SetupHatChoice;
typedef struct SetupCharacterHat { int model; DWORD count; } SetupCharacterHat;
typedef struct SetupCharacterHatEdit {
    DWORD characterindex, sourceoffset;
    unsigned short chrnum;
    int model;
} SetupCharacterHatEdit;
const SetupHatChoice *SetupHatChoices(DWORD *count);
const SetupHatChoice *SetupHatChoiceForModel(int model);
BOOL SetupFileGetCharacterHat(const SetupFile *setup, DWORD index, SetupCharacterHat *out);
const SetupObject *SetupFileGetCharacterWornHat(const SetupFile *setup, DWORD index);
/* Atomic; No hat disables the owner's active hat variants. Restoring a hat
 * reuses those records and preserves their scale, flags and exclusions. */
BOOL SetupFileSetCharacterHat(SetupFile *setup, const SetupCharacterHatEdit *edit,
    BOOL *changedout, const char **reasonout);

/* Standalone shared AI entry points (GAILISTID in bondaicommands.h).
 * Other shared scripts may be subroutines requiring a return list. */
enum { SETUP_BEHAVIOR_DO_NOTHING = 1, SETUP_BEHAVIOR_STANDARD_GUARD = 2 };
typedef struct SetupBehaviorChoice { int id; const char *name; } SetupBehaviorChoice;
typedef struct SetupCharacterBehaviorEdit {
    DWORD characterindex, sourceoffset;
    unsigned short chrnum, previous;
    int ailistid;
} SetupCharacterBehaviorEdit;
const SetupBehaviorChoice *SetupCharacterBehaviorChoices(DWORD *count);
const SetupBehaviorChoice *SetupCharacterBehaviorChoiceForId(int id);
/* Changes only the guard's starting AI assignment; rejects stale edits.
 * Existing custom assignments are preserved until explicitly changed. */
BOOL SetupFileSetCharacterBehavior(SetupFile *setup, const SetupCharacterBehaviorEdit *edit,
    BOOL *changedout, const char **reasonout);

/* Discard unreachable tables and relocate native pointers in a separate copy.
 * Clear negligible pad-direction residues which can trap on the R4300. */
BOOL SetupCompactNative(const unsigned char *data, DWORD size,
    unsigned char **out, DWORD *sizeout, const char **reasonout);
/* Transaction boundary: also updates decoded command offsets and directions, atomically.
 * Indices, selections, notes and dirty state are unchanged. */
BOOL SetupFileCompact(SetupFile *setup, const char **reasonout);
/* Missing optional catalogs leave globalrefs NULL and disable pad/character
 * recycling. Invalid catalogs fail without replacing an existing cache. */
BOOL SetupFileSetGlobalReferences(SetupFile *setup, const RomFile *rom, const char **reasonout);
/* References from the project's shared AI catalog, when available. */
BOOL SetupFileGlobalBlockReference(const SetupFile *setup, DWORD id);
BOOL SetupFileGlobalPatrolReference(const SetupFile *setup, DWORD id);

/* Requires a base ROM with a shared Action Block catalog so every script
 * can be checked. Preserves table indices and makes no changes on failure. */
BOOL SetupFileDeletePad(SetupFile *setup, const SetupPadRef *ref,
                       const RomFile *rom, const char **reasonout);

/* Derived patrol connections between ordinary pads. Canonical pad order lets
 * shared routes draw once; direction bits are 1 = pads[0] -> pads[1], 2 = reverse. */
typedef struct SetupPatrolLink {
    DWORD pads[2];
    unsigned int directions;
} SetupPatrolLink;
/* Optional pathpads has setup->padcount bytes; includes single-pad routes. */
BOOL SetupFileBuildPatrolLinks(const SetupFile *setup, SetupPatrolLink **links,
                               DWORD *count, unsigned char *pathpads, const char **reasonout);

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

/* Dedicated bound pad; initial box is 100 world units per side. */
BOOL SetupFileAddOccluder(SetupFile *setup, float levelscale, const double position[3],
    SetupPadRef *out, const char **reasonout);
/* Standalone ordinary pad, owned by the user (never recycled as a private
 * object pad). Atomic and compacted; position is in gameplay world units. */
BOOL SetupFileAddPad(SetupFile *setup, float levelscale, const double position[3],
    const char *stanname, SetupPadRef *out, const char **reasonout);

/* Duplicate a placed prop or tagged character from a same-level snapshot.
 * Existing command/pad IDs remain stable; the copy owns its placement and optional look-at pad.
 * Characters get a fresh ID and copies of their weapons, hats and attributes.
 * Tags, links and shared action lists are not copied. Atomic. */
BOOL SetupFileCanDuplicateObject(const SetupFile *setup, DWORD index);
BOOL SetupFileDuplicateObject(SetupFile *setup, const SetupFile *source,
    DWORD index, DWORD *selectionout, const char **reasonout);

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

/* Appends an ordinary prop or an unarmed standard guard and its private pad.
   Position is in world units; placement follows the normal stan-grounded rules.
   Existing command indices, pad indices and file-relative links are retained.
   On failure the setup is unchanged. Returns the new viewport selection ID. */
BOOL SetupFileAddModel(SetupFile *setup, BOOL character, int modelid,
                      float levelscale, const double position[3],
                      DWORD *selectionout, const char **reasonout);

/* Create an ordinary prop or native door fitted to an existing bound pad. The pad's
 * index, placement, bounds and other references are retained. Atomic; no new
 * pad is allocated. Occupied pads and render-only occluders are rejected. */
BOOL SetupFileCanAddBoundPadModel(const SetupFile *setup, DWORD padindex, BOOL door,
                                 const char **reasonout);
BOOL SetupFileAddBoundPadModel(SetupFile *setup, DWORD padindex, int modelid, BOOL door,
                              DWORD *selectionout, const char **reasonout);

#define SETUP_DEFAULT_ARMOR_MODEL "PbodyarmourZ"
/* Add a full-strength armor pickup at the stock 1.5x model scale, with a
 * private normal pad. Uses normal floor/support placement and can fall if
 * its support is destroyed. Available on all difficulties and in multiplayer. */
BOOL SetupFileAddArmor(SetupFile *setup, int modelid, float levelscale,
                       const double position[3], DWORD *selectionout,
                       const char **reasonout);

#define SETUP_DEFAULT_TANK_MODEL "PtankZ"
/* Add a driveable tank at stock scale with 30 shells and a private normal
 * pad. Upright, floor grounded, facing the viewer. Multiple tanks are valid
 * in solo; multiplayer driving uses shared global state and is unsupported. */
BOOL SetupFileAddTank(SetupFile *setup, int modelid, float levelscale,
                      const double position[3], const double facing[3],
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

#define SETUP_DEFAULT_CCTV_MODEL "PcctvZ"
#define SETUP_DEFAULT_ALARM_MODEL "Palarm2Z"
#define SETUP_DEFAULT_DRONE_MODEL "PgroundgunZ"
/* Mounted objects keep the drop height, with their back at position and their
 * front facing the viewer (facing is the viewing direction). Each gets a
 * private normal pad; CCTV also gets a look-at pad 200 world units in front.
 * These append complete native records and support solo/MP, save and history. */
BOOL SetupFileAddCctv(SetupFile *setup, int modelid, float levelscale,
                      const double position[3], const double facing[3],
                      DWORD *selectionout, const char **reasonout);
BOOL SetupFileAddAlarm(SetupFile *setup, int modelid, float levelscale,
                       const double position[3], const double facing[3],
                       DWORD *selectionout, const char **reasonout);
/* Upright drone gun with its base at the drop point and a private aim pad
 * 200 world units towards the viewer. Starts enabled, with full yaw travel,
 * about 90 degrees/sec tracking and a 2000-world-unit acquisition range. */
BOOL SetupFileAddDroneGun(SetupFile *setup, int modelid, float levelscale,
                          const double position[3], const double facing[3],
                          DWORD *selectionout, const char **reasonout);


/* Used within an edit transaction. Accepts a prop index or a character index
   tagged with SETUP_CHARACTER_SELECTION_BIT. Bound-pad IDs remain stable for
   scripts unless another placed object shares the pad. Ordinary pads retain
   reference-aware detachment. Props gain explicit placement flags; characters
   retain their flags and the game's stan-grounded placement. */
BOOL SetupFileTranslateModel(SetupFile *setup, DWORD selection,
                              float levelscale, const double offset[3],
                              const char **reasonout);

/* Translate a prop's geometry while placing its private bound-pad reference
 * at a separately resolved Stan position (world units). Bounds carry the
 * remaining offset; native dimensions, model scale and orientation survive.
 * Caller owns the edit transaction and validates reference/stanname. */
BOOL SetupFileTranslateModelReferenced(SetupFile *setup, DWORD selection,
    float levelscale, const double offset[3], const float reference[3],
    const char *stanname, const char **reasonout);

/* Moves the authored pad in place, retaining all references and bounds.
   Invalidates only this pad's stan link so it resolves at its new position. */
BOOL SetupFileTranslatePad(SetupFile *setup, const SetupPadRef *ref,
                            float levelscale, const double offset[3],
                            BOOL *changedout, const char **reasonout);

/* Store a resolved native plink name without changing pad coordinates or
 * another pad's shared string. Caller owns pad isolation and the transaction. */
BOOL SetupFileSetPadStanName(SetupFile *setup, const SetupPadRef *ref,
    const char *name, const char **reasonout);

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
