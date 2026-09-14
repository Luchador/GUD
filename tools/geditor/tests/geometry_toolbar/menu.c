#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "edittool.h"
typedef void *HWND;
typedef unsigned UINT;
typedef intptr_t LPARAM;
typedef struct RECT { int left, top, right, bottom; } RECT;
#define WM_COMMAND 1
#define MF_STRING 0
#define MF_ENABLED 0
#define MF_GRAYED 1
#define MF_CHECKED 2
#define MF_UNCHECKED 0
#define MF_SEPARATOR 4
#define TPM_RETURNCMD 1
#define TPM_NONOTIFY 2
#define TPM_LEFTALIGN 0
#define TPM_TOPALIGN 0
#define TPM_RIGHTBUTTON 4
#define RECENT_PROJECTS_MAX 10
typedef enum ToolToolbarMenu { TOOLTOOLBAR_MENU_VERTEX, TOOLTOOLBAR_MENU_EDGE, TOOLTOOLBAR_MENU_FACE, TOOLTOOLBAR_MENU_COUNT } ToolToolbarMenu;
typedef struct Item { UINT flags, id; const char *label; } Item;
typedef struct Menu { Item items[12]; int count; } Menu;
typedef Menu *HMENU;
static Menu lastmenu;
static HWND g_Viewport = (HWND)1;
static BgDocument g_CurrentBgDocument;
static EditorTool selectedtool = EDITOR_TOOL_FACE_SELECT;
static int facecount, edgecount;
static BOOL flying, transforming, snap, hidden, bridgevalid;
static UINT choose, dispatched;
static unsigned focuscalls;
static EditorTool ViewportGetTool(HWND hwnd) { return selectedtool; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return facecount; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL ViewportGetVertexSnap(HWND hwnd) { return snap; }
static BOOL ViewportHasHiddenBgFaces(HWND hwnd) { return hidden; }
static BOOL ViewportGetSelectedBgEdges(HWND hwnd, BgDocumentEdgeRef *edges, DWORD count)
{ return selectedtool == EDITOR_TOOL_EDGE_SELECT && (int)count == edgecount; }
BOOL BgDocumentCanBridgeEdges(const BgDocument *doc, const BgDocumentEdgeRef edges[2], const char **why)
{ return bridgevalid; }
static HMENU CreatePopupMenu(void) { memset(&lastmenu, 0, sizeof(lastmenu)); return &lastmenu; }
static void DestroyMenu(HMENU menu) { assert(menu == &lastmenu); }
static void AppendMenu(HMENU menu, UINT flags, UINT id, const char *label)
{ assert(menu->count < 12); menu->items[menu->count++] = (Item){flags,id,label}; }
static void GetWindowRect(HWND button, RECT *rect) { *rect = (RECT){180,4,260,36}; }
static UINT TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int reserved, HWND hwnd, void *rect)
{
    assert(x == 180 && y == 36 && (flags & TPM_RETURNCMD));
    if (choose) for (int i = 0; i < menu->count; i++) if (menu->items[i].id == choose)
    { assert(!(menu->items[i].flags & MF_GRAYED)); return choose; }
    assert(!choose); return 0;
}
static void SetFocus(HWND hwnd) { assert(hwnd == g_Viewport); focuscalls++; }
static void SendMessage(HWND hwnd, UINT message, UINT command, LPARAM data)
{ assert(message == WM_COMMAND); dispatched = command; }
#include "menu.inc"

static Item *Find(UINT id)
{
    for (int i = 0; i < lastmenu.count; i++) { if (lastmenu.items[i].id == id) { return &lastmenu.items[i]; } }
    assert(0); return NULL;
}
static BOOL Enabled(UINT id) { return !(Find(id)->flags & MF_GRAYED); }
static void Show(ToolToolbarMenu menu)
{
    EditorTool before = selectedtool; unsigned focus = focuscalls;
    GEditorShowGeometryMenu(NULL, menu, (HWND)2);
    assert(selectedtool == before); /* merely opening a category never switches tools */
    if (!choose) { assert(focus == focuscalls); }
}

typedef struct ToolToolbarState { HWND buttons[5]; HWND menus[3]; } ToolToolbarState;
static RECT positions[8];
static void MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL repaint)
{ int index = (int)(intptr_t)hwnd-1; assert(index >= 0 && index < 8); positions[index] = (RECT){x,y,x+w,y+h}; }
#include "layout.inc"

