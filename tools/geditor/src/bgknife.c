#include "bgknife.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KNIFE_MAX_VERTICES 0x100000u
#define KNIFE_EPSILON 1e-7

typedef struct KnifeEdge {
    DWORD room, a, b, vertex;
} KnifeEdge;

BOOL BgKnifeNormalizePlane(const BgKnifePlane *source, BgKnifePlane *out)
{
    double scale = 0, length = 0;
    int axis;
    if (!source || !out) { return FALSE; }
    for (axis = 0; axis < 3; axis++)
    {
        if (!isfinite(source->position[axis]) || !isfinite(source->normal[axis])) { return FALSE; }
        if (fabs(source->normal[axis]) > scale) { scale = fabs(source->normal[axis]); }
    }
    if (scale == 0) { return FALSE; }
    *out = *source;
    for (axis = 0; axis < 3; axis++)
    { out->normal[axis] /= scale; length += out->normal[axis] * out->normal[axis]; }
    length = sqrt(length);
    for (axis = 0; axis < 3; axis++) { out->normal[axis] /= length; }
    return TRUE;
}

static int KnifeCompareRef(const void *a, const void *b)
{
    const BgFaceRef *x = a, *y = b;
    if (x->room != y->room) { return x->room < y->room ? -1 : 1; }
    if (x->faceid != y->faceid) { return x->faceid < y->faceid ? -1 : 1; }
    return (int)x->layer - (int)y->layer;
}

static BgFaceRef KnifeRef(const BgDocumentFace *face)
{
    BgFaceRef ref = {0};
    ref.faceid = face->id; ref.room = face->room; ref.layer = face->layer;
    return ref;
}

static void KnifeNormal(const BgDocumentRoom *room, const DWORD indices[3], double normal[3])
{
    const BgDocumentVertex *a = room->vertices + indices[0];
    const BgDocumentVertex *b = room->vertices + indices[1];
    const BgDocumentVertex *c = room->vertices + indices[2];
    double u[3] = {b->x - a->x, b->y - a->y, b->z - a->z};
    double v[3] = {c->x - a->x, c->y - a->y, c->z - a->z};
    normal[0] = u[1]*v[2] - u[2]*v[1];
    normal[1] = u[2]*v[0] - u[0]*v[2];
    normal[2] = u[0]*v[1] - u[1]*v[0];
}

/* Cache by source identity, not position: adjoining faces share the new
 * vertex, but authored UV/color seams remain separate. */
static BOOL KnifeIntersection(BgDocument *doc, DWORD roomindex, DWORD a, DWORD b,
    double da, double db, KnifeEdge *cache, size_t capacity, DWORD *out, const char **why)
{
    BgDocumentRoom *room = doc->rooms + roomindex;
    BgDocumentVertex vertex;
    const BgDocumentVertex *va, *vb;
    size_t slot;
    double t;
    if (a > b)
    { DWORD swap = a; double dswap = da; a = b; b = swap; da = db; db = dswap; }
    slot = ((size_t)a * 73856093u ^ (size_t)b * 19349663u ^ (size_t)roomindex * 83492791u) & (capacity - 1);
    while (cache[slot].room)
    {
        if (cache[slot].room == roomindex && cache[slot].a == a && cache[slot].b == b)
        { *out = cache[slot].vertex; return TRUE; }
        slot = (slot + 1) & (capacity - 1);
    }
    va = room->vertices + a; vb = room->vertices + b;
    t = da / (da - db);
    vertex = *va;
#define KNIFE_LERP(field) vertex.field = round(va->field + (vb->field - va->field) * t)
    KNIFE_LERP(x); KNIFE_LERP(y); KNIFE_LERP(z);
    KNIFE_LERP(s); KNIFE_LERP(t);
    KNIFE_LERP(r); KNIFE_LERP(g); KNIFE_LERP(b); KNIFE_LERP(a);
#undef KNIFE_LERP
    /* A sub-grid cut can round onto an existing endpoint. Retain its
     * identity/attributes rather than introducing a coincident vertex. */
    if ((vertex.x == va->x && vertex.y == va->y && vertex.z == va->z)
        || (vertex.x == vb->x && vertex.y == vb->y && vertex.z == vb->z))
    {
        *out = vertex.x == va->x && vertex.y == va->y && vertex.z == va->z ? a : b;
        cache[slot] = (KnifeEdge){roomindex, a, b, *out};
        return TRUE;
    }
    if (room->vertexcount >= KNIFE_MAX_VERTICES || !doc->nextvertexid || doc->nextvertexid == UINT32_MAX)
    { *why = "The cut would exceed the background vertex limit."; return FALSE; }
    vertex.id = doc->nextvertexid++;
    vertex.usecount = 0;
    *out = room->vertexcount;
    room->vertices[room->vertexcount++] = vertex;
    cache[slot] = (KnifeEdge){roomindex, a, b, *out};
    return TRUE;
}

