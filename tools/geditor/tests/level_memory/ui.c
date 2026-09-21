typedef void *HWND;typedef uintptr_t WPARAM;typedef intptr_t LPARAM;
#define WM_APP 0x8000
enum {DWLP_USER=1,GW_OWNER,MB_ICONERROR,MB_ICONWARNING,EM_SETSEL};
#include "stageoptions.h"
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
#include "ui.inc"
static void UI(void)
{
    HWND hwnd=(HWND)1;RomFile rom;unsigned char data[2048];Fixture(&rom,data);
    strcpy(g_Project.name,"Memory");strcpy(g_Project.geppath,"memory.gep");
    g_Project.levelcount=3;memcpy(g_Project.levels,rom.info.levels,3*sizeof(RomLevel));
    StageOptionsRefresh(hwnd,&g_Project,0);OK(!panel.ready&&!enabled[MEMORY_FIELD_FIRST-3200]);
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
    OK(Commit(hwnd,&g_Project.memory.rows[0])&&!g_Project.memoryOverrides.count);
    g_CurrentLevelIndex=1;StageOptionsRefresh(hwnd,&g_Project,1);OK(!strcmp(controls[MEMORY_FIELD_FIRST-3200],"130"));
    strcpy(controls[MEMORY_FIELD_FIRST+2-3200],"900");panel.draft=TRUE;OK(StageOptionsApply(hwnd));
    OK(g_Project.memoryOverrides.rows[0].value.id==427);
    g_CurrentLevelIndex=2;StageOptionsRefresh(hwnd,&g_Project,2);OK(!strcmp(controls[MEMORY_FIELD_FIRST+3-3200],"1"));
    StageOptionsRefresh(hwnd,NULL,0);OK(!panel.ready&&!panel.draft&&!enabled[MEMORY_RESET-3200]);
    OK(StageOptionsApply(NULL)&&!StageOptionsHasDraft(NULL));
    puts("PASS: Stage Options defaults, doubled graphics total, draft retention, invalid-field focus, apply/revert/reset, dirty state and independent stage selection.");
}
