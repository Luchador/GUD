#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define TRUE 1
#define FALSE 0
#undef NULL
#define NULL 0
#define LEVEL_MENU_SLOT_COUNT 32
#define IDC_PROJECT_LEVEL_SLOT_FIRST 6200
#define BM_SETSTATE 1
#define MK_LBUTTON 1
#define IDC_HAND 1
#define IDC_NO 2
#define GET_X_LPARAM(l) ((int)(int16_t)((l)&0xffff))
#define GET_Y_LPARAM(l) ((int)(int16_t)((l)>>16))
typedef int BOOL;typedef int HWND;
typedef struct {int x,y;} POINT;
typedef struct {int left,top,right,bottom;} RECT;
enum { WM_MOUSEMOVE,WM_LBUTTONUP,WM_CANCELMODE,WM_CAPTURECHANGED };
static HWND g_ProjectSettings=1,capture;
static BOOL g_LevelDragging;
static int g_LevelDropSlot=-1,g_LevelDragStage=34;
static RECT boxes[32];static int visible[32],enabled[32],pressed[32];
static int assigned=-1,assignedStage=-1,assignments;
static HWND GetCapture(void) {return capture;}
static void ReleaseCapture(void) {capture=0;}
static void SendDlgItemMessage(HWND h,int id,int m,int value,int unused)
{(void)h;(void)m;(void)unused;pressed[id-6200]=value;}
static void ClientToScreen(HWND h,POINT *p) {(void)h;p->x+=100;p->y+=50;}
static HWND GetDlgItem(HWND h,int id) {(void)h;return id;}
static void GetWindowRect(HWND h,RECT *r) {*r=boxes[h-6200];}
static int IsWindowVisible(HWND h) {return visible[h-6200];}
static int IsWindowEnabled(HWND h) {return enabled[h-6200];}
static int PtInRect(const RECT *r,POINT p) {return p.x>=r->left&&p.x<r->right&&p.y>=r->top&&p.y<r->bottom;}
static int LoadCursor(int h,int c) {(void)h;return c;}
static void SetCursor(int c) {(void)c;}
static void ProjectSettingsAssignSlot(unsigned slot,int stage) {assigned=slot;assignedStage=stage;assignments++;}
#include "events.inc"
static unsigned Pos(int x,int y) {return (unsigned)(uint16_t)x|((unsigned)(uint16_t)y<<16);}
static void Begin(void) {capture=g_ProjectSettings;g_LevelDragging=TRUE;assigned=-1;}
int main(void)
{
    for(int i=0;i<32;i++) {int n=i<20?i:i-20,cols=i<20?5:4;
        boxes[i]=(RECT){100+(n%cols)*60,50+(i<20?0:120)+(n/cols)*25,155+(n%cols)*60,70+(i<20?0:120)+(n/cols)*25};
        visible[i]=enabled[i]=TRUE;
    }
    for(int i=0;i<32;i++) {
        int x=boxes[i].left-99,y=boxes[i].top-49;
        assert(ProjectSettingsSlotAt(1,x,y)==i);Begin();
        Event(1,WM_MOUSEMOVE,MK_LBUTTON,Pos(x,y));assert(pressed[i]);
        Event(1,WM_LBUTTONUP,0,Pos(x,y));assert(assigned==i&&assignedStage==34&&!g_LevelDragging&&!capture&&!pressed[i]);
    }
    Begin();Event(1,WM_MOUSEMOVE,MK_LBUTTON,Pos(1,1));
    Event(1,WM_MOUSEMOVE,MK_LBUTTON,Pos(61,1));assert(!pressed[0]&&pressed[1]);
    Event(1,WM_LBUTTONUP,0,Pos(-1,-1));assert(assigned==-1&&!pressed[1]&&!capture);
    Begin();Event(1,WM_LBUTTONUP,0,Pos(61,1));assert(assigned==1); /* use release position */
    Begin();enabled[0]=0;Event(1,WM_LBUTTONUP,0,Pos(1,1));assert(assigned==-1);enabled[0]=1;
    Begin();visible[0]=0;Event(1,WM_LBUTTONUP,0,Pos(1,1));assert(assigned==-1);visible[0]=1;
    for(int event=WM_CANCELMODE;event<=WM_CAPTURECHANGED;event++) {
        Begin();Event(1,WM_MOUSEMOVE,MK_LBUTTON,Pos(1,1));Event(1,event,0,0);
        Event(1,WM_LBUTTONUP,0,Pos(1,1));assert(assigned==-1&&!capture&&!g_LevelDragging&&!pressed[0]);
    }
    Begin();Event(1,WM_MOUSEMOVE,0,Pos(1,1));assert(!g_LevelDragging&&!capture);
    assert(assignments==33);
    puts("PASS: all 32 drop targets, hover changes, drop at release position, outside/disabled/hidden targets, cancellation, lost capture and lost mouse button.");
}
