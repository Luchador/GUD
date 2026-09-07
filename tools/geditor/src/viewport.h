#ifndef GEDITOR_VIEWPORT_H
#define GEDITOR_VIEWPORT_H

#include <windows.h>

#include "bgdocument.h"
#include "edittool.h"
#include "setupload.h"
#include "stanload.h"
#include "texload.h"

/* Sent to the frame after mouse picking changes the scene selection. The
   frame can query the selected background face or setup object below. */
#define VIEWPORT_WM_SELECTION_CHANGED (WM_APP + 3)
/* Sent only while the viewport owns keyboard focus, so Delete in future
   property editors cannot accidentally remove scene geometry. */
#define VIEWPORT_WM_DELETE_SELECTION  (WM_APP + 4)

/**
 * Main 3D viewport functions. Create a child window with OpenGL context.
 */
BOOL ViewportRegisterClass(HINSTANCE hinstance);
HWND ViewportCreate(HWND parent, HINSTANCE hinstance);
void ViewportRedraw(HWND viewport);

/* Face selection is the initial tool. Changing tools clears the current
   selection without editing assets or adding an undo history entry. */
EditorTool ViewportGetTool(HWND viewport);
void ViewportSetTool(HWND viewport, EditorTool tool);

/**
 * Frame loop hooks. The main loop asks whether a viewport is flying
 * and if so drives one flight frame per pass instead of waiting for paint or timer messages.
 **/
BOOL ViewportIsFlying(HWND viewport);
void ViewportFlyFrame(HWND viewport);

/*
 * Replaces the viewport's scene with a triangle soup. All supplied arrays
 * are copied and remain caller-owned. NULL/0 restores the built-in test
 * scene. Set framecamera when opening a level; clear it when rebuilding an
 * edited level so undo and redo do not move the user's viewpoint. Surviving
 * selected BG faces are retained when framecamera is FALSE. Object
 * indices parallel the triangle suffix beginning at objectfirsttriangle.
 */
BOOL ViewportSetScene(HWND hwnd, const BgVertex *tris,
                      const unsigned short *tritags,
                      const BgFaceRef *facerefs,
                      const DWORD *objectindices, int objectfirsttriangle,
                      int tricount,
                      const char *projectdir, BOOL framecamera);

/* Background selection is stored in texture-sorted viewport order. These
   accessors expose stable source identities so callers never depend on draw
   order. A setup object and background faces are mutually exclusive. */
int ViewportGetSelectedBgFaceCount(HWND hwnd);
BOOL ViewportGetSelectedBgFaces(HWND hwnd, BgFaceRef *out, int count);
BOOL ViewportGetSingleSelectedBgFace(HWND hwnd, BgFaceRef *out);
BOOL ViewportGetSelectedObject(HWND hwnd, DWORD *setupobjectindex);

/* Replaces the pad overlay. PadRecords are small green wireframe
   cubes; BoundPadRecords are red wireframes of their authored volume.
   Pads retain their authored positions, just as setupLoadFiles does;
   stan grounding belongs to the objects placed at those pads.
   A nonzero occupied entry suppresses the corresponding marker because
   the setup object's model is already visible at that pad. */
void ViewportSetSetupPads(HWND hwnd, const SetupFile *setup,
                          float levelscale,
                          const unsigned char *occupiedpads,
                          const unsigned char *occupiedboundpads);

/* Replaces the colored stan overlay. Stan positions have already been
   converted to gameplay world coordinates by StanLoadProjectFile. */
void ViewportSetStanTiles(HWND hwnd, const StanFile *stan);

/* Replaces the BG portal overlay. Shared portal polygons are rendered
   once even when several authored table entries refer to them. */
void ViewportSetPortals(HWND hwnd, const BgPortalFile *portals);

/* Controls the four independently previewable world-geometry layers.
   The loaded scene remains resident while a layer is hidden. */
void ViewportSetGeometryVisibility(HWND hwnd, BOOL bgprimary,
                                   BOOL bgsecondary, BOOL stan,
                                   BOOL portals);

/* Master culling toggle. Enabled honors the BG's per-triangle state;
   disabled renders every triangle double-sided. */
BOOL ViewportGetBackfaceCulling(HWND hwnd);
void ViewportSetBackfaceCulling(HWND hwnd, BOOL enabled);

#endif
