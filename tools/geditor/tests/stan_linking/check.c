#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stanload.h"
#include "bghistory.h"
#include "edittool.h"
#include "project.h"
#include "setupmeta.h"
void BgDocumentFree(BgDocument *doc) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static int failafter=-1;
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t);
static BOOL Fail(void) { if(failafter<0)return FALSE;if(!failafter)return TRUE;failafter--;return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { return Fail() ? NULL : __real_calloc(n,s); }
static char g_RomExportError[256];
#include "export.inc"
#include "helpers.inc"

static StanFile Fixture(const char *dir)
{
    const short points[3][4][3]={
        {{-20,0,-20},{-20,0,20},{0,0,20},{0,0,-20}},
        {{0,0,-20},{0,0,20},{20,10,20},{20,10,-20}},
        {{100,0,-20},{100,0,20},{120,0,20},{120,0,-20}}};
    StanFile raw={0},stan={0};const char *why="";
    raw.size=164;raw.data=calloc(raw.size,1);assert(raw.data);strcpy(raw.name,"Tbg_link_test_stanZ");
    Put(raw.data+4,0x0e00000c);
    for(int t=0;t<3;t++) {
        unsigned char *p=raw.data+12+t*40;
        Put(p,((100u+t)<<16)|(t+1));Put16(p+4,0x0abc);Put16(p+6,0x4012);
        for(int v=0;v<4;v++)for(int axis=0;axis<3;axis++)Put16(p+8+v*8+axis*2,points[t][v][axis]);
    }
    memcpy(raw.data+140,"unstric",8);
    assert(StanSaveProjectFile(dir,&raw,&why));assert(StanLoadProjectFile(dir,raw.name,.2f,&stan,&why));free(raw.data);
    return stan;
}
static void SetLink(StanFile *stan,DWORD tile,DWORD edge,unsigned short link)
{ stan->tiles[tile].points[edge].link=link;Put16(stan->data+stan->tiles[tile].sourceoffset+8+edge*8+6,link); }
static void EditPoint(StanFile *stan,DWORD tile,DWORD point,const short xyz[3])
{
    unsigned char *raw=stan->data+stan->tiles[tile].sourceoffset+8+point*8;
    for(int a=0;a<3;a++)Put16(raw+a*2,xyz[a]);
    stan->tiles[tile].points[point].x=xyz[0]/stan->levelscale;
    stan->tiles[tile].points[point].y=xyz[1]/stan->levelscale;
    stan->tiles[tile].points[point].z=xyz[2]/stan->levelscale;
}
static void Walk(const StanFile *stan,BOOL linked)
{
    DWORD tile=0;float scale=stan->levelscale;
    assert(StanWalkTiles(stan,&tile,-10/scale,0,10/scale,0)==linked);
    if(linked)assert(tile==1);
    tile=1;assert(StanWalkTiles(stan,&tile,10/scale,0,-10/scale,0)==linked);
    if(linked)assert(tile==0);
}
static void UnchangedExceptLinks(const StanFile *before,const StanFile *after,DWORD a,DWORD ae,DWORD b,DWORD be)
{
    assert(before->size==after->size && before->tilecount==after->tilecount);
    DWORD ao=before->tiles[a].sourceoffset+8+ae*8+6,bo=before->tiles[b].sourceoffset+8+be*8+6;
    for(DWORD i=0;i<before->size;i++)if(i!=ao && i!=ao+1 && i!=bo && i!=bo+1)assert(before->data[i]==after->data[i]);
    for(DWORD i=0;i<before->tilecount;i++) {
        StanTile tile=after->tiles[i];
        if(i==a)tile.points[ae].link=before->tiles[i].points[ae].link;
        if(i==b)tile.points[be].link=before->tiles[i].points[be].link;
        assert(!memcmp(&tile,&before->tiles[i],sizeof(tile)));
    }
}
static void Reject(StanFile *stan,DWORD a,DWORD b)
{
    StanFile before={0};const char *why="";BOOL changed=TRUE;
    assert(StanFileClone(stan,&before,&why));
    assert(!StanLinkTiles(stan,a,b,&changed,&why) && !changed && why[0]);Same(stan,&before);StanFileFree(&before);
}

typedef void *HWND;
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
typedef struct ViewportState { EditorTool tool;BOOL showstan;int stanopacity,stancomponentcount;StanFile stan;unsigned char *stanselected; } ViewportState;
static ViewportState view;
static HWND g_Viewport=&view;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL flying,transforming,failrebuild;
static unsigned errors,restores,rebuilds;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
#include "selection.inc"
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **why)
{
    rebuilds++;view.stan=g_CurrentStan;
    if(failrebuild) { failrebuild=FALSE;memset(view.stanselected,0,3);*why="Rebuild failed.";return FALSE; }
    return TRUE;
}
static void GEditorRestoreHistorySelection(HWND hwnd)
{ restores++;assert(g_EditHistory.selectionsize==3);memcpy(view.stanselected,g_EditHistory.selection,3); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *why,const char *title,unsigned flags) { assert(why[0]);errors++; }
#include "controller.inc"
static void Controller(const StanFile *source,const char *dir)
{
    const char *why="";unsigned char selected[3]={1,1,0};
    assert(StanFileClone(source,&g_CurrentStan,&why));view=(ViewportState){EDITOR_TOOL_FACE_SELECT,TRUE,44,0,g_CurrentStan,selected};
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory,selected,3,FALSE,&why));
    for(int failure=0;failure<3;failure++) {
        ULONGLONG revision=g_EditHistory.nextrevision;
        failrebuild=failure==0;if(failure==1)g_EditHistory.nextrevision=0;if(failure==2)failafter=0;
        assert(!GEditorLinkSelectedStanTiles(NULL));failafter=-1;g_EditHistory.nextrevision=revision;
        Same(source,&g_CurrentStan);assert(selected[0] && selected[1] && !g_EditHistory.undocount);
    }
    assert(errors==3 && restores==2);
    flying=TRUE;assert(!GEditorLinkSelectedStanTiles(NULL));flying=FALSE;
    transforming=TRUE;assert(!GEditorLinkSelectedStanTiles(NULL));transforming=FALSE;
    view.tool=EDITOR_TOOL_EDGE_SELECT;assert(!GEditorLinkSelectedStanTiles(NULL));view.tool=EDITOR_TOOL_FACE_SELECT;
    view.showstan=FALSE;assert(!GEditorLinkSelectedStanTiles(NULL));view.showstan=TRUE;
    selected[1]=0;assert(!GEditorLinkSelectedStanTiles(NULL));selected[1]=1;selected[2]=1;
    assert(!GEditorLinkSelectedStanTiles(NULL));selected[2]=0;
    assert(GEditorLinkSelectedStanTiles(NULL));Walk(&g_CurrentStan,TRUE);
    assert(selected[0] && selected[1] && g_EditHistory.undocount==1);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Link Stan Tiles"));
    Persist(dir,&g_CurrentStan);EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    unsigned oldrebuilds=rebuilds;
    assert(GEditorLinkSelectedStanTiles(NULL) && rebuilds==oldrebuilds && !g_CurrentStan.dirty && g_EditHistory.undocount==1);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Walk(&g_CurrentStan,FALSE);
    GEditorReloadCurrentObjectsAndViewport(&why);GEditorRestoreHistorySelection(NULL);assert(selected[0] && selected[1]);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Walk(&g_CurrentStan,TRUE);
    assert(!g_CurrentStan.dirty);EditHistoryFree(&g_EditHistory);StanFileFree(&g_CurrentStan);
}
int main(int argc,char **argv)
{
    assert(argc==2);const char *why="";BOOL changed;StanFile original=Fixture(argv[1]),work={0};
    Walk(&original,FALSE);
    for(int reverse=0;reverse<2;reverse++) {
        assert(StanFileClone(&original,&work,&why));
        assert(StanLinkTiles(&work,reverse,1-reverse,&changed,&why) && changed && work.dirty);
        Walk(&work,TRUE);assert(StanLinkedTile(&work,work.tiles[0].points[2].link)==1);
        assert(StanLinkedTile(&work,work.tiles[1].points[0].link)==0);UnchangedExceptLinks(&original,&work,0,2,1,0);
        DWORD *map=StanBuildPointMap(&work,&why);assert(map && map[2]==map[STAN_TILE_MAX_POINTS+1] && map[3]==map[STAN_TILE_MAX_POINTS]);free(map);
        Persist(argv[1],&work);work.dirty=FALSE;
        assert(StanLinkTiles(&work,0,1,&changed,&why) && !changed && !work.dirty);
        SetLink(&work,1,0,0);assert(StanLinkTiles(&work,0,1,&changed,&why) && changed);Walk(&work,TRUE);StanFileFree(&work);
    }
    assert(StanFileClone(&original,&work,&why));
    Reject(&work,0,0);Reject(&work,0,9);Reject(&work,0,2);
    /* Height-separated, offset, partial, wrong-winding and ambiguous edges. */
    const short raised[4][3]={{0,5,-20},{0,5,20},{20,15,20},{20,15,-20}};
    for(int p=0;p<4;p++) { EditPoint(&work,1,p,raised[p]); }Reject(&work,0,1);StanFileFree(&work);
    assert(StanFileClone(&original,&work,&why));EditPoint(&work,1,0,(short[3]){1,0,-20});EditPoint(&work,1,1,(short[3]){1,0,20});Reject(&work,0,1);StanFileFree(&work);
    assert(StanFileClone(&original,&work,&why));EditPoint(&work,1,1,(short[3]){0,0,10});Reject(&work,0,1);StanFileFree(&work);
    assert(StanFileClone(&original,&work,&why));EditPoint(&work,1,1,(short[3]){20,10,-20});EditPoint(&work,1,3,(short[3]){0,0,20});Reject(&work,0,1);StanFileFree(&work);
    assert(StanFileClone(&original,&work,&why));const short opposite[4][3]={{-20,0,-20},{0,0,-20},{0,0,20},{-20,0,20}};
    for(int p=0;p<4;p++) { EditPoint(&work,1,p,opposite[p]); }Reject(&work,0,1);StanFileFree(&work);
    assert(StanFileClone(&original,&work,&why));SetLink(&work,1,0,0x10+(work.tiles[2].sourceoffset-work.tiles[0].sourceoffset)/8);Reject(&work,0,1);StanFileFree(&work);
    assert(StanFileClone(&original,&work,&why));work.tiles[1].sourceoffset=work.size;Reject(&work,0,1);StanFileFree(&work);
    Controller(&original,argv[1]);StanFileFree(&original);
    /* Repair the bottom-riser/floor connection at the actual Depot stairs.
     * Walking must cross both riser triangles as well as the edited link. */
    assert(StanLoadProjectFile(argv[1],"Tbg_depo_all_p_stanZ",.21847887f,&original,&why));
    assert(original.tiles[1253].id==0x03fe12 && original.tiles[1217].id==0x039a12);
    float scale=original.levelscale;DWORD tile=1217;
    assert(StanWalkTiles(&original,&tile,-165/scale,183/scale,-165/scale,186/scale) && tile==1220);
    assert(StanFileClone(&original,&work,&why));SetLink(&work,1253,2,0);SetLink(&work,1217,2,0);
    tile=1217;assert(!StanWalkTiles(&work,&tile,-165/scale,183/scale,-165/scale,186/scale));
    tile=1220;assert(!StanWalkTiles(&work,&tile,-165/scale,186/scale,-165/scale,183/scale));
    assert(StanLinkTiles(&work,1253,1217,&changed,&why) && changed);
    tile=1217;assert(StanWalkTiles(&work,&tile,-165/scale,183/scale,-165/scale,186/scale) && tile==1220);
    tile=1220;assert(StanWalkTiles(&work,&tile,-165/scale,186/scale,-165/scale,183/scale) && tile==1217);
    original.dirty=TRUE;Same(&original,&work);Persist(argv[1],&work);
    StanFileFree(&work);StanFileFree(&original);
    puts("PASS: reciprocal native links, boundary crossing both ways, stair risers, shared vertices, no-op/one-way repair, invalid/conflicting edges, save/ROM reads, history and real Depot stairs.");
}
