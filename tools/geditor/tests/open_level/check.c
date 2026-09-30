#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
typedef uint32_t DWORD;
typedef int BOOL;
typedef unsigned UINT;
typedef intptr_t HWND, LPARAM, LRESULT, LONG_PTR, INT_PTR;
typedef uintptr_t WPARAM;
typedef struct { int left,top,right,bottom; } RECT;
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define GEDITOR_NO_LEVEL UINT32_MAX
#define GEDITOR_TITLE "GEditor"
#define LOWORD(n) ((n)&0xffff)
#define HIWORD(n) (((n)>>16)&0xffff)
#define MAKEINTRESOURCE(n) (n)
enum { IDOK=1,IDCANCEL=2,IDYES=6,IDNO=7,WM_INITDIALOG=20,WM_COMMAND,WM_CLOSE,
    DWLP_USER,IDC_OPEN_LEVEL_LIST,IDD_OPEN_LEVEL,LB_ADDSTRING,LB_SETITEMDATA,
    LB_GETITEMDATA,LB_GETCURSEL,LB_SETCURSEL,LBN_SELCHANGE,LBN_DBLCLK,
    GEDITOR_WM_OPEN_LEVEL,MB_ICONERROR=0x100,MB_ICONQUESTION=0x200,MB_YESNOCANCEL=0x400 };
#define LB_ERR (-1)
#define LB_ERRSPACE (-2)
#include "types.inc"
static struct Project { char name[64];DWORD levelcount;struct {char name[64];DWORD levelid;} levels[64]; } g_Project;
static DWORD g_CurrentLevelIndex=GEDITOR_NO_LEVEL;
static struct { char name[64];DWORD index; } rows[64];
static int rowcount,selection,ended,errors,opens,saves,prompts,dialogs;
static BOOL openenabled,dirty,saveok=TRUE;
static int failadd=-1,faildata=-1,choice=IDYES;
static INT_PTR dialogresult=IDOK;
static DWORD requested;
static LONG_PTR userdata;
static LONG_PTR GetWindowLongPtr(HWND hwnd,int field) { return userdata; }
static void SetWindowLongPtr(HWND hwnd,int field,LONG_PTR value) { userdata=value; }
static HWND GetDlgItem(HWND hwnd,int id) { return id; }
static void SetFocus(HWND hwnd) { assert(hwnd==IDC_OPEN_LEVEL_LIST); }
static void EnableWindow(HWND hwnd,BOOL enabled) { assert(hwnd==IDOK);openenabled=enabled; }
static void EndDialog(HWND hwnd,INT_PTR result) { ended=result; }
static int MessageBox(HWND hwnd,const char *text,const char *title,int flags)
{ if(flags==MB_ICONERROR) errors++;else {assert(flags==(MB_ICONQUESTION|MB_YESNOCANCEL));prompts++;}return choice; }
static LRESULT SendMessage(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if (msg==GEDITOR_WM_OPEN_LEVEL)
    { assert(hwnd==100 && wp<g_Project.levelcount);opens++;g_CurrentLevelIndex=wp;return 0; }
    assert(hwnd==IDC_OPEN_LEVEL_LIST);
    if(msg==LB_ADDSTRING)
    {
        if(rowcount==failadd) return LB_ERRSPACE;
        int at=0;while(at<rowcount && strcasecmp(rows[at].name,(const char *)lp)<=0) at++;
        memmove(rows+at+1,rows+at,(rowcount-at)*sizeof(*rows));
        snprintf(rows[at].name,sizeof(rows[at].name),"%s",(const char *)lp);rows[at].index=GEDITOR_NO_LEVEL;rowcount++;return at;
    }
    if(msg==LB_SETITEMDATA)
    { if(rowcount-1==faildata) return LB_ERR;assert(wp<(WPARAM)rowcount);rows[wp].index=lp;return 0; }
    if(msg==LB_GETITEMDATA) return wp<(WPARAM)rowcount?(LRESULT)rows[wp].index:LB_ERR;
    if(msg==LB_GETCURSEL) return selection;
    assert(msg==LB_SETCURSEL);selection=wp<(WPARAM)rowcount?(int)wp:LB_ERR;return selection;
}
static BOOL GEditorHasUnsavedChanges(void) { return dirty; }
static BOOL GEditorSaveProject(HWND hwnd) { saves++;if(saveok) dirty=FALSE;return saveok; }
static HWND GetModuleHandle(void *name) { return 0; }
static INT_PTR DialogBoxParam(HWND instance,int resource,HWND owner,
    INT_PTR (*proc)(HWND,UINT,WPARAM,LPARAM),LPARAM param)
{
    assert(resource==IDD_OPEN_LEVEL && owner==100);dialogs++;
    if(dialogresult==-1) return -1;
    rowcount=0;selection=-1;ended=0;userdata=0;
    proc(200,WM_INITDIALOG,0,param);
    if(ended) return ended;
    if(dialogresult==IDCANCEL) proc(200,WM_COMMAND,IDCANCEL,0);
    else
    {
        for(int i=0;i<rowcount;i++) if(rows[i].index==requested) selection=i;
        proc(200,WM_COMMAND,IDOK,0);
    }
    assert(ended);return ended;
}
typedef struct { BOOL expanded;RECT headerrc,bodyrc; } BrowserSection;
typedef struct { BOOL fileimages;BrowserSection sections[BROWSER_SECTION_COUNT];int objectheight; } BrowserState;
static void SetRect(RECT *r,int l,int t,int right,int b) { *r=(RECT){l,t,right,b}; }
static void SetRectEmpty(RECT *r) { memset(r,0,sizeof(*r)); }
static int BrowserContentHeight(const BrowserState *state,int section)
{ assert(section==BROWSER_SECTION_OBJECTS);return state->objectheight; }
#include "logic.inc"

