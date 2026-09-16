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

void BgDocumentFree(BgDocument *doc) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static int failafter = -1;
void *__real_malloc(size_t); void *__real_calloc(size_t, size_t);
static BOOL Fail(void) { if (failafter < 0) return FALSE; if (!failafter) return TRUE; failafter--; return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return Fail() ? NULL : __real_calloc(n, s); }
static char g_RomExportError[256];
#include "export.inc"
#include "helpers.inc"
#include "input.inc"

static DWORD ById(const StanFile *stan, DWORD id)
{
    for (DWORD i = 0; i < stan->tilecount; i++) if (stan->tiles[i].id == id) return i;
    assert(!"Missing tile"); return STAN_TILE_NONE;
}
static DWORD PointerId(const StanFile *stan, DWORD pointer)
{
    for (DWORD i = 0; i < stan->tilecount; i++)
        if (stan->tiles[i].sourceoffset == (pointer & 0xffffffu)) return stan->tiles[i].id;
    assert(!"Missing header target"); return 0;
}
static void Equivalent(const StanFile *a, const StanFile *b)
{
    assert(a->size == b->size && a->tilecount == b->tilecount);
    for (DWORD i = 0; i < a->tilecount; i++)
    {
        const StanTile *x = a->tiles + i, *y = b->tiles + ById(b, x->id);
        StanTile expected = *x;
        expected.sourceoffset = y->sourceoffset;
        assert(!memcmp(a->data + x->sourceoffset, b->data + y->sourceoffset, 8));
        for (DWORD p = 0; p < x->pointcount; p++)
        {
            unsigned short link = x->points[p].link;
            if (link >= 0x10)
            {
                DWORD target = StanLinkedTile(a, link), other = StanLinkedTile(b, y->points[p].link);
                assert(target != STAN_TILE_NONE && other != STAN_TILE_NONE);
                assert(a->tiles[target].id == b->tiles[other].id);
            }
            else { assert(link == y->points[p].link); }
            expected.points[p].link = y->points[p].link;
            assert(!memcmp(a->data + x->sourceoffset + 8 + p * 8,
                           b->data + y->sourceoffset + 8 + p * 8, 6));
        }
        assert(!memcmp(&expected, y, sizeof(expected)));
    }
    DWORD first = a->tiles[0].sourceoffset;
    assert(first == b->tiles[0].sourceoffset && Get(a->data) == Get(b->data));
    for (DWORD o = 4; o < first - 4; o += 4)
    {
        DWORD x = Get(a->data + o), y = Get(b->data + o);
        assert((x & 0xff000000u) == (y & 0xff000000u));
        assert(PointerId(a, x) == PointerId(b, y));
    }
    DWORD end = a->tiles[a->tilecount - 1].sourceoffset + 8 + a->tiles[a->tilecount - 1].pointcount * 8;
    assert(!memcmp(a->data + end, b->data + end, a->size - end));
}
static void Grouped(const StanFile *stan)
{
    /* Mirror the engine: only the last run indexed for each room is scanned.
       Every tile must be reachable through that room's one contiguous run. */
    DWORD first[139];
    for (int r = 0; r < 139; r++) first[r] = STAN_TILE_NONE;
    for (DWORD i = 0; i < stan->tilecount; i++)
        if (!i || stan->tiles[i].room != stan->tiles[i - 1].room) first[stan->tiles[i].room] = i;
    DWORD reached = 0;
    for (int r = 0; r < 139; r++)
        for (DWORD i = first[r]; i < stan->tilecount && stan->tiles[i].room == r; i++) reached++;
    assert(reached == stan->tilecount);
}
static StanFile Persist(const char *dir, const StanFile *stan)
{
    const char *why = ""; StanFile loaded = {0}, before = {0}; GEditorProject project = {0}; char path[MAX_PATH]; DWORD size;
    assert(StanFileClone(stan, &before, &why));
    assert(StanSaveProjectFile(dir, stan, &why)); Same(stan, &before); StanFileFree(&before);
    assert(StanLoadProjectFile(dir, stan->name, stan->levelscale, &loaded, &why));
    Equivalent(stan, &loaded); Grouped(&loaded);
    strcpy(project.dir, dir); assert(RomExportProjectResourcePath(&project, stan->name, path, sizeof(path)) == 1);
    unsigned char *resource = RomExportReadResource(path, stan->name, &size, &why);
    assert(resource && size == loaded.size && !memcmp(resource, loaded.data, size)); free(resource);
    assert(StanSaveProjectFile(dir, &loaded, &why));
    resource = RomExportReadResource(path, stan->name, &size, &why);
    assert(resource && size == loaded.size && !memcmp(resource, loaded.data, size)); free(resource);
    return loaded;
}

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
        assert(!GEditorSetSelectedStanRoom(NULL, 3)); failafter = -1; g_EditHistory.nextrevision = revision;
        Same(source, &g_CurrentStan); assert(selected[1] && selected[3] && !g_EditHistory.undocount);
    }
    assert(errors == 3 && restores == 2);
    flying = TRUE; assert(!GEditorSetSelectedStanRoom(NULL, 3)); flying = FALSE;
    transforming = TRUE; assert(!GEditorSetSelectedStanRoom(NULL, 3)); transforming = FALSE;
    view.tool = EDITOR_TOOL_EDGE_SELECT; assert(!GEditorSetSelectedStanRoom(NULL, 3)); view.tool = EDITOR_TOOL_FACE_SELECT;
    view.showstan = FALSE; assert(!GEditorSetSelectedStanRoom(NULL, 3)); view.showstan = TRUE;
    assert(!GEditorSetSelectedStanRoom(NULL, 11)); Same(source, &g_CurrentStan);
    assert(GEditorSetSelectedStanRoom(NULL, 3));
    assert(selected[1] && selected[3] && g_EditHistory.undocount == 1);
    assert(g_CurrentStan.tiles[1].room == 3 && g_CurrentStan.tiles[3].room == 3);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Change Stan Room"));
    StanFile saved = Persist(dir, &g_CurrentStan); StanFileFree(&saved);
    EditHistoryMarkStanSaved(&g_EditHistory, &g_CurrentStan);
    unsigned before = rebuilds;
    assert(GEditorSetSelectedStanRoom(NULL, 3) && rebuilds == before && !g_CurrentStan.dirty && g_EditHistory.undocount == 1);
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    assert(g_CurrentStan.tiles[1].room == 1 && g_CurrentStan.tiles[3].room == 2 && g_CurrentStan.dirty);
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    assert(g_CurrentStan.tiles[1].room == 3 && g_CurrentStan.tiles[3].room == 3 && !g_CurrentStan.dirty);
    EditHistoryFree(&g_EditHistory); StanFileFree(&g_CurrentStan);
}
static void Reject(StanFile *stan, const DWORD *selected, DWORD count, DWORD room, DWORD rooms)
{
    StanFile before = {0}; const char *why = ""; DWORD changed;
    assert(StanFileClone(stan, &before, &why));
    assert(!StanSetTileRooms(stan, selected, count, room, rooms, &changed, &why) && !changed && why[0]);
    Same(stan, &before); StanFileFree(&before);
}
int main(int argc, char **argv)
{
    assert(argc == 2); const char *why = ""; DWORD changed, room;
    assert(RightPanelParseStanRoom("34", 90, &room) && room == 34);
    assert(RightPanelParseStanRoom("138", 200, &room) && room == 138);
    const char *bad[] = {"", "Mixed", "0", "-1", "+3", "1.5", "139", "4294967297", "3x"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++) assert(!RightPanelParseStanRoom(bad[i], 200, &room));
    assert(!RightPanelParseStanRoom("91", 90, &room));
    StanFile original = Fixture(argv[1]), work = {0};
    assert(StanFileClone(&original, &work, &why));
    DWORD selected[] = {1, 3}, invalid[] = {1, 4};
    Reject(&work, selected, 2, 0, 90); Reject(&work, selected, 2, 139, 200);
    Reject(&work, selected, 2, 91, 90); Reject(&work, invalid, 2, 3, 90);
    work.tiles[3].sourceoffset++; Reject(&work, selected, 2, 3, 90); work.tiles[3].sourceoffset--;
    assert(StanSetTileRooms(&work, selected, 2, 3, 90, &changed, &why) && changed == 2);
    for (DWORD i = 0; i < work.size; i++)
        if (i != work.tiles[1].sourceoffset + 3 && i != work.tiles[3].sourceoffset + 3) assert(work.data[i] == original.data[i]);
    /* Allocation failures during regrouping must leave the existing file and live document untouched. */
    for (int fail = 0; fail < 2; fail++)
    {
        failafter = fail; assert(!StanSaveProjectFile(argv[1], &work, &why)); failafter = -1;
        StanFile loaded = {0}; assert(StanLoadProjectFile(argv[1], work.name, work.levelscale, &loaded, &why));
        Same(&loaded, &original); StanFileFree(&loaded);
    }
    StanFile saved = Persist(argv[1], &work);
    assert(saved.tiles[0].id == original.tiles[0].id && saved.tiles[1].id == original.tiles[1].id
        && saved.tiles[2].id == original.tiles[3].id && saved.tiles[3].id == original.tiles[2].id);
    StanFileFree(&saved); StanFileFree(&work);
    Controller(&original, argv[1]); StanFileFree(&original);

    /* The two Depot doorway tiles from the reported disappearing-level case. */
    assert(StanLoadProjectFile(argv[1], "Tbg_depo_all_p_stanZ", .21847887f, &work, &why));
    selected[0] = ById(&work, 0x03e711); selected[1] = ById(&work, 0x03e712);
    assert(work.tiles[selected[0]].room == 36 && work.tiles[selected[1]].room == 36);
    assert(StanSetTileRooms(&work, selected, 2, 34, 89, &changed, &why) && changed == 2);
    saved = Persist(argv[1], &work);
    DWORD start = ById(&saved, 0x000619);
    assert(StanWalkTiles(&saved, &start, 345, 950, 352, 950));
    assert(saved.tiles[start].id == 0x03e711 && saved.tiles[start].room == 34);
    for (DWORD i = 0; i < saved.tilecount; i++) assert(saved.tiles[i].room != 36);
    StanFileFree(&saved); StanFileFree(&work);
    puts("PASS: room input, multi-selection, limits/atomic failures, stable indices, grouped runtime scans, preserved tile attributes/header/footer/link targets, save/ROM data, undo/redo, rollback and Depot doorway crossing.");
    return 0;
}
