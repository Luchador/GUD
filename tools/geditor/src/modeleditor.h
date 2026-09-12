#ifndef GEDITOR_MODELEDITOR_H
#define GEDITOR_MODELEDITOR_H

#include <windows.h>

#define MODELEDITOR_CHANGED (WM_APP + 0x241)

BOOL ModelEditorShow(HWND owner, HINSTANCE instance, const char *projectdir);
/* Open/reuse the viewer and select a named project asset. Returns FALSE if
   the window or list entry is unavailable; model-load errors appear in the viewer. */
BOOL ModelEditorOpenModel(HWND owner, HINSTANCE instance, const char *projectdir,
                          const char *name, const char **reasonout);
/* Update or clear an already-open viewer when the main project changes. */
void ModelEditorSetProject(const char *projectdir);
/* Reload the displayed model and textures without resetting its orbit camera. */
void ModelEditorRefreshImages(void);
BOOL ModelEditorHandleMessage(MSG *message);

#endif
