typedef void *HWND;typedef uintptr_t WPARAM;typedef intptr_t LPARAM;
#define WM_APP 0x8000
enum {DWLP_USER=1,GW_OWNER,MB_ICONERROR,MB_ICONWARNING,EM_SETSEL};
#include <ctype.h>
#include <errno.h>
#include "stageoptions.h"
#include "levelscale.h"
#include "objectload.h"
#include "editorpath.h"
#define ROM_EXPORT_BASE_FILENAME "base.z64"
#include "ui_types.inc"
static StageOptions panel;
static GEditorProject g_Project;
static DWORD g_CurrentLevelIndex;
static BOOL g_ProjectMetadataDirty;
static char controls[100][256];static BOOL enabled[100];static HWND focus;static int errors;
static BOOL GEditorApplyStageOptions(HWND hwnd,StageOptionsEditRequest *request);
static intptr_t GetWindowLongPtr(HWND hwnd,int index) { return (intptr_t)&panel; }
static HWND GetParent(HWND hwnd) { return (HWND)2; }
static HWND GetWindow(HWND hwnd,int which) { return (HWND)3; }
static intptr_t SendMessage(HWND hwnd,unsigned msg,WPARAM wp,LPARAM lp)
{ if(msg==STAGEOPTIONS_WM_APPLY)return GEditorApplyStageOptions(hwnd,(StageOptionsEditRequest *)lp);assert(msg==STAGEOPTIONS_WM_DRAFT);return 0; }
static HWND GetDlgItem(HWND hwnd,int id) { return (HWND)(intptr_t)id; }
static void GetDlgItemText(HWND hwnd,int id,char *out,int size) { snprintf(out,size,"%s",controls[id-3200]); }
static void SetDlgItemText(HWND hwnd,int id,const char *text) { snprintf(controls[id-3200],256,"%s",text); }
static void EnableWindow(HWND hwnd,BOOL enable) { enabled[(int)(intptr_t)hwnd-3200]=enable; }
static void MessageBox(HWND hwnd,const char *text,const char *title,unsigned flags) { assert(text[0]);errors++; }
static void SetFocus(HWND hwnd) { focus=hwnd; }
static void SendDlgItemMessage(HWND hwnd,int id,unsigned msg,WPARAM wp,LPARAM lp) {}
static void GEditorSetTitleForProject(HWND hwnd) {}
static HWND g_Viewport;
static BgDocument g_CurrentBgDocument, g_FaceClipboard;
static BgPortalFile g_PortalClipboard;
static StanFile g_CurrentStan;
static SetupFile g_CurrentSetup, g_ObjectClipboard;
static SetupObjectGeometry g_ObjectClipboardPose;
static EditHistory g_EditHistory;
static int rebuilds, previews;
static BOOL fail_rebuild, fail_defaults;
static void ViewportCancelTransform(HWND hwnd) {}
static void UVEditorCancelInteraction(HWND owner) {}
static void GEditorPreviewEnvironment(DWORD id, BOOL selected) { previews++; }
static void LevelManagerRefreshRooms(const BgDocument *bg,const SetupFile *setup,const StanFile *stan) {}
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **reason) { rebuilds++; *reason="Rebuild failed."; return !fail_rebuild; }
static BOOL GEditorRebuildCurrentViewport(const char **reason) { return TRUE; }
BOOL ObjectLoadSetupGeometry(const char *dir,const SetupFile *setup,const StanFile *stan,float scale,SetupObjectGeometry *out,const char **reason) { return TRUE; }
void ObjectGeometryFree(SetupObjectGeometry *objects) {}
static BOOL TestRomValidate(const char *path,RomInfo *info,const char **reason)
{
    assert(strstr(path,"base.z64"));
    if(fail_defaults) { *reason="Could not read base ROM."; return FALSE; }
    *info=(RomInfo){.levelcount=1};
    info->levels[0]=(RomLevel){.levelID=27,.levelscale=1,.renderScale=2,.chrLODDistance=.8f}; return TRUE;
}
#define RomValidate TestRomValidate
#include "ui.inc"
#undef RomValidate
static void UI(void)
{
    HWND hwnd=(HWND)1;RomFile rom;unsigned char data[2048];Fixture(&rom,data);
    strcpy(g_Project.name,"Memory");strcpy(g_Project.geppath,"memory.gep");
    g_Project.levelcount=3;memcpy(g_Project.levels,rom.info.levels,3*sizeof(RomLevel));
    StageOptionsRefresh(hwnd,&g_Project,0);OK(panel.ready&&!panel.memoryready&&!enabled[MEMORY_FIELD_FIRST-3200]);
    OK(LevelMemoryReadRom(&rom,&g_Project.memory,&why));StageOptionsRefresh(hwnd,&g_Project,0);
    OK(panel.ready&&!panel.draft&&!strcmp(controls[MEMORY_FIELD_FIRST-3200],"100"));
    OK(strstr(controls[MEMORY_TOTAL-3200],"1175 KiB"));
    strcpy(controls[MEMORY_FIELD_FIRST-3200],"0");panel.draft=TRUE;
    OK(StageOptionsHasDraft(hwnd)&&!StageOptionsApply(hwnd)&&errors==1&&panel.draft&&!g_ProjectMetadataDirty);
    OK(focus==GetDlgItem(hwnd,MEMORY_FIELD_FIRST));
    StageOptionsRefresh(hwnd,&g_Project,0);OK(!strcmp(controls[MEMORY_FIELD_FIRST-3200],"0"));
    strcpy(controls[MEMORY_FIELD_FIRST-3200],"200");OK(StageOptionsApply(hwnd)&&!panel.draft&&g_ProjectMetadataDirty);
    OK(g_Project.memoryOverrides.count==1&&g_Project.memory.rows[0].kib[0]==100);
    strcpy(controls[MEMORY_FIELD_FIRST-3200],"300");panel.draft=TRUE;Load(hwnd);
    OK(!panel.draft&&!strcmp(controls[MEMORY_FIELD_FIRST-3200],"200"));
    StageOptionsEditRequest reset={.value=g_Project.memory.rows[0],.why="",.defaults=TRUE};
    OK(Commit(hwnd,&reset)&&!g_Project.memoryOverrides.count);
    OK(g_Project.levels[0].levelscale==1&&g_Project.levels[0].renderScale==2&&g_Project.levels[0].chrLODDistance==.8f);
    /* Parse, retain and round-trip each float without converting native data. */
    const char *badscales[]={"", "0", "-0.1", "nan", "inf", "1e40", "1e-50", "0.25junk", "1 2"};
    g_ProjectMetadataDirty=FALSE;
    for(unsigned i=0;i<sizeof(badscales)/sizeof(*badscales);i++)
    {
        strcpy(controls[SCALE_FIELD_FIRST-3200],badscales[i]);panel.draft=TRUE;
        OK(!StageOptionsApply(hwnd)&&!g_ProjectMetadataDirty&&g_Project.levels[0].levelscale==1);
        OK(focus==GetDlgItem(hwnd,SCALE_FIELD_FIRST));
    }
    strcpy(controls[SCALE_FIELD_FIRST-3200],"0.333333343");
    strcpy(controls[SCALE_FIELD_FIRST+1-3200]," 1.25 ");
    OK(StageOptionsApply(hwnd)&&g_ProjectMetadataDirty&&!panel.draft);
    OK(g_Project.levels[0].levelscale==0.333333343f&&g_Project.levels[0].renderScale==1.25f);
    int before=rebuilds;g_ProjectMetadataDirty=FALSE;panel.draft=TRUE;
    OK(StageOptionsApply(hwnd)&&rebuilds==before&&!g_ProjectMetadataDirty);
    strcpy(controls[SCALE_FIELD_FIRST+1-3200],"2");panel.draft=TRUE;
    OK(StageOptionsApply(hwnd)&&rebuilds==before&&g_ProjectMetadataDirty);
    g_ProjectMetadataDirty=FALSE;
    for(unsigned i=0;i<sizeof(badscales)/sizeof(*badscales);i++) {
        strcpy(controls[LOD_FIELD-3200],badscales[i]);panel.draft=TRUE;
        OK(!StageOptionsApply(hwnd)&&!g_ProjectMetadataDirty&&g_Project.levels[0].chrLODDistance==.8f);
        OK(focus==GetDlgItem(hwnd,LOD_FIELD));
    }
    strcpy(controls[LOD_FIELD-3200]," 1.375 ");panel.draft=TRUE;
    OK(StageOptionsApply(hwnd)&&g_ProjectMetadataDirty&&rebuilds==before&&g_Project.levels[0].chrLODDistance==1.375f);
    strcpy(controls[LOD_FIELD-3200],"2");panel.draft=TRUE;Load(hwnd);
    OK(!panel.draft&&!strcmp(controls[LOD_FIELD-3200],"1.375"));
    g_Project.levels[0].chrLODDistance=1.25f;StageOptionsRefresh(hwnd,&g_Project,0);
    OK(!strcmp(controls[LOD_FIELD-3200],"1.25"));
    float oldscale=g_Project.levels[0].levelscale;
    strcpy(controls[SCALE_FIELD_FIRST-3200],"0.5");
    strcpy(controls[MEMORY_FIELD_FIRST-3200],"250");panel.draft=TRUE;fail_rebuild=TRUE;g_ProjectMetadataDirty=FALSE;
    OK(!StageOptionsApply(hwnd)&&panel.draft&&!g_ProjectMetadataDirty&&!g_Project.memoryOverrides.count);
    OK(g_Project.levels[0].levelscale==oldscale&&g_CurrentBgDocument.levelscale==oldscale);
    fail_rebuild=FALSE;fail_defaults=TRUE;
    OK(!Commit(hwnd,&reset)&&panel.draft&&g_Project.levels[0].levelscale==oldscale);
    fail_defaults=FALSE;OK(Commit(hwnd,&reset)&&!panel.draft&&g_Project.levels[0].levelscale==1&&g_Project.levels[0].chrLODDistance==.8f);
    /* Scale controls work on bases lacking the memory-allocation manifest. */
    LevelMemoryTable savedmemory=g_Project.memory;g_Project.memory=(LevelMemoryTable){0};
    StageOptionsRefresh(hwnd,&g_Project,0);OK(panel.ready&&!panel.memoryready);
    strcpy(controls[SCALE_FIELD_FIRST-3200],"0.75");panel.draft=TRUE;
    OK(StageOptionsApply(hwnd)&&g_Project.levels[0].levelscale==0.75f);
    g_Project.memory=savedmemory;
    g_CurrentLevelIndex=1;StageOptionsRefresh(hwnd,&g_Project,1);OK(!strcmp(controls[MEMORY_FIELD_FIRST-3200],"130"));
    strcpy(controls[MEMORY_FIELD_FIRST+2-3200],"900");panel.draft=TRUE;OK(StageOptionsApply(hwnd));
    OK(g_Project.memoryOverrides.rows[0].value.id==427);
    g_CurrentLevelIndex=2;StageOptionsRefresh(hwnd,&g_Project,2);OK(!strcmp(controls[MEMORY_FIELD_FIRST+3-3200],"1"));
    StageOptionsRefresh(hwnd,NULL,0);OK(!panel.ready&&!panel.draft&&!enabled[MEMORY_RESET-3200]);
    OK(StageOptionsApply(NULL)&&!StageOptionsHasDraft(NULL));
    OK(previews>0);
    puts("PASS: Stage Options scales, float validation, scale-only bases, preview rollback, defaults, doubled graphics total, draft retention, invalid-field focus, apply/revert/reset, dirty state and independent stage selection.");
}
