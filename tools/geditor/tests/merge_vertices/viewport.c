#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "edittool.h"
typedef void *HWND;
typedef intptr_t LPARAM;
#define VIEWPORT_WM_SELECTION_CHANGED 1
typedef struct SceneBatch { int first, count; BOOL object, secondary; } SceneBatch;
typedef struct ViewportComponent { BgDocumentVertexRef refs[2]; int corners[2]; } ViewportComponent;
typedef struct ViewportState {
    EditorTool tool;
    int scenecount, batchcount, componentcount, componentcapacity;
    unsigned char *hiddentris;
    SceneBatch *batches;
    ViewportComponent *components;
    BgDocumentVertexRef *scenevertexrefs;
    BOOL showbgprimary, showbgsecondary;
} ViewportState;
static unsigned notifications;
static BOOL failallocation;
static void *TestCalloc(size_t n, size_t size) { return failallocation ? NULL : calloc(n, size); }
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportClearAllSelection(ViewportState *s) { s->componentcount = 0; }
static void ViewportUpdateGizmo(ViewportState *s) {}
static void InvalidateRect(HWND hwnd, void *rect, BOOL erase) {}
static HWND GetParent(HWND hwnd) { return hwnd; }
static void SendMessage(HWND hwnd, int message, int wparam, LPARAM lparam) { notifications++; }
#define calloc TestCalloc
#include "viewport.inc"
#undef calloc
int main(void)
{
    BgDocumentVertexRef refs[] = {{1,4},{1,2},{1,1},{1,1},{1,0},{1,2}};
    unsigned char hidden[2] = {1,0}; SceneBatch batches[2] = {{0,3,FALSE,FALSE},{3,3,FALSE,TRUE}};
    ViewportState s = {.tool=EDITOR_TOOL_VERTEX_SELECT, .scenecount=6, .batchcount=2, .scenevertexrefs=refs,
        .hiddentris=hidden, .batches=batches, .showbgprimary=TRUE, .showbgsecondary=TRUE};
    BgDocumentVertexRef merged = {1,1};
    assert(ViewportSelectBgVertex(&s, &merged) && s.componentcount == 1 && notifications == 1);
    assert(s.components[0].corners[0] == 3 && s.components[0].corners[1] == 3); /* skip hidden primary face */
    assert(!memcmp(&s.components[0].refs[0], &merged, sizeof(merged)));
    ViewportComponent old = s.components[0];
    failallocation = TRUE; assert(!ViewportSelectBgVertex(&s, &merged)); failallocation = FALSE;
    assert(s.componentcount == 1 && !memcmp(&old, s.components, sizeof(old)) && notifications == 1);
    hidden[1] = 1; assert(ViewportSelectBgVertex(&s, &merged) && !s.componentcount && !s.components);
    hidden[1] = 0; assert(ViewportSelectBgVertex(&s, &merged) && s.componentcount == 1);
    merged.index = 99; assert(ViewportSelectBgVertex(&s, &merged) && !s.componentcount); /* no incident faces remain */
    assert(ViewportSelectBgVertex(&s, NULL) && !s.componentcount);
    s.tool = EDITOR_TOOL_EDGE_SELECT; assert(!ViewportSelectBgVertex(&s, &merged));
    free(s.components);
    puts("PASS: survivor selection uses visible source identity after reindexing, handles hidden/deleted faces, and preserves selection on allocation failure.");
    return 0;
}
