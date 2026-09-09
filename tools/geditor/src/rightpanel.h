#ifndef GEDITOR_RIGHTPANEL_H
#define GEDITOR_RIGHTPANEL_H

#include <windows.h>

#include "bgdocument.h"
#include "setupload.h"
#include "stanload.h"
#include "edittool.h"

/* Sent to the frame whenever a visibility checkbox changes. wparam is
   a bitwise combination of the RIGHTPANEL_SHOW_* flags below. */
#define RIGHTPANEL_WM_VISIBILITY_CHANGED (WM_APP + 2)
/* wparam is opacity percent, 0-100; this changes only the preview. */
#define RIGHTPANEL_WM_STAN_OPACITY (WM_APP + 10)

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

#define RIGHTPANEL_SHOW_BG_PRIMARY   0x01
#define RIGHTPANEL_SHOW_BG_SECONDARY 0x02
#define RIGHTPANEL_SHOW_STAN         0x04
#define RIGHTPANEL_SHOW_PORTALS      0x08
#define RIGHTPANEL_SHOW_OBJECTS      0x10

BOOL RightPanelRegisterClass(HINSTANCE hinstance);
HWND RightPanelCreate(HWND parent, HINSTANCE hinstance);
/* Reveal a newly placed model and keep the visibility checkbox in sync. */
void RightPanelShowObjects(HWND panel);
/* A NULL position clears the fields; a noneditable position remains visible. */
void RightPanelSetTransformState(HWND panel, const double position[3],
                                 DWORD count, BOOL editable, double gridstep);
void RightPanelSetTransformMode(HWND panel, TransformMode mode);
void RightPanelSetScaleLocal(HWND panel, BOOL local);
void RightPanelSetRotationAxes(HWND panel, unsigned int axes);
BOOL RightPanelHandleMessage(HWND panel, MSG *message);
void RightPanelSetVertexPaintMode(HWND panel, BOOL enabled);
void RightPanelGetPaintColor(HWND panel, unsigned char rgba[4]);

/* The lower pane is a generic properties surface. Background triangles are
   its first supported selection type. */
void RightPanelSetBgTriangle(HWND panel, const BgDocument *document,
                             const BgFaceRef *ref);
void RightPanelSetStanSelection(HWND panel, const StanFile *stan, EditorTool tool,
                                 DWORD count, DWORD singletile);
void RightPanelSetBgComponentSelection(HWND panel, BOOL edges, int count);
void RightPanelSetBgSelectionCount(HWND panel, int count);
void RightPanelSetSetupObject(HWND panel, const SetupObject *object,
                              DWORD objectindex);
void RightPanelSetSetupCharacter(HWND panel, const SetupCharacter *character);
void RightPanelSetSetupPad(HWND panel, const SetupFile *setup, const SetupPadRef *ref);

#endif /* GEDITOR_RIGHTPANEL_H */
