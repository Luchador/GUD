/* Merge/weld explicitly selected source identities within each room. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static int BgMergeAverage(int64_t sum, DWORD count)
{
    /* Match native position rounding: nearest integer, half away from zero. */
    return (int)(sum < 0 ? -((-sum + count/2)/count) : (sum + count/2)/count);
}

static void BgMergeAccumulate(int64_t sum[9], const BgDocumentVertex *v)
{
    sum[0] += v->x; sum[1] += v->y; sum[2] += v->z;
    sum[3] += v->s; sum[4] += v->t;
    sum[5] += v->r; sum[6] += v->g; sum[7] += v->b; sum[8] += v->a;
}

static void BgMergeSetAverage(BgDocumentVertex *v, const int64_t sum[9], DWORD count)
{
    v->x = (short)BgMergeAverage(sum[0], count);
    v->y = (short)BgMergeAverage(sum[1], count);
    v->z = (short)BgMergeAverage(sum[2], count);
    v->s = (short)BgMergeAverage(sum[3], count);
    v->t = (short)BgMergeAverage(sum[4], count);
    v->r = (unsigned char)BgMergeAverage(sum[5], count);
    v->g = (unsigned char)BgMergeAverage(sum[6], count);
    v->b = (unsigned char)BgMergeAverage(sum[7], count);
    v->a = (unsigned char)BgMergeAverage(sum[8], count);
}

static BOOL BgMergeFaceHasArea(const BgDocumentVertex *vertices, const BgDocumentFace *face)
{
    const BgDocumentVertex *a = &vertices[face->vertexindices[0]];
    const BgDocumentVertex *b = &vertices[face->vertexindices[1]];
    const BgDocumentVertex *c = &vertices[face->vertexindices[2]];
    int64_t abx = (int64_t)b->x-a->x, aby = (int64_t)b->y-a->y, abz = (int64_t)b->z-a->z;
    int64_t acx = (int64_t)c->x-a->x, acy = (int64_t)c->y-a->y, acz = (int64_t)c->z-a->z;
    return aby*acz != abz*acy || abz*acx != abx*acz || abx*acy != aby*acx;
}

BOOL BgDocumentMergeVertices(BgDocument *document, const BgDocumentVertexRef *refs,
    DWORD count, BgDocumentVertexRef *out, DWORD *removedout, DWORD *deletedout,
    const char **reasonout)
{
    BgDocumentRoom *room;
    BgDocumentVertex *vertices = NULL, merged;
    BgDocumentFace *faces = NULL;
    unsigned char *selected = NULL;
    DWORD *mapping = NULL;
    DWORD unique = 0, first = UINT32_MAX, vertexcount = 0, facecount = 0, deleted = 0;
    int64_t sum[9] = {0}; /* XYZ, ST, RGBA; each source vertex contributes once. */
    const char *why = "Select at least two background vertices in one room.";
    BOOL ok = FALSE;
    if (removedout) { *removedout = 0; }
    if (deletedout) { *deletedout = 0; }
    if (!document || !document->rooms || !refs || count < 2
        || !out || !refs[0].room || refs[0].room > document->roomcount) { goto done; }
    room = &document->rooms[refs[0].room];
    why = "The background room has invalid geometry.";
    if (!room->vertices || !room->vertexcount || room->vertexcount > 0x100000u
        || (room->facecount && !room->faces) || room->facecount > document->facecount
        || room->facecount > UINT32_MAX/sizeof(*faces)) { goto done; }
    why = "Out of memory merging background vertices.";
    selected = calloc(room->vertexcount, sizeof(*selected));
    mapping = malloc((size_t)room->vertexcount*sizeof(*mapping));
    if (!selected || !mapping) { goto done; }
    for (DWORD i = 0; i < count; i++)
    {
        why = "Vertices from different background rooms cannot share one vertex. Select vertices within one room.";
        if (refs[i].room != refs[0].room) { goto done; }
        why = "A selected background vertex no longer exists.";
        DWORD v = refs[i].index;
        if (v >= room->vertexcount || room->vertices[v].room != refs[i].room) { goto done; }
        if (selected[v]) { continue; }
        selected[v] = 1; unique++;
        if (v < first) { first = v; }
        BgMergeAccumulate(sum, &room->vertices[v]);
    }
    why = "Select at least two distinct background vertices.";
    if (unique < 2) { goto done; }
    merged = room->vertices[first]; /* Stable survivor ID and native flag. */
    BgMergeSetAverage(&merged, sum, unique);
    why = "Out of memory merging background vertices.";
    vertices = malloc((size_t)(room->vertexcount-unique+1)*sizeof(*vertices));
    if (room->facecount) { faces = malloc((size_t)room->facecount*sizeof(*faces)); }
    if (!vertices || (room->facecount && !faces)) { goto done; }
    for (DWORD v = 0; v < room->vertexcount; v++)
    {
        if (selected[v] && v != first) { mapping[v] = first; continue; }
        /* No vertices before the survivor are removed, so its new index is first. */
        mapping[v] = vertexcount;
        vertices[vertexcount] = v == first ? merged : room->vertices[v];
        vertices[vertexcount++].usecount = 0;
    }
    for (DWORD f = 0; f < room->facecount; f++)
    {
        BgDocumentFace face = room->faces[f];
        why = "A background face references an invalid vertex.";
        for (int c = 0; c < 3; c++)
        {
            if (face.vertexindices[c] >= room->vertexcount) { goto done; }
            face.vertexindices[c] = mapping[face.vertexindices[c]];
        }
        /* Preserve existing degenerates; remove only triangles collapsed by this edit. */
        if (!BgMergeFaceHasArea(vertices, &face) && BgMergeFaceHasArea(room->vertices, &room->faces[f]))
        { deleted++; continue; }
        for (int c = 0; c < 3; c++)
        {
            DWORD *uses = &vertices[face.vertexindices[c]].usecount;
            why = "The background vertex reference count exceeds its limit.";
            if (*uses == UINT32_MAX) { goto done; }
            (*uses)++;
        }
        faces[facecount++] = face;
    }
    /* Commit only after validation/allocation. Reindex both layers together. */
    free(room->vertices); free(room->faces);
    room->vertices = vertices; vertices = NULL;
    room->faces = faces; faces = NULL;
    room->vertexcount = vertexcount;
    room->facecapacity = room->facecount;
    room->facecount = facecount;
    document->facecount -= deleted;
    document->dirty = TRUE;
    *out = (BgDocumentVertexRef){refs[0].room, first};
    if (removedout) { *removedout = unique-1; }
    if (deletedout) { *deletedout = deleted; }
    why = ""; ok = TRUE;
done:
    free(selected); free(mapping); free(vertices); free(faces);
    if (reasonout) { *reasonout = why; }
    return ok;
}

