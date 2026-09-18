#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"
#include "bghistory.h"
#include "edittool.h"
#include "project.h"
#include "setupmeta.h"
void BgDocumentFree(BgDocument *doc) { abort(); }
BOOL SetupFileCompact(SetupFile *setup, const char **reason) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static int failafter=-1;
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t);
static BOOL Fail(void) { if(failafter<0)return FALSE; if(!failafter)return TRUE; failafter--;return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { return Fail() ? NULL : __real_calloc(n,s); }
static void Require(BOOL ok,const char *reason) { if(!ok) { fprintf(stderr,"%s\n",reason);abort(); } }
static const char *why="";
static char g_RomExportError[256];
#include "export.inc"
#include "fixture.inc"
static StanFile Gap(const char *dir)
{
    StanFile s=Fixture(dir); DWORD removed=1, deleted;
    Require(StanDeleteTiles(&s,&removed,1,&deleted,&why),why);s.dirty=FALSE;
    return s;
}
static DWORD Find(const StanFile *s,DWORD id)
{ for(DWORD t=0;t<s->tilecount;t++) if(s->tiles[t].id==id)return t;assert(0);return 0; }
static void Walk(const StanFile *s,BOOL connected)
{
    for(int z=6;z<=14;z+=8)for(int reverse=0;reverse<2;reverse++)
    {
        DWORD t=Find(s,reverse?103:100);
        const StanTile *last=&s->tiles[Find(s,103)];
        float lastx=(last->points[0].x+last->points[2].x)*.5f;
        float from=reverse?lastx:10/s->levelscale,to=reverse?10/s->levelscale:lastx;
        assert(StanWalkTiles(s,&t,from,z/s->levelscale,to,z/s->levelscale)==connected);
        if(connected)assert(s->tiles[t].id==(DWORD)(reverse?100:103));
    }
}
static void Connections(const StanFile *s)
{
    unsigned int directed=0;
    for(DWORD t=0;t<s->tilecount;t++)for(DWORD p=0;p<s->tiles[t].pointcount;p++)
    {
        DWORD other=StanLinkedTile(s,s->tiles[t].points[p].link);
        if(other==STAN_TILE_NONE)continue;
        unsigned int matches=0;
        for(DWORD q=0;q<s->tiles[other].pointcount;q++)
        {
            if(StanLinkedTile(s,s->tiles[other].points[q].link)!=t)continue;
            const unsigned char *a=s->data+s->tiles[t].sourceoffset+8+p*8;
            const unsigned char *b=s->data+s->tiles[t].sourceoffset+8+(p+1)%s->tiles[t].pointcount*8;
            const unsigned char *c=s->data+s->tiles[other].sourceoffset+8+q*8;
            const unsigned char *d=s->data+s->tiles[other].sourceoffset+8+(q+1)%s->tiles[other].pointcount*8;
            if(!memcmp(a,d,6)&&!memcmp(b,c,6))matches++;
        }
        assert(matches==1);directed++;
    }
    assert(directed==6); /* source -> bridge -> bridge -> source, both ways */
}
static void Persist(const char *dir,const StanFile *s,BOOL connected)
{
    StanFile loaded={0},again={0};GEditorProject project={0};char path[MAX_PATH];DWORD size;
    Require(StanSaveProjectFile(dir,s,&why),why);
    Require(StanLoadProjectFile(dir,s->name,s->levelscale,&loaded,&why),why);
    assert(loaded.tilecount==s->tilecount&&loaded.size==s->size);
    BOOL seen[STAN_MAX_ROOM+1]={0}; unsigned previous=STAN_MAX_ROOM+1;
    for(DWORD t=0;t<s->tilecount;t++)
    {
        const StanTile *a=&s->tiles[t],*b=&loaded.tiles[Find(&loaded,a->id)];
        assert(a->room==b->room && a->special==b->special && a->pointcount==b->pointcount);
        assert(a->red==b->red&&a->green==b->green&&a->blue==b->blue);
        assert(!memcmp(a->extreme,b->extreme,3));
        for(DWORD p=0;p<a->pointcount;p++)
        {
            assert(!memcmp(s->data+a->sourceoffset+8+p*8,loaded.data+b->sourceoffset+8+p*8,6));
            DWORD x=StanLinkedTile(s,a->points[p].link),y=StanLinkedTile(&loaded,b->points[p].link);
            if(x==STAN_TILE_NONE)assert(a->points[p].link==b->points[p].link);
            else assert(y!=STAN_TILE_NONE && s->tiles[x].id==loaded.tiles[y].id);
        }
        unsigned room=loaded.tiles[t].room;
        if(room!=previous) { assert(!seen[room]); }
        seen[room]=TRUE;previous=room;
    }
    /* Header tile pointers follow identities through room grouping. */
    for(DWORD p=4;p<s->tiles[0].sourceoffset-4;p+=4)
    {
        DWORD before=Get(s->data+p),after=Get(loaded.data+p),id=0;
        for(DWORD t=0;t<s->tilecount;t++)if(s->tiles[t].sourceoffset==(before&0xffffff))id=s->tiles[t].id;
        assert(loaded.tiles[Find(&loaded,id)].sourceoffset==(after&0xffffff));
        assert((before&0xff000000)==(after&0xff000000));
    }
    assert(!memcmp(s->data+s->size-32,loaded.data+loaded.size-32,32));
    Walk(&loaded,connected);if(connected)Connections(&loaded);
    strcpy(project.dir,dir);assert(RomExportProjectResourcePath(&project,s->name,path,sizeof(path))==1);
    unsigned char *resource=RomExportReadResource(path,s->name,&size,&why);
    assert(resource && size==loaded.size&&!memcmp(resource,loaded.data,size));free(resource);
    Require(StanSaveProjectFile(dir,&loaded,&why),why);
    Require(StanLoadProjectFile(dir,s->name,s->levelscale,&again,&why),why);
    assert(again.size==loaded.size&&!memcmp(again.data,loaded.data,again.size));
    StanFileFree(&again);StanFileFree(&loaded);
}
static void Reject(StanFile *s,const StanEdgeRef edges[2])
{
    StanFile before={0};DWORD out[2]={777,888};Require(StanFileClone(s,&before,&why),why);
    assert(!StanCanBridgeEdges(s,edges,&why));
    assert(!StanBridgeEdges(s,edges,out,&why)&&why[0]);Same(s,&before);
    assert(out[0]==777&&out[1]==888);StanFileFree(&before);
}
static void Native(const char *dir)
{
    const StanEdgeRef edges[2]={{0,2},{2,0}};
    StanFile s=Gap(dir),before={0};DWORD out[2];
    Require(StanFileClone(&s,&before,&why),why);Walk(&s,FALSE);
    assert(StanCanBridgeEdges(&s,edges,&why));Same(&s,&before);
    for(int f=0;f<2;f++)
    {
        failafter=f;assert(!StanBridgeEdges(&s,edges,out,&why));failafter=-1;Same(&s,&before);
    }
    Require(StanBridgeEdges(&s,edges,out,&why),why);
    assert(out[0]==3&&out[1]==4&&s.tilecount==5&&s.size==before.size+64&&s.dirty);
    for(DWORD t=3;t<5;t++)
    {
        assert(s.tiles[t].room==before.tiles[0].room && s.tiles[t].pointcount==3);
        assert(s.tiles[t].special==before.tiles[0].special);
        assert(!memcmp(s.data+s.tiles[t].sourceoffset+4,before.data+before.tiles[0].sourceoffset+4,2));
        for(DWORD i=0;i<t;i++)assert(s.tiles[t].id!=s.tiles[i].id&&s.tiles[t].editorid!=s.tiles[i].editorid);
        float height;assert(StanGetTileHeight(&s,t,120,40,&height)&&height==0);
    }
    /* Existing records retain coordinates, attributes, identities and offsets;
     * only the two selected boundary link words change. */
    for(DWORD t=0;t<3;t++)
    {
        StanTile expected=before.tiles[t];
        if(t==0)expected.points[2].link=s.tiles[t].points[2].link;
        if(t==2)expected.points[0].link=s.tiles[t].points[0].link;
        assert(!memcmp(&expected,&s.tiles[t],sizeof(expected)));
    }
    Walk(&s,TRUE);Connections(&s);Persist(dir,&s,TRUE);Reject(&s,edges);
    StanFileFree(&s);StanFileFree(&before);
    /* Reverse selection order chooses the other room; ramp corners need not
     * be coplanar because each new tile is a triangle. */
    s=Gap(dir);
    for(unsigned p=0;p<4;p++)
    { short y=p==1?12:8;Put16(s.data+s.tiles[2].sourceoffset+8+p*8+2,y);s.tiles[2].points[p].y=y/s.levelscale; }
    StanUpdateRepresentativeTriangle(&s,2);
    const StanEdgeRef reversed[2]={{2,0},{0,2}};
    Require(StanBridgeEdges(&s,reversed,out,&why),why);assert(s.tiles[3].room==4&&s.tiles[4].room==4);
    for(DWORD t=3;t<5;t++)
    {
        float center[3]={0},height;
        for(int p=0;p<3;p++) {center[0]+=s.tiles[t].points[p].x/3;center[1]+=s.tiles[t].points[p].y/3;center[2]+=s.tiles[t].points[p].z/3;}
        assert(StanGetTileHeight(&s,t,center[0],center[2],&height)&&fabsf(height-center[1])<.0001f);
    }
    Walk(&s,TRUE);Connections(&s);Persist(dir,&s,TRUE);StanFileFree(&s);
    /* Vertical stair risers are valid Stan, despite having zero XZ area. */
    s=Gap(dir);
    for(unsigned p=0;p<4;p++)
    {
        unsigned char *raw=s.data+s.tiles[2].sourceoffset+8+p*8;
        short x=p<2?20:40;Put16(raw,x);Put16(raw+2,8);
        s.tiles[2].points[p].x=x/s.levelscale;s.tiles[2].points[p].y=8/s.levelscale;
    }
    StanUpdateRepresentativeTriangle(&s,2);
    Require(StanBridgeEdges(&s,edges,out,&why),why);Walk(&s,TRUE);Connections(&s);Persist(dir,&s,TRUE);
    StanFileFree(&s);
    s=Gap(dir);
    Reject(&s,(StanEdgeRef[]){{0,2},{0,0}});Reject(&s,(StanEdgeRef[]){{0,2},{99,0}});
    Reject(&s,(StanEdgeRef[]){{0,2},{2,4}});Reject(&s,(StanEdgeRef[]){{0,0},{2,0}});
    /* Occupied edges, including one-way and unlinked coincident neighbors. */
    StanFile linked=Fixture(dir);Reject(&linked,(StanEdgeRef[]){{0,2},{3,0}});
    SetLink(&linked,0,2,0);Reject(&linked,(StanEdgeRef[]){{0,2},{3,0}});
    SetLink(&linked,1,0,0);Reject(&linked,(StanEdgeRef[]){{0,2},{3,0}});StanFileFree(&linked);
    DWORD id=s.tiles[0].id;
    s.tiles[0].id=0x7fff00;Put(s.data+s.tiles[0].sourceoffset,s.tiles[0].id<<8|s.tiles[0].room);Reject(&s,edges);
    s.tiles[0].id=id;Put(s.data+s.tiles[0].sourceoffset,id<<8|s.tiles[0].room);
    DWORD editor=s.tiles[0].editorid;s.tiles[0].editorid=0xffffffffu;Reject(&s,edges);s.tiles[0].editorid=editor;
    /* Degenerate footprint / coincident endpoints must not create bad Stan. */
    for(unsigned p=0;p<4;p++)
    {Put16(s.data+s.tiles[2].sourceoffset+8+p*8,20);s.tiles[2].points[p].x=20/s.levelscale;}
    Reject(&s,edges);StanFileFree(&s);
    puts("PASS: exact endpoints/attributes, source and diagonal links, bidirectional traversal, ramps/vertical risers, mixed rooms, grouped save/ROM data, unique IDs and atomic rejection/allocation failures.");
}

#include "viewport_harness.inc"
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
static ViewportState view;
static HWND g_Viewport=&view,g_ToolToolbar=(HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL flying,transforming,snap,failrebuild,failselection;
static unsigned errors,restores;
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportGetVertexSnap(HWND hwnd) { return snap; }
static void ViewportSetTool(HWND hwnd,EditorTool tool) { view.tool=tool;ViewportClearAllSelection(&view); }
static void ToolToolbarSetTool(HWND hwnd,EditorTool tool) { assert(tool==view.tool); }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **reason)
{
    if(failrebuild) {failrebuild=FALSE;*reason="Rebuild failed.";return FALSE;}
    return ViewportSetStanTiles(&view,&g_CurrentStan);
}
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *reason,const char *title,int flags) { assert(reason[0]);errors++; }
static void Edges(const StanEdgeRef edges[2])
{
    if(view.stancomponentcapacity<2)
    { free(view.stancomponents);view.stancomponents=calloc(2,sizeof(*view.stancomponents));assert(view.stancomponents);view.stancomponentcapacity=2; }
    ViewportClearAllSelection(&view);view.tool=EDITOR_TOOL_EDGE_SELECT;
    for(int e=0;e<2;e++)
    {
        DWORD t=edges[e].tile,p=edges[e].point;
        view.stancomponents[e].refs[0]=ViewportStanPointRef(&view,t,p);
        view.stancomponents[e].refs[1]=ViewportStanPointRef(&view,t,(p+1)%view.stan.tiles[t].pointcount);
    }
    view.stancomponentcount=2;
}
typedef struct Selection { EditorTool tool;StanEdgeRef edges[2];DWORD tiles[2]; } Selection;
static void Remember(void)
{
    Selection s={0};s.tool=view.tool;
    if(s.tool==EDITOR_TOOL_EDGE_SELECT)assert(ViewportGetSelectedStanEdges(&view,s.edges,2));
    else assert(ViewportGetSelectedStanTiles(&view,s.tiles,2));
    Require(EditHistorySetSelection(&g_EditHistory,&s,sizeof(s),FALSE,&why),why);
}
static void GEditorRestoreHistorySelection(HWND hwnd)
{
    restores++;const Selection *s=g_EditHistory.selection;assert(s);
    ViewportSetTool(&view,s->tool);
    if(s->tool==EDITOR_TOOL_EDGE_SELECT)Edges(s->edges);
    else assert(ViewportSelectStanTiles(&view,s->tiles,2));
}
static BOOL Select(HWND hwnd,const DWORD *tiles,DWORD count)
{ if(failselection){failselection=FALSE;return FALSE;}return ViewportSelectStanTiles(hwnd,tiles,count); }
#define ViewportSelectStanTiles Select
#include "controller.inc"
#undef ViewportSelectStanTiles
static void Controller(const char *dir)
{
    StanFile before={0};const StanEdgeRef edges[2]={{0,2},{2,0}};StanEdgeRef read[2];DWORD selected[2];
    g_CurrentStan=Gap(dir);Require(StanFileClone(&g_CurrentStan,&before,&why),why);
    view=(ViewportState){.showstan=TRUE,.stanopacity=44,.tool=EDITOR_TOOL_EDGE_SELECT};
    assert(ViewportSetStanTiles(&view,&g_CurrentStan));Edges(edges);
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);Remember();
    assert(ViewportGetSelectedStanEdges(&view,read,2)&&!memcmp(read,edges,sizeof(read)));
    assert(!ViewportGetSelectedStanEdges(&view,read,1));
    view.showstan=FALSE;assert(!ViewportGetSelectedStanEdges(&view,read,2));view.showstan=TRUE;
    view.stanopacity=0;assert(!ViewportGetSelectedStanEdges(&view,read,2));view.stanopacity=44;
    view.componentcount=1;assert(!ViewportGetSelectedStanEdges(&view,read,2));view.componentcount=0;
    view.stanhiddenids=malloc(sizeof(DWORD));*view.stanhiddenids=view.stan.tiles[0].editorid;view.stanhiddencount=1;
    assert(!ViewportGetSelectedStanEdges(&view,read,2));view.stanhiddencount=0;
    flying=TRUE;assert(!GEditorBridgeSelectedStanEdges(NULL));flying=FALSE;
    transforming=TRUE;assert(!GEditorBridgeSelectedStanEdges(NULL));transforming=FALSE;
    snap=TRUE;assert(!GEditorBridgeSelectedStanEdges(NULL));snap=FALSE;
    view.tool=EDITOR_TOOL_FACE_SELECT;assert(!GEditorBridgeSelectedStanEdges(NULL));view.tool=EDITOR_TOOL_EDGE_SELECT;
    for(int f=0;f<4;f++)
    {
        ULONGLONG revision=g_EditHistory.nextrevision;
        failafter=f==0?0:-1;failrebuild=f==1;failselection=f==2;
        if(f==3)g_EditHistory.nextrevision=0;
        assert(!GEditorBridgeSelectedStanEdges(NULL));failafter=-1;g_EditHistory.nextrevision=revision;
        Same(&g_CurrentStan,&before);assert(!g_EditHistory.undocount);
        assert(view.tool==EDITOR_TOOL_EDGE_SELECT&&ViewportGetSelectedStanEdges(&view,read,2));
        assert(!memcmp(read,edges,sizeof(read)));
    }
    assert(errors==4&&restores==3);
    assert(GEditorBridgeSelectedStanEdges(NULL)&&g_EditHistory.undocount==1);
    assert(view.tool==EDITOR_TOOL_FACE_SELECT&&ViewportGetSelectedStanTiles(&view,selected,2));
    assert(selected[0]==3&&selected[1]==4&&!view.stancomponentcount);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Bridge Stan Edges"));Remember();
    Walk(&g_CurrentStan,TRUE);Persist(dir,&g_CurrentStan,TRUE);EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    Require(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tilecount==3&&g_CurrentStan.dirty);Walk(&g_CurrentStan,FALSE);
    assert(GEditorReloadCurrentObjectsAndViewport(&why));GEditorRestoreHistorySelection(NULL);
    assert(ViewportGetSelectedStanEdges(&view,read,2)&&!memcmp(read,edges,sizeof(read)));Persist(dir,&g_CurrentStan,FALSE);
    EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    Require(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tilecount==5&&g_CurrentStan.dirty);Walk(&g_CurrentStan,TRUE);
    assert(GEditorReloadCurrentObjectsAndViewport(&why));GEditorRestoreHistorySelection(NULL);
    assert(ViewportGetSelectedStanTiles(&view,selected,2)&&selected[0]==3&&selected[1]==4);Persist(dir,&g_CurrentStan,TRUE);
    /* Invalid reselection cannot partially clear the valid new tile selection. */
    assert(!ViewportSelectStanTiles(&view,(DWORD[]){0,99},2));
    assert(!ViewportSelectStanTiles(&view,(DWORD[]){0,0},2));
    assert(ViewportGetSelectedStanTiles(&view,selected,2)&&selected[0]==3&&selected[1]==4);
    EditHistoryFree(&g_EditHistory);StanFileFree(&g_CurrentStan);StanFileFree(&before);FreeView(&view);
    puts("PASS: production edge selection, visibility/mode guards, controller rollback, new tile selection and undo/save/redo with original edge selection.");
}
int main(int argc,char **argv) { assert(argc==2);Native(argv[1]);Controller(argv[1]);return 0; }
