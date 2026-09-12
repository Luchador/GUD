/* Edge extrusion uses the source draw group, so the ordinary BG compiler
 * retains its complete native rendering state and handles vertex batching. */
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

/* Reverse the shared base edge relative to the source face. */
static const unsigned char g_ExtrudeCorners[2][3] = {{1,0,3}, {1,3,2}};

typedef struct BgExtrudeQuad {
    BgDocumentFace face;
    BgDocumentVertex vertices[4]; /* A, B, B+offset, A+offset */
    DWORD source[4];
} BgExtrudeQuad;

typedef struct BgExtrudeRoom {
    BgDocumentVertex *vertices;
    BgDocumentFace *faces;
    DWORD vertexcount, facecount, added;
} BgExtrudeRoom;

static BOOL BgExtrudeDelta(const BgDocument *document, const double offset[3],
    double delta[3], const char **why)
{
    *why = "No editable background is loaded.";
    if (!document || !document->rooms || !document->roomcount) { return FALSE; }
    *why = "The extrusion has an invalid offset or level scale.";
    if (!offset || !isfinite(document->levelscale) || document->levelscale <= 0) { return FALSE; }
    for (int axis = 0; axis < 3; axis++)
    {
        delta[axis] = round(offset[axis] * document->levelscale);
        if (!isfinite(delta[axis]) || fabs(delta[axis]) > 65535) { return FALSE; }
    }
    return TRUE;
}

static BOOL BgExtrudeBuildQuad(const BgDocument *document, const BgDocumentEdgeRef *edge,
    const double delta[3], BgExtrudeQuad *quad, const char **why)
{
    const BgDocumentRoom *room;
    const BgDocumentFace *face = BgDocumentFindFace(document, &edge->face, &room);
    const BgDocumentVertex *a, *b, *c;
    double along[3], across[3], edge2 = 0, height2 = 0, projection = 0, travel = 0;
    double side = 0, width2 = 0, length, height, distance, uv[4][2];
    *why = "A selected background edge no longer exists.";
    if (!face || edge->corner >= 3 || face->layer > 1
        || face->drawgroup >= room->layers[face->layer].groupcount) { return FALSE; }
    for (int i = 0; i < 3; i++)
    { if (face->vertexindices[i] >= room->vertexcount) { return FALSE; } }
    quad->face = *face;
    quad->source[0] = quad->source[3] = face->vertexindices[edge->corner];
    quad->source[1] = quad->source[2] = face->vertexindices[(edge->corner+1)%3];
    a = &room->vertices[quad->source[0]];
    b = &room->vertices[quad->source[1]];
    c = &room->vertices[face->vertexindices[(edge->corner+2)%3]];
    const double av[3] = {a->x,a->y,a->z}, bv[3] = {b->x,b->y,b->z}, cv[3] = {c->x,c->y,c->z};
    for (int axis = 0; axis < 3; axis++)
    {
        along[axis] = bv[axis]-av[axis]; across[axis] = cv[axis]-av[axis];
        edge2 += along[axis]*along[axis]; projection += along[axis]*across[axis];
    }
    *why = "Cannot extrude a zero-length background edge.";
    if (!(edge2 > 0)) { return FALSE; }
    length = sqrt(edge2); projection /= length;
    for (int axis = 0; axis < 3; axis++)
    {
        along[axis] /= length; across[axis] -= along[axis]*projection;
        height2 += across[axis]*across[axis]; travel += delta[axis]*along[axis];
    }
    for (int axis = 0; axis < 3; axis++)
    {
        double perpendicular = delta[axis]-travel*along[axis];
        width2 += perpendicular*perpendicular; side += delta[axis]*across[axis];
    }
    *why = "This axis is parallel to a selected edge and would create a zero-area quad. Choose another axis.";
    if (width2 < 1e-12) { return FALSE; }
    height = sqrt(height2);
    /* Unfold the new surface into the source triangle's plane. In-plane
     * drags continue the affine mapping exactly; folds retain texture scale.
     * A perpendicular fold extends away from the source triangle. */
    distance = sqrt(width2) * (side > 1e-9 ? 1 : -1);
    uv[0][0] = a->s; uv[0][1] = a->t;
    uv[1][0] = b->s; uv[1][1] = b->t;
    double edgeuv[2] = {b->s-(double)a->s, b->t-(double)a->t};
    double thirdUV[2] = {c->s-(double)a->s, c->t-(double)a->t};
    double determinant = edgeuv[0]*thirdUV[1]-edgeuv[1]*thirdUV[0];
    if (fabs(determinant) < 1e-9 || height < 1e-9)
    {
        /* Unmapped/degenerate UVs cannot define an affine projection. Keep
         * the edge UV direction and density, or use one texel per world cm.
         * Only the new surface gets any required UV seam at its base. */
        if (hypot(edgeuv[0], edgeuv[1]) < 1e-9)
        { edgeuv[0] = length * 32 / document->levelscale; edgeuv[1] = 0; }
        uv[1][0] = uv[0][0]+edgeuv[0]; uv[1][1] = uv[0][1]+edgeuv[1];
        thirdUV[0] = -edgeuv[1]; thirdUV[1] = edgeuv[0];
        height = length; projection = 0;
    }
    for (int axis = 0; axis < 2; axis++)
    {
        double u = edgeuv[axis]/length;
        double v = (thirdUV[axis]-u*projection)/height;
        double shift = travel*u + distance*v;
        uv[2][axis] = uv[1][axis]+shift; uv[3][axis] = uv[0][axis]+shift;
    }
    for (int i = 0; i < 4; i++)
    {
        BgDocumentVertex *v = &quad->vertices[i];
        *v = room->vertices[quad->source[i]];
        int position[3] = {v->x,v->y,v->z};
        for (int axis = 0; axis < 3; axis++)
        {
            double p = position[axis] + (i >= 2 ? delta[axis] : 0);
            *why = "An extruded vertex exceeds its room's coordinate range. Use a shorter drag.";
            if (p < SHRT_MIN || p > SHRT_MAX) { return FALSE; }
            position[axis] = (int)p;
        }
        v->x = position[0]; v->y = position[1]; v->z = position[2];
        *why = "The extrusion exceeds the native UV range. Use a shorter drag or adjust the source UVs.";
        for (int axis = 0; axis < 2; axis++)
        {
            double value = round(uv[i][axis]);
            if (!isfinite(value) || value < SHRT_MIN || value > SHRT_MAX) { return FALSE; }
            if (axis) { v->t = (short)value; } else { v->s = (short)value; }
        }
    }
    return TRUE;
}

