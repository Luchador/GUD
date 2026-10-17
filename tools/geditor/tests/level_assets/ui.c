#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "levelmenuassets.h"
#include "resource.h"
typedef int BOOL;typedef int HWND;typedef intptr_t LPARAM;
#define TRUE 1
#define FALSE 0
#define MB_ICONWARNING 0
#define PROJECTSETTINGS_WM_DRAFT 1
#define PROJECTSETTINGS_WM_LEVEL_ASSETS 2
#define PROJECTSETTINGS_WM_LEVEL_SLOT 3
typedef struct {unsigned slot;LevelMenuAsset value;const char *why;} ProjectLevelAssetEdit;
typedef struct {unsigned slot;int stage;const char *why;} ProjectLevelSlotEdit;
static HWND g_ProjectSettings=1;
static BOOL g_AssetDraft;
static int g_AssetSlot,combos[7],assetCalls,stageCalls,reloads,warnings,fail;
static char names[3][32];static ProjectLevelAssetEdit saved;
static HWND GetParent(HWND w){return w;}
static void GetDlgItemText(HWND w,int id,char *s,int size)
{(void)w;assert(size==32);strcpy(s,names[id-IDC_PROJECT_ASSET_FIELD_FIRST-3]);}
static int AssetComboValue(int field){return combos[field];}
static void ProjectSettingsPopulateSlots(void){}
static void ProjectSettingsLoadInspector(void){reloads++;g_AssetDraft=FALSE;}
static void MessageBox(HWND w,const char *a,const char *b,int f){(void)w;(void)a;(void)b;(void)f;warnings++;}
static int SendMessage(HWND w,int m,int unused,LPARAM p)
{
    (void)w;(void)unused;
    if(m==PROJECTSETTINGS_WM_LEVEL_ASSETS){assetCalls++;saved=*(ProjectLevelAssetEdit *)p;return !fail;}
    if(m==PROJECTSETTINGS_WM_LEVEL_SLOT){stageCalls++;return !fail;}
    return 1;
}
#include "inspector.inc"
int main(void)
{
    for(int i=0;i<7;i++)combos[i]=-2;
    ProjectSettingsSelectSlot(3);assert(g_AssetSlot==3&&!stageCalls&&!assetCalls);
    g_AssetDraft=TRUE;strcpy(names[0],"Harbour");strcpy(names[1],"Nightfall");strcpy(names[2],"Boarding");
    combos[0]=17;combos[1]=18;combos[2]=19;combos[6]=26;
    ProjectSettingsSelectSlot(4);
    assert(assetCalls==1&&saved.slot==3&&saved.value.flags==127&&!g_AssetDraft&&g_AssetSlot==4&&!stageCalls);
    assert(!strcmp(saved.value.names[1],"Nightfall")&&saved.value.briefingStage==26&&saved.value.images[2]==19);
    g_AssetDraft=TRUE;fail=TRUE;int old=reloads;
    ProjectSettingsSelectSlot(5);assert(g_AssetSlot==4&&g_AssetDraft&&reloads==old&&warnings==1);
    ProjectSettingsAssignSlot(6,33);assert(!stageCalls&&g_AssetSlot==4&&g_AssetDraft);
    fail=FALSE;ProjectSettingsAssignSlot(6,33);assert(stageCalls==1&&g_AssetSlot==6&&!g_AssetDraft);
    ProjectSettingsSelectSlot(20);g_AssetDraft=TRUE;assert(ProjectSettingsApply());
    assert(saved.slot==20&&saved.value.flags==LEVEL_ASSET_MP_FIELDS&&saved.value.images[1]==0&&!saved.value.names[1][0]);
    combos[0]=-2;names[0][0]=0;g_AssetDraft=TRUE;assert(ProjectSettingsApply()&&!saved.value.flags);
    assert(!ProjectSettingsHasDraft());g_AssetDraft=TRUE;assert(ProjectSettingsHasDraft());
    g_ProjectSettings=0;assert(ProjectSettingsApply()&&!ProjectSettingsHasDraft());
    puts("PASS: click selects without assigning, independent form values, auto-apply on selection, invalid-draft retention, explicit assignment, MP fields and default resets.");
}
