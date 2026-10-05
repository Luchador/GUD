#include "theme.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include "doorshadowproperties.h"

#define SHADOW_CLASS "GEditorDoorShadowProperties"
enum { TITLE, LINK, PICK, UNLINK, DIRECTION_LABEL, DIRECTION, MATCH_DOOR, DIRECTION_HELP,
    LIGHT, DARK, COLOR_HELP, PREVIEW_LABEL, PREVIEW, HELP, CONTROL_COUNT };
typedef struct ShadowPanel {
    HWND controls[CONTROL_COUNT];
    DWORD index;
    DoorShadowProperties values;
    int scroll, wheel;
    BOOL updating;
    BOOL mixeddirection;
} ShadowPanel;
static ShadowPanel *State(HWND hwnd) { return (ShadowPanel *)GetWindowLongPtr(hwnd, GWLP_USERDATA); }
static void Layout(HWND hwnd, ShadowPanel *s)
{
    RECT client;
    int y = 4, heights[CONTROL_COUNT];
    HDC dc = GetDC(hwnd);
    HFONT old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    GetClientRect(hwnd, &client);
    for (int i = 0; i < CONTROL_COUNT; i++) {
        int height = 26;
        if (i == TITLE || i == LINK || i == DIRECTION_LABEL || i == DIRECTION_HELP
            || i == COLOR_HELP || i == PREVIEW_LABEL || i == HELP) {
            char text[512]; RECT r = {0, 0, max(1, client.right - 8), 0};
            GetWindowText(s->controls[i], text, sizeof(text));
            DrawText(dc, text, -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            height = r.bottom;
        }
        heights[i] = height; y += height + 8;
    }
    SelectObject(dc, old); ReleaseDC(hwnd, dc);
    s->scroll = max(0, min(s->scroll, y - client.bottom));
    SCROLLINFO info = {sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS, 0, y - 1,
        (UINT)max(0, client.bottom), s->scroll, 0};
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
    y = 4 - s->scroll;
    for (int i = 0; i < CONTROL_COUNT; i++) {
        SetWindowPos(s->controls[i], NULL, 4, y, max(1, client.right - 8),
            i == DIRECTION ? 160 : heights[i], SWP_NOZORDER | SWP_NOACTIVATE);
        y += heights[i] + 8;
    }
    InvalidateRect(hwnd, NULL, TRUE);
}
static BOOL Edit(HWND hwnd, ShadowPanel *s, DoorShadowField field, LONG value)
{
    DoorShadowEdit edit = {s->index, field, value};
    return (BOOL)SendMessage(GetAncestor(hwnd, GA_ROOT), DOORSHADOW_WM_CHANGED, 0, (LPARAM)&edit);
}
static void ColorText(HWND control, const char *name, DWORD rgb)
{
    char text[96];
    snprintf(text, sizeof(text), "%s RGB: %u, %u, %u...", name, (unsigned)(rgb >> 16),
        (unsigned)((rgb >> 8) & 255), (unsigned)(rgb & 255));
    SetWindowText(control, text);
}
void DoorShadowPropertiesSetSelection(HWND hwnd, const SetupFile *setup, DWORD index)
{ DoorShadowPropertiesSetSelections(hwnd, setup, &index, 1); }

void DoorShadowPropertiesSetSelections(HWND hwnd, const SetupFile *setup, const DWORD *indices, DWORD count)
{
    if (!count || !indices) return;
    DWORD index = indices[0];
    ShadowPanel *s = State(hwnd); char text[160];
    if (!s || !DoorShadowGet(setup, index, &s->values)) { return; }
    s->updating = TRUE;
    if (s->index != index) { s->scroll = 0; }
    s->index = index; s->mixeddirection = FALSE;
    snprintf(text, sizeof(text), "Door Shadow %lu (room %lu)", (unsigned long)index, (unsigned long)s->values.room);
    if (count > 1) snprintf(text, sizeof(text), "%lu Door Shadows", (unsigned long)count);
    SetWindowText(s->controls[TITLE], text);
    if (s->values.door < 0) { snprintf(text, sizeof(text), "Linked door: None (closed)"); }
    else { snprintf(text, sizeof(text), "Linked door: %ld", (long)s->values.door); }
    SetWindowText(s->controls[LINK], text);
    EnableWindow(s->controls[UNLINK], s->values.door >= 0);
    SendMessage(s->controls[DIRECTION], CB_SETCURSEL, s->values.direction, 0);
    ColorText(s->controls[LIGHT], "Light", s->values.light);
    ColorText(s->controls[DARK], "Dark", s->values.dark);
    int percent = DoorShadowGetPreview(setup, index);
    snprintf(text, sizeof(text), "Preview door opening: %d%%", percent);
    SetWindowText(s->controls[PREVIEW_LABEL], text);
    SendMessage(s->controls[PREVIEW], TBM_SETPOS, TRUE, percent);
    BOOL mixedlink = FALSE, mixedlight = FALSE, mixeddark = FALSE, linked = s->values.door >= 0;
    for (DWORD i = 1; i < count; i++)
    {
        DoorShadowProperties other;
        if (!DoorShadowGet(setup, indices[i], &other)) continue;
        mixedlink |= other.door != s->values.door; linked |= other.door >= 0;
        mixedlight |= other.light != s->values.light; mixeddark |= other.dark != s->values.dark;
        s->mixeddirection |= other.direction != s->values.direction;
    }
    if (mixedlink) SetWindowText(s->controls[LINK], "Linked door: Mixed");
    if (mixedlight) SetWindowText(s->controls[LIGHT], "Light RGB: Mixed...");
    if (mixeddark) SetWindowText(s->controls[DARK], "Dark RGB: Mixed...");
    if (SendMessage(s->controls[DIRECTION], CB_GETCOUNT, 0, 0) > 4)
        SendMessage(s->controls[DIRECTION], CB_DELETESTRING, 4, 0);
    if (s->mixeddirection)
    {
        SendMessage(s->controls[DIRECTION], CB_ADDSTRING, 0, (LPARAM)"Mixed");
        SendMessage(s->controls[DIRECTION], CB_SETCURSEL, 4, 0);
    }
    EnableWindow(s->controls[UNLINK], linked);
    EnableWindow(s->controls[PICK], count == 1);
    DWORD direction;
    const char *why="";
    BOOL horizontal=count==1&&DoorShadowDoorDirection(setup,index,&direction,&why);
    EnableWindow(s->controls[MATCH_DOOR],horizontal);
    SetWindowText(s->controls[DIRECTION_HELP], horizontal
        ? "Match horizontal door selects its X/Z opening direction. You can also choose a direction manually."
        : "Choose +X, -X, +Z or -Z. For horizontal doors, use the door's opening direction. The shadow must have width on that axis.");
    EnableWindow(s->controls[PREVIEW], count == 1);
    SetWindowText(s->controls[HELP], count > 1
        ? "Changes apply to all selected shadows. Select one shadow to pick its door or preview opening."
        : "Preview only. In game, the shadow follows the linked door's actual opening. Click Pick door, then a door in the viewport; Esc cancels.");
    Layout(hwnd, s); s->updating = FALSE;
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ShadowPanel *s = State(hwnd);
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE instance = ((CREATESTRUCT *)lp)->hInstance;
        const char *texts[CONTROL_COUNT] = {"Door Shadow", "Linked door: None", "Pick door", "Unlink door",
            "Light expands toward", "", "Match horizontal door", "Choose +X, -X, +Z or -Z. Horizontal shadows sweep along the door's movement axis.",
            "Light RGB", "Dark RGB", "RGB tints the original texture. Its UVs stay fixed as the boundary moves.",
            "Preview door opening: 0%", "", "Preview only. In game, the shadow follows the linked door's actual opening. Click Pick door, then a door in the viewport; Esc cancels."};
        s = calloc(1, sizeof(*s)); if (!s) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)s);
        s->index = (DWORD)-1;
        for (int i = 0; i < CONTROL_COUNT; i++) {
            BOOL button = i == PICK || i == UNLINK || i == MATCH_DOOR || i == LIGHT || i == DARK;
            const char *cls = button ? "BUTTON" : i == DIRECTION ? "COMBOBOX" : i == PREVIEW ? TRACKBAR_CLASS : "STATIC";
            DWORD style = button ? WS_TABSTOP | BS_PUSHBUTTON : i == DIRECTION ? WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL
                : i == PREVIEW ? WS_TABSTOP | TBS_AUTOTICKS : SS_NOPREFIX;
            s->controls[i] = CreateWindow(cls, texts[i], WS_CHILD | WS_VISIBLE | style,
                0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(200 + i), instance, NULL);
            if (!s->controls[i]) { return -1; }
            SendMessage(s->controls[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
        }
        const char *directions[] = {"+X", "-X", "+Z", "-Z"};
        for (int i = 0; i < 4; i++) { SendMessage(s->controls[DIRECTION], CB_ADDSTRING, 0, (LPARAM)directions[i]); }
        SendMessage(s->controls[PREVIEW], TBM_SETRANGE, FALSE, MAKELONG(0, 100));
        SendMessage(s->controls[PREVIEW], TBM_SETTICFREQ, 25, 0);
        return 0;
    }
    case WM_SIZE: if (s) { Layout(hwnd, s); } return 0;
    case WM_COMMAND:
        if (s && !s->updating) {
            int id = LOWORD(wp) - 200;
            if (id == PICK && HIWORD(wp) == BN_CLICKED) {
                SendMessage(GetAncestor(hwnd, GA_ROOT), DOORSHADOW_WM_PICK, s->index, 0);
            } else if (id == UNLINK && HIWORD(wp) == BN_CLICKED) {
                Edit(hwnd, s, DOOR_SHADOW_EDIT_DOOR, -1);
            } else if (id == MATCH_DOOR && HIWORD(wp) == BN_CLICKED) {
                Edit(hwnd, s, DOOR_SHADOW_EDIT_MATCH_DOOR, 0);
            } else if (id == DIRECTION && HIWORD(wp) == CBN_SELCHANGE) {
                int choice = (int)SendMessage(s->controls[DIRECTION], CB_GETCURSEL, 0, 0);
                if (choice < 0 || choice > 3 || !Edit(hwnd, s, DOOR_SHADOW_EDIT_DIRECTION, choice))
                    SendMessage(s->controls[DIRECTION], CB_SETCURSEL, s->mixeddirection ? 4 : s->values.direction, 0);
            } else if ((id == LIGHT || id == DARK) && HIWORD(wp) == BN_CLICKED) {
                static COLORREF custom[16];
                DWORD rgb = id == LIGHT ? s->values.light : s->values.dark;
                CHOOSECOLOR cc = {0}; cc.lStructSize = sizeof(cc); cc.hwndOwner = hwnd;
                cc.rgbResult = RGB(rgb >> 16, (rgb >> 8) & 255, rgb & 255);
                cc.lpCustColors = custom; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColor(&cc)) { Edit(hwnd, s, id == LIGHT ? DOOR_SHADOW_EDIT_LIGHT : DOOR_SHADOW_EDIT_DARK,
                    (GetRValue(cc.rgbResult) << 16) | (GetGValue(cc.rgbResult) << 8) | GetBValue(cc.rgbResult)); }
            }
        }
        return 0;
    case WM_HSCROLL:
        if (s && !s->updating && (HWND)lp == s->controls[PREVIEW]) {
            int percent = SendMessage(s->controls[PREVIEW], TBM_GETPOS, 0, 0);
            SendMessage(GetAncestor(hwnd, GA_ROOT), DOORSHADOW_WM_PREVIEW, s->index, percent);
        }
        return 0;
    case WM_VSCROLL:
        if (s) {
            SCROLLINFO info = {0}; info.cbSize = sizeof(info); info.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &info);
            switch (LOWORD(wp)) {
            case SB_LINEUP: s->scroll -= 24; break; case SB_LINEDOWN: s->scroll += 24; break;
            case SB_PAGEUP: s->scroll -= info.nPage; break; case SB_PAGEDOWN: s->scroll += info.nPage; break;
            case SB_THUMBTRACK: s->scroll = info.nTrackPos; break;
            }
            Layout(hwnd, s);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (s) { s->wheel += GET_WHEEL_DELTA_WPARAM(wp); s->scroll -= 72 * (s->wheel / WHEEL_DELTA);
            s->wheel %= WHEEL_DELTA; Layout(hwnd, s); }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT); return (LRESULT)ThemeSystemBrush(COLOR_WINDOW);
    case WM_NCDESTROY: free(s); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
BOOL DoorShadowPropertiesRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0}; wc.lpfnWndProc = WndProc; wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = ThemeSystemBrush(COLOR_WINDOW);
    wc.lpszClassName = SHADOW_CLASS; return RegisterClass(&wc) != 0;
}
HWND DoorShadowPropertiesCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, SHADOW_CLASS, NULL, WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN,
        0, 0, 1, 1, parent, NULL, instance, NULL);
}
