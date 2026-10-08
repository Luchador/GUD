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
    CB_SETITEMDATA,CB_SETCURSEL,MB_ICONERROR,WM_HSCROLL,TBM_GETPOS,TBM_SETPOS,TBM_SETRANGE};
#define BST_CHECKED 1
#define BST_UNCHECKED 0
#define MAKELPARAM(a,b) ((a)|((b)<<16))
#define BAKE_ROOMS_KEY "test"
static char g_RoomKey[MAX_PATH+64];
static HWND g_Window=1;
static const BgDocument *g_Document;
static char g_Level[64];
static BgLightingSettings g_Settings;
static BOOL g_HaveSettings;
typedef struct Control {char text[128];DWORD items[20];BOOL sel[20],enabled;int count,chosen;} Control;
static Control controls[32];
static int baked,errors;
static void CheckDlgButton(HWND h,int id,int checked) { controls[id-1600].chosen=checked; }
static int IsDlgButtonChecked(HWND h,int id) { return controls[id-1600].chosen; }
typedef unsigned char BYTE;
typedef unsigned HKEY;
#define HKEY_CURRENT_USER 1
#define KEY_QUERY_VALUE 1
#define KEY_SET_VALUE 2
#define ERROR_SUCCESS 0
#define REG_BINARY 3
#define REG_OPTION_NON_VOLATILE 0
static struct { char name[MAX_PATH+64];unsigned char bits[8192];DWORD type,size; } registry[8];
static LONG RegCreateKeyExA(HKEY root,const char *path,int a,void *b,int c,int d,void *e,HKEY *key,void *f) { *key=1;return 0; }
static LONG RegOpenKeyExA(HKEY root,const char *path,int a,int access,HKEY *key) { *key=1;return 0; }
static void RegCloseKey(HKEY key) {}
static LONG RegSetValueExA(HKEY key,const char *name,int a,DWORD type,const BYTE *bits,DWORD size)
{
    int i;for(i=0;i<8;i++)if(!strcmp(registry[i].name,name))break;
    if(i==8)for(i=0;i<8&&registry[i].name[0];i++){}
    assert(i<8&&size<=8192);strcpy(registry[i].name,name);memcpy(registry[i].bits,bits,size);
    registry[i].type=type;registry[i].size=size;return 0;
}
static LONG RegQueryValueExA(HKEY key,const char *name,void *a,DWORD *type,BYTE *bits,DWORD *size)
{
    for(int i=0;i<8;i++)if(!strcmp(registry[i].name,name))
    { assert(*size>=registry[i].size);*type=registry[i].type;*size=registry[i].size;memcpy(bits,registry[i].bits,*size);return 0; }
    return 1;
}
static LONG RegDeleteValueA(HKEY key,const char *name)
{ for(int i=0;i<8;i++)if(!strcmp(registry[i].name,name)){registry[i].name[0]=0;return 0;}return 1; }
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
    case TBM_GETPOS:return c->chosen;
    case CB_GETCURSEL:return c->count ? c->chosen : -1;
    case TBM_SETPOS:c->chosen=lp;return 0;
    case TBM_SETRANGE:return 0;
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
    BakedLightingRefresh(&doc,"Egypt","projectA","egypt");
    Control *list=controls+IDC_BAKE_ROOM_LIST-1600,*combo=controls+IDC_BAKE_ROOM-1600;
    assert(combo->count==3&&combo->items[0]==1&&combo->items[1]==3&&combo->items[2]==4);
    assert(!list->count&&!controls[IDC_BAKE_RUN-1600].enabled);
    combo->chosen=1;Dialog(1,WM_COMMAND,IDC_BAKE_ADD,0);Dialog(1,WM_COMMAND,IDC_BAKE_ADD,0);
    assert(list->count==1&&list->items[0]==3&&controls[IDC_BAKE_RUN-1600].enabled);
    Dialog(1,WM_COMMAND,IDC_BAKE_ADD_ALL,0);Dialog(1,WM_COMMAND,IDC_BAKE_ADD_ALL,0);
    assert(list->count==3&&list->items[0]==1&&list->items[1]==3&&list->items[2]==4);
    list->sel[0]=list->sel[2]=1;Dialog(1,WM_COMMAND,IDC_BAKE_REMOVE,0);
    assert(list->count==1&&list->items[0]==3);
    BakedLightingRefresh(&doc,"Egypt","projectA","egypt");assert(list->count==1&&combo->chosen==1);
    rooms[3].facecount=0;BakedLightingRefresh(&doc,"Egypt","projectA","egypt");assert(!list->count&&!controls[IDC_BAKE_RUN-1600].enabled);
    Dialog(1,WM_COMMAND,IDC_BAKE_ADD_ALL,0);assert(list->count==2);
    BakedLightingRefresh(&doc,"Depot","projectA","depot");assert(!list->count);
    BakedLightingRefresh(&doc,"Renamed Egypt","projectA","egypt");assert(list->count==2);
    BakedLightingRefresh(&doc,"Egypt","projectB","egypt");assert(!list->count);
    BakedLightingRefresh(&doc,"Egypt","projectA","egypt");assert(list->count==2);
    /* Closing/reopening clears process/window state, retaining only the store. */
    Dialog(1,WM_NCDESTROY,0,0);memset(controls,0,sizeof(controls));g_Window=1;g_HaveSettings=FALSE;
    Dialog(1,WM_INITDIALOG,0,0);BakedLightingRefresh(&doc,"Egypt","projectA","egypt");
    assert(list->count==2&&list->items[0]==1&&list->items[1]==4);
    Dialog(1,WM_NCDESTROY,0,0);BakedLightingResetRooms("projectA","egypt");g_Window=1;
    BakedLightingRefresh(&doc,"Egypt","projectA","egypt");assert(!list->count);
    assert(!controls[IDC_BAKE_AO_RADIUS-1600].enabled);
    CheckDlgButton(1,IDC_BAKE_AO,BST_CHECKED);Dialog(1,WM_COMMAND,IDC_BAKE_AO,0);
    assert(controls[IDC_BAKE_AO_RADIUS-1600].enabled&&controls[IDC_BAKE_AO_STRENGTH-1600].enabled);
    SendDlgItemMessage(1,IDC_BAKE_AO_STRENGTH,TBM_SETPOS,TRUE,50);Dialog(1,WM_HSCROLL,0,0);
    assert(!strcmp(controls[IDC_BAKE_AO_PERCENT-1600].text,"50%"));
    SetDlgItemText(1,IDC_BAKE_AO_RADIUS,"350");assert(Settings(1,&parsed,&why)&&parsed.aoEnabled&&parsed.aoRadius==350&&parsed.aoStrength==.5);
    SetDlgItemText(1,IDC_BAKE_AO_RADIUS,"0");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_AO_RADIUS,"nan");assert(!Settings(1,&parsed,&why));
    CheckDlgButton(1,IDC_BAKE_AO,BST_UNCHECKED);assert(Settings(1,&parsed,&why)&&!parsed.aoEnabled);
    SetDlgItemText(1,IDC_BAKE_AMBIENT_R,"1.5");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_AMBIENT_R,"256");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_AMBIENT_R,"255");
    SetDlgItemText(1,IDC_BAKE_SMOOTH_ANGLE,"60junk");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_SMOOTH_ANGLE,"nan");assert(!Settings(1,&parsed,&why));
    SetDlgItemText(1,IDC_BAKE_SMOOTH_ANGLE,"60 ");assert(Settings(1,&parsed,&why));
    BakedLightingRefresh(NULL,NULL,NULL,NULL);assert(!combo->count&&!controls[IDC_BAKE_ADD_ALL-1600].enabled);
    assert(!errors&&!baked);
    puts("PASS: production room persistence across restart/projects/levels, add/remove/empty lists, renumber invalidation, AO controls and light input validation.");
}
