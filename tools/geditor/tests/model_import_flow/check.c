#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resource.h"
typedef void *HWND, *HINSTANCE, *HCURSOR;
typedef intptr_t INT_PTR, LPARAM;
typedef uintptr_t WPARAM;
typedef unsigned int UINT, DWORD;
typedef int BOOL;
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define ZeroMemory(p,n) memset(p,0,n)
#define MAKEINTRESOURCE(n) ((const char *)(uintptr_t)(n))
#define HIWORD(n) ((unsigned)(n)>>16)
#define CB_ERR (-1)
#define LOWORD(n) ((unsigned)(n)&65535)
enum { WM_INITDIALOG=1, WM_COMMAND, WM_CLOSE, DWLP_USER, GW_OWNER, GWLP_HINSTANCE,
       IDC_WAIT, MB_ICONERROR, EM_SETLIMITTEXT, CB_ADDSTRING, CB_SETCURSEL, CB_GETCURSEL, CB_GETITEMDATA, CBN_SELCHANGE,
       OFN_EXPLORER=32, OFN_NOCHANGEDIR=64, OFN_PATHMUSTEXIST=128,
       OFN_FILEMUSTEXIST=256, OFN_OVERWRITEPROMPT=512, MODELEDITOR_CHANGED=1024,
       NEW_MODEL_CHARACTERS=0, NEW_MODEL_ITEMS=1, NEW_MODEL_PROPS=2 };
