#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "bgcommandswindow.h"
#include "resource.h"

typedef struct CommandsWindow { BgVisProgram program; char level[64],file[64]; BOOL filling,editable; DWORD rooms,portalcount; BgPortal portals[BG_MAX_PORTALS]; } CommandsWindow;
static HWND g_Window;
static CommandsWindow g_State;
static int Selected(HWND h)
{ int row=ListView_GetNextItem(GetDlgItem(h,IDC_BG_COMMANDS_LIST),-1,LVNI_SELECTED);return row>=0 && (DWORD)row<g_State.program.count ? row : -1; }
static void SelectRow(DWORD row)
{
    if (!g_Window || row>=g_State.program.count) { return; }
    HWND list=GetDlgItem(g_Window,IDC_BG_COMMANDS_LIST);
    ListView_SetItemState(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_SetItemState(list,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_EnsureVisible(list,row,FALSE);
}
static BOOL CanReplace(DWORD row)
{
    if (row>=g_State.program.count || BgVisReturnCell(&g_State.program,row)) { return FALSE; }
    unsigned op=g_State.program.instructions[row].opcode;
    return op && op!=0x50 && op!=0x52 && op!=0x5a && op!=0x5b && op!=0x5c;
}
static DWORD ComboValue(HWND h,int id)
{
    int row=(int)SendDlgItemMessage(h,id,CB_GETCURSEL,0,0);
    return row<0 ? (DWORD)-1 : (DWORD)SendDlgItemMessage(h,id,CB_GETITEMDATA,row,0);
}
static void Choice(HWND h,int id,const char *text,DWORD value)
{
    int row=(int)SendDlgItemMessage(h,id,CB_ADDSTRING,0,(LPARAM)text);
    if (row>=0) { SendDlgItemMessage(h,id,CB_SETITEMDATA,row,value); }
}
static void Choose(HWND h,int id,DWORD value)
{
    int count=(int)SendDlgItemMessage(h,id,CB_GETCOUNT,0,0);
    for (int row=0;row<count;row++) if ((DWORD)SendDlgItemMessage(h,id,CB_GETITEMDATA,row,0)==value)
    { SendDlgItemMessage(h,id,CB_SETCURSEL,row,0);return; }
    char text[32];snprintf(text,sizeof(text),"%lu",(unsigned long)value);SetDlgItemText(h,id,text);
}
static void CommandFields(HWND h)
{
    DWORD op=ComboValue(h,IDC_BG_COMMAND_TYPE);BgVisArgument arg=BgVisArgumentType(op);
    BOOL second=arg==BG_VIS_ROOM_RANGE,first=arg!=BG_VIS_NONE;
    for (int id=IDC_BG_COMMAND_ARG1;id<=IDC_BG_COMMAND_ARG2;id++)
    { SendDlgItemMessage(h,id,CB_RESETCONTENT,0,0);SendDlgItemMessage(h,id,CB_LIMITTEXT,63,0); }
    SetDlgItemText(h,IDC_BG_COMMAND_LABEL1,arg==BG_VIS_ONE_PORTAL ? "Portal" : arg==BG_VIS_VALUE ? "Value" : second ? "First room" : "Room");
    ShowWindow(GetDlgItem(h,IDC_BG_COMMAND_LABEL1),first ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h,IDC_BG_COMMAND_ARG1),first ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h,IDC_BG_COMMAND_LABEL2),second ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h,IDC_BG_COMMAND_ARG2),second ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h,IDC_BG_COMMAND_ELSE),op==0x5a ? SW_SHOW : SW_HIDE);
    char text[768];snprintf(text,sizeof(text),"%s%s",BgVisExplanation(op),
        op==0x5a ? " END IF is added automatically. Add body commands before it." :
        op==0x50 ? " CATCH and a skipped return cell are added automatically. Add body commands before CATCH." :
        arg==BG_VIS_ONE_PORTAL ? " Choose a polygon's first portal entry; shared entries resolve to that same entry in game." : "");
    SetDlgItemText(h,IDC_BG_COMMAND_HELP,text);
    if (arg==BG_VIS_ONE_PORTAL)
    {
        for (DWORD n=0;n<g_State.portalcount;n++)
        {
            const BgPortal *p=g_State.portals+n;BOOL firstEntry=TRUE;
            for (DWORD j=0;j<n;j++) if (g_State.portals[j].geometryoffset==p->geometryoffset) { firstEntry=FALSE;break; }
            if (!firstEntry) { continue; }
            snprintf(text,sizeof(text),"%lu: rooms %u / %u",(unsigned long)n,p->connectedroom1,p->connectedroom2);
            Choice(h,IDC_BG_COMMAND_ARG1,text,n);
        }
        SendDlgItemMessage(h,IDC_BG_COMMAND_ARG1,CB_SETCURSEL,0,0);
    }
    else if (arg==BG_VIS_ONE_ROOM || second)
    {
        for (DWORD room=1;room<=g_State.rooms;room++)
        {
            snprintf(text,sizeof(text),"%lu",(unsigned long)room);Choice(h,IDC_BG_COMMAND_ARG1,text,room);
            if (second) { Choice(h,IDC_BG_COMMAND_ARG2,text,room); }
        }
        SendDlgItemMessage(h,IDC_BG_COMMAND_ARG1,CB_SETCURSEL,0,0);
        if (second) { SendDlgItemMessage(h,IDC_BG_COMMAND_ARG2,CB_SETCURSEL,0,0); }
    }
    else if (arg==BG_VIS_VALUE) { SetDlgItemText(h,IDC_BG_COMMAND_ARG1,"1"); }
}
static BOOL ReadArgument(HWND h,int id,BOOL value,DWORD *out)
{
    int selected=(int)SendDlgItemMessage(h,id,CB_GETCURSEL,0,0);
    if (selected>=0) { *out=(DWORD)SendDlgItemMessage(h,id,CB_GETITEMDATA,selected,0);return TRUE; }
    char text[128],*end,*start;GetDlgItemText(h,id,text,sizeof(text));start=text;
    while (isspace((unsigned char)*start)) { start++; }
    errno=0;long long n=strtoll(start,&end,(!strncmp(start,"0x",2) || !strncmp(start,"0X",2)) ? 16 : 10);
    if (end==start || errno) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end || n<(value ? -2147483648LL : 0) || n>4294967295LL) { return FALSE; }
    *out=(DWORD)n;return TRUE;
}
static INT_PTR CALLBACK CommandDialog(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
    BgVisEditRequest *r=(BgVisEditRequest *)GetWindowLongPtr(h,DWLP_USER);
    if (msg==WM_INITDIALOG)
    {
        r=(BgVisEditRequest *)lp;SetWindowLongPtr(h,DWLP_USER,lp);
        BOOL replace=r->operation==BG_VIS_REPLACE;
        SetWindowText(h,replace ? "Edit BG Command" : "Add BG Command");
        SetDlgItemText(h,IDOK,replace ? "Apply" : "Add");
        for (unsigned op=1;op<=0x5c;op++)
        {
            if (!strcmp(BgVisName(op),"UNKNOWN") || op==0x52 || op==0x5c
                || (replace && (op==0x50 || op==0x5a || op==0x5b))) { continue; }
            char label[96];snprintf(label,sizeof(label),"%s (0x%02X)",BgVisName(op),op);Choice(h,IDC_BG_COMMAND_TYPE,label,op);
        }
        Choose(h,IDC_BG_COMMAND_TYPE,r->opcode);
        Choice(h,IDC_BG_COMMAND_POSITION,"Before selected command",0);
        Choice(h,IDC_BG_COMMAND_POSITION,"After selected command",1);
        SendDlgItemMessage(h,IDC_BG_COMMAND_POSITION,CB_SETCURSEL,0,0);
        ShowWindow(GetDlgItem(h,IDC_BG_COMMAND_POSITION),replace ? SW_HIDE : SW_SHOW);
        ShowWindow(GetDlgItem(h,IDC_BG_COMMAND_POSITION_LABEL),replace ? SW_HIDE : SW_SHOW);
        CommandFields(h);
        if (replace) { Choose(h,IDC_BG_COMMAND_ARG1,r->arg[0]);Choose(h,IDC_BG_COMMAND_ARG2,r->arg[1]); }
        return TRUE;
    }
    if (msg==WM_COMMAND)
    {
        if (LOWORD(wp)==IDC_BG_COMMAND_TYPE && HIWORD(wp)==CBN_SELCHANGE) { CommandFields(h);return TRUE; }
        if (LOWORD(wp)==IDCANCEL) { EndDialog(h,IDCANCEL);return TRUE; }
        if (LOWORD(wp)==IDOK)
        {
            r->opcode=ComboValue(h,IDC_BG_COMMAND_TYPE);BgVisArgument arg=BgVisArgumentType(r->opcode);
            r->after=ComboValue(h,IDC_BG_COMMAND_POSITION)==1;
            r->withElse=IsDlgButtonChecked(h,IDC_BG_COMMAND_ELSE)==BST_CHECKED;
            if ((arg!=BG_VIS_NONE && !ReadArgument(h,IDC_BG_COMMAND_ARG1,arg==BG_VIS_VALUE,r->arg))
                || (arg==BG_VIS_ROOM_RANGE && !ReadArgument(h,IDC_BG_COMMAND_ARG2,FALSE,r->arg+1)))
            { MessageBox(h,"Enter a whole number (decimal or 0x hexadecimal), or select a listed room or portal.","BG Commands",MB_ICONERROR);return TRUE; }
            /* Apply while the dialog is open, retaining all inputs on error. */
            if (!SendMessage(GetWindow(h,GW_OWNER),BGCOMMANDS_WM_EDIT,0,(LPARAM)r))
            { MessageBox(h,r->why && r->why[0] ? r->why : "Could not apply this command.","BG Commands",MB_ICONERROR);return TRUE; }
            SelectRow(r->selected);EndDialog(h,IDOK);return TRUE;
        }
    }
    return FALSE;
}
static void EditCommand(HWND h,BgVisEditOperation operation)
{
    int row=Selected(h);if (!g_State.editable || (operation!=BG_VIS_INSERT && row<0)) { return; }
    BgVisEditRequest r={0};r.operation=operation;r.row=row<0 ? 0 : (DWORD)row;r.opcode=0x14;
    if (operation==BG_VIS_REPLACE)
    {
        if (!CanReplace(r.row)) { return; }
        const BgVisInstruction *i=g_State.program.instructions+row;
        r.opcode=i->opcode;r.arg[0]=i->argument==BG_VIS_ONE_PORTAL ? i->portal : i->arg[0];r.arg[1]=i->arg[1];
    }
    if (operation==BG_VIS_DELETE)
    {
        DWORD first,last;if (!BgVisDeleteRange(&g_State.program,r.row,&first,&last)) { return; }
        if (!SendMessage(GetWindow(h,GW_OWNER),BGCOMMANDS_WM_EDIT,0,(LPARAM)&r))
        { MessageBox(h,r.why && r.why[0] ? r.why : "Could not delete this command.","BG Commands",MB_ICONERROR); }
        else { SelectRow(r.selected); }
        return;
    }
    /* Disable both editor windows while choosing operands. The modeless
     * inspector otherwise permits level switches and viewport interaction. */
    EnableWindow(h,FALSE);
    DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_BG_COMMAND_EDIT),GetWindow(h,GW_OWNER),CommandDialog,(LPARAM)&r);
    EnableWindow(h,TRUE);SetForegroundWindow(h);
}

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
        n+=snprintf(text+n,sizeof(text)-n,"\r\n%s offset 0x%06lX; length byte %u (eight-byte cells).\r\nRaw cells:\r\n",p->relativeOffsets ? "Command stream" : "BG file",(unsigned long)i->offset,i->length);
        for (DWORD b=0;b<i->bytes && n<(int)sizeof(text)-96;b+=8)
        {
            const unsigned char *c=p->data+i->offset-p->offset+b;
            n+=snprintf(text+n,sizeof(text)-n,"%06lX: %02X %02X %02X %02X  %02X %02X %02X %02X%s\r\n",(unsigned long)i->offset+b,c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],b ? c[0]==0x64 ? "  polygon address" : c[0]==0x65 ? "  value" : "  operand" : "  instruction");
        }
        if (n>=(int)sizeof(text)-96) { snprintf(text+n,sizeof(text)-n,"... Use Copy All for the complete byte listing.\r\n"); }
        if (BgVisReturnCell(p,(DWORD)row)) { strncat(text,"\r\nBranch return cell: skipped by the runtime and managed with the preceding BRANCH/CATCH block.\r\n",sizeof(text)-strlen(text)-1); }
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
    DWORD first,last;
    BOOL remove=g_State.editable && row>=0 && BgVisDeleteRange(p,(DWORD)row,&first,&last);
    EnableWindow(GetDlgItem(h,IDC_BG_COMMANDS_ADD),g_State.editable);
    EnableWindow(GetDlgItem(h,IDC_BG_COMMANDS_EDIT),g_State.editable && row>=0 && CanReplace((DWORD)row));
    EnableWindow(GetDlgItem(h,IDC_BG_COMMANDS_DELETE),remove);
    SetDlgItemText(h,IDC_BG_COMMANDS_DELETE,remove && last>first ? "Delete Block" : "Delete");
    EnableWindow(GetDlgItem(h,IDC_BG_COMMANDS_UNDO),(BOOL)SendMessage(GetWindow(h,GW_OWNER),BGCOMMANDS_WM_CANHISTORY,FALSE,0));
    EnableWindow(GetDlgItem(h,IDC_BG_COMMANDS_REDO),(BOOL)SendMessage(GetWindow(h,GW_OWNER),BGCOMMANDS_WM_CANHISTORY,TRUE,0));
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
    int details=max(gap+line*8,bottom-line*6-gap*3),toolbar=gap*2+line*2,top=toolbar+line+gap;
    MoveWindow(GetDlgItem(h,IDC_BG_COMMANDS_STATUS),gap,gap,width,line*2,TRUE);
    const int actions[]={IDC_BG_COMMANDS_ADD,IDC_BG_COMMANDS_EDIT,IDC_BG_COMMANDS_DELETE,IDC_BG_COMMANDS_UNDO,IDC_BG_COMMANDS_REDO};
    for (int n=0;n<5;n++) { MoveWindow(GetDlgItem(h,actions[n]),gap+n*(button+gap),toolbar,button,line,TRUE); }
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
        const char *labels[]={"#","Offset","Opcode","Command","References / arguments"};const int widths[]={34,62,42,200,280};
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
        { case IDCANCEL:DestroyWindow(h);return TRUE;case IDOK:Locate(h);return TRUE;case IDC_BG_COMMANDS_COPY:Copy(h);return TRUE;
          case IDC_BG_COMMANDS_ADD:EditCommand(h,BG_VIS_INSERT);return TRUE;
          case IDC_BG_COMMANDS_EDIT:EditCommand(h,BG_VIS_REPLACE);return TRUE;
          case IDC_BG_COMMANDS_DELETE:EditCommand(h,BG_VIS_DELETE);return TRUE;
          case IDC_BG_COMMANDS_UNDO:case IDC_BG_COMMANDS_REDO:
            SendMessage(GetWindow(h,GW_OWNER),BGCOMMANDS_WM_HISTORY,LOWORD(wp)==IDC_BG_COMMANDS_REDO,0);return TRUE; }
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
    BOOL ok=BgVisDecodeDocument(bg,doc,&next,&why);
    g_State.editable=ok && doc && doc->rooms && doc->viscommandsloaded && BgVisCanEdit(&next);
    g_State.rooms=doc ? doc->roomcount : 0;
    g_State.portalcount=doc && !doc->portalwarning && doc->portals.portalcount<BG_MAX_PORTALS ? doc->portals.portalcount : 0;
    if (g_State.portalcount) { memcpy(g_State.portals,doc->portals.portals,g_State.portalcount*sizeof(BgPortal)); }
    if (!ok) { snprintf(next.problem,sizeof(next.problem),"%s",why); }
    const char *name=level ? level : "No level",*file=bg ? bg->name : "";
    BgVisProgram *old=&g_State.program;BOOL samelevel=!strcmp(name,g_State.level) && !strcmp(file,g_State.file);
    if (samelevel && next.offset==old->offset && next.count==old->count && next.size==old->size
        && next.relativeOffsets==old->relativeOffsets && next.complete==old->complete && next.present==old->present && next.singleDisplayList==old->singleDisplayList
        && !strcmp(next.problem,old->problem) && (!next.size || !memcmp(next.data,old->data,next.size))
        && (!next.count || !memcmp(next.instructions,old->instructions,next.count*sizeof(*next.instructions))))
    { BgVisFree(&next);Details(g_Window);return; }
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
    if (!next.present) { snprintf(text,sizeof(text),"%s\r\n%s",file,next.problem[0] ? next.problem : next.singleDisplayList ? "Single-display-list background: no visibility commands." : "No special BG visibility commands. Use Add to create the first command."); }
    else { snprintf(text,sizeof(text),"%s - %lu instructions%s, %lu bytes, %lu warning rows.\r\n%s",file,(unsigned long)next.count,next.complete ? " (including END)" : " (partial)",(unsigned long)next.size,(unsigned long)next.warnings,next.problem[0] ? next.problem : g_State.editable ? "Edits apply immediately; Save Project writes them to the BG. Offsets are relative to the command stream." : "Inspect only: malformed or unsupported command structure prevents editing."); }
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
    if (msg->message==WM_KEYDOWN)
    {
        BOOL ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        if (ctrl && (msg->wParam=='Z' || msg->wParam=='Y'))
        { SendMessage(GetWindow(g_Window,GW_OWNER),BGCOMMANDS_WM_HISTORY,msg->wParam=='Y' || (GetKeyState(VK_SHIFT)&0x8000),0);return TRUE; }
        if (msg->hwnd==GetDlgItem(g_Window,IDC_BG_COMMANDS_LIST))
        {
            if (msg->wParam==VK_INSERT) { EditCommand(g_Window,BG_VIS_INSERT);return TRUE; }
            if (msg->wParam==VK_DELETE) { EditCommand(g_Window,BG_VIS_DELETE);return TRUE; }
            if (msg->wParam==VK_F2) { EditCommand(g_Window,BG_VIS_REPLACE);return TRUE; }
        }
    }
    if (!IsDialogMessage(g_Window,msg)) { TranslateMessage(msg);DispatchMessage(msg); }return TRUE;
}
