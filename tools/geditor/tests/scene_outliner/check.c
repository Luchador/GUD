#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *HWND;
typedef struct MSG MSG;
typedef uintptr_t ULONG_PTR, WPARAM;
typedef intptr_t LPARAM, LRESULT;
#define WM_APP 0x8000
#define WM_SETREDRAW 11
#define SCENEOUTLINER_WM_FRAME (WM_APP + 121)
#include "sceneoutliner.h"
#include "characterload.h"
#include "edittool.h"
#include "visibilitymenu.h"
#include "characternames.h"

typedef struct TreeItem {
    struct TreeItem *parent, *next, *child;
    char text[128]; LPARAM key;
    BOOL expanded;
} TreeItem, *HTREEITEM;
typedef struct { unsigned mask; HTREEITEM hItem; char *pszText; LPARAM lParam; } TVITEM;
typedef struct { HTREEITEM hParent, hInsertAfter; TVITEM item; } TVINSERTSTRUCT;
typedef struct { int x, y; } POINT;
typedef struct { POINT pt; unsigned flags; HTREEITEM hItem; } TVHITTESTINFO;
#define TVHT_ONITEM 0x46
#define TVHT_ONITEMLABEL 0x04
#define TVHT_ONITEMBUTTON 0x10
#define GET_X_LPARAM(p) ((int)(int16_t)((p) & 0xffff))
#define GET_Y_LPARAM(p) ((int)(int16_t)((p) >> 16))
#define TVIF_PARAM 1
#define TVIF_TEXT 2
#define TVI_FIRST ((HTREEITEM)(intptr_t)-1)
#define TVE_EXPAND 1
#define TVE_COLLAPSE 2
#define SCENEOUTLINER_GROUPS 3
#include "types.inc"

static SceneOutlinerState state;
static TreeItem roots[3];
static HWND outliner = &state;
static HTREEITEM treeSelection;
static int redraws, activations, zooms, selections;
static HWND focus;
static ULONG_PTR pendingFrame;
static int queuedFrames;
static HTREEITEM hitItem;
static unsigned hitFlags;
static BOOL nativeClickInProgress;
static DWORD GetMessagePos(void) { return ((DWORD)360 << 16) | (uint16_t)-1200; }
static BOOL ScreenToClient(HWND hwnd, POINT *point)
{
    assert(point->x == -1200 && point->y == 360); /* Secondary monitor; signed coordinates. */
    point->x += 1300; point->y -= 300; return TRUE;
}
static HTREEITEM TreeView_HitTest(HWND hwnd, TVHITTESTINFO *hit)
{
    assert(hit->pt.x == 100 && hit->pt.y == 60);
    hit->hItem = hitItem; hit->flags = hitFlags; return hitItem;
}
static BOOL PostMessage(HWND hwnd, unsigned message, WPARAM wparam, LPARAM lparam)
{
    assert(message == SCENEOUTLINER_WM_FRAME && !lparam);
    pendingFrame = wparam; queuedFrames++; return TRUE;
}
static SceneOutlinerSelection last;
static HWND g_Viewport = (HWND)1, g_ToolToolbar = (HWND)2, g_VisibilityMenu = (HWND)3;
static SetupFile g_CurrentSetup;
static struct { BgPortalFile portals; } g_CurrentBgDocument;
static struct { DWORD levelcount; } g_Project = {1};
static DWORD g_CurrentLevelIndex, selectedIndex;
static SceneOutlinerKind selectedKind;
static BOOL flying, transforming;
static int revealed;
static EditorTool tool = EDITOR_TOOL_VERTEX_SELECT;

