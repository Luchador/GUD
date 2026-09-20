#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cameraframe.h"
#include "bgdocument.h"
#include "edittool.h"
#include "stanload.h"
#include "setupload.h"

typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef void *HWND;
typedef intptr_t LPARAM;
typedef struct { LONGLONG QuadPart; } LARGE_INTEGER;
typedef struct { HWND hwnd; unsigned message; uintptr_t wParam; LPARAM lParam; } MSG;
#include "types.inc"
typedef struct ViewportState {
    BOOL padpick;
    BOOL orbit, flying, boxpending, showbgprimary, showbgsecondary, showobjects, showstan, showportals;
    BOOL markerselected, padpreview, zooming;
    EditorTool tool;
    int width, height, dragaxis, scenecount, batchcount, componentcount, stancomponentcount, stanopacity;
    int objectselectionboxcount, padmarkercount;
    float posx, posy, posz, yaw, pitch;
    double selectionfar;
    CameraFrame zoomframe;
    LARGE_INTEGER zoomstart, zoomfrequency;
    Vertex *scene, *padmarkers, objectselectionbox[VIEWPORT_BOX_VERTICES];
    SceneBatch *batches;
    ViewportComponent *components;
    unsigned char *selectedtris, *hiddentris, *stanselected;
    DWORD *selectedobjects, selectedobjectcount;
    DWORD *sceneobjectindices, selectedobject, *stanpointmap, *stanhiddenids, stanhiddencount;
    StanFile stan;
    ViewportStanComponent *stancomponents;
    BgPortalFile portals;
    unsigned char portalselection[BG_MAX_PORTALS];
    SetupPadRef selectedpad;
    ViewportPad *pads;
    DWORD padcount, setupmarkercount, markermodeltris[SETUP_MARKER_KIND_COUNT];
    BgVertex *markermodels[SETUP_MARKER_KIND_COUNT];
    SetupMarker *setupmarkers;
    SetupMarkerRef selectedmarker;
    SetupSwirlPath swirlpath;
} ViewportState;
static LONGLONG clockticks;
static int timer, repaints, commands;
static BOOL failtimer;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static BOOL QueryPerformanceCounter(LARGE_INTEGER *out) { out->QuadPart = clockticks; return TRUE; }
static BOOL QueryPerformanceFrequency(LARGE_INTEGER *out) { out->QuadPart = 1000000; return TRUE; }
static int SetTimer(HWND h, int id, int interval, void *callback) { return failtimer ? 0 : (timer = id); }
static void KillTimer(HWND h, int id) { assert(id == timer); timer = 0; }
static void InvalidateRect(HWND h, void *r, BOOL erase) { repaints++; }
static void ViewportResizeGL(ViewportState *s, int w, int h) {}
static void ViewportRefreshStanOverlay(ViewportState *s) {}
#include "logic.inc"
enum { WM_SIZE = 1, WM_LBUTTONDOWN, WM_LBUTTONDBLCLK, WM_RBUTTONDOWN, WM_RBUTTONDBLCLK,
       WM_MBUTTONDOWN, WM_MOUSEWHEEL, WM_CANCELMODE, WM_CAPTURECHANGED, WM_KILLFOCUS,
       WM_DESTROY, WM_KEYDOWN, WM_MOUSEMOVE, WM_COMMAND, VK_ESCAPE, VK_CONTROL, VK_SHIFT, VK_MENU,
       ID_VIEW_ZOOM_SELECTED };
