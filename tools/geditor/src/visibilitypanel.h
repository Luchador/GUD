#ifndef GEDITOR_VISIBILITYPANEL_H
#define GEDITOR_VISIBILITYPANEL_H

#include <windows.h>

/* Preview-only settings, shared by the panel and automatic reveal commands. */
#define VISIBILITY_SHOW_BG_PRIMARY   0x01
#define VISIBILITY_SHOW_BG_SECONDARY 0x02
#define VISIBILITY_SHOW_STAN         0x04
#define VISIBILITY_SHOW_PORTALS      0x08
#define VISIBILITY_SHOW_OBJECTS      0x10
#define VISIBILITY_WM_CHANGED (WM_APP + 2) /* wparam: VISIBILITY_SHOW_* */
#define VISIBILITY_WM_STAN_OPACITY (WM_APP + 10) /* wparam: percent, 0-100 */

#define VISIBILITY_PANEL_HEIGHT 208

BOOL VisibilityPanelRegisterClass(HINSTANCE instance);
HWND VisibilityPanelCreate(HWND parent, HINSTANCE instance);
void VisibilityPanelReveal(HWND panel, DWORD flags);
BOOL VisibilityPanelHandleMessage(HWND panel, MSG *message);

#endif
