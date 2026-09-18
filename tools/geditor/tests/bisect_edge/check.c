#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
BOOL SetupFileCompact(SetupFile *setup,const char **why) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
BOOL StanFileClone(const StanFile *s,StanFile *out,const char **why) { abort(); }
BOOL StanBisectEdge(StanFile *s,const StanEdgeRef *e,StanEdgeRef *out,const char **why) { abort(); }
static int allocations=-1;
static void *TestMalloc(size_t size) { if(!allocations)return NULL;if(allocations>0)allocations--;return malloc(size); }
#define malloc TestMalloc
#include "bgbisect.c"
#undef malloc
#include "fixture.inc"
#include "counts.inc"
static BgDocumentEdgeRef Edge(const BgDocumentFace *f,unsigned int c)
{ return (BgDocumentEdgeRef){{f->id,f->room,f->layer,0},c}; }
static void Same(const BgDocument *a,const BgDocument *b)
{
    assert(a->roomcount==b->roomcount && a->facecount==b->facecount && a->nextfaceid==b->nextfaceid
        && a->nextvertexid==b->nextvertexid && a->dirty==b->dirty);
    for(DWORD r=1;r<=a->roomcount;r++) {
        assert(a->rooms[r].vertexcount==b->rooms[r].vertexcount && a->rooms[r].facecount==b->rooms[r].facecount);
        if(a->rooms[r].vertexcount)assert(!memcmp(a->rooms[r].vertices,b->rooms[r].vertices,a->rooms[r].vertexcount*sizeof(BgDocumentVertex)));
        if(a->rooms[r].facecount)assert(!memcmp(a->rooms[r].faces,b->rooms[r].faces,a->rooms[r].facecount*sizeof(BgDocumentFace)));
    }
}
static void Prepare(BgDocument *doc,BOOL seam)
{
    BgDocumentRoom *r=&doc->rooms[1];
    r->vertices=realloc(r->vertices,6*sizeof(*r->vertices));assert(r->vertices);
    r->faces=realloc(r->faces,2*sizeof(*r->faces));assert(r->faces);
    const short xy[6][2]={{-100,0},{100,0},{0,100},{100,0},{-100,0},{0,-100}};
    for(int v=0;v<6;v++) {
        if(v>=3) { r->vertices[v]=r->vertices[0];r->vertices[v].id=doc->nextvertexid++; }
        r->vertices[v].x=xy[v][0];r->vertices[v].y=xy[v][1];r->vertices[v].z=0;
        r->vertices[v].s=xy[v][0]*8;r->vertices[v].t=xy[v][1]*8;
        r->vertices[v].r=10+v*20;r->vertices[v].g=20+v*20;r->vertices[v].b=30+v*20;r->vertices[v].a=40+v*20;
    }
    r->faces[1]=r->faces[0];r->faces[1].id=doc->nextfaceid++;
    r->faces[1].vertexindices[0]=seam?3:1;r->faces[1].vertexindices[1]=seam?4:0;r->faces[1].vertexindices[2]=5;
    r->vertexcount=6;r->facecount=r->facecapacity=doc->facecount=2;
    for(int v=0;v<6;v++)r->vertices[v].usecount=0;
    for(int f=0;f<2;f++)for(int c=0;c<3;c++)r->vertices[r->faces[f].vertexindices[c]].usecount++;
}
static void Geometry(const char *dir)
{
    const char *why="";
    for(int seam=0;seam<2;seam++)for(int corner=0;corner<3;corner++) {
        BgFile bg=Fixture();BgDocument doc={0},before={0};BgDocumentEdgeRef halves[2];
        assert(BgDocumentLoad(bg.data,bg.size,.1f,&doc,&why));Prepare(&doc,seam);
        BgDocumentRoom *r=&doc.rooms[1];
        /* Rotate corners to exercise every directed edge slot, including wraparound. */
        for(int c=0;c<3;c++)r->faces[0].vertexindices[(corner+c)%3]=c;
        BgDocumentEdgeRef edge=Edge(&r->faces[0],corner);
        assert(BgDocumentClone(&doc,&before,&why));
        for(int fail=0;fail<2;fail++) {
            allocations=fail;assert(!BgDocumentBisectEdge(&doc,&edge,halves,&why));allocations=-1;Same(&doc,&before);
        }
        BgDocumentEdgeRef bad=edge;bad.corner=3;assert(!BgDocumentBisectEdge(&doc,&bad,halves,&why));Same(&doc,&before);
        assert(BgDocumentBisectEdge(&doc,&edge,halves,&why));Counts(&doc);
        assert(doc.facecount==(seam?3:4) && r->vertexcount==7);
        const BgDocumentVertex *m=&r->vertices[6];
        assert(m->x==0 && m->y==0 && m->z==0 && m->s==0 && m->t==0);
        assert(m->r==20 && m->g==30 && m->b==40 && m->a==50 && m->usecount==(seam?2:4));
        for(DWORD f=0;f<r->facecount;f++) {
            const BgDocumentFace *face=&r->faces[f];double n[3];
            BgBisectNormal(&r->vertices[face->vertexindices[0]],&r->vertices[face->vertexindices[1]],&r->vertices[face->vertexindices[2]],n);
            assert(n[2]>0);assert(!memcmp(&face->material,&before.rooms[1].faces[f<2?0:1].material,sizeof(face->material)));
            assert(face->drawgroup==before.rooms[1].faces[0].drawgroup);
        }
        if(seam)assert(!memcmp(&r->faces[2],&before.rooms[1].faces[1],sizeof(BgDocumentFace)));
        for(int half=0;half<2;half++)assert(BgDocumentFindFace(&doc,&halves[half].face,NULL));
        RoundTrip(&doc,&bg,dir);BgDocumentFree(&before);BgDocumentFree(&doc);BgFileFree(&bg);
    }
    /* Native rounding is symmetric; reject a midpoint collapsing to an endpoint. */
    BgFile bg=Fixture();BgDocument doc={0},before={0};BgDocumentEdgeRef halves[2];
    assert(BgDocumentLoad(bg.data,bg.size,1,&doc,&why));
    BgDocumentRoom *r=&doc.rooms[1];r->vertices[0].x=-3;r->vertices[1].x=0;
    r->vertices[0].s=-32768;r->vertices[1].s=32767;
    BgDocumentEdgeRef edge=Edge(&r->faces[0],0);
    assert(BgDocumentBisectEdge(&doc,&edge,halves,&why));assert(r->vertices[3].x==-2 && r->vertices[3].s==-1);
    BgDocumentFree(&doc);
    assert(BgDocumentLoad(bg.data,bg.size,1,&doc,&why));r=&doc.rooms[1];r->vertices[1].x=1;
    assert(BgDocumentClone(&doc,&before,&why));edge=Edge(&r->faces[0],0);
    assert(!BgDocumentBisectEdge(&doc,&edge,halves,&why));Same(&doc,&before);
    BgDocumentFree(&before);BgDocumentFree(&doc);BgFileFree(&bg);
}