BOOL BgDocumentPreviewEdgeExtrusion(const BgDocument *document,
    const BgDocumentEdgeRef *edges, DWORD count, const double offset[3],
    BgVertex *triangles, double applied[3], const char **reasonout)
{
    double delta[3];
    if (!BgExtrudeDelta(document, offset, delta, reasonout)) { return FALSE; }
    *reasonout = "No background edges are selected.";
    if (!edges || !count || count > UINT32_MAX/(6*sizeof(*triangles)) || !triangles || !applied) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { applied[axis] = delta[axis]/document->levelscale; }
    if (!delta[0] && !delta[1] && !delta[2])
    { memset(triangles, 0, (size_t)count*6*sizeof(*triangles)); *reasonout = ""; return TRUE; }
    for (DWORD i = 0; i < count; i++)
    {
        BgExtrudeQuad quad;
        if (!BgExtrudeBuildQuad(document, &edges[i], delta, &quad, reasonout)) { return FALSE; }
        const BgDocumentRoom *room = &document->rooms[quad.face.room];
        for (int corner = 0; corner < 6; corner++)
        {
            const BgDocumentVertex *v = &quad.vertices[g_ExtrudeCorners[corner/3][corner%3]];
            BgVertex *p = &triangles[i*6+corner]; float world[3];
            BgDocumentGetWorldPosition(document, room, v, world);
            memset(p, 0, sizeof(*p));
            p->x = world[0]; p->y = world[1]; p->z = world[2];
            p->s = v->s/32.0f; p->t = v->t/32.0f;
            p->r = v->r; p->g = v->g; p->b = v->b;
            p->a = BgDocumentPreviewVertexAlpha(room, &quad.face, v->a);
        }
    }
    *reasonout = "";
    return TRUE;
}

static BOOL BgExtrudeSameVertex(const BgDocumentVertex *a, const BgDocumentVertex *b)
{
    /* The original ID is kept in staging until all edges have been added.
     * Never weld unrelated coincident vertices, rooms, or UV seams. */
    return a->id == b->id && a->x == b->x && a->y == b->y && a->z == b->z
        && a->s == b->s && a->t == b->t;
}

