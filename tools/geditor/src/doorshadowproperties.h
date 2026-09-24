#ifndef GEDITOR_DOORSHADOWPROPERTIES_H
#define GEDITOR_DOORSHADOWPROPERTIES_H
#include "doorshadow.h"
#define DOORSHADOW_WM_CHANGED (WM_APP + 103)
#define DOORSHADOW_WM_PICK (WM_APP + 104)
#define DOORSHADOW_WM_PREVIEW (WM_APP + 105)
BOOL DoorShadowPropertiesRegisterClass(HINSTANCE instance);
HWND DoorShadowPropertiesCreate(HWND parent, HINSTANCE instance);
void DoorShadowPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index);
#endif
