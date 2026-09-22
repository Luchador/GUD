#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *HWND;
typedef void *HMENU;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef struct { int x, y; } POINT;
#define WM_APP 0x8000
#define VK_CONTROL 17
#define MB_ICONERROR 16
#define GA_ROOTOWNER 3
#define UVCANVAS_PI 3.14159265358979323846
#define UVCANVAS_ROTATE_RADIUS 56
#include "uvcanvas.h"
#include "setupload.h"
#include "stanload.h"
#include "rotation.h"
#include "types.inc"

static UVCanvasState state;
static HWND canvas = &state, capture;
static UVCanvasTriangle source;
static int commits, refreshes, inspectorUpdates, notificationDepth;
static DWORD inspectorOffset;
static BOOL objectSelected;
static BOOL g_SelectionHistoryPending;
static HWND g_Viewport = (HWND)1, g_RightPanel = (HWND)2, g_Browser = (HWND)3;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static struct { char dir[MAX_PATH]; int levelcount; struct { const char *name; } levels[1]; } g_Project;
static int g_CurrentLevelIndex;

static UVCanvasState *UVCanvasGetState(HWND hwnd) { return hwnd; }
static HWND GetParent(HWND hwnd) { return (HWND)4; }
static HWND GetCapture(void) { return capture; }
static void ReleaseCapture(void) { capture = NULL; UVCanvasCancelInteraction(canvas); }
static void InvalidateRect(HWND hwnd, const void *rect, BOOL erase) {}
static int GetKeyState(int key) { return 0; }
static int MessageBox(HWND hwnd, const char *text, const char *title, int flags) { abort(); }
static LRESULT SendMessage(HWND hwnd, unsigned message, WPARAM wparam, LPARAM lparam);
static void UVCanvasUpdatePreview(UVCanvasState *s);
static void UVCanvasEndPan(HWND hwnd, UVCanvasState *s) { s->panning = FALSE; }
#include "canvas.inc"

/* Presentation calls are inert. Replacing the UV overlay uses the real
 * cancellation, shared-vertex selection and preview rebuilding functions. */
static void UVEditorRefreshSelection(HWND hwnd, const BgDocument *document, const char *projectdir)
{
    UVCanvasTriangle *copy = malloc(sizeof(*copy)); assert(copy);
    *copy = source; refreshes++;
    assert(UVCanvasSetTriangles(canvas, copy, 1));
}
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return 1; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd, BgFaceRef *refs, int count)
{ refs[0] = source.face; return TRUE; }
static BOOL ViewportGetSelectedObject(HWND hwnd, DWORD *index) { *index = 0; return objectSelected; }
static DWORD ViewportGetSelectedModelCount(HWND hwnd) { return objectSelected ? 1 : 0; }
static BOOL ViewportGetSelectedModels(HWND hwnd, DWORD *models, DWORD count) { models[0]=0; return TRUE; }
static int ViewportGetSelectedComponentCount(HWND hwnd) { return 0; }
static DWORD ViewportGetStanSelectionCount(HWND hwnd, DWORD *tile) { *tile = 0; return 0; }
static BOOL ViewportGetSelectedStanTiles(HWND hwnd, DWORD *tiles, DWORD count) { return FALSE; }
static EditorTool ViewportGetTool(HWND hwnd) { return EDITOR_TOOL_FACE_SELECT; }
static BOOL ViewportGetSelectedPortal(HWND hwnd, DWORD *index) { return FALSE; }
static BOOL ViewportGetSelectedMarker(HWND hwnd, SetupMarkerRef *ref, void *marker) { return FALSE; }
static BOOL ViewportGetSelectedPad(HWND hwnd, SetupPadRef *ref) { return FALSE; }
static void Ignore(HWND hwnd, ...) {}
#define RightPanelSetObjectFlags Ignore
#define RightPanelSetVertexPaintMode Ignore
#define RightPanelSetPortal Ignore
#define RightPanelSetSetupMarker Ignore
#define RightPanelSetSetupPad Ignore
#define RightPanelSetStanSelection Ignore
#define RightPanelSetSetupCharacter Ignore
#define RightPanelSetBgComponentSelection Ignore
#define RightPanelSetBgFaces Ignore
#define RightPanelSetBgSelectionCount Ignore
#define RightPanelSetModelSelectionCount Ignore
static void LevelManagerRefresh(const SetupFile *setup,const char *name) {}
static void LevelManagerRefreshSettings(const void *project,int index) {}
static void RightPanelSetSetupObject(HWND hwnd, const SetupFile *setup, DWORD index, const char *dir)
{ inspectorUpdates++; inspectorOffset = setup->objects[index].sourceoffset; }
static void GEditorRefreshTransformFields(void) {}
static HWND GetAncestor(HWND hwnd, int flags) { return hwnd; }
static void GEditorSetTitleForProject(HWND hwnd) {}
static HMENU GetMenu(HWND hwnd) { return NULL; }
static HMENU GetSubMenu(HMENU menu, int index) { return NULL; }
static void GEditorUpdateHistoryMenu(HMENU menu) {}
static void DrawMenuBar(HWND hwnd) {}
#include "editor.inc"

