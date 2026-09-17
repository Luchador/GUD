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

static BOOL ShadeOnly(const BgMaterial *m)
{
    return m->combineword0 == 0xFCFFFFFFu
        && (m->combineword1 == 0xFFFE793Cu || m->combineword1 == 0xFFFE7838u);
}

static void Materials(void)
{
    for (DWORD type = 0; type <= 4; type++)
    for (DWORD pass = 0; pass < 2; pass++)
    for (DWORD same = 0; same < 2; same++)
    {
        BgMaterial m, expected;
        BgMaterialInit(&m);
        m.textureword0 = 0xC068F800u | type; /* Wrap and detail UV settings. */
        m.textureword1 = 0x80456000u | (same ? 0x558u : 0x123u);
        m.modeword0 = 0xBB001801u;
        m.modeword1 = 0x80004000u;
        m.combineword0 = 0xFCFFFFFFu;
        m.combineword1 = pass ? 0xFFFE7838u : 0xFFFE793Cu;
        m.alphasource = pass ? BG_ALPHA_VERTEX : BG_ALPHA_AUTO;
        expected = m;
        expected.textureword1 = 0x80456558u;
        expected.combineword0 = type < 2 ? 0xFC26E404u : type == 2 ? 0xFC26A004u : 0xFC121824u;
        expected.combineword1 = type < 2 ? 0x1F10FFFFu : type == 2 ? 0x1F1093FFu : 0xFF33FFFFu;
        BgMaterialSetTexture(&m, 0x558);
        assert(BgMaterialEqual(&m, &expected));
        BgMaterialSetTexture(&m, 0x558);
        assert(BgMaterialEqual(&m, &expected));
        /* Existing textured and custom combiners must survive image changes. */
        expected.textureword1 = 0x80456234u;
        BgMaterialSetTexture(&m, 0x234);
        assert(BgMaterialEqual(&m, &expected));
        m.combineword0 = expected.combineword0 = 0xFCFFFFFFu;
        m.combineword1 = expected.combineword1 = 0xFFFE79FCu;
        expected.textureword1 = 0x80456558u;
        BgMaterialSetTexture(&m, 0x558);
        assert(BgMaterialEqual(&m, &expected));
        BgMaterialSetTexture(&m, BG_TEX_NONE);
        assert(BgMaterialTextureId(&m) == BG_TEX_NONE && ShadeOnly(&m));
        BgMaterialSetTexture(&m, 0x558);
        assert(BgMaterialTextureId(&m) == 0x558 && !ShadeOnly(&m));
    }
    BgMaterial fresh;
    BgMaterialInit(&fresh);
    BgMaterialSetTexture(&fresh, 0x558);
    assert(fresh.textureword0 == 0xC0000002u && fresh.textureword1 == 0x558);
    assert(fresh.combineword0 == 0xFC26A004u && fresh.combineword1 == 0x1F1093FFu);
    puts("PASS: shade-only image assignments, detail/mipmap/single-tile modes, custom materials and repeated assignments.");
}

static void SameMaterials(const BgDocument *a, const BgDocument *b)
{
    Equivalent(a, b);
    assert(a->roomcount == b->roomcount);
    for (DWORD r = 1; r <= a->roomcount; r++)
    {
        assert(a->rooms[r].facecount == b->rooms[r].facecount);
        for (DWORD f = 0; f < a->rooms[r].facecount; f++)
        { assert(BgMaterialEqual(&a->rooms[r].faces[f].material, &b->rooms[r].faces[f].material)); }
    }
}

static void RoundTripMaterials(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled = {0}, saved = {0}; BgDocument loaded = {0}; const char *why = "";
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/bg", dir); CreateDirectory(path, NULL);
    assert(BgDocumentCompile(doc, source, &compiled, &why));
    assert(BgFileValidateVertexBatches(&compiled, &why));
    assert(BgSaveProjectFile(dir, &compiled, &why));
    assert(BgLoadProjectFile(dir, compiled.name, &saved, &why));
    assert(saved.size == compiled.size && !memcmp(saved.data, compiled.data, saved.size));
    assert(BgDocumentLoad(saved.data, saved.size, doc->levelscale, &loaded, &why));
    SameMaterials(doc, &loaded);
    BgDocumentFree(&loaded); BgFileFree(&compiled); BgFileFree(&saved);
}

static void CheckSelection(const BgDocument *doc, const BgDocument *before,
                           const BgFaceRef *selected, DWORD count)
{
    for (DWORD r = 1; r <= doc->roomcount; r++)
    {
        const BgDocumentRoom *room = &doc->rooms[r], *old = &before->rooms[r];
        assert(room->vertexcount == old->vertexcount && room->facecount == old->facecount);
        assert(!room->vertexcount || !memcmp(room->vertices, old->vertices,
                                            room->vertexcount * sizeof(*room->vertices)));
        for (DWORD f = 0; f < room->facecount; f++)
        {
            const BgDocumentFace *face = &room->faces[f];
            BOOL chosen = FALSE;
            for (DWORD s = 0; s < count; s++)
            { if (selected[s].room == r && selected[s].faceid == face->id) { chosen = TRUE; } }
            assert(!memcmp(face->vertexindices, old->faces[f].vertexindices, sizeof(face->vertexindices)));
            if (chosen)
            {
                assert(face->textureid == 0x558 && !ShadeOnly(&face->material));
                BOOL detail = (face->material.textureword0 & 7) < 2;
                assert(face->material.combineword0 == (detail ? 0xFC26E404u : 0xFC26A004u)
                    && face->material.combineword1 == (detail ? 0x1F10FFFFu : 0x1F1093FFu));
            }
            else { assert(BgMaterialEqual(&face->material, &old->faces[f].material)); }
        }
    }
}

