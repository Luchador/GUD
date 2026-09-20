/* Production sorted-row dispatch and camera command with Win32 calls stubbed. */
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef struct { unsigned mask; int iItem; LPARAM lParam; } LVITEM;
enum { LVIF_PARAM=4, GW_OWNER=4, LEVELMANAGER_WM_FRAME_ROOM=0x8253 };
#include "resource.h"
static BgDocument g_CurrentBgDocument;
static StanFile g_CurrentStan;
static HWND g_Viewport=(HWND)3, focus;
static DWORD listrooms[]={3,1,0};
static int roomcommands, zooms, cancels;
static BOOL canzoom=TRUE;
static double framedmin[3],framedmax[3];
static BOOL GEditorFrameRoom(DWORD room);
static HWND GetDlgItem(HWND hwnd,int id) { assert(hwnd==(HWND)2 && id==IDC_LEVEL_ROOMS_LIST);return (HWND)4; }
static BOOL ListView_GetItem(HWND hwnd,LVITEM *item)
{
    assert(hwnd==(HWND)4 && item->mask==LVIF_PARAM);
    if(item->iItem<0 || item->iItem>=3)return FALSE;
    item->lParam=listrooms[item->iItem];return TRUE;
}
static HWND GetWindow(HWND hwnd,int relation) { assert(hwnd==(HWND)2 && relation==GW_OWNER);return (HWND)1; }
static LPARAM SendMessage(HWND hwnd,unsigned message,WPARAM wp,LPARAM lp)
{ assert(hwnd==(HWND)1 && message==LEVELMANAGER_WM_FRAME_ROOM && !lp);roomcommands++;return GEditorFrameRoom((DWORD)wp); }
static void ViewportCancelTransform(HWND hwnd) { assert(hwnd==g_Viewport);cancels++; }
static BOOL ViewportZoomToBounds(HWND hwnd,const double min[3],const double max[3])
{ assert(hwnd==g_Viewport);zooms++;memcpy(framedmin,min,sizeof(framedmin));memcpy(framedmax,max,sizeof(framedmax));return canzoom; }
static void SetFocus(HWND hwnd) { focus=hwnd; }
#include "navigation.inc"
static void Navigation(void)
{
    BgDocumentRoom rooms[4]={[1]={.origin={10,20,30}},[3]={.origin={100,200,300}}};
    g_CurrentBgDocument=(BgDocument){.rooms=rooms,.roomcount=3,.levelscale=.5f};g_RoomStats.roomcount=3;
    LevelManagerFrameRoom((HWND)2,0); /* Sorted row 0 represents room 3. */
    assert(roomcommands==1 && zooms==1 && cancels==1 && focus==g_Viewport && framedmin[0]==200 && framedmax[2]==600);
    LevelManagerFrameRoom((HWND)2,1);assert(roomcommands==2 && zooms==2 && framedmin[0]==20 && framedmax[2]==60);
    LevelManagerFrameRoom((HWND)2,-1);LevelManagerFrameRoom((HWND)2,2);LevelManagerFrameRoom((HWND)2,99);
    assert(roomcommands==2 && zooms==2); /* Blank space / Unassigned do not move the camera. */
    canzoom=FALSE;focus=NULL;LevelManagerFrameRoom((HWND)2,0);assert(!focus && zooms==3);canzoom=TRUE;
    g_CurrentBgDocument.roomcount=0;LevelManagerFrameRoom((HWND)2,0);assert(zooms==3 && !focus);
    g_CurrentBgDocument=(BgDocument){0};g_RoomStats=(RoomStats){0};
    puts("PASS: double-click uses sorted room IDs, frames the viewport without selection commands, and ignores blank/Unassigned/stale rows.");
}
