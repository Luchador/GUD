/* Turn a quad's shared diagonal without creating vertices or changing its
 * boundary. Native vertex identities keep UV/color seams separate. */
#include <stdint.h>
#include "bgdocument.h"

static void BgReverseNormal(const BgDocumentRoom *room, const DWORD indices[3], double n[3])
{
    const BgDocumentVertex *a = room->vertices + indices[0];
    const BgDocumentVertex *b = room->vertices + indices[1];
    const BgDocumentVertex *c = room->vertices + indices[2];
    double u[3] = {b->x-a->x, b->y-a->y, b->z-a->z};
    double v[3] = {c->x-a->x, c->y-a->y, c->z-a->z};
    for (int k = 0; k < 3; k++) { n[k] = u[(k+1)%3]*v[(k+2)%3] - u[(k+2)%3]*v[(k+1)%3]; }
}

static BOOL BgReversePrepare(const BgDocument *doc, const BgDocumentEdgeRef *edge,
    DWORD indices[2], BgDocumentFace faces[2], const char **why)
{
    const BgDocumentRoom *room;
    const BgDocumentFace *picked, *other = NULL;
    DWORD a, b, c, d = 0, uses = 0;
    *why = "Select one shared background edge forming the diagonal of a quad.";
    if (!doc || !doc->rooms || !edge || edge->corner >= 3
        || !(picked = BgDocumentFindFace(doc, &edge->face, &room)) || !room->vertices) { return FALSE; }
    for (int k = 0; k < 3; k++) if (picked->vertexindices[k] >= room->vertexcount) { return FALSE; }
    a = picked->vertexindices[edge->corner];
    b = picked->vertexindices[(edge->corner+1)%3];
    c = picked->vertexindices[(edge->corner+2)%3];
    if (a == b || b == c || c == a) { return FALSE; }
    indices[0] = (DWORD)(picked - room->faces);
    for (DWORD f = 0; f < room->facecount; f++) for (int k = 0; k < 3; k++)
    {
        const BgDocumentFace *face = room->faces + f;
        DWORD x = face->vertexindices[k], y = face->vertexindices[(k+1)%3];
        if (!((x == a && y == b) || (x == b && y == a))) { continue; }
        uses++;
        if (face == picked) { continue; }
        *why = "The triangles must share the edge with opposite winding.";
        if (x != b || y != a) { return FALSE; }
        other = face; indices[1] = f; d = face->vertexindices[(k+2)%3];
    }
    *why = "Reverse Edge requires exactly two triangles sharing the edge.";
    if (uses != 2 || !other) { return FALSE; }
    *why = "The triangles must form a quad with four distinct vertices.";
    if (d >= room->vertexcount || d == a || d == b || d == c) { return FALSE; }
    *why = "The triangles must have the same room, layer, material and render state.";
    if (picked->room != other->room || picked->layer != other->layer || picked->layer > 1
        || picked->textureid != other->textureid || picked->cullbackfaces != other->cullbackfaces
        || !BgMaterialEqual(&picked->material, &other->material)) { return FALSE; }
    const BgDocumentLayerData *layer = &room->layers[picked->layer];
    DWORD first = picked->drawgroup < other->drawgroup ? picked->drawgroup : other->drawgroup;
    DWORD last = picked->drawgroup > other->drawgroup ? picked->drawgroup : other->drawgroup;
    if (!layer->groups || last >= layer->groupcount) { return FALSE; }
    /* Materials contain texture/combiner state. Crossing an opaque state
     * command could otherwise move a lighting/render-mode boundary. */
    for (DWORD g = first+1; g <= last; g++)
    {
        const BgDocumentDrawGroup *group = layer->groups + g;
        if ((group->commandsize & 7u) || (group->commandsize && !group->commands)) { return FALSE; }
        for (DWORD off = 0; off < group->commandsize; off += 8)
            if (group->commands[off] != BG_G_PIPESYNC) { return FALSE; }
    }
    *why = "The opposite diagonal already belongs to another triangle.";
    for (DWORD f = 0; f < room->facecount; f++) for (int k = 0; k < 3; k++)
    {
        DWORD x = room->faces[f].vertexindices[k], y = room->faces[f].vertexindices[(k+1)%3];
        if ((x == c && y == d) || (x == d && y == c)) { return FALSE; }
    }
    faces[0] = *picked; faces[1] = *other;
    faces[0].vertexindices[0] = c; faces[0].vertexindices[1] = d; faces[0].vertexindices[2] = b;
    faces[1].vertexindices[0] = d; faces[1].vertexindices[1] = c; faces[1].vertexindices[2] = a;
    double normals[4][3], reference[3];
    BgReverseNormal(room, picked->vertexindices, normals[0]);
    BgReverseNormal(room, other->vertexindices, normals[1]);
    BgReverseNormal(room, faces[0].vertexindices, normals[2]);
    BgReverseNormal(room, faces[1].vertexindices, normals[3]);
    for (int k = 0; k < 3; k++) { reference[k] = normals[0][k] + normals[1][k]; }
    *why = "The triangles must form a convex quad without collapsed or inverted faces.";
    for (int f = 0; f < 4; f++)
        if (normals[f][0]*reference[0] + normals[f][1]*reference[1] + normals[f][2]*reference[2] <= 0)
        { return FALSE; }
    *why = "The quad has invalid vertex reference counts.";
    if (room->vertices[a].usecount < 2 || room->vertices[b].usecount < 2
        || !room->vertices[c].usecount || !room->vertices[d].usecount
        || room->vertices[c].usecount == UINT32_MAX || room->vertices[d].usecount == UINT32_MAX)
    { return FALSE; }
    /* Boundary seam guides remain on their original edges; a guide on the
     * removed diagonal follows the new diagonal. No corner data is changed. */
    BOOL diagonal = (picked->uvseams & (1u << edge->corner)) != 0;
    for (int k = 0; k < 3; k++)
        if (other->vertexindices[k] == b && other->vertexindices[(k+1)%3] == a)
        { diagonal |= (other->uvseams & (1u << k)) != 0; }
    for (int f = 0; f < 2; f++)
    {
        faces[f].uvseams = diagonal ? 1 : 0;
        for (int k = 1; k < 3; k++) for (int source = 0; source < 2; source++)
        {
            const BgDocumentFace *old = source ? other : picked;
            for (int j = 0; j < 3; j++)
                if ((old->uvseams & (1u << j))
                    && faces[f].vertexindices[k] == old->vertexindices[j]
                    && faces[f].vertexindices[(k+1)%3] == old->vertexindices[(j+1)%3])
                { faces[f].uvseams |= (unsigned char)(1u << k); }
        }
    }
    *why = "";
    return TRUE;
}

