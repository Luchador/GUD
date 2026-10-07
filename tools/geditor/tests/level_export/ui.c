#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "levelexport.h"
#include "editorpath.h"
typedef void *HWND;
typedef void *HCURSOR;
typedef struct {
    unsigned int lStructSize; HWND hwndOwner; const char *lpstrTitle,*lpstrFilter;
    char *lpstrFile; unsigned int nMaxFile; const char *lpstrDefExt,*lpstrInitialDir; unsigned int Flags;
} OPENFILENAME;
enum { OFN_OVERWRITEPROMPT=1,OFN_PATHMUSTEXIST=2,OFN_NOCHANGEDIR=4,MB_ICONERROR=16,IDC_WAIT=1 };
#define GEDITOR_TITLE "GEditor"
static struct { DWORD levelcount;char dir[MAX_PATH]; } g_Project;
static DWORD g_CurrentLevelIndex;
static StanFile g_CurrentStan;
static BgDocument g_CurrentBgDocument;
static BgFile g_CurrentBg;
static HWND g_Viewport;
static BOOL accepted,fail;
static unsigned int dialogs,backgrounds,stans,errors,cancels,dialogerror;
static const char *expectedname;
static char lastmessage[256];
static BOOL GetSaveFileName(OPENFILENAME *ofn)
{
    assert(ofn->Flags==(OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR));
    assert(!strcmp(ofn->lpstrDefExt,"glb"));dialogs++;
    /* Native background names are paths; the dialog needs a filename. */
    assert(!strpbrk(ofn->lpstrFile,"/\\:"));
    if (expectedname) { assert(!strcmp(ofn->lpstrFile,expectedname)); }
    return accepted;
}
static unsigned int CommDlgExtendedError(void) { return dialogerror; }
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags)
{ assert(text&&*text);snprintf(lastmessage,sizeof(lastmessage),"%s",text);errors++; }
static void ViewportCancelTransform(HWND hwnd) { cancels++; }
static void UVEditorCancelInteraction(HWND hwnd) { cancels++; }
static HCURSOR SetCursor(HCURSOR c) { return c; }
static HCURSOR LoadCursor(void *instance,int name) { return NULL; }
BOOL LevelExportBackground(const char *path,const char *project,const BgDocument *doc,const char **why)
{ assert(doc==&g_CurrentBgDocument&&!strcmp(path,"bg_dest_all_p.glb"));backgrounds++;*why="failed";return !fail; }
BOOL LevelExportStans(const char *path,const StanFile *stan,const char **why)
{ assert(stan==&g_CurrentStan&&!strcmp(path,"Tbg_dest_all_p_stanZ.glb"));stans++;*why="failed";return !fail; }
#include "ui.inc"
int main(void)
{
    BgDocumentRoom room={0};StanTile tile={0};
    g_CurrentLevelIndex=(DWORD)-1;
    GEditorExportLevel(NULL,FALSE);GEditorExportLevel(NULL,TRUE);assert(!dialogs);
    g_CurrentLevelIndex=0;g_Project.levelcount=1;
    assert(!GEditorCanExportLevel(FALSE)&&!GEditorCanExportLevel(TRUE));
    g_CurrentBgDocument.rooms=&room;g_CurrentBgDocument.facecount=1;
    g_CurrentStan.tiles=&tile;g_CurrentStan.tilecount=1;
    strcpy(g_CurrentBg.name,"bg/bg_dest_all_p.seg");strcpy(g_CurrentStan.name,"Tbg_dest_all_p_stanZ");
    GEditorExportLevel(NULL,FALSE);assert(dialogs==1&&!backgrounds&&!cancels&&!errors);
    accepted=TRUE;GEditorExportLevel(NULL,FALSE);GEditorExportLevel(NULL,TRUE);
    assert(backgrounds==1&&stans==1&&cancels==4&&!errors);
    fail=TRUE;GEditorExportLevel(NULL,TRUE);assert(errors==1&&stans==2);
    accepted=FALSE;dialogerror=0x3002;GEditorExportLevel(NULL,FALSE);
    assert(errors==2&&backgrounds==1&&strstr(lastmessage,"0x3002"));
    dialogerror=0;
    const char *assets[]={"bg/bg_dest_all_p.seg","bg\\bg_dest_all_p.seg","C:\\project/bg/bg_dest_all_p.seg",
        "bg_dest_all_p.seg","bg_dest_all_p","bg/custom.map.seg","", "bg/"};
    const char *names[]={"bg_dest_all_p.glb","bg_dest_all_p.glb","bg_dest_all_p.glb",
        "bg_dest_all_p.glb","bg_dest_all_p.glb","custom.map.glb","background.glb","background.glb"};
    for(unsigned int i=0;i<sizeof(assets)/sizeof(*assets);i++) {
        strcpy(g_CurrentBg.name,assets[i]);expectedname=names[i];GEditorExportLevel(NULL,FALSE);
    }
    strcpy(g_CurrentStan.name,"stan\\Tbg_dest_all_p_stanZ.stan");
    expectedname="Tbg_dest_all_p_stanZ.glb";GEditorExportLevel(NULL,TRUE);
    g_CurrentStan.name[0]=0;expectedname="stans.glb";GEditorExportLevel(NULL,TRUE);
    assert(backgrounds==1&&stans==2&&errors==2);
    puts("PASS: native BG paths, extension replacement, mixed separators, fallback names, no-level guards, cancellation and dialog error codes.");
    return 0;
}
