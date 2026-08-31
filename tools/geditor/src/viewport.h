#ifndef GEDITOR_VIEWPORT_H
#define GEDITOR_VIEWPORT_H

#include <windows.h>

/**
 * Main 3D viewport functions. Create a child window with OpenGL context.
 */
BOOL ViewportRegisterClass(HINSTANCE hinstance);
HWND ViewportCreate(HWND parent, HINSTANCE hinstance);
void ViewportRedraw(HWND viewport);

/**
 * Frame loop hooks. The main loop asks whether a viewport is flying
 * and if so drives one flight frame per pass instead of waiting for paint or timer messages.
 **/
BOOL ViewportIsFlying(HWND viewport);
void ViewportFlyFrame(HWND viewport);

#endif