BOOL BgDocumentCanReverseEdge(const BgDocument *doc, const BgDocumentEdgeRef *edge, const char **reasonout)
{
    DWORD indices[2]; BgDocumentFace faces[2]; const char *unused;
    return BgReversePrepare(doc, edge, indices, faces, reasonout ? reasonout : &unused);
}

BOOL BgDocumentReverseEdge(BgDocument *doc, const BgDocumentEdgeRef *edge,
    BgDocumentEdgeRef *out, const char **reasonout)
{
    DWORD indices[2]; BgDocumentFace faces[2]; const char *unused;
    if (!BgReversePrepare(doc, edge, indices, faces, reasonout ? reasonout : &unused)) { return FALSE; }
    BgDocumentRoom *room = &doc->rooms[edge->face.room];
    for (int f = 0; f < 2; f++) for (int k = 0; k < 3; k++)
    { room->vertices[room->faces[indices[f]].vertexindices[k]].usecount--; }
    for (int f = 0; f < 2; f++)
    {
        room->faces[indices[f]] = faces[f];
        for (int k = 0; k < 3; k++) { room->vertices[faces[f].vertexindices[k]].usecount++; }
    }
    doc->dirty = TRUE;
    if (out) { *out = (BgDocumentEdgeRef){{faces[0].id, faces[0].room, faces[0].layer, faces[0].uvseams}, 0}; }
    return TRUE;
}
