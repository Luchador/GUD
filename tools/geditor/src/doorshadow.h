#ifndef GEDITOR_DOORSHADOW_H
#define GEDITOR_DOORSHADOW_H
#include "bgdocument.h"
#include "setupload.h"
#include "../../../src/doorshadowformat.h"

typedef struct DoorShadowProperties {
    DWORD room, layer, direction, light, dark;
    LONG door; /* setup object index, -1 if unlinked/deleted */
} DoorShadowProperties;
typedef enum DoorShadowField {
    DOOR_SHADOW_EDIT_DOOR, DOOR_SHADOW_EDIT_DIRECTION,
    DOOR_SHADOW_EDIT_LIGHT, DOOR_SHADOW_EDIT_DARK
} DoorShadowField;
typedef struct DoorShadowEdit {
    DWORD objectindex;
    DoorShadowField field;
    LONG value;
} DoorShadowEdit;

BOOL DoorShadowCreate(BgDocument *bg, SetupFile *setup, const BgFaceRef faces[2],
    DWORD *selection, const char **why);
BOOL DoorShadowGet(const SetupFile *setup, DWORD index, DoorShadowProperties *out);
BOOL DoorShadowTranslate(SetupFile *setup, DWORD index, float scale, const double offset[3], const char **why);
BOOL DoorShadowSet(SetupFile *setup, const DoorShadowEdit *edit, BOOL *changed, const char **why);
BOOL DoorShadowBuildPreview(const SetupFile *setup, DWORD index, float levelscale,
    BgVertex vertices[18], unsigned short *tag, BgRenderFlags *flags, const char **why);
void DoorShadowSetPreview(DWORD index, int percent);
int DoorShadowGetPreview(const SetupFile *setup, DWORD index);
BOOL DoorShadowValidateNative(const unsigned char *data, DWORD size,
    const RomFile *rom, const char **why);

/* Setup module owns command append/recycling and decoded object indexes. */
BOOL SetupFileAddDoorShadow(SetupFile *setup, const unsigned char record[DOOR_SHADOW_BYTES],
    DWORD *selection, const char **why);
LONG SetupFileObjectCommand(const SetupFile *setup, DWORD objectindex);
LONG SetupFileCommandObject(const SetupFile *setup, LONG command);
#endif
