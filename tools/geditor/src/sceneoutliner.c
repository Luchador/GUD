/* Level outliner. Keep rows in native ID order, independent of their labels.
 * Incremental updates preserve expansion, scroll position and tree handles. */
#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sceneoutliner.h"
#include "characterload.h"
#include "objectload.h"

#define SCENEOUTLINER_CLASS "GEditorSceneOutliner"
#define SCENEOUTLINER_GROUPS 3

typedef struct SceneOutlinerRow {
    HTREEITEM item;
    char label[128];
} SceneOutlinerRow;

typedef struct SceneOutlinerGroup {
    HTREEITEM root;
    SceneOutlinerRow *rows;
    DWORD count;
    BOOL expanded;
} SceneOutlinerGroup;

typedef struct SceneOutlinerState {
    HWND tree;
    SceneOutlinerGroup groups[SCENEOUTLINER_GROUPS];
    BOOL updating, redraw;
    ULONG_PTR selection;
} SceneOutlinerState;

static SceneOutlinerState *SceneOutlinerGetState(HWND hwnd)
{ return (SceneOutlinerState *)GetWindowLongPtr(hwnd, GWLP_USERDATA); }

static ULONG_PTR SceneOutlinerKey(SceneOutlinerKind kind, DWORD index)
{ return kind == SCENE_OUTLINER_NONE ? 0 : ((ULONG_PTR)index << 2) | kind; }

static BOOL SceneOutlinerLabel(SceneOutlinerKind kind, DWORD index,
    const SetupFile *setup, const BgPortalFile *portals, char label[128])
{
    const char *name = NULL;
    if (kind == SCENE_OUTLINER_CHARACTER)
    {
        if (!setup || !setup->characters || index >= setup->charactercount
            || setup->characters[index].deleted) { return FALSE; }
        const SetupCharacter *character = &setup->characters[index];
        snprintf(label, 128, "%s %u", CharacterGetBodyName(character->bodyid), (unsigned)character->chrnum);
    }
    else if (kind == SCENE_OUTLINER_OBJECT)
    {
        if (!setup || !setup->objects || index >= setup->objectcount
            || setup->objects[index].deleted) { return FALSE; }
        ObjectGetSetupModelName(setup, index, &name);
        snprintf(label, 128, "%s %u", name ? name : "Object", (unsigned)index);
    }
    else if (kind == SCENE_OUTLINER_PORTAL)
    {
        if (!portals || !portals->portals || index >= portals->portalcount) { return FALSE; }
        snprintf(label, 128, "Portal %u", (unsigned)index);
    }
    else { return FALSE; }
    return TRUE;
}

static void SceneOutlinerBeginUpdate(SceneOutlinerState *state)
{
    if (!state->redraw)
    { state->redraw = TRUE; SendMessage(state->tree, WM_SETREDRAW, FALSE, 0); }
}

static void SceneOutlinerDeleteRow(SceneOutlinerState *state, SceneOutlinerRow *row)
{
    if (row->item)
    {
        SceneOutlinerBeginUpdate(state);
        if (TreeView_GetSelection(state->tree) == row->item)
        { state->selection = ~(ULONG_PTR)0; }
        TreeView_DeleteItem(state->tree, row->item);
        memset(row, 0, sizeof(*row));
    }
}

static void SceneOutlinerRefreshGroup(SceneOutlinerState *state, SceneOutlinerKind kind,
    DWORD count, const SetupFile *setup, const BgPortalFile *portals)
{
    SceneOutlinerGroup *group = &state->groups[kind - 1];
    if (count > group->count)
    {
        /* Setup files cap their record counts well below the packed-key limit. */
        SceneOutlinerRow *rows = count <= 1000000u ? realloc(group->rows, (size_t)count * sizeof(*rows)) : NULL;
        if (!rows)
        {
            for (DWORD i = 0; i < group->count; i++) { SceneOutlinerDeleteRow(state, &group->rows[i]); }
            free(group->rows); group->rows = NULL; group->count = 0;
            return;
        }
        memset(rows + group->count, 0, (size_t)(count - group->count) * sizeof(*rows));
        group->rows = rows;
    }
    for (DWORD i = count; i < group->count; i++) { SceneOutlinerDeleteRow(state, &group->rows[i]); }
    group->count = count;
    HTREEITEM previous = TVI_FIRST;
    for (DWORD i = 0; i < count; i++)
    {
        SceneOutlinerRow *row = &group->rows[i];
        char label[128];
        if (!SceneOutlinerLabel(kind, i, setup, portals, label))
        { SceneOutlinerDeleteRow(state, row); continue; }
        if (!row->item)
        {
            TVINSERTSTRUCT insert = {0};
            insert.hParent = group->root; insert.hInsertAfter = previous;
            insert.item.mask = TVIF_TEXT | TVIF_PARAM;
            insert.item.pszText = label; insert.item.lParam = (LPARAM)SceneOutlinerKey(kind, i);
            SceneOutlinerBeginUpdate(state);
            row->item = TreeView_InsertItem(state->tree, &insert);
            if (!row->item) { continue; }
            strcpy(row->label, label);
        }
        else if (strcmp(row->label, label))
        {
            TVITEM item = {0};
            item.hItem = row->item; item.mask = TVIF_TEXT; item.pszText = label;
            SceneOutlinerBeginUpdate(state);
            if (TreeView_SetItem(state->tree, &item)) { strcpy(row->label, label); }
        }
        previous = row->item;
    }
    if (state->redraw)
    { TreeView_Expand(state->tree, group->root, group->expanded ? TVE_EXPAND : TVE_COLLAPSE); }
}

