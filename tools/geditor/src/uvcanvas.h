#ifndef GEDITOR_UVCANVAS_H
#define GEDITOR_UVCANVAS_H

#include <windows.h>

#define UVCANVAS_BACKGROUND RGB(40, 40, 40)

/* A 2D UV workspace with its own pan/zoom camera. Registers its window
   class on first use; the parent owns the returned child window. */
HWND UVCanvasCreate(HWND parent, HINSTANCE instance);

#endif /* GEDITOR_UVCANVAS_H */
