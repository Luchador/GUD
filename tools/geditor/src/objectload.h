#ifndef GEDITOR_OBJECTLOAD_H
#define GEDITOR_OBJECTLOAD_H

#include <windows.h>

#include "bgload.h"
#include "bgrender.h"
#include "setupload.h"
#include "stanload.h"
#include "monitorload.h"
#include "glasspreview.h"

/* One level's placed prop and character models. The occupied arrays
   parallel SetupFile's pad arrays and tell the overlay not to draw a
   marker where a model was successfully loaded. */
typedef struct SetupObjectGeometry {
    BgVertex *tris;
    unsigned short *tritags;
    BgRenderFlags *renderflags;
    DWORD *objectindices;       /* prop index, or SETUP_CHARACTER_SELECTION_BIT | character index */
    DWORD tricount;
    unsigned char *occupiedpads;
    unsigned char *occupiedboundpads;
    DWORD objectcount;
    MonitorGeometry monitors;
    GlassPreview *glass; /* Indexed by setup object ID. */
    DWORD glasscount;
} SetupObjectGeometry;

/* Resolves browser names through the game's model tables. Items, heads and
   first-person hands are not standalone setup instances. */
BOOL ObjectResolvePlaceableModel(const char *name, BOOL *character, int *modelid);
/* Model Editor asset name for a viewport selection. Character attachments
   share their owner's selection, so these resolve to the preview body. */
BOOL ObjectGetSetupModelName(const SetupFile *setup, DWORD selection, const char **nameout);

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

/* Clipboard pose: only triangles and selection IDs, not a renderable scene. */
BOOL ObjectCopySetupModelPose(const SetupObjectGeometry *source, DWORD index,
    SetupObjectGeometry *out, const char **reasonout);
/* Keep the copied orientation/scale, centering it over the hit and moving its
 * nearest surface onto the plane. Works on floors, slopes, walls and ceilings. */
BOOL ObjectGetPasteOffset(const SetupObjectGeometry *pose, DWORD index,
    const double position[3], const double normal[3], double offset[3], const char **reasonout);
/* Characters retain their upright facing and ground at the clicked X/Z.
 * The clicked height chooses the destination Stan floor. */
BOOL ObjectGetCharacterPasteOffset(const SetupFile *source, const StanFile *stan,
    float levelscale, DWORD index, const double position[3], double offset[3], const char **reasonout);
/* Copy native properties and the visible pose, then transform only the copy.
 * Exactly one of offset/rotation/scaling is supplied. Caller owns rollback. */
BOOL ObjectDuplicateSetupModel(const char *projectdir, SetupFile *setup,
    const SetupFile *source, const StanFile *stan, float levelscale,
    const SetupObjectGeometry *before, DWORD index, const double offset[3],
    const Rotation *rotation, const double pivot[3], const Scaling *scaling,
    DWORD *selectionout, SetupObjectGeometry *out, const char **reasonout);

/* Rotate one model through its private placement pad; caller owns rollback. */
BOOL ObjectRotateSetupModel(const char *projectdir, SetupFile *setup,
    const StanFile *stan, float levelscale, const SetupObjectGeometry *before,
    DWORD index, const Rotation *rotation, const double pivot[3], SetupObjectGeometry *out,
    const char **reasonout);

BOOL ObjectGetSetupModelRotation(const SetupFile *setup, DWORD selection, Rotation *out);

BOOL ObjectScaleSetupModel(const char *projectdir, SetupFile *setup, const StanFile *stan,
                           float levelscale, const SetupObjectGeometry *before, DWORD index,
                           const Scaling *scale, SetupObjectGeometry *out, const char **reasonout);

/* Transform a selection from its original visible poses. One history
 * transaction surrounds the whole call; the caller rolls back on failure. */
BOOL ObjectTransformSetupModels(const char *projectdir, SetupFile *setup, const StanFile *stan,
    float levelscale, const SetupObjectGeometry *before, const DWORD *ids, DWORD count,
    const double offset[3], const Rotation *rotation, const double pivot[3], const Scaling *scaling,
    SetupObjectGeometry *out, const char **why);

#endif /* GEDITOR_OBJECTLOAD_H */
