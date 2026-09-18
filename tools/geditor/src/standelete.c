/* Removing variable-length collision tiles also relocates every surviving
 * edge link. Keep the raw file and decoded tiles in step, and commit only
 * after all references and allocations have been checked. */
#include <stdlib.h>
#include <string.h>
#include "stanload.h"

static DWORD StanDeleteRead32(const unsigned char *p)
{
    return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3];
}

static void StanDeleteWrite32(unsigned char *p, DWORD value)
{
    p[0] = (unsigned char)(value >> 24); p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8); p[3] = (unsigned char)value;
}

static DWORD StanDeleteTileAtOffset(const StanFile *stan, DWORD offset)
{
    DWORD low = 0, high = stan->tilecount;
    while (low < high)
    {
        DWORD mid = low + (high - low) / 2;
        if (stan->tiles[mid].sourceoffset < offset) { low = mid + 1; }
        else { high = mid; }
    }
    return low < stan->tilecount && stan->tiles[low].sourceoffset == offset ? low : STAN_TILE_NONE;
}

BOOL StanDeleteTiles(StanFile *stan, const DWORD *selected, DWORD count,
    DWORD *deletedout, const char **reasonout)
{
    unsigned char *removed = NULL, *data = NULL;
    DWORD *offsets = NULL;
    StanTile *tiles = NULL;
    DWORD i, first, oldend, newend, newsize, kept = 0;
    const char *why = "Out of memory deleting stan tiles.";
    BOOL ok = FALSE;
    *deletedout = 0;
    if (!stan || !stan->data || !stan->tiles || !stan->tilecount || !selected || !count || count > stan->tilecount)
    { why = "Select stan tiles to delete."; goto done; }
    removed = calloc(stan->tilecount, 1);
    offsets = malloc((size_t)stan->tilecount * sizeof(*offsets));
    if (!removed || !offsets) { goto done; }
    for (i = 0; i < count; i++)
    {
        if (selected[i] >= stan->tilecount || removed[selected[i]])
        { why = "The stan tile selection is invalid."; goto done; }
        removed[selected[i]] = 1;
    }
    if (count == stan->tilecount)
    { why = "At least one stan tile must remain in the level."; goto done; }
    first = stan->tiles[0].sourceoffset;
    if (stan->size < 12 || first < 12 || first > stan->size - 8 || (first & 3)
        || (StanDeleteRead32(stan->data + 4) & 0x00ffffffu) != first)
    { why = "The stan header has an invalid first-tile pointer."; goto done; }
    oldend = newend = first;
    for (i = 0; i < stan->tilecount; i++)
    {
        const StanTile *tile = stan->tiles + i;
        DWORD size = 8u + tile->pointcount * 8u;
        if (tile->sourceoffset != oldend || tile->pointcount < 3 || tile->pointcount > STAN_TILE_MAX_POINTS
            || oldend > stan->size || size > stan->size - oldend
            || StanDeleteRead32(stan->data + oldend) != (tile->id << 8 | tile->room)
            || (stan->data[oldend + 6] >> 4) != tile->pointcount)
        { why = "The stan tile records are inconsistent."; goto done; }
        offsets[i] = removed[i] ? STAN_TILE_NONE : newend;
        if (!removed[i]) { newend += size; }
        oldend += size;
    }
    if (oldend > stan->size - 8 || StanDeleteRead32(stan->data + oldend) != 0)
    { why = "The stan tile list has no terminator."; goto done; }
    newsize = stan->size - (oldend - newend);
    data = malloc(newsize);
    tiles = malloc((size_t)(stan->tilecount - count) * sizeof(*tiles));
    if (!data || !tiles) { goto done; }
    memcpy(data, stan->data, first);
    /* Preserve the terminator, unstric footer and any trailing opaque data. */
    memcpy(data + newend, stan->data + oldend, stan->size - oldend);
    for (i = 0; i < stan->tilecount; i++)
    {
        const StanTile *old = stan->tiles + i;
        StanTile *tile;
        unsigned int point;
        if (removed[i]) { continue; }
        tile = tiles + kept++;
        *tile = *old;
        tile->sourceoffset = offsets[i];
        memcpy(data + tile->sourceoffset, stan->data + old->sourceoffset, 8u + old->pointcount * 8u);
        for (point = 0; point < tile->pointcount; point++)
        {
            DWORD target;
            unsigned short link = old->points[point].link;
            unsigned char *raw = data + tile->sourceoffset + 8u + point * 8u + 6u;
            if (link < 0x10) { continue; } /* Preserve authored non-link values. */
            target = StanLinkedTile(stan, link);
            if (target == STAN_TILE_NONE)
            { why = "A surviving stan tile has an invalid edge link."; goto done; }
            if (removed[target]) { link = 0; }
            else
            {
                DWORD relocated = (offsets[target] - first) / 8u + 0x10u;
                if (relocated > 0xffffu)
                { why = "A relocated stan edge link is outside the native range."; goto done; }
                link = (unsigned short)relocated;
            }
            tile->points[point].link = link;
            raw[0] = (unsigned char)(link >> 8); raw[1] = (unsigned char)link;
        }
    }
    /* The runtime relocates this zero-terminated pointer table, then begins
     * walking tiles immediately after it. Never insert an early zero when a
     * table entry's tile was removed: advance to the next surviving tile (or
     * the first survivor at the end), retaining the table and segment byte. */
    for (i = 4; i < first; i += 4)
    {
        DWORD pointer = StanDeleteRead32(stan->data + i), target;
        if (!pointer)
        {
            if (i != first - 4)
            { why = "The stan header pointer table ends before the tile list."; goto done; }
            break;
        }
        if (i == first - 4)
        { why = "The stan header pointer table has no terminator."; goto done; }
        target = StanDeleteTileAtOffset(stan, pointer & 0x00ffffffu);
        if (target == STAN_TILE_NONE)
        { why = "The stan header has an invalid tile pointer."; goto done; }
        while (target < stan->tilecount && removed[target]) { target++; }
        if (target == stan->tilecount)
        { target = 0; while (removed[target]) { target++; } }
        StanDeleteWrite32(data + i, (pointer & 0xff000000u) | offsets[target]);
    }
    free(stan->data); free(stan->tiles);
    stan->data = data; data = NULL;
    stan->tiles = tiles; tiles = NULL;
    stan->size = newsize; stan->tilecount = kept; stan->dirty = TRUE;
    *deletedout = count;
    ok = TRUE;
done:
    free(removed); free(offsets); free(data); free(tiles);
    *reasonout = ok ? "" : why;
    return ok;
}

