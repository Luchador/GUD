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
static int allocation = -1;
static void *TestMalloc(size_t size)
{ if (allocation == 0) return NULL; if (allocation > 0) allocation--; return malloc(size); }
#define malloc TestMalloc
#include "bgportal.c"
#undef malloc
static void Same(const BgPortalFile *a, const BgPortalFile *b, BOOL native)
{
    assert(a->portalcount == b->portalcount);
    for (DWORD i = 0; i < a->portalcount; i++)
    {
        BgPortal expected = a->portals[i], actual = b->portals[i];
        if (native)
        {
            if (expected.geometryoffset & BG_PORTAL_NEW_GEOMETRY) expected.geometryoffset = actual.geometryoffset;
            /* Unused point slots are not serialized. */
            for (DWORD p = expected.pointcount; p < BG_PORTAL_MAX_POINTS; p++)
            {
                expected.points[p] = expected.nativepoints[p] = (BgPortalPoint){0};
                actual.points[p] = actual.nativepoints[p] = (BgPortalPoint){0};
            }
        }
        assert(!memcmp(&expected, &actual, sizeof(expected)));
    }
}
#include "fixture.inc"

static void Native(const char *dir)
{
    const char *why = "";
    BgFile source = Fixture(); BgDocument doc = {0}, original = {0}; BgPortalFile clip = {0};
    DWORD chosen[] = {0, 1}, indices[BG_MAX_PORTALS];
    const double offset[3] = {0, 10, 0};
    assert(BgDocumentLoad(source.data, source.size, .5f, &doc, &why));
    assert(BgDocumentClone(&doc, &original, &why));
    assert(BgDocumentCopyPortals(&doc, chosen, 2, &clip, &why));
    assert(clip.portals != doc.portals.portals && !doc.dirty);
    BgPortal snapshot[2]; memcpy(snapshot, clip.portals, sizeof(snapshot));
    /* Bad copy must retain the last successful snapshot. */
    DWORD bad[] = {0, 3}, duplicate[] = {1, 1};
    assert(!BgDocumentCopyPortals(&doc, bad, 2, &clip, &why));
    assert(!BgDocumentCopyPortals(&doc, duplicate, 2, &clip, &why));
    allocation = 0; assert(!BgDocumentCopyPortals(&doc, chosen, 2, &clip, &why)); allocation = -1;
    assert(!memcmp(snapshot, clip.portals, sizeof(snapshot)));
    for (int failure = 0; failure < 2; failure++)
    {
        allocation = failure; /* result allocation, then displacement snapshot */
        assert(!BgDocumentPastePortals(&doc, &clip, offset, indices, &why)); allocation = -1;
        Same(&doc.portals, &original.portals, FALSE); assert(!doc.dirty);
    }
    const double overflow[] = {0, 1e300, 0}, nonfinite[] = {0, NAN, 0};
    assert(!BgDocumentPastePortals(&doc, &clip, overflow, indices, &why));
    assert(!BgDocumentPastePortals(&doc, &clip, nonfinite, indices, &why));
    doc.portalwarning = "bad table"; assert(!BgDocumentPastePortals(&doc, &clip, offset, indices, &why));
    doc.portalwarning = NULL;
    clip.portals[0].connectedroom2 = 4; assert(!BgDocumentPastePortals(&doc, &clip, offset, indices, &why));
    clip.portals[0] = snapshot[0];
    clip.portals[0].nativepoints[0].x = NAN;
    assert(!BgDocumentPastePortals(&doc, &clip, offset, indices, &why));
    clip.portals[0] = snapshot[0];
    Same(&doc.portals, &original.portals, FALSE); assert(!doc.dirty);
    assert(BgDocumentPastePortals(&doc, &clip, offset, indices, &why));
    assert(indices[0] == 3 && indices[1] == 4 && doc.portals.portalcount == 5);
    assert(!memcmp(doc.portals.portals, original.portals.portals, 3 * sizeof(BgPortal)));
    assert(doc.portals.portals[3].geometryoffset == doc.portals.portals[4].geometryoffset);
    assert(doc.portals.portals[3].geometryoffset != snapshot[0].geometryoffset);
    for (int i = 0; i < 2; i++)
    {
        BgPortal expected = snapshot[i]; expected.geometryoffset = doc.portals.portals[3+i].geometryoffset;
        for (int p = 0; p < 4; p++) { expected.points[p].y += 10; expected.nativepoints[p].y += 5; }
        assert(!memcmp(&expected, &doc.portals.portals[3+i], sizeof(expected)));
    }
    BgFile saved = Persist(&doc, &source, dir);
    /* Moving the copied pair changes both aliases, never the originals/clipboard. */
    BgPortalPointRef point = {3, 0}; DWORD moved;
    const double move[] = {2, 0, 0};
    assert(BgDocumentTranslatePortalPoints(&doc, &point, 1, move, &moved, &why) && moved == 2);
    assert(!memcmp(doc.portals.portals, original.portals.portals, 3 * sizeof(BgPortal)));
    assert(!memcmp(snapshot, clip.portals, sizeof(snapshot)));
    assert(BgDocumentPastePortals(&doc, &clip, offset, indices, &why) && indices[0] == 5);
    assert(doc.portals.portals[5].geometryoffset != doc.portals.portals[3].geometryoffset);
    assert(doc.portals.portals[5].points[0].x == snapshot[0].points[0].x);
    /* Source deletion and index compaction cannot invalidate the snapshot. */
    assert(BgDocumentDeletePortals(&doc, &saved, chosen, 2, &why));
    assert(BgDocumentPastePortals(&doc, &clip, offset, indices, &why));
    BgFile again = Persist(&doc, &saved, dir); BgFileFree(&again); BgFileFree(&saved);
    BgDocumentFree(&doc);
    /* All polygon sizes survive a reused, already-saved temporary identity. */
    assert(BgDocumentLoad(source.data, source.size, .5f, &doc, &why));
    BgPortalFileFree(&clip); DWORD zero = 0;
    assert(BgDocumentCopyPortals(&doc, &zero, 1, &clip, &why));
    BgFile current = source; memset(&source, 0, sizeof(source));
    for (unsigned size = 3; size <= BG_PORTAL_MAX_POINTS; size++)
    {
        clip.portals[0].pointcount = size;
        for (unsigned p = 0; p < size; p++)
        {
            float x = 20 * cos(p * 6.283185307 / size), y = 20 * sin(p * 6.283185307 / size);
            clip.portals[0].points[p] = (BgPortalPoint){x, y, 10};
            clip.portals[0].nativepoints[p] = (BgPortalPoint){x*.5f, y*.5f, 5};
        }
        assert(BgDocumentPastePortals(&doc, &clip, offset, indices, &why));
        saved = Persist(&doc, &current, dir); BgFileFree(&current); current = saved;
        assert(BgDocumentDeletePortals(&doc, &current, indices, 1, &why));
    }
    /* Empty tables and the full native capacity are supported atomically. */
    DWORD all[] = {0, 1, 2};
    assert(BgDocumentDeletePortals(&doc, &current, all, 3, &why));
    for (DWORD i = 0; i < BG_MAX_PORTALS - 1; i++)
    {
        assert(BgDocumentPastePortals(&doc, &clip, offset, indices, &why) && indices[0] == i);
        for (DWORD j = 0; j < i; j++) assert(doc.portals.portals[i].geometryoffset != doc.portals.portals[j].geometryoffset);
    }
    assert(!BgDocumentPastePortals(&doc, &clip, offset, indices, &why));
    assert(doc.portals.portalcount == 199 && strstr(why, "199"));
    saved = Persist(&doc, &current, dir); BgFileFree(&saved); BgFileFree(&current);
    BgPortalFileFree(&clip); BgDocumentFree(&doc); BgDocumentFree(&original);
    puts("PASS: independent snapshots, native attributes/shared polygons, repeat paste, deletion, allocation/coordinate rollback, all polygon sizes, save/reload and 199-portal limit.");
}

