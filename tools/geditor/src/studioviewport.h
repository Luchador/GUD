#ifndef GEDITOR_STUDIOVIEWPORT_H
#define GEDITOR_STUDIOVIEWPORT_H
#include <windows.h>
#include "studioscene.h"
#include "texload.h"

#define STUDIO_WM_SELECT (WM_APP + 140) /* wparam: shared scene selection ID; lparam: material, -1 clears */
#define STUDIO_WM_TRANSFORM (WM_APP + 141) /* 0 preview, 1 commit (lparam: old StudioTransform), 2 cancel */
#define STUDIO_WM_LIGHT_TRANSFORM (WM_APP + 142) /* Same phases; lparam: old StudioLight. */

/* Independent preview context and camera; no game assets or rendering state. */
HWND StudioViewportCreate(HWND parent, HINSTANCE instance);
void StudioViewportReset(HWND viewport);
void StudioViewportSetScene(HWND viewport, StudioScene *scene, BOOL frame);
void StudioViewportSelect(HWND viewport, int index);
void StudioViewportRefreshImages(HWND viewport);
BOOL StudioViewportDropPoint(HWND viewport, POINT screen, double position[3]);
void StudioViewportSetTool(HWND viewport,int tool);
BOOL StudioViewportCancelTransform(HWND viewport);
void StudioViewportCommitTransform(HWND viewport);

/* Caller frees bottom-up RGBA output. Uses the scene camera and render size. */
BOOL StudioViewportRender(HWND viewport,TexPixel **pixels,const char **why);

#endif
