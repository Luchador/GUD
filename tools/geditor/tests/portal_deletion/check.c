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
#include "fixture.inc"

static void Same(const BgPortalFile *a, const BgPortalFile *b, BOOL native)
{
    assert(a->portalcount == b->portalcount);
    for (DWORD i = 0; i < a->portalcount; i++)
    {
        BgPortal expected = a->portals[i];
        if (native && (expected.geometryoffset & BG_PORTAL_NEW_GEOMETRY))
            expected.geometryoffset = b->portals[i].geometryoffset;
        assert(!memcmp(&expected, &b->portals[i], sizeof(expected)));
    }
}

static BgFile Persist(const BgDocument *doc, const BgFile *source, const char *dir)
{
    const char *why = "";
    BgFile saved = {0}, disk = {0}, again = {0};
    BgDocument loaded = {0};
    assert(BgDocumentCompile(doc, source, &saved, &why));
    DWORD table = Get(saved.data + 8) & 0x00ffffffu;
    assert(!Get(saved.data + table + doc->portals.portalcount * 8));
    assert(BgFileValidateVertexBatches(&saved, &why)); /* Create ROM's BG gate. */
    assert(BgSaveProjectFile(dir, &saved, &why));
    assert(BgLoadProjectFile(dir, saved.name, &disk, &why));
    assert(saved.size == disk.size && !memcmp(saved.data, disk.data, disk.size));
    assert(BgDocumentLoad(disk.data, disk.size, doc->levelscale, &loaded, &why));
    assert(!loaded.portalwarning && loaded.facecount == doc->facecount);
    Same(&doc->portals, &loaded.portals, TRUE);
    assert(BgDocumentCompile(doc, &saved, &again, &why));
    assert(saved.size == again.size && !memcmp(saved.data, again.data, saved.size));
    assert(!memcmp(saved.newportaloffsets, again.newportaloffsets, sizeof(saved.newportaloffsets)));
    assert(!memcmp(source->data + 416, saved.data + 416, 92)); /* visibility/opaque metadata */
    BgFileFree(&again); BgFileFree(&disk); BgDocumentFree(&loaded);
    return saved;
}

static void NativeDeletion(const char *dir)
{
    const char *why = "";
    BgFile source = Fixture(); BgDocument original = {0};
    assert(BgDocumentLoad(source.data, source.size, .5f, &original, &why));
    for (unsigned mask = 1; mask < 8; mask++)
    {
        DWORD selected[3], count = 0, kept = 0;
        BgDocument doc = {0}; assert(BgDocumentClone(&original, &doc, &why));
        for (DWORD i = 0; i < 3; i++) if (mask & (1u << i)) selected[count++] = i;
        assert(BgDocumentDeletePortals(&doc, &source, selected, count, &why));
        assert(doc.dirty && doc.portals.portalcount == 3 - count);
        for (DWORD i = 0; i < 3; i++) if (!(mask & (1u << i)))
            assert(!memcmp(&doc.portals.portals[kept++], &original.portals.portals[i], sizeof(BgPortal)));
        BgFile saved = Persist(&doc, &source, dir);
        /* Restore deleted native polygons even after the source table shrank. */
        BgFile restored = Persist(&original, &saved, dir);
        BgFileFree(&restored); BgFileFree(&saved); BgDocumentFree(&doc);
    }
    DWORD invalid[] = {0, 3}, duplicate[] = {1, 1}, both[] = {0, 1}, one = 0;
    BgDocument doc = {0}; assert(BgDocumentClone(&original, &doc, &why));
    assert(!BgDocumentDeletePortals(&doc, &source, invalid, 2, &why));
    assert(!BgDocumentDeletePortals(&doc, &source, duplicate, 2, &why));
    Same(&original.portals, &doc.portals, FALSE); assert(!doc.dirty);
    /* A script can still resolve its polygon after only one alias is removed. */
    Put(source.data + 12, 0x0f0001a0);
    Put(source.data + 416, 0x1f020000); Put(source.data + 420, 0);
    Put(source.data + 424, 0x64000000); Put(source.data + 428, 0x0f0000c0);
    Put(source.data + 432, 0x00010000); Put(source.data + 436, 0);
    assert(!BgDocumentDeletePortals(&doc, &source, both, 2, &why));
    assert(strstr(why, "visibility script")); Same(&original.portals, &doc.portals, FALSE);
    assert(BgDocumentDeletePortals(&doc, &source, &one, 1, &why));
    BgFile scripted = Persist(&doc, &source, dir);
    assert(!BgDocumentDeletePortals(&doc, &scripted, &one, 1, &why));
    BgFileFree(&scripted); BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: first/middle/last/all deletion, shared polygons, atomic rejection, visibility references, save/reload and restoration.");
}