static void DispatchCancel(HWND hwnd, unsigned msg, unsigned wparam)
{
    ViewportState *state = hwnd;
#include "cancel.inc"
}
static HWND g_Viewport;
static int modifiers;
static const char *inputclass = "GEditorViewport";
static BOOL ViewportIsFlying(HWND h) { return ((ViewportState *)h)->flying; }
static BOOL ViewportIsTransforming(HWND h) { return ((ViewportState *)h)->dragaxis >= 0; }
static BOOL IsChild(HWND parent, HWND child) { return child == g_Viewport; }
static int GetKeyState(int key) { return key == modifiers ? 0x8000 : 0; }
static void GetClassName(HWND h, char *out, size_t size) { snprintf(out, size, "%s", inputclass); }
#define lstrcmpi strcasecmp
static void SendMessage(HWND h, unsigned message, unsigned command, LPARAM data)
{ assert(message == WM_COMMAND && command == ID_VIEW_ZOOM_SELECTED); commands++; }
#include "hotkey.inc"

static ViewportState Empty(void)
{
    ViewportState s = {0};
    s.width = 1280; s.height = 720; s.dragaxis = -1; s.stanopacity = 44;
    s.tool = EDITOR_TOOL_FACE_SELECT; s.showbgprimary = s.showobjects = TRUE;
    s.selectedobject = VIEWPORT_OBJECT_NONE; s.selectedpad.index = SETUP_PAD_INDEX_NONE;
    return s;
}

static void TestFraming(void)
{
    const double aspect[] = {0.25, 0.75, 1, 16.0/9, 4};
    const double pitch[] = {-89, -35, 0, 40, 89}, yaw[] = {0, 30, 90, 180, 270};
    const double halfsize[][3] = {{0,0,0}, {200,300,0}, {2000,10,10}, {10000,15000,300000}};
    double center[] = {321, -500, -1234}, eye[] = {123, 456, 30000};
    for (unsigned a = 0; a < sizeof(aspect)/sizeof(*aspect); a++)
    for (unsigned p = 0; p < sizeof(pitch)/sizeof(*pitch); p++)
    for (unsigned y = 0; y < sizeof(yaw)/sizeof(*yaw); y++)
    for (unsigned size = 0; size < sizeof(halfsize)/sizeof(*halfsize); size++)
    {
        CameraFrame f;
        double min[3], max[3], position[3], radians = acos(-1.0)/180.0;
        double cy = cos(yaw[y]*radians), sy = sin(yaw[y]*radians);
        double cp = cos(pitch[p]*radians), sp = sin(pitch[p]*radians);
        for (int axis = 0; axis < 3; axis++)
        { min[axis] = center[axis] - halfsize[size][axis]; max[axis] = center[axis] + halfsize[size][axis]; }
        assert(CameraFrameBegin(&f, min, max, eye, yaw[y], pitch[p], aspect[a], 60, 10));
        assert(CameraFrameSample(&f, f.duration + 1, position));
        assert(!memcmp(position, f.end, sizeof(position)));
        for (int corner = 0; corner < 8; corner++)
        {
            double x = ((corner & 1) ? max[0] : min[0]) - position[0];
            double yv = ((corner & 2) ? max[1] : min[1]) - position[1];
            double z = ((corner & 4) ? max[2] : min[2]) - position[2];
            /* Apply the viewport's inverse yaw then inverse pitch rotations. */
            double viewx = x*cy - z*sy, yawz = x*sy + z*cy;
            double viewy = yv*cp + yawz*sp, depth = yv*sp - yawz*cp;
            assert(depth >= 20 - 1e-8 && depth < f.farclip);
            assert(fabs(viewx)/(depth*tan(30*radians)*aspect[a]) <= 1/1.1 + 1e-8);
            assert(fabs(viewy)/(depth*tan(30*radians)) <= 1/1.1 + 1e-8);
        }
        assert(!CameraFrameSample(&f, -1, position));
        assert(!memcmp(position, eye, sizeof(eye)));
        for (int tick = 0; tick <= 100; tick++)
        {
            CameraFrameSample(&f, f.duration*tick/100.0, position);
            for (int axis = 0; axis < 3; axis++)
            {
                assert(position[axis] >= fmin(eye[axis], f.end[axis]) - 1e-8);
                assert(position[axis] <= fmax(eye[axis], f.end[axis]) + 1e-8);
            }
        }
    }
    {
        CameraFrame f, saved;
        double point[3] = {0}, position[3];
        const double meters[] = {0,5,10,55,100,500}, seconds[] = {0.5,0.5,0.5,1,1.5,1.5};
        for (unsigned i = 0; i < sizeof(meters)/sizeof(*meters); i++)
        {
            double from[3] = {0,0,meters[i]*100};
            assert(CameraFrameBegin(&f, point, point, from, 0, 0, 1, 60, 10));
            assert(fabs(f.duration - seconds[i]) < 1e-12);
            CameraFrameSample(&f, f.duration/2, position);
            assert(position[2] == (from[2] + f.end[2])*0.5);
            CameraFrameSample(&f, f.duration*0.25, position);
            assert(fabs(position[2] - (from[2]*0.84375 + f.end[2]*0.15625)) < 1e-9);
        }
        saved = f;
        assert(!CameraFrameBegin(&f, point, point, eye, 0, 0, 0, 60, 10));
        assert(!memcmp(&f, &saved, sizeof(f)));
        point[0] = NAN;
        assert(!CameraFrameBegin(&f, point, point, eye, 0, 0, 1, 60, 10));
    }
}

