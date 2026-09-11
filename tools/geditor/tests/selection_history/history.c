#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bghistory.h"

/* Document contents are represented by counts here; production history owns
 * all stack/revision/snapshot logic. These stubs isolate it from asset IO. */
BOOL BgDocumentClone(const BgDocument *src, BgDocument *dst, const char **why) { *dst = *src; return TRUE; }
BOOL SetupFileClone(const SetupFile *src, SetupFile *dst, const char **why) { *dst = *src; return TRUE; }
BOOL StanFileClone(const StanFile *src, StanFile *dst, const char **why) { *dst = *src; return TRUE; }
void BgDocumentFree(BgDocument *doc) { memset(doc, 0, sizeof(*doc)); }
void SetupFileFree(SetupFile *doc) { memset(doc, 0, sizeof(*doc)); }
void StanFileFree(StanFile *doc) { memset(doc, 0, sizeof(*doc)); }
char *lstrcpyn(char *dst, const char *src, int size) { snprintf(dst, size, "%s", src); return dst; }
static int allocations = -1;
static void *TestMalloc(size_t size)
{
    if (allocations == 0) { return NULL; }
    if (allocations > 0) { allocations--; }
    return malloc(size);
}
static void *TestRealloc(void *data, size_t size)
{
    if (allocations == 0) { return NULL; }
    if (allocations > 0) { allocations--; }
    return realloc(data, size);
}
#define malloc TestMalloc
#define realloc TestRealloc
#include "bghistory.c"
#undef malloc
#undef realloc

typedef int HWND;
typedef unsigned int UINT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
#define CALLBACK
#define MB_ICONERROR 0
#define GEDITOR_TITLE "test"
static EditHistory g_EditHistory;
static BgDocument bg;
static SetupFile setup;
static StanFile stan;
static BOOL g_SelectionHistoryPending, g_SelectionHistoryReset, g_SelectionHistoryNavigation;
static DWORD selected;
static unsigned int errors;
static LRESULT CALLBACK GEditorWndProc(HWND, UINT, WPARAM, LPARAM);
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static int MessageBox(HWND hwnd, const char *text, const char *title, int flags) { errors++; return 0; }
static BOOL GEditorCaptureSelection(void **data, size_t *size)
{
    *size = sizeof(selected);
    *data = malloc(*size);
    assert(*data);
    memcpy(*data, &selected, *size);
    return TRUE;
}
static BOOL GEditorRestoreHistorySelection(HWND hwnd)
{
    assert(g_EditHistory.selectionsize == sizeof(selected));
    selected = *(DWORD *)g_EditHistory.selection;
    return TRUE;
}
/* A realistic nesting pattern: mutation/rebuild/reselection all notify the
 * frame synchronously, while CommitEdit occurs between those notifications. */
static LRESULT GEditorDispatchMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    const char *why;
    EditHistoryTransaction tx;
    EditHistoryAsset asset;
    switch (msg)
    {
    case 1: /* load, including resets from another level at the same revision */
        EditHistoryReset(&g_EditHistory, &bg, &setup, &stan);
        selected = (DWORD)wparam;
        g_SelectionHistoryReset = TRUE;
        break;
    case 2: /* click / completed marquee / Escape */
        selected = (DWORD)wparam;
        g_SelectionHistoryPending = TRUE;
        break;
    case 3: /* delete selection, including nested scene rebuild notifications */
        assert(EditHistoryBeginBgEdit(&g_EditHistory, &bg, "Delete Faces", &tx, &why));
        bg.roomcount++;
        GEditorWndProc(hwnd, 2, 0, 0);
        assert(EditHistoryCommitEdit(&g_EditHistory, &bg, &setup, &stan, &tx, &why));
        GEditorWndProc(hwnd, 2, 0, 0);
        break;
    case 5: /* snap source to target, then clear selection after the edit */
    {
        DWORD source = selected;
        GEditorWndProc(hwnd, 2, 77, 0); /* Temporary target picking. */
        selected = source;
        GEditorWndProc(hwnd, 3, 0, 0); /* Nested edit/commit. */
        GEditorWndProc(hwnd, 2, 0, 0); /* Final automatic deselection. */
        break;
    }
    case 4: /* undo or redo, followed by the same rebuild/reselection sequence */
        g_SelectionHistoryNavigation = TRUE;
        assert(wparam ? EditHistoryRedo(&g_EditHistory, &bg, &setup, &stan, &asset, &why)
                      : EditHistoryUndo(&g_EditHistory, &bg, &setup, &stan, &asset, &why));
        GEditorWndProc(hwnd, 2, 0, 0);
        GEditorRestoreHistorySelection(hwnd);
        break;
    }
    return 0;
}
#include "frame.inc"

static void Selection(DWORD value) { GEditorWndProc(1, 2, value, 0); }
static void Step(BOOL redo) { GEditorWndProc(1, 4, redo, 0); }
static void Clean(void) { assert(!bg.dirty && !setup.dirty && !stan.dirty); }

