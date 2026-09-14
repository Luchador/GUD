#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations = -1;
static BOOL FailAllocation(void) { if (!allocations) { return TRUE; } if (allocations > 0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t size) { return FailAllocation() ? NULL : malloc(size); }
static void *TestCalloc(size_t count, size_t size) { return FailAllocation() ? NULL : calloc(count, size); }
#define malloc TestMalloc
#define calloc TestCalloc
#include "bgmerge.c"
#undef malloc
#undef calloc
#include "fixture.inc"

static void Counts(const BgDocument *doc)
{
    for (DWORD room = 1; room <= doc->roomcount; room++)
    {
        const BgDocumentRoom *r = &doc->rooms[room];
        for (DWORD v = 0; v < r->vertexcount; v++)
        {
            DWORD count = 0;
            for (DWORD f = 0; f < r->facecount; f++) for (int c = 0; c < 3; c++) { count += r->faces[f].vertexindices[c] == v; }
            assert(count == r->vertices[v].usecount);
        }
    }
}
static void Same(const BgDocument *a, const BgDocument *b)
{
    assert(a->facecount == b->facecount && a->roomcount == b->roomcount && a->dirty == b->dirty);
    assert(a->nextvertexid == b->nextvertexid && a->nextfaceid == b->nextfaceid);
    for (DWORD room = 1; room <= a->roomcount; room++)
    {
        const BgDocumentRoom *x = &a->rooms[room], *y = &b->rooms[room];
        assert(x->vertexcount == y->vertexcount && x->facecount == y->facecount);
        if (x->vertexcount) { assert(!memcmp(x->vertices, y->vertices, x->vertexcount*sizeof(*x->vertices))); }
        if (x->facecount) { assert(!memcmp(x->faces, y->faces, x->facecount*sizeof(*x->faces))); }
    }
}
static void MakeStrip(BgDocument *doc)
{
    BgDocumentRoom *r = &doc->rooms[1];
    r->vertices = realloc(r->vertices, 6*sizeof(*r->vertices)); assert(r->vertices);
    r->faces = realloc(r->faces, 4*sizeof(*r->faces)); assert(r->faces);
    const short positions[6][2] = {{0,0},{100,0},{0,100},{100,100},{200,0},{200,100}};
    const DWORD triangles[4][3] = {{0,1,2},{1,3,2},{1,4,3},{4,5,3}};
    for (int v = 0; v < 6; v++)
    {
        if (v >= 3) { r->vertices[v] = r->vertices[0]; r->vertices[v].id = doc->nextvertexid++; }
        BgDocumentVertex *p = &r->vertices[v];
        p->x = positions[v][0]; p->y = positions[v][1]; p->z = 0;
        p->s = p->x*8; p->t = p->y*8; p->usecount = 0;
    }
    r->vertices[1].s = -321; r->vertices[4].s = 64;
    r->vertices[1].t = 30; r->vertices[4].t = -11;
    r->vertices[1].r = 10; r->vertices[1].g = 70; r->vertices[1].b = 230; r->vertices[1].a = 80;
    r->vertices[4].r = 111; r->vertices[4].g = 151; r->vertices[4].b = 10; r->vertices[4].a = 221;
    r->vertices[1].flag = 0xab;
    r->layers[1].groups = calloc(1, sizeof(*r->layers[1].groups)); assert(r->layers[1].groups);
    r->layers[1].groupcount = r->layers[1].groupcapacity = 1; r->layers[1].sourcepresent = TRUE;
    r->faces[0].textureid = 17; r->faces[0].cullbackfaces = TRUE; BgMaterialSetTexture(&r->faces[0].material, 17);
    for (int f = 0; f < 4; f++)
    {
        if (f) { r->faces[f] = r->faces[0]; r->faces[f].id = doc->nextfaceid++; }
        r->faces[f].layer = f%2;
        memcpy(r->faces[f].vertexindices, triangles[f], sizeof(triangles[f]));
        for (int c = 0; c < 3; c++) { r->vertices[triangles[f][c]].usecount++; }
    }
    r->vertexcount = 6; r->facecount = r->facecapacity = doc->facecount = 4;
}

static void Geometry(const char *dir)
{
    BgFile source = Fixture(); BgDocument original = {0}, doc = {0}; const char *why = "";
    BgDocumentVertexRef refs[] = {{1,4},{1,1},{1,4}}, out; DWORD removed, deleted;
    assert(BgDocumentLoad(source.data, source.size, .1f, &original, &why)); MakeStrip(&original);
    assert(BgDocumentClone(&original, &doc, &why));
    /* Duplicate references and unequal incident-face counts must not bias the mean. */
    assert(BgDocumentMergeVertices(&doc, refs, 3, &out, &removed, &deleted, &why));
    assert(out.room == 1 && out.index == 1 && removed == 1 && deleted == 1);
    assert(doc.rooms[1].vertexcount == 5 && doc.facecount == 3 && doc.dirty);
    const BgDocumentVertex *v = &doc.rooms[1].vertices[1];
    assert(v->x == 150 && v->y == 0 && v->z == 0 && v->s == -129 && v->t == 10);
    assert(v->r == 61 && v->g == 111 && v->b == 120 && v->a == 151 && v->flag == 0xab);
    assert(v->id == original.rooms[1].vertices[1].id && doc.nextvertexid == original.nextvertexid);
    Counts(&doc);
    for (DWORD f = 0; f < doc.rooms[1].facecount; f++)
    {
        BgDocumentFace actual = doc.rooms[1].faces[f];
        const BgDocumentFace *before = &original.rooms[1].faces[f < 2 ? f : f+1];
        memcpy(actual.vertexindices, before->vertexindices, sizeof(actual.vertexindices));
        assert(!memcmp(&actual, before, sizeof(actual))); /* IDs, material, layer, winding order. */
    }
    for (DWORD i = 0; i < doc.rooms[1].vertexcount; i++) if (i != 1)
    {
        BgDocumentVertex actual = doc.rooms[1].vertices[i];
        const BgDocumentVertex *before = &original.rooms[1].vertices[i < 4 ? i : i+1];
        actual.usecount = before->usecount; assert(!memcmp(&actual, before, sizeof(actual)));
    }
    RoundTrip(&doc, &source, dir);
    BgFile compiled = {0}; BgDocument loaded = {0};
    assert(BgDocumentCompile(&doc, &source, &compiled, &why));
    assert(BgDocumentLoad(compiled.data, compiled.size, doc.levelscale, &loaded, &why));
    assert(loaded.rooms[1].vertexcount == 5); Counts(&loaded);
    assert(loaded.rooms[1].vertices[1].usecount == 3); /* true shared identity survives native output */
    BgDocumentFree(&loaded); BgFileFree(&compiled); BgDocumentFree(&doc);
    /* Only one selected corner in each collapsed face: catch coincident/collinear geometry too. */
    assert(BgDocumentClone(&original, &doc, &why));
    BgDocumentVertexRef other[] = {{1,2},{1,5}};
    assert(BgDocumentMergeVertices(&doc, other, 2, &out, &removed, &deleted, &why) && deleted == 2);
    Counts(&doc); RoundTrip(&doc, &source, dir); BgDocumentFree(&doc);
    /* Coincident vertices with differing attributes still weld. No proximity threshold. */
    assert(BgDocumentClone(&original, &doc, &why)); doc.rooms[1].vertices[4].x = 100;
    assert(BgDocumentMergeVertices(&doc, refs, 3, &out, &removed, &deleted, &why) && removed == 1 && deleted == 0);
    Counts(&doc); BgDocumentFree(&doc);
    /* Quantization stays in the 16-bit range, including negative halves. */
    assert(BgDocumentClone(&original, &doc, &why));
    doc.rooms[1].vertices[1].x = -32768; doc.rooms[1].vertices[4].x = 32767;
    doc.rooms[1].vertices[1].y = -20; doc.rooms[1].vertices[4].y = -21;
    doc.rooms[1].vertices[1].z = 11; doc.rooms[1].vertices[4].z = 12;
    assert(BgDocumentMergeVertices(&doc, refs, 3, &out, &removed, &deleted, &why));
    assert(doc.rooms[1].vertices[1].x == -1 && doc.rooms[1].vertices[1].y == -21 && doc.rooms[1].vertices[1].z == 12);
    BgDocumentFree(&doc);
    for (int budget = 0; budget < 4; budget++)
    {
        assert(BgDocumentClone(&original, &doc, &why)); allocations = budget;
        assert(!BgDocumentMergeVertices(&doc, refs, 3, &out, &removed, &deleted, &why)); allocations = -1;
        assert(!removed && !deleted && why[0]); Same(&doc, &original); BgDocumentFree(&doc);
    }
    assert(BgDocumentClone(&original, &doc, &why));
    BgDocumentVertexRef bad[][2] = {{{1,1},{2,0}},{{1,1},{1,99}},{{1,1},{1,1}},{{0,1},{0,2}}};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(*bad); i++)
    { assert(!BgDocumentMergeVertices(&doc, bad[i], 2, &out, &removed, &deleted, &why)); Same(&doc, &original); }
    assert(!BgDocumentMergeVertices(&doc, refs, 1, &out, NULL, NULL, NULL)); Same(&doc, &original);
    BgDocumentFree(&doc);
    /* Merging a whole triangle removes it without leaving an invalid referenced vertex. */
    BgFile single = Fixture(); assert(BgDocumentLoad(single.data, single.size, 1, &doc, &why));
    BgDocumentVertexRef all[] = {{1,0},{1,1},{1,2}};
    assert(BgDocumentMergeVertices(&doc, all, 3, &out, &removed, &deleted, &why));
    assert(removed == 2 && deleted == 1 && doc.facecount == 0 && doc.rooms[1].vertexcount == 1);
    Counts(&doc); RoundTrip(&doc, &single, dir);
    BgDocumentFree(&doc); BgFileFree(&single); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: equal-weight XYZ/RGBA/UV averaging, native rounding, both layers, compacted shared vertices, collapse detection, invalid selections/allocation failure, save/reload and ROM batch validation.");
}

