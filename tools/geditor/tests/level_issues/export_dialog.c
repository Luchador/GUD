/* The export completion path may refresh existing reports, but must never open one. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef void *HWND;
typedef intptr_t INT_PTR,LPARAM;
typedef uintptr_t WPARAM;
typedef struct { int unused; } Project;
typedef struct { Project *project; } CreateRomInfo;
static Project g_Project;
enum { IDOK=1, IDCANCEL=2, IDD_CREATE_ROM=100, MB_ICONERROR=16 };
#define GEDITOR_TITLE "GEditor"
#define MAKEINTRESOURCE(id) ((void *)(intptr_t)(id))
static INT_PTR outcome;
static int refreshed,errors;
static INT_PTR GEditorCreateRomProc(HWND hwnd,unsigned msg,WPARAM wp,LPARAM lp) { return 0; }
static void *GetModuleHandle(void *name) { return (void *)7; }
static INT_PTR DialogBoxParam(void *module,void *resource,HWND owner,
    INT_PTR (*proc)(HWND,unsigned,WPARAM,LPARAM),LPARAM lp)
{
    assert(module==(void *)7 && resource==MAKEINTRESOURCE(IDD_CREATE_ROM) && owner==(HWND)1);
    assert(proc==GEditorCreateRomProc && ((CreateRomInfo *)lp)->project==&g_Project);return outcome;
}
static void MessageBox(HWND owner,const char *message,const char *title,unsigned flags)
{ assert(owner==(HWND)1 && message[0] && title[0] && flags==MB_ICONERROR);errors++; }
static void IssuesWindowRefreshExport(void) { refreshed++; }
#include "export_dialog.inc"
int main(void)
{
    outcome=IDOK;GEditorPromptForRomExport((HWND)1);assert(refreshed==1 && !errors);
    outcome=IDCANCEL;GEditorPromptForRomExport((HWND)1);assert(refreshed==1 && !errors);
    outcome=-1;GEditorPromptForRomExport((HWND)1);assert(refreshed==1 && errors==1);
    puts("PASS: export completion refreshes existing reports without opening a window; cancellation/errors do not refresh.");
    return 0;
}