BOOL BgDocumentKnifeFaces(BgDocument *document, const BgFaceRef *faces, DWORD count,
    const BgKnifePlane *input, BgFaceRef **selection, DWORD *selectioncount,
    DWORD *cutcount, const char **reason)
{
    BgDocument work = {0};
    BgKnifePlane plane;
    BgFaceRef *sorted = NULL, *result = NULL;
    KnifeEdge *cache = NULL;
    BgDocumentFace *newfaces = NULL;
    const char *why = "Out of memory cutting the background faces.";
    size_t capacity = 8;
    DWORD resultcount = 0, cuts = 0, index, roomindex;
    double native[3];
    BOOL ok = FALSE;
    if (selection) { *selection = NULL; }
    if (selectioncount) { *selectioncount = 0; }
    if (cutcount) { *cutcount = 0; }
    if (!document || !document->rooms || !faces || !count || !selection || !selectioncount || !cutcount)
    { why = "Select background faces to cut."; goto done; }
    if (!BgKnifeNormalizePlane(input, &plane) || !isfinite(document->levelscale) || document->levelscale <= 0)
    { why = "Enter a finite plane position and a nonzero direction vector."; goto done; }
    for (index = 0; index < 3; index++)
    {
        native[index] = plane.position[index] * document->levelscale;
        if (!isfinite(native[index]) || fabs(native[index]) > 1e30)
        { why = "The knife position is outside the background coordinate range."; goto done; }
    }
    if (count > UINT32_MAX / 3 || sizeof(*result) > SIZE_MAX / ((size_t)count * 3)) { goto done; }
    while (capacity < (size_t)count * 4)
    { if (capacity > SIZE_MAX / 2) { goto done; } capacity *= 2; }
    sorted = malloc((size_t)count * sizeof(*sorted));
    result = malloc((size_t)count * 3 * sizeof(*result));
    cache = calloc(capacity, sizeof(*cache));
    if (!sorted || !result || !cache) { goto done; }
    memcpy(sorted, faces, (size_t)count * sizeof(*sorted));
    qsort(sorted, count, sizeof(*sorted), KnifeCompareRef);
    for (index = 0; index < count; index++)
    {
        if ((index && !KnifeCompareRef(sorted + index - 1, sorted + index))
            || !BgDocumentFindFace(document, sorted + index, NULL))
        { why = "The background selection is invalid or contains duplicate faces."; goto done; }
    }
    if (!BgDocumentClone(document, &work, &why)) { goto done; }
    for (roomindex = 1; roomindex <= work.roomcount; roomindex++)
    {
        BgDocumentRoom *room = work.rooms + roomindex;
        DWORD selected = 0, outputcount = 0, faceindex, maxvertices;
        DWORD originalvertices = room->vertexcount;
        size_t maxfaces;
        BgDocumentVertex *vertices;
        for (index = 0; index < count; index++) { if (sorted[index].room == roomindex) { selected++; } }
        if (!selected) { continue; }
        maxfaces = (size_t)room->facecount + (size_t)selected * 2;
        if (maxfaces > UINT32_MAX || maxfaces > SIZE_MAX / sizeof(*newfaces)
            || room->vertexcount > KNIFE_MAX_VERTICES) { goto done; }
        maxvertices = room->vertexcount + (selected > KNIFE_MAX_VERTICES / 2 ? KNIFE_MAX_VERTICES : selected * 2);
        if (maxvertices > KNIFE_MAX_VERTICES) { maxvertices = KNIFE_MAX_VERTICES; }
        vertices = realloc(room->vertices, (size_t)maxvertices * sizeof(*vertices));
        if (!vertices) { goto done; }
        room->vertices = vertices;
        newfaces = malloc(maxfaces * sizeof(*newfaces));
        if (!newfaces) { goto done; }
        for (faceindex = 0; faceindex < room->facecount; faceindex++)
        {
            BgDocumentFace face = room->faces[faceindex];
            BgFaceRef ref = KnifeRef(&face);
            double d[3], originalnormal[3];
            DWORD edge[3] = {0}, children = 0;
            int c, side, positive = 0, negative = 0;
            if (!bsearch(&ref, sorted, count, sizeof(*sorted), KnifeCompareRef))
            { newfaces[outputcount++] = face; continue; }
            for (c = 0; c < 3; c++)
            {
                const BgDocumentVertex *v;
                if (face.vertexindices[c] >= room->vertexcount)
                { why = "The selected face has an invalid vertex."; goto done; }
                v = room->vertices + face.vertexindices[c];
                d[c] = (room->origin[0] + (double)v->x - native[0]) * plane.normal[0]
                     + (room->origin[1] + (double)v->y - native[1]) * plane.normal[1]
                     + (room->origin[2] + (double)v->z - native[2]) * plane.normal[2];
                if (!isfinite(d[c])) { why = "The background has invalid coordinates."; goto done; }
                if (fabs(d[c]) <= KNIFE_EPSILON) { d[c] = 0; }
                positive += d[c] > 0; negative += d[c] < 0;
            }
            if (!positive || !negative)
            { newfaces[outputcount++] = face; result[resultcount++] = ref; continue; }
            KnifeNormal(room, face.vertexindices, originalnormal);
            for (c = 0; c < 3; c++)
            {
                int next = (c + 1) % 3;
                if ((d[c] > 0 && d[next] < 0) || (d[c] < 0 && d[next] > 0))
                {
                    if (!KnifeIntersection(&work, roomindex, face.vertexindices[c], face.vertexindices[next],
                        d[c], d[next], cache, capacity, edge + c, &why)) { goto done; }
                }
                if (room->vertices[face.vertexindices[c]].usecount == 0)
                { why = "The background vertex references are inconsistent."; goto done; }
                room->vertices[face.vertexindices[c]].usecount--;
            }
            for (side = -1; side <= 1; side += 2)
            {
                DWORD polygon[4], n = 0, triangle;
                for (c = 0; c < 3; c++)
                {
                    int next = (c + 1) % 3;
                    if (d[c] * side >= 0) { polygon[n++] = face.vertexindices[c]; }
                    if ((d[c] > 0 && d[next] < 0) || (d[c] < 0 && d[next] > 0)) { polygon[n++] = edge[c]; }
                }
                for (triangle = 1; triangle + 1 < n; triangle++)
                {
                    BgDocumentFace child = face;
                    double normal[3];
                    child.vertexindices[0] = polygon[0];
                    child.vertexindices[1] = polygon[triangle];
                    child.vertexindices[2] = polygon[triangle + 1];
                    BgDocumentInheritFaceSeams(room->vertices, &face, &child);
                    KnifeNormal(room, child.vertexindices, normal);
                    /* Thin railing triangles can leave a fragment narrower
                     * than one native unit. Never emit its zero-area triangle. */
                    if (normal[0] == 0 && normal[1] == 0 && normal[2] == 0) { continue; }
                    if (normal[0]*originalnormal[0] + normal[1]*originalnormal[1] + normal[2]*originalnormal[2] <= 0)
                    { why = "The cut is too close to a vertex at native precision. Move the plane slightly and try again."; goto done; }
                    if (children++)
                    {
                        if (!work.nextfaceid || work.nextfaceid == UINT32_MAX || work.facecount == UINT32_MAX)
                        { why = "The cut would exceed the background face limit."; goto done; }
                        child.id = work.nextfaceid++; work.facecount++;
                    }
                    for (c = 0; c < 3; c++) { room->vertices[child.vertexindices[c]].usecount++; }
                    newfaces[outputcount++] = child;
                    result[resultcount++] = KnifeRef(&child);
                }
            }
            if (!children)
            { why = "The selected face cannot be cut at native coordinate precision."; goto done; }
            if (children == 1)
            {
                BgDocumentFace *child = newfaces + outputcount - 1;
                BOOL same = FALSE;
                for (c = 0; c < 3; c++)
                {
                    if (child->vertexindices[0] == face.vertexindices[c]
                        && child->vertexindices[1] == face.vertexindices[(c + 1) % 3]
                        && child->vertexindices[2] == face.vertexindices[(c + 2) % 3]) { same = TRUE; }
                }
                if (same) { *child = face; continue; }
            }
            cuts++;
        }
        free(room->faces); room->faces = newfaces; newfaces = NULL;
        room->facecount = outputcount; room->facecapacity = (DWORD)maxfaces;
        if (room->vertexcount > originalvertices)
        {
            DWORD *map = malloc((size_t)(room->vertexcount - originalvertices) * sizeof(*map));
            DWORD next = originalvertices;
            if (!map) { goto done; }
            for (index = originalvertices; index < room->vertexcount; index++)
            {
                map[index - originalvertices] = next;
                if (room->vertices[index].usecount) { room->vertices[next++] = room->vertices[index]; }
            }
            for (faceindex = 0; faceindex < room->facecount; faceindex++)
            {
                for (int c = 0; c < 3; c++)
                {
                    DWORD *v = &room->faces[faceindex].vertexindices[c];
                    if (*v >= originalvertices) { *v = map[*v - originalvertices]; }
                }
            }
            free(map); room->vertexcount = next;
        }
    }
    if (cuts)
    { work.dirty = TRUE; BgDocumentFree(document); *document = work; memset(&work, 0, sizeof(work)); }
    *selection = result; result = NULL;
    *selectioncount = resultcount; *cutcount = cuts;
    ok = TRUE;
done:
    free(newfaces); free(sorted); free(result); free(cache); BgDocumentFree(&work);
    if (reason) { *reason = ok ? "" : why; }
    return ok;
}
