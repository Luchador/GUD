#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgstan.h"
#include "bghistory.h"
#include "edittool.h"
static int failafter = -1;
void *__real_malloc(size_t); void *__real_calloc(size_t, size_t);
static BOOL Fail(void) { if (failafter < 0) return FALSE; if (!failafter) return TRUE; failafter--; return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return Fail() ? NULL : __real_calloc(n, s); }
BOOL SetupFileCompact(SetupFile *setup, const char **reason) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static const char *why = "";
static void Require(BOOL ok, const char *reason) { if (!ok) { fprintf(stderr, "%s\n", reason); abort(); } }
#include "fixture.inc"
static const BgFaceRef refs[] = {{1,1,0,0}, {2,2,1,0}};
static BgDocument Background(void)
{
    BgDocument bg = {.roomcount=2, .facecount=2, .levelscale=.5f};
    bg.rooms = calloc(3, sizeof(*bg.rooms)); assert(bg.rooms);
    for (DWORD r=1; r<=2; r++)
    {
        BgDocumentRoom *room=&bg.rooms[r];
        room->origin[0]=r==1?100:110; room->origin[1]=20; room->origin[2]=r==1?200:210;
        room->vertices=calloc(3,sizeof(*room->vertices)); room->vertexcount=3;
        room->faces=calloc(1,sizeof(*room->faces)); room->facecount=room->facecapacity=1;
        *room->faces=(BgDocumentFace){.id=r,.room=r,.layer=r-1,.vertexindices={0,1,2}};
        const short xz[2][3][2]={{{0,0},{0,20},{20,20}},{{-10,-10},{10,-10},{10,10}}};
        for (int p=0;p<3;p++)
        {
            room->vertices[p]=(BgDocumentVertex){.id=r*3+p,.room=r,
                .x=xz[r-1][p][0],.z=xz[r-1][p][1],.r=p*51,.g=20+p*10,.b=230+p*10,.a=p*100};
        }
    }
    return bg;
}
static void Native(const char *dir)
{
    StanFile s=Fixture(dir), before={0}; BgDocument bg=Background(); DWORD out[2]={999,999};
    Require(StanFileClone(&s,&before,&why),why);
    int failures=0;
    for (int f=0; f<10; f++)
    {
        failafter=f; BOOL ok=BgCreateStanFromFaces(&bg,refs,2,&s,out,&why); failafter=-1;
        if (ok) break;
        Same(&s,&before); assert(why[0] && out[0]==999 && out[1]==999); failures++;
    }
    assert(failures==5 && s.tilecount==6 && s.dirty && out[0]==4 && out[1]==5);
    assert(!memcmp(s.data,before.data,180) && !memcmp(s.tiles,before.tiles,4*sizeof(*s.tiles)));
    assert(Connections(&s)==Connections(&before)+2);
    for (int i=0;i<2;i++)
    {
        const StanTile *t=&s.tiles[4+i];
        assert(t->room==i+1 && !t->special && t->pointcount==3);
        assert(t->red==51 && t->green==34 && t->blue==238);
        assert(t->points[0].x==200 && t->points[0].y==40 && t->points[0].z==400);
        for (DWORD j=0;j<4+(DWORD)i;j++) assert(t->id!=s.tiles[j].id && t->editorid!=s.tiles[j].editorid);
    }
    DWORD tile=4; assert(StanWalkTiles(&s,&tile,205,425,235,415) && tile==5);
    assert(StanWalkTiles(&s,&tile,235,415,205,425) && tile==4);
    float height; assert(StanGetTileHeight(&s,4,205,425,&height) && height==40);
    char name[16]; assert(StanResolveSavedPadName(&s,"",(float[]){205,40,425},name));
    assert(!bg.dirty && bg.rooms[2].faces[0].vertexindices[1]==1); /* BG winding stays intact. */
    RoundTrip(&s);
    Require(StanSaveProjectFile(dir,&s,&why),why);
    StanFile loaded={0}; Require(StanLoadProjectFile(dir,s.name,s.levelscale,&loaded,&why),why);
    assert(loaded.tilecount==6); tile=Find(&loaded,s.tiles[4].id);
    assert(StanWalkTiles(&loaded,&tile,205,425,235,415) && loaded.tiles[tile].id==s.tiles[5].id);
    StanFileFree(&loaded); StanFileFree(&s); s=before; memset(&before,0,sizeof(before));
    Require(StanFileClone(&s,&before,&why),why);
    BgFaceRef invalid[2]={refs[0],refs[1]}; invalid[1].faceid=99;
    assert(!BgCreateStanFromFaces(&bg,invalid,2,&s,out,&why)); Same(&s,&before);
    assert(!BgCreateStanFromFaces(&bg,(BgFaceRef[]){refs[0],refs[0]},2,&s,out,&why)); Same(&s,&before);
    bg.rooms[2].origin[0]=1000000;
    assert(!BgCreateStanFromFaces(&bg,refs,2,&s,out,&why)); Same(&s,&before);
    bg.rooms[2].origin[0]=NAN;
    assert(!BgCreateStanFromFaces(&bg,refs,2,&s,out,&why)); Same(&s,&before);
    bg.rooms[2].origin[0]=110;
    bg.rooms[2].vertices[2]=bg.rooms[2].vertices[1];
    assert(!BgCreateStanFromFaces(&bg,refs,2,&s,out,&why)); Same(&s,&before);
    /* Valid world triangle collapses on the native grid: reject the whole batch. */
    StanTriangle collapsed={.room=1,.points={{0,0,0},{.2,0,0},{0,0,.2}}};
    assert(!StanCreateTriangles(&s,&collapsed,1,out,&why)); Same(&s,&before);
    s.tiles[0].editorid=UINT32_MAX;
    assert(!BgCreateStanFromFaces(&bg,refs,1,&s,out,&why)); s.tiles[0].editorid=before.tiles[0].editorid; Same(&s,&before);
    StanTriangle vertical={.room=3,.red=255,.points={{-131072,0,0},{-131072,0,4},{-131072,4,4}}};
    Require(StanCreateTriangles(&s,&vertical,1,out,&why),why); assert(out[0]==4 && s.tiles[4].red==255);
    RoundTrip(&s);
    StanFileFree(&s); StanFileFree(&before); BgDocumentFree(&bg);
    puts("PASS: scaled/translated primary and secondary BG, native floor winding, RGB mean, cross-room navigation, fresh IDs, pad names and save/reload; atomic range/area/allocation failures.");
}
static void Ambiguous(const char *dir)
{
    StanFile s=Fixture(dir); DWORD out[3];
    StanTriangle tris[3]={
        {.room=1,.points={{0,40,0},{0,40,40},{40,40,40}}},
        {.room=2,.points={{0,40,0},{40,40,40},{40,40,0}}},
        {.room=3,.points={{0,40,0},{40,40,40},{60,40,0}}}};
    DWORD connections=Connections(&s);
    Require(StanCreateTriangles(&s,tris,3,out,&why),why);
    assert(Connections(&s)==connections); /* Three owners: do not pick an arbitrary neighbor. */
    RoundTrip(&s); StanFileFree(&s);
    puts("PASS: nonmanifold boundaries remain unlinked and existing links remain intact.");
}

