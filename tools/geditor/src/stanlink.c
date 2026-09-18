/* Stan links belong to directed perimeter edges, not whole polygons.
 * Match native endpoints in reverse order, including Y so stacked floors
 * and the two sides of a stair riser are never confused. */
#include <string.h>
#include "stanload.h"

static unsigned short StanLinkRead16(const unsigned char *p)
{
    return (unsigned short)((unsigned int)p[0] << 8 | p[1]);
}

static const unsigned char *StanLinkPoint(const StanFile *stan, DWORD tile, unsigned int point)
{
    return stan->data + stan->tiles[tile].sourceoffset + 8u + point * 8u;
}

static BOOL StanLinkValidateTile(const StanFile *stan, DWORD index)
{
    const StanTile *tile = stan->tiles + index;
    DWORD size = 8u + tile->pointcount * 8u;
    const unsigned char *raw;
    if (tile->pointcount < 3 || tile->pointcount > STAN_TILE_MAX_POINTS
        || tile->sourceoffset > stan->size || size > stan->size - tile->sourceoffset) { return FALSE; }
    raw = stan->data + tile->sourceoffset;
    if ((raw[6] >> 4) != tile->pointcount) { return FALSE; }
    for (unsigned int point = 0; point < tile->pointcount; point++)
    {
        if (StanLinkRead16(StanLinkPoint(stan, index, point) + 6) != tile->points[point].link) { return FALSE; }
    }
    return TRUE;
}

/* The caller has matched these two directed edges. Validate both records and
 * link words before writing either side, including one-way link repairs. */
static BOOL StanLinkEdgePair(StanFile *stan, const DWORD tileindices[2], const unsigned int edges[2],
    BOOL *changedout, const char **reasonout)
{
    unsigned short links[2];
    const char *why = "";
    *changedout = FALSE;
    for (int side = 0; side < 2; side++)
    {
        DWORD target = tileindices[1 - side], base = stan->tiles[0].sourceoffset;
        DWORD offset = stan->tiles[target].sourceoffset, encoded;
        if (!StanLinkValidateTile(stan, tileindices[side]))
        { why = "The selected stan tile records are inconsistent."; goto fail; }
        if (offset < base || ((offset - base) & 7u) || (offset - base) / 8u > 0xffefu)
        { why = "The stan tile is outside the native edge-link range."; goto fail; }
        encoded = (offset - base) / 8u + 0x10u;
        links[side] = (unsigned short)encoded;
        if (StanLinkedTile(stan, links[side]) != target)
        { why = "The stan tile offsets are inconsistent."; goto fail; }
    }
    for (int side = 0; side < 2; side++)
    {
        unsigned short old = stan->tiles[tileindices[side]].points[edges[side]].link;
        if (old >= 0x10 && old != links[side])
        { why = "The shared edge already links to a different stan tile."; goto fail; }
    }
    /* Validate both sides before touching either word. Already-linked pairs
     * and one-way pairs respectively become a no-op and a reciprocal repair. */
    for (int side = 0; side < 2; side++)
    {
        StanTile *tile = stan->tiles + tileindices[side];
        unsigned char *raw = stan->data + tile->sourceoffset + 8u + edges[side] * 8u + 6u;
        if (tile->points[edges[side]].link == links[side]) { continue; }
        tile->points[edges[side]].link = links[side];
        raw[0] = (unsigned char)(links[side] >> 8); raw[1] = (unsigned char)links[side];
        *changedout = TRUE;
    }
    if (*changedout) { stan->dirty = TRUE; }
    *reasonout = "";
    return TRUE;
fail:
    *reasonout = why;
    return FALSE;
}