static void Init(GEditorOpenLevelDialog *state)
{
    rowcount=0;selection=-1;ended=0;userdata=0;state->index=GEDITOR_NO_LEVEL;
    GEditorOpenLevelDialogProc(200,WM_INITDIALOG,0,(LPARAM)state);
}
int main(void)
{
    const char *names[]={"Depot","control","Aztec","Train","Bunker 2 MP","Control","Title"};
    strcpy(g_Project.name,"Test");g_Project.levelcount=7;
    for(int i=0;i<7;i++) {strcpy(g_Project.levels[i].name,names[i]);g_Project.levels[i].levelid=40-i*3;}
    struct Project original=g_Project;
    GEditorOpenLevelDialog state;
    g_CurrentLevelIndex=0;Init(&state);
    assert(openenabled && rows[selection].index==0 && state.index==GEDITOR_NO_LEVEL);
    for(int i=0;i<7;i++)
    {
        assert(!strcmp(rows[i].name,g_Project.levels[rows[i].index].name));
        if(i) assert(strcasecmp(rows[i-1].name,rows[i].name)<=0);
        selection=i;ended=0;GEditorOpenLevelDialogProc(200,WM_COMMAND,IDOK,0);
        assert(ended==IDOK && state.index==rows[i].index);
    }
    assert(!memcmp(&g_Project,&original,sizeof(original))); /* Sorting never rewrites campaign/table order. */
    for(DWORD index=0;index<7;index++) {g_CurrentLevelIndex=index;Init(&state);assert(rows[selection].index==index);}
    g_CurrentLevelIndex=GEDITOR_NO_LEVEL;Init(&state);assert(selection==0);
    selection=-1;GEditorOpenLevelDialogProc(200,WM_COMMAND,IDC_OPEN_LEVEL_LIST|((WPARAM)LBN_SELCHANGE<<16),0);
    assert(!openenabled);GEditorOpenLevelDialogProc(200,WM_COMMAND,IDOK,0);assert(!ended);
    selection=2;GEditorOpenLevelDialogProc(200,WM_COMMAND,IDC_OPEN_LEVEL_LIST|((WPARAM)LBN_DBLCLK<<16),0);
    assert(ended==IDOK && state.index==rows[2].index);
    Init(&state);GEditorOpenLevelDialogProc(200,WM_COMMAND,IDCANCEL,0);assert(ended==IDCANCEL && state.index==GEDITOR_NO_LEVEL);
    Init(&state);GEditorOpenLevelDialogProc(200,WM_CLOSE,0,0);assert(ended==IDCANCEL && state.index==GEDITOR_NO_LEVEL);
    failadd=2;Init(&state);assert(ended==IDCANCEL && state.index==GEDITOR_NO_LEVEL);failadd=-1;
    faildata=2;Init(&state);assert(ended==IDCANCEL && state.index==GEDITOR_NO_LEVEL);faildata=-1;
    g_Project.levelcount=0;Init(&state);assert(!openenabled && selection==-1);g_Project=original;
    requested=2;GEditorPromptForOpenLevel(100);assert(opens==1 && g_CurrentLevelIndex==2);
    dirty=TRUE;GEditorPromptForOpenLevel(100);assert(opens==1 && prompts==0); /* Same level keeps unsaved edits. */
    requested=0;dialogresult=IDCANCEL;GEditorPromptForOpenLevel(100);assert(opens==1 && prompts==0);
    dialogresult=IDOK;choice=IDCANCEL;GEditorPromptForOpenLevel(100);assert(opens==1 && prompts==1);
    choice=IDYES;saveok=FALSE;GEditorPromptForOpenLevel(100);assert(opens==1 && saves==1);
    saveok=TRUE;GEditorPromptForOpenLevel(100);assert(opens==2 && saves==2 && g_CurrentLevelIndex==0);
    dirty=TRUE;choice=IDNO;requested=4;GEditorPromptForOpenLevel(100);assert(opens==3 && saves==2 && g_CurrentLevelIndex==4);
    dialogresult=-1;GEditorPromptForOpenLevel(100);assert(opens==3 && errors==3);
    int calls=dialogs;g_Project.levelcount=0;GEditorPromptForOpenLevel(100);assert(dialogs==calls);
    g_Project=original;g_Project.name[0]=0;GEditorPromptForOpenLevel(100);assert(dialogs==calls);
    puts("PASS alphabetical level picker, stable identities/duplicate names, current-level selection, Open/double-click/Cancel/close, empty/failure states and unsaved-change guards.");

    assert(BROWSER_SECTION_COUNT==3);
    for(int mask=0;mask<8;mask++) for(int height=90;height<=1200;height+=30)
    {
        BrowserState browser={.objectheight=260};RECT client={0,0,300,height};
        for(int i=0;i<3;i++) browser.sections[i].expanded=(mask>>i)&1;
        BrowserLayoutSections(&browser,&client);int previous=0,total=0;
        for(int i=0;i<3;i++)
        {
            BrowserSection *s=&browser.sections[i];
            assert(s->headerrc.top==previous && s->headerrc.bottom-s->headerrc.top==BROWSER_HEADER_H);
            assert(s->bodyrc.top==s->headerrc.bottom && s->bodyrc.bottom>=s->bodyrc.top);
            assert(s->bodyrc.bottom<=height);if(!s->expanded) assert(s->bodyrc.bottom==s->bodyrc.top);
            total+=s->bodyrc.bottom-s->bodyrc.top;previous=s->bodyrc.bottom;
        }
        if(mask) assert(previous==height && total==height-3*BROWSER_HEADER_H);
        browser.fileimages=TRUE;BrowserLayoutSections(&browser,&client);
        for(int i=0;i<3;i++)
        {
            assert(browser.sections[i].headerrc.bottom==0);
            assert(browser.sections[i].expanded==(i==BROWSER_SECTION_IMAGES));
        }
        assert(!memcmp(&browser.sections[BROWSER_SECTION_IMAGES].bodyrc,&client,sizeof(client)));
    }
    puts("PASS three-section browser reclaims all body space across collapse/resize combinations; studio image-only panel remains full-size.");
    return 0;
}
