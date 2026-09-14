/* Author new portals without changing existing script-facing table indices. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include "bgdocument.h"

BOOL BgDocumentAddPortal(BgDocument *document, const BgPortalPlacement *placement,
                         DWORD *indexout, const char **reasonout)
{
    static const int signs[4][2] = {{-1,-1}, {1,-1}, {1,1}, {-1,1}};
    BgPortal portal = {0}, *grown;
    DWORD count;
    int horizontal, vertical;
    float extent[2], area;
    *indexout = BG_PORTAL_INDEX_NONE;
    *reasonout = "This background does not have an editable portal table.";
    if (!document || !document->rooms || document->portalwarning || !placement
        || !isfinite(document->levelscale) || document->levelscale <= 0) { return FALSE; }
    count = document->portals.portalcount;
    if (count && !document->portals.portals) { return FALSE; }
    if (count >= BG_MAX_PORTALS - 1)
    { *reasonout = "This level has reached the limit of 199 portals."; return FALSE; }
    if (!placement->room1 || !placement->room2 || placement->room1 > document->roomcount
        || placement->room2 > document->roomcount || placement->room1 > 255 || placement->room2 > 255)
    { *reasonout = "Choose two existing rooms from this level."; return FALSE; }
    if (placement->room1 == placement->room2)
    { *reasonout = "A portal must connect two different rooms."; return FALSE; }
    if (placement->plane < BG_PORTAL_XY || placement->plane > BG_PORTAL_XZ
        || !isfinite(placement->width) || !isfinite(placement->height)
        || placement->width <= 0 || placement->height <= 0)
    { *reasonout = "Choose a portal orientation and positive width and height."; return FALSE; }
    horizontal = placement->plane == BG_PORTAL_YZ ? 2 : 0;
    vertical = placement->plane == BG_PORTAL_XZ ? 2 : 1;
    portal.geometryoffset = BG_PORTAL_NEW_GEOMETRY | count;
    portal.connectedroom1 = (unsigned char)placement->room1;
    portal.connectedroom2 = (unsigned char)placement->room2;
    portal.pointcount = 4;
    for (int point = 0; point < 4; point++)
    {
        float coords[3];
        for (int axis = 0; axis < 3; axis++)
        {
            double world = placement->center[axis]
                + (axis == horizontal ? signs[point][0] * placement->width * .5 : 0)
                + (axis == vertical ? signs[point][1] * placement->height * .5 : 0);
            double native = world * document->levelscale;
            if (!isfinite(world) || !isfinite(native) || fabs(world) > FLT_MAX || fabs(native) > FLT_MAX)
            { *reasonout = "The portal coordinates are outside the supported range."; return FALSE; }
            coords[axis] = (float)world;
            native = (double)coords[axis] * document->levelscale;
            if (!isfinite(native) || fabs(native) > FLT_MAX)
            { *reasonout = "The portal coordinates are outside the supported range."; return FALSE; }
        }
        portal.points[point] = (BgPortalPoint){coords[0], coords[1], coords[2]};
    }
    /* Reject widths lost to float rounding in either the viewport or ROM. */
    for (int dimension = 0; dimension < 2; dimension++)
    {
        int axis = dimension ? vertical : horizontal, corner = dimension ? 3 : 1;
        const BgPortalPoint *a = &portal.points[0], *b = &portal.points[corner];
        float lo = axis == 0 ? a->x : axis == 1 ? a->y : a->z;
        float hi = axis == 0 ? b->x : axis == 1 ? b->y : b->z;
        if (!(hi > lo) || !((float)((double)hi * document->levelscale)
                           > (float)((double)lo * document->levelscale)))
        { *reasonout = "The portal is too small at these coordinates."; return FALSE; }
        extent[dimension] = (float)((double)hi * document->levelscale)
            - (float)((double)lo * document->levelscale);
    }
    /* bgCalcPortalPlane normalizes a float cross product, including its
     * squared length. Do not author a plane that overflows or underflows it. */
    area = extent[0] * extent[1];
    if (!isfinite(area * area) || !(area * area > 0))
    { *reasonout = "The portal dimensions are outside the supported range."; return FALSE; }
    grown = realloc(document->portals.portals, (count + 1) * sizeof(*grown));
    if (!grown) { *reasonout = "Out of memory adding the portal."; return FALSE; }
    grown[count] = portal;
    document->portals.portals = grown;
    document->portals.portalcount = count + 1;
    document->dirty = TRUE;
    *indexout = count;
    *reasonout = "";
    return TRUE;
}
