#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
typedef void *HWND;
typedef struct {int unused;} MSG;
#include "bakedlighting.h"
BOOL SetupFileClone(const SetupFile *s,SetupFile *out,const char **why) {abort();}
BOOL SetupFileCompact(SetupFile *s,const char **why) {abort();}
void SetupFileFree(SetupFile *s) {abort();}
void StanFileFree(StanFile *s) {abort();}
static int allocations=-1;
static BOOL FailAllocation(void) {if (!allocations) return TRUE;if (allocations>0) allocations--;return FALSE;}
static void *TestMalloc(size_t n) {return FailAllocation()?NULL:malloc(n);}
static void *TestCalloc(size_t n,size_t size) {return FailAllocation()?NULL:calloc(n,size);}
#define malloc TestMalloc
#define calloc TestCalloc
#include "bgao.c"
#include "bglighting.c"
#undef malloc
#undef calloc
#include "fixture.inc"
static void Same(const BgDocument *a,const BgDocument *b)
{
    assert(a->roomcount==b->roomcount&&a->facecount==b->facecount&&a->dirty==b->dirty);
    assert(a->nextvertexid==b->nextvertexid&&a->nextfaceid==b->nextfaceid);
    for (DWORD r=1;r<=a->roomcount;r++)
    {
        const BgDocumentRoom *x=a->rooms+r,*y=b->rooms+r;
        assert(x->vertexcount==y->vertexcount&&x->facecount==y->facecount);
        if (x->vertexcount) assert(!memcmp(x->vertices,y->vertices,x->vertexcount*sizeof(*x->vertices)));
        if (x->facecount) assert(!memcmp(x->faces,y->faces,x->facecount*sizeof(*x->faces)));
    }
}
static void Counts(const BgDocument *doc)
{
    for (DWORD r=1;r<=doc->roomcount;r++) for (DWORD v=0;v<doc->rooms[r].vertexcount;v++)
    {
        DWORD count=0;
        for (DWORD f=0;f<doc->rooms[r].facecount;f++) for (int c=0;c<3;c++)
            count+=doc->rooms[r].faces[f].vertexindices[c]==v;
        assert(count==doc->rooms[r].vertices[v].usecount);
    }
}
static void Corner(BgDocument *doc)
{
    BgDocumentRoom *r=doc->rooms+1;
    r->vertices=realloc(r->vertices,4*sizeof(*r->vertices));assert(r->vertices);
    r->faces=realloc(r->faces,2*sizeof(*r->faces));assert(r->faces);
    const short p[4][3]={{0,0,0},{100,0,0},{0,100,0},{0,0,100}};
    for (int v=0;v<4;v++)
    {
        r->vertices[v]=(BgDocumentVertex){.id=doc->nextvertexid++,.room=1,
            .x=p[v][0],.y=p[v][1],.z=p[v][2],.flag=123,.s=v*123,.t=-v*321,
            .r=17,.g=29,.b=37,.a=80+v*20,.usecount=v<2?2:1};
    }
    r->faces[1]=r->faces[0];r->faces[1].id=doc->nextfaceid++;
    DWORD indices[2][3]={{0,1,2},{1,0,3}};
    memcpy(r->faces[0].vertexindices,indices[0],sizeof(indices[0]));
    memcpy(r->faces[1].vertexindices,indices[1],sizeof(indices[1]));
    r->facecount=r->facecapacity=doc->facecount=2;r->vertexcount=4;
    /* A second room proves that selection is scoped and multi-room failure atomic. */
    BgDocumentRoom *other=doc->rooms+2;
    other->vertices=malloc(3*sizeof(*other->vertices));other->faces=malloc(sizeof(*other->faces));
    assert(other->vertices&&other->faces);
    memcpy(other->vertices,r->vertices,3*sizeof(*other->vertices));
    for (int v=0;v<3;v++) {other->vertices[v].room=2;other->vertices[v].id=doc->nextvertexid++;other->vertices[v].usecount=1;}
    other->faces[0]=r->faces[0];other->faces[0].room=2;other->faces[0].id=doc->nextfaceid++;
    other->vertexcount=3;other->facecount=other->facecapacity=1;doc->facecount++;
    other->layers[0].groups=calloc(1,sizeof(BgDocumentDrawGroup));assert(other->layers[0].groups);
    other->layers[0].groupcount=other->layers[0].groupcapacity=1;
    other->layers[0].sourcepresent=TRUE;other->faces[0].drawgroup=0;
}
static BgLightingSettings Lights(void)
{ return (BgLightingSettings){{20,40,60},{180,120,60},1,1,{0,0,10},60,FALSE,1,200}; }
static const BgDocumentVertex *Vertex(const BgDocument *doc,DWORD f,int c)
{return doc->rooms[1].vertices+doc->rooms[1].faces[f].vertexindices[c];}
static void RGB(const BgDocumentVertex *v,int r,int g,int b) {assert(v->r==r&&v->g==g&&v->b==b);}
static void Attributes(const BgDocument *a,const BgDocument *b)
{
    for (DWORD f=0;f<a->rooms[1].facecount;f++)
    {
        BgDocumentFace face=b->rooms[1].faces[f];
        memcpy(face.vertexindices,a->rooms[1].faces[f].vertexindices,sizeof(face.vertexindices));
        assert(!memcmp(&face,a->rooms[1].faces+f,sizeof(face)));
        for (int c=0;c<3;c++)
        {
            const BgDocumentVertex *x=Vertex(a,f,c),*y=Vertex(b,f,c);
            assert(x->x==y->x&&x->y==y->y&&x->z==y->z&&x->s==y->s&&x->t==y->t&&x->a==y->a&&x->flag==y->flag);
        }
    }
    assert(!memcmp(a->rooms[2].vertices,b->rooms[2].vertices,3*sizeof(BgDocumentVertex)));
    assert(!memcmp(a->rooms[2].faces,b->rooms[2].faces,sizeof(BgDocumentFace)));
    Counts(b);
}
static void Geometry(const char *dir)
{
    BgFile source=Fixture();BgDocument original={0},doc={0},baked={0};const char *why="";
    assert(BgDocumentLoad(source.data,source.size,1,&original,&why));Corner(&original);
    DWORD rooms[]={1,1};BgLightingResult result;BgLightingSettings light=Lights();
    assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentBakeLighting(&doc,rooms,2,&light,&result,&why));
    assert(result.rooms==1&&result.faces==2&&result.splits==2&&doc.rooms[1].vertexcount==6);
    for (int c=0;c<3;c++) {RGB(Vertex(&doc,0,c),200,160,120);RGB(Vertex(&doc,1,c),20,40,60);}
    Attributes(&original,&doc);RoundTrip(&doc,&source,dir);
    assert(BgDocumentClone(&doc,&baked,&why));
    assert(BgDocumentBakeLighting(&doc,rooms,2,&light,&result,&why)&&!result.vertices&&!result.splits);
    Same(&doc,&baked);BgDocumentFree(&doc);BgDocumentFree(&baked);

    light.smoothAngle=100;assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why)&&!result.splits);
    RGB(Vertex(&doc,0,0),147,125,102);RGB(Vertex(&doc,1,1),147,125,102);
    RGB(Vertex(&doc,0,1),147,125,102);RGB(Vertex(&doc,1,0),147,125,102);
    RGB(Vertex(&doc,0,2),200,160,120);RGB(Vertex(&doc,1,2),20,40,60);
    Attributes(&original,&doc);RoundTrip(&doc,&source,dir);BgDocumentFree(&doc);

    /* Existing splits at the same position must stay hard, even at 180 degrees. */
    assert(BgDocumentClone(&original,&doc,&why));
    doc.rooms[1].vertices=realloc(doc.rooms[1].vertices,6*sizeof(BgDocumentVertex));assert(doc.rooms[1].vertices);
    for (int i=0;i<2;i++) {doc.rooms[1].vertices[4+i]=doc.rooms[1].vertices[i];doc.rooms[1].vertices[4+i].id=doc.nextvertexid++;}
    doc.rooms[1].vertexcount=6;doc.rooms[1].faces[1].vertexindices[0]=5;doc.rooms[1].faces[1].vertexindices[1]=4;
    light.smoothAngle=180;assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why));
    RGB(Vertex(&doc,0,0),200,160,120);RGB(Vertex(&doc,1,1),20,40,60);assert(!result.splits);BgDocumentFree(&doc);

    /* Secondary geometry is included but does not smooth across layer boundaries. */
    assert(BgDocumentClone(&original,&doc,&why));doc.rooms[1].faces[1].layer=1;
    assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why)&&result.splits==2);
    RGB(Vertex(&doc,1,1),20,40,60);BgDocumentFree(&doc);

    /* Ambient-only accepts a zero direction, clamps channels, and needs no splits. */
    assert(BgDocumentClone(&original,&doc,&why));light=Lights();light.directionalIntensity=0;
    memset(light.direction,0,sizeof(light.direction));light.ambientIntensity=4;
    assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why)&&!result.splits);
    RGB(Vertex(&doc,0,0),80,160,240);BgDocumentFree(&doc);
    assert(BgDocumentClone(&original,&doc,&why));light=Lights();light.direction[2]=-1;
    assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why));RGB(Vertex(&doc,0,0),20,40,60);BgDocumentFree(&doc);
    assert(BgDocumentClone(&original,&doc,&why));light=Lights();light.ambientIntensity=4;light.directionalIntensity=4;
    assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why));RGB(Vertex(&doc,0,0),255,255,255);BgDocumentFree(&doc);

    /* Degenerate triangles are kept unchanged and excluded from normal sums. */
    assert(BgDocumentClone(&original,&doc,&why));doc.rooms[1].faces[1].vertexindices[2]=1;
    light=Lights();assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why));
    RGB(Vertex(&doc,1,0),17,29,37);RGB(Vertex(&doc,0,0),200,160,120);assert(result.faces==1);BgDocumentFree(&doc);
    BgDocumentFree(&original);BgFileFree(&source);
    puts("PASS: Lambert RGB, normalized/sign-correct direction, ambient/clamping, hard/smooth/UV-split/layer normals, degenerates, room scope, repeat bake, alpha/UV/material preservation and native save/export.");
}
static void AmbientOcclusion(const char *dir)
{
    BgFile source=Fixture();BgDocument original={0},doc={0};const char *why="";
    assert(BgDocumentLoad(source.data,source.size,2,&original,&why));Corner(&original);
    BgDocumentRoom *floor=original.rooms+1,*ceiling=original.rooms+2;
    memset(floor->origin,0,sizeof(floor->origin));
    const short ground[3][3]={{0,0,0},{0,0,100},{100,0,0}};
    for (int i=0;i<3;i++)
    { floor->vertices[i].x=ground[i][0];floor->vertices[i].y=ground[i][1];floor->vertices[i].z=ground[i][2];floor->vertices[i].usecount=1; }
    floor->vertexcount=3;floor->facecount=1;
    ceiling->vertices=realloc(ceiling->vertices,4*sizeof(*ceiling->vertices));assert(ceiling->vertices);
    ceiling->vertices[3]=ceiling->vertices[0];ceiling->vertices[3].id=original.nextvertexid++;
    ceiling->faces=realloc(ceiling->faces,2*sizeof(*ceiling->faces));assert(ceiling->faces);
    ceiling->faces[1]=ceiling->faces[0];ceiling->faces[1].id=original.nextfaceid++;
    ceiling->vertexcount=4;ceiling->facecount=ceiling->facecapacity=2;original.facecount=3;
    ceiling->origin[0]=300;ceiling->origin[1]=400;ceiling->origin[2]=500;
    const short top[4][3]={{-1000,10,-1000},{1000,10,-1000},{1000,10,1000},{-1000,10,1000}};
    for (int i=0;i<4;i++)
    {
        ceiling->vertices[i].x=top[i][0]-300;ceiling->vertices[i].y=top[i][1]-400;ceiling->vertices[i].z=top[i][2]-500;
        ceiling->vertices[i].usecount=(i==0||i==2)?2:1;
    }
    DWORD indices[2][3]={{0,1,2},{0,2,3}};
    for (int i=0;i<2;i++)memcpy(ceiling->faces[i].vertexindices,indices[i],sizeof(indices[i]));
    DWORD rooms[]={1};BgLightingResult result;BgLightingSettings light=g_BgLightingDefaults;
    light.ambient[0]=light.ambient[1]=light.ambient[2]=100;light.ambientIntensity=1;
    light.directional[0]=light.directional[1]=light.directional[2]=60;light.directionalIntensity=1;
    light.direction[0]=light.direction[2]=0;light.direction[1]=1;light.aoEnabled=TRUE;light.aoRadius=100;
    int full=0;
    for (int mode=0;mode<8;mode++)
    {
        assert(BgDocumentClone(&original,&doc,&why));light.aoEnabled=mode!=1;light.aoStrength=mode==2?0:mode==3?.5:1;
        light.aoRadius=mode==4||mode==7?4:100;
        if (mode==5) { doc.rooms[2].faces[0].layer=doc.rooms[2].faces[1].layer=1; }
        if (mode==6) { doc.rooms[1].faces[0].layer=1; }
        if (mode==7) { doc.levelscale=4; } /* Ceiling is now 2.5 world units away. */
        assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why));
        const BgDocumentVertex *v=Vertex(&doc,0,0);
        if (!mode) { full=v->r;assert(full>=60&&full<85);RoundTrip(&doc,&source,dir); }
        if (mode==1||mode==2||mode==4||mode==5) { RGB(v,160,160,160); }
        if (mode==3) { assert(abs(v->r-(full+160)/2)<=1); }
        if (mode==6) { assert(v->r==full); }
        if (mode==7) { assert(v->r<160); }
        assert(v->a==original.rooms[1].vertices[0].a&&v->s==original.rooms[1].vertices[0].s);
        assert(!memcmp(doc.rooms[2].vertices,original.rooms[2].vertices,4*sizeof(BgDocumentVertex)));
        assert(BgDocumentBakeLighting(&doc,rooms,1,&light,&result,&why)&&!result.vertices&&!result.splits);
        BgDocumentFree(&doc);
    }
    /* Reversing an occluder's winding does not let ambient light through. */
    for (int i=0;i<2;i++)
    { DWORD swap=ceiling->faces[i].vertexindices[1];ceiling->faces[i].vertexindices[1]=ceiling->faces[i].vertexindices[2];ceiling->faces[i].vertexindices[2]=swap; }
    light.aoRadius=100;assert(BgDocumentBakeLighting(&original,rooms,1,&light,&result,&why));assert(Vertex(&original,0,0)->r==full);
    BgDocumentFree(&original);BgFileFree(&source);
    puts("PASS: primary-only AO across unselected rooms, receiver layers, two-sided blockers, radius/world scale/origins, 0/50/100% strength, direct-light preservation, repeat bake and native persistence.");
}
static void Acceleration(void)
{
    /* Compare tree traversal against a flat exhaustive traversal, including
     * parallel rays, shared edges and sparse geometry spread over a level. */
    BgAoScene scene={0};scene.count=400;
    scene.triangles=calloc(scene.count,sizeof(*scene.triangles));scene.nodes=calloc(scene.count*2,sizeof(*scene.nodes));
    assert(scene.triangles&&scene.nodes);
    for (DWORD i=0;i<scene.count;i++)
    {
        AoTriangle *t=scene.triangles+i;t->p[0]=(i%20)*20;t->p[1]=10+(i%7)*5;t->p[2]=(i/20)*20;
        t->a[0]=20;t->b[2]=20;
        for (int k=0;k<3;k++) { t->min[k]=t->p[k];t->max[k]=t->p[k]+t->a[k]+t->b[k]; }
    }
    AoBuildNode(&scene,0,scene.count,0);assert(scene.nodecount>1);
    BgAoScene flat=scene;AoNode root=scene.nodes[0];root.first=0;root.count=scene.count;flat.nodes=&root;
    for (int i=0;i<2000;i++)
    {
        double p[3]={(i*137)%500-50,0,(i*73)%500-50},d[3]={sin(i),1,cos(i)},a=500,b=500;
        if (i%3==0) { d[0]=d[2]=0; }
        AoTrace(&scene,0,p,d,.0001,&a);AoTrace(&flat,0,p,d,.0001,&b);assert(fabs(a-b)<1e-8);
    }
    free(scene.triangles);free(scene.nodes);
    puts("PASS: AO acceleration tree matches exhaustive triangle tracing for 2000 rays.");
}
static void Failures(void)
{
    BgFile source=Fixture();BgDocument original={0},doc={0};const char *why="";
    assert(BgDocumentLoad(source.data,source.size,1,&original,&why));Corner(&original);
    BgLightingSettings light=Lights();BgLightingResult result;DWORD rooms[]={1,2};
    light.aoEnabled=TRUE;
    for (int fail=0;;fail++)
    {
        assert(fail<100);assert(BgDocumentClone(&original,&doc,&why));allocations=fail;
        BOOL ok=BgDocumentBakeLighting(&doc,rooms,2,&light,&result,&why);allocations=-1;
        if (ok) {assert(result.rooms==2);BgDocumentFree(&doc);break;}
        assert(why[0]&&!result.rooms&&!result.vertices);Same(&doc,&original);BgDocumentFree(&doc);
    }
    assert(BgDocumentClone(&original,&doc,&why));rooms[1]=99;
    assert(!BgDocumentBakeLighting(&doc,rooms,2,&light,&result,&why));Same(&doc,&original);
    rooms[1]=2;doc.rooms[2].faces[0].vertexindices[2]=99;
    assert(!BgDocumentBakeLighting(&doc,rooms,2,&light,&result,&why));doc.rooms[2].faces[0].vertexindices[2]=2;Same(&doc,&original);
    for (int invalid=0;invalid<6;invalid++)
    {
        light=Lights();
        if (invalid==0) light.ambientIntensity=NAN;
        if (invalid==1) light.direction[0]=INFINITY;
        if (invalid==2) light.direction[2]=0;
        if (invalid==3) light.smoothAngle=-1;
        if (invalid==4) light.directionalIntensity=5;
        if (invalid==5) light.smoothAngle=181;
        assert(!BgDocumentBakeLighting(&doc,rooms,2,&light,&result,&why));Same(&doc,&original);
    }
    BgDocumentFree(&doc);BgDocumentFree(&original);BgFileFree(&source);
    puts("PASS: validation, invalid room/face, every allocation failure and atomic multi-room rollback.");
}
static BgDocument g_CurrentBgDocument;
static BgFile g_CurrentBg;
static EditHistory g_EditHistory;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static HWND g_Viewport=(HWND)1;
static BOOL flying,transforming,failrebuild;
static BOOL ViewportIsFlying(HWND h) {return flying;}
static BOOL ViewportIsTransforming(HWND h) {return transforming;}
static BOOL GEditorRebuildCurrentViewport(const char **why)
{if (failrebuild) {failrebuild=FALSE;*why="Injected viewport failure";return FALSE;}return TRUE;}
static void GEditorRestoreHistorySelection(HWND h) {}
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND h) {}
#include "editor.inc"
static void History(void)
{
    const char *why="";BgDocument original={0},baked={0};DWORD rooms[]={1};
    g_CurrentBg=Fixture();assert(BgDocumentLoad(g_CurrentBg.data,g_CurrentBg.size,1,&g_CurrentBgDocument,&why));Corner(&g_CurrentBgDocument);
    assert(BgDocumentClone(&g_CurrentBgDocument,&original,&why));
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    BakedLightingRequest request={.rooms=rooms,.count=1,.settings=Lights()};
    request.settings.aoEnabled=TRUE;
    failrebuild=TRUE;assert(!GEditorBakeLighting(NULL,&request));Same(&g_CurrentBgDocument,&original);assert(!g_EditHistory.undocount);
    ULONGLONG revision=g_EditHistory.nextrevision;g_EditHistory.nextrevision=0;
    assert(!GEditorBakeLighting(NULL,&request));g_EditHistory.nextrevision=revision;Same(&g_CurrentBgDocument,&original);
    flying=TRUE;assert(!GEditorBakeLighting(NULL,&request));flying=FALSE;
    transforming=TRUE;assert(!GEditorBakeLighting(NULL,&request));transforming=FALSE;
    assert(GEditorBakeLighting(NULL,&request)&&g_EditHistory.undocount==1);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Bake Lighting"));
    assert(BgDocumentClone(&g_CurrentBgDocument,&baked,&why));
    assert(GEditorBakeLighting(NULL,&request)&&g_EditHistory.undocount==1);Same(&g_CurrentBgDocument,&baked);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(&g_CurrentBgDocument,&original);
    /* Invalid requests leave the available redo intact. */
    request.settings.direction[2]=0;assert(!GEditorBakeLighting(NULL,&request));assert(EditHistoryCanRedo(&g_EditHistory));
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(&g_CurrentBgDocument,&baked);
    EditHistoryMarkBgSaved(&g_EditHistory,&g_CurrentBgDocument);assert(!g_CurrentBgDocument.dirty);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)&&g_CurrentBgDocument.dirty);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)&&!g_CurrentBgDocument.dirty);
    EditHistoryFree(&g_EditHistory);BgDocumentFree(&g_CurrentBgDocument);BgDocumentFree(&original);BgDocumentFree(&baked);BgFileFree(&g_CurrentBg);
    puts("PASS: real editor transaction, viewport/commit rollback, one-step undo/redo, no-op rebake, redo preservation and save revision tracking.");
}
int main(int argc,char **argv) {assert(argc==2);Geometry(argv[1]);AmbientOcclusion(argv[1]);Acceleration();Failures();History();return 0;}
