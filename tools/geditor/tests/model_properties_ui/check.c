#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include "modelload.h"
#include "bgdocument.h"
#include "resource.h"
#include "orbitcamera.h"
typedef unsigned int GLenum, GLuint, UINT;
typedef int GLsizei;
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef struct {int x, y;} POINT;
typedef struct {int left,top,right,bottom;} RECT;
#define HIWORD(n) (((uintptr_t)(n)>>16)&0xffff)
#define LOWORD(n) ((uintptr_t)(n)&0xffff)
#define MAKELPARAM(x,y) ((unsigned short)(x)|((LPARAM)(unsigned short)(y)<<16))
#define LB_ERR (-1)
enum {LB_ITEMFROMPOINT=10,LB_GETITEMRECT,LB_SETCURSEL,LB_GETITEMDATA,MB_ICONERROR,GW_OWNER,MODELEDITOR_CHANGED};
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define MK_LBUTTON 1
#define MK_RBUTTON 2
#define MK_SHIFT 4
#define MK_CONTROL 8
#define MK_MBUTTON 16
#define VIEWPORT_FOV_Y 60
#define WHEEL_DELTA 120
#define VK_ESCAPE 27
#define EDITOR_TOOL_VERTEX_PAINT 3
#define GET_X_LPARAM(l) ((short)(l))
#define GET_Y_LPARAM(l) ((short)((l) >> 16))
#define GET_WHEEL_DELTA_WPARAM(w) ((short)((w) >> 16))
enum {WM_LBUTTONDOWN=0x201, WM_LBUTTONUP, WM_LBUTTONDBLCLK,
    WM_RBUTTONDOWN=0x204, WM_RBUTTONUP, WM_RBUTTONDBLCLK,
    WM_MBUTTONDOWN=0x207, WM_MBUTTONUP, WM_MBUTTONDBLCLK,
    WM_MOUSEMOVE=0x200, WM_MOUSEWHEEL=0x20a, WM_CANCELMODE=0x1f, WM_CAPTURECHANGED=0x215,
    WM_KILLFOCUS=8, WM_KEYDOWN=0x100, WM_KEYUP};
