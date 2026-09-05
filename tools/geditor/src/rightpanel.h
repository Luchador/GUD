#ifndef GEDITOR_RIGHTPANEL_H
#define GEDITOR_RIGHTPANEL_H

#include <windows.h>

/* Sent to the frame whenever a visibility checkbox changes. wparam is
   a bitwise combination of the RIGHTPANEL_SHOW_* flags below. */
#define RIGHTPANEL_WM_VISIBILITY_CHANGED (WM_APP + 2)

#define RIGHTPANEL_SHOW_BG_PRIMARY   0x01
#define RIGHTPANEL_SHOW_BG_SECONDARY 0x02
#define RIGHTPANEL_SHOW_STAN         0x04
#define RIGHTPANEL_SHOW_PORTALS      0x08

BOOL RightPanelRegisterClass(HINSTANCE hinstance);
HWND RightPanelCreate(HWND parent, HINSTANCE hinstance);

#endif /* GEDITOR_RIGHTPANEL_H */
