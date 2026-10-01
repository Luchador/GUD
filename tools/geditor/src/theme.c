#include "theme.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

/* Custom dark mode colors. Light mode uses the Windows system palette. */
static const COLORREF g_DarkPalette[THEME_COLOR_COUNT] = {
    [THEME_BACKGROUND]       = RGB( 36,  36,  36), /* Empty window/dialog areas */
    [THEME_PANEL]            = RGB( 64,  64,  64), /* Headers and tool panels */
    [THEME_INPUT]            = RGB( 29,  29,  29), /* Text fields and lists */
    [THEME_BUTTON]           = RGB( 58,  58,  58),
    [THEME_HOVER]            = RGB( 73,  73,  73),
    [THEME_PRESSED]          = RGB( 62,  85,  183),
    [THEME_BORDER]           = RGB( 82,  82,  82),
    [THEME_TEXT]             = RGB(222, 222, 222),
    [THEME_MUTED]            = RGB(153, 153, 153), /* Disabled/help text */
    [THEME_SELECTION]        = RGB( 58,  91, 138),
    [THEME_SELECTION_TEXT]   = RGB(255, 255, 255),
    [THEME_MENU]             = RGB( 40,  40,  40),
    [THEME_MENU_BORDER]      = RGB( 90,  90,  90), /* One-pixel line below menu bars */
    [THEME_TITLE]            = RGB( 30,  30,  30), /* Windows 11 title bar */
    [THEME_TITLE_TEXT]       = RGB(222, 222, 222),
    [THEME_ERROR_BACKGROUND] = RGB( 93,  42,  42)
};

static EditorTheme g_Theme = EDITOR_THEME_DARK;
static HBRUSH g_Brushes[THEME_COLOR_COUNT];
static HHOOK g_WindowHook;
static HFONT g_MenuFont;
static DWORD g_Thread;
static BOOL g_Applying;

typedef enum ThemeWindowKind {
    THEME_WINDOW, THEME_DIALOG, THEME_NATIVE, THEME_BUTTON_CONTROL,
    THEME_COMBO_CONTROL, THEME_TAB_CONTROL, THEME_TRACKBAR_CONTROL, THEME_HEADER_CONTROL
} ThemeWindowKind;
typedef struct ThemeWindow {
    ThemeWindowKind kind;
    BOOL hot;
} ThemeWindow;

typedef struct ThemeMenuItem {
    struct ThemeMenuItem *next;
    HMENU menu;
    HWND owner;
    ULONG_PTR originaldata;
    UINT originaltype;
    BOOL bar;
    WCHAR text[];
} ThemeMenuItem;
static ThemeMenuItem *g_MenuItems;
static LRESULT CALLBACK ThemeWindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
static void ThemeApplyWindow(HWND hwnd);

static int ThemeLightIndex(ThemeColorRole role)
{
    switch (role) {
    case THEME_INPUT: return COLOR_WINDOW;
    case THEME_TEXT: return COLOR_WINDOWTEXT;
    case THEME_MUTED: return COLOR_GRAYTEXT;
    case THEME_SELECTION: case THEME_PRESSED: return COLOR_HIGHLIGHT;
    case THEME_SELECTION_TEXT: return COLOR_HIGHLIGHTTEXT;
    case THEME_BORDER: case THEME_MENU_BORDER: return COLOR_BTNSHADOW;
    case THEME_MENU: return COLOR_MENU;
    case THEME_TITLE: return COLOR_ACTIVECAPTION;
    case THEME_TITLE_TEXT: return COLOR_CAPTIONTEXT;
    default: return COLOR_BTNFACE;
    }
}

COLORREF ThemeColor(ThemeColorRole role)
{
    if ((unsigned int)role >= THEME_COLOR_COUNT) { role = THEME_BACKGROUND; }
    return g_Theme == EDITOR_THEME_DARK ? g_DarkPalette[role]
        : role == THEME_ERROR_BACKGROUND ? RGB(255,220,220) : GetSysColor(ThemeLightIndex(role));
}

HBRUSH ThemeBrush(ThemeColorRole role)
{
    if ((unsigned int)role >= THEME_COLOR_COUNT) { role = THEME_BACKGROUND; }
    if (g_Theme == EDITOR_THEME_LIGHT) { return GetSysColorBrush(ThemeLightIndex(role)); }
    if (!g_Brushes[role]) { g_Brushes[role] = CreateSolidBrush(g_DarkPalette[role]); }
    return g_Brushes[role];
}

