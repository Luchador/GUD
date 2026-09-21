/* The production form/controller with Win32 controls replaced by a tiny text
 * store. This exercises draft lifetime, validation, project edits and previews. */
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
#define WM_APP 0x8000
#define GEDITOR_TITLE "GEditor"
enum { DWLP_USER=1, GW_OWNER, BST_UNCHECKED=0, BST_CHECKED=1, CB_RESETCONTENT=10,
    CB_ADDSTRING, CB_SETCURSEL, EM_SETSEL, MB_ICONERROR, MB_ICONWARNING, SW_SHOW, SW_HIDE };
#include "environmentpanel.h"
#include "ui_types.inc"
static EnvironmentPanel panel;
static GEditorProject g_Project;
static DWORD g_CurrentLevelIndex;
static BOOL g_ProjectMetadataDirty;
static HWND g_Viewport=(HWND)3, focus;
static int errors, previews, drafts;
static char controls[100][256];
static BOOL enabled[100];
static RomFog previewfog;
static RomClouds previewclouds;
static unsigned char previewrgb[3];
static BOOL GEditorApplyEnvironment(HWND hwnd, EnvironmentEditRequest *r);
static void GEditorPreviewEnvironment(DWORD id, BOOL selected);
static void GEditorSetTitleForProject(HWND hwnd) {}
static void ViewportSetBackgroundColor(HWND hwnd,const unsigned char *rgb) { memcpy(previewrgb,rgb,3); }
static void ViewportSetLevelFog(HWND hwnd,const RomFog *fog,float scale) { previewfog=*fog; }
static void ViewportSetLevelClouds(HWND hwnd,const RomClouds *clouds,const char *dir) { previewclouds=*clouds;previews++; }
static intptr_t GetWindowLongPtr(HWND hwnd,int field) { assert(hwnd==(HWND)1);return (intptr_t)&panel; }
static HWND GetParent(HWND hwnd) { return (HWND)2; }
static HWND GetWindow(HWND hwnd,int which) { assert(which==GW_OWNER);return (HWND)4; }
static intptr_t SendMessage(HWND hwnd,unsigned message,WPARAM wp,LPARAM lp)
{
    assert(hwnd==(HWND)4);
    if(message==ENVIRONMENT_WM_APPLY)return GEditorApplyEnvironment(hwnd,(EnvironmentEditRequest *)lp);
    if(message==ENVIRONMENT_WM_PREVIEW)GEditorPreviewEnvironment((DWORD)wp,lp!=0);
    else { assert(message==ENVIRONMENT_WM_DRAFT);drafts++; }
    return 0;
}
static intptr_t SendDlgItemMessage(HWND hwnd,int id,unsigned message,WPARAM wp,LPARAM lp) { return 0; }
static HWND GetDlgItem(HWND hwnd,int id) { return (HWND)(intptr_t)id; }
static void EnableWindow(HWND hwnd,BOOL enable) { enabled[(int)(intptr_t)hwnd-3000]=enable; }
static void SetDlgItemText(HWND hwnd,int id,const char *text) { snprintf(controls[id-3000],256,"%s",text); }
static void GetDlgItemText(HWND hwnd,int id,char *text,int max) { snprintf(text,max,"%s",controls[id-3000]); }
static void CheckDlgButton(HWND hwnd,int id,int checked) { SetDlgItemText(hwnd,id,checked ? "1" : "0"); }
static int IsDlgButtonChecked(HWND hwnd,int id) { return controls[id-3000][0]=='1'; }
static void SetFocus(HWND hwnd) { focus=hwnd; }
static void MessageBox(HWND hwnd,const char *text,const char *title,unsigned flags) { assert(text[0]);errors++; }
static void ShowWindow(HWND hwnd,int show) {}
static EnvironmentTable diskdefaults;
static BOOL LevelManagerHasSettingsDraft(void) { return EnvironmentPanelHasDraft((HWND)1); }
static BOOL GEditorConfirmExit(HWND hwnd)
{ OK(ProjectSave(&g_Project,&why));g_ProjectMetadataDirty=FALSE;return TRUE; }
static BOOL RomExportRefreshProjectLevelMetadata(GEditorProject *project,const char **error)
{
    project->environments=diskdefaults;
    EnvironmentRefreshLevels(&project->environments,&project->environmentOverrides,project->levels,project->levelcount);
    return EnvironmentValidateOverrides(&project->environments,&project->environmentOverrides,error);
}
static BOOL PatrolEditorConfirmClose(BOOL close) { return TRUE; }
static void GEditorCloseProject(HWND hwnd) { memset(&g_Project,0,sizeof(g_Project)); }
static void GEditorRefreshProjectAssets(void) {}
static void GEditorRememberProject(void) {}
#include "ui.inc"
static void UI(const char *dir)
{
    HWND hwnd=(HWND)1;
    g_Project.environments.count=2;g_Project.environments.rows[0]=Row(29);g_Project.environments.rows[1]=Row(129);
    Edit(&g_Project.environments.rows[1],"farclip","9000");
    g_Project.levelcount=1;g_Project.levels[0].levelID=29;g_Project.levels[0].levelscale=g_Project.levels[0].renderScale=1;
    strcpy(g_Project.levels[0].name,"Depot");
    strcpy(g_Project.name,"Drafts");strcpy(g_Project.geppath,"drafts.gep");
    EnvironmentRefreshLevels(&g_Project.environments,&g_Project.environmentOverrides,g_Project.levels,1);
    EnvironmentPanelRefresh(hwnd,&g_Project,0);
    assert(!panel.draft&&panel.count==2&&panel.selected==0&&enabled[ENV_FIELD_FIRST+2-3000]);
    EnvironmentPanelShow(hwnd,TRUE);assert(previewfog.farclip==3000&&previewclouds.height==7500);
    strcpy(controls[ENV_FIELD_FIRST+2-3000],"invalid");panel.draft=TRUE;
    assert(EnvironmentPanelHasDraft(hwnd));assert(!EnvironmentPanelApply(hwnd)&&errors==1&&panel.draft&&!g_ProjectMetadataDirty);
    assert(focus==GetDlgItem(hwnd,ENV_FIELD_FIRST+2)&&!g_Project.environmentOverrides.count);
    EnvironmentPanelRefresh(hwnd,&g_Project,0);assert(!strcmp(controls[ENV_FIELD_FIRST+2-3000],"invalid"));
    strcpy(controls[ENV_FIELD_FIRST+2-3000],"6000");assert(EnvironmentPanelApply(hwnd));
    assert(!panel.draft&&g_ProjectMetadataDirty&&g_Project.environmentOverrides.count==1&&previewfog.farclip==6000);
    assert(g_Project.levels[0].fog.farclip==6000&&F32(g_Project.environments.rows[0].data+12)==3000);
    strcpy(controls[ENV_FIELD_FIRST+9-3000],"996");panel.draft=TRUE;
    assert(!EnvironmentPanelApply(hwnd)&&panel.draft&&errors==2&&g_Project.levels[0].fog.end==1000);
    Load(hwnd);assert(!panel.draft&&!strcmp(controls[ENV_FIELD_FIRST+9-3000],"1000"));
    panel.selected=1;Load(hwnd);Preview(hwnd);assert(previewfog.farclip==9000);
    EnvironmentPanelShow(hwnd,FALSE);assert(previewfog.farclip==6000);
    panel.selected=0;assert(Commit(hwnd,&g_Project.environments.rows[0]));assert(!g_Project.environmentOverrides.count&&!panel.draft);
    EnvironmentPanelRefresh(hwnd,NULL,(DWORD)-1);assert(!enabled[ENV_APPLY-3000]&&!panel.count&&!panel.project);
    assert(EnvironmentPanelApply(NULL)&&!EnvironmentPanelHasDraft(NULL));
    /* Reopening this same project after choosing Save must read the new
     * metadata, not an older snapshot read before the save prompt. */
    snprintf(g_Project.geppath,sizeof(g_Project.geppath),"%s/reopen.gep",dir);
    strcpy(g_Project.dir,dir);OK(ProjectSave(&g_Project,&why));
    EditorEnvironment value=g_Project.environments.rows[0];Edit(&value,"farclip","7000");
    OK(EnvironmentSet(&g_Project.environments,&g_Project.environmentOverrides,&value,&why));
    g_ProjectMetadataDirty=TRUE;diskdefaults=g_Project.environments;
    GEditorOpenProject((HWND)4,g_Project.geppath);
    assert(g_Project.levelcount==1&&g_Project.environmentOverrides.count==1&&g_Project.levels[0].fog.farclip==7000);
    puts("PASS: form apply/revert/reset, invalid-draft retention, refresh protection, dirty state, variant previews and reopening the same project after Save.");
}
