#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
BOOL SetupFileCompact(SetupFile *setup, const char **why) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations = -1;
static void *TestMalloc(size_t n)
{ if (!allocations) { return NULL; } if (allocations > 0) { allocations--; } return malloc(n); }
#define malloc TestMalloc
#include "bgdocument.c"
#undef malloc
#include "fixture.inc"

static void Same(const BgDocument *a, const BgDocument *b)
{
    assert(a->facecount == b->facecount && a->roomcount == b->roomcount && a->dirty == b->dirty);
    assert(a->nextvertexid == b->nextvertexid && a->nextfaceid == b->nextfaceid);
    for (DWORD r = 1; r <= a->roomcount; r++)
    {
        const BgDocumentRoom *x = &a->rooms[r], *y = &b->rooms[r];
        assert(x->facecount == y->facecount && x->vertexcount == y->vertexcount);
        assert(!memcmp(x->origin, y->origin, sizeof(x->origin)));
        assert(!memcmp(x->vertices, y->vertices, x->vertexcount * sizeof(*x->vertices)));
        assert(!memcmp(x->faces, y->faces, x->facecount * sizeof(*x->faces)));
    }
}

static void Prepare(BgDocument *doc)
{
    const float origins[2][3] = {{100,-70,30}, {160,-20,-10}};
    const short positions[2][3][3] = {{{-8,5,-3}, {2,-7,9}, {5,11,2}},
                                     {{-11,-6,-10}, {4,3,12}, {8,10,1}}};
    for (DWORD r = 1; r <= 2; r++)
    {
        BgDocumentRoom *room = &doc->rooms[r];
        assert(room->vertexcount == 3);
        memcpy(room->origin, origins[r-1], sizeof(room->origin));
        for (DWORD v = 0; v < 3; v++)
        {
            BgDocumentVertex *p = &room->vertices[v];
            p->x = positions[r-1][v][0]; p->y = positions[r-1][v][1]; p->z = positions[r-1][v][2];
            p->s = (short)(v*71-100); p->t = (short)(v*113+34);
            p->r = 73+v; p->g = 116+v; p->b = 49+v; p->a = 190+v;
        }
        for (DWORD f = 0; f < room->facecount; f++) { room->faces[f].uvseams = f%8; }
    }
}

static short Position(const BgDocumentVertex *v, int axis)
{ return axis == 0 ? v->x : axis == 1 ? v->y : v->z; }

static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled = {0}, saved = {0}; BgDocument loaded = {0}, before = {0}; const char *why = "";
    assert(BgDocumentClone(doc, &before, &why));
    assert(BgDocumentCompile(doc, source, &compiled, &why));
    assert(BgFileValidateVertexBatches(&compiled, &why));
    assert(BgDocumentLoad(compiled.data, compiled.size, doc->levelscale, &loaded, &why));
    Equivalent(doc, &loaded);
    BgDocumentFree(&loaded); BgFileFree(&compiled);
    assert(BgDocumentCompileProject(doc, source, &compiled, &why));
    char path[MAX_PATH]; snprintf(path, sizeof(path), "%s/bg", dir); CreateDirectory(path, NULL);
    assert(BgSaveProjectFile(dir, &compiled, &why));
    assert(BgLoadProjectFile(dir, compiled.name, &saved, &why));
    assert(BgDocumentLoad(saved.data, saved.size, doc->levelscale, &loaded, &why));
    Equivalent(doc, &loaded);
    for (DWORD r = 1; r <= doc->roomcount; r++)
    {
        const BgDocumentRoom *a = &doc->rooms[r], *b = &loaded.rooms[r];
        assert(a->vertexcount == b->vertexcount && a->facecount == b->facecount);
        for (DWORD f = 0; f < a->facecount; f++)
        {
            /* Seam guides use a separate project sidecar, not the BG data.
               Their corner remapping is checked on the live edit below. */
            assert(!memcmp(a->faces[f].vertexindices, b->faces[f].vertexindices,
                           sizeof(a->faces[f].vertexindices)));
        }
    }
    Same(doc, &before);
    BgDocumentFree(&before); BgDocumentFree(&loaded); BgFileFree(&compiled); BgFileFree(&saved);
}

