#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static BOOL failallocation;
static void *TestMalloc(size_t size) { return failallocation ? NULL : malloc(size); }
#define malloc TestMalloc
#include "bgbridge.c"
#undef malloc
#include "fixture.inc"

static BgDocumentEdgeRef Edge(const BgDocumentFace *face)
{ return (BgDocumentEdgeRef){{face->id,face->room,face->layer,0},0}; }

static void MakeGap(BgDocument *doc)
{
    BgDocumentRoom *r = &doc->rooms[1];
    r->vertices = realloc(r->vertices, 6*sizeof(*r->vertices)); assert(r->vertices);
    r->faces = realloc(r->faces, 2*sizeof(*r->faces)); assert(r->faces);
    const short pos[6][2] = {{0,0},{100,0},{0,100},{100,-100},{0,-100},{100,-200}};
    for (int i = 0; i < 6; i++)
    {
        if (i >= 3) { r->vertices[i] = r->vertices[0]; r->vertices[i].id = doc->nextvertexid++; }
        r->vertices[i].x = pos[i][0]; r->vertices[i].y = pos[i][1]; r->vertices[i].z = 0;
        r->vertices[i].s = pos[i][0]*8; r->vertices[i].t = pos[i][1]*16;
        r->vertices[i].r = 20+i*30; r->vertices[i].a = 255-i*10; r->vertices[i].usecount = 1;
    }
    r->faces[0].textureid = 17; r->faces[0].cullbackfaces = TRUE;
    BgMaterialSetTexture(&r->faces[0].material, 17);
    r->faces[1] = r->faces[0]; r->faces[1].id = doc->nextfaceid++;
    for (int c = 0; c < 3; c++) { r->faces[0].vertexindices[c] = c; r->faces[1].vertexindices[c] = c+3; }
    r->vertexcount = 6; r->facecount = r->facecapacity = doc->facecount = 2;
}

static void Counts(const BgDocument *doc)
{
    const BgDocumentRoom *r = &doc->rooms[1];
    for (DWORD v = 0; v < r->vertexcount; v++)
    {
        DWORD uses = 0;
        for (DWORD f = 0; f < r->facecount; f++) for (int c = 0; c < 3; c++) { uses += r->faces[f].vertexindices[c] == v; }
        assert(uses == r->vertices[v].usecount);
    }
}

static void Geometry(const char *dir)
{
    const char *why = "";
    for (int layer = 0; layer < 2; layer++) for (int reverse = 0; reverse < 2; reverse++)
    {
        BgFile source = Fixture(); BgDocument doc = {0}; BgFaceRef out[2];
        assert(BgDocumentLoad(source.data, source.size, .1f, &doc, &why)); MakeGap(&doc);
        BgDocumentRoom *r = &doc.rooms[1];
        if (layer)
        {
            r->layers[1] = r->layers[0]; memset(&r->layers[0], 0, sizeof(r->layers[0]));
            r->faces[0].layer = r->faces[1].layer = 1;
        }
        BgDocumentVertex before[6]; memcpy(before, r->vertices, sizeof(before));
        BgDocumentFace faces[2]; memcpy(faces, r->faces, sizeof(faces));
        BgDocumentEdgeRef edges[2] = {Edge(&r->faces[reverse]), Edge(&r->faces[!reverse])};
        assert(BgDocumentCanBridgeEdges(&doc, edges, &why));
        BgDocument snapshot = doc; BgDocumentRoom room = *r;
        failallocation = TRUE; assert(!BgDocumentBridgeEdges(&doc, edges, out, &why)); failallocation = FALSE;
        assert(!memcmp(&snapshot, &doc, sizeof(doc)) && !memcmp(&room, r, sizeof(room)));
        assert(!memcmp(before, r->vertices, sizeof(before)) && !memcmp(faces, r->faces, sizeof(faces)));
        /* Validation failures must not mutate any arrays, counts or identities. */
        BgDocumentEdgeRef bad[2] = {edges[0], edges[0]};
        assert(!BgDocumentBridgeEdges(&doc, bad, out, &why));
        bad[1] = edges[1]; bad[1].corner = 3; assert(!BgDocumentCanBridgeEdges(&doc, bad, &why));
        bad[1] = edges[1]; bad[1].face.room = 2; assert(!BgDocumentCanBridgeEdges(&doc, bad, &why));
        bad[1] = edges[1]; bad[1].face.layer ^= 1; assert(!BgDocumentCanBridgeEdges(&doc, bad, &why));
        bad[1] = edges[1]; bad[1].face.faceid = 999; assert(!BgDocumentCanBridgeEdges(&doc, bad, &why));
        /* Reversing only the second edge would produce a crossed bridge. */
        DWORD swap = r->faces[1].vertexindices[0]; r->faces[1].vertexindices[0] = r->faces[1].vertexindices[1]; r->faces[1].vertexindices[1] = swap;
        assert(!BgDocumentCanBridgeEdges(&doc, edges, &why)); memcpy(r->faces, faces, sizeof(faces));
        r->vertices[3].y = r->vertices[4].y = 0;
        assert(!BgDocumentCanBridgeEdges(&doc, edges, &why)); memcpy(r->vertices, before, sizeof(before));
        assert(BgDocumentBridgeEdges(&doc, edges, out, &why) && doc.facecount == 4 && r->vertexcount == 6 && doc.dirty);
        Counts(&doc); assert(!memcmp(faces, r->faces, sizeof(faces)));
        for (int v = 0; v < 6; v++)
        { BgDocumentVertex current = r->vertices[v]; current.usecount = before[v].usecount; assert(!memcmp(&current, &before[v], sizeof(current))); }
        for (int t = 0; t < 2; t++)
        {
            const BgDocumentFace *f = BgDocumentFindFace(&doc, &out[t], NULL); assert(f);
            assert(f->drawgroup == faces[reverse].drawgroup && f->textureid == 17 && f->layer == layer && f->cullbackfaces);
            assert(!memcmp(&f->material, &faces[reverse].material, sizeof(f->material)));
            const BgDocumentVertex *a = &r->vertices[f->vertexindices[0]], *b = &r->vertices[f->vertexindices[1]], *c = &r->vertices[f->vertexindices[2]];
            assert((b->x-a->x)*(c->y-a->y) - (b->y-a->y)*(c->x-a->x) > 0);
        }
        assert(!BgDocumentCanBridgeEdges(&doc, edges, &why)); /* now interior */
        RoundTrip(&doc, &source, dir);
        BgDocumentFree(&doc); BgFileFree(&source);
    }
    puts("PASS: both layers and edge orders, native winding/material/UV/RGBA, boundary and degenerate rejection, atomic allocation failure, saved BG and ROM batch validation.");
}

