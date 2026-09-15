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
            if ((selected && !selected[f]) || face->layer != layer || face->drawgroup != g) { continue; }
            if (before.othermode != after.othermode || before.othermodehigh != after.othermodehigh
                || ((before.geometrymode ^ after.geometrymode) & ~0x2000u)
                || before.environmentalpha != after.environmentalpha || before.primitivealpha != after.primitivealpha
                || before.surfacepolicy != after.surfacepolicy || before.surfacebasemode != after.surfacebasemode)
            { return FALSE; }
        }
    }
    return TRUE;
}

static BOOL BgRoomSameLayerPrefix(const BgDocumentLayerData *dst, const BgDocumentLayerData *src)
{
    if (!src->groupcount || dst->groupcount < src->groupcount) { return FALSE; }
    for (DWORD g = 0; g < src->groupcount; g++)
    {
        const BgDocumentDrawGroup *a = &src->groups[g], *b = &dst->groups[g];
        if (a->commandsize != b->commandsize
            || (a->commandsize && memcmp(a->commands, b->commands, a->commandsize))) { return FALSE; }
    }
    return TRUE;
}

/* Append to a staged document. NULL selection copies every source face.
 * Both room transfers and clipboard paste preserve inherited draw state. */
static BOOL BgRoomAppendFaces(BgDocument *doc, const BgDocumentRoom *src, DWORD target,
    const unsigned char *selected, BOOL newids, const double translation[3],
    BgFaceRef *out, const char **reasonout)
{
    BgDocumentRoom *dst = &doc->rooms[target];
    DWORD *vertices = malloc((size_t)src->vertexcount * sizeof(*vertices));
    DWORD faces = 0, added = 0, i, f, bases[2] = {0, 0};
    BOOL layers[2] = {FALSE, FALSE}, ok = FALSE;
    double offset[3];
    if (src->vertexcount && !vertices) { goto done; }
    for (i = 0; i < src->vertexcount; i++) { vertices[i] = (DWORD)-1; }
    for (f = 0; f < src->facecount; f++)
    {
        const BgDocumentFace *face = &src->faces[f];
        if (selected && !selected[f]) { continue; }
        if (face->layer > 1 || face->drawgroup >=
            (src->layers[face->layer].groupcount ? src->layers[face->layer].groupcount : 1))
        { *reasonout = "A background face has invalid display-list state."; goto done; }
        faces++; layers[face->layer] = TRUE;
        for (unsigned int c = 0; c < 3; c++)
        {
            DWORD v = face->vertexindices[c];
            if (v >= src->vertexcount) { *reasonout = "A selected face references a missing vertex."; goto done; }
            if (vertices[v] == (DWORD)-1) { vertices[v] = added++; }
        }
    }
    for (i = 0; i < 3; i++)
    {
        offset[i] = (double)src->origin[i] - dst->origin[i] + translation[i];
        if (!isfinite(offset[i]) || offset[i] != floor(offset[i]))
        { *reasonout = "These room origins cannot preserve exact vertex positions on the native coordinate grid."; goto done; }
    }
    if (dst->vertexcount > 0x100000u || added > 0x100000u - dst->vertexcount
        || !doc->nextvertexid || added > (DWORD)-1 - doc->nextvertexid
        || faces > (DWORD)-1 / sizeof(*dst->faces)
        || dst->facecount > (DWORD)-1 / sizeof(*dst->faces) - faces
        || (newids && (!doc->nextfaceid || faces > (DWORD)-1 - doc->nextfaceid
            || faces > (DWORD)-1 - doc->facecount)))
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
        /* A level-local paste normally still has the exact source groups.
         * Reuse their original positions: replaying a layer at its end could
         * inherit settings which its first faces did not originally have. */
        if (newids && BgRoomSameLayerPrefix(&dst->layers[i], &src->layers[i])) { continue; }
        if (!BgRoomAppendLayer(&dst->layers[i], &src->layers[i], &bases[i], reasonout)) { goto done; }
        if (!BgRoomCompatibleLayer(src, selected, &dst->layers[i], bases[i], i))
        { *reasonout = "These faces depend on render state that conflicts with the destination room."; goto done; }
    }
    for (f = 0, i = 0; f < src->facecount; f++)
    {
        if (!selected || selected[f])
        {
            BgDocumentFace *face = &dst->faces[dst->facecount++];
            *face = src->faces[f]; face->room = (unsigned short)target;
            if (newids) { face->id = doc->nextfaceid++; }
            face->drawgroup += bases[face->layer];
            for (unsigned int c = 0; c < 3; c++)
            {
                DWORD v = face->vertexindices[c];
                face->vertexindices[c] = vertices[v];
                dst->vertices[vertices[v]].usecount++;
            }
            if (out) { out[i++] = (BgFaceRef){face->id, face->room, face->layer, 0}; }
        }
    }
    if (newids) { doc->facecount += faces; }
    ok = TRUE;