static SceneOutlinerState *SceneOutlinerGetState(HWND hwnd) { return hwnd; }
static HWND GetParent(HWND hwnd) { return (HWND)4; }
static LRESULT SendMessage(HWND hwnd, unsigned message, WPARAM wparam, LPARAM lparam);
static void InvalidateRect(HWND hwnd, const void *rect, BOOL erase) {}
static HTREEITEM TreeView_GetSelection(HWND hwnd) { return treeSelection; }
static void TreeView_SelectItem(HWND hwnd, HTREEITEM item);
static void TreeView_EnsureVisible(HWND hwnd, HTREEITEM item) { item->parent->expanded = TRUE; }
static void TreeView_Expand(HWND hwnd, HTREEITEM item, unsigned mode) { item->expanded = mode == TVE_EXPAND; }
static BOOL TreeView_GetItem(HWND hwnd, TVITEM *item)
{ item->lParam = item->hItem->key; return TRUE; }
static BOOL TreeView_SetItem(HWND hwnd, const TVITEM *item)
{ strcpy(item->hItem->text, item->pszText); return TRUE; }
static HTREEITEM TreeView_InsertItem(HWND hwnd, const TVINSERTSTRUCT *insert)
{
    TreeItem *item = calloc(1, sizeof(*item)); assert(item);
    item->parent = insert->hParent; item->key = insert->item.lParam;
    strcpy(item->text, insert->item.pszText);
    TreeItem **slot = insert->hInsertAfter == TVI_FIRST ? &item->parent->child : &insert->hInsertAfter->next;
    item->next = *slot; *slot = item; return item;
}
static void TreeView_DeleteItem(HWND hwnd, HTREEITEM item)
{
    TreeItem **slot = &item->parent->child;
    while (*slot != item) { assert(*slot); slot = &(*slot)->next; }
    *slot = item->next;
    if (treeSelection == item) { TreeView_SelectItem(hwnd, item->parent); }
    free(item);
}

BOOL CharacterGetModelDefinition(int id, CharacterModelDefinition *out)
{
    const char *file = id == 17 ? "CarmourguardZ" : id == 38 ? "CbluecamguardZ"
        : id == 19 ? "CgreatguardZ" : id == 5 ? "CdjbondZ" : id == 99 ? "CcustomZ" : NULL;
    if (!file) { return FALSE; }
    out->filename = file; return TRUE;
}
#include "names.inc"
BOOL ObjectGetSetupModelName(const SetupFile *setup, DWORD index, const char **out)
{ *out = setup->objects[index].modelid == 99 ? "Pdesk_arecibo1Z" : NULL; return *out != NULL; }
#include "outliner.inc"

static BOOL DoubleClickNotification(HWND hwnd, SceneOutlinerState *state)
{
#include "double_click.inc"
    return FALSE;
}
static LRESULT DispatchFrameMessage(HWND hwnd, SceneOutlinerState *state, unsigned message, WPARAM wparam)
{
    switch (message) {
#include "frame_message.inc"
    default: assert(0); return 0;
    }
}
static void DoubleClick(HTREEITEM item)
{
    focus = outliner; nativeClickInProgress = TRUE;
    TreeView_SelectItem(state.tree, item);
    int before = zooms;
    hitItem = item; hitFlags = TVHT_ONITEMLABEL;
    assert(DoubleClickNotification(outliner, &state));
    assert(zooms == before && focus == outliner);
    /* Native control finishes focus processing after sending its notification. */
    focus = outliner; nativeClickInProgress = FALSE;
    DispatchFrameMessage(outliner, &state, SCENEOUTLINER_WM_FRAME, pendingFrame);
    assert(zooms == before + 1 && focus == g_Viewport);
}

