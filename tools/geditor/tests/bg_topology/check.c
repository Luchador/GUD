#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "fixture.inc"

static BOOL SameVertex(const BgDocumentVertex *a, const BgDocumentVertex *b)
{
    return a->x == b->x && a->y == b->y && a->z == b->z && a->flag == b->flag
        && a->s == b->s && a->t == b->t
        && a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
}

/* Match reordered triangles by their visible attributes, then compare the
 * complete corner-equivalence relation, not just vertex positions/counts. */
static BOOL SameTopology(const BgDocument *a, const BgDocument *b)
{
    if (a->roomcount != b->roomcount || a->facecount != b->facecount) { return FALSE; }
    for (DWORD r = 1; r <= a->roomcount; r++)
    {
        const BgDocumentRoom *x = &a->rooms[r], *y = &b->rooms[r];
        if (x->facecount != y->facecount) { return FALSE; }
        DWORD matched[128]; BOOL taken[128] = {0};
        assert(x->facecount < 128);
        for (DWORD f = 0; f < x->facecount; f++)
        {
            matched[f] = (DWORD)-1;
            for (DWORD g = 0; g < y->facecount; g++)
            {
                if (taken[g] || x->faces[f].layer != y->faces[g].layer
                    || x->faces[f].textureid != y->faces[g].textureid) { continue; }
                int c;
                for (c = 0; c < 3; c++)
                    if (!SameVertex(&x->vertices[x->faces[f].vertexindices[c]],
                            &y->vertices[y->faces[g].vertexindices[c]])) { break; }
                if (c == 3) { matched[f] = g; taken[g] = TRUE; break; }
            }
            if (matched[f] == (DWORD)-1) { return FALSE; }
        }
        for (DWORD f = 0; f < x->facecount; f++) for (DWORD g = 0; g < x->facecount; g++)
            for (int c = 0; c < 3; c++) for (int d = 0; d < 3; d++)
                if ((x->faces[f].vertexindices[c] == x->faces[g].vertexindices[d])
                    != (y->faces[matched[f]].vertexindices[c] == y->faces[matched[g]].vertexindices[d])) { return FALSE; }
    }
    return TRUE;
}

