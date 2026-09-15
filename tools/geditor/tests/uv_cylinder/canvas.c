#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
typedef void *HWND;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef struct { int x,y; } POINT;
#define WM_APP 0x8000
#define MB_ICONERROR 16
#define BST_CHECKED 1
#define BST_UNCHECKED 0
#define SW_HIDE 0
#define SW_SHOW 1
#define UVCANVAS_PI 3.14159265358979323846
#define lstrcpy strcpy
#include "uvcanvas.h"
#include "resource.h"
#include "types.inc"
static UVCanvasState state;
static HWND g_UVEditor=(HWND)1,g_UVCanvas=&state,focus;
static char fields[2][96];
static BOOL enabled[2];
static int messages,errors,transforms;
static BgDocumentUVEdit captured[64];
static BgDocumentFaceUVEdit mapped[64];
static DWORD capturedcount,mappedcount;
static BOOL destroyoncommit;
static UVCanvasState *UVCanvasGetState(HWND hwnd) { return hwnd; }
BOOL UVCanvasCancelInteraction(HWND canvas) { ((UVCanvasState *)canvas)->draghandle=0; return TRUE; }
static HWND GetParent(HWND hwnd) { return (HWND)1; }
static LRESULT SendMessage(HWND hwnd,unsigned message,int wparam,LPARAM lparam)
{
    messages++;
    if (message==UVCANVAS_WM_COMMIT)
    {
        const UVCanvasEdit *request=(const UVCanvasEdit *)lparam;
        assert(request->count<=64); capturedcount=request->count;
        memcpy(captured,request->vertices,capturedcount*sizeof(*captured));
        for (int n=0;n<state.nodecount;n++) for (DWORD i=0;i<capturedcount;i++)
        { if (state.nodes[n].source.vertexid==captured[i].vertexid) { state.nodes[n].source=captured[i]; } }
    }
    else
    {
        assert(message==UVCANVAS_WM_COMMIT_FACES);
        const UVCanvasFaceEdit *request=(const UVCanvasFaceEdit *)lparam;
        assert(request->count<=64 && !strcmp(request->action,"Cylindrical UV Mapping")); mappedcount=request->count;
        memcpy(mapped,request->faces,mappedcount*sizeof(*mapped));
    }
    if (destroyoncommit) { free(state.nodes); free(state.triangles); memset(&state,0,sizeof(state)); }
    return TRUE;
}
#include "canvas.inc"
static HWND GetFocus(void) { return focus; }
static HWND GetDlgItem(HWND hwnd,int id) { return (HWND)(intptr_t)id; }
static void SetFocus(HWND hwnd) { focus=hwnd; }
static void GetDlgItemText(HWND hwnd,int id,char *text,int size)
{ snprintf(text,size,"%s",fields[id==IDC_UV_V]); }
static void SetDlgItemText(HWND hwnd,int id,const char *text)
{ if (id==IDC_UV_U || id==IDC_UV_V) { snprintf(fields[id==IDC_UV_V],96,"%s",text); } }
static void EnableWindow(HWND hwnd,BOOL value)
{ if ((intptr_t)hwnd==IDC_UV_U || (intptr_t)hwnd==IDC_UV_V) { enabled[(intptr_t)hwnd==IDC_UV_V]=value; } }
static void ShowWindow(HWND hwnd,int mode) {}
static void CheckDlgButton(HWND hwnd,int id,int value) {}
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags) { assert(text[0]); errors++; }
BOOL UVCanvasApplyTransform(HWND canvas,const double values[2],const char **reason)
{ transforms++; return TRUE; }
#include "fields.inc"
static void Coordinates(void)
{
    state.nodecount=3; state.nodes=calloc(3,sizeof(*state.nodes)); assert(state.nodes);
    for (int i=0;i<3;i++)
    {
        state.nodes[i]=(UVCanvasNode){.width=32<<i,.height=16<<i,.selected=i<2,
            .source={.vertex={1,(DWORD)i},.vertexid=(DWORD)(10+i),.s=(i+1)*1024,.t=(i+2)*1024}};
    }
    state.nodes[1].source.s=6144;
    UVEditorUpdateFields();
    assert(enabled[0] && enabled[1] && strtod(fields[0],NULL)==2 && strtod(fields[1],NULL)==3.5);
    strcpy(fields[0],"-1.25"); strcpy(fields[1],"not a number"); focus=GetDlgItem(g_UVEditor,IDC_UV_U);
    UVEditorApplyFields();
    assert(messages==1 && !errors && capturedcount==2);
    assert(captured[0].s==-1280 && captured[1].s==-2560 && captured[0].t==2048 && captured[1].t==3072);
    assert(strtod(fields[0],NULL)==-1.25 && strtod(fields[1],NULL)==3.5);
    strcpy(fields[0],"also ignored"); strcpy(fields[1],"0.375"); focus=GetDlgItem(g_UVEditor,IDC_UV_V);
    UVEditorApplyFields();
    assert(messages==2 && !errors && capturedcount==2 && captured[0].t==192 && captured[1].t==384);
    assert(captured[0].s==-1280 && captured[1].s==-2560);
    assert(state.nodes[2].source.s==3072 && state.nodes[2].source.t==4096);
    UVEditorApplyFields(); assert(messages==2); /* Already aligned: no history message. */
    /* Range failure in the second node must not commit the first. */
    const char *why=""; assert(!UVCanvasSetCoordinate(g_UVCanvas,0,20,&why) && why[0] && messages==2);
    assert(state.nodes[0].source.s==-1280);
    const char *bad[]={"nan","inf","1e999","1.2oops",""};
    for (int i=0;i<5;i++) { strcpy(fields[1],bad[i]); UVEditorApplyFields(); }
    assert(errors==5 && messages==2);
    state.nodes[1].selected=FALSE;
    UVEditorUpdateFields(); assert(enabled[0] && enabled[1]);
    assert(UVCanvasSetCoordinate(g_UVCanvas,1,-64,&why) && capturedcount==1 && captured[0].t==-32768);
    state.nodes[0].selected=FALSE; UVEditorUpdateFields(); assert(!enabled[0] && !enabled[1] && !fields[0][0]);
    assert(!UVCanvasSetCoordinate(g_UVCanvas,0,0,&why));
    state.nodes[0].selected=state.nodes[1].selected=TRUE; state.mode=TRANSFORM_ROTATE;
    UVEditorUpdateFields(); assert(enabled[0] && !enabled[1]); strcpy(fields[0],"90"); UVEditorApplyFields(); assert(transforms==1);
    state.mode=TRANSFORM_SCALE; UVEditorUpdateFields(); assert(enabled[0] && enabled[1]);
    strcpy(fields[0],"2"); strcpy(fields[1],"3"); UVEditorApplyFields(); assert(transforms==2);
    free(state.nodes); memset(&state,0,sizeof(state));
    puts("PASS: actual U/V fields show averages; Enter changes only the focused axis; multiple texture sizes, native limits, no-op input and existing rotate/scale controls.");
}
static void Cylinder(void)
{
    state.nodecount=16; state.trianglecount=16;
    state.nodes=calloc(16,sizeof(*state.nodes)); state.triangles=calloc(16,sizeof(*state.triangles));
    assert(state.nodes && state.triangles);
    for (int i=0;i<8;i++) for (int t=0;t<2;t++)
    {
        UVCanvasTriangle *face=&state.triangles[i*2+t]; int next=(i+1)%8;
        face->face=(BgFaceRef){.room=1,.faceid=(DWORD)(i*2+t+50)}; face->width=128; face->height=32;
        int v[3]={t ? next : i,i+8,t ? next+8 : next};
        for (int c=0;c<3;c++)
        {
            face->nodes[c]=v[c]; face->source[c].vertexid=v[c]+10;
            face->position[c][0]=100*cos((v[c]%8)*UVCANVAS_PI/4);
            face->position[c][1]=v[c]>=8 ? 400 : 0;
            face->position[c][2]=100*sin((v[c]%8)*UVCANVAS_PI/4);
        }
    }
    const char *why=""; int before=messages;
    state.triangles[15].height=2048;
    assert(!UVCanvasProjectCylinder(g_UVCanvas,&why) && why[0] && messages==before);
    state.triangles[15].height=32;
    /* Synchronous editor rebuild frees/replaces every canvas pointer. */
    destroyoncommit=TRUE;
    assert(UVCanvasProjectCylinder(g_UVCanvas,&why) && mappedcount==16 && messages==before+1);
    for (int f=0;f<16;f++)
    {
        int lo=99999,hi=-99999;
        assert(mapped[f].face.faceid==(DWORD)(f+50));
        for (int c=0;c<3;c++)
        {
            assert(mapped[f].vertexids[c]>=10 && mapped[f].vertexids[c]<26);
            if (mapped[f].s[c]<lo) { lo=mapped[f].s[c]; } if (mapped[f].s[c]>hi) { hi=mapped[f].s[c]; }
            assert(mapped[f].t[c]==0 || mapped[f].t[c]==1024);
        }
        assert(hi-lo==512);
    }
    puts("PASS: actual cylindrical button operation produces native per-corner UVs and face identities, rejects range overflow, and survives synchronous canvas replacement.");
}
int main(void) { Coordinates(); Cylinder(); }
