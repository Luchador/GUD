#ifndef GEDITOR_STUDIOVIEWPORT_H
#define GEDITOR_STUDIOVIEWPORT_H
#include <windows.h>
#include "studioscene.h"

#define STUDIO_WM_SELECT (WM_APP + 140) /* wparam: instance index, lparam: material slot, -1 clears */

/* Independent preview context and camera; no game assets or rendering state. */
HWND StudioViewportCreate(HWND parent, HINSTANCE instance);
void StudioViewportReset(HWND viewport);
void StudioViewportSetScene(HWND viewport, const StudioScene *scene, BOOL frame);
void StudioViewportSelect(HWND viewport, int index);
void StudioViewportRefreshImages(HWND viewport);
BOOL StudioViewportDropPoint(HWND viewport, POINT screen, double position[3]);

#endif
