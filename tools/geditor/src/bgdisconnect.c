/* UVs and colors belong to native vertices. Separate source identities while
 * preserving every attribute, face identity and authored draw group. */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

typedef struct BgDisconnectFace {
    BgFaceRef ref;
    unsigned int mask;
    DWORD index;
} BgDisconnectFace;

typedef struct BgDisconnectRoom {
    BgDocumentVertex *vertices;
    BgDocumentFace *faces;
    DWORD count, capacity, vertexcount;
} BgDisconnectRoom;

static int BgDisconnectCompareFaces(const void *left, const void *right)
{
    const BgFaceRef *a = &((const BgDisconnectFace *)left)->ref;
    const BgFaceRef *b = &((const BgDisconnectFace *)right)->ref;
    if (a->room != b->room) { return a->room < b->room ? -1 : 1; }
    if (a->layer != b->layer) { return a->layer < b->layer ? -1 : 1; }
    return a->faceid < b->faceid ? -1 : a->faceid > b->faceid;
}

/* Stage only touched rooms. No live arrays or IDs change until every
 * reference, allocation and native limit has passed. */
static BOOL BgDisconnectCorners(BgDocument *document, BgDisconnectFace *edits,
    DWORD count, DWORD *duplicatedout, const char **why)
{
    BgDisconnectRoom *rooms = NULL;
    DWORD nextvertex = document->nextvertexid, duplicated = 0;
    BOOL ok = FALSE;
    qsort(edits, count, sizeof(*edits), BgDisconnectCompareFaces);
    *why = "Out of memory separating background vertices.";
    rooms = calloc((size_t)document->roomcount + 1, sizeof(*rooms));
    if (!rooms) { return FALSE; }
    for (DWORD i = 0; i < count; i++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face = BgDocumentFindFace(document, &edits[i].ref, &room);
        *why = "A selected background face no longer exists.";
        if (!face || !room->vertices || room->vertexcount > 0x100000u) { goto done; }
        if (i && !BgDisconnectCompareFaces(&edits[i-1], &edits[i]))
        { *why = "The background face selection contains duplicates."; goto done; }
        for (int c = 0; c < 3; c++)
        { if (face->vertexindices[c] >= room->vertexcount) { goto done; } }
        edits[i].index = (DWORD)(face - room->faces);
        rooms[edits[i].ref.room].count++;
    }
    for (DWORD r = 1; r <= document->roomcount; r++)
    {
        BgDisconnectRoom *pending = &rooms[r];
        const BgDocumentRoom *room = &document->rooms[r];
        if (!pending->count) { continue; }
        /* The bound may overestimate: private endpoints and the last face
           using a shared vertex can retain their existing identities. */
        DWORD available = 0x100000u - room->vertexcount;
        DWORD extra = pending->count > available/3 ? available : pending->count*3;
        pending->capacity = room->vertexcount + extra;
        pending->vertexcount = room->vertexcount;
        *why = "The background geometry exceeds the allocation limit.";
        if (pending->capacity > UINT32_MAX/sizeof(*pending->vertices)
            || room->facecount > UINT32_MAX/sizeof(*pending->faces)) { goto done; }
        *why = "Out of memory separating background vertices.";
        pending->vertices = malloc((size_t)pending->capacity * sizeof(*pending->vertices));
        pending->faces = malloc((size_t)room->facecount * sizeof(*pending->faces));
        if (!pending->vertices || !pending->faces) { goto done; }
        memcpy(pending->vertices, room->vertices, (size_t)room->vertexcount * sizeof(*pending->vertices));
        memcpy(pending->faces, room->faces, (size_t)room->facecount * sizeof(*pending->faces));
    }
    for (DWORD i = 0; i < count; i++)
    {
        BgDisconnectRoom *room = &rooms[edits[i].ref.room];
        BgDocumentFace *face = &room->faces[edits[i].index];
        const BgDocumentFace *original = &document->rooms[edits[i].ref.room].faces[edits[i].index];
        for (int c = 0; c < 3; c++)
        {
            DWORD index = original->vertexindices[c], uses = 0;
            BOOL repeated = FALSE;
            if (!(edits[i].mask & (1u << c))) { continue; }
            for (int previous = 0; previous < c; previous++)
            { if ((edits[i].mask & (1u << previous)) && original->vertexindices[previous] == index) { repeated = TRUE; } }
            if (repeated) { continue; }
            for (int corner = 0; corner < 3; corner++)
            { uses += original->vertexindices[corner] == index; }
            BgDocumentVertex *source = &room->vertices[index];
            *why = "A background vertex has an invalid reference count.";
            if (source->usecount < uses) { goto done; }
            if (source->usecount == uses) { continue; }
            *why = "Separating these vertices exceeds the native background vertex limit.";
            if (room->vertexcount == room->capacity || !nextvertex || nextvertex == UINT32_MAX) { goto done; }
            DWORD copy = room->vertexcount++;
            room->vertices[copy] = *source;
            room->vertices[copy].id = nextvertex++;
            room->vertices[copy].usecount = uses;
            source->usecount -= uses;
            for (int corner = 0; corner < 3; corner++)
            { if (original->vertexindices[corner] == index) { face->vertexindices[corner] = copy; } }
            duplicated++;
        }
    }
    if (duplicated)
    {
        for (DWORD r = 1; r <= document->roomcount; r++)
        {
            BgDisconnectRoom *pending = &rooms[r];
            BgDocumentRoom *room = &document->rooms[r];
            if (!pending->count) { continue; }
            free(room->vertices);
            room->vertices = pending->vertices; pending->vertices = NULL;
            room->vertexcount = pending->vertexcount;
            memcpy(room->faces, pending->faces, (size_t)room->facecount * sizeof(*room->faces));
        }
        document->nextvertexid = nextvertex;
        document->dirty = TRUE;
    }
    *duplicatedout = duplicated; *why = ""; ok = TRUE;
done:
    for (DWORD r = 1; r <= document->roomcount; r++) { free(rooms[r].vertices); free(rooms[r].faces); }
    free(rooms);
    return ok;
}