#include "types.inc"
#define lstrcmpi strcasecmp
#define lstrcpyn(dst,src,size) snprintf(dst,size,"%s",src)
enum {CB_GETCOUNT, CB_GETITEMDATA, CB_SETCURSEL, CB_GETCURSEL};
static const int g_ModelCombos[] = {0, 1, 2};
static HWND g_ModelEditor = (HWND)(uintptr_t)1, g_ModelViewport = (HWND)(uintptr_t)5;
static ModelEditorEntry g_ModelEntries[] = {
    {"Pjungle3_treeZ", "objects"}, {"CcamguardZ", "characters"},
    {"Gpp7Z", "guns"}, {"Pjungle1_treeZ", "objects"}
};
static int g_ModelCount = 4, g_ModelSelected = -1;
static ModelSource g_ModelSource;
/* Sorted combo rows deliberately differ from the asset array order. */
static const int rows[3][2] = {{1, -1}, {2, -1}, {3, 0}};
static int selectedrows[3] = {-1, -1, -1}, shown, loads;
static BOOL showok = TRUE;
static BOOL editorvisible=TRUE,hitlist=TRUE,assignok=TRUE;
static int assigned,refreshed,selectedslot=-1,notified,errors;
static DWORD assignedtexture,assignedslot,g_ModelRevision=123;
static const char g_ModelProject[]="project";
static BOOL IsWindowVisible(HWND hwnd) {return editorvisible;}
static HWND WindowFromPoint(POINT p) {return hitlist?(HWND)(uintptr_t)12:NULL;}
static BOOL ScreenToClient(HWND hwnd,POINT *p) {p->x+=1000;return TRUE;}
static BOOL GetClientRect(HWND hwnd,RECT *r) {*r=(RECT){0,0,200,300};return TRUE;}
static BOOL PtInRect(const RECT *r,POINT p) {return p.x>=r->left && p.x<r->right && p.y>=r->top && p.y<r->bottom;}
static HWND GetWindow(HWND hwnd,int type) {return (HWND)(uintptr_t)99;}
static void MessageBox(HWND hwnd,const char *why,const char *title,int type) {errors++;}
static int choices[1200], selectedfaces, properties;
static BOOL enabled[1200];
static char current[256];
static BgFaceRef faceselection[2];
static void EnableWindow(HWND hwnd,BOOL enable) {enabled[(uintptr_t)hwnd-2]=enable;}
static LRESULT SendDlgItemMessage(HWND hwnd,int id,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==CB_GETCURSEL) return choices[id];
    assert(message==CB_SETCURSEL);choices[id]=wp;return 0;
}
static int ViewportGetSelectedBgFaceCount(HWND hwnd) {return selectedfaces;}
static BOOL ViewportGetSelectedBgFaces(HWND hwnd,BgFaceRef *refs,int count)
{assert(count==selectedfaces);memcpy(refs,faceselection,count*sizeof(*refs));return TRUE;}
static void SetDlgItemText(HWND hwnd,int id,const char *text)
{if(id==IDC_MODEL_CURRENT) snprintf(current,sizeof(current),"%s",text);}
static BOOL ModelEditsSetProperties(const char *project,const char *name,DWORD revision,
    const DWORD *faces,DWORD count,int cull,int surface,int u,int v,const char **why)
{
    assert(!strcmp(project,"project") && !strcmp(name,"Gpp7Z") && revision==123);
    assert(count==2 && faces[0]==1 && faces[1]==0);
    assert(cull==-1 && surface==-1 && u==2 && v==1);properties++;return assignok;
}
static void ModelEditorRefreshImages(void) {refreshed++;}
static void ModelEditorSelectGroup(BOOL all) {assert(!all);}
static BOOL ModelEditsSetMaterial(const char *project,const char *name,DWORD revision,DWORD slot,DWORD texture,const char **why)
{assert(!strcmp(project,"project") && !strcmp(name,"Gpp7Z") && revision==123);assigned++;assignedslot=slot;assignedtexture=texture;return assignok;}
static HWND GetDlgItem(HWND hwnd, int item) {return (HWND)(uintptr_t)(item==IDC_MODEL_MATERIAL_LIST?12:item+2);}
static LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(hwnd==(HWND)(uintptr_t)99) {assert(msg==MODELEDITOR_CHANGED);notified++;return 0;}
    if(hwnd==(HWND)(uintptr_t)12)
    {
        if(msg==LB_ITEMFROMPOINT) return HIWORD(lparam)<132?HIWORD(lparam)/66:1;
        if(msg==LB_GETITEMRECT) {*(RECT *)lparam=(RECT){0,(int)wparam*66,200,(int)(wparam+1)*66};return 0;}
        if(msg==LB_GETITEMDATA) return wparam==0?4:2; /* Filtered rows are native slot IDs, not row numbers. */
        assert(msg==LB_SETCURSEL);selectedslot=wparam;return 0;
    }
    int category = (int)(uintptr_t)hwnd-2;
    assert(category >= 0 && category < 3);
    if (msg == CB_GETCOUNT) {return category == 2 ? 2 : 1;}
    if (msg == CB_GETITEMDATA) {return rows[category][wparam];}
    assert(msg == CB_SETCURSEL);
    selectedrows[category] = (int)wparam;
    return wparam;
}
static BOOL ModelEditorShow(HWND owner, HINSTANCE instance, const char *projectdir)
{shown++; return showok;}
static void ModelEditorSelect(int category, BOOL framecamera)
{
    static BgVertex vertex;
    assert(framecamera);
    g_ModelSelected = rows[category][selectedrows[category]];
    g_ModelSource.vertices = &vertex;
    loads++;
}
typedef struct {
    OrbitCamera orbitcamera;
    unsigned int orbitbuttons;
    POINT lastmouse, orbitstart;
    BOOL orbitdragged;
    BOOL colorsampleclick, colorpick;
    int tool, height;
} ViewportState;
static HWND captured;
static int picks, updates, paints, samples;
static BOOL samplehit=TRUE;
static ViewportState *activeorbit;
static BOOL add, deselected;
static void SetFocus(HWND hwnd) {(void)hwnd;}
static void SetCapture(HWND hwnd) {captured=hwnd;}
static HWND GetCapture(void) {return captured;}
static void ReleaseCapture(void) {captured=NULL;}
static void InvalidateRect(HWND hwnd, void *rect, BOOL erase) {}
static void ViewportUpdateOrbit(ViewportState *state) {updates++;}
static void ViewportPickAt(HWND hwnd, ViewportState *state, int x, int y, BOOL a, BOOL r)
{picks++; add=a; deselected=r;}
static void ViewportSetColorPick(HWND hwnd, BOOL enabled) {activeorbit->colorpick=enabled;}
static BOOL ViewportSampleColorAt(HWND hwnd, ViewportState *state, int x, int y)
{ if(!state->colorpick) return FALSE; samples++; if(samplehit) state->colorpick=FALSE; return TRUE; }
static void ViewportPaintAt(HWND hwnd, ViewportState *state, int x, int y) {paints++;}
#include "logic.inc"
static LPARAM Position(int x, int y) {return (unsigned short)x | (LPARAM)(unsigned short)y << 16;}
int main(void)
{
    HWND hwnd=(HWND)(uintptr_t)1;
    ViewportState state={0};
    activeorbit=&state;
    SceneBatch batch={0};
    ModelSourceFace face={0};
    const double low[3]={-10,-10,-10}, high[3]={10,10,10};
    OrbitCameraFrame(&state.orbitcamera, low, high, 1, 60);
    assert(ViewportBatchCullMode(&batch, TRUE)==0);
    batch.cullbackfaces=TRUE;
    assert(ViewportBatchCullMode(&batch, TRUE)==GL_BACK); /* legacy BG */
    batch.renderflags=BG_RENDER_CULL_EXPLICIT;
    assert(ViewportBatchCullMode(&batch, TRUE)==0);
    batch.renderflags|=BG_RENDER_CULL_FRONT;
    assert(ViewportBatchCullMode(&batch, TRUE)==GL_FRONT);
    batch.renderflags|=BG_RENDER_CULL_BACK;
    assert(ViewportBatchCullMode(&batch, TRUE)==GL_FRONT_AND_BACK);
    assert(ViewportBatchCullMode(&batch, FALSE)==0);
    assert(ModelEditorCulling(&face)==4); /* inherited is distinct from disabled */
    face.state.geometryknown=0x3000;
    assert(ModelEditorCulling(&face)==0);
    face.state.geometrymode=0x2000;
    assert(ModelEditorCulling(&face)==2);
    assert(ModelEditorSurface(BG_RENDER_DEPTH_WRITE)==0);
    assert(ModelEditorSurface(BG_RENDER_ALPHA_TEST)==1);
    assert(ModelEditorSurface(BG_RENDER_BLEND)==2);
    assert(ModelEditorSurface(BG_RENDER_ALPHA_TEST|BG_RENDER_BLEND)==4);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,MK_SHIFT,Position(10,20));
    assert(picks==1 && add && !deselected && !captured);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_MOUSEMOVE,0,Position(12,21));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,MK_CONTROL,Position(12,21));
    assert(picks==2 && !add && deselected && updates==0); /* small hand jitter */
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_MOUSEMOVE,0,Position(40,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(40,20));
    assert(picks==2 && updates==1);
    ViewportOrbitInput(hwnd,&state,WM_RBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_RBUTTONUP,0,Position(10,20));
    assert(picks==2 && !captured);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_CANCELMODE,0,0);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    assert(picks==2 && !captured);
    /* CS_DBLCLKS replaces the second DOWN with DBLCLK in both viewports. */
    assert(ViewportOrbitInput(hwnd,&state,WM_LBUTTONDBLCLK,0,Position(10,20)));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,MK_SHIFT,Position(10,20));
    assert(picks==3 && add && !captured);
    assert(ViewportOrbitInput(hwnd,&state,WM_RBUTTONDBLCLK,0,Position(10,20)));
    ViewportOrbitInput(hwnd,&state,WM_MOUSEMOVE,0,Position(40,20));
    ViewportOrbitInput(hwnd,&state,WM_RBUTTONUP,0,Position(40,20));
    assert(picks==3 && updates==2 && !captured);
    state.tool=EDITOR_TOOL_VERTEX_PAINT;
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    assert(paints==1 && picks==3 && !captured);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_MOUSEMOVE,0,Position(40,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(40,20));
    assert(paints==1 && updates==3); /* Dragging still orbits without painting. */
    state.colorpick=TRUE;
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    assert(samples==1 && !state.colorpick && paints==1);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDBLCLK,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    assert(paints==1 && samples==1 && !captured);
    samplehit=FALSE;state.colorpick=TRUE;
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_CAPTURECHANGED,0,0);
    assert(state.colorpick && paints==1); /* Releasing our capture preserves a missed sample. */
    ViewportOrbitInput(hwnd,&state,WM_KEYDOWN,VK_ESCAPE,0);
    assert(!state.colorpick);
    state.colorpick=TRUE;
    ViewportOrbitInput(hwnd,&state,WM_KILLFOCUS,0,0);
    assert(!state.colorpick);
    {
        const char *why;
        assert(ModelEditorOpenModel(hwnd,NULL,"project","Pjungle3_treeZ",&why));
        assert(g_ModelSelected==0 && selectedrows[2]==1 && loads==1 && shown==1);
        assert(ModelEditorOpenModel(hwnd,NULL,"project","Pjungle3_treeZ",&why));
        assert(loads==1 && shown==2); /* same asset keeps camera/face selection */
        assert(ModelEditorOpenModel(hwnd,NULL,"project","ccamguardz",&why));
        assert(g_ModelSelected==1 && selectedrows[0]==0 && loads==2);
        assert(ModelEditorOpenModel(hwnd,NULL,"project","Gpp7Z",&why));
        assert(g_ModelSelected==2 && selectedrows[1]==0 && loads==3);
        assert(!ModelEditorOpenModel(hwnd,NULL,"project","missing",&why) && why[0]);
        assert(g_ModelSelected==2 && loads==3); /* never load a different asset as fallback */
        g_ModelSource.vertices=NULL;
        assert(ModelEditorOpenModel(hwnd,NULL,"project","Gpp7Z",&why) && loads==4);
        showok=FALSE;
        assert(!ModelEditorOpenModel(hwnd,NULL,"project","Gpp7Z",&why) && why[0]);
        assert(loads==4);
        assert(!ModelEditorOpenModel(hwnd,NULL,"project",NULL,&why) && why[0]);
    }
    g_ModelSource.materials.count=5;
    assert(ModelEditorCanAssignImages());
    assert(ModelEditorDropImage(0xa93,(POINT){-950,80}));
    assert(assigned==1 && assignedslot==2 && assignedtexture==0xa93 && selectedslot==1 && refreshed==1 && notified==1);
    assert(ModelEditorDropImage(BG_TEX_NONE,(POINT){-950,5}));
    assert(assigned==2 && assignedslot==4 && assignedtexture==BG_TEX_NONE && refreshed==2 && notified==2);
    assert(!ModelEditorDropImage(0xd4,(POINT){-950,250})); /* Empty area below slots. */
    assert(!ModelEditorDropImage(0xd4,(POINT){-795,20})); /* Scrollbar. */
    assert(!ModelEditorDropImage(0xd4,(POINT){-1001,20})); /* Border. */
    hitlist=FALSE;assert(!ModelEditorDropImage(0xd4,(POINT){-950,20}));hitlist=TRUE;
    editorvisible=FALSE;assert(!ModelEditorDropImage(0xd4,(POINT){-950,20}));editorvisible=TRUE;
    assert(assigned==2);
    assignok=FALSE;assert(ModelEditorDropImage(0xbad,(POINT){-950,20}));
    assert(assigned==3 && errors==1 && refreshed==2 && notified==2); /* Consume failed model drop; no BG fallthrough. */
    ModelSourceFace wrapfaces[2]={0}; BgRenderFlags wrapflags[2]={0};
    g_ModelSource.faces=wrapfaces;g_ModelSource.flags=wrapflags;g_ModelSource.count=2;
    for(int i=0;i<2;i++)
    {
        BgMaterialInit(&wrapfaces[i].material);
        BgMaterialSetTexture(&wrapfaces[i].material,1);
    }
    BgMaterialSetWrap(&wrapfaces[1].material,FALSE,BG_TEXTURE_MIRROR);
    faceselection[0].faceid=2;faceselection[1].faceid=1;selectedfaces=2;
    ModelEditorProperties();
    assert(strstr(current,"Wrap U: Mixed; V: Repeat"));
    assert(enabled[IDC_MODEL_WRAP_U] && enabled[IDC_MODEL_WRAP_V]);
    assert(!enabled[IDC_MODEL_CULL] && !enabled[IDC_MODEL_APPLY]); /* Inherited culling permits wrapping. */
    assert(choices[IDC_MODEL_WRAP_U]==0 && choices[IDC_MODEL_WRAP_V]==0);
    ModelEditorApplyProperties();assert(properties==0);
    choices[IDC_MODEL_WRAP_U]=3;choices[IDC_MODEL_WRAP_V]=2;assignok=TRUE;
    ModelEditorApplyProperties();assert(properties==1 && refreshed==3 && notified==3);
    assignok=FALSE;ModelEditorApplyProperties();
    assert(properties==2 && refreshed==3 && notified==3 && errors==2);
    BgMaterialSetTexture(&wrapfaces[0].material,BG_TEX_NONE);
    ModelEditorProperties();assert(enabled[IDC_MODEL_WRAP_U] && strstr(current,"Wrap U: Mixed"));
    BgMaterialSetTexture(&wrapfaces[1].material,BG_TEX_NONE);
    ModelEditorProperties();assert(!enabled[IDC_MODEL_WRAP_U] && !enabled[IDC_MODEL_WRAP_V]);
    assert(strstr(current,"Wrap U: No texture; V: No texture"));
    selectedfaces=0;ModelEditorProperties();
    assert(!enabled[IDC_MODEL_WRAP_U] && !enabled[IDC_MODEL_WRAP_V]);
    assert(strstr(current,"Select faces"));
    puts("PASS wrap inspector mixed/untextured/empty selections, independent U/V application and failed-edit refresh guards.");
    puts("PASS material drop targeting, negative screen coordinates, No Texture, invalid targets and failed assignment.");
    puts("PASS model viewer click/double-click/orbit, sorted asset opening/reuse, culling and inspector labels (ASan + UBSan)");
    return 0;
}
