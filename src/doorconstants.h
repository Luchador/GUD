#ifndef GUD_DOORCONSTANTS_H
#define GUD_DOORCONSTANTS_H

/* Authored DoorRecord enums, shared by the game and GEditor. Native values
 * are unchanged. Runtime DOORSTATE remains in bondconstants.h. */
typedef enum DOORFLAG
{
    DOORFLAG_EXTENDEDY       = 0x00000001,
    DOORFLAG_WINDOWED        = 0x00000002,
    DOORFLAG_CLIP_TO_BBOX    = 0x00000004, // Clip the mesh to the shrinking/expanding bbox while sliding
    DOORFLAG_FLIP            = 0x00000008
} DOORFLAG;

typedef enum DOORTYPE
{
    DOORTYPE_SLIDING,
    DOORTYPE_FLEXI1,   // Bunker flexi door
    DOORTYPE_FLEXI2,
    DOORTYPE_FLEXI3,
    DOORTYPE_VERTICAL,
    DOORTYPE_SWINGING,
    DOORTYPE_EYE,      // Caverns
    DOORTYPE_IRIS,     // Caverns
    DOORTYPE_FALLAWAY, // Surface grate and Train floor panel
    DOORTYPE_AZTECCHAIR
} DOORTYPE;

typedef enum DOOR_OPEN_SOUND
{
    DOOR_OPEN_SOUND_NONE,
    DOOR_OPEN_SOUND_01,
    DOOR_OPEN_SOUND_02,
    DOOR_OPEN_SOUND_METAL,
    DOOR_OPEN_SOUND_04,
    DOOR_OPEN_SOUND_WOOD,
    DOOR_OPEN_SOUND_06,
    DOOR_OPEN_SOUND_WOOD_2,
    DOOR_OPEN_SOUND_WOOD_3,
    DOOR_OPEN_SOUND_09,
    DOOR_OPEN_SOUND_METAL_2,
    DOOR_OPEN_SOUND_11,
    DOOR_OPEN_SOUND_METAL_3,
    DOOR_OPEN_SOUND_13,
    DOOR_OPEN_SOUND_HYDROLIC,
    DOOR_OPEN_SOUND_STONE,
    DOOR_OPEN_SOUND_16,
    DOOR_OPEN_SOUND_METAL_4,
    DOOR_OPEN_SOUND_18
} DOOR_OPEN_SOUND;

#endif