BOOL BgDocumentDisconnectFaces(BgDocument *document, const BgFaceRef *refs,
    DWORD count, DWORD *duplicatedout, const char **reasonout)
{
    const char *why = "No background faces are selected.";
    BgDisconnectFace *edits = NULL;
    BOOL ok = FALSE;
    if (duplicatedout) { *duplicatedout = 0; }
    if (!document || !document->rooms || document->roomcount > USHRT_MAX
        || !refs || !count || !duplicatedout) { goto done; }
    why = "Too many background faces to disconnect.";
    if (count > UINT32_MAX/sizeof(*edits)) { goto done; }
    why = "Out of memory disconnecting background faces.";
    edits = malloc((size_t)count * sizeof(*edits));
    if (!edits) { goto done; }
    for (DWORD i = 0; i < count; i++) { edits[i] = (BgDisconnectFace){.ref=refs[i], .mask=7}; }
    ok = BgDisconnectCorners(document, edits, count, duplicatedout, &why);
done:
    free(edits);
    if (reasonout) { *reasonout = why; }
    return ok;
}

BOOL BgDocumentSplitEdge(BgDocument *document, const BgDocumentEdgeRef *edge,
    DWORD *duplicatedout, const char **reasonout)
{
    const char *why = "The background edge no longer exists.";
    const BgDocumentRoom *room;
    const BgDocumentFace *face;
    BgDisconnectFace *edits = NULL;
    DWORD count = 0, a, b;
    BOOL ok = FALSE;
    if (duplicatedout) { *duplicatedout = 0; }
    if (!document || !document->rooms || document->roomcount > USHRT_MAX
        || !edge || edge->corner >= 3 || !duplicatedout) { goto done; }
    face = BgDocumentFindFace(document, &edge->face, &room);
    if (!face) { goto done; }
    a = face->vertexindices[edge->corner]; b = face->vertexindices[(edge->corner+1)%3];
    if (a == b || a >= room->vertexcount || b >= room->vertexcount) { goto done; }
    why = "Too many background faces touch this edge.";
    if (room->facecount > UINT32_MAX/sizeof(*edits)) { goto done; }
    why = "Out of memory splitting the background edge.";
    edits = malloc((size_t)room->facecount * sizeof(*edits));
    if (!edits) { goto done; }
    /* Match source endpoints across both layers, never coincident positions.
       Each incident face gets independent endpoints, including non-manifold edges. */
    for (DWORD i = 0; i < room->facecount; i++)
    {
        const BgDocumentFace *f = &room->faces[i];
        unsigned int mask = 0;
        BOOL hasa = FALSE, hasb = FALSE;
        for (int c = 0; c < 3; c++)
        {
            if (f->vertexindices[c] == a) { hasa = TRUE; mask |= 1u << c; }
            if (f->vertexindices[c] == b) { hasb = TRUE; mask |= 1u << c; }
        }
        if (hasa && hasb)
        { edits[count++] = (BgDisconnectFace){.ref={f->id,f->room,f->layer,0}, .mask=mask}; }
    }
    ok = BgDisconnectCorners(document, edits, count, duplicatedout, &why);
done:
    free(edits);
    if (reasonout) { *reasonout = why; }
    return ok;
}
