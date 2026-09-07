#ifndef GEDITOR_RIGHTPANEL_H
#define GEDITOR_RIGHTPANEL_H

#include <windows.h>

#include "bgdocument.h"
#include "setupload.h"

/* Sent to the frame whenever a visibility checkbox changes. wparam is
   a bitwise combination of the RIGHTPANEL_SHOW_* flags below. */
#define RIGHTPANEL_WM_VISIBILITY_CHANGED (WM_APP + 2)

#define RIGHTPANEL_SHOW_BG_PRIMARY   0x01
#define RIGHTPANEL_SHOW_BG_SECONDARY 0x02
#define RIGHTPANEL_SHOW_STAN         0x04
#define RIGHTPANEL_SHOW_PORTALS      0x08

BOOL RightPanelRegisterClass(HINSTANCE hinstance);
HWND RightPanelCreate(HWND parent, HINSTANCE hinstance);

/* The lower pane is a generic properties surface. Background triangles are
   its first supported selection type. */
void RightPanelSetBgTriangle(HWND panel, const BgDocument *document,
                             const BgFaceRef *ref);
void RightPanelSetBgSelectionCount(HWND panel, int count);
void RightPanelSetSetupObject(HWND panel, const SetupObject *object,
                              DWORD objectindex);

#endif /* GEDITOR_RIGHTPANEL_H */