static ThemeColorRole ThemeSystemRole(int index)
{
    switch (index) {
    case COLOR_WINDOW: case COLOR_APPWORKSPACE: return THEME_BACKGROUND;
    case COLOR_BTNFACE: case COLOR_SCROLLBAR: return THEME_PANEL;
    case COLOR_WINDOWTEXT: case COLOR_BTNTEXT: case COLOR_MENUTEXT: case COLOR_INFOTEXT: return THEME_TEXT;
    case COLOR_GRAYTEXT: return THEME_MUTED;
    case COLOR_HIGHLIGHT: case COLOR_MENUHILIGHT: return THEME_SELECTION;
    case COLOR_HIGHLIGHTTEXT: return THEME_SELECTION_TEXT;
    case COLOR_BTNHIGHLIGHT: case COLOR_3DLIGHT: return THEME_HOVER;
    case COLOR_BTNSHADOW: case COLOR_3DDKSHADOW: case COLOR_WINDOWFRAME: return THEME_BORDER;
    case COLOR_MENU: case COLOR_MENUBAR: case COLOR_INFOBK: return THEME_MENU;
    default: return THEME_BACKGROUND;
    }
}

COLORREF ThemeSystemColor(int index)
{ return g_Theme == EDITOR_THEME_LIGHT ? GetSysColor(index) : ThemeColor(ThemeSystemRole(index)); }
HBRUSH ThemeSystemBrush(int index)
{ return g_Theme == EDITOR_THEME_LIGHT ? GetSysColorBrush(index) : ThemeBrush(ThemeSystemRole(index)); }

static int ThemePixels(HWND hwnd, int value)
{
    typedef UINT (WINAPI *DpiFunction)(HWND);
    DpiFunction getdpi = (DpiFunction)(void *)GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow");
    UINT dpi = getdpi ? getdpi(hwnd) : 96;
    return MulDiv(value, dpi ? dpi : 96, 96);
}

static void ThemeFill(HDC dc, const RECT *rect, ThemeColorRole role)
{ FillRect(dc, rect, ThemeBrush(role)); }
static void ThemeFrame(HDC dc, const RECT *rect, ThemeColorRole role)
{ FrameRect(dc, rect, ThemeBrush(role)); }
static void ThemeLine(HDC dc, int x1, int y1, int x2, int y2, ThemeColorRole role, int width)
{
    HPEN pen = CreatePen(PS_SOLID, width, ThemeColor(role));
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL); LineTo(dc, x2, y2);
    SelectObject(dc, old); DeleteObject(pen);
}
static HFONT ThemeSelectFont(HWND hwnd, HDC dc)
{
    HFONT font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    return (HFONT)SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
}

static void ThemeDrawMenuBorder(HWND hwnd)
{
    RECT window, client, border;
    POINT origin = {0, 0};
    HDC dc;
    if ((GetWindowLongPtr(hwnd, GWL_STYLE) & (WS_CHILD | WS_MINIMIZE)) || !GetMenu(hwnd)) { return; }
    if (!GetWindowRect(hwnd, &window) || !GetClientRect(hwnd, &client)
        || !ClientToScreen(hwnd, &origin) || IsRectEmpty(&client)) { return; }
    /* Windows draws this strip outside the owner-drawn menu items. Use the
     * actual client edge so resizing, menu wrapping and DPI changes need no
     * assumed caption/menu heights. Keep it one pixel and outside the client. */
    OffsetRect(&client, origin.x - window.left, origin.y - window.top);
    SetRect(&border, client.left, client.top - 1, client.right, client.top);
    dc = GetWindowDC(hwnd);
    if (dc) {
        ThemeFill(dc, &border, THEME_MENU_BORDER);
        ReleaseDC(hwnd, dc);
    }
}

/* Only our menu records are interpreted as pointers. Existing owner-drawn
 * items, item IDs, Unicode labels, application item data and commands survive. */
static ThemeMenuItem *ThemeFindMenuItem(ULONG_PTR data)
{
    ThemeMenuItem *item;
    for (item = g_MenuItems; item; item = item->next)
        if ((ULONG_PTR)item == data) { return item; }
    return NULL;
}

static void ThemeRestoreMenu(HMENU menu)
{
    ThemeMenuItem **link = &g_MenuItems;
    while (*link) {
        ThemeMenuItem *item = *link;
        if (menu && item->menu != menu) { link = &item->next; continue; }
        if (IsMenu(item->menu)) {
            int count = GetMenuItemCount(item->menu);
            for (int i = 0; i < count; i++) {
                MENUITEMINFOW info = {0}; info.cbSize = sizeof(info); info.fMask = MIIM_DATA | MIIM_FTYPE;
                if (GetMenuItemInfoW(item->menu, i, TRUE, &info) && info.dwItemData == (ULONG_PTR)item) {
                    info.dwItemData = item->originaldata;
                    info.fType = (info.fType & ~MFT_OWNERDRAW) | (item->originaltype & MFT_OWNERDRAW);
                    SetMenuItemInfoW(item->menu, i, TRUE, &info);
                    break;
                }
            }
        }
        *link = item->next; free(item);
    }
}

