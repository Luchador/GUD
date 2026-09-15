#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static void Put(unsigned char *p, DWORD v)
{ p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static BgFile Read(const char *path)
{
    BgFile bg = {0};
    FILE *f = fopen(path, "rb");
    assert(f); fseek(f, 0, SEEK_END); bg.size = ftell(f); rewind(f);
    bg.data = malloc(bg.size); assert(bg.data);
    assert(fread(bg.data, 1, bg.size, f) == bg.size); fclose(f);
    return bg;
}

static void Equivalent(const BgDocument *a, const BgDocument *b)
{
    assert(a->roomcount == b->roomcount && a->facecount == b->facecount);
    assert(a->portals.portalcount == b->portals.portalcount);
    if (a->portals.portalcount)
    { assert(!memcmp(a->portals.portals, b->portals.portals, a->portals.portalcount * sizeof(BgPortal))); }
    for (DWORD r = 1; r <= a->roomcount; r++)
    {
        const BgDocumentRoom *x = &a->rooms[r], *y = &b->rooms[r];
        assert(x->facecount == y->facecount && !memcmp(x->origin, y->origin, sizeof(x->origin)));
        for (DWORD f = 0; f < x->facecount; f++)
        {
            const BgDocumentFace *p = &x->faces[f], *q = &y->faces[f];
            assert(p->id == q->id && p->layer == q->layer && p->cullbackfaces == q->cullbackfaces);
            assert(BgMaterialEqual(&p->material, &q->material));
            for (DWORD c = 0; c < 3; c++)
            {
                BgDocumentVertex v = x->vertices[p->vertexindices[c]], w = y->vertices[q->vertexindices[c]];
                v.id = w.id = v.usecount = w.usecount = 0;
                assert(!memcmp(&v, &w, sizeof(v)));
            }
        }
        /* Empty room bounds are a native engine input, not editable geometry. */
        if (!x->facecount)
        {
            assert(x->vertexcount == y->vertexcount);
            for (DWORD v = 0; v < x->vertexcount; v++)
            { assert(x->vertices[v].x == y->vertices[v].x && x->vertices[v].y == y->vertices[v].y && x->vertices[v].z == y->vertices[v].z); }
        }
    }
}

static float CenterX(const BgDocumentRoom *room)
{
    short lo = 32767, hi = -32768;
    for (DWORD v = 0; v < room->vertexcount; v++)
    {
        if (room->vertices[v].x < lo) { lo = room->vertices[v].x; }
        if (room->vertices[v].x > hi) { hi = room->vertices[v].x; }
    }
    return room->origin[0] + (lo + hi) * 0.5f;
}

static void Check(const BgFile *source, BOOL depot)
{
    const char *why = "";
    BgFile clean = {0}, again = {0}, compiled = {0};
    BgDocument before = {0}, after = {0};
    unsigned char *original = malloc(source->size);
    assert(original); memcpy(original, source->data, source->size);
    assert(BgDocumentLoad(source->data, source->size, 1, &before, &why));
    assert(BgFileRemoveUnusedVertices(source, &clean, &why));
    assert(!memcmp(original, source->data, source->size));
    if (clean.data)
    {
        assert(clean.size == source->size && BgFileValidateVertexBatches(&clean, &why));
        assert(BgDocumentLoad(clean.data, clean.size, 1, &after, &why));
        Equivalent(&before, &after);
        assert(BgFileRemoveUnusedVertices(&clean, &again, &why) && !again.data);
    }
    if (depot)
    {
        assert(clean.data && CenterX(&before.rooms[42]) == -27.5f);
        assert(CenterX(&after.rooms[42]) == 30.5f);
        assert(CenterX(&after.rooms[41]) < -15 && CenterX(&after.rooms[42]) > -15);
        assert(before.rooms[42].vertexcount == 262 && after.rooms[42].vertexcount <= 256);
    }
    /* The normal compile path must clean copies too, without changing the
       live vertex identities held by selection and undo snapshots. */
    assert(BgDocumentCompile(&before, source, &compiled, &why));
    assert(BgFileRemoveUnusedVertices(&compiled, &again, &why) && !again.data);
    assert(!memcmp(original, source->data, source->size));
    BgDocumentFree(&after);
    assert(BgDocumentLoad(compiled.data, compiled.size, 1, &after, &why));
    Equivalent(&before, &after);
    if (depot) { assert(CenterX(&after.rooms[42]) == 30.5f); }
    BgDocumentFree(&before); BgDocumentFree(&after);
    BgFileFree(&clean); BgFileFree(&compiled); free(original);
}

static void Batches(void)
{
    unsigned char data[512] = {0};
    BgFile bg = {.data = data, .size = sizeof(data)}, out = {0};
    const char *why = "";
    Put(data + 4, 0x0e000020); Put(data + 56, 0x0e000084); Put(data + 60, 0x0e000104);
    Put(data + 64, 0x0e000144); Put(data + 128, 6 * 16);
    for (DWORD v = 0; v < 6; v++)
    { data[132 + v * 16 + 1] = v * 10; data[132 + v * 16 + 3] = (v & 1) * 10; memset(data + 132 + v * 16 + 12, 255, 4); }
    Put(data + 256, 32); Put(data + 260, 0x04500060); Put(data + 264, 0x0e000000);
    Put(data + 268, 0xbf000000); Put(data + 272, 0x000A1E32); /* 1,3,5 */
    Put(data + 276, 0xb1000333); Put(data + 280, 0x00333333); /* three real point triangles */
    Put(data + 284, 0xb8000000);
    Put(data + 320, 24); Put(data + 324, 0x04500060); Put(data + 328, 0x0e000000);
    Put(data + 332, 0xbf000000); Put(data + 336, 0x001E2832); /* secondary-only vertex 4 */
    Put(data + 340, 0xb8000000);
    Check(&bg, FALSE);
    /* No triangles: preserve bounds-only vertices and the room slot. */
    Put(data + 268, 0xb8000000); Put(data + 332, 0xb8000000);
    assert(BgFileRemoveUnusedVertices(&bg, &out, &why) && !out.data);
    /* Reject bad references before any mutation. */
    Put(data + 268, 0xbf000000); Put(data + 272, 0x00960A14);
    assert(!BgFileRemoveUnusedVertices(&bg, &out, &why) && !out.data);
}

int main(int argc, char **argv)
{
    Batches();
    for (int i = 1; i < argc; i++)
    {
        BgFile bg = Read(argv[i]);
        Check(&bg, i == 1);
        BgFileFree(&bg);
    }
    printf("PASS: mixed-layer batches, point triangles, immutable inputs, empty bounds, Depot and %d native backgrounds.\n", argc - 2);
    return 0;
}
