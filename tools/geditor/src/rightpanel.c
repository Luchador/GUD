/*
 * GEditor right-hand tool panel.
 *
 * Visibility and Transform sit above a draggable Properties/Color splitter. The
 * transform controls send absolute world-position requests to the frame, which
 * owns selection dispatch, document edits, and undo history. Vertex paint
 * replaces the lower properties view with a persistent RGBA color picker.
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "rightpanel.h"
#include "colorpicker.h"
#include "faceproperties.h"
#include "characterload.h"

#define RIGHTPANEL_CLASS "GEditorRightPanel"

#define RIGHTPANEL_SPLITTER_H 5
#define RIGHTPANEL_TOP_MIN 416
#define RIGHTPANEL_TRANSFORM_TOP 208
#define RIGHTPANEL_BOTTOM_MIN 160
#define RIGHTPANEL_INITIAL_TOP_H 424
#define RIGHTPANEL_MARGIN 12
#define RIGHTPANEL_CHECK_H 22
#define RIGHTPANEL_CHECK_GAP 4

enum {
    RIGHTPANEL_ID_BG_PRIMARY = 2001,
    RIGHTPANEL_ID_BG_SECONDARY,
    RIGHTPANEL_ID_STAN,
    RIGHTPANEL_ID_PORTALS,
    RIGHTPANEL_ID_OBJECTS,
    RIGHTPANEL_ID_POSITION_X,
    RIGHTPANEL_ID_POSITION_Y,
    RIGHTPANEL_ID_POSITION_Z,
    RIGHTPANEL_ID_STAN_OPACITY,
    RIGHTPANEL_ID_MOVE_MODE,
    RIGHTPANEL_ID_ROTATE_MODE,
    RIGHTPANEL_ID_SCALE_MODE
};

typedef struct RightPanelState {
    HWND bgprimary;
    HWND bgsecondary;
    HWND stan;
    HWND stanopacity;
    HWND stanopacitylabel;
    HWND portals;
    HWND positions[3];
    HWND movemode, rotatemode, scalebutton;
    BOOL rotationmode, scalemode, scaleislocal;
    unsigned int rotationaxes;
    HWND objects;
    HWND details;
    HWND colorpicker;
    HWND faceproperties;
    BOOL showingfaces;
    BOOL vertexpaint;
    BOOL transformenabled;
    BOOL updatingposition;
    unsigned int editedaxes;
    DWORD selectioncount;
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
    MoveWindow(state->stanopacitylabel, RIGHTPANEL_MARGIN + 24, y + 4, 90, 20, TRUE);
    MoveWindow(state->stanopacity, RIGHTPANEL_MARGIN + 114, y,
               width > 114 ? width - 114 : 1, 26, TRUE);
    y += 34;
    MoveWindow(state->portals, RIGHTPANEL_MARGIN, y, width, RIGHTPANEL_CHECK_H, TRUE);
    y += RIGHTPANEL_CHECK_H + RIGHTPANEL_CHECK_GAP;
    MoveWindow(state->objects, RIGHTPANEL_MARGIN, y, width, RIGHTPANEL_CHECK_H, TRUE);

    MoveWindow(state->movemode, RIGHTPANEL_MARGIN, RIGHTPANEL_TRANSFORM_TOP+22, width/3, 23, TRUE);
    MoveWindow(state->rotatemode, RIGHTPANEL_MARGIN+width/3, RIGHTPANEL_TRANSFORM_TOP+22, width/3, 23, TRUE);
    MoveWindow(state->scalebutton, RIGHTPANEL_MARGIN+width*2/3, RIGHTPANEL_TRANSFORM_TOP+22, width/3, 23, TRUE);
    for (axis = 0; axis < 3; axis++)
    {
        MoveWindow(state->positions[axis], RIGHTPANEL_MARGIN + 24,
                   RIGHTPANEL_TRANSFORM_TOP + 80 + axis * 28,
                   width > 24 ? width - 24 : 1, 23, TRUE);
    }
    detailtop = state->topheight + RIGHTPANEL_SPLITTER_H + 56;
    detailheight = client.bottom - RIGHTPANEL_MARGIN - detailtop;
    MoveWindow(state->details, RIGHTPANEL_MARGIN, detailtop, width,
               detailheight > 0 ? detailheight : 0, TRUE);
    ShowWindow(state->details, detailheight > 0 && !state->vertexpaint && !state->showingfaces ? SW_SHOW : SW_HIDE);
    MoveWindow(state->faceproperties, RIGHTPANEL_MARGIN, detailtop, width,
               detailheight > 0 ? detailheight : 0, TRUE);
    ShowWindow(state->faceproperties, detailheight > 0 && !state->vertexpaint && state->showingfaces ? SW_SHOW : SW_HIDE);
    detailtop = state->topheight + RIGHTPANEL_SPLITTER_H + 32;
    detailheight = client.bottom - detailtop;
    MoveWindow(state->colorpicker, 4, detailtop, client.right > 8 ? client.right - 8 : 1,
               detailheight > 0 ? detailheight : 0, TRUE);
    ShowWindow(state->colorpicker, detailheight > 0 && state->vertexpaint ? SW_SHOW : SW_HIDE);
    InvalidateRect(hwnd, NULL, FALSE);
}


static void RightPanelShowFaceProperties(HWND panel, RightPanelState *state, BOOL show)
{
    if (state->showingfaces == show) { return; }
    state->showingfaces = show;
    RightPanelLayout(panel, state);
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

    if (SendMessage(state->objects, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        visibility |= RIGHTPANEL_SHOW_OBJECTS;
    }

    return visibility;
}

static void RightPanelNotifyVisibility(HWND hwnd, RightPanelState *state)
{
    SendMessage(GetParent(hwnd), RIGHTPANEL_WM_VISIBILITY_CHANGED,
                (WPARAM)RightPanelGetVisibility(state), 0);
}

void RightPanelShowObjects(HWND panel)
{
    RightPanelState *state = RightPanelGetState(panel);
    if (state == NULL)
    {
        return;
    }
    SendMessage(state->objects, BM_SETCHECK, BST_CHECKED, 0);
    RightPanelNotifyVisibility(panel, state);
}

/* Position fields accept sums/differences of signed numbers. Let strtod
   consume each complete number, including exponent signs such as 1e-3.
   Rotation and scale retain their existing single-number input. */
