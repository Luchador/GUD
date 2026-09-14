/* Move background faces between existing rooms without moving them in space. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static DWORD BgRoomRead32(const unsigned char *p)
{
    return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3];
}

static void BgRoomReadGroup(BgRenderState *state, const BgDocumentDrawGroup *group)
{
    DWORD i;
    for (i = 0; i < group->commandsize; i += 8)
    { BgRenderStateRead(state, BgRoomRead32(group->commands + i), BgRoomRead32(group->commands + i + 4)); }
}

/* Replay the entire source layer, including state-only groups and its tail.
 * A group's commands are incremental, so copying only the selected group's
 * commands would lose inherited render modes and authored surface overrides.
 * The destination's existing groups/faces remain first and unchanged. */
static BOOL BgRoomAppendLayer(BgDocumentLayerData *dst, const BgDocumentLayerData *src,
    DWORD *baseout, const char **reasonout)
{
    DWORD count = src->groupcount ? src->groupcount : 1;
    DWORD base = dst->groupcount ? dst->groupcount : 1;
    BgDocumentDrawGroup *groups;
    DWORD i;
    if (base > (DWORD)-1 - count || base + count > (DWORD)-1 / sizeof(*groups)) { return FALSE; }
    groups = realloc(dst->groups, (size_t)(base + count) * sizeof(*groups));
    if (!groups) { return FALSE; }
    memset(groups + dst->groupcount, 0, (base + count - dst->groupcount) * sizeof(*groups));
    dst->groups = groups;
    dst->groupcount = dst->groupcapacity = base + count;
    dst->sourcepresent = TRUE;
    *baseout = base;
    for (i = 0; i < count; i++)
    {
        const BgDocumentDrawGroup *from = src->groupcount ? &src->groups[i] : NULL;
        BgDocumentDrawGroup *to = &groups[base + i];
        DWORD size = from ? from->commandsize : 0;
        /* Reset editor policy at the room boundary. This marker has no RDP
         * side effect; subsequent source overrides take precedence as usual. */
        DWORD prefix = i == 0 ? 8 : 0;
        if ((size & 7u) || (size && !from->commands) || size > (DWORD)-1 - prefix)
        { *reasonout = "A source room has malformed display-list state."; return FALSE; }
        to->commandsize = to->commandcapacity = size + prefix;
        if (!to->commandsize) { continue; }
        to->commands = malloc(to->commandsize);
        if (!to->commands) { return FALSE; }
        if (prefix)
        {
            DWORD w0 = BG_SURFACE_MARKER, w1 = BG_SURFACE_TAG_VALUE(BG_SURFACE_AUTO, 0);
            unsigned int b;
            for (b = 0; b < 4; b++)
            { to->commands[b] = (unsigned char)(w0 >> (24 - b * 8)); to->commands[b + 4] = (unsigned char)(w1 >> (24 - b * 8)); }
        }
        if (size) { memcpy(to->commands + prefix, from->commands, size); }
    }
    return TRUE;
}

/* Do not silently change faces which rely on a different inherited pipeline.
 * Per-face materials/culling are emitted by the compiler independently. */
static BOOL BgRoomCompatibleLayer(const BgDocumentRoom *src, const unsigned char *selected,
    const BgDocumentLayerData *dst, DWORD base, unsigned int layer)
{
    BgRenderState before, after;
    DWORD g, f;
    BgRenderStateInit(&before, layer == BG_GEOMETRY_SECONDARY);
    BgRenderStateInit(&after, layer == BG_GEOMETRY_SECONDARY);
    for (g = 0; g < base; g++) { BgRoomReadGroup(&after, &dst->groups[g]); }
    for (g = 0; g < dst->groupcount - base; g++)
    {
        if (src->layers[layer].groupcount) { BgRoomReadGroup(&before, &src->layers[layer].groups[g]); }
        BgRoomReadGroup(&after, &dst->groups[base + g]);
        for (f = 0; f < src->facecount; f++)
        {
            const BgDocumentFace *face = &src->faces[f];
            if (!selected[f] || face->layer != layer || face->drawgroup != g) { continue; }
            if (before.othermode != after.othermode || before.othermodehigh != after.othermodehigh
                || ((before.geometrymode ^ after.geometrymode) & ~0x2000u)
                || before.environmentalpha != after.environmentalpha || before.primitivealpha != after.primitivealpha
                || before.surfacepolicy != after.surfacepolicy || before.surfacebasemode != after.surfacebasemode)
            { return FALSE; }
        }
    }
    return TRUE;
}

