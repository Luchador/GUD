#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "bgdocument.h"
#include "stanload.h"
#include "edittool.h"

typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef int HWND;
typedef struct { int x, y; } POINT;
typedef struct { int left, top, right, bottom; } RECT;
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define SM_CXDRAG 0
#define SM_CYDRAG 1
#define MB_ICONERROR 0
#define VIEWPORT_WM_SELECTION_CHANGED 1
#include "types.inc"

typedef struct ViewportState {
    double selectionfar;
    EditorTool tool;
    int width, height, scenecount, batchcount;
    float posx, posy, posz, yaw, pitch;
    BOOL showbgprimary, showbgsecondary, showstan;
    int stanopacity;
    Vertex *scene;
    BgDocumentVertexRef *scenevertexrefs;
    SceneBatch *batches;
    unsigned char *hiddentris;
    unsigned char *selectedtris, *stanselected;
    BgFaceRef *scenefacerefs;
    int selectedtricount;
    StanFile stan;
    DWORD *stanhiddenids, stanhiddencount;
    DWORD *stanpointmap;
    ViewportComponent *components;
    int componentcount, componentcapacity;
    ViewportStanComponent *stancomponents;
    int stancomponentcount, stancomponentcapacity;
    BOOL boxpending, boxdragging, boxadd, boxremove;
    POINT boxstart, boxend;
    int hoveraxis;
    BOOL showportals; BgPortalFile portals; DWORD selectedportal;
    unsigned char portalselection[BG_MAX_PORTALS];
} ViewportState;

static HWND capture;
static ViewportState *capturedstate;
static int clicks, notifications, faceclicks, errors;
static int picktarget, pickcalls;
static BOOL clickadd, clickremove;
static HWND GetCapture(void) { return capture; }
static void SetCapture(HWND hwnd) { capture = hwnd; }
static void ReleaseCapture(void)
{
    /* Win32 can synchronously notify capture loss; cancellation must already
       be committed before that notification can reenter viewport code. */
    assert(!capturedstate->boxpending && !capturedstate->boxdragging);
    capture = 0;
}
static int GetSystemMetrics(int index) { return 4; }
static void InvalidateRect(HWND hwnd, const RECT *rect, BOOL erase) {}
static HWND GetParent(HWND hwnd) { return 2; }
static void SendMessage(HWND hwnd, int msg, int wparam, int lparam) { notifications++; }
static void MessageBox(HWND hwnd, const char *text, const char *title, int flags) { errors++; }
static void ViewportUpdateGizmo(ViewportState *state) {}
static void ViewportRefreshStanOverlay(ViewportState *state) {}
static void ViewportSetTriangleColor(ViewportState *state, int tri, BOOL selected) {}
static void ViewportClearAllSelection(ViewportState *state)
{
    state->componentcount = state->stancomponentcount = 0;
    state->selectedtricount=0;
    if (state->selectedtris) { memset(state->selectedtris,0,state->scenecount/3); }
    if (state->stanselected) { memset(state->stanselected,0,state->stan.tilecount); }
    state->selectedportal = BG_PORTAL_INDEX_NONE; memset(state->portalselection,0,sizeof(state->portalselection));
}
static BOOL ViewportTryPickStan(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove)
{
    pickcalls|=8; return picktarget==4;
}
static void ViewportRefreshPortalColors(ViewportState *state) {}
static BOOL ViewportTryPickMarker(HWND hwnd, ViewportState *state, int x, int y, BOOL remove)
{ pickcalls|=1; return picktarget==1; }
static BOOL ViewportTryPickPortal(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove)
{ pickcalls|=2; return picktarget==2; }
static BOOL ViewportTryPickPad(HWND hwnd, ViewportState *state, int x, int y, BOOL remove)
{ pickcalls|=4; return picktarget==3; }
static void ViewportPickAt(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove)
{ faceclicks++; clickadd=add; clickremove=remove; }
static void ViewportPickComponent(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove)
{
    clicks++; clickadd = add; clickremove = remove;
}
/* Visibility itself is exercised by run_visible.py with an actual GL context.
 * This suite keeps all geometric candidates to isolate selection transactions. */
