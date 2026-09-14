/* Merge explicitly selected source identities; never weld by proximity. */
#include <stdint.h>
#include <stdlib.h>
#include "bgdocument.h"

static int BgMergeAverage(int64_t sum, DWORD count)
{
    /* Match native position rounding: nearest integer, half away from zero. */
    return (int)(sum < 0 ? -((-sum + count/2)/count) : (sum + count/2)/count);
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
        const BgDocumentVertex *p = &room->vertices[v];
        sum[0] += p->x; sum[1] += p->y; sum[2] += p->z;
        sum[3] += p->s; sum[4] += p->t;
        sum[5] += p->r; sum[6] += p->g; sum[7] += p->b; sum[8] += p->a;
    }
    why = "Select at least two distinct background vertices.";
    if (unique < 2) { goto done; }
    merged = room->vertices[first]; /* Stable survivor ID and native flag. */
    merged.x = (short)BgMergeAverage(sum[0], unique);
    merged.y = (short)BgMergeAverage(sum[1], unique);
    merged.z = (short)BgMergeAverage(sum[2], unique);
    merged.s = (short)BgMergeAverage(sum[3], unique);
    merged.t = (short)BgMergeAverage(sum[4], unique);
    merged.r = (unsigned char)BgMergeAverage(sum[5], unique);
    merged.g = (unsigned char)BgMergeAverage(sum[6], unique);
    merged.b = (unsigned char)BgMergeAverage(sum[7], unique);
    merged.a = (unsigned char)BgMergeAverage(sum[8], unique);
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
