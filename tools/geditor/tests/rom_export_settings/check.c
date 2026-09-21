#include <assert.h>
#include <stdlib.h>
#include <wchar.h>
#include "romexportsettings.c"
#include "path_export.inc"

static struct { BYTE data[512]; DWORD size,type; BOOL present; } values[2];
static BOOL keyExists, denyWrites;
static unsigned writes;
static int Value(const char *name) { return !strcmp(name,"Directory"); }
LONG RegOpenKeyExA(HKEY root,const char *path,DWORD reserved,DWORD access,HKEY *key)
{ assert(root==HKEY_CURRENT_USER && !strcmp(path,ROM_EXPORT_SETTINGS_KEY));*key=(HKEY)2;return keyExists ? 0 : 2; }
LONG RegCreateKeyExA(HKEY root,const char *path,DWORD reserved,char *className,DWORD options,DWORD access,void *security,HKEY *key,DWORD *disposition)
{ if (denyWrites) return ERROR_ACCESS_DENIED;keyExists=TRUE;*key=(HKEY)2;return 0; }
LONG RegQueryValueExA(HKEY key,const char *name,DWORD *reserved,DWORD *type,BYTE *data,DWORD *size)
{
    int i=Value(name);if (!values[i].present) return 2;
    DWORD cap=*size;*size=values[i].size;*type=values[i].type;
    if (cap<*size) return ERROR_MORE_DATA;
    memcpy(data,values[i].data,*size);return 0;
}
LONG RegSetValueExA(HKEY key,const char *name,DWORD reserved,DWORD type,const BYTE *data,DWORD size)
{ int i=Value(name);assert(size<=sizeof(values[i].data));memcpy(values[i].data,data,size);values[i].size=size;values[i].type=type;values[i].present=TRUE;writes++;return 0; }
LONG RegCloseKey(HKEY key) { return 0; }
char *lstrcpyn(char *out,const char *in,int size)
{ if (size>0) { snprintf(out,(size_t)size,"%s",in); } return out; }
DWORD GetFullPathName(const char *path,DWORD capacity,char *out,char **part)
{
    char full[1024];
    if ((strlen(path)>1 && path[1]==':') || EditorPathSeparator(path[0])) snprintf(full,sizeof(full),"%s",path);
    else snprintf(full,sizeof(full),"C:\\cwd\\%s",path);
    EditorPathNormalize(full);DWORD size=strlen(full);
    if (size>=capacity) return size+1;
    memcpy(out,full,size+1);return size;
}
static BOOL existingRom;
DWORD GetFileAttributes(const char *path)
{
    if (strstr(path,"missing")) return INVALID_FILE_ATTRIBUTES;
    if (RomExportEndsWithNoCase(path,".z64")) return existingRom ? FILE_ATTRIBUTE_NORMAL : INVALID_FILE_ATTRIBUTES;
    return FILE_ATTRIBUTE_DIRECTORY;
}

typedef void *HWND;typedef void *HDC;
typedef intptr_t INT_PTR,LONG_PTR,LPARAM;typedef uintptr_t WPARAM;typedef unsigned UINT;
#define CALLBACK
#define GEDITOR_TITLE "GEditor"
#define RGB(r,g,b) ((r)|((g)<<8)|((b)<<16))
#define LOWORD(v) ((v)&0xffff)
#define HIWORD(v) (((v)>>16)&0xffff)
enum { WM_INITDIALOG=1,WM_COMMAND,WM_CTLCOLORSTATIC,DWLP_USER,EM_LIMITTEXT,EN_CHANGE,
       TRANSPARENT,COLOR_3DFACE,IDOK=100,IDCANCEL,IDYES,IDNO,MB_ICONERROR=16,MB_YESNO=32,MB_ICONWARNING=64,MB_DEFBUTTON2=128,MB_ICONINFORMATION=256 };
