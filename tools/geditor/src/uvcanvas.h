#ifndef GEDITOR_UVCANVAS_H
#define GEDITOR_UVCANVAS_H

#include <windows.h>
#include "bgdocument.h"

#define UVCANVAS_BACKGROUND RGB(40, 40, 40)

/* A 2D UV workspace with its own pan/zoom camera. Registers its window
   class on first use; the parent owns the returned child window. */
HWND UVCanvasCreate(HWND parent, HINSTANCE instance);

typedef struct UVCanvasTriangle {
    double uv[3][2]; /* normalized, unwrapped UV coordinates at each corner */
    BgDocumentUVEdit source[3];
    int width, height;
    int nodes[3]; /* canvas-owned mapping to unique shared source vertices */
} UVCanvasTriangle;

#define UVCANVAS_WM_SELECTION_CHANGED (WM_APP + 40)
#define UVCANVAS_WM_COMMIT (WM_APP + 41)
typedef struct UVCanvasEdit {
    const BgDocumentUVEdit *vertices;
    DWORD count;
} UVCanvasEdit;

/* Takes ownership of a malloc'd triangle array, replacing the previous
   overlay. NULL/0 clears it. Retains selection by shared source identity
   and leaves the camera unchanged. FALSE clears the overlay on allocation failure. */
BOOL UVCanvasSetTriangles(HWND canvas, UVCanvasTriangle *triangles, int count);
int UVCanvasGetSelection(HWND canvas, double uv[2]);
BOOL UVCanvasSetPosition(HWND canvas, const double uv[2], const char **reason);
BOOL UVCanvasCancelInteraction(HWND canvas);

#endif /* GEDITOR_UVCANVAS_H */