static void RoundTrip(const BgDocument *document, const BgFile *source, const char *dir)
{
    const char *why = "";
    BgDocument before = {0}, loaded = {0};
    BgFile compiled = {0}, native = {0}, packed = {0}, saved = {0}, nativepacked = {0};
    assert(BgDocumentClone(document, &before, &why));
    assert(BgDocumentCompile(document, source, &native, &why));
    assert(BgDocumentCompileProject(document, source, &compiled, &why));
    Same(document, &before);
    assert(BgFileValidateVertexBatches(&compiled, &why));
    assert(BgFileCompact(&compiled, &packed, &why) && BgFileCompact(&native, &nativepacked, &why));
    /* Project metadata must not affect a single byte of game geometry. */
    assert(packed.size == nativepacked.size && !memcmp(packed.data, nativepacked.data, packed.size));
    assert(BgSaveProjectFile(dir, &compiled, &why));
    assert(BgLoadProjectFile(dir, source->name, &saved, &why));
    assert(BgDocumentLoad(saved.data, saved.size, .1f, &loaded, &why));
    assert(SameTopology(document, &loaded)); Counts(&loaded);
    DWORD savesize = saved.size;
    for (int pass = 0; pass < 3; pass++)
    {
        BgFile again = {0};
        assert(BgDocumentCompileProject(&loaded, &saved, &again, &why));
        assert(BgSaveProjectFile(dir, &again, &why));
        BgDocumentFree(&loaded); BgFileFree(&saved); BgFileFree(&again);
        assert(BgLoadProjectFile(dir, source->name, &saved, &why));
        assert(BgDocumentLoad(saved.data, saved.size, .1f, &loaded, &why));
        assert(SameTopology(document, &loaded)); Counts(&loaded);
        assert(saved.size <= savesize); savesize = saved.size;
    }
    /* Failed replacement preserves the entire previous BG and topology. */
    test_fail_move = 1;
    assert(!BgSaveProjectFile(dir, &compiled, &why));
    BgFile previous = {0};
    assert(BgLoadProjectFile(dir, source->name, &previous, &why));
    assert(saved.size == previous.size && !memcmp(saved.data, previous.data, saved.size));
    BgFileFree(&previous);

    /* Exercise the ROM exporter sequence, including old native-only assets. */
    BgFile export = saved, cleaned = {0};
    export.data = malloc(saved.size); assert(export.data); memcpy(export.data, saved.data, saved.size);
    assert(BgFileRemoveUnusedVertices(&export, &cleaned, &why));
    if (cleaned.data) { BgFileFree(&export); export = cleaned; memset(&cleaned, 0, sizeof(cleaned)); }
    assert(BgFileOptimize(&export, &cleaned, &why));
    if (cleaned.data) { BgFileFree(&export); export = cleaned; memset(&cleaned, 0, sizeof(cleaned)); }
    assert(BgFileCompact(&export, &cleaned, &why));
    const unsigned char *topology; DWORD bytes, nativesize;
    assert(BgFileGetEditorTopology(cleaned.data, cleaned.size, &topology, &bytes, &nativesize, &why) && !topology);
    assert(BgFileValidateVertexBatches(&cleaned, &why));
    BgFileFree(&export); BgFileFree(&cleaned);

    BgDocument damaged = {0};
    assert(BgFileGetEditorTopology(saved.data, saved.size, &topology, &bytes, &nativesize, &why) && topology);
    saved.data[nativesize] ^= 1;
    assert(!BgDocumentLoad(saved.data, saved.size, .1f, &damaged, &why) && !damaged.rooms);
    saved.data[nativesize] ^= 1;
    /* A valid checksum with invalid connectivity must also fail closed. */
    unsigned char *bad = malloc(bytes); assert(bad); memcpy(bad, topology, bytes);
    Put(bad + 12, 0xffffffffu);
    BgFile invalid = {0};
    assert(BgFileCompact(&saved, &invalid, &why));
    assert(BgFileAppendEditorTopology(&invalid, bad, bytes, &why));
    assert(!BgDocumentLoad(invalid.data, invalid.size, .1f, &damaged, &why) && !damaged.rooms);
    free(bad); BgFileFree(&invalid);

    BgDocumentFree(&before); BgDocumentFree(&loaded);
    BgFileFree(&compiled); BgFileFree(&native); BgFileFree(&packed); BgFileFree(&nativepacked); BgFileFree(&saved);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/bg", argv[1]); assert(CreateDirectory(path, NULL));
    const char *why = "";
    BgFile source = Fixture(); BgDocument original = {0}, edited = {0}, nativeview = {0};
    assert(BgDocumentLoad(source.data, source.size, .1f, &original, &why)); MakeStrip(&original);
    assert(BgDocumentClone(&original, &edited, &why));
    BgDocumentVertexRef refs[] = {{1, 1}, {1, 4}}, survivor;
    DWORD removed, deleted;
    assert(BgDocumentMergeVertices(&edited, refs, 2, &survivor, &removed, &deleted, &why));
    BgFile legacy = {0};
    assert(BgDocumentCompile(&edited, &source, &legacy, &why));
    assert(BgDocumentLoad(legacy.data, legacy.size, .1f, &nativeview, &why));
    assert(!SameTopology(&edited, &nativeview)); /* Reproduces the old save/reload bug. */
    BgFileFree(&legacy); BgDocumentFree(&nativeview);
    RoundTrip(&edited, &source, argv[1]); BgDocumentFree(&edited);

    assert(BgDocumentClone(&original, &edited, &why));
    edited.rooms[1].vertices[4].x = edited.rooms[1].vertices[1].x;
    DWORD count = 2;
    assert(BgDocumentWeldVertices(&edited, refs, &count, &removed, &why));
    assert(removed && count == 1);
    RoundTrip(&edited, &source, argv[1]); BgDocumentFree(&edited);

    /* Undoing back to the original graph and saving must also be lossless. */
    RoundTrip(&original, &source, argv[1]);
    assert(BgDocumentClone(&original, &edited, &why));
    BgDocumentFace *face = &edited.rooms[1].faces[0];
    BgFaceRef selection = {.faceid = face->id, .room = 1, .layer = face->layer};
    DWORD separated;
    assert(BgDocumentDisconnectFaces(&edited, &selection, 1, &separated, &why) && separated);
    RoundTrip(&edited, &source, argv[1]);
    /* Interleaved materials exercise the compiler's trial/sorted order. */
    for (DWORD f = 0; f < edited.rooms[1].facecount; f++)
    {
        edited.rooms[1].faces[f].layer = BG_GEOMETRY_PRIMARY;
        edited.rooms[1].faces[f].textureid = (f % 2) ? 18 : 17;
        BgMaterialSetTexture(&edited.rooms[1].faces[f].material, edited.rooms[1].faces[f].textureid);
    }
    RoundTrip(&edited, &source, argv[1]);
    BgDocumentFree(&edited);

    /* Many shared vertices cross the 16-entry cache limit and material
     * sorting boundaries. Repeated local keys in another room stay local. */
    assert(BgDocumentClone(&original, &edited, &why));
    BgDocumentRoom *room = &edited.rooms[1];
    BgDocumentVertex prototype = room->vertices[0];
    BgDocumentFace triangle = room->faces[0];
    free(room->vertices); free(room->faces);
    room->vertexcount = 42; room->facecount = room->facecapacity = 40;
    room->vertices = calloc(room->vertexcount, sizeof(*room->vertices));
    room->faces = calloc(room->facecount, sizeof(*room->faces));
    assert(room->vertices && room->faces);
    for (DWORD v = 0; v < room->vertexcount; v++)
    {
        room->vertices[v] = prototype; room->vertices[v].id = edited.nextvertexid++;
        room->vertices[v].x = (v / 2) * 100; room->vertices[v].y = (v % 2) * 100;
        room->vertices[v].usecount = 0;
    }
    for (DWORD f = 0; f < room->facecount; f++)
    {
        DWORD start = (f / 2) * 2;
        room->faces[f] = triangle; room->faces[f].id = edited.nextfaceid++;
        DWORD indices[3] = {start + (f % 2 ? 2 : 0), start + (f % 2 ? 3 : 2), start + 1};
        memcpy(room->faces[f].vertexindices, indices, sizeof(indices));
        room->faces[f].textureid = f % 2 ? 18 : 17;
        BgMaterialSetTexture(&room->faces[f].material, room->faces[f].textureid);
        for (int c = 0; c < 3; c++) { room->vertices[indices[c]].usecount++; }
    }
    BgDocumentRoom *other = &edited.rooms[2];
    other->vertexcount = 3; other->facecount = other->facecapacity = 1;
    other->vertices = malloc(3 * sizeof(*other->vertices)); other->faces = malloc(sizeof(*other->faces));
    assert(other->vertices && other->faces);
    other->faces[0] = triangle; other->faces[0].room = 2; other->faces[0].id = edited.nextfaceid++;
    for (int c = 0; c < 3; c++)
    {
        other->vertices[c] = room->vertices[c]; other->vertices[c].id = edited.nextvertexid++;
        other->vertices[c].room = 2; other->vertices[c].usecount = 1; other->faces[0].vertexindices[c] = c;
    }
    edited.facecount = 41;
    RoundTrip(&edited, &source, argv[1]);
    BgDocumentFree(&edited); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: legacy split reproduced; Merge, Weld, Disconnect, reordered batches and repeated project saves preserve topology; native export is unchanged and failures preserve the previous save.");
    return 0;
}
