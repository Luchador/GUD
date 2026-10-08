#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bakedlighting.h"
#include <commctrl.h>
#include "resource.h"

static HWND g_Window;
static const BgDocument *g_Document;
static char g_Level[64];
static BgLightingSettings g_Settings;
static BOOL g_HaveSettings;
/* Like the editor's other preferences, room lists are saved immediately.
 * Key by project file AND BG asset, never by a level's editable display name. */
#define BAKE_ROOMS_KEY "Software\\GUD\\GEditor\\Baked Lighting Rooms"
static char g_RoomKey[MAX_PATH+64];
static void RoomKey(char key[MAX_PATH+64], const char *project, const char *bg)
{
    key[0]=0;
    if (project && *project && bg && *bg)
    {
        int length=snprintf(key,MAX_PATH+64,"%s|%s",project,bg);
        if (length<0 || length>=MAX_PATH+64) { key[0]=0; }
    }
}
static void SaveRooms(void)
{
    if (!g_Window || !g_RoomKey[0]) { return; }
    unsigned char bits[8192]={0};HKEY key;
    int count=(int)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETCOUNT,0,0);
    for (int i=0;i<count;i++)
    {
        DWORD room=(DWORD)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETITEMDATA,i,0);
        if (room && room<=65535) { bits[room/8]|=1u<<(room%8); }
    }
    if (RegCreateKeyExA(HKEY_CURRENT_USER,BAKE_ROOMS_KEY,0,NULL,REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,NULL,&key,NULL)==ERROR_SUCCESS)
    { RegSetValueExA(key,g_RoomKey,0,REG_BINARY,bits,sizeof(bits));RegCloseKey(key); }
}

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
    s->aoEnabled=IsDlgButtonChecked(h,IDC_BAKE_AO)==BST_CHECKED;
    s->aoStrength=SendDlgItemMessage(h,IDC_BAKE_AO_STRENGTH,TBM_GETPOS,0,0)/100.0;
    *why="Enter a valid ambient occlusion radius in world units.";
    if (s->aoEnabled && !ReadNumber(h,IDC_BAKE_AO_RADIUS,&s->aoRadius))
    { SetFocus(GetDlgItem(h,IDC_BAKE_AO_RADIUS));return FALSE; }
    return BgLightingValidate(s,why);
}
static void UpdateAo(HWND h)
{
    BOOL enabled=IsDlgButtonChecked(h,IDC_BAKE_AO)==BST_CHECKED;
    EnableWindow(GetDlgItem(h,IDC_BAKE_AO_STRENGTH),enabled);
    EnableWindow(GetDlgItem(h,IDC_BAKE_AO_RADIUS),enabled);
    char text[24];snprintf(text,sizeof(text),"%ld%%",(long)SendDlgItemMessage(h,IDC_BAKE_AO_STRENGTH,TBM_GETPOS,0,0));
    SetDlgItemText(h,IDC_BAKE_AO_PERCENT,text);
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
static void LoadRooms(void)
{
    unsigned char bits[8192];DWORD size=sizeof(bits),type=0;HKEY key;
    if (!g_RoomKey[0] || RegOpenKeyExA(HKEY_CURRENT_USER,BAKE_ROOMS_KEY,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS) { return; }
    LONG result=RegQueryValueExA(key,g_RoomKey,NULL,&type,bits,&size);RegCloseKey(key);
    if (result!=ERROR_SUCCESS || type!=REG_BINARY || size!=sizeof(bits)) { return; }
    for (DWORD room=1;g_Document && room<=g_Document->roomcount && room<=65535;room++)
    { if (bits[room/8] & (1u<<(room%8))) { AddRoom(room); } }
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
    SetDlgItemText(h,IDC_BAKE_STATUS,request.settings.aoEnabled && request.settings.aoStrength>0
        ? "Baking lighting and ambient occlusion..." : "Baking lighting...");
    UpdateWindow(h);
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
        CheckDlgButton(h,IDC_BAKE_AO,g_Settings.aoEnabled ? BST_CHECKED : BST_UNCHECKED);
        SendDlgItemMessage(h,IDC_BAKE_AO_STRENGTH,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
        SendDlgItemMessage(h,IDC_BAKE_AO_STRENGTH,TBM_SETPOS,TRUE,(LPARAM)floor(g_Settings.aoStrength*100+.5));
        Number(h,IDC_BAKE_AO_RADIUS,g_Settings.aoRadius);
        SendDlgItemMessage(h,IDC_BAKE_AO_RADIUS,EM_SETLIMITTEXT,63,0);UpdateAo(h);
        for (int id=IDC_BAKE_AMBIENT_R;id<=IDC_BAKE_SMOOTH_ANGLE;id++)
        { SendDlgItemMessage(h,id,EM_SETLIMITTEXT,63,0); }
        return TRUE;
    }
    if (msg==WM_HSCROLL) { UpdateAo(h);return TRUE; }
    if (msg==WM_COMMAND)
    {
        int id=LOWORD(wp);
        if (id==IDCANCEL) { DestroyWindow(h);return TRUE; }
        if (id==IDC_BAKE_RUN) { RunBake(h);return TRUE; }
        if (id==IDC_BAKE_AO) { UpdateAo(h);return TRUE; }
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
        if (id==IDC_BAKE_ADD || id==IDC_BAKE_ADD_ALL || id==IDC_BAKE_REMOVE) { SaveRooms(); }
        UpdateButtons();return TRUE;
    }
    if (msg==WM_CLOSE) { DestroyWindow(h);return TRUE; }
    if (msg==WM_NCDESTROY) { g_Window=NULL;g_Document=NULL;g_Level[0]=0;g_RoomKey[0]=0; }
    return FALSE;
}
void BakedLightingRefresh(const BgDocument *doc, const char *level, const char *project, const char *bg)
{
    if (!g_Window) { return; }
    g_Document=doc;
    const char *name=level ? level : "No level";
    char key[MAX_PATH+64];RoomKey(key,project,bg);
    if (strcmp(g_RoomKey,key) || !g_Level[0])
    {
        SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_RESETCONTENT,0,0);
        lstrcpyn(g_RoomKey,key,sizeof(g_RoomKey));LoadRooms();
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
    BOOL pruned=FALSE;
    int count=(int)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETCOUNT,0,0);
    for (int i=count-1;i>=0;i--)
    {
        DWORD room=(DWORD)SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_GETITEMDATA,i,0);
        if (!doc || !doc->rooms || !room || room>doc->roomcount || !doc->rooms[room].facecount)
        { SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_DELETESTRING,i,0);pruned=TRUE; }
    }
    if (pruned) { SaveRooms(); }
    UpdateButtons();
}
BOOL BakedLightingShow(HWND owner, const BgDocument *doc, const char *level, const char *project, const char *bg)
{
    if (!g_Window) { g_Window=CreateDialog(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_BAKED_LIGHTING),owner,Dialog); }
    if (!g_Window) { return FALSE; }
    BakedLightingRefresh(doc,level,project,bg);ShowWindow(g_Window,SW_RESTORE);SetForegroundWindow(g_Window);return TRUE;
}
void BakedLightingClose(void) { if (g_Window) { DestroyWindow(g_Window); } }
void BakedLightingResetRooms(const char *project, const char *bg)
{
    char name[MAX_PATH+64];HKEY key;RoomKey(name,project,bg);
    if (!name[0]) { return; }
    if (RegOpenKeyExA(HKEY_CURRENT_USER,BAKE_ROOMS_KEY,0,KEY_SET_VALUE,&key)==ERROR_SUCCESS)
    { RegDeleteValueA(key,name);RegCloseKey(key); }
    if (g_Window && !strcmp(g_RoomKey,name))
    { SendDlgItemMessage(g_Window,IDC_BAKE_ROOM_LIST,LB_RESETCONTENT,0,0);UpdateButtons(); }
}
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