static void ThemePrepareMenu(HWND owner, HMENU menu, BOOL bar)
{
    MENUINFO style = {0};
    if (!menu || !IsMenu(menu)) { return; }
    ThemeRestoreMenu(menu);
    style.cbSize = sizeof(style); style.fMask = MIM_BACKGROUND;
    style.hbrBack = g_Theme == EDITOR_THEME_DARK ? ThemeBrush(THEME_MENU) : GetSysColorBrush(COLOR_MENU);
    SetMenuInfo(menu, &style);
    if (g_Theme != EDITOR_THEME_DARK) { return; }
    for (int i = 0, count = GetMenuItemCount(menu); i < count; i++) {
        MENUITEMINFOW info = {0}; ThemeMenuItem *item;
        info.cbSize = sizeof(info); info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING;
        if (!GetMenuItemInfoW(menu, i, TRUE, &info) || (info.fType & (MFT_OWNERDRAW | MFT_BITMAP))) { continue; }
        item = calloc(1, sizeof(*item) + (info.cch + 1) * sizeof(WCHAR));
        if (!item) { continue; }
        item->menu = menu; item->owner = owner; item->bar = bar;
        item->originaldata = info.dwItemData; item->originaltype = info.fType;
        info.dwTypeData = item->text; info.cch++;
        if (!GetMenuItemInfoW(menu, i, TRUE, &info)) { free(item); continue; }
        info.fMask = MIIM_FTYPE | MIIM_DATA;
        info.fType |= MFT_OWNERDRAW; info.dwItemData = (ULONG_PTR)item;
        if (!SetMenuItemInfoW(menu, i, TRUE, &info)) { free(item); continue; }
        item->next = g_MenuItems; g_MenuItems = item;
    }
}

static BOOL ThemeMeasureMenu(MEASUREITEMSTRUCT *measure)
{
    ThemeMenuItem *item; HDC dc; HGDIOBJ old; RECT rect = {0};
    if (measure->CtlType != ODT_MENU || !(item = ThemeFindMenuItem(measure->itemData))) { return FALSE; }
    dc = GetDC(item->owner); old = SelectObject(dc, g_MenuFont);
    DrawTextW(dc, item->text, -1, &rect, DT_CALCRECT | DT_SINGLELINE | DT_EXPANDTABS);
    measure->itemWidth = rect.right + ThemePixels(item->owner, item->bar ? 16 : 64);
    measure->itemHeight = item->originaltype & MFT_SEPARATOR ? ThemePixels(item->owner, 9)
        : max(rect.bottom + ThemePixels(item->owner, 8), GetSystemMetrics(SM_CYMENU));
    SelectObject(dc, old); ReleaseDC(item->owner, dc); return TRUE;
}

