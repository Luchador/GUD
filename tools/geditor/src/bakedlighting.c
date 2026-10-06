#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bakedlighting.h"
#include "resource.h"

static HWND g_Window;
static const BgDocument *g_Document;
static char g_Level[64];
static BgLightingSettings g_Settings;
static BOOL g_HaveSettings;

static void Number(HWND h, int id, double value)
{ char text[48];snprintf(text,sizeof(text),"%.8g",value);SetDlgItemText(h,id,text); }
static BOOL ReadNumber(HWND h, int id, double *value)
{
    char text[80],*end;GetDlgItemText(h,id,text,sizeof(text));errno=0;
    *value=strtod(text,&end);
    if (end==text || errno || !isfinite(*value)) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    return !*end;
}
static BOOL Settings(HWND h, BgLightingSettings *s, const char **why)
{
    *s=g_Settings;
    *why="Enter RGB channels as whole numbers from 0 to 255.";
    for (int i=0;i<6;i++)
    {
        double n;
        if (!ReadNumber(h,IDC_BAKE_AMBIENT_R+i,&n) || n<0 || n>255 || n!=floor(n))
        { SetFocus(GetDlgItem(h,IDC_BAKE_AMBIENT_R+i));return FALSE; }
        if (i<3) { s->ambient[i]=(unsigned char)n; } else { s->directional[i-3]=(unsigned char)n; }
    }
    int ids[]={IDC_BAKE_AMBIENT_INTENSITY,IDC_BAKE_DIRECTIONAL_INTENSITY,
        IDC_BAKE_DIRECTION_X,IDC_BAKE_DIRECTION_Y,IDC_BAKE_DIRECTION_Z,IDC_BAKE_SMOOTH_ANGLE};
    double *values[]={&s->ambientIntensity,&s->directionalIntensity,
        s->direction,s->direction+1,s->direction+2,&s->smoothAngle};
    *why="Enter valid numbers for intensity, direction and smoothing angle.";
    for (int i=0;i<6;i++) if (!ReadNumber(h,ids[i],values[i]))
    { SetFocus(GetDlgItem(h,ids[i]));return FALSE; }
    return BgLightingValidate(s,why);
}
static void UpdateButtons(void)
{
    if (!g_Window) { return; }
    BOOL rooms=SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETCOUNT,0,0)>0;
    BOOL selected=SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETSELCOUNT,0,0)>0;
    EnableWindow(GetDlgItem(g_Window,IDC_BAKE_REMOVE),selected);
    EnableWindow(GetDlgItem(g_Window,IDC_BAKE_RUN),rooms && g_Document && g_Document->rooms);
    BOOL available=SendDlgItemMessage(g_Window,IDC_BAKE_ROOM,CB_GETCOUNT,0,0)>0;
    EnableWindow(GetDlgItem(g_Window,IDC_BAKE_ADD),available);
    EnableWindow(GetDlgItem(g_Window,IDC_BAKE_ADD_ALL),available);
}
static void AddRoom(DWORD room)
{
    HWND list=GetDlgItem(g_Window,IDC_BAKE_ROOM_LIST);
    if (!g_Document || !g_Document->rooms || !room || room>g_Document->roomcount
        || !g_Document->rooms[room].facecount) { return; }
    int count=(int)SendMessage(list,LB_GETCOUNT,0,0),at=count;
    for (int i=0;i<count;i++)
    {
        DWORD existing=(DWORD)SendMessage(list,LB_GETITEMDATA,i,0);
        if (existing==room) { return; }
        if (existing>room) { at=i;break; }
    }
    char text[80];snprintf(text,sizeof(text),"Room %lu",(unsigned long)room);
    int row=(int)SendMessage(list,LB_INSERTSTRING,at,(LPARAM)text);
    if (row>=0) { SendMessage(list,LB_SETITEMDATA,row,room); }
}
static void RunBake(HWND h)
{
    BakedLightingRequest request={0};const char *why="";
    if (!Settings(h,&request.settings,&why))
    { MessageBox(h,why,"Baked Lighting",MB_ICONERROR);return; }
    int count=(int)SendDlgItemMessage(h,IDC_BAKE_ROOM_LIST,LB_GETCOUNT,0,0);
    if (count<=0) { return; }
    DWORD *rooms=malloc((size_t)count*sizeof(*rooms));
    if (!rooms) { MessageBox(h,"Out of memory reading the room list.","Baked Lighting",MB_ICONERROR);return; }
    for (int i=0;i<count;i++) { rooms[i]=(DWORD)SendDlgItemMessage(h,IDC_BAKE_ROOM_LIST,LB_GETITEMDATA,i,0); }
    request.rooms=rooms;request.count=(DWORD)count;
    HCURSOR old=SetCursor(LoadCursor(NULL,IDC_WAIT));
    BOOL ok=(BOOL)SendMessage(GetWindow(h,GW_OWNER),BAKEDLIGHTING_WM_BAKE,0,(LPARAM)&request);
    SetCursor(old);free(rooms);
    if (!ok) { MessageBox(h,request.why ? request.why : "Could not bake the lighting.","Baked Lighting",MB_ICONERROR);return; }
    g_Settings=request.settings;
    char text[256];
    if (!request.result.vertices && !request.result.splits)
    { snprintf(text,sizeof(text),"No colors changed. These rooms already match the lighting, or contain only degenerate faces."); }
    else
    { snprintf(text,sizeof(text),"Baked %lu rooms / %lu triangles. Updated %lu vertices; added %lu for hard edges. Save Project to keep the result.",
        (unsigned long)request.result.rooms,(unsigned long)request.result.faces,
        (unsigned long)request.result.vertices,(unsigned long)request.result.splits); }
    SetDlgItemText(h,IDC_BAKE_STATUS,text);
}
static INT_PTR CALLBACK Dialog(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    if (msg==WM_INITDIALOG)
    {
        if (!g_HaveSettings) { g_Settings=g_BgLightingDefaults;g_HaveSettings=TRUE; }
        for (int i=0;i<3;i++)
        {
            Number(h,IDC_BAKE_AMBIENT_R+i,g_Settings.ambient[i]);
            Number(h,IDC_BAKE_DIRECTIONAL_R+i,g_Settings.directional[i]);
            Number(h,IDC_BAKE_DIRECTION_X+i,g_Settings.direction[i]);
        }
        Number(h,IDC_BAKE_AMBIENT_INTENSITY,g_Settings.ambientIntensity);
        Number(h,IDC_BAKE_DIRECTIONAL_INTENSITY,g_Settings.directionalIntensity);
        Number(h,IDC_BAKE_SMOOTH_ANGLE,g_Settings.smoothAngle);
        for (int id=IDC_BAKE_AMBIENT_R;id<=IDC_BAKE_SMOOTH_ANGLE;id++)
        { SendDlgItemMessage(h,id,EM_SETLIMITTEXT,63,0); }
        return TRUE;
    }
    if (msg==WM_COMMAND)
    {
        int id=LOWORD(wp);
        if (id==IDCANCEL) { DestroyWindow(h);return TRUE; }
        if (id==IDC_BAKE_RUN) { RunBake(h);return TRUE; }
        if (id==IDC_BAKE_ADD)
        {
            int row=(int)SendDlgItemMessage(h,IDC_BAKE_ROOM,CB_GETCURSEL,0,0);
            if (row>=0) { AddRoom((DWORD)SendDlgItemMessage(h,IDC_BAKE_ROOM,CB_GETITEMDATA,row,0)); }
        }
        if (id==IDC_BAKE_ADD_ALL && g_Document && g_Document->rooms)
        { for (DWORD room=1;room<=g_Document->roomcount;room++) { AddRoom(room); } }
        if (id==IDC_BAKE_REMOVE)
        {
            int count=(int)SendDlgItemMessage(h,IDC_BAKE_ROOM_LIST,LB_GETCOUNT,0,0);
            for (int i=count-1;i>=0;i--) if (SendDlgItemMessage(h,IDC_BAKE_ROOM_LIST,LB_GETSEL,i,0)>0)
            { SendDlgItemMessage(h,IDC_BAKE_ROOM_LIST,LB_DELETESTRING,i,0); }
        }
        UpdateButtons();return TRUE;
    }
    if (msg==WM_CLOSE) { DestroyWindow(h);return TRUE; }
    if (msg==WM_NCDESTROY) { g_Window=NULL;g_Document=NULL;g_Level[0]=0; }
    return FALSE;
}
void BakedLightingRefresh(const BgDocument *doc, const char *level)
{
    if (!g_Window) { return; }
    g_Document=doc;
    const char *name=level ? level : "No level";
    if (strcmp(g_Level,name))
    {
        SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_RESETCONTENT,0,0);
        SetDlgItemText(g_Window,IDC_BAKE_STATUS,"Add rooms, set the lights, then bake. Ctrl+Z undoes the whole bake.");
    }
    lstrcpyn(g_Level,name,sizeof(g_Level));
    char text[128];snprintf(text,sizeof(text),"Baked Lighting - %s",name);SetWindowText(g_Window,text);
    int chosen=(int)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM,CB_GETCURSEL,0,0);
    DWORD selected=chosen>=0 ? (DWORD)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM,CB_GETITEMDATA,chosen,0) : 0;
    SendDlgItemMessage(g_Window,IDC_BAKE_ROOM,CB_RESETCONTENT,0,0);
    for (DWORD room=1;doc && doc->rooms && room<=doc->roomcount;room++) if (doc->rooms[room].facecount)
    {
        snprintf(text,sizeof(text),"Room %lu (%lu tris)",(unsigned long)room,(unsigned long)doc->rooms[room].facecount);
        int row=(int)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM,CB_ADDSTRING,0,(LPARAM)text);
        if (row>=0)
        {
            SendDlgItemMessage(g_Window,IDC_BAKE_ROOM,CB_SETITEMDATA,row,room);
            if (!row || room==selected) { SendDlgItemMessage(g_Window,IDC_BAKE_ROOM,CB_SETCURSEL,row,0); }
        }
    }
    int count=(int)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETCOUNT,0,0);
    for (int i=count-1;i>=0;i--)
    {
        DWORD room=(DWORD)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETITEMDATA,i,0);
        if (!doc || !doc->rooms || !room || room>doc->roomcount || !doc->rooms[room].facecount)
        { SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_DELETESTRING,i,0); }
    }
    UpdateButtons();
}
BOOL BakedLightingShow(HWND owner, const BgDocument *doc, const char *level)
{
    if (!g_Window) { g_Window=CreateDialog(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_BAKED_LIGHTING),owner,Dialog); }
    if (!g_Window) { return FALSE; }
    BakedLightingRefresh(doc,level);ShowWindow(g_Window,SW_RESTORE);SetForegroundWindow(g_Window);return TRUE;
}
void BakedLightingClose(void) { if (g_Window) { DestroyWindow(g_Window); } }
void BakedLightingResetRooms(void)
{ if (g_Window) { SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_RESETCONTENT,0,0);UpdateButtons(); } }
BOOL BakedLightingHandleMessage(MSG *msg)
{
    if (!g_Window || !msg || (msg->hwnd!=g_Window && !IsChild(g_Window,msg->hwnd))) { return FALSE; }
    if (msg->message==WM_KEYDOWN && (GetKeyState(VK_CONTROL)&0x8000) && (msg->wParam=='Z' || msg->wParam=='Y'))
    {
        char name[32];GetClassName(msg->hwnd,name,sizeof(name));
        /* Let text fields keep their standard text-editing shortcuts. */
        if (lstrcmpi(name,"EDIT"))
        { SendMessage(GetWindow(g_Window,GW_OWNER),BAKEDLIGHTING_WM_HISTORY,msg->wParam=='Y' || (GetKeyState(VK_SHIFT)&0x8000),0);return TRUE; }
    }
    if (msg->message==WM_KEYDOWN && msg->wParam==VK_DELETE && msg->hwnd==GetDlgItem(g_Window,IDC_BAKE_ROOM_LIST))
    { SendMessage(g_Window,WM_COMMAND,IDC_BAKE_REMOVE,0);return TRUE; }
    if (!IsDialogMessage(g_Window,msg)) { TranslateMessage(msg);DispatchMessage(msg); }return TRUE;
}
