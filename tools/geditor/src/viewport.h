#ifndef GEDITOR_VIEWPORT_H
#define GEDITOR_VIEWPORT_H

#include <windows.h>

/*
 * The 3D viewport: a child window owning an OpenGL context.
 *
 * The parent registers the class once, creates one viewport, and
 * repositions it with MoveWindow from its WM_SIZE handler. The
 * viewport repaints only when something invalidates it (on-demand
 * rendering) - call ViewportRedraw after changing what it shows.
 */

BOOL ViewportRegisterClass(HINSTANCE hinstance);
HWND ViewportCreate(HWND parent, HINSTANCE hinstance);
void ViewportRedraw(HWND viewport);

#endif /* GEDITOR_VIEWPORT_H */