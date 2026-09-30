#include "visibilitymenu.h"
#include "theme.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>

#define VISIBILITY_CLASS "GEditorVisibilityMenu"
#define VISIBILITY_WIDTH 280
#define VISIBILITY_HEIGHT 184
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

typedef struct VisibilityMenuState {
    HWND checks[VISIBILITY_CHECK_COUNT];
    HWND opacity, opacitylabel, anchor;
    DWORD flags;
    int percent;
} VisibilityMenuState;

static VisibilityMenuState *VisibilityMenuGetState(HWND menu)
{ return (VisibilityMenuState *)GetWindowLongPtr(menu, GWLP_USERDATA); }

static void VisibilityMenuNotify(HWND menu, VisibilityMenuState *state)
{ SendMessage(GetWindow(menu, GW_OWNER), VISIBILITY_WM_CHANGED, state->flags, 0); }

static void VisibilityMenuUpdateOpacity(HWND menu, VisibilityMenuState *state)
{
    char label[32];
    snprintf(label, sizeof(label), "Opacity: %d%%", state->percent);
    SetWindowText(state->opacitylabel, label);
    SendMessage(GetWindow(menu, GW_OWNER), VISIBILITY_WM_STAN_OPACITY, state->percent, 0);
}

void VisibilityMenuClose(HWND menu, BOOL restorefocus)
{
    VisibilityMenuState *state = VisibilityMenuGetState(menu);
    if (!state || !IsWindowVisible(menu)) { return; }
    ShowWindow(menu, SW_HIDE);
    if (IsWindow(state->anchor)) {
        SendMessage(state->anchor, BM_SETSTATE, FALSE, 0);
        if (restorefocus) {
            SetActiveWindow(GetWindow(menu, GW_OWNER));
            SetFocus(state->anchor);
        }
    }
}

void VisibilityMenuReveal(HWND menu, DWORD flags)
{
    VisibilityMenuState *state = VisibilityMenuGetState(menu);
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
        VisibilityMenuUpdateOpacity(menu, state);
    }
    VisibilityMenuNotify(menu, state);
}

void VisibilityMenuOpen(HWND menu, HWND anchor)
{
    VisibilityMenuState *state = VisibilityMenuGetState(menu);
    RECT button;
    MONITORINFO monitor = {0};
    int x, y;
    if (!state || !GetWindowRect(anchor, &button)) { return; }
    if (IsWindowVisible(menu)) { VisibilityMenuClose(menu, TRUE); return; }
    state->anchor = anchor;
    x = button.right - VISIBILITY_WIDTH;
    y = button.bottom;
    monitor.cbSize = sizeof(monitor);
    if (GetMonitorInfo(MonitorFromRect(&button, MONITOR_DEFAULTTONEAREST), &monitor)) {
        if (y + VISIBILITY_HEIGHT > monitor.rcWork.bottom) { y = button.top - VISIBILITY_HEIGHT; }
        x = max(monitor.rcWork.left, min(x, monitor.rcWork.right - VISIBILITY_WIDTH));
        y = max(monitor.rcWork.top, min(y, monitor.rcWork.bottom - VISIBILITY_HEIGHT));
    }
    SetWindowPos(menu, HWND_TOP, x, y, VISIBILITY_WIDTH, VISIBILITY_HEIGHT, SWP_SHOWWINDOW);
    SetFocus(state->checks[0]);
    SendMessage(anchor, BM_SETSTATE, TRUE, 0);
}

BOOL VisibilityMenuConsumeAnchorClick(HWND menu, LPARAM lparam)
{
    VisibilityMenuState *state = VisibilityMenuGetState(menu);
    POINT point;
    if (!state || !IsWindowVisible(menu) || HIWORD(lparam) != WM_LBUTTONDOWN
        || !GetCursorPos(&point) || WindowFromPoint(point) != state->anchor) { return FALSE; }
    VisibilityMenuClose(menu, FALSE);
    return TRUE;
}

