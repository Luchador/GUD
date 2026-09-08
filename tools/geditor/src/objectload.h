#ifndef GEDITOR_OBJECTLOAD_H
#define GEDITOR_OBJECTLOAD_H

#include <windows.h>

#include "bgload.h"
#include "setupload.h"
#include "stanload.h"

/* One level's placed prop and character models. The occupied arrays
   parallel SetupFile's pad arrays and tell the overlay not to draw a
   marker where a model was successfully loaded. */
typedef struct SetupObjectGeometry {
    BgVertex *tris;
    unsigned short *tritags;
    DWORD *objectindices;       /* prop index, or SETUP_CHARACTER_SELECTION_BIT | character index */
    DWORD tricount;
    unsigned char *occupiedpads;
    unsigned char *occupiedboundpads;
    DWORD objectcount;
} SetupObjectGeometry;

/* Recreates initial setup placement using stan links, authored model boxes,
   placement flags, and previously placed solid props. NULL stan retains a
   pad-based preview for projects with missing collision data. All placement
   is derived: the setup's authored pads and raw project bytes stay intact. */
BOOL ObjectLoadSetupGeometry(const char *projectdir, const SetupFile *setup,
                             const StanFile *stan, float levelscale,
                             SetupObjectGeometry *out,
                             const char **reasonout);
/* Moves a selected prop or tagged character as one model. Character pad
   height selects a floor; the rendered feet follow that floor's height. */
BOOL ObjectTranslateSetupModel(const char *projectdir, SetupFile *setup,
    const StanFile *stan, float levelscale, const SetupObjectGeometry *before,
    DWORD index, const double offset[3], SetupObjectGeometry *out,
    const char **reasonout);

void ObjectGeometryFree(SetupObjectGeometry *geometry);

/* Rotate one model through its private placement pad; caller owns rollback. */
BOOL ObjectRotateSetupModel(const char *projectdir, SetupFile *setup,
    const StanFile *stan, float levelscale, const SetupObjectGeometry *before,
    DWORD index, const Rotation *rotation, const double pivot[3], SetupObjectGeometry *out,
    const char **reasonout);

BOOL ObjectGetSetupModelRotation(const SetupFile *setup, DWORD selection, Rotation *out);

#endif /* GEDITOR_OBJECTLOAD_H */
