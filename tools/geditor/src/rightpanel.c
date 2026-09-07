/*
 * GEditor right-hand tool panel.
 *
 * Visibility and Transform sit above a draggable Properties splitter. The
 * transform controls send generic displacement requests to the frame, which
 * owns selection dispatch, document edits, and undo history.
 */

#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "rightpanel.h"

#define RIGHTPANEL_CLASS "GEditorRightPanel"

#define RIGHTPANEL_SPLITTER_H 5
#define RIGHTPANEL_TOP_MIN 356
#define RIGHTPANEL_TRANSFORM_TOP 148
#define RIGHTPANEL_BOTTOM_MIN 160
#define RIGHTPANEL_INITIAL_TOP_H 364
#define RIGHTPANEL_MARGIN 12
#define RIGHTPANEL_CHECK_H 22
#define RIGHTPANEL_CHECK_GAP 4

enum {
    RIGHTPANEL_ID_BG_PRIMARY = 2001,
    RIGHTPANEL_ID_BG_SECONDARY,
    RIGHTPANEL_ID_STAN,
    RIGHTPANEL_ID_PORTALS,
    RIGHTPANEL_ID_MOVE_X,
    RIGHTPANEL_ID_MOVE_Y,
    RIGHTPANEL_ID_MOVE_Z,
    RIGHTPANEL_ID_MOVE
};

typedef struct RightPanelState {
    HWND bgprimary;
    HWND bgsecondary;
    HWND stan;
    HWND portals;
    HWND offsets[3];
    HWND move;
    HWND details;
    BOOL transformenabled;
    int wheelremainder;
    char transformhint[128];
    int topheight;
    BOOL draggingsplitter;
    char detailtitle[64];
    char detailtext[2048];
} RightPanelState;

