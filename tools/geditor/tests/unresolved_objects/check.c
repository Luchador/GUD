#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <src/propconstants.h>
#include "setupload.h"
#include "bgrender.h"
#include "edittool.h"
typedef void *HWND;
typedef intptr_t LPARAM;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef struct { int left,top,right,bottom; } RECT;
#include "types.inc"
enum { VIEWPORT_WM_PICK_PAD, VIEWPORT_WM_SELECTION_CHANGED };
typedef struct ViewportState {
    const SetupFile *markersetup;
    ViewportPad *pads;
    Vertex *padmarkers;
    DWORD padcount;
    SetupPadRef selectedpad;
    EditorTool tool;
    BOOL showobjects,showstan,padpick,flying,gizmovisible;
    int width,height,dragaxis,stanopacity;
    float posx,posy,posz,yaw,pitch;
    double selectionfar,gizmoposition[3];
} ViewportState;
static double scenedistance=DBL_MAX,standistance=DBL_MAX;
static unsigned notifications,picks;
static BOOL ViewportPadSelectionPosition(const ViewportState *,double[3],BOOL);
static BOOL GetClientRect(HWND hwnd,RECT *r)
{ ViewportState *s=hwnd;*r=(RECT){0,0,s->width,s->height};return TRUE; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static void InvalidateRect(HWND hwnd,const RECT *r,BOOL erase) {}
static void ViewportRefreshPadColors(ViewportState *s) {}
static void ViewportClearAllSelection(ViewportState *s) { s->selectedpad.index=SETUP_PAD_INDEX_NONE; }
static void ViewportUpdateGizmo(ViewportState *s)
{ s->gizmovisible=ViewportPadSelectionPosition(s,s->gizmoposition,TRUE); }
static intptr_t SendMessage(HWND hwnd,unsigned msg,DWORD wparam,LPARAM lparam)
{ if(msg==VIEWPORT_WM_PICK_PAD)picks++;else { assert(msg==VIEWPORT_WM_SELECTION_CHANGED);notifications++; } return TRUE; }
static BOOL ViewportPadPosition(const ViewportState *s,const SetupPadRef *ref,BOOL preview,double p[3])
{ for(int a=0;a<3;a++)p[a]=s->pads[0].position[a];return TRUE; }
static double ViewportSceneHitDistance(const ViewportState *s,const ViewportPickRay *ray) { return scenedistance; }
static DWORD ViewportFindPickedStan(const ViewportState *s,const ViewportPickRay *ray,double *distance)
{ *distance=s->showstan?s->stanopacity?standistance:DBL_MAX:DBL_MAX;return 0; }
#include "selection.inc"

static void Box(Vertex *out,float minz,float maxz)
{
    static const unsigned char edges[12][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for(int e=0;e<12;e++)for(int end=0;end<2;end++)
    {
        unsigned c=edges[e][end];
        out[e*2+end]=(Vertex){.x=c&1?20:-20,.y=c&2?20:-20,.z=c&4?maxz:minz};
    }
}
int main(void)
{
    SetupObject object={.type=PROPDEF_PROP,.pad=10000};
    SetupFile setup={.objects=&object,.objectcount=1,.boundpadcount=1};
    ViewportPad pad={.ref={0,TRUE},.position={4000,2000,-6000}};
    Vertex vertices[24];Box(vertices,-100,-100); /* flat red rectangle */
    ViewportState s={.markersetup=&setup,.pads=&pad,.padmarkers=vertices,.padcount=1,
        .selectedpad={SETUP_PAD_INDEX_NONE,FALSE},.tool=EDITOR_TOOL_FACE_SELECT,.showobjects=TRUE,
        .width=400,.height=300,.dragaxis=-1,.stanopacity=44};
    assert(ViewportPadHasMissingModel(&s,0));
    assert(ViewportTryPickPad(&s,&s,200,150,FALSE)); /* well away from every wire edge */
    assert(s.selectedpad.bound && s.selectedpad.index==0 && s.gizmovisible && notifications==1);
    assert(fabs(s.gizmoposition[0])<1e-8 && fabs(s.gizmoposition[1])<1e-8 && fabs(s.gizmoposition[2]+100)<1e-8);
    double p[3];assert(ViewportPadSelectionPosition(&s,p,FALSE) && p[0]==4000 && p[1]==2000 && p[2]==-6000);
    for(int i=0;i<24;i++) { vertices[i].x+=15;vertices[i].y+=7; }
    assert(ViewportPadSelectionPosition(&s,p,TRUE) && fabs(p[0]-15)<1e-8 && fabs(p[1]-7)<1e-8);
    Box(vertices,-100,-100); /* The preview's gizmo follows the box, not its distant anchor. */
    assert(ViewportTryPickPad(&s,&s,200,150,TRUE) && !s.gizmovisible && s.selectedpad.index==SETUP_PAD_INDEX_NONE);
    /* A translucent stan does not write depth and must not hide a visible box from picking. */
    s.showstan=TRUE;standistance=50;
    assert(ViewportTryPickPad(&s,&s,200,150,FALSE));
    Vertex point={.z=-100};assert(!ViewportStanComponentVisible(&s,&point)); /* stan vertex picking unchanged */
    ViewportClearAllSelection(&s);s.stanopacity=100;
    assert(!ViewportTryPickPad(&s,&s,200,150,FALSE));
    s.showstan=FALSE;scenedistance=50;
    assert(!ViewportTryPickPad(&s,&s,200,150,FALSE));scenedistance=DBL_MAX;
    /* Only live, missing models receive interior picking. Free/occupied pads keep wire picking. */
    object.deleted=TRUE;assert(!ViewportTryPickPad(&s,&s,200,150,FALSE));object.deleted=FALSE;
    pad.occupied=TRUE;assert(!ViewportPadHasMissingModel(&s,0));
    assert(!ViewportTryPickPad(&s,&s,200,150,FALSE));pad.occupied=FALSE;
    s.markersetup=NULL;assert(!ViewportTryPickPad(&s,&s,200,150,FALSE));
    double screen[2];assert(ViewportProject(&s,vertices,screen));
    assert(ViewportTryPickPad(&s,&s,(int)screen[0],(int)screen[1],FALSE));
    s.markersetup=&setup;s.padpick=TRUE;assert(!ViewportTryPickPad(&s,&s,200,150,FALSE) && !picks);s.padpick=FALSE;
    s.flying=TRUE;assert(!ViewportTryPickPad(&s,&s,200,150,FALSE));s.flying=FALSE;
    pad.deleted=TRUE;assert(!ViewportTryPickPad(&s,&s,200,150,FALSE));pad.deleted=FALSE;
    /* An actual pad edge crossing behind the eye remains selectable where GL clips it. */
    object.deleted=TRUE;
    for(int i=0;i<24;i++)vertices[i]=(Vertex){.x=20,.z=i&1?-100:10};
    point=(Vertex){.x=20,.z=-50};assert(ViewportProject(&s,&point,screen));
    assert(ViewportTryPickPad(&s,&s,(int)screen[0],(int)screen[1],FALSE));
    Vertex a={.x=20,.z=10},b={.x=20,.z=20};
    assert(!ViewportPadEdgePoint(&s,&a,&b,200,150,&point,screen));
    a.z=-100;b.z=-2*VIEWPORT_FAR_Z;
    assert(ViewportPadEdgePoint(&s,&a,&b,200,150,&point,screen));
    /* Occupied pads and ordinary navigation pads still use the authored gizmo anchor. */
    pad.occupied=TRUE;assert(ViewportPadSelectionPosition(&s,p,TRUE) && p[0]==4000);
    pad.occupied=FALSE;object.deleted=FALSE;
    for(int i=0;i<24;i++)vertices[i].x=NAN;
    assert(!ViewportTryPickPad(&s,&s,200,150,FALSE)); /* malformed invisible boxes cannot steal clicks */
    puts("PASS: flat fallback-box interior/outline selection, visible gizmo despite offset anchor, Ctrl-deselect, translucent/opaque stan and wall occlusion, clipped edges, deleted/occupied/free pads and input scope.");
}
