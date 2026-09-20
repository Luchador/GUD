#include <stdio.h>
#include <stdlib.h>
#include "patroleditor.h"
#include "resource.h"
#include "viewport.h"
#include "texload.h"

typedef struct PatrolEditor {
    const SetupFile *source;
    PatrolDocument doc, baseline;
    HWND viewport;
    BOOL picking, stale, applying;
    TexThumb icon;
    unsigned char pixels[TEX_THUMB_MAX*TEX_THUMB_MAX*4];
    DWORD selectedpad;
} PatrolEditor;
static HWND g_PatrolEditor;
static const char *help = "Pick pads with the eyedropper to append them to the selected route. Escape stops picking. Use at least two different pads along a walkable route. Apply updates the level; then assign a guard in Properties.";
static int Row(HWND dialog, int id)
{ return (int)SendDlgItemMessage(dialog,id,LB_GETCURSEL,0,0); }
static void Enable(HWND dialog, int id, BOOL enabled) { EnableWindow(GetDlgItem(dialog,id),enabled); }
static void Buttons(HWND dialog, PatrolEditor *e)
{
    int path=Row(dialog,IDC_PATROL_PATHS),point=Row(dialog,IDC_PATROL_POINTS);
    BOOL valid=!e->stale && path>=0 && (DWORD)path<e->doc.count;
    DWORD count=valid ? e->doc.paths[path].count : 0;
    BOOL waypoint=SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETCURSEL,0,0)!=CB_ERR;
    Enable(dialog,IDC_PATROL_NEW,!e->stale && e->doc.count<PATROL_MAX_PATHS);
    Enable(dialog,IDC_PATROL_DELETE,valid);
    Enable(dialog,IDC_PATROL_LOOP,valid);
    Enable(dialog,IDC_PATROL_ADD,valid && waypoint);
    Enable(dialog,IDC_PATROL_INSERT,valid && waypoint && point>=0);
    Enable(dialog,IDC_PATROL_REMOVE,valid && point>=0);
    Enable(dialog,IDC_PATROL_UP,valid && point>0);
    Enable(dialog,IDC_PATROL_DOWN,valid && point>=0 && (DWORD)(point+1)<count);
    Enable(dialog,IDC_PATROL_PICK,valid && !e->stale);
    Enable(dialog,IDOK,e->doc.changed && !e->stale);
    InvalidateRect(GetDlgItem(dialog,IDC_PATROL_PICK),NULL,TRUE);
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
static void Pads(HWND dialog, PatrolEditor *e)
{
    int selected=0;
    SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_RESETCONTENT,0,0);
    for (DWORD pad=0;pad<e->source->padcount;pad++)
    {
        const SetupPad *p=&e->source->pads[pad]; char text[200];
        if (p->deleted) { continue; }
        snprintf(text,sizeof(text),"Pad %lu  |  stan %s",(unsigned long)pad,p->stanname);
        int row=(int)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_ADDSTRING,0,(LPARAM)text);
        SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_SETITEMDATA,row,pad);
        if (pad==e->selectedpad) { selected=row; }
    }
    SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_SETCURSEL,selected,0);
}
static BOOL Reload(HWND dialog, PatrolEditor *e, const char **why)
{
    PatrolDocument doc={0}, baseline={0};
    int path=Row(dialog,IDC_PATROL_PATHS),point=Row(dialog,IDC_PATROL_POINTS);
    if (!PatrolDocumentLoad(e->source,&doc,why)
        || !PatrolDocumentLoad(e->source,&baseline,why))
    { PatrolDocumentFree(&doc); PatrolDocumentFree(&baseline); return FALSE; }
    PatrolDocumentFree(&e->doc); PatrolDocumentFree(&e->baseline);
    e->doc=doc; e->baseline=baseline; e->stale=FALSE;
    Pads(dialog,e); Paths(dialog,e,path); Points(dialog,e,point);
    SetDlgItemText(dialog,IDC_PATROL_HELP,help); return TRUE;
}
void PatrolEditorSetPicking(BOOL enabled)
{
    PatrolEditor *e=g_PatrolEditor ? (PatrolEditor *)GetWindowLongPtr(g_PatrolEditor,DWLP_USER) : NULL;
    if (!e) { return; }
    e->picking=enabled && !e->stale && Row(g_PatrolEditor,IDC_PATROL_PATHS)>=0;
    ViewportSetPadPick(e->viewport,e->picking);
    if (!e->stale) { SetDlgItemText(g_PatrolEditor,IDC_PATROL_HELP,e->picking
        ? "Click ordinary pads in travel order to add them to this patrol. Right mouse / wheel navigation remains available. Press Escape or click the eyedropper again to stop." : help); }
    InvalidateRect(GetDlgItem(g_PatrolEditor,IDC_PATROL_PICK),NULL,TRUE);
}
void PatrolEditorRefresh(void)
{
    PatrolEditor *e=g_PatrolEditor ? (PatrolEditor *)GetWindowLongPtr(g_PatrolEditor,DWLP_USER) : NULL;
    const char *why=""; PatrolDocument current={0};
    if (!e || e->applying || e->stale) { return; }
    if (!e->doc.changed)
    {
        if (Reload(g_PatrolEditor,e,&why)) { PatrolEditorSetPicking(e->picking); return; }
    }
    else if (PatrolDocumentLoad(e->source,&current,&why))
    {
        BOOL matches=PatrolDocumentMatches(&e->baseline,&current);
        PatrolDocumentFree(&current);
        for (DWORD i=0;i<e->doc.waypointcount;i++)
        { if (e->doc.pads[i]>=e->source->padcount || e->source->pads[e->doc.pads[i]].deleted) { matches=FALSE; } }
        if (matches) { Pads(g_PatrolEditor,e); Buttons(g_PatrolEditor,e); return; }
    }
    e->stale=TRUE; PatrolEditorSetPicking(FALSE);
    SetDlgItemText(g_PatrolEditor,IDC_PATROL_HELP,
        "The level's patrols or a draft pad changed through undo/redo. Close and reopen Patrol Paths to reload before editing.");
    Buttons(g_PatrolEditor,e);
}
BOOL PatrolEditorApply(void)
{
    PatrolEditor *e=g_PatrolEditor ? (PatrolEditor *)GetWindowLongPtr(g_PatrolEditor,DWLP_USER) : NULL;
    const char *why="";
    if (!e || !e->doc.changed) { return TRUE; }
    PatrolEditorRefresh();
    if (e->stale) { MessageBox(g_PatrolEditor,"Close and reopen Patrol Paths to reload the changed level.","Patrol Paths",MB_ICONINFORMATION); return FALSE; }
    PatrolEditorSetPicking(FALSE); e->applying=TRUE;
    BOOL ok=(BOOL)SendMessage(GetParent(g_PatrolEditor),PATROLEDITOR_WM_APPLY,0,(LPARAM)&e->doc);
    e->applying=FALSE;
    if (!ok) { return FALSE; }
    if (!Reload(g_PatrolEditor,e,&why))
    {
        /* The edit already committed. Do not leave a dirty draft to apply twice. */
        e->doc.changed=FALSE; e->stale=TRUE; Buttons(g_PatrolEditor,e);
        MessageBox(g_PatrolEditor,why,"Patrol Paths",MB_ICONERROR);
    }
    return TRUE;
}
BOOL PatrolEditorConfirmClose(BOOL save)
{
    PatrolEditor *e=g_PatrolEditor ? (PatrolEditor *)GetWindowLongPtr(g_PatrolEditor,DWLP_USER) : NULL;
    if (!e) { return TRUE; }
    if (e->doc.changed || (save && e->source->dirty))
    {
        int choice=MessageBox(g_PatrolEditor,save
            ? "Save the current level and pending patrol changes before leaving?"
            : "Apply the pending patrol changes before closing?",
            "Patrol Paths",MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice==IDCANCEL || (choice==IDYES && !PatrolEditorApply())) { return FALSE; }
        if (choice==IDYES && save && !SendMessage(GetParent(g_PatrolEditor),PATROLEDITOR_WM_SAVE,0,0)) { return FALSE; }
    }
    PatrolEditorClose(); return TRUE;
}
BOOL PatrolEditorPickPad(const SetupPadRef *pad)
{
    PatrolEditor *e=g_PatrolEditor ? (PatrolEditor *)GetWindowLongPtr(g_PatrolEditor,DWLP_USER) : NULL;
    const char *why="";
    if (!e || !e->picking || e->stale || !pad || pad->bound) { return FALSE; }
    int path=Row(g_PatrolEditor,IDC_PATROL_PATHS);
    if (path<0 || (DWORD)path>=e->doc.count) { return FALSE; }
    DWORD at=e->doc.paths[path].count;
    if (!PatrolPathInsertPad(&e->doc,e->source,path,at,pad->index,&why))
    { MessageBox(g_PatrolEditor,why,"Patrol Paths",MB_ICONERROR); return FALSE; }
    e->selectedpad=pad->index; Pads(g_PatrolEditor,e); Points(g_PatrolEditor,e,at);
    ViewportSelectSetupPad(e->viewport,pad); return TRUE;
}
static void DrawPicker(PatrolEditor *e, const DRAWITEMSTRUCT *draw)
{
    RECT rect=draw->rcItem;
    BOOL pressed=e->picking || (draw->itemState&ODS_SELECTED);
    FillRect(draw->hDC,&rect,GetSysColorBrush(pressed ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
    DrawEdge(draw->hDC,&rect,pressed ? EDGE_SUNKEN : EDGE_RAISED,BF_RECT);
    if (e->icon.w && e->icon.h)
    {
        unsigned char pixels[sizeof(e->pixels)];
        COLORREF bg=GetSysColor(pressed ? COLOR_HIGHLIGHT : COLOR_BTNFACE);
        unsigned char background[3]={GetBValue(bg),GetGValue(bg),GetRValue(bg)};
        for (int i=0;i<TEX_THUMB_MAX*TEX_THUMB_MAX;i++)
        {
            unsigned int alpha=e->pixels[i*4+3];
            if (draw->itemState&ODS_DISABLED) { alpha/=2; }
            for (int c=0;c<3;c++) { pixels[i*4+c]=(e->pixels[i*4+c]*alpha+background[c]*(255-alpha))/255; }
            pixels[i*4+3]=255;
        }
        BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=TEX_THUMB_MAX; info.bmiHeader.biHeight=-TEX_THUMB_MAX;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
        int size=min(rect.right-rect.left,rect.bottom-rect.top)-6;
        StretchDIBits(draw->hDC,(rect.right+rect.left-size)/2,(rect.bottom+rect.top-size)/2,size,size,
            0,0,e->icon.w,e->icon.h,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
    }
    else { DrawText(draw->hDC,"Pick",-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE); }
    if (draw->itemState&ODS_FOCUS) { InflateRect(&rect,-3,-3); DrawFocusRect(draw->hDC,&rect); }
}
static INT_PTR CALLBACK Dialog(HWND dialog, UINT message, WPARAM wp, LPARAM lp)
{
    PatrolEditor *e=(PatrolEditor *)GetWindowLongPtr(dialog,DWLP_USER);
    if (message==WM_INITDIALOG)
    {
        e=(PatrolEditor *)lp; SetWindowLongPtr(dialog,DWLP_USER,(LONG_PTR)e);
        TexLoadResourceThumbnail(GetModuleHandle(NULL),IDR_EYEDROPPER,&e->icon,e->pixels);
        Pads(dialog,e); Paths(dialog,e,0); SetDlgItemText(dialog,IDC_PATROL_HELP,help); return TRUE;
    }
    if (!e) { return FALSE; }
    if (message==WM_DESTROY)
    {
        PatrolEditorSetPicking(FALSE); g_PatrolEditor=NULL;
        PatrolDocumentFree(&e->doc); PatrolDocumentFree(&e->baseline); free(e);
        SetWindowLongPtr(dialog,DWLP_USER,0); return TRUE;
    }
    if (message==WM_DRAWITEM && wp==IDC_PATROL_PICK) { DrawPicker(e,(DRAWITEMSTRUCT *)lp); return TRUE; }
    if (message==WM_CLOSE) { PatrolEditorConfirmClose(FALSE); return TRUE; }
    if (message!=WM_COMMAND) { return FALSE; }
    int id=LOWORD(wp),path=Row(dialog,IDC_PATROL_PATHS),point=Row(dialog,IDC_PATROL_POINTS);
    const char *why="";
    if (e->stale && id!=IDCANCEL && id!=IDOK) { return TRUE; }
    switch (id)
    {
    case IDCANCEL: PatrolEditorConfirmClose(FALSE); return TRUE;
    case IDOK: PatrolEditorApply(); return TRUE;
    case IDC_PATROL_PICK:
        PatrolEditorSetPicking(!e->picking);
        if (e->picking) { SetFocus(e->viewport); }
        return TRUE;
    case IDC_PATROL_PATHS:
        if (HIWORD(wp)==LBN_SELCHANGE) { Points(dialog,e,0); }
        return TRUE;
    case IDC_PATROL_POINTS:
        if (HIWORD(wp)==LBN_SELCHANGE && path>=0 && point>=0)
        {
            DWORD pad=e->doc.pads[e->doc.paths[path].points[point]];
            e->selectedpad=pad;
            int count=(int)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETCOUNT,0,0);
            for (int i=0;i<count;i++)
                if ((DWORD)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETITEMDATA,i,0)==pad)
                { SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_SETCURSEL,i,0); break; }
            Buttons(dialog,e);
        }
        return TRUE;
    case IDC_PATROL_PAD:
        e->selectedpad=(DWORD)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETITEMDATA,
            SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETCURSEL,0,0),0);
        Buttons(dialog,e); return TRUE;
    case IDC_PATROL_NEW:
    {
        DWORD added;
        if (!PatrolDocumentAdd(&e->doc,e->source,&added,&why)) { break; }
        Paths(dialog,e,(int)added); return TRUE;
    }
    case IDC_PATROL_DELETE:
        PatrolEditorSetPicking(FALSE);
        if (!PatrolDocumentDelete(&e->doc,e->source,path,&why)) { break; }
        Paths(dialog,e,path); return TRUE;
    case IDC_PATROL_ADD:
    case IDC_PATROL_INSERT:
    {
        int row=(int)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETCURSEL,0,0);
        if (row==CB_ERR || path<0) { return TRUE; }
        DWORD pad=(DWORD)SendDlgItemMessage(dialog,IDC_PATROL_PAD,CB_GETITEMDATA,row,0);
        DWORD at=id==IDC_PATROL_ADD ? e->doc.paths[path].count : (DWORD)point;
        if (!PatrolPathInsertPad(&e->doc,e->source,path,at,pad,&why)) { break; }
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
BOOL PatrolEditorShow(HWND owner, HWND viewport, const SetupFile *source, DWORD selectedpad, const char **why)
{
    if (g_PatrolEditor) { ShowWindow(g_PatrolEditor,SW_RESTORE); SetForegroundWindow(g_PatrolEditor); *why=""; return TRUE; }
    PatrolEditor *e=calloc(1,sizeof(*e));
    if (!e) { *why="Out of memory opening Patrol Paths."; return FALSE; }
    e->source=source; e->viewport=viewport; e->selectedpad=selectedpad;
    if (!PatrolDocumentLoad(source,&e->doc,why) || !PatrolDocumentLoad(source,&e->baseline,why))
    { PatrolDocumentFree(&e->doc); PatrolDocumentFree(&e->baseline); free(e); return FALSE; }
    g_PatrolEditor=CreateDialogParam(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_PATROL_PATHS),owner,Dialog,(LPARAM)e);
    if (!g_PatrolEditor)
    { PatrolDocumentFree(&e->doc); PatrolDocumentFree(&e->baseline); free(e); *why="Could not open Patrol Paths."; return FALSE; }
    ShowWindow(g_PatrolEditor,SW_SHOW); *why=""; return TRUE;
}
void PatrolEditorClose(void) { if (g_PatrolEditor) { DestroyWindow(g_PatrolEditor); } }
BOOL PatrolEditorHandleMessage(MSG *message)
{
    if (!g_PatrolEditor || !message || (message->hwnd!=g_PatrolEditor && !IsChild(g_PatrolEditor,message->hwnd))) { return FALSE; }
    PatrolEditor *e=(PatrolEditor *)GetWindowLongPtr(g_PatrolEditor,DWLP_USER);
    if (e && e->picking && message->message==WM_KEYDOWN && message->wParam==VK_ESCAPE)
    { PatrolEditorSetPicking(FALSE); return TRUE; }
    /* Keep typing and dialog navigation ahead of the main editor's shortcuts. */
    if (!IsDialogMessage(g_PatrolEditor,message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
