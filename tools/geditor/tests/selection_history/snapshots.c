#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "stanload.h"
#include "setupload.h"
#include "edittool.h"
typedef void *HWND;
typedef struct { int x, y; } POINT;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
#include "uvcanvas.h"
#include "types.inc"
#define VIEWPORT_OBJECT_NONE ((DWORD)-1)

typedef struct ViewportState {
    EditorTool tool;
    BOOL vertexsnap, showbgprimary, showbgsecondary, showstan, showobjects, showportals;
    BOOL markerselected, gizmovisible;
    int hoveraxis, stanopacity, scenecount, batchcount;
    int selectedtricount, componentcount, componentcapacity, stancomponentcount, stancomponentcapacity;
    unsigned char *selectedtris, *hiddentris, *stanselected;
    BgFaceRef *scenefacerefs;
    BgDocumentVertexRef *scenevertexrefs;
    ViewportComponent *components;
    ViewportStanComponent *stancomponents;
    SceneBatch *batches;
    StanFile stan;
    DWORD selectedobject, selectedportal, padcount;
    SetupPadRef selectedpad;
    SetupMarkerRef selectedmarker;
    ViewportPad *pads;
    BgPortalFile portals;
    float posx, posy, posz; /* Restoring a selection must leave camera unchanged. */
} ViewportState;

static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static UVCanvasState *UVCanvasGetState(HWND hwnd) { return hwnd; }
static void ViewportRefreshStanOverlay(ViewportState *state) {}
static void ViewportRefreshPadColors(ViewportState *state) {}
static void ViewportRefreshPortalColors(ViewportState *state) {}
static void ViewportUpdateGizmo(ViewportState *state) { state->gizmovisible = TRUE; }
static void ViewportRedraw(HWND hwnd) {}
static void ViewportCancelTransform(HWND hwnd) {}
static void ViewportSetTriangleColor(ViewportState *state, int triangle, BOOL selected) {}
static void ViewportSelectObject(ViewportState *state, DWORD index) { state->selectedobject = index; }
static BOOL ViewportSelectedMarker(const ViewportState *state, SetupMarker *marker)
{ return state->markerselected && state->showobjects && state->selectedmarker.command < 10; }
static void ViewportClearPadSelection(ViewportState *state)
{ state->selectedpad.index = SETUP_PAD_INDEX_NONE; state->selectedportal = BG_PORTAL_INDEX_NONE; state->markerselected = FALSE; }
static void ViewportClearBgSelection(ViewportState *state)
{ if (state->selectedtris) { memset(state->selectedtris, 0, state->scenecount / 3); } state->selectedtricount = 0; }
static void ViewportClearObjectSelection(ViewportState *state) { state->selectedobject = VIEWPORT_OBJECT_NONE; }
static void ViewportClearStanSelection(ViewportState *state)
{ if (state->stanselected) { memset(state->stanselected, 0, state->stan.tilecount); } state->stancomponentcount = 0; }
BOOL UVCanvasCancelInteraction(HWND canvas) { return FALSE; }
static void UVCanvasUpdatePreview(UVCanvasState *state) {}
static void UVCanvasNotify(HWND canvas) {}
static BOOL failalloc;
static void *TestMalloc(size_t size) { return failalloc ? NULL : malloc(size); }
static void *TestCalloc(size_t count, size_t size) { return failalloc ? NULL : calloc(count, size); }
#define malloc TestMalloc
#define calloc TestCalloc
#include "snapshots.inc"
#undef malloc
#undef calloc

static void *Capture(ViewportState *state, size_t *size)
{
    void *snapshot;
    assert(ViewportCaptureSelection(state, &snapshot, size));
    return snapshot;
}

