#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "edittool.h"
#include "stanload.h"
typedef struct { int x,y; } POINT;
typedef struct { int left,top,right,bottom; } RECT;
typedef union { LONGLONG QuadPart; } LARGE_INTEGER;
typedef void *HWND;
typedef void *HMENU;
typedef unsigned int GLuint,GLenum,UINT;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef intptr_t LPARAM;
#include "types.inc"
enum { GL_FRONT=1, GL_BACK, GL_FRONT_AND_BACK, MF_STRING=0,
       TPM_RETURNCMD=1, TPM_NONOTIFY=2, TPM_RIGHTBUTTON=4,
       VIEWPORT_WM_SPLIT_EDGE=51, VIEWPORT_WM_DISCONNECT_FACES=52 };
typedef struct ViewportState {
    EditorTool tool;
    ViewportRenderMode rendermode;
    BOOL orbit,flying,vertexsnap,boxpending,contextpending,cullbackfaces;
    BOOL showbgprimary,showbgsecondary,showobjects,keyw,keya,keys,keyd,keyq,keye;
    int dragaxis,hoveraxis,width,height,batchcount,selectedtricount;
    float posx,posy,posz,yaw,pitch,speed;
    LONGLONG lastqpc;
    POINT contextpoint,lastmouse;
    Vertex *scene;
    SceneBatch *batches;
    BgDocumentVertexRef *scenevertexrefs;
    BgFaceRef *scenefacerefs;
    unsigned char *hiddentris,*selectedtris;
    ViewportTexture *texturecache;
} ViewportState;
static HWND captured;
static POINT cursor;
static int cursorhide,menus,destroyed,commands,edgepicks,facepicks,commandchoice=1;
static BOOL heldkey,failmenu;
static char label[40];
static UINT sent;
static BgDocumentEdgeRef sentedge,selectededge;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportGetBasis(const ViewportState *s,float f[3],float r[3])
{ f[0]=f[1]=0;f[2]=1;r[0]=1;r[1]=r[2]=0; }
static BOOL GetClientRect(HWND hwnd,RECT *rect)
{ ViewportState *s=hwnd;*rect=(RECT){0,0,s->width,s->height};return TRUE; }
static void SetFocus(HWND hwnd) {}
static void SetCapture(HWND hwnd) { captured=hwnd; }
static void ReleaseCapture(void) { captured=NULL; }
static void GetCursorPos(POINT *p) { *p=cursor; }
static void SetCursorPos(int x,int y) { cursor=(POINT){x,y}; }
static void ShowCursor(BOOL show) { cursorhide+=show?-1:1;assert(cursorhide>=0 && cursorhide<=1); }
static short GetKeyState(int key) { return key=='W' && heldkey ? (short)0x8000 : 0; }
static void QueryPerformanceCounter(LARGE_INTEGER *counter) { static LONGLONG n;counter->QuadPart=n+=10; }
static void QueryPerformanceFrequency(LARGE_INTEGER *counter) { counter->QuadPart=1000; }
static void ViewportPaintGL(ViewportState *s) {}
static void ValidateRect(HWND hwnd,const RECT *rect) {}
static void ViewportCancelTransform(HWND hwnd) { if(hwnd) { ((ViewportState *)hwnd)->dragaxis=-1; } }
static HWND GetParent(HWND hwnd) { return hwnd; }
static void ClientToScreen(HWND hwnd,POINT *p) { p->x+=10;p->y+=20; }
static HMENU CreatePopupMenu(void) { if(failmenu)return NULL;menus++;return (HMENU)1; }
static BOOL AppendMenu(HMENU menu,UINT flags,UINT id,const char *text)
{ assert(menu && id==1);snprintf(label,sizeof(label),"%s",text);return TRUE; }
static UINT TrackPopupMenu(HMENU menu,UINT flags,int x,int y,int reserved,HWND hwnd,const RECT *rect)
{ assert(!((ViewportState *)hwnd)->flying && !cursorhide && !captured);return commandchoice; }
static void DestroyMenu(HMENU menu) { destroyed++; }
static void SendMessage(HWND hwnd,UINT msg,UINT wparam,LPARAM lparam)
{ commands++;sent=msg;if(msg==VIEWPORT_WM_SPLIT_EDGE)sentedge=*(const BgDocumentEdgeRef *)lparam;else assert(!lparam); }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return ((ViewportState *)hwnd)->selectedtricount; }
static BOOL ViewportSelectBgEdges(HWND hwnd,const BgDocumentEdgeRef *edge,DWORD count)
{ assert(count==1);edgepicks++;selectededge=*edge;return TRUE; }
static BOOL ViewportSelectBgFaces(HWND hwnd,const BgFaceRef *face,DWORD count)
{ ViewportState *s=hwnd;assert(count==1 && face->faceid);s->selectedtricount=1;facepicks++;return TRUE; }
static DWORD ViewportFindPickedStan(const ViewportState *s,const ViewportPickRay *r,double *d) { return STAN_TILE_NONE; }
static void ViewportEnvironmentAxes(const ViewportState *s,float *r,float *u) { abort(); }
static void ViewportEnvironmentCoordinates(const ViewportState *s,int i,BgRenderFlags f,const float *r,const float *u,float *uv) { abort(); }
#include "input.inc"

