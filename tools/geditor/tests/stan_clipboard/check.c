#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"
#include "bghistory.h"
#include "edittool.h"
static int failafter = -1;
void *__real_malloc(size_t); void *__real_calloc(size_t, size_t);
static BOOL Fail(void) { if (failafter < 0) return FALSE; if (!failafter) return TRUE; failafter--; return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return Fail() ? NULL : __real_calloc(n, s); }
void BgDocumentFree(BgDocument *doc) { assert(!doc->rooms); }
void BgPortalFileFree(BgPortalFile *portals) { assert(!portals->portals); }
BOOL SetupFileCompact(SetupFile *setup, const char **reason) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static const char *why = "";
static void Require(BOOL ok, const char *reason) { if (!ok) { fprintf(stderr, "%s\n", reason); abort(); } }
#include "fixture.inc"
static void RoundTrip(const StanFile *s)
{
    unsigned char *data = NULL; DWORD size; StanFile loaded = {0};
    Require(StanPrepareSave(s, &data, &size, &why), why);
    Require(StanLoadNative(data, size, s->levelscale, &loaded, &why), why);
    assert(s->tilecount == loaded.tilecount && Connections(s) == Connections(&loaded));
    for (DWORD t = 0; t < s->tilecount; t++)
    {
        const StanTile *a = &s->tiles[t], *b = &loaded.tiles[Find(&loaded, a->id)];
        assert(a->room == b->room && a->pointcount == b->pointcount && a->special == b->special);
        assert(a->red == b->red && a->green == b->green && a->blue == b->blue);
        for (DWORD p = 0; p < a->pointcount; p++)
        {
            assert(!memcmp(s->data + a->sourceoffset + 8 + p*8, loaded.data + b->sourceoffset + 8 + p*8, 6));
            DWORD x = StanLinkedTile(s, a->points[p].link), y = StanLinkedTile(&loaded, b->points[p].link);
            if (x == STAN_TILE_NONE) assert(a->points[p].link == b->points[p].link);
            else assert(y != STAN_TILE_NONE && s->tiles[x].id == loaded.tiles[y].id);
        }
    }
    assert(!memcmp(s->data + s->size - 32, loaded.data + loaded.size - 32, 32));
    free(data); StanFileFree(&loaded);
}
static void Native(const char *dir)
{
    StanFile source = Fixture(dir), before = {0}, copy = {0}, snapshot = {0};
    DWORD pair[] = {1,0}, out[4], changed;
    Require(StanSetTileTypes(&source, (DWORD[]){0}, 1, STAN_TYPE_LADDER, &changed, &why), why);
    Require(StanSetTileTypes(&source, (DWORD[]){1}, 1, STAN_TYPE_FORCED_CROUCH, &changed, &why), why);
    Require(StanFileClone(&source, &before, &why), why);
    Require(StanCopyTiles(&source, pair, 2, &copy, &why), why); Same(&source, &before);
    assert(copy.tilecount == 2 && Connections(&copy) == 2 && !copy.tiles[1].points[2].link);
    Require(StanFileClone(&copy, &snapshot, &why), why);
    assert(!StanCopyTiles(&source, (DWORD[]){0,0}, 2, &copy, &why)); Same(&copy, &snapshot);
    assert(!StanCopyTiles(&source, (DWORD[]){4}, 1, &copy, &why)); Same(&copy, &snapshot);
    int failures = 0;
    for (int f = 0; f < 32; f++)
    {
        failafter = f; BOOL ok = StanCopyTiles(&source, pair, 2, &copy, &why); failafter = -1;
        Same(&source, &before); Same(&copy, &snapshot);
        if (ok) break;
        assert(why[0]); failures++;
    }
    assert(failures >= 8);
    for (int f = 0; f < 4; f++)
    {
        out[0] = 999; failafter = f;
        BOOL ok = StanPasteTiles(&source, &copy, (double[]){0,40,0}, out, &why); failafter = -1;
        if (ok) break;
        assert(why[0] && out[0] == 999); Same(&source, &before);
    }
    assert(source.tilecount == 6 && out[0] == 4 && out[1] == 5 && Connections(&source) == 6);
    assert(!memcmp(source.tiles, before.tiles, before.tilecount * sizeof(*source.tiles)));
    assert(!memcmp(source.data, before.data, 180));
    for (DWORD t = 0; t < 2; t++)
    {
        const StanTile *a = &before.tiles[t], *b = &source.tiles[4+t];
        assert(a->room == b->room && a->special == b->special && a->red == b->red);
        for (DWORD p = 0; p < a->pointcount; p++)
        { assert(a->points[p].x == b->points[p].x && a->points[p].y + 40 == b->points[p].y && a->points[p].z == b->points[p].z); }
        for (DWORD i = 0; i < 4+t; i++) assert(source.tiles[i].id != b->id && source.tiles[i].editorid != b->editorid);
    }
    DWORD tile = 4; assert(StanWalkTiles(&source, &tile, 40,40,120,40) && tile == 5);
    RoundTrip(&source); Same(&copy, &snapshot);
    /* Clipboard survives deletion of its source; new scale still uses native positions. */
    StanFileFree(&source); source = Fixture(dir);
    source.levelscale = .5f;
    Require(StanPasteTiles(&source, &copy, (double[]){0,20,0}, out, &why), why);
    assert(source.tiles[out[0]].points[1].z == 40 && source.tiles[out[0]].points[0].y == 20);
    StanFileFree(&before); Require(StanFileClone(&source, &before, &why), why);
    assert(!StanPasteTiles(&source, &copy, (double[]){NAN,0,0}, out, &why)); Same(&source, &before);
    assert(!StanPasteTiles(&source, &copy, (double[]){65535,0,0}, out, &why)); Same(&source, &before);
    source.tiles[0].editorid = UINT32_MAX;
    assert(!StanPasteTiles(&source, &copy, (double[]){0,0,0}, out, &why));
    source.tiles[0].editorid = before.tiles[0].editorid; Same(&source, &before);
    StanFileFree(&copy); Require(StanCopyTiles(&source, (DWORD[]){2}, 1, &copy, &why), why);
    assert(copy.tiles[0].points[0].link == 0xf); /* Reserved boundary survives. */
    StanFileFree(&source); StanFileFree(&before); StanFileFree(&copy); StanFileFree(&snapshot);
    puts("PASS: native clipboard, attributes, internal links, detached boundaries, fresh identities, save/reload, scale changes, allocation and range failures.");
}

