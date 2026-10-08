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

/* Compare decoded triangles by geometry/material/state, allowing only primary
 * order changes. Group numbers and parser-assigned face IDs are not geometry. */
typedef struct FaceKey {
    unsigned char vertices[3][16];
    DWORD material[9],state[7];
    DWORD layer,cull;
} FaceKey;
static FaceKey Key(const BgDocument *doc,DWORD room,DWORD f)
{
    const BgDocumentFace *face=&doc->rooms[room].faces[f];
    BgFaceRef ref={face->id,(unsigned short)room,face->layer,0};
    BgRenderState state;FaceKey key={0};
    assert(BgDocumentGetFaceRenderStates(doc,&ref,1,&state));
    for (DWORD c=0;c<3;c++)
    {
        const BgDocumentVertex *v=&doc->rooms[room].vertices[face->vertexindices[c]];
        unsigned char *p=key.vertices[c];
        p[0]=(unsigned short)v->x>>8;p[1]=v->x;p[2]=(unsigned short)v->y>>8;p[3]=v->y;
        p[4]=(unsigned short)v->z>>8;p[5]=v->z;p[6]=v->flag>>8;p[7]=v->flag;
        p[8]=(unsigned short)v->s>>8;p[9]=v->s;p[10]=(unsigned short)v->t>>8;p[11]=v->t;
        p[12]=v->r;p[13]=v->g;p[14]=v->b;p[15]=v->a;
    }
    memcpy(key.material,&face->material,sizeof(key.material));
    key.state[0]=state.othermode;key.state[1]=state.othermodehigh;key.state[2]=state.environmentalpha;
    key.state[3]=state.primitiveword0;key.state[4]=state.primitiveword1;
    key.state[5]=state.geometrymode&~0x2000u;key.state[6]=state.surfacepolicy;
    key.layer=face->layer;key.cull=face->cullbackfaces;return key;
}
static int Compare(const void *a,const void *b) { return memcmp(a,b,sizeof(FaceKey)); }
static void SameMaterials(const BgDocument *x,const BgDocument *y)
{
    assert(x->roomcount==y->roomcount && x->facecount==y->facecount);
    for (DWORD r=1;r<=x->roomcount;r++)
    {
        DWORD count=x->rooms[r].facecount;assert(count==y->rooms[r].facecount);
        assert(!memcmp(x->rooms[r].origin,y->rooms[r].origin,sizeof(x->rooms[r].origin)));
        FaceKey *left=calloc(count?count:1,sizeof(*left)),*right=calloc(count?count:1,sizeof(*right));assert(left&&right);
        for (DWORD f=0;f<count;f++)
        {
            left[f]=Key(x,r,f);right[f]=Key(y,r,f);
            if (left[f].layer || (left[f].state[0]&0x5c00u) || left[f].state[6]>=BG_SURFACE_CUTOUT)
                assert(!memcmp(&left[f],&right[f],sizeof(*left)));
        }
        qsort(left,count,sizeof(*left),Compare);qsort(right,count,sizeof(*right),Compare);
        assert(!memcmp(left,right,count*sizeof(*left)));free(left);free(right);
    }
}

static DWORD InvalidNativeCombiners(const BgFile *bg);

static void RoundTripMaterials(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled = {0}, saved = {0}; BgDocument loaded = {0}; const char *why = "";
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/bg", dir); CreateDirectory(path, NULL);
    assert(BgDocumentCompile(doc, source, &compiled, &why));
    assert(BgFileValidateVertexBatches(&compiled, &why));
    assert(BgSaveProjectFile(dir, &compiled, &why));
    assert(BgLoadProjectFile(dir, compiled.name, &saved, &why));
    /* Saving may compact/rebatch the compiled streams. Check the emitted
     * pipeline before the loader can repair it, then compare decoded assets. */
    assert(BgFileValidateVertexBatches(&saved, &why));
    assert(!InvalidNativeCombiners(&compiled) && !InvalidNativeCombiners(&saved));
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
                BgFaceRef ref={.room=r,.layer=face->layer,.faceid=face->id}; BgRenderState state;
                assert(BgDocumentGetFaceRenderStates(doc,&ref,1,&state));
                BOOL single=!detail && (state.othermodehighknown&0x300000u)==0x300000u
                    && !(state.othermodehigh&0x300000u);
                assert(face->material.combineword0 == (detail ? 0xFC26E404u : single ? 0xFC121824u : 0xFC26A004u)
                    && face->material.combineword1 == (detail ? 0x1F10FFFFu : single ? 0xFF33FFFFu : 0x1F1093FFu));
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

#include "cycles.c"

int main(int argc, char **argv)
{
    assert(argc == 3 || argc == 4);
    setvbuf(stdout, NULL, _IONBF, 0);
    Materials(); CycleMaterials(); CycleDocuments(argv[1]);
    Synthetic(argv[1]); Native(argv[2], argv[1], FALSE);
    if (argc == 4) { Native(argv[3], argv[1], TRUE); }
    return 0;
}
