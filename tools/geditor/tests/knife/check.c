#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include "bgknife.h"
#include "bghistory.h"

BOOL SetupFileCompact(SetupFile *setup, const char **why) { (void)setup; (void)why; abort(); }
void SetupFileFree(SetupFile *file) { (void)file; abort(); }
void StanFileFree(StanFile *file) { (void)file; abort(); }
#include "fixture.inc"
#include "input.inc"

/* Fail each allocation in the real edit/clone path in turn. */
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t); void *__real_realloc(void *,size_t);
static int failafter = -1;
static int Fail(void) { if (failafter < 0) return 0; if (!failafter) return 1; failafter--; return 0; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { return Fail() ? NULL : __real_calloc(n,s); }
void *__wrap_realloc(void *p,size_t n) { return Fail() ? NULL : __real_realloc(p,n); }

static BgDocument Square(const BgFile *source, BOOL seam)
{
    BgDocument doc={0}; const char *why="";
    assert(BgDocumentLoad(source->data,source->size,.5f,&doc,&why));
    BgDocumentRoom *r=&doc.rooms[1]; BgDocumentFace proto=r->faces[0];
    free(r->vertices); free(r->faces);
    r->vertexcount=seam ? 6 : 4;
    r->vertices=calloc(r->vertexcount,sizeof(*r->vertices)); assert(r->vertices);
    r->faces=calloc(2,sizeof(*r->faces)); assert(r->faces); r->facecount=r->facecapacity=2;
    r->origin[0]=100; r->origin[1]=200; r->origin[2]=300;
    const short p[6][2]={{0,0},{20,0},{20,20},{0,20},{0,0},{20,20}};
    for(DWORD i=0;i<r->vertexcount;i++) {
        BgDocumentVertex *v=&r->vertices[i];
        v->id=i+1; v->room=1; v->x=p[i][0]; v->y=p[i][1];
        v->s=v->x*32; v->t=v->y*32;
        v->r=v->x*10; v->g=v->y*10; v->b=50; v->a=20+v->x*10;
        if(i>=4) { v->s+=1000; v->b=200; }
    }
    r->faces[0]=r->faces[1]=proto;
    r->faces[0].id=1; r->faces[1].id=2;
    r->faces[0].vertexindices[0]=0; r->faces[0].vertexindices[1]=1; r->faces[0].vertexindices[2]=2;
    r->faces[1].vertexindices[0]=seam ? 4 : 0; r->faces[1].vertexindices[1]=seam ? 5 : 2; r->faces[1].vertexindices[2]=3;
    r->faces[0].cullbackfaces=r->faces[1].cullbackfaces=1;
    for(int f=0;f<2;f++) for(int c=0;c<3;c++) r->vertices[r->faces[f].vertexindices[c]].usecount++;
    doc.facecount=2; doc.nextfaceid=3; doc.nextvertexid=r->vertexcount+1;
    return doc;
}

static void Same(const BgDocument *a, const BgDocument *b)
{
    assert(a->facecount==b->facecount && a->nextfaceid==b->nextfaceid && a->nextvertexid==b->nextvertexid && a->dirty==b->dirty);
    for(DWORD i=1;i<=a->roomcount;i++) {
        const BgDocumentRoom *x=a->rooms+i, *y=b->rooms+i;
        assert(x->facecount==y->facecount && x->vertexcount==y->vertexcount);
        if(x->facecount) assert(!memcmp(x->faces,y->faces,x->facecount*sizeof(*x->faces)));
        if(x->vertexcount) assert(!memcmp(x->vertices,y->vertices,x->vertexcount*sizeof(*x->vertices)));
    }
    assert(!memcmp(a->portals.portals,b->portals.portals,a->portals.portalcount*sizeof(BgPortal)));
}

static void CheckGeometry(const BgDocument *doc, const BgDocumentFace *proto)
{
    const BgDocumentRoom *r=&doc->rooms[1]; double area=0;
    DWORD *uses=calloc(r->vertexcount,sizeof(*uses)); assert(uses);
    for(DWORD i=0;i<r->facecount;i++) {
        const BgDocumentFace *f=r->faces+i;
        assert(f->room==1 && f->layer==proto->layer && f->drawgroup==proto->drawgroup);
        assert(f->textureid==proto->textureid && f->cullbackfaces==proto->cullbackfaces);
        assert(!memcmp(&f->material,&proto->material,sizeof(f->material)));
        const BgDocumentVertex *a=r->vertices+f->vertexindices[0], *b=r->vertices+f->vertexindices[1], *c=r->vertices+f->vertexindices[2];
        double cross=(b->x-a->x)*(c->y-a->y)-(b->y-a->y)*(c->x-a->x);
        assert(cross>0); area+=cross*.5;
        for(int c=0;c<3;c++) { assert(f->vertexindices[c]<r->vertexcount); uses[f->vertexindices[c]]++; }
    }
    assert(area==400);
    for(DWORD i=0;i<r->vertexcount;i++) assert(uses[i]==r->vertices[i].usecount);
    free(uses);
}

static void Persist(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile out={0}, saved={0}; BgDocument loaded={0}; const char *why=""; char path[MAX_PATH];
    assert(BgDocumentCompile(doc,source,&out,&why)); assert(BgFileValidateVertexBatches(&out,&why));
    snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,&out,&why)); assert(BgLoadProjectFile(dir,out.name,&saved,&why));
    assert(saved.size==out.size && !memcmp(saved.data,out.data,out.size));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    assert(loaded.facecount==doc->facecount && loaded.roomcount==doc->roomcount);
    assert(!memcmp(loaded.portals.portals,doc->portals.portals,doc->portals.portalcount*sizeof(BgPortal)));
    /* IDs/vertex indices may be compacted; compare rendered corner data. */
    for(DWORD r=1;r<=doc->roomcount;r++) {
        assert(loaded.rooms[r].facecount==doc->rooms[r].facecount);
        for(DWORD f=0;f<doc->rooms[r].facecount;f++) {
            const BgDocumentFace *a=&doc->rooms[r].faces[f], *b=&loaded.rooms[r].faces[f];
            assert(a->layer==b->layer && a->cullbackfaces==b->cullbackfaces && a->textureid==b->textureid);
            assert(!memcmp(&a->material,&b->material,sizeof(a->material)));
            for(int c=0;c<3;c++) {
                const BgDocumentVertex *v=&doc->rooms[r].vertices[a->vertexindices[c]], *w=&loaded.rooms[r].vertices[b->vertexindices[c]];
                assert(v->x==w->x && v->y==w->y && v->z==w->z && v->s==w->s && v->t==w->t);
                assert(v->r==w->r && v->g==w->g && v->b==w->b && v->a==w->a);
            }
        }
    }
    BgDocumentFree(&loaded); BgFileFree(&out); BgFileFree(&saved);
}

