#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"
#include "bghistory.h"
#include "edittool.h"
#include "project.h"
#include "setupmeta.h"
void BgDocumentFree(BgDocument *doc) { abort(); }
BOOL SetupFileCompact(SetupFile *setup,const char **reason) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static int failafter=-1;
void *__real_malloc(size_t);void *__real_calloc(size_t,size_t);
static BOOL Fail(void) { if(failafter<0)return FALSE;if(!failafter)return TRUE;failafter--;return FALSE; }
void *__wrap_malloc(size_t n) { return Fail()?NULL:__real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { return Fail()?NULL:__real_calloc(n,s); }
static const char *why="";
static void Require(BOOL ok,const char *reason) { if(!ok){fprintf(stderr,"%s\n",reason);abort();} }
static char g_RomExportError[256];
#include "export.inc"
#include "fixture.inc"
static StanFile Gap(const char *dir)
{
    StanFile s=Fixture(dir);DWORD deleted;
    Require(StanDeleteTiles(&s,(DWORD[]){1},1,&deleted,&why),why);s.dirty=FALSE;return s;
}
static DWORD Find(const StanFile *s,DWORD id)
{ for(DWORD t=0;t<s->tilecount;t++)if(s->tiles[t].id==id)return t;assert(0);return 0; }
static DWORD Connections(const StanFile *s)
{
    DWORD directed=0;
    for(DWORD t=0;t<s->tilecount;t++)for(DWORD p=0;p<s->tiles[t].pointcount;p++)
    {
        DWORD other=StanLinkedTile(s,s->tiles[t].points[p].link);if(other==STAN_TILE_NONE)continue;
        DWORD matches=0;
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
    return directed;
}
static void Persist(const char *dir,const StanFile *s)
{
    StanFile loaded={0};GEditorProject project={0};char path[MAX_PATH];DWORD size;
    Require(StanSaveProjectFile(dir,s,&why),why);
    Require(StanLoadProjectFile(dir,s->name,s->levelscale,&loaded,&why),why);
    assert(s->tilecount==loaded.tilecount&&s->size==loaded.size&&Connections(s)==Connections(&loaded));
    for(DWORD t=0;t<s->tilecount;t++)
    {
        const StanTile *a=&s->tiles[t],*b=&loaded.tiles[Find(&loaded,a->id)];
        assert(a->pointcount==b->pointcount&&a->room==b->room&&a->special==b->special);
        assert(a->red==b->red&&a->green==b->green&&a->blue==b->blue&&!memcmp(a->extreme,b->extreme,3));
        for(DWORD p=0;p<a->pointcount;p++)
        {
            assert(!memcmp(s->data+a->sourceoffset+8+p*8,loaded.data+b->sourceoffset+8+p*8,6));
            DWORD x=StanLinkedTile(s,a->points[p].link),y=StanLinkedTile(&loaded,b->points[p].link);
            if(x==STAN_TILE_NONE)assert(a->points[p].link==b->points[p].link);
            else assert(y!=STAN_TILE_NONE&&s->tiles[x].id==loaded.tiles[y].id);
        }
    }
    assert(!memcmp(s->data+s->size-32,loaded.data+loaded.size-32,32));
    for(DWORD p=4;p<s->tiles[0].sourceoffset-4;p+=4)
    {
        DWORD before=Get(s->data+p),after=Get(loaded.data+p),id=0;
        for(DWORD t=0;t<s->tilecount;t++)if(s->tiles[t].sourceoffset==(before&0xffffff))id=s->tiles[t].id;
        assert(loaded.tiles[Find(&loaded,id)].sourceoffset==(after&0xffffff));
        assert((before&0xff000000)==(after&0xff000000));
    }
    strcpy(project.dir,dir);assert(RomExportProjectResourcePath(&project,s->name,path,sizeof(path))==1);
    unsigned char *raw=RomExportReadResource(path,s->name,&size,&why);
    assert(raw&&size==loaded.size&&!memcmp(raw,loaded.data,size));free(raw);StanFileFree(&loaded);
}
static void Reject(StanFile *s,const StanEdgeRef *edges,DWORD count,const double delta[3])
{
    StanFile before={0};StanEdgeRef out[2]={{777,888},{999,111}};DWORD made=99;
    StanPoint preview[12];double applied[3];
    Require(StanFileClone(s,&before,&why),why);
    assert(!StanPreviewEdgeExtrusion(s,edges,count,delta,preview,applied,&why)&&why[0]);Same(s,&before);
    assert(!StanExtrudeEdges(s,edges,count,delta,out,&made,&why)&&why[0]);Same(s,&before);
    assert(!made&&out[0].tile==777&&out[1].tile==999);StanFileFree(&before);
}
static void Native(const char *dir)
{
    const double offsets[][3]={{24,0,0},{-24,0,0},{0,0,24},{0,0,-24},{0,24,0},{0,-24,0},{24,16,0}};
    const unsigned points[]={2,0,1,3,2,2,2};
    for(unsigned mode=0;mode<7;mode++)
    {
        StanFile s=Gap(dir),before={0};StanEdgeRef edge={0,points[mode]},out;DWORD made;
        StanPoint preview[6];double applied[3];
        Require(StanFileClone(&s,&before,&why),why);
        Require(StanPreviewEdgeExtrusion(&s,&edge,1,offsets[mode],preview,applied,&why),why);Same(&s,&before);
        Require(StanExtrudeEdges(&s,&edge,1,offsets[mode],&out,&made,&why),why);
        assert(made==1&&out.tile==4&&out.point==1&&s.tilecount==5&&s.size==before.size+64&&s.dirty);
        for(DWORD t=3;t<5;t++)
        {
            const StanTile *tile=&s.tiles[t];assert(tile->pointcount==3&&tile->room==before.tiles[0].room);
            assert(tile->special==before.tiles[0].special&&tile->red==before.tiles[0].red);
            assert(tile->green==before.tiles[0].green&&tile->blue==before.tiles[0].blue);
            for(DWORD p=0;p<3;p++)
            {
                const StanPoint *v=&preview[(t-3)*3+p];
                assert(tile->points[p].x==v->x&&tile->points[p].y==v->y&&tile->points[p].z==v->z);
            }
            for(DWORD i=0;i<t;i++)assert(tile->id!=s.tiles[i].id&&tile->editorid!=s.tiles[i].editorid);
        }
        assert(Connections(&s)==4&&StanLinkedTile(&s,s.tiles[0].points[edge.point].link)==3);
        assert(!s.tiles[out.tile].points[out.point].link);
        for(DWORD t=0;t<3;t++)
        {
            StanTile expected=before.tiles[t];if(!t)expected.points[edge.point].link=s.tiles[t].points[edge.point].link;
            assert(!memcmp(&expected,&s.tiles[t],sizeof(expected)));
        }
        if(mode==0||mode==6)
        {
            DWORD t=0;assert(StanWalkTiles(&s,&t,40,24,96,24)&&t>=3);
            assert(StanWalkTiles(&s,&t,96,24,40,24)&&t==0);
            /* Continue from the selected outer edge; old tiles stay fixed. */
            StanEdgeRef next;Require(StanExtrudeEdges(&s,&out,1,offsets[mode],&next,&made,&why),why);
            assert(s.tilecount==7&&next.tile==6&&Connections(&s)==8);
        }
        Persist(dir,&s);StanFileFree(&s);StanFileFree(&before);
    }
    /* Adjacent selected boundaries create a connected strip across rooms. */
    StanFile s=Fixture(dir),before={0};StanEdgeRef edges[2]={{0,1},{1,1}},out[2];DWORD made;
    const double delta[3]={0,0,24};StanPoint preview[12];double applied[3];
    Require(StanFileClone(&s,&before,&why),why);
    for(int f=0;f<10;f++)
    {
        failafter=f;BOOL ok=StanExtrudeEdges(&s,edges,2,delta,out,&made,&why);failafter=-1;
        if(!ok){assert(!made);Same(&s,&before);}else break;assert(f<9);
    }
    assert(made==2&&s.tilecount==8&&Connections(&s)==14&&out[0].tile==5&&out[1].tile==7);
    assert(s.tiles[4].room==1&&s.tiles[6].room==2);
    assert(StanLinkedTile(&s,s.tiles[5].points[2].link)==6&&StanLinkedTile(&s,s.tiles[6].points[1].link)==5);
    DWORD t=5;assert(StanWalkTiles(&s,&t,72,88,88,88)&&t==6);assert(StanWalkTiles(&s,&t,88,88,72,88)&&t==5);
    Persist(dir,&s);StanFileFree(&s);
    Require(StanPreviewEdgeExtrusion(&before,edges,2,(double[]){0,.1,0},preview,applied,&why),why);
    Require(StanExtrudeEdges(&before,edges,2,(double[]){0,.1,0},out,&made,&why),why);assert(!made&&!applied[1]);
    Reject(&before,(StanEdgeRef[]){{0,2}},1,(double[]){24,0,0}); /* Already linked. */
    SetLink(&before,0,2,0);Reject(&before,(StanEdgeRef[]){{0,2}},1,(double[]){24,0,0}); /* Incoming only. */
    SetLink(&before,1,0,0);Reject(&before,(StanEdgeRef[]){{0,2}},1,(double[]){24,0,0}); /* Coincident but unlinked. */
    StanFileFree(&before);s=Gap(dir);
    Reject(&s,(StanEdgeRef[]){{0,2}},1,(double[]){-24,0,0}); /* Inward. */
    Reject(&s,(StanEdgeRef[]){{0,2}},1,(double[]){0,0,24}); /* Parallel. */
    Reject(&s,(StanEdgeRef[]){{0,2},{0,2}},2,(double[]){24,0,0});
    Reject(&s,(StanEdgeRef[]){{99,0}},1,(double[]){24,0,0});
    Reject(&s,(StanEdgeRef[]){{0,2}},1,(double[]){INFINITY,0,0});
    Reject(&s,(StanEdgeRef[]){{0,2}},1,(double[]){131068,0,0}); /* Native overflow. */
    s.tiles[0].id=0x7ffe00;Put(s.data+s.tiles[0].sourceoffset,s.tiles[0].id<<8|s.tiles[0].room);
    Reject(&s,(StanEdgeRef[]){{0,2}},1,(double[]){24,0,0});StanFileFree(&s);
    puts("PASS: Stan extrusion previews/native coordinates, six axis directions/ramps, reciprocal links, linked multi-edge strips, continued extrusion, traversal, grouped save/ROM data and atomic failures.");
}

#include "viewport_harness.inc"
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
typedef struct ViewportEdgeExtrusion {
    const BgDocumentEdgeRef *edges;DWORD count;double offset[3],applied[3];BgVertex *preview;
    const StanEdgeRef *stanedges;StanPoint *stanpreview;
} ViewportEdgeExtrusion;
static ViewportState view;static HWND g_Viewport=&view;
static BgDocument g_CurrentBgDocument;static SetupFile g_CurrentSetup;static StanFile g_CurrentStan;static EditHistory g_EditHistory;
static BOOL flying,transforming,failrebuild,failselection;
static unsigned errors;
static EditorTool ViewportGetTool(HWND hwnd){return view.tool;}
static BOOL ViewportIsFlying(HWND hwnd){return flying;}
static BOOL ViewportIsTransforming(HWND hwnd){return transforming;}
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **reason)
{if(failrebuild){failrebuild=FALSE;*reason="Rebuild failed.";return FALSE;}return ViewportSetStanTiles(&view,&g_CurrentStan);}
static void GEditorRefreshSelectionDetails(void){}
static void GEditorRefreshHistoryMenu(HWND hwnd){}
static void MessageBox(HWND hwnd,const char *reason,const char *title,int flags){assert(reason[0]);errors++;}
static void Remember(void)
{StanEdgeRef edge;assert(ViewportGetSelectedStanEdges(&view,&edge,1));Require(EditHistorySetSelection(&g_EditHistory,&edge,sizeof(edge),FALSE,&why),why);}
static void GEditorRestoreHistorySelection(HWND hwnd)
{assert(g_EditHistory.selection&&ViewportSelectStanEdges(&view,g_EditHistory.selection,1));}
static BOOL Select(HWND hwnd,const StanEdgeRef *edges,DWORD count)
{if(failselection){failselection=FALSE;return FALSE;}return ViewportSelectStanEdges(hwnd,edges,count);}
#define ViewportSelectStanEdges Select
#include "controller.inc"
#undef ViewportSelectStanEdges
static void Controller(const char *dir)
{
    StanFile before={0};StanEdgeRef edge={0,2},read;
    ViewportEdgeExtrusion request={.count=1,.offset={24,0,0},.stanedges=&edge};
    g_CurrentStan=Gap(dir);Require(StanFileClone(&g_CurrentStan,&before,&why),why);
    view=(ViewportState){.showstan=TRUE,.stanopacity=44,.tool=EDITOR_TOOL_EDGE_SELECT};
    assert(ViewportSetStanTiles(&view,&g_CurrentStan)&&ViewportSelectStanEdges(&view,&edge,1));
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);Remember();
    flying=TRUE;assert(!GEditorExtrudeStanEdges(NULL,&request));flying=FALSE;
    transforming=TRUE;assert(!GEditorExtrudeStanEdges(NULL,&request));transforming=FALSE;
    for(int f=0;f<4;f++)
    {
        ULONGLONG revision=g_EditHistory.nextrevision;
        failafter=f==0?0:-1;failrebuild=f==1;failselection=f==2;if(f==3)g_EditHistory.nextrevision=0;
        assert(!GEditorExtrudeStanEdges(NULL,&request));failafter=-1;g_EditHistory.nextrevision=revision;
        Same(&g_CurrentStan,&before);assert(!g_EditHistory.undocount);
        assert(ViewportGetSelectedStanEdges(&view,&read,1)&&read.tile==0&&read.point==2);
    }
    assert(errors==4);request.offset[0]=.1;
    assert(GEditorExtrudeStanEdges(NULL,&request)&&!g_EditHistory.undocount);Same(&g_CurrentStan,&before);
    request.offset[0]=24;
    assert(GEditorExtrudeStanEdges(NULL,&request)&&g_EditHistory.undocount==1&&g_CurrentStan.tilecount==5);
    assert(ViewportGetSelectedStanEdges(&view,&read,1)&&read.tile==4&&read.point==1);Remember();
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Extrude Stan Edges"));Persist(dir,&g_CurrentStan);
    EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    Require(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tilecount==3&&g_CurrentStan.dirty);assert(GEditorReloadCurrentObjectsAndViewport(&why));GEditorRestoreHistorySelection(NULL);
    assert(ViewportGetSelectedStanEdges(&view,&read,1)&&read.tile==0&&read.point==2);
    Require(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tilecount==5&&!g_CurrentStan.dirty);assert(GEditorReloadCurrentObjectsAndViewport(&why));GEditorRestoreHistorySelection(NULL);
    assert(ViewportGetSelectedStanEdges(&view,&read,1)&&read.tile==4&&read.point==1);
    assert(!ViewportSelectStanEdges(&view,(StanEdgeRef[]){{99,0}},1));assert(ViewportGetSelectedStanEdges(&view,&read,1)&&read.tile==4);
    EditHistoryFree(&g_EditHistory);StanFileFree(&before);StanFileFree(&g_CurrentStan);FreeView(&view);
    puts("PASS: production Stan extrusion controller, outer-edge selection, no-op/guard handling, rollback and undo/save/redo.");
}
int main(int argc,char **argv){assert(argc==2);Native(argv[1]);Controller(argv[1]);return 0;}