static void TestSelection(void)
{
    ViewportState s = Empty();
    double min[3], max[3];
    Vertex scene[9] = {{.x=-10}, {.x=10,.y=30}, {.z=-20},
                       {.x=10000}, {.x=20000}, {.x=30000},
                       {.x=400,.y=50}, {.x=600,.y=100}, {.x=500,.z=-300}};
    SceneBatch batches[] = {{.first=0,.count=3}, {.first=3,.count=3,.secondary=TRUE}, {.first=6,.count=3,.object=TRUE}};
    DWORD objects[] = {VIEWPORT_OBJECT_NONE, VIEWPORT_OBJECT_NONE, 0x80000002};
    unsigned char selected[] = {1,1,0}, hidden[] = {0,0,0};
    ViewportComponent component = {.corners={0,1}};
    assert(!ViewportCanZoomToSelected(&s));
    s.scene=scene; s.scenecount=9; s.batches=batches; s.batchcount=3;
    s.selectedtris=selected; s.hiddentris=hidden; s.sceneobjectindices=objects;
    assert(ViewportSelectionBounds(&s,min,max) && min[0]==-10 && max[0]==10 && min[2]==-20);
    s.showbgsecondary=TRUE;
    assert(ViewportSelectionBounds(&s,min,max) && max[0]==30000);
    hidden[1]=1;
    assert(ViewportSelectionBounds(&s,min,max) && max[0]==10);
    s.tool=EDITOR_TOOL_VERTEX_SELECT; s.components=&component; s.componentcount=1;
    assert(ViewportSelectionBounds(&s,min,max) && min[0]==-10 && max[0]==-10);
    s.tool=EDITOR_TOOL_EDGE_SELECT;
    assert(ViewportSelectionBounds(&s,min,max) && min[0]==-10 && max[0]==10 && max[1]==30);
    s.componentcount=0; s.selectedobject=objects[2];
    assert(ViewportSelectionBounds(&s,min,max) && min[0]==400 && max[0]==600 && min[2]==-300);
    DWORD group[] = {4,0x80000002};
    objects[1]=4; s.selectedobjects=group; s.selectedobjectcount=2;
    assert(ViewportSelectionBounds(&s,min,max) && min[0]==400 && max[0]==30000);
    s.selectedobjectcount=0; s.selectedobjects=NULL; objects[1]=VIEWPORT_OBJECT_NONE;
    s.showobjects=FALSE;
    assert(!ViewportSelectionBounds(&s,min,max));
    s=Empty();
    {
        StanTile tiles[2] = {{.editorid=10,.pointcount=3,.points={{-100,0,0,0},{-200,0,0,0},{-100,0,-100,0}}},
                            {.editorid=11,.pointcount=3,.points={{-100,0,0,0},{-200,0,0,0},{-150,0,200,0}}}};
        unsigned char chosen[]={1,1}; DWORD hiddenid=10, map[20]={0};
        ViewportStanComponent edge = {.refs={{0,0},{0,1}}};
        s.showstan=TRUE; s.stan.tiles=tiles; s.stan.tilecount=2; s.stanselected=chosen;
        s.stanhiddenids=&hiddenid; s.stanhiddencount=1; s.stanpointmap=map;
        assert(ViewportSelectionBounds(&s,min,max) && min[2]==0 && max[2]==200);
        /* The visible edge's canonical points are on the hidden neighbor. */
        map[10]=0; map[11]=1; map[12]=12;
        s.tool=EDITOR_TOOL_EDGE_SELECT; s.stancomponents=&edge; s.stancomponentcount=1;
        assert(ViewportSelectionBounds(&s,min,max) && min[0]==-200 && max[0]==-100 && max[2]==0);
        s.stanopacity=0;
        assert(!ViewportSelectionBounds(&s,min,max));
    }
    s=Empty();
    {
        BgPortal portal={.pointcount=4,.points={{-50,0,0},{50,0,0},{50,100,0},{-50,100,0}}};
        s.portals.portals=&portal; s.portals.portalcount=1; s.showportals=TRUE; s.portalselection[0]=1;
        assert(ViewportSelectionBounds(&s,min,max) && min[0]==-50 && max[1]==100);
        s.tool=EDITOR_TOOL_EDGE_SELECT;
        assert(ViewportSelectionBounds(&s,min,max) && max[0]==50 && max[1]==0);
        s.tool=EDITOR_TOOL_VERTEX_SELECT;
        assert(ViewportSelectionBounds(&s,min,max) && min[0]==-50 && max[0]==-50);
    }
    s=Empty();
    {
        ViewportPad pad={.ref={1,FALSE},.position={100,200,300},.previewposition={100,0,300}};
        s.pads=&pad; s.padcount=1; s.selectedpad=pad.ref;
        assert(ViewportSelectionBounds(&s,min,max) && min[1]==200 && max[2]==300);
        s.padpreview=TRUE;
        assert(ViewportSelectionBounds(&s,min,max) && min[1]==0);
        pad.deleted=TRUE;
        assert(!ViewportSelectionBounds(&s,min,max));
    }
    s=Empty();
    {
        SetupMarker marker={.kind=SETUP_MARKER_INTRO,.command=3,.position={300,400,500},.look={0,0,-1},.up={0,1,0}};
        BgVertex model[3]={[0]={.x=2,.y=1,.z=3}};
        s.setupmarkers=&marker; s.setupmarkercount=1; s.markerselected=TRUE;
        s.selectedmarker=(SetupMarkerRef){marker.kind,marker.command};
        s.markermodels[marker.kind]=model; s.markermodeltris[marker.kind]=1;
        assert(ViewportSelectionBounds(&s,min,max) && min[2]==300 && max[0]==600 && max[1]==500);
        s.orbit=TRUE; assert(!ViewportCanZoomToSelected(&s)); s.orbit=FALSE;
        s.flying=TRUE; assert(!ViewportCanZoomToSelected(&s)); s.flying=FALSE;
        s.dragaxis=0; assert(!ViewportCanZoomToSelected(&s)); s.dragaxis=-1;
        s.width=0; assert(!ViewportCanZoomToSelected(&s));
    }
}