static void TreeView_SelectItem(HWND hwnd, HTREEITEM item)
{
    if (treeSelection == item) { return; }
    treeSelection = item;
    SceneOutlinerActivate(outliner, &state, item, FALSE);
}
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static void PatrolEditorSetPicking(BOOL value) { assert(!value); }
static void ViewportSetColorPick(HWND hwnd, BOOL value) { assert(!value); }
static void ViewportSetDoorPick(HWND hwnd, BOOL value) { assert(!value); }
static void ViewportSetTool(HWND hwnd, EditorTool value) { tool = value; }
static void ToolToolbarSetTool(HWND hwnd, EditorTool value) { assert(value == tool); }
void VisibilityMenuReveal(HWND hwnd, DWORD mask) { revealed = mask; }
static BOOL ViewportSelectSetupModels(HWND hwnd, const DWORD *ids, DWORD count)
{
    assert(count == 1 && tool == EDITOR_TOOL_FACE_SELECT && revealed == VISIBILITY_SHOW_OBJECTS);
    selectedKind = *ids & SETUP_CHARACTER_SELECTION_BIT ? SCENE_OUTLINER_CHARACTER : SCENE_OUTLINER_OBJECT;
    selectedIndex = *ids & ~SETUP_CHARACTER_SELECTION_BIT; selections++; return TRUE;
}
static BOOL ViewportSelectPortal(HWND hwnd, DWORD index)
{
    assert(tool == EDITOR_TOOL_FACE_SELECT && revealed == VISIBILITY_SHOW_PORTALS);
    selectedKind = SCENE_OUTLINER_PORTAL; selectedIndex = index; selections++; return TRUE;
}
static void GEditorRefreshSelectionDetails(void)
{
    SceneOutlinerRefresh(outliner, &g_CurrentSetup, &g_CurrentBgDocument.portals);
    SceneOutlinerSelect(outliner, selectedKind, selectedIndex);
}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static BOOL ViewportZoomToSelected(HWND hwnd)
{
    assert(!nativeClickInProgress && focus == g_Viewport);
    zooms++; return TRUE;
}
static void SetFocus(HWND hwnd) { focus = hwnd; }
#include "dispatch.inc"
static LRESULT SendMessage(HWND hwnd, unsigned message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_SETREDRAW) { redraws++; return 0; }
    assert(message == SCENEOUTLINER_WM_SELECT);
    last = *(SceneOutlinerSelection *)lparam; activations++;
    return GEditorSelectSceneItem(hwnd, &last);
}

