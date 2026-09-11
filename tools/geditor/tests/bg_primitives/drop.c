#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bgdocument.h"
#include "edittool.h"
#include "stanload.h"
typedef void *HWND;
typedef struct { int x,y; } POINT;
typedef struct { int left,top,right,bottom; } RECT;
typedef struct ViewportState {
    BOOL flying,orbit;
    int dragaxis,scenecount,selectedtricount;
    float yaw,pitch,posx,posy,posz;
    EditorTool tool;
    BgFaceRef *scenefacerefs;
    unsigned char *selectedtris,*hiddentris;
} ViewportState;
#define VIEWPORT_WM_SELECTION_CHANGED 1
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static BOOL GetClientRect(HWND hwnd,RECT *r) { *r=(RECT){0,0,201,201}; return TRUE; }
static void ScreenToClient(HWND hwnd,POINT *p) { p->x-=100; p->y-=200; }
static BOOL PtInRect(const RECT *r,POINT p) { return p.x>=r->left && p.y>=r->top && p.x<r->right && p.y<r->bottom; }
#include "ray.inc"
static double surface=DBL_MAX, floorhit=DBL_MAX;
static int ViewportFindVisibleSceneTriangle(const ViewportState *s,const ViewportPickRay *ray,double *distance)
{ *distance=surface; return surface==DBL_MAX?-1:0; }
static DWORD ViewportFindPickedStan(const ViewportState *s,const ViewportPickRay *ray,double *distance)
{ *distance=floorhit; return floorhit==DBL_MAX?STAN_TILE_NONE:0; }
#include "drop.inc"
static unsigned notifications;
static void ViewportClearAllSelection(ViewportState *s)
{ memset(s->selectedtris,0,s->scenecount/3); s->selectedtricount=0; }
static void ViewportSetTriangleColor(ViewportState *s,int triangle,BOOL selected) { assert(selected); }
static void ViewportUpdateGizmo(ViewportState *s) {}
static void InvalidateRect(HWND hwnd,const RECT *r,BOOL erase) {}
static HWND GetParent(HWND hwnd) { return hwnd; }
static void SendMessage(HWND hwnd,int message,int wparam,int lparam) { notifications++; }
#include "select.inc"

int main(void)
{
    BgFaceRef faces[3]={{7,2,0,0},{9,1,0,0},{8,1,0,0}};
    unsigned char selected[3]={0}, hidden[3]={0};
    ViewportState s={.dragaxis=-1,.posx=10,.posy=20,.posz=30,.scenefacerefs=faces,
        .scenecount=9,.selectedtris=selected,.hiddentris=hidden,.tool=EDITOR_TOOL_FACE_SELECT};
    double p[3],right[3]; DWORD room; POINT center={200,300};
    assert(ViewportGetPrimitiveDrop(&s,center,p,right,&room));
    assert(!room && p[0]==10 && p[1]==20 && p[2]==-970 && right[0]==1 && !right[1] && !right[2]);
    /* Off-centre void drop follows the cursor, at exactly 10m radial distance,
       even with a pitched/yawed camera. Geometry's up axis stays world Y. */
    s.yaw=65; s.pitch=40;
    assert(ViewportGetPrimitiveDrop(&s,(POINT){250,260},p,right,&room));
    assert(fabs(sqrt(pow(p[0]-10,2)+pow(p[1]-20,2)+pow(p[2]-30,2))-1000)<1e-8);
    assert(!right[1] && fabs(hypot(right[0],right[2])-1)<1e-6);
    s.yaw=s.pitch=0; surface=250;
    assert(ViewportGetPrimitiveDrop(&s,center,p,right,&room) && room==2 && p[2]==-220);
    floorhit=100;
    assert(ViewportGetPrimitiveDrop(&s,center,p,right,&room) && !room && p[2]==-70);
    floorhit=300;
    assert(ViewportGetPrimitiveDrop(&s,center,p,right,&room) && room==2);
    faces[0].faceid=BG_FACE_ID_NONE;
    assert(ViewportGetPrimitiveDrop(&s,center,p,right,&room) && !room); /* Model surface. */
    faces[0].faceid=7;
    assert(!ViewportGetPrimitiveDrop(&s,(POINT){99,300},p,right,&room));
    assert(!ViewportGetPrimitiveDrop(&s,(POINT){301,300},p,right,&room));
    s.flying=TRUE; assert(!ViewportGetPrimitiveDrop(&s,center,p,right,&room)); s.flying=FALSE;
    s.orbit=TRUE; assert(!ViewportGetPrimitiveDrop(&s,center,p,right,&room)); s.orbit=FALSE;
    s.dragaxis=0; assert(!ViewportGetPrimitiveDrop(&s,center,p,right,&room)); s.dragaxis=-1;
    /* Stable identities select both new faces despite draw reordering. */
    BgFaceRef added[2]={faces[2],faces[1]};
    selected[0]=1; s.selectedtricount=1;
    assert(ViewportSelectBgFaces(&s,added,2) && s.selectedtricount==2 && !selected[0] && selected[1] && selected[2]);
    assert(notifications==1); hidden[2]=1;
    assert(!ViewportSelectBgFaces(&s,added,2) && s.selectedtricount==2 && notifications==1);
    hidden[2]=0; added[0].faceid=999;
    assert(!ViewportSelectBgFaces(&s,added,2) && s.selectedtricount==2 && notifications==1);
    puts("PASS: surface/void cursor placement, room hints, upright orientation, bounds, and new-face selection.");
    return 0;
}
