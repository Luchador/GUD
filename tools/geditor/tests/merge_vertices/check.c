#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
BOOL SetupFileCompact(SetupFile *setup, const char **why) { (void)setup; (void)why; abort(); }
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
    Counts(&loaded);
    /* Native compilation may emit separate vertex batches for each layer.
     * Compare the merged source's attributes and total references, not indices. */
    DWORD mergeduses = 0;
    for (DWORD i = 0; i < loaded.rooms[1].vertexcount; i++)
    {
        const BgDocumentVertex *p = &loaded.rooms[1].vertices[i];
        if (p->x == 150 && p->y == 0 && p->z == 0)
        {
            assert(p->s == -129 && p->t == 10 && p->r == 61 && p->g == 111 && p->b == 120 && p->a == 151);
            mergeduses += p->usecount;
        }
    }
    assert(mergeduses == 3);
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
       ID_GEOMETRY_MERGE_VERTICES = 100, ID_GEOMETRY_WELD_VERTICES, MB_ICONERROR = 16, MB_OKCANCEL = 1, MB_ICONWARNING = 48, IDOK = 1, IDCANCEL = 2 };
#define GEDITOR_TITLE "GEditor"
static HWND g_Viewport = (HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BgDocumentVertexRef selection[16];
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
static BOOL ViewportSelectBgVertices(HWND hwnd, const BgDocumentVertexRef *refs, DWORD count)
{
    if (failselection) { return FALSE; }
    assert(count <= 16);
    memcpy(selection, refs, count*sizeof(*refs)); selectedcount = count; return TRUE;
}
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ rebuilds++; if (failrebuild) { failrebuild = FALSE; *why = "test rebuild failure"; return FALSE; } return TRUE; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd)
{
    selectedcount = (DWORD)(g_EditHistory.selectionsize/sizeof(*selection));
    assert(selectedcount <= 16); memcpy(selection, g_EditHistory.selection, g_EditHistory.selectionsize); restores++;
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
static BOOL GEditorWeldSelectedBgVertices(HWND hwnd);
static void SendMessage(HWND hwnd, unsigned message, uintptr_t wparam, LPARAM lparam)
{
    assert(hwnd == (HWND)1 && message == WM_COMMAND); sent++;
    if (wparam == ID_GEOMETRY_MERGE_VERTICES) { GEditorMergeSelectedBgVertices(hwnd); }
    else { assert(wparam == ID_GEOMETRY_WELD_VERTICES); GEditorWeldSelectedBgVertices(hwnd); }
}
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


static void MakeWeld(BgDocument *doc)
{
    MakeStrip(doc);
    BgDocumentRoom *r = &doc->rooms[1];
    r->vertices = realloc(r->vertices, 9*sizeof(*r->vertices)); assert(r->vertices);
    r->vertices[4].x = 100; /* group A: 1,4,8 */
    r->vertices[5].x = 0;   /* group B: 2,5 */
    for (DWORD v = 6; v < 9; v++)
    {
        r->vertices[v] = r->vertices[v == 8 ? 1 : 0];
        r->vertices[v].id = doc->nextvertexid++; r->vertices[v].usecount = 0;
    }
    r->vertices[6].x = 1; /* one native unit away, even at tiny world scales */
    r->vertices[8].s = 101;
    r->vertexcount = 9;
    /* A second real room with coincident native positions; identities stay local. */
    BgDocument copy = {0}; const char *why = "";
    assert(doc->roomcount == 2 && BgDocumentClone(doc, &copy, &why));
    BgDocumentRoom empty = doc->rooms[2]; doc->rooms[2] = copy.rooms[1]; copy.rooms[1] = empty;
    r = &doc->rooms[2];
    for (DWORD v = 0; v < r->vertexcount; v++) { r->vertices[v].room = 2; r->vertices[v].id = doc->nextvertexid++; }
    for (DWORD f = 0; f < r->facecount; f++) { r->faces[f].room = 2; r->faces[f].id = doc->nextfaceid++; }
    doc->facecount += r->facecount;
    BgDocumentFree(&copy);
}

static void WeldGeometry(void)
{
    BgFile source = Fixture(); BgDocument original = {0}, doc = {0}; const char *why = "";
    assert(BgDocumentLoad(source.data, source.size, 1000, &original, &why)); MakeWeld(&original);
    const BgDocumentVertexRef input[] = {{1,8},{1,4},{1,1},{1,2},{1,5},{1,6},{1,0},{1,8},{2,4},{2,1},{2,0}};
    BgDocumentVertexRef refs[11]; DWORD count, removed;
    assert(BgDocumentClone(&original, &doc, &why));
    memcpy(refs, input, sizeof(refs)); count = 11;
    assert(BgDocumentWeldVertices(&doc, refs, &count, &removed, &why));
    assert(removed == 4 && count == 6 && doc.dirty && doc.facecount == original.facecount);
    assert(doc.rooms[1].vertexcount == 6 && doc.rooms[2].vertexcount == 8);
    assert(doc.rooms[1].vertices[1].s == -52); /* each distinct source gets equal weight */
    assert(doc.rooms[1].vertices[1].id == original.rooms[1].vertices[1].id);
    assert(doc.rooms[1].vertices[1].flag == original.rooms[1].vertices[1].flag);
    assert(doc.rooms[1].vertices[4].x == 1 && doc.rooms[1].vertices[4].id == original.rooms[1].vertices[6].id);
    assert(doc.rooms[1].vertices[5].id == original.rooms[1].vertices[7].id); /* unselected coincident source stays */
    assert(doc.nextvertexid == original.nextvertexid && doc.nextfaceid == original.nextfaceid);
    Counts(&doc);
    for (DWORD i = 0; i < count; i++)
    {
        assert(refs[i].index < doc.rooms[refs[i].room].vertexcount);
        for (DWORD j = 0; j < i; j++) { assert(memcmp(&refs[i], &refs[j], sizeof(refs[i]))); }
    }
    for (DWORD room = 1; room <= doc.roomcount; room++)
    for (DWORD f = 0; f < doc.rooms[room].facecount; f++)
    {
        BgDocumentFace face = doc.rooms[room].faces[f];
        const BgDocumentFace *before = &original.rooms[room].faces[f];
        for (int c = 0; c < 3; c++)
        {
            const BgDocumentVertex *v = &doc.rooms[room].vertices[face.vertexindices[c]];
            const BgDocumentVertex *w = &original.rooms[room].vertices[before->vertexindices[c]];
            assert(v->x == w->x && v->y == w->y && v->z == w->z);
        }
        memcpy(face.vertexindices, before->vertexindices, sizeof(face.vertexindices));
        assert(!memcmp(&face, before, sizeof(face))); /* materials, flags, layers, winding, IDs */
    }
    /* Idempotence retains the document, selection and dirty state. */
    BgDocument welded = {0}; assert(BgDocumentClone(&doc, &welded, &why));
    assert(BgDocumentWeldVertices(&doc, refs, &count, &removed, &why) && !removed && count == 6);
    Same(&doc, &welded); BgDocumentFree(&welded);
    BgFile compiled = {0}; BgDocument loaded = {0};
    assert(BgDocumentCompile(&doc, &source, &compiled, &why));
    assert(BgFileValidateVertexBatches(&compiled, &why));
    assert(BgDocumentLoad(compiled.data, compiled.size, doc.levelscale, &loaded, &why));
    assert(loaded.facecount == doc.facecount); Counts(&loaded);
    BgDocumentFree(&loaded); BgFileFree(&compiled); BgDocumentFree(&doc);
    /* Fail each allocation, including after room 1 has been staged. */
    for (int budget = 0; budget < 8; budget++)
    {
        assert(BgDocumentClone(&original, &doc, &why));
        memcpy(refs, input, sizeof(refs)); count = 11; allocations = budget;
        assert(!BgDocumentWeldVertices(&doc, refs, &count, &removed, &why)); allocations = -1;
        assert(!removed && count == 11 && !memcmp(refs, input, sizeof(refs)));
        Same(&doc, &original); BgDocumentFree(&doc);
    }
    assert(BgDocumentClone(&original, &doc, &why));
    BgDocumentVertexRef nohits[] = {{1,0},{2,0},{1,6},{1,0}}; count = 4;
    assert(BgDocumentWeldVertices(&doc, nohits, &count, &removed, &why) && !removed && count == 4);
    Same(&doc, &original); /* no cross-room weld; 1 native unit is not <= 0.1 */
    BgDocumentVertexRef bad[] = {{1,1},{1,4},{2,999}}; count = 3;
    assert(!BgDocumentWeldVertices(&doc, bad, &count, &removed, &why)); Same(&doc, &original);
    doc.rooms[2].faces[0].vertexindices[0] = 99;
    BgDocument invalid = {0}; assert(BgDocumentClone(&doc, &invalid, &why));
    memcpy(refs, input, sizeof(refs)); count = 11;
    assert(!BgDocumentWeldVertices(&doc, refs, &count, &removed, &why)); Same(&doc, &invalid);
    BgDocumentFree(&invalid); BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: multi-group/multi-room welds, native threshold, equal-weight attributes, stable IDs, unselected vertices, unchanged face geometry, save/reload, idempotence and atomic failures.");
}

static void WeldCommands(void)
{
    BgFile source = Fixture(); BgDocument original = {0}, welded = {0}; const char *why = "";
    HWND frame = (HWND)1; MSG msg = {.hwnd=g_Viewport, .message=WM_KEYDOWN, .wParam='W'};
    errors = warnings = rebuilds = restores = sent = 0;
    assert(BgDocumentLoad(source.data, source.size, 1, &g_CurrentBgDocument, &why)); MakeWeld(&g_CurrentBgDocument);
    assert(BgDocumentClone(&g_CurrentBgDocument, &original, &why));
    /* A no-op does not rebuild, dirty the document, clear selection or add history. */
    selection[0] = (BgDocumentVertexRef){1,0}; selection[1] = (BgDocumentVertexRef){2,0}; selectedcount = 2;
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory, selection, selectedcount*sizeof(*selection), FALSE, &why));
    assert(GEditorWeldSelectedBgVertices(frame) && !g_EditHistory.undocount && !rebuilds && selectedcount == 2);
    Same(&g_CurrentBgDocument, &original);
    selection[0] = (BgDocumentVertexRef){1,1}; selection[1] = (BgDocumentVertexRef){1,4};
    selection[2] = (BgDocumentVertexRef){2,1}; selection[3] = (BgDocumentVertexRef){2,4}; selectedcount = 4;
    assert(EditHistorySetSelection(&g_EditHistory, selection, selectedcount*sizeof(*selection), FALSE, &why));
    for (int failure = 0; failure < 3; failure++)
    {
        ULONGLONG revision = g_EditHistory.nextrevision;
        failrebuild = failure == 0; failselection = failure == 1;
        if (failure == 2) { g_EditHistory.nextrevision = 0; }
        assert(!GEditorWeldSelectedBgVertices(frame));
        g_EditHistory.nextrevision = revision; failselection = FALSE;
        Same(&g_CurrentBgDocument, &original); assert(selectedcount == 4 && !g_EditHistory.undocount);
    }
    assert(!errors && !warnings && restores == 3);
    assert(!GEditorHandleWeldVerticesHotkey(frame, &msg)); /* plain W remains translation */
    control = TRUE;
    assert(GEditorHandleWeldVerticesHotkey(frame, &msg) && sent == 1 && g_EditHistory.undocount == 1);
    assert(selectedcount == 2 && g_CurrentBgDocument.rooms[1].vertexcount == 8 && g_CurrentBgDocument.rooms[2].vertexcount == 8);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Weld Vertices"));
    assert(BgDocumentClone(&g_CurrentBgDocument, &welded, &why));
    assert(EditHistorySetSelection(&g_EditHistory, selection, selectedcount*sizeof(*selection), FALSE, &why));
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    GEditorRestoreHistorySelection(frame); Same(&g_CurrentBgDocument, &original); assert(selectedcount == 4);
    msg.lParam = (LPARAM)1 << 30;
    assert(GEditorHandleWeldVerticesHotkey(frame, &msg) && sent == 1); msg.lParam = 0;
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    GEditorRestoreHistorySelection(frame); Same(&g_CurrentBgDocument, &welded); assert(selectedcount == 2);
    const char *inputs[] = {"Edit", "ComboBox", "ComboLBox"};
    for (unsigned i = 0; i < 3; i++) { classname = inputs[i]; assert(!GEditorHandleWeldVerticesHotkey(frame, &msg)); }
    classname = "GEditorViewport";
    msg.hwnd = (HWND)3; assert(!GEditorHandleWeldVerticesHotkey(frame, &msg)); msg.hwnd = g_Viewport;
    alt = TRUE; assert(!GEditorHandleWeldVerticesHotkey(frame, &msg)); alt = FALSE;
    shift = TRUE; assert(!GEditorHandleWeldVerticesHotkey(frame, &msg)); shift = FALSE;
    flying = TRUE; assert(!GEditorHandleWeldVerticesHotkey(frame, &msg)); flying = FALSE;
    transforming = TRUE; assert(!GEditorHandleWeldVerticesHotkey(frame, &msg)); transforming = FALSE;
    control = FALSE;
    assert(sent == 1 && !errors && !warnings);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&original); BgDocumentFree(&welded); BgFileFree(&source);
    puts("PASS: silent no-op/refusal, transactional rollback, one undo/redo step with selection, Ctrl+W scoping and repeat suppression.");
}

int main(int argc, char **argv)
{ assert(argc == 2); Geometry(argv[1]); Commands(); WeldGeometry(); WeldCommands(); return 0; }
