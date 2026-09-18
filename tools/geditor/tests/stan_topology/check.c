#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include "stanload.h"
#include "bghistory.h"
#include "edittool.h"
#include "project.h"
#include "setupmeta.h"
void BgDocumentFree(BgDocument *doc) { abort(); }
BOOL SetupFileCompact(SetupFile *setup, const char **why) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static int failafter=-1;
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t);
static BOOL Fail(void) { if(failafter<0)return FALSE;if(!failafter)return TRUE;failafter--;return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { return Fail() ? NULL : __real_calloc(n,s); }
static char g_RomExportError[256];
#include "export.inc"
#include "helpers.inc"
static void Require(BOOL ok,const char *why) { if(!ok) { fprintf(stderr,"%s\n",why);abort(); } }
static const char *why="";
static StanFile Fixture(const char *dir)
{
    StanFile raw={0},s={0}; raw.size=212;raw.data=calloc(raw.size,1);strcpy(raw.name,"Tbg_topology_test_stanZ");
    Put(raw.data,0x12345678);Put(raw.data+4,0x0e000014);Put(raw.data+8,0x0e000064);Put(raw.data+12,0x0e00008c);
    const short xyz[4][4][3]={
        {{0,0,0},{0,0,20},{20,0,20},{20,0,0}},
        {{20,0,0},{20,0,20},{40,0,20},{40,0,0}},
        {{0,20,0},{0,20,20},{20,20,20},{20,20,0}},
        {{40,0,0},{40,0,20},{60,0,20},{60,0,0}}};
    for(DWORD t=0;t<4;t++) {
        unsigned char *p=raw.data+20+t*40;Put(p,((100+t)<<8)|(t+1));Put16(p+4,0x7abc);Put16(p+6,0x4012);
        for(int v=0;v<4;v++)for(int a=0;a<3;a++)Put16(p+8+v*8+a*2,xyz[t][v][a]);
    }
    Put16(raw.data+20+8+2*8+6,0x15);Put16(raw.data+60+8+6,0x10);
    Put16(raw.data+60+8+2*8+6,0x1f);Put16(raw.data+140+8+6,0x15);
    Put16(raw.data+100+8+6,0xf); /* Reserved boundary value must survive. */
    memcpy(raw.data+188,"unstric",8);memset(raw.data+196,0x5a,16);
    Require(StanSaveProjectFile(dir,&raw,&why),why);Require(StanLoadProjectFile(dir,raw.name,.25f,&s,&why),why);
    free(raw.data);return s;
}
static void SetLink(StanFile *s,DWORD t,DWORD p,unsigned short link)
{ s->tiles[t].points[p].link=link;Put16(s->data+s->tiles[t].sourceoffset+8+p*8+6,link); }
static void Preserved(const StanFile *a,const StanFile *b)
{
    assert(a->tilecount==b->tilecount && Get(a->data)==Get(b->data));
    for(DWORD t=0;t<a->tilecount;t++) {
        assert(a->tiles[t].id==b->tiles[t].id && a->tiles[t].room==b->tiles[t].room);
        assert(!memcmp(a->data+a->tiles[t].sourceoffset,b->data+b->tiles[t].sourceoffset,6));
        for(unsigned p=0;p<b->tiles[t].pointcount;p++) {
            assert(b->tiles[t].extreme[0]<b->tiles[t].pointcount && b->tiles[t].extreme[1]<b->tiles[t].pointcount
                && b->tiles[t].extreme[2]<b->tiles[t].pointcount);
            if(b->tiles[t].points[p].link>=0x10)assert(StanLinkedTile(b,b->tiles[t].points[p].link)!=STAN_TILE_NONE);
        }
    }
    assert((Get(b->data+8)&0xffffff)==b->tiles[2].sourceoffset);
    assert((Get(b->data+12)&0xffffff)==b->tiles[3].sourceoffset);
    assert(!memcmp(a->data+a->size-32,b->data+b->size-32,32));
}
static void Walk(const StanFile *s,BOOL pass)
{
    DWORD t=0;assert(StanWalkTiles(s,&t,40,40,120,40)==pass);if(pass)assert(t==1);
    t=1;assert(StanWalkTiles(s,&t,120,40,40,40)==pass);if(pass)assert(t==0);
}
static void Reject(StanFile *s,const StanPointRef *refs,DWORD count)
{
    StanFile copy={0};Require(StanFileClone(s,&copy,&why),why);
    assert(!StanMergeVertices(s,refs,count,NULL,&why) && why[0]);Same(s,&copy);StanFileFree(&copy);
}

/* Actual viewport visibility, mesh rebuild, canonical selection and ray picker,
 * with only native-window/OpenGL notifications replaced by inert stubs. */
typedef void *HWND;
typedef int GLsizei;
typedef struct Vertex { float x,y,z;unsigned char r,g,b,a;float s,t; } Vertex;
typedef struct ViewportStanComponent { StanPointRef refs[2]; } ViewportStanComponent;
typedef struct ViewportPickRay { double origin[3],direction[3],mindistance,maxdistance; } ViewportPickRay;
typedef struct ViewportState {
    StanFile stan;DWORD *stanpointmap,*stanhiddenids,stanhiddencount;
    unsigned char *stanselected;ViewportStanComponent *stancomponents;
    int stancomponentcount,stancomponentcapacity,componentcount,stanopacity;
    BOOL showstan;EditorTool tool;Vertex *stanfill,*stanedges;GLsizei stanfillcount,stanedgecount;
} ViewportState;
#define VIEWPORT_WM_SELECTION_CHANGED 3
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportRefreshStanOverlay(ViewportState *s);
static void ViewportClearStanSelection(ViewportState *s);
static void ViewportClearAllSelection(ViewportState *s) { s->componentcount=0;ViewportClearStanSelection(s); }
static void ViewportCancelTransform(HWND hwnd) {}
static void ViewportUpdateGizmo(ViewportState *s) {}
static void ViewportRefreshPadPreview(ViewportState *s) {}
static void InvalidateRect(HWND hwnd,const void *rect,BOOL erase) {}
static HWND GetParent(HWND hwnd) { return NULL; }
static void SendMessage(HWND hwnd,int message,int w,int l) {}
#include "viewport.inc"
static void FreeView(ViewportState *v) { assert(ViewportSetStanTiles(v,NULL));free(v->stancomponents);memset(v,0,sizeof(*v)); }
static void Visibility(const StanFile *source,const char *dir)
{
    ViewportState v={.showstan=TRUE,.stanopacity=44,.tool=EDITOR_TOOL_FACE_SELECT};
    assert(ViewportSetStanTiles(&v,source));int fill=v.stanfillcount,edges=v.stanedgecount;
    ViewportPickRay ray={.origin={40,200,40},.direction={0,-1,0},.maxdistance=1000};double distance;
    assert(ViewportFindPickedStan(&v,&ray,&distance)==2);v.stanselected[2]=TRUE;
    failafter=0;assert(!ViewportHideSelectedStanTiles(&v));failafter=-1;
    assert(v.stanselected[2] && !ViewportHasHiddenStanTiles(&v));
    assert(ViewportHideSelectedStanTiles(&v) && ViewportStanTileHidden(&v,2));
    assert(!ViewportGetStanSelectionCount(&v,NULL));Same(source,&v.stan);
    assert(v.stanfillcount==fill-6 && v.stanedgecount==edges-8);
    assert(ViewportFindPickedStan(&v,&ray,&distance)==0);Persist(dir,&v.stan);
    assert(ViewportSetStanTiles(&v,source) && ViewportStanTileHidden(&v,2));
    /* Keep hidden editor identities as offsets/indices shift through delete and undo. */
    StanFile changed={0};DWORD tile=0,deleted;
    Require(StanFileClone(source,&changed,&why),why);Require(StanDeleteTiles(&changed,&tile,1,&deleted,&why),why);
    assert(ViewportSetStanTiles(&v,&changed) && ViewportStanTileHidden(&v,1));
    assert(ViewportSetStanTiles(&v,source) && ViewportStanTileHidden(&v,2));StanFileFree(&changed);
    /* Some levels contain duplicate native IDs. H must affect only the
     * selected tile even if a different tile has that same authored ID. */
    Require(StanFileClone(source,&changed,&why),why);
    changed.tiles[0].id=changed.tiles[2].id;
    Put(changed.data+changed.tiles[0].sourceoffset,(changed.tiles[0].id<<8)|changed.tiles[0].room);
    assert(ViewportSetStanTiles(&v,&changed) && ViewportStanTileHidden(&v,2) && !ViewportStanTileHidden(&v,0));
    StanFileFree(&changed);assert(ViewportSetStanTiles(&v,source));
    /* A shared vertex whose canonical owner is hidden can still be selected
     * through a visible incident tile. A wholly hidden vertex cannot. */
    v.stanselected[0]=TRUE;assert(ViewportHideSelectedStanTiles(&v));v.tool=EDITOR_TOOL_VERTEX_SELECT;
    assert(ViewportSelectStanVertex(&v,&(StanPointRef){1,0}));
    assert(!ViewportSelectStanVertex(&v,&(StanPointRef){2,0}));
    ViewportUnhideAllStanTiles(&v);assert(!ViewportHasHiddenStanTiles(&v));
    assert(v.stanfillcount==fill && v.stanedgecount==edges && ViewportFindPickedStan(&v,&ray,&distance)==2);
    v.tool=EDITOR_TOOL_FACE_SELECT;v.stanselected[2]=TRUE;assert(ViewportHideSelectedStanTiles(&v));
    Require(StanFileClone(source,&changed,&why),why);strcpy(changed.name,"Other_level");
    assert(ViewportSetStanTiles(&v,&changed) && !ViewportHasHiddenStanTiles(&v));StanFileFree(&changed);FreeView(&v);
}

#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
static ViewportState view;
static HWND g_Viewport=&view;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL flying,transforming,failrebuild;
static unsigned errors,restores;
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static int ViewportGetSelectedComponentCount(HWND hwnd) { return view.componentcount; }
static BOOL ViewportGetVertexSnap(HWND hwnd) { return FALSE; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **reason)
{
    if(failrebuild) { failrebuild=FALSE;*reason="Rebuild failed.";return FALSE; }
    return ViewportSetStanTiles(&view,&g_CurrentStan);
}
static void GEditorRestoreHistorySelection(HWND hwnd) { restores++; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *reason,const char *title,unsigned flags) { assert(reason[0]);errors++; }
/* This harness exercises the Stan branch; BG is covered in bisect_edge. */
BOOL BgDocumentBisectEdge(BgDocument *doc,const BgDocumentEdgeRef *edge,BgDocumentEdgeRef out[2],const char **why) { abort(); }
static BOOL ViewportGetSelectedBgEdges(HWND hwnd,BgDocumentEdgeRef *out,DWORD count) { return FALSE; }
static BOOL ViewportSelectBgEdges(HWND hwnd,const BgDocumentEdgeRef *edges,DWORD count) { abort(); }
static BOOL GEditorRebuildCurrentViewport(const char **why) { abort(); }
#include "controller.inc"
static void Controller(const StanFile *source,const char *dir)
{
    Require(StanFileClone(source,&g_CurrentStan,&why),why);
    view=(ViewportState){.showstan=TRUE,.stanopacity=44,.tool=EDITOR_TOOL_EDGE_SELECT};
    assert(ViewportSetStanTiles(&view,&g_CurrentStan));StanEdgeRef edge={0,2},read;
    assert(ViewportSelectStanEdge(&view,&edge));assert(ViewportGetSelectedStanEdge(&view,&read) && read.tile==0 && read.point==2);
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    for(int f=0;f<3;f++) {
        ULONGLONG revision=g_EditHistory.nextrevision;
        failrebuild=f==0;if(f==1)g_EditHistory.nextrevision=0;if(f==2)failafter=0;
        assert(!GEditorEditStanTopology(NULL,&edge));failafter=-1;g_EditHistory.nextrevision=revision;
        Same(source,&g_CurrentStan);assert(!g_EditHistory.undocount);
    }
    assert(errors==3 && restores==2);
    assert(GEditorEditStanTopology(NULL,&edge) && g_EditHistory.undocount==1);Walk(&g_CurrentStan,FALSE);
    unsigned n=g_EditHistory.undocount;assert(GEditorEditStanTopology(NULL,&edge) && g_EditHistory.undocount==n);
    Require(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);Walk(&g_CurrentStan,TRUE);
    assert(GEditorReloadCurrentObjectsAndViewport(&why));
    view.tool=EDITOR_TOOL_VERTEX_SELECT;assert(ViewportSelectStanVertex(&view,&(StanPointRef){0,0}));
    ViewportStanComponent *components=realloc(view.stancomponents,2*sizeof(*components));assert(components);
    view.stancomponents=components;view.stancomponentcapacity=2;view.stancomponentcount=2;
    view.stancomponents[1].refs[0]=view.stancomponents[1].refs[1]=(StanPointRef){0,1};
    flying=TRUE;assert(!GEditorEditStanTopology(NULL,NULL));flying=FALSE;
    transforming=TRUE;assert(!GEditorEditStanTopology(NULL,NULL));transforming=FALSE;
    assert(GEditorEditStanTopology(NULL,NULL));assert(g_CurrentStan.tiles[0].pointcount==3 && view.stancomponentcount==1);
    Persist(dir,&g_CurrentStan);EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    Require(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tiles[0].pointcount==4 && g_CurrentStan.dirty);
    Require(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tiles[0].pointcount==3 && !g_CurrentStan.dirty);
    EditHistoryFree(&g_EditHistory);StanFileFree(&g_CurrentStan);FreeView(&view);
}

static void EdgeLinkController(const StanFile *source,const char *dir)
{
    const StanEdgeRef edge={1,0};StanEdgeRef read;StanFile unlinked={0};
    Require(StanFileClone(source,&g_CurrentStan,&why),why);
    SetLink(&g_CurrentStan,0,2,0);SetLink(&g_CurrentStan,1,0,0);
    Require(StanFileClone(&g_CurrentStan,&unlinked,&why),why);
    view=(ViewportState){.showstan=TRUE,.stanopacity=44,.tool=EDITOR_TOOL_EDGE_SELECT};
    assert(ViewportSetStanTiles(&view,&g_CurrentStan));assert(ViewportSelectStanEdge(&view,&edge));
    assert(view.stancomponents[0].refs[0].tile==1); /* This canonical owner changes when linked. */
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    unsigned olderrors=errors,oldrestores=restores;
    for(int f=0;f<3;f++) {
        ULONGLONG revision=g_EditHistory.nextrevision;
        failrebuild=f==0;if(f==1)g_EditHistory.nextrevision=0;if(f==2)failafter=0;
        assert(!GEditorLinkStanTiles(NULL,&edge));failafter=-1;g_EditHistory.nextrevision=revision;
        Same(&unlinked,&g_CurrentStan);assert(!g_EditHistory.undocount);
    }
    assert(errors==olderrors+3 && restores==oldrestores+2);
    flying=TRUE;assert(!GEditorLinkStanTiles(NULL,&edge));flying=FALSE;
    transforming=TRUE;assert(!GEditorLinkStanTiles(NULL,&edge));transforming=FALSE;
    view.tool=EDITOR_TOOL_FACE_SELECT;assert(!GEditorLinkStanTiles(NULL,&edge));view.tool=EDITOR_TOOL_EDGE_SELECT;
    assert(GEditorLinkStanTiles(NULL,&edge) && g_EditHistory.undocount==1);Walk(&g_CurrentStan,TRUE);
    assert(ViewportGetSelectedStanEdge(&view,&read) && read.tile==0 && read.point==2);
    assert(view.stancomponents[0].refs[0].tile==0 && view.stancomponents[0].refs[1].tile==0);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Link Stan Tiles"));
    Persist(dir,&g_CurrentStan);EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    assert(GEditorLinkStanTiles(NULL,&edge) && g_EditHistory.undocount==1 && !g_CurrentStan.dirty);
    Require(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    Walk(&g_CurrentStan,FALSE);
    Require(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    Walk(&g_CurrentStan,TRUE);assert(!g_CurrentStan.dirty);
    EditHistoryFree(&g_EditHistory);StanFileFree(&g_CurrentStan);StanFileFree(&unlinked);FreeView(&view);
}
static void BisectWalk(const StanFile *s)
{
    for(int z=6;z<=14;z+=8)for(int direction=0;direction<2;direction++) {
        float scale=s->levelscale,from[3]={(direction?30:10)/scale,0,z/scale};
        DWORD tile=StanResolvePadTile(s,"",from);
        if(tile==STAN_TILE_NONE)fprintf(stderr,"BisectWalk failed: tiles=%u firstpoints=%u z=%d direction=%d\n",s->tilecount,s->tiles[0].pointcount,z,direction);
        assert(tile!=STAN_TILE_NONE);
        assert(StanWalkTiles(s,&tile,from[0],from[2],(direction?10:30)/scale,from[2]));
        assert(s->tiles[tile].room==(direction?1:2));
        float height;assert(StanGetTileHeight(s,tile,(direction?10:30)/scale,from[2],&height) && height==0);
    }
}
static void BisectPersist(const StanFile *s,const char *dir)
{
    StanFile loaded={0};char path[MAX_PATH];GEditorProject project={0};DWORD size;
    Require(StanSaveProjectFile(dir,s,&why),why);
    Require(StanLoadProjectFile(dir,s->name,s->levelscale,&loaded,&why),why);
    assert(loaded.tilecount==s->tilecount && loaded.size==s->size);
    /* Appended children are regrouped by room in the saved copy. Compare each
     * record's geometry/flags and destination identities after relocation. */
    for(DWORD t=0;t<s->tilecount;t++) {
        const StanTile *a=&s->tiles[t],*b=NULL;
        for(DWORD u=0;u<loaded.tilecount;u++)if(loaded.tiles[u].id==a->id && loaded.tiles[u].room==a->room
            && loaded.tiles[u].pointcount==a->pointcount) {
            BOOL same=TRUE;
            for(unsigned int p=0;p<a->pointcount;p++)if(memcmp(s->data+a->sourceoffset+8+p*8,loaded.data+loaded.tiles[u].sourceoffset+8+p*8,6))same=FALSE;
            if(same) { b=&loaded.tiles[u];break; }
        }
        assert(b && a->red==b->red && a->green==b->green && a->blue==b->blue && a->special==b->special);
        for(unsigned int p=0;p<a->pointcount;p++) {
            DWORD x=StanLinkedTile(s,a->points[p].link),y=StanLinkedTile(&loaded,b->points[p].link);
            assert((x==STAN_TILE_NONE)==(y==STAN_TILE_NONE));
            if(x!=STAN_TILE_NONE)assert(s->tiles[x].id==loaded.tiles[y].id && s->tiles[x].room==loaded.tiles[y].room);
            else assert(a->points[p].link==b->points[p].link);
        }
    }
    strcpy(project.dir,dir);assert(RomExportProjectResourcePath(&project,s->name,path,sizeof(path))==1);
    unsigned char *resource=RomExportReadResource(path,s->name,&size,&why);
    assert(resource && size==loaded.size && !memcmp(resource,loaded.data,size));free(resource);
    if(!strcmp(s->name,"Tbg_topology_test_stanZ"))BisectWalk(&loaded);
    StanFileFree(&loaded);
}
static void Bisect(const StanFile *source,const char *dir)
{
    StanFile s={0},tri={0};StanEdgeRef out;
    for(int f=0;f<16;f++) {
        Require(StanFileClone(source,&s,&why),why);failafter=f;
        BOOL ok=StanBisectEdge(&s,&(StanEdgeRef){0,2},&out,&why);failafter=-1;
        if(!ok) { Same(source,&s); }
        StanFileFree(&s);if(ok)break;assert(f<15);
    }
    for(int side=0;side<2;side++) {
        Require(StanFileClone(source,&s,&why),why);
        Require(StanBisectEdge(&s,&(StanEdgeRef){side,side?0:2},&out,&why),why);
        assert(s.tilecount==6 && out.tile==(DWORD)side && out.point==0);
        assert(s.tiles[0].pointcount==4 && s.tiles[1].pointcount==4 && s.tiles[4].pointcount==3 && s.tiles[5].pointcount==3);
        assert(s.tiles[0].points[1].x==80 && s.tiles[0].points[1].z==40);
        assert(s.tiles[2].points[0].link==0xf && s.tiles[0].editorid==source->tiles[0].editorid);
        assert(StanLinkedTile(&s,s.tiles[0].points[1].link)==4 && StanLinkedTile(&s,s.tiles[4].points[2].link)==0);
        assert(StanLinkedTile(&s,s.tiles[3].points[0].link)==1);
        assert((Get(s.data+8)&0xffffff)==s.tiles[2].sourceoffset && (Get(s.data+12)&0xffffff)==s.tiles[3].sourceoffset);
        assert(!memcmp(s.data+s.size-32,source->data+source->size-32,32));
        BisectWalk(&s);BisectPersist(&s,dir);StanFileFree(&s);
    }
    /* Turn the adjacent quads into triangles, keeping their shared boundary. */
    Require(StanFileClone(source,&tri,&why),why);
    Require(StanMergeVertices(&tri,(StanPointRef[]){{0,0},{0,1}},2,NULL,&why),why);
    /* Leave the right neighbor's outer link alone: a triangle boundary test on
     * the left still exercises a mixed triangle/quad shared subdivision. */
    Require(StanFileClone(&tri,&s,&why),why);
    Require(StanBisectEdge(&s,&(StanEdgeRef){0,1},&out,&why),why);
    assert(s.tiles[0].pointcount==3 && s.tiles[4].pointcount==3 && s.tilecount==6);
    BisectPersist(&s,dir);StanFileFree(&s);StanFileFree(&tri);
    /* Each triangle edge, including wraparound, gives two proper triangles. */
    Require(StanFileClone(source,&tri,&why),why);
    Require(StanMergeVertices(&tri,(StanPointRef[]){{0,0},{0,1}},2,NULL,&why),why);
    for(unsigned int c=0;c<3;c++) {
        Require(StanFileClone(&tri,&s,&why),why);
        Require(StanBisectEdge(&s,&(StanEdgeRef){0,c},&out,&why),why);
        assert(s.tiles[0].pointcount==3 && s.tiles[4].pointcount==3);
        BisectWalk(&s);StanFileFree(&s);
    }
    StanFileFree(&tri);
    /* Boundary/wraparound edge, reserved links and p/q naming semantics. */
    Require(StanFileClone(source,&s,&why),why);
    s.tiles[2].id|=0x800000;Put(s.data+s.tiles[2].sourceoffset,s.tiles[2].id<<8|s.tiles[2].room);
    Require(StanBisectEdge(&s,&(StanEdgeRef){0,3},&out,&why),why);
    assert(s.tilecount==5 && !(s.tiles[4].id&0x800000) && (s.tiles[4].id&255)==(source->tiles[0].id&255));
    BisectWalk(&s);BisectPersist(&s,dir);StanFileFree(&s);
    Require(StanFileClone(source,&s,&why),why);
    s.tiles[2].id|=0x800000;Put(s.data+s.tiles[2].sourceoffset,s.tiles[2].id<<8|s.tiles[2].room);
    Require(StanBisectEdge(&s,&(StanEdgeRef){2,0},&out,&why),why);assert(s.tiles[4].id&0x800000);StanFileFree(&s);
    /* One-way connections preserve their authored direction across both halves. */
    Require(StanFileClone(source,&s,&why),why);SetLink(&s,0,2,0);
    Require(StanBisectEdge(&s,&(StanEdgeRef){0,2},&out,&why),why);
    assert(!s.tiles[0].points[0].link && !s.tiles[4].points[0].link);
    assert(StanLinkedTile(&s,s.tiles[1].points[0].link)==4 && StanLinkedTile(&s,s.tiles[5].points[0].link)==0);StanFileFree(&s);
    /* Stale/malformed input must not change native bytes. */
    Require(StanFileClone(source,&s,&why),why);
    assert(!StanBisectEdge(&s,&(StanEdgeRef){99,0},&out,&why));Same(source,&s);
    assert(!StanBisectEdge(&s,&(StanEdgeRef){0,4},&out,&why));Same(source,&s);StanFileFree(&s);
    Require(StanFileClone(source,&s,&why),why);SetLink(&s,3,0,0x10);
    Require(StanFileClone(&s,&tri,&why),why);
    assert(!StanBisectEdge(&s,&(StanEdgeRef){0,2},&out,&why));Same(&s,&tri);StanFileFree(&tri);StanFileFree(&s);
    Require(StanFileClone(source,&s,&why),why);
    s.tiles[0].id=0x7fff00;Put(s.data+s.tiles[0].sourceoffset,s.tiles[0].id<<8|s.tiles[0].room);
    Require(StanFileClone(&s,&tri,&why),why);
    assert(!StanBisectEdge(&s,&(StanEdgeRef){0,2},&out,&why));Same(&s,&tri);StanFileFree(&tri);StanFileFree(&s);
    Require(StanFileClone(source,&s,&why),why);
    Put16(s.data+s.tiles[2].sourceoffset+8+8+4,1);s.tiles[2].points[1].z=4;
    Require(StanFileClone(&s,&tri,&why),why);
    assert(!StanBisectEdge(&s,&(StanEdgeRef){2,0},&out,&why));Same(&s,&tri);StanFileFree(&tri);StanFileFree(&s);
    /* Actual controller and viewport: topology reload, selected half and undo. */
    Require(StanFileClone(source,&g_CurrentStan,&why),why);
    view=(ViewportState){.showstan=TRUE,.stanopacity=44,.tool=EDITOR_TOOL_EDGE_SELECT};
    assert(ViewportSetStanTiles(&view,&g_CurrentStan));
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    for(int f=0;f<3;f++) {
        assert(ViewportSelectStanEdge(&view,&(StanEdgeRef){0,2}));
        ULONGLONG revision=g_EditHistory.nextrevision;
        failrebuild=f==0;if(f==1)g_EditHistory.nextrevision=0;if(f==2)failafter=0;
        assert(!GEditorBisectSelectedEdge(NULL));failafter=-1;g_EditHistory.nextrevision=revision;
        Same(source,&g_CurrentStan);assert(!g_EditHistory.undocount);
    }
    assert(ViewportSelectStanEdge(&view,&(StanEdgeRef){0,2}));
    assert(GEditorBisectSelectedEdge(NULL) && g_EditHistory.undocount==1 && g_CurrentStan.tilecount==6);
    assert(ViewportGetSelectedStanEdge(&view,&out) && out.tile==0 && out.point==0);BisectWalk(&g_CurrentStan);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(source,&g_CurrentStan);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));BisectWalk(&g_CurrentStan);
    EditHistoryFree(&g_EditHistory);StanFileFree(&g_CurrentStan);FreeView(&view);
}
int main(int argc,char **argv)
{
    assert(argc==2);StanFile source=Fixture(argv[1]),s={0};BOOL changed;
    const StanPointRef left[2]={{0,0},{0,1}},shared[2]={{0,2},{0,3}},wrap[2]={{0,3},{0,0}};
    for(int side=0;side<2;side++) {
        Require(StanFileClone(&source,&s,&why),why);StanEdgeRef edge={side,side ? 0 : 2};
        Require(StanSplitEdge(&s,&edge,&changed,&why),why);assert(changed);Walk(&s,FALSE);
        DWORD *map=StanBuildPointMap(&s,&why);assert(map && map[2]!=map[11] && map[3]!=map[10]);free(map);
        assert(StanLinkedTile(&s,s.tiles[1].points[2].link)==3 && s.tiles[2].points[0].link==0xf);
        assert(s.size==source.size);Preserved(&source,&s);Persist(argv[1],&s);
        s.dirty=FALSE;Require(StanSplitEdge(&s,&edge,&changed,&why),why);assert(!changed && !s.dirty);StanFileFree(&s);
    }
    /* A four-tile fan can still weld the endpoints around an unlinked edge.
     * Splitting must detach both incident tiles while retaining the other link. */
    Require(StanFileClone(&source,&s,&why),why);
    for(DWORD t=0;t<4;t++)for(DWORD p=0;p<4;p++)SetLink(&s,t,p,0);
    const short top[2][4][3]={{{0,0,20},{0,0,40},{20,0,40},{20,0,20}},
                              {{20,0,20},{20,0,40},{40,0,40},{40,0,20}}};
    for(int t=2;t<4;t++)for(int p=0;p<4;p++) {
        unsigned char *raw=s.data+s.tiles[t].sourceoffset+8+p*8;
        for(int a=0;a<3;a++)Put16(raw+a*2,top[t-2][p][a]);
        s.tiles[t].points[p]=(StanPoint){top[t-2][p][0]*4,top[t-2][p][1]*4,top[t-2][p][2]*4,0};
    }
    Require(StanLinkTiles(&s,0,1,&changed,&why),why);Require(StanLinkTiles(&s,0,2,&changed,&why),why);
    Require(StanLinkTiles(&s,1,3,&changed,&why),why);Require(StanLinkTiles(&s,2,3,&changed,&why),why);
    Require(StanSplitEdge(&s,&(StanEdgeRef){0,2},&changed,&why),why);assert(changed);
    DWORD *fanmap=StanBuildPointMap(&s,&why);assert(fanmap && fanmap[2]!=fanmap[11] && fanmap[2]!=fanmap[23]);
    assert(fanmap[23]==fanmap[30] && StanLinkedTile(&s,s.tiles[2].points[2].link)==3);
    free(fanmap);StanFileFree(&s);
    /* One-way links and all allocation failure points remain atomic. */
    Require(StanFileClone(&source,&s,&why),why);SetLink(&s,0,2,0);
    Require(StanSplitEdge(&s,&(StanEdgeRef){0,2},&changed,&why),why);assert(changed && !s.tiles[1].points[0].link);StanFileFree(&s);
    for(int op=0;op<2;op++)for(int f=0;f<12;f++) {
        Require(StanFileClone(&source,&s,&why),why);failafter=f;
        BOOL ok=op ? StanMergeVertices(&s,left,2,NULL,&why) : StanSplitEdge(&s,&(StanEdgeRef){0,2},&changed,&why);
        failafter=-1;if(!ok)Same(&source,&s);StanFileFree(&s);if(ok)break;assert(f<11);
    }
    const StanPointRef *selections[]={left,shared,wrap};
    for(int i=0;i<3;i++) {
        Require(StanFileClone(&source,&s,&why),why);StanPointRef merged;
        Require(StanMergeVertices(&s,selections[i],2,&merged,&why),why);
        assert(s.tiles[0].pointcount==3 && s.tiles[1].pointcount==(i==1 ? 3 : 4));
        assert(s.size==source.size-(i==1 ? 16 : 8));Preserved(&source,&s);Persist(argv[1],&s);
        if(i==0) { Walk(&s,TRUE);assert(s.tiles[0].points[merged.point].z==40); }
        Reject(&s,(StanPointRef[]){{0,0},{0,1}},2);StanFileFree(&s);
    }
    Reject(&source,(StanPointRef[]){{0,2},{1,1}},2); /* One shared identity, not two. */
    Reject(&source,(StanPointRef[]){{0,0},{0,2}},2);
    Reject(&source,(StanPointRef[]){{0,0},{0,1},{0,2}},3);
    Reject(&source,(StanPointRef[]){{0,0},{9,0}},2);
    Reject(&source,(StanPointRef[]){{0,0},{3,2}},2); /* Would fold a tile across itself. */
    /* A triangle on either side rejects the complete shared-point edit. */
    Require(StanFileClone(&source,&s,&why),why);Require(StanMergeVertices(&s,(StanPointRef[]){{1,2},{1,3}},2,NULL,&why),why);
    Reject(&s,shared,2);StanFileFree(&s);
    Visibility(&source,argv[1]);Controller(&source,argv[1]);EdgeLinkController(&source,argv[1]);Bisect(&source,argv[1]);StanFileFree(&source);
    /* Exercise real Depot IDs/links and save/reload after a merge. */
    Require(StanLoadProjectFile(argv[1],"Tbg_depo_all_p_stanZ",.21847887f,&source,&why),why);
    Require(StanFileClone(&source,&s,&why),why);
    Require(StanSplitEdge(&s,&(StanEdgeRef){1253,2},&changed,&why),why);assert(changed);Persist(argv[1],&s);StanFileFree(&s);
    BOOL found=FALSE;
    for(DWORD t=0;t<source.tilecount && !found;t++)if(source.tiles[t].pointcount>=4) {
        for(DWORD p=0;p<source.tiles[t].pointcount && !found;p++) {
            Require(StanFileClone(&source,&s,&why),why);
            StanPointRef refs[2]={{t,p},{t,(p+1)%source.tiles[t].pointcount}};
            if(StanMergeVertices(&s,refs,2,NULL,&why)) { Persist(argv[1],&s);found=TRUE; }
            else Same(&s,&source);
            StanFileFree(&s);
        }
    }
    assert(found);
    Require(StanFileClone(&source,&s,&why),why);StanEdgeRef half;
    Require(StanBisectEdge(&s,&(StanEdgeRef){1253,2},&half,&why),why);
    DWORD step=1217;float scale=s.levelscale;
    assert(StanWalkTiles(&s,&step,-165/scale,183/scale,-165/scale,186/scale));
    assert(StanWalkTiles(&s,&step,-165/scale,186/scale,-165/scale,183/scale));
    BisectPersist(&s,argv[1]);StanFileFree(&s);StanFileFree(&source);
    puts("PASS: Stan bisection/links/walking/export/native IDs, edge detachment, shared/wraparound merges, three-point and shape guards, native links/header/footer, allocation rollback, save/ROM bytes, controller undo/redo, temporary hiding/rebuild/picking and real Depot edits.");
}