static BOOL RightPanelParseTransformValue(const char *text, BOOL arithmetic, double *out)
{
    const char *cursor = text;
    double result = 0;
    char operation = '\0';
    for (;;)
    {
        char *end;
        double operand;
        errno = 0;
        operand = strtod(cursor, &end);
        if (end == cursor || errno == ERANGE || !isfinite(operand)) { return FALSE; }
        if (operation == '+') { result += operand; }
        else if (operation == '-') { result -= operand; }
        else { result = operand; }
        if (!isfinite(result)) { return FALSE; }
        while (isspace((unsigned char)*end)) { end++; }
        if (*end == '\0') { *out = result; return TRUE; }
        if (!arithmetic || (*end != '+' && *end != '-')) { return FALSE; }
        operation = *end;
        cursor = end + 1;
    }
}

static void RightPanelSetPosition(HWND hwnd, RightPanelState *state)
{
    RightPanelPosition request;
    int axis;

    if (!state->transformenabled || state->editedaxes == 0) { return; }
    ZeroMemory(&request, sizeof(request));
    request.axismask = state->editedaxes;
    for (axis = 0; axis < 3; axis++)
    {
        char text[64];
        double value;

        /* Untouched fields must not move an axis through display rounding. */
        if (!(request.axismask & (1u << axis))) { continue; }
        GetWindowText(state->positions[axis], text, sizeof(text));
        if (!RightPanelParseTransformValue(text, !state->rotationmode && !state->scalemode, &value)
            || (state->scalemode && (value <= 0 || value > 1000000)))
        {
            MessageBox(hwnd, state->scalemode ? "Enter a scale factor greater than zero and no larger than 1000000." : state->rotationmode ? "Enter a finite angle in degrees." : "Enter a finite position or an addition/subtraction expression, such as 250 + 100.",
                       "Transform", MB_ICONWARNING);
            SetFocus(state->positions[axis]);
            SendMessage(state->positions[axis], EM_SETSEL, 0, -1);
            return;
        }
        request.position[axis] = value;
    }

    /* The frame refreshes these fields from the resulting geometry, including
       native asset rounding. All edited axes form one undoable move. */
    SendMessage(GetParent(hwnd), state->scalemode ? RIGHTPANEL_WM_SET_SCALE : state->rotationmode ? RIGHTPANEL_WM_SET_ROTATION : RIGHTPANEL_WM_SET_POSITION, 0, (LPARAM)&request);
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
    transform.top += 52;
    transform.bottom += 52;
    DrawText(hdc, state->scalemode ? (state->scaleislocal ? "Scale factors (pad axes)" : "Scale factors (world axes)") : state->rotationmode ? "Rotation (degrees)" : state->selectioncount > 1 ? "Average world position" : "World position",
             -1, &transform,
             DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
    for (axis = 0; axis < 3; axis++)
    {
        char label[2] = { (char)('X' + axis), '\0' };

        transform.top = RIGHTPANEL_TRANSFORM_TOP + 80 + axis * 28;
        transform.bottom = transform.top + 23;
        DrawText(hdc, label, -1, &transform,
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    }
    transform.top = RIGHTPANEL_TRANSFORM_TOP + 168;
    transform.bottom = transform.top + 32;
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawText(hdc, state->transformhint, -1, &transform,
             DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    detailtitle.left = RIGHTPANEL_MARGIN;
    detailtitle.right = client.right - RIGHTPANEL_MARGIN;
    detailtitle.top = splitter.bottom + 8;
    detailtitle.bottom = detailtitle.top + 20;
    DrawText(hdc, state->vertexpaint ? "Paint Color" : "Properties", -1, &detailtitle,
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);

    if (state->vertexpaint)
    {
        SelectObject(hdc, oldfont);
        return;
    }
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
        lstrcpyn(state->transformhint, "Select geometry, a model or a pad.",
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
        state->stanopacitylabel = CreateWindowEx(
            0, "STATIC", "Opacity: 44%", WS_CHILD | WS_VISIBLE,
            0, 0, 1, 1, hwnd, NULL, cs->hInstance, NULL);
        state->stanopacity = CreateWindowEx(
            0, TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | TBS_HORZ | TBS_NOTICKS,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_STAN_OPACITY, cs->hInstance, NULL);
        SendMessage(state->stanopacity, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
        SendMessage(state->stanopacity, TBM_SETPOS, TRUE, 44);
        SendMessage(state->stanopacitylabel, WM_SETFONT, (WPARAM)font, TRUE);
        state->portals = CreateWindowEx(
            0, "BUTTON", "Portals",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_PORTALS,
            cs->hInstance, NULL);
        state->objects = CreateWindowEx(
            0, "BUTTON", "Objects / Characters",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)RIGHTPANEL_ID_OBJECTS,
            cs->hInstance, NULL);

        state->movemode=CreateWindowEx(0,"BUTTON","Move",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_RADIOBUTTON,
            0,0,1,1,hwnd,(HMENU)(INT_PTR)RIGHTPANEL_ID_MOVE_MODE,cs->hInstance,NULL);
        state->rotatemode=CreateWindowEx(0,"BUTTON","Rotate",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_RADIOBUTTON,
            0,0,1,1,hwnd,(HMENU)(INT_PTR)RIGHTPANEL_ID_ROTATE_MODE,cs->hInstance,NULL);
        state->scalebutton=CreateWindowEx(0,"BUTTON","Scale",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_RADIOBUTTON,
            0,0,1,1,hwnd,(HMENU)(INT_PTR)RIGHTPANEL_ID_SCALE_MODE,cs->hInstance,NULL);
        SendMessage(state->scalebutton,WM_SETFONT,(WPARAM)font,TRUE);
        SendMessage(state->movemode,WM_SETFONT,(WPARAM)font,TRUE);
        SendMessage(state->rotatemode,WM_SETFONT,(WPARAM)font,TRUE);
        SendMessage(state->movemode,BM_SETCHECK,BST_CHECKED,0);
        for (axis = 0; axis < 3; axis++)
        {
            state->positions[axis] = CreateWindowEx(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED | ES_AUTOHSCROLL,
                0, 0, 1, 1, hwnd,
                (HMENU)(INT_PTR)(RIGHTPANEL_ID_POSITION_X + axis), cs->hInstance, NULL);
            SendMessage(state->positions[axis], WM_SETFONT, (WPARAM)font, TRUE);
            SendMessage(state->positions[axis], EM_SETLIMITTEXT, 63, 0);
        }
        state->details = CreateWindowEx(
            0, "EDIT", state->detailtext,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 1, 1, hwnd, NULL, cs->hInstance, NULL);
        state->colorpicker = ColorPickerCreate(hwnd, cs->hInstance);
        state->faceproperties = FacePropertiesCreate(hwnd, cs->hInstance);
        SendMessage(state->details, WM_SETFONT, (WPARAM)font, TRUE);

        if (state->bgprimary == NULL || state->bgsecondary == NULL
            || state->stan == NULL || state->stanopacity == NULL || state->stanopacitylabel == NULL
            || state->portals == NULL
            || state->positions[0] == NULL || state->positions[1] == NULL
            || state->positions[2] == NULL || state->objects == NULL || state->details == NULL
            || state->movemode == NULL || state->rotatemode == NULL || state->scalebutton == NULL
            || state->colorpicker == NULL || state->faceproperties == NULL)
        {
            free(state);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
            return -1;
        }

        SendMessage(state->bgprimary, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->bgsecondary, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->stan, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->portals, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->objects, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->objects, BM_SETCHECK, BST_CHECKED, 0);
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

    case FACEPROPERTIES_WM_CHANGED:
    case FACEPROPERTIES_WM_REVEAL_IMAGE:
        return SendMessage(GetParent(hwnd), msg, wparam, lparam);

    case WM_COMMAND:
        if (state != NULL && HIWORD(wparam) == EN_CHANGE
            && LOWORD(wparam) >= RIGHTPANEL_ID_POSITION_X
            && LOWORD(wparam) <= RIGHTPANEL_ID_POSITION_Z)
        {
            if (!state->updatingposition)
            {
                state->editedaxes |= 1u << (LOWORD(wparam) - RIGHTPANEL_ID_POSITION_X);
            }
            return 0;
        }
        if (state != NULL && HIWORD(wparam) == BN_CLICKED)
        {
            switch (LOWORD(wparam))
            {
            case RIGHTPANEL_ID_MOVE_MODE:
            case RIGHTPANEL_ID_ROTATE_MODE:
            case RIGHTPANEL_ID_SCALE_MODE:
                SendMessage(GetParent(hwnd), RIGHTPANEL_WM_TRANSFORM_MODE,
                    LOWORD(wparam) == RIGHTPANEL_ID_SCALE_MODE ? TRANSFORM_SCALE
                    : LOWORD(wparam) == RIGHTPANEL_ID_ROTATE_MODE ? TRANSFORM_ROTATE : TRANSFORM_MOVE, 0);
                return 0;
            case RIGHTPANEL_ID_BG_PRIMARY:
            case RIGHTPANEL_ID_BG_SECONDARY:
            case RIGHTPANEL_ID_STAN:
            case RIGHTPANEL_ID_PORTALS:
            case RIGHTPANEL_ID_OBJECTS:
                EnableWindow(state->stanopacity, (RightPanelGetVisibility(state) & RIGHTPANEL_SHOW_STAN) != 0);
                RightPanelNotifyVisibility(hwnd, state);
                return 0;
            }
        }
        break;

    case WM_HSCROLL:
        if (state != NULL && (HWND)lparam == state->stanopacity)
        {
            int percent = (int)SendMessage(state->stanopacity, TBM_GETPOS, 0, 0);
            char label[32];
            snprintf(label, sizeof(label), "Opacity: %d%%", percent);
            SetWindowText(state->stanopacitylabel, label);
            SendMessage(GetParent(hwnd), RIGHTPANEL_WM_STAN_OPACITY, percent, 0);
            return 0;
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
            if (state->vertexpaint)
            {
                GetWindowRect(state->colorpicker, &bounds);
                if (PtInRect(&bounds, point))
                {
                    SendMessage(state->colorpicker, WM_MOUSEWHEEL, wparam, lparam);
                }
                return 0;
            }
            if (state->showingfaces)
            {
                GetWindowRect(state->faceproperties, &bounds);
                if (PtInRect(&bounds, point))
                {
                    SendMessage(state->faceproperties, WM_MOUSEWHEEL, wparam, lparam);
                }
                return 0;
            }
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
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES};

    if (!InitCommonControlsEx(&controls)) { return FALSE; }
    if (!ColorPickerRegisterClass(hinstance) || !FacePropertiesRegisterClass(hinstance)) { return FALSE; }
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


void RightPanelSetTransformState(HWND panel, const double position[3],
                                 DWORD count, BOOL editable, double gridstep)
{
    RightPanelState *state = RightPanelGetState(panel);
    int axis;

    if (state == NULL) { return; }
    state->transformenabled = position != NULL && editable;
    state->selectioncount = position != NULL ? count : 0;
    state->updatingposition = TRUE;
    for (axis = 0; axis < 3; axis++)
    {
        char text[64] = "";
        if (position != NULL) { snprintf(text, sizeof(text), "%.9g", position[axis]); }
        SetWindowText(state->positions[axis], text);
        EnableWindow(state->positions[axis], position != NULL);
        SendMessage(state->positions[axis], EM_SETREADONLY, !editable, 0);
    }
    state->editedaxes = 0;
    state->updatingposition = FALSE;
    if (state->scalemode)
    {
        const char *hint = position == NULL ? "Select geometry, props or pads.\r\nVertices need a group; characters cannot scale."
            : "Enter applies factors: 1 unchanged, 2 doubles. Drag in 1% steps.";
        lstrcpyn(state->transformhint, hint, sizeof(state->transformhint));
    }
    else if(state->rotationmode){
        const char *hint=position==NULL?"Select geometry or a model. Vertices need a group."
            :state->rotationaxes==2?"Heading in degrees. Characters stay upright."
            :"Press Enter to set angles. World axes; XYZ Euler order.";
        lstrcpyn(state->transformhint,hint,sizeof(state->transformhint));
    }
    else if (state->transformenabled && gridstep > 0)
    {
        snprintf(state->transformhint, sizeof(state->transformhint),
                 "Enter sets position; + and - allowed.\r\nAsset precision: %.6g units.", gridstep);
    }
    else
    {
        const char *hint = state->vertexpaint ? "BG: vertex RGBA. Stan: whole-tile RGB."
            : position == NULL ? "Select geometry, a model or a pad."
            : editable ? "Enter sets position; + and - allowed." : "This selection cannot be moved.";
        lstrcpyn(state->transformhint, hint, sizeof(state->transformhint));
    }
    InvalidateRect(panel, NULL, FALSE);
}

BOOL RightPanelHandleMessage(HWND panel, MSG *message)
{
    RightPanelState *state = RightPanelGetState(panel);
    HWND focus = GetFocus();
    BOOL isposition;

    if (state == NULL || !IsChild(panel, focus) || message->message != WM_KEYDOWN)
    {
        return FALSE;
    }
    if (state->vertexpaint && ColorPickerHandleMessage(state->colorpicker, message))
    {
        return TRUE;
    }
    isposition = focus == state->positions[0] || focus == state->positions[1]
            || focus == state->positions[2];
    if (message->wParam == VK_TAB)
    {
        return IsDialogMessage(panel, message);
    }
    if (message->wParam == VK_RETURN && isposition)
    {
        RightPanelSetPosition(panel, state);
        return TRUE;
    }
    if (isposition && message->wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)
        && SendMessage(focus, EM_CANUNDO, 0, 0))
    {
        SendMessage(focus, WM_UNDO, 0, 0);
        return TRUE;
    }
    return FALSE;
}


void RightPanelSetVertexPaintMode(HWND panel, BOOL enabled)
{
    RightPanelState *state = RightPanelGetState(panel);
    if (state == NULL || state->vertexpaint == enabled) { return; }
    state->vertexpaint = enabled;
    RightPanelLayout(panel, state);
}


void RightPanelGetPaintColor(HWND panel, unsigned char rgba[4])
{
    RightPanelState *state = RightPanelGetState(panel);
    ColorPickerGetColor(state != NULL ? state->colorpicker : NULL, rgba);
}


void RightPanelSetStanSelection(HWND panel, const StanFile *stan, EditorTool tool,
                                 DWORD count, DWORD singletile)
{
    RightPanelState *state = RightPanelGetState(panel);
    const char *kind = tool == EDITOR_TOOL_VERTEX_SELECT ? "vertices"
        : tool == EDITOR_TOOL_EDGE_SELECT ? "edges" : "tiles";
    if (state == NULL) { return; }
    lstrcpyn(state->detailtitle, "Stan Selection", sizeof(state->detailtitle));
    if (tool == EDITOR_TOOL_FACE_SELECT && count == 1 && singletile < stan->tilecount)
    {
        const StanTile *tile = &stan->tiles[singletile];
        snprintf(state->detailtext, sizeof(state->detailtext),
            "Tile: %06lX\r\nRoom: %u\r\nPoints: %u\r\nSpecial: 0x%X\r\nRGB: %u, %u, %u\r\n\r\nDrag an arrow or enter a world position.",
            (unsigned long)tile->id, tile->room, tile->pointcount, tile->special,
            tile->red, tile->green, tile->blue);
    }
    else
    {
        snprintf(state->detailtext, sizeof(state->detailtext),
            "%lu stan %s selected.\r\n\r\nShift-click to add.\r\nControl-click to remove.\r\nDrag an arrow or enter a world position.",
            (unsigned long)count, kind);
    }
    RightPanelShowFaceProperties(panel, state, FALSE);
    SetWindowText(state->details, state->detailtext);
    InvalidateRect(panel, NULL, FALSE);
}

void RightPanelSetBgComponentSelection(HWND panel, BOOL edges, int count)
{
    RightPanelState *state=RightPanelGetState(panel);
    if (state==NULL) { return; }
    lstrcpyn(state->detailtitle,edges ? "Background Edges" : "Background Vertices",sizeof(state->detailtitle));
    snprintf(state->detailtext,sizeof(state->detailtext),
        "%d %s selected.\r\n\r\nShift-click to add.\r\nControl-click to remove.\r\nDrag an arrow to move.\r\nEscape cancels a drag.",
        count,edges ? "edges" : "vertices");
    RightPanelShowFaceProperties(panel, state, FALSE);
    SetWindowText(state->details,state->detailtext);
    InvalidateRect(panel,NULL,FALSE);
}

void RightPanelSetBgSelectionCount(HWND panel, int count)
{
    RightPanelState *state = RightPanelGetState(panel);

    if (state == NULL)
    {
        return;
    }

    if (count > 0)
    {
        lstrcpyn(state->detailtitle, "Background Triangles",
                 sizeof(state->detailtitle));
        snprintf(state->detailtext, sizeof(state->detailtext),
                 "%d background triangles selected.\r\n\r\n"
                 "The face properties could not be read.", count);
        state->detailtext[sizeof(state->detailtext) - 1] = '\0';
    }
    else
    {
        lstrcpyn(state->detailtitle, "Selection",
                 sizeof(state->detailtitle));
        lstrcpyn(state->detailtext, "No scene item selected.",
                 sizeof(state->detailtext));
    }

    RightPanelShowFaceProperties(panel, state, FALSE);
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

    RightPanelShowFaceProperties(panel, state, FALSE);
    SetWindowText(state->details, state->detailtext);
    InvalidateRect(panel, NULL, FALSE);
}


void RightPanelSetSetupPad(HWND panel, const SetupFile *setup, const SetupPadRef *ref)
{
    RightPanelState *state = RightPanelGetState(panel);
    const SetupPad *pad;

    if (state == NULL || setup == NULL || ref == NULL
        || ref->index >= (ref->bound ? setup->boundpadcount : setup->padcount)) { return; }
    pad = ref->bound ? &setup->boundpads[ref->index].pad : &setup->pads[ref->index];
    lstrcpyn(state->detailtitle, ref->bound ? "Bound Pad" : "Pad", sizeof(state->detailtitle));
    snprintf(state->detailtext, sizeof(state->detailtext),
        "Pad index: %lu\r\n"
        "Stan link: %s\r\n"
        "Up: %.4g, %.4g, %.4g\r\n"
        "Look: %.4g, %.4g, %.4g\r\n\r\n"
        "Drag an arrow or enter a world position.\r\n"
        "Moving a pad updates all references to it.",
        (unsigned long)ref->index, pad->stanname[0] ? pad->stanname : "Automatic",
        pad->up[0], pad->up[1], pad->up[2], pad->look[0], pad->look[1], pad->look[2]);
    RightPanelShowFaceProperties(panel, state, FALSE);
    SetWindowText(state->details, state->detailtext);
    InvalidateRect(panel, NULL, FALSE);
}

void RightPanelSetSetupCharacter(HWND panel, const SetupCharacter *character)
{
    RightPanelState *state = RightPanelGetState(panel);
    CharacterModelDefinition body, head;
    int bodyid, headid;
    const char *headname = "Included in body";

    if (state == NULL || character == NULL
        || !CharacterResolveModels(character, &bodyid, &headid)
        || !CharacterGetModelDefinition(bodyid, &body)) { return; }
    if (headid >= 0 && CharacterGetModelDefinition(headid, &head))
    {
        headname = head.filename;
    }
    lstrcpyn(state->detailtitle, "Setup Character", sizeof(state->detailtitle));
    snprintf(state->detailtext, sizeof(state->detailtext),
        "Character ID: %u\r\n"
        "Body: %s\r\n"
        "Head: %s%s\r\n"
        "Pad: %u\r\n"
        "AI list: 0x%04X\r\n"
        "Flags: 0x%04X\r\n"
        "\r\nDrag an arrow or enter a world position.\r\n"
        "Characters settle onto a stan floor when placed.",
        (unsigned int)character->chrnum, body.filename, headname,
        headid >= 0 && character->headid < 0 ? " (random preview)" : "",
        (unsigned int)character->pad, (unsigned int)character->ailistid,
        (unsigned int)character->flags);
    state->detailtext[sizeof(state->detailtext) - 1] = '\0';
    RightPanelShowFaceProperties(panel, state, FALSE);
    SetWindowText(state->details, state->detailtext);
    InvalidateRect(panel, NULL, FALSE);
}


void RightPanelSetBgFaces(HWND panel, const BgDocument *document,
                         const BgFaceRef *refs, DWORD count, HWND browser)
{
    RightPanelState *state = RightPanelGetState(panel);
    if (state == NULL) { return; }
    if (!FacePropertiesSetSelection(state->faceproperties, document, refs, count, browser))
    {
        RightPanelSetBgSelectionCount(panel, (int)count);
        return;
    }
    lstrcpyn(state->detailtitle, "Background Face / Texture", sizeof(state->detailtitle));
    RightPanelShowFaceProperties(panel, state, TRUE);
    InvalidateRect(panel, NULL, FALSE);
}


void RightPanelSetRotationAxes(HWND panel, unsigned int axes)
{
    RightPanelState *state = RightPanelGetState(panel);
    int axis;
    if (!state)
    {
        return;
    }
    state->rotationaxes = axes;
    if (!state->rotationmode)
    {
        return;
    }
    for (axis = 0; axis < 3; axis++)
    {
        EnableWindow(state->positions[axis], (axes & (1u << axis)) != 0);
        SendMessage(state->positions[axis], EM_SETREADONLY, !(axes & (1u << axis)), 0);
    }
}

/* The frame applies panel clicks and keyboard shortcuts through one path. */
void RightPanelSetTransformMode(HWND panel, TransformMode mode)
{
    RightPanelState *state = RightPanelGetState(panel);
    if (state == NULL) { return; }
    state->rotationmode = mode == TRANSFORM_ROTATE;
    state->scalemode = mode == TRANSFORM_SCALE;
    SendMessage(state->movemode, BM_SETCHECK, mode == TRANSFORM_MOVE ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(state->rotatemode, BM_SETCHECK, state->rotationmode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(state->scalebutton, BM_SETCHECK, state->scalemode ? BST_CHECKED : BST_UNCHECKED, 0);
    InvalidateRect(panel, NULL, FALSE);
}

void RightPanelSetScaleLocal(HWND panel, BOOL local)
{
    RightPanelState *state = RightPanelGetState(panel);
    if (state) { state->scaleislocal = local; }
}
