#ifndef GEDITOR_BGKNIFE_H
#define GEDITOR_BGKNIFE_H
#include "bgdocument.h"

/* Position is in editor/world units. Direction is the plane's normal. */
typedef struct BgKnifePlane { double position[3], normal[3]; } BgKnifePlane;
BOOL BgKnifeNormalizePlane(const BgKnifePlane *source, BgKnifePlane *out);
/* Atomic edit: keep both halves, return an owned selection of all resulting
 * faces, including selected faces which did not cross the plane. No crossings
 * succeeds with cutcount == 0 and leaves the document untouched. */
BOOL BgDocumentKnifeFaces(BgDocument *document, const BgFaceRef *faces, DWORD count,
    const BgKnifePlane *plane, BgFaceRef **selection, DWORD *selectioncount,
    DWORD *cutcount, const char **reason);
#endif