static GEditorProject g_Project;
typedef struct CreateRomInfo { const GEditorProject *project; } CreateRomInfo;
static char fieldName[ROM_EXPORT_NAME_MAX],fieldDirectory[MAX_PATH],warning[512],browse[MAX_PATH];
static LONG_PTR dialogInfo;
static BOOL saveOk=TRUE,exportOk=TRUE,buttonEnabled;
static int ended,overwrite=IDYES;
static unsigned exports;
static LONG_PTR SetWindowLongPtr(HWND h,int index,LONG_PTR value) { dialogInfo=value;return 0; }
static LONG_PTR GetWindowLongPtr(HWND h,int index) { return dialogInfo; }
static void SendDlgItemMessage(HWND h,int id,int msg,WPARAM wp,LPARAM lp) {}
static void SetDlgItemText(HWND h,int id,const char *text)
{ if (id==IDC_ROM_NAME) lstrcpyn(fieldName,text,sizeof(fieldName));else if(id==IDC_ROM_OUTPUT_DIR) lstrcpyn(fieldDirectory,text,sizeof(fieldDirectory));else lstrcpyn(warning,text,sizeof(warning)); }
static void GetDlgItemText(HWND h,int id,char *out,int max)
{ lstrcpyn(out,id==IDC_ROM_NAME ? fieldName : fieldDirectory,max); }
static HWND GetDlgItem(HWND h,int id) { return (HWND)(intptr_t)id; }
static void EnableWindow(HWND h,BOOL enabled) { buttonEnabled=enabled; }
static int GetDlgCtrlID(HWND h) { return (int)(intptr_t)h; }
static void SetTextColor(HDC h,unsigned color) {}
static void SetBkMode(HDC h,int mode) {}
static void *GetSysColorBrush(int color) { return NULL; }
static BOOL GEditorPromptForFolder(HWND h,const wchar_t *title,char *out,int max)
{ lstrcpyn(out,browse,max);return browse[0]!=0; }
static int MessageBox(HWND h,const char *message,const char *title,unsigned flags) { return flags&MB_YESNO ? overwrite : IDOK; }
static void EndDialog(HWND h,int result) { ended=result; }
static BOOL GEditorSaveProject(HWND h) { return saveOk; }
BOOL RomExportCreate(const GEditorProject *project,const char *name,const char *directory,char *out,size_t max,const char **why)
{ exports++;*why="test export failure";return exportOk && RomExportBuildOutputPath(directory,name,out,max,why); }
const char *RomExportCleanupWarning(void) { return ""; }
#include "dialog.inc"

