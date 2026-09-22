#ifndef GEDITOR_MODELUV_H
#define GEDITOR_MODELUV_H
#include "modeledits.h"
#include "uvcanvas.h"

/* Adapt selected native model faces to the shared UV workspace. Matching
 * native vertices with the same UVs, texture basis and joint-space position
 * share a canvas node. References always identify model corners, never BG. */
BOOL ModelUVBuild(const ModelSource *source, const BgFaceRef *faces, DWORD count,
    const char *project, UVCanvasTriangle **triangles, const char **why);
/* Convert either a vertex transform or a per-face unwrap back to normalized
 * model corners. Only corners in this selection may change. Caller frees out. */
BOOL ModelUVConvert(const UVCanvasTriangle *triangles, DWORD count,
    const UVCanvasEdit *vertices, const UVCanvasFaceEdit *faces,
    ModelUVEdit **out, DWORD *outcount, const char **why);
#endif
