#ifndef GEDITOR_EDITORSETTINGS_H
#define GEDITOR_EDITORSETTINGS_H
#include <windows.h>
#include "editorunits.h"

#define EDITORSETTINGS_WM_UNITS_CHANGED (WM_APP + 110)
void EditorSettingsLoad(void);
EditorCoordinateUnits EditorSettingsGetUnits(void);
void EditorSettingsSetUnits(EditorCoordinateUnits units);
BOOL EditorSettingsShow(HWND owner, HINSTANCE instance);
void EditorSettingsClose(void);
BOOL EditorSettingsHandleMessage(MSG *message);
#endif