static void Open(CreateRomInfo *info)
{ ended=0;assert(GEditorCreateRomProc(NULL,WM_INITDIALOG,0,(LPARAM)info)); }
static void Create(void) { assert(GEditorCreateRomProc(NULL,WM_COMMAND,IDC_CREATE_ROM,0)); }
int main(void)
{
    const char *why;char output[MAX_PATH],small[8],large[300];
    const struct { const char *input,*expected; } paths[]={
        {"C:\\GUD\\\\GUD_rebased27","C:\\GUD\\GUD_rebased27\\GUD.z64"},
        {"C:\\GUD\\","C:\\GUD\\GUD.z64"},
        {"C:\\","C:\\GUD.z64"},
        {"C:/GUD//","C:\\GUD\\GUD.z64"},
        {"\\\\server\\share\\\\roms\\","\\\\server\\share\\roms\\GUD.z64"},
        {"\\\\?\\C:\\roms\\","\\\\?\\C:\\roms\\GUD.z64"},
        {"\\\\?\\UNC\\server\\share\\\\roms","\\\\?\\UNC\\server\\share\\roms\\GUD.z64"}
    };
    for (unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++)
    { assert(RomExportBuildOutputPath(paths[i].input,"GUD",output,sizeof(output),&why));assert(!strcmp(output,paths[i].expected)); }
    assert(RomExportBuildOutputPath("C:\\", "GUD.Z64",output,sizeof(output),&why) && !strcmp(output,"C:\\GUD.Z64"));
    assert(!RomExportBuildOutputPath("C:\\out","GUD",small,sizeof(small),&why) && !small[0]);
    memset(large,'a',sizeof(large)-1);large[sizeof(large)-1]=0;
    assert(!RomExportBuildOutputPath(large,"GUD",output,sizeof(output),&why));
    assert(!RomExportBuildOutputPath("C:\\out","..\\base",output,sizeof(output),&why));
    assert(EditorPathJoin(output,sizeof(output),"C:\\GUD\\","Rebased") && !strcmp(output,"C:\\GUD\\Rebased"));
    assert(EditorPathJoin(output,sizeof(output),"\\\\server\\share\\","Rebased") && !strcmp(output,"\\\\server\\share\\Rebased"));
    lstrcpyn(g_Project.name,"Project",sizeof(g_Project.name));lstrcpyn(g_Project.dir,"C:\\GUD\\\\Rebased",sizeof(g_Project.dir));
    CreateRomInfo info={&g_Project};Open(&info);
    assert(!strcmp(fieldName,"Project") && !strcmp(fieldDirectory,"C:\\GUD\\Rebased") && buttonEnabled);
    /* Cancel, validation errors, failed saves/exports and declined overwrites
     * must not replace the last successful values. */
    strcpy(fieldName,"Cancelled");GEditorCreateRomProc(NULL,WM_COMMAND,IDCANCEL,0);Open(&info);assert(!strcmp(fieldName,"Project") && !writes);
    strcpy(fieldName,"Invalid?");Create();assert(!writes && !exports && !ended);
    strcpy(fieldName,"Failed");saveOk=FALSE;Create();assert(!writes && !exports);saveOk=TRUE;
    exportOk=FALSE;Create();assert(!writes && exports==1);exportOk=TRUE;
    existingRom=TRUE;overwrite=IDNO;Create();assert(!writes && exports==1);existingRom=FALSE;
    strcpy(fieldName,"GUD");strcpy(browse,"D:\\ROMs\\\\");
    GEditorCreateRomProc(NULL,WM_COMMAND,IDC_BROWSE_ROM_OUTPUT,0);
    assert(!strcmp(fieldDirectory,"D:\\ROMs\\"));Create();assert(ended==IDOK && writes==2);
    strcpy(g_Project.name,"Other project");strcpy(g_Project.dir,"C:\\Other");Open(&info);
    assert(!strcmp(fieldName,"GUD") && !strcmp(fieldDirectory,"D:\\ROMs\\"));
    ZeroMemory(&g_RomExportSettings,sizeof(g_RomExportSettings));Open(&info); /* Restart. */
    assert(!strcmp(fieldName,"GUD") && !strcmp(fieldDirectory,"D:\\ROMs\\"));
    strcpy(fieldDirectory,"missing");Create();assert(!ended && writes==2);
    strcpy(fieldName,"Relative");strcpy(fieldDirectory,"exports\\\\");Create();
    ZeroMemory(&g_RomExportSettings,sizeof(g_RomExportSettings));Open(&info);
    assert(!strcmp(fieldDirectory,"C:\\cwd\\exports\\"));
    denyWrites=TRUE;strcpy(fieldName,"Session only");Create();Open(&info);assert(!strcmp(fieldName,"Session only"));denyWrites=FALSE;
    /* Invalid registry type, truncated strings, oversized and invalid names
     * fall back to the current project without accepting a shortened path. */
    values[0].type=4;values[1].data[values[1].size-1]='X';
    ZeroMemory(&g_RomExportSettings,sizeof(g_RomExportSettings));Open(&info);
    assert(!strcmp(fieldName,"Other project") && !strcmp(fieldDirectory,"C:\\Other"));
    values[0].type=REG_SZ;memcpy(values[0].data,"bad?",5);values[0].size=5;values[1].size=300;
    ZeroMemory(&g_RomExportSettings,sizeof(g_RomExportSettings));Open(&info);
    assert(!strcmp(fieldName,"Other project") && !strcmp(fieldDirectory,"C:\\Other"));
    assert(!RomExportDestinationIsValid(&g_Project,"C:\\Other\\\\","base",output,sizeof(output),&why));
    puts("PASS: drive/UNC/extended paths, separator joins and length limits; real Create ROM dialog defaults, browse, successful persistence/restart, cancellations/failures, invalid preferences, relative destinations and source-ROM protection.");
    return 0;
}