static BOOL failvisibility;
static BOOL ViewportFilterBoxFaces(ViewportState *state,const RECT *box,ViewportBoxFaceKind kind,
    unsigned char *hits,size_t capacity,int *countout)
{ if(failvisibility) return FALSE; *countout=0; for(size_t i=0;i<capacity;i++) { *countout+=hits[i]!=0; } return TRUE; }

static BOOL failalloc;
static void *TestCalloc(size_t count, size_t size) { return failalloc ? NULL : calloc(count,size); }
#define calloc TestCalloc
#include "logic.inc"
#undef calloc

static void ExpectHits(ViewportState *state, RECT box, BOOL stan, int expected)
{
    ViewportBoxComponent *hits = NULL;
    int count;
    assert(ViewportCollectBoxComponents(state, &box, stan, &hits, &count));
    assert(count == expected);
    free(hits);
}

static void FaceIntersection(void)
{
    ViewportState state={.width=200,.height=200};
    RECT center={90,90,110,110}, corner={100,100,120,120};
    ViewportBoxFrustum box;
    assert(ViewportBuildBoxFrustum(&state,&center,&box));
    const struct { Vertex vertices[3]; BOOL hit; } cases[]={
        {{{.x=-2,.y=-2,.z=-100},{.x=2,.y=-2,.z=-100},{.x=0,.y=2,.z=-100}},TRUE},
        /* All vertices and edges lie outside: only the triangle interior hits. */
        {{{.x=-50,.y=-30,.z=-100},{.x=50,.y=-30,.z=-100},{.x=0,.y=50,.z=-100}},TRUE},
        {{{.x=-20,.y=0,.z=-100},{.x=20,.y=0,.z=-100},{.x=20,.y=1,.z=-100}},TRUE},
        {{{.x=-20,.y=-20,.z=-100},{.x=20,.y=-20,.z=-100},{.x=0,.y=40,.z=100}},TRUE},
        {{{.x=-20,.y=-20,.z=-100},{.x=20,.y=-20,.z=-100},{.x=0,.y=40000,.z=-200000}},TRUE},
        {{{.x=0,.y=0,.z=100},{.x=20,.y=0,.z=100},{.x=0,.y=20,.z=100}},FALSE},
        {{{.x=0,.y=0,.z=-1},{.x=20,.y=0,.z=-1},{.x=0,.y=20,.z=-1}},FALSE},
        {{{.x=0,.y=0,.z=-200000},{.x=20,.y=0,.z=-200000},{.x=0,.y=20,.z=-200000}},FALSE},
        {{{.x=NAN,.y=0,.z=-100},{.x=20,.y=0,.z=-100},{.x=0,.y=20,.z=-100}},FALSE},
        {{{.x=0,.y=0,.z=-100},{.x=0,.y=0,.z=-100},{.x=0,.y=0,.z=-100}},TRUE}
    };
    for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
    {
        assert(ViewportTriangleInBox(cases[i].vertices,&box)==cases[i].hit);
        Vertex reversed[]={cases[i].vertices[2],cases[i].vertices[1],cases[i].vertices[0]};
        assert(ViewportTriangleInBox(reversed,&box)==cases[i].hit);
    }
    for (int i=0;i<3;i++)
    {
        assert(!ViewportVertexInBox(&state,&cases[1].vertices[i],&center));
        assert(!ViewportEdgeInBox(&state,&cases[1].vertices[i],&cases[1].vertices[(i+1)%3],&center));
    }
    assert(ViewportBuildBoxFrustum(&state,&corner,&box));
    Vertex tangent[]={{.x=-10,.y=10,.z=-100},{.x=-10,.y=0,.z=-100},{.x=0,.y=0,.z=-100}};
    Vertex miss[]={{.x=-20,.y=-10,.z=-100},{.x=10,.y=20,.z=-100},{.x=-20,.y=20,.z=-100}};
    assert(ViewportTriangleInBox(tangent,&box));
    assert(!ViewportTriangleInBox(miss,&box)); /* Bounding rectangles overlap, actual faces don't. */
    state.selectionfar=300000;
    assert(ViewportBuildBoxFrustum(&state,&center,&box) && ViewportTriangleInBox(cases[7].vertices,&box));
    state.posx=1000;state.posy=-200;state.posz=50;state.yaw=37;state.pitch=21;
    float forward[3],right[3];double up[3];Vertex rotated[3]={0};
    ViewportGetBasis(&state,forward,right);
    up[0]=right[1]*forward[2]-right[2]*forward[1];
    up[1]=right[2]*forward[0]-right[0]*forward[2];
    up[2]=right[0]*forward[1]-right[1]*forward[0];
    for(int i=0;i<3;i++)
    {
        const Vertex *v=&cases[1].vertices[i];
        rotated[i].x=state.posx+v->x*right[0]+v->y*up[0]-v->z*forward[0];
        rotated[i].y=state.posy+v->x*right[1]+v->y*up[1]-v->z*forward[1];
        rotated[i].z=state.posz+v->x*right[2]+v->y*up[2]-v->z*forward[2];
    }
    assert(ViewportBuildBoxFrustum(&state,&center,&box) && ViewportTriangleInBox(rotated,&box));
    assert(!ViewportBuildBoxFrustum(&state,&(RECT){110,90,90,110},&box));
    state.width=0;assert(!ViewportBuildBoxFrustum(&state,&center,&box));
    puts("PASS: triangle/box overlap, enclosing faces, boundary contacts, both windings, near/far crossings, extended range, camera rotation and non-finite rejection.");
}

