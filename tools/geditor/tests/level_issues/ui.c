/* Compile production report/controller functions with a small window-system
 * shim. Diagnostics and location validation still use the real level code. */
typedef void *HWND;
typedef void *HCURSOR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM, LRESULT;
typedef unsigned int UINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
enum { WM_APP=0x8000, DWLP_USER=1, WM_SETREDRAW, IDC_WAIT, LVNI_SELECTED,
       LVIF_TEXT, CB_GETCURSEL, LVIS_SELECTED=1, LVIS_FOCUSED=2, IDOK=1,
       IDYES=6, IDNO=7, IDCANCEL=2, MB_ICONQUESTION=0, MB_YESNOCANCEL=0 };
#define BROWSER_WM_LEVEL_OPEN (WM_APP + 1)
#define GEDITOR_TITLE "GEditor"
typedef struct { int mask,iItem; char *pszText; } LVITEM;
#include "issueswindow.h"
#include "resource.h"
#include "edittool.h"
#include "ui_type.inc"
static HWND g_IssuesWindow;
static int row=-1, rows, dispatched, scans, locates;
static int scope;
static LRESULT OpenLevel(WPARAM index);
static LRESULT SendDlgItemMessage(HWND hwnd,int id,UINT message,WPARAM wp,LPARAM lp)
{ assert(id==IDC_ISSUES_SCOPE && message==CB_GETCURSEL);return scope; }
static BOOL button, fail_scan, fail_insert, fail_locate, consume, invalidate_on_locate;
static BOOL empty_scan;
static char status[512], detail[512];
static intptr_t GetWindowLongPtr(HWND hwnd,int field) { return (intptr_t)hwnd; }
static HWND GetParent(HWND hwnd) { return (HWND)1; }
static HWND GetDlgItem(HWND hwnd,int id) { return (HWND)(intptr_t)id; }
static int ListView_GetNextItem(HWND hwnd,int start,int flags) { return row; }
static void ListView_DeleteAllItems(HWND hwnd) { row=-1;rows=0; }
static int ListView_InsertItem(HWND hwnd,LVITEM *item) { return fail_insert ? -1 : rows++; }
static void ListView_SetItemText(HWND hwnd,int at,int column,const char *text) {}
static void ListView_SetItemState(HWND hwnd,int at,int state,int mask) { row=at; }
static void EnableWindow(HWND hwnd,BOOL enabled) { button=enabled; }
static void SetDlgItemText(HWND hwnd,int id,const char *text)
{ snprintf(id==IDC_ISSUES_STATUS ? status : detail,sizeof(status),"%s",text); }
static HCURSOR SetCursor(HCURSOR c) { return c; }
static HCURSOR LoadCursor(void *instance,int id) { return NULL; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static BOOL IsChild(HWND parent,HWND child) { return child==(HWND)2; }
static BOOL IsDialogMessage(HWND hwnd,MSG *msg) { return consume; }
static void TranslateMessage(MSG *msg) {}
static void DispatchMessage(MSG *msg) { dispatched++; }
static LRESULT SendMessage(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==BROWSER_WM_LEVEL_OPEN)return OpenLevel(wp);
    if(message==WM_SETREDRAW)return TRUE;
    if(message==ISSUES_WM_SCAN)
    {
        scans++; IssuesScanRequest *r=(IssuesScanRequest *)lp;assert(r->exported==(scope==1));
        if(fail_scan) { r->reason="Failed scan";return FALSE; }
        if(empty_scan) { return TRUE; }
        r->report.items=calloc(1,sizeof(LevelIssue));assert(r->report.items);
        r->report.count=r->report.capacity=r->report.warnings=1;
        r->report.items[0].target=LEVEL_ISSUE_PAD;r->report.items[0].pad.index=7;
        strcpy(r->report.items[0].description,"Pad problem");return TRUE;
    }
    assert(message==ISSUES_WM_LOCATE);locates++;
    LevelIssue *issue=(LevelIssue *)lp;
    if(invalidate_on_locate)IssuesWindowInvalidate();
    assert(issue->target==LEVEL_ISSUE_PAD && issue->pad.index==7);
    return !fail_locate;
}
#include "ui_logic.inc"
static void Window(void)
{
    IssuesWindow state={0};g_IssuesWindow=&state;
    Check(g_IssuesWindow,&state);assert(!state.stale && rows==1 && button && scans==1 && !strcmp(detail,"Pad problem"));
    Locate(g_IssuesWindow,&state);assert(locates==1);
    fail_locate=TRUE;Locate(g_IssuesWindow,&state);assert(strstr(detail,"could not be located"));fail_locate=FALSE;
    state.report.items[0].target=LEVEL_ISSUE_NOWHERE;Details(g_IssuesWindow,&state);assert(!button);
    Locate(g_IssuesWindow,&state);assert(locates==2);
    IssuesWindowInvalidate();assert(state.stale && !state.report.items && !rows && !button && strstr(status,"level changed"));
    row=0;Locate(g_IssuesWindow,&state);assert(locates==2);
    Check(g_IssuesWindow,&state);assert(!state.stale && rows==1 && scans==2);
    invalidate_on_locate=TRUE;Locate(g_IssuesWindow,&state);assert(state.stale && !state.report.items);invalidate_on_locate=FALSE;
    fail_scan=TRUE;Check(g_IssuesWindow,&state);assert(state.stale && !rows && !strcmp(detail,"Failed scan"));fail_scan=FALSE;
    fail_insert=TRUE;Check(g_IssuesWindow,&state);assert(state.stale && !state.report.items && !rows);fail_insert=FALSE;
    scope=1;Check(g_IssuesWindow,&state);assert(state.exported && !state.stale && strstr(status,"All setups"));
    IssuesWindowInvalidate();assert(!state.stale && state.report.items && rows==1); /* immutable export snapshot */
    empty_scan=TRUE;IssuesWindowRefreshExport();assert(!rows && !state.stale && !button && strstr(status,"No unresolved pads"));empty_scan=FALSE;
    scope=0;Check(g_IssuesWindow,&state);IssuesWindowInvalidate();assert(state.stale && !rows);
    MSG msg={(HWND)3,0,0,0};assert(!IssuesWindowHandleMessage(&msg)); /* viewport remains live */
    msg.hwnd=(HWND)2;assert(IssuesWindowHandleMessage(&msg) && dispatched==1);
    consume=TRUE;assert(IssuesWindowHandleMessage(&msg) && dispatched==1);
    Clear(g_IssuesWindow,&state);g_IssuesWindow=NULL;
    assert(!IssuesWindowHandleMessage(&msg));
    int previous_scans=scans;IssuesWindowRefreshExport();assert(!g_IssuesWindow && scans==previous_scans);
    puts("PASS: modeless message scope, scan/recheck, stale-result protection, failures and safe copied navigation.");
}

