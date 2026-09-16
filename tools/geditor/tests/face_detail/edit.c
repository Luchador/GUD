#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
typedef void *HWND;
#include "texencode.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
/* Exercise every allocation failure in the edit, including its native group
   rebuild. The live document must stay intact even after earlier rooms succeeded. */
static int allocations = -1;
static BOOL Fail(void) { if (!allocations) { return TRUE; } if (allocations > 0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t n) { return Fail() ? NULL : malloc(n); }
static void *TestCalloc(size_t n, size_t s) { return Fail() ? NULL : calloc(n, s); }
static void *TestRealloc(void *p, size_t n) { return Fail() ? NULL : realloc(p, n); }
#define malloc TestMalloc
#define calloc TestCalloc
#define realloc TestRealloc
#include "bgdocument.c"
#undef malloc
#undef calloc
#undef realloc

static BgDocument g_CurrentBgDocument;
static HWND g_Browser;
BOOL BrowserCopyImageThumbnail(HWND browser, DWORD id, TexThumb *thumb, unsigned char *pixels)
{
    (void)browser; (void)pixels;
    memset(thumb,0,sizeof(*thumb)); thumb->info.valid=TRUE;
    thumb->imagewidth=thumb->imageheight=16;
    thumb->info.format=8;
    if (id==17) { thumb->imagewidth=thumb->imageheight=32; thumb->info.format=1; thumb->info.mipmaps=3; }
    else if (id==20) { thumb->imagewidth=thumb->imageheight=64; thumb->info.format=7; }
    else if (id==21) { thumb->info.format=9; }
    else if (id!=0x456) return FALSE;
    return TRUE;
}
#include "validation.inc"

static void Put(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static BgFile Fixture(void)
{
    BgFile bg={0}; bg.data=calloc(4096,1); assert(bg.data);
    strcpy(bg.name,"bg/bg_transparency_test.seg");
    Put(bg.data+4,0x0e000020);
    DWORD next=160;
    for (DWORD room=1; room<=2; room++)
    {
        Put(bg.data+next,48); next+=4;
        Put(bg.data+32+room*24,0x0e000000|next);
        bg.data[next+17]=10; bg.data[next+35]=10;
        for (int v=0; v<3; v++) { memset(bg.data+next+v*16+12,255,4); }
        next+=48;
        for (DWORD layer=0; layer<2; layer++)
        {
            /* Four faces in one native TRI4, then an inherited-state face in
               another group, followed by a state-only tail group. */
            const DWORD commands[]={0xe7000000,0, 0xba001402,0x00100000, 0xba001003,0x10000, 0xba000c02,0x2000,
                0xb900031d,0x0c1849d8, 0xb9000002,0, 0xb7000000,0x2005,
                0xfa190700,0x12345678, 0xbb000001,0xffffffff, 0xc0000002,17, 0xfc26a004,0x1f1093ff,
                0x04200030,0, 0xb1002222,0x10101010,
                0xfb000000,0xffffffff, 0xbf000000,0xa14,
                0xe7000000,0, 0xfa000000,0xffffffff, 0xb8000000,0};
            Put(bg.data+next,sizeof(commands)); next+=4;
            Put(bg.data+36+room*24+layer*4,0x0e000000|next);
            for (DWORD i=0; i<sizeof(commands)/4; i++) { Put(bg.data+next+i*4,commands[i]); }
            next+=sizeof(commands);
        }
    }
    bg.size=next; return bg;
}
static DWORD Refs(const BgDocument *doc, BgFaceRef *refs)
{
    DWORD n=0;
    for (DWORD r=1; r<=doc->roomcount; r++)
    for (DWORD f=0; f<doc->rooms[r].facecount; f++)
    { const BgDocumentFace *p=&doc->rooms[r].faces[f]; refs[n++]=(BgFaceRef){.room=r,.faceid=p->id,.layer=p->layer}; }
    return n;
}

static void Equivalent(const BgDocument *a, const BgDocument *b)
{
    assert(a->facecount == b->facecount && a->roomcount == b->roomcount);
    BgFaceRef *ra = malloc(a->facecount * sizeof(*ra)), *rb = malloc(b->facecount * sizeof(*rb));
    BgRenderState *sa = malloc(a->facecount * sizeof(*sa)), *sb = malloc(b->facecount * sizeof(*sb));
    assert(ra && rb && sa && sb && Refs(a, ra) == Refs(b, rb));
    assert(BgDocumentGetFaceRenderStates(a, ra, a->facecount, sa));
    assert(BgDocumentGetFaceRenderStates(b, rb, b->facecount, sb));
    for (DWORD i = 0; i < a->facecount; i++)
    {
        const BgDocumentFace *x = BgDocumentFindFace(a, &ra[i], NULL), *y = BgDocumentFindFace(b, &rb[i], NULL);
        if (!BgMaterialEqual(&x->material, &y->material)) {
            fprintf(stderr,"material %lu: %08x %08x / %08x %08x vs %08x %08x / %08x %08x\n",
                (unsigned long)i,x->material.textureword0,x->material.textureword1,x->material.combineword0,x->material.combineword1,
                y->material.textureword0,y->material.textureword1,y->material.combineword0,y->material.combineword1);
        }
        assert(BgMaterialEqual(&x->material, &y->material));
        assert(x->textureid == y->textureid && x->cullbackfaces == y->cullbackfaces && x->layer == y->layer);
        assert(sa[i].othermode == sb[i].othermode && sa[i].othermodehigh == sb[i].othermodehigh);
        assert(sa[i].geometrymode == sb[i].geometrymode && sa[i].surfacepolicy == sb[i].surfacepolicy);
        for (DWORD c = 0; c < 3; c++)
        {
            const BgDocumentVertex *va = &a->rooms[x->room].vertices[x->vertexindices[c]];
            const BgDocumentVertex *vb = &b->rooms[y->room].vertices[y->vertexindices[c]];
            assert(va->x==vb->x && va->y==vb->y && va->z==vb->z && va->flag==vb->flag);
            assert(va->s==vb->s && va->t==vb->t && va->r==vb->r && va->g==vb->g && va->b==vb->b && va->a==vb->a);
        }
    }
    free(ra); free(rb); free(sa); free(sb);
}
static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled={0}, saved={0}; BgDocument loaded={0}; const char *why="";
    assert(BgDocumentCompile(doc, source, &compiled, &why));
    assert(BgFileValidateVertexBatches(&compiled, &why));
    char path[MAX_PATH]; snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir, &compiled, &why));
    assert(BgLoadProjectFile(dir, compiled.name, &saved, &why));
    assert(saved.size == compiled.size && !memcmp(saved.data, compiled.data, saved.size));
    assert(BgDocumentLoad(saved.data, saved.size, doc->levelscale, &loaded, &why));
    Equivalent(doc, &loaded);
    BgDocumentFree(&loaded); BgFileFree(&compiled); BgFileFree(&saved);
}
static void CheckSelection(const BgDocument *doc, const BgDocument *original, const BgFaceRef *refs)
{
    BgRenderState before[20], after[20];
    assert(BgDocumentGetFaceRenderStates(original, refs, 20, before));
    assert(BgDocumentGetFaceRenderStates(doc, refs, 20, after));
    for (DWORD i = 0; i < 20; i++)
    {
        BOOL selected = i == 1 || i == 3 || i == 6 || i == 11;
        const BgDocumentFace *face = BgDocumentFindFace(doc, &refs[i], NULL);
        const BgDocumentFace *old = BgDocumentFindFace(original, &refs[i], NULL);
        BgDetailTexture detail; BgMaterialGetDetail(&face->material, &detail);
        assert(detail.mode == (selected ? BG_DETAIL_BASE_IMAGE : BG_DETAIL_NONE));
        if (selected) {
            assert(detail.textureid == 17 && detail.shiftu == 15 && detail.shiftv == 15);
            assert((after[i].othermodehigh & 0x73000u) == 0x52000u);
            assert(face->material.combineword0 == 0xfc26e404 && face->material.combineword1 == 0x1f10ffff);
        } else {
            assert(BgMaterialEqual(&face->material, &old->material));
            assert(after[i].othermodehigh == before[i].othermodehigh);
        }
        assert(after[i].othermode == before[i].othermode);
        assert(after[i].geometrymode == before[i].geometrymode);
    }
    /* Simulate the extra FA command emitted by the native texture loader.
       The selected span must restore the original color for following faces. */
    for (DWORD r=1;r<=doc->roomcount;r++) for (DWORD l=0;l<2;l++) {
        const BgDocumentLayerData *layer=&doc->rooms[r].layers[l];
        DWORD color=0xffffffff;
        for (DWORD g=0;g<layer->groupcount;g++) {
            const BgDocumentDrawGroup *group=&layer->groups[g];
            for (DWORD k=0;k<group->commandsize;k+=8)
                if (group->commands[k]==0xfa) color=BgDocumentRead32(group->commands+k+4);
            for (DWORD f=0;f<doc->rooms[r].facecount;f++) {
                const BgDocumentFace *face=&doc->rooms[r].faces[f];
                if (face->layer!=l || face->drawgroup!=g) continue;
                BgDetailTexture detail; BgMaterialGetDetail(&face->material,&detail);
                if (detail.mode!=BG_DETAIL_NONE) color=0xffffffff;
                else assert(color==0x12345678);
            }
        }
    }

}
static void Native(const char *path, const char *dir)
{
    BgFile file={0}; BgDocument doc={0}; const char *why=""; BOOL changed;
    FILE *f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); file.size=ftell(f); rewind(f);
    file.data=malloc(file.size); assert(file.data && fread(file.data,1,file.size,f)==file.size); fclose(f);
    strcpy(file.name,"bg/bg_detail_native.seg");
    assert(BgDocumentLoad(file.data,file.size,1,&doc,&why));
    BgFaceRef *all=malloc(doc.facecount*sizeof(*all)), *selected=malloc(doc.facecount*sizeof(*selected));
    assert(all && selected); Refs(&doc,all); DWORD n=0;
    for (DWORD i=0;i<doc.facecount;i++) {
        BgDetailTexture detail; BgMaterialGetDetail(&BgDocumentFindFace(&doc,&all[i],NULL)->material,&detail);
        if (detail.mode==BG_DETAIL_BASE_IMAGE || detail.mode==BG_DETAIL_SEPARATE_IMAGE) selected[n++]=all[i];
    }
    assert(n);
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_DETAIL_MODE,.detail={.mode=BG_DETAIL_NONE}};
    assert(BgDocumentSetFaceProperties(&doc,selected,n,&edit,&changed,&why) && changed);
    RoundTrip(&doc,&file,dir);
    for (DWORD i=0;i<n;i++) {
        BgDetailTexture detail; BgMaterialGetDetail(&BgDocumentFindFace(&doc,&selected[i],NULL)->material,&detail);
        assert(detail.mode==BG_DETAIL_NONE);
    }
    printf("PASS: disabled detail on %lu native faces in %s\n",(unsigned long)n,path);
    free(all); free(selected); BgDocumentFree(&doc); BgFileFree(&file);
}
int main(int argc, char **argv)
{
    assert(argc >= 2);
    BgFile source=Fixture(); BgDocument doc={0}, original={0}; const char *why=""; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,1,&original,&why));
    BgFaceRef refs[20]; assert(Refs(&original,refs)==20);
    BgFaceRef selected[]={refs[1],refs[3],refs[6],refs[11],refs[1]};
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_DETAIL_MODE,.detail={.mode=BG_DETAIL_BASE_IMAGE}};
    g_CurrentBgDocument=original;
    assert(GEditorValidateDetailImages(selected,5,&edit,&why));
    BgFacePropertiesEdit imageedit={.fields=BG_FACE_PROPERTY_DETAIL_IMAGE,.detail={.textureid=0x456}};
    assert(GEditorValidateDetailImages(selected,5,&imageedit,&why));
    imageedit.detail.textureid=20; assert(!GEditorValidateDetailImages(selected,5,&imageedit,&why));
    imageedit.detail.textureid=21; assert(!GEditorValidateDetailImages(selected,5,&imageedit,&why));
    imageedit.detail.textureid=22; assert(!GEditorValidateDetailImages(selected,5,&imageedit,&why));
    int failures;
    for (failures=0;failures<400;failures++) {
        BgDocumentFree(&doc); assert(BgDocumentClone(&original,&doc,&why));
        allocations=failures;
        BOOL ok=BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why);
        allocations=-1;
        if (ok) break;
        assert(!changed && !doc.dirty && why[0]); Equivalent(&doc,&original);
    }
    assert(failures>20 && failures<400); CheckSelection(&doc,&original,refs);
    assert(BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && !changed);
    RoundTrip(&doc,&source,argv[1]);
    /* Independent edits retain different images/scales on all other fields. */
    edit=(BgFacePropertiesEdit){.fields=BG_FACE_PROPERTY_DETAIL_IMAGE,.detail={.textureid=0x456}};
    assert(BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && changed);
    edit=(BgFacePropertiesEdit){.fields=BG_FACE_PROPERTY_DETAIL_U|BG_FACE_PROPERTY_DETAIL_MINLOD,
        .detail={.shiftu=3,.minlod=193}};
    assert(BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && changed);
    for (DWORD i=0;i<5;i++) {
        BgDetailTexture d; BgMaterialGetDetail(&BgDocumentFindFace(&doc,&selected[i],NULL)->material,&d);
        assert(d.mode==BG_DETAIL_SEPARATE_IMAGE && d.textureid==0x456 && d.shiftu==3 && d.shiftv==15 && d.minlod==193);
    }
    RoundTrip(&doc,&source,argv[1]);
    /* A bad second face rejects the entire combined edit, before cull/wrap writes. */
    selected[4]=refs[2]; edit.fields|=BG_FACE_PROPERTY_CULL; edit.cullbackfaces=FALSE;
    assert(!BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && !changed);
    assert(BgDocumentFindFace(&doc,&selected[0],NULL)->cullbackfaces);
    selected[4]=refs[1];
    edit=(BgFacePropertiesEdit){.fields=BG_FACE_PROPERTY_DETAIL_MODE,.detail={.mode=BG_DETAIL_NONE}};
    assert(BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && changed);
    /* Disabling preserves unused marker fields, but restores ordinary sampling. */
    for (DWORD i=0;i<5;i++) {
        const BgDocumentFace *face=BgDocumentFindFace(&doc,&selected[i],NULL);
        BgDetailTexture detail; BgMaterialGetDetail(&face->material,&detail);
        assert(detail.mode==BG_DETAIL_NONE);
        assert(face->material.combineword0==0xfc26a004 && face->material.combineword1==0x1f1093ff);
    }
    RoundTrip(&doc,&source,argv[1]);
    /* Actual history snapshots retain native commands as well as the material. */
    BgDocumentFree(&doc); assert(BgDocumentClone(&original,&doc,&why));
    EditHistory history={0}; EditHistoryTransaction transaction={0}; EditHistoryAsset asset;
    SetupFile setup={0}; StanFile stan={0}; EditHistoryReset(&history,&doc,&setup,&stan);
    edit.detail.mode=BG_DETAIL_BASE_IMAGE;
    assert(EditHistoryBeginBgEdit(&history,&doc,"Change BG Detail Texture",&transaction,&why));
    assert(BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && changed);
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&transaction,&why));
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why)); Equivalent(&doc,&original);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why)); CheckSelection(&doc,&original,refs);
    EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    for (int i=2;i<argc;i++) Native(argv[i],argv[1]);
    printf("PASS: detail edits, multi-room/layer isolation, no-op, round trip, history, and %d allocation failures.\n",failures);
    return 0;
}