#include "viewport_harness.inc"
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
typedef struct ViewportTranslation { double offset[3]; } ViewportTranslation;
typedef struct ViewportRotation { Rotation rotation; double pivot[3]; } ViewportRotation;
typedef struct ViewportStanDuplicate {
    TransformMode mode; ViewportTranslation translation; ViewportRotation rotation; Scaling scaling;
} ViewportStanDuplicate;
static ViewportState view;
static HWND g_Viewport = &view;
static BgDocument g_CurrentBgDocument, g_FaceClipboard;
static BgPortalFile g_PortalClipboard;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan, g_StanClipboard;
static EditHistory g_EditHistory;
static BOOL flying, transforming, knife, failrebuild, failselection;
static unsigned errors;
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportKnifeActive(HWND hwnd) { return knife; }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **reason)
{ if (failrebuild) { failrebuild=FALSE; *reason="Rebuild failed."; return FALSE; } return ViewportSetStanTiles(&view, &g_CurrentStan); }
static void GEditorClearObjectClipboard(void) {}
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd, const char *reason, const char *title, int flags) { assert(reason[0]); errors++; }
static void Remember(void)
{
    DWORD count = ViewportGetStanSelectionCount(&view, NULL), selected[8];
    assert(ViewportGetSelectedStanTiles(&view, selected, count));
    Require(EditHistorySetSelection(&g_EditHistory, selected, count*sizeof(DWORD), FALSE, &why), why);
}
static void GEditorRestoreHistorySelection(HWND hwnd)
{ assert(ViewportSelectStanTiles(&view, g_EditHistory.selection, g_EditHistory.selectionsize/sizeof(DWORD))); }
static BOOL Select(HWND hwnd, const DWORD *selected, DWORD count)
{ if (failselection) { failselection=FALSE; return FALSE; } return ViewportSelectStanTiles(hwnd, selected, count); }
#define ViewportSelectStanTiles Select
#include "controller.inc"
#undef ViewportSelectStanTiles
static void Controller(const char *dir)
{
    StanFile before = {0}, clip = {0}; DWORD pair[] = {0,1}, selected[2];
    g_CurrentStan = Fixture(dir); Require(StanFileClone(&g_CurrentStan, &before, &why), why);
    view = (ViewportState){.showstan=TRUE, .stanopacity=100, .tool=EDITOR_TOOL_FACE_SELECT};
    assert(ViewportSetStanTiles(&view, &g_CurrentStan) && ViewportSelectStanTiles(&view, pair, 2));
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan); Remember();
    assert(GEditorCopyStanTiles(NULL)); Require(StanFileClone(&g_StanClipboard, &clip, &why), why);
    for (int f = 0; f < 4; f++)
    {
        ULONGLONG revision = g_EditHistory.nextrevision;
        failafter = f==0 ? 0 : -1; failrebuild=f==1; failselection=f==2; if(f==3) g_EditHistory.nextrevision=0;
        assert(!GEditorPasteStanTiles(NULL)); failafter=-1; g_EditHistory.nextrevision=revision;
        Same(&g_CurrentStan, &before); assert(!g_EditHistory.undocount);
        assert(ViewportGetSelectedStanTiles(&view, selected, 2) && selected[0]==0 && selected[1]==1);
    }
    assert(errors==4);
    knife=TRUE; assert(!GEditorCanPasteStanTiles()); knife=FALSE;
    transforming=TRUE; assert(!GEditorCanPasteStanTiles()); transforming=FALSE;
    flying=TRUE; assert(!GEditorCanCopyStanTiles()); flying=FALSE;
    assert(GEditorPasteStanTiles(NULL) && g_CurrentStan.tilecount==6 && g_EditHistory.undocount==1);
    assert(g_CurrentStan.tiles[4].points[0].y==40); Remember();
    assert(ViewportGetSelectedStanTiles(&view, selected, 2) && selected[0]==4 && selected[1]==5);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Paste Stan Tiles")); RoundTrip(&g_CurrentStan);
    EditHistoryMarkStanSaved(&g_EditHistory, &g_CurrentStan);
    Require(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why), why);
    assert(g_CurrentStan.tilecount==4 && g_CurrentStan.dirty); assert(GEditorReloadCurrentObjectsAndViewport(&why)); GEditorRestoreHistorySelection(NULL);
    assert(ViewportGetSelectedStanTiles(&view, selected, 2) && selected[0]==0);
    Require(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why), why);
    assert(g_CurrentStan.tilecount==6 && !g_CurrentStan.dirty); assert(GEditorReloadCurrentObjectsAndViewport(&why)); GEditorRestoreHistorySelection(NULL);
    for (int mode=0; mode<3; mode++)
    {
        assert(ViewportSelectStanTiles(&view, pair, 2)); Remember();
        ViewportStanDuplicate request={.mode=mode}; request.translation.offset[2]=40;
        RotationAxis(&request.rotation.rotation, 1, 90); request.rotation.pivot[0]=80;
        RotationAxis(&request.scaling.axes, 1, 0); request.scaling.factor[0]=2; request.scaling.factor[1]=1; request.scaling.factor[2]=.5;
        DWORD oldcount=g_CurrentStan.tilecount, oldundo=g_EditHistory.undocount;
        assert(GEditorDuplicateStanTiles(NULL, &request));
        assert(g_CurrentStan.tilecount==oldcount+2 && g_EditHistory.undocount==oldundo+1);
        assert(!memcmp(g_CurrentStan.tiles, before.tiles, before.tilecount*sizeof(*before.tiles))); Same(&g_StanClipboard, &clip);
        for (DWORD t=0; t<2; t++) for (DWORD p=0; p<4; p++)
        {
            const StanPoint *a=&before.tiles[t].points[p], *b=&g_CurrentStan.tiles[oldcount+t].points[p];
            double source[3]={a->x,a->y,a->z}, target[3];
            if(mode==TRANSFORM_MOVE) { memcpy(target,source,sizeof(target)); target[2]+=40; }
            else if(mode==TRANSFORM_ROTATE) RotationPoint(&request.rotation.rotation,request.rotation.pivot,source,target);
            else ScalingPoint(&request.scaling,source,target);
            assert(fabs(b->x-target[0])<.01 && fabs(b->y-target[1])<.01 && fabs(b->z-target[2])<.01);
        }
        RoundTrip(&g_CurrentStan);
    }
    EditHistoryFree(&g_EditHistory); StanFileFree(&g_CurrentStan); StanFileFree(&g_StanClipboard);
    StanFileFree(&before); StanFileFree(&clip); FreeView(&view);
    puts("PASS: production controller, native paste offset, move/rotate/scale copies, selection, one-step history, save state and rollback.");
}
int main(int argc, char **argv) { assert(argc==2); Native(argv[1]); Controller(argv[1]); return 0; }
