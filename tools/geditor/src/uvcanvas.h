#ifndef GEDITOR_UVCANVAS_H
#define GEDITOR_UVCANVAS_H

#include <windows.h>
#include "bgdocument.h"
#include "uvprojection.h"
#include "edittool.h"
#include "texload.h"

#define UVCANVAS_BACKGROUND RGB(40, 40, 40)

/* A 2D UV workspace with its own pan/zoom camera. Registers its window
   class on first use; the parent owns the returned child window. */
HWND UVCanvasCreate(HWND parent, HINSTANCE instance);

typedef struct UVCanvasTriangle {
    BgFaceRef face;
    double uv[3][2]; /* normalized, unwrapped UV coordinates at each corner */
    double position[3][3]; /* world (BG) or model rest-pose positions for projection */
    BgDocumentUVEdit source[3];
    unsigned char seams; /* marked geometric edges, one bit per corner */
    int width, height;
    int nodes[3]; /* canvas-owned mapping to unique shared source vertices */
} UVCanvasTriangle;

#define UVCANVAS_WM_SELECTION_CHANGED (WM_APP + 40)
#define UVCANVAS_WM_COMMIT (WM_APP + 41)
typedef struct UVCanvasEdit {
    const BgDocumentUVEdit *vertices;
    DWORD count;
    const char *action; /* NULL uses the default move action */
} UVCanvasEdit;

#define UVCANVAS_WM_COMMIT_FACES (WM_APP + 60)
typedef struct UVCanvasFaceEdit {
    const BgDocumentFaceUVEdit *faces;
    DWORD count;
    const char *action;
} UVCanvasFaceEdit;

/* Takes ownership of a malloc'd triangle array, replacing the previous
   overlay. NULL/0 clears it. Retains selection by shared source identity
   and leaves the camera unchanged. FALSE clears the overlay on allocation failure. */
BOOL UVCanvasSetTriangles(HWND canvas, UVCanvasTriangle *triangles, int count);
/* Takes ownership of native-order RGBA pixels. NULL clears the background.
 * The image tiles with the camera, at half opacity outside the 0-1 UV square.
 * Invalid sizes or allocation failure clear the preview and return FALSE.
 * No selection/edit notifications are sent. Opacity is presentation-only. */
BOOL UVCanvasSetTexture(HWND canvas, TexPixel *pixels, int width, int height);
void UVCanvasSetTextureOpacity(HWND canvas, int percent);
int UVCanvasGetSelection(HWND canvas, double uv[2]);
/* Pointer-free source identities, owned by the caller after capture. */
BOOL UVCanvasCaptureSelection(HWND canvas, void **data, size_t *size);
BOOL UVCanvasRestoreSelection(HWND canvas, const void *data, size_t size);
/* Set one normalized coordinate on every selected source vertex. */
BOOL UVCanvasSetCoordinate(HWND canvas, int axis, double value, const char **reason);
void UVCanvasSetTransformMode(HWND canvas, TransformMode mode);
/* Current drag values: UV offsets, degrees, or U/V scale factors. */
TransformMode UVCanvasGetTransform(HWND canvas, double values[2], BOOL *limited);
/* Apply an angle (values[0]) or U/V factors about the selection's mean UV.
 * Requires two or more shared source vertices; one history step per call. */
BOOL UVCanvasApplyTransform(HWND canvas, const double values[2], const char **reason);
BOOL UVCanvasCancelInteraction(HWND canvas);
BOOL UVCanvasHasFaces(HWND canvas);
/* Zero keeps fit-to-0-1 mapping; positive sizes are world units per texel. */
BOOL UVCanvasProjectFaces(HWND canvas, UVProjection projection, double unitspertexel, const char **reason);
BOOL UVCanvasProjectCylinder(HWND canvas, int axis, double unitspertexel, const char **reason);

#endif /* GEDITOR_UVCANVAS_H */
