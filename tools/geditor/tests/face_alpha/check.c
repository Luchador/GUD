#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"

BOOL SetupFileCompact(SetupFile *setup, const char **why) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
#include "fixture.inc"

static DWORD Read(const unsigned char *p)
{ return ((DWORD)p[0]<<24) | ((DWORD)p[1]<<16) | ((DWORD)p[2]<<8) | p[3]; }

static void CheckScopes(const BgFile *bg)
{
    DWORD table = Read(bg->data + 4) & 0xffffff;
    DWORD scopes = 0, loads = 0;
    for (DWORD room = 1; room <= 2; room++)
    for (DWORD layer = 0; layer < 2; layer++)
    {
        DWORD start = Read(bg->data + table + room*24 + 4 + layer*4) & 0xffffff;
        DWORD size = Read(bg->data + start - 4), source = BG_ALPHA_AUTO;
        DWORD vertexsource = 255, pending = 0;
        for (DWORD pc = start; pc < start + size; pc += 8)
        {
            DWORD w0 = Read(bg->data + pc), w1 = Read(bg->data + pc + 4), op = w0 >> 24;
            if (BG_ALPHA_IS_MARKER(w0, w1))
            {
                DWORD kind = BG_ALPHA_TAG_KIND(w1);
                if (kind <= BG_ALPHA_VERTEX)
                { assert(!pending && source != kind); source = kind; pending = BG_ALPHA_SYNC; scopes++; }
                else { assert(kind == pending); pending = kind == BG_ALPHA_LAST_SLOT ? 0 : kind + 1; }
            }
            else { assert(!pending); }
            if (op == 4) { vertexsource = source; loads++; }
            if (op == 0xbf || op == 0xb1) { assert(vertexsource == source); }
            if (op == 0xb8) { assert(source == BG_ALPHA_AUTO); }
        }
        assert(!pending && source == BG_ALPHA_AUTO);
    }
    assert(scopes > 0 && loads >= scopes / 2);
}

static void CheckFaces(const BgDocument *doc, const BgDocument *original)
{
    BgDocumentRenderMesh mesh = {0};
    const char *why;
    assert(BgDocumentBuildRenderMesh(doc, &mesh, &why));
    for (DWORD f = 0; f < mesh.facecount; f++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face = BgDocumentFindFace(doc, &mesh.facerefs[f], &room);
        BOOL vertex = face->material.alphasource == BG_ALPHA_VERTEX;
        const BgDocumentFace *before = BgDocumentFindFace(original, &mesh.facerefs[f], NULL);
        assert(before && before->material.combineword0 == face->material.combineword0
            && before->material.combineword1 == face->material.combineword1);
        assert(!!(mesh.renderflags[f] & BG_RENDER_NO_FOG) == vertex);
        for (DWORD c = 0; c < 3; c++)
        {
            const BgDocumentVertex *v = &room->vertices[face->vertexindices[c]];
            assert(mesh.vertices[f*3+c].a == (vertex ? v->a : 255));
            assert(BgDocumentPreviewVertexAlpha(room, face, v->a) == (vertex ? v->a : 255));
        }
    }
    BgDocumentRenderMeshFree(&mesh);
}

