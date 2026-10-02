#include "visibilitypanel.h"
#include "theme.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>

#define VISIBILITY_CLASS "GEditorVisibilityPanel"
#define VISIBILITY_CHECK_FIRST 100
#define VISIBILITY_OPACITY 105

static const struct {
    const char *label;
    DWORD flag;
} g_VisibilityItems[] = {
    {"Background Primary", VISIBILITY_SHOW_BG_PRIMARY},
    {"Background Secondary", VISIBILITY_SHOW_BG_SECONDARY},
    {"Stan Geometry", VISIBILITY_SHOW_STAN},
    {"Portals", VISIBILITY_SHOW_PORTALS},
    {"Objects / Characters", VISIBILITY_SHOW_OBJECTS}
};
#define VISIBILITY_CHECK_COUNT (sizeof(g_VisibilityItems) / sizeof(g_VisibilityItems[0]))

typedef struct VisibilityPanelState {
    HWND checks[VISIBILITY_CHECK_COUNT];
    HWND opacity, opacitylabel;
    DWORD flags;
    int percent;
} VisibilityPanelState;

static VisibilityPanelState *VisibilityPanelGetState(HWND panel)
{ return (VisibilityPanelState *)GetWindowLongPtr(panel, GWLP_USERDATA); }

static void VisibilityPanelNotify(HWND panel, VisibilityPanelState *state)
{ SendMessage(GetParent(panel), VISIBILITY_WM_CHANGED, state->flags, 0); }

static void VisibilityPanelUpdateOpacity(HWND panel, VisibilityPanelState *state)
{
    char label[32];
    snprintf(label, sizeof(label), "Opacity: %d%%", state->percent);
    SetWindowText(state->opacitylabel, label);
    SendMessage(GetParent(panel), VISIBILITY_WM_STAN_OPACITY, state->percent, 0);
}

