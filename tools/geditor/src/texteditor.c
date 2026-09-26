#include "../../../src/levelids.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "texteditor.h"
#include "textbank.h"
#include "editorpath.h"
#include "romexport.h"

enum { TE_FILES=2400, TE_FILTER, TE_STRINGS, TE_TEXT, TE_REVERT, TE_SAVE,
    TE_FILELABEL, TE_FILTERLABEL, TE_ID, TE_HELP, TE_STATUS };
typedef struct TextEditor {
    HWND window;
    const GEditorProject *project;
    RomFile rom;
    TextBankFile files[TEXT_BANK_MAX_FILES];
    DWORD count, file, slot;
    TextBank bank, saved;
    BOOL refreshing, pending, dirty, failed;
} TextEditor;
/* Labels/default selection only. Encoded bank IDs come from the ROM's TXBK
 * catalog, and resources are enumerated from its file table. */
static const struct { const char *file, *title; LONG level; } titles[]={
    {"LameE","Library",LEVELID_LIBRARY},{"LarchE","Archives",LEVELID_ARCHIVES},{"LarecE","Control",LEVELID_CONTROL},
    {"LarkE","Facility",LEVELID_FACILITY},{"LashE","Stack",LEVELID_STACK},{"LaztE","Aztec",LEVELID_AZTEC},
    {"LcatE","Citadel",LEVELID_CITADEL},{"LcaveE","Caverns",LEVELID_CAVERNS},{"LcradE","Cradle",LEVELID_CRADLE},
    {"LcrypE","Egypt",LEVELID_EGYPT},{"LdamE","Dam",LEVELID_DAM},{"LdepoE","Depot",LEVELID_DEPOT},
    {"LdestE","Frigate",LEVELID_FRIGATE},{"LdishE","Temple",LEVELID_TEMPLE},{"LimpE","Basement",LEVELID_BASEMENT},
    {"LjunE","Jungle",LEVELID_JUNGLE},{"LlenE","Cuba",LEVELID_CUBA},{"LoatE","Caves",LEVELID_CAVES},
    {"LpeteE","Streets",LEVELID_STREETS},{"LrefE","Complex",LEVELID_COMPLEX},{"LrunE","Runway",LEVELID_RUNWAY},
    {"LsevE","Bunker 1",LEVELID_BUNKER1},{"LsevbE","Bunker 2",LEVELID_BUNKER2},{"LsevxE","Surface 1",LEVELID_SURFACE},
    {"LsevxbE","Surface 2",LEVELID_SURFACE2},{"LsiloE","Silo",LEVELID_SILO},{"LstatE","Statue",LEVELID_STATUE},
    {"LtraE","Train",LEVELID_TRAIN},{"LgunE","Weapons",LEVELID_NONE},{"LtitleE","Stage and menu titles",LEVELID_NONE},
    {"LmpmenuE","Multiplayer menus",LEVELID_NONE},{"LpropobjE","Pickups",LEVELID_NONE},
    {"LmpweaponsE","Multiplayer weapons",LEVELID_NONE},{"LoptionsE","Solo menus",LEVELID_NONE},
    {"LmiscE","Cheat options",LEVELID_NONE}
};
static HWND Control(TextEditor *e, int id) { return GetDlgItem(e->window,id); }
static void Error(TextEditor *e, const char *why)
{ MessageBox(e->window,why,"Text Editor",MB_OK|MB_ICONEXCLAMATION); }
static BOOL Contains(const char *s, const char *part)
{
    if (!*part) { return TRUE; }
    for (;*s;s++)
    {
        const char *a=s,*b=part;
        while (*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) { return TRUE; }
    }
    return FALSE;
}
static void Title(TextEditor *e)
{
    char text[120];
    snprintf(text,sizeof(text),"Text Editor - %s%s",e->files[e->file].name,e->dirty || e->pending ? " *" : "");
    SetWindowText(e->window,text);
    EnableWindow(Control(e,TE_SAVE),e->dirty || e->pending);
}
static void Status(TextEditor *e)
{
    char text[256]; const char *s=TextBankString(&e->bank,e->slot); size_t n=s ? strlen(s) : 0;
    snprintf(text,sizeof(text),"%lu slots (including unused) | %lu bytes | %s%s%s",
        (unsigned long)e->bank.count,(unsigned long)e->bank.size,
        s ? (n && s[n-1]=='\n' ? "Ends with a newline" : "No trailing newline") : "Unused slot",
        e->dirty ? " | Unsaved file changes" : "",e->pending ? " | Text being edited" : "");
    SetWindowText(Control(e,TE_STATUS),text); Title(e);
}
static BOOL ApplyText(TextEditor *e)
{
    const char *why=""; char *text=NULL; wchar_t *wide; BOOL ok; int n;
    if (!e->pending) { return TRUE; }
    n=GetWindowTextLengthW(Control(e,TE_TEXT)); wide=malloc(((size_t)n+1)*sizeof(*wide));
    if (!wide) { Error(e,"Out of memory reading edited text."); return FALSE; }
    GetWindowTextW(Control(e,TE_TEXT),wide,n+1);
    ok=TextBankParse(wide,&text,&why) && TextBankSet(&e->bank,e->slot,text,&why);
    free(wide); free(text);
    if (!ok) { Error(e,why); SetFocus(Control(e,TE_TEXT)); return FALSE; }
    e->pending=FALSE; e->dirty=!TextBankEqual(e->bank.data,e->bank.size,e->saved.data,e->saved.size);
    Status(e); return TRUE;
}
static void ShowString(TextEditor *e)
{
    char label[200]; const char *s=TextBankString(&e->bank,e->slot); wchar_t *text=TextBankFormat(s);
    if (!text) { Error(e,"Out of memory displaying text."); return; }
    e->refreshing=TRUE;
    SetWindowTextW(Control(e,TE_TEXT),text); free(text);
    SendMessage(Control(e,TE_TEXT),EM_SETREADONLY,!s,0);
    EnableWindow(Control(e,TE_REVERT),s!=NULL);
    if (e->slot==TEXT_BANK_UNKNOWN_ID) { strcpy(label,"No matching strings"); }
    else if (e->files[e->file].id==TEXT_BANK_UNKNOWN_ID)
    { snprintf(label,sizeof(label),"Slot %lu [0x%03lX]%s",(unsigned long)e->slot,(unsigned long)e->slot,s ? "" : " - unused (NULL)"); }
    else
    { snprintf(label,sizeof(label),"Slot %lu [0x%03lX]    String ID: 0x%04lX%s",(unsigned long)e->slot,
        (unsigned long)e->slot,(unsigned long)((e->files[e->file].id<<10)|e->slot),s ? "" : " - unused (NULL)"); }
    SetWindowText(Control(e,TE_ID),label); e->pending=FALSE; e->refreshing=FALSE; Status(e);
}
static void SelectSlot(TextEditor *e)
{
    HWND list=Control(e,TE_STRINGS); LRESULT n=SendMessage(list,LB_GETCOUNT,0,0);
    for (LRESULT i=0;i<n;i++) if ((DWORD)SendMessage(list,LB_GETITEMDATA,i,0)==e->slot)
    { SendMessage(list,LB_SETCURSEL,i,0); return; }
}
static void ListStrings(TextEditor *e)
{
    char filter[256],id[100],preview[160],line[280]; HWND list=Control(e,TE_STRINGS);
    DWORD first=TEXT_BANK_UNKNOWN_ID; BOOL found=FALSE;
    GetWindowText(Control(e,TE_FILTER),filter,sizeof(filter));
    e->refreshing=TRUE; SendMessage(list,WM_SETREDRAW,FALSE,0); SendMessage(list,LB_RESETCONTENT,0,0);
    for (DWORD i=0;i<e->bank.count;i++)
    {
        const char *s=TextBankString(&e->bank,i); size_t at=0;
        if (e->files[e->file].id==TEXT_BANK_UNKNOWN_ID)
        { snprintf(id,sizeof(id),"%lu [0x%03lX]",(unsigned long)i,(unsigned long)i); }
        else { snprintf(id,sizeof(id),"%lu [0x%04lX]",(unsigned long)i,(unsigned long)((e->files[e->file].id<<10)|i)); }
        if (!Contains(id,filter) && (!s || !Contains(s,filter))) { continue; }
        if (!s) { strcpy(preview,"<unused>"); }
        else if (!*s) { strcpy(preview,"<empty string>"); }
        else
        {
            for (const unsigned char *p=(const unsigned char *)s;*p && at+5<sizeof(preview);p++)
            {
                if (*p=='\n') { preview[at++]='\\'; preview[at++]='n'; }
                else if (*p>=32 && *p<=126) { preview[at++]=*p; }
                else { snprintf(preview+at,sizeof(preview)-at,"\\x%02X",*p); at+=4; }
            }
            preview[at]=0;
        }
        snprintf(line,sizeof(line),"%s  %s",id,preview);
        LRESULT row=SendMessage(list,LB_ADDSTRING,0,(LPARAM)line);
        if (row>=0) { SendMessage(list,LB_SETITEMDATA,row,i); }
        if (first==TEXT_BANK_UNKNOWN_ID) { first=i; }
        if (i==e->slot) { found=TRUE; }
    }
    if (!found) { e->slot=first; }
    SelectSlot(e); SendMessage(list,WM_SETREDRAW,TRUE,0); InvalidateRect(list,NULL,TRUE);
    e->refreshing=FALSE; ShowString(e);
}
static BOOL Save(TextEditor *e)
{
    TextBank saved={0}; const char *why="";
    if (!ApplyText(e)) { return FALSE; }
    if (!e->dirty) { return TRUE; }
    /* Reserve the in-memory saved snapshot before publishing on disk. */
    if (!TextBankLoad(e->bank.data,e->bank.size,&saved,&why)
        || !TextBankSaveProject(e->project->dir,e->files[e->file].name,&e->bank,&why))
    { TextBankFree(&saved); Error(e,why); return FALSE; }
    TextBankFree(&e->saved); e->saved=saved; e->dirty=FALSE; ListStrings(e); return TRUE;
}
static BOOL Confirm(TextEditor *e)
{
    char question[200]; int answer;
    if (!e->dirty && !e->pending) { return TRUE; }
    snprintf(question,sizeof(question),"Save changes to %s?\n\nYes saves this text file to the project. No discards this file's unsaved changes.",e->files[e->file].name);
    answer=MessageBox(e->window,question,"Text Editor",MB_YESNOCANCEL|MB_ICONQUESTION);
    return answer==IDNO || (answer==IDYES && Save(e));
}
static BOOL Load(TextEditor *e, DWORD file)
{
    TextBank bank={0}, saved={0}; const char *why="";
    if (!TextBankLoadProject(e->project->dir,&e->rom,e->files[file].name,&bank,&why)
        || !TextBankLoad(bank.data,bank.size,&saved,&why))
    { TextBankFree(&bank); TextBankFree(&saved); Error(e,why); return FALSE; }
    TextBankFree(&e->bank); TextBankFree(&e->saved); e->bank=bank; e->saved=saved;
    e->file=file; e->slot=0; e->pending=e->dirty=FALSE;
    e->refreshing=TRUE; SetWindowText(Control(e,TE_FILTER),""); e->refreshing=FALSE;
    ListStrings(e); return TRUE;
}
static void Place(TextEditor *e, int id, int x, int y, int w, int h)
{ MoveWindow(Control(e,id),x,y,w,h,TRUE); }
static void Layout(TextEditor *e)
{
    RECT r; GetClientRect(e->window,&r); int w=r.right,h=r.bottom,left=w*2/5,right=left+24;
    Place(e,TE_FILELABEL,12,12,70,20); Place(e,TE_FILES,86,8,w-98,350);
    Place(e,TE_FILTERLABEL,12,46,70,20); Place(e,TE_FILTER,86,42,left-74,24);
    Place(e,TE_STRINGS,12,74,left,h-152); Place(e,TE_ID,right,46,w-right-12,24);
    Place(e,TE_TEXT,right,74,w-right-12,h-228);
    Place(e,TE_HELP,right,h-146,w-right-12,64);
    Place(e,TE_STATUS,12,h-68,w-24,22);
    Place(e,TE_REVERT,right,h-38,130,26); Place(e,TE_SAVE,w-226,h-38,110,26);
    Place(e,IDCANCEL,w-108,h-38,96,26);
}
static void Add(TextEditor *e, int id, const char *cls, const char *text, DWORD style)
{
    HWND control=CreateWindowEx(!strcmp(cls,"EDIT") || !strcmp(cls,"LISTBOX") ? WS_EX_CLIENTEDGE : 0,
        cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,e->window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    if (!control) { e->failed=TRUE; return; }
    SendMessage(control,WM_SETFONT,SendMessage(e->window,WM_GETFONT,0,0),TRUE);
}
static void Controls(TextEditor *e)
{
    Add(e,TE_FILELABEL,"STATIC","Text file",SS_NOPREFIX);
    Add(e,TE_FILES,"COMBOBOX","",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
    Add(e,TE_FILTERLABEL,"STATIC","Find / ID",SS_NOPREFIX);
    Add(e,TE_FILTER,"EDIT","",WS_TABSTOP|ES_AUTOHSCROLL);
    Add(e,TE_STRINGS,"LISTBOX","",WS_TABSTOP|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_HSCROLL);
    Add(e,TE_ID,"STATIC","",SS_NOPREFIX);
    /* Unicode input prevents ANSI conversion silently replacing pasted
     * unsupported characters before the byte-level validator sees them. */
    HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|
        ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,0,0,0,0,e->window,
        (HMENU)(INT_PTR)TE_TEXT,GetModuleHandle(NULL),NULL);
    if (!edit) { e->failed=TRUE; }
    SendMessage(edit,WM_SETFONT,(WPARAM)GetStockObject(ANSI_FIXED_FONT),TRUE);
    SendMessage(edit,EM_LIMITTEXT,4u*TEXT_BANK_MAX_SIZE,0);
    SendMessage(Control(e,TE_FILTER),EM_LIMITTEXT,255,0);
    SendMessage(Control(e,TE_STRINGS),LB_SETHORIZONTALEXTENT,1600,0);
    Add(e,TE_HELP,"STATIC","Enter keeps a line break; window wrapping does not. Escapes: \\\\, \\n, \\r, \\t, \\xHH.\r\nSave File writes to the project. Use Create ROM to play your edits.",SS_NOPREFIX);
    Add(e,TE_STATUS,"STATIC","",SS_NOPREFIX);
    Add(e,TE_REVERT,"BUTTON","Revert &string",WS_TABSTOP|BS_PUSHBUTTON);
    Add(e,TE_SAVE,"BUTTON","&Save File",WS_TABSTOP|BS_PUSHBUTTON);
    Add(e,IDCANCEL,"BUTTON","&Close",WS_TABSTOP|BS_PUSHBUTTON);
    for (DWORD i=0;i<e->count;i++)
    {
        const char *title=""; char label[160];
        for (size_t j=0;j<sizeof(titles)/sizeof(*titles);j++)
        { if (!strcmp(titles[j].file,e->files[i].name)) { title=titles[j].title; break; } }
        snprintf(label,sizeof(label),"%s%s%s",e->files[i].name,*title ? " - " : "",title);
        SendMessage(Control(e,TE_FILES),CB_ADDSTRING,0,(LPARAM)label);
    }
    SendMessage(Control(e,TE_FILES),CB_SETCURSEL,e->file,0);
}
static INT_PTR CALLBACK Dialog(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    TextEditor *e=(TextEditor *)GetWindowLongPtr(hwnd,DWLP_USER);
    if (message==WM_INITDIALOG)
    {
        e=(TextEditor *)l; e->window=hwnd; SetWindowLongPtr(hwnd,DWLP_USER,(LONG_PTR)e);
        Controls(e); if (e->failed) { EndDialog(hwnd,IDCANCEL); return TRUE; }
        Layout(e); ListStrings(e); return TRUE;
    }
    if (!e) { return FALSE; }
    switch (message)
    {
        case WM_SIZE: Layout(e); return TRUE;
        case WM_GETMINMAXINFO:
            ((MINMAXINFO *)l)->ptMinTrackSize.x=900; ((MINMAXINFO *)l)->ptMinTrackSize.y=540; return TRUE;
        case WM_CLOSE: if (Confirm(e)) { EndDialog(hwnd,IDCANCEL); } return TRUE;
        case WM_COMMAND:
        {
            int id=LOWORD(w),code=HIWORD(w);
            if (e->refreshing) { return TRUE; }
            if (id==IDCANCEL) { if (Confirm(e)) { EndDialog(hwnd,IDCANCEL); } return TRUE; }
            if (id==IDOK) { return TRUE; } /* Enter in the filter does not close the editor. */
            if (id==TE_TEXT && code==EN_CHANGE) { e->pending=TRUE; Title(e); return TRUE; }
            if (id==TE_FILTER && code==EN_CHANGE) { if (ApplyText(e)) { ListStrings(e); } return TRUE; }
            if (id==TE_FILES && code==CBN_SELCHANGE)
            {
                LRESULT file=SendMessage(Control(e,TE_FILES),CB_GETCURSEL,0,0);
                if (file>=0 && (DWORD)file!=e->file && Confirm(e)) { Load(e,(DWORD)file); }
                SendMessage(Control(e,TE_FILES),CB_SETCURSEL,e->file,0); return TRUE;
            }
            if (id==TE_STRINGS && code==LBN_SELCHANGE)
            {
                LRESULT row=SendMessage(Control(e,TE_STRINGS),LB_GETCURSEL,0,0);
                DWORD slot=row<0 ? TEXT_BANK_UNKNOWN_ID : (DWORD)SendMessage(Control(e,TE_STRINGS),LB_GETITEMDATA,row,0);
                if (ApplyText(e)) { e->slot=slot; ListStrings(e); } else { SelectSlot(e); }
                return TRUE;
            }
            if (id==TE_SAVE && code==BN_CLICKED) { Save(e); return TRUE; }
            if (id==TE_REVERT && code==BN_CLICKED)
            {
                const char *why="",*original=TextBankString(&e->saved,e->slot);
                if (original && TextBankSet(&e->bank,e->slot,original,&why))
                { e->pending=FALSE; e->dirty=!TextBankEqual(e->bank.data,e->bank.size,e->saved.data,e->saved.size); ListStrings(e); }
                else if (*why) { Error(e,why); }
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}
BOOL TextEditorShow(HWND owner, const GEditorProject *project, DWORD level, const char **why)
{
    TextEditor *e=calloc(1,sizeof(*e)); char path[MAX_PATH]; BOOL ok=FALSE;
    if (!e) { *why="Out of memory opening Text Editor."; return FALSE; }
    e->project=project;
    if (!EditorPathJoin(path,sizeof(path),project->dir,ROM_EXPORT_BASE_FILENAME))
    { *why="The project path is too long."; goto done; }
    if (!RomLoad(path,&e->rom,why) || !TextBankCatalog(&e->rom,e->files,&e->count,why)) { goto done; }
    if (!e->count) { *why="This ROM has no English text banks."; goto done; }
    if (level<project->levelcount)
    {
        LONG id=project->levels[level].levelID; if (LEVELID_IS_MP(id)) { id-=ENVIRONMENTDATA_PLAYERS_4; }
        for (size_t j=0;j<sizeof(titles)/sizeof(*titles);j++) if (titles[j].level==id)
        { for (DWORD i=0;i<e->count;i++) if (!strcmp(e->files[i].name,titles[j].file)) { e->file=i; break; } break; }
    }
    if (!TextBankLoadProject(project->dir,&e->rom,e->files[e->file].name,&e->bank,why)
        || !TextBankLoad(e->bank.data,e->bank.size,&e->saved,why)) { goto done; }
    ok=DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_TEXT_EDITOR),owner,Dialog,(LPARAM)e)!=-1 && !e->failed;
    *why=ok ? "" : "The Text Editor window could not be created.";
done:
    TextBankFree(&e->bank); TextBankFree(&e->saved); RomFree(&e->rom); free(e); return ok;
}