BOOL VisibilityMenuHandleMessage(HWND menu, MSG *message)
{
    if (!menu || !message || !IsWindowVisible(menu)
        || (message->hwnd != menu && !IsChild(menu, message->hwnd))) { return FALSE; }
    if (message->message == WM_KEYDOWN && message->wParam == VK_ESCAPE) {
        VisibilityMenuClose(menu, TRUE);
        return TRUE;
    }
    if (message->message == WM_KEYDOWN && message->wParam == VK_RETURN) {
        int id = GetDlgCtrlID(message->hwnd);
        if (id >= VISIBILITY_CHECK_FIRST && id < VISIBILITY_CHECK_FIRST + (int)VISIBILITY_CHECK_COUNT)
        { SendMessage(message->hwnd, BM_CLICK, 0, 0); }
        return TRUE;
    }
    /* Keep Tab, Space and the slider's arrow keys local to the popup, without
     * allowing editor accelerators to edit the selection behind it. */
    if (!IsDialogMessage(menu, message)) {
        TranslateMessage(message);
        DispatchMessage(message);
    }
    return TRUE;
}

static LRESULT CALLBACK VisibilityMenuWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    VisibilityMenuState *state = VisibilityMenuGetState(hwnd);
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
            int y = 12 + (int)i * 26 + (i >= 3 ? 34 : 0);
            state->checks[i] = CreateWindowEx(0, "BUTTON", g_VisibilityItems[i].label,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                12, y, VISIBILITY_WIDTH - 24, 22, hwnd,
                (HMENU)(INT_PTR)(VISIBILITY_CHECK_FIRST + i), instance, NULL);
            if (!state->checks[i]) { return -1; }
            SendMessage(state->checks[i], WM_SETFONT, (WPARAM)font, FALSE);
            SendMessage(state->checks[i], BM_SETCHECK,
                (state->flags & g_VisibilityItems[i].flag) ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        state->opacitylabel = CreateWindowEx(0, "STATIC", "Opacity: 44%", WS_CHILD | WS_VISIBLE,
            34, 94, 92, 20, hwnd, NULL, instance, NULL);
        state->opacity = CreateWindowEx(0, TRACKBAR_CLASS, "Stan opacity",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | TBS_HORZ | TBS_NOTICKS,
            126, 90, VISIBILITY_WIDTH - 138, 26, hwnd,
            (HMENU)(INT_PTR)VISIBILITY_OPACITY, instance, NULL);
        if (!state->opacitylabel || !state->opacity) { return -1; }
        /* The opacity control follows the Stan checkbox in keyboard order. */
        SetWindowPos(state->opacity, state->checks[2], 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SendMessage(state->opacitylabel, WM_SETFONT, (WPARAM)font, FALSE);
        SendMessage(state->opacity, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
        SendMessage(state->opacity, TBM_SETPOS, TRUE, state->percent);
        return 0;
    }
    case WM_COMMAND:
        if (state && HIWORD(wp) == BN_CLICKED) {
            unsigned int i = LOWORD(wp) - VISIBILITY_CHECK_FIRST;
            if (i < VISIBILITY_CHECK_COUNT) {
                if (SendMessage(state->checks[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
                { state->flags |= g_VisibilityItems[i].flag; }
                else { state->flags &= ~g_VisibilityItems[i].flag; }
                EnableWindow(state->opacity, (state->flags & VISIBILITY_SHOW_STAN) != 0);
                VisibilityMenuNotify(hwnd, state);
                return 0;
            }
        }
        break;
    case WM_HSCROLL:
        if (state && (HWND)lp == state->opacity) {
            state->percent = (int)SendMessage(state->opacity, TBM_GETPOS, 0, 0);
            VisibilityMenuUpdateOpacity(hwnd, state);
            return 0;
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) { VisibilityMenuClose(hwnd, FALSE); }
        return 0;
    case WM_CLOSE:
        VisibilityMenuClose(hwnd, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        RECT rect;
        HDC dc = BeginPaint(hwnd, &paint);
        GetClientRect(hwnd, &rect);
        FillRect(dc, &rect, ThemeBrush(THEME_BACKGROUND));
        FrameRect(dc, &rect, ThemeBrush(THEME_BORDER));
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

BOOL VisibilityMenuRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = VisibilityMenuWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = VISIBILITY_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND VisibilityMenuCreate(HWND owner, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT, VISIBILITY_CLASS, "Visibility",
        WS_POPUP | WS_CLIPCHILDREN, 0, 0, VISIBILITY_WIDTH, VISIBILITY_HEIGHT,
        owner, NULL, instance, NULL);
}