/* Real editor transaction and hotkey logic; only window/viewport calls are stubbed. */
typedef void *HWND;
typedef intptr_t LPARAM;
typedef struct MSG { HWND hwnd; unsigned message; uintptr_t wParam; LPARAM lParam; } MSG;
enum { WM_KEYDOWN = 256, WM_COMMAND = 273, VK_CONTROL = 17, VK_MENU = 18, VK_SHIFT = 16,
       ID_GEOMETRY_MERGE_VERTICES = 100, MB_ICONERROR = 16, MB_OKCANCEL = 1, MB_ICONWARNING = 48, IDOK = 1, IDCANCEL = 2 };
#define GEDITOR_TITLE "GEditor"
static HWND g_Viewport = (HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BgDocumentVertexRef selection[4];
static DWORD selectedcount = 2;
static int rebuilds, errors, warnings, restores;
static BOOL flying, transforming, control, alt, shift, snap, stan, failrebuild, failselection;
static int response = IDOK;
static EditorTool tool = EDITOR_TOOL_VERTEX_SELECT;
static const char *classname = "GEditorViewport";
static unsigned sent;
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportGetVertexSnap(HWND hwnd) { return snap; }
static DWORD ViewportGetStanSelectionCount(HWND hwnd, DWORD *tile) { return stan ? 1 : 0; }
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static int ViewportGetSelectedComponentCount(HWND hwnd) { return (int)selectedcount; }
static BgDocumentVertexRef *ViewportGetMoveVertices(HWND hwnd, DWORD *count)
{
    BgDocumentVertexRef *refs = malloc(selectedcount*sizeof(*refs)); assert(refs);
    memcpy(refs, selection, selectedcount*sizeof(*refs)); *count = selectedcount; return refs;
}
static BOOL ViewportSelectBgVertex(HWND hwnd, const BgDocumentVertexRef *ref)
{
    if (failselection) { return FALSE; }
    assert(ref && ref->index < g_CurrentBgDocument.rooms[ref->room].vertexcount);
    selection[0] = *ref; selectedcount = 1; return TRUE;
}
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ rebuilds++; if (failrebuild) { failrebuild = FALSE; *why = "test rebuild failure"; return FALSE; } return TRUE; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd)
{
    selectedcount = (DWORD)(g_EditHistory.selectionsize/sizeof(*selection));
    assert(selectedcount <= 4); memcpy(selection, g_EditHistory.selection, g_EditHistory.selectionsize); restores++;
}
static int MessageBox(HWND hwnd, const char *why, const char *title, unsigned flags)
{
    assert(why[0]);
    if (flags & MB_OKCANCEL) { assert(strstr(why, "collapse")); warnings++; return response; }
    errors++; return IDOK;
}
static BOOL IsChild(HWND parent, HWND child) { return parent == (HWND)1 && child == (HWND)2; }
static short GetKeyState(int key) { return (key == VK_CONTROL ? control : key == VK_MENU ? alt : shift) ? (short)0x8000 : 0; }
static void GetClassName(HWND hwnd, char *out, int size) { snprintf(out, size, "%s", classname); }
static BOOL GEditorMergeSelectedBgVertices(HWND hwnd);
static void SendMessage(HWND hwnd, unsigned message, uintptr_t wparam, LPARAM lparam)
{ assert(hwnd == (HWND)1 && message == WM_COMMAND && wparam == ID_GEOMETRY_MERGE_VERTICES); sent++; GEditorMergeSelectedBgVertices(hwnd); }
#include "editor.inc"

