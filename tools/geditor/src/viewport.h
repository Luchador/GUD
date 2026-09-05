#ifndef GEDITOR_VIEWPORT_H
#define GEDITOR_VIEWPORT_H

#include <windows.h>

#include "bgload.h"
#include "rom.h"
#include "texload.h"

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

/*
 * Replaces the viewport's scene with a triangle soup (copied; the
 * caller keeps ownership of tris). NULL/0 restores the built-in test
 * scene. The camera is repositioned to frame the new geometry.
 */
void ViewportSetScene(HWND hwnd, const BgVertex *tris,
                      const unsigned short *tritags, int tricount,
                      const RomFile *rom);

/* Master culling toggle. Enabled honors the BG's per-triangle state;
   disabled renders every triangle double-sided. */
BOOL ViewportGetBackfaceCulling(HWND hwnd);
void ViewportSetBackfaceCulling(HWND hwnd, BOOL enabled);

#endif