typedef void *HWND;
#define GEDITOR_TITLE "test"
#define MB_ICONERROR 1
static HWND g_Viewport, g_ToolToolbar;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static EditorTool tool = EDITOR_TOOL_EDGE_SELECT;
static BgDocumentEdgeRef selection[2];
static BOOL flying, transforming, failrebuild, failselection;
static unsigned errors, restores;
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static void ViewportSetTool(HWND hwnd, EditorTool value) { tool = value; }
static BOOL ViewportGetSelectedBgEdges(HWND hwnd, BgDocumentEdgeRef *out, DWORD count)
{ assert(count == 2); if (tool != EDITOR_TOOL_EDGE_SELECT) { return FALSE; } memcpy(out, selection, sizeof(selection)); return TRUE; }
static BOOL ViewportSelectBgFaces(HWND hwnd, const BgFaceRef *faces, DWORD count)
{ assert(count == 2 && BgDocumentFindFace(&g_CurrentBgDocument, faces, NULL)); return !failselection; }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ if (failrebuild) { failrebuild = FALSE; *why = "test rebuild failure"; return FALSE; } return TRUE; }
static void ToolToolbarSetTool(HWND hwnd, EditorTool value) { assert(tool == value); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd) { tool = EDITOR_TOOL_EDGE_SELECT; restores++; }
static void MessageBox(HWND hwnd, const char *why, const char *title, unsigned flags) { assert(why[0]); errors++; }
#include "bridge_editor.inc"

static void Commands(void)
{
    const char *why = ""; BgFile source = Fixture();
    assert(BgDocumentLoad(source.data, source.size, 1, &g_CurrentBgDocument, &why)); MakeGap(&g_CurrentBgDocument);
    for (int i = 0; i < 2; i++) { selection[i] = Edge(&g_CurrentBgDocument.rooms[1].faces[i]); }
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    for (int failure = 0; failure < 3; failure++)
    {
        ULONGLONG revision = g_EditHistory.nextrevision;
        failrebuild = failure == 0; failselection = failure == 1;
        if (failure == 2) { g_EditHistory.nextrevision = 0; }
        assert(!GEditorBridgeSelectedBgEdges(NULL));
        g_EditHistory.nextrevision = revision; failselection = FALSE;
        assert(tool == EDITOR_TOOL_EDGE_SELECT && g_CurrentBgDocument.facecount == 2 && !g_CurrentBgDocument.dirty);
        assert(g_EditHistory.undocount == 0); Counts(&g_CurrentBgDocument);
    }
    assert(errors == 3 && restores == 3);
    flying = TRUE; assert(!GEditorBridgeSelectedBgEdges(NULL)); flying = FALSE;
    transforming = TRUE; assert(!GEditorBridgeSelectedBgEdges(NULL)); transforming = FALSE;
    assert(GEditorBridgeSelectedBgEdges(NULL) && tool == EDITOR_TOOL_FACE_SELECT && g_CurrentBgDocument.facecount == 4);
    assert(g_EditHistory.undocount == 1 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Bridge Edges"));
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    assert(g_CurrentBgDocument.facecount == 2 && !g_CurrentBgDocument.dirty); Counts(&g_CurrentBgDocument);
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    assert(g_CurrentBgDocument.facecount == 4 && g_CurrentBgDocument.dirty); Counts(&g_CurrentBgDocument);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgFileFree(&source);
    puts("PASS: real bridge command, face reselection, undo/redo, rebuild/selection/commit rollback and edit guards.");
}
int main(int argc, char **argv) { assert(argc == 2); Geometry(argv[1]); Commands(); return 0; }