static void Click(ViewportState *s,int x,int y)
{
    ViewportBeginRightGesture(s,s,x,y);
    assert(s->flying && captured==s && cursorhide==1);
    ViewportEndRightGesture(s,s);
    assert(!s->flying && !captured && !cursorhide);
}
int main(void)
{
    Vertex v[6]={{.x=-10,.y=-10,.z=100,.a=255},{.x=10,.y=-10,.z=100,.a=255},
        {.x=0,.y=10,.z=100,.a=255}};
    for(int i=0;i<3;i++) { v[i+3]=v[i];v[i+3].z=50; }
    SceneBatch batches[]={{.first=0,.count=3,.secondary=TRUE},
        {.first=3,.count=3,.object=TRUE,.renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE}};
    BgFaceRef faces[]={{1,1,1,0},{0,0,0,0}};
    BgDocumentVertexRef refs[]={{1,0},{1,1},{1,2},{0,0},{0,1},{0,2}};
    unsigned char hidden[2]={0},selected[2]={0};
    ViewportState s={.tool=EDITOR_TOOL_EDGE_SELECT,.width=400,.height=300,.dragaxis=-1,
        .scene=v,.batches=batches,.batchcount=2,.scenevertexrefs=refs,.scenefacerefs=faces,
        .hiddentris=hidden,.selectedtris=selected,.showbgsecondary=TRUE,.speed=100};
    BgDocumentEdgeRef hit;
    assert(ViewportFindContextEdge(&s,200,124,&hit) && hit.face.faceid==1 && hit.corner==0);
    assert(!ViewportFindContextEdge(&s,30,30,&hit));
    hidden[0]=1;assert(!ViewportFindContextEdge(&s,200,124,&hit));hidden[0]=0;
    s.showbgsecondary=FALSE;assert(!ViewportFindContextEdge(&s,200,124,&hit));s.showbgsecondary=TRUE;
    s.showobjects=TRUE;assert(!ViewportFindContextEdge(&s,200,124,&hit));s.showobjects=FALSE;
    s.cullbackfaces=TRUE;batches[0].renderflags=BG_RENDER_CULL_EXPLICIT|BG_RENDER_CULL_BACK;
    assert(!ViewportFindContextEdge(&s,200,124,&hit));
    batches[0].renderflags=BG_RENDER_CULL_EXPLICIT|BG_RENDER_CULL_FRONT;
    assert(ViewportFindContextEdge(&s,200,124,&hit));s.cullbackfaces=FALSE;batches[0].renderflags=BG_RENDER_BLEND;
    for(int i=0;i<3;i++) { v[i].a=0; } /* Outlined transparent geometry remains editable. */
    assert(ViewportFindContextEdge(&s,200,124,&hit));
    Click(&s,200,124);
    assert(menus==1 && destroyed==1 && commands==1 && edgepicks==1 && sent==VIEWPORT_WM_SPLIT_EDGE);
    assert(!strcmp(label,"Split Edge") && sentedge.face.faceid==selectededge.face.faceid && sentedge.corner==0);
    Click(&s,30,30);assert(menus==1 && commands==1 && edgepicks==1);
    commandchoice=0;Click(&s,200,124);assert(menus==2 && destroyed==2 && commands==1);commandchoice=1;
    /* Sub-threshold jitter cannot move the camera or suppress the context menu. */
    ViewportBeginRightGesture(&s,&s,200,124);cursor.x+=2;ViewportFlyLook(&s,&s);
    assert(s.contextpending && s.yaw==0);ViewportEndRightGesture(&s,&s);assert(commands==2);
    /* Dragging, held movement keys, or lost capture never open a menu. */
    ViewportBeginRightGesture(&s,&s,200,124);cursor.x+=8;ViewportFlyLook(&s,&s);
    assert(!s.contextpending && s.yaw!=0);ViewportEndRightGesture(&s,&s);assert(commands==2);
    heldkey=TRUE;ViewportBeginRightGesture(&s,&s,200,124);assert(!s.contextpending);
    ViewportFlyFrame(&s);assert(s.posz>0);ViewportEndRightGesture(&s,&s);heldkey=FALSE;s.posz=0;
    assert(commands==2);
    ViewportBeginRightGesture(&s,&s,200,124);s.keyw=TRUE;ViewportFlyFrame(&s);
    assert(!s.contextpending);ViewportEndRightGesture(&s,&s);s.posz=0;assert(commands==2);
    ViewportBeginRightGesture(&s,&s,200,124);ViewportEndFly(&s,&s);ViewportEndRightGesture(&s,&s);assert(commands==2);
    s.tool=EDITOR_TOOL_VERTEX_SELECT;Click(&s,200,124);assert(commands==2);
    s.tool=EDITOR_TOOL_FACE_SELECT;s.selectedtricount=3;Click(&s,30,30);
    assert(commands==3 && sent==VIEWPORT_WM_DISCONNECT_FACES && !strcmp(label,"Disconnect Face"));
    assert(s.selectedtricount==3 && !facepicks); /* Right-click retains the whole face selection. */
    s.selectedtricount=0;s.rendermode=VIEWPORT_RENDER_WIREFRAME;Click(&s,200,150);
    assert(commands==4 && facepicks==1 && s.selectedtricount==1);
    s.vertexsnap=TRUE;Click(&s,200,150);assert(commands==4);s.vertexsnap=FALSE;
    s.dragaxis=0;Click(&s,200,150);assert(commands==4);
    failmenu=TRUE;Click(&s,200,150);assert(commands==4 && destroyed==menus);
    puts("PASS: real edge hit testing, culling/occlusion, context labels/targets, preserved face selections, click jitter, camera drag/keys and capture cancellation.");
    return 0;
}
