#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
BOOL SetupFileCompact(SetupFile *setup, const char **why) { (void)setup; (void)why; abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static BOOL failallocation;
static void *TestMalloc(size_t size) { return failallocation ? NULL : malloc(size); }
#define malloc TestMalloc
#include "bgportal.c"
#undef malloc
#include "fixture.inc"

static void NativeEdits(void)
{
    BgFile source=Fixture(), saved={0}, undone={0}, redone={0};
    BgDocument doc={0}, original={0}, reloaded={0}; EditHistory history={0}; EditHistoryTransaction tx={0};
    SetupFile setup={0}; StanFile stan={0}; const char *why=""; DWORD moved, table;
    /* Values whose world->native round trip is lossy must still undo exactly. */
    Put(source.data+192+4,0xc1133333); Put(source.data+192+8,0xc1524925);
    assert(BgDocumentLoad(source.data,source.size,.3f,&doc,&why));
    assert(BgDocumentClone(&doc,&original,&why));
    EditHistoryReset(&history,&doc,&setup,&stan);
    BgPortalPointRef refs[]={{0,0},{1,0},{0,0},{0,1}}; double offset[]={12.5,-3.25,7.125};
    assert(EditHistoryBeginBgEdit(&history,&doc,"Move Portal Edge",&tx,&why));
    assert(BgDocumentTranslatePortalPoints(&doc,refs,4,offset,&moved,&why) && moved==4);
    for(int point=0;point<4;point++)
    {
        assert(!memcmp(&doc.portals.portals[0].points[point],&doc.portals.portals[1].points[point],sizeof(BgPortalPoint)));
        if(point<2)assert(fabs(doc.portals.portals[0].points[point].x-original.portals.portals[0].points[point].x-12.5)<1e-5);
        else assert(!memcmp(&doc.portals.portals[0].points[point],&original.portals.portals[0].points[point],sizeof(BgPortalPoint)));
    }
    assert(!memcmp(&doc.portals.portals[2],&original.portals.portals[2],sizeof(BgPortal)));
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&tx,&why));
    assert(BgDocumentCompile(&doc,&source,&saved,&why) && BgFileValidateVertexBatches(&saved,&why));
    table=Get(source.data+8)&0xffffff;
    assert(!memcmp(source.data+table,saved.data+table,32)); /* links/flags/margins/pointers */
    assert(!memcmp(source.data+416,saved.data+416,508-416)); /* visibility bytes */
    assert(!memcmp(source.data+256,saved.data+256,52)); /* unrelated polygon */
    assert(BgDocumentLoad(saved.data,saved.size,.3f,&reloaded,&why));
    assert(!memcmp(doc.portals.portals,reloaded.portals.portals,3*sizeof(BgPortal)));
    BgDocumentFree(&reloaded);EditHistoryMarkBgSaved(&history,&doc);
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,NULL,&why));
    assert(BgDocumentCompile(&doc,&saved,&undone,&why));
    assert(!memcmp(source.data+192,undone.data+192,52)); /* exact original coordinate bits */
    EditHistoryMarkBgSaved(&history,&doc);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,NULL,&why));
    assert(BgDocumentCompile(&doc,&undone,&redone,&why));
    assert(saved.size==redone.size && !memcmp(saved.data,redone.data,saved.size));
    /* Invalid requests/allocation failure leave every portal and dirty state alone. */
    BgPortal before[3];memcpy(before,doc.portals.portals,sizeof(before));BOOL dirty=doc.dirty;
    failallocation=TRUE;assert(!BgDocumentTranslatePortalPoints(&doc,refs,4,offset,&moved,&why));failallocation=FALSE;
    refs[1].point=8;assert(!BgDocumentTranslatePortalPoints(&doc,refs,4,offset,&moved,&why));refs[1].point=0;
    offset[0]=NAN;assert(!BgDocumentTranslatePortalPoints(&doc,refs,4,offset,&moved,&why));
    offset[0]=DBL_MAX;assert(!BgDocumentTranslatePortalPoints(&doc,refs,4,offset,&moved,&why));
    assert(!memcmp(before,doc.portals.portals,sizeof(before)) && dirty==doc.dirty);
    offset[0]=offset[1]=offset[2]=0;assert(BgDocumentTranslatePortalPoints(&doc,refs,4,offset,&moved,&why) && !moved);
    /* A new portal can be reshaped, saved and loaded with its new coordinates. */
    BgPortalPlacement placement={1,3,{50,60,-70},300,300,BG_PORTAL_XY};DWORD index;
    assert(BgDocumentAddPortal(&doc,&placement,&index,&why));
    BgPortalPointRef corner={index,2};offset[0]=37.25;offset[1]=-18.75;
    assert(BgDocumentTranslatePortalPoints(&doc,&corner,1,offset,&moved,&why) && moved==1);
    BgFile added={0};assert(BgDocumentCompile(&doc,&saved,&added,&why));
    assert(BgDocumentLoad(added.data,added.size,.3f,&reloaded,&why));
    assert(!memcmp(doc.portals.portals[index].nativepoints,reloaded.portals.portals[index].nativepoints,sizeof(BgPortalPoint)*4));
    BgFileFree(&added);BgDocumentFree(&reloaded);EditHistoryFree(&history);BgDocumentFree(&doc);BgDocumentFree(&original);
    BgFileFree(&source);BgFileFree(&saved);BgFileFree(&undone);BgFileFree(&redone);
    for(int points=3;points<=BG_PORTAL_MAX_POINTS;points++)
    {
        source=Fixture();source.data[256]=(unsigned char)points;
        for(int point=0;point<points;point++)
        {
            double angle=point*6.283185307179586/points;
            Float(source.data+260+point*12,cos(angle)*10);
            Float(source.data+264+point*12,sin(angle)*10);
            Float(source.data+268+point*12,20);
        }
        assert(BgDocumentLoad(source.data,source.size,.3f,&doc,&why));
        BgPortalPointRef last={2,points-1};double delta[]={1.25,0,0};
        assert(BgDocumentTranslatePortalPoints(&doc,&last,1,delta,&moved,&why) && moved==1);
        assert(BgDocumentCompile(&doc,&source,&saved,&why));
        assert(BgDocumentLoad(saved.data,saved.size,.3f,&reloaded,&why));
        assert(!memcmp(&doc.portals.portals[2],&reloaded.portals.portals[2],sizeof(BgPortal)));
        assert(!memcmp(source.data+356,saved.data+356,508-356));
        BgDocumentFree(&reloaded);BgDocumentFree(&doc);BgFileFree(&saved);BgFileFree(&source);
    }
    puts("PASS: existing/new portal point edits, shared polygons, atomic rejection, exact native save/undo/save/redo, metadata preservation.");
}

