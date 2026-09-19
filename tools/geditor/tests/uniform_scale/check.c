#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef void *HWND;
typedef intptr_t LPARAM, LRESULT;
typedef uintptr_t WPARAM;
typedef unsigned int UINT;
typedef struct { int x,y; } POINT;
#define WM_APP 0x8000
#define VK_CONTROL 17
#define MB_ICONERROR 1
#define MB_ICONINFORMATION 2
#include "viewport.h"
#include "cameraframe.h"
#define VIEWPORT_BOX_VERTICES 24
#define VIEWPORT_OBJECT_NONE 0xffffffffu
#define GL_TRIANGLES 4
/* Unused native rendering fields retain the production viewport structure. */
typedef int HDC,HGLRC,HCURSOR,GLuint,GLsizei,FogCurve,ViewportTexture,LARGE_INTEGER,FogCoordPointerFn;
typedef int OrbitCamera,ModelLighting,VertexColor,ViewportAimGuide,ViewportMonitors,ViewportStanComponent;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef struct { double previewposition[3]; } ViewportPad;
#include "types.inc"
#include "asset.inc"

static HWND capture;
static double parameter, eye[3]={100,150,200};
static Vertex original[9];
static int commits, copies;
static Scaling committed;
static ViewportObjectDuplicate copied;
static int groups, drawn[4], colors[4][3], color[3];
static BgEnvironmentVertex environment;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static int GetKeyState(int key) { return 0; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static HWND GetCapture(void) { return capture; }
static void SetCapture(HWND hwnd) { capture=hwnd; }
static void ReleaseCapture(void) { capture=NULL; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags) { abort(); }
static LRESULT SendMessage(HWND hwnd,int message,int wparam,LPARAM lparam)
{
    ViewportState *s=hwnd;
    if (message==VIEWPORT_WM_TRANSFORM_PREVIEW) return TRUE;
    /* Preview must be restored before handing one transaction to the frame. */
    assert(!capture && s->dragaxis==-1 && !s->dragvertices && !s->dragmask);
    assert(!memcmp(s->scene,original,sizeof(original)));
    if (message==VIEWPORT_WM_SCALE_SELECTION) { committed=*(Scaling *)lparam;commits++; }
    else if (message==VIEWPORT_WM_DUPLICATE_OBJECT) { copied=*(ViewportObjectDuplicate *)lparam;copies++; }
    else abort();
    return TRUE;
}
static void glColor3ub(int r,int g,int b) { color[0]=r;color[1]=g;color[2]=b; }
static void glBegin(int mode) { assert(mode==GL_TRIANGLES && groups<4);memcpy(colors[groups],color,sizeof(color)); }
static void glVertex3f(float x,float y,float z) { assert(isfinite(x)&&isfinite(y)&&isfinite(z));drawn[groups]++; }
static void glEnd(void) { groups++; }
BOOL SetupFileCanDuplicateObject(const SetupFile *s,DWORD i) { return TRUE; }
static BOOL ViewportSelectedMarker(const ViewportState *s,SetupMarker *m) { return FALSE; }
static int ViewportSelectedPadIndex(const ViewportState *s) { return -1; }
static BOOL ViewportCornerVisible(const ViewportState *s,int corner) { return corner>=0 && corner<s->scenecount; }
static BOOL ViewportBuildPickRay(HWND h,const ViewportState *s,int x,int y,ViewportPickRay *r)
{
    /* Orthographic ray fixture; camera orientation varies independently of the
     * screen motion. Production triangle intersection and handle picking run. */
    double length=sqrt(eye[0]*eye[0]+eye[1]*eye[1]+eye[2]*eye[2]);
    double horizontal=hypot(eye[0],eye[2]);
    double right[3]={eye[2]/horizontal,0,-eye[0]/horizontal};
    double up[3]={-eye[0]*eye[1]/(length*horizontal),horizontal/length,-eye[2]*eye[1]/(length*horizontal)};
    for(int k=0;k<3;k++) {
        r->origin[k]=s->gizmoposition[k]+eye[k]+(x-100)*right[k]-(y-100)*up[k];
        r->direction[k]=-eye[k]/length;
    }
    r->mindistance=.1;r->maxdistance=10000;return TRUE;
}
static double ViewportRotationParameter(const ViewportState *s,const ViewportPickRay *r,int x,int y) { return parameter; }
static double ViewportDragParameter(const ViewportState *s,const ViewportPickRay *r,int y) { return parameter; }
static double ViewportGizmoScale(const ViewportState *s) { return s->gizmovisible && s->scalevalid ? 90 : 0; }
static BOOL ViewportPreviewMarker(HWND h,ViewportState *s,double d,const Rotation *r) { abort(); }
static void ViewportPreviewPortalDrag(ViewportState *s,double d) { abort(); }
static BOOL ViewportPadPosition(const ViewportState *s,const SetupPadRef *p,BOOL preview,double out[3]) { abort(); }
static void ViewportRefreshStanOverlay(ViewportState *s) { abort(); }
static void ViewportBuildObjectSelectionBox(ViewportState *s) {}
static void ViewportUpdateGizmo(ViewportState *s) { s->hoveraxis=-1; }
static void ViewportRefreshKnifePlane(HWND h,ViewportState *s) { abort(); }
static void ViewportFinishKnifeTransform(HWND h,ViewportState *s,BOOL cancel) { abort(); }
static void ViewportCancelBoxSelection(HWND h,ViewportState *s) {}
static void ViewportSetSetupMarkers(HWND h,ViewportState *s,const SetupFile *f,float scale) { abort(); }
DWORD ViewportGetPortalSelectionCount(HWND h) { return 0; }
DWORD ViewportGetStanSelectionCount(HWND h,DWORD *t) { return 0; }
static void ViewportPreparePortalDrag(ViewportState *s) { abort(); }
StanPointRef *ViewportGetMoveStanPoints(HWND h,DWORD *n) { abort(); }
static StanPointRef ViewportStanPointRef(const ViewportState *s,DWORD t,DWORD p) { abort(); }
static int ViewportCompareStanRefs(const void *a,const void *b) { abort(); }
static BOOL ViewportProject(const ViewportState *s,const Vertex *v,double out[2]) { return FALSE; }
static BOOL ViewportPrepareEdgeExtrusion(ViewportState *s) { abort(); }
void BgRenderEnvironmentCoordinates(const BgEnvironmentVertex *v,BgRenderFlags f,const float r[3],const float u[3],float uv[2]) { environment=*v; }
#include "functions.inc"

static void Near(double a,double b) { assert(fabs(a-b)<.0001); }
static void Factors(const Scaling *s,double factor)
{
    assert(ScalingValid(s));
    for(int k=0;k<3;k++) Near(s->factor[k],factor);
}
static void Render(const ViewportState *s)
{
    groups=0;memset(drawn,0,sizeof(drawn));ViewportDrawGizmoHandles(s,90);
}
static void Handles(ViewportState *s)
{
    Render(s);assert(groups==4 && drawn[3]==36);
    for(int k=0;k<3;k++) assert(colors[3][k]==255);
    s->hoveraxis=3;Render(s);
    assert(colors[3][0]==255 && colors[3][1]==205 && colors[3][2]==0);
    s->hoveraxis=-1;s->dragaxis=3;Render(s);
    assert(colors[3][0]==255 && colors[3][1]==205 && colors[3][2]==0);s->dragaxis=-1;
    for(int view=0;view<4;view++) {
        eye[0]=view&1 ? -100 : 100;eye[1]=view&2 ? -150 : 150;
        assert(ViewportPickGizmo(s,s,100,100)==3);
        assert(ViewportPickGizmo(s,s,400,400)==-1);
    }
    eye[0]=100;eye[1]=150;
    /* Every original axis handle remains pickable with the center present. */
    unsigned hits=0;
    for(int y=0;y<200;y+=2)for(int x=0;x<200;x+=2) {
        int picked=ViewportPickGizmo(s,s,x,y);if(picked>=0)hits|=1u<<picked;
    }
    assert(hits==15);
    s->scalemode=FALSE;Render(s);assert(groups==3);
    assert(ViewportPickGizmo(s,s,100,100)!=3);
    s->rotationmode=TRUE;s->rotationaxes=7;Render(s);assert(groups==3);
    assert(ViewportPickGizmo(s,s,100,100)!=3);
    s->rotationmode=FALSE;s->scalemode=TRUE;
    s->flying=TRUE;assert(ViewportPickGizmo(s,s,100,100)==-1);s->flying=FALSE;
    s->gizmovisible=FALSE;assert(ViewportPickGizmo(s,s,100,100)==-1);s->gizmovisible=TRUE;
    puts("PASS: shipped axis meshes plus center cube, white/gold drawing, picking from four views and mode gating.");
}
static void Preview(ViewportState *s,double factor)
{
    Scaling live;assert(ViewportGetScaling(s,&live));Factors(&live,factor);
    for(int i=0;i<s->scenecount;i++) {
        double before[3]={original[i].x,original[i].y,original[i].z};
        double now[3]={s->scene[i].x,s->scene[i].y,s->scene[i].z};
        for(int k=0;k<3;k++) Near(now[k],s->dragmask[i] ? s->dragorigin[k]+(before[k]-s->dragorigin[k])*factor : before[k]);
    }
    double guide[3]={11,22,33};ViewportPreviewGuidePoint(s,guide);
    for(int k=0;k<3;k++) Near(guide[k],s->dragorigin[k]+((k+1)*11-s->dragorigin[k])*factor);
    if(s->selectedobject!=VIEWPORT_OBJECT_NONE) {
        float right[3]={1,0,0},up[3]={0,1,0},uv[2];
        ViewportEnvironmentCoordinates(s,0,0,right,up,uv);
        for(int k=0;k<3;k++) Near(environment.normal[k],original[0].environment.normal[k]/factor);
        ViewportEnvironmentCoordinates(s,3,0,right,up,uv);
        for(int k=0;k<3;k++) Near(environment.normal[k],original[3].environment.normal[k]);
    }
}
static void Begin(ViewportState *s,BOOL shift)
{
    assert(ViewportBeginTransform(s,s,100,100,shift));
    assert(s->dragaxis==3 && s->dragscaling && !s->dragextruding && capture==s);
    assert(s->dragduplicating==(shift && s->selectedobject!=VIEWPORT_OBJECT_NONE));
    for(int i=0;i<9;i++) {
        BOOL selected=s->selectedobject!=VIEWPORT_OBJECT_NONE ? i<3
            : (i<(s->tool==EDITOR_TOOL_EDGE_SELECT ? 2 : 3) || i==6);
        assert(s->dragmask[i]==selected);
    }
}
static void Drags(ViewportState *s)
{
    const int moves[][2]={{190,100},{100,10},{55,100},{100,145},{145,55},{145,145},{-900,100},{100,100}};
    const double factors[]={2,2,.5,.5,2,1,.01,1};
    int before=commits;
    Begin(s,FALSE);
    for(int i=0;i<8;i++){ViewportDragTransform(s,s,moves[i][0],moves[i][1]);Preview(s,factors[i]);}
    ViewportEndTransform(s,s);assert(commits==before && !capture);
    assert(!memcmp(s->scene,original,sizeof(original)));
    Begin(s,FALSE);ViewportDragTransform(s,s,190,100);Preview(s,2);
    ViewportCancelTransform(s);assert(commits==before && !capture);
    assert(!memcmp(s->scene,original,sizeof(original)));
    Begin(s,FALSE);ViewportDragTransform(s,s,190,100);ViewportEndTransform(s,s);
    assert(commits==before+1);Factors(&committed,2);
    assert(!memcmp(committed.pivot,s->gizmoposition,sizeof(committed.pivot)));
    /* Shift only duplicates an object; BG edge scaling must not extrude. */
    int copycount=copies;
    Begin(s,TRUE);ViewportDragTransform(s,s,100,145);ViewportEndTransform(s,s);
    if(s->selectedobject!=VIEWPORT_OBJECT_NONE) {
        assert(copies==copycount+1 && copied.mode==TRANSFORM_SCALE && copied.source==s->selectedobject);
        Factors(&copied.scaling,.5);
        assert(!memcmp(copied.scaling.pivot,s->gizmoposition,sizeof(copied.scaling.pivot)));
        for(int k=0;k<3;k++)assert(copied.translation.offset[k]==0);
    } else {assert(copies==copycount && commits==before+2);Factors(&committed,.5);}
    Begin(s,TRUE);ViewportEndTransform(s,s);assert(copies==copycount+(s->selectedobject!=VIEWPORT_OBJECT_NONE));
}
static void AxisDrag(ViewportState *s)
{
    for(int y=0;y<200;y++)for(int x=0;x<200;x++)if(ViewportPickGizmo(s,s,x,y)==0) {
        parameter=0;assert(ViewportBeginTransform(s,s,x,y,FALSE));
        parameter=90;ViewportDragTransform(s,s,x,y);
        Scaling scale;assert(ViewportGetScaling(s,&scale));
        Near(scale.factor[0],2);Near(scale.factor[1],1);Near(scale.factor[2],1);
        ViewportEndTransform(s,s);Near(committed.factor[0],2);Near(committed.factor[1],1);Near(committed.factor[2],1);
        return;
    }
    abort();
}
int main(void)
{
    Vertex scene[9]={0};BgDocumentVertexRef refs[9];unsigned char selected[3]={1,0,0};
    DWORD objects[3]={7,8,9};ViewportComponent edge={.corners={0,1}};
    ViewportState s={.dragaxis=-1,.hoveraxis=-1,.scalemode=TRUE,.scalevalid=TRUE,.gizmovisible=TRUE,
        .selectedobject=VIEWPORT_OBJECT_NONE,.tool=EDITOR_TOOL_FACE_SELECT,.scene=scene,.scenecount=9,
        .scenevertexrefs=refs,.selectedtris=selected,.sceneobjectindices=objects,.gizmoposition={10.25,20.5,30.75},
        .scalehandle=handle,.scalehandletris=sizeof(handle)/sizeof(*handle)/3,
        .arrow=handle,.arrowtris=sizeof(handle)/sizeof(*handle)/3,.cylinder=handle,.cylindertris=sizeof(handle)/sizeof(*handle)/3};
    double degrees[3]={0};RotationEuler(&s.scaleaxes,degrees);
    for(int i=0;i<9;i++) {
        scene[i].x=11+i;scene[i].y=17-i;scene[i].z=35+2*i;
        scene[i].environment.normal[0]=.2;scene[i].environment.normal[1]=.4;scene[i].environment.normal[2]=.8;
        refs[i]=(BgDocumentVertexRef){.room=1,.index=i};
    }
    scene[6]=scene[0];refs[6]=refs[0]; /* Same native vertex on another face follows. */
    scene[7]=scene[0]; /* A coincident but separate native vertex does not. */
    memcpy(original,scene,sizeof(original));edge.refs[0]=refs[0];edge.refs[1]=refs[1];
    Handles(&s);
    for(int mode=0;mode<3;mode++) {
        s.tool=mode==1 ? EDITOR_TOOL_EDGE_SELECT : EDITOR_TOOL_FACE_SELECT;
        s.components=&edge;s.componentcount=mode==1;
        s.selectedobject=mode==2 ? 7 : VIEWPORT_OBJECT_NONE;
        for(int rotated=0;rotated<2;rotated++) {
            degrees[0]=rotated*19;degrees[1]=rotated*31;degrees[2]=rotated*47;
            RotationEuler(&s.scaleaxes,degrees);Drags(&s);
        }
    }
    AxisDrag(&s);
    puts("PASS: object/face/edge masks, rotated axes, screen directions, snapshot preview, proportional XYZ factors, guides/normals, no-op, clamp, cancellation, one commit and Shift duplication; axis scaling unchanged.");
    return 0;
}