static BOOL ThemeDrawMenu(const DRAWITEMSTRUCT *draw)
{
    ThemeMenuItem *item; RECT textrect; HGDIOBJ old; int saved;
    const WCHAR *tab;
    BOOL selected, disabled;
    if (draw->CtlType != ODT_MENU || !(item = ThemeFindMenuItem(draw->itemData))) { return FALSE; }
    saved = SaveDC(draw->hDC);
    IntersectClipRect(draw->hDC, draw->rcItem.left, draw->rcItem.top, draw->rcItem.right, draw->rcItem.bottom);
    selected = (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
    disabled = (draw->itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;
    ThemeFill(draw->hDC, &draw->rcItem, selected ? THEME_SELECTION : THEME_MENU);
    textrect = draw->rcItem;
    textrect.left += ThemePixels(item->owner, item->bar ? 8 : 28);
    textrect.right -= ThemePixels(item->owner, item->bar ? 8 : 24);
    if (item->originaltype & MFT_SEPARATOR) {
        int y = (textrect.top + textrect.bottom) / 2;
        ThemeLine(draw->hDC, textrect.left, y, textrect.right, y, THEME_BORDER, 1);
    } else {
        UINT flags = DT_SINGLELINE | DT_VCENTER | ((draw->itemState & ODS_NOACCEL) ? DT_HIDEPREFIX : 0);
        old = SelectObject(draw->hDC, g_MenuFont);
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, ThemeColor(disabled ? THEME_MUTED : selected ? THEME_SELECTION_TEXT : THEME_TEXT));
        tab = wcschr(item->text, L'\t');
        DrawTextW(draw->hDC, item->text, tab ? (int)(tab-item->text) : -1, &textrect, flags);
        if (tab) { DrawTextW(draw->hDC, tab+1, -1, &textrect, flags | DT_RIGHT); }
        if (draw->itemState & ODS_CHECKED) {
            RECT check = draw->rcItem; check.left += ThemePixels(item->owner, 5);
            check.right = textrect.left - ThemePixels(item->owner, 5);
            DrawTextW(draw->hDC, item->originaltype & MFT_RADIOCHECK ? L"\x2022" : L"\x2713", 1,
                &check, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
        SelectObject(draw->hDC, old);
        /* Windows draws the submenu arrow for owner-drawn menu items. */
    }
    RestoreDC(draw->hDC, saved); return TRUE;
}

static LRESULT ThemeMenuChar(WPARAM wp, HMENU menu)
{
    int first = -1, next = -1, matches = 0, selected = -1;
    WCHAR key = (WCHAR)towupper((WCHAR)LOWORD(wp));
    for (int i = 0; i < GetMenuItemCount(menu); i++) {
        MENUITEMINFOW info = {0}; info.cbSize = sizeof(info); info.fMask = MIIM_STATE;
        if (GetMenuItemInfoW(menu, i, TRUE, &info) && (info.fState & MFS_HILITE)) { selected = i; }
    }
    for (int i = 0; i < GetMenuItemCount(menu); i++) {
        MENUITEMINFOW info = {0}; ThemeMenuItem *item;
        info.cbSize = sizeof(info); info.fMask = MIIM_DATA | MIIM_STATE;
        if (!GetMenuItemInfoW(menu, i, TRUE, &info) || (info.fState & MFS_DISABLED)
            || !(item = ThemeFindMenuItem(info.dwItemData))) { continue; }
        for (const WCHAR *p = item->text; *p; p++) {
            if (*p != L'&') { continue; }
            p++; if (!*p) { break; } if (*p == L'&') { continue; }
            if (towupper(*p) == key) {
                if (first < 0) { first = i; }
                if (next < 0 && i > selected) { next = i; }
                matches++; break;
            }
        }
    }
    return matches ? MAKELRESULT(next >= 0 ? next : first, matches == 1 ? MNC_EXECUTE : MNC_SELECT)
        : MAKELRESULT(0, MNC_IGNORE);
}

static void ThemeDrawButton(HWND hwnd, ThemeWindow *state, HDC dc)
{
    RECT r, textrect; WCHAR text[1024]; DWORD style = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
    UINT type = style & BS_TYPEMASK;
    BOOL enabled = IsWindowEnabled(hwnd), focused = GetFocus() == hwnd;
    LRESULT status = SendMessage(hwnd, BM_GETSTATE, 0, 0);
    BOOL pressed = (status & BST_PUSHED) != 0;
    BOOL check = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
    BOOL radio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
    HFONT old = ThemeSelectFont(hwnd, dc);
    GetClientRect(hwnd, &r); textrect = r; GetWindowTextW(hwnd, text, 1024);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, ThemeColor(enabled ? THEME_TEXT : THEME_MUTED));
    ThemeFill(dc, &r, THEME_BACKGROUND);
    if (type == BS_GROUPBOX) {
        SIZE size; RECT border = r; GetTextExtentPoint32W(dc, text, lstrlenW(text), &size);
        border.top += size.cy/2; ThemeFrame(dc, &border, THEME_BORDER);
        textrect.left += ThemePixels(hwnd, 8); textrect.right = textrect.left + size.cx + ThemePixels(hwnd, 4);
        textrect.bottom = textrect.top + size.cy; ThemeFill(dc, &textrect, THEME_BACKGROUND);
        DrawTextW(dc, text, -1, &textrect, DT_SINGLELINE);
    } else if (check || radio) {
        int size = ThemePixels(hwnd, 13), center = (r.top+r.bottom)/2;
        RECT mark = {ThemePixels(hwnd, 2), center-size/2, ThemePixels(hwnd, 2)+size, center-size/2+size};
        if (style & BS_LEFTTEXT) { OffsetRect(&mark, r.right-size-ThemePixels(hwnd, 4), 0); textrect.right = mark.left-ThemePixels(hwnd, 5); }
        else { textrect.left = mark.right+ThemePixels(hwnd, 5); }
        ThemeFill(dc, &mark, pressed ? THEME_PRESSED : state->hot && enabled ? THEME_HOVER : THEME_INPUT);
        if (radio) {
            HPEN pen = CreatePen(PS_SOLID, 1, ThemeColor(focused ? THEME_SELECTION : THEME_BORDER));
            HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, ThemeBrush(THEME_INPUT));
            Ellipse(dc, mark.left, mark.top, mark.right, mark.bottom);
            if (status & BST_CHECKED) {
                InflateRect(&mark, -ThemePixels(hwnd, 3), -ThemePixels(hwnd, 3));
                SelectObject(dc, ThemeBrush(enabled ? THEME_TEXT : THEME_MUTED));
                Ellipse(dc, mark.left, mark.top, mark.right, mark.bottom);
            }
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
        } else {
            ThemeFrame(dc, &mark, focused ? THEME_SELECTION : THEME_BORDER);
            if (status & BST_INDETERMINATE) {
                InflateRect(&mark, -ThemePixels(hwnd, 3), -ThemePixels(hwnd, 3)); ThemeFill(dc, &mark, THEME_MUTED);
            } else if (status & BST_CHECKED) {
                int x = mark.left, y = mark.top, a = ThemePixels(hwnd, 3);
                ThemeLine(dc, x+a, y+size/2, x+size/2, y+size-a, enabled ? THEME_TEXT : THEME_MUTED, ThemePixels(hwnd, 2));
                ThemeLine(dc, x+size/2, y+size-a, x+size-a, y+a, enabled ? THEME_TEXT : THEME_MUTED, ThemePixels(hwnd, 2));
            }
        }
        if (style & BS_MULTILINE) {
            /* DT_VCENTER only works for single-line text. Measure the wrapped
             * label at its available width, then center it beside the mark. */
            RECT measured = textrect;
            int height = DrawTextW(dc, text, -1, &measured, DT_WORDBREAK | DT_CALCRECT);
            textrect.top += max(0, (textrect.bottom - textrect.top - height) / 2);
            DrawTextW(dc, text, -1, &textrect, DT_WORDBREAK);
        } else {
            DrawTextW(dc, text, -1, &textrect, DT_SINGLELINE | DT_VCENTER);
        }
    } else {
        ThemeFill(dc, &r, pressed ? THEME_PRESSED : state->hot && enabled ? THEME_HOVER : THEME_BUTTON);
        ThemeFrame(dc, &r, focused || type == BS_DEFPUSHBUTTON ? THEME_SELECTION : THEME_BORDER);
        InflateRect(&textrect, -ThemePixels(hwnd, 4), 0);
        DrawTextW(dc, text, -1, &textrect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (focused && type != BS_GROUPBOX && !(SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        RECT focus = r; InflateRect(&focus, -3, -3); DrawFocusRect(dc, &focus);
    }
    SelectObject(dc, old);
}

static void ThemeDrawCombo(HWND hwnd, ThemeWindow *state, HDC dc)
{
    RECT r, arrow, textrect; WCHAR text[1024]; HFONT old = ThemeSelectFont(hwnd, dc);
    BOOL enabled = IsWindowEnabled(hwnd);
    DWORD style = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
    GetClientRect(hwnd, &r); arrow = r;
    arrow.left = max(r.left, r.right - GetSystemMetrics(SM_CXVSCROLL));
    ThemeFill(dc, &r, THEME_INPUT);
    ThemeFill(dc, &arrow, state->hot && enabled ? THEME_HOVER : THEME_BUTTON);
    ThemeFrame(dc, &r, GetFocus() == hwnd ? THEME_SELECTION : THEME_BORDER);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, ThemeColor(enabled ? THEME_TEXT : THEME_MUTED));
    if ((style & 3) == CBS_DROPDOWNLIST) {
        GetWindowTextW(hwnd, text, 1024); textrect = r; textrect.left += ThemePixels(hwnd, 5); textrect.right = arrow.left-3;
        DrawTextW(dc, text, -1, &textrect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    int x = (arrow.left+arrow.right)/2, y = (arrow.top+arrow.bottom)/2, size = ThemePixels(hwnd, 3);
    ThemeLine(dc, x-size, y-1, x, y+size-1, enabled ? THEME_TEXT : THEME_MUTED, 1);
    ThemeLine(dc, x, y+size-1, x+size+1, y-2, enabled ? THEME_TEXT : THEME_MUTED, 1);
    SelectObject(dc, old);
}

static void ThemeDrawTabs(HWND hwnd, HDC dc)
{
    RECT r; HFONT old = ThemeSelectFont(hwnd, dc); int selected = TabCtrl_GetCurSel(hwnd);
    GetClientRect(hwnd, &r); ThemeFill(dc, &r, THEME_BACKGROUND);
    SetBkMode(dc, TRANSPARENT);
    for (int i = 0; i < TabCtrl_GetItemCount(hwnd); i++) {
        WCHAR text[256]; TCITEMW item = {0}; item.mask = TCIF_TEXT; item.pszText = text; item.cchTextMax = 256;
        if (!SendMessageW(hwnd, TCM_GETITEMW, i, (LPARAM)&item) || !TabCtrl_GetItemRect(hwnd, i, &r)) { continue; }
        ThemeFill(dc, &r, selected == i ? THEME_BUTTON : THEME_PANEL);
        ThemeFrame(dc, &r, THEME_BORDER); SetTextColor(dc, ThemeColor(THEME_TEXT));
        DrawTextW(dc, text, -1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        if (selected == i) { RECT accent = r; accent.bottom = accent.top+ThemePixels(hwnd, 2); ThemeFill(dc, &accent, THEME_SELECTION); }
    }
    SelectObject(dc, old);
}

static void ThemeDrawHeader(HWND hwnd, HDC dc)
{
    RECT r; HFONT old = ThemeSelectFont(hwnd, dc);
    GetClientRect(hwnd, &r); ThemeFill(dc, &r, THEME_PANEL); SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ThemeColor(THEME_TEXT));
    for (int i = 0; i < Header_GetItemCount(hwnd); i++) {
        WCHAR text[256]; HDITEMW item = {0};
        item.mask = HDI_TEXT | HDI_FORMAT; item.pszText = text; item.cchTextMax = 256;
        if (!SendMessageW(hwnd, HDM_GETITEMW, i, (LPARAM)&item) || !Header_GetItemRect(hwnd, i, &r)) { continue; }
        ThemeFrame(dc, &r, THEME_BORDER); InflateRect(&r, -ThemePixels(hwnd, 5), 0);
        DrawTextW(dc, text, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS
            | ((item.fmt & HDF_JUSTIFYMASK) == HDF_RIGHT ? DT_RIGHT
                : (item.fmt & HDF_JUSTIFYMASK) == HDF_CENTER ? DT_CENTER : DT_LEFT));
    }
    SelectObject(dc, old);
}

static void ThemeDrawTrackbar(HWND hwnd, HDC dc)
{
    RECT r, channel, thumb; GetClientRect(hwnd, &r); ThemeFill(dc, &r, THEME_BACKGROUND);
    SendMessage(hwnd, TBM_GETCHANNELRECT, 0, (LPARAM)&channel);
    SendMessage(hwnd, TBM_GETTHUMBRECT, 0, (LPARAM)&thumb);
    ThemeFill(dc, &channel, THEME_INPUT); ThemeFrame(dc, &channel, THEME_BORDER);
    ThemeFill(dc, &thumb, IsWindowEnabled(hwnd) ? THEME_SELECTION : THEME_BUTTON);
    ThemeFrame(dc, &thumb, THEME_BORDER);
}

static void ThemeWindowColors(HWND hwnd)
{
    WCHAR cls[64]; BOOL dark = g_Theme == EDITOR_THEME_DARK;
    GetClassNameW(hwnd, cls, 64);
    if (!(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CHILD)) {
        /* Attribute 20 is dark frames; 35/36 are Windows 11 caption/text
         * colors. Older systems can reject those without affecting editing. */
        COLORREF caption = dark ? ThemeColor(THEME_TITLE) : 0xffffffffu;
        COLORREF text = dark ? ThemeColor(THEME_TITLE_TEXT) : 0xffffffffu;
        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
        DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
        ThemePrepareMenu(hwnd, GetMenu(hwnd), TRUE);
        DrawMenuBar(hwnd);
    }
    if (!lstrcmpiW(cls, L"SysListView32")) {
        ListView_SetBkColor(hwnd, dark ? ThemeColor(THEME_INPUT) : GetSysColor(COLOR_WINDOW));
        ListView_SetTextBkColor(hwnd, dark ? ThemeColor(THEME_INPUT) : GetSysColor(COLOR_WINDOW));
        ListView_SetTextColor(hwnd, ThemeSystemColor(COLOR_WINDOWTEXT));
    } else if (!lstrcmpiW(cls, L"SysTreeView32")) {
        TreeView_SetBkColor(hwnd, dark ? ThemeColor(THEME_INPUT) : GetSysColor(COLOR_WINDOW));
        TreeView_SetTextColor(hwnd, ThemeSystemColor(COLOR_WINDOWTEXT));
        TreeView_SetLineColor(hwnd, ThemeSystemColor(COLOR_BTNSHADOW));
    } else if (!lstrcmpiW(cls, TOOLTIPS_CLASSW)) {
        SendMessage(hwnd, TTM_SETTIPBKCOLOR, ThemeSystemColor(COLOR_INFOBK), 0);
        SendMessage(hwnd, TTM_SETTIPTEXTCOLOR, ThemeSystemColor(COLOR_INFOTEXT), 0);
    }
    /* Use documented classic control drawing underneath our paint handlers.
     * Input, selection, accessibility and keyboard navigation stay native. */
    if (!lstrcmpiW(cls, L"Button") || !lstrcmpiW(cls, L"ComboBox")
        || !lstrcmpiW(cls, L"SysTabControl32") || !lstrcmpiW(cls, L"SysTreeView32")
        || !lstrcmpiW(cls, L"SysListView32") || !lstrcmpiW(cls, TOOLTIPS_CLASSW)) {
        SetWindowTheme(hwnd, dark ? L"" : NULL, dark ? L"" : NULL);
    }
}

static BOOL CALLBACK ThemeRefreshChild(HWND hwnd, LPARAM unused)
{ (void)unused; ThemeApplyWindow(hwnd); ThemeWindowColors(hwnd); InvalidateRect(hwnd, NULL, TRUE); return TRUE; }
static BOOL CALLBACK ThemeRefreshTop(HWND hwnd, LPARAM unused)
{
    ThemeRefreshChild(hwnd, unused); EnumChildWindows(hwnd, ThemeRefreshChild, 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    return TRUE;
}

static void ThemeApplyWindow(HWND hwnd)
{
    WCHAR cls[64]; DWORD_PTR existing; ThemeWindowKind kind = THEME_NATIVE; ThemeWindow *state;
    if (GetWindowSubclass(hwnd, ThemeWindowProc, 1, &existing)) { return; }
    GetClassNameW(hwnd, cls, 64);
    if (!wcsncmp(cls, L"GEditor", 7)) { kind = THEME_WINDOW; }
    else if (!lstrcmpW(cls, L"#32770")) { kind = THEME_DIALOG; }
    else if (!lstrcmpiW(cls, L"Button")) { kind = THEME_BUTTON_CONTROL; }
    else if (!lstrcmpiW(cls, L"ComboBox")) { kind = THEME_COMBO_CONTROL; }
    else if (!lstrcmpiW(cls, L"SysTabControl32")) { kind = THEME_TAB_CONTROL; }
    else if (!lstrcmpiW(cls, L"SysHeader32")) { kind = THEME_HEADER_CONTROL; }
    else if (!lstrcmpiW(cls, TRACKBAR_CLASSW)) { kind = THEME_TRACKBAR_CONTROL; }
    else if (lstrcmpiW(cls, L"Edit") && lstrcmpiW(cls, L"Static") && lstrcmpiW(cls, L"ListBox")
        && lstrcmpiW(cls, L"ComboLBox") && lstrcmpiW(cls, L"SysTreeView32")
        && lstrcmpiW(cls, L"SysListView32") && lstrcmpiW(cls, L"SysHeader32")
        && lstrcmpiW(cls, TOOLTIPS_CLASSW)) { return; }
    state = calloc(1, sizeof(*state)); if (!state) { return; } state->kind = kind;
    if (!SetWindowSubclass(hwnd, ThemeWindowProc, 1, (DWORD_PTR)state)) { free(state); return; }
    ThemeWindowColors(hwnd);
}

static LRESULT CALLBACK ThemeWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    ThemeWindow *state = (ThemeWindow *)data;
    BOOL dark = g_Theme == EDITOR_THEME_DARK;
    BOOL paintedcontrol = state->kind == THEME_BUTTON_CONTROL || state->kind == THEME_COMBO_CONTROL
        || state->kind == THEME_TAB_CONTROL || state->kind == THEME_TRACKBAR_CONTROL
        || state->kind == THEME_HEADER_CONTROL;
    if (msg == WM_NCDESTROY) {
        if (!(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CHILD) && GetMenu(hwnd)) { ThemeRestoreMenu(GetMenu(hwnd)); }
        RemoveWindowSubclass(hwnd, ThemeWindowProc, id); free(state);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    if (msg == WM_INITMENUPOPUP) {
        LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
        if (!HIWORD(lp)) { ThemePrepareMenu(hwnd, (HMENU)wp, FALSE); }
        return result;
    }
    if (msg == WM_UNINITMENUPOPUP) { ThemeRestoreMenu((HMENU)wp); }
    if (dark && msg == WM_MEASUREITEM && lp && ThemeMeasureMenu((MEASUREITEMSTRUCT *)lp)) { return TRUE; }
    if (dark && msg == WM_DRAWITEM && lp && ThemeDrawMenu((DRAWITEMSTRUCT *)lp)) { return TRUE; }
    if (dark && msg == WM_MENUCHAR) {
        for (ThemeMenuItem *item = g_MenuItems; item; item = item->next)
            if (item->menu == (HMENU)lp) { return ThemeMenuChar(wp, (HMENU)lp); }
    }
    if (dark && msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC) {
        HDC dc = (HDC)wp; HWND control = (HWND)lp;
        DefSubclassProc(hwnd, msg, wp, lp); /* Preserve custom status/error text. */
        COLORREF foreground = GetTextColor(dc);
        ThemeColorRole background = msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX ? THEME_INPUT : THEME_BACKGROUND;
        WCHAR cls[32]; GetClassNameW(control, cls, 32);
        if (!lstrcmpiW(cls, L"Edit") || !lstrcmpiW(cls, L"ComboBox")) { background = THEME_INPUT; }
        if (GetBkColor(dc) == ThemeColor(THEME_ERROR_BACKGROUND)) { background = THEME_ERROR_BACKGROUND; }
        if (!IsWindowEnabled(control)) { SetTextColor(dc, ThemeColor(THEME_MUTED)); }
        else if (foreground == GetSysColor(COLOR_WINDOWTEXT) || foreground == GetSysColor(COLOR_BTNTEXT))
        { SetTextColor(dc, ThemeColor(THEME_TEXT)); }
        SetBkColor(dc, ThemeColor(background));
        return (LRESULT)ThemeBrush(background);
    }
    /* A NULL class brush means the window owns its background (GL or a
     * buffered painter). Preserve its erase handler instead of flashing a
     * theme-colored fill before its next complete frame. */
    if (msg == WM_ERASEBKGND && (state->kind == THEME_DIALOG
        || (state->kind == THEME_WINDOW && GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND)))
        && !(GetClassLongPtr(hwnd, GCL_STYLE) & CS_OWNDC)) {
        RECT r; GetClientRect(hwnd, &r);
        FillRect((HDC)wp, &r, dark ? ThemeBrush(THEME_BACKGROUND)
            : GetSysColorBrush(state->kind == THEME_DIALOG ? COLOR_BTNFACE : COLOR_WINDOW)); return 1;
    }
    if (dark && (msg == WM_PAINT || msg == WM_PRINTCLIENT)) {
        UINT style = (UINT)GetWindowLongPtr(hwnd, GWL_STYLE);
        BOOL button = state->kind == THEME_BUTTON_CONTROL && (style & BS_TYPEMASK) != BS_OWNERDRAW
            && !(style & (BS_ICON | BS_BITMAP));
        BOOL combo = state->kind == THEME_COMBO_CONTROL && (style & 3) != CBS_SIMPLE
            && !(style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE));
        if (button || combo || state->kind == THEME_TAB_CONTROL || state->kind == THEME_TRACKBAR_CONTROL || state->kind == THEME_HEADER_CONTROL) {
            PAINTSTRUCT paint; HDC dc = msg == WM_PAINT ? BeginPaint(hwnd, &paint) : (HDC)wp;
            int saved = SaveDC(dc);
            if (button) { ThemeDrawButton(hwnd, state, dc); }
            else if (combo) { ThemeDrawCombo(hwnd, state, dc); }
            else if (state->kind == THEME_TAB_CONTROL) { ThemeDrawTabs(hwnd, dc); }
            else if (state->kind == THEME_HEADER_CONTROL) { ThemeDrawHeader(hwnd, dc); }
            else { ThemeDrawTrackbar(hwnd, dc); }
            RestoreDC(dc, saved); if (msg == WM_PAINT) { EndPaint(hwnd, &paint); } return 0;
        }
    }
    BOOL hoverable = state->kind == THEME_BUTTON_CONTROL || state->kind == THEME_COMBO_CONTROL;
    if (hoverable && msg == WM_MOUSEMOVE && !state->hot) {
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
        state->hot = TRUE; TrackMouseEvent(&track); if (dark) { InvalidateRect(hwnd, NULL, FALSE); }
    } else if (hoverable && msg == WM_MOUSELEAVE) { state->hot = FALSE; if (dark) { InvalidateRect(hwnd, NULL, FALSE); } }
    LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
    /* Let Windows finish painting the native frame before replacing its
     * menu/client separator. Activation can repaint it without WM_NCPAINT. */
    if (dark && (msg == WM_NCPAINT || (msg == WM_NCACTIVATE && lp != -1))) { ThemeDrawMenuBorder(hwnd); }
    /* Only our custom-painted controls need these extra refreshes. In
     * particular, selection changes set the frame title: repainting the
     * entire frame for WM_SETTEXT also invalidates its GL child windows. */
    if (dark && paintedcontrol && (msg == WM_ENABLE || msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_SETTEXT
        || msg == BM_SETCHECK || msg == BM_SETSTATE || msg == BM_SETSTYLE || msg == CB_SETCURSEL
        || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_KEYDOWN || msg == WM_KEYUP
        || msg == WM_UPDATEUISTATE)) { InvalidateRect(hwnd, NULL, FALSE); }
    return result;
}

/* A thread-local, post-creation hook covers dialogs and controls created by
 * all editor tools, including ones opened after changing theme. No system-wide
 * hooks or undocumented dark-mode exports are used. */
static LRESULT CALLBACK ThemeCreated(int code, WPARAM wp, LPARAM lp)
{
    if (code >= 0 && !g_Applying) {
        const CWPRETSTRUCT *message = (const CWPRETSTRUCT *)lp;
        if (message->message == WM_CREATE || message->message == WM_INITDIALOG) {
            g_Applying = TRUE; ThemeApplyWindow(message->hwnd);
            if (message->message == WM_INITDIALOG) { EnumChildWindows(message->hwnd, ThemeRefreshChild, 0); }
            g_Applying = FALSE;
        }
    }
    return CallNextHookEx(g_WindowHook, code, wp, lp);
}

BOOL ThemeInitialize(EditorTheme theme)
{
    NONCLIENTMETRICSW metrics = {0};
    g_Thread = GetCurrentThreadId(); g_Theme = theme == EDITOR_THEME_LIGHT ? theme : EDITOR_THEME_DARK;
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    { g_MenuFont = CreateFontIndirectW(&metrics.lfMenuFont); }
    g_WindowHook = SetWindowsHookExW(WH_CALLWNDPROCRET, ThemeCreated, NULL, g_Thread);
    return g_WindowHook != NULL;
}

void ThemeSet(EditorTheme theme)
{
    if (theme != EDITOR_THEME_LIGHT && theme != EDITOR_THEME_DARK) { return; }
    if (theme == g_Theme) { return; }
    g_Theme = theme;
    g_Applying = TRUE;
    ThemeRestoreMenu(NULL);
    EnumThreadWindows(g_Thread, ThemeRefreshTop, 0);
    g_Applying = FALSE;
}

void ThemeShutdown(void)
{
    if (g_WindowHook) { UnhookWindowsHookEx(g_WindowHook); g_WindowHook = NULL; }
    ThemeRestoreMenu(NULL);
    if (g_MenuFont) { DeleteObject(g_MenuFont); g_MenuFont = NULL; }
    for (int i = 0; i < THEME_COLOR_COUNT; i++) {
        if (g_Brushes[i]) { DeleteObject(g_Brushes[i]); g_Brushes[i] = NULL; }
    }
}
