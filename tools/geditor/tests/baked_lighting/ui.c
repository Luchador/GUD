/* Run the production room-list, validation and dialog handlers with a small
 * control adapter. No OS window or renderer is needed for these checks. */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bglighting.h"
#include "resource.h"
typedef uintptr_t HWND;
typedef intptr_t LPARAM,INT_PTR;
typedef uintptr_t WPARAM;
typedef unsigned UINT;
#undef NULL
#define NULL 0
#define CALLBACK
#define IDCANCEL 2
#define LOWORD(v) ((v)&65535)
enum {WM_INITDIALOG=1,WM_COMMAND,WM_CLOSE,WM_NCDESTROY,LB_GETCOUNT,LB_GETSELCOUNT,
    CB_GETCOUNT,LB_GETITEMDATA,LB_INSERTSTRING,LB_SETITEMDATA,CB_GETCURSEL,CB_GETITEMDATA,
    EM_SETLIMITTEXT,LB_GETSEL,LB_DELETESTRING,LB_RESETCONTENT,CB_RESETCONTENT,CB_ADDSTRING,
    CB_SETITEMDATA,CB_SETCURSEL,MB_ICONERROR};
static HWND g_Window=1;
static const BgDocument *g_Document;
static char g_Level[64];
static BgLightingSettings g_Settings;
static BOOL g_HaveSettings;
typedef struct Control {char text[128];DWORD items[20];BOOL sel[20],enabled;int count,chosen;} Control;
static Control controls[32];
static int baked,errors;
static HWND GetDlgItem(HWND h,int id) {assert(id>=1600&&id<1632);return id;}
static void EnableWindow(HWND id,BOOL enabled) {controls[id-1600].enabled=enabled;}
static void SetDlgItemText(HWND h,int id,const char *s) {snprintf(controls[id-1600].text,128,"%s",s);}
static void GetDlgItemText(HWND h,int id,char *s,int size) {snprintf(s,size,"%s",controls[id-1600].text);}
static void SetWindowText(HWND h,const char *s) {}
static void SetFocus(HWND h) {}
static void DestroyWindow(HWND h) {g_Window=0;}
static void RunBake(HWND h) {baked++;}
static intptr_t SendMessage(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
    Control *c=controls+h-1600;
    switch(msg)
    {
    case LB_GETCOUNT:case CB_GETCOUNT:return c->count;
    case LB_GETSELCOUNT:{int n=0;for(int i=0;i<c->count;i++)n+=c->sel[i];return n;}
    case LB_GETITEMDATA:case CB_GETITEMDATA:assert(wp<(unsigned)c->count);return c->items[wp];
    case LB_INSERTSTRING:
        assert(wp<=(unsigned)c->count&&c->count<20);
        for(int i=c->count;i>(int)wp;i--){c->items[i]=c->items[i-1];c->sel[i]=c->sel[i-1];}
        c->count++;c->sel[wp]=0;return wp;
    case CB_ADDSTRING:assert(c->count<20);return c->count++;
    case LB_SETITEMDATA:case CB_SETITEMDATA:c->items[wp]=lp;return 0;
    case CB_GETCURSEL:return c->chosen;
    case CB_SETCURSEL:c->chosen=wp;return wp;
    case LB_GETSEL:assert(wp<(unsigned)c->count);return c->sel[wp];
    case LB_DELETESTRING:
        assert(wp<(unsigned)c->count);for(int i=wp;i<c->count-1;i++){c->items[i]=c->items[i+1];c->sel[i]=c->sel[i+1];}
        return --c->count;
    case LB_RESETCONTENT:case CB_RESETCONTENT:c->count=0;c->chosen=-1;memset(c->sel,0,sizeof(c->sel));return 0;
    case EM_SETLIMITTEXT:return 0;
    default:assert(0);return 0;
    }
}
static intptr_t SendDlgItemMessage(HWND h,int id,UINT msg,WPARAM wp,LPARAM lp)
{return SendMessage(id,msg,wp,lp);}
#include "ui.inc"
int main(void)
{
    BgDocumentRoom rooms[5]={0};BgDocument doc={.rooms=rooms,.roomcount=4};
    rooms[1].facecount=2;rooms[3].facecount=10;rooms[4].facecount=5;
    for(int i=0;i<32;i++)controls[i].chosen=-1;
    assert(Dialog(1,WM_INITDIALOG,0,0));
    BgLightingSettings parsed;const char *why="";
    assert(Settings(1,&parsed,&why)&&parsed.smoothAngle==60&&parsed.ambientIntensity==.25);
    BakedLightingRefresh(&doc,"Egypt");
    Control *list=controls+IDC_BAKE_ROOM_LIST-1600,*combo=controls+IDC_BAKE_ROOM-1600;
    assert(combo->count==3&&combo->items[0]==1&&combo->items[1]==3&&combo->items[2]==4);
    assert(!list->count&&!controls[IDC_BAKE_RUN-1600].enabled);
    combo->chosen=1;Dialog(1,WM_COMMAND,IDC_BAKE_ADD,0);Dialog(1,WM_COMMAND,IDC_BAKE_ADD,0);
    assert(list->count==1&&list->items[0]==3&&controls[IDC_BAKE_RUN-1600].enabled);
    Dialog(1,WM_COMMAND,IDC_BAKE_ADD_ALL,0);Dialog(1,WM_COMMAND,IDC_BAKE_ADD_ALL,0);
    assert(list->count==3&&list->items[0]==1&&list->items[1]==3&&list->items[2]==4);
    list->sel[0]=list->sel[2]=1;Dialog(1,WM_COMMAND,IDC_BAKE_REMOVE,0);
    assert(list->count==1&&list->items[0]==3);
    BakedLightingRefresh(&doc,"Egypt");assert(list->count==1&&combo->chosen==1);
    rooms[3].facecount=0;BakedLightingRefresh(&doc,"Egypt");assert(!list->count&&!controls[IDC_BAKE_RUN-1600].enabled);
    Dialog(1,WM_COMMAND,IDC_BAKE_ADD_ALL,0);assert(list->count==2);
    BakedLightingRefresh(&doc,"Depot");assert(!list->count);
    SetDlgItemText(1,IDC_BAKE_AMBIENT_R,"1.5");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_AMBIENT_R,"256");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_AMBIENT_R,"255");
    SetDlgItemText(1,IDC_BAKE_SMOOTH_ANGLE,"60junk");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_SMOOTH_ANGLE,"nan");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_SMOOTH_ANGLE,"60 ");assert(Settings(1,&parsed,&why));
    BakedLightingRefresh(NULL,NULL);assert(!combo->count&&!controls[IDC_BAKE_ADD_ALL-1600].enabled);
    assert(!errors&&!baked);
    puts("PASS: production room add/all/remove, duplicates, empty rooms, active-level refresh, invalid/stale rooms and light input validation.");
}
