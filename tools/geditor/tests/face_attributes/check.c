#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations = -1;
static BOOL Fail(void) { if (!allocations) { return TRUE; } if (allocations > 0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t n) { return Fail() ? NULL : malloc(n); }
static void *TestCalloc(size_t n, size_t s) { return Fail() ? NULL : calloc(n,s); }
static void *TestRealloc(void *p, size_t n) { return Fail() ? NULL : realloc(p,n); }
#define malloc TestMalloc
#define calloc TestCalloc
#define realloc TestRealloc
#include "bgdocument.c"
#include "bgroom.c"
#include "bguv.c"
#include "bgcorrect.c"
#undef malloc
#undef calloc
#undef realloc
#include "fixture.inc"
#include "common.inc"
#include "controller.inc"

static void LayerAppearance(const BgDocument *before,const BgDocument *after)
{
    assert(before->facecount==after->facecount);
    for(DWORD r=1;r<=before->roomcount;r++) {
        const BgDocumentRoom *a=&before->rooms[r],*b=&after->rooms[r];
        assert(a->vertexcount==b->vertexcount && a->facecount==b->facecount);
        if(a->vertexcount)assert(!memcmp(a->vertices,b->vertices,a->vertexcount*sizeof(*a->vertices)));
        for(DWORD f=0;f<a->facecount;f++) {
            const BgDocumentFace *x=&a->faces[f],*y=NULL;
            for(DWORD j=0;j<b->facecount;j++) { if(b->faces[j].id==x->id) {y=&b->faces[j];break;} }
            assert(y);BgDocumentFace restored=*y;restored.layer=x->layer;restored.drawgroup=x->drawgroup;
            BgDocumentFace expected=*x;
            if(x->layer!=y->layer && x->textureid==BG_TEX_NONE && (x->material.modeword0&0xffu))
            { BgMaterialSetTexture(&expected.material,BG_TEX_NONE); }
            assert(!memcmp(&expected,&restored,sizeof(expected)));
            BgFaceRef ra={x->id,x->room,x->layer,0},rb={y->id,y->room,y->layer,0};BgRenderState sa,sb;
            assert(BgDocumentGetFaceRenderStates(before,&ra,1,&sa) && BgDocumentGetFaceRenderStates(after,&rb,1,&sb));
            assert(!((sa.othermode^sb.othermode)&(x->layer==y->layer?0xffffffffu:sa.othermodeknown)));
            assert(!((sa.othermodehigh^sb.othermodehigh)&(x->layer==y->layer?0xffffffffu:sa.othermodehighknown)));
            assert(!((sa.geometrymode^sb.geometrymode)&~0x2000u&(x->layer==y->layer?0xffffffffu:sa.geometryknown)));
            assert(sa.surfacepolicy==sb.surfacepolicy && sa.surfacebasemode==sb.surfacebasemode);
            assert(sa.environmentalpha==sb.environmentalpha && sa.primitiveword1==sb.primitiveword1);
        }
    }
}
static void Layers(const char *dir)
{
    BgFile source=Fixture();BgDocument original={0},doc={0};BgFaceRef refs[20];const char *why="";BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,.25f,&original,&why));Refs(&original,refs);
    BgFaceRef selected[]={refs[1],refs[6],refs[11],refs[1]};
    for(int layer=0;layer<2;layer++) {
        assert(BgDocumentClone(&original,&doc,&why));
        assert(BgDocumentSetFaceLayer(&doc,selected,4,(BgGeometryLayer)layer,&changed,&why) && changed);
        LayerAppearance(&original,&doc);RoundTrip(&doc,&source,dir);
        BgFaceRef updated[4];memcpy(updated,selected,sizeof(updated));for(int i=0;i<4;i++)updated[i].layer=layer;
        assert(BgDocumentSetFaceLayer(&doc,updated,4,(BgGeometryLayer)layer,&changed,&why) && !changed);
        BgDocumentFree(&doc);
    }
    BOOL done=FALSE;
    for(int budget=0;budget<180 && !done;budget++) {
        assert(BgDocumentClone(&original,&doc,&why));allocations=budget;
        BOOL ok=BgDocumentSetFaceLayer(&doc,selected,4,BG_GEOMETRY_SECONDARY,&changed,&why);allocations=-1;
        if(ok) {done=TRUE;LayerAppearance(&original,&doc);} else {assert(!changed && why[0]);Same(&original,&doc);}
        BgDocumentFree(&doc);
    }
    assert(done);assert(BgDocumentClone(&original,&doc,&why));
    assert(!BgDocumentSetFaceLayer(&doc,selected,4,(BgGeometryLayer)2,&changed,&why));Same(&original,&doc);
    selected[0].faceid=0;assert(!BgDocumentSetFaceLayer(&doc,selected,4,BG_GEOMETRY_SECONDARY,&changed,&why));Same(&original,&doc);
    BgDocumentFree(&doc);BgDocumentFree(&original);BgFileFree(&source);
}
static void CheckUVOnly(const BgDocument *before,const BgDocument *after)
{
    for(DWORD r=1;r<=before->roomcount;r++) {
        const BgDocumentRoom *a=&before->rooms[r],*b=&after->rooms[r];
        assert(a->vertexcount==b->vertexcount && a->facecount==b->facecount);
        if(a->facecount)assert(!memcmp(a->faces,b->faces,a->facecount*sizeof(*a->faces)));
        for(DWORD i=0;i<a->vertexcount;i++) {
            BgDocumentVertex v=b->vertices[i];v.s=a->vertices[i].s;v.t=a->vertices[i].t;
            assert(!memcmp(&v,&a->vertices[i],sizeof(v)));
        }
    }
}
static void UVs(const char *dir)
{
    BgFile source=Fixture();BgDocument original={0},doc={0},positioned={0};const char *why="";DWORD moved;double applied[3];
    assert(BgDocumentLoad(source.data,source.size,.25f,&original,&why));
    for(DWORD r=1;r<=original.roomcount;r++)for(DWORD i=0;i<original.rooms[r].vertexcount;i++) {
        BgDocumentVertex *v=&original.rooms[r].vertices[i];v->s=v->x*32;v->t=v->y*32;
    }
    BgDocumentVertexRef refs[]={{1,2},{1,1},{1,0},{2,2}};
    const double offsets[][3]={{0,40,0},{40,0,0},{20,-16,32},{40,40,0},{.1,.1,0}};
    const int counts[]={1,2,1,3,1};
    for(int test=0;test<5;test++) {
        assert(BgDocumentClone(&original,&doc,&why));
        assert(BgDocumentTranslateVertices(&doc,refs,counts[test],offsets[test],applied,&moved,&why));
        assert(BgDocumentClone(&doc,&positioned,&why));
        assert(BgDocumentCorrectMovedUVs(&original,&doc,&why));CheckUVOnly(&positioned,&doc);
        const BgDocumentVertex *v=&doc.rooms[1].vertices[2];
        if(test==0)assert(v->s==0 && v->t==640);
        if(test==1)assert(v->s==320 && v->t==320 && doc.rooms[1].vertices[1].s==640);
        if(test==2)assert(v->s==160 && v->t==320); /* 6/8/10 fold retains density. */
        if(test>=3)assert(v->s==0 && v->t==320); /* Whole face and snapped-zero moves. */
        RoundTrip(&doc,&source,dir);BgDocumentFree(&positioned);BgDocumentFree(&doc);
    }
    assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentTranslateVertices(&doc,refs,1,offsets[0],applied,&moved,&why));
    assert(BgDocumentClone(&doc,&positioned,&why));BOOL done=FALSE;
    for(int budget=0;budget<30 && !done;budget++) {
        allocations=budget;BOOL ok=BgDocumentCorrectMovedUVs(&original,&doc,&why);allocations=-1;
        if(ok)done=TRUE;else {assert(why[0]);Same(&positioned,&doc);}
    }
    assert(done);BgDocumentFree(&doc);BgDocumentFree(&positioned);
    original.rooms[1].vertices[2].t=30000;
    assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentTranslateVertices(&doc,refs,1,offsets[0],applied,&moved,&why));
    assert(BgDocumentClone(&doc,&positioned,&why));
    assert(!BgDocumentCorrectMovedUVs(&original,&doc,&why));Same(&positioned,&doc);
    BgDocumentFree(&doc);BgDocumentFree(&positioned);
    /* Independent seam vertices and a different room retain their own mapping. */
    original.rooms[1].vertices[2].t=320;original.rooms[2].vertices[2].t=640;
    BgDocumentVertexRef both[]={{1,2},{2,2}};
    assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentTranslateVertices(&doc,both,2,offsets[0],applied,&moved,&why));
    assert(BgDocumentCorrectMovedUVs(&original,&doc,&why));
    assert(doc.rooms[1].vertices[2].t==640 && doc.rooms[2].vertices[2].t==1280);
    BgDocumentFree(&doc);
    /* Coincident but independently authored vertices keep separate UV seams. */
    BgDocumentRoom *room=&original.rooms[1];
    room->vertices=realloc(room->vertices,4*sizeof(*room->vertices));assert(room->vertices);
    room->vertices[3]=room->vertices[2];room->vertices[3].id=original.nextvertexid++;
    room->vertices[3].t=640;room->vertices[3].usecount=1;room->vertices[2].usecount--;
    room->vertexcount=4;
    for(int c=0;c<3;c++)if(room->faces[0].vertexindices[c]==2)room->faces[0].vertexindices[c]=3;
    BgDocumentVertexRef seam[]={{1,2},{1,3}};
    assert(BgDocumentClone(&original,&doc,&why));
    EditHistory history={0};EditHistoryTransaction transaction={0};SetupFile setup={0};StanFile stan={0};
    EditHistoryReset(&history,&doc,&setup,&stan);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Move BG Vertices",&transaction,&why));
    assert(BgDocumentTranslateVertices(&doc,seam,2,offsets[0],applied,&moved,&why));
    assert(doc.rooms[1].vertices[2].t==320 && doc.rooms[1].vertices[3].t==640); /* Toggle off. */
    assert(BgDocumentCorrectMovedUVs(&transaction.beforebg,&doc,&why));
    assert(doc.rooms[1].vertices[2].t==640 && doc.rooms[1].vertices[3].t==1280);
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&transaction,&why));
    assert(BgDocumentClone(&doc,&positioned,&why));
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,NULL,&why));Same(&doc,&original);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,NULL,&why));Same(&doc,&positioned);
    RoundTrip(&doc,&source,dir);
    EditHistoryFree(&history);BgDocumentFree(&positioned);
    BgDocumentFree(&doc);BgDocumentFree(&original);BgFileFree(&source);
}
static void Depot(const char *path,const char *dir)
{
    BgFile source={0};BgDocument original={0},doc={0};const char *why="";BOOL changed;
    FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);source.size=ftell(f);rewind(f);source.data=malloc(source.size);
    assert(fread(source.data,1,source.size,f)==source.size);fclose(f);strcpy(source.name,"bg/depot_layer_test.seg");
    assert(BgDocumentLoad(source.data,source.size,.233333333f,&original,&why));
    unsigned success=0,rejected=0;
    for(int target=0;target<2;target++)for(DWORD r=1;r<=original.roomcount;r++) {
        const BgDocumentRoom *room=&original.rooms[r];if(!room->facecount)continue;
        BgFaceRef *refs=malloc(room->facecount*sizeof(*refs));DWORD count=0;
        for(DWORD i=0;i<room->facecount;i++) {const BgDocumentFace *face=&room->faces[i];if(face->layer!=target)refs[count++]=(BgFaceRef){face->id,r,face->layer,0};}
        if(count) {
            assert(BgDocumentClone(&original,&doc,&why));
            if(BgDocumentSetFaceLayer(&doc,refs,count,(BgGeometryLayer)target,&changed,&why)) {
                success++;LayerAppearance(&original,&doc);
                if(r==37 || r==42 || r==22)RoundTrip(&doc,&source,dir);
            } else {rejected++;Same(&original,&doc);
                fprintf(stderr,"Depot room %u to %d: %s\n",r,target,why);}
            BgDocumentFree(&doc);
        }
        free(refs);
    }
    printf("Depot layer transfers: %u succeeded, %u rejected.\n",success,rejected);assert(success>0 && !rejected);
    BgDocumentFree(&original);BgFileFree(&source);
}
int main(int argc,char **argv)
{ assert(argc==3);UVs(argv[1]);Layers(argv[1]);Commands();Depot(argv[2],argv[1]);puts("PASS: UV correction, native round trips, layer materials, atomic failures and editor layer history.");return 0; }