static void Synthetic(const char *dir)
{
    BgFile source = Fixture(); BgDocument doc = {0}, before = {0};
    const char *why = ""; BOOL changed; BgFaceRef refs[20], selected[4];
    EditHistory history = {0}; EditHistoryTransaction tx = {0}; EditHistoryAsset asset;
    SetupFile setup = {0}; StanFile stan = {0};
    assert(BgDocumentLoad(source.data, source.size, 1, &doc, &why));
    assert(Refs(&doc, refs) == 20);
    /* Inherited image with texturing ON, across both layers, groups and rooms. */
    for (DWORD r = 1; r <= doc.roomcount; r++)
    for (DWORD f = 0; f < doc.rooms[r].facecount; f++)
    {
        BgDocumentFace *face = &doc.rooms[r].faces[f];
        face->material.combineword0 = 0xFCFFFFFFu;
        face->material.combineword1 = f % 2 ? 0xFFFE7838u : 0xFFFE793Cu;
        if (f % 2) { face->textureid = face->material.textureword1 = 0x558; }
    }
    assert(BgDocumentClone(&doc, &before, &why));
    RoundTripMaterials(&doc, &source, dir); /* No edit: leave native shading intact. */
    selected[0] = refs[1]; selected[1] = refs[3]; selected[2] = refs[9]; selected[3] = refs[15];
    EditHistoryReset(&history, &doc, NULL, NULL);
    assert(EditHistoryBeginBgEdit(&history, &doc, "Texture", &tx, &why));
    assert(BgDocumentSetFaceTexture(&doc, selected, 4, 0x558, &changed, &why) && changed && doc.dirty);
    assert(EditHistoryCommitEdit(&history, &doc, NULL, NULL, &tx, &why));
    CheckSelection(&doc, &before, selected, 4);
    assert(BgDocumentSetFaceTexture(&doc, selected, 4, 0x558, &changed, &why) && !changed);
    RoundTripMaterials(&doc, &source, dir);
    assert(EditHistoryUndo(&history, &doc, &setup, &stan, &asset, &why));
    SameMaterials(&doc, &before);
    assert(EditHistoryRedo(&history, &doc, &setup, &stan, &asset, &why));
    CheckSelection(&doc, &before, selected, 4);
    EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&before); BgFileFree(&source);
    puts("PASS: selective same-image repair, untouched neighbors, UVs/colors, project/compiled BG reload and undo/redo.");
}

static void Native(const char *path, const char *dir, BOOL reported)
{
    BgFile source = {0}; BgDocument doc = {0}, before = {0}; const char *why = "";
    FILE *fp = fopen(path, "rb"); assert(fp);
    fseek(fp, 0, SEEK_END); source.size = (DWORD)ftell(fp); rewind(fp);
    source.data = malloc(source.size); assert(source.data);
    assert(fread(source.data, 1, source.size, fp) == source.size); fclose(fp);
    strcpy(source.name, "bg/texture_depot.seg");
    assert(BgDocumentLoad(source.data, source.size, 1, &doc, &why));
    assert(BgDocumentClone(&doc, &before, &why));
    BgFaceRef *refs = malloc(doc.facecount * sizeof(*refs)); assert(refs);
    DWORD count = 0; BOOL changed;
    for (DWORD r = 1; r <= doc.roomcount; r++)
    for (DWORD f = 0; f < doc.rooms[r].facecount; f++)
    {
        const BgDocumentFace *face = &doc.rooms[r].faces[f];
        if (reported && face->id != 4312 && face->id != 4313) { continue; }
        if (!reported && (!ShadeOnly(&face->material) || face->textureid == BG_TEX_NONE
                          || (face->material.textureword0 & 7) > 2)) { continue; }
        if (reported) { assert(r == 37 && ShadeOnly(&face->material) && face->textureid == 0x558); }
        refs[count++] = (BgFaceRef){.room = r, .faceid = face->id, .layer = face->layer};
    }
    assert(count >= 2 && (!reported || count == 2));
    assert(BgDocumentSetFaceTexture(&doc, refs, count, 0x558, &changed, &why) && changed);
    CheckSelection(&doc, &before, refs, count);
    RoundTripMaterials(&doc, &source, dir);
    printf("PASS: %lu %s Depot faces repaired and exported; other faces retain their materials.\n",
           (unsigned long)count, reported ? "reported" : "native shade-only");
    free(refs); BgDocumentFree(&doc); BgDocumentFree(&before); BgFileFree(&source);
}

int main(int argc, char **argv)
{
    assert(argc == 3 || argc == 4);
    setvbuf(stdout, NULL, _IONBF, 0);
    Materials(); Synthetic(argv[1]); Native(argv[2], argv[1], FALSE);
    if (argc == 4) { Native(argv[3], argv[1], TRUE); }
    return 0;
}