static void Scaled(const char *dir)
{
    const float scales[] = {.3f, .2184788f};
    const double offset[] = {0, 10, 0}; const char *why = "";
    BgFile source = Fixture();
    for (unsigned scale = 0; scale < sizeof(scales)/sizeof(scales[0]); scale++)
    {
        BgDocument doc = {0}; BgPortalFile clip = {0}; DWORD index = 0, added[BG_MAX_PORTALS];
        assert(BgDocumentLoad(source.data, source.size, scales[scale], &doc, &why));
        assert(BgDocumentCopyPortals(&doc, &index, 1, &clip, &why));
        assert(BgDocumentPastePortals(&doc, &clip, offset, added, &why));
        for (unsigned p = 0; p < clip.portals[0].pointcount; p++)
        {
            assert(doc.portals.portals[3].nativepoints[p].x == clip.portals[0].nativepoints[p].x);
            assert(doc.portals.portals[3].nativepoints[p].z == clip.portals[0].nativepoints[p].z);
            assert(fabs(doc.portals.portals[3].points[p].y - clip.portals[0].points[p].y - 10) < .0001);
        }
        BgFile saved = Persist(&doc, &source, dir); BgFileFree(&saved);
        BgPortalFileFree(&clip); BgDocumentFree(&doc);
    }
    BgFileFree(&source);
    puts("PASS: non-power-of-two level scales retain untouched native coordinates and reload exactly.");
}

