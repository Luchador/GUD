#include <stdio.h>
#include <stdlib.h>
#include "patroleditor.h"
#include "resource.h"

typedef struct PatrolEditor {
    const SetupFile *source;
    PatrolDocument doc;
    SetupFile *edited;
    DWORD selectedpad;
    BOOL changed;
} PatrolEditor;
static int Row(HWND dialog, int id)
{ return (int)SendDlgItemMessage(dialog,id,LB_GETCURSEL,0,0); }
static void Enable(HWND dialog, int id, BOOL enabled) { EnableWindow(GetDlgItem(dialog,id),enabled); }
static void Buttons(HWND dialog, PatrolEditor *e)
{
    int path=Row(dialog,IDC_PATROL_PATHS),point=Row(dialog,IDC_PATROL_POINTS);
    BOOL valid=path>=0 && (DWORD)path<e->doc.count;
    DWORD count=valid ? e->doc.paths[path].count : 0;
    BOOL waypoint=SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETCURSEL,0,0)!=CB_ERR;
    Enable(dialog,IDC_PATROL_NEW,e->doc.count<PATROL_MAX_PATHS && e->doc.waypointcount);
    Enable(dialog,IDC_PATROL_DELETE,valid);
    Enable(dialog,IDC_PATROL_LOOP,valid);
    Enable(dialog,IDC_PATROL_ADD,valid && waypoint);
    Enable(dialog,IDC_PATROL_INSERT,valid && waypoint && point>=0);
    Enable(dialog,IDC_PATROL_REMOVE,valid && point>=0);
    Enable(dialog,IDC_PATROL_UP,valid && point>0);
    Enable(dialog,IDC_PATROL_DOWN,valid && point>=0 && (DWORD)(point+1)<count);
    Enable(dialog,IDOK,e->doc.changed);
}
static void Points(HWND dialog, PatrolEditor *e, int selected)
{
    int path=Row(dialog,IDC_PATROL_PATHS); char text[160];
    SendDlgItemMessage(dialog,IDC_PATROL_POINTS,LB_RESETCONTENT,0,0);
    if (path>=0 && (DWORD)path<e->doc.count)
    {
        PatrolPath *p=&e->doc.paths[path];
        for (DWORD i=0;i<p->count;i++)
        {
            DWORD pad=e->doc.pads[p->points[i]];
            snprintf(text,sizeof(text),"%lu. Pad %lu   (waypoint %lu)",
                (unsigned long)i+1,(unsigned long)pad,(unsigned long)p->points[i]);
            SendDlgItemMessage(dialog,IDC_PATROL_POINTS,LB_ADDSTRING,0,(LPARAM)text);
        }
        CheckDlgButton(dialog,IDC_PATROL_LOOP,(p->flags&1) ? BST_CHECKED : BST_UNCHECKED);
        if (p->count) { SendDlgItemMessage(dialog,IDC_PATROL_POINTS,LB_SETCURSEL,min(max(0,selected),(int)p->count-1),0); }
    }
    else { CheckDlgButton(dialog,IDC_PATROL_LOOP,BST_UNCHECKED); }
    Buttons(dialog,e);
}
static void Paths(HWND dialog, PatrolEditor *e, int selected)
{
    char text[96];
    SendDlgItemMessage(dialog,IDC_PATROL_PATHS,LB_RESETCONTENT,0,0);
    for (DWORD i=0;i<e->doc.count;i++)
    {
        snprintf(text,sizeof(text),"Patrol %u",e->doc.paths[i].id);
        SendDlgItemMessage(dialog,IDC_PATROL_PATHS,LB_ADDSTRING,0,(LPARAM)text);
    }
    if (e->doc.count) { SendDlgItemMessage(dialog,IDC_PATROL_PATHS,LB_SETCURSEL,min(max(0,selected),(int)e->doc.count-1),0); }
    Points(dialog,e,0);
}
static INT_PTR CALLBACK Dialog(HWND dialog, UINT message, WPARAM wp, LPARAM lp)
{
    PatrolEditor *e=(PatrolEditor *)GetWindowLongPtr(dialog,DWLP_USER);
    if (message==WM_INITDIALOG)
    {
        e=(PatrolEditor *)lp; SetWindowLongPtr(dialog,DWLP_USER,(LONG_PTR)e);
        int selected=0;
        for (DWORD i=0;i<e->doc.waypointcount;i++)
        {
            DWORD pad=e->doc.pads[i]; const SetupPad *p=&e->source->pads[pad]; char text[200];
            snprintf(text,sizeof(text),"Pad %lu  |  waypoint %lu  |  stan %s",
                (unsigned long)pad,(unsigned long)i,p->stanname);
            int row=(int)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_ADDSTRING,0,(LPARAM)text);
            SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_SETITEMDATA,row,i);
            if (pad==e->selectedpad) { selected=row; }
        }
        SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_SETCURSEL,selected,0);
        if (!e->doc.waypointcount) { SetDlgItemText(dialog,IDC_PATROL_HELP,"This setup has no navigation waypoints. Patrols need pads that belong to the level's navigation network."); }
        Paths(dialog,e,0); return TRUE;
    }
    if (!e) { return FALSE; }
    if (message==WM_CLOSE) { EndDialog(dialog,IDCANCEL); return TRUE; }
    if (message!=WM_COMMAND) { return FALSE; }
    int id=LOWORD(wp),path=Row(dialog,IDC_PATROL_PATHS),point=Row(dialog,IDC_PATROL_POINTS);
    const char *why="";
    switch (id)
    {
    case IDCANCEL: EndDialog(dialog,IDCANCEL); return TRUE;
    case IDOK:
        if (!e->doc.changed) { EndDialog(dialog,IDCANCEL); return TRUE; }
        if (!PatrolDocumentCompile(&e->doc,e->source,e->edited,&why)) { break; }
        e->changed=TRUE; EndDialog(dialog,IDOK); return TRUE;
    case IDC_PATROL_PATHS:
        if (HIWORD(wp)==LBN_SELCHANGE) { Points(dialog,e,0); }
        return TRUE;
    case IDC_PATROL_POINTS:
        if (HIWORD(wp)==LBN_SELCHANGE && path>=0 && point>=0)
        {
            DWORD waypoint=e->doc.paths[path].points[point];
            for (DWORD i=0;i<e->doc.waypointcount;i++)
                if ((DWORD)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETITEMDATA,i,0)==waypoint)
                { SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_SETCURSEL,i,0); break; }
            Buttons(dialog,e);
        }
        return TRUE;
    case IDC_PATROL_PAD: Buttons(dialog,e); return TRUE;
    case IDC_PATROL_NEW:
    {
        DWORD added;
        if (!PatrolDocumentAdd(&e->doc,e->source,&added,&why)) { break; }
        Paths(dialog,e,(int)added); return TRUE;
    }
    case IDC_PATROL_DELETE:
        if (!PatrolDocumentDelete(&e->doc,e->source,path,&why)) { break; }
        Paths(dialog,e,path); return TRUE;
    case IDC_PATROL_ADD:
    case IDC_PATROL_INSERT:
    {
        int row=(int)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETCURSEL,0,0);
        if (row==CB_ERR || path<0) { return TRUE; }
        DWORD waypoint=(DWORD)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETITEMDATA,row,0);
        DWORD at=id==IDC_PATROL_ADD ? e->doc.paths[path].count : (DWORD)point;
        if (!PatrolPathInsert(&e->doc,path,at,waypoint,&why)) { break; }
        Points(dialog,e,at); return TRUE;
    }
    case IDC_PATROL_REMOVE:
        PatrolPathRemove(&e->doc,path,point); Points(dialog,e,point); return TRUE;
    case IDC_PATROL_UP:
    case IDC_PATROL_DOWN:
    {
        int direction=id==IDC_PATROL_UP ? -1 : 1;
        if (PatrolPathMove(&e->doc,path,point,direction)) { point+=direction; }
        Points(dialog,e,point); return TRUE;
    }
    case IDC_PATROL_LOOP:
        PatrolPathSetLoop(&e->doc,path,IsDlgButtonChecked(dialog,id)==BST_CHECKED);
        Buttons(dialog,e); return TRUE;
    default: return FALSE;
    }
    MessageBox(dialog,why,"Patrol Paths",MB_ICONERROR); return TRUE;
}
BOOL PatrolEditorShow(HWND owner, const SetupFile *source, DWORD selectedpad,
    SetupFile *edited, BOOL *changed, const char **why)
{
    PatrolEditor e={.source=source,.edited=edited,.selectedpad=selectedpad};
    *changed=FALSE; ZeroMemory(edited,sizeof(*edited));
    if (!PatrolDocumentLoad(source,&e.doc,why)) { return FALSE; }
    INT_PTR result=DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_PATROL_PATHS),owner,Dialog,(LPARAM)&e);
    PatrolDocumentFree(&e.doc); *changed=e.changed;
    if (result==-1) { *why="Could not open Patrol Paths."; return FALSE; }
    *why=""; return TRUE;
}