void SceneOutlinerRefresh(HWND hwnd, const SetupFile *setup, const BgPortalFile *portals)
{
    SceneOutlinerState *state = SceneOutlinerGetState(hwnd);
    if (!state) { return; }
    state->updating = TRUE;
    SceneOutlinerRefreshGroup(state, SCENE_OUTLINER_CHARACTER,
        setup && setup->characters ? setup->charactercount : 0, setup, portals);
    SceneOutlinerRefreshGroup(state, SCENE_OUTLINER_OBJECT,
        setup && setup->objects ? setup->objectcount : 0, setup, portals);
    SceneOutlinerRefreshGroup(state, SCENE_OUTLINER_PORTAL,
        portals && portals->portals ? portals->portalcount : 0, setup, portals);
    state->updating = FALSE;
    if (state->redraw)
    {
        state->redraw = FALSE;
        SendMessage(state->tree, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(state->tree, NULL, FALSE);
    }
}

void SceneOutlinerSelect(HWND hwnd, SceneOutlinerKind kind, DWORD index)
{
    SceneOutlinerState *state = SceneOutlinerGetState(hwnd);
    if (!state || kind < SCENE_OUTLINER_NONE || kind > SCENE_OUTLINER_PORTAL) { return; }
    ULONG_PTR key = SceneOutlinerKey(kind, index);
    if (state->selection == key) { return; }
    HTREEITEM item = NULL;
    if (kind != SCENE_OUTLINER_NONE)
    {
        SceneOutlinerGroup *group = &state->groups[kind - 1];
        if (index < group->count) { item = group->rows[index].item; }
    }
    state->selection = key;
    state->updating = TRUE;
    TreeView_SelectItem(state->tree, item);
    if (item)
    {
        TreeView_EnsureVisible(state->tree, item);
        state->groups[kind - 1].expanded = TRUE;
    }
    state->updating = FALSE;
}

static BOOL SceneOutlinerActivate(HWND hwnd, SceneOutlinerState *state, HTREEITEM item, BOOL frame)
{
    TVITEM entry = {0}; entry.mask = TVIF_PARAM; entry.hItem = item;
    if (state->updating || !item || !TreeView_GetItem(state->tree, &entry) || !entry.lParam) { return FALSE; }
    ULONG_PTR key = (ULONG_PTR)entry.lParam;
    SceneOutlinerSelection selection = {(SceneOutlinerKind)(key & 3), (DWORD)(key >> 2), frame};
    /* Copy the request before notifying: edits/selection refreshes may remove rows. */
    SendMessage(GetParent(hwnd), SCENEOUTLINER_WM_SELECT, 0, (LPARAM)&selection);
    return TRUE;
}

static LRESULT CALLBACK SceneOutlinerWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    SceneOutlinerState *state = SceneOutlinerGetState(hwnd);
    switch (message)
    {
    case WM_CREATE:
    {
        static const char *titles[] = {"Characters", "Objects", "Portals"};
        CREATESTRUCT *create = (CREATESTRUCT *)lparam;
        state = calloc(1, sizeof(*state));
        if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->tree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, "",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
            0, 0, 1, 1, hwnd, NULL, create->hInstance, NULL);
        if (!state->tree) { return -1; }
        SendMessage(state->tree, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        for (int i = 0; i < SCENEOUTLINER_GROUPS; i++)
        {
            TVINSERTSTRUCT insert = {0};
            insert.hParent = TVI_ROOT; insert.hInsertAfter = TVI_LAST;
            insert.item.mask = TVIF_TEXT; insert.item.pszText = (char *)titles[i];
            state->groups[i].root = TreeView_InsertItem(state->tree, &insert);
            state->groups[i].expanded = TRUE;
            if (!state->groups[i].root) { return -1; }
        }
        return 0;
    }
    case WM_SIZE:
        if (state) { MoveWindow(state->tree, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE); }
        return 0;
    case WM_NOTIFY:
        if (!state || ((NMHDR *)lparam)->hwndFrom != state->tree) { break; }
        if (((NMHDR *)lparam)->code == TVN_SELCHANGED)
        { SceneOutlinerActivate(hwnd, state, ((NMTREEVIEW *)lparam)->itemNew.hItem, FALSE); }
        else if (((NMHDR *)lparam)->code == TVN_ITEMEXPANDED)
        {
            NMTREEVIEW *event = (NMTREEVIEW *)lparam;
            if (!state->updating)
                for (int i = 0; i < SCENEOUTLINER_GROUPS; i++)
                    if (event->itemNew.hItem == state->groups[i].root)
                    { state->groups[i].expanded = (event->itemNew.state & TVIS_EXPANDED) != 0; }
        }
        else if (((NMHDR *)lparam)->code == NM_DBLCLK)
        {
            TVHITTESTINFO hit = {0}; GetCursorPos(&hit.pt); ScreenToClient(state->tree, &hit.pt);
            TreeView_HitTest(state->tree, &hit);
            if ((hit.flags & TVHT_ONITEM) && SceneOutlinerActivate(hwnd, state, hit.hItem, TRUE)) { return TRUE; }
        }
        return 0;
    case WM_ERASEBKGND:
        return 1; /* The tree fills this entire child window. */
    case WM_NCDESTROY:
        if (state)
        {
            for (int i = 0; i < SCENEOUTLINER_GROUPS; i++) { free(state->groups[i].rows); }
            free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

BOOL SceneOutlinerRegisterClass(HINSTANCE instance)
{
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TREEVIEW_CLASSES};
    if (!InitCommonControlsEx(&controls)) { return FALSE; }
    WNDCLASS wc = {0}; wc.lpfnWndProc = SceneOutlinerWndProc;
    wc.hInstance = instance; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = SCENEOUTLINER_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND SceneOutlinerCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, SCENEOUTLINER_CLASS, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL, instance, NULL);
}
