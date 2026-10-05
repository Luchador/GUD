#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelanimation.h"
#include "bgdocument.h"
#include "resource.h"
typedef void *HWND;
typedef intptr_t LRESULT,LPARAM;
typedef BgVertex Vertex;
typedef struct ViewportState {
    BOOL orbit;Vertex *scene;BgFaceRef *scenefacerefs;int scenecount;
} ViewportState;
enum { GWLP_USERDATA, CB_RESETCONTENT, CB_ADDSTRING, CB_SETCURSEL, CB_GETCURSEL, CBN_SELCHANGE };
#define LOWORD(x) ((x)&65535)
#define HIWORD(x) (((x)>>16)&65535)
#define MODEL_ANIMATION_TIMER 1
static HWND g_ModelEditor=(HWND)1,g_ModelViewport=(HWND)2;
static ModelSource g_ModelSource;
static ModelAnimationPreview g_ModelAnimation;
static int g_ModelAnimationClip=-1;
static double g_ModelAnimationFrame;
static BOOL g_ModelAnimationPlaying;
static DWORD g_ModelAnimationTick,now;
static char g_ModelProject[]="project";
static BOOL enabled[2048],timeractive,minimized,failpose,failtimer;
static int row,rows,invalidates;
static ViewportState viewport;
static HWND GetDlgItem(HWND hwnd,int id) { return (HWND)(uintptr_t)id; }
static void EnableWindow(HWND hwnd,BOOL value) { enabled[(uintptr_t)hwnd]=!!value; }
static void KillTimer(HWND hwnd,int id) { assert(id==MODEL_ANIMATION_TIMER);timeractive=FALSE; }
static unsigned SetTimer(HWND hwnd,int id,unsigned interval,void *callback)
{ assert(id==MODEL_ANIMATION_TIMER && interval==16);timeractive=!failtimer;return timeractive; }
static DWORD GetTickCount(void) { return now; }
static BOOL IsIconic(HWND hwnd) { return minimized; }
static void SetDlgItemText(HWND hwnd,int id,const char *text) { assert(id==IDC_MODEL_STATUS && *text); }
static LRESULT SendDlgItemMessage(HWND hwnd,int id,unsigned msg,uintptr_t wparam,LPARAM lparam)
{
    assert(id==IDC_MODEL_ANIMATION);
    if(msg==CB_RESETCONTENT) { row=-1;rows=0;return 0; }
    if(msg==CB_ADDSTRING) { assert(*(const char *)lparam);return rows++; }
    if(msg==CB_SETCURSEL) { assert((int)wparam<rows);row=(int)wparam;return row; }
    assert(msg==CB_GETCURSEL);return row;
}
static intptr_t GetWindowLongPtr(HWND hwnd,int field) { return (intptr_t)&viewport; }
static void ViewportUpdateGizmo(ViewportState *state) { assert(state==&viewport); }
static void InvalidateRect(HWND hwnd,const void *rect,BOOL erase) { invalidates++; }
void ModelAnimationClose(ModelAnimationPreview *p) { free(p->clips);memset(p,0,sizeof(*p)); }
BOOL ModelAnimationOpen(ModelAnimationPreview *p,const char *project,const char *name,const char **why)
{
    if(!strcmp(name,"static")) return TRUE;
    p->clips=calloc(2,sizeof(*p->clips));assert(p->clips);p->count=2;
    p->clips[0]=(ModelAnimationClip){.name="idle",.frames=10};
    p->clips[1]=(ModelAnimationClip){.name="walk",.frames=20};return TRUE;
}
BgVertex *ModelAnimationPose(const ModelAnimationPreview *p,DWORD clip,double frame,DWORD count,const char **why)
{
    if(failpose) return NULL;
    assert(clip<p->count && count==2);BgVertex *v=calloc(6,sizeof(*v));assert(v);
    for(int i=0;i<6;i++) { v[i].x=(float)frame+i;v[i].environment.normal[2]=1; }
    return v;
}
#include "logic.inc"
int main(void)
{
    BgVertex scene[6]={0},source[6]={0};BgFaceRef refs[2]={{.room=1,.faceid=2},{.room=1,.faceid=1}};
    for(int i=0;i<6;i++) { scene[i].s=0.25f;scene[i].r=123;source[i].x=100+i; }
    viewport=(ViewportState){TRUE,scene,refs,6};g_ModelSource.count=2;g_ModelSource.vertices=source;
    ModelEditorAnimationLoad("guard");assert(rows==3 && row==0 && enabled[IDC_MODEL_ANIMATION] && !enabled[IDC_MODEL_ANIMATION_PLAY]);
    row=1;assert(Command(g_ModelEditor,IDC_MODEL_ANIMATION | (CBN_SELCHANGE<<16)));
    assert(g_ModelAnimationClip==0 && !g_ModelAnimationPlaying && enabled[IDC_MODEL_ANIMATION_PLAY]);
    assert(scene[0].x==3 && scene[3].x==0); /* Sorted draw order uses stable source IDs. */
    assert(scene[0].s==0.25f && scene[0].r==123 && source[0].x==100);
    now=100;Command(g_ModelEditor,IDC_MODEL_ANIMATION_PLAY);assert(timeractive && g_ModelAnimationPlaying);
    now=200;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);assert(fabs(g_ModelAnimationFrame-3)<1e-9);
    Command(g_ModelEditor,IDC_MODEL_ANIMATION_STOP);assert(!timeractive && !g_ModelAnimationPlaying && g_ModelAnimationFrame==3);
    now=700;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);assert(g_ModelAnimationFrame==3);
    Command(g_ModelEditor,IDC_MODEL_ANIMATION_PLAY);now=750;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);
    assert(g_ModelAnimationFrame==4.5);
    Command(g_ModelEditor,IDC_MODEL_ANIMATION_RESET);assert(!timeractive && g_ModelAnimationFrame==0 && scene[3].x==0);
    failtimer=TRUE;Command(g_ModelEditor,IDC_MODEL_ANIMATION_PLAY);assert(!timeractive && !g_ModelAnimationPlaying);failtimer=FALSE;
    now=0xfffffff0u;Command(g_ModelEditor,IDC_MODEL_ANIMATION_PLAY);
    now=84;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);assert(g_ModelAnimationFrame==3);
    minimized=TRUE;now=1084;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);assert(g_ModelAnimationFrame==3);
    minimized=FALSE;now=1184;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);assert(g_ModelAnimationFrame==6);
    now=1384;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);assert(g_ModelAnimationFrame==2); /* loop */
    ModelEditorAnimationLoad("guard");assert(row==1 && g_ModelAnimationFrame==2 && !timeractive); /* edited model/LOD reload */
    row=2;Command(g_ModelEditor,IDC_MODEL_ANIMATION | (CBN_SELCHANGE<<16));assert(g_ModelAnimationClip==1 && g_ModelAnimationFrame==0);
    Command(g_ModelEditor,IDC_MODEL_ANIMATION_PLAY);failpose=TRUE;now+=100;Tick(g_ModelEditor,MODEL_ANIMATION_TIMER);
    assert(!timeractive && !g_ModelAnimationPlaying);failpose=FALSE;
    row=0;Command(g_ModelEditor,IDC_MODEL_ANIMATION | (CBN_SELCHANGE<<16));assert(scene[0].x==103 && scene[3].x==100);
    Vertex before[6];memcpy(before,scene,sizeof(scene));refs[1].faceid=99;
    assert(!ViewportSetModelPose(g_ModelViewport,source,2) && !memcmp(before,scene,sizeof(scene)));refs[1].faceid=1;
    ModelEditorAnimationLoad("static");assert(rows==1 && !enabled[IDC_MODEL_ANIMATION] && !enabled[IDC_MODEL_ANIMATION_RESET]);
    ModelEditorAnimationClear();assert(!timeractive && !g_ModelAnimation.clips && invalidates>0);
    puts("PASS playback: selection, play/resume, stop, reset, looping, clock wrap, minimization, reload, failures, bind pose and atomic viewport updates preserving source/UV/color.");
}
