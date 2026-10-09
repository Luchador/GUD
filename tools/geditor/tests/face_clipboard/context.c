/* Exercise the actual hit-to-payload and context-menu functions. The scene
 * picker is stubbed at its visible-triangle boundary, not at the paste target. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bgdocument.h"
#include "stanload.h"
#include "edittool.h"
typedef void *HWND;
typedef void *HMENU;
typedef unsigned int UINT;
typedef intptr_t LPARAM;
typedef struct { int x,y; } POINT;
typedef struct { float x,y,z; } Vertex;
typedef struct { double origin[3],direction[3],mindistance,maxdistance; } ViewportPickRay;
#include "messages.inc"
typedef struct ViewportState {
    BOOL orbit,flying,vertexsnap,boxpending;
    int dragaxis;
    EditorTool tool;
    BgFaceRef *scenefacerefs;
    Vertex *scene;
} ViewportState;
enum { MF_STRING=0,MF_ENABLED=0,MF_GRAYED=1,MF_CHECKED=8,MF_SEPARATOR=0x800,
       TPM_RETURNCMD=1,TPM_NONOTIFY=2,TPM_RIGHTBUTTON=4 };
static BOOL bg=TRUE,object=FALSE,rayok=TRUE;
static int hit=0,menus,bgactions,objectactions,selects,choice=4;
static DWORD selectedcount=2;
static UINT ids[20],flags[20],itemcount;
static ViewportObjectPaste received;
static HWND GetParent(HWND h) { return h; }
static intptr_t SendMessage(HWND h,UINT msg,UINT wparam,LPARAM lparam)
{
    if(msg==VIEWPORT_WM_CAN_PASTE_BG_FACES)return bg;
    if(msg==VIEWPORT_WM_CAN_PASTE_OBJECT)return object;
    if(msg==VIEWPORT_WM_CAN_CREATE_STAN)return TRUE;
    if(msg==VIEWPORT_WM_PASTE_BG_FACES_HERE)bgactions++;
    else { assert(msg==VIEWPORT_WM_PASTE_OBJECT_HERE);objectactions++; }
    assert(!wparam && lparam);received=*(const ViewportObjectPaste *)lparam;return TRUE;
}
static BOOL ViewportBuildPickRay(HWND h,const ViewportState *s,int x,int y,ViewportPickRay *r)
{ assert(x==50 && y==60);*r=(ViewportPickRay){.origin={3,10,7},.direction={0,-1,0}};return rayok; }
static int ViewportFindNearestBgTriangle(const ViewportState *s,const ViewportPickRay *r,double *d)
{ *d=10;return hit; }
static int ViewportFindPickedTriangle(const ViewportState *s,const ViewportPickRay *r,BOOL a,BOOL b,double *d)
{ return ViewportFindNearestBgTriangle(s,r,d); }
static BOOL ViewportTryPickStan(HWND h,ViewportState *s,int x,int y,BOOL a,BOOL b) { return FALSE; }
static BOOL ViewportGetSelectedStanEdge(HWND h,StanEdgeRef *e) { return FALSE; }
static BOOL ViewportFindContextEdge(const ViewportState *s,int x,int y,BgDocumentEdgeRef *e) { return FALSE; }
static BOOL ViewportSelectBgEdges(HWND h,const BgDocumentEdgeRef *e,DWORD count) { return FALSE; }
static DWORD ViewportContextStanType(HWND h,ViewportState *s,int x,int y,int *type) { *type=-1;return 0; }
static DWORD ViewportGetSelectedBgFaceCount(HWND h) { return selectedcount; }
static BOOL ViewportSelectBgFaces(HWND h,const BgFaceRef *f,DWORD count)
{ assert(count==1 && f->faceid==42 && f->room==7);selectedcount=count;selects++;return TRUE; }
static HMENU CreatePopupMenu(void) { menus++;itemcount=0;return (HMENU)1; }
static BOOL AppendMenu(HMENU menu,UINT flag,UINT id,const char *label)
{
    assert(itemcount<20);ids[itemcount]=id;flags[itemcount++]=flag;
    if(id==4)assert(!strcmp(label,"Paste Here"));
    if(id==13)assert(!strcmp(label,"Create Stan"));
    return TRUE;
}
static void ClientToScreen(HWND h,POINT *p) {}
static UINT TrackPopupMenu(HMENU m,UINT f,int x,int y,int reserved,HWND h,void *rect)
{
    UINT matches=0;BOOL enabled=FALSE;
    for(UINT i=0;i<itemcount;i++)if(ids[i]==4) { matches++;enabled=!(flags[i]&MF_GRAYED); }
    assert(matches==1);return enabled ? choice : 0;
}
static void DestroyMenu(HMENU m) {}
#include "context.inc"
static void FaceOrder(void)
{
    assert(itemcount>=7 && ids[0]==1 && ids[1]==13 && ids[2]==4
        && flags[3]==MF_SEPARATOR && ids[4]==10 && ids[5]==11 && ids[6]==12);
}
static void Target(void)
{
    assert(received.room==7 && received.position[0]==3 && received.position[1]==0 && received.position[2]==7);
    assert(received.normal[0]==0 && received.normal[1]==1 && received.normal[2]==0);
}
int main(void)
{
    BgFaceRef ref={.faceid=42,.room=7};
    Vertex vertices[3]={{0,0,0},{0,0,20},{20,0,0}};
    ViewportState s={.dragaxis=-1,.tool=EDITOR_TOOL_FACE_SELECT,.scenefacerefs=&ref,.scene=vertices};
    ViewportShowGeometryContextMenu(&s,&s,50,60);
    assert(menus==1 && bgactions==1 && !objectactions && !selects && selectedcount==2);FaceOrder();Target();
    selectedcount=0;ViewportShowGeometryContextMenu(&s,&s,50,60);
    assert(bgactions==2 && selects==1 && selectedcount==1);FaceOrder();Target();
    choice=0;ViewportShowGeometryContextMenu(&s,&s,50,60);assert(bgactions==2);choice=4;
    /* No clipboard, no hit, bad ray and an object/invalid face under the cursor
     * leave the face menu visible, with Paste Here disabled. */
    bg=FALSE;ViewportShowGeometryContextMenu(&s,&s,50,60);FaceOrder();assert(flags[2]&MF_GRAYED);
    bg=TRUE;hit=-1;ViewportShowGeometryContextMenu(&s,&s,50,60);assert(flags[2]&MF_GRAYED);hit=0;
    rayok=FALSE;ViewportShowGeometryContextMenu(&s,&s,50,60);assert(flags[2]&MF_GRAYED);rayok=TRUE;
    ref.room=0;ViewportShowGeometryContextMenu(&s,&s,50,60);assert(flags[2]&MF_GRAYED);ref.room=7;
    ref.faceid=0;ViewportShowGeometryContextMenu(&s,&s,50,60);assert(flags[2]&MF_GRAYED);ref.faceid=42;
    assert(bgactions==2);
    /* Existing object Paste Here remains available and receives the same hit. */
    bg=FALSE;object=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,60);
    assert(objectactions==1 && bgactions==2);FaceOrder();Target();
    s.tool=EDITOR_TOOL_VERTEX_SELECT;ViewportShowGeometryContextMenu(&s,&s,50,60);
    assert(itemcount==1 && ids[0]==4 && objectactions==2);Target();
    s.tool=EDITOR_TOOL_FACE_SELECT;bg=TRUE;object=FALSE;
    int before=menus;
    s.orbit=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,60);s.orbit=FALSE;
    s.flying=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,60);s.flying=FALSE;
    s.vertexsnap=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,60);s.vertexsnap=FALSE;
    s.boxpending=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,60);s.boxpending=FALSE;
    s.dragaxis=0;ViewportShowGeometryContextMenu(&s,&s,50,60);s.dragaxis=-1;
    s.tool=EDITOR_TOOL_VERTEX_PAINT;ViewportShowGeometryContextMenu(&s,&s,50,60);
    assert(menus==before && bgactions==2 && objectactions==2);
    puts("PASS: Paste Here menu order, captured BG hit/room, retained selection, cancellation, disabled invalid targets, object compatibility and gesture scope.");
    return 0;
}