int main(void)
{
    const char *why;
    EditHistoryTransaction tx;
    EditHistoryAsset asset;
    GEditorWndProc(1, 1, 0, 0);
    GEditorWndProc(1, 1, 3, 0);
    assert(!EditHistoryCanUndo(&g_EditHistory)); /* New level baseline. */
    Selection(1);
    Selection(2);
    Selection(2); /* No-op, including repeated notifications and previews. */
    assert(g_EditHistory.undocount == 2); Clean();
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Change Selection"));
    Step(FALSE); assert(selected == 1); Clean();
    Step(TRUE); assert(selected == 2); Clean();
    GEditorWndProc(1, 3, 0, 0);
    assert(g_EditHistory.undocount == 3 && selected == 0 && bg.dirty && bg.roomcount == 1);
    Step(FALSE); assert(selected == 2 && bg.roomcount == 0); Clean();
    Step(FALSE); assert(selected == 1); Clean();
    Step(TRUE); assert(selected == 2); Clean();
    Step(TRUE); assert(selected == 0 && bg.roomcount == 1 && bg.dirty);
    EditHistoryMarkBgSaved(&g_EditHistory, &bg); Clean();
    Selection(9); Clean();
    Step(FALSE); assert(selected == 0); Clean();
    Step(FALSE); assert(bg.roomcount == 0 && bg.dirty && selected == 2);
    Step(TRUE); assert(bg.roomcount == 1 && selected == 0); Clean();
    Selection(4); assert(!EditHistoryCanRedo(&g_EditHistory)); Clean();

    /* Setup and stan share chronology without dirtying each other. */
    assert(EditHistoryBeginSetupEdit(&g_EditHistory, &setup, "Move Camera", &tx, &why));
    setup.objectcount = 5;
    assert(EditHistoryCommitEdit(&g_EditHistory, &bg, &setup, &stan, &tx, &why));
    assert(setup.dirty && !bg.dirty && !stan.dirty);
    Selection(8);
    assert(EditHistoryBeginStanEdit(&g_EditHistory, &stan, "Move Stan", &tx, &why));
    stan.tilecount = 7;
    assert(EditHistoryCommitEdit(&g_EditHistory, &bg, &setup, &stan, &tx, &why));
    assert(stan.dirty && setup.dirty && !bg.dirty);
    Step(FALSE); assert(stan.tilecount == 0 && !stan.dirty && setup.dirty && selected == 8);
    Step(FALSE); assert(selected == 4 && setup.dirty && !bg.dirty && !stan.dirty);
    Step(FALSE); assert(setup.objectcount == 0); Clean();

    /* Failed selection allocation preserves both stacks, the current selection
     * and saved state. The frame restores the old highlight. */
    DWORD undocount = g_EditHistory.undocount, redocount = g_EditHistory.redocount;
    allocations = 0;
    Selection(99);
    assert(errors == 1 && selected == 4);
    assert(g_EditHistory.undocount == undocount && g_EditHistory.redocount == redocount); Clean();
    allocations = -1;
    assert(EditHistoryBeginBgEdit(&g_EditHistory, &bg, "Failed Edit", &tx, &why));
    bg.roomcount = 99;
    allocations = 0;
    assert(!EditHistoryCommitEdit(&g_EditHistory, &bg, &setup, &stan, &tx, &why));
    EditHistoryRollbackEdit(&tx, &bg, &setup, &stan);
    allocations = -1;
    assert(bg.roomcount == 1 && selected == 4);
    assert(g_EditHistory.undocount == undocount && g_EditHistory.redocount == redocount); Clean();

    /* The snap gesture is routed through one outer frame dispatch. Undo must
     * restore its source, with no intermediate target-pick history entry. */
    GEditorWndProc(1, 5, 0, 0);
    assert(selected == 0 && g_EditHistory.undocount == undocount + 1);
    Step(FALSE); assert(selected == 4 && bg.roomcount == 1); Clean();
    Step(TRUE); assert(selected == 0 && bg.roomcount == 2 && bg.dirty);
    Step(FALSE); assert(selected == 4 && bg.roomcount == 1); Clean();

    /* Bounded stack eviction and branch replacement free selection snapshots. */
    for (DWORD i = 100; i < 200; i++) { Selection(i); }
    assert(g_EditHistory.undocount == EDIT_HISTORY_LIMIT); Clean();
    for (DWORD i = 0; i < EDIT_HISTORY_LIMIT; i++) { Step(FALSE); }
    assert(!EditHistoryCanUndo(&g_EditHistory) && g_EditHistory.redocount == EDIT_HISTORY_LIMIT);
    for (DWORD i = 0; i < EDIT_HISTORY_LIMIT; i++) { Step(TRUE); }
    assert(selected == 199); Clean();
    EditHistoryReset(&g_EditHistory, &bg, &setup, &stan);
    assert(!EditHistoryCanUndo(&g_EditHistory) && !EditHistoryCanRedo(&g_EditHistory) && !g_EditHistory.selection);
    /* Empty/no-UI history consumers remain supported. */
    assert(EditHistoryBeginBgEdit(&g_EditHistory, &bg, "Edit", &tx, &why));
    bg.roomcount++;
    assert(EditHistoryCommitEdit(&g_EditHistory, &bg, &setup, &stan, &tx, &why));
    assert(EditHistoryUndo(&g_EditHistory, &bg, &setup, &stan, &asset, &why) && asset == EDIT_HISTORY_ASSET_BG);
    EditHistoryFree(&g_EditHistory);
    puts("Selection chronology, nested notifications, saved revisions, branching, limits and allocation failures passed.");
    return 0;
}
