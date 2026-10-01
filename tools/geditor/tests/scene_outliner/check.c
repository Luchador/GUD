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
static BOOL ViewportZoomToSelected(HWND hwnd) { zooms++; return TRUE; }
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
    assert(SceneOutlinerActivate(outliner, &state, desk, TRUE));
    assert(activations == 2 && selections == 2 && selectedKind == SCENE_OUTLINER_OBJECT && selectedIndex == 13);
    assert(zooms == 1 && focus == g_Viewport && treeSelection == desk);
    assert(!SceneOutlinerActivate(outliner, &state, &roots[0], TRUE));
    assert(SceneOutlinerActivate(outliner, &state, roots[2].child->next, TRUE));
    assert(selectedKind == SCENE_OUTLINER_PORTAL && selectedIndex == 1 && zooms == 2);

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
    assert(zooms == 2 && selections == 3);

    /* Closing a level empties all groups without touching the document. */
    g_CurrentSetup = (SetupFile){0}; g_CurrentBgDocument.portals = (BgPortalFile){0};
    GEditorRefreshSelectionDetails();
    for (int i = 0; i < 3; i++) { assert(!roots[i].child); free(state.groups[i].rows); }
    puts("PASS: canonical body names, stable IDs, grouped updates, selection feedback, single-click selection and double-click framing.");
    return 0;
}