static RightPanelState *RightPanelGetState(HWND hwnd)
{
    return (RightPanelState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static void RightPanelClampTopHeight(RightPanelState *state, int height)
{
    int available = height - RIGHTPANEL_SPLITTER_H;
    int minimum;
    int maximum;

    if (available < 0) { available = 0; }
    minimum = available < RIGHTPANEL_TOP_MIN ? available : RIGHTPANEL_TOP_MIN;
    maximum = available - RIGHTPANEL_BOTTOM_MIN;
    if (maximum < minimum) { maximum = minimum; }

    /* On shorter windows, leave whatever space remains to the scrollable
       properties view after accommodating Visibility and Transform. */
    if (state->topheight > maximum) { state->topheight = maximum; }
    if (state->topheight < minimum) { state->topheight = minimum; }
}

static void RightPanelLayout(HWND hwnd, RightPanelState *state)
{
    RECT client;
    int width;
    int y = 32;
    int axis;
    int detailtop;
    int detailheight;

    GetClientRect(hwnd, &client);
    width = client.right - RIGHTPANEL_MARGIN * 2;

    if (width < 1)
    {
        width = 1;
    }

    RightPanelClampTopHeight(state, client.bottom);

    MoveWindow(state->bgprimary, RIGHTPANEL_MARGIN, y, width, RIGHTPANEL_CHECK_H, TRUE);
    y += RIGHTPANEL_CHECK_H + RIGHTPANEL_CHECK_GAP;
    MoveWindow(state->bgsecondary, RIGHTPANEL_MARGIN, y, width, RIGHTPANEL_CHECK_H, TRUE);
    y += RIGHTPANEL_CHECK_H + RIGHTPANEL_CHECK_GAP;
    MoveWindow(state->stan, RIGHTPANEL_MARGIN, y, width, RIGHTPANEL_CHECK_H, TRUE);
    y += RIGHTPANEL_CHECK_H + RIGHTPANEL_CHECK_GAP;
    MoveWindow(state->portals, RIGHTPANEL_MARGIN, y, width, RIGHTPANEL_CHECK_H, TRUE);

    for (axis = 0; axis < 3; axis++)
    {
        MoveWindow(state->offsets[axis], RIGHTPANEL_MARGIN + 24,
                   RIGHTPANEL_TRANSFORM_TOP + 54 + axis * 28,
                   width > 24 ? width - 24 : 1, 23, TRUE);
    }
    MoveWindow(state->move, RIGHTPANEL_MARGIN + 24,
               RIGHTPANEL_TRANSFORM_TOP + 138,
               width > 24 ? width - 24 : 1, 26, TRUE);
    detailtop = state->topheight + RIGHTPANEL_SPLITTER_H + 56;
    detailheight = client.bottom - RIGHTPANEL_MARGIN - detailtop;
    MoveWindow(state->details, RIGHTPANEL_MARGIN, detailtop, width,
               detailheight > 0 ? detailheight : 0, TRUE);
    ShowWindow(state->details, detailheight > 0 ? SW_SHOW : SW_HIDE);
    InvalidateRect(hwnd, NULL, FALSE);
}


static BOOL RightPanelInSplitter(const RightPanelState *state, int y)
{
    return y >= state->topheight && y < state->topheight + RIGHTPANEL_SPLITTER_H;
}


static DWORD RightPanelGetVisibility(const RightPanelState *state)
{
    DWORD visibility = 0;

    if (SendMessage(state->bgprimary, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        visibility |= RIGHTPANEL_SHOW_BG_PRIMARY;
    }

    if (SendMessage(state->bgsecondary, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        visibility |= RIGHTPANEL_SHOW_BG_SECONDARY;
    }

    if (SendMessage(state->stan, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        visibility |= RIGHTPANEL_SHOW_STAN;
    }

    if (SendMessage(state->portals, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        visibility |= RIGHTPANEL_SHOW_PORTALS;
    }

    return visibility;
}

static void RightPanelNotifyVisibility(HWND hwnd, RightPanelState *state)
{
    SendMessage(GetParent(hwnd), RIGHTPANEL_WM_VISIBILITY_CHANGED,
                (WPARAM)RightPanelGetVisibility(state), 0);
}

static void RightPanelTranslate(HWND hwnd, RightPanelState *state)
{
    RightPanelTranslation translation;
    int axis;

    if (!state->transformenabled) { return; }
    ZeroMemory(&translation, sizeof(translation));
    for (axis = 0; axis < 3; axis++)
    {
        char text[64];
        char *end;
        double value;

        GetWindowText(state->offsets[axis], text, sizeof(text));
        errno = 0;
        value = strtod(text, &end);
        if (end != text)
        {
            while (isspace((unsigned char)*end)) { end++; }
        }
        if (end == text || *end != '\0' || errno == ERANGE || !isfinite(value))
        {
            MessageBox(hwnd, "Enter a finite number for each X, Y and Z offset.",
                       "Transform", MB_ICONWARNING);
            SetFocus(state->offsets[axis]);
            SendMessage(state->offsets[axis], EM_SETSEL, 0, -1);
            return;
        }
        translation.offset[axis] = value;
    }

    if (SendMessage(GetParent(hwnd), RIGHTPANEL_WM_TRANSLATE_SELECTION,
                    0, (LPARAM)&translation))
    {
        /* Show the snapped displacement, keeping it ready for another move. */
        for (axis = 0; axis < 3; axis++)
        {
            char text[64];

            snprintf(text, sizeof(text), "%.9g", translation.applied[axis]);
            SetWindowText(state->offsets[axis], text);
        }
    }
}


static void RightPanelPaint(HWND hwnd, RightPanelState *state, HDC hdc)
{
    RECT client;
    RECT title;
    RECT splitter;
    RECT line;
    RECT detailtitle;
    RECT detailtype;
    RECT transform;
    int axis;
    HFONT font;
    HFONT oldfont;

    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetSysColorBrush(COLOR_WINDOW));

    font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    oldfont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    title.left = RIGHTPANEL_MARGIN;
    title.right = client.right - RIGHTPANEL_MARGIN;
    title.top = 8;
    title.bottom = 28;
    DrawText(hdc, "Visibility", -1, &title,
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);

    splitter.left = 0;
    splitter.right = client.right;
    splitter.top = state->topheight;
    splitter.bottom = state->topheight + RIGHTPANEL_SPLITTER_H;
    FillRect(hdc, &splitter, GetSysColorBrush(COLOR_BTNFACE));

    line = splitter;
    line.bottom = line.top + 1;
    FillRect(hdc, &line, GetSysColorBrush(COLOR_BTNSHADOW));
    line.top = splitter.bottom - 1;
    line.bottom = splitter.bottom;
    FillRect(hdc, &line, GetSysColorBrush(COLOR_BTNHIGHLIGHT));

    line.left = 0;
    line.right = client.right;
    line.top = RIGHTPANEL_TRANSFORM_TOP - 6;
    line.bottom = line.top + 1;
    FillRect(hdc, &line, GetSysColorBrush(COLOR_BTNSHADOW));

    transform.left = RIGHTPANEL_MARGIN;
    transform.right = client.right - RIGHTPANEL_MARGIN;
    transform.top = RIGHTPANEL_TRANSFORM_TOP;
    transform.bottom = transform.top + 20;
    DrawText(hdc, "Transform", -1, &transform, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
    transform.top += 26;
    transform.bottom += 26;
    DrawText(hdc, "Move by (world units)", -1, &transform,
             DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
    for (axis = 0; axis < 3; axis++)
    {
        char label[2] = { (char)('X' + axis), '\0' };

        transform.top = RIGHTPANEL_TRANSFORM_TOP + 54 + axis * 28;
        transform.bottom = transform.top + 23;
        DrawText(hdc, label, -1, &transform,
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    }
    transform.top = RIGHTPANEL_TRANSFORM_TOP + 174;
    transform.bottom = transform.top + 32;
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawText(hdc, state->transformhint, -1, &transform,
             DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    detailtitle.left = RIGHTPANEL_MARGIN;
    detailtitle.right = client.right - RIGHTPANEL_MARGIN;
    detailtitle.top = splitter.bottom + 8;
    detailtitle.bottom = detailtitle.top + 20;
    DrawText(hdc, "Properties", -1, &detailtitle,
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);

    detailtype = detailtitle;
    detailtype.top = detailtitle.bottom + 2;
    detailtype.bottom = detailtype.top + 20;
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawText(hdc, state->detailtitle, -1, &detailtype,
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);

    SelectObject(hdc, oldfont);
}

static LRESULT CALLBACK RightPanelWndProc(HWND hwnd, UINT msg,
                                           WPARAM wparam, LPARAM lparam)
{
    RightPanelState *state = RightPanelGetState(hwnd);

    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        int axis;

        state = (RightPanelState *)calloc(1, sizeof(*state));
        if (state == NULL)
        {
            return -1;
        }

        state->topheight = RIGHTPANEL_INITIAL_TOP_H;
        lstrcpyn(state->transformhint, "Select background faces to move.",
                 sizeof(state->transformhint));
        lstrcpyn(state->detailtitle, "Selection",
                 sizeof(state->detailtitle));
        lstrcpyn(state->detailtext, "No scene item selected.",
                 sizeof(state->detailtext));
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        state->bgprimary = CreateWindowEx(
            0, "BUTTON", "Background Primary",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_BG_PRIMARY,
            cs->hInstance, NULL);
        state->bgsecondary = CreateWindowEx(
            0, "BUTTON", "Background Secondary",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_BG_SECONDARY,
            cs->hInstance, NULL);
        state->stan = CreateWindowEx(
            0, "BUTTON", "Stan Geometry",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_STAN,
            cs->hInstance, NULL);
        state->portals = CreateWindowEx(
            0, "BUTTON", "Portals",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_PORTALS,
            cs->hInstance, NULL);

        for (axis = 0; axis < 3; axis++)
        {
            state->offsets[axis] = CreateWindowEx(
                WS_EX_CLIENTEDGE, "EDIT", "0",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | ES_AUTOHSCROLL,
                0, 0, 1, 1, hwnd,
                (HMENU)(INT_PTR)(RIGHTPANEL_ID_MOVE_X + axis), cs->hInstance, NULL);
            SendMessage(state->offsets[axis], WM_SETFONT, (WPARAM)font, TRUE);
            SendMessage(state->offsets[axis], EM_SETLIMITTEXT, 63, 0);
        }
        state->move = CreateWindowEx(
            0, "BUTTON", "Move Selection",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | BS_PUSHBUTTON,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_MOVE, cs->hInstance, NULL);
        state->details = CreateWindowEx(
            0, "EDIT", state->detailtext,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 1, 1, hwnd, NULL, cs->hInstance, NULL);
        SendMessage(state->move, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->details, WM_SETFONT, (WPARAM)font, TRUE);

        if (state->bgprimary == NULL || state->bgsecondary == NULL
            || state->stan == NULL || state->portals == NULL
            || state->offsets[0] == NULL || state->offsets[1] == NULL
            || state->offsets[2] == NULL || state->move == NULL || state->details == NULL)
        {
            free(state);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
            return -1;
        }

        SendMessage(state->bgprimary, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->bgsecondary, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->stan, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->portals, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->bgprimary, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(state->bgsecondary, BM_SETCHECK, BST_CHECKED, 0);

        return 0;
    }

    case WM_SIZE:
        if (state != NULL)
        {
            RightPanelLayout(hwnd, state);
        }
        return 0;

    case WM_COMMAND:
        if (state != NULL && HIWORD(wparam) == BN_CLICKED)
        {
            switch (LOWORD(wparam))
            {
            case RIGHTPANEL_ID_MOVE:
                RightPanelTranslate(hwnd, state);
                return 0;
            case RIGHTPANEL_ID_BG_PRIMARY:
            case RIGHTPANEL_ID_BG_SECONDARY:
            case RIGHTPANEL_ID_STAN:
            case RIGHTPANEL_ID_PORTALS:
                RightPanelNotifyVisibility(hwnd, state);
                return 0;
            }
        }
        break;

    case WM_SETCURSOR:
        if (state != NULL && LOWORD(lparam) == HTCLIENT)
        {
            POINT point;

            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            if (RightPanelInSplitter(state, point.y))
            {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONDOWN:
        if (state != NULL
            && RightPanelInSplitter(state, GET_Y_LPARAM(lparam)))
        {
            state->draggingsplitter = TRUE;
            SetCapture(hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (state != NULL && state->draggingsplitter)
        {
            RECT client;

            GetClientRect(hwnd, &client);
            state->topheight = GET_Y_LPARAM(lparam)
                             - RIGHTPANEL_SPLITTER_H / 2;
            RightPanelClampTopHeight(state, client.bottom);
            RightPanelLayout(hwnd, state);
        }
        return 0;

    case WM_LBUTTONUP:
        if (state != NULL && state->draggingsplitter)
        {
            state->draggingsplitter = FALSE;
            ReleaseCapture();
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (state != NULL)
        {
            state->draggingsplitter = FALSE;
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (state != NULL)
        {
            POINT point;
            RECT bounds;

            point.x = GET_X_LPARAM(lparam);
            point.y = GET_Y_LPARAM(lparam);
            GetWindowRect(state->details, &bounds);
            if (PtInRect(&bounds, point))
            {
                int lines;

                state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
                lines = -3 * (state->wheelremainder / WHEEL_DELTA);
                state->wheelremainder %= WHEEL_DELTA;
                SendMessage(state->details, EM_LINESCROLL, 0, lines);
            }
        }
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wparam, GetSysColor(COLOR_WINDOW));
        SetBkMode((HDC)wparam, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);

    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hwnd, &paint);

        if (state != NULL)
        {
            RightPanelPaint(hwnd, state, hdc);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        free(state);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL RightPanelRegisterClass(HINSTANCE hinstance)
{
    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = RightPanelWndProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = RIGHTPANEL_CLASS;

    return RegisterClass(&wc) != 0;
}

HWND RightPanelCreate(HWND parent, HINSTANCE hinstance)
{
    return CreateWindowEx(
        WS_EX_CONTROLPARENT,
        RIGHTPANEL_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 16, 16,
        parent, NULL, hinstance, NULL);
}


void RightPanelSetTransformState(HWND panel, BOOL enabled, double gridstep)
{
    RightPanelState *state = RightPanelGetState(panel);
    int axis;

    if (state == NULL) { return; }
    if (state->transformenabled != enabled)
    {
        state->transformenabled = enabled;
        for (axis = 0; axis < 3; axis++)
        {
            EnableWindow(state->offsets[axis], enabled);
        }
        EnableWindow(state->move, enabled);
    }
    if (enabled)
    {
        snprintf(state->transformhint, sizeof(state->transformhint),
                 "Grid step: %.6g world units.", gridstep);
    }
    else
    {
        lstrcpyn(state->transformhint, "Select background faces to move.",
                 sizeof(state->transformhint));
    }
    InvalidateRect(panel, NULL, FALSE);
}


BOOL RightPanelHandleMessage(HWND panel, MSG *message)
{
    RightPanelState *state = RightPanelGetState(panel);
    HWND focus = GetFocus();
    BOOL isoffset;

    if (state == NULL || !IsChild(panel, focus) || message->message != WM_KEYDOWN)
    {
        return FALSE;
    }
    isoffset = focus == state->offsets[0] || focus == state->offsets[1]
            || focus == state->offsets[2];
    if (message->wParam == VK_TAB)
    {
        return IsDialogMessage(panel, message);
    }
    if (message->wParam == VK_RETURN && (isoffset || focus == state->move))
    {
        RightPanelTranslate(panel, state);
        return TRUE;
    }
    if (isoffset && message->wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)
        && SendMessage(focus, EM_CANUNDO, 0, 0))
    {
        SendMessage(focus, WM_UNDO, 0, 0);
        return TRUE;
    }
    return FALSE;
}


void RightPanelSetBgSelectionCount(HWND panel, int count)
{
    RightPanelState *state = RightPanelGetState(panel);

    if (state == NULL)
    {
        return;
    }

    if (count > 1)
    {
        lstrcpyn(state->detailtitle, "Background Triangles",
                 sizeof(state->detailtitle));
        snprintf(state->detailtext, sizeof(state->detailtext),
                 "%d background triangles selected.\r\n\r\n"
                 "Select one triangle to inspect its properties.", count);
        state->detailtext[sizeof(state->detailtext) - 1] = '\0';
    }
    else
    {
        lstrcpyn(state->detailtitle, "Selection",
                 sizeof(state->detailtitle));
        lstrcpyn(state->detailtext, "No scene item selected.",
                 sizeof(state->detailtext));
    }

    SetWindowText(state->details, state->detailtext);
    InvalidateRect(panel, NULL, FALSE);
}


void RightPanelSetSetupObject(HWND panel, const SetupObject *object,
                              DWORD objectindex)
{
    RightPanelState *state = RightPanelGetState(panel);

    if (state == NULL || object == NULL)
    {
        return;
    }

    lstrcpyn(state->detailtitle, "Setup Object",
             sizeof(state->detailtitle));
    snprintf(state->detailtext, sizeof(state->detailtext),
        "Object index: %lu\r\n"
        "Type: %u\r\n"
        "Model ID: %d\r\n"
        "Pad: %d\r\n"
        "Extra scale: %.3f\r\n"
        "Flags: 0x%08lX\r\n"
        "Flags 2: 0x%08lX",
        (unsigned long)objectindex, (unsigned int)object->type,
        (int)object->modelid, (int)object->pad,
        (float)object->extrascale / 256.0f,
        (unsigned long)object->flags, (unsigned long)object->flags2);
    state->detailtext[sizeof(state->detailtext) - 1] = '\0';

    SetWindowText(state->details, state->detailtext);
    InvalidateRect(panel, NULL, FALSE);
}


void RightPanelSetBgTriangle(HWND panel, const BgDocument *document,
                             const BgFaceRef *ref)
{
    RightPanelState *state = RightPanelGetState(panel);
    const BgDocumentRoom *room = NULL;
    const BgDocumentFace *face;
    const BgDocumentVertex *vertices[3];
    float positions[3][3];
    const char *layer;
    char texture[48];
    int corner;

    if (state == NULL)
    {
        return;
    }

    face = BgDocumentFindFace(document, ref, &room);
    if (face == NULL || room == NULL)
    {
        RightPanelSetBgSelectionCount(panel, 0);
        return;
    }

    for (corner = 0; corner < 3; corner++)
    {
        if (face->vertexindices[corner] >= room->vertexcount)
        {
            RightPanelSetBgSelectionCount(panel, 0);
            return;
        }
        vertices[corner] = &room->vertices[face->vertexindices[corner]];
        BgDocumentGetWorldPosition(document, room, vertices[corner],
                                   positions[corner]);
    }

    layer = face->layer == BG_GEOMETRY_SECONDARY
        ? "Secondary" : "Primary";
    if (face->textureid == BG_TEX_NONE)
    {
        lstrcpyn(texture, "None", sizeof(texture));
    }
    else
    {
        snprintf(texture, sizeof(texture), "%u",
                 (unsigned int)face->textureid);
        texture[sizeof(texture) - 1] = '\0';
    }

    lstrcpyn(state->detailtitle, "Background Triangle",
             sizeof(state->detailtitle));
    snprintf(state->detailtext, sizeof(state->detailtext),
        "Face ID: %lu\r\n"
        "Room: %u\r\n"
        "Layer: %s\r\n"
        "Texture: %s\r\n"
        "Backface culling: %s\r\n\r\n"
        "Vertex 1  [ID %lu, index %lu]\r\n"
        "Shared by: %lu faces\r\n"
        "World: %.3f, %.3f, %.3f\r\n"
        "Local: %d, %d, %d\r\n"
        "UV: %.3f, %.3f texels\r\n"
        "Color: %u, %u, %u, %u\r\n\r\n"
        "Vertex 2  [ID %lu, index %lu]\r\n"
        "Shared by: %lu faces\r\n"
        "World: %.3f, %.3f, %.3f\r\n"
        "Local: %d, %d, %d\r\n"
        "UV: %.3f, %.3f texels\r\n"
        "Color: %u, %u, %u, %u\r\n\r\n"
        "Vertex 3  [ID %lu, index %lu]\r\n"
        "Shared by: %lu faces\r\n"
        "World: %.3f, %.3f, %.3f\r\n"
        "Local: %d, %d, %d\r\n"
        "UV: %.3f, %.3f texels\r\n"
        "Color: %u, %u, %u, %u",
        (unsigned long)face->id, (unsigned int)face->room, layer, texture,
        face->cullbackfaces ? "On" : "Off",
        (unsigned long)vertices[0]->id,
        (unsigned long)face->vertexindices[0],
        (unsigned long)vertices[0]->usecount,
        positions[0][0], positions[0][1], positions[0][2],
        (int)vertices[0]->x, (int)vertices[0]->y, (int)vertices[0]->z,
        vertices[0]->s / 32.0f, vertices[0]->t / 32.0f,
        (unsigned int)vertices[0]->r, (unsigned int)vertices[0]->g,
        (unsigned int)vertices[0]->b, (unsigned int)vertices[0]->a,
        (unsigned long)vertices[1]->id,
        (unsigned long)face->vertexindices[1],
        (unsigned long)vertices[1]->usecount,
        positions[1][0], positions[1][1], positions[1][2],
        (int)vertices[1]->x, (int)vertices[1]->y, (int)vertices[1]->z,
        vertices[1]->s / 32.0f, vertices[1]->t / 32.0f,
        (unsigned int)vertices[1]->r, (unsigned int)vertices[1]->g,
        (unsigned int)vertices[1]->b, (unsigned int)vertices[1]->a,
        (unsigned long)vertices[2]->id,
        (unsigned long)face->vertexindices[2],
        (unsigned long)vertices[2]->usecount,
        positions[2][0], positions[2][1], positions[2][2],
        (int)vertices[2]->x, (int)vertices[2]->y, (int)vertices[2]->z,
        vertices[2]->s / 32.0f, vertices[2]->t / 32.0f,
        (unsigned int)vertices[2]->r, (unsigned int)vertices[2]->g,
        (unsigned int)vertices[2]->b, (unsigned int)vertices[2]->a);
    state->detailtext[sizeof(state->detailtext) - 1] = '\0';

    SetWindowText(state->details, state->detailtext);
    InvalidateRect(panel, NULL, FALSE);
}
