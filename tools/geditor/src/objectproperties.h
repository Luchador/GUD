#ifndef GEDITOR_OBJECTPROPERTIES_H
#define GEDITOR_OBJECTPROPERTIES_H

#include <windows.h>
#include "setupload.h"

/* Synchronous, stack-owned SetupObjectPropertyEdit, forwarded by RightPanel.
 * The frame owns validation against selection, history, and scene rebuilding. */
#define OBJECTPROPERTIES_WM_CHANGED (WM_APP + 46)
BOOL ObjectPropertiesRegisterClass(HINSTANCE instance);
HWND ObjectPropertiesCreate(HWND parent, HINSTANCE instance);
/* NULL setup clears the binding before the panel is hidden or a level closes. */
BOOL ObjectPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index,
                                   const char *projectdir);
BOOL ObjectPropertiesHandleMessage(HWND panel, MSG *message);

#endif
