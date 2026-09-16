/* Author, reshape and remove portal connections. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

/* Native visibility operand converted by bgLoadFile to a portal index. */
#define BG_VIS_PORTAL_REFERENCE 0x64

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
    /* Table positions can change after deletion. Never alias another live
     * editor polygon when reusing a temporary identity. */
    for (DWORD attempt = 0; attempt < BG_MAX_PORTALS; attempt++)
    {
        BOOL used = FALSE;
        portal.geometryoffset = BG_PORTAL_NEW_GEOMETRY | ((count + attempt) % BG_MAX_PORTALS);
        for (DWORD i = 0; i < count; i++)
        { if (document->portals.portals[i].geometryoffset == portal.geometryoffset) { used = TRUE; break; } }
        if (!used) { break; }
    }
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
        portal.nativepoints[point] = (BgPortalPoint){
            (float)((double)coords[0] * document->levelscale),
            (float)((double)coords[1] * document->levelscale),
            (float)((double)coords[2] * document->levelscale)};
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

static DWORD BgPortalRead32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3];
}

BOOL BgDocumentDeletePortals(BgDocument *document, const BgFile *source,
    const DWORD *indices, DWORD count, const char **reasonout)
{
    unsigned char removed[BG_MAX_PORTALS] = {0};
    DWORD total, kept = 0, vis;
    *reasonout = "The selected portals could not be read.";
    if (!document || document->portalwarning || !document->portals.portals
        || !indices || !count || !source || !source->data || source->size < 16) { return FALSE; }
    total = document->portals.portalcount;
    if (total >= BG_MAX_PORTALS || count > total) { return FALSE; }
    for (DWORD i = 0; i < count; i++)
    {
        if (indices[i] >= total || removed[indices[i]]) { return FALSE; }
        removed[indices[i]] = 1;
    }
    /* The runtime resolves polygon-address operands to portal indices. A
     * missing polygon silently resolves to portal 0, changing the script.
     * Keep those references valid; deleting one of several aliases is safe. */
    vis = BgPortalRead32(source->data + 12) & 0x00ffffffu;
    if (vis)
    {
        for (;; vis += 8)
        {
            DWORD geometry;
            BOOL deleted = FALSE, survives = FALSE;
            if (vis > source->size || source->size - vis < 8)
            { *reasonout = "The background visibility script is invalid; portals cannot be deleted safely."; return FALSE; }
            if (!source->data[vis]) { break; }
            if (source->data[vis] != BG_VIS_PORTAL_REFERENCE) { continue; }
            geometry = BgPortalRead32(source->data + vis + 4) & 0x00ffffffu;
            for (DWORD i = 0; i < total; i++)
            {
                DWORD address = document->portals.portals[i].geometryoffset;
                if (address & BG_PORTAL_NEW_GEOMETRY)
                {
                    DWORD slot = address & ~BG_PORTAL_NEW_GEOMETRY;
                    address = slot < BG_MAX_PORTALS ? source->newportaloffsets[slot] : 0;
                }
                if (address != geometry) { continue; }
                if (removed[i]) { deleted = TRUE; } else { survives = TRUE; }
            }
            if (deleted && !survives)
            {
                *reasonout = "A selected portal is referenced by the level's visibility script. "
                    "Its last connection cannot be deleted until that script is updated.";
                return FALSE;
            }
        }
    }
    for (DWORD i = 0; i < total; i++)
    { if (!removed[i]) { document->portals.portals[kept++] = document->portals.portals[i]; } }
    document->portals.portalcount = kept;
    document->dirty = TRUE;
    *reasonout = "";
    return TRUE;
}

/* Portal points are floats, unlike quantized BG vertices. Resolve and validate
 * the complete edit first, including aliases of a shared native polygon. */
BOOL BgDocumentTranslatePortalPoints(BgDocument *document, const BgPortalPointRef *refs,
    DWORD count, const double offset[3], DWORD *movedout, const char **reasonout)
{
    unsigned char masks[BG_MAX_PORTALS] = {0};
    BgPortal *copy;
    DWORD moved = 0, total;
    *movedout = 0;
    *reasonout = "There are no editable selected portal points.";
    if (!document || document->portalwarning || !refs || !count || !offset
        || !document->portals.portals || !isfinite(document->levelscale)
        || document->levelscale <= 0) { return FALSE; }
    total = document->portals.portalcount;
    if (total >= BG_MAX_PORTALS) { return FALSE; }
    for (DWORD i = 0; i < total; i++)
    {
        if (document->portals.portals[i].pointcount < 3
            || document->portals.portals[i].pointcount > BG_PORTAL_MAX_POINTS) { return FALSE; }
    }
    for (int axis = 0; axis < 3; axis++)
    {
        if (!isfinite(offset[axis]))
        { *reasonout = "Enter a finite portal displacement."; return FALSE; }
    }
    for (DWORD r = 0; r < count; r++)
    {
        if (refs[r].portal >= total || refs[r].point >= document->portals.portals[refs[r].portal].pointcount
            || refs[r].point >= BG_PORTAL_MAX_POINTS) { return FALSE; }
        DWORD geometry = document->portals.portals[refs[r].portal].geometryoffset;
        for (DWORD i = 0; i < total; i++)
        {
            if (document->portals.portals[i].geometryoffset != geometry) { continue; }
            if (document->portals.portals[i].pointcount != document->portals.portals[refs[r].portal].pointcount)
            { return FALSE; }
            masks[i] |= 1u << refs[r].point;
        }
    }
    copy = malloc(total * sizeof(*copy));
    if (!copy) { *reasonout = "Out of memory moving portal points."; return FALSE; }
    memcpy(copy, document->portals.portals, total * sizeof(*copy));
    for (DWORD i = 0; i < total; i++)
    {
        for (DWORD point = 0; point < copy[i].pointcount; point++)
        {
            float world[3], native[3];
            BgPortalPoint *p = &copy[i].points[point], *n = &copy[i].nativepoints[point];
            if (!(masks[i] & (1u << point))) { continue; }
            world[0] = p->x; world[1] = p->y; world[2] = p->z;
            native[0] = n->x; native[1] = n->y; native[2] = n->z;
            for (int axis = 0; axis < 3; axis++)
            {
                double value;
                if (offset[axis] == 0) { continue; } /* Preserve untouched native bits. */
                value = ((double)world[axis] + offset[axis]) * document->levelscale;
                if (!isfinite(value) || fabs(value) > FLT_MAX
                    || fabs((double)(float)value * (1.0f / document->levelscale)) > FLT_MAX)
                {
                    free(copy);
                    *reasonout = "The portal coordinates are outside the supported range.";
                    return FALSE;
                }
                native[axis] = (float)value;
                world[axis] = native[axis] * (1.0f / document->levelscale);
            }
            BgPortalPoint result = {native[0], native[1], native[2]};
            if (memcmp(n, &result, sizeof(result)))
            {
                *n = result;
                *p = (BgPortalPoint){world[0], world[1], world[2]};
                moved++;
            }
        }
    }
    if (moved)
    {
        memcpy(document->portals.portals, copy, total * sizeof(*copy));
        document->dirty = TRUE;
    }
    free(copy);
    *movedout = moved;
    *reasonout = "";
    return TRUE;
}
