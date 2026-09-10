#ifndef GEDITOR_MODELEDITOR_H
#define GEDITOR_MODELEDITOR_H

#include <windows.h>

#define MODELEDITOR_CHANGED (WM_APP + 0x241)

BOOL ModelEditorShow(HWND owner, HINSTANCE instance, const char *projectdir);
/* Update or clear an already-open viewer when the main project changes. */
void ModelEditorSetProject(const char *projectdir);
BOOL ModelEditorHandleMessage(MSG *message);

#endif