static void CreatedDeletion(const char *dir)
{
    const char *why = ""; DWORD index, selected = 0;
    BgFile source = Fixture(); BgDocument doc = {0}, before = {0};
    BgPortalPlacement placement = {1, 3, {15, -25, 35}, 200, 300, BG_PORTAL_XY};
    assert(BgDocumentLoad(source.data, source.size, .5f, &doc, &why));
    for (int i = 0; i < 3; i++)
    { placement.center[0] += 100; assert(BgDocumentAddPortal(&doc, &placement, &index, &why)); }
    assert(BgDocumentClone(&doc, &before, &why));
    BgFile saved = Persist(&doc, &source, dir);
    /* Remove an early native entry, then append: old count-based IDs collide. */
    assert(BgDocumentDeletePortals(&doc, &saved, &selected, 1, &why));
    placement.center[0] += 100; assert(BgDocumentAddPortal(&doc, &placement, &index, &why));
    for (DWORD i = 0; i < index; i++)
        assert(doc.portals.portals[i].geometryoffset != doc.portals.portals[index].geometryoffset);
    BgFile added = Persist(&doc, &saved, dir);
    /* Delete a saved editor polygon, save, and reuse its slot for a new one. */
    selected = index;
    BgDocument after = {0}; assert(BgDocumentClone(&doc, &after, &why));
    assert(BgDocumentDeletePortals(&doc, &added, &selected, 1, &why));
    BgFile removed = Persist(&doc, &added, dir);
    placement.center[1] += 70; assert(BgDocumentAddPortal(&doc, &placement, &index, &why));
    BgFile replacement = Persist(&doc, &removed, dir);
    BgFile undo = Persist(&after, &replacement, dir);
    BgFile redo = Persist(&doc, &undo, dir);
    BgFile original = Persist(&before, &redo, dir);
    BgFileFree(&original); BgFileFree(&redo); BgFileFree(&undo); BgFileFree(&replacement);
    BgFileFree(&removed); BgFileFree(&added); BgFileFree(&saved);
    BgDocumentFree(&before); BgDocumentFree(&after);
    /* Deletion returns a slot at the 199-portal limit. */
    while (doc.portals.portalcount < BG_MAX_PORTALS - 1)
        assert(BgDocumentAddPortal(&doc, &placement, &index, &why));
    assert(!BgDocumentAddPortal(&doc, &placement, &index, &why));
    selected = 5; assert(BgDocumentDeletePortals(&doc, &source, &selected, 1, &why));
    assert(BgDocumentAddPortal(&doc, &placement, &index, &why));
    for (DWORD i = 0; i < index; i++) assert(doc.portals.portals[i].geometryoffset != doc.portals.portals[index].geometryoffset);
    BgDocumentFree(&doc); BgFileFree(&source);
    puts("PASS: saved/new portal deletion and replacement, unique IDs, repeated saves, restored coordinates and capacity reuse.");
}

typedef void *HWND;
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
typedef struct ViewportState {
    EditorTool tool; BOOL showportals;
    BgPortalFile portals; unsigned char portalselection[BG_MAX_PORTALS];
} ViewportState;
static ViewportState view;
static HWND g_Viewport = &view;
static BgDocument g_CurrentBgDocument;
static BgFile g_CurrentBg;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL flying, transforming, failrebuild;
static int errors, restores;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
#include "selection.inc"
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static void ViewportSetPortals(HWND hwnd, const BgPortalFile *portals)
{ assert(!portals); memset(view.portalselection, 0, sizeof(view.portalselection)); }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{
    view.portals = g_CurrentBgDocument.portals;
    if (failrebuild) { failrebuild = FALSE; *why = "Rebuild failed."; return FALSE; }
    return TRUE;
}
static void GEditorRestoreHistorySelection(HWND hwnd)
{ restores++; memcpy(view.portalselection, g_EditHistory.selection, sizeof(view.portalselection)); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd, const char *why, const char *title, unsigned flags)
{ assert(why[0]); errors++; }
#include "controller.inc"