int main(void)
{
    BgDocumentRoom room = {0};
    Show(TOOLTOOLBAR_MENU_VERTEX); assert(!Enabled(ID_GEOMETRY_PAINT_VERTEX) && !Enabled(ID_GEOMETRY_SNAP_VERTEX));
    Show(TOOLTOOLBAR_MENU_EDGE); assert(!Enabled(ID_GEOMETRY_SPLIT_EDGE) && !Enabled(ID_GEOMETRY_BRIDGE_EDGES));
    g_CurrentBgDocument.rooms = &room;
    selectedtool = EDITOR_TOOL_VERTEX_SELECT; snap = TRUE;
    Show(TOOLTOOLBAR_MENU_VERTEX); assert(Enabled(ID_GEOMETRY_SNAP_VERTEX) && (Find(ID_GEOMETRY_SNAP_VERTEX)->flags & MF_CHECKED));
    selectedtool = EDITOR_TOOL_EDGE_SELECT; edgecount = 1;
    Show(TOOLTOOLBAR_MENU_EDGE); assert(Enabled(ID_GEOMETRY_SPLIT_EDGE) && !Enabled(ID_GEOMETRY_BRIDGE_EDGES));
    edgecount = 2; Show(TOOLTOOLBAR_MENU_EDGE); assert(!Enabled(ID_GEOMETRY_SPLIT_EDGE) && !Enabled(ID_GEOMETRY_BRIDGE_EDGES));
    bridgevalid = TRUE; Show(TOOLTOOLBAR_MENU_EDGE); assert(Enabled(ID_GEOMETRY_BRIDGE_EDGES));
    choose = ID_GEOMETRY_BRIDGE_EDGES; Show(TOOLTOOLBAR_MENU_EDGE); assert(dispatched == choose && focuscalls == 1); choose = 0;
    selectedtool = EDITOR_TOOL_FACE_SELECT; facecount = 3;
    Show(TOOLTOOLBAR_MENU_FACE);
    assert(Enabled(ID_EDIT_FLIP_FACE) && Enabled(ID_GEOMETRY_DISCONNECT_FACE) && Enabled(ID_TOOLS_UV_EDITOR));
    assert(Enabled(ID_VIEW_HIDE_SELECTED) && !Enabled(ID_VIEW_UNHIDE_ALL));
    hidden = TRUE; facecount = 0; Show(TOOLTOOLBAR_MENU_FACE);
    assert(!Enabled(ID_EDIT_FLIP_FACE) && !Enabled(ID_GEOMETRY_DISCONNECT_FACE) && Enabled(ID_VIEW_UNHIDE_ALL));
    for (int mode = 0; mode < 2; mode++)
    {
        flying = mode == 0; transforming = mode == 1;
        selectedtool = EDITOR_TOOL_EDGE_SELECT; Show(TOOLTOOLBAR_MENU_EDGE); assert(!Enabled(ID_GEOMETRY_BRIDGE_EDGES));
        selectedtool = EDITOR_TOOL_FACE_SELECT; facecount = 1; Show(TOOLTOOLBAR_MENU_FACE); assert(!Enabled(ID_EDIT_FLIP_FACE));
    }
    ToolToolbarState toolbar;
    for (int i = 0; i < 5; i++) { toolbar.buttons[i] = (HWND)(intptr_t)(i+1); }
    for (int i = 0; i < 3; i++) { toolbar.menus[i] = (HWND)(intptr_t)(i+6); }
    for (int width = 88; width < 900; width++)
    {
        int height = ToolToolbarLayout(&toolbar, width); assert(height == ToolToolbarGetHeight(width));
        for (int i = 0; i < 8; i++)
        {
            const RECT *a = &positions[i]; assert(a->left >= 4 && a->right <= width-4 && a->top >= 4 && a->bottom <= height-4);
            for (int j = 0; j < i; j++)
            { const RECT *b = &positions[j]; assert(a->left >= b->right || a->top >= b->bottom); }
        }
        if (width >= 440) { assert(height == 40); }
    }
    puts("PASS: selection-aware menus, inactive categories, flight/drag guards, cancellation/focus/command routing and toolbar wrapping across 812 viewport widths.");
    return 0;
}
