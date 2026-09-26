#include <float.h>
#include <math.h>
#include <string.h>
#include "objectload.h"
#include "setupselection.h"
#include <src/propconstants.h>

static BOOL ObjectModelCenter(const SetupObjectGeometry *geometry, DWORD id, double center[3])
{
    double min[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
    double max[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
    if (!geometry || !geometry->tris || !geometry->objectindices) { return FALSE; }
    for (DWORD t = 0; t < geometry->tricount; t++) if (geometry->objectindices[t] == id)
        for (int c = 0; c < 3; c++)
        {
            const BgVertex *v = geometry->tris + t * 3 + c;
            double point[3] = {v->x, v->y, v->z};
            for (int a = 0; a < 3; a++)
            { min[a] = fmin(min[a], point[a]); max[a] = fmax(max[a], point[a]); }
        }
    if (min[0] > max[0]) { return FALSE; }
    for (int a = 0; a < 3; a++) { center[a] = (min[a] + max[a]) * .5; }
    return TRUE;
}

BOOL ObjectTransformSetupModels(const char *projectdir, SetupFile *setup, const StanFile *stan,
    float levelscale, const SetupObjectGeometry *before, const DWORD *ids, DWORD count,
    const double offset[3], const Rotation *rotation, const double pivot[3], const Scaling *scaling,
    SetupObjectGeometry *out, const char **why)
{
    SetupObjectGeometry result = {0}, placed = {0};
    *out = (SetupObjectGeometry){0};
    *why = "The selected models cannot use this transform.";
    if (!ids || !count || count > 1000000u || (!!offset + !!rotation + !!scaling) != 1
        || (rotation && (!pivot || !RotationValid(rotation)))
        || (scaling && !ScalingValid(scaling))) { return FALSE; }
    /* Validate the entire selection before changing any member. */
    for (DWORD i = 0; i < count; i++)
    {
        SetupPadRef ref;
        double center[3];
        if (!SetupSelectionValid(setup, ids[i]) || !SetupFileGetModelPad(setup, ids[i], &ref)
            || !ObjectModelCenter(before, ids[i], center)) { return FALSE; }
        if (ids[i] & SETUP_CHARACTER_SELECTION_BIT)
        {
            if (scaling) { *why = "Characters cannot be scaled."; return FALSE; }
            if (rotation && (fabs(rotation->m[1][1] - 1) > 1e-6
                || fabs(rotation->m[0][1]) > 1e-6 || fabs(rotation->m[2][1]) > 1e-6))
            { *why = "Selections containing characters support Y-axis rotation only."; return FALSE; }
        }
        else if (setup->objects[ids[i]].type == PROPDEF_DOOR_SHADOW)
        { *why = "Door shadows follow their doors and cannot be transformed directly."; return FALSE; }
        for (DWORD j = 0; j < i; j++) if (ids[j] == ids[i]) { return FALSE; }
    }
    for (DWORD i = 0; i < count; i++)
    {
        ObjectGeometryFree(&result);
        if (scaling && count > 1)
        {
            SetupPadRef ref;
            Rotation axes;
            Scaling local;
            double center[3], move[3];
            if (!SetupFileGetModelPad(setup, ids[i], &ref)
                || !SetupFilePadRotation(setup, &ref, &axes)
                || !ObjectModelCenter(before, ids[i], center)) { goto fail; }
            ScalingGroupMember(scaling, &axes, center, &local, move);
            if (!ObjectScaleSetupModel(projectdir, setup, stan, levelscale, before, ids[i], &local, &placed, why)
                || !ObjectTranslateSetupModel(projectdir, setup, stan, levelscale, &placed, ids[i], move, &result, why))
            { goto fail; }
            ObjectGeometryFree(&placed);
        }
        else if (!(offset ? ObjectTranslateSetupModel(projectdir, setup, stan, levelscale, before, ids[i], offset, &result, why)
            : rotation ? ObjectRotateSetupModel(projectdir, setup, stan, levelscale, before, ids[i], rotation, pivot, &result, why)
            : ObjectScaleSetupModel(projectdir, setup, stan, levelscale, before, ids[i], scaling, &result, why))) { goto fail; }
    }
    *out = result;
    *why = "";
    return TRUE;
fail:
    ObjectGeometryFree(&placed);
    ObjectGeometryFree(&result);
    return FALSE;
}
