#ifndef GEDITOR_RIGHTPANEL_H
#define GEDITOR_RIGHTPANEL_H

#include <windows.h>

#include "bgdocument.h"
#include "setupload.h"
#include "stanload.h"
#include "edittool.h"
#include "sceneoutliner.h"
#include "visibilitypanel.h"

/* Assign all selected stan faces to the existing room in wparam. */
#define RIGHTPANEL_WM_STAN_ROOM_CHANGED (WM_APP + 73)
#define RIGHTPANEL_WM_STAN_TYPE_CHANGED (WM_APP + 113) /* wparam: StanTileType */
#define RIGHTPANEL_WM_BOUND_PAD_MODEL (WM_APP + 91)
typedef struct RightPanelPadModel {
    SetupPadRef pad;
    ULONG_PTR document;
    int modelid;
    BOOL door;
} RightPanelPadModel;

/* Synchronous absolute-position request. axismask marks edited X/Y/Z fields;
   the frame translates the selection's average position, then refreshes it. */
#define RIGHTPANEL_WM_SET_POSITION (WM_APP + 5)
#define RIGHTPANEL_WM_TRANSFORM_MODE (WM_APP + 17)
#define RIGHTPANEL_WM_SET_ROTATION (WM_APP + 18)
#define RIGHTPANEL_WM_SET_SCALE (WM_APP + 24)
typedef struct RightPanelPosition {
    double position[3];
    unsigned int axismask;
} RightPanelPosition;

BOOL RightPanelRegisterClass(HINSTANCE hinstance);
HWND RightPanelCreate(HWND parent, HINSTANCE hinstance);
/* Reveal newly created, pasted or selected content and synchronize the toggles. */
void RightPanelReveal(HWND panel, DWORD flags);
void RightPanelSetScene(HWND panel, const SetupFile *setup, const BgPortalFile *portals,
                       SceneOutlinerKind selectedkind, DWORD selectedindex);
/* A NULL position clears the fields; a noneditable position remains visible. */
void RightPanelSetTransformState(HWND panel, const double position[3],
                                 DWORD count, BOOL editable, double gridstep);
void RightPanelSetTransformMode(HWND panel, TransformMode mode);
void RightPanelSetNativeUnits(HWND panel, BOOL native);
void RightPanelSetScaleSpace(HWND panel, BOOL local, BOOL group);
void RightPanelSetRotationAxes(HWND panel, unsigned int axes);
BOOL RightPanelHandleMessage(HWND panel, MSG *message);
void RightPanelSetVertexPaintMode(HWND panel, BOOL enabled);
void RightPanelGetPaintColor(HWND panel, unsigned char rgba[4]);
void RightPanelSetPaintColor(HWND panel, const unsigned char rgba[4]);
void RightPanelSetColorSampling(HWND panel, BOOL enabled);
#define RIGHTPANEL_WM_PICK_COLOR (WM_APP + 55)

/* The lower pane is a generic properties surface. Background triangles are
   its first supported selection type. */
void RightPanelSetBgFaces(HWND panel, const BgDocument *document,
                         const BgFaceRef *refs, DWORD count, HWND browser);
void RightPanelSetStanSelection(HWND panel, const StanFile *stan, EditorTool tool,
                                 DWORD count, DWORD singletile,
                                 const DWORD *selected, DWORD roomcount);
void RightPanelSetBgComponentSelection(HWND panel, BOOL edges, int count);
void RightPanelSetModelSelectionCount(HWND panel, DWORD count);
void RightPanelSetBgSelectionCount(HWND panel, int count);
void RightPanelSetRoomSelection(HWND panel, DWORD room);
void RightPanelSetRoomMode(HWND panel, BOOL enabled);
void RightPanelSetGlassPortals(HWND panel, const BgPortalFile *portals, float levelscale);
void RightPanelSetSetupObject(HWND panel, const SetupFile *setup,
                              DWORD objectindex, const char *projectdir);
/* Clear with NULL whenever the selection is not an ObjectRecord. */
void RightPanelSetObjectFlags(HWND panel, const SetupFile *setup, const DWORD *ids, DWORD count);
void RightPanelSetSetupCharacter(HWND panel, const SetupFile *setup, DWORD index);
void RightPanelSetSetupPad(HWND panel, const SetupFile *setup, const SetupPadRef *ref,
                          const char *projectdir);
void RightPanelSetSetupMarker(HWND panel, const SetupMarkerRef *ref);
void RightPanelSetPortal(HWND panel, const BgDocument *document, DWORD index);

#endif /* GEDITOR_RIGHTPANEL_H */
