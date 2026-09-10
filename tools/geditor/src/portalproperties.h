#ifndef GEDITOR_PORTALPROPERTIES_H
#define GEDITOR_PORTALPROPERTIES_H
#include "bgdocument.h"

/* Atomic request: both endpoints are applied together, allowing room swaps. */
#define PORTALPROPERTIES_WM_CHANGED (WM_APP + 31)
typedef struct PortalPropertiesEdit { DWORD portal, room1, room2; } PortalPropertiesEdit;
BOOL PortalPropertiesRegisterClass(HINSTANCE instance);
HWND PortalPropertiesCreate(HWND parent, HINSTANCE instance);
BOOL PortalPropertiesSetSelection(HWND panel, const BgDocument *document, DWORD index);
BOOL PortalPropertiesHandleMessage(HWND panel, MSG *message);
#endif