BOOL BgDocumentExtrudeEdges(BgDocument *document, const BgDocumentEdgeRef *edges,
    DWORD count, const double offset[3], BgDocumentEdgeRef *out,
    DWORD *createdout, const char **reasonout)
{
    double delta[3];
    BgExtrudeQuad *quads = NULL;
    BgExtrudeRoom *rooms = NULL;
    DWORD nextvertex, nextface;
    BOOL ok = FALSE;
    *createdout = 0;
    if (!BgExtrudeDelta(document, offset, delta, reasonout)) { return FALSE; }
    *reasonout = "No background edges are selected.";
    if (!count || !edges || !out) { return FALSE; }
    if (!delta[0] && !delta[1] && !delta[2]) { *reasonout = ""; return TRUE; }
    *reasonout = "Too many background edges to extrude.";
    if (count > UINT32_MAX / sizeof(*quads) || count > UINT32_MAX/4
        || !document->nextvertexid || !document->nextfaceid
        || document->nextvertexid > UINT32_MAX-count*4
        || document->nextfaceid > UINT32_MAX-count*2
        || document->facecount > UINT32_MAX-count*2) { return FALSE; }
    *reasonout = "Out of memory extruding background edges.";
    quads = malloc((size_t)count*sizeof(*quads));
    rooms = calloc((size_t)document->roomcount+1, sizeof(*rooms));
    if (!quads || !rooms) { goto done; }
    for (DWORD i = 0; i < count; i++)
    {
        if (!BgExtrudeBuildQuad(document, &edges[i], delta, &quads[i], reasonout)) { goto done; }
        /* A shared edge is selected once, even when two faces own it. Reject
         * duplicate input instead of silently emitting overlapping quads. */
        for (DWORD j = 0; j < i; j++)
        {
            if (quads[i].face.room == quads[j].face.room
                && ((quads[i].source[0] == quads[j].source[0] && quads[i].source[1] == quads[j].source[1])
                 || (quads[i].source[0] == quads[j].source[1] && quads[i].source[1] == quads[j].source[0])))
            { *reasonout = "The same background edge was selected twice."; goto done; }
        }
        rooms[quads[i].face.room].added++;
    }
    for (DWORD r = 1; r <= document->roomcount; r++)
    {
        BgExtrudeRoom *pending = &rooms[r];
        const BgDocumentRoom *room = &document->rooms[r];
        DWORD n = pending->added;
        if (!n) { continue; }
        *reasonout = "The extrusion exceeds the background vertex or face limit.";
        if (n > 0x100000u/4 || room->vertexcount > 0x100000u-n*4
            || n*2 > UINT32_MAX/sizeof(*pending->faces)
            || room->facecount > UINT32_MAX/sizeof(*pending->faces)-n*2) { goto done; }
        *reasonout = "Out of memory extruding background edges.";
        pending->vertices = malloc((size_t)(room->vertexcount+n*4)*sizeof(*pending->vertices));
        pending->faces = malloc((size_t)(room->facecount+n*2)*sizeof(*pending->faces));
        if (!pending->vertices || !pending->faces) { goto done; }
        pending->vertexcount = room->vertexcount; pending->facecount = room->facecount;
        memcpy(pending->vertices, room->vertices, room->vertexcount*sizeof(*room->vertices));
        memcpy(pending->faces, room->faces, room->facecount*sizeof(*room->faces));
    }
    nextface = document->nextfaceid; nextvertex = document->nextvertexid;
    for (DWORD i = 0; i < count; i++)
    {
        const BgExtrudeQuad *q = &quads[i];
        const BgDocumentRoom *room = &document->rooms[q->face.room];
        BgExtrudeRoom *pending = &rooms[q->face.room];
        DWORD indices[4];
        for (int c = 0; c < 4; c++)
        {
            DWORD index = q->source[c];
            if (!BgExtrudeSameVertex(&pending->vertices[index], &q->vertices[c]))
            {
                for (index = room->vertexcount; index < pending->vertexcount; index++)
                { if (BgExtrudeSameVertex(&pending->vertices[index], &q->vertices[c])) { break; } }
                if (index == pending->vertexcount)
                {
                    pending->vertices[index] = q->vertices[c];
                    pending->vertices[index].usecount = 0;
                    pending->vertexcount++;
                }
            }
            indices[c] = index;
        }
        for (int tri = 0; tri < 2; tri++)
        {
            BgDocumentFace *face = &pending->faces[pending->facecount++];
            *face = q->face; face->id = nextface++;
            for (int c = 0; c < 3; c++)
            {
                face->vertexindices[c] = indices[g_ExtrudeCorners[tri][c]];
                pending->vertices[face->vertexindices[c]].usecount++;
            }
            if (tri == 1)
            { out[i] = (BgDocumentEdgeRef){{face->id,face->room,face->layer,0},1}; }
        }
    }
    /* All allocations and validation succeeded. Publish the affected rooms. */
    for (DWORD r = 1; r <= document->roomcount; r++)
    {
        BgDocumentRoom *room = &document->rooms[r];
        BgExtrudeRoom *pending = &rooms[r];
        if (!pending->added) { continue; }
        for (DWORD v = room->vertexcount; v < pending->vertexcount; v++)
        { pending->vertices[v].id = nextvertex++; }
        free(room->vertices); free(room->faces);
        room->vertices = pending->vertices; pending->vertices = NULL;
        room->faces = pending->faces; pending->faces = NULL;
        room->vertexcount = pending->vertexcount;
        room->facecount = room->facecapacity = pending->facecount;
    }
    document->nextvertexid = nextvertex; document->nextfaceid = nextface;
    document->facecount += count*2; document->dirty = TRUE;
    *createdout = count; *reasonout = ""; ok = TRUE;
done:
    if (rooms) for (DWORD r = 1; r <= document->roomcount; r++)
    { free(rooms[r].vertices); free(rooms[r].faces); }
    free(rooms); free(quads);
    return ok;
}
