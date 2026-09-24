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
    const unsigned char *commands; DWORD commandsize, start;
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
    commands = document->viscommandsloaded ? document->viscommands : source->data;
    commandsize = document->viscommandsloaded ? document->viscommandssize : source->size;
    if (document->viscommandsloaded) { vis = 0; }
    start = vis;
    if ((document->viscommandsloaded && commandsize) || (!document->viscommandsloaded && vis))
    {
        for (;; vis += 8)
        {
            DWORD geometry;
            BOOL deleted = FALSE, survives = FALSE;
            if (vis > commandsize || commandsize - vis < 8)
            { *reasonout = "The background visibility script is invalid; portals cannot be deleted safely."; return FALSE; }
            if (!commands[vis]) { break; }
            /* Literal portal indices must retain the same table entry when
             * earlier unrelated entries are deleted. Validate before mutation. */
            if (commands[vis] == 0x65 && vis >= start + 8
                && (commands[vis-8] == 0x1f || commands[vis-8] == 0x22 || commands[vis-8] == 0x23))
            {
                DWORD index = BgPortalRead32(commands + vis + 4);
                if (index >= total || removed[index])
                { *reasonout = "A BG command names a selected portal index. Update that command first."; return FALSE; }
                if (!document->viscommandsloaded)
                { *reasonout = "The visibility stream must be editable to remap its portal indices."; return FALSE; }
            }
            if (commands[vis] != BG_VIS_PORTAL_REFERENCE) { continue; }
            geometry = BgPortalRead32(commands + vis + 4);
            if (!(geometry & BG_PORTAL_NEW_GEOMETRY)) { geometry &= 0xffffffu; }
            for (DWORD i = 0; i < total; i++)
            {
                DWORD address = document->portals.portals[i].geometryoffset;
                if (!(geometry & BG_PORTAL_NEW_GEOMETRY) && (address & BG_PORTAL_NEW_GEOMETRY))
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
    if (document->viscommandsloaded)
    {
        for (DWORD pc = 8; pc + 8 <= document->viscommandssize && document->viscommands[pc]; pc += 8)
        {
            unsigned char *p = document->viscommands + pc;
            if (p[0] != 0x65 || (p[-8] != 0x1f && p[-8] != 0x22 && p[-8] != 0x23)) { continue; }
            DWORD old = BgPortalRead32(p + 4), index = old;
            for (DWORD n = 0; n < old; n++) { if (removed[n]) { index--; } }
            p[4] = (unsigned char)(index >> 24); p[5] = (unsigned char)(index >> 16);
            p[6] = (unsigned char)(index >> 8); p[7] = (unsigned char)index;
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

BOOL BgDocumentCopyPortals(const BgDocument *document, const DWORD *indices,
    DWORD count, BgPortalFile *clipboard, const char **reasonout)
{
    unsigned char selected[BG_MAX_PORTALS] = {0};
    BgPortalFile snapshot = {0};
    *reasonout = "The selected portals could not be read.";
    if (!document || document->portalwarning || !document->portals.portals
        || document->portals.portalcount >= BG_MAX_PORTALS || !clipboard
        || !indices || !count || count > document->portals.portalcount) { return FALSE; }
    for (DWORD i = 0; i < count; i++)
    {
        if (indices[i] >= document->portals.portalcount || selected[indices[i]]) { return FALSE; }
        const BgPortal *portal = &document->portals.portals[indices[i]];
        if (portal->pointcount < 3 || portal->pointcount > BG_PORTAL_MAX_POINTS) { return FALSE; }
        selected[indices[i]] = 1;
    }
    snapshot.portals = malloc(count * sizeof(*snapshot.portals));
    if (!snapshot.portals) { *reasonout = "Out of memory copying portals."; return FALSE; }
    snapshot.portalcount = count;
    for (DWORD i = 0; i < count; i++) { snapshot.portals[i] = document->portals.portals[indices[i]]; }
    BgPortalFileFree(clipboard);
    *clipboard = snapshot;
    *reasonout = "";
    return TRUE;
}

BOOL BgDocumentPastePortals(BgDocument *document, const BgPortalFile *clipboard,
    const double offset[3], DWORD indices[BG_MAX_PORTALS], const char **reasonout)
{
    unsigned char used[BG_MAX_PORTALS] = {0};
    BgPortalPointRef refs[BG_MAX_PORTALS * BG_PORTAL_MAX_POINTS];
    BgDocument pasted = {0};
    BgPortal *combined;
    DWORD count, added, refcount = 0, moved;
    *reasonout = "There are no editable copied portals.";
    if (!document || !document->rooms || document->portalwarning || !clipboard
        || !clipboard->portals || !clipboard->portalcount || !offset || !indices
        || !isfinite(document->levelscale) || document->levelscale <= 0) { return FALSE; }
    count = document->portals.portalcount;
    added = clipboard->portalcount;
    if (count >= BG_MAX_PORTALS || added >= BG_MAX_PORTALS || (count && !document->portals.portals))
    { return FALSE; }
    if (added > BG_MAX_PORTALS - 1 - count)
    { *reasonout = "Pasting these portals would exceed the level limit of 199 portals."; return FALSE; }
    for (DWORD i = 0; i < added; i++)
    {
        const BgPortal *portal = &clipboard->portals[i];
        if (portal->pointcount < 3 || portal->pointcount > BG_PORTAL_MAX_POINTS) { return FALSE; }
        if (!portal->connectedroom1 || !portal->connectedroom2
            || portal->connectedroom1 > document->roomcount || portal->connectedroom2 > document->roomcount
            || portal->connectedroom1 == portal->connectedroom2)
        { *reasonout = "The copied portal must connect two existing, different rooms."; return FALSE; }
        for (DWORD point = 0; point < portal->pointcount; point++)
        {
            const BgPortalPoint *p = &portal->points[point], *n = &portal->nativepoints[point];
            if (!isfinite(p->x) || !isfinite(p->y) || !isfinite(p->z)
                || !isfinite(n->x) || !isfinite(n->y) || !isfinite(n->z))
            { *reasonout = "The copied portal has invalid coordinates."; return FALSE; }
        }
    }
    /* Reserve all live editor identities, including those shifted by deletion.
     * Native offsets cannot collide with these high-bit temporary identities. */
    for (DWORD i = 0; i < count; i++)
    {
        DWORD geometry = document->portals.portals[i].geometryoffset;
        if ((geometry & BG_PORTAL_NEW_GEOMETRY) && (geometry & ~BG_PORTAL_NEW_GEOMETRY) < BG_MAX_PORTALS)
        { used[geometry & ~BG_PORTAL_NEW_GEOMETRY] = 1; }
    }
    combined = malloc((count + added) * sizeof(*combined));
    if (!combined) { *reasonout = "Out of memory pasting portals."; return FALSE; }
    if (count) { memcpy(combined, document->portals.portals, count * sizeof(*combined)); }
    memcpy(combined + count, clipboard->portals, added * sizeof(*combined));
    pasted.portals = (BgPortalFile){combined + count, added};
    pasted.levelscale = document->levelscale;
    for (DWORD i = 0; i < added; i++)
    {
        DWORD previous, slot;
        for (previous = 0; previous < i; previous++)
        { if (clipboard->portals[previous].geometryoffset == clipboard->portals[i].geometryoffset) { break; } }
        if (previous < i)
        { pasted.portals.portals[i].geometryoffset = pasted.portals.portals[previous].geometryoffset; }
        else
        {
            for (slot = 0; slot < BG_MAX_PORTALS && used[slot]; slot++) {}
            /* There are fewer than BG_MAX_PORTALS entries in the entire result. */
            used[slot] = 1;
            pasted.portals.portals[i].geometryoffset = BG_PORTAL_NEW_GEOMETRY | slot;
        }
        for (DWORD point = 0; point < clipboard->portals[i].pointcount; point++)
        { refs[refcount++] = (BgPortalPointRef){i, point}; }
    }
    /* Apply the same float/native-coordinate rules as moving existing portals,
     * but only to the detached snapshot. No live edit occurs until it succeeds. */
    if (!BgDocumentTranslatePortalPoints(&pasted, refs, refcount, offset, &moved, reasonout))
    { free(combined); return FALSE; }
    free(document->portals.portals);
    document->portals.portals = combined;
    document->portals.portalcount = count + added;
    document->dirty = TRUE;
    for (DWORD i = 0; i < added; i++) { indices[i] = count + i; }
    *reasonout = "";
    return TRUE;
}
