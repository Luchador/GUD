#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef intptr_t HWND, LPARAM, LRESULT;
typedef uintptr_t WPARAM, UINT_PTR, DWORD_PTR;
typedef unsigned UINT;
typedef int BOOL;
typedef struct { int x,y; } POINT;
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define LOWORD(n) ((unsigned short)(n))
#define HIWORD(n) ((unsigned short)((uintptr_t)(n)>>16))
#define GET_X_LPARAM(n) ((short)LOWORD(n))
#define GET_Y_LPARAM(n) ((short)HIWORD(n))
enum { WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE, WM_CANCELMODE, WM_CAPTURECHANGED,
       WM_KEYDOWN, WM_NCDESTROY, LB_ITEMFROMPOINT, LB_GETTEXTLEN, LB_GETTEXT,
       VK_ESCAPE, SM_CXDRAG, SM_CYDRAG, IDC_CROSS, IDC_NO, IDC_ARROW };
static const HWND list=1,g_StudioViewport=2,other=3;
static HWND capture;
static BOOL g_StudioDragArmed,g_StudioDragging,empty;
static POINT g_StudioDragPress,dropped;
static char g_StudioDragModel[MAX_PATH];
static int captures,drops,cursor;
static LRESULT CALLBACK RenderStudioModelDragProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
static HWND GetCapture(void) { return capture; }
static HWND SetCapture(HWND hwnd)
{
    HWND previous=capture; capture=hwnd; captures++;
    if (previous==list) { RenderStudioModelDragProc(list,WM_CAPTURECHANGED,0,hwnd,2,0); }
    return previous;
}
static void ReleaseCapture(void)
{
    HWND previous=capture; capture=0;
    if (previous==list) { RenderStudioModelDragProc(list,WM_CAPTURECHANGED,0,0,2,0); }
}
static LRESULT DefSubclassProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    if (message==WM_LBUTTONDOWN) { SetCapture(hwnd); }
    if ((message==WM_LBUTTONUP || message==WM_CANCELMODE) && GetCapture()==hwnd) { ReleaseCapture(); }
    return 0;
}
static LRESULT SendMessage(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    switch(message)
    {
    case LB_ITEMFROMPOINT: return (empty || GET_Y_LPARAM(lparam)>50) ? 0x10000 : 0;
    case LB_GETTEXTLEN: return empty ? -1 : (LRESULT)strlen("Light fixture.gltf");
    case LB_GETTEXT: strcpy((char *)lparam,"Light fixture.gltf"); return 18;
    default: assert(0); return 0;
    }
}
static int GetSystemMetrics(int value) { return 4; }
static void ClientToScreen(HWND hwnd,POINT *point) { point->x+=100; point->y+=100; }
static HWND WindowFromPoint(POINT point) { return point.x>=200 && point.x<800 && point.y>=100 && point.y<700 ? g_StudioViewport : other; }
static int LoadCursor(void *instance,int id) { return id; }
static void SetCursor(int id) { cursor=id; }
static void lstrcpyn(char *out,const char *in,int length) { assert(strlen(in)<(size_t)length); strcpy(out,in); }
static void RenderStudioDropModel(const char *name,POINT point)
{
    assert(!g_StudioDragArmed && !g_StudioDragging && GetCapture()!=list);
    assert(!strcmp(name,"Light fixture.gltf"));
    if (WindowFromPoint(point)==g_StudioViewport) { drops++; dropped=point; }
}
static void RemoveWindowSubclass(HWND hwnd,void *callback,UINT_PTR id) {}
#include "callback.inc"

static void Mouse(UINT message,int x,int y)
{ RenderStudioModelDragProc(list,message,0,(LPARAM)((uint32_t)(uint16_t)x|((uint32_t)(uint16_t)y<<16)),2,0); }
static void Start(void)
{
    assert(!capture && !g_StudioDragArmed && !g_StudioDragging);
    captures=0; Mouse(WM_LBUTTONDOWN,10,10);
    assert(capture==list && g_StudioDragArmed && !g_StudioDragging);
    assert(captures==1); /* The native list already captured: never recapture it. */
}
int main(void)
{
    Start(); Mouse(WM_MOUSEMOVE,12,12); Mouse(WM_LBUTTONUP,12,12);
    assert(!drops && !capture && !g_StudioDragArmed); /* Ordinary selection click. */
    Start(); Mouse(WM_MOUSEMOVE,350,200);
    assert(g_StudioDragging && capture==list && cursor==IDC_CROSS);
    Mouse(WM_LBUTTONUP,350,200); assert(drops==1 && dropped.x==450 && dropped.y==300 && !capture);
    Start(); Mouse(WM_MOUSEMOVE,-40,-40); assert(cursor==IDC_NO);
    Mouse(WM_LBUTTONUP,-40,-40); assert(drops==1 && !capture);
    Start(); Mouse(WM_MOUSEMOVE,350,200);
    RenderStudioModelDragProc(list,WM_KEYDOWN,VK_ESCAPE,0,2,0);
    Mouse(WM_LBUTTONUP,350,200); assert(drops==1 && !capture && !g_StudioDragArmed);
    Start(); SetCapture(other); assert(capture==other && !g_StudioDragArmed && !g_StudioDragging);
    Mouse(WM_LBUTTONUP,350,200); assert(drops==1 && capture==other); ReleaseCapture();
    Start(); RenderStudioModelDragProc(list,WM_CANCELMODE,0,0,2,0);
    assert(!capture && !g_StudioDragArmed && drops==1);
    empty=TRUE; Mouse(WM_LBUTTONDOWN,10,10); Mouse(WM_MOUSEMOVE,350,200); Mouse(WM_LBUTTONUP,350,200);
    assert(!g_StudioDragArmed && !capture && drops==1);
    empty=FALSE; Mouse(WM_LBUTTONDOWN,10,70); Mouse(WM_LBUTTONUP,10,70);
    assert(!g_StudioDragArmed && !capture && drops==1);
    puts("PASS: native-list capture, drag/drop coordinates, click threshold, outside drop, Escape, capture loss, cancellation and empty list.");
    return 0;
}