static void Geometry(const char *dir)
{
    BgFile source = Fixture(); BgDocument original = {0}, doc = {0};
    BgFaceRef refs[20], selected[4]; const char *why = "";
    assert(BgDocumentLoad(source.data, source.size, 0.5f, &original, &why)); Prepare(&original);
    assert(Refs(&original, refs) == 20);
    /* Asymmetric triangles distinguish a bounding-box center from a centroid.
       Multiple faces share vertices across primary/secondary layers. */
    selected[0] = refs[1]; selected[1] = refs[6]; selected[2] = refs[11]; selected[3] = refs[16];
    const short single[3][3] = {{5,-5,-8}, {-1,11,-7}, {9,-3,4}};
    const double plane[3] = {130, -43.5, 9.5};
    for (unsigned axis = 0; axis < 3; axis++) for (int group = 0; group <= 1; group++)
    {
        assert(BgDocumentClone(&original, &doc, &why));
        assert(BgDocumentMirrorFaces(&doc, selected, group ? 4 : 1, axis, &why));
        assert(doc.dirty && doc.facecount == original.facecount);
        assert(doc.nextvertexid == original.nextvertexid && doc.nextfaceid == original.nextfaceid);
        for (DWORD r = 1; r <= 2; r++)
        {
            const BgDocumentRoom *a = &original.rooms[r], *b = &doc.rooms[r];
            assert(a->vertexcount == b->vertexcount);
            for (DWORD v = 0; v < 3; v++)
            {
                const BgDocumentVertex *p = &a->vertices[v], *q = &b->vertices[v];
                BgDocumentVertex expected = *p;
                if (r == 1 || group)
                {
                    short destination = group
                        ? (short)(2*plane[axis] - 2*a->origin[axis] - Position(p, axis)) : single[axis][v];
                    if (axis == 0) { expected.x = destination; }
                    else if (axis == 1) { expected.y = destination; }
                    else { expected.z = destination; }
                    /* A reflected point and its source have the same midpoint. */
                    if (group) { assert((Position(p, axis)+Position(q, axis))/2.0+a->origin[axis] == plane[axis]); }
                }
                assert(!memcmp(&expected, q, sizeof(expected))); /* Includes UV/RGBA, IDs and use counts. */
            }
            for (DWORD f = 0; f < a->facecount; f++)
            {
                BgDocumentFace expected = a->faces[f]; BOOL mirrored = FALSE;
                for (DWORD i = 0; i < (group ? 4u : 1u); i++)
                { if (selected[i].faceid == expected.id && selected[i].room == r) { mirrored = TRUE; } }
                if (mirrored)
                {
                    expected.vertexindices[1] = a->faces[f].vertexindices[2];
                    expected.vertexindices[2] = a->faces[f].vertexindices[1];
                    static const unsigned char seams[8] = {0,4,2,6,1,5,3,7};
                    expected.uvseams = seams[expected.uvseams];
                }
                assert(!memcmp(&expected, &b->faces[f], sizeof(expected)));
            }
        }
        RoundTrip(&doc, &source, dir);
        assert(BgDocumentMirrorFaces(&doc, selected, group ? 4 : 1, axis, &why));
        doc.dirty = original.dirty; Same(&doc, &original); BgDocumentFree(&doc);
    }
    /* A face on the mirror plane keeps its positions but changes orientation. */
    for (DWORD v = 0; v < 3; v++) { original.rooms[1].vertices[v].z = 17; }
    assert(BgDocumentClone(&original, &doc, &why));
    assert(BgDocumentMirrorFaces(&doc, selected, 1, 2, &why));
    assert(!memcmp(doc.rooms[1].vertices, original.rooms[1].vertices, 3*sizeof(BgDocumentVertex)));
    assert(doc.rooms[1].faces[1].vertexindices[1] == original.rooms[1].faces[1].vertexindices[2]);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: X/Y/Z, single/group bounding boxes, shared vertices, cross-room/layer selection, winding, seams, UV/RGBA and native/project saves.");
}

static void Failures(void)
{
    BgFile source = Fixture(); BgDocument doc = {0}, before = {0}; BgFaceRef refs[20], bad[2]; const char *why = "";
    assert(BgDocumentLoad(source.data, source.size, 1, &doc, &why)); Prepare(&doc); Refs(&doc, refs);
    assert(BgDocumentClone(&doc, &before, &why));
    for (int i = 0; i < 3; i++)
    {
        allocations = i; assert(!BgDocumentMirrorFaces(&doc, refs, 2, 0, &why)); allocations = -1;
        assert(why[0]); Same(&doc, &before);
    }
    bad[0] = refs[0]; bad[1] = refs[0];
    assert(!BgDocumentMirrorFaces(&doc, bad, 2, 0, &why)); Same(&doc, &before);
    bad[1] = refs[1]; bad[1].faceid = (DWORD)-1;
    assert(!BgDocumentMirrorFaces(&doc, bad, 2, 0, &why)); Same(&doc, &before);
    assert(!BgDocumentMirrorFaces(&doc, refs, 2, 3, &why)); Same(&doc, &before);
    assert(!BgDocumentMirrorFaces(&doc, refs, 0, 0, &why)); Same(&doc, &before);
    doc.rooms[1].faces[1].vertexindices[0] = (DWORD)-1;
    before.rooms[1].faces[1].vertexindices[0] = (DWORD)-1;
    assert(!BgDocumentMirrorFaces(&doc, refs, 2, 0, &why)); Same(&doc, &before);
    BgDocumentFree(&before); BgDocumentFree(&doc);
    assert(BgDocumentLoad(source.data, source.size, 1, &doc, &why)); Prepare(&doc); Refs(&doc, refs);
    doc.rooms[2].origin[0] = 50000;
    assert(BgDocumentClone(&doc, &before, &why));
    assert(!BgDocumentMirrorFaces(&doc, refs, 20, 0, &why)); Same(&doc, &before);
    doc.rooms[2].origin[0] = before.rooms[2].origin[0] = NAN;
    assert(!BgDocumentMirrorFaces(&doc, refs, 20, 0, &why)); Same(&doc, &before);
    doc.levelscale = 0;
    assert(!BgDocumentMirrorFaces(&doc, refs, 20, 0, &why)); Same(&doc, &before);
    BgDocumentFree(&before); BgDocumentFree(&doc); BgFileFree(&source);
    puts("PASS: allocation failures, duplicate/stale references, invalid axis/vertices/scale/origins and coordinate overflow leave geometry untouched.");
}

