#ifndef GEDITOR_STUDIOVIEWPORT_H
#define GEDITOR_STUDIOVIEWPORT_H
#include <windows.h>

/* Independent preview context and camera; no game assets or rendering state. */
HWND StudioViewportCreate(HWND parent, HINSTANCE instance);
void StudioViewportReset(HWND viewport);

#endif