int main(void)
{
    BgFaceRef faces[2] = {{.faceid=20,.room=1}, {.faceid=10,.room=2}};
    BgDocumentVertexRef refs[6] = {{1,0},{1,1},{1,2}, {2,0},{2,1},{2,2}};
    unsigned char selected[2] = {1,0}, hidden[2] = {0}, stanselected[2] = {0};
    SceneBatch batches[2] = {{.first=0,.count=3}, {.first=3,.count=3,.secondary=TRUE}};
    StanTile tiles[2] = {{.pointcount=3}, {.pointcount=3}};
    ViewportPad pads[2] = {{.ref={3,FALSE}}, {.ref={3,TRUE}}};
    ViewportState state = {.tool=EDITOR_TOOL_FACE_SELECT, .scenecount=6, .scenefacerefs=faces,
        .selectedtris=selected, .hiddentris=hidden, .selectedtricount=1, .scenevertexrefs=refs,
        .batches=batches, .batchcount=2, .stan={.tiles=tiles,.tilecount=2}, .stanselected=stanselected,
        .showbgprimary=TRUE, .showbgsecondary=TRUE, .showstan=TRUE, .stanopacity=44,
        .showobjects=TRUE, .showportals=TRUE, .selectedobject=VIEWPORT_OBJECT_NONE,
        .selectedportal=BG_PORTAL_INDEX_NONE, .selectedpad={SETUP_PAD_INDEX_NONE,FALSE},
        .pads=pads, .padcount=2, .portals={.portalcount=2}, .posx=123, .posy=456, .posz=789};
    size_t size, other_size;
    void *snapshot = Capture(&state, &size), *other;
    /* Reorder triangles like a material edit does. Selection follows face ID. */
    BgFaceRef temp = faces[0]; faces[0] = faces[1]; faces[1] = temp;
    selected[0] = selected[1] = 0;
    state.tool = EDITOR_TOOL_VERTEX_SELECT;
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(state.tool == EDITOR_TOOL_FACE_SELECT && !selected[0] && selected[1] && state.selectedtricount == 1);
    assert(state.posx == 123 && state.posy == 456 && state.posz == 789);
    other = Capture(&state, &other_size);
    assert(size == other_size && !memcmp(snapshot, other, size)); free(other);
    hidden[1] = 1;
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(!state.selectedtricount && !selected[1]); hidden[1] = 0;
    state.showbgsecondary = FALSE;
    assert(ViewportRestoreSelection(&state, snapshot, size)); assert(!state.selectedtricount);
    state.showbgsecondary = TRUE;
    free(snapshot);

    /* Component history stores native IDs, not cached scene-corner indices. */
    ViewportClearAllSelection(&state);
    state.tool = EDITOR_TOOL_EDGE_SELECT;
    state.components = calloc(1, sizeof(*state.components));
    state.componentcount = state.componentcapacity = 1;
    state.components[0] = (ViewportComponent){.refs={{2,1},{1,0}}, .corners={4,0}};
    snapshot = Capture(&state, &size);
    refs[4] = (BgDocumentVertexRef){1,0}; refs[0] = (BgDocumentVertexRef){2,1};
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(state.componentcount == 1 && state.components[0].corners[0] == 0 && state.components[0].corners[1] == 4);
    other = Capture(&state, &other_size);
    assert(size == other_size && !memcmp(snapshot, other, size)); free(other);
    failalloc = TRUE;
    assert(!ViewportRestoreSelection(&state, snapshot, size));
    assert(state.componentcount == 1 && state.components[0].corners[0] == 0);
    failalloc = FALSE;
    refs[0].room = 0; /* Deleted identity must not select an unrelated vertex. */
    assert(ViewportRestoreSelection(&state, snapshot, size)); assert(!state.componentcount);
    free(snapshot);
    state.tool = EDITOR_TOOL_VERTEX_SELECT;
    state.vertexsnap = TRUE;
    state.componentcount = 1;
    state.components[0] = (ViewportComponent){.refs={{1,1},{0,0}}, .corners={1,1}};
    snapshot = Capture(&state, &size);
    state.tool = EDITOR_TOOL_FACE_SELECT; state.vertexsnap = FALSE;
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(state.tool == EDITOR_TOOL_VERTEX_SELECT && state.vertexsnap && state.componentcount == 1);
    assert(state.components[0].corners[0] == 1 && state.components[0].corners[1] == 1);
    free(snapshot);

    ViewportClearAllSelection(&state);
    state.tool = EDITOR_TOOL_EDGE_SELECT;
    state.stancomponents = calloc(2, sizeof(*state.stancomponents));
    state.stancomponentcount = state.stancomponentcapacity = 2;
    state.stancomponents[0] = (ViewportStanComponent){.refs={{0,1},{1,2}}};
    state.stancomponents[1] = (ViewportStanComponent){.refs={{0,0},{1,0}}};
    snapshot = Capture(&state, &size);
    ViewportClearAllSelection(&state);
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(state.stancomponentcount == 2 && state.stancomponents[0].refs[1].point == 2);
    tiles[1].pointcount = 1;
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(state.stancomponentcount == 1 && state.stancomponents[0].refs[0].point == 0);
    tiles[1].pointcount = 3;
    free(snapshot);
    ViewportClearAllSelection(&state);
    state.tool = EDITOR_TOOL_FACE_SELECT; stanselected[1] = TRUE;
    snapshot = Capture(&state, &size);
    stanselected[0] = TRUE; stanselected[1] = FALSE;
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(!stanselected[0] && stanselected[1]); free(snapshot);

    /* Setup models (including character IDs), bound/ordinary pads, camera,
     * spawn/spline markers and portal identities survive selection changes. */
    ViewportClearAllSelection(&state);
    state.selectedobject = 0x80000003u;
    snapshot = Capture(&state, &size);
    state.selectedobject = 0;
    assert(ViewportRestoreSelection(&state, snapshot, size));
    assert(state.selectedobject == 0x80000003u); free(snapshot);
    for (int bound = 0; bound < 2; bound++)
    {
        ViewportClearAllSelection(&state);
        state.selectedpad = (SetupPadRef){3, bound};
        snapshot = Capture(&state, &size);
        ViewportClearAllSelection(&state);
        assert(ViewportRestoreSelection(&state, snapshot, size));
        assert(state.selectedpad.index == 3 && state.selectedpad.bound == bound); free(snapshot);
    }
    for (int kind = SETUP_MARKER_SPAWN; kind < SETUP_MARKER_KIND_COUNT; kind++)
    {
        ViewportClearAllSelection(&state);
        state.selectedmarker = (SetupMarkerRef){kind, 7}; state.markerselected = TRUE;
        snapshot = Capture(&state, &size);
        ViewportClearAllSelection(&state);
        assert(ViewportRestoreSelection(&state, snapshot, size));
        assert(state.markerselected && state.selectedmarker.kind == (SetupMarkerKind)kind && state.selectedmarker.command == 7);
        free(snapshot);
    }
    ViewportClearAllSelection(&state); state.selectedportal = 1;
    snapshot = Capture(&state, &size);
    state.selectedportal = 0;
    assert(ViewportRestoreSelection(&state, snapshot, size)); assert(state.selectedportal == 1);
    assert(!ViewportRestoreSelection(&state, snapshot, size - 1)); assert(state.selectedportal == 1);
    state.showportals = FALSE;
    assert(ViewportRestoreSelection(&state, snapshot, size)); assert(state.selectedportal == BG_PORTAL_INDEX_NONE);
    free(snapshot);

    /* UV snapshots ignore transformed coordinates, but retain source identity
     * and texture dimensions for mixed-size texture selections. */
    UVCanvasNode nodes[2] = {{.source={.vertex={1,0},.vertexid=10,.s=32}, .width=64,.height=32,.selected=TRUE},
                            {.source={.vertex={1,1},.vertexid=11}, .width=32,.height=32}};
    UVCanvasState uv = {.nodes=nodes,.nodecount=2};
    assert(UVCanvasCaptureSelection(&uv, &snapshot, &size));
    nodes[0].source.s = 900;
    assert(UVCanvasCaptureSelection(&uv, &other, &other_size));
    assert(size == other_size && !memcmp(snapshot, other, size)); free(other);
    nodes[0].selected = FALSE; nodes[1].selected = TRUE; nodes[0].width = 32;
    assert(UVCanvasRestoreSelection(&uv, snapshot, size));
    assert(nodes[0].selected && !nodes[1].selected && nodes[0].width == 64 && nodes[0].source.s == 900);
    nodes[0].source.vertexid++;
    assert(UVCanvasRestoreSelection(&uv, snapshot, size)); assert(!nodes[0].selected && !nodes[1].selected);
    free(snapshot);
    assert(UVCanvasCaptureSelection(&uv, &snapshot, &size)); assert(!snapshot && !size);
    assert(UVCanvasRestoreSelection(&uv, NULL, 0));
    free(state.components); free(state.stancomponents);
    puts("BG/stan, object/character, pad/marker, portal and UV selection snapshot restoration passed.");
    return 0;
}