/* Exercise the real editor transaction with only presentation calls stubbed. */
typedef void *HWND;
#define GEDITOR_TITLE "GEditor"
#define MB_ICONERROR 16
static HWND g_Viewport = (HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BgFaceRef selection[4];
static int selectedcount = 2, errors, restores;
static BOOL failrebuild, flying, transforming;
static EditorTool tool = EDITOR_TOOL_FACE_SELECT;
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return selectedcount; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd, BgFaceRef *out, int count)
{ assert(count == selectedcount); memcpy(out, selection, count*sizeof(*out)); return TRUE; }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ if (failrebuild) { failrebuild = FALSE; *why = "test rebuild failure"; return FALSE; } return TRUE; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd) { restores++; }
static void MessageBox(HWND hwnd, const char *why, const char *title, unsigned flags) { assert(why[0]); errors++; }
#include "editor.inc"

static void Commands(void)
{
    BgFile source = Fixture(); BgDocument original = {0}, mirrored = {0}; BgFaceRef refs[20]; const char *why = "";
    assert(BgDocumentLoad(source.data, source.size, 1, &g_CurrentBgDocument, &why)); Prepare(&g_CurrentBgDocument);
    assert(BgDocumentClone(&g_CurrentBgDocument, &original, &why)); Refs(&original, refs);
    selection[0] = refs[1]; selection[1] = refs[16];
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    const char *labels[] = {"Mirror X", "Mirror Y", "Mirror Z"};
    for (unsigned axis = 0; axis < 3; axis++)
    {
        assert(GEditorMirrorSelectedBgFaces((HWND)1, axis));
        assert(g_EditHistory.undocount == 1 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory), labels[axis]));
        assert(BgDocumentClone(&g_CurrentBgDocument, &mirrored, &why));
        assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why)); Same(&g_CurrentBgDocument, &original);
        assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why)); Same(&g_CurrentBgDocument, &mirrored);
        failrebuild = TRUE; assert(!GEditorMirrorSelectedBgFaces((HWND)1, axis)); Same(&g_CurrentBgDocument, &mirrored);
        ULONGLONG revision = g_EditHistory.nextrevision; g_EditHistory.nextrevision = 0;
        assert(!GEditorMirrorSelectedBgFaces((HWND)1, axis)); g_EditHistory.nextrevision = revision; Same(&g_CurrentBgDocument, &mirrored);
        assert(g_EditHistory.undocount == 1 && restores == (int)(2*(axis+1)) && errors == restores);
        assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why)); Same(&g_CurrentBgDocument, &original);
        assert(selectedcount == 2 && selection[0].faceid == refs[1].faceid && selection[1].faceid == refs[16].faceid);
        BgDocumentFree(&mirrored);
    }
    assert(!GEditorMirrorSelectedBgFaces((HWND)1, 3));
    flying = TRUE; assert(!GEditorMirrorSelectedBgFaces((HWND)1, 0)); flying = FALSE;
    transforming = TRUE; assert(!GEditorMirrorSelectedBgFaces((HWND)1, 0)); transforming = FALSE;
    tool = EDITOR_TOOL_EDGE_SELECT; assert(!GEditorMirrorSelectedBgFaces((HWND)1, 0));
    tool = EDITOR_TOOL_FACE_SELECT; selectedcount = 0; assert(!GEditorMirrorSelectedBgFaces((HWND)1, 0));
    Same(&g_CurrentBgDocument, &original);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: mirror commands, one-step undo/redo, rebuild/commit rollback and edit guards.");
}

int main(int argc, char **argv) { assert(argc == 2); Geometry(argv[1]); Failures(); Commands(); return 0; }
