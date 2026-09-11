#ifndef GEDITOR_VIEWPORT_H
#define GEDITOR_VIEWPORT_H

#include <windows.h>

#include "bgdocument.h"
#include "edittool.h"
#include "rom.h"
#include "setupload.h"
#include "stanload.h"
#include "texload.h"
#include "monitorload.h"

/* Sent to the frame after mouse picking changes the scene selection. The
   frame can query the selected background face or setup object below. */
#define VIEWPORT_WM_SELECTION_CHANGED (WM_APP + 3)
/* Sent only while the viewport owns keyboard focus, so Delete in future
   property editors cannot accidentally remove scene geometry. */
#define VIEWPORT_WM_DELETE_SELECTION  (WM_APP + 4)

/* Synchronous paint request. lparam points to a stack-owned hit, copied
   before the frame edits the document and rebuilds the viewport. */
#define VIEWPORT_WM_PAINT_VERTEX (WM_APP + 7)
#define VIEWPORT_WM_TRANSLATE_SELECTION (WM_APP + 8)
/* Same ViewportTranslation payload; commits one snap through edit history. */
#define VIEWPORT_WM_SNAP_VERTEX (WM_APP + 22)
/* Refresh coordinate fields after transient drag movement or cancellation. */
#define VIEWPORT_WM_TRANSFORM_PREVIEW (WM_APP + 9)
/* wparam is the document tile index; painting changes its complete RGB. */
#define VIEWPORT_WM_PAINT_STAN (WM_APP + 11)
typedef struct ViewportTranslation { double offset[3]; } ViewportTranslation;

/* Cancels transient transform/marquee drags before history, saving, or changing tools. */
void ViewportCancelTransform(HWND hwnd);
BgDocumentVertexRef *ViewportGetMoveVertices(HWND hwnd, DWORD *countout);
int ViewportGetSelectedComponentCount(HWND hwnd);

/* Mean vertex positions, edge midpoints or face centers; a model uses its
   surface centroid. Includes the live drag preview, excludes hidden items. */
BOOL ViewportGetSelectionPosition(HWND hwnd, double position[3], DWORD *countout);

void ViewportSetBackgroundColor(HWND viewport, const unsigned char rgb[3]);

/* Level settings and the user's View toggle are independent; changing levels
   keeps the toggle. NULL clears the level fog. Orbit previews never use it. */
void ViewportSetLevelFog(HWND viewport, const RomFog *fog, float renderscale);
BOOL ViewportGetFogVisible(HWND viewport);
void ViewportSetFogVisible(HWND viewport, BOOL visible);

typedef struct ViewportBgVertexHit {
    BgFaceRef face;
    unsigned int corner;
} ViewportBgVertexHit;

/**
 * Main 3D viewport functions. Create a child window with OpenGL context.
 */
BOOL ViewportRegisterClass(HINSTANCE hinstance);
HWND ViewportCreate(HWND parent, HINSTANCE hinstance);
/* Independent textured model preview: either mouse button orbits, wheel dollies.
   Uses ViewportSetScene, with no level selection/editing or test geometry. */
HWND ViewportCreateOrbit(HWND parent, HINSTANCE hinstance);
void ViewportRedraw(HWND viewport);

/* Vertex mode: left-drag selects through geometry in the visible layers;
   Shift adds and Control subtracts. BG and stan remain separate selections.
   Face selection is the initial tool. Changing tools clears the current
   selection without editing assets or adding an undo history entry. */
EditorTool ViewportGetTool(HWND viewport);
void ViewportSetTool(HWND viewport, EditorTool tool);
BOOL ViewportGetVertexSnap(HWND viewport);
void ViewportSetVertexSnap(HWND viewport, BOOL enabled);

/* Refreshes every rendered corner sharing this vertex after a color-only
   document edit. Keeps geometry, camera, and textures resident. */
void ViewportRefreshBgVertexColor(HWND viewport, const BgDocument *document,
                                   const ViewportBgVertexHit *hit);

/**
 * Frame loop hooks. The main loop asks whether a viewport is flying
 * and if so drives one flight frame per pass instead of waiting for paint or timer messages.
 **/
BOOL ViewportIsFlying(HWND viewport);
void ViewportFlyFrame(HWND viewport);

/*
 * Replaces the viewport's scene with a triangle soup. All supplied arrays
 * are copied and remain caller-owned. renderflags supplies decoded render
 * state and S/T wrapping; NULL uses primary/secondary defaults. NULL/0 restores the built-in test
 * scene (an empty canvas for orbit previews). Set framecamera when opening a level; clear it when rebuilding an
 * edited level so undo and redo do not move the user's viewpoint. Surviving
 * selected and hidden BG faces are retained when framecamera is FALSE. Object
 * indices parallel the triangle suffix beginning at objectfirsttriangle.
 */
BOOL ViewportSetScene(HWND hwnd, const BgVertex *tris,
                      const unsigned short *tritags,
                      const BgRenderFlags *renderflags,
                      const BgFaceRef *facerefs,
                      const BgDocumentVertexRef *vertexrefs,
                      const DWORD *objectindices, int objectfirsttriangle,
                      const MonitorGeometry *monitors,
                      int tricount,
                      const char *projectdir, BOOL framecamera);

/* Background selection is stored in draw-ordered viewport order. These
   accessors expose stable source identities so callers never depend on draw
   order. A setup object and background faces are mutually exclusive. */