#include "viewport_harness.inc"
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
#define VISIBILITY_SHOW_STAN 4
static ViewportState view;
static HWND g_Viewport=&view, g_RightPanel;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static DWORD bgcount;
static BOOL flying,transforming,knife,snap,failrebuild,failselection;
static unsigned errors;
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportKnifeActive(HWND hwnd) { return knife; }
static BOOL ViewportGetVertexSnap(HWND hwnd) { return snap; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return bgcount; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd,BgFaceRef *out,DWORD count)
{ assert(count==bgcount && count<=2); memcpy(out,refs,count*sizeof(*out)); return TRUE; }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **reason)
{ if (failrebuild) { failrebuild=FALSE; *reason="Rebuild failed."; return FALSE; } return ViewportSetStanTiles(&view,&g_CurrentStan); }
static void RightPanelReveal(HWND hwnd,DWORD flags)
{ assert(flags==VISIBILITY_SHOW_STAN); view.showstan=TRUE; if (!view.stanopacity) view.stanopacity=44; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *reason,const char *title,int flags) { assert(reason[0]); errors++; }
typedef struct Selection { DWORD bgcount,stancount,tiles[2]; } Selection;
static void Remember(void)
{
    Selection selection={.bgcount=bgcount,.stancount=ViewportGetStanSelectionCount(&view,NULL)};
    if (selection.stancount) assert(ViewportGetSelectedStanTiles(&view,selection.tiles,selection.stancount));
    Require(EditHistorySetSelection(&g_EditHistory,&selection,sizeof(selection),FALSE,&why),why);
}
static void GEditorRestoreHistorySelection(HWND hwnd)
{
    const Selection *s=g_EditHistory.selection; bgcount=s->bgcount;
    ViewportClearStanSelection(&view);
    if (s->stancount) assert(ViewportSelectStanTiles(&view,s->tiles,s->stancount));
}
static BOOL Select(HWND hwnd,const DWORD *selected,DWORD count)
{
    if (failselection) { failselection=FALSE; return FALSE; }
    BOOL ok=ViewportSelectStanTiles(hwnd,selected,count); if (ok) bgcount=0; return ok;
}
#define ViewportSelectStanTiles Select
#include "controller.inc"
#undef ViewportSelectStanTiles
static void Controller(const char *dir)
{
    StanFile before={0};
    g_CurrentBgDocument=Background(); g_CurrentStan=Fixture(dir);
    Require(StanFileClone(&g_CurrentStan,&before,&why),why);
    view=(ViewportState){.tool=EDITOR_TOOL_FACE_SELECT}; bgcount=2;
    assert(ViewportSetStanTiles(&view,&g_CurrentStan));
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan); Remember();
    knife=TRUE; assert(!GEditorCanCreateStan()); knife=FALSE;
    snap=TRUE; assert(!GEditorCanCreateStan()); snap=FALSE;
    flying=TRUE; assert(!GEditorCanCreateStan()); flying=FALSE;
    transforming=TRUE; assert(!GEditorCanCreateStan()); transforming=FALSE;
    view.tool=EDITOR_TOOL_EDGE_SELECT; assert(!GEditorCanCreateStan()); view.tool=EDITOR_TOOL_FACE_SELECT;
    for (int f=0;f<4;f++)
    {
        ULONGLONG revision=g_EditHistory.nextrevision;
        failafter=f==0?0:-1; failrebuild=f==1; failselection=f==2; if (f==3) g_EditHistory.nextrevision=0;
        assert(!GEditorCreateStanFromFaces(NULL)); failafter=-1; g_EditHistory.nextrevision=revision;
        Same(&g_CurrentStan,&before); assert(!g_EditHistory.undocount && bgcount==2);
    }
    assert(errors==4); view.showstan=FALSE; view.stanopacity=0;
    assert(GEditorCreateStanFromFaces(NULL)); Remember();
    assert(g_CurrentStan.tilecount==6 && g_EditHistory.undocount==1 && !bgcount && !g_CurrentBgDocument.dirty);
    assert(view.showstan && view.stanopacity==44 && ViewportGetStanSelectionCount(&view,NULL)==2);
    assert(!GEditorCanCreateStan() && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Create Stan"));
    EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    Require(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tilecount==4 && g_CurrentStan.dirty);
    assert(GEditorReloadCurrentObjectsAndViewport(&why)); GEditorRestoreHistorySelection(NULL); assert(bgcount==2);
    Require(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why),why);
    assert(g_CurrentStan.tilecount==6 && !g_CurrentStan.dirty);
    assert(GEditorReloadCurrentObjectsAndViewport(&why)); GEditorRestoreHistorySelection(NULL);
    assert(!bgcount && ViewportGetStanSelectionCount(&view,NULL)==2);
    RoundTrip(&g_CurrentStan);
    EditHistoryFree(&g_EditHistory); StanFileFree(&g_CurrentStan); StanFileFree(&before);
    BgDocumentFree(&g_CurrentBgDocument); FreeView(&view);
    puts("PASS: controller gating, visibility/selection, one-step undo/redo, save dirty state and rebuild/selection/history failure rollback.");
}
int main(int argc,char **argv) { assert(argc==2); Native(argv[1]); Ambiguous(argv[1]); Controller(argv[1]); return 0; }
