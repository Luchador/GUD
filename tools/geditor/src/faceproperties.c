/* Scrollable face controls shared by single- and multiple-face selections. */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>

#include "faceproperties.h"

#define FACEPROPERTIES_CLASS "GEditorFaceProperties"
#define FACEPROPERTIES_MARGIN 4

enum { FACE_SUMMARY, FACE_CULL_LABEL, FACE_CULL, FACE_CULL_HELP,
       FACE_WRAP_LABEL, FACE_U_LABEL, FACE_U, FACE_V_LABEL, FACE_V,
       FACE_WRAP_HELP, FACE_SELECTION_HELP, FACE_CONTROL_COUNT };

typedef struct FacePropertiesState {
    HWND controls[FACE_CONTROL_COUNT];
    int scroll, wheelremainder;
    BOOL updating;
} FacePropertiesState;

static FacePropertiesState *FacePropertiesGetState(HWND hwnd)
{
    return (FacePropertiesState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static int FacePropertiesTextHeight(HWND control, int width)
{
    char text[512];
    RECT rect = {0, 0, width, 0};
    HDC dc = GetDC(control);
    HFONT previous = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    GetWindowText(control, text, sizeof(text));
    DrawText(dc, text, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, previous);
    ReleaseDC(control, dc);
    return rect.bottom;
}

static void FacePropertiesLayout(HWND hwnd, FacePropertiesState *state)
{
    RECT client, bounds[FACE_CONTROL_COUNT];
    SCROLLINFO info = {0};
    int width, y = FACEPROPERTIES_MARGIN, i, maximum;
    GetClientRect(hwnd, &client);
    width = client.right - FACEPROPERTIES_MARGIN * 2;
    if (width < 1) { width = 1; }
    for (i = 0; i < FACE_CONTROL_COUNT; i++)
    {
        int x = FACEPROPERTIES_MARGIN, w = width, height;
        if (i == FACE_U_LABEL || i == FACE_V_LABEL) { w = 20; }
        if (i == FACE_U || i == FACE_V) { x += 24; w = width > 24 ? width - 24 : 1; }
        height = i == FACE_CULL || i == FACE_U || i == FACE_V ? 24
            : FacePropertiesTextHeight(state->controls[i], w);
        SetRect(&bounds[i], x, y, x + w, y + height);
        if (i != FACE_U_LABEL && i != FACE_V_LABEL)
        {
            y += height + (i == FACE_SUMMARY || i == FACE_CULL_HELP ? 12 : 6);
        }
    }
    maximum = y - client.bottom;
    if (maximum < 0) { maximum = 0; }
    if (state->scroll > maximum) { state->scroll = maximum; }
    if (state->scroll < 0) { state->scroll = 0; }
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = y - 1;
    info.nPage = client.bottom;
    info.nPos = state->scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
    for (i = 0; i < FACE_CONTROL_COUNT; i++)
    {
        RECT *r = &bounds[i];
        /* The height of a native combo includes its opened list. */
        int height = i == FACE_CULL || i == FACE_U || i == FACE_V ? 160 : r->bottom - r->top;
        MoveWindow(state->controls[i], r->left, r->top - state->scroll,
                   r->right - r->left, height, TRUE);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static void FacePropertiesRevealControl(HWND hwnd, FacePropertiesState *state, HWND control)
{
    RECT client, bounds;
    GetClientRect(hwnd, &client);
    GetWindowRect(control, &bounds);
    MapWindowPoints(NULL, hwnd, (POINT *)&bounds, 2);
    if (bounds.top < 0) { state->scroll += bounds.top - FACEPROPERTIES_MARGIN; }
    else if (bounds.bottom > client.bottom)
    {
        state->scroll += bounds.bottom - client.bottom + FACEPROPERTIES_MARGIN;
    }
    else { return; }
    FacePropertiesLayout(hwnd, state);
}

static LRESULT CALLBACK FacePropertiesWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    FacePropertiesState *state = FacePropertiesGetState(hwnd);
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
        const char *labels[FACE_CONTROL_COUNT] = {
            "", "Backface culling", "",
            "On hides the back of a face. Off shows both sides.",
            "Texture wrapping", "U", "", "V", "",
            "Repeat: tile the image.\r\nMirror: alternate flipped tiles.\r\nClamp: extend the edge pixels.", ""
        };
        int i;
        state = (FacePropertiesState *)calloc(1, sizeof(*state));
        if (state == NULL) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        for (i = 0; i < FACE_CONTROL_COUNT; i++)
        {
            BOOL combo = i == FACE_CULL || i == FACE_U || i == FACE_V;
            state->controls[i] = CreateWindowEx(0, combo ? "COMBOBOX" : "STATIC", labels[i],
                WS_CHILD | WS_VISIBLE | (combo ? WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST : SS_NOPREFIX),
                0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(i + 1), cs->hInstance, NULL);
            if (state->controls[i] == NULL) { return -1; }
            SendMessage(state->controls[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            if (combo)
            {
                HWND control = state->controls[i];
                SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Mixed");
                SendMessage(control, CB_ADDSTRING, 0, (LPARAM)(i == FACE_CULL ? "Off - show both sides" : "Repeat"));
                SendMessage(control, CB_ADDSTRING, 0, (LPARAM)(i == FACE_CULL ? "On - hide back faces" : "Clamp"));
                if (i != FACE_CULL) { SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Mirror"); }
                SendMessage(control, CB_SETDROPPEDWIDTH, 180, 0);
            }
        }
        return 0;
    }
    case WM_SIZE:
        if (state) { FacePropertiesLayout(hwnd, state); }
        return 0;
    case WM_COMMAND:
        if (state && !state->updating
            && (LOWORD(wparam) == FACE_CULL + 1 || LOWORD(wparam) == FACE_U + 1 || LOWORD(wparam) == FACE_V + 1))
        {
            HWND control = (HWND)lparam;
            if (HIWORD(wparam) == CBN_SETFOCUS)
            {
                FacePropertiesRevealControl(hwnd, state, control);
            }
            else if (HIWORD(wparam) == CBN_SELENDOK)
            {
                BgFacePropertiesEdit edit = {0};
                int choice = (int)SendMessage(control, CB_GETCURSEL, 0, 0);
                if (choice > 0)
                {
                    if (control == state->controls[FACE_CULL])
                    { edit.fields = BG_FACE_PROPERTY_CULL; edit.cullbackfaces = choice == 2; }
                    else if (control == state->controls[FACE_U])
                    { edit.fields = BG_FACE_PROPERTY_WRAP_U; edit.wrapu = (BgTextureWrap)(choice - 1); }
                    else
                    { edit.fields = BG_FACE_PROPERTY_WRAP_V; edit.wrapv = (BgTextureWrap)(choice - 1); }
                }
                /* Mixed is display-only: a zero-field request just refreshes
                 * the controls from the unchanged document. */
                SendMessage(GetParent(hwnd), FACEPROPERTIES_WM_CHANGED, 0, (LPARAM)&edit);
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state)
        {
            state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
            state->scroll -= 48 * (state->wheelremainder / WHEEL_DELTA);
            state->wheelremainder %= WHEEL_DELTA;
            FacePropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_VSCROLL:
        if (state)
        {
            SCROLLINFO info = {0};
            info.cbSize = sizeof(info);
            info.fMask = SIF_ALL;
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
            FacePropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wparam, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_NCDESTROY:
        free(state);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL FacePropertiesRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = FacePropertiesWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = FACEPROPERTIES_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND FacePropertiesCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, FACEPROPERTIES_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL, instance, NULL);
}

BOOL FacePropertiesSetSelection(HWND panel, const BgDocument *document,
                                const BgFaceRef *refs, DWORD count)
{
    FacePropertiesState *state = FacePropertiesGetState(panel);
    const BgDocumentFace *first;
    int cull, wrapu, wrapv;
    BOOL textured = TRUE;
    DWORD i;
    char summary[256], texture[32];
    if (state == NULL || refs == NULL || count == 0
        || (first = BgDocumentFindFace(document, refs, NULL)) == NULL) { return FALSE; }
    cull = first->cullbackfaces + 1;
    wrapu = BgMaterialGetWrap(&first->material, FALSE) + 1;
    wrapv = BgMaterialGetWrap(&first->material, TRUE) + 1;
    for (i = 0; i < count; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);
        if (face == NULL) { return FALSE; }
        if (cull != face->cullbackfaces + 1) { cull = 0; }
        if (wrapu != (int)BgMaterialGetWrap(&face->material, FALSE) + 1) { wrapu = 0; }
        if (wrapv != (int)BgMaterialGetWrap(&face->material, TRUE) + 1) { wrapv = 0; }
        if (face->textureid == BG_TEX_NONE) { textured = FALSE; }
    }
    if (count == 1)
    {
        if (first->textureid == BG_TEX_NONE) { lstrcpyn(texture, "None", sizeof(texture)); }
        else { snprintf(texture, sizeof(texture), "%u", (unsigned int)first->textureid); }
        snprintf(summary, sizeof(summary), "Face: %lu   Room: %u\r\nLayer: %s\r\nTexture: %s",
            (unsigned long)first->id, (unsigned int)first->room,
            first->layer == BG_GEOMETRY_SECONDARY ? "Secondary" : "Primary", texture);
    }
    else { snprintf(summary, sizeof(summary), "%lu faces selected.\r\nMixed means their settings differ.", (unsigned long)count); }
    state->updating = TRUE;
    SetWindowText(state->controls[FACE_SUMMARY], summary);
    SendMessage(state->controls[FACE_CULL], CB_SETCURSEL, cull, 0);
    SendMessage(state->controls[FACE_U], CB_SETCURSEL, textured ? wrapu : -1, 0);
    SendMessage(state->controls[FACE_V], CB_SETCURSEL, textured ? wrapv : -1, 0);
    EnableWindow(state->controls[FACE_U], textured);
    EnableWindow(state->controls[FACE_V], textured);
    SetWindowText(state->controls[FACE_SELECTION_HELP], textured
        ? "Changes apply to all selected faces."
        : "Assign a texture to every selected face to edit wrapping.");
    state->updating = FALSE;
    FacePropertiesLayout(panel, state);
    return TRUE;
}