static void TestAnimationAndInput(void)
{
    ViewportState s=Empty();
    ViewportPad pad={.ref={1,FALSE},.position={0,0,0}};
    HWND frame=(HWND)(uintptr_t)1;
    MSG message={.hwnd=&s,.message=WM_KEYDOWN,.wParam='Z'};
    s.pads=&pad; s.padcount=1; s.selectedpad=pad.ref; s.posz=5500;
    assert(ViewportZoomToSelected(&s) && s.zooming && timer==VIEWPORT_ZOOM_TIMER);
    assert(s.zoomframe.duration==1 && s.posz==5500);
    clockticks=500000; ViewportZoomFrame(&s,&s);
    assert(s.posz==2775 && s.zooming && repaints);
    pad.position[0]=1000;
    assert(ViewportZoomToSelected(&s) && s.zoomframe.start[2]==2775);
    clockticks=3000000; ViewportZoomFrame(&s,&s);
    assert(s.posx==1000 && s.posz==50 && !s.zooming && !timer);
    assert(s.yaw==0 && s.pitch==0);
    for (unsigned msg=WM_SIZE; msg<=WM_KEYDOWN; msg++)
    {
        assert(ViewportZoomToSelected(&s));
        DispatchCancel(&s,WM_MOUSEMOVE,0); assert(s.zooming);
        DispatchCancel(&s,msg,VK_ESCAPE); assert(!s.zooming && !timer);
    }
    failtimer=TRUE; assert(!ViewportZoomToSelected(&s) && !s.zooming); failtimer=FALSE;
    g_Viewport=&s;
    assert(GEditorHandleZoomSelectedHotkey(frame,&message) && commands==1);
    message.lParam=(LPARAM)1<<30;
    assert(GEditorHandleZoomSelectedHotkey(frame,&message) && commands==1);
    message.lParam=0;
    for (int key=VK_CONTROL; key<=VK_MENU; key++)
    { modifiers=key; assert(!GEditorHandleZoomSelectedHotkey(frame,&message)); }
    modifiers=0;
    const char *classes[]={"Edit","ComboBox","ComboLBox"};
    for (int i=0;i<3;i++) { inputclass=classes[i]; assert(!GEditorHandleZoomSelectedHotkey(frame,&message)); }
    inputclass="GEditorViewport"; message.hwnd=(HWND)(uintptr_t)2;
    assert(!GEditorHandleZoomSelectedHotkey(frame,&message));
    message.hwnd=&s; s.flying=TRUE; assert(!GEditorHandleZoomSelectedHotkey(frame,&message));
    s.flying=FALSE; s.dragaxis=0; assert(!GEditorHandleZoomSelectedHotkey(frame,&message));
    assert(commands==1);
}