static void FaceSelection(void)
{
    Vertex scene[21]={0};BgFaceRef refs[7]={0};SceneBatch batches[7]={0};
    unsigned char selected[7]={0},hidden[7]={0};
    Vertex triangle[]={{.x=-50,.y=-30,.z=-100},{.x=50,.y=-30,.z=-100},{.x=0,.y=50,.z=-100}};
    for(int i=0;i<7;i++)
    {
        memcpy(&scene[i*3],triangle,sizeof(triangle));
        refs[i]=(BgFaceRef){.faceid=i+1,.room=i+1};
        batches[i]=(SceneBatch){.first=i*3,.count=3,.cullbackfaces=TRUE};
    }
    for(int i=3;i<6;i++){scene[i].x*=2;scene[i].y*=2;scene[i].z*=2;}
    Vertex swapped=scene[3];scene[3]=scene[5];scene[5]=swapped;
    for(int i=6;i<9;i++){scene[i].x+=500;}
    hidden[3]=1;batches[4].secondary=TRUE;batches[5].object=TRUE;refs[6].faceid=BG_FACE_ID_NONE;
    ViewportState state={.tool=EDITOR_TOOL_FACE_SELECT,.width=200,.height=200,.showbgprimary=TRUE,
        .scene=scene,.scenecount=21,.scenefacerefs=refs,.batches=batches,.batchcount=7,
        .selectedtris=selected,.hiddentris=hidden,.selectedportal=BG_PORTAL_INDEX_NONE};
    capturedstate=&state;
    unsigned char *hits;int count;RECT center={90,90,110,110},empty={0,0,5,5};
    assert(ViewportCollectBoxFaces(&state,&center,FALSE,&hits,&count) && count==2);
    assert(hits[0] && hits[1] && !hits[2] && !hits[3] && !hits[4] && !hits[5] && !hits[6]);free(hits);
    int before=notifications;
    ViewportBeginBoxSelection(1,&state,110,110,FALSE,FALSE);
    ViewportUpdateBoxSelection(1,&state,90,90);
    assert(!state.selectedtricount); /* No changes until mouse-up. */
    ViewportEndBoxSelection(1,&state,90,90);
    assert(state.selectedtricount==2 && selected[0] && selected[1] && notifications==before+1 && !capture);
    selected[2]=1;state.selectedtricount++;
    assert(ViewportApplyFaceBox(&state,&center,TRUE,FALSE) && state.selectedtricount==3);
    assert(ViewportApplyFaceBox(&state,&center,TRUE,TRUE) && state.selectedtricount==1 && selected[2]);
    assert(ViewportApplyFaceBox(&state,&empty,TRUE,FALSE) && state.selectedtricount==1);
    assert(ViewportApplyFaceBox(&state,&empty,FALSE,TRUE) && state.selectedtricount==1);
    assert(ViewportApplyFaceBox(&state,&empty,FALSE,FALSE) && !state.selectedtricount);
    state.showbgsecondary=TRUE;
    assert(ViewportApplyFaceBox(&state,&center,FALSE,FALSE) && state.selectedtricount==3 && selected[4]);
    state.showbgprimary=FALSE;
    assert(ViewportApplyFaceBox(&state,&center,FALSE,FALSE) && state.selectedtricount==1 && selected[4]);
    state.showbgprimary=TRUE;state.showbgsecondary=FALSE;
    ViewportBeginBoxSelection(1,&state,90,90,FALSE,FALSE);
    ViewportUpdateBoxSelection(1,&state,110,110);ViewportCancelBoxSelection(1,&state);
    assert(state.selectedtricount==1 && selected[4]);
    before=notifications;int errorbefore=errors;
    failalloc=TRUE;
    ViewportBeginBoxSelection(1,&state,90,90,FALSE,FALSE);ViewportEndBoxSelection(1,&state,110,110);
    failalloc=FALSE;
    assert(state.selectedtricount==1 && selected[4] && notifications==before && errors==errorbefore+1 && !capture);
    failvisibility=TRUE; errorbefore=errors;
    ViewportBeginBoxSelection(1,&state,90,90,FALSE,FALSE);ViewportEndBoxSelection(1,&state,110,110);
    failvisibility=FALSE;
    assert(state.selectedtricount==1 && selected[4] && notifications==before && errors==errorbefore+1 && !capture);
    /* A click below the drag threshold retains the old marker/portal/pad/stan
       priority, and otherwise uses face/object picking, not component picking. */
    int oldclicks=clicks,oldfaces=faceclicks;
    for(int target=0;target<=4;target++)
    {
        picktarget=target;pickcalls=0;
        ViewportBeginBoxSelection(1,&state,90,90,TRUE,TRUE);ViewportEndBoxSelection(1,&state,91,91);
        assert(pickcalls==(target?(1<<target)-1:15));
    }
    picktarget=0;
    assert(faceclicks==oldfaces+1 && clicks==oldclicks && clickadd && clickremove);

    StanTile tiles[3]={0};unsigned char stanselected[3]={0};DWORD hiddenstan=3;
    for(int t=0;t<3;t++)
    {
        tiles[t].editorid=t+1;tiles[t].pointcount=4;
        for(int p=0;p<4;p++)
        {
            tiles[t].points[p].x=(p==1||p==2?30:-30)*(t==1?2:1);
            tiles[t].points[p].y=(p>=2?30:-30)*(t==1?2:1);
            tiles[t].points[p].z=t==1?-200:-100;
        }
    }
    state.stan=(StanFile){.tiles=tiles,.tilecount=3};state.stanselected=stanselected;
    state.showstan=TRUE;state.stanopacity=100;state.stanhiddenids=&hiddenstan;state.stanhiddencount=1;
    ViewportClearAllSelection(&state);stanselected[0]=1;
    assert(ViewportApplyFaceBox(&state,&center,FALSE,FALSE));
    assert(stanselected[0] && stanselected[1] && !stanselected[2] && !state.selectedtricount);
    assert(ViewportApplyFaceBox(&state,&center,TRUE,TRUE) && !stanselected[0] && !stanselected[1]);
    state.showbgprimary=FALSE;
    assert(ViewportApplyFaceBox(&state,&center,FALSE,FALSE)); /* No BG hits: fallback to stan faces. */
    assert(stanselected[0] && stanselected[1] && !stanselected[2]);
    state.stanopacity=0;
    assert(ViewportApplyFaceBox(&state,&center,FALSE,FALSE) && !stanselected[0] && !stanselected[1]);

    BgPortal portals[3]={0};
    for(int i=0;i<3;i++)
    {
        portals[i].geometryoffset=i<2?5:8;portals[i].pointcount=4;
        for(int p=0;p<4;p++)
        { portals[i].points[p]=(BgPortalPoint){p==1||p==2?30:-30,p>=2?30:-30,i==2?100:-100}; }
    }
    state.portals=(BgPortalFile){.portals=portals,.portalcount=3};state.showportals=TRUE;
    state.stanopacity=100;
    assert(ViewportApplyFaceBox(&state,&center,FALSE,FALSE)); /* Portal fallback precedes stans. */
    assert(state.selectedportal==0 && state.portalselection[0]==1 && state.portalselection[1]==1
        && !state.portalselection[2] && !stanselected[0]);
    before=notifications;
    ViewportBeginBoxSelection(1,&state,90,90,TRUE,TRUE);ViewportEndBoxSelection(1,&state,110,110);
    assert(state.selectedportal==BG_PORTAL_INDEX_NONE && !state.portalselection[0]
        && !state.portalselection[1] && notifications==before+1);
    assert(ViewportApplyPortalBox(&state,&center,FALSE,FALSE));
    failvisibility=TRUE; errorbefore=errors;
    ViewportBeginBoxSelection(1,&state,90,90,FALSE,FALSE);ViewportEndBoxSelection(1,&state,110,110);
    failvisibility=FALSE;
    assert(state.selectedportal==0 && state.portalselection[0]==1 && state.portalselection[1]==1 && errors==errorbefore+1);
    assert(ViewportApplyPortalBox(&state,&empty,FALSE,FALSE) && state.selectedportal==BG_PORTAL_INDEX_NONE);
    puts("PASS: BG/stan/portal candidate boxes, hidden/layer filters, modifiers, asset priorities, click/cancel/reverse drag and atomic allocation failure.");
}

