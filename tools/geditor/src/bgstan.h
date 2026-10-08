#ifndef GEDITOR_BGSTAN_H
#define GEDITOR_BGSTAN_H

#include "bgdocument.h"
#include "stanload.h"

/* One normal stan triangle per selected BG face, in selection order. Uses the
 * face's room and mean vertex RGB. Both geometry layers are supported. Failure
 * leaves the stan unchanged; the BG is always read-only. out holds count tiles. */
BOOL BgCreateStanFromFaces(const BgDocument *bg, const BgFaceRef *faces, DWORD count,
    StanFile *stan, DWORD *out, const char **why);

#endif
