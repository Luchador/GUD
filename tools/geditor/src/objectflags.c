/* Scrollable object flags. Enum aliases deliberately expose the same bit. */
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <src/propconstants.h>
#include "objectflags.h"

#define OBJECTFLAGS_CLASS "GEditorObjectFlags"
typedef struct ObjectFlagDefinition {
    unsigned int bank;
    DWORD mask;
    const char *name, *label, *description;
} ObjectFlagDefinition;
static const ObjectFlagDefinition g_ObjectFlags[] = {
#define OBJECT_FLAG(bank, symbol, label, description) {bank, (DWORD)symbol, #symbol, label, description},
#include "objectflagdefs.h"
#undef OBJECT_FLAG
};
#define OBJECTFLAGS_COUNT ((int)(sizeof(g_ObjectFlags) / sizeof(*g_ObjectFlags)))

typedef struct ObjectFlagsState {
    HWND help, headings[2], checks[OBJECTFLAGS_COUNT], tooltip;
    DWORD objectindex, values[2];
    BOOL selected;
    int scroll, wheelremainder;
    char tip[2048];
} ObjectFlagsState;

static ObjectFlagsState *ObjectFlagsGetState(HWND hwnd)
{ return (ObjectFlagsState *)GetWindowLongPtr(hwnd, GWLP_USERDATA); }