typedef struct BgWeldVertex {
    BgDocumentVertexRef ref;
    short x, y, z;
} BgWeldVertex;

typedef struct BgWeldRoom {
    DWORD room, vertexcount;
    BgDocumentVertex *vertices;
    BgDocumentFace *faces;
    DWORD *mapping;
} BgWeldRoom;

static int BgWeldComparePosition(const void *left, const void *right)
{
    const BgWeldVertex *a = left, *b = right;
    if (a->ref.room != b->ref.room) { return a->ref.room < b->ref.room ? -1 : 1; }
    if (a->x != b->x) { return (int)a->x - b->x; }
    if (a->y != b->y) { return (int)a->y - b->y; }
    return (int)a->z - b->z;
}

static int BgWeldCompareVertex(const void *left, const void *right)
{
    const BgWeldVertex *a = left, *b = right;
    int order = BgWeldComparePosition(left, right);
    if (order) { return order; }
    return a->ref.index < b->ref.index ? -1 : a->ref.index != b->ref.index;
}

BOOL BgDocumentWeldVertices(BgDocument *document, BgDocumentVertexRef *refs,
    DWORD *countinout, DWORD *removedout, const char **reasonout)
{
    BgWeldVertex *selected = NULL;
    BgWeldRoom *pending = NULL;
    DWORD count, unique = 0, roomcount = 0, removed = 0;
    const char *why = "Invalid background vertex selection.";
    BOOL ok = FALSE;
    if (removedout) { *removedout = 0; }
    if (!document || !document->rooms || !refs || !countinout
        || !(count = *countinout) || count > UINT32_MAX/sizeof(*selected)
        || count > UINT32_MAX/sizeof(*pending)) { goto done; }
    why = "Out of memory welding background vertices.";
    selected = malloc((size_t)count*sizeof(*selected));
    pending = calloc(count, sizeof(*pending));
    if (!selected || !pending) { goto done; }
    for (DWORD i = 0; i < count; i++)
    {
        why = "A selected background vertex no longer exists.";
        if (!refs[i].room || refs[i].room > document->roomcount) { goto done; }
        const BgDocumentRoom *r = &document->rooms[refs[i].room];
        if (!r->vertices || refs[i].index >= r->vertexcount
            || r->vertices[refs[i].index].room != refs[i].room) { goto done; }
        const BgDocumentVertex *v = &r->vertices[refs[i].index];
        selected[i] = (BgWeldVertex){refs[i], v->x, v->y, v->z};
    }
    /* XYZ are signed native integers: distance <= 0.1 is exactly coincident.
     * Do not compare world-space floats or scale the tolerance by levelscale.
     * Sorting also makes duplicate refs contribute once and keeps the lowest
     * source index (and its stable ID/native flag) as each group's survivor. */
    qsort(selected, count, sizeof(*selected), BgWeldCompareVertex);
    for (DWORD i = 0; i < count; i++)
    {
        if (!unique || BgWeldCompareVertex(&selected[i], &selected[unique-1]))
        { selected[unique++] = selected[i]; }
    }
    for (DWORD start = 0, end; start < unique; start = end)
    {
        for (end = start+1; end < unique && selected[end].ref.room == selected[start].ref.room; end++) {}
        BOOL changes = FALSE;
        for (DWORD i = start+1; i < end; i++)
        { if (!BgWeldComparePosition(&selected[i-1], &selected[i])) { changes = TRUE; break; } }
        if (!changes) { continue; }
        const BgDocumentRoom *r = &document->rooms[selected[start].ref.room];
        why = "The background room has invalid geometry.";
        if (r->vertexcount > 0x100000u || (r->facecount && !r->faces)
            || r->facecount > document->facecount || r->facecount > UINT32_MAX/sizeof(*r->faces)) { goto done; }
        BgWeldRoom *edit = &pending[roomcount++];
        edit->room = selected[start].ref.room;
        why = "Out of memory welding background vertices.";
        edit->vertices = malloc((size_t)r->vertexcount*sizeof(*edit->vertices));
        edit->mapping = malloc((size_t)r->vertexcount*sizeof(*edit->mapping));
        if (r->facecount) { edit->faces = malloc((size_t)r->facecount*sizeof(*edit->faces)); }
        if (!edit->vertices || !edit->mapping || (r->facecount && !edit->faces)) { goto done; }
        memcpy(edit->vertices, r->vertices, (size_t)r->vertexcount*sizeof(*edit->vertices));
        for (DWORD v = 0; v < r->vertexcount; v++) { edit->mapping[v] = v; }
        for (DWORD first = start, next; first < end; first = next)
        {
            for (next = first+1; next < end && !BgWeldComparePosition(&selected[first], &selected[next]); next++) {}
            if (next == first+1) { continue; }
            DWORD survivor = selected[first].ref.index;
            int64_t sum[9] = {0};
            for (DWORD i = first; i < next; i++)
            {
                DWORD v = selected[i].ref.index;
                edit->mapping[v] = survivor;
                BgMergeAccumulate(sum, &r->vertices[v]);
            }
            BgMergeSetAverage(&edit->vertices[survivor], sum, next-first);
            removed += next-first-1;
        }
        /* Compact once per room, not once per group. Every survivor precedes
         * its removed sources, so its final index is already known here. */
        for (DWORD v = 0; v < r->vertexcount; v++)
        {
            if (edit->mapping[v] != v) { edit->mapping[v] = edit->mapping[edit->mapping[v]]; continue; }
            edit->mapping[v] = edit->vertexcount;
            edit->vertices[edit->vertexcount] = edit->vertices[v];
            edit->vertices[edit->vertexcount++].usecount = 0;
        }
        for (DWORD f = 0; f < r->facecount; f++)
        {
            BgDocumentFace *face = &edit->faces[f]; *face = r->faces[f];
            for (int c = 0; c < 3; c++)
            {
                why = "A background face references an invalid vertex.";
                if (face->vertexindices[c] >= r->vertexcount) { goto done; }
                face->vertexindices[c] = edit->mapping[face->vertexindices[c]];
                DWORD *uses = &edit->vertices[face->vertexindices[c]].usecount;
                if (*uses == UINT32_MAX) { goto done; }
                (*uses)++;
            }
        }
        /* Positions never move, so no new degenerate faces can be produced.
         * As with Merge, preserve any degenerates that already existed. */
        for (DWORD i = start; i < end; i++) { selected[i].ref.index = edit->mapping[selected[i].ref.index]; }
    }
    /* Allocate and validate every room before changing the document/selection. */
    if (removed)
    {
        for (DWORD i = 0; i < roomcount; i++)
        {
            BgWeldRoom *edit = &pending[i]; BgDocumentRoom *r = &document->rooms[edit->room];
            free(r->vertices); free(r->faces);
            r->vertices = edit->vertices; edit->vertices = NULL;
            r->faces = edit->faces; edit->faces = NULL;
            r->vertexcount = edit->vertexcount; r->facecapacity = r->facecount;
        }
        DWORD kept = 0;
        for (DWORD i = 0; i < unique; i++)
        {
            if (!kept || selected[i].ref.room != refs[kept-1].room || selected[i].ref.index != refs[kept-1].index)
            { refs[kept++] = selected[i].ref; }
        }
        *countinout = kept;
        document->dirty = TRUE;
    }
    if (removedout) { *removedout = removed; }
    why = ""; ok = TRUE;
done:
    for (DWORD i = 0; i < roomcount; i++)
    { free(pending[i].vertices); free(pending[i].faces); free(pending[i].mapping); }
    free(selected); free(pending);
    if (reasonout) { *reasonout = why; }
    return ok;
}
