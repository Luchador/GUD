#ifndef GEDITOR_LEVELEXPORT_H
#define GEDITOR_LEVELEXPORT_H
#include "bgdocument.h"
#include "stanload.h"

/* Export the live authored data without saving/mutating the project. Both
 * exports use Y-up world positions in meters and preserve room numbers.
 * Background includes both layers, regardless of viewport visibility.
 * Stans use their native tile colors, opaque and visible from either side. */
BOOL LevelExportBackground(const char *path, const char *projectdir,
    const BgDocument *document, const char **why);
BOOL LevelExportStans(const char *path, const StanFile *stan, const char **why);
#endif
