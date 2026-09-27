/* Native control calls are stand-ins; shortcut dispatch and Save Scene are the
 * actual production functions. This verifies ordering and keyboard ownership. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "resource.h"
typedef intptr_t HWND,LPARAM,LRESULT;
typedef uintptr_t WPARAM;
typedef unsigned UINT;
typedef int BOOL;
typedef struct { int x,y; } POINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
#define TRUE 1
#define FALSE 0
#define GET_X_LPARAM(n) ((short)(n))
#define GET_Y_LPARAM(n) ((short)((uintptr_t)(n)>>16))
#define lstrcmpi strcasecmp
enum { WM_KEYDOWN=10,WM_MOUSEWHEEL, VK_ESCAPE=27,VK_CONTROL=40,VK_MENU,GW_OWNER,MB_ICONERROR,
       STUDIO_TRANSLATE,STUDIO_ROTATE,STUDIO_SCALE };
static HWND g_Studio=1,g_StudioViewport=2,focus=2;
static BOOL g_StudioDragArmed,dragging,control,alt;
static struct { char filename[32]; int value; } g_StudioScene={"Main.rnd",0};
static int tool=-1,forwarded,cancelled,saved,lastsaved,pending;
static const char *focusclass="Viewport";
static BOOL IsChild(HWND parent,HWND child) { return parent==g_Studio && child>=2 && child<=10; }
static HWND GetFocus(void) { return focus; }
static HWND GetCapture(void) { return 0; }
static HWND GetWindow(HWND hwnd,int kind) { return 99; }
static HWND WindowFromPoint(POINT point) { return g_StudioViewport; }
static HWND GetDlgItem(HWND hwnd,int id) { return id; }
static int GetKeyState(int key) { return (key==VK_CONTROL ? control : key==VK_MENU ? alt : FALSE) ? 0x8000 : 0; }
static int GetClassName(HWND hwnd,char *out,int size) { snprintf(out,size,"%s",focusclass); return strlen(out); }
static void SetFocus(HWND hwnd) { focus=hwnd; if(pending) { g_StudioScene.value=pending; pending=0; } }
static LRESULT SendMessage(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) { return 0; }
static LRESULT SendDlgItemMessage(HWND hwnd,int id,UINT message,WPARAM wparam,LPARAM lparam)
{ if(message==WM_KEYDOWN && wparam==VK_ESCAPE) g_StudioDragArmed=FALSE; return 0; }
static void SetDlgItemText(HWND hwnd,int id,const char *text) {}
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags) { assert(0); }
static BOOL IsDialogMessage(HWND hwnd,MSG *message) { return FALSE; }
static void TranslateMessage(MSG *message) {}
static void DispatchMessage(MSG *message) { forwarded++; }
static BOOL StudioViewportCancelTransform(HWND hwnd)
{ if(!dragging) return FALSE; dragging=FALSE; cancelled++; return TRUE; }
static void StudioViewportCommitTransform(HWND hwnd) { dragging=FALSE; }
static void RenderStudioTool(int mode) { tool=mode; }
static BOOL StudioSceneSave(const void *scene,const char **why)
{ assert(!dragging && !pending); saved++; lastsaved=g_StudioScene.value; return TRUE; }
#include "input.inc"
static void Key(int key) { MSG message={2,WM_KEYDOWN,key,0}; assert(RenderStudioHandleMessage(&message)); }
int main(void)
{
    Key('W'); assert(tool==STUDIO_TRANSLATE && !forwarded);
    Key('E'); assert(tool==STUDIO_ROTATE && !forwarded);
    Key('R'); assert(tool==STUDIO_SCALE && !forwarded);
    focusclass="Edit"; Key('E'); assert(tool==STUDIO_SCALE && forwarded==1);
    focusclass="ComboBox"; Key('W'); assert(tool==STUDIO_SCALE && forwarded==2);
    focusclass="ListBox"; Key('W'); assert(tool==STUDIO_TRANSLATE && forwarded==2);
    alt=TRUE; Key('R'); assert(tool==STUDIO_TRANSLATE && forwarded==3); alt=FALSE;
    dragging=TRUE; Key(VK_ESCAPE); assert(!dragging && cancelled==1 && forwarded==3);
    g_StudioDragArmed=TRUE; Key(VK_ESCAPE); assert(!g_StudioDragArmed && forwarded==3);
    dragging=TRUE; focusclass="Edit"; pending=87; control=TRUE; Key('S'); control=FALSE;
    assert(saved==1 && lastsaved==87 && !pending && !dragging && forwarded==3);
    pending=99; RenderStudioSaveScene(); assert(saved==2 && lastsaved==99);
    g_StudioScene.filename[0]=0; RenderStudioSaveScene(); assert(saved==2);
    MSG foreign={99,WM_KEYDOWN,'E',0}; assert(!RenderStudioHandleMessage(&foreign) && tool==STUDIO_TRANSLATE);
    puts("PASS: W/E/R, edit/combo input isolation, Escape cancellation, studio ownership and save after pending edits/drags.");
    return 0;
}