static int ObjectFlagsTextHeight(HDC dc, HWND control, int width)
{
    char text[512]; RECT bounds = {0, 0, max(1, width), 0};
    GetWindowText(control, text, sizeof(text));
    DrawText(dc, text, -1, &bounds, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    return max(22, bounds.bottom + 6);
}

static void ObjectFlagsLayout(HWND hwnd, ObjectFlagsState *state)
{
    RECT client, bounds[OBJECTFLAGS_COUNT], headings[2];
    SCROLLINFO info = {0};
    HDC dc = GetDC(hwnd);
    HFONT previous = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    int width, helpheight, y, bank = -1;
    GetClientRect(hwnd, &client); width = max(1, client.right - 8);
    helpheight = ObjectFlagsTextHeight(dc, state->help, width);
    y = helpheight + 12;
    if (state->selected)
    {
        for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
        {
            int height = ObjectFlagsTextHeight(dc, state->checks[i], width - 24);
            if (bank != (int)g_ObjectFlags[i].bank)
            {
                bank = (int)g_ObjectFlags[i].bank;
                y += 8;
                SetRect(&headings[bank], 4, y, width + 4, y + 24);
                y += 28;
            }
            SetRect(&bounds[i], 4, y, width + 4, y + height);
            y += height + 2;
        }
    }
    SelectObject(dc, previous); ReleaseDC(hwnd, dc);
    state->scroll = max(0, min(state->scroll, y - client.bottom));
    info.cbSize = sizeof(info); info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = y - 1; info.nPage = max(0, client.bottom); info.nPos = state->scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
    MoveWindow(state->help, 4, 4 - state->scroll, width, helpheight, TRUE);
    for (int i = 0; i < 2; i++)
    {
        if (state->selected)
        { MoveWindow(state->headings[i], 4, headings[i].top - state->scroll, width, 24, TRUE); }
        ShowWindow(state->headings[i], state->selected ? SW_SHOW : SW_HIDE);
    }
    for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
    {
        if (state->selected)
        { MoveWindow(state->checks[i], 4, bounds[i].top - state->scroll, width, bounds[i].bottom - bounds[i].top, TRUE); }
        ShowWindow(state->checks[i], state->selected ? SW_SHOW : SW_HIDE);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static void ObjectFlagsSyncChecks(ObjectFlagsState *state)
{
    for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
    {
        const ObjectFlagDefinition *flag = &g_ObjectFlags[i];
        SendMessage(state->checks[i], BM_SETCHECK,
            state->selected && (state->values[flag->bank] & flag->mask) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

static void ObjectFlagsDescribe(ObjectFlagsState *state, int index)
{
    const ObjectFlagDefinition *flag = &g_ObjectFlags[index];
    BOOL aliases = FALSE;
    snprintf(state->tip, sizeof(state->tip), "%s\r\nBit: 0x%08lX\r\n%s", flag->name,
        (unsigned long)flag->mask, flag->description[0] ? flag->description : "No description in propconstants.h.");
    for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
    {
        size_t used;
        if (i == index || g_ObjectFlags[i].bank != flag->bank || g_ObjectFlags[i].mask != flag->mask) { continue; }
        used = strlen(state->tip);
        snprintf(state->tip + used, sizeof(state->tip) - used, "%s%s",
            aliases ? ", " : "\r\nSame bit as: ", g_ObjectFlags[i].name);
        aliases = TRUE;
    }
}

static LRESULT CALLBACK ObjectFlagsWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ObjectFlagsState *state = ObjectFlagsGetState(hwnd);
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
        state = calloc(1, sizeof(*state)); if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->help = CreateWindowEx(0, "STATIC", "Select a setup object to edit its flags.\r\nCharacters, geometry, pads and camera markers use different properties.",
            WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 0, 0, 1, 1, hwnd, NULL, cs->hInstance, NULL);
        if (!state->help) { return -1; }
        SendMessage(state->help, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        for (int i = 0; i < 2; i++)
        {
            state->headings[i] = CreateWindowEx(0, "STATIC", "", WS_CHILD | SS_NOPREFIX,
                0, 0, 1, 1, hwnd, NULL, cs->hInstance, NULL);
            if (!state->headings[i]) { return -1; }
            SendMessage(state->headings[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        }
        state->tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, NULL, cs->hInstance, NULL);
        if (!state->tooltip) { return -1; }
        SendMessage(state->tooltip, TTM_SETMAXTIPWIDTH, 0, 430);
        SendMessage(state->tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 20000);
        for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
        {
            TOOLINFO tool = {0};
            state->checks[i] = CreateWindowEx(0, "BUTTON", g_ObjectFlags[i].label,
                WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX | BS_MULTILINE | BS_NOTIFY,
                0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(i + 1), cs->hInstance, NULL);
            if (!state->checks[i]) { return -1; }
            SendMessage(state->checks[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            tool.cbSize = sizeof(tool); tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            tool.hwnd = hwnd; tool.uId = (UINT_PTR)state->checks[i]; tool.lpszText = LPSTR_TEXTCALLBACK;
            SendMessage(state->tooltip, TTM_ADDTOOL, 0, (LPARAM)&tool);
        }
        return 0;
    }
    case WM_SIZE:
        if (state) { ObjectFlagsLayout(hwnd, state); } return 0;
    case WM_COMMAND:
        if (state && state->selected)
        {
            int index = LOWORD(wparam) - 1;
            if (index < 0 || index >= OBJECTFLAGS_COUNT || (HWND)lparam != state->checks[index]) { return 0; }
            if (HIWORD(wparam) == BN_SETFOCUS)
            {
                RECT rect, client;
                GetWindowRect((HWND)lparam, &rect); MapWindowPoints(NULL, hwnd, (POINT *)&rect, 2);
                GetClientRect(hwnd, &client);
                if (rect.top < 0) { state->scroll += rect.top; }
                else if (rect.bottom > client.bottom) { state->scroll += rect.bottom - client.bottom; }
                ObjectFlagsLayout(hwnd, state);
            }
            else if (HIWORD(wparam) == BN_CLICKED)
            {
                ObjectFlagEdit edit = {state->objectindex, g_ObjectFlags[index].bank, g_ObjectFlags[index].mask,
                    SendMessage(state->checks[index], BM_GETCHECK, 0, 0) == BST_CHECKED};
                /* The frame refreshes these copied values after success or rollback. */
                SendMessage(GetParent(hwnd), OBJECTFLAGS_WM_CHANGED, 0, (LPARAM)&edit);
                ObjectFlagsSyncChecks(state);
            }
        }
        return 0;
    case WM_NOTIFY:
        if (state && ((NMHDR *)lparam)->hwndFrom == state->tooltip && ((NMHDR *)lparam)->code == TTN_GETDISPINFO)
        {
            NMTTDISPINFO *tip = (NMTTDISPINFO *)lparam;
            for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
            {
                if ((HWND)tip->hdr.idFrom != state->checks[i]) { continue; }
                ObjectFlagsDescribe(state, i); tip->lpszText = state->tip; break;
            }
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        if (state)
        {
            state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
            state->scroll -= 72 * (state->wheelremainder / WHEEL_DELTA);
            state->wheelremainder %= WHEEL_DELTA; ObjectFlagsLayout(hwnd, state);
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
            ObjectFlagsLayout(hwnd, state);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wparam, GetSysColor(COLOR_WINDOW)); return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_DESTROY:
        if (state && state->tooltip) { DestroyWindow(state->tooltip); state->tooltip = NULL; }
        return 0;
    case WM_NCDESTROY:
        free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL ObjectFlagsRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0}; wc.lpfnWndProc = ObjectFlagsWndProc; wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = OBJECTFLAGS_CLASS; return RegisterClass(&wc) != 0;
}

HWND ObjectFlagsCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, OBJECTFLAGS_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL, instance, NULL);
}

void ObjectFlagsSetSelection(HWND panel, const SetupObject *object, DWORD index)
{
    ObjectFlagsState *state = ObjectFlagsGetState(panel);
    char heading[64];
    if (!state) { return; }
    if (state->selected != (object != NULL) || (object && state->objectindex != index))
    { state->scroll = 0; state->wheelremainder = 0; SendMessage(state->tooltip, TTM_POP, 0, 0); }
    state->selected = object != NULL; state->objectindex = index;
    state->values[0] = object ? object->flags : 0;
    state->values[1] = object ? object->flags2 : 0;
    SetWindowText(state->help, object
        ? "Meanings depend on object type. Aliases share a bit. Hover for details.\r\nFlags can hide an object; Undo restores it."
        : "Select a setup object to edit its flags.\r\nCharacters, geometry, pads and camera markers use different properties.");
    for (int bank = 0; bank < 2; bank++)
    {
        snprintf(heading, sizeof(heading), "%s: 0x%08lX", bank ? "PROPFLAG2" : "PROPFLAG", (unsigned long)state->values[bank]);
        SetWindowText(state->headings[bank], heading);
    }
    ObjectFlagsSyncChecks(state);
    ObjectFlagsLayout(panel, state);
}