int main(void)
{
    for (int i = 0; i < 3; i++) { state.groups[i].root = &roots[i]; state.groups[i].expanded = TRUE; }
    SetupCharacter characters[] = {
        {.chrnum = 713, .bodyid = 17}, {.chrnum = 54, .bodyid = 38}, {.chrnum = 99, .bodyid = 19, .deleted = TRUE}};
    SetupObject objects[15] = {0};
    for (int i = 0; i < 15; i++) { objects[i].deleted = TRUE; }
    objects[13] = (SetupObject){.modelid = 99}; objects[14].deleted = FALSE;
    BgPortal portals[3] = {0};
    g_CurrentSetup = (SetupFile){.characters = characters, .charactercount = 3, .objects = objects, .objectcount = 15};
    g_CurrentBgDocument.portals = (BgPortalFile){portals, 3};
    GEditorRefreshSelectionDetails();
    assert(!strcmp(roots[0].child->text, "Janus Marine 713"));
    assert(!strcmp(roots[0].child->next->text, "Arctic Commando 54") && !roots[0].child->next->next);
    assert(!strcmp(roots[1].child->text, "Pdesk_arecibo1Z 13"));
    assert(!strcmp(roots[1].child->next->text, "Object 14"));
    assert(!strcmp(roots[2].child->text, "Portal 0"));
    assert(!strcmp(CharacterGetBodyName(19), "Siberian Guard"));
    assert(!strcmp(CharacterGetBodyName(5), "Bond"));
    assert(!strcmp(CharacterGetBodyName(0xffff), "Random Guard"));
    assert(!strcmp(CharacterGetBodyName(99), "CcustomZ"));
    assert(!strcmp(CharacterGetBodyName(-1), "Unknown Character"));
    assert(!activations);

    /* Repeated inspector refreshes must not rebuild the tree or repaint it. */
    HTREEITEM marine = roots[0].child, desk = roots[1].child;
    int before = redraws;
    GEditorRefreshSelectionDetails();
    assert(redraws == before && roots[0].child == marine && roots[1].child == desk);
    focus = outliner;
    TreeView_SelectItem(state.tree, marine);
    assert(activations == 1 && selectedKind == SCENE_OUTLINER_CHARACTER && selectedIndex == 0);
    assert(!zooms && focus == outliner); /* chrnum 713 is a label, not index 0. */
    DoubleClick(desk);
    assert(activations == 3 && selections == 3 && selectedKind == SCENE_OUTLINER_OBJECT && selectedIndex == 13);
    assert(zooms == 1 && focus == g_Viewport && treeSelection == desk);
    DoubleClick(desk); /* Repeated activation of the already-selected item. */
    DoubleClick(marine);
    DoubleClick(roots[2].child->next);
    assert(selectedKind == SCENE_OUTLINER_PORTAL && selectedIndex == 1 && zooms == 4);
    assert(queuedFrames == 4);

    /* Group labels, expand buttons and empty space do not frame anything. */
    hitItem = &roots[0]; assert(!DoubleClickNotification(outliner, &state));
    hitItem = desk; hitFlags = TVHT_ONITEMBUTTON;
    assert(!DoubleClickNotification(outliner, &state));
    hitItem = NULL; hitFlags = 0; assert(!DoubleClickNotification(outliner, &state));
    hitItem = desk; hitFlags = TVHT_ONITEMLABEL; state.updating = TRUE;
    assert(!DoubleClickNotification(outliner, &state)); state.updating = FALSE;
    assert(queuedFrames == 4 && zooms == 4);

    /* Delayed requests must not jump to the previous item after selection changes. */
    assert(DoubleClickNotification(outliner, &state));
    DispatchFrameMessage(outliner, &state, SCENEOUTLINER_WM_FRAME, pendingFrame);
    assert(zooms == 4);

    /* Body/number changes, tombstones, undo, additions and removed portal IDs. */
    state.groups[0].expanded = FALSE; roots[0].expanded = FALSE;
    characters[0].bodyid = 38; characters[0].chrnum = 27;
    objects[13].deleted = TRUE;
    GEditorRefreshSelectionDetails();
    assert(roots[0].child == marine && !strcmp(marine->text, "Arctic Commando 27") && !roots[0].expanded);
    assert(!state.groups[1].rows[13].item && roots[1].child == state.groups[1].rows[14].item);
    objects[13].deleted = FALSE; characters[2].deleted = FALSE;
    GEditorRefreshSelectionDetails();
    assert(roots[1].child == state.groups[1].rows[13].item);
    assert(!strcmp(roots[0].child->next->next->text, "Siberian Guard 99"));
    selectedKind = SCENE_OUTLINER_CHARACTER; selectedIndex = 2;
    GEditorRefreshSelectionDetails();
    assert(treeSelection == state.groups[0].rows[2].item && roots[0].expanded);
    before = activations;
    characters[2].deleted = TRUE; selectedKind = SCENE_OUTLINER_NONE;
    g_CurrentBgDocument.portals.portalcount = 1;
    GEditorRefreshSelectionDetails();
    assert(!treeSelection && !roots[2].child->next && activations == before);
    g_CurrentBgDocument.portals.portalcount = 3;
    GEditorRefreshSelectionDetails();
    assert(!strcmp(roots[2].child->next->next->text, "Portal 2"));
    SceneOutlinerSelection stale = {SCENE_OUTLINER_CHARACTER, 2, TRUE};
    assert(!GEditorSelectSceneItem(NULL, &stale));
    stale = (SceneOutlinerSelection){SCENE_OUTLINER_PORTAL, 9, TRUE};
    assert(!GEditorSelectSceneItem(NULL, &stale));
    assert(zooms == 4 && selections == 8);

    /* Closing a level empties all groups without touching the document. */
    g_CurrentSetup = (SetupFile){0}; g_CurrentBgDocument.portals = (BgPortalFile){0};
    GEditorRefreshSelectionDetails();
    for (int i = 0; i < 3; i++) { assert(!roots[i].child); free(state.groups[i].rows); }
    puts("PASS: canonical body names, stable IDs, grouped updates, selection feedback, single-click selection, deferred double-click framing, focus order and stale requests.");
    return 0;
}