static HWND g_Viewport=(HWND)3,g_RightPanel=(HWND)4,g_ToolToolbar=(HWND)5;
static BgDocument g_CurrentBgDocument;
static StanFile g_CurrentStan;
static SetupFile g_CurrentSetup;
static struct { DWORD levelcount; struct { float levelscale; char setupname[32]; } levels[2]; } g_Project={.levelcount=1,.levels={{.levelscale=.25f}}};
static DWORD g_CurrentLevelIndex;
static BOOL dirty,fail_save,fail_open;
static int choice=IDYES,saves,opens;
static LRESULT OpenLevel(WPARAM index) { opens++;if(!fail_open)g_CurrentLevelIndex=index;return 0; }
static BOOL GEditorHasUnsavedChanges(void) { return dirty; }
static BOOL GEditorSaveProject(HWND hwnd) { saves++;return !fail_save; }
static int MessageBox(HWND hwnd,const char *text,const char *title,int flags) { return choice; }
static BOOL transforming,flying,rendered=TRUE,zoom_selected=TRUE,show_objects,show_stan,show_portals,picking=TRUE;
static int selected_model,selected_pad,selected_stan,selected_portal,revealed,focused,zoom_bounds;
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static void PatrolEditorSetPicking(BOOL enabled) { picking=enabled; }
static void ViewportSetColorPick(HWND hwnd,BOOL enabled) { assert(!enabled); }
static void ViewportSetTool(HWND hwnd,EditorTool tool) { assert(tool==EDITOR_TOOL_FACE_SELECT); }
static void ToolToolbarSetTool(HWND hwnd,EditorTool tool) { assert(tool==EDITOR_TOOL_FACE_SELECT); }
static void RightPanelShowObjects(HWND hwnd) { show_objects=TRUE; }
static void RightPanelShowStan(HWND hwnd) { show_stan=TRUE; }
static void RightPanelShowPortals(HWND hwnd) { show_portals=TRUE; }
static void ViewportSelectSetupModel(HWND hwnd,DWORD index) { selected_model=index; }
static BOOL ViewportGetSelectedObject(HWND hwnd,DWORD *index) { *index=selected_model;return TRUE; }
static BOOL ViewportCanZoomToSelected(HWND hwnd) { return rendered; }
static void ViewportSelectSetupPad(HWND hwnd,const SetupPadRef *pad) { selected_pad=pad->index; }
static BOOL ViewportRevealStanTile(HWND hwnd,DWORD index) { revealed=index;return TRUE; }
static BOOL ViewportSelectStanTiles(HWND hwnd,const DWORD *indices,DWORD count)
{ assert(show_stan && revealed==(int)*indices && count==1);selected_stan=*indices;return TRUE; }
static BOOL ViewportSelectPortal(HWND hwnd,DWORD index) { assert(show_portals);selected_portal=index;return TRUE; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static BOOL ViewportZoomToSelected(HWND hwnd) { return zoom_selected; }
static BOOL ViewportZoomToBounds(HWND hwnd,const double min[3],const double max[3])
{ for(int a=0;a<3;a++)assert(isfinite(min[a])&&max[a]>=min[a]);zoom_bounds++;return TRUE; }
static void SetFocus(HWND hwnd) { assert(hwnd==g_Viewport);focused++; }
#include "navigation.inc"
static void Navigation(void)
{
    SetupPad pad={.pos={10,20,10}};SetupObject object={.type=PROPDEF_PROP};
    g_CurrentSetup=(SetupFile){.pads=&pad,.padcount=1,.objects=&object,.objectcount=1};
    LevelIssue issue={.target=LEVEL_ISSUE_MODEL,.pad={0,FALSE}};
    selected_model=selected_pad=-1;
    Require(GEditorLocateIssue(NULL,&issue));assert(show_objects && !picking && selected_model==0 && selected_pad==-1 && focused==1);
    rendered=FALSE;zoom_selected=FALSE;
    Require(GEditorLocateIssue(NULL,&issue));assert(selected_pad==0 && zoom_bounds==1);
    issue.target=LEVEL_ISSUE_PAD;Require(GEditorLocateIssue(NULL,&issue));
    issue.target=LEVEL_ISSUE_STAN;g_CurrentStan=Collision();selected_stan=revealed=-1;
    Require(GEditorLocateIssue(NULL,&issue));assert(show_stan && selected_stan==0 && revealed==0);
    BgPortal portal={.pointcount=3};g_CurrentBgDocument.portals.portals=&portal;g_CurrentBgDocument.portals.portalcount=1;
    issue.target=LEVEL_ISSUE_PORTAL_FACE;selected_portal=-1;
    Require(GEditorLocateIssue(NULL,&issue));assert(selected_portal==0 && show_portals);
    int focus_before=focused;
    transforming=TRUE;assert(!GEditorLocateIssue(NULL,&issue));transforming=FALSE;
    flying=TRUE;assert(!GEditorLocateIssue(NULL,&issue));flying=FALSE;
    g_CurrentLevelIndex=1;assert(!GEditorLocateIssue(NULL,&issue));g_CurrentLevelIndex=0;
    issue.target=LEVEL_ISSUE_NOWHERE;assert(!GEditorLocateIssue(NULL,&issue));
    assert(focused==focus_before);
    /* Opening another level preserves the report and honors cancel/save failure. */
    g_Project.levelcount=2;g_Project.levels[1].levelscale=.25f;
    strcpy(g_Project.levels[1].setupname,"UsetupissueZ");strcpy(g_CurrentSetup.name,"UsetupissueZ");
    issue=(LevelIssue){.kind=LEVEL_ISSUE_UNRESOLVED_PAD,.target=LEVEL_ISSUE_PAD,.exportlevel=1,.exportscale=.25f};
    strcpy(issue.exportsetup,"UsetupissueZ");
    for(int a=0;a<3;a++)issue.exportposition[a]=pad.pos[a]*4;
    dirty=TRUE;choice=IDCANCEL;assert(!GEditorLocateIssue(NULL,&issue) && !opens && !saves);
    choice=IDYES;fail_save=TRUE;assert(!GEditorLocateIssue(NULL,&issue) && !opens && saves==1);fail_save=FALSE;
    fail_open=TRUE;assert(!GEditorLocateIssue(NULL,&issue) && opens==1);fail_open=FALSE;
    Require(GEditorLocateIssue(NULL,&issue));assert(g_CurrentLevelIndex==1 && opens==2 && selected_model==0);
    pad.pos[0]++;assert(!GEditorLocateIssue(NULL,&issue));pad.pos[0]--;
    strcpy(pad.stanname,"changed");assert(!GEditorLocateIssue(NULL,&issue));pad.stanname[0]=0;
    pad.deleted=TRUE;assert(!GEditorLocateIssue(NULL,&issue));pad.deleted=FALSE;
    issue.exportscale=1;assert(!GEditorLocateIssue(NULL,&issue));issue.exportscale=.25f;
    strcpy(issue.exportsetup,"Ump_setupissueZ");assert(!GEditorLocateIssue(NULL,&issue));
    StanFileFree(&g_CurrentStan);
    puts("PASS: viewport navigation, cross-level export navigation, cancel/save/load failure, and changed/deleted pad snapshot protection.");
}
