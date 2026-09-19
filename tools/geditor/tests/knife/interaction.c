#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef void *HWND;
typedef intptr_t LPARAM, LRESULT;
typedef uintptr_t WPARAM;
typedef unsigned int UINT;
typedef struct { int x,y; } POINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
#define WM_APP 0x8000
#define WM_KEYDOWN 1
#define WM_LBUTTONDOWN 2
#define WM_LBUTTONDBLCLK 3
#define WM_LBUTTONUP 4
#define VK_ESCAPE 27
#define VK_CONTROL 17
#define MB_ICONERROR 1
#define MB_ICONINFORMATION 2
#define GET_X_LPARAM(p) ((short)((p)&65535))
#define GET_Y_LPARAM(p) ((short)(((p)>>16)&65535))
#include "viewport.h"
#include "cameraframe.h"
#include "uvcanvas.h"
#include "resource.h"
#define VIEWPORT_BOX_VERTICES 24
#define VIEWPORT_OBJECT_NONE 0xffffffffu
#define UVCANVAS_PI 3.14159265358979323846
/* Unused rendering/camera fields keep the production state layout in this host fixture. */
typedef int HDC,HGLRC,HCURSOR,GLuint,GLsizei,FogCurve,ViewportTexture,LARGE_INTEGER,FogCoordPointerFn;
typedef int OrbitCamera,ModelLighting,VertexColor,ViewportAimGuide,ViewportMonitors,ViewportStanComponent;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef struct { double previewposition[3]; } ViewportPad;
#include "interaction_types.inc"
BOOL SetupFileCanDuplicateObject(const SetupFile *s,DWORD i) { abort(); }
static BOOL control;
static HWND capture;
static int scenecommits, previews, uvcommits;
static double parameter;
static int pickedaxis;
static BgDocumentUVEdit uvedit;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static int GetKeyState(int key) { assert(key==VK_CONTROL); return control ? 0x8000 : 0; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static HWND GetCapture(void) { return capture; }
static void SetCapture(HWND h) { capture=h; }
static void ReleaseCapture(void) { capture=NULL; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags) { abort(); }
static LRESULT SendMessage(HWND hwnd,int message,int wparam,LPARAM lparam)
{
    if (message==VIEWPORT_WM_TRANSFORM_PREVIEW) previews++;
    else if (message==UVCANVAS_WM_COMMIT) {
        const UVCanvasEdit *edit=(void *)lparam;
        assert(edit->count==1 && !strcmp(edit->action,"Rotate UV Vertices"));
        uvedit=edit->vertices[0];uvcommits++;
    } else scenecommits++;
    return TRUE;
}
static BOOL ViewportPortalSelectionPosition(const ViewportState *s,double p[3],DWORD *n) { return FALSE; }
static BOOL ViewportSelectedMarker(const ViewportState *s,SetupMarker *m) { return FALSE; }
static int ViewportSelectedPadIndex(const ViewportState *s) { return -1; }
static BOOL ViewportStanSelectionPosition(const ViewportState *s,BOOL anchor,double p[3],DWORD *n) { return FALSE; }
static BOOL ViewportPadSelectionPosition(const ViewportState *s,double p[3],BOOL anchor) { return FALSE; }
static BOOL ViewportCornerVisible(const ViewportState *s,int corner) { return TRUE; }
static BOOL ViewportBuildPickRay(HWND h,const ViewportState *s,int x,int y,ViewportPickRay *r) { memset(r,0,sizeof(*r));r->direction[2]=-1;return TRUE; }
static double ViewportRotationParameter(const ViewportState *s,const ViewportPickRay *r,int x,int y) { return parameter; }
static double ViewportDragParameter(const ViewportState *s,const ViewportPickRay *r,int y) { return parameter; }
static BOOL ViewportPreviewMarker(HWND h,ViewportState *s,double d,const Rotation *r) { abort(); }
static void ViewportPreviewPortalDrag(ViewportState *s,double d) { abort(); }
static BOOL ViewportPadPosition(const ViewportState *s,const SetupPadRef *p,BOOL preview,double out[3]) { abort(); }
static void ViewportRefreshStanOverlay(ViewportState *s) {}
static void ViewportBuildObjectSelectionBox(ViewportState *s) {}
static void ViewportCancelBoxSelection(HWND h,ViewportState *s) {}
static void ViewportSetSetupMarkers(HWND h,ViewportState *s,const SetupFile *f,float scale) { abort(); }
static int ViewportPickGizmo(HWND h,ViewportState *s,int x,int y) { return s->gizmovisible ? pickedaxis : -1; }
static double ViewportGizmoScale(const ViewportState *s) { return 100; }
DWORD ViewportGetPortalSelectionCount(HWND h) { abort(); }
DWORD ViewportGetStanSelectionCount(HWND h,DWORD *t) { abort(); }
static void ViewportPreparePortalDrag(ViewportState *s) { abort(); }
StanPointRef *ViewportGetMoveStanPoints(HWND h,DWORD *n) { abort(); }
static StanPointRef ViewportStanPointRef(const ViewportState *s,DWORD t,DWORD p) { abort(); }
static int ViewportCompareStanRefs(const void *a,const void *b) { abort(); }
BgDocumentVertexRef *ViewportGetMoveVertices(HWND h,DWORD *n) { abort(); }
static int ViewportCompareVertexRefs(const void *a,const void *b) { abort(); }
static BOOL ViewportProject(const ViewportState *s,const Vertex *v,double out[2]) { return FALSE; }
static BOOL ViewportPrepareEdgeExtrusion(ViewportState *s) { abort(); }
BOOL ViewportGetMarkerRotation(HWND h,Rotation *r) { abort(); }
#include "plane.inc"
static void UVCanvasUpdatePreview(UVCanvasState *s) {}
static void UVCanvasNotify(HWND hwnd) {}
BOOL UVCanvasCancelInteraction(HWND hwnd) { ((UVCanvasState *)hwnd)->draghandle=0; return TRUE; }
#include "uv_drag.inc"

static void Begin(ViewportState *s,int axis,BOOL rotate)
{
    s->rotationmode=rotate;s->rotationframe=s->knifeframe;s->rotationaxes=7;
    pickedaxis=axis;parameter=0;
    assert(ViewportBeginKnifeTransform(s,0,0));
    assert(s->dragknife && s->dragaxis==axis && capture==s && !s->dragvertices && !s->dragmask);
}
static void CheckPlane(const ViewportState *s)
{
    assert(RotationValid(&s->knifeframe));
    for (int a=0;a<3;a++) assert(fabs(s->knifeframe.m[a][1]-s->knifeplane.normal[a])<1e-10);
    for(int c=0;c<4;c++) {
        double dot=0;
        for(int a=0;a<3;a++) dot+=(s->knifecorners[c][a]-s->knifeplane.position[a])*s->knifeplane.normal[a];
        assert(fabs(dot)<1e-7);
    }
}
static void Plane(void)
{
    ViewportState s={.dragaxis=-1,.selectedobject=VIEWPORT_OBJECT_NONE,.tool=EDITOR_TOOL_FACE_SELECT};
    Vertex scene[3]={{.x=1},{.y=1},{.z=1}}, before[3];unsigned char selected=1;
    s.scene=scene;s.scenecount=3;s.selectedtris=&selected;memcpy(before,scene,sizeof(scene));
    BgKnifePlane plane={{10,20,30},{0,1,0}}, got;
    ViewportSetKnifePlane(&s,&plane,150);
    assert(ViewportKnifeActive(&s) && ViewportGetKnifePlane(&s,&got));CheckPlane(&s);
    assert(s.gizmovisible && !memcmp(s.gizmoposition,plane.position,sizeof(plane.position)));
    Begin(&s,0,FALSE);parameter=17.4;ViewportDragTransform(&s,&s,0,0);
    assert(s.knifeplane.position[0]==27);ViewportEndTransform(&s,&s);
    assert(s.knifeplane.position[0]==27 && !capture && !scenecommits);
    Begin(&s,1,FALSE);parameter=-10;ViewportDragTransform(&s,&s,0,0);ViewportCancelTransform(&s);
    assert(s.knifeplane.position[1]==20 && s.dragaxis==-1);
    Begin(&s,2,TRUE);control=TRUE;parameter=26;ViewportDragTransform(&s,&s,0,0);
    assert(s.dragdelta==30 && s.rotationtotal==26);CheckPlane(&s);
    Rotation frame; double degrees[3],pivot[3];
    assert(ViewportGetRotation(&s,&frame,degrees,pivot) && fabs(degrees[2]-30)<1e-10);
    assert(!memcmp(pivot,s.knifeplane.position,sizeof(pivot)));
    control=FALSE;ViewportDragTransform(&s,&s,0,0);assert(s.dragdelta==26 && s.rotationtotal==26);
    control=TRUE;parameter=-16;ViewportDragTransform(&s,&s,0,0);assert(s.dragdelta==-20);
    ViewportCancelTransform(&s);assert(!memcmp(s.knifeplane.normal,plane.normal,sizeof(plane.normal)));
    plane.normal[0]=1;plane.normal[1]=2;plane.normal[2]=3;
    ViewportSetKnifePlane(&s,&plane,150);CheckPlane(&s);
    Rotation rotate;RotationAxis(&rotate,0,40);double offset[3]={0,4,0};
    assert(ViewportTransformKnife(&s,offset,&rotate));CheckPlane(&s);
    assert(s.knifeplane.position[1]==24 && selected==1 && !memcmp(scene,before,sizeof(scene)) && !scenecommits);
    ViewportSetKnifePlane(&s,NULL,150);assert(ViewportKnifeActive(&s) && !s.gizmovisible);
    assert(!ViewportTransformKnife(&s,offset,NULL));
    ViewportSetKnifePlane(&s,NULL,0);assert(!ViewportKnifeActive(&s));
    /* Same production drag path rotates ordinary geometry with Ctrl. */
    s.dragknife=FALSE;s.dragrotation=TRUE;s.dragaxis=2;s.rotationlast=s.rotationtotal=s.dragdelta=0;
    memset(s.dragorigin,0,sizeof(s.dragorigin));
    s.dragvertices=calloc(3,sizeof(*s.dragvertices));s.dragmask=calloc(3,1);assert(s.dragvertices&&s.dragmask);
    for(int i=0;i<3;i++){s.dragvertices[i][0]=scene[i].x;s.dragvertices[i][1]=scene[i].y;s.dragvertices[i][2]=scene[i].z;s.dragmask[i]=1;}
    parameter=14;ViewportDragTransform(&s,&s,0,0);assert(s.dragdelta==10);
    assert(fabs(scene[0].x-cos(10*UVCANVAS_PI/180))<1e-6);
    ViewportCancelTransform(&s);assert(!memcmp(scene,before,sizeof(scene)));
    puts("PASS: knife preview center/normal, move/rotate, exact scene isolation, commit/cancel, invalid fields and Ctrl snapping in the shared 3D drag path.");
}
static void UV(void)
{
    UVCanvasNode node={.selected=TRUE,.width=32,.height=32,.source={.s=1024,.t=0}};
    UVCanvasState s={.mode=TRANSFORM_ROTATE,.draghandle=3,.nodes=&node,.nodecount=1,.pixelsperunit=100};
    control=TRUE;UVCanvasDrag(&s,&s,97,-23);assert(s.values[0]==10);
    double raw=s.dragangle;control=FALSE;UVCanvasDrag(&s,&s,97,-23);
    assert(s.values[0]==raw && raw>13 && raw<14);
    control=TRUE;UVCanvasDrag(&s,&s,97,23);assert(s.values[0]==-10);
    assert(node.source.s==1024 && node.source.t==0);
    assert(UVCanvasCommit(&s,&s) && uvcommits==1);
    assert(uvedit.s==(int)round(cos(-10*UVCANVAS_PI/180)*1024));
    assert(uvedit.t==(int)round(sin(-10*UVCANVAS_PI/180)*1024));
    s.draghandle=3;s.lastangle=179*UVCANVAS_PI/180;s.dragangle=179;
    UVCanvasDrag(&s,&s,-100,2);assert(s.values[0]==180 && s.dragangle>180 && s.dragangle<182);
    assert(RotationSnapDegrees(-15)==-20 && RotationSnapDegrees(15)==20 && RotationSnapDegrees(364)==360);
    puts("PASS: UV Ctrl snapping, press/release without drift, negative angles, wraparound and native UV commit.");
}

static HWND g_Knife=(HWND)1,g_KnifeViewport;
static BOOL g_KnifePick,hit,transforming;
static BgKnifePlane g_KnifePlane;
static int writes,closed,cancelled,starts;
static void SetFocus(HWND h) {}
static HWND GetDlgItem(HWND h,int id) { return h; }
static void EnableWindow(HWND h,BOOL e) {}
static void SetDlgItemText(HWND h,int id,const char *text) {}
static BOOL IsChild(HWND a,HWND b) { return FALSE; }
static BOOL IsDialogMessage(HWND a,MSG *m) { return TRUE; }
static void TranslateMessage(MSG *m) {}
static void DispatchMessage(MSG *m) {}
BOOL ViewportIsTransforming(HWND h) { return transforming; }
BOOL ViewportGetKnifePoint(HWND h,int x,int y,double p[3]) { p[0]=3;p[1]=4;p[2]=5;return hit; }
static BOOL PickBeginTransform(HWND h,int x,int y) { starts++;return TRUE; }
static void KnifeCancelPick(void) { cancelled++;g_KnifePick=FALSE; }
static void KnifeDialogClose(void) { closed++; }
static void KnifeWritePlane(void) { writes++; }
#define ViewportBeginKnifeTransform PickBeginTransform
#include "pick.inc"
#undef ViewportBeginKnifeTransform
static void Pick(void)
{
    g_KnifeViewport=(HWND)2;g_KnifePick=TRUE;g_KnifePlane.normal[1]=1;
    MSG m={g_KnifeViewport,WM_LBUTTONDOWN,0,0};
    assert(KnifeDialogHandleMessage(&m) && g_KnifePick && !writes);
    hit=TRUE;assert(KnifeDialogHandleMessage(&m) && !g_KnifePick && writes==1);
    assert(g_KnifePlane.position[0]==3 && g_KnifePlane.position[1]==4 && g_KnifePlane.position[2]==5);
    assert(g_KnifePlane.normal[1]==1 && g_KnifePlane.normal[0]==0 && g_KnifePlane.normal[2]==0);
    assert(KnifeDialogHandleMessage(&m) && starts==1 && writes==1);
    m.message=WM_LBUTTONUP;transforming=TRUE;assert(!KnifeDialogHandleMessage(&m));
    m.message=WM_KEYDOWN;m.wParam=VK_ESCAPE;assert(!KnifeDialogHandleMessage(&m) && !closed);
    transforming=FALSE;g_KnifePick=TRUE;assert(KnifeDialogHandleMessage(&m) && cancelled==1);
    assert(KnifeDialogHandleMessage(&m) && closed==1);
    puts("PASS: actual one-point message routing, misses, preserved orientation, gizmo routing and Escape/cancel precedence.");
}
int main(void) { Plane();UV();Pick();return 0; }
