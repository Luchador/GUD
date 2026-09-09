#ifndef GEDITOR_UVCANVAS_H
#define GEDITOR_UVCANVAS_H

#include <windows.h>

#define UVCANVAS_BACKGROUND RGB(40, 40, 40)

/* A 2D UV workspace with its own pan/zoom camera. Registers its window
   class on first use; the parent owns the returned child window. */
HWND UVCanvasCreate(HWND parent, HINSTANCE instance);

typedef struct UVCanvasTriangle {
    double uv[3][2]; /* normalized, unwrapped UV coordinates at each corner */
} UVCanvasTriangle;

/* Takes ownership of a malloc'd triangle array, replacing the previous
   overlay. NULL/0 clears it. The camera position and zoom are unchanged. */
void UVCanvasSetTriangles(HWND canvas, UVCanvasTriangle *triangles, int count);

#endif /* GEDITOR_UVCANVAS_H */