done:
    free(vertices);
    return ok;
}

static BOOL BgRoomMoveFrom(BgDocument *doc, DWORD source, DWORD target,
    const BgFaceRef *refs, DWORD count, const char **reasonout)
{
    BgDocumentRoom *src = &doc->rooms[source];
    unsigned char *selected = calloc(src->facecount, 1);
    const double offset[3] = {0};
    DWORD i, f;
    if (!selected) { return FALSE; }
    for (i = 0; i < count; i++) if (refs[i].room == source)
    {
        const BgDocumentFace *face = BgDocumentFindFace(doc, &refs[i], NULL);
        selected[face - src->faces] = TRUE;
    }
    if (!BgRoomAppendFaces(doc, src, target, selected, FALSE, offset, NULL, reasonout))
    { free(selected); return FALSE; }
    for (f = 0, i = 0; f < src->facecount; f++)
    {
        if (selected[f])
        {
            for (unsigned int c = 0; c < 3; c++)
            { src->vertices[src->faces[f].vertexindices[c]].usecount--; }
        }
        else { src->faces[i++] = src->faces[f]; }
    }
    /* Retain orphan vertices and room slots for native bounds. */
    src->facecount = i;
    free(selected);
    return TRUE;
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

BOOL BgDocumentCopyFaces(const BgDocument *document, const BgFaceRef *refs,
    DWORD count, BgDocument *clipboard, const char **reasonout)
{
    BgDocument view = {0}, copy = {0};
    unsigned char **selected = NULL;
    const char *ignored;
    DWORD i, r;
    BOOL ok = FALSE;
    if (!reasonout) { reasonout = &ignored; }
    *reasonout = "Select background faces to copy.";
    if (!document || !document->rooms || !clipboard || document == clipboard || !refs || !count
        || document->roomcount > 65535) { return FALSE; }
    view.roomcount = document->roomcount; view.levelscale = document->levelscale;
    view.nextfaceid = document->nextfaceid; view.nextvertexid = document->nextvertexid;
    *reasonout = "Out of memory copying background faces.";
    view.rooms = calloc((size_t)view.roomcount + 1, sizeof(*view.rooms));
    selected = calloc((size_t)view.roomcount + 1, sizeof(*selected));
    if (!view.rooms || !selected) { goto done; }
    for (i = 0; i < count; i++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], &room);
        if (!face || face->layer > 1 || face->drawgroup >= room->layers[face->layer].groupcount)
        { *reasonout = "A selected background face is no longer available."; goto done; }
        r = refs[i].room;
        if (!selected[r]) { selected[r] = calloc(room->facecount, 1); }
        if (!selected[r]) { goto done; }
        if (selected[r][face - room->faces]) { continue; }
        for (unsigned int c = 0; c < 3; c++) if (face->vertexindices[c] >= room->vertexcount)
        { *reasonout = "A selected face references a missing vertex."; goto done; }
        selected[r][face - room->faces] = TRUE;
        view.facecount++;
    }
    for (r = 1; r <= view.roomcount; r++) if (selected[r])
    {
        const BgDocumentRoom *src = &document->rooms[r];
        BgDocumentRoom *dst = &view.rooms[r];
        DWORD faces = 0;
        for (i = 0; i < src->facecount; i++) { faces += selected[r][i] != 0; }
        *dst = *src;
        dst->faces = malloc((size_t)faces * sizeof(*dst->faces));
        dst->facecount = 0;
        if (!dst->faces) { goto done; }
        for (i = 0; i < src->facecount; i++) if (selected[r][i])
        { dst->faces[dst->facecount++] = src->faces[i]; }
        /* Retain the state leading to the copied faces, but not unrelated
         * layers or later groups. Edits after this prefix cannot invalidate
         * reuse of the original draw groups when pasting. */
        for (unsigned int layer = 0; layer < 2; layer++)
        {
            DWORD groups = 0;
            for (i = 0; i < dst->facecount; i++)
            {
                const BgDocumentFace *face = &dst->faces[i];
                if (face->layer == layer && face->drawgroup >= groups) { groups = face->drawgroup + 1; }
            }
            dst->layers[layer].groupcount = groups;
        }
    }
    /* The view borrows vertices and state only from rooms being copied.
     * Clone takes ownership of an independent snapshot, with no portals. */
    if (!BgDocumentClone(&view, &copy, reasonout)) { goto done; }
    for (r = 1; r <= copy.roomcount; r++)
    {
        BgDocumentRoom *room = &copy.rooms[r];
        for (i = 0; i < room->vertexcount; i++) { room->vertices[i].usecount = 0; }
        for (i = 0; i < room->facecount; i++) for (unsigned int c = 0; c < 3; c++)
        { room->vertices[room->faces[i].vertexindices[c]].usecount++; }
    }
    BgDocumentFree(clipboard);
    *clipboard = copy;
    *reasonout = ""; ok = TRUE;
