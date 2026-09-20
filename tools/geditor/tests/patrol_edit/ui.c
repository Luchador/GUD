#define Paths UiPaths
#define Reload UiReload
/* Exercise the real modeless draft functions with window-system calls stubbed. */
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM, LRESULT;
typedef unsigned int UINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
#define WM_APP 0x8000
#define DWLP_USER 1
#define WM_KEYDOWN 2
#define VK_ESCAPE 27
#define MB_ICONINFORMATION 0
#define MB_ICONERROR 0
#define MB_YESNOCANCEL 0
#define MB_ICONQUESTION 0
#define IDYES 6
#define IDNO 7
#define IDCANCEL 2
#include "patroleditor.h"
#include "resource.h"
#include "texload.h"
#include "ui_type.inc"
static HWND g_PatrolEditor;
static const char *help="help";
static int ui_path,ui_point,ui_messages,ui_selected,ui_applies,ui_dispatch;
static BOOL ui_fail_apply,ui_viewport_pick,ui_dialog_consumes,ui_fail_save;
static int ui_choice,ui_saves;
static intptr_t GetWindowLongPtr(HWND hwnd,int field) { return (intptr_t)hwnd; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static HWND GetDlgItem(HWND hwnd,int id) { return hwnd; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void SetDlgItemText(HWND hwnd,int id,const char *text) {}
static int MessageBox(HWND hwnd,const char *text,const char *title,int flags) { ui_messages++; return ui_choice; }
static int Row(HWND hwnd,int id) { return id==IDC_PATROL_PATHS ? ui_path : ui_point; }
static void Pads(HWND hwnd,PatrolEditor *e) {}
static void Paths(HWND hwnd,PatrolEditor *e,int selected) { ui_path=selected; }
static void Points(HWND hwnd,PatrolEditor *e,int selected) { ui_point=selected; }
static void Buttons(HWND hwnd,PatrolEditor *e) {}
static void ViewportSetPadPick(HWND hwnd,BOOL enabled) { ui_viewport_pick=enabled; }
static void ViewportSelectSetupPad(HWND hwnd,const SetupPadRef *pad) { ui_selected++; }
static BOOL IsChild(HWND parent,HWND child) { return child==(HWND)2; }
static BOOL IsDialogMessage(HWND hwnd,MSG *msg) { return ui_dialog_consumes; }
static void TranslateMessage(MSG *msg) {}
static void DispatchMessage(MSG *msg) { ui_dispatch++; }
static void DestroyWindow(HWND hwnd) { assert(hwnd==g_PatrolEditor); g_PatrolEditor=NULL; }
static LRESULT SendMessage(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    PatrolEditor *e=hwnd; SetupFile out={0};
    if (msg==PATROLEDITOR_WM_SAVE) {
        ui_saves++; if(ui_fail_save)return FALSE;
        ((SetupFile *)e->source)->dirty=FALSE; return TRUE;
    }
    assert(msg==PATROLEDITOR_WM_APPLY && e->applying);
    ui_applies++;
    if(ui_fail_apply)return FALSE;
    Require(PatrolDocumentCompile((PatrolDocument *)lp,e->source,&out,&why));
    SetupFile *source=(SetupFile *)e->source;
    SetupFileFree(source); *source=out;
    PatrolEditorRefresh(); /* Nested frame notification during the commit. */
    return TRUE;
}
#include "ui_logic.inc"
static void Modeless(const SetupFile *source)
{
    SetupFile s={0}; Require(SetupFileClone(source,&s,&why));
    PatrolEditor e={.source=&s,.viewport=(HWND)3};
    Require(PatrolDocumentLoad(&s,&e.doc,&why)); Require(PatrolDocumentLoad(&s,&e.baseline,&why));
    g_PatrolEditor=&e; ui_path=0; PatrolEditorSetPicking(TRUE); assert(e.picking && ui_viewport_pick);
    DWORD count=e.doc.paths[0].count;
    SetupPadRef pad={0,TRUE}; assert(!PatrolEditorPickPad(&pad));
    pad.bound=FALSE; Require(PatrolEditorPickPad(&pad));
    assert(e.doc.changed && e.doc.paths[0].count==count+1 && ui_selected==1);
    assert(e.doc.pads[e.doc.paths[0].points[count]]==0); /* ID != waypoint */
    SetupPadRef newpad; const double position[]={100,0,100};
    Require(SetupFileAddPad(&s,1,position,"test_stan",&newpad,&why));
    PatrolEditorRefresh(); assert(!e.stale && e.doc.changed && e.doc.paths[0].count==count+1);
    Require(PatrolEditorPickPad(&newpad));
    ui_fail_apply=TRUE; assert(!PatrolEditorApply()); assert(e.doc.changed && !e.applying && !ui_viewport_pick);
    ui_fail_apply=FALSE; Require(PatrolEditorApply());
    assert(!e.doc.changed && !e.stale && ui_applies==2);
    assert(e.doc.paths[0].count==count+2 && s.padcount==source->padcount+1);
    Navigation(&s);
    /* An already-applied but unsaved draft must not be lost on level change. */
    ui_choice=IDCANCEL; assert(!PatrolEditorConfirmClose(TRUE) && g_PatrolEditor);
    ui_choice=IDYES; ui_fail_save=TRUE;
    assert(!PatrolEditorConfirmClose(TRUE) && g_PatrolEditor && s.dirty);
    ui_fail_save=FALSE; assert(PatrolEditorConfirmClose(TRUE) && !g_PatrolEditor && !s.dirty && ui_saves==2);
    g_PatrolEditor=&e; ui_choice=0;
    /* Clean drafts reload external route changes, dirty drafts preserve themselves
     * and refuse to overwrite the changed level. */
    DWORD table=R(s.data+16); s.data[table+5]^=1;
    PatrolEditorRefresh(); assert(!e.stale && e.doc.paths[0].flags==s.data[table+5]);
    PatrolEditorSetPicking(TRUE); Require(PatrolEditorPickPad(&pad));
    s.data[table+5]^=1; PatrolEditorRefresh();
    assert(e.stale && e.doc.changed && !ui_viewport_pick);
    int applies=ui_applies; assert(!PatrolEditorApply() && ui_applies==applies);
    PatrolDocumentFree(&e.doc); PatrolDocumentFree(&e.baseline);
    Require(PatrolDocumentLoad(&s,&e.doc,&why)); Require(PatrolDocumentLoad(&s,&e.baseline,&why)); e.stale=FALSE;
    PatrolEditorSetPicking(TRUE);
    MSG msg={(HWND)3,WM_KEYDOWN,VK_ESCAPE,0}; assert(!PatrolEditorHandleMessage(&msg)); /* main viewport still dispatches */
    msg.hwnd=(HWND)2; assert(PatrolEditorHandleMessage(&msg) && !e.picking && !ui_viewport_pick);
    ui_dialog_consumes=FALSE; assert(PatrolEditorHandleMessage(&msg) && ui_dispatch==1);
    ui_dialog_consumes=TRUE; assert(PatrolEditorHandleMessage(&msg) && ui_dispatch==1);
    /* Undoing a newly placed draft-only pad invalidates the draft safely. */
    Require(SetupFileAddPad(&s,1,position,"test_stan",&newpad,&why));
    PatrolEditorSetPicking(TRUE); Require(PatrolEditorPickPad(&newpad));
    s.pads[newpad.index].deleted=TRUE; PatrolEditorRefresh(); assert(e.stale && !e.picking);
    PatrolDocumentFree(&e.doc); PatrolDocumentFree(&e.baseline); SetupFileFree(&s); g_PatrolEditor=NULL;
    puts("PASS: modeless live pad picks, failed/successful Apply, unrelated edits, stale undo protection and independent dialog/viewport keyboard dispatch.");
}

#undef Paths
#undef Reload