static void Synthetic(const char *dir)
{
    BgFile source = Fixture(), compiled = {0};
    BgDocument doc = {0}, before = {0};
    BgFaceRef refs[20], chosen[4];
    BgFacePropertiesEdit edit = {.fields = BG_FACE_PROPERTY_ALPHA_SOURCE, .alphasource = BG_ALPHA_VERTEX};
    EditHistory history = {0}; EditHistoryTransaction tx = {0}; EditHistoryAsset asset;
    SetupFile setup = {0}; StanFile stan = {0};
    const char *why = ""; BOOL changed;
    assert(BgDocumentLoad(source.data, source.size, 1, &doc, &why));
    assert(Refs(&doc, refs) == 20);
    /* Untextured light beams share all vertices with adjacent Auto faces. */
    for (DWORD r = 1; r <= doc.roomcount; r++)
    {
        for (DWORD f = 0; f < doc.rooms[r].facecount; f++)
        {
            BgDocumentFace *face = &doc.rooms[r].faces[f];
            BgMaterialSetTexture(&face->material, BG_TEX_NONE); face->textureid = BG_TEX_NONE;
        }
        for (DWORD v = 0; v < doc.rooms[r].vertexcount; v++)
        { doc.rooms[r].vertices[v].a = (unsigned char)(v * 127); }
    }
    assert(BgDocumentClone(&doc, &before, &why));
    EditHistoryReset(&history, &doc, NULL, NULL);
    chosen[0] = refs[1]; chosen[1] = refs[3]; chosen[2] = refs[9]; chosen[3] = refs[15];
    assert(EditHistoryBeginBgEdit(&history, &doc, "Alpha Source", &tx, &why));
    assert(BgDocumentSetFaceProperties(&doc, chosen, 4, &edit, &changed, &why) && changed);
    assert(EditHistoryCommitEdit(&history, &doc, NULL, NULL, &tx, &why));
    CheckFaces(&doc, &before);
    assert(BgDocumentSetFaceProperties(&doc, chosen, 4, &edit, &changed, &why) && !changed);
    assert(EditHistoryUndo(&history, &doc, &setup, &stan, &asset, &why));
    Equivalent(&doc, &before);
    assert(EditHistoryRedo(&history, &doc, &setup, &stan, &asset, &why));
    CheckFaces(&doc, &before);
    RoundTrip(&doc, &source, dir);
    assert(BgDocumentCompile(&doc, &source, &compiled, &why));
    CheckScopes(&compiled);
    /* Metadata and reserved runtime slots must not accumulate with saves. */
    for (int iteration = 0; iteration < 50; iteration++)
    {
        BgFile next = {0}; BgDocument loaded = {0};
        assert(BgDocumentLoad(compiled.data, compiled.size, 1, &loaded, &why));
        Equivalent(&doc, &loaded);
        assert(BgDocumentCompile(&loaded, &compiled, &next, &why));
        assert(next.size <= compiled.size);
        CheckScopes(&next);
        BgDocumentFree(&loaded); BgFileFree(&compiled); compiled = next;
    }
    {
        BgDocument clip = {0}; BgFaceRef *pasted; DWORD count;
        const double offset[3] = {0, 10, 0};
        assert(BgDocumentCopyFaces(&doc, chosen, 4, &clip, &why));
        assert(BgDocumentPasteFaces(&doc, &clip, offset, &pasted, &count, &why) && count == 4);
        for (DWORD i = 0; i < count; i++)
        { assert(BgDocumentFindFace(&doc, &pasted[i], NULL)->material.alphasource == BG_ALPHA_VERTEX); }
        RoundTrip(&doc, &source, dir);
        free(pasted); BgDocumentFree(&clip);
    }
    edit.alphasource = 2;
    assert(!BgDocumentSetFaceProperties(&doc, chosen, 4, &edit, &changed, &why) && !changed);
    edit.alphasource = BG_ALPHA_AUTO;
    assert(BgDocumentSetFaceProperties(&doc, chosen, 4, &edit, &changed, &why) && changed);
    for (DWORD i = 0; i < 4; i++)
    { assert(BgMaterialEqual(&BgDocumentFindFace(&doc, &chosen[i], NULL)->material,
                             &BgDocumentFindFace(&before, &chosen[i], NULL)->material)); }
    RoundTrip(&doc, &source, dir);
    EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&before);
    BgFileFree(&compiled); BgFileFree(&source);
    puts("PASS: vertex alpha, shared-vertex reloads, authored combiner restoration, copy/paste, undo/redo and 50 stable saves.");
}

static void Native(const char *path, const char *dir)
{
    BgFile source = {0}; BgDocument doc = {0};
    const char *why; BOOL changed; DWORD count = 0;
    FILE *fp = fopen(path, "rb"); assert(fp);
    fseek(fp, 0, SEEK_END); source.size = (DWORD)ftell(fp); rewind(fp);
    source.data = malloc(source.size); assert(source.data && fread(source.data, 1, source.size, fp) == source.size);
    fclose(fp); strcpy(source.name, "bg/alpha_depot.seg");
    assert(BgDocumentLoad(source.data, source.size, 1, &doc, &why));
    BgFaceRef *refs = malloc(doc.facecount * sizeof(*refs));
    BgRenderState *states = malloc(doc.facecount * sizeof(*states));
    assert(refs && states && Refs(&doc, refs) == doc.facecount);
    assert(BgDocumentGetFaceRenderStates(&doc, refs, doc.facecount, states));
    for (DWORD i = 0; i < doc.facecount; i++)
    { if (i % 2 && BgRenderSupportsVertexAlpha(&states[i])) { refs[count++] = refs[i]; } }
    BgFacePropertiesEdit edit = {.fields = BG_FACE_PROPERTY_ALPHA_SOURCE, .alphasource = BG_ALPHA_VERTEX};
    assert(count && BgDocumentSetFaceProperties(&doc, refs, count, &edit, &changed, &why) && changed);
    RoundTrip(&doc, &source, dir);
    printf("PASS: %lu alternating native Depot faces retain vertex alpha through save/reload.\n", (unsigned long)count);
    free(refs); free(states); BgDocumentFree(&doc); BgFileFree(&source);
}

int main(int argc, char **argv)
{ assert(argc == 3); Synthetic(argv[1]); Native(argv[2], argv[1]); return 0; }
