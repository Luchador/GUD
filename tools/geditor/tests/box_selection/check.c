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
    EditorTool tool;
    int width, height, scenecount, batchcount;
    float posx, posy, posz, yaw, pitch;
    BOOL showbgprimary, showbgsecondary, showstan;
    int stanopacity;
    Vertex *scene;
    BgDocumentVertexRef *scenevertexrefs;
    SceneBatch *batches;
    unsigned char *hiddentris;
    StanFile stan;
    DWORD *stanpointmap;
    ViewportComponent *components;
    int componentcount, componentcapacity;
    ViewportStanComponent *stancomponents;
    int stancomponentcount, stancomponentcapacity;
    BOOL boxpending, boxdragging, boxadd, boxremove;
    POINT boxstart, boxend;
    int hoveraxis;
} ViewportState;

static HWND capture;
static ViewportState *capturedstate;
static int clicks, notifications;
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
static void MessageBox(HWND hwnd, const char *text, const char *title, int flags) { assert(0); }
static void ViewportUpdateGizmo(ViewportState *state) {}
static void ViewportRefreshStanOverlay(ViewportState *state) {}
static void ViewportClearAllSelection(ViewportState *state)
{
    state->componentcount = state->stancomponentcount = 0;
}
static BOOL ViewportTryPickStan(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove)
{
    return FALSE;
}
static void ViewportPickComponent(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove)
{
    clicks++; clickadd = add; clickremove = remove;
}
#include "logic.inc"

static void ExpectHits(ViewportState *state, RECT box, BOOL stan, int expected)
{
    ViewportBoxComponent *hits = NULL;
    int count;
    assert(ViewportCollectBoxComponents(state, &box, stan, &hits, &count));
    assert(count == expected);
    free(hits);
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
    ExpectHits(&state, bottom, FALSE, 2); /* One contained edge in each room. */
    ExpectHits(&state, center, FALSE, 0); /* Crossing without enclosing endpoints. */
    ExpectHits(&state, (RECT){125,125,140,140}, FALSE, 0); /* One endpoint only. */
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
    assert(state.componentcount == 6); /* Ctrl wins over Shift; only full matching edges removed. */
    assert(ViewportApplyBoxComponents(&state, hits, count, FALSE, TRUE, FALSE));
    assert(state.componentcount == 8 && state.components[0].refs[1].index == 2);
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
    assert(state.componentcount == 2 && notifications == 1 && !capture);
    ViewportBeginBoxSelection(1, &state, 20, 20, FALSE, FALSE);
    ViewportEndBoxSelection(1, &state, 30, 30);
    assert(state.componentcount == 0);

    /* Vertex-mode click selection leaves refs[1] unused. Its identity still
       must match a marquee vertex after sharing the edge-aware code path. */
    state.tool = EDITOR_TOOL_VERTEX_SELECT;
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
        {.pointcount=4, .points={{-20,-20,-100,0},{20,-20,-100,0},{20,20,-100,0},{-20,20,-100,0}}},
        {.pointcount=3, .points={{20,20,-100,0},{20,-20,-100,0},{30,0,-100,0}}}
    };
    DWORD pointmap[2 * STAN_TILE_MAX_POINTS];
    for (unsigned int i=0; i<2*STAN_TILE_MAX_POINTS; i++) { pointmap[i] = i; }
    pointmap[STAN_TILE_MAX_POINTS] = 2;
    pointmap[STAN_TILE_MAX_POINTS+1] = 1;
    state.stan.tiles = tiles; state.stan.tilecount = 2; state.stanpointmap = pointmap;
    state.showstan = TRUE; state.stanopacity = 50; state.tool = EDITOR_TOOL_EDGE_SELECT;
    RECT wide = {40,40,160,160};
    ExpectHits(&state, wide, TRUE, 6); /* Shared edge occurs only once. */
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
    free(state.components); free(state.stancomponents);
    puts("PASS: BG/stan edge boxes, identities, visibility/clipping, modifiers, drag/cancel/click, vertex regression.");
    return 0;
}
