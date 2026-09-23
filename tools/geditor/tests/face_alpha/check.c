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
    assert(Refs(&doc, refs) == doc.facecount);
    count = 0;
    for (DWORD i = 0; i < doc.facecount && count < 32; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(&doc, &refs[i], NULL);
        if (BgRenderSupportsAlphaPreset(&states[i], &face->material, BG_ALPHA_TEXTURE_CONSTANT))
        { refs[count++] = refs[i]; }
    }
    assert(count);
    edit.alphasource = BG_ALPHA_TEXTURE_VERTEX;
    assert(BgDocumentSetFaceProperties(&doc, refs, count, &edit, &changed, &why) && changed);
    RoundTrip(&doc, &source, dir);
    edit.fields |= BG_FACE_PROPERTY_OPACITY;
    edit.alphasource = BG_ALPHA_TEXTURE_CONSTANT; edit.opacity = 96;
    assert(BgDocumentSetFaceProperties(&doc, refs, count, &edit, &changed, &why) && changed);
    RoundTrip(&doc, &source, dir);
    puts("PASS: native Depot texture x vertex alpha and texture x opacity survive save/reload.");
    free(refs); free(states); BgDocumentFree(&doc); BgFileFree(&source);
}

static void Presets(const char *dir)
{
    const DWORD presets[] = {BG_ALPHA_OPAQUE, BG_ALPHA_TEXTURE, BG_ALPHA_VERTEX,
        BG_ALPHA_TEXTURE_VERTEX, BG_ALPHA_CONSTANT, BG_ALPHA_TEXTURE_CONSTANT, BG_ALPHA_AUTO};
    BgFile source = Fixture();
    BgDocument doc = {0}, before = {0};
    BgFaceRef refs[20], selected[4];
    BgRenderState original[20], states[20];
    EditHistory history = {0}; EditHistoryTransaction tx = {0}; EditHistoryAsset asset;
    SetupFile setup = {0}; StanFile stan = {0};
    BOOL changed; const char *why = "";
    DWORD table = Read(source.data + 4) & 0xffffff;
    /* Explicit two-cycle/mip settings and non-black authored RGB constants. */
    for (DWORD r = 1; r <= 2; r++) for (DWORD l = 0; l < 2; l++)
    {
        DWORD start = Read(source.data + table + r * 24 + 4 + l * 4) & 0xffffff;
        DWORD size = Read(source.data + start - 4);
        for (DWORD pc = start; pc < start + size; pc += 8)
        {
            DWORD w0 = Read(source.data + pc);
            if (w0 == 0xba001402) { Put(source.data + pc, 0xba000017); Put(source.data + pc + 4, 0x00112000); }
            if (w0 == 0xfb000000) { Put(source.data + pc + 4, 0x12345699); }
        }
    }
    assert(BgDocumentLoad(source.data, source.size, 1, &doc, &why));
    assert(Refs(&doc, refs) == 20);
    selected[0] = refs[1]; selected[1] = refs[3]; selected[2] = refs[9]; selected[3] = refs[15];
    for (DWORD r = 1; r <= 2; r++) for (DWORD v = 0; v < doc.rooms[r].vertexcount; v++)
    { doc.rooms[r].vertices[v].a = (unsigned char)(v * 127); }
    assert(BgDocumentClone(&doc, &before, &why));
    assert(BgDocumentGetFaceRenderStates(&doc, refs, 20, original));
    EditHistoryReset(&history, &doc, NULL, NULL);
    BgFacePropertiesEdit edit = {.fields = BG_FACE_PROPERTY_ALPHA_SOURCE | BG_FACE_PROPERTY_OPACITY,
        .alphasource = BG_ALPHA_TEXTURE_CONSTANT, .opacity = 64};
    assert(EditHistoryBeginBgEdit(&history, &doc, "Alpha and opacity", &tx, &why));
    assert(BgDocumentSetFaceProperties(&doc, selected, 4, &edit, &changed, &why) && changed);
    assert(EditHistoryCommitEdit(&history, &doc, NULL, NULL, &tx, &why));
    assert(EditHistoryUndo(&history, &doc, &setup, &stan, &asset, &why));
    Equivalent(&before, &doc);
    assert(EditHistoryRedo(&history, &doc, &setup, &stan, &asset, &why));
    for (unsigned int p = 0; p < sizeof(presets) / sizeof(*presets); p++)
    {
        edit.alphasource = presets[p];
        assert(BgDocumentSetFaceProperties(&doc, selected, 4, &edit, &changed, &why));
        assert(BgDocumentGetFaceRenderStates(&doc, refs, 20, states));
        BgDocumentRenderMesh mesh = {0};
        assert(BgDocumentBuildRenderMesh(&doc, &mesh, &why));
        for (DWORD f = 0; f < 20; f++)
        {
            BOOL chosen = f == 1 || f == 3 || f == 9 || f == 15;
            DWORD policy = chosen ? presets[p] : BG_ALPHA_AUTO;
            const BgDocumentRoom *room;
            const BgDocumentFace *face = BgDocumentFindFace(&doc, &refs[f], &room);
            const BgDocumentFace *old = BgDocumentFindFace(&before, &refs[f], NULL);
            assert(face->material.alphasource == policy);
            assert(face->material.combineword0 == old->material.combineword0
                && face->material.combineword1 == old->material.combineword1);
            assert(states[f].environmentword1 == (chosen ? (original[f].environmentword1 & 0xffffff00u) | 64 : original[f].environmentword1));
            assert(states[f].othermode == original[f].othermode);
            assert(!!(mesh.renderflags[f] & BG_RENDER_NO_FOG) == BG_ALPHA_USES_VERTEX(policy));
            assert(!!(mesh.renderflags[f] & BG_RENDER_IGNORE_TEXTURE_ALPHA)
                == (policy != BG_ALPHA_AUTO && !BG_ALPHA_USES_TEXTURE(policy)));
            for (DWORD c = 0; c < 3; c++)
            {
                unsigned char vertex = room->vertices[face->vertexindices[c]].a;
                unsigned char expected = BG_ALPHA_USES_VERTEX(policy) ? vertex
                    : policy == BG_ALPHA_AUTO ? states[f].environmentalpha
                    : BG_ALPHA_USES_CONSTANT(policy) ? 64 : 255;
                assert(mesh.vertices[f * 3 + c].a == expected);
                assert(BgDocumentPreviewVertexAlpha(room, face, vertex) == expected);
            }
        }
        BgDocumentRenderMeshFree(&mesh);
        RoundTrip(&doc, &source, dir);
    }
    /* Changing opacity alone preserves mixed constant presets and RGB. */
    edit.fields = BG_FACE_PROPERTY_ALPHA_SOURCE; edit.alphasource = BG_ALPHA_CONSTANT;
    assert(BgDocumentSetFaceProperties(&doc, selected, 2, &edit, &changed, &why));
    edit.alphasource = BG_ALPHA_TEXTURE_CONSTANT;
    assert(BgDocumentSetFaceProperties(&doc, selected + 2, 2, &edit, &changed, &why));
    edit.fields = BG_FACE_PROPERTY_OPACITY;
    for (DWORD a = 0; a <= 255; a += 255)
    {
        edit.opacity = a;
        assert(BgDocumentSetFaceProperties(&doc, selected, 4, &edit, &changed, &why));
        assert(BgDocumentGetFaceRenderStates(&doc, selected, 4, states));
        for (DWORD i = 0; i < 4; i++)
        {
            assert(states[i].environmentalpha == a);
            assert(BgDocumentFindFace(&doc, &selected[i], NULL)->material.alphasource
                == (i < 2 ? BG_ALPHA_CONSTANT : BG_ALPHA_TEXTURE_CONSTANT));
        }
    }
    DWORD commandsize = 0;
    for (int iteration = 0; iteration < 100; iteration++)
    {
        DWORD size = 0;
        edit.opacity = iteration % 2 ? 101 : 202;
        assert(BgDocumentSetFaceProperties(&doc, selected, 4, &edit, &changed, &why));
        for (DWORD r = 1; r <= doc.roomcount; r++) for (DWORD l = 0; l < 2; l++)
        for (DWORD g = 0; g < doc.rooms[r].layers[l].groupcount; g++)
        { size += doc.rooms[r].layers[l].groups[g].commandsize; }
        if (iteration) { assert(size == commandsize); }
        commandsize = size;
    }
    edit.opacity = 256;
    assert(!BgDocumentSetFaceProperties(&doc, selected, 4, &edit, &changed, &why) && !changed);
    edit.opacity = 77;
    assert(BgDocumentSetFaceProperties(&doc, selected, 4, &edit, &changed, &why) && changed);
    assert(BgDocumentSetFaceProperties(&doc, selected, 4, &edit, &changed, &why) && !changed);
    {
        BgFile compiled = {0};
        assert(BgDocumentCompile(&doc, &source, &compiled, &why));
        BgFile packed = {0};
        assert(BgFileCompact(&compiled, &packed, &why));
        BgFileFree(&compiled); compiled = packed;
        for (int save = 0; save < 20; save++)
        {
            BgFile next = {0}; BgDocument loaded = {0};
            assert(BgDocumentLoad(compiled.data, compiled.size, 1, &loaded, &why));
            Equivalent(&doc, &loaded);
            assert(BgDocumentCompile(&loaded, &compiled, &next, &why));
            BgFile packed = {0};
            assert(BgFileCompact(&next, &packed, &why));
            BgFileFree(&next); next = packed;
            assert(next.size <= compiled.size);
            BgDocumentFree(&loaded); BgFileFree(&compiled); compiled = next;
        }
        BgFileFree(&compiled);
    }
    {
        BgDocument clip = {0}; BgFaceRef *pasted; DWORD count;
        const double offset[3] = {0, 10, 0};
        assert(BgDocumentCopyFaces(&doc, selected, 4, &clip, &why));
        assert(BgDocumentPasteFaces(&doc, &clip, offset, &pasted, &count, &why) && count == 4);
        assert(BgDocumentGetFaceRenderStates(&doc, pasted, count, states));
        for (DWORD i = 0; i < count; i++) { assert(states[i].environmentalpha == 77); }
        RoundTrip(&doc, &source, dir);
        free(pasted); BgDocumentFree(&clip);
    }
    /* Removing an image removes only the texture factor. Reject a texture
       preset on the resulting untextured face before making any changes. */
    const DWORD textured[] = {BG_ALPHA_TEXTURE, BG_ALPHA_TEXTURE_VERTEX, BG_ALPHA_TEXTURE_CONSTANT};
    const DWORD untextured[] = {BG_ALPHA_OPAQUE, BG_ALPHA_VERTEX, BG_ALPHA_CONSTANT};
    for (DWORD i = 0; i < 3; i++)
    {
        edit.fields = BG_FACE_PROPERTY_ALPHA_SOURCE; edit.alphasource = textured[i];
        assert(BgDocumentSetFaceProperties(&doc, selected, 1, &edit, &changed, &why));
        assert(BgDocumentSetFaceTexture(&doc, selected, 1, BG_TEX_NONE, &changed, &why) && changed);
        assert(BgDocumentFindFace(&doc, selected, NULL)->material.alphasource == untextured[i]);
        assert(!BgDocumentSetFaceProperties(&doc, selected, 1, &edit, &changed, &why) && !changed);
        RoundTrip(&doc, &source, dir);
        assert(BgDocumentSetFaceTexture(&doc, selected, 1, 17, &changed, &why) && changed);
    }
    EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&before); BgFileFree(&source);
    puts("PASS: every alpha preset, matching preview/paint alpha, isolated opacity/RGB, mixed presets, undo/redo, copy/paste and stable saves.");
}

int main(int argc, char **argv)
{ assert(argc == 3); Synthetic(argv[1]); Presets(argv[1]); Native(argv[2], argv[1]); return 0; }
