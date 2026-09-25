#include <math.h>
#include <float.h>
#include <src/propconstants.h>
#include <src/game/glassmath.h>
#include "glasspreview.h"

int GlassPreviewAlpha(const GlassPreview *glass, const float eye[3])
{
    float x = glass->position[0] - eye[0], y = glass->position[1] - eye[1], z = glass->position[2] - eye[2];
    return glass->active ? glassOpacityAtDistance(sqrtf(x*x + y*y + z*z),
        glass->tintdistance, glass->opaquedistance, glass->minimumopacity) : 0;
}
static void Cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1]*b[2] - a[2]*b[1]; out[1] = a[2]*b[0] - a[0]*b[2]; out[2] = a[0]*b[1] - a[1]*b[0];
}
static float Dot(const float a[3], const float b[3]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static void Point(const BgPortal *portal, int i, float out[3])
{ out[0] = portal->nativepoints[i].x; out[1] = portal->nativepoints[i].y; out[2] = portal->nativepoints[i].z; }
int GlassFindPortal(const SetupFile *setup, DWORD index, const BgPortalFile *portals, float levelscale)
{
    if (!setup || !portals || !portals->portals || index >= setup->objectcount
        || !isfinite(levelscale) || levelscale <= 0) return -1;
    const SetupObject *object = &setup->objects[index];
    if (object->deleted || object->type != PROPDEF_TINTED_GLASS || object->pad < 10000
        || (DWORD)(object->pad - 10000) >= setup->boundpadcount) return -1;
    const SetupBoundPad *bound = &setup->boundpads[object->pad - 10000];
    const SetupPad *pad = &bound->pad;
    float side[3], from[3], to[3], diff[3];
    Cross(pad->up, pad->look, side);
    float length = sqrtf(Dot(side, side));
    if (!(length > 0)) return -1;
    for (int a = 0; a < 3; a++)
    {
        float center = (pad->pos[a] + ((bound->xmin + bound->xmax)*side[a]/length
            + (bound->ymin + bound->ymax)*pad->up[a] + (bound->zmin + bound->zmax)*pad->look[a])*.5f) / levelscale;
        from[a] = center - 10.0f*pad->up[a]; to[a] = center + 10.0f*pad->up[a];
        diff[a] = to[a] - from[a];
    }
    int best = -1; float bestdistance = FLT_MAX;
    for (DWORD p = 0; p < portals->portalcount; p++)
    {
        const BgPortal *portal = &portals->portals[p];
        float v0[3], v1[3], v2[3], edge1[3], edge2[3], normal[3], plane = FLT_MAX;
        if (portal->pointcount < 3 || portal->pointcount > BG_PORTAL_MAX_POINTS) continue;
        Point(portal, 0, v0); Point(portal, 1, v1); Point(portal, 2, v2);
        for (int a = 0; a < 3; a++) { edge1[a] = v0[a] - v1[a]; edge2[a] = v2[a] - v1[a]; }
        Cross(edge1, edge2, normal); length = sqrtf(Dot(normal, normal));
        if (!(length > 0)) continue;
        for (int a = 0; a < 3; a++) normal[a] /= length;
        for (int i = 0; i < portal->pointcount; i++) { Point(portal, i, v0); plane = fminf(plane, Dot(v0, normal)); }
        float value1 = Dot(from, normal)*levelscale, value2 = Dot(to, normal)*levelscale;
        if ((value1 < plane && value2 < plane) || (value1 > plane && value2 > plane)) continue;
        BOOL seenA = FALSE, seenB = FALSE, hit = TRUE;
        for (int i = 0; i < portal->pointcount; i++)
        {
            float cross[3];
            Point(portal, i, v0); Point(portal, (i + 1)%portal->pointcount, v1);
            for (int a = 0; a < 3; a++) edge1[a] = v1[a] - v0[a];
            Cross(edge1, diff, cross);
            if (Dot(cross, cross) == 0) { hit = FALSE; break; }
            if (Dot(cross, from)*levelscale < Dot(cross, v0)) seenA = TRUE; else seenB = TRUE;
            if (seenA && seenB) { hit = FALSE; break; }
        }
        float distance = fabsf((value1 + value2)*.5f - plane);
        if (hit && distance < bestdistance) { bestdistance = distance; best = (int)p; }
    }
    return best;
}
