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
static void Prepare(BgDocument *doc,int plane,int corner)
{
    BgDocumentRoom *r=&doc->rooms[1];
    r->vertices=realloc(r->vertices,4*sizeof(*r->vertices));assert(r->vertices);
    r->faces=realloc(r->faces,3*sizeof(*r->faces));assert(r->faces);
    const short xy[4][2]={{0,0},{120,0},{120,90},{0,90}};
    for(int v=0;v<4;v++) {
        if(v==3) { r->vertices[v]=r->vertices[0];r->vertices[v].id=doc->nextvertexid++; }
        r->vertices[v].x=plane==2?0:xy[v][0];
        r->vertices[v].y=plane==1?0:xy[v][plane==2?0:1];
        r->vertices[v].z=plane==0?0:xy[v][1];
        r->vertices[v].s=xy[v][0]*8;r->vertices[v].t=xy[v][1]*8;
        r->vertices[v].r=10+v*20;r->vertices[v].g=20+v*20;r->vertices[v].b=30+v*20;r->vertices[v].a=40+v*20;
        r->vertices[v].usecount=(v==0 || v==2)?2:1;
    }
    r->faces[1]=r->faces[0];r->faces[1].id=doc->nextfaceid++;
    const DWORD triangles[2][3]={{0,2,3},{2,0,1}};
    for(int f=0;f<2;f++)for(int c=0;c<3;c++) r->faces[f].vertexindices[(corner+c)%3]=triangles[f][c];
    r->faces[0].uvseams=5;r->faces[1].uvseams=6;
    r->vertexcount=4;r->facecount=doc->facecount=2;r->facecapacity=3;
}
static void CheckDiagonal(const BgDocument *doc,const BgDocumentEdgeRef *edge,DWORD a,DWORD b)
{
    const BgDocumentFace *f=BgDocumentFindFace(doc,&edge->face,NULL);assert(f);
    DWORD x=f->vertexindices[edge->corner],y=f->vertexindices[(edge->corner+1)%3];
    assert((x==a && y==b)||(x==b && y==a));
    Counts(doc);
}
static void Geometry(const char *dir)
{
    const char *why="";
    for(int plane=0;plane<3;plane++)for(int corner=0;corner<3;corner++)for(int side=0;side<2;side++) {
        BgFile bg=Fixture();BgDocument doc={0},before={0};BgDocumentEdgeRef out;
        assert(BgDocumentLoad(bg.data,bg.size,.1f,&doc,&why));Prepare(&doc,plane,corner);
        BgDocumentRoom *r=&doc.rooms[1];
        assert(BgDocumentClone(&doc,&before,&why));
        BgDocumentEdgeRef edge=Edge(r->faces+side,corner);
        assert(BgDocumentCanReverseEdge(&doc,&edge,&why));Same(&doc,&before);
        assert(BgDocumentReverseEdge(&doc,&edge,&out,&why));CheckDiagonal(&doc,&out,1,3);
        assert(doc.facecount==2 && r->vertexcount==4 && doc.nextfaceid==before.nextfaceid && doc.nextvertexid==before.nextvertexid);
        for(int v=0;v<4;v++) {
            BgDocumentVertex current=r->vertices[v];current.usecount=before.rooms[1].vertices[v].usecount;
            assert(!memcmp(&current,before.rooms[1].vertices+v,sizeof(current)));
        }
        for(int f=0;f<2;f++) {
            BgDocumentFace current=r->faces[f];
            memcpy(current.vertexindices,before.rooms[1].faces[f].vertexindices,sizeof(current.vertexindices));
            current.uvseams=before.rooms[1].faces[f].uvseams;
            assert(!memcmp(&current,before.rooms[1].faces+f,sizeof(current)));
            for(int c=1;c<3;c++) { /* Boundary seam masks follow the same directed edge. */
                DWORD a=r->faces[f].vertexindices[c],b=r->faces[f].vertexindices[(c+1)%3];BOOL seam=FALSE;
                for(int old=0;old<2;old++)for(int k=0;k<3;k++)
                    if(before.rooms[1].faces[old].vertexindices[k]==a && before.rooms[1].faces[old].vertexindices[(k+1)%3]==b)
                        seam=(before.rooms[1].faces[old].uvseams&(1u<<k))!=0;
                assert(!!(r->faces[f].uvseams&(1u<<c))==seam);
            }
            assert(!!(r->faces[f].uvseams&1)==!!((5|6)&(1u<<corner)));
        }
        RoundTrip(&doc,&bg,dir);
        assert(BgDocumentReverseEdge(&doc,&out,&out,&why));CheckDiagonal(&doc,&out,0,2);
        BgDocumentFree(&before);BgDocumentFree(&doc);BgFileFree(&bg);
    }
    for(int failure=0;failure<12;failure++) {
        BgFile bg=Fixture();BgDocument doc={0},before={0};BgDocumentEdgeRef out;
        assert(BgDocumentLoad(bg.data,bg.size,1,&doc,&why));Prepare(&doc,0,0);
        BgDocumentRoom *r=&doc.rooms[1];BgDocumentEdgeRef edge=Edge(r->faces,0);
        switch(failure) {
        case 0: edge.corner=1;break; /* Boundary. */
        case 1: edge.corner=3;break;
        case 2: edge.face.faceid=999;break;
        case 3: r->faces[1].vertexindices[0]=0;r->faces[1].vertexindices[1]=2;break; /* Inconsistent winding. */
        case 4: r->vertices[1].x=30;r->vertices[1].y=50;break; /* Concave quad. */
        case 5: r->vertices[1].x=0;r->vertices[1].y=90;break; /* Collapsed opposite edge. */
        case 6: r->faces[1].material.fog++;break;
        case 7: r->faces[1].layer^=1;break;
        case 8: r->faces[1].cullbackfaces^=1;break;
        case 9: r->faces[2]=r->faces[1];r->faces[2].id=doc.nextfaceid++;r->facecount=doc.facecount=3;break; /* Non-manifold. */
        case 10: r->faces[2]=r->faces[1];r->faces[2].id=doc.nextfaceid++;r->faces[2].vertexindices[0]=1;r->faces[2].vertexindices[1]=3;r->facecount=doc.facecount=3;break; /* Existing new diagonal. */
        case 11: r->vertices[0].usecount=0;break;
        }
        assert(BgDocumentClone(&doc,&before,&why));
        assert(!BgDocumentCanReverseEdge(&doc,&edge,&why) && why[0]);
        assert(!BgDocumentReverseEdge(&doc,&edge,&out,&why) && why[0]);Same(&doc,&before);
        BgDocumentFree(&before);BgDocumentFree(&doc);BgFileFree(&bg);
    }
    puts("PASS: diagonal flips on XY/XZ/YZ, all corner slots/both owners, unchanged native UV/RGBA/materials/IDs, seam guides, repeated flips, save/reload/ROM batch validation and atomic invalid-quad rejection.");
}
typedef void *HWND;
typedef intptr_t LPARAM;
typedef struct MSG { HWND hwnd;unsigned message;uintptr_t wParam;LPARAM lParam; } MSG;
enum { WM_KEYDOWN=256,WM_COMMAND=273,VK_CONTROL=17,VK_MENU=18,VK_SHIFT=16,ID_GEOMETRY_REVERSE_EDGE=100,MB_ICONERROR=16 };
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
static BOOL ViewportGetSelectedBgEdges(HWND hwnd,BgDocumentEdgeRef *out,DWORD count)
{ if(count!=selectedcount)return FALSE;memcpy(out,selection,count*sizeof(*out));return TRUE; }
static BOOL ViewportSelectBgEdges(HWND hwnd,const BgDocumentEdgeRef *edges,DWORD count)
{ if(failselection)return FALSE;assert(count==1);memcpy(selection,edges,count*sizeof(*edges));selectedcount=count;return TRUE; }
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
static BOOL GEditorReverseSelectedEdge(HWND hwnd);
static void SendMessage(HWND hwnd,unsigned message,unsigned command,LPARAM data)
{ assert(message==WM_COMMAND && command==ID_GEOMETRY_REVERSE_EDGE);sent++;GEditorReverseSelectedEdge(hwnd); }
#include "editor.inc"
static void Commands(void)
{
    const char *why="";BgFile bg=Fixture();BgDocument before={0};HWND frame=(HWND)1;
    assert(BgDocumentLoad(bg.data,bg.size,1,&g_CurrentBgDocument,&why));
    Prepare(&g_CurrentBgDocument,0,0);
    assert(BgDocumentClone(&g_CurrentBgDocument,&before,&why));selection[0]=Edge(&g_CurrentBgDocument.rooms[1].faces[0],0);
    assert(GEditorCanReverseSelectedEdge());
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory,selection,sizeof(*selection),FALSE,&why));
    for(int failure=0;failure<3;failure++) {
        ULONGLONG revision=g_EditHistory.nextrevision;failrebuild=failure==0;failselection=failure==1;
        if(failure==2)g_EditHistory.nextrevision=0;
        assert(!GEditorReverseSelectedEdge(frame));g_EditHistory.nextrevision=revision;failselection=FALSE;
        Same(&g_CurrentBgDocument,&before);assert(!g_EditHistory.undocount && selectedcount==1);
    }
    assert(errors==3 && restores==3);
    MSG msg={g_Viewport,WM_KEYDOWN,'T',0};control=FALSE;
    assert(GEditorHandleReverseEdgeHotkey(frame,&msg) && sent==1 && g_EditHistory.undocount==1 && selectedcount==1);
    assert(g_CurrentBgDocument.facecount==2 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Reverse Edge"));
    assert(EditHistorySetSelection(&g_EditHistory,selection,sizeof(*selection),FALSE,&why));
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    GEditorRestoreHistorySelection(frame);Same(&g_CurrentBgDocument,&before);assert(selectedcount==1);
    msg.lParam=(LPARAM)1<<30;assert(GEditorHandleReverseEdgeHotkey(frame,&msg) && sent==1);msg.lParam=0;
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    GEditorRestoreHistorySelection(frame);assert(selectedcount==1 && g_CurrentBgDocument.facecount==2);
    const char *inputs[]={"Edit","ComboBox","ComboLBox"};
    for(int i=0;i<3;i++) { classname=inputs[i];assert(!GEditorHandleReverseEdgeHotkey(frame,&msg)); }
    classname="GEditorViewport";msg.hwnd=(HWND)3;assert(!GEditorHandleReverseEdgeHotkey(frame,&msg));msg.hwnd=g_Viewport;
    control=TRUE;assert(!GEditorHandleReverseEdgeHotkey(frame,&msg));control=FALSE;
    alt=TRUE;assert(!GEditorHandleReverseEdgeHotkey(frame,&msg));alt=FALSE;
    shift=TRUE;assert(!GEditorHandleReverseEdgeHotkey(frame,&msg));shift=FALSE;
    flying=TRUE;assert(!GEditorHandleReverseEdgeHotkey(frame,&msg) && !GEditorReverseSelectedEdge(frame));flying=FALSE;
    transforming=TRUE;assert(!GEditorHandleReverseEdgeHotkey(frame,&msg) && !GEditorReverseSelectedEdge(frame));transforming=FALSE;
    snap=TRUE;assert(!GEditorReverseSelectedEdge(frame));snap=FALSE;
    tool=EDITOR_TOOL_FACE_SELECT;assert(!GEditorReverseSelectedEdge(frame));tool=EDITOR_TOOL_EDGE_SELECT;
    EditHistoryFree(&g_EditHistory);BgDocumentFree(&g_CurrentBgDocument);BgDocumentFree(&before);BgFileFree(&bg);
}
int main(int argc,char **argv)
{
    assert(argc==2);Geometry(argv[1]);Commands();
    puts("PASS: real command rollback on rebuild/selection/commit failures, one-step undo/redo, diagonal reselection and T input scope without affecting Ctrl+T or text inputs.");
    return 0;
}