static LRESULT SendMessage(HWND hwnd, unsigned message, WPARAM wparam, LPARAM lparam)
{
    if (message == UVCANVAS_WM_SELECTION_CHANGED)
    {
        /* UVEditor forwards this to the frame, which refreshes history after
         * its outermost notification. Nested rebuild notifications coalesce. */
        notificationDepth++;
        if (notificationDepth == 1) { GEditorRefreshHistoryMenu((HWND)5); }
        notificationDepth--;
    }
    else
    {
        const UVCanvasEdit *edit = (const UVCanvasEdit *)lparam;
        assert(message == UVCANVAS_WM_COMMIT && edit->count == 3);
        assert(!state.draghandle && !capture);
        commits++;
        for (DWORD i = 0; i < edit->count; i++)
        {
            assert(edit->vertices[i].vertexid == source.source[i].vertexid);
            source.source[i] = edit->vertices[i];
        }
        /* Actual edits must still rebuild the overlay and retain selection. */
        GEditorRefreshSelectionDetails();
    }
    return TRUE;
}

static void Fixture(TransformMode mode)
{
    free(state.nodes); free(state.triangles); memset(&state, 0, sizeof(state));
    memset(&source, 0, sizeof(source));
    state.width = state.height = 600; state.pixelsperunit = 200;
    state.centeru = state.centerv = .5; state.mode = mode;
    source.width = source.height = 32;
    source.face = (BgFaceRef){.room = 1, .faceid = 1};
    for (DWORD i = 0; i < 3; i++)
    { source.source[i] = (BgDocumentUVEdit){.vertex = {1, i}, .vertexid = i + 1,
        .s = i == 1 ? 1024 : 0, .t = i == 2 ? 1024 : 0}; }
    capture = NULL; commits = 0;
    UVEditorRefreshSelection(g_Viewport, &g_CurrentBgDocument, g_Project.dir);
    for (int i = 0; i < 3; i++) { state.nodes[i].selected = TRUE; }
    UVCanvasResetTransform(&state);
}

static void BeginDrag(void)
{
    POINT origin;
    assert(UVCanvasGizmo(&state, &origin));
    state.dragstart = origin;
    if (state.mode == TRANSFORM_ROTATE) { state.dragstart.x += UVCANVAS_ROTATE_RADIUS; }
    state.draghandle = UVCanvasPickHandle(&state, state.dragstart.x, state.dragstart.y);
    assert(state.draghandle == 3);
    UVCanvasSelectionPosition(&state, state.pivot);
    state.dragorigin = origin; state.lastangle = state.dragangle = 0;
    capture = canvas;
}

int main(void)
{
    for (int mode = TRANSFORM_MOVE; mode <= TRANSFORM_SCALE; mode++)
    {
        Fixture((TransformMode)mode); BeginDrag();
        UVCanvasNode *nodes = state.nodes;
        int before = refreshes;
        int x = state.dragstart.x + 36, y = state.dragstart.y - 36;
        if (mode == TRANSFORM_ROTATE) { x = state.dragorigin.x; y = state.dragorigin.y - UVCANVAS_ROTATE_RADIUS; }
        UVCanvasDrag(canvas, &state, x, y);
        assert(state.draghandle == 3 && capture == canvas);
        assert(state.nodes == nodes && refreshes == before && !commits);
        assert(state.nodes[1].source.s == 1024); /* Source stays intact during preview. */
        assert(state.triangles[0].uv[0][0] != 0);
        assert(state.values[0] == (mode == TRANSFORM_MOVE ? .18 : mode == TRANSFORM_ROTATE ? 90 : 1.5));
        UVCanvasDrag(canvas, &state, x, y);
        assert(state.draghandle == 3 && capture == canvas && !commits);
        assert(UVCanvasCommit(canvas, &state));
        assert(commits == 1 && refreshes == before + 1 && !state.draghandle && !capture);
        assert(source.source[0].s != 0);
        for (int i = 0; i < 3; i++) { assert(state.nodes[i].selected); }

        Fixture((TransformMode)mode); BeginDrag();
        UVCanvasDrag(canvas, &state, state.dragstart.x + 36, state.dragstart.y - 36);
        assert(UVCanvasCancelInteraction(canvas) && !commits && !capture);
        assert(state.triangles[0].uv[0][0] == 0 && state.nodes[1].source.s == 1024);
    }
    /* Setup compaction still refreshes cached inspector command offsets. */
    SetupObject object = {.sourceoffset = 512};
    g_CurrentSetup.objects = &object; g_CurrentSetup.objectcount = 1; objectSelected = TRUE;
    int before = refreshes;
    GEditorRefreshHistoryMenu((HWND)5);
    assert(inspectorUpdates && inspectorOffset == 512 && refreshes == before);
    object.sourceoffset = 128;
    GEditorRefreshHistoryMenu((HWND)5);
    assert(inspectorOffset == 128 && refreshes == before);
    free(state.nodes); free(state.triangles);
    puts("PASS: Move/Rotate/Scale survive history refresh; previews commit once, cancellation restores UVs, and setup inspector offsets still refresh.");
    return 0;
}
