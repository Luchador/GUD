/* Production room selection and preview with only window-system calls stubbed. */
#include "edittool.h"
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
#define VIEWPORT_BOX_VERTICES 24
#define VIEWPORT_OBJECT_NONE ((DWORD)-1)
#define VIEWPORT_WM_TRANSFORM_PREVIEW 1
#define VIEWPORT_WM_SELECTION_CHANGED 2
#include "viewtypes.inc"
typedef struct ViewportState {
    EditorTool tool;
    DWORD selectedroom;
    const SetupFile *markersetup;
    float markerlevelscale;
    Vertex *scene, *padmarkers;
    int scenecount, selectedtricount, padmarkercount;
    BgFaceRef *scenefacerefs;
    DWORD *sceneobjectindices;
    unsigned char *selectedtris, *stanselected;
    unsigned char portalselection[BG_MAX_PORTALS];
    StanFile stan;
    BgPortalFile portals;
    ViewportPad *pads;
    DWORD padcount;
    BOOL flying, dragroom;
    int dragaxis;
    double dragdelta;
    ViewportRoomDragPoint *roomdragpoints;
    size_t roomdragcount;
} ViewportState;
static HWND captured;
static int hittriangle, notifications;
static ViewportState *ViewportGetState(HWND h) { return h; }
static void ViewportCancelTransform(HWND h) { assert(((ViewportState *)h)->dragaxis < 0); }
static void ViewportClearAllSelection(ViewportState *s)
{
    s->selectedroom=0; s->selectedtricount=0;
    memset(s->selectedtris,0,s->scenecount/3);
}
static void ViewportSetTriangleColor(ViewportState *s, int i, BOOL selected) {}
static void ViewportRefreshStanOverlay(ViewportState *s) {}
static void ViewportRefreshPortalColors(ViewportState *s) {}
static void ViewportRefreshPortalGeometry(ViewportState *s) {}
static void ViewportUpdateGizmo(ViewportState *s) {}
static void ViewportRedraw(HWND h) {}
static void InvalidateRect(HWND h, void *rect, BOOL erase) {}
static HWND GetParent(HWND h) { return h; }
static HWND GetCapture(void) { return captured; }
static void ReleaseCapture(void) { captured=NULL; }
static void SendMessage(HWND h, unsigned int msg, WPARAM w, LPARAM l) { notifications++; }
static BOOL ViewportBuildPickRay(HWND h, ViewportState *s, int x, int y, ViewportPickRay *ray)
{ memset(ray,0,sizeof(*ray)); return TRUE; }
static int ViewportFindVisibleSceneTriangle(const ViewportState *s, const ViewportPickRay *ray, double *distance)
{ *distance=100; return hittriangle; }
#include "viewlogic.inc"

static void ViewportTest(SetupFile *setup, StanFile *stan, BgDocument *bg, DWORD prop, DWORD other)
{
    Vertex scene[12]={0}, original[12];
    unsigned char selected[4]={0}, tiles[3]={0};
    BgFaceRef faces[4]={{.faceid=1,.room=1,.layer=0},{.faceid=2,.room=1,.layer=1},{0},{0}};
    DWORD models[4]={VIEWPORT_OBJECT_NONE,VIEWPORT_OBJECT_NONE,prop,other};
    for(int i=0;i<12;i++) { scene[i].x=(float)i; scene[i].y=20; scene[i].z=3; }
    memcpy(original,scene,sizeof(scene));
    ViewportState s={.tool=EDITOR_TOOL_ROOM_SELECT,.markersetup=setup,.markerlevelscale=.5f,
        .scene=scene,.scenecount=12,.scenefacerefs=faces,.sceneobjectindices=models,.selectedtris=selected,
        .stanselected=tiles,.stan=*stan,.portals=bg->portals,.dragaxis=-1};
    hittriangle=2; ViewportPickRoomAt(&s,&s,0,0,FALSE); /* Click an object. */
    assert(ViewportGetSelectedRoom(&s)==1 && selected[0] && selected[1] && selected[2] && !selected[3]);
    assert(tiles[0] && !tiles[1] && !tiles[2]);
    assert(s.portalselection[0] && s.portalselection[1] && !s.portalselection[2]);
    double center[3]; assert(ViewportRoomPosition(&s,center)); Close((float)center[0],4);
    hittriangle=0; ViewportPickRoomAt(&s,&s,0,0,FALSE); /* Click a BG face. */
    assert(ViewportGetSelectedRoom(&s)==1);
    s.dragroom=TRUE; s.dragaxis=1; captured=&s;
    float floory=stan->tiles[0].points[0].y, portaly=bg->portals.portals[0].points[0].y;
    OK(ViewportBeginRoomDrag(&s));
    ViewportPreviewRoomDrag(&s,&s,21);
    assert(s.dragdelta==22); Close(scene[0].y,42); Close(scene[6].y,42); Close(scene[9].y,20);
    Close(stan->tiles[0].points[0].y,floory+22); Close(bg->portals.portals[0].points[0].y,portaly+22);
    ViewportPreviewRoomDrag(&s,&s,5); /* Absolute-from-press, never cumulative. */
    Close(scene[0].y,26); Close(stan->tiles[0].points[0].y,floory+6);
    ViewportCancelRoomDrag(&s,&s);
    assert(!captured && !s.dragroom && !s.roomdragpoints && s.dragaxis==-1);
    assert(!memcmp(original,scene,sizeof(scene))); Close(stan->tiles[0].points[0].y,floory);
    Close(bg->portals.portals[0].points[0].y,portaly);
    ViewportPickRoomAt(&s,&s,0,0,TRUE); assert(!ViewportGetSelectedRoom(&s));
    hittriangle=-1; ViewportPickRoomAt(&s,&s,0,0,FALSE); assert(!s.selectedroom);
    assert(notifications>=5);
    puts("PASS room viewport: BG/object clicks, both layers, mixed selection, absolute preview and cancel");
}
