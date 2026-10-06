/* Modeless fitting tool: keep the assembled character visible in the level.
 * Edits belong to the head asset, not one setup instance or one body pairing. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "headoffset.h"
#include "modeledits.h"
#include "modelcompile.h"
#include "modeleditor.h"
#include "resource.h"

typedef struct HeadOffsetStep {
    ModelUVChange model;
    int before[3],after[3];
} HeadOffsetStep;
static HWND g_Window;
static struct {
    char project[MAX_PATH],name[64];
    DWORD revision;
    int offset[3],count,position;
    HeadOffsetStep steps[32];
} g_Head;

static void HeadOffsetFields(void)
{
    char text[32];
    for(int i=0;i<3;i++) {
        snprintf(text,sizeof(text),"%d",g_Head.offset[i]);
        SetDlgItemText(g_Window,IDC_HEAD_OFFSET_X+i,text);
    }
    EnableWindow(GetDlgItem(g_Window,IDC_HEAD_OFFSET_UNDO),g_Head.position>0);
    EnableWindow(GetDlgItem(g_Window,IDC_HEAD_OFFSET_REDO),g_Head.position<g_Head.count);
}
static BOOL HeadOffsetRead(int value[3])
{
    for(int i=0;i<3;i++) {
        char text[64],*end;GetDlgItemText(g_Window,IDC_HEAD_OFFSET_X+i,text,sizeof(text));
        const char *start=text;while(isspace((unsigned char)*start)) start++;
        errno=0;long n=strtol(start,&end,10);
        if(start==end) return FALSE;
        while(isspace((unsigned char)*end)) end++;
        if(errno || *end || n< -32768 || n>32767) return FALSE;
        value[i]=(int)n;
    }
    return TRUE;
}
static void HeadOffsetNotify(void)
{
    HeadOffsetFields();
    ModelEditorRefreshImages();
    SendMessage(GetWindow(g_Window,GW_OWNER),MODELEDITOR_CHANGED,0,(LPARAM)g_Head.name);
    SetDlgItemText(g_Window,IDC_HEAD_OFFSET_STATUS,"Head updated. Save Project to keep this adjustment.");
}
static void HeadOffsetApply(void)
{
    int value[3],delta[3];const char *why="";HeadOffsetStep step={0};
    if(!HeadOffsetRead(value)) {
        MessageBox(g_Window,"Enter whole native model units from -32768 to 32767.","Head Offset",MB_ICONERROR);return;
    }
    for(int i=0;i<3;i++) delta[i]=value[i]-g_Head.offset[i];
    if(!ModelEditsOffsetHead(g_Head.project,g_Head.name,g_Head.revision,delta,&step.model,&why)) {
        MessageBox(g_Window,why,"Head Offset",MB_ICONERROR);return;
    }
    if(!step.model.before) return;
    memcpy(step.before,g_Head.offset,sizeof(step.before));memcpy(step.after,value,sizeof(step.after));
    while(g_Head.count>g_Head.position) ModelEditsFreeUVChange(&g_Head.steps[--g_Head.count].model);
    if(g_Head.count==32) {
        ModelEditsFreeUVChange(&g_Head.steps[0].model);
        memmove(g_Head.steps,g_Head.steps+1,31*sizeof(*g_Head.steps));g_Head.count--;g_Head.position--;
    }
    g_Head.steps[g_Head.count++]=step;g_Head.position=g_Head.count;
    g_Head.revision=step.model.afterRevision;memcpy(g_Head.offset,value,sizeof(value));
    HeadOffsetNotify();
}
static void HeadOffsetUndo(BOOL redo)
{
    int index=redo ? g_Head.position : g_Head.position-1;const char *why="";
    if(index<0 || index>=g_Head.count) return;
    HeadOffsetStep *step=&g_Head.steps[index];
    if(!ModelEditsRestoreUVs(g_Head.project,g_Head.name,&step->model,redo,&why)) {
        MessageBox(g_Window,why,"Head Offset",MB_ICONERROR);return;
    }
    g_Head.position+=redo ? 1 : -1;
    g_Head.revision=redo ? step->model.afterRevision : step->model.beforeRevision;
    memcpy(g_Head.offset,redo ? step->after : step->before,sizeof(g_Head.offset));
    HeadOffsetNotify();
}
static INT_PTR CALLBACK HeadOffsetDialog(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_INITDIALOG) {
        g_Window=hwnd;
        char title[100];snprintf(title,sizeof(title),"Head Offset - %s",g_Head.name);SetWindowText(hwnd,title);
        for(int i=0;i<3;i++) {
            SendDlgItemMessage(hwnd,IDC_HEAD_OFFSET_X+i,EM_SETLIMITTEXT,16,0);
            SendDlgItemMessage(hwnd,IDC_HEAD_OFFSET_SPIN_X+i,UDM_SETBUDDY,(WPARAM)GetDlgItem(hwnd,IDC_HEAD_OFFSET_X+i),0);
            SendDlgItemMessage(hwnd,IDC_HEAD_OFFSET_SPIN_X+i,UDM_SETRANGE32,(WPARAM)-32768,32767);
        }
        HeadOffsetFields();return TRUE;
    }
    if(message==WM_COMMAND) {
        switch(LOWORD(wp)) {
        case IDC_HEAD_OFFSET_APPLY: HeadOffsetApply();return TRUE;
        case IDC_HEAD_OFFSET_RESET:
            for(int i=0;i<3;i++) SetDlgItemText(hwnd,IDC_HEAD_OFFSET_X+i,"0");
            HeadOffsetApply();return TRUE;
        case IDC_HEAD_OFFSET_UNDO: HeadOffsetUndo(FALSE);return TRUE;
        case IDC_HEAD_OFFSET_REDO: HeadOffsetUndo(TRUE);return TRUE;
        case IDCANCEL: DestroyWindow(hwnd);return TRUE;
        }
    }
    if(message==WM_CLOSE) {DestroyWindow(hwnd);return TRUE;}
    if(message==WM_NCDESTROY) {
        for(int i=0;i<g_Head.count;i++) ModelEditsFreeUVChange(&g_Head.steps[i].model);
        memset(&g_Head,0,sizeof(g_Head));g_Window=NULL;
    }
    return FALSE;
}
BOOL HeadOffsetShow(HWND owner,const char *project,const char *name,const char **why)
{
    if(g_Window && !strcmp(project,g_Head.project) && !strcmp(name,g_Head.name)) {
        ShowWindow(g_Window,SW_RESTORE);SetForegroundWindow(g_Window);return TRUE;
    }
    unsigned char *data=NULL,*unused=NULL;DWORD size;ModelSource source={0};const int zero[3]={0};
    if(!ModelEditsCopyNative(project,name,&data,&size,why)) return FALSE;
    BOOL ok=ModelReadSource(data,size,&source,why)
        && ModelCompileOffsetHead(data,size,&source,zero,&unused,why);
    DWORD revision=ModelDataHash(data,size);free(data);free(unused);ModelFreeSource(&source);
    if(!ok) return FALSE;
    HeadOffsetClose();
    lstrcpyn(g_Head.project,project,sizeof(g_Head.project));lstrcpyn(g_Head.name,name,sizeof(g_Head.name));g_Head.revision=revision;
    g_Window=CreateDialog(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_HEAD_OFFSET),owner,HeadOffsetDialog);
    if(!g_Window) {*why="Could not open Head Offset.";return FALSE;}
    ShowWindow(g_Window,SW_SHOW);SetForegroundWindow(g_Window);*why="";return TRUE;
}
void HeadOffsetClose(void) {if(g_Window) DestroyWindow(g_Window);}
BOOL HeadOffsetHandleMessage(MSG *message)
{
    if(!g_Window || !message || (message->hwnd!=g_Window && !IsChild(g_Window,message->hwnd))) return FALSE;
    if(message->message==WM_KEYDOWN && (GetKeyState(VK_CONTROL)&0x8000)
        && (message->wParam=='Z' || message->wParam=='Y')) {
        char name[32];GetClassName(message->hwnd,name,sizeof(name));
        if(lstrcmpi(name,"EDIT")) {
            HeadOffsetUndo(message->wParam=='Y' || (GetKeyState(VK_SHIFT)&0x8000));return TRUE;
        }
    }
    return IsDialogMessage(g_Window,message);
}
