#include "briefingpanel.h"
#include "resource.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

enum { PAGES=3400,HEADING,HELP,MENU_LABEL,WATCH_LABEL,MENU_SOURCE,WATCH_SOURCE,MENU_TEXT,WATCH_TEXT,
    ENABLED,MENU_DIFF,WATCH_DIFF,CONDITIONS,ADD,EDIT,REMOVE,RULE_HELP,STATUS,APPLY,REVERT,
    C_TYPE=3450,C_LABEL1,C_VALUE1,C_LABEL2,C_VALUE2,C_HELP };
typedef struct BriefingPanel {
    const GEditorProject *project; const SetupFile *setup; DWORD level;
    char projectpath[MAX_PATH]; BOOL ready,loading,draft; int page; unsigned generation;
    BriefingDocument doc; TextBank bank;
    unsigned char *committed; DWORD committedsize;
} BriefingPanel;
static BriefingPanel *State(HWND h) { return (BriefingPanel *)GetWindowLongPtr(h,DWLP_USER); }
static HWND Owner(HWND h) { return GetWindow(GetParent(h),GW_OWNER); }
static HWND Control(HWND h,const char *type,const char *text,int id,DWORD style)
{
    HWND c=CreateWindowEx(!strcmp(type,"EDIT") || !strcmp(type,"LISTBOX") ? WS_EX_CLIENTEDGE : 0,type,text,
        WS_CHILD|WS_VISIBLE|style,0,0,1,1,h,(HMENU)(INT_PTR)id,(HINSTANCE)GetWindowLongPtr(h,GWLP_HINSTANCE),NULL);
    if (c) { SendMessage(c,WM_SETFONT,SendMessage(h,WM_GETFONT,0,0),TRUE); }return c;
}
static void Place(HWND h,int id,int x,int y,int w,int height)
{ MoveWindow(GetDlgItem(h,id),x,y,max(1,w),max(1,height),TRUE); }
static void Draft(HWND h,BOOL draft)
{
    BriefingPanel *s=State(h);s->draft=draft;
    SetDlgItemText(h,STATUS,draft ? "Unapplied changes. Save Project keeps edits; Create ROM builds the game text." : "Applied to this level. Save Project keeps edits; Create ROM builds the game text.");
    EnableWindow(GetDlgItem(h,APPLY),s->ready && draft);EnableWindow(GetDlgItem(h,REVERT),s->ready && draft);
    SendMessage(Owner(h),BRIEFING_WM_DRAFT,0,0);
}
static BriefingText *PageText(BriefingPanel *s,int watch)
{
    if (s->page==0) { return watch ? &s->doc.watch[0] : NULL; }
    if (s->page<5) { return watch ? &s->doc.watch[s->page] : &s->doc.menu[s->page-1]; }
    return watch ? &s->doc.objectives[s->page-5].watch : &s->doc.objectives[s->page-5].menu;
}
static void SetText(HWND h,int id,const char *text)
{
    wchar_t wide[BRIEFING_TEXT_SIZE*2];size_t n=0;
    for (const unsigned char *p=(const unsigned char *)text;*p && n+2<sizeof(wide)/sizeof(*wide);p++)
    { if (*p=='\n') { wide[n++]=L'\r'; }wide[n++]=*p; }wide[n]=0;
    SetWindowTextW(GetDlgItem(h,id),wide);
}
static BOOL Capture(HWND h)
{
    BriefingPanel *s=State(h);if (!s->ready) { return TRUE; }
    for (int col=0;col<2;col++)
    {
        BriefingText *t=PageText(s,col);if (!t || !t->custom) { continue; }
        wchar_t wide[BRIEFING_TEXT_SIZE*2];char text[BRIEFING_TEXT_SIZE];size_t n=0;
        GetWindowTextW(GetDlgItem(h,MENU_TEXT+col),wide,sizeof(wide)/sizeof(*wide));
        for (const wchar_t *p=wide;*p;p++)
        {
            if (*p==L'\r' && p[1]==L'\n') { continue; }
            if ((*p!=L'\n' && (*p<32 || *p>126)) || n+1>=sizeof(text))
            { MessageBox(h,"Use English ASCII and line breaks, up to 1499 bytes per string.","Briefing",MB_ICONWARNING);SetFocus(GetDlgItem(h,MENU_TEXT+col));return FALSE; }
            text[n++]=(char)*p;
        }
        if (s->page>=5 && col==1 && n && text[n-1]!='\n' && n+1<sizeof(text)) { text[n++]='\n'; }
        text[n]=0;strcpy(t->text,text);
    }
    return TRUE;
}
static void Layout(HWND h)
{
    BriefingPanel *s=State(h);if (!s) { return; }
    RECT r,u={0,0,8,14};GetClientRect(h,&r);MapDialogRect(h,&u);
    int gap=max(4,u.right/2),line=u.bottom,left=u.right*11,x=left+gap,w=max(1,r.right-x),col=(w-gap)/2;
    BOOL objective=s->page>=5;
    Place(h,PAGES,0,0,left,max(1,r.bottom));
    Place(h,HEADING,x,0,w,line);Place(h,HELP,x,line+gap,w,line*2);
    int y=line*3+gap*2;
    Place(h,ENABLED,x,y,w,line);y+=objective ? line+gap : 0;
    for (int c=0;c<2;c++)
    { Place(h,MENU_LABEL+c,x+c*(col+gap),y,col,line);Place(h,MENU_SOURCE+c,x+c*(col+gap),y+line,col,line*12); }
    y+=line*2+gap;
    int footer=line*3+gap*2,conditions=objective ? line*6+gap*3 : 0;
    int height=max(line*2,r.bottom-y-footer-conditions);
    for (int c=0;c<2;c++) { Place(h,MENU_TEXT+c,x+c*(col+gap),y,col,height); }
    y+=height+gap;
    for (int c=0;c<2;c++) { Place(h,MENU_DIFF+c,x+c*(col+gap),y,col,line*8); }
    y+=line+gap;
    int buttons=u.right*7;
    Place(h,CONDITIONS,x,y,w-buttons-gap,line*4);
    Place(h,ADD,x+w-buttons,y,buttons,line);Place(h,EDIT,x+w-buttons,y+line+gap,buttons,line);
    Place(h,REMOVE,x+w-buttons,y+2*(line+gap),buttons,line);
    Place(h,RULE_HELP,x,y+line*4+gap,w,line);
    Place(h,STATUS,x,r.bottom-footer,w,line*2);
    Place(h,APPLY,x,r.bottom-line-gap,buttons,line+gap);
    Place(h,REVERT,x+buttons+gap,r.bottom-line-gap,buttons+u.right*2,line+gap);
    for (int id=ENABLED;id<=RULE_HELP;id++) { ShowWindow(GetDlgItem(h,id),objective ? SW_SHOW : SW_HIDE); }
}
static void FillConditions(HWND h)
{
    BriefingPanel *s=State(h);SendDlgItemMessage(h,CONDITIONS,LB_RESETCONTENT,0,0);
    if (s->page<5) { return; }
    BriefingObjective *o=&s->doc.objectives[s->page-5];
    for (DWORD i=0;i<o->count;i++)
    {
        BriefingCondition *c=&o->conditions[i];DWORD type=c->words[0]&255;char row[200];
        if (type==33) { snprintf(row,sizeof(row),"%lu. %s: weapon %lu, pad %lu",(unsigned long)i+1,BriefingConditionName(type),(unsigned long)c->words[1],(unsigned long)c->words[2]); }
        else if (type==26 || type==27) { snprintf(row,sizeof(row),"%lu. %s: 0x%08lX",(unsigned long)i+1,BriefingConditionName(type),(unsigned long)c->words[1]); }
        else if (type==31 || type==34) { snprintf(row,sizeof(row),"%lu. %s",(unsigned long)i+1,BriefingConditionName(type)); }
        else { snprintf(row,sizeof(row),"%lu. %s: %lu",(unsigned long)i+1,BriefingConditionName(type),(unsigned long)c->words[1]); }
        SendDlgItemMessage(h,CONDITIONS,LB_ADDSTRING,0,(LPARAM)row);
    }
    EnableWindow(GetDlgItem(h,ADD),s->ready && o->enabled && o->count<BRIEFING_CONDITIONS);
    EnableWindow(GetDlgItem(h,EDIT),FALSE);EnableWindow(GetDlgItem(h,REMOVE),FALSE);
}
static void Display(HWND h)
{
    BriefingPanel *s=State(h);s->loading=TRUE;
    const char *names[]={"Watch title","Background","M briefing","Q briefing","Moneypenny briefing"};char title[80];
    if (s->page<5) { snprintf(title,sizeof(title),"%s",names[s->page]); }
    else { snprintf(title,sizeof(title),"Objective %d (Action Block objective ID %d)",s->page-4,s->page-5); }
    SetDlgItemText(h,HEADING,title);
    SetDlgItemText(h,HELP,s->page>=5
        ? "Choose an existing string, or type custom text. Watch: up to 120 bytes / 2 lines.\r\nA final line break is added to custom watch objectives when applied."
        : "Choose an existing string, or type to create custom text for this page.\r\nMenu and watch text are independent. Enter inserts a line break.");
    for (int col=0;col<2;col++)
    {
        BriefingText *t=PageText(s,col);HWND combo=GetDlgItem(h,MENU_SOURCE+col);SendMessage(combo,CB_RESETCONTENT,0,0);
        SendMessage(combo,CB_ADDSTRING,0,(LPARAM)"Custom text");SendMessage(combo,CB_SETITEMDATA,0,(LPARAM)-1);
        int selected=0;
        for (DWORD i=0;s->ready && i<s->bank.count;i++)
        {
            const char *text=TextBankString(&s->bank,i);if (!text) { continue; }
            char row[140];DWORD id=(s->doc.bank<<10)|i;snprintf(row,sizeof(row),"0x%04lX  %.110s",(unsigned long)id,text);
            for (char *p=row;*p;p++) { if (*p=='\n' || *p=='\r') { *p=' '; } }
            int n=(int)SendMessage(combo,CB_ADDSTRING,0,(LPARAM)row);SendMessage(combo,CB_SETITEMDATA,n,id);
            if (t && !t->custom && t->id==id) { selected=n; }
        }
        SendMessage(combo,CB_SETCURSEL,selected,0);EnableWindow(combo,s->ready && t);
        SetText(h,MENU_TEXT+col,s->ready && t ? t->text : "");EnableWindow(GetDlgItem(h,MENU_TEXT+col),s->ready && t);
    }
    if (s->page>=5)
    {
        BriefingObjective *o=&s->doc.objectives[s->page-5];CheckDlgButton(h,ENABLED,o->enabled ? BST_CHECKED : BST_UNCHECKED);
        SendDlgItemMessage(h,MENU_DIFF,CB_SETCURSEL,o->menuDifficulty,0);SendDlgItemMessage(h,WATCH_DIFF,CB_SETCURSEL,o->watchDifficulty,0);
        EnableWindow(GetDlgItem(h,MENU_DIFF),s->ready && o->enabled);EnableWindow(GetDlgItem(h,WATCH_DIFF),s->ready && o->enabled);
    }
    EnableWindow(GetDlgItem(h,ENABLED),s->ready);FillConditions(h);Layout(h);s->loading=FALSE;
}
static void Load(HWND h)
{
    BriefingPanel *s=State(h);const char *why="";s->generation++;TextBankFree(&s->bank);free(s->committed);s->committed=NULL;s->committedsize=0;
    s->ready=BriefingLoad(s->project,s->level,s->setup,&s->doc,&s->bank,&why);
    if (s->ready && s->setup->briefmetasize)
    {
        s->committed=malloc(s->setup->briefmetasize);
        if (!s->committed) { s->ready=FALSE;why="Out of memory loading briefing edits."; }
        else { memcpy(s->committed,s->setup->briefmeta,s->setup->briefmetasize);s->committedsize=s->setup->briefmetasize; }
    }
    Display(h);Draft(h,FALSE);EnableWindow(GetDlgItem(h,PAGES),s->ready);
    if (!s->ready) { SetDlgItemText(h,STATUS,why); }
}
void BriefingPanelRefresh(HWND h,const GEditorProject *project,DWORD level,const SetupFile *setup)
{
    BriefingPanel *s=h ? State(h) : NULL;if (!s) { return; }
    BOOL same=project && s->project==project && s->level==level && !strcmp(s->projectpath,project->geppath);
    s->setup=setup;
    if (same && s->ready && (s->draft || (setup && setup->briefmetasize==s->committedsize
        && (!s->committedsize || !memcmp(setup->briefmeta,s->committed,s->committedsize))))) { return; }
    s->project=project;s->level=level;lstrcpyn(s->projectpath,project ? project->geppath : "",sizeof(s->projectpath));Load(h);
}
BOOL BriefingPanelHasDraft(HWND h) { BriefingPanel *s=h ? State(h) : NULL;return s && s->draft; }
BOOL BriefingPanelApply(HWND h)
{
    BriefingPanel *s=h ? State(h) : NULL;if (!s || !s->ready || !s->draft) { return TRUE; }
    BriefingEditRequest request={&s->doc,&s->bank,""};
    if (!Capture(h)) { return FALSE; }
    if (!BriefingValidate(&s->doc,&s->bank,&request.why) || !SendMessage(Owner(h),BRIEFING_WM_APPLY,0,(LPARAM)&request))
    { MessageBox(h,request.why[0] ? request.why : "The briefing could not be applied.","Briefing",MB_ICONWARNING);return FALSE; }
    Load(h);return TRUE;
}
static void ConditionFields(HWND h,BriefingCondition *c)
{
    DWORD type=c->words[0]&255;
    SetDlgItemText(h,C_LABEL1,type==26 || type==27 ? "Stage flag mask (decimal or 0x hex)" : type==32 ? "Pad ID (bound pad = 10000 + index)" : type==33 ? "Weapon ID" : "Object tag ID");
    SetDlgItemText(h,C_LABEL2,"Pad ID (bound pad = 10000 + index)");
    SetDlgItemText(h,C_HELP,type==34 ? "Completes when the key analyzer has copied the GoldenEye key." : type==31 ? "Always complete. This condition adds no requirement." : type==33 ? "Completes when the weapon is used/deposited in the pad's room." : "Use IDs from this level. Stage flags are set and cleared by Action Blocks.");
    char value[32];snprintf(value,sizeof(value),"0x%lX",(unsigned long)c->words[1]);SetDlgItemText(h,C_VALUE1,value);
    snprintf(value,sizeof(value),"%lu",(unsigned long)c->words[2]);SetDlgItemText(h,C_VALUE2,value);
    for (int id=C_LABEL1;id<=C_VALUE1;id++) { ShowWindow(GetDlgItem(h,id),type!=31 && type!=34 ? SW_SHOW : SW_HIDE); }
    for (int id=C_LABEL2;id<=C_VALUE2;id++) { ShowWindow(GetDlgItem(h,id),type==33 ? SW_SHOW : SW_HIDE); }
}
static INT_PTR CALLBACK ConditionDialog(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
    BriefingCondition *c=(BriefingCondition *)GetWindowLongPtr(h,DWLP_USER);
    if (msg==WM_INITDIALOG)
    {
        c=(BriefingCondition *)lp;SetWindowLongPtr(h,DWLP_USER,lp);
        for (int type=25;type<=34;type++) { SendDlgItemMessage(h,C_TYPE,CB_ADDSTRING,0,(LPARAM)BriefingConditionName(type)); }
        SendDlgItemMessage(h,C_TYPE,CB_SETCURSEL,(c->words[0]&255)-25,0);ConditionFields(h,c);return TRUE;
    }
    if (msg==WM_CLOSE || (msg==WM_COMMAND && LOWORD(wp)==IDCANCEL)) { EndDialog(h,IDCANCEL);return TRUE; }
    if (msg!=WM_COMMAND || !c) { return FALSE; }
    if (LOWORD(wp)==C_TYPE && HIWORD(wp)==CBN_SELCHANGE)
    {
        DWORD type=(DWORD)SendDlgItemMessage(h,C_TYPE,CB_GETCURSEL,0,0)+25;
        memset(c,0,sizeof(*c));c->words[0]=type;
        if (type==34) { c->words[1]=29;c->words[2]=4; }ConditionFields(h,c);return TRUE;
    }
    if (LOWORD(wp)==IDOK)
    {
        DWORD type=c->words[0]&255;
        for (int i=0;i<(type==33 ? 2 : type==31 || type==34 ? 0 : 1);i++)
        {
            char text[64],*end;GetDlgItemText(h,C_VALUE1+i*2,text,sizeof(text));errno=0;
            const char *start=text;while (isspace((unsigned char)*start)) { start++; }
            unsigned long long value=strtoull(start,&end,start[0]=='0' && (start[1]=='x' || start[1]=='X') ? 16 : 10);
            while (isspace((unsigned char)*end)) { end++; }
            DWORD limit=type==26 || type==27 ? 0xffffffffu : 65535;
            if (!*start || start==end || *start=='-' || *end || errno==ERANGE || value>limit || ((type==26 || type==27) && !value))
            { MessageBox(h,"Enter a valid nonnegative ID, or a nonzero 32-bit stage flag mask.","Objective condition",MB_ICONWARNING);SetFocus(GetDlgItem(h,C_VALUE1+i*2));return TRUE; }
            c->words[i+1]=(DWORD)value;
        }
        EndDialog(h,IDOK);return TRUE;
    }
    return FALSE;
}
static void EditCondition(HWND h,BOOL add)
{
    BriefingPanel *s=State(h);if (!s->ready || s->page<5) { return; }
    BriefingObjective *o=&s->doc.objectives[s->page-5];int row=(int)SendDlgItemMessage(h,CONDITIONS,LB_GETCURSEL,0,0);
    if (!o->enabled || (add ? o->count>=BRIEFING_CONDITIONS : row<0 || (DWORD)row>=o->count)) { return; }
    BriefingCondition c=add ? (BriefingCondition){{26,1}} : o->conditions[row];
    unsigned generation=s->generation;
    if (DialogBoxParam((HINSTANCE)GetWindowLongPtr(h,GWLP_HINSTANCE),MAKEINTRESOURCE(IDD_BRIEFING_CONDITION),h,ConditionDialog,(LPARAM)&c)!=IDOK) { return; }
    if (State(h)!=s) { return; }
    if (s->generation!=generation)
    { MessageBox(h,"The level was refreshed while this condition was open. Please edit the condition again.","Briefing",MB_ICONINFORMATION);return; }
    if (add) { row=(int)o->count++; }o->conditions[row]=c;FillConditions(h);Draft(h,TRUE);
}
static INT_PTR CALLBACK Dialog(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
    BriefingPanel *s=State(h);
    if (msg==WM_INITDIALOG)
    {
        s=calloc(1,sizeof(*s));if (!s) { return FALSE; }SetWindowLongPtr(h,DWLP_USER,(LONG_PTR)s);s->loading=TRUE;
        BOOL ok=Control(h,"LISTBOX","",PAGES,WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT)!=NULL;
        const char *pages[]={"Watch title","Background","M","Q","Moneypenny","Objective 1","Objective 2","Objective 3","Objective 4","Objective 5","Objective 6"};
        for (int i=0;i<11;i++) { SendDlgItemMessage(h,PAGES,LB_ADDSTRING,0,(LPARAM)pages[i]); }SendDlgItemMessage(h,PAGES,LB_SETCURSEL,0,0);
        ok=ok && Control(h,"STATIC","",HEADING,0) && Control(h,"STATIC","",HELP,0);
        for (int i=0;i<2;i++)
        {
            ok=ok && Control(h,"STATIC",i ? "Watch" : "Mission menu",MENU_LABEL+i,0)
                && Control(h,"COMBOBOX","",MENU_SOURCE+i,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL)
                && Control(h,"EDIT","",MENU_TEXT+i,WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL)
                && Control(h,"COMBOBOX","",MENU_DIFF+i,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
            SendDlgItemMessage(h,MENU_TEXT+i,EM_SETLIMITTEXT,BRIEFING_TEXT_SIZE*2-1,0);
            const char *diffs[]={"Agent and higher","Secret Agent and higher","00 Agent and 007","007 only"};
            for (int j=0;j<4;j++) { char label[80];snprintf(label,sizeof(label),"%s: %s",i ? "Watch / gameplay" : "Menu",diffs[j]);SendDlgItemMessage(h,MENU_DIFF+i,CB_ADDSTRING,0,(LPARAM)label); }
        }
        ok=ok && Control(h,"BUTTON","Enable this objective",ENABLED,WS_TABSTOP|BS_AUTOCHECKBOX)
            && Control(h,"LISTBOX","",CONDITIONS,WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT)
            && Control(h,"BUTTON","Add...",ADD,WS_TABSTOP) && Control(h,"BUTTON","Edit...",EDIT,WS_TABSTOP)
            && Control(h,"BUTTON","Remove",REMOVE,WS_TABSTOP)
            && Control(h,"STATIC","All completion conditions must pass. Any failure condition fails the objective.",RULE_HELP,0)
            && Control(h,"STATIC","",STATUS,0) && Control(h,"BUTTON","Apply",APPLY,WS_TABSTOP)
            && Control(h,"BUTTON","Revert edits",REVERT,WS_TABSTOP);
        if (!ok) { DestroyWindow(h);return FALSE; }s->loading=FALSE;Load(h);return TRUE;
    }
    if (msg==WM_SIZE) { Layout(h);return TRUE; }
    if (msg==WM_NCDESTROY) { SetWindowLongPtr(h,DWLP_USER,0);if (s) { TextBankFree(&s->bank);free(s->committed);free(s); }return TRUE; }
    if (msg!=WM_COMMAND || !s || s->loading) { return FALSE; }
    int id=LOWORD(wp),event=HIWORD(wp);
    if (id==APPLY || id==IDOK) { BriefingPanelApply(h);return TRUE; }
    if (id==REVERT) { Load(h);return TRUE; }
    if (!s->ready) { return TRUE; }
    if (id==PAGES && event==LBN_SELCHANGE)
    { int page=(int)SendDlgItemMessage(h,PAGES,LB_GETCURSEL,0,0);if (page>=0 && Capture(h)) { s->page=page;Display(h); }else { SendDlgItemMessage(h,PAGES,LB_SETCURSEL,s->page,0); }return TRUE; }
    if ((id==MENU_TEXT || id==WATCH_TEXT) && event==EN_CHANGE)
    { BriefingText *t=PageText(s,id-MENU_TEXT);if (t) { t->custom=TRUE;SendDlgItemMessage(h,MENU_SOURCE+id-MENU_TEXT,CB_SETCURSEL,0,0);Draft(h,TRUE); }return TRUE; }
    if ((id==MENU_SOURCE || id==WATCH_SOURCE) && event==CBN_SELCHANGE)
    {
        int col=id-MENU_SOURCE,sel=(int)SendDlgItemMessage(h,id,CB_GETCURSEL,0,0);BriefingText *t=PageText(s,col);if (!t || sel<0) { return TRUE; }
        if (!Capture(h)) { Display(h);return TRUE; }
        t->custom=sel==0;
        if (!t->custom) { t->id=(DWORD)SendDlgItemMessage(h,id,CB_GETITEMDATA,sel,0);const char *v=TextBankString(&s->bank,t->id&1023);
            if (!v || strlen(v)>=sizeof(t->text)) { t->custom=TRUE;MessageBox(h,"This string is too long for a briefing.","Briefing",MB_ICONWARNING); }
            else { strcpy(t->text,v); } }
        Display(h);Draft(h,TRUE);return TRUE;
    }
    if (id==ENABLED && event==BN_CLICKED)
    { if (!Capture(h)) { return TRUE; }s->doc.objectives[s->page-5].enabled=IsDlgButtonChecked(h,ENABLED)==BST_CHECKED;Display(h);Draft(h,TRUE);return TRUE; }
    if ((id==MENU_DIFF || id==WATCH_DIFF) && event==CBN_SELCHANGE)
    { BriefingObjective *o=&s->doc.objectives[s->page-5];DWORD value=(DWORD)SendDlgItemMessage(h,id,CB_GETCURSEL,0,0);if (id==MENU_DIFF) { o->menuDifficulty=value; }else { o->watchDifficulty=value; }Draft(h,TRUE);return TRUE; }
    if (id==ADD || id==EDIT || (id==CONDITIONS && event==LBN_DBLCLK)) { EditCondition(h,id==ADD);return TRUE; }
    if (id==CONDITIONS && event==LBN_SELCHANGE)
    { BOOL enabled=s->doc.objectives[s->page-5].enabled && SendDlgItemMessage(h,CONDITIONS,LB_GETCURSEL,0,0)!=LB_ERR;EnableWindow(GetDlgItem(h,EDIT),enabled);EnableWindow(GetDlgItem(h,REMOVE),enabled);return TRUE; }
    if (id==REMOVE)
    {
        BriefingObjective *o=&s->doc.objectives[s->page-5];int row=(int)SendDlgItemMessage(h,CONDITIONS,LB_GETCURSEL,0,0);
        if (o->enabled && row>=0 && (DWORD)row<o->count) { memmove(o->conditions+row,o->conditions+row+1,(o->count-row-1)*sizeof(*o->conditions));memset(o->conditions+--o->count,0,sizeof(*o->conditions));FillConditions(h);Draft(h,TRUE); }return TRUE;
    }
    return FALSE;
}
HWND BriefingPanelCreate(HWND parent,HINSTANCE instance)
{ HWND h=CreateDialog(instance,MAKEINTRESOURCE(IDD_BRIEFING),parent,Dialog);if (h && !State(h)) { DestroyWindow(h);return NULL; }return h; }