void VisibilityPanelReveal(HWND panel, DWORD flags)
{
    VisibilityPanelState *state = VisibilityPanelGetState(panel);
    if (!state) { return; }
    state->flags |= flags;
    for (unsigned int i = 0; i < VISIBILITY_CHECK_COUNT; i++) {
        SendMessage(state->checks[i], BM_SETCHECK,
            (state->flags & g_VisibilityItems[i].flag) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    EnableWindow(state->opacity, (state->flags & VISIBILITY_SHOW_STAN) != 0);
    /* Revealing a pasted stan must make it visible even after opacity was zero. */
    if ((flags & VISIBILITY_SHOW_STAN) && state->percent == 0) {
        state->percent = 44;
        SendMessage(state->opacity, TBM_SETPOS, TRUE, state->percent);
        VisibilityPanelUpdateOpacity(panel, state);
    }
    VisibilityPanelNotify(panel, state);
}

/* Keep native control keys local while letting Tab traverse the whole right column. */
BOOL VisibilityPanelHandleMessage(HWND panel, MSG *message)
{
    VisibilityPanelState *state = VisibilityPanelGetState(panel);
    if (!state || !message || message->message != WM_KEYDOWN
        || !IsChild(panel, message->hwnd)) { return FALSE; }
    if (message->wParam == VK_RETURN) {
        for (unsigned int i = 0; i < VISIBILITY_CHECK_COUNT; i++) {
            if (message->hwnd == state->checks[i]) {
                SendMessage(message->hwnd, BM_CLICK, 0, 0);
                return TRUE;
            }
        }
    }
    if (message->wParam == VK_SPACE || (message->hwnd == state->opacity
        && (message->wParam == VK_LEFT || message->wParam == VK_RIGHT
            || message->wParam == VK_UP || message->wParam == VK_DOWN
            || message->wParam == VK_HOME || message->wParam == VK_END
            || message->wParam == VK_PRIOR || message->wParam == VK_NEXT))) {
        TranslateMessage(message);
        DispatchMessage(message);
        return TRUE;
    }
    return FALSE;
}

static void VisibilityPanelLayout(HWND panel, VisibilityPanelState *state)
{
    RECT client;
    GetClientRect(panel, &client);
    for (unsigned int i = 0; i < VISIBILITY_CHECK_COUNT; i++) {
        int y = 32 + (int)i * 26 + (i >= 3 ? 34 : 0);
        MoveWindow(state->checks[i], 12, y, max(1, client.right - 24), 22, TRUE);
    }
    MoveWindow(state->opacitylabel, 34, 114, 92, 20, TRUE);
    MoveWindow(state->opacity, 126, 110, max(1, client.right - 138), 26, TRUE);
    InvalidateRect(panel, NULL, FALSE);
}

static LRESULT CALLBACK VisibilityPanelWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    VisibilityPanelState *state = VisibilityPanelGetState(hwnd);
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE instance = ((CREATESTRUCT *)lp)->hInstance;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        state = calloc(1, sizeof(*state));
        if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->flags = VISIBILITY_SHOW_BG_PRIMARY | VISIBILITY_SHOW_BG_SECONDARY | VISIBILITY_SHOW_OBJECTS;
        state->percent = 44;
        for (unsigned int i = 0; i < VISIBILITY_CHECK_COUNT; i++) {
            state->checks[i] = CreateWindowEx(0, "BUTTON", g_VisibilityItems[i].label,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                0, 0, 1, 1, hwnd,
                (HMENU)(INT_PTR)(VISIBILITY_CHECK_FIRST + i), instance, NULL);
            if (!state->checks[i]) { return -1; }
            SendMessage(state->checks[i], WM_SETFONT, (WPARAM)font, FALSE);
            SendMessage(state->checks[i], BM_SETCHECK,
                (state->flags & g_VisibilityItems[i].flag) ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        state->opacitylabel = CreateWindowEx(0, "STATIC", "Opacity: 44%", WS_CHILD | WS_VISIBLE,
            0, 0, 1, 1, hwnd, NULL, instance, NULL);
        state->opacity = CreateWindowEx(0, TRACKBAR_CLASS, "Stan opacity",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | TBS_HORZ | TBS_NOTICKS,
            0, 0, 1, 1, hwnd,
            (HMENU)(INT_PTR)VISIBILITY_OPACITY, instance, NULL);
        if (!state->opacitylabel || !state->opacity) { return -1; }
        /* The opacity control follows the Stan checkbox in keyboard order. */
        SetWindowPos(state->opacity, state->checks[2], 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SendMessage(state->opacitylabel, WM_SETFONT, (WPARAM)font, FALSE);
        SendMessage(state->opacity, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
        SendMessage(state->opacity, TBM_SETPOS, TRUE, state->percent);
        VisibilityPanelLayout(hwnd, state);
        return 0;
    }
    case WM_SIZE:
        if (state) { VisibilityPanelLayout(hwnd, state); }
        return 0;
    case WM_COMMAND:
        if (state && HIWORD(wp) == BN_CLICKED) {
            unsigned int i = LOWORD(wp) - VISIBILITY_CHECK_FIRST;
            if (i < VISIBILITY_CHECK_COUNT) {
                if (SendMessage(state->checks[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
                { state->flags |= g_VisibilityItems[i].flag; }
                else { state->flags &= ~g_VisibilityItems[i].flag; }
                EnableWindow(state->opacity, (state->flags & VISIBILITY_SHOW_STAN) != 0);
                VisibilityPanelNotify(hwnd, state);
                return 0;
            }
        }
        break;
    case WM_HSCROLL:
        if (state && (HWND)lp == state->opacity) {
            state->percent = (int)SendMessage(state->opacity, TBM_GETPOS, 0, 0);
            VisibilityPanelUpdateOpacity(hwnd, state);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        RECT rect;
        HDC dc = BeginPaint(hwnd, &paint);
        GetClientRect(hwnd, &rect);
        FillRect(dc, &rect, ThemeBrush(THEME_BACKGROUND));
        RECT title = {12, 8, max(12, rect.right - 12), 28};
        HFONT oldfont = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, ThemeColor(THEME_TEXT));
        DrawText(dc, "Visibility", -1, &title, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
        SelectObject(dc, oldfont);
        rect.top = max(0, rect.bottom - 1);
        FillRect(dc, &rect, ThemeBrush(THEME_BORDER));
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_NCDESTROY:
        free(state);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

BOOL VisibilityPanelRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = VisibilityPanelWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = VISIBILITY_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND VisibilityPanelCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, VISIBILITY_CLASS, "Visibility",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 1, VISIBILITY_PANEL_HEIGHT,
        parent, NULL, instance, NULL);
}