#define IDOK 1
#define IDCANCEL 2
#define BST_CHECKED 1
#define BST_UNCHECKED 0
typedef struct {
    DWORD lStructSize; HWND hwndOwner; char *lpstrFile; DWORD nMaxFile;
    const char *lpstrDefExt,*lpstrTitle,*lpstrFilter; DWORD Flags;
} OPENFILENAME;
typedef struct { char name[MAX_PATH];const char *folder; } ModelEditorEntry;
static HWND g_ModelEditor=(HWND)1;
static char g_ModelProject[MAX_PATH]="project";
static ModelEditorEntry g_ModelEntries[1];
static int g_ModelCount=1,g_ModelSelected=-1;
static int files,dialogs,imports,replacements,exports,changed,reloads,opens,errors;
static int cancelFile,cancelDialog,failImport,selectedCategory,dialogResult,rows;
static int failShow,shows,clears,kindChoice,selectedKind,characterImports,lastTemplate;
static int fitChecked,fitChoice=1,lastFit;
static int modes,replaceChoice,replaceChecked,cancelMode,failMode;
static void CheckDlgButton(HWND hwnd,int id,int checked)
{
    if (id==IDC_IMPORT_MODEL_NEW) { assert(checked==BST_CHECKED);replaceChecked=FALSE; }
    else { assert(id==IDC_NEW_HEAD_FIT);fitChecked=checked; }
}
static int IsDlgButtonChecked(HWND hwnd,int id)
{ if(id==IDC_IMPORT_MODEL_REPLACE) return replaceChecked;assert(id==IDC_NEW_HEAD_FIT);return fitChecked; }
static intptr_t dialogData;
static char nameText[64],addedName[64],openedName[64],lastTitle[80],order[32];
static const char *typedName;
static const char *filePath="C:\\models\\test mesh.glb";
static void Event(char e) { size_t n=strlen(order);assert(n<sizeof(order)-1);order[n]=e;order[n+1]=0; }
static void lstrcpyn(char *out,const char *in,int n) { snprintf(out,n,"%s",in); }
static intptr_t GetWindowLongPtr(HWND hwnd,int field) { return field==DWLP_USER ? dialogData : 1; }
static void SetWindowLongPtr(HWND hwnd,int field,intptr_t value) { assert(field==DWLP_USER);dialogData=value; }
static HWND GetWindow(HWND hwnd,int field) { assert(field==GW_OWNER);return (HWND)3; }
static HCURSOR LoadCursor(void *instance,int cursor) { return (HCURSOR)1; }
static HCURSOR SetCursor(HCURSOR cursor) { return (HCURSOR)1; }
static void MessageBox(HWND hwnd,const char *message,const char *title,int flags) { assert(*message);errors++; }
static void SetDlgItemText(HWND hwnd,int id,const char *text)
{
    if(id==IDC_NEW_MODEL_NAME) lstrcpyn(nameText,text,sizeof(nameText));
    else if(id==IDC_IMPORT_MODEL_TARGET) assert(!strcmp(text,g_ModelEntries[0].name));
    else assert(id==IDC_MODEL_STATUS);
}
static void GetDlgItemText(HWND hwnd,int id,char *text,int size)
{ assert(id==IDC_NEW_MODEL_NAME);lstrcpyn(text,nameText,size); }
static intptr_t SendDlgItemMessage(HWND hwnd,int id,UINT message,WPARAM wparam,LPARAM lparam)
{
    if(message==EM_SETLIMITTEXT) { assert(id==IDC_NEW_MODEL_NAME && wparam==63);return 0; }
    if(id==IDC_NEW_CHARACTER_KIND) {
        if(message==CB_SETCURSEL) selectedKind=wparam;
        return message==CB_GETCURSEL ? selectedKind : 0;
    }
    if(id==IDC_NEW_CHARACTER_TEMPLATE) return message==CB_GETCURSEL ? 0 : selectedKind ? 78 : 5;
    assert(id==IDC_NEW_MODEL_CATEGORY);
    if(message==CB_ADDSTRING)
    { const char *expected[]={"Characters","Items","Props"};assert(rows<3 && !strcmp((const char *)lparam,expected[rows]));return rows++; }
    if(message==CB_SETCURSEL) return selectedCategory=(int)wparam;
    assert(message==CB_GETCURSEL);return selectedCategory;
}
static void EndDialog(HWND hwnd,int result) { dialogResult=result; }
static BOOL GetOpenFileName(OPENFILENAME *ofn)
{
    files++;Event('F');assert(ofn->Flags&OFN_FILEMUSTEXIST);
    lstrcpyn(lastTitle,ofn->lpstrTitle,sizeof(lastTitle));
    if(cancelFile) return FALSE;
    lstrcpyn(ofn->lpstrFile,filePath,ofn->nMaxFile);return TRUE;
}
static BOOL GetSaveFileName(OPENFILENAME *ofn) { files++;assert(strstr(ofn->lpstrFile,"PexistingZ.gltf"));return TRUE; }
static int choice;
static INT_PTR DialogBoxParam(HINSTANCE instance,const char *resource,HWND owner,
    INT_PTR (*proc)(HWND,UINT,WPARAM,LPARAM),LPARAM data)
{
    if ((uintptr_t)resource==IDD_MODEL_IMPORT_MODE)
    {
        modes++;Event('M');dialogResult=0;
        if(failMode) return -1;
        replaceChecked=TRUE;
        proc((HWND)2,WM_INITDIALOG,0,data);assert(!replaceChecked);
        replaceChecked=replaceChoice;
        proc((HWND)2,cancelMode==2 ? WM_CLOSE : WM_COMMAND,cancelMode ? IDCANCEL : IDOK,0);
        return dialogResult;
    }
    dialogs++;Event('D');rows=dialogResult=0;
    assert((uintptr_t)resource==IDD_IMPORT_MODEL);
    proc((HWND)2,WM_INITDIALOG,0,data);assert(rows==3);
    selectedCategory=choice;selectedKind=kindChoice;fitChecked=fitChoice;
    if(typedName) lstrcpyn(nameText,typedName,sizeof(nameText));
    proc((HWND)2,cancelDialog==2 ? WM_CLOSE : WM_COMMAND,cancelDialog ? IDCANCEL : IDOK,0);
    return dialogResult; /* Invalid input stays open; no import may follow. */
}
static BOOL NewPropsImport(const char *project,const char *name,const char *path,BOOL replace,DWORD *triangles,const char **why)
{
    Event('N');imports++;assert(!replace && !strcmp(path,filePath));
    lstrcpyn(addedName,name,sizeof(addedName));*triangles=4;*why="Import failed";return !failImport;
}
static void ModelEditorCharacterTemplates(HWND hwnd) {}
static int NewPropsCharacterId(const char *name) { return 80; }
static BOOL NewPropsImportCharacter(const char *project,const char *name,const char *path,int templateid,BOOL fithead,DWORD *triangles,const char **why)
{ characterImports++;lastTemplate=templateid;lastFit=fithead;return NewPropsImport(project,name,path,FALSE,triangles,why); }
static BOOL ModelEditsImport(const char *project,const char *name,const char *path,DWORD *before,DWORD *after,const char **why)
{ replacements++;assert(!strcmp(name,"PexistingZ"));*before=3;*after=4;*why="Import failed";return !failImport; }
static BOOL ModelEditsExport(const char *project,const char *name,const char *path,const char **why)
{ exports++;assert(!strcmp(name,"PexistingZ"));return TRUE; }
static void ModelEditorLoad(int index,BOOL frame) { reloads++;assert(index==0 && frame); }
static void ModelEditorNotifyChanged(void) { changed++; }
static void SendMessage(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{ assert(message==MODELEDITOR_CHANGED && wparam==1);changed++;Event('C'); }
static void ModelEditorSetProject(const char *project) { assert(!strcmp(project,g_ModelProject));g_ModelSelected=-1;Event('R'); }
static BOOL ModelEditorOpenModel(HWND owner,HINSTANCE instance,const char *project,const char *name,const char **why)
{ opens++;g_ModelSelected=0;lstrcpyn(openedName,name,sizeof(openedName));Event('O');return TRUE; }
static BOOL ModelEditorShow(HWND owner,HINSTANCE instance,const char *project)
{ shows++;assert(!strcmp(project,g_ModelProject));Event('S');return !failShow; }
static void ModelEditorClearSelection(void)
{ clears++;g_ModelSelected=-1;Event('E'); }
static void ModelEditorImportNew(const char *path);
#include "logic.inc"
static void Reset(void)
{
    files=dialogs=imports=replacements=exports=changed=reloads=opens=errors=0;
    cancelFile=cancelDialog=failImport=0;choice=2;typedName=NULL;order[0]=0;
    failShow=shows=clears=kindChoice=characterImports=0;lastTemplate=-1;fitChoice=1;lastFit=-1;
    modes=replaceChoice=replaceChecked=cancelMode=failMode=0;
    strcpy(g_ModelProject,"project");strcpy(g_ModelEntries[0].name,"PexistingZ");g_ModelSelected=-1;
}
int main(void)
{
    for(int category=0;category<3;category++)
    {
        Reset();choice=category;ModelEditorTransfer(TRUE);
        char expected[64];snprintf(expected,sizeof(expected),"%ctest_meshZ","CGP"[category]);
        assert(!strcmp(addedName,expected) && !strcmp(openedName,expected));
        assert(characterImports==(category==0));
        assert(files==1 && dialogs==1 && imports==1 && !replacements && changed==1 && opens==1 && !errors);
        assert(!strcmp(order,"FDNCRO") && !strcmp(lastTitle,"Import New Model"));
    }
    Reset();choice=0;kindChoice=1;ModelEditorTransfer(TRUE);assert(characterImports==1 && lastTemplate==78 && lastFit);
    Reset();choice=0;kindChoice=1;fitChoice=0;ModelEditorTransfer(TRUE);assert(characterImports==1 && lastTemplate==78 && !lastFit);
    Reset();cancelFile=1;ModelEditorTransfer(TRUE);assert(files==1 && !dialogs && !imports && !changed);
    for(int cancel=1;cancel<=2;cancel++)
    { Reset();cancelDialog=cancel;ModelEditorTransfer(TRUE);assert(dialogs==1 && !imports && !changed && g_ModelSelected==-1); }
    Reset();typedName="../invalid";ModelEditorTransfer(TRUE);assert(errors==1 && !imports && !changed && !dialogResult);
    Reset();choice=-1;ModelEditorTransfer(TRUE);assert(errors==1 && !imports && !changed);
    Reset();failImport=1;ModelEditorTransfer(TRUE);assert(imports==1 && errors==1 && !changed && !opens && g_ModelSelected==-1);
    /* The toolbar defaults to adding, even with a head already selected. */
    for(int category=0;category<3;category++)
    {
        Reset();choice=category;g_ModelSelected=0;
        strcpy(g_ModelEntries[0].name,"CheadmooreZ");kindChoice=1;
        ModelEditorTransfer(TRUE);
        assert(modes==1 && files==1 && dialogs==1 && imports==1 && !replacements && opens==1 && !errors);
        assert(!strcmp(order,"MFDNCRO") && !strcmp(lastTitle,"Import New Model"));
        assert(characterImports==(category==0));
        if(category==0) assert(lastTemplate==78 && lastFit);
    }
    for(int cancel=1;cancel<=2;cancel++)
    {
        Reset();g_ModelSelected=0;cancelMode=cancel;ModelEditorTransfer(TRUE);
        assert(modes==1 && !files && !imports && !replacements && !changed && g_ModelSelected==0);
        Reset();g_ModelSelected=0;cancelDialog=cancel;ModelEditorTransfer(TRUE);
        assert(modes==1 && dialogs==1 && !imports && !replacements && !changed && g_ModelSelected==0);
    }
    Reset();g_ModelSelected=0;cancelFile=1;ModelEditorTransfer(TRUE);
    assert(modes==1 && files==1 && !dialogs && !imports && !replacements && !changed && g_ModelSelected==0);
    Reset();g_ModelSelected=0;failMode=1;ModelEditorTransfer(TRUE);
    assert(modes==1 && !files && !changed && g_ModelSelected==0);
    Reset();g_ModelSelected=0;failImport=1;ModelEditorTransfer(TRUE);
    assert(imports==1 && errors==1 && !replacements && !changed && !opens && g_ModelSelected==0);
    Reset();g_ModelSelected=0;replaceChoice=1;ModelEditorTransfer(TRUE);
    assert(files==1 && !dialogs && !imports && replacements==1 && reloads==1 && changed==1);
    assert(modes==1 && !strcmp(lastTitle,"Import replacement model"));
    Reset();g_ModelSelected=0;replaceChoice=1;failImport=1;ModelEditorTransfer(TRUE);
    assert(replacements==1 && !dialogs && !imports && !reloads && !changed && errors==1);
    Reset();g_ModelSelected=0;replaceChoice=1;cancelFile=1;ModelEditorTransfer(TRUE);
    assert(files==1 && !replacements && !changed && g_ModelSelected==0);
    Reset();g_ModelSelected=0;ModelEditorTransfer(FALSE);assert(exports==1 && !modes && !dialogs && !changed);
    Reset();ModelEditorTransfer(FALSE);assert(!files);
    Reset();g_ModelProject[0]=0;ModelEditorTransfer(TRUE);assert(!files);
    /* External import commands must never replace an already-selected model. */
    for(int category=0;category<3;category++)
    {
        Reset();choice=category;g_ModelSelected=0;
        assert(ModelEditorBeginNewImport((HWND)3,(HINSTANCE)1,g_ModelProject));
        assert(shows==1 && clears==1 && imports==1 && !modes && !replacements && !strcmp(order,"SEFDNCRO"));
    }
    Reset();g_ModelSelected=0;cancelFile=1;
    assert(ModelEditorBeginNewImport((HWND)3,(HINSTANCE)1,g_ModelProject));
    assert(shows==1 && clears==1 && files==1 && !dialogs && !imports && !replacements && g_ModelSelected==-1);
    Reset();cancelDialog=1;
    assert(ModelEditorBeginNewImport((HWND)3,(HINSTANCE)1,g_ModelProject));
    assert(dialogs==1 && !imports && !replacements && !errors);
    Reset();g_ModelSelected=0;failShow=1;
    assert(!ModelEditorBeginNewImport((HWND)3,(HINSTANCE)1,g_ModelProject));
    assert(shows==1 && !clears && !files && g_ModelSelected==0);
    Reset();
    assert(!ModelEditorBeginNewImport((HWND)3,(HINSTANCE)1,NULL));
    assert(!ModelEditorBeginNewImport((HWND)3,(HINSTANCE)1,""));
    assert(!shows && !clears && !files);
    puts("PASS import flow: new/import-replacement choice, raw head with existing head selected, Characters/Items/Props, cancellation/close, failures retain selection, export, no-project guard and external new-import commands.");
}
