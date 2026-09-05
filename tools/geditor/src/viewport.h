#ifndef GEDITOR_VIEWPORT_H
#define GEDITOR_VIEWPORT_H

#include <windows.h>

#include "bgload.h"
#include "setupload.h"
#include "stanload.h"
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
                      const char *projectdir);

/* Replaces the pad overlay. PadRecords are small green wireframe
   cubes; BoundPadRecords are red wireframes of their authored volume. */
void ViewportSetSetupPads(HWND hwnd, const SetupFile *setup,
                          float levelscale);

/* Replaces the colored stan overlay. Stan positions have already been
   converted to gameplay world coordinates by StanLoadProjectFile. */
void ViewportSetStanTiles(HWND hwnd, const StanFile *stan);

/* Master culling toggle. Enabled honors the BG's per-triangle state;
   disabled renders every triangle double-sided. */
BOOL ViewportGetBackfaceCulling(HWND hwnd);
void ViewportSetBackfaceCulling(HWND hwnd, BOOL enabled);

#endif
