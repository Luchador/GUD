#include <assert.h>
#include <stdio.h>
#include "bgdocument.h"
#include "stanload.h"
#include "edittool.h"
typedef void *HWND;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef intptr_t LPARAM;
#include "types.inc"
typedef struct { double offset[3]; } ViewportTranslation;
typedef struct ViewportState {
    EditorTool tool;
    BOOL vertexsnap;
    int componentcount,stancomponentcount;
    ViewportComponent components[1];
    ViewportStanComponent stancomponents[1];
} ViewportState;
#define VIEWPORT_WM_SNAP_VERTEX 1
#define VIEWPORT_WM_SELECTION_CHANGED 2
static BOOL targetstan,accept;
static unsigned requests;
static int sourceid;
static BOOL sourcestan;
static ViewportTranslation last;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportClearAllSelection(ViewportState *s) { s->componentcount=s->stancomponentcount=0; }
static void ViewportUpdateGizmo(ViewportState *s) {}
static void InvalidateRect(HWND hwnd,const void *rect,BOOL erase) {}
static HWND GetParent(HWND hwnd) { return hwnd; }
static int Selected(const ViewportState *s)
{ return s->stancomponentcount ? (int)s->stancomponents[0].refs[0].point : (int)s->components[0].refs[0].index; }
static void Pick(ViewportState *s,int id,BOOL stan)
{
    ViewportClearAllSelection(s);
    if(id<0) { return; }
    if(stan) { s->stancomponentcount=1;s->stancomponents[0].refs[0]=(StanPointRef){1,id}; }
    else { s->componentcount=1;s->components[0].refs[0]=(BgDocumentVertexRef){1,id}; }
}
static BOOL ViewportTryPickStan(HWND hwnd,ViewportState *s,int x,int y,BOOL add,BOOL remove)
{ if(!targetstan) { return FALSE; }Pick(s,x,TRUE);return TRUE; }
static void ViewportPickComponent(HWND hwnd,ViewportState *s,int x,int y,BOOL add,BOOL remove)
{ Pick(s,x,FALSE); }
static BOOL ViewportGetSelectionPosition(HWND hwnd,double p[3],DWORD *count)
{
    ViewportState *s=hwnd;
    *count=s->componentcount+s->stancomponentcount;
    if(*count!=1) { return FALSE; }
    p[0]=Selected(s)*10.0;p[1]=Selected(s)*-20.0;p[2]=Selected(s)*30.0;return TRUE;
}
static BOOL SendMessage(HWND hwnd,unsigned msg,unsigned wparam,LPARAM lparam)
{
    ViewportState *s=hwnd;
    if(msg!=VIEWPORT_WM_SNAP_VERTEX) { return TRUE; }
    assert(s->componentcount+s->stancomponentcount==1);
    assert(Selected(s)==sourceid && !!s->stancomponentcount==sourcestan);
    last=*(const ViewportTranslation *)lparam;requests++;
    return accept;
}
#include "gesture.inc"

int main(void)
{
    ViewportState s={.tool=EDITOR_TOOL_VERTEX_SELECT,.vertexsnap=TRUE};
    for(int kind=0;kind<2;kind++)
    {
        requests=0;sourceid=2;sourcestan=kind;targetstan=kind;accept=TRUE;
        ViewportClearAllSelection(&s);
        ViewportSnapVertexAt(&s,-1,0);assert(!s.componentcount && !s.stancomponentcount);
        ViewportSnapVertexAt(&s,sourceid,0);assert(Selected(&s)==sourceid);
        ViewportSnapVertexAt(&s,-1,0); /* Missed target must retain the source. */
        assert(s.componentcount+s.stancomponentcount==1 && Selected(&s)==sourceid && requests==0);
        ViewportSnapVertexAt(&s,sourceid,0);assert(requests==0 && s.componentcount+s.stancomponentcount==1);
        accept=FALSE;targetstan=!kind;
        ViewportSnapVertexAt(&s,5,0); /* Cross-asset target, rejected transaction. */
        assert(requests==1 && Selected(&s)==sourceid && !!s.stancomponentcount==sourcestan);
        assert(last.offset[0]==30 && last.offset[1]==-60 && last.offset[2]==90);
        ViewportSnapVertexAt(&s,-1,0);assert(requests==1 && s.componentcount+s.stancomponentcount==1);
        accept=TRUE;ViewportSnapVertexAt(&s,5,0);
        assert(requests==2 && !s.componentcount && !s.stancomponentcount);
    }
    Pick(&s,2,FALSE);s.vertexsnap=FALSE;
    ViewportSnapVertexAt(&s,5,0);assert(Selected(&s)==2);
    puts("PASS: BG/stan snap sources survive misses, self-clicks and rejected edits; cross-asset success clears selection.");
    return 0;
}
