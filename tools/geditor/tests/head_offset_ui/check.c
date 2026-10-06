#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM,LRESULT;
#define GW_OWNER 4
#define MB_ICONERROR 1
#define MODELEDITOR_CHANGED 5000
#include "modeledits.h"
#include "resource.h"
#include "types.inc"
static char fields[3][64];
static int native[3],errors,notifications;
static DWORD revision=1;
static void SetDlgItemText(HWND h,int id,const char *text)
{ if(id>=IDC_HEAD_OFFSET_X && id<=IDC_HEAD_OFFSET_Z) snprintf(fields[id-IDC_HEAD_OFFSET_X],64,"%s",text); }
static void GetDlgItemText(HWND h,int id,char *text,int size)
{ snprintf(text,size,"%s",fields[id-IDC_HEAD_OFFSET_X]); }
static HWND GetDlgItem(HWND h,int id) {return (HWND)(uintptr_t)id;}
static HWND GetWindow(HWND h,int id) {return (HWND)2;}
static void EnableWindow(HWND h,BOOL enable) {}
static void ModelEditorRefreshImages(void) {}
static LRESULT SendMessage(HWND h,int msg,WPARAM wp,LPARAM lp)
{assert(msg==MODELEDITOR_CHANGED && !strcmp((const char *)lp,"CheadconneryZ"));notifications++;return 0;}
static void MessageBox(HWND h,const char *text,const char *title,int flags) {errors++;}
void ModelEditsFreeUVChange(ModelUVChange *c) {free(c->before);free(c->after);memset(c,0,sizeof(*c));}
BOOL ModelEditsOffsetHead(const char *p,const char *name,DWORD rev,const int delta[3],ModelUVChange *c,const char **why)
{
    if(rev!=revision) return FALSE;
    if(!delta[0]&&!delta[1]&&!delta[2]) return TRUE;
    c->before=malloc(sizeof(native));c->after=malloc(sizeof(native));assert(c->before && c->after);
    memcpy(c->before,native,sizeof(native));c->beforeRevision=revision;
    for(int i=0;i<3;i++) native[i]+=delta[i];
    memcpy(c->after,native,sizeof(native));c->afterRevision=++revision;return TRUE;
}
BOOL ModelEditsRestoreUVs(const char *p,const char *name,const ModelUVChange *c,BOOL redo,const char **why)
{
    if(revision!=(redo ? c->beforeRevision : c->afterRevision)) return FALSE;
    memcpy(native,redo ? c->after : c->before,sizeof(native));revision=redo ? c->afterRevision : c->beforeRevision;return TRUE;
}
#include "logic.inc"
static void Values(const char *x,const char *y,const char *z)
{snprintf(fields[0],64,"%s",x);snprintf(fields[1],64,"%s",y);snprintf(fields[2],64,"%s",z);}
int main(void)
{
    g_Window=(HWND)1;g_Head.revision=revision;strcpy(g_Head.name,"CheadconneryZ");
    Values("3","-9","-17");HeadOffsetApply();
    assert(!errors && notifications==1 && g_Head.count==1 && native[0]==3 && native[1]==-9 && native[2]==-17);
    HeadOffsetApply();assert(notifications==1 && g_Head.count==1 && native[2]==-17);
    Values("4","-8","-16");HeadOffsetApply();assert(native[0]==4 && native[1]==-8 && native[2]==-16);
    HeadOffsetUndo(FALSE);assert(native[0]==3 && !strcmp(fields[2],"-17"));
    HeadOffsetUndo(TRUE);assert(native[0]==4 && !strcmp(fields[2],"-16"));
    Values("0","0","0");HeadOffsetApply();assert(!native[0] && !native[1] && !native[2]);
    HeadOffsetUndo(FALSE);assert(native[0]==4);Values("5","0","0");HeadOffsetApply();
    assert(g_Head.count==3 && g_Head.position==3 && native[0]==5);
    const char *bad[]={""," ","1.5","nan","32768","-32769","4units","9999999999999999999999"};
    int previous=notifications;
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) {Values(bad[i],"0","0");HeadOffsetApply();}
    assert(errors==8 && notifications==previous && native[0]==5);
    for(int i=6;i<50;i++) {char n[32];snprintf(n,sizeof(n),"%d",i);Values(n,"0","0");HeadOffsetApply();}
    assert(g_Head.count==32 && g_Head.position==32 && native[0]==49);
    revision++;Values("50","0","0");HeadOffsetApply();assert(native[0]==49 && errors==9);
    HeadOffsetUndo(FALSE);assert(native[0]==49 && errors==10);
    for(int i=0;i<g_Head.count;i++) ModelEditsFreeUVChange(&g_Head.steps[i].model);
    puts("PASS head offset controls: cumulative XYZ, repeated Apply, Reset, undo/redo, history branching/limit, invalid input and stale-model rejection.");
    return 0;
}
