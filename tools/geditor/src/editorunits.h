#ifndef GEDITOR_EDITORUNITS_H
#define GEDITOR_EDITORUNITS_H
#include <math.h>

typedef enum EditorCoordinateUnits {
    EDITOR_UNITS_NATIVE = 0,
    EDITOR_UNITS_WORLD = 1
} EditorCoordinateUnits;

/* UI values = world values * factor. Geometry, camera and file formats keep
 * their existing units. A missing level uses an identity conversion. */
static inline double EditorUnitsFactor(EditorCoordinateUnits units, double levelscale)
{
    return units == EDITOR_UNITS_NATIVE && isfinite(levelscale) && levelscale > 0
        ? levelscale : 1.0;
}

static inline double EditorUnitsSnap(double worldoffset, double factor)
{
    return round(worldoffset * factor) / factor;
}
#endif