BOOL StanLinkTiles(StanFile *stan, DWORD first, DWORD second,
    BOOL *changedout, const char **reasonout)
{
    DWORD tileindices[2] = {first, second};
    unsigned int edges[2] = {0}, matches = 0;
    const char *why = "";
    *changedout = FALSE;
    if (!stan || !stan->data || !stan->tiles || first >= stan->tilecount
        || second >= stan->tilecount || first == second)
    { why = "Select two different stan tiles in Face mode."; goto fail; }
    if (!StanLinkValidateTile(stan, first) || !StanLinkValidateTile(stan, second))
    { why = "The selected stan tile records are inconsistent."; goto fail; }
    for (unsigned int a = 0; a < stan->tiles[first].pointcount; a++)
    {
        const unsigned char *a0 = StanLinkPoint(stan, first, a);
        const unsigned char *a1 = StanLinkPoint(stan, first, (a + 1) % stan->tiles[first].pointcount);
        if (!memcmp(a0, a1, 6)) { continue; } /* A collapsed edge cannot be crossed. */
        for (unsigned int b = 0; b < stan->tiles[second].pointcount; b++)
        {
            const unsigned char *b0 = StanLinkPoint(stan, second, b);
            const unsigned char *b1 = StanLinkPoint(stan, second, (b + 1) % stan->tiles[second].pointcount);
            if (!memcmp(a0, b1, 6) && !memcmp(a1, b0, 6))
            { edges[0] = a; edges[1] = b; matches++; }
        }
    }
    if (!matches)
    {
        why = "The tiles do not share a complete edge with matching endpoints and opposite directions. Align their boundary vertices first.";
        goto fail;
    }
    if (matches != 1)
    { why = "The tiles share more than one edge, so the crossing is ambiguous. Select tiles with one shared boundary edge."; goto fail; }
    return StanLinkEdgePair(stan, tileindices, edges, changedout, reasonout);
fail:
    *reasonout = why;
    return FALSE;
}

BOOL StanLinkEdgeTiles(StanFile *stan, const StanEdgeRef *edge,
    BOOL *changedout, const char **reasonout)
{
    DWORD tileindices[2] = {0};
    unsigned int edges[2] = {0}, matches = 0;
    const unsigned char *a0, *a1;
    const char *why = "";
    *changedout = FALSE;
    if (!stan || !stan->data || !stan->tiles || !edge || edge->tile >= stan->tilecount)
    { why = "Select a stan tile edge in Edge mode."; goto fail; }
    if (!StanLinkValidateTile(stan, edge->tile))
    { why = "The selected stan tile record is inconsistent."; goto fail; }
    if (edge->point >= stan->tiles[edge->tile].pointcount)
    { why = "The selected stan edge no longer exists."; goto fail; }
    tileindices[0] = edge->tile; edges[0] = edge->point;
    a0 = StanLinkPoint(stan, edge->tile, edge->point);
    a1 = StanLinkPoint(stan, edge->tile, (edge->point + 1) % stan->tiles[edge->tile].pointcount);
    if (!memcmp(a0, a1, 6))
    { why = "A collapsed stan edge cannot be linked."; goto fail; }
    for (DWORD tile = 0; tile < stan->tilecount; tile++)
    {
        if (tile == edge->tile) { continue; }
        if (!StanLinkValidateTile(stan, tile))
        { why = "The stan tile records are inconsistent."; goto fail; }
        for (unsigned int point = 0; point < stan->tiles[tile].pointcount; point++)
        {
            const unsigned char *b0 = StanLinkPoint(stan, tile, point);
            const unsigned char *b1 = StanLinkPoint(stan, tile, (point + 1) % stan->tiles[tile].pointcount);
            if (memcmp(a0, b1, 6) || memcmp(a1, b0, 6)) { continue; }
            tileindices[1] = tile; edges[1] = point;
            if (++matches > 1)
            {
                why = "More than one stan edge matches this boundary. Select the two tiles in Face mode and use Link Stan Tiles.";
                goto fail;
            }
        }
    }
    if (!matches)
    {
        why = "No other stan tile shares this complete edge with matching endpoints and opposite direction. Align the boundary vertices, including height, first.";
        goto fail;
    }
    return StanLinkEdgePair(stan, tileindices, edges, changedout, reasonout);
fail:
    *reasonout = why;
    return FALSE;
}
