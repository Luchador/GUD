#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"
#include "bghistory.h"
#include "edittool.h"
#include "project.h"
#include "setupmeta.h"
#include "editorpath.h"

void BgDocumentFree(BgDocument *doc) { abort(); }
BOOL SetupFileCompact(SetupFile *setup, const char **why) { (void)setup; (void)why; abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static int failafter = -1;
void *__real_malloc(size_t); void *__real_calloc(size_t, size_t);
static BOOL Fail(void) { if (failafter < 0) return FALSE; if (!failafter) return TRUE; failafter--; return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return Fail() ? NULL : __real_calloc(n, s); }
static char g_RomExportError[256];
#include "export.inc"
#include "helpers.inc"

#include "persistence.inc"

/* Exercise the actual frame handler and selection readers with injected
 * rebuild/commit failures. Stable tile indices keep the selection intact. */
typedef void *HWND;
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
typedef struct ViewportState { EditorTool tool; BOOL showstan; int stanopacity, stancomponentcount; StanFile stan; unsigned char *stanselected; } ViewportState;
static ViewportState view;
static HWND g_Viewport = &view;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL flying, transforming, failrebuild;
static unsigned errors, restores, rebuilds;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
#include "selection.inc"
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **why)
{
    rebuilds++; view.stan = g_CurrentStan;
    if (failrebuild) { failrebuild = FALSE; memset(view.stanselected, 0, 4); *why = "Rebuild failed."; return FALSE; }
    return TRUE;
}
static void GEditorRestoreHistorySelection(HWND hwnd)
{ restores++; assert(g_EditHistory.selectionsize == 4); memcpy(view.stanselected, g_EditHistory.selection, 4); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd, const char *why, const char *title, unsigned flags) { assert(why[0]); errors++; }
#include "controller.inc"
static void Controller(const StanFile *source, const char *dir)
{
    const char *why = ""; unsigned char selected[4] = {0, 1, 0, 1};
    assert(StanFileClone(source, &g_CurrentStan, &why));
    view = (ViewportState){EDITOR_TOOL_FACE_SELECT, TRUE, 44, 0, g_CurrentStan, selected};
    g_CurrentBgDocument.roomcount = 10;
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory, selected, 4, FALSE, &why));
    for (int failure = 0; failure < 3; failure++)
    {
        ULONGLONG revision = g_EditHistory.nextrevision;
        failrebuild = failure == 0; if (failure == 1) g_EditHistory.nextrevision = 0; if (failure == 2) failafter = 0;
        assert(!GEditorSetSelectedStanType(NULL, 3)); failafter = -1; g_EditHistory.nextrevision = revision;
        Same(source, &g_CurrentStan); assert(selected[1] && selected[3] && !g_EditHistory.undocount);
    }
    assert(errors == 3 && restores == 2);
    flying = TRUE; assert(!GEditorSetSelectedStanType(NULL, 3)); flying = FALSE;
    transforming = TRUE; assert(!GEditorSetSelectedStanType(NULL, 3)); transforming = FALSE;
    view.tool = EDITOR_TOOL_EDGE_SELECT; assert(!GEditorSetSelectedStanType(NULL, 3)); view.tool = EDITOR_TOOL_FACE_SELECT;
    view.showstan = FALSE; assert(!GEditorSetSelectedStanType(NULL, 3)); view.showstan = TRUE;
    assert(!GEditorSetSelectedStanType(NULL, 11)); Same(source, &g_CurrentStan);
    assert(GEditorSetSelectedStanType(NULL, 3));
    assert(selected[1] && selected[3] && g_EditHistory.undocount == 1);
    assert(g_CurrentStan.tiles[1].special == STAN_TYPE_LADDER && g_CurrentStan.tiles[3].special == STAN_TYPE_LADDER);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Change Stan Type"));
    StanFile saved = Persist(dir, &g_CurrentStan); StanFileFree(&saved);
    EditHistoryMarkStanSaved(&g_EditHistory, &g_CurrentStan);
    unsigned before = rebuilds;
    assert(GEditorSetSelectedStanType(NULL, 3) && rebuilds == before && !g_CurrentStan.dirty && g_EditHistory.undocount == 1);
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    assert(g_CurrentStan.tiles[1].special == source->tiles[1].special && g_CurrentStan.tiles[3].special == source->tiles[3].special && g_CurrentStan.dirty);
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    assert(g_CurrentStan.tiles[1].special == STAN_TYPE_LADDER && g_CurrentStan.tiles[3].special == STAN_TYPE_LADDER && !g_CurrentStan.dirty);
    EditHistoryFree(&g_EditHistory); StanFileFree(&g_CurrentStan);
}
static void Reject(StanFile *stan, const DWORD *selected, DWORD count, StanTileType type)
{
    StanFile before = {0}; const char *why = ""; DWORD changed;
    assert(StanFileClone(stan, &before, &why));
    assert(!StanSetTileTypes(stan, selected, count, type, &changed, &why) && !changed && why[0]);
    Same(stan, &before); StanFileFree(&before);
}
int main(int argc, char **argv)
{
    assert(argc == 2); const char *why = ""; DWORD changed;
    StanFile original = Fixture(argv[1]), work = {0};
    assert(StanFileClone(&original, &work, &why));
    DWORD selected[] = {1, 2}, invalid[] = {1, 4}, duplicate[] = {1, 1};
    Reject(&work, selected, 2, 2); Reject(&work, selected, 2, 4);
    Reject(&work, selected, 2, -1); Reject(&work, selected, 2, 16);
    Reject(&work, invalid, 2, STAN_TYPE_LADDER); Reject(&work, selected, 0, STAN_TYPE_LADDER);
    work.tiles[2].sourceoffset++; Reject(&work, selected, 2, STAN_TYPE_LADDER); work.tiles[2].sourceoffset--;
    work.tiles[2].special = 3; Reject(&work, selected, 2, STAN_TYPE_LADDER); work.tiles[2].special = 2;
    work.tiles[2].sourceoffset = work.size; Reject(&work, selected, 2, STAN_TYPE_LADDER);
    work.tiles[2].sourceoffset = original.tiles[2].sourceoffset;
    assert(StanSetTileTypes(&work, duplicate, 2, STAN_TYPE_LADDER, &changed, &why) && changed == 1);
    assert(work.tiles[2].special == 2); /* An unselected unknown type survives. */
    assert(StanSetTileTypes(&work, selected, 2, STAN_TYPE_LADDER, &changed, &why) && changed == 1);
    assert(work.dirty);
    for (DWORD i = 0; i < work.size; i++)
        if (i == work.tiles[1].sourceoffset + 4 || i == work.tiles[2].sourceoffset + 4)
            assert((work.data[i] & 0x0f) == (original.data[i] & 0x0f) && work.data[i] >> 4 == STAN_TYPE_LADDER);
        else assert(work.data[i] == original.data[i]);
    for (DWORD i = 0; i < work.tilecount; i++)
    {
        StanTile expected = original.tiles[i];
        if (i == 1 || i == 2) expected.special = STAN_TYPE_LADDER;
        assert(!memcmp(&expected, work.tiles + i, sizeof(expected)));
    }
    const StanTileType types[] = {STAN_TYPE_NORMAL, STAN_TYPE_FORCED_CROUCH, STAN_TYPE_LADDER};
    for (unsigned i = 0; i < sizeof(types) / sizeof(*types); i++)
    {
        assert(StanSetTileTypes(&work, selected, 2, types[i], &changed, &why) && changed == 2);
        StanFile saved = Persist(argv[1], &work);
        assert(saved.tiles[1].special == types[i] && saved.tiles[2].special == types[i]);
        StanFileFree(&saved);
        work.dirty = FALSE;
        assert(StanSetTileTypes(&work, selected, 2, types[i], &changed, &why) && !changed && !work.dirty);
    }
    StanFileFree(&work);
    Controller(&original, argv[1]); StanFileFree(&original);
    puts("PASS: three native types, atomic rejection, mixed/unknown and duplicate selections, exact RGB/link/geometry preservation, save/ROM data, no-op edits, undo/redo and rollback.");
    return 0;
}