int main(void)
{
    /* Two triangles share an edge in opposite directions. A third, behind
       them, has identical projected positions but unrelated room identities. */
    Vertex scene[9] = {
        {.x=-20,.y=-20,.z=-100}, {.x=20,.y=-20,.z=-100}, {.x=20,.y=20,.z=-100},
        {.x=20,.y=20,.z=-100}, {.x=-20,.y=-20,.z=-100}, {.x=-20,.y=20,.z=-100},
        {.x=-40,.y=-40,.z=-200}, {.x=40,.y=-40,.z=-200}, {.x=40,.y=40,.z=-200}
    };
    BgDocumentVertexRef refs[9] = {{1,0},{1,1},{1,2},{1,2},{1,0},{1,3},{2,0},{2,1},{2,2}};
    SceneBatch batches[3] = {{.first=0,.count=3}, {.first=3,.count=3}, {.first=6,.count=3}};
    unsigned char hidden[3] = {0};
    ViewportState state = {.tool=EDITOR_TOOL_EDGE_SELECT, .width=200, .height=200,
        .showbgprimary=TRUE, .showbgsecondary=TRUE, .scene=scene, .scenecount=9,
        .scenevertexrefs=refs, .batches=batches, .batchcount=3, .hiddentris=hidden};
    RECT all = {60,60,140,140}, bottom = {60,125,140,140}, center = {90,90,110,110};
    ViewportBoxComponent *hits = NULL;
    int count;
    capturedstate = &state;
    ExpectHits(&state, all, FALSE, 8); /* Five front edges, three occluded edges. */
    ExpectHits(&state, bottom, FALSE, 7); /* Contained and partially enclosed edges. */
    ExpectHits(&state, center, FALSE, 2); /* Crossing with both endpoints outside. */
    ExpectHits(&state, (RECT){125,125,140,140}, FALSE, 4); /* One endpoint only. */
    /* Exact boundary contact, parallel misses and overlapping bounding boxes
       whose segments miss the rectangle. Check both edge directions. */
    const struct { Vertex a, b; BOOL hit; } contacts[] = {
        {{.x=-20,.y=20,.z=-100}, {.x=0,.y=0,.z=-100}, TRUE}, /* Corner endpoint. */
        {{.x=-20,.y=-20,.z=-100}, {.x=20,.y=20,.z=-100}, TRUE}, /* Corner tangent. */
        {{.x=0,.y=20,.z=-100}, {.x=0,.y=-20,.z=-100}, TRUE}, /* Along boundary. */
        {{.x=-1,.y=20,.z=-100}, {.x=-1,.y=-20,.z=-100}, FALSE}, /* Parallel outside. */
        {{.x=-20,.y=-10,.z=-100}, {.x=10,.y=20,.z=-100}, FALSE}, /* Bounding box only. */
        {{.x=0,.y=0,.z=-100}, {.x=0,.y=0,.z=-100}, TRUE}, /* Projects to a point. */
        {{.x=0,.y=0,.z=100}, {.x=0,.y=0,.z=-100}, TRUE}, /* Crosses near plane. */
        {{.x=0,.y=0,.z=-200000}, {.x=0,.y=0,.z=-100}, TRUE}, /* Crosses far plane. */
        {{.x=0,.y=0,.z=-1}, {.x=0,.y=0,.z=-5}, FALSE}, /* Entirely clipped. */
        {{.x=0,.y=0,.z=100}, {.x=0,.y=0,.z=200}, FALSE}, /* Behind camera. */
        {{.x=-20,.y=0,.z=-100}, {.x=20,.y=0,.z=100}, FALSE} /* Only extension hits. */
    };
    for (unsigned int i=0; i<sizeof(contacts)/sizeof(contacts[0]); i++)
    {
        RECT corner = {100,100,120,120};
        assert(ViewportEdgeInBox(&state, &contacts[i].a, &contacts[i].b, &corner) == contacts[i].hit);
        assert(ViewportEdgeInBox(&state, &contacts[i].b, &contacts[i].a, &corner) == contacts[i].hit);
    }
    hidden[0] = 1;
    ExpectHits(&state, all, FALSE, 6); /* Shared edge survives via visible face. */
    assert(ViewportCollectBoxComponents(&state, &all, FALSE, &hits, &count));
    assert(hits[0].ends[0].corner == 4 && hits[0].ends[1].corner == 3);
    free(hits); hidden[0] = 0;
    batches[2].secondary = TRUE; state.showbgsecondary = FALSE;
    ExpectHits(&state, all, FALSE, 5);
    state.showbgsecondary = TRUE; batches[2].object = TRUE;
    ExpectHits(&state, all, FALSE, 5);
    batches[2].object = FALSE; state.showbgprimary = FALSE;
    ExpectHits(&state, all, FALSE, 3);
    state.showbgprimary = TRUE;
    for (int i=6; i<9; i++) { scene[i].z = 100; }
    ExpectHits(&state, all, FALSE, 5); /* Behind camera. */
    for (int i=6; i<9; i++) { scene[i].z = -1; }
    ExpectHits(&state, all, FALSE, 5); /* Before near plane. */
    for (int i=6; i<9; i++) { scene[i].z = -200000; }
    ExpectHits(&state, all, FALSE, 5); /* Beyond far plane. */
    for (int i=6; i<9; i++) { scene[i].z = -200; }

    assert(ViewportCollectBoxComponents(&state, &all, FALSE, &hits, &count));
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, FALSE, FALSE));
    assert(state.componentcount == 8);
    for (int i=0; i<state.componentcount; i++)
    {
        const ViewportComponent *c = &state.components[i];
        assert(ViewportCompareVertexRefs(&c->refs[0], &c->refs[1]) < 0);
        for (int end=0; end<2; end++)
            assert(!ViewportCompareVertexRefs(&c->refs[end], &refs[c->corners[end]]));
    }
    /* Two different edges with the same first vertex must both remain selected. */
    assert(state.components[0].refs[0].index == 0 && state.components[1].refs[0].index == 0);
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, TRUE, FALSE));
    assert(state.componentcount == 8); /* Shift union, no duplicates. */
    free(hits);
    assert(ViewportCollectBoxComponents(&state, &bottom, FALSE, &hits, &count));
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, TRUE, TRUE));
    assert(state.componentcount == 1); /* Ctrl wins over Shift; intersecting edges removed. */
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, TRUE, FALSE));
    assert(state.componentcount == 8 && state.components[0].refs[0].index == 2
           && state.components[0].refs[1].index == 3);
    free(hits);
    assert(ViewportApplyBoxComponents(&state, NULL, 0, FALSE, TRUE, FALSE));
    assert(ViewportApplyBoxComponents(&state, NULL, 0, FALSE, FALSE, TRUE));
    assert(state.componentcount == 8); /* Empty modified boxes preserve selection. */

    ViewportBeginBoxSelection(1, &state, 140, 140, FALSE, FALSE);
    ViewportUpdateBoxSelection(1, &state, 60, 60);
    assert(state.componentcount == 8 && clicks == 0);
    ViewportCancelBoxSelection(1, &state);
    assert(state.componentcount == 8 && !capture && clicks == 0);
    ViewportBeginBoxSelection(1, &state, 70, 70, TRUE, FALSE);
    ViewportEndBoxSelection(1, &state, 71, 71);
    assert(clicks == 1 && clickadd && !clickremove && !capture && state.componentcount == 8);
    ViewportBeginBoxSelection(1, &state, 140, 140, FALSE, FALSE);
    ViewportEndBoxSelection(1, &state, 60, 125); /* Reverse drag, bottom edges. */
    assert(state.componentcount == 7 && notifications == 1 && !capture);
    ViewportBeginBoxSelection(1, &state, 20, 20, FALSE, FALSE);
    ViewportEndBoxSelection(1, &state, 30, 30);
    assert(state.componentcount == 0);

    /* Vertex-mode click selection leaves refs[1] unused. Its identity still
       must match a marquee vertex after sharing the edge-aware code path. */
    state.tool = EDITOR_TOOL_VERTEX_SELECT;
    ExpectHits(&state, center, FALSE, 0); /* Vertices still require containment. */
    assert(ViewportCollectBoxComponents(&state, &all, FALSE, &hits, &count));
    assert(count == 7);
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, FALSE, FALSE));
    for (int i=0; i<state.componentcount; i++) { state.components[i].refs[1] = (BgDocumentVertexRef){0}; }
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, TRUE, FALSE));
    assert(state.componentcount == 7);
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, FALSE, TRUE));
    assert(state.componentcount == 0);
    free(hits);

    /* Stan uses polygon boundaries, including the closing edge, and joins
       shared points by its existing map without selecting fan diagonals. */
    StanTile tiles[2] = {
        {.id=1, .editorid=1, .pointcount=4, .points={{-20,-20,-100,0},{20,-20,-100,0},{20,20,-100,0},{-20,20,-100,0}}},
        {.id=2, .editorid=2, .pointcount=3, .points={{20,20,-100,0},{20,-20,-100,0},{30,0,-100,0}}}
    };
    DWORD pointmap[2 * STAN_TILE_MAX_POINTS];
    for (unsigned int i=0; i<2*STAN_TILE_MAX_POINTS; i++) { pointmap[i] = i; }
    pointmap[STAN_TILE_MAX_POINTS] = 2;
    pointmap[STAN_TILE_MAX_POINTS+1] = 1;
    state.stan.tiles = tiles; state.stan.tilecount = 2; state.stanpointmap = pointmap;
    state.showstan = TRUE; state.stanopacity = 50; state.tool = EDITOR_TOOL_EDGE_SELECT;
    RECT wide = {40,40,160,160};
    ExpectHits(&state, wide, TRUE, 6); /* Shared edge occurs only once. */
    ExpectHits(&state, bottom, TRUE, 4); /* Partially enclosed stan edges. */
    ExpectHits(&state, (RECT){125,90,140,110}, TRUE, 1); /* Crosses with endpoints outside. */
    ExpectHits(&state, center, TRUE, 0); /* No artificial polygon diagonals. */
    assert(ViewportCollectBoxComponents(&state, &wide, TRUE, &hits, &count));
    assert(ViewportApplyBoxComponents(&state, hits, count, TRUE, FALSE, FALSE));
    assert(state.stancomponentcount == 6 && state.componentcount == 0);
    assert(state.stancomponents[1].refs[0].point == 0 && state.stancomponents[1].refs[1].point == 3);
    assert(ViewportApplyBoxComponents(&state, hits, count, TRUE, TRUE, FALSE));
    assert(state.stancomponentcount == 6);
    assert(ViewportApplyBoxComponents(&state, hits, count, TRUE, FALSE, TRUE));
    assert(state.stancomponentcount == 0);
    free(hits);
    state.showstan = FALSE; ExpectHits(&state, wide, TRUE, 0);
    state.showstan = TRUE; state.stanopacity = 0; ExpectHits(&state, wide, TRUE, 0);
    state.stanopacity = 50; state.tool = EDITOR_TOOL_VERTEX_SELECT;
    ExpectHits(&state, wide, TRUE, 5);
    DWORD hiddenstan=1;
    state.stanhiddenids=&hiddenstan;state.stanhiddencount=1;
    ExpectHits(&state, wide, TRUE, 3); /* Hidden canonical owner, visible shared endpoints. */
    state.tool=EDITOR_TOOL_EDGE_SELECT;ExpectHits(&state, wide, TRUE, 3);
    hiddenstan=2;ExpectHits(&state, wide, TRUE, 4);
    state.stanhiddencount=0;ExpectHits(&state, wide, TRUE, 6);
    free(state.components); free(state.stancomponents);
    puts("PASS: BG/stan edge boxes, identities, visibility/clipping, modifiers, drag/cancel/click, vertex regression.");
    FaceIntersection();FaceSelection();
    return 0;
}