static void TestIssueFraming(void)
{
    ViewportState s=Empty();
    StanTile tiles[3]={{.editorid=10},{.editorid=20},{.editorid=30}};
    DWORD hidden[3]={10,20,30};
    s.stan.tiles=tiles;s.stan.tilecount=3;s.stanhiddenids=hidden;s.stanhiddencount=3;
    assert(ViewportRevealStanTile(&s,1) && s.stanhiddencount==2 && hidden[0]==10 && hidden[1]==30);
    assert(ViewportRevealStanTile(&s,1) && s.stanhiddencount==2);
    assert(!ViewportRevealStanTile(&s,3) && !ViewportRevealStanTile(NULL,0));
    const double min[]={100,-10,200},max[]={150,10,250};
    assert(!ViewportCanZoomToSelected(&s)); /* A report can frame a location without geometry. */
    assert(ViewportZoomToBounds(&s,min,max) && s.zooming);
    ViewportCancelZoom(&s,&s);
    s.flying=TRUE;assert(!ViewportZoomToBounds(&s,min,max));s.flying=FALSE;
    s.dragaxis=0;assert(!ViewportZoomToBounds(&s,min,max));s.dragaxis=-1;
    s.width=0;assert(!ViewportZoomToBounds(&s,min,max));
}

int main(void)
{
    TestFraming(); TestSelection(); TestAnimationAndInput(); TestIssueFraming();
    puts("Zoom to Selected: framing, timing, selections, cancellation and shortcut checks passed.");
    return 0;
}
