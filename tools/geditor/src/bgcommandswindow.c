#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgcommandswindow.h"
#include "resource.h"

typedef struct CommandsWindow { BgVisProgram program; char level[64],file[64]; BOOL filling; } CommandsWindow;
static HWND g_Window;
static CommandsWindow g_State;
static int Selected(HWND h)
{ int row=ListView_GetNextItem(GetDlgItem(h,IDC_BG_COMMANDS_LIST),-1,LVNI_SELECTED);return row>=0 && (DWORD)row<g_State.program.count ? row : -1; }
static void Reference(HWND h,const char *label,DWORD value)
{
    int at=(int)SendDlgItemMessage(h,IDC_BG_COMMANDS_TARGET,CB_ADDSTRING,0,(LPARAM)label);
    if (at>=0) { SendDlgItemMessage(h,IDC_BG_COMMANDS_TARGET,CB_SETITEMDATA,at,value); }
}
static void Room(HWND h,DWORD id)
{ if (id && id<=255) { char label[64];snprintf(label,sizeof(label),"Room %lu",(unsigned long)id);Reference(h,label,id); } }
static void Details(HWND h)
{
    int row=Selected(h);SendDlgItemMessage(h,IDC_BG_COMMANDS_TARGET,CB_RESETCONTENT,0,0);
    const BgVisProgram *p=&g_State.program;
    if (row<0)
    {
        SetDlgItemText(h,IDC_BG_COMMANDS_DETAILS,p->problem[0] ? p->problem :
            "These commands run in game before normal portal traversal. Room tests push conditions onto a stack; IF consumes a condition.\r\n\r\nPortal tests update a separate visibility result and screen rectangle. Room commands use that result to control visibility. Select an instruction to inspect its meaning, references, and original bytes.");
    }
    else
    {
        const BgVisInstruction *i=p->instructions+row;char summary[256],warning[768],text[8192];
        BgVisSummary(i,summary,sizeof(summary));BgVisWarnings(i,warning,sizeof(warning));
        int n=snprintf(text,sizeof(text),"%s (opcode 0x%02X)\r\n%s\r\n\r\n%s\r\n",BgVisName(i->opcode),i->opcode,summary,BgVisExplanation(i->opcode));
        if (i->matches>1) { n+=snprintf(text+n,sizeof(text)-n,"\r\n%lu portal entries share this polygon. The game resolves its address to the first entry, portal %lu.\r\n",(unsigned long)i->matches,(unsigned long)i->portal); }
        if (warning[0]) { n+=snprintf(text+n,sizeof(text)-n,"\r\nWarning: %s\r\n",warning); }
        n+=snprintf(text+n,sizeof(text)-n,"\r\nBG file offset 0x%06lX; length byte %u (eight-byte cells).\r\nRaw cells:\r\n",(unsigned long)i->offset,i->length);
        for (DWORD b=0;b<i->bytes && n<(int)sizeof(text)-96;b+=8)
        {
            const unsigned char *c=p->data+i->offset-p->offset+b;
            n+=snprintf(text+n,sizeof(text)-n,"%06lX: %02X %02X %02X %02X  %02X %02X %02X %02X%s\r\n",(unsigned long)i->offset+b,c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],b ? c[0]==0x64 ? "  polygon address" : c[0]==0x65 ? "  value" : "  operand" : "  instruction");
        }
        if (n>=(int)sizeof(text)-96) { snprintf(text+n,sizeof(text)-n,"... Use Copy All for the complete byte listing.\r\n"); }
        SetDlgItemText(h,IDC_BG_COMMANDS_DETAILS,text);
        if (!(i->warnings&(BG_VIS_LENGTH|BG_VIS_OPERAND)))
        {
            if (i->argument==BG_VIS_ONE_PORTAL && i->portal!=BG_VIS_NO_TARGET)
            {
                char label[96];snprintf(label,sizeof(label),"Portal %lu (rooms %lu / %lu)",(unsigned long)i->portal,(unsigned long)i->room1,(unsigned long)i->room2);
                Reference(h,label,0x80000000u|i->portal);Room(h,i->room1);if (i->room2!=i->room1) { Room(h,i->room2); }
            }
            else if (!(i->warnings&BG_VIS_ROOM) && (i->argument==BG_VIS_ONE_ROOM || i->argument==BG_VIS_ROOM_RANGE))
            { DWORD end=i->argument==BG_VIS_ONE_ROOM ? i->arg[0] : i->arg[1];for (DWORD r=i->arg[0];r<=end && r<=255;r++) { Room(h,r); } }
        }
    }
    BOOL targets=SendDlgItemMessage(h,IDC_BG_COMMANDS_TARGET,CB_GETCOUNT,0,0)>0;
    if (targets) { SendDlgItemMessage(h,IDC_BG_COMMANDS_TARGET,CB_SETCURSEL,0,0); }
    EnableWindow(GetDlgItem(h,IDC_BG_COMMANDS_TARGET),targets);EnableWindow(GetDlgItem(h,IDOK),targets);
}
static void Locate(HWND h)
{
    int row=(int)SendDlgItemMessage(h,IDC_BG_COMMANDS_TARGET,CB_GETCURSEL,0,0);if (row<0) { return; }
    DWORD value=(DWORD)SendDlgItemMessage(h,IDC_BG_COMMANDS_TARGET,CB_GETITEMDATA,row,0);
    if (!SendMessage(GetWindow(h,GW_OWNER),BGCOMMANDS_WM_LOCATE,(value&0x80000000u)!=0,value&0x7fffffffu))
    { MessageBox(h,"This reference has no viewable geometry, or its target is no longer available.","BG Commands",MB_ICONINFORMATION); }
}
static void Copy(HWND h)
{
    char *text=BgVisReport(&g_State.program,g_State.level);if (!text) { MessageBox(h,"Not enough memory to copy the command report.","BG Commands",MB_ICONERROR);return; }
    int count=MultiByteToWideChar(CP_ACP,0,text,-1,NULL,0);HGLOBAL memory=count>0 ? GlobalAlloc(GMEM_MOVEABLE,(size_t)count*sizeof(wchar_t)) : NULL;
    wchar_t *wide=memory ? GlobalLock(memory) : NULL;BOOL ok=FALSE;
    if (wide)
    {
        MultiByteToWideChar(CP_ACP,0,text,-1,wide,count);GlobalUnlock(memory);
        if (OpenClipboard(h)) { if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT,memory)) { ok=TRUE;memory=NULL; }CloseClipboard(); }
    }
    if (memory) { GlobalFree(memory); }free(text);
    if (!ok) { MessageBox(h,"The command report could not be copied to the clipboard.","BG Commands",MB_ICONERROR); }
}
static void Layout(HWND h)
{
    RECT r,u={8,8,84,18};GetClientRect(h,&r);MapDialogRect(h,&u);
    int gap=u.left,line=u.bottom,button=u.right,width=r.right-gap*2,bottom=r.bottom-gap-line;
    int details=max(gap+line*8,bottom-line*6-gap*3),top=gap*2+line*2;
    MoveWindow(GetDlgItem(h,IDC_BG_COMMANDS_STATUS),gap,gap,width,line*2,TRUE);
    MoveWindow(GetDlgItem(h,IDC_BG_COMMANDS_LIST),gap,top,width,max(line*3,details-top-gap),TRUE);
    MoveWindow(GetDlgItem(h,IDC_BG_COMMANDS_DETAILS),gap,details,width,max(line*2,bottom-details-gap),TRUE);
    MoveWindow(GetDlgItem(h,IDC_BG_COMMANDS_COPY),gap,bottom,button,line,TRUE);
    MoveWindow(GetDlgItem(h,IDC_BG_COMMANDS_TARGET),gap*2+button,bottom,max(button,width-button*3-gap*4),line*12,TRUE);
    MoveWindow(GetDlgItem(h,IDOK),r.right-gap*2-button*2,bottom,button,line,TRUE);
    MoveWindow(GetDlgItem(h,IDCANCEL),r.right-gap-button,bottom,button,line,TRUE);
    RECT cols={0,0,338,0};MapDialogRect(h,&cols);
    ListView_SetColumnWidth(GetDlgItem(h,IDC_BG_COMMANDS_LIST),4,max(button,width-cols.right-GetSystemMetrics(SM_CXVSCROLL)-8));
}
static INT_PTR CALLBACK Dialog(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
    if (msg==WM_INITDIALOG)
    {
        HWND list=GetDlgItem(h,IDC_BG_COMMANDS_LIST);ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES);
        const char *labels[]={"#","BG offset","Opcode","Command","References / arguments"};const int widths[]={34,62,42,200,280};
        for (int c=0;c<5;c++)
        { RECT r={0,0,widths[c],0};MapDialogRect(h,&r);LVCOLUMN col={0};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=(char *)labels[c];col.cx=r.right;if (ListView_InsertColumn(list,c,&col)<0) { return FALSE; } }
        Layout(h);Details(h);return TRUE;
    }
    if (msg==WM_SIZE && wp!=SIZE_MINIMIZED) { Layout(h);return TRUE; }
    if (msg==WM_GETMINMAXINFO)
    {
        RECT r={0,0,650,380};MapDialogRect(h,&r);AdjustWindowRectEx(&r,(DWORD)GetWindowLongPtr(h,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtr(h,GWL_EXSTYLE));
        MINMAXINFO *info=(MINMAXINFO *)lp;info->ptMinTrackSize.x=r.right-r.left;info->ptMinTrackSize.y=r.bottom-r.top;return TRUE;
    }
    if (msg==WM_CLOSE) { DestroyWindow(h);return TRUE; }
    if (msg==WM_NCDESTROY) { if (g_Window==h) { g_Window=NULL;BgVisFree(&g_State.program);memset(&g_State,0,sizeof(g_State)); }return TRUE; }
    if (msg==WM_COMMAND)
    {
        switch (LOWORD(wp))
        { case IDCANCEL:DestroyWindow(h);return TRUE;case IDOK:Locate(h);return TRUE;case IDC_BG_COMMANDS_COPY:Copy(h);return TRUE; }
    }
    if (msg==WM_NOTIFY && ((NMHDR *)lp)->idFrom==IDC_BG_COMMANDS_LIST)
    {
        if (((NMHDR *)lp)->code==LVN_ITEMCHANGED && !g_State.filling) { Details(h);return TRUE; }
        if (((NMHDR *)lp)->code==NM_DBLCLK && ((NMITEMACTIVATE *)lp)->iItem>=0) { Locate(h);return TRUE; }
    }
    return FALSE;
}
void BgCommandsWindowRefresh(const BgFile *bg,const BgDocument *doc,const char *level)
{
    if (!g_Window) { return; }BgVisProgram next={0};const char *why="";
    BOOL ok=BgVisDecode(bg,doc && !doc->portalwarning ? &doc->portals : NULL,doc ? doc->roomcount : 0,&next,&why);
    if (!ok) { snprintf(next.problem,sizeof(next.problem),"%s",why); }
    const char *name=level ? level : "No level",*file=bg ? bg->name : "";
    BgVisProgram *old=&g_State.program;BOOL samelevel=!strcmp(name,g_State.level) && !strcmp(file,g_State.file);
    if (samelevel && next.offset==old->offset && next.count==old->count && next.size==old->size
        && next.complete==old->complete && next.present==old->present && next.singleDisplayList==old->singleDisplayList
        && !strcmp(next.problem,old->problem) && (!next.size || !memcmp(next.data,old->data,next.size))
        && (!next.count || !memcmp(next.instructions,old->instructions,next.count*sizeof(*next.instructions))))
    { BgVisFree(&next);return; }
    int selected=samelevel ? Selected(g_Window) : 0;g_State.filling=TRUE;
    HWND list=GetDlgItem(g_Window,IDC_BG_COMMANDS_LIST);SendMessage(list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list);
    BgVisFree(old);*old=next;lstrcpyn(g_State.level,name,sizeof(g_State.level));lstrcpyn(g_State.file,file,sizeof(g_State.file));
    char text[384];snprintf(text,sizeof(text),"BG Commands - %s",name);SetWindowText(g_Window,text);
    for (DWORD row=0;row<next.count;row++)
    {
        const BgVisInstruction *i=next.instructions+row;LVITEM item={0};item.mask=LVIF_TEXT;item.iItem=(int)row;item.pszText=text;
        snprintf(text,sizeof(text),"%lu%s",(unsigned long)row,i->warnings ? " !" : "");
        if (ListView_InsertItem(list,&item)<0) { break; }
        snprintf(text,sizeof(text),"0x%06lX",(unsigned long)i->offset);ListView_SetItemText(list,row,1,text);
        snprintf(text,sizeof(text),"0x%02X",i->opcode);ListView_SetItemText(list,row,2,text);
        snprintf(text,sizeof(text),"%*s%s",(int)(i->depth>12 ? 24 : i->depth*2),"",BgVisName(i->opcode));ListView_SetItemText(list,row,3,text);
        BgVisSummary(i,text,sizeof(text));ListView_SetItemText(list,row,4,text);
    }
    if (!next.present) { snprintf(text,sizeof(text),"%s\r\n%s",file,next.problem[0] ? next.problem : next.singleDisplayList ? "Single-display-list background: no visibility commands." : "This level has no special BG visibility commands."); }
    else { snprintf(text,sizeof(text),"%s - %lu instructions%s, %lu bytes, %lu warning rows.\r\n%s",file,(unsigned long)next.count,next.complete ? " (including END)" : " (partial)",(unsigned long)next.size,(unsigned long)next.warnings,next.problem[0] ? next.problem : "Read-only. Select a command for its meaning and bytes; Go to frames its selected reference."); }
    SetDlgItemText(g_Window,IDC_BG_COMMANDS_STATUS,text);EnableWindow(GetDlgItem(g_Window,IDC_BG_COMMANDS_COPY),ok);
    if (next.count) { selected=max(0,min(selected,(int)next.count-1));ListView_SetItemState(list,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(list,selected,FALSE); }
    g_State.filling=FALSE;Details(g_Window);SendMessage(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,NULL,TRUE);
}
BOOL BgCommandsWindowShow(HWND owner,const BgFile *bg,const BgDocument *doc,const char *level,const char **why)
{
    *why="Could not open BG Commands.";
    if (!g_Window)
    {
        INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES};if (!InitCommonControlsEx(&controls)) { return FALSE; }
        g_Window=CreateDialog(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_BG_COMMANDS),owner,Dialog);if (!g_Window) { return FALSE; }
    }
    BgCommandsWindowRefresh(bg,doc,level);ShowWindow(g_Window,SW_RESTORE);SetForegroundWindow(g_Window);*why="";return TRUE;
}
void BgCommandsWindowClose(void) { if (g_Window) { DestroyWindow(g_Window); } }
BOOL BgCommandsWindowHandleMessage(MSG *msg)
{
    if (!g_Window || !msg || (msg->hwnd!=g_Window && !IsChild(g_Window,msg->hwnd))) { return FALSE; }
    if (msg->message==WM_KEYDOWN && msg->wParam=='C' && (GetKeyState(VK_CONTROL)&0x8000)
        && msg->hwnd==GetDlgItem(g_Window,IDC_BG_COMMANDS_LIST)) { Copy(g_Window);return TRUE; }
    if (!IsDialogMessage(g_Window,msg)) { TranslateMessage(msg);DispatchMessage(msg); }return TRUE;
}
