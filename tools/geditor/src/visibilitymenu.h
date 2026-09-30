#ifndef GEDITOR_VISIBILITYMENU_H
#define GEDITOR_VISIBILITYMENU_H

#include <windows.h>

/* Preview-only settings, shared by the popup and automatic reveal commands. */
#define VISIBILITY_SHOW_BG_PRIMARY   0x01
#define VISIBILITY_SHOW_BG_SECONDARY 0x02
#define VISIBILITY_SHOW_STAN         0x04
#define VISIBILITY_SHOW_PORTALS      0x08
#define VISIBILITY_SHOW_OBJECTS      0x10
#define VISIBILITY_WM_CHANGED (WM_APP + 2) /* wparam: VISIBILITY_SHOW_* */
#define VISIBILITY_WM_STAN_OPACITY (WM_APP + 10) /* wparam: percent, 0-100 */

BOOL VisibilityMenuRegisterClass(HINSTANCE instance);
HWND VisibilityMenuCreate(HWND owner, HINSTANCE instance);
void VisibilityMenuOpen(HWND menu, HWND anchor);
void VisibilityMenuClose(HWND menu, BOOL restorefocus);
void VisibilityMenuReveal(HWND menu, DWORD flags);
BOOL VisibilityMenuHandleMessage(HWND menu, MSG *message);
/* Called by the frame for WM_MOUSEACTIVATE. Clicking the open menu's button
 * dismisses it and consumes that click instead of immediately reopening it. */
BOOL VisibilityMenuConsumeAnchorClick(HWND menu, LPARAM lparam);

#endif