typedef void *HWND;
typedef intptr_t LPARAM;
typedef struct MSG { HWND hwnd;unsigned message;uintptr_t wParam;LPARAM lParam; } MSG;
enum { WM_KEYDOWN=256,WM_COMMAND=273,VK_CONTROL=17,VK_MENU=18,VK_SHIFT=16,ID_GEOMETRY_BISECT_EDGE=100,MB_ICONERROR=16 };
#define GEDITOR_TITLE "test"
static HWND g_Viewport=(HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BgDocumentEdgeRef selection[2];
static DWORD selectedcount=1;
static EditorTool tool=EDITOR_TOOL_EDGE_SELECT;
static BOOL flying,transforming,snap,control,alt,shift,failrebuild,failselection;
static unsigned errors,restores,sent;
static const char *classname="GEditorViewport";
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static BOOL ViewportGetVertexSnap(HWND hwnd) { return snap; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportGetSelectedStanEdge(HWND hwnd,StanEdgeRef *out) { return FALSE; }
static BOOL ViewportSelectStanEdge(HWND hwnd,const StanEdgeRef *edge) { abort(); }
static BOOL ViewportGetSelectedBgEdges(HWND hwnd,BgDocumentEdgeRef *out,DWORD count)
{ if(count!=selectedcount)return FALSE;memcpy(out,selection,count*sizeof(*out));return TRUE; }
static BOOL ViewportSelectBgEdges(HWND hwnd,const BgDocumentEdgeRef *edges,DWORD count)
{ if(failselection)return FALSE;assert(count==2);memcpy(selection,edges,count*sizeof(*edges));selectedcount=count;return TRUE; }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **why) { abort(); }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ if(failrebuild) { failrebuild=FALSE;*why="test rebuild failure";return FALSE; }return TRUE; }
static void GEditorRestoreHistorySelection(HWND hwnd)
{ restores++;selectedcount=g_EditHistory.selectionsize/sizeof(*selection);memcpy(selection,g_EditHistory.selection,g_EditHistory.selectionsize); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *why,const char *title,unsigned flags) { assert(why[0]);errors++; }
static BOOL IsChild(HWND parent,HWND child) { return parent==(HWND)1 && child==(HWND)2; }
static short GetKeyState(int key) { return (key==VK_CONTROL?control:key==VK_MENU?alt:shift)?(short)0x8000:0; }
static void GetClassName(HWND hwnd,char *out,int size) { snprintf(out,size,"%s",classname); }
#define lstrcmpi strcmp
static BOOL GEditorBisectSelectedEdge(HWND hwnd);
static void SendMessage(HWND hwnd,unsigned message,unsigned command,LPARAM data)
{ assert(message==WM_COMMAND && command==ID_GEOMETRY_BISECT_EDGE);sent++;GEditorBisectSelectedEdge(hwnd); }
#include "editor.inc"
static void Commands(void)
{
    const char *why="";BgFile bg=Fixture();BgDocument before={0};HWND frame=(HWND)1;
    assert(BgDocumentLoad(bg.data,bg.size,1,&g_CurrentBgDocument,&why));
    assert(BgDocumentClone(&g_CurrentBgDocument,&before,&why));selection[0]=Edge(&g_CurrentBgDocument.rooms[1].faces[0],0);
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory,selection,sizeof(*selection),FALSE,&why));
    for(int failure=0;failure<3;failure++) {
        ULONGLONG revision=g_EditHistory.nextrevision;failrebuild=failure==0;failselection=failure==1;
        if(failure==2)g_EditHistory.nextrevision=0;
        assert(!GEditorBisectSelectedEdge(frame));g_EditHistory.nextrevision=revision;failselection=FALSE;
        Same(&g_CurrentBgDocument,&before);assert(!g_EditHistory.undocount && selectedcount==1);
    }
    assert(errors==3 && restores==3);
    MSG msg={g_Viewport,WM_KEYDOWN,'Q',0};control=TRUE;
    assert(GEditorHandleBisectEdgeHotkey(frame,&msg) && sent==1 && g_EditHistory.undocount==1 && selectedcount==2);
    assert(g_CurrentBgDocument.facecount==2 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Bisect Edge"));
    assert(EditHistorySetSelection(&g_EditHistory,selection,sizeof(selection),FALSE,&why));
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    GEditorRestoreHistorySelection(frame);Same(&g_CurrentBgDocument,&before);assert(selectedcount==1);
    msg.lParam=(LPARAM)1<<30;assert(GEditorHandleBisectEdgeHotkey(frame,&msg) && sent==1);msg.lParam=0;
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    GEditorRestoreHistorySelection(frame);assert(selectedcount==2 && g_CurrentBgDocument.facecount==2);
    const char *inputs[]={"Edit","ComboBox","ComboLBox"};
    for(int i=0;i<3;i++) { classname=inputs[i];assert(!GEditorHandleBisectEdgeHotkey(frame,&msg)); }
    classname="GEditorViewport";msg.hwnd=(HWND)3;assert(!GEditorHandleBisectEdgeHotkey(frame,&msg));msg.hwnd=g_Viewport;
    control=FALSE;assert(!GEditorHandleBisectEdgeHotkey(frame,&msg));control=TRUE;
    alt=TRUE;assert(!GEditorHandleBisectEdgeHotkey(frame,&msg));alt=FALSE;
    shift=TRUE;assert(!GEditorHandleBisectEdgeHotkey(frame,&msg));shift=FALSE;
    flying=TRUE;assert(!GEditorHandleBisectEdgeHotkey(frame,&msg) && !GEditorBisectSelectedEdge(frame));flying=FALSE;
    transforming=TRUE;assert(!GEditorHandleBisectEdgeHotkey(frame,&msg) && !GEditorBisectSelectedEdge(frame));transforming=FALSE;
    snap=TRUE;assert(!GEditorBisectSelectedEdge(frame));snap=FALSE;
    tool=EDITOR_TOOL_FACE_SELECT;assert(!GEditorBisectSelectedEdge(frame));tool=EDITOR_TOOL_EDGE_SELECT;
    EditHistoryFree(&g_EditHistory);BgDocumentFree(&g_CurrentBgDocument);BgDocumentFree(&before);BgFileFree(&bg);
}
int main(int argc,char **argv)
{
    assert(argc==2);Geometry(argv[1]);Commands();
    puts("PASS: midpoint XYZ/UV/RGBA, shared edges and seams, winding/materials, native round trip, collapse/allocation rejection, controller rollback/history/selection and Ctrl+Q routing.");
}
