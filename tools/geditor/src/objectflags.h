#ifndef GEDITOR_OBJECTFLAGS_H
#define GEDITOR_OBJECTFLAGS_H

#include "setupload.h"

#define OBJECTFLAGS_WM_CHANGED (WM_APP + 32)
typedef struct ObjectFlagEdit {
    DWORD objectindex;
    unsigned int bank;
    DWORD mask;
    BOOL enabled;
} ObjectFlagEdit;

BOOL ObjectFlagsRegisterClass(HINSTANCE instance);
HWND ObjectFlagsCreate(HWND parent, HINSTANCE instance);
/* Copies values; never retains a pointer into the undoable setup document. */
void ObjectFlagsSetSelection(HWND panel, const SetupObject *object, DWORD index);

#endif
