#ifndef GEDITOR_FACEPROPERTIES_H
#define GEDITOR_FACEPROPERTIES_H

#include "bgdocument.h"

/* Synchronous request to the frame via the right panel; lparam points to a
 * BgFacePropertiesEdit. The frame owns selection, history and rebuilding. */
#define FACEPROPERTIES_WM_CHANGED (WM_APP + 25)
/* Read-only navigation request; wparam is the selected texture ID. */
#define FACEPROPERTIES_WM_REVEAL_IMAGE (WM_APP + 26)

/* Room reassignment request; wparam is the existing destination room. */
#define FACEPROPERTIES_WM_ROOM_CHANGED (WM_APP + 59)
#define FACEPROPERTIES_WM_LAYER_CHANGED (WM_APP + 72)

BOOL FacePropertiesHandleMessage(HWND panel, MSG *message);
BOOL FacePropertiesRegisterClass(HINSTANCE instance);
HWND FacePropertiesCreate(HWND parent, HINSTANCE instance);
BOOL FacePropertiesSetSelection(HWND panel, const BgDocument *document,
                                const BgFaceRef *refs, DWORD count, HWND browser);

#endif