static void Commands(void)
{
    BgFile source = Fixture(); BgDocument original = {0}, merged = {0}; const char *why = "";
    HWND frame = (HWND)1; MSG msg = {.hwnd = g_Viewport, .message = WM_KEYDOWN, .wParam = 'M'};
    assert(BgDocumentLoad(source.data, source.size, 1, &g_CurrentBgDocument, &why)); MakeStrip(&g_CurrentBgDocument);
    assert(BgDocumentClone(&g_CurrentBgDocument, &original, &why));
    selection[0] = (BgDocumentVertexRef){1,1}; selection[1] = (BgDocumentVertexRef){1,4};
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory, selection, selectedcount*sizeof(*selection), FALSE, &why));
    response = IDCANCEL;
    assert(!GEditorMergeSelectedBgVertices(frame) && warnings == 1 && errors == 0 && rebuilds == 0);
    Same(&g_CurrentBgDocument, &original); assert(selectedcount == 2 && g_EditHistory.undocount == 0);
    response = IDOK;
    for (int failure = 0; failure < 3; failure++)
    {
        ULONGLONG revision = g_EditHistory.nextrevision;
        failrebuild = failure == 0; failselection = failure == 1;
        if (failure == 2) { g_EditHistory.nextrevision = 0; }
        assert(!GEditorMergeSelectedBgVertices(frame));
        g_EditHistory.nextrevision = revision; failselection = FALSE;
        Same(&g_CurrentBgDocument, &original); assert(selectedcount == 2 && g_EditHistory.undocount == 0);
    }
    assert(errors == 3 && restores == 3);
    assert(GEditorCanMergeSelectedBgVertices());
    snap = TRUE; assert(!GEditorCanMergeSelectedBgVertices()); snap = FALSE;
    stan = TRUE; assert(!GEditorCanMergeSelectedBgVertices()); stan = FALSE;
    tool = EDITOR_TOOL_EDGE_SELECT; assert(!GEditorCanMergeSelectedBgVertices()); tool = EDITOR_TOOL_VERTEX_SELECT;
    assert(GEditorHandleMergeVerticesHotkey(frame, &msg) && sent == 1 && g_EditHistory.undocount == 1);
    assert(selectedcount == 1 && selection[0].index == 1 && g_CurrentBgDocument.facecount == 3);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Merge Vertices"));
    assert(BgDocumentClone(&g_CurrentBgDocument, &merged, &why));
    /* The outer frame records the post-edit selection with the same history entry. */
    assert(EditHistorySetSelection(&g_EditHistory, selection, selectedcount*sizeof(*selection), FALSE, &why));
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    GEditorRestoreHistorySelection(frame); Same(&g_CurrentBgDocument, &original);
    assert(selectedcount == 2 && selection[1].index == 4);
    msg.lParam = (LPARAM)1 << 30;
    assert(GEditorHandleMergeVerticesHotkey(frame, &msg) && sent == 1); /* repeats cannot merge again after Undo */
    msg.lParam = 0;
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    GEditorRestoreHistorySelection(frame); Same(&g_CurrentBgDocument, &merged); assert(selectedcount == 1);
    const char *inputs[] = {"Edit", "ComboBox", "ComboLBox"};
    for (unsigned i = 0; i < 3; i++) { classname = inputs[i]; assert(!GEditorHandleMergeVerticesHotkey(frame, &msg)); }
    classname = "GEditorViewport";
    msg.hwnd = (HWND)3; assert(!GEditorHandleMergeVerticesHotkey(frame, &msg)); msg.hwnd = g_Viewport;
    control = TRUE; assert(!GEditorHandleMergeVerticesHotkey(frame, &msg)); control = FALSE;
    alt = TRUE; assert(!GEditorHandleMergeVerticesHotkey(frame, &msg)); alt = FALSE;
    shift = TRUE; assert(!GEditorHandleMergeVerticesHotkey(frame, &msg)); shift = FALSE;
    flying = TRUE; assert(!GEditorHandleMergeVerticesHotkey(frame, &msg)); flying = FALSE;
    transforming = TRUE; assert(!GEditorHandleMergeVerticesHotkey(frame, &msg)); transforming = FALSE;
    assert(sent == 1);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&original); BgDocumentFree(&merged); BgFileFree(&source);
    puts("PASS: collapse OK/Cancel, rebuild/selection/commit rollback, undo/redo restores topology and selection, menu guards, and M scoping/repeat suppression.");
}
int main(int argc, char **argv) { assert(argc == 2); Geometry(argv[1]); Commands(); return 0; }
