#ifndef GEDITOR_OBJECTPROPERTIES_H
#define GEDITOR_OBJECTPROPERTIES_H

#include <windows.h>
#include "setupload.h"
#include "bgload.h"

/* Shared project model choices, including newly imported props. */
BOOL ObjectPropertiesFillModelList(HWND combo, const char *projectdir);

/* Synchronous, stack-owned SetupObjectPropertyEdit, forwarded by RightPanel.
 * The frame owns validation against selection, history, and scene rebuilding. */
#define OBJECTPROPERTIES_WM_CHANGED (WM_APP + 46)
BOOL ObjectPropertiesRegisterClass(HINSTANCE instance);
HWND ObjectPropertiesCreate(HWND parent, HINSTANCE instance);
/* NULL setup clears the binding before the panel is hidden or a level closes. */
BOOL ObjectPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index,
                                   const char *projectdir);
void ObjectPropertiesSetGlassPortals(HWND panel, const BgPortalFile *portals, float levelscale);
BOOL ObjectPropertiesIsDoorShadow(HWND panel);
BOOL ObjectPropertiesHandleMessage(HWND panel, MSG *message);

#endif