static void Controller(const char *dir)
{
    const char *why = ""; DWORD indices[BG_MAX_PORTALS]; BgDocument original = {0};
    g_CurrentBg = Fixture();
    assert(BgDocumentLoad(g_CurrentBg.data, g_CurrentBg.size, .5f, &g_CurrentBgDocument, &why));
    assert(BgDocumentClone(&g_CurrentBgDocument, &original, &why));
    view.tool = EDITOR_TOOL_FACE_SELECT; view.showportals = TRUE; view.portals = g_CurrentBgDocument.portals;
    view.portalselection[0] = view.portalselection[1] = 1;
    /* Shared-polygon entries are separate connections and must both delete. */
    assert(ViewportGetSelectedPortalFaces(&view, indices) == 2 && indices[0] == 0 && indices[1] == 1);
    view.showportals = FALSE; assert(!GEditorDeleteSelectedPortals(NULL)); view.showportals = TRUE;
    view.tool = EDITOR_TOOL_EDGE_SELECT; assert(!GEditorDeleteSelectedPortals(NULL)); view.tool = EDITOR_TOOL_FACE_SELECT;
    flying = TRUE; assert(!GEditorDeleteSelectedPortals(NULL)); flying = FALSE;
    transforming = TRUE; assert(!GEditorDeleteSelectedPortals(NULL)); transforming = FALSE;
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory, view.portalselection, sizeof(view.portalselection), FALSE, &why));
    for (int failure = 0; failure < 2; failure++)
    {
        ULONGLONG revision = g_EditHistory.nextrevision;
        failrebuild = failure == 0; if (failure == 1) g_EditHistory.nextrevision = 0;
        assert(!GEditorDeleteSelectedPortals(NULL)); g_EditHistory.nextrevision = revision;
        Same(&original.portals, &g_CurrentBgDocument.portals, FALSE);
        assert(view.portalselection[0] && view.portalselection[1] && !g_EditHistory.undocount);
    }
    assert(errors == 2 && restores == 2);
    assert(GEditorDeleteSelectedPortals(NULL) && g_CurrentBgDocument.portals.portalcount == 1);
    assert(!ViewportGetSelectedPortalFaces(&view, NULL));
    assert(EditHistorySetSelection(&g_EditHistory, view.portalselection, sizeof(view.portalselection), FALSE, &why));
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Delete Portals"));
    BgFile saved = Persist(&g_CurrentBgDocument, &g_CurrentBg, dir);
    EditHistoryMarkBgSaved(&g_EditHistory, &g_CurrentBgDocument);
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    Same(&original.portals, &g_CurrentBgDocument.portals, FALSE); assert(g_CurrentBgDocument.dirty);
    GEditorRebuildCurrentViewport(&why); GEditorRestoreHistorySelection(NULL);
    assert(ViewportGetSelectedPortalFaces(&view, NULL) == 2);
    BgFile undone = Persist(&g_CurrentBgDocument, &saved, dir);
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    assert(g_CurrentBgDocument.portals.portalcount == 1 && !g_CurrentBgDocument.dirty);
    GEditorRebuildCurrentViewport(&why); GEditorRestoreHistorySelection(NULL);
    assert(!ViewportGetSelectedPortalFaces(&view, NULL));
    BgFile redone = Persist(&g_CurrentBgDocument, &undone, dir);
    BgFileFree(&redone); BgFileFree(&undone); BgFileFree(&saved); BgFileFree(&g_CurrentBg);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&original);
    puts("PASS: production selection/controller, mode guards, rollback, cleared selection and undo/save/redo.");
}

int main(int argc, char **argv)
{
    assert(argc == 2); NativeDeletion(argv[1]); CreatedDeletion(argv[1]); Controller(argv[1]);
    return 0;
}