typedef struct StanDeleteEdge { DWORD a, b; } StanDeleteEdge;

static StanDeleteEdge StanDeleteEdgeKey(const StanFile *stan, const DWORD *map, DWORD t, DWORD p)
{
    DWORD a = map[t * STAN_TILE_MAX_POINTS + p];
    DWORD b = map[t * STAN_TILE_MAX_POINTS + (p + 1) % stan->tiles[t].pointcount];
    return a < b ? (StanDeleteEdge){a,b} : (StanDeleteEdge){b,a};
}

static int StanDeleteCompareEdges(const void *left, const void *right)
{
    const StanDeleteEdge *a = left, *b = right;
    if (a->a != b->a) { return a->a < b->a ? -1 : 1; }
    return a->b < b->b ? -1 : a->b > b->b;
}

BOOL StanDeleteEdgeTiles(StanFile *stan, const StanEdgeRef *edges, DWORD count,
    DWORD *deletedout, const char **reasonout)
{
    DWORD *map = NULL, *selected = NULL, selectedcount = 0;
    StanDeleteEdge *keys = NULL;
    BOOL ok = FALSE;
    *deletedout = 0;
    if (!stan || !stan->tiles || !edges || !count
        || count > (size_t)stan->tilecount * STAN_TILE_MAX_POINTS)
    { *reasonout = "Select stan edges to delete their incident tiles."; return FALSE; }
    for (DWORD i = 0; i < count; i++)
    {
        if (edges[i].tile >= stan->tilecount || edges[i].point >= stan->tiles[edges[i].tile].pointcount)
        { *reasonout = "A selected stan edge no longer exists."; return FALSE; }
    }
    map = StanBuildPointMap(stan, reasonout);
    if (!map) { return FALSE; }
    *reasonout = "Out of memory resolving stan edge deletion.";
    keys = malloc((size_t)count * sizeof(*keys));
    selected = malloc((size_t)stan->tilecount * sizeof(*selected));
    if (!keys || !selected) { goto done; }
    for (DWORD i = 0; i < count; i++)
    { keys[i] = StanDeleteEdgeKey(stan, map, edges[i].tile, edges[i].point); }
    qsort(keys, count, sizeof(*keys), StanDeleteCompareEdges);
    for (DWORD t = 0; t < stan->tilecount; t++)
    for (DWORD p = 0; p < stan->tiles[t].pointcount; p++)
    {
        StanDeleteEdge key = StanDeleteEdgeKey(stan, map, t, p);
        if (bsearch(&key, keys, count, sizeof(*keys), StanDeleteCompareEdges))
        { selected[selectedcount++] = t; break; }
    }
    ok = StanDeleteTiles(stan, selected, selectedcount, deletedout, reasonout);
done:
    free(map); free(keys); free(selected); return ok;
}

BOOL StanDeleteCollapsedTiles(const StanFile *before, StanFile *stan,
    DWORD *deletedout, const char **reasonout)
{
    DWORD *selected = NULL, count = 0;
    BOOL ok = FALSE;
    *deletedout = 0;
    *reasonout = "The stan position edit changed the tile layout unexpectedly.";
    if (!before || !stan || !before->data || !stan->data || !before->tiles || !stan->tiles
        || before->tilecount != stan->tilecount) { return FALSE; }
    /* Position edits keep the record layout. Ignore unrelated pre-existing
     * damage: only tiles whose native positions changed belong to this edit. */
    for (DWORD t = 0; t < stan->tilecount; t++)
    {
        const StanTile *old = &before->tiles[t], *tile = &stan->tiles[t];
        DWORD bytes = 8u + tile->pointcount * 8u;
        if (old->editorid != tile->editorid || old->pointcount != tile->pointcount
            || tile->pointcount > STAN_TILE_MAX_POINTS
            || old->sourceoffset > before->size || bytes > before->size - old->sourceoffset
            || tile->sourceoffset > stan->size || bytes > stan->size - tile->sourceoffset) { goto done; }
        const unsigned char *a = before->data + old->sourceoffset + 8;
        const unsigned char *b = stan->data + tile->sourceoffset + 8;
        BOOL changed = FALSE;
        for (DWORD p = 0; p < tile->pointcount; p++)
        { if (memcmp(a + p*8, b + p*8, 6)) { changed = TRUE; break; } }
        if (!changed || StanPointsHaveArea(b, tile->pointcount)) { continue; }
        if (!selected)
        {
            selected = malloc((size_t)stan->tilecount * sizeof(*selected));
            if (!selected) { *reasonout = "Out of memory removing collapsed stan tiles."; goto done; }
        }
        selected[count++] = t;
    }
    if (count) { ok = StanDeleteTiles(stan, selected, count, deletedout, reasonout); }
    else { *reasonout = ""; ok = TRUE; }
done:
    free(selected); return ok;
}
