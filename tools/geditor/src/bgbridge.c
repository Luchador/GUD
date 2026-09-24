/* A bridge reuses native vertices and the first edge's complete draw state. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static BOOL BgBridgeTriangles(const BgDocumentRoom *room, const DWORD indices[4],
    const unsigned char corners[2][3], DWORD count)
{
    double normals[2][3];
    for (DWORD t = 0; t < count; t++)
    {
        const BgDocumentVertex *a = &room->vertices[indices[corners[t][0]]];
        const BgDocumentVertex *b = &room->vertices[indices[corners[t][1]]];
        const BgDocumentVertex *c = &room->vertices[indices[corners[t][2]]];
        double u[3] = {b->x-a->x, b->y-a->y, b->z-a->z};
        double v[3] = {c->x-a->x, c->y-a->y, c->z-a->z};
        double area = 0;
        for (int axis = 0; axis < 3; axis++)
        {
            normals[t][axis] = u[(axis+1)%3]*v[(axis+2)%3] - u[(axis+2)%3]*v[(axis+1)%3];
            area += normals[t][axis]*normals[t][axis];
        }
        if (area == 0) { return FALSE; }
    }
    return count == 1 || normals[0][0]*normals[1][0] + normals[0][1]*normals[1][1]
        + normals[0][2]*normals[1][2] >= 0;
}

static BOOL BgBridgePrepare(const BgDocument *document, const BgDocumentEdgeRef edges[2],
    BgDocumentFace faces[2], DWORD *count, const char **why)
{
    const BgDocumentRoom *room = NULL;
    const BgDocumentFace *source[2];
    DWORD indices[4];
    static const unsigned char diagonals[2][2][3] = {{{1,0,3}, {1,3,2}}, {{1,0,2}, {0,3,2}}};
    int diagonal, shared = 0;
    *why = "Select two boundary edges in the same background room and layer.";
    if (!document || !document->rooms || !edges || edges[0].face.room != edges[1].face.room
        || edges[0].face.layer != edges[1].face.layer) { return FALSE; }
    for (int e = 0; e < 2; e++)
    {
        source[e] = BgDocumentFindFace(document, &edges[e].face, &room);
        if (!source[e] || edges[e].corner >= 3 || source[e]->layer > 1
            || source[e]->drawgroup >= room->layers[source[e]->layer].groupcount) { return FALSE; }
        for (int k = 0; k < 2; k++)
        {
            indices[e*2+k] = source[e]->vertexindices[(edges[e].corner+k)%3];
            if (indices[e*2+k] >= room->vertexcount) { return FALSE; }
        }
    }
    *why = "Select two distinct boundary edges on different background faces.";
    if (source[0] == source[1] || indices[0] == indices[1] || indices[2] == indices[3]) { return FALSE; }
    for (int a = 0; a < 2; a++) for (int b = 2; b < 4; b++)
    { shared += indices[a] == indices[b]; }
    if (shared == 2) { return FALSE; }
    *why = "The selected edges have incompatible winding at their shared vertex.";
    if (shared && indices[0] != indices[3] && indices[1] != indices[2]) { return FALSE; }
    *count = shared ? 1 : 2;
    *why = "Only boundary edges can be bridged; an edge already shared by faces is not a boundary.";
    for (int e = 0; e < 2; e++)
    {
        DWORD uses = 0;
        for (DWORD f = 0; f < room->facecount; f++) for (int c = 0; c < 3; c++)
        {
            DWORD a = room->faces[f].vertexindices[c], b = room->faces[f].vertexindices[(c+1)%3];
            if ((a == indices[e*2] && b == indices[e*2+1])
                || (b == indices[e*2] && a == indices[e*2+1])) { uses++; }
        }
        if (uses != 1) { return FALSE; }
    }
    *why = "These edges would form a twisted or zero-area bridge. Check their positions and face winding.";
    if (shared)
    {
        /* Reverse both boundary edges, omitting the repeated corner. */
        diagonal = indices[0] == indices[3] ? 1 : 0;
        if (!BgBridgeTriangles(room, indices, diagonals[diagonal], *count)) { return FALSE; }
    }
    else for (diagonal = 0; diagonal < 2; diagonal++)
    { if (BgBridgeTriangles(room, indices, diagonals[diagonal], *count)) { break; } }
    if (diagonal == 2) { return FALSE; }
    *why = "The background has too many faces or vertex references to add a bridge.";
    if (!document->nextfaceid || document->nextfaceid > UINT32_MAX-*count
        || document->facecount > UINT32_MAX-*count || room->facecount > UINT32_MAX-*count
        || (size_t)room->facecount + *count > SIZE_MAX/sizeof(*faces)) { return FALSE; }
    for (DWORD t = 0; t < *count; t++)
    {
        faces[t] = *source[0];
        faces[t].id = document->nextfaceid + t;
        faces[t].uvseams = 0;
        for (int c = 0; c < 3; c++)
        {
            DWORD v = indices[diagonals[diagonal][t][c]];
            if (room->vertices[v].usecount > UINT32_MAX-*count) { return FALSE; }
            faces[t].vertexindices[c] = v;
        }
        BgDocumentInheritFaceSeams(room->vertices, source[0], &faces[t]);
        BgDocumentFace other = faces[t];
        BgDocumentInheritFaceSeams(room->vertices, source[1], &other);
        faces[t].uvseams |= other.uvseams;
    }
    *why = "";
    return TRUE;
}

BOOL BgDocumentCanBridgeEdges(const BgDocument *document,
    const BgDocumentEdgeRef edges[2], const char **reasonout)
{
    BgDocumentFace faces[2];
    DWORD count;
    const char *unused;
    return BgBridgePrepare(document, edges, faces, &count, reasonout ? reasonout : &unused);
}

BOOL BgDocumentBridgeEdges(BgDocument *document, const BgDocumentEdgeRef edges[2],
    BgFaceRef out[2], DWORD *countout, const char **reasonout)
{
    BgDocumentFace additions[2], *faces;
    BgDocumentRoom *room;
    DWORD count;
    const char *unused, **why = reasonout ? reasonout : &unused;
    if (countout) { *countout = 0; }
    if (!BgBridgePrepare(document, edges, additions, &count, why)) { return FALSE; }
    room = &document->rooms[edges[0].face.room];
    faces = malloc(((size_t)room->facecount+count)*sizeof(*faces));
    if (!faces) { *why = "Out of memory adding the bridge."; return FALSE; }
    memcpy(faces, room->faces, room->facecount*sizeof(*faces));
    memcpy(faces+room->facecount, additions, count*sizeof(*additions));
    free(room->faces);
    room->faces = faces;
    room->facecount += count;
    room->facecapacity = room->facecount;
    document->facecount += count;
    document->nextfaceid += count;
    document->dirty = TRUE;
    for (DWORD t = 0; t < count; t++)
    {
        for (int c = 0; c < 3; c++) { room->vertices[additions[t].vertexindices[c]].usecount++; }
        if (out) { out[t] = (BgFaceRef){additions[t].id, additions[t].room, additions[t].layer, 0}; }
    }
    if (countout) { *countout = count; }
    return TRUE;
}
