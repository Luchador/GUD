/* Portal connections are edited in native table order, including shared polygons. */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include "portalproperties.h"

#define PORTALPROPERTIES_CLASS "GEditorPortalProperties"
enum { PORTAL_SUMMARY, PORTAL_ROOM1_LABEL, PORTAL_ROOM1, PORTAL_ROOM2_LABEL,
       PORTAL_ROOM2, PORTAL_APPLY, PORTAL_HELP, PORTAL_CONTROL_COUNT };
typedef struct PortalPropertiesState {
    HWND controls[PORTAL_CONTROL_COUNT];
    DWORD portal;
    int scroll, wheelremainder;
} PortalPropertiesState;

static PortalPropertiesState *PortalPropertiesGetState(HWND hwnd)
{ return (PortalPropertiesState *)GetWindowLongPtr(hwnd, GWLP_USERDATA); }

static void PortalPropertiesLayout(HWND hwnd, PortalPropertiesState *state)
{
    RECT client, bounds[PORTAL_CONTROL_COUNT];
    SCROLLINFO info = {0};
    int width, y = 4, maximum;
    GetClientRect(hwnd, &client); width = max(1, client.right - 8);
    for (int i = 0; i < PORTAL_CONTROL_COUNT; i++)
    {
        int height = 24;
        if (i != PORTAL_ROOM1 && i != PORTAL_ROOM2 && i != PORTAL_APPLY)
        {
            char text[512]; RECT rect = {0, 0, width, 0};
            HDC dc = GetDC(state->controls[i]);
            HFONT font = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
            GetWindowText(state->controls[i], text, sizeof(text));
            DrawText(dc, text, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(dc, font); ReleaseDC(state->controls[i], dc); height = rect.bottom;
        }
        SetRect(&bounds[i], 4, y, width + 4, y + height);
        y += height + 8;
    }
    maximum = max(0, y - client.bottom);
    state->scroll = max(0, min(state->scroll, maximum));
    info.cbSize = sizeof(info); info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = y - 1; info.nPage = client.bottom; info.nPos = state->scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
    for (int i = 0; i < PORTAL_CONTROL_COUNT; i++)
    {
        RECT *r = &bounds[i];
        MoveWindow(state->controls[i], r->left, r->top - state->scroll, r->right - r->left,
            i == PORTAL_ROOM1 || i == PORTAL_ROOM2 ? 240 : r->bottom - r->top, TRUE);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static void PortalPropertiesRevealControl(HWND hwnd, PortalPropertiesState *state, HWND control)
{
    RECT rect, client;
    GetWindowRect(control, &rect); MapWindowPoints(NULL, hwnd, (POINT *)&rect, 2);
    GetClientRect(hwnd, &client);
    if (rect.top < 0) { state->scroll += rect.top; }
    else if (rect.bottom > client.bottom) { state->scroll += rect.bottom - client.bottom; }
    PortalPropertiesLayout(hwnd, state);
}

static void PortalPropertiesApply(HWND hwnd, PortalPropertiesState *state)
{
    PortalPropertiesEdit edit = {state->portal, 0, 0};
    int a = (int)SendMessage(state->controls[PORTAL_ROOM1], CB_GETCURSEL, 0, 0);
    int b = (int)SendMessage(state->controls[PORTAL_ROOM2], CB_GETCURSEL, 0, 0);
    if (a != CB_ERR) { edit.room1 = (DWORD)SendMessage(state->controls[PORTAL_ROOM1], CB_GETITEMDATA, a, 0); }
    if (b != CB_ERR) { edit.room2 = (DWORD)SendMessage(state->controls[PORTAL_ROOM2], CB_GETITEMDATA, b, 0); }
    SendMessage(GetParent(hwnd), PORTALPROPERTIES_WM_CHANGED, 0, (LPARAM)&edit);
}

static LRESULT CALLBACK PortalPropertiesWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    PortalPropertiesState *state = PortalPropertiesGetState(hwnd);
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
        static const char *labels[] = {"", "Room 1", "", "Room 2", "", "Apply connections",
            "Choose two different rooms, then apply.\r\nClick overlapping portals again to cycle their connections.\r\nCtrl-click or Escape clears selection."};
        state = calloc(1, sizeof(*state)); if (!state) { return -1; }
        state->portal = BG_PORTAL_INDEX_NONE;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        for (int i = 0; i < PORTAL_CONTROL_COUNT; i++)
        {
            BOOL combo = i == PORTAL_ROOM1 || i == PORTAL_ROOM2, button = i == PORTAL_APPLY;
            DWORD style = combo ? WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST
                : button ? WS_TABSTOP | BS_PUSHBUTTON | BS_NOTIFY : SS_NOPREFIX;
            state->controls[i] = CreateWindowEx(0, combo ? "COMBOBOX" : button ? "BUTTON" : "STATIC", labels[i],
                WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(i + 1), cs->hInstance, NULL);
            if (!state->controls[i]) { return -1; }
            SendMessage(state->controls[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        }
        return 0;
    }
    case WM_SIZE:
        if (state) { PortalPropertiesLayout(hwnd, state); } return 0;
    case WM_COMMAND:
        if (state)
        {
            int id = LOWORD(wparam) - 1, event = HIWORD(wparam);
            if (((id == PORTAL_ROOM1 || id == PORTAL_ROOM2) && event == CBN_SETFOCUS)
                || (id == PORTAL_APPLY && event == BN_SETFOCUS))
            { PortalPropertiesRevealControl(hwnd, state, (HWND)lparam); }
            if (id == PORTAL_APPLY && event == BN_CLICKED) { PortalPropertiesApply(hwnd, state); }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state)
        {
            state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
            state->scroll -= 48 * (state->wheelremainder / WHEEL_DELTA);
            state->wheelremainder %= WHEEL_DELTA; PortalPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_VSCROLL:
        if (state)
        {
            SCROLLINFO info = {0}; info.cbSize = sizeof(info); info.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &info);
            switch (LOWORD(wparam))
            {
            case SB_LINEUP: state->scroll -= 24; break;
            case SB_LINEDOWN: state->scroll += 24; break;
            case SB_PAGEUP: state->scroll -= info.nPage; break;
            case SB_PAGEDOWN: state->scroll += info.nPage; break;
            case SB_THUMBTRACK: state->scroll = info.nTrackPos; break;
            case SB_TOP: state->scroll = 0; break;
            case SB_BOTTOM: state->scroll = info.nMax; break;
            }
            PortalPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wparam, GetSysColor(COLOR_WINDOW)); return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_NCDESTROY:
        free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL PortalPropertiesRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0}; wc.lpfnWndProc = PortalPropertiesWndProc; wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = PORTALPROPERTIES_CLASS; return RegisterClass(&wc) != 0;
}
HWND PortalPropertiesCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, PORTALPROPERTIES_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL, instance, NULL);
}
BOOL PortalPropertiesSetSelection(HWND panel, const BgDocument *document, DWORD index)
{
    PortalPropertiesState *state = PortalPropertiesGetState(panel);
    const BgPortal *portal; DWORD shared = 0; char text[512]; double margin;
    if (!state || !document || !document->portals.portals || index >= document->portals.portalcount) { return FALSE; }
    portal = &document->portals.portals[index];
    if (state->portal != index) { state->scroll = 0; }
    state->portal = index;
    for (DWORD i = 0; i < document->portals.portalcount; i++)
    { shared += document->portals.portals[i].geometryoffset == portal->geometryoffset; }
    /* bgGetPortalMargin is in BG units; the viewport uses world units. */
    margin = BgPortalGetMargin(portal) / document->levelscale;
    snprintf(text, sizeof(text), "Portal: %lu\r\nPoints: %u\r\nExtra margin: %s (%.4g world units)\r\nPolygon used by: %lu connection(s)",
        (unsigned long)index, portal->pointcount, margin > 0 ? "Yes" : "No", margin, (unsigned long)shared);
    SetWindowText(state->controls[PORTAL_SUMMARY], text);
    for (int side = 0; side < 2; side++)
    {
        HWND control = state->controls[side ? PORTAL_ROOM2 : PORTAL_ROOM1];
        DWORD room = side ? portal->connectedroom2 : portal->connectedroom1;
        int choice = -1;
        SendMessage(control, CB_RESETCONTENT, 0, 0);
        for (DWORD i = 1; i <= document->roomcount && i <= 255; i++)
        {
            int entry;
            snprintf(text, sizeof(text), "%lu", (unsigned long)i);
            entry = (int)SendMessage(control, CB_ADDSTRING, 0, (LPARAM)text);
            if (entry >= 0) { SendMessage(control, CB_SETITEMDATA, entry, i); if (room == i) { choice = entry; } }
        }
        if (choice < 0)
        {
            snprintf(text, sizeof(text), "%lu (invalid room)", (unsigned long)room);
            choice = (int)SendMessage(control, CB_ADDSTRING, 0, (LPARAM)text);
            if (choice >= 0) { SendMessage(control, CB_SETITEMDATA, choice, room); }
        }
        SendMessage(control, CB_SETCURSEL, choice, 0);
    }
    PortalPropertiesLayout(panel, state); return TRUE;
}
BOOL PortalPropertiesHandleMessage(HWND panel, MSG *message)
{
    PortalPropertiesState *state = PortalPropertiesGetState(panel); HWND focus = GetFocus();
    if (!state || !IsChild(panel, focus) || message->message != WM_KEYDOWN || message->wParam != VK_RETURN) { return FALSE; }
    if ((focus == state->controls[PORTAL_ROOM1] || focus == state->controls[PORTAL_ROOM2])
        && SendMessage(focus, CB_GETDROPPEDSTATE, 0, 0)) { return FALSE; }
    PortalPropertiesApply(panel, state); return TRUE;
}