int ViewportGetSelectedBgFaceCount(HWND hwnd);
BOOL ViewportGetSelectedBgFaces(HWND hwnd, BgFaceRef *out, int count);
BOOL ViewportGetSingleSelectedBgFace(HWND hwnd, BgFaceRef *out);
/* Temporary viewport visibility only: never edits assets or history. Hidden
   identities survive scene rebuilds; opening/closing a level resets them. */
BOOL ViewportHideSelectedBgFaces(HWND hwnd);
void ViewportUnhideAllBgFaces(HWND hwnd);
BOOL ViewportHasHiddenBgFaces(HWND hwnd);
/* Native image dimensions already loaded by the scene. Missing/untextured
   images return FALSE and a 1x1 fallback, as in model UV export. */
BOOL ViewportGetTextureSize(HWND hwnd, unsigned short textureid, int *width, int *height);
BOOL ViewportGetSelectedObject(HWND hwnd, DWORD *setupobjectindex);

/* Drop onto the nearest rendered BG/model or visible stan surface. A void
   drop fails. Selecting a new instance clears other selections. */
BOOL ViewportGetModelDropPosition(HWND hwnd, POINT screen, double position[3]);
BOOL ViewportGetCameraDirection(HWND hwnd, double look[3]);
void ViewportSelectSetupModel(HWND hwnd, DWORD selection);
BOOL ViewportGetSelectedPad(HWND hwnd, SetupPadRef *out);
BOOL ViewportGetSelectedMarker(HWND hwnd, SetupMarkerRef *out, SetupMarker *spawn);
/* NULL clears selection. Call after a spawn edit changes intro command indices. */
void ViewportSelectSetupMarker(HWND hwnd, const SetupMarkerRef *ref);
BOOL ViewportGetMarkerRotation(HWND hwnd, Rotation *frame);

/* Non-selecting face-mode drop query in screen coordinates. Uses the nearest
 * visible surface without cycling the selection stack. Objects and stan tiles
 * in front block a BG drop. selectedout identifies drops onto the selection. */
BOOL ViewportGetTextureDropFace(HWND hwnd, POINT screen, BgFaceRef *out, BOOL *selectedout);

/* Replaces the pad overlay. PadRecords are small green wireframe
   cubes; BoundPadRecords are red wireframes of their authored volume.
   Pads retain their authored positions, just as setupLoadFiles does;
   stan grounding belongs to the objects placed at those pads.
   Occupied markers appear when objects are hidden, or the pad is selected.
   Face mode selects whole pad markers; pads are independent setup identities. */
void ViewportSetSetupPads(HWND hwnd, const SetupFile *setup,
                          float levelscale,
                          const unsigned char *occupiedpads,
                          const unsigned char *occupiedboundpads);
/* Call after loading the level's stan and setup. Positions the level camera
 * 2 metres above the first spawn marker, facing its horizontal direction.
 * Does nothing without a spawn or in an orbit/model viewport. */
void ViewportMoveCameraToSpawn(HWND hwnd);

/* Replaces the colored stan overlay. Stan positions have already been
   converted to gameplay world coordinates by StanLoadProjectFile. */
BOOL ViewportSetStanTiles(HWND hwnd, const StanFile *stan);
void ViewportSetStanOpacity(HWND hwnd, int percent);
DWORD ViewportGetStanSelectionCount(HWND hwnd, DWORD *singletile);
StanPointRef *ViewportGetMoveStanPoints(HWND hwnd, DWORD *countout);

/* Replaces the BG portal overlay. Shared portal polygons are rendered
   once even when several authored table entries refer to them. */
void ViewportSetPortals(HWND hwnd, const BgPortalFile *portals);
/* Table index, not shared polygon index. Hidden portals cannot be selected. */
BOOL ViewportGetSelectedPortal(HWND hwnd, DWORD *index);

/* Controls background, stan, portal and object/character visibility.
   The loaded scene remains resident while a layer is hidden. */
void ViewportSetGeometryVisibility(HWND hwnd, BOOL bgprimary, BOOL bgsecondary, BOOL stan, BOOL portals, BOOL objects);

/* Master culling toggle. Enabled honors the BG's per-triangle state;
   disabled renders every triangle double-sided. */
BOOL ViewportGetBackfaceCulling(HWND hwnd);
void ViewportSetBackfaceCulling(HWND hwnd, BOOL enabled);
/* Counts the complete loaded BG, regardless of layer visibility. */
BOOL ViewportGetBgStatisticsVisible(HWND hwnd);
void ViewportSetBgStatisticsVisible(HWND hwnd, BOOL enabled);

#define VIEWPORT_WM_ROTATE_SELECTION (WM_APP + 16)
typedef struct ViewportRotation { Rotation rotation; double pivot[3]; } ViewportRotation;
void ViewportSetTransformMode(HWND hwnd, TransformMode mode);
TransformMode ViewportGetTransformMode(HWND hwnd);
#define VIEWPORT_WM_SCALE_SELECTION (WM_APP + 23)
void ViewportSetScaleAxes(HWND hwnd, const Rotation *axes);
BOOL ViewportGetScaling(HWND hwnd, Scaling *scale);
void ViewportSelectPad(HWND hwnd, const SetupPadRef *ref);
BOOL ViewportIsRotating(HWND hwnd);
BOOL ViewportIsTransforming(HWND hwnd);
/* Faces use a reference face; components use their selected positions.
   Single vertices and coincident/zero-length component selections return FALSE. */
BOOL ViewportGetGeometryRotation(HWND hwnd, Rotation *frame);
void ViewportSetRotationFrame(HWND hwnd, const Rotation *frame, unsigned int axes);
BOOL ViewportGetRotation(HWND hwnd, Rotation *frame, double degrees[3], double pivot[3]);

#endif