static BOOL BgRoomMoveFrom(BgDocument *doc, DWORD source, DWORD target,
    const BgFaceRef *refs, DWORD count, const char **reasonout)
{
    BgDocumentRoom *src = &doc->rooms[source], *dst = &doc->rooms[target];
    unsigned char *selected = calloc(src->facecount, 1);
    DWORD *vertices = malloc((size_t)src->vertexcount * sizeof(*vertices));
    DWORD faces = 0, added = 0, i, f, bases[2] = {0, 0};
    BOOL layers[2] = {FALSE, FALSE}, ok = FALSE;
    double offset[3];
    if (!selected || (src->vertexcount && !vertices)) { goto done; }
    for (i = 0; i < src->vertexcount; i++) { vertices[i] = (DWORD)-1; }
    for (i = 0; i < count; i++)
    {
        const BgDocumentFace *face;
        if (refs[i].room != source) { continue; }
        face = BgDocumentFindFace(doc, &refs[i], NULL);
        f = (DWORD)(face - src->faces);
        if (selected[f]) { continue; }
        selected[f] = TRUE; faces++; layers[face->layer] = TRUE;
        for (unsigned int c = 0; c < 3; c++)
        {
            DWORD v = face->vertexindices[c];
            if (v >= src->vertexcount) { *reasonout = "A selected face references a missing vertex."; goto done; }
            if (vertices[v] == (DWORD)-1) { vertices[v] = added++; }
        }
    }
    for (i = 0; i < 3; i++)
    {
        offset[i] = (double)src->origin[i] - dst->origin[i];
        if (!isfinite(offset[i]) || offset[i] != floor(offset[i]))
        { *reasonout = "These room origins cannot preserve exact vertex positions on the native coordinate grid."; goto done; }
    }
    if (dst->vertexcount > 0x100000u || added > 0x100000u - dst->vertexcount
        || !doc->nextvertexid || added > (DWORD)-1 - doc->nextvertexid
        || dst->facecount > (DWORD)-1 / sizeof(*dst->faces) - faces)
    { *reasonout = "The destination room would exceed the native geometry limits."; goto done; }
    {
        BgDocumentVertex *v = realloc(dst->vertices, (size_t)(dst->vertexcount + added) * sizeof(*v));
        BgDocumentFace *p;
        if (!v) { goto done; }
        dst->vertices = v;
        p = realloc(dst->faces, (size_t)(dst->facecount + faces) * sizeof(*p));
        if (!p) { goto done; }
        dst->faces = p; dst->facecapacity = dst->facecount + faces;
    }
    for (i = 0; i < src->vertexcount; i++)
    {
        BgDocumentVertex *v;
        double xyz[3];
        if (vertices[i] == (DWORD)-1) { continue; }
        vertices[i] += dst->vertexcount;
        v = &dst->vertices[vertices[i]];
        *v = src->vertices[i];
        xyz[0] = v->x + offset[0]; xyz[1] = v->y + offset[1]; xyz[2] = v->z + offset[2];
        for (unsigned int c = 0; c < 3; c++)
        {
            if (xyz[c] < -32768 || xyz[c] > 32767)
            { *reasonout = "A moved vertex is outside the destination room's native coordinate range."; goto done; }
        }
        v->x = (short)xyz[0]; v->y = (short)xyz[1]; v->z = (short)xyz[2];
        v->room = (unsigned short)target; v->id = doc->nextvertexid++; v->usecount = 0;
    }
    dst->vertexcount += added;
    for (i = 0; i < 2; i++) if (layers[i])
    {
        if (!BgRoomAppendLayer(&dst->layers[i], &src->layers[i], &bases[i], reasonout)) { goto done; }
        if (!BgRoomCompatibleLayer(src, selected, &dst->layers[i], bases[i], i))
        { *reasonout = "These faces depend on render state that conflicts with the destination room."; goto done; }
    }
    for (f = 0, i = 0; f < src->facecount; f++)
    {
        if (selected[f])
        {
            BgDocumentFace *face = &dst->faces[dst->facecount++];
            *face = src->faces[f]; face->room = (unsigned short)target;
            face->drawgroup += bases[face->layer];
            for (unsigned int c = 0; c < 3; c++)
            {
                DWORD v = face->vertexindices[c];
                src->vertices[v].usecount--;
                face->vertexindices[c] = vertices[v];
                dst->vertices[vertices[v]].usecount++;
            }
        }
        else { src->faces[i++] = src->faces[f]; }
    }
    /* Retain orphan vertices and room slots: room bounds and all later room
     * numbers remain valid, even when this removes the room's last face. */
    src->facecount = i;
    ok = TRUE;
done:
    free(selected); free(vertices);
    return ok;
}