int main(int argc,char **argv)
{
    assert(argc==2); const char *why=""; double value;
    assert(KnifeParseNumber(" -1.25e2 ",&value) && value==-125);
    const char *bad[]={""," ","NaN","Inf","1e9999","3x","2 3"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) assert(!KnifeParseNumber(bad[i],&value));
    BgKnifePlane plane={{220,400,600},{1,0,0}}, normal;
    assert(BgKnifeNormalizePlane(&plane,&normal));
    normal=plane; normal.normal[0]=1e300; assert(BgKnifeNormalizePlane(&normal,&normal) && normal.normal[0]==1);
    normal=plane; normal.normal[0]=0; assert(!BgKnifeNormalizePlane(&normal,&normal));
    BgFile source=Fixture(); BgDocument doc=Square(&source,FALSE), before={0};
    BgDocumentFace proto=doc.rooms[1].faces[0];
    BgFaceRef refs[2]={{1,1,0,0},{2,1,0,0}}, *result=NULL;
    DWORD n=0,cuts=0;
    assert(BgDocumentClone(&doc,&before,&why));
    assert(BgDocumentKnifeFaces(&doc,refs,2,&plane,&result,&n,&cuts,&why));
    assert(cuts==2 && n==6 && doc.rooms[1].vertexcount==7); free(result); result=NULL;
    CheckGeometry(&doc,&proto);
    int shared=0,edge=0;
    for(DWORD i=4;i<doc.rooms[1].vertexcount;i++) {
        BgDocumentVertex *v=&doc.rooms[1].vertices[i];
        assert(v->x==10 && v->s==320 && v->t==v->y*32 && v->r==100 && v->g==v->y*10 && v->b==50 && v->a==120);
        if(v->y==10) { shared++; assert(v->usecount>=4); }
        if(v->y==0) edge++;
    }
    assert(shared==1 && edge==1); Persist(&doc,&source,argv[1]); BgDocumentFree(&doc);
    /* On an existing edge, coplanar, or outside: no mutation, even to IDs. */
    for(int mode=0;mode<3;mode++) {
        assert(BgDocumentClone(&before,&doc,&why)); normal=plane;
        if(mode==0) { normal.position[0]=200; normal.normal[1]=-1; }
        if(mode==1) { normal.normal[0]=0; normal.normal[2]=1; }
        if(mode==2) { normal.position[0]=1000; }
        assert(BgDocumentKnifeFaces(&doc,refs,2,&normal,&result,&n,&cuts,&why) && !cuts && n==2);
        Same(&doc,&before); free(result); BgDocumentFree(&doc);
    }
    /* A cut through a corner creates two children per triangle, no duplicate on-plane vertex. */
    assert(BgDocumentClone(&before,&doc,&why)); normal=plane; normal.position[0]=240; normal.normal[1]=1;
    assert(BgDocumentKnifeFaces(&doc,refs,2,&normal,&result,&n,&cuts,&why) && cuts==2 && n==4);
    assert(doc.rooms[1].vertexcount==5); CheckGeometry(&doc,&proto); free(result); BgDocumentFree(&doc);
    /* An unselected adjoining face remains byte-identical. */
    assert(BgDocumentClone(&before,&doc,&why));
    assert(BgDocumentKnifeFaces(&doc,refs,1,&plane,&result,&n,&cuts,&why) && cuts==1 && n==3);
    assert(!memcmp(&doc.rooms[1].faces[3],&before.rooms[1].faces[1],sizeof(proto)));
    CheckGeometry(&doc,&proto); free(result); BgDocumentFree(&doc);
    /* Preserve coincident vertices with different UV/color attributes. */
    doc=Square(&source,TRUE);
    assert(BgDocumentKnifeFaces(&doc,refs,2,&plane,&result,&n,&cuts,&why) && cuts==2 && doc.rooms[1].vertexcount==10);
    shared=0;
    for(DWORD i=6;i<doc.rooms[1].vertexcount;i++) {
        BgDocumentVertex *v=&doc.rooms[1].vertices[i];
        if(v->x==10 && v->y==10) { shared++; assert((v->s==320 && v->b==50) || (v->s==1320 && v->b==200)); }
    }
    assert(shared==2); CheckGeometry(&doc,&proto); Persist(&doc,&source,argv[1]); free(result); BgDocumentFree(&doc);
    /* Sub-grid cuts snap to endpoints and become no-ops; invalid references are atomic. */
    assert(BgDocumentClone(&before,&doc,&why)); normal=plane; normal.position[0]=200.01;
    assert(BgDocumentKnifeFaces(&doc,refs,2,&normal,&result,&n,&cuts,&why) && !cuts); Same(&doc,&before); free(result); result=NULL;
    BgFaceRef invalid[2]={refs[0],refs[0]};
    assert(!BgDocumentKnifeFaces(&doc,invalid,2,&plane,&result,&n,&cuts,&why)); Same(&doc,&before);
    invalid[1].faceid=999;
    assert(!BgDocumentKnifeFaces(&doc,invalid,2,&plane,&result,&n,&cuts,&why)); Same(&doc,&before);
    BgDocumentFree(&doc);
    int allocation;
    for(allocation=0;allocation<100;allocation++) {
        assert(BgDocumentClone(&before,&doc,&why)); failafter=allocation;
        BOOL ok=BgDocumentKnifeFaces(&doc,refs,2,&plane,&result,&n,&cuts,&why); failafter=-1;
        if(!ok) { assert(!result && !n && !cuts); Same(&doc,&before); }
        free(result); BgDocumentFree(&doc);
        if(ok) break;
    }
    assert(allocation>8 && allocation<100);
    /* One-unit-thick railing, like Depot: rounded-away fragments are omitted,
     * while representable geometry is kept without degenerates or orphans. */
    assert(BgDocumentClone(&before,&doc,&why));
    doc.rooms[1].faces[1].vertexindices[0]=0; doc.rooms[1].faces[1].vertexindices[1]=2; doc.rooms[1].faces[1].vertexindices[2]=3;
    const short railing[4][3]={{-47,21,179},{-47,21,178},{-76,43,178},{-76,43,179}};
    for(int i=0;i<4;i++) { doc.rooms[1].vertices[i].x=railing[i][0]; doc.rooms[1].vertices[i].y=railing[i][1]; doc.rooms[1].vertices[i].z=railing[i][2]; }
    normal=plane; normal.normal[0]=0; normal.normal[1]=1; normal.position[1]=(200+23)/.5;
    assert(BgDocumentKnifeFaces(&doc,refs,2,&normal,&result,&n,&cuts,&why) && cuts==2);
    for(DWORD i=0;i<doc.rooms[1].vertexcount;i++) assert(doc.rooms[1].vertices[i].usecount);
    for(DWORD i=0;i<doc.rooms[1].facecount;i++) {
        BgDocumentFace *f=doc.rooms[1].faces+i;
        BgDocumentVertex *a=doc.rooms[1].vertices+f->vertexindices[0], *b=doc.rooms[1].vertices+f->vertexindices[1], *c=doc.rooms[1].vertices+f->vertexindices[2];
        int ux=b->x-a->x,uy=b->y-a->y,uz=b->z-a->z,vx=c->x-a->x,vy=c->y-a->y,vz=c->z-a->z;
        assert(uy*vz-uz*vy || uz*vx-ux*vz || ux*vy-uy*vx);
    }
    Persist(&doc,&source,argv[1]); free(result); BgDocumentFree(&doc);
    /* Room/layer/material ownership and shared edges must remain independent. */
    assert(BgDocumentClone(&before,&doc,&why));
    BgDocument extra={0}; assert(BgDocumentClone(&before,&extra,&why));
    BgDocumentRoom swap=doc.rooms[2]; doc.rooms[2]=extra.rooms[1]; extra.rooms[1]=swap; BgDocumentFree(&extra);
    doc.rooms[2].origin[1]+=100;
    doc.facecount=4; doc.nextfaceid=5; doc.nextvertexid=9;
    BgDetailTexture detail={BG_DETAIL_SEPARATE_IMAGE,7,2,3,4,5};
    for(int i=0;i<4;i++) { doc.rooms[2].vertices[i].room=2; doc.rooms[2].vertices[i].id=i+5; }
    /* Move the room's draw groups to the secondary stream. */
    doc.rooms[2].layers[1]=doc.rooms[2].layers[0]; memset(&doc.rooms[2].layers[0],0,sizeof(doc.rooms[2].layers[0]));
    for(int i=0;i<2;i++) {
        BgDocumentFace *f=doc.rooms[2].faces+i; f->room=2; f->layer=1; f->id=i+3;
        BgMaterialSetTexture(&f->material,9); BgMaterialSetDetail(&f->material,&detail); f->textureid=9;
    }
    BgMaterial material=doc.rooms[2].faces[0].material;
    BgFaceRef both[4]={{1,1,0,0},{2,1,0,0},{3,2,1,0},{4,2,1,0}};
    normal=plane; normal.normal[0]=-5;
    assert(BgDocumentKnifeFaces(&doc,both,4,&normal,&result,&n,&cuts,&why) && cuts==4 && n==12);
    assert(doc.rooms[1].vertexcount==7 && doc.rooms[2].vertexcount==7);
    for(DWORD i=0;i<doc.rooms[2].facecount;i++) { assert(doc.rooms[2].faces[i].layer==1 && doc.rooms[2].faces[i].room==2); assert(!memcmp(&doc.rooms[2].faces[i].material,&material,sizeof(material))); }
    Persist(&doc,&source,argv[1]); free(result); BgDocumentFree(&doc);
    /* The entire cut is one undo action; redo and rollback keep all corners. */
    EditHistory history={0}; EditHistoryTransaction transaction={0}; EditHistoryAsset asset;
    SetupFile setup={0}; StanFile stan={0}; BgDocument after={0};
    assert(BgDocumentClone(&before,&doc,&why)); EditHistoryReset(&history,&doc,&setup,&stan);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Knife Faces",&transaction,&why));
    assert(BgDocumentKnifeFaces(&doc,refs,2,&plane,&result,&n,&cuts,&why)); free(result);
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&transaction,&why));
    assert(BgDocumentClone(&doc,&after,&why)); assert(history.undocount==1);
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why)); Same(&doc,&before);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why)); Same(&doc,&after);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Knife Faces",&transaction,&why));
    normal=plane; normal.normal[0]=0; normal.normal[1]=1; normal.position[1]=420;
    BgFaceRef all[6]; for(int i=0;i<6;i++) all[i]=(BgFaceRef){doc.rooms[1].faces[i].id,1,0,0};
    assert(BgDocumentKnifeFaces(&doc,all,6,&normal,&result,&n,&cuts,&why)); free(result);
    EditHistoryRollbackEdit(&transaction,&doc,&setup,&stan); Same(&doc,&after);
    EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&before); BgDocumentFree(&after); BgFileFree(&source);
    puts("PASS: plane input, bisect/triangulate, shared edges and seams, UV/RGBA, winding/area, no-op cuts, native precision, atomic allocation failures, save/reload, undo/redo and rollback.");
}
