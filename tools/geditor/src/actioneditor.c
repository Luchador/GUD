#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "actioneditor.h"
#include "resource.h"
#include "romexport.h"

enum {
    AE_BLOCKS=2000, AE_FILTER, AE_NEW, AE_NEWLEVEL, AE_DUPLICATE, AE_DELETEBLOCK,
    AE_STEPS, AE_BLOCKNAME, AE_NAME, AE_NOTE, AE_DEBUGTEXT, AE_APPLYSTEP, AE_FOLLOW,
    AE_SEARCH, AE_COMMAND, AE_INSERT, AE_DELETE, AE_UP, AE_DOWN, AE_UNDO, AE_REDO,
    AE_VALIDATE, AE_ISSUES, AE_CHARACTERS, AE_ASSIGN, AE_USED, AE_HELP, AE_STATUS,
    AE_BLOCKLABEL, AE_NAMELABEL, AE_NOTELABEL, AE_TEXTLABEL, AE_SEARCHLABEL,
    AE_CHARLABEL, AE_USEDLABEL, AE_ISSUELABEL, AE_PARAM=2100, AE_PARAMLABEL=2120
};
#define AE_HISTORY 32
#define AE_HISTORY_BYTES (64u * 1024u * 1024u)
typedef struct ActionSnapshot { ActionDocument doc; DWORD block, row; } ActionSnapshot;
typedef struct ActionEditor {
    HWND window;
    const SetupFile *setup;
    SetupFile *result;
    ActionDocument doc;
    ActionSnapshot undo[AE_HISTORY], redo[AE_HISTORY];
    DWORD undocount, redocount, block, row, selectedcharacter;
    BOOL refreshing, pending, accepted, failed;
    BOOL typed[8];
    ActionIssue *issues;
    DWORD issuecount;
    char globalstatus[160];
} ActionEditor;
static HWND Control(ActionEditor *e, int id) { return GetDlgItem(e->window,id); }
static void Error(ActionEditor *e, const char *why)
{ MessageBox(e->window,why,"Action Blocks",MB_OK|MB_ICONEXCLAMATION); }
static void SetText(ActionEditor *e, int id, const char *s) { SetWindowText(Control(e,id),s); }
static void GetText(ActionEditor *e, int id, char *s, int n) { GetWindowText(Control(e,id),s,n); }
static void Enable(ActionEditor *e, int id, BOOL enabled) { EnableWindow(Control(e,id),enabled); }
static void Place(ActionEditor *e, int id, int x, int y, int w, int h)
{ MoveWindow(Control(e,id),x,y,w,h,TRUE); }
static BOOL Contains(const char *s, const char *part)
{
    for (;*s;s++)
    {
        const char *a=s,*b=part;
        while (*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) { return TRUE; }
    }
    return !*part;
}
static ActionBlock *Block(ActionEditor *e) { return e->block<e->doc.count ? &e->doc.blocks[e->block] : NULL; }
static ActionInstruction *Instruction(ActionEditor *e)
{ ActionBlock *b=Block(e); return b && e->row<b->count ? &b->instructions[e->row] : NULL; }
static int BlockParameter(const ActionInstruction *ins)
{
    if (ins) for (int p=0;p<g_ActionOpcodes[ins->bytes[0]].paramcount;p++)
    { if (g_ActionOpcodes[ins->bytes[0]].params[p].kind==ACTION_BLOCK) { return p; } }
    return -1;
}
static void BlockTitle(ActionEditor *e, DWORD index, char *text, size_t size)
{
    ActionBlockTitle(&e->doc.blocks[index],text,size);
    for (DWORD i=0;i<e->doc.count;i++) if (i!=index && e->doc.blocks[i].id==e->doc.blocks[index].id)
    {
        size_t n=strlen(text); snprintf(text+n,size-n," (table entry %lu)",(unsigned long)index+1); break;
    }
}
static BOOL Editable(ActionEditor *e) { ActionBlock *b=Block(e); return b && !b->global; }
static DWORD ListData(HWND list, UINT selection, UINT data)
{
    LRESULT row=SendMessage(list,selection,0,0);
    return row<0 ? ACTION_MISSING_TARGET : (DWORD)SendMessage(list,data,row,0);
}
static void Choice(HWND c, const char *text, DWORD value)
{
    LRESULT row=SendMessage(c,CB_ADDSTRING,0,(LPARAM)text);
    if (row>=0) { SendMessage(c,CB_SETITEMDATA,row,value); }
}
static void SelectChoice(HWND c, DWORD value)
{
    LRESULT n=SendMessage(c,CB_GETCOUNT,0,0);
    for (LRESULT i=0;i<n;i++) if ((DWORD)SendMessage(c,CB_GETITEMDATA,i,0)==value)
    { SendMessage(c,CB_SETCURSEL,i,0); return; }
}
static void ClearHistory(ActionSnapshot *s, DWORD *count)
{ while (*count) { ActionDocumentFree(&s[--*count].doc); } }
static size_t DocumentSize(const ActionDocument *d)
{
    size_t n=sizeof(*d)+(size_t)d->count*sizeof(*d->blocks)+d->charactercount*sizeof(*d->assignments);
    for (DWORD b=0;b<d->count;b++)
    {
        n+=(size_t)d->blocks[b].count*sizeof(ActionInstruction);
        for (DWORD i=0;i<d->blocks[b].count;i++) { n+=d->blocks[b].instructions[i].size; }
    }
    return n;
}
static void Push(ActionSnapshot *list, DWORD *count, ActionDocument *doc, DWORD block, DWORD row)
{
    size_t bytes=DocumentSize(doc);
    for (DWORD i=0;i<*count;i++) { bytes+=DocumentSize(&list[i].doc); }
    while (*count && (*count>=AE_HISTORY || bytes>AE_HISTORY_BYTES))
    {
        bytes-=DocumentSize(&list[0].doc); ActionDocumentFree(&list[0].doc);
        memmove(list,list+1,(--*count)*sizeof(*list));
    }
    list[*count].doc=*doc; list[*count].block=block; list[*count].row=row; (*count)++;
    memset(doc,0,sizeof(*doc));
}
static BOOL Begin(ActionEditor *e, ActionDocument *next)
{
    const char *why;
    if (!ActionDocumentClone(&e->doc,next,&why)) { Error(e,why); return FALSE; }
    return TRUE;
}
static void Commit(ActionEditor *e, ActionDocument *next)
{
    Push(e->undo,&e->undocount,&e->doc,e->block,e->row);
    ClearHistory(e->redo,&e->redocount); e->doc=*next; memset(next,0,sizeof(*next));
    free(e->issues); e->issues=NULL; e->issuecount=0;
    SendMessage(Control(e,AE_ISSUES),LB_RESETCONTENT,0,0);
    SetText(e,AE_STATUS,"Draft changed. Apply to project when you are finished.");
}
static void ParamLabel(const ActionParam *p, char *text, size_t size)
{
    size_t j=0; BOOL start=TRUE;
    if (p->kind==ACTION_LABEL) { snprintf(text,size,"Destination when true / successful"); return; }
    for (const char *s=p->name;*s && j+1<size;s++)
    {
        text[j++]=*s=='_' ? ' ' : start ? (char)toupper((unsigned char)*s) : (char)tolower((unsigned char)*s);
        start=*s=='_';
    }
    text[j]=0;
    if (p->kind==ACTION_METERS) { snprintf(text+j,size-j," (metres)"); }
    else if (p->kind==ACTION_SECONDS60 || p->kind==ACTION_SECONDS30) { snprintf(text+j,size-j," (seconds)"); }
}
static void FormatNumber(const ActionParam *p, DWORD value, char *s, size_t n)
{
    if (p->kind==ACTION_BLOCK || p->kind==ACTION_MASK) { snprintf(s,n,"0x%lX",(unsigned long)value); }
    else { snprintf(s,n,"%.12g",ActionDisplayValue(p,value)); }
}
static void DisplayInstruction(ActionEditor *e)
{
    ActionBlock *b=Block(e); ActionInstruction *ins=Instruction(e);
    const ActionOpcode *op=ins ? &g_ActionOpcodes[ins->bytes[0]] : NULL;
    char text[1200]; BOOL editable=Editable(e);
    e->refreshing=TRUE;
    SetText(e,AE_NAME,ins ? ins->name : ""); SetText(e,AE_NOTE,ins ? ins->note : "");
    SetText(e,AE_DEBUGTEXT,ins && ins->bytes[0]==0xad ? (char *)ins->bytes+1 : "");
    for (int p=0;p<8;p++)
    {
        HWND c=Control(e,AE_PARAM+p); BOOL shown=op && p<op->paramcount;
        e->typed[p]=FALSE;
        SendMessage(c,CB_RESETCONTENT,0,0); SetWindowText(c,"");
        ShowWindow(c,shown ? SW_SHOW : SW_HIDE); ShowWindow(Control(e,AE_PARAMLABEL+p),shown ? SW_SHOW : SW_HIDE);
        EnableWindow(c,editable && ins && ins->bytes[0]!=2);
        if (!shown) { continue; }
        const ActionParam *param=&op->params[p]; DWORD value=ActionReadValue(ins,p);
        ParamLabel(param,text,sizeof(text)); SetText(e,AE_PARAMLABEL+p,text);
        if (param->kind==ACTION_LABEL)
        {
            /* Only offer occurrences the native forward/first search can reach. */
            for (DWORD i=ins->bytes[0]==1 ? 0 : e->row;i<b->count;i++) if (b->instructions[i].bytes[0]==2)
            {
                BOOL first=TRUE;
                for (DWORD j=ins->bytes[0]==1 ? 0 : e->row;j<i;j++)
                { if (b->instructions[j].bytes[0]==2 && b->instructions[j].bytes[1]==b->instructions[i].bytes[1]) { first=FALSE; break; } }
                if (first) { ActionInstructionFormat(b,i,text,sizeof(text)); Choice(c,text,b->instructions[i].uid); }
            }
            SelectChoice(c,ins->target);
        }
        else
        {
            if (param->kind==ACTION_CHARACTER)
            {
                Choice(c,"253 - This character",253); Choice(c,"248 - Bond (cutscene model only)",248);
                Choice(c,"250 - Character seen shot",250); Choice(c,"251 - Character seen dying",251);
                Choice(c,"252 - Character preset",252); Choice(c,"249 - Clone",249);
                for (DWORD i=0;i<e->setup->charactercount;i++) if (!e->setup->characters[i].deleted)
                { snprintf(text,sizeof(text),"%u - Character",e->setup->characters[i].chrnum); Choice(c,text,e->setup->characters[i].chrnum); }
            }
            else if (param->kind==ACTION_BLOCK)
            { for (DWORD i=0;i<e->doc.count;i++) { BlockTitle(e,i,text,sizeof(text)); Choice(c,text,e->doc.blocks[i].id); } }
            else if (param->kind==ACTION_PAD)
            {
                for (DWORD i=0;i<e->setup->padcount;i++) if (!e->setup->pads[i].deleted)
                { snprintf(text,sizeof(text),"%lu - Pad (%s)",(unsigned long)i,e->setup->pads[i].stanname); Choice(c,text,i); }
                Choice(c,"9000 - Current pad preset",9000);
            }
            if (ins->bytes[0]==0x0a && (p==1 || p==2))
            { Choice(c,p==1 ? "65535 - Start of animation" : "65535 - End of animation",65535); }
            SelectChoice(c,value);
            if (SendMessage(c,CB_GETCURSEL,0,0)<0)
            { FormatNumber(param,value,text,sizeof(text)); SetWindowText(c,text); }
        }
    }
    Enable(e,AE_NAME,editable && ins); Enable(e,AE_NOTE,editable && ins);
    Enable(e,AE_DEBUGTEXT,editable && ins && ins->bytes[0]==0xad && ins->size<=4096);
    ShowWindow(Control(e,AE_DEBUGTEXT),ins && ins->bytes[0]==0xad ? SW_SHOW : SW_HIDE);
    ShowWindow(Control(e,AE_TEXTLABEL),ins && ins->bytes[0]==0xad ? SW_SHOW : SW_HIDE);
    Enable(e,AE_APPLYSTEP,editable); Enable(e,AE_FOLLOW,ins && (ActionTargetParameter(ins->bytes[0])>=0 || BlockParameter(ins)>=0));
    SetText(e,AE_FOLLOW,ins && ActionTargetParameter(ins->bytes[0])<0 ? "Open referenced block" : "Go to destination");
    if (!ins) { snprintf(text,sizeof(text),"Select a block, then an instruction.\r\n\r\nNew behavior creates a waiting loop. Level logic starts automatically when the level loads."); }
    else
    {
        const char *help="Commands run in order until a wait or a behavior switch. Names and notes are editor-only.";
        if (ins->bytes[0]==2) { help="Name this section to explain its purpose. Repeated native label numbers are distinct sections; jumps retain their exact destination."; }
        else if (ins->bytes[0]==3) { help="Pauses this script until its next AI update. Put a wait in polling loops so other game code gets time to run."; }
        else if (ins->bytes[0]==4) { help="Ends this update without saving the current position. On the next update, the script resumes from its last saved position. Use a waiting loop or switch behavior when finished."; }
        else if (ins->bytes[0]==0 || ins->bytes[0]==1) { help=ins->bytes[0]==0 ? "Always jumps forward to the next matching section. The destination list contains only reachable occurrences." : "Always jumps to the first matching section, searching from the start of this block. Can jump backward."; }
        else if (ActionTargetParameter(ins->bytes[0])>=0) { help="If the condition is true (or the action starts successfully), jump to the chosen section. Otherwise continue with the next instruction. Starting an action does not wait for it to finish."; }
        else if (ins->bytes[0]==5) { help="Changes the chosen character's behavior. For This character, execution restarts at the new block immediately. It does not return here afterward."; }
        else if (ins->bytes[0]==6 || ins->bytes[0]==7) { help="Return behavior is one stored block ID, not a call stack. Return starts that block from its beginning. Set it before using Return."; }
        else if (ins->bytes[0]==0xad) { help="Legacy debug text is preserved byte for byte. It is not shown in gameplay. Use the Note box for new editor comments."; }
        snprintf(text,sizeof(text),"%s%s\r\n\r\nNative command: %s [0x%02X]\r\nNumbers accept decimal or 0x hex. Lists accept a listed reference or a numeric ID.",b->global ? "Shared behavior (read-only). Duplicate into this level to customize.\r\n\r\n" : "",help,op->symbol,ins->bytes[0]);
    }
    SetText(e,AE_HELP,text); e->pending=FALSE; e->refreshing=FALSE;
}
static void DisplayReferences(ActionEditor *e)
{
    char text[512]; ActionBlock *b=Block(e); HWND used=Control(e,AE_USED), chars=Control(e,AE_CHARACTERS);
    DWORD selected=ListData(chars,CB_GETCURSEL,CB_GETITEMDATA);
    if (selected==ACTION_MISSING_TARGET) { selected=e->selectedcharacter; }
    SendMessage(chars,CB_RESETCONTENT,0,0); SendMessage(used,LB_RESETCONTENT,0,0);
    for (DWORD i=0;i<e->setup->charactercount;i++) if (!e->setup->characters[i].deleted)
    {
        snprintf(text,sizeof(text),"Character %u -> %04X",e->setup->characters[i].chrnum,e->doc.assignments[i]); Choice(chars,text,i);
        if (b && e->doc.assignments[i]==b->id)
        { snprintf(text,sizeof(text),"Character %u (initial behavior)",e->setup->characters[i].chrnum); LRESULT n=SendMessage(used,LB_ADDSTRING,0,(LPARAM)text); SendMessage(used,LB_SETITEMDATA,n,ACTION_MISSING_TARGET); }
    }
    SelectChoice(chars,selected); if (SendMessage(chars,CB_GETCURSEL,0,0)<0) { SendMessage(chars,CB_SETCURSEL,0,0); }
    if (!b) { return; }
    if (!b->global && b->id>=0x1000)
    { LRESULT n=SendMessage(used,LB_ADDSTRING,0,(LPARAM)"Runs automatically at level start"); SendMessage(used,LB_SETITEMDATA,n,ACTION_MISSING_TARGET); }
    for (DWORD i=0;i<e->setup->objectcount;i++)
    {
        const SetupObject *o=&e->setup->objects[i];
        if (!o->deleted && (o->type==39 || o->type==40) && o->sourceoffset<=e->setup->size && e->setup->size-o->sourceoffset>=0x84)
        {
            const unsigned char *p=e->setup->data+o->sourceoffset+0x80;
            DWORD id=(DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3];
            if (id==b->id)
            { snprintf(text,sizeof(text),"%s %lu (initial behavior)",o->type==39 ? "Vehicle" : "Aircraft",(unsigned long)i); LRESULT n=SendMessage(used,LB_ADDSTRING,0,(LPARAM)text); SendMessage(used,LB_SETITEMDATA,n,ACTION_MISSING_TARGET); }
        }
    }
    for (DWORD i=0;i<e->doc.count;i++) for (DWORD j=0;j<e->doc.blocks[i].count;j++)
    {
        const ActionInstruction *ins=&e->doc.blocks[i].instructions[j]; const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
        for (int p=0;p<op->paramcount;p++) if (op->params[p].kind==ACTION_BLOCK && ActionReadValue(ins,p)==b->id)
        {
            snprintf(text,sizeof(text),"Block %04lX, step %lu",(unsigned long)e->doc.blocks[i].id,(unsigned long)j+1);
            LRESULT n=SendMessage(used,LB_ADDSTRING,0,(LPARAM)text); SendMessage(used,LB_SETITEMDATA,n,((DWORD)i<<16)|j);
        }
    }
}
static void Refresh(ActionEditor *e)
{
    char filter[96],title[1200],text[1250]; HWND list=Control(e,AE_BLOCKS), steps=Control(e,AE_STEPS);
    e->refreshing=TRUE;
    GetText(e,AE_FILTER,filter,sizeof(filter)); SendMessage(list,LB_RESETCONTENT,0,0);
    for (DWORD i=0;i<e->doc.count;i++)
    {
        BlockTitle(e,i,title,sizeof(title));
        if (Contains(title,filter))
        {
            LRESULT n=SendMessage(list,LB_ADDSTRING,0,(LPARAM)title); SendMessage(list,LB_SETITEMDATA,n,i);
            if (i==e->block) { SendMessage(list,LB_SETCURSEL,n,0); }
        }
    }
    ActionBlock *b=Block(e); SendMessage(steps,WM_SETREDRAW,FALSE,0); SendMessage(steps,LB_RESETCONTENT,0,0);
    if (b)
    {
        if (e->row>=b->count) { e->row=b->count-1; }
        for (DWORD i=0;i<b->count;i++)
        {
            ActionInstructionFormat(b,i,title,sizeof(title));
            snprintf(text,sizeof(text),"%lu. %s%s%s",(unsigned long)i+1,b->instructions[i].bytes[0]==2 ? "" : "    ",title,b->instructions[i].note[0] ? "  [note]" : "");
            SendMessage(steps,LB_ADDSTRING,0,(LPARAM)text);
        }
        SendMessage(steps,LB_SETCURSEL,e->row,0);
        if (e->row>3) { SendMessage(steps,LB_SETTOPINDEX,e->row-3,0); }
    }
    SendMessage(steps,WM_SETREDRAW,TRUE,0); InvalidateRect(steps,NULL,TRUE);
    SetText(e,AE_BLOCKNAME,b ? b->name : ""); Enable(e,AE_BLOCKNAME,Editable(e));
    Enable(e,AE_DUPLICATE,b!=NULL); Enable(e,AE_DELETEBLOCK,Editable(e)); Enable(e,AE_INSERT,Editable(e));
    Enable(e,AE_DELETE,Editable(e)); Enable(e,AE_UP,Editable(e)); Enable(e,AE_DOWN,Editable(e));
    Enable(e,AE_UNDO,e->undocount!=0); Enable(e,AE_REDO,e->redocount!=0);
    Enable(e,AE_ASSIGN,b && b->id<0x1000 && e->setup->charactercount);
    DisplayReferences(e); DisplayInstruction(e);
    e->refreshing=FALSE;
}
static BOOL ApplyFields(ActionEditor *e)
{
    const char *why; ActionDocument next; ActionBlock *b=Block(e); ActionInstruction *ins=Instruction(e);
    DWORD values[8]={0},target=ins ? ins->target : 0; char name[ACTION_NAME_SIZE],note[ACTION_NOTE_SIZE],blockname[ACTION_NAME_SIZE],text[4096];
    if (!e->pending || !b || b->global) { return TRUE; }
    GetText(e,AE_BLOCKNAME,blockname,sizeof(blockname)); GetText(e,AE_NAME,name,sizeof(name)); GetText(e,AE_NOTE,note,sizeof(note));
    if (ins)
    {
        const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
        for (int p=0;p<op->paramcount;p++)
        {
            HWND c=Control(e,AE_PARAM+p); DWORD choice=e->typed[p] ? ACTION_MISSING_TARGET : ListData(c,CB_GETCURSEL,CB_GETITEMDATA);
            if (op->params[p].kind==ACTION_LABEL)
            {
                if (choice==ACTION_MISSING_TARGET && (ins->target!=ACTION_MISSING_TARGET || e->typed[p]))
                { Error(e,"Choose a destination section from the list."); SetFocus(c); return FALSE; }
                target=choice; values[p]=ActionReadValue(ins,p);
            }
            else if (choice!=ACTION_MISSING_TARGET) { values[p]=choice; }
            else
            {
                GetWindowText(c,text,sizeof(text));
                if (!ActionParseValue(&op->params[p],text,&values[p]))
                { Error(e,"Enter a number within this parameter's native range, or choose an entry from the list."); SetFocus(c); return FALSE; }
            }
        }
    }
    /* An unchanged large legacy PRINT is never copied through the edit limit. */
    GetText(e,AE_DEBUGTEXT,text,sizeof(text));
    const char *debug=ins && ins->bytes[0]==0xad && ins->size>sizeof(text) ? (char *)ins->bytes+1 : text;
    if (!Begin(e,&next)) { return FALSE; }
    if (ins && !ActionInstructionSet(&next,e->block,e->row,values,target,name,note,debug,&why))
    { ActionDocumentFree(&next); Error(e,why); return FALSE; }
    if (strcmp(next.blocks[e->block].name,blockname))
    { snprintf(next.blocks[e->block].name,sizeof(next.blocks[e->block].name),"%s",blockname); next.changed=TRUE; }
    BOOL same=!strcmp(next.blocks[e->block].name,b->name);
    if (ins)
    {
        const ActionInstruction *n=&next.blocks[e->block].instructions[e->row];
        same=same && n->size==ins->size && !memcmp(n->bytes,ins->bytes,ins->size)
            && !strcmp(n->name,ins->name) && !strcmp(n->note,ins->note);
    }
    if (same) { ActionDocumentFree(&next); }
    else { Commit(e,&next); }
    e->pending=FALSE; return TRUE;
}
static void Commands(ActionEditor *e)
{
    char filter[128],text[192]; HWND c=Control(e,AE_COMMAND);
    GetText(e,AE_SEARCH,filter,sizeof(filter)); SendMessage(c,CB_RESETCONTENT,0,0);
    for (DWORD i=0;i<ACTION_OPCODE_COUNT;i++) if (i!=4)
    {
        snprintf(text,sizeof(text),"%s  [%02lX]",g_ActionOpcodes[i].title,(unsigned long)i);
        if (Contains(text,filter) || Contains(g_ActionOpcodes[i].symbol,filter)) { Choice(c,text,i); }
    }
    SendMessage(c,CB_SETCURSEL,0,0);
}
static BOOL Validate(ActionEditor *e)
{
    const char *why; DWORD errors=0,warnings=0; char text[300];
    free(e->issues); e->issues=NULL; e->issuecount=0; SendMessage(Control(e,AE_ISSUES),LB_RESETCONTENT,0,0);
    if (!ActionDocumentValidate(&e->doc,e->setup,&e->issues,&e->issuecount,&why)) { Error(e,why); return FALSE; }
    for (DWORD i=0;i<e->issuecount;i++)
    {
        const ActionIssue *issue=&e->issues[i]; if (issue->error) { errors++; } else { warnings++; }
        snprintf(text,sizeof(text),"%s  Block %04lX, step %lu: %s",issue->error ? "ERROR" : "Warning",
            (unsigned long)(issue->block<e->doc.count ? e->doc.blocks[issue->block].id : 0),(unsigned long)issue->instruction+1,issue->text);
        SendMessage(Control(e,AE_ISSUES),LB_ADDSTRING,0,(LPARAM)text);
    }
    snprintf(text,sizeof(text),"%lu errors, %lu warnings. Double-click a result to visit its instruction.",(unsigned long)errors,(unsigned long)warnings);
    SetText(e,AE_STATUS,text); return !errors;
}
static void Navigate(ActionEditor *e, DWORD block, DWORD row)
{
    if (block>=e->doc.count || !ApplyFields(e)) { return; }
    e->refreshing=TRUE; SetText(e,AE_FILTER,""); e->refreshing=FALSE;
    e->block=block; e->row=row; Refresh(e);
}
static void Command(ActionEditor *e, int id)
{
    const char *why=""; ActionDocument next; DWORD block=e->block,row=e->row; BOOL ok=FALSE;
    if (!ApplyFields(e)) { return; }
    if (id==AE_APPLYSTEP) { Refresh(e); return; }
    if (id==AE_VALIDATE) { Refresh(e); Validate(e); return; }
    if (id==AE_FOLLOW)
    {
        ActionInstruction *ins=Instruction(e);
        if (ins && ActionTargetParameter(ins->bytes[0])>=0)
        { Navigate(e,e->block,ActionFindInstruction(Block(e),ins->target)); }
        else if (BlockParameter(ins)>=0)
        {
            DWORD id=ActionReadValue(ins,BlockParameter(ins)); BOOL found=FALSE;
            for (DWORD i=0;i<e->doc.count;i++) if (e->doc.blocks[i].id==id)
            { Navigate(e,i,0); found=TRUE; break; }
            if (!found) { Error(e,"That block is unavailable. It may be missing, or belong to an older base ROM without a shared-script catalog."); }
        }
        return;
    }
    if (id==AE_UNDO || id==AE_REDO)
    {
        ActionSnapshot *from=id==AE_UNDO ? e->undo : e->redo, *to=id==AE_UNDO ? e->redo : e->undo;
        DWORD *n=id==AE_UNDO ? &e->undocount : &e->redocount, *m=id==AE_UNDO ? &e->redocount : &e->undocount;
        if (*n)
        {
            ActionSnapshot previous=from[--*n]; Push(to,m,&e->doc,e->block,e->row);
            e->doc=previous.doc; e->block=previous.block; e->row=previous.row;
            free(e->issues); e->issues=NULL; e->issuecount=0; SendMessage(Control(e,AE_ISSUES),LB_RESETCONTENT,0,0);
            SetText(e,AE_STATUS,"Draft history restored.");
        }
        Refresh(e); return;
    }
    if (id==IDOK)
    {
        if (!e->doc.changed) { EndDialog(e->window,IDCANCEL); return; }
        Refresh(e); if (!Validate(e)) { Error(e,"Fix the errors before applying. Warnings are advisory."); return; }
        if (!ActionDocumentCompile(&e->doc,e->setup,e->result,&why)) { Error(e,why); return; }
        e->accepted=TRUE; EndDialog(e->window,IDOK); return;
    }
    if (!Begin(e,&next)) { return; }
    switch (id)
    {
        case AE_NEW: case AE_NEWLEVEL: case AE_DUPLICATE:
            ok=ActionDocumentAddBlock(&next,id==AE_DUPLICATE ? e->block : ACTION_MISSING_TARGET,
                id==AE_NEWLEVEL || (id==AE_DUPLICATE && Block(e) && Block(e)->id>=0x1000),&block,&why); row=0; break;
        case AE_DELETEBLOCK:
            ok=ActionDocumentDeleteBlock(&next,e->setup,e->block,&why); if (ok) { block=next.count ? 0 : ACTION_MISSING_TARGET; row=0; } break;
        case AE_INSERT:
            ok=ActionBlockInsert(&next,e->block,e->row,ListData(Control(e,AE_COMMAND),CB_GETCURSEL,CB_GETITEMDATA),&why); break;
        case AE_DELETE: ok=ActionBlockDelete(&next,e->block,e->row,&why); break;
        case AE_UP: case AE_DOWN:
            ok=ActionBlockMove(&next,e->block,e->row,id==AE_UP ? -1 : 1,&why); if (ok) { row+=id==AE_UP ? -1 : 1; } break;
        case AE_ASSIGN:
            ok=ActionDocumentAssign(&next,ListData(Control(e,AE_CHARACTERS),CB_GETCURSEL,CB_GETITEMDATA),e->block,&why); break;
        default: ActionDocumentFree(&next); return;
    }
    if (ok) { Commit(e,&next); e->block=block; e->row=row; }
    else { ActionDocumentFree(&next); Error(e,why); }
    Refresh(e);
}
static void Layout(ActionEditor *e)
{
    RECT r; GetClientRect(e->window,&r); int w=r.right,h=r.bottom,left=226,right=326,x=left+22,rx=w-right-12,middle=rx-x-12;
    int body=h-167, listbottom=body-244;
    Place(e,AE_FILTER,12,12,left,25); Place(e,AE_BLOCKS,12,43,left,listbottom-43);
    Place(e,AE_NEW,12,listbottom+6,109,26); Place(e,AE_NEWLEVEL,129,listbottom+6,109,26);
    Place(e,AE_DUPLICATE,12,listbottom+38,109,26); Place(e,AE_DELETEBLOCK,129,listbottom+38,109,26);
    Place(e,AE_CHARLABEL,12,listbottom+76,left,18); Place(e,AE_CHARACTERS,12,listbottom+98,left,230);
    Place(e,AE_ASSIGN,12,listbottom+129,left,27); Place(e,AE_USEDLABEL,12,listbottom+166,left,18);
    Place(e,AE_USED,12,listbottom+186,left,body-listbottom-190);
    Place(e,AE_BLOCKLABEL,x,12,86,20); Place(e,AE_BLOCKNAME,x+90,10,middle-90,25);
    Place(e,AE_STEPS,x,43,middle,body-163);
    Place(e,AE_UP,x,body-114,60,27); Place(e,AE_DOWN,x+66,body-114,60,27); Place(e,AE_DELETE,x+132,body-114,105,27);
    Place(e,AE_SEARCHLABEL,x,body-78,104,20); Place(e,AE_SEARCH,x+110,body-81,middle-110,25);
    Place(e,AE_COMMAND,x,body-48,middle-88,350); Place(e,AE_INSERT,x+middle-82,body-49,82,27);
    Place(e,AE_NAMELABEL,rx,12,right,18); Place(e,AE_NAME,rx,33,right,24);
    Place(e,AE_NOTELABEL,rx,63,right,18); Place(e,AE_NOTE,rx,83,right,51);
    for (int p=0;p<8;p++)
    { Place(e,AE_PARAMLABEL+p,rx,145+p*39,right,16); Place(e,AE_PARAM+p,rx,161+p*39,right,240); }
    Place(e,AE_TEXTLABEL,rx,145,right,18); Place(e,AE_DEBUGTEXT,rx,167,right,124);
    Place(e,AE_APPLYSTEP,rx,465,150,27); Place(e,AE_FOLLOW,rx+160,465,right-160,27);
    Place(e,AE_HELP,rx,503,right,body-506);
    Place(e,AE_ISSUELABEL,12,body+1,400,18); Place(e,AE_ISSUES,12,body+23,w-24,77);
    Place(e,AE_STATUS,12,h-61,w-24,19);
    Place(e,AE_UNDO,12,h-34,75,26); Place(e,AE_REDO,94,h-34,75,26); Place(e,AE_VALIDATE,183,h-34,88,26);
    Place(e,IDOK,w-270,h-34,157,26); Place(e,IDCANCEL,w-103,h-34,91,26);
}
static LRESULT CALLBACK Keys(HWND hwnd, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data)
{
    if (message==WM_KEYDOWN && (GetKeyState(VK_CONTROL)&0x8000) && (w=='Z' || w=='Y'))
    { SendMessage((HWND)data,WM_COMMAND,w=='Z' ? AE_UNDO : AE_REDO,0); return 0; }
    if (message==WM_NCDESTROY) { RemoveWindowSubclass(hwnd,Keys,id); }
    return DefSubclassProc(hwnd,message,w,l);
}
static void AddControl(ActionEditor *e, int id, const char *cls, const char *text, DWORD style)
{
    HWND c=CreateWindowEx(!strcmp(cls,"EDIT") || !strcmp(cls,"LISTBOX") ? WS_EX_CLIENTEDGE : 0,
        cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,e->window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    if (!c) { e->failed=TRUE; return; }
    SendMessage(c,WM_SETFONT,SendMessage(e->window,WM_GETFONT,0,0),FALSE);
    if (!strcmp(cls,"LISTBOX")) { SendMessage(c,LB_SETHORIZONTALEXTENT,2200,0); }
    if (strcmp(cls,"EDIT") && strcmp(cls,"COMBOBOX")) { SetWindowSubclass(c,Keys,1,(DWORD_PTR)e->window); }
}
static void Controls(ActionEditor *e)
{
    const DWORD edit=WS_TABSTOP|ES_AUTOHSCROLL, list=WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT;
    const struct { int id; const char *text; } buttons[]={
        {AE_NEW,"New behavior"},{AE_NEWLEVEL,"New level logic"},{AE_DUPLICATE,"Duplicate"},{AE_DELETEBLOCK,"Delete block"},
        {AE_INSERT,"Insert"},{AE_DELETE,"Delete step"},{AE_UP,"Up"},{AE_DOWN,"Down"},
        {AE_APPLYSTEP,"Update instruction"},{AE_FOLLOW,"Go to destination"},{AE_ASSIGN,"Assign selected block"},
        {AE_UNDO,"Undo"},{AE_REDO,"Redo"},{AE_VALIDATE,"Validate"},{IDOK,"Apply to project"},{IDCANCEL,"Cancel"}};
    const struct { int id; const char *text; } labels[]={
        {AE_BLOCKLABEL,"Block name"},{AE_NAMELABEL,"Section / instruction name"},{AE_NOTELABEL,"Note"},
        {AE_TEXTLABEL,"Legacy debug text"},{AE_SEARCHLABEL,"Find command"},{AE_CHARLABEL,"Character initial behavior"},
        {AE_USEDLABEL,"Used by"},{AE_ISSUELABEL,"Validation"},{AE_HELP,""},{AE_STATUS,""}};
    AddControl(e,AE_FILTER,"EDIT","",edit); SendMessage(Control(e,AE_FILTER),EM_SETCUEBANNER,FALSE,(LPARAM)L"Find block by name or ID");
    AddControl(e,AE_BLOCKS,"LISTBOX","",list); AddControl(e,AE_STEPS,"LISTBOX","",list);
    AddControl(e,AE_BLOCKNAME,"EDIT","",edit); AddControl(e,AE_NAME,"EDIT","",edit);
    AddControl(e,AE_NOTE,"EDIT","",WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL);
    AddControl(e,AE_DEBUGTEXT,"EDIT","",WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL);
    AddControl(e,AE_SEARCH,"EDIT","",edit);
    AddControl(e,AE_COMMAND,"COMBOBOX","",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
    AddControl(e,AE_CHARACTERS,"COMBOBOX","",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
    AddControl(e,AE_USED,"LISTBOX","",list); AddControl(e,AE_ISSUES,"LISTBOX","",list);
    for (int p=0;p<8;p++)
    {
        AddControl(e,AE_PARAMLABEL+p,"STATIC","",0);
        AddControl(e,AE_PARAM+p,"COMBOBOX","",WS_TABSTOP|CBS_DROPDOWN|CBS_AUTOHSCROLL|WS_VSCROLL);
        SendMessage(Control(e,AE_PARAM+p),CB_LIMITTEXT,128,0);
    }
    for (unsigned int i=0;i<sizeof(buttons)/sizeof(*buttons);i++)
    { AddControl(e,buttons[i].id,"BUTTON",buttons[i].text,WS_TABSTOP|BS_PUSHBUTTON); }
    for (unsigned int i=0;i<sizeof(labels)/sizeof(*labels);i++)
    {
        if (labels[i].id==AE_HELP) { AddControl(e,AE_HELP,"EDIT","",ES_MULTILINE|ES_READONLY|WS_VSCROLL); }
        else { AddControl(e,labels[i].id,"STATIC",labels[i].text,SS_LEFT|SS_NOPREFIX); }
    }
    SendMessage(Control(e,AE_NAME),EM_LIMITTEXT,ACTION_NAME_SIZE-1,0);
    SendMessage(Control(e,AE_BLOCKNAME),EM_LIMITTEXT,ACTION_NAME_SIZE-1,0);
    SendMessage(Control(e,AE_NOTE),EM_LIMITTEXT,ACTION_NOTE_SIZE-1,0);
    SendMessage(Control(e,AE_DEBUGTEXT),EM_LIMITTEXT,4094,0);
    SendMessage(Control(e,AE_COMMAND),CB_SETDROPPEDWIDTH,500,0);
}
static INT_PTR CALLBACK Dialog(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    ActionEditor *e=(ActionEditor *)GetWindowLongPtr(hwnd,DWLP_USER);
    if (msg==WM_INITDIALOG)
    {
        e=(ActionEditor *)l; e->window=hwnd; SetWindowLongPtr(hwnd,DWLP_USER,(LONG_PTR)e);
        Controls(e); if (e->failed) { EndDialog(hwnd,IDCANCEL); return TRUE; }
        Layout(e); Commands(e); Refresh(e); SetText(e,AE_STATUS,e->globalstatus); return TRUE;
    }
    if (!e) { return FALSE; }
    switch (msg)
    {
        case WM_SIZE: Layout(e); return TRUE;
        case WM_GETMINMAXINFO: ((MINMAXINFO *)l)->ptMinTrackSize.x=1120; ((MINMAXINFO *)l)->ptMinTrackSize.y=820; return TRUE;
        case WM_CLOSE: EndDialog(hwnd,IDCANCEL); return TRUE;
        case WM_COMMAND:
        {
            int id=LOWORD(w),code=HIWORD(w);
            if (e->refreshing) { return TRUE; }
            if (id==IDCANCEL) { EndDialog(hwnd,IDCANCEL); return TRUE; }
            if (id==AE_FILTER && code==EN_CHANGE) { if (ApplyFields(e)) { Refresh(e); } return TRUE; }
            if (id==AE_SEARCH && code==EN_CHANGE) { Commands(e); return TRUE; }
            if (((id==AE_NAME || id==AE_NOTE || id==AE_BLOCKNAME || id==AE_DEBUGTEXT) && code==EN_CHANGE)
                || (id>=AE_PARAM && id<AE_PARAM+8 && (code==CBN_EDITCHANGE || code==CBN_SELCHANGE)))
            {
                if (id>=AE_PARAM && id<AE_PARAM+8) { e->typed[id-AE_PARAM]=code==CBN_EDITCHANGE; }
                e->pending=TRUE; return TRUE;
            }
            if (id==AE_BLOCKS && code==LBN_SELCHANGE)
            {
                DWORD block=ListData(Control(e,id),LB_GETCURSEL,LB_GETITEMDATA);
                if (ApplyFields(e)) { e->block=block; e->row=0; Refresh(e); }
                else
                {
                    HWND list=Control(e,AE_BLOCKS); LRESULT n=SendMessage(list,LB_GETCOUNT,0,0);
                    for (LRESULT i=0;i<n;i++) if ((DWORD)SendMessage(list,LB_GETITEMDATA,i,0)==e->block)
                    { SendMessage(list,LB_SETCURSEL,i,0); break; }
                }
                return TRUE;
            }
            if (id==AE_STEPS && code==LBN_SELCHANGE)
            {
                DWORD row=(DWORD)SendMessage(Control(e,id),LB_GETCURSEL,0,0);
                if (ApplyFields(e)) { e->row=row; Refresh(e); }
                else { SendMessage(Control(e,id),LB_SETCURSEL,e->row,0); }
                return TRUE;
            }
            if (id==AE_ISSUES && code==LBN_DBLCLK)
            {
                DWORD n=(DWORD)SendMessage(Control(e,id),LB_GETCURSEL,0,0);
                if (n<e->issuecount) { DWORD b=e->issues[n].block,r=e->issues[n].instruction; Navigate(e,b,r); } return TRUE;
            }
            if (id==AE_USED && code==LBN_DBLCLK)
            { DWORD n=ListData(Control(e,id),LB_GETCURSEL,LB_GETITEMDATA); if (n!=ACTION_MISSING_TARGET) { Navigate(e,n>>16,n&0xffff); } return TRUE; }
            if (code==BN_CLICKED)
            {
                char cls[32]; GetClassName(GetFocus(),cls,sizeof(cls));
                /* Enter in a parameter commits that edit without closing the draft. */
                if (id==IDOK && (!lstrcmpi(cls,"EDIT") || !lstrcmpi(cls,"COMBOBOX")))
                { Command(e,AE_APPLYSTEP); }
                else { Command(e,id); }
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}
BOOL ActionEditorShow(HWND owner, const GEditorProject *project, const SetupFile *setup,
    DWORD selectedcharacter, SetupFile *result, BOOL *changed, const char **why)
{
    ActionEditor *e=calloc(1,sizeof(*e)); RomFile rom={0}; char path[MAX_PATH]; INT_PTR response;
    *changed=FALSE; memset(result,0,sizeof(*result));
    if (!e) { *why="Out of memory opening Action Blocks."; return FALSE; }
    e->setup=setup; e->result=result; e->selectedcharacter=selectedcharacter;
    if (!ActionDocumentLoad(setup,&e->doc,why)) { free(e); return FALSE; }
    snprintf(e->globalstatus,sizeof(e->globalstatus),"Shared scripts unavailable. Local scripts can still be edited.");
    if (snprintf(path,sizeof(path),"%s\\%s",project->dir,ROM_EXPORT_BASE_FILENAME)<(int)sizeof(path)
        && RomLoad(path,&rom,why))
    {
        DWORD count=e->doc.count;
        if (ActionDocumentLoadGlobals(&e->doc,&rom,why) && e->doc.count>count)
        { snprintf(e->globalstatus,sizeof(e->globalstatus),"Shared scripts are read-only. Duplicate one into this level to customize it."); }
        else if (**why) { snprintf(e->globalstatus,sizeof(e->globalstatus),"%s",*why); }
    }
    RomFree(&rom); e->block=e->doc.count ? 0 : ACTION_MISSING_TARGET;
    if (selectedcharacter<e->doc.charactercount)
    { for (DWORD i=0;i<e->doc.count;i++) if (e->doc.blocks[i].id==e->doc.assignments[selectedcharacter]) { e->block=i; break; } }
    response=DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_ACTION_BLOCKS),owner,Dialog,(LPARAM)e);
    *changed=e->accepted;
    BOOL success=response!=-1 && !e->failed;
    if (!success) { *why="The Action Blocks window could not be created."; }
    else { *why=""; }
    ActionDocumentFree(&e->doc); ClearHistory(e->undo,&e->undocount); ClearHistory(e->redo,&e->redocount);
    free(e->issues); free(e); return success;
}