static BOOL BgRoomOrderFaces(BgDocumentRoom *room)
{
    BgDocumentFace *ordered = malloc((size_t)room->facecount * sizeof(*ordered));
    DWORD layer, group, f, count = 0;
    if (!ordered) { return FALSE; }
    /* Match the compiler's layer/group order, keeping face order within each
     * group. This also keeps blended geometry consistent after reloading. */
    for (layer = 0; layer < 2; layer++)
    for (group = 0; group < room->layers[layer].groupcount; group++)
    for (f = 0; f < room->facecount; f++)
    {
        if (room->faces[f].layer == layer && room->faces[f].drawgroup == group)
        { ordered[count++] = room->faces[f]; }
    }
    if (count != room->facecount) { free(ordered); return FALSE; }
    free(room->faces); room->faces = ordered; room->facecapacity = count;
    return TRUE;
}

BOOL BgDocumentMoveFacesToRoom(BgDocument *document, const BgFaceRef *refs,
    DWORD count, DWORD target, BOOL *changedout, const char **reasonout)
{
    BgDocument staged = {0};
    DWORD i, room;
    BOOL changed = FALSE;
    const char *ignored;
    if (!reasonout) { reasonout = &ignored; }
    *reasonout = "";
    if (changedout) { *changedout = FALSE; }
    if (!document || !document->rooms || !refs || !count || !target
        || target > document->roomcount || target > 65535)
    { *reasonout = "Choose an existing room number."; return FALSE; }
    for (i = 0; i < count; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);
        if (!face || face->layer > 1 || face->drawgroup >=
            (document->rooms[face->room].layers[face->layer].groupcount
                ? document->rooms[face->room].layers[face->layer].groupcount : 1))
        { *reasonout = "A selected background face is no longer available."; return FALSE; }
        changed |= refs[i].room != target;
    }
    if (!changed) { return TRUE; }
    /* All mutations occur on a private document. Allocation, range or render
     * validation failures leave both geometry and selection identities intact. */
    if (!BgDocumentClone(document, &staged, reasonout)) { return FALSE; }
    *reasonout = "Out of memory moving background faces between rooms.";
    for (room = 1; room <= document->roomcount; room++)
    {
        if (room == target) { continue; }
        for (i = 0; i < count && refs[i].room != room; i++) {}
        if (i < count && !BgRoomMoveFrom(&staged, room, target, refs, count, reasonout))
        { BgDocumentFree(&staged); return FALSE; }
    }
    if (!BgRoomOrderFaces(&staged.rooms[target])) { BgDocumentFree(&staged); return FALSE; }
    BgDocumentFree(document);
    *document = staged; document->dirty = TRUE;
    if (changedout) { *changedout = TRUE; }
    *reasonout = "";
    return TRUE;
}