typedef void *HWND;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef int GLsizei;
typedef intptr_t LPARAM;
typedef struct { int left,top,right,bottom; } RECT;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define VIEWPORT_WM_SELECTION_CHANGED 1
#define MB_ICONERROR 1
#define GEDITOR_TITLE "GEditor"
#include "types.inc"
typedef struct ViewportState {
    EditorTool tool;BOOL showportals,flying,markerselected;int componentcount;
    float posx,posy,posz,yaw,pitch;int width,height,dragaxis;
    BgPortalFile portals;DWORD selectedportal;unsigned char portalselection[BG_MAX_PORTALS];
    Vertex *portalfill,*portaledges;GLsizei portalfillcount,portaledgecount;
    float (*dragvertices)[3];unsigned char *dragmask;
    SetupPadRef selectedpad;
} ViewportState;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static BOOL GetClientRect(HWND hwnd,RECT *rect) { ViewportState *s=hwnd;*rect=(RECT){0,0,s->width,s->height};return TRUE; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static void InvalidateRect(HWND hwnd,const RECT *rect,BOOL erase) {}
static int SendMessage(HWND hwnd,unsigned msg,unsigned wparam,LPARAM lparam) { return TRUE; }
static void ViewportUpdateGizmo(ViewportState *s) {}
static void ViewportRefreshPadColors(ViewportState *s) {}
static void ViewportClearBgSelection(ViewportState *s) {}
static void ViewportClearObjectSelection(ViewportState *s) {}
static void ViewportClearStanSelection(ViewportState *s) {}
static void ViewportClearAllSelection(ViewportState *s)
{ memset(s->portalselection,0,sizeof(s->portalselection));s->selectedportal=BG_PORTAL_INDEX_NONE;s->componentcount=0; }
static double occluder=DBL_MAX;
static double ViewportSceneHitDistance(const ViewportState *s,const ViewportPickRay *ray) { return occluder; }
#include "viewport.inc"

static ViewportState viewport;
static HWND g_Viewport=&viewport;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static unsigned errors,restores;
static BOOL failcommit;
static EditorTool ViewportGetTool(HWND hwnd) { return ((ViewportState *)hwnd)->tool; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd) { restores++; }
static int MessageBox(HWND hwnd,const char *text,const char *title,int flags) { assert(text && *text);errors++;return 0; }
static BOOL Commit(EditHistory *h,BgDocument *b,SetupFile *s,StanFile *st,EditHistoryTransaction *tx,const char **why)
{ if(failcommit) { *why="commit failure";return FALSE; }return EditHistoryCommitEdit(h,b,s,st,tx,why); }
#define EditHistoryCommitEdit Commit
#include "controller.inc"
#undef EditHistoryCommitEdit

static void ViewportEdits(void)
{
    BgFile source=Fixture();const char *why="";DWORD count,index;BgPortalPointRef *refs;double position[3];
    assert(BgDocumentLoad(source.data,source.size,.5f,&g_CurrentBgDocument,&why));
    /* Put portals in front of a default (-Z-facing) editor camera. */
    for(int i=0;i<3;i++)for(int p=0;p<4;p++)
    { g_CurrentBgDocument.portals.portals[i].points[p].z=-100-(i==2?100:0);g_CurrentBgDocument.portals.portals[i].nativepoints[p].z=-50-(i==2?50:0); }
    viewport=(ViewportState){.tool=EDITOR_TOOL_VERTEX_SELECT,.width=640,.height=480,.showportals=TRUE,.dragaxis=-1,.selectedportal=BG_PORTAL_INDEX_NONE};
    ViewportSetPortals(&viewport,&g_CurrentBgDocument.portals);
    const BgPortalPoint *point=&viewport.portals.portals[0].points[0];Vertex v={.x=point->x,.y=point->y,.z=point->z};
    double screen[2];assert(ViewportProject(&viewport,&v,screen));
    assert(ViewportTryPickPortal(&viewport,&viewport,lround(screen[0]),lround(screen[1]),FALSE,FALSE));
    assert(viewport.selectedportal==0 && viewport.portalselection[0]==1 && ViewportGetPortalSelectionCount(&viewport)==1);
    occluder=10;assert(!ViewportFindPortalComponent(&viewport,lround(screen[0]),lround(screen[1]),&index,&count));occluder=DBL_MAX;
    /* Add a second corner then remove it; only the clicked native corner moves. */
    point=&viewport.portals.portals[0].points[1];v=(Vertex){.x=point->x,.y=point->y,.z=point->z};assert(ViewportProject(&viewport,&v,screen));
    assert(ViewportTryPickPortal(&viewport,&viewport,lround(screen[0]),lround(screen[1]),TRUE,FALSE));
    assert(viewport.portalselection[0]==3 && ViewportGetPortalSelectionCount(&viewport)==2);
    assert(ViewportTryPickPortal(&viewport,&viewport,lround(screen[0]),lround(screen[1]),FALSE,TRUE));
    assert(viewport.portalselection[0]==1);
    viewport.tool=EDITOR_TOOL_EDGE_SELECT;ViewportClearAllSelection(&viewport);
    assert(ViewportTryPickPortal(&viewport,&viewport,320,282,FALSE,FALSE)); /* lower perimeter near y=-10 */
    refs=ViewportGetMovePortalPoints(&viewport,&count);assert(refs && count==2 && refs[0].point==0 && refs[1].point==1);free(refs);
    assert(ViewportPortalSelectionPosition(&viewport,position,&count) && count==1 && position[0]==0 && position[1]==-10);
    assert(!ViewportFindPortalComponent(&viewport,320,240,&index,&count)); /* no triangulation diagonal */
    /* Box selection works through geometry, like BG component marquee selection. */
    viewport.tool=EDITOR_TOOL_VERTEX_SELECT;RECT box={270,280,370,295};
    assert(ViewportApplyPortalBox(&viewport,&box,FALSE,FALSE));assert(viewport.portalselection[0]==3);
    viewport.tool=EDITOR_TOOL_EDGE_SELECT;viewport.portalselection[0]=3;viewport.portalselection[2]=0;
    refs=ViewportGetMovePortalPoints(&viewport,&count);assert(refs && count==3);free(refs); /* shared corner dedup */
    viewport.dragaxis=0;viewport.dragvertices=calloc(3*BG_PORTAL_MAX_POINTS,sizeof(*viewport.dragvertices));viewport.dragmask=calloc(3*BG_PORTAL_MAX_POINTS,1);
    ViewportPreparePortalDrag(&viewport);BgPortal before[3];memcpy(before,viewport.portals.portals,sizeof(before));
    ViewportPreviewPortalDrag(&viewport,11);ViewportPreviewPortalDrag(&viewport,17);
    for(int i=0;i<3;i++)for(int p=0;p<4;p++)
    { assert(viewport.portals.portals[i].points[p].x==before[i].points[p].x+(i<2&&p<3?17:0)); }
    assert(viewport.portalfill[0].x==before[0].points[0].x+17);
    ViewportPreviewPortalDrag(&viewport,0);assert(!memcmp(before,viewport.portals.portals,sizeof(before)));
    free(viewport.dragvertices);free(viewport.dragmask);viewport.dragvertices=NULL;viewport.dragmask=NULL;viewport.dragaxis=-1;
    viewport.tool=EDITOR_TOOL_FACE_SELECT;ViewportClearAllSelection(&viewport);
    assert(ViewportTryPickPortal(&viewport,&viewport,320,240,FALSE,FALSE));assert(viewport.selectedportal==0);
    assert(ViewportTryPickPortal(&viewport,&viewport,320,240,FALSE,FALSE));assert(viewport.selectedportal==1); /* aliased table entries cycle */
    refs=ViewportGetMovePortalPoints(&viewport,&count);assert(refs && count==4);free(refs);
    viewport.portalselection[0]=1; /* both aliased faces still translate once */
    assert(ViewportGetPortalSelectionCount(&viewport)==1);
    viewport.portalselection[2]=1;refs=ViewportGetMovePortalPoints(&viewport,&count);assert(refs && count==8);free(refs);
    ViewportClearAllSelection(&viewport);viewport.tool=EDITOR_TOOL_VERTEX_SELECT;viewport.portalselection[0]=1;viewport.selectedportal=0;
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    double offset[]={2.25,3.5,0};
    failcommit=TRUE;assert(!GEditorTranslatePortals(NULL,offset,FALSE));failcommit=FALSE;
    assert(errors==1 && restores==1 && !g_EditHistory.undocount && !g_CurrentBgDocument.dirty);
    assert(!memcmp(before,g_CurrentBgDocument.portals.portals,sizeof(before)));
    assert(GEditorTranslatePortals(NULL,offset,TRUE) && g_EditHistory.undocount==1);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Snap Portal Vertex"));
    assert(g_CurrentBgDocument.portals.portals[0].points[0].x==before[0].points[0].x+2.25);
    assert(viewport.portals.portals[0].points[0].x==g_CurrentBgDocument.portals.portals[0].points[0].x);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    assert(!memcmp(before,g_CurrentBgDocument.portals.portals,sizeof(before)));
    EditHistoryFree(&g_EditHistory);BgDocumentFree(&g_CurrentBgDocument);BgFileFree(&source);
    ViewportSetPortals(&viewport,NULL);
    puts("PASS: real portal corner/edge/face picks, occlusion, aliases, modifier/marquee selection, translation previews/restoration, controller rollback and undo.");
}
int main(void) { NativeEdits();ViewportEdits();return 0; }