done:
    for (r = 0; r <= view.roomcount; r++)
    {
        if (view.rooms) { free(view.rooms[r].faces); }
        if (selected) { free(selected[r]); }
    }
    free(view.rooms); free(selected);
    return ok;
}

BOOL BgDocumentPasteFaces(BgDocument *document, const BgDocument *clipboard,
    const double offset[3], BgFaceRef **facesout, DWORD *countout, const char **reasonout)
{
    BgDocument staged = {0};
    BgFaceRef *faces = NULL;
    DWORD r, count = 0;
    double translation[3];
    const char *ignored;
    if (!reasonout) { reasonout = &ignored; }
    *reasonout = "There are no copied faces for this level.";
    if (facesout) { *facesout = NULL; }
    if (countout) { *countout = 0; }
    if (!document || !document->rooms || !clipboard || !clipboard->rooms || !clipboard->facecount
        || !offset || !facesout || !countout || document == clipboard
        || document->roomcount != clipboard->roomcount || document->levelscale != clipboard->levelscale
        || !isfinite(document->levelscale) || document->levelscale <= 0) { return FALSE; }
    for (r = 0; r < 3; r++)
    {
        translation[r] = round(offset[r] * document->levelscale);
        if (!isfinite(translation[r]) || fabs(translation[r]) > 65535)
        { *reasonout = "The paste offset exceeds the native coordinate range."; return FALSE; }
    }
    if (clipboard->facecount > (DWORD)-1 / sizeof(*faces)) { return FALSE; }
    faces = malloc((size_t)clipboard->facecount * sizeof(*faces));
    *reasonout = "Out of memory pasting background faces.";
    if (!faces || !BgDocumentClone(document, &staged, reasonout)) { goto fail; }
    *reasonout = "Out of memory pasting background faces.";
    for (r = 1; r <= clipboard->roomcount; r++)
    {
        const BgDocumentRoom *src = &clipboard->rooms[r];
        if (!src->facecount) { continue; }
        if (src->facecount > clipboard->facecount - count) { goto fail; }
        if (!BgRoomAppendFaces(&staged, src, r, NULL, TRUE, translation, faces + count, reasonout)
            || !BgRoomOrderFaces(&staged.rooms[r])) { goto fail; }
        count += src->facecount;
    }
    if (count != clipboard->facecount) { goto fail; }
    BgDocumentFree(document);
    *document = staged; document->dirty = TRUE;
    *facesout = faces; *countout = count; *reasonout = "";
    return TRUE;
fail:
    free(faces); BgDocumentFree(&staged);
    return FALSE;
}