typedef void *HWND;
typedef struct ViewportState {
    EditorTool tool;
    BOOL showportals;
    BgPortalFile portals;
    unsigned int portalselection[BG_MAX_PORTALS];
    DWORD selectedportal;
} ViewportState;
#define VIEWPORT_WM_SELECTION_CHANGED 1
#define GEDITOR_TITLE "GEditor"
#define MB_ICONERROR 1
static ViewportState view;
static HWND g_Viewport = &view, g_RightPanel = (HWND)2;
static BgDocument g_CurrentBgDocument, g_FaceClipboard;
static BgPortalFile g_PortalClipboard;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL flying, transforming, knife, faildisplay, objectclipboard;
static int errors, restores, notifications;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportClearAllSelection(ViewportState *state)
{ memset(state->portalselection, 0, sizeof(state->portalselection)); state->selectedportal = BG_PORTAL_INDEX_NONE; }
static void ViewportRefreshPortalColors(ViewportState *state) {}
static void ViewportUpdateGizmo(ViewportState *state) {}
static void InvalidateRect(HWND hwnd, const void *rect, BOOL erase) {}
static HWND GetParent(HWND hwnd) { return NULL; }
static void SendMessage(HWND hwnd, int msg, int w, int l) { notifications++; }
#include "selection.inc"
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportKnifeActive(HWND hwnd) { return knife; }
static void ViewportSetPortals(HWND hwnd, const BgPortalFile *portals)
{ view.portals = *portals; if (faildisplay) { faildisplay = FALSE; view.portals.portalcount = 0; } }
static void RightPanelShowPortals(HWND hwnd) { view.showportals = TRUE; }
static void GEditorClearObjectClipboard(void) { objectclipboard = FALSE; }
static void GEditorRestoreHistorySelection(HWND hwnd)
{ restores++; memcpy(view.portalselection, g_EditHistory.selection, sizeof(view.portalselection)); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void SetFocus(HWND hwnd) {}
static void MessageBox(HWND hwnd, const char *why, const char *title, int flags)
{ assert(why[0]); errors++; }
#include "controller.inc"

static void Controller(const char *dir)
{
    const char *why = ""; DWORD indices[BG_MAX_PORTALS]; BgFile source = Fixture(); BgDocument original = {0};
    assert(BgDocumentLoad(source.data, source.size, .5f, &g_CurrentBgDocument, &why));
    assert(BgDocumentClone(&g_CurrentBgDocument, &original, &why));
    ViewportSetPortals(g_Viewport, &g_CurrentBgDocument.portals);
    view.tool = EDITOR_TOOL_FACE_SELECT; view.showportals = TRUE;
    DWORD pair[] = {0, 1}, invalid[] = {1, 3}, duplicate[] = {1, 1};
    assert(ViewportSelectPortalFaces(g_Viewport, pair, 2) && notifications == 1 && view.selectedportal == 0);
    assert(!ViewportSelectPortalFaces(g_Viewport, invalid, 2));
    assert(!ViewportSelectPortalFaces(g_Viewport, duplicate, 2));
    assert(ViewportGetSelectedPortalFaces(g_Viewport, indices) == 2 && indices[0] == 0 && indices[1] == 1);
    view.showportals = FALSE; assert(!GEditorCanCopyPortals()); view.showportals = TRUE;
    view.tool = EDITOR_TOOL_EDGE_SELECT; assert(!GEditorCanCopyPortals()); view.tool = EDITOR_TOOL_FACE_SELECT;
    flying = TRUE; assert(!GEditorCanCopyPortals()); flying = FALSE;
    transforming = TRUE; assert(!GEditorCanCopyPortals()); transforming = FALSE;
    knife = TRUE; assert(!GEditorCanCopyPortals()); knife = FALSE;
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory, view.portalselection, sizeof(view.portalselection), FALSE, &why));
    objectclipboard = TRUE;
    assert(BgDocumentClone(&original, &g_FaceClipboard, &why));
    allocation = 0; assert(!GEditorCopyPortals(NULL)); allocation = -1;
    assert(objectclipboard && g_FaceClipboard.rooms);
    assert(GEditorCopyPortals(NULL) && !objectclipboard && !g_FaceClipboard.rooms);
    assert(!g_CurrentBgDocument.dirty && !g_EditHistory.undocount);
    for (int failure = 0; failure < 3; failure++)
    {
        ULONGLONG revision = g_EditHistory.nextrevision;
        allocation = failure == 0 ? 0 : -1;
        faildisplay = failure == 1;
        if (failure == 2) g_EditHistory.nextrevision = 0;
        assert(!GEditorPastePortals(NULL)); allocation = -1; g_EditHistory.nextrevision = revision;
        Same(&original.portals, &g_CurrentBgDocument.portals, FALSE);
        assert(view.portalselection[0] && view.portalselection[1] && !g_EditHistory.undocount);
        assert(g_PortalClipboard.portalcount == 2 && !g_CurrentBgDocument.dirty);
    }
    assert(errors == 4 && restores == 2);
    view.showportals = FALSE; /* Paste makes its results visible again. */
    assert(GEditorPastePortals(NULL) && view.showportals && g_EditHistory.undocount == 1);
    assert(ViewportGetSelectedPortalFaces(g_Viewport, indices) == 2 && indices[0] == 3 && indices[1] == 4);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Paste Portals"));
    assert(EditHistorySetSelection(&g_EditHistory, view.portalselection, sizeof(view.portalselection), FALSE, &why));
    BgFile saved = Persist(&g_CurrentBgDocument, &source, dir);
    EditHistoryMarkBgSaved(&g_EditHistory, &g_CurrentBgDocument);
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    ViewportSetPortals(g_Viewport, &g_CurrentBgDocument.portals); GEditorRestoreHistorySelection(NULL);
    Same(&original.portals, &g_CurrentBgDocument.portals, FALSE);
    assert(view.portalselection[0] && view.portalselection[1] && g_CurrentBgDocument.dirty);
    BgFile undone = Persist(&g_CurrentBgDocument, &saved, dir);
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    ViewportSetPortals(g_Viewport, &g_CurrentBgDocument.portals); GEditorRestoreHistorySelection(NULL);
    assert(view.portalselection[3] && view.portalselection[4] && !g_CurrentBgDocument.dirty);
    BgFile redone = Persist(&g_CurrentBgDocument, &undone, dir);
    assert(GEditorPastePortals(NULL) && g_CurrentBgDocument.portals.portalcount == 7);
    BgFileFree(&redone); BgFileFree(&undone); BgFileFree(&saved); BgFileFree(&source);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&original);
    BgPortalFileFree(&g_PortalClipboard);
    puts("PASS: production selection/controller, mode guards, mutually exclusive copy, failure rollback, pasted selection, repeated paste and undo/save/redo.");
}

int main(int argc, char **argv) { assert(argc == 2); Native(argv[1]); Scaled(argv[1]); Controller(argv[1]); return 0; }
