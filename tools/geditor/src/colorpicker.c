/* Compact HSV picker with byte RGBA inputs and an alpha-checkered swatch. */
#include <windows.h>
#include <windowsx.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "colorpicker.h"

#define COLORPICKER_CLASS "GEditorColorPicker"
#define COLORPICKER_MARGIN 8
#define COLORPICKER_IMAGE_SIZE 256
#define COLORPICKER_EDIT_ID 3000

typedef struct ColorPickerState {
    HWND channels[4];
    unsigned char rgba[4];
    double hue, saturation, value;
    BOOL updatingtext;
    BOOL invalid[4];
    BOOL imagedirty;
    DWORD pixels[COLORPICKER_IMAGE_SIZE * COLORPICKER_IMAGE_SIZE];
    RECT square, huebar, swatch;
    int channelstop;
    int contentheight;
    int scroll;
    int wheelremainder;
    int dragging; /* 1: saturation/value, 2: hue */
} ColorPickerState;

static ColorPickerState *ColorPickerGetState(HWND hwnd)
{
    return (ColorPickerState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static double ColorPickerClamp(double value)
{
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

static void ColorPickerHsvToRgb(double hue, double saturation, double value,
                                unsigned char rgb[3])
{
    double h = fmod(hue, 360.0) / 60.0;
    double chroma = value * saturation;
    double x = chroma * (1.0 - fabs(fmod(h, 2.0) - 1.0));
    double m = value - chroma;
    double r = 0.0, g = 0.0, b = 0.0;

    if (h < 1.0) { r = chroma; g = x; }
    else if (h < 2.0) { r = x; g = chroma; }
    else if (h < 3.0) { g = chroma; b = x; }
    else if (h < 4.0) { g = x; b = chroma; }
    else if (h < 5.0) { r = x; b = chroma; }
    else { r = chroma; b = x; }
    rgb[0] = (unsigned char)(ColorPickerClamp(r + m) * 255.0 + 0.5);
    rgb[1] = (unsigned char)(ColorPickerClamp(g + m) * 255.0 + 0.5);
    rgb[2] = (unsigned char)(ColorPickerClamp(b + m) * 255.0 + 0.5);
}

static void ColorPickerRgbToHsv(ColorPickerState *state)
{
    double r = state->rgba[0] / 255.0;
    double g = state->rgba[1] / 255.0;
    double b = state->rgba[2] / 255.0;
    double maximum = fmax(r, fmax(g, b));
    double minimum = fmin(r, fmin(g, b));
    double delta = maximum - minimum;

    state->value = maximum;
    /* Hue is undefined for gray, and saturation is undefined for black.
       Retain those controls so the next graphical adjustment is useful. */
    if (maximum > 0.0) { state->saturation = delta / maximum; }
    if (delta > 0.0)
    {
        if (maximum == r) { state->hue = 60.0 * fmod((g - b) / delta, 6.0); }
        else if (maximum == g) { state->hue = 60.0 * ((b - r) / delta + 2.0); }
        else { state->hue = 60.0 * ((r - g) / delta + 4.0); }
        if (state->hue < 0.0) { state->hue += 360.0; }
    }
    state->imagedirty = TRUE;
}

static void ColorPickerSyncText(ColorPickerState *state, int channel)
{
    char text[4];

    state->updatingtext = TRUE;
    snprintf(text, sizeof(text), "%u", (unsigned int)state->rgba[channel]);
    SetWindowText(state->channels[channel], text);
    state->invalid[channel] = FALSE;
    state->updatingtext = FALSE;
}

static void ColorPickerReadText(HWND hwnd, ColorPickerState *state, int channel)
{
    char text[16];
    unsigned int value = 0;
    int i;
    BOOL valid;

    if (state->updatingtext) { return; }
    GetWindowText(state->channels[channel], text, sizeof(text));
    valid = text[0] != '\0';
    for (i = 0; text[i] != '\0'; i++)
    {
        if (text[i] < '0' || text[i] > '9') { valid = FALSE; break; }
        value = value * 10 + (unsigned int)(text[i] - '0');
        if (value > 255) { valid = FALSE; break; }
    }
    state->invalid[channel] = !valid;
    if (valid)
    {
        state->rgba[channel] = (unsigned char)value;
        if (channel < 3) { ColorPickerRgbToHsv(state); }
    }
    InvalidateRect(state->channels[channel], NULL, TRUE);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ColorPickerLayout(HWND hwnd, ColorPickerState *state)
{
    RECT client;
    SCROLLINFO info;
    int size, halfwidth, maximum, channel;

    GetClientRect(hwnd, &client);
    size = client.right - COLORPICKER_MARGIN * 2 - 30;
    if (size > 224) { size = 224; }
    if (size < 1) { size = 1; }
    state->contentheight = size + 180;
    maximum = state->contentheight - client.bottom;
    if (maximum < 0) { maximum = 0; }
    if (state->scroll > maximum) { state->scroll = maximum; }
    if (state->scroll < 0) { state->scroll = 0; }
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = state->contentheight - 1;
    info.nPage = client.bottom;
    info.nPos = state->scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);

    SetRect(&state->square, COLORPICKER_MARGIN, COLORPICKER_MARGIN - state->scroll,
            COLORPICKER_MARGIN + size, COLORPICKER_MARGIN + size - state->scroll);
    state->huebar = state->square;
    state->huebar.left = state->square.right + 10;
    state->huebar.right = state->huebar.left + 20;
    SetRect(&state->swatch, COLORPICKER_MARGIN, state->square.bottom + 12,
            COLORPICKER_MARGIN + 60, state->square.bottom + 48);
    state->channelstop = state->swatch.bottom + 14;
    halfwidth = (client.right - COLORPICKER_MARGIN * 2) / 2;
    for (channel = 0; channel < 4; channel++)
    {
        MoveWindow(state->channels[channel], COLORPICKER_MARGIN + 20 + (channel % 2) * halfwidth,
                   state->channelstop + (channel / 2) * 28,
                   halfwidth > 28 ? halfwidth - 28 : 1, 23, TRUE);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ColorPickerDrag(HWND hwnd, ColorPickerState *state, int x, int y)
{
    int channel;
    int width = state->square.right - state->square.left - 1;
    int height = state->square.bottom - state->square.top - 1;

    if (width < 1) { width = 1; }
    if (height < 1) { height = 1; }
    if (state->dragging == 1)
    {
        state->saturation = ColorPickerClamp((double)(x - state->square.left) / width);
        state->value = 1.0 - ColorPickerClamp((double)(y - state->square.top) / height);
    }
    else if (state->dragging == 2)
    {
        state->hue = 360.0 * ColorPickerClamp((double)(y - state->huebar.top) / height);
        state->imagedirty = TRUE;
    }
    ColorPickerHsvToRgb(state->hue, state->saturation, state->value, state->rgba);
    for (channel = 0; channel < 3; channel++) { ColorPickerSyncText(state, channel); }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ColorPickerRevealChannel(HWND hwnd, ColorPickerState *state, int channel)
{
    RECT client;
    int top = state->channelstop + (channel / 2) * 28;

    GetClientRect(hwnd, &client);
    if (top < 0) { state->scroll += top - COLORPICKER_MARGIN; }
    else if (top + 23 > client.bottom)
    {
        state->scroll += top + 23 - client.bottom + COLORPICKER_MARGIN;
    }
    ColorPickerLayout(hwnd, state);
}

static void ColorPickerDrawImage(HDC hdc, const RECT *rect,
                                  const DWORD *pixels, int width, int height)
{
    BITMAPINFO bitmap;

    ZeroMemory(&bitmap, sizeof(bitmap));
    bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
    bitmap.bmiHeader.biWidth = width;
    bitmap.bmiHeader.biHeight = -height;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, rect->left, rect->top, rect->right - rect->left,
                  rect->bottom - rect->top, 0, 0, width, height, pixels,
                  &bitmap, DIB_RGB_COLORS, SRCCOPY);
}

static void ColorPickerPaint(HWND hwnd, ColorPickerState *state, HDC hdc)
{
    RECT client, label;
    DWORD huepixels[COLORPICKER_IMAGE_SIZE];
    unsigned char rgb[3], hue[3];
    HFONT oldfont;
    HPEN oldpen;
    HBRUSH oldbrush;
    int x, y, channel, halfwidth;
    char hex[16];

    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetSysColorBrush(COLOR_WINDOW));
    if (state->imagedirty)
    {
        ColorPickerHsvToRgb(state->hue, 1.0, 1.0, hue);
        for (y = 0; y < COLORPICKER_IMAGE_SIZE; y++)
        {
            double value = 1.0 - y / 255.0;
            for (x = 0; x < COLORPICKER_IMAGE_SIZE; x++)
            {
                for (channel = 0; channel < 3; channel++)
                {
                    rgb[channel] = (unsigned char)((255.0 + (hue[channel] - 255.0) * x / 255.0) * value + 0.5);
                }
                state->pixels[y * COLORPICKER_IMAGE_SIZE + x] = ((DWORD)rgb[0] << 16) | ((DWORD)rgb[1] << 8) | rgb[2];
            }
        }
        state->imagedirty = FALSE;
    }
    for (y = 0; y < COLORPICKER_IMAGE_SIZE; y++)
    {
        ColorPickerHsvToRgb(y * 360.0 / 255.0, 1.0, 1.0, rgb);
        huepixels[y] = ((DWORD)rgb[0] << 16) | ((DWORD)rgb[1] << 8) | rgb[2];
    }
    ColorPickerDrawImage(hdc, &state->square, state->pixels, 256, 256);
    ColorPickerDrawImage(hdc, &state->huebar, huepixels, 1, 256);
    x = state->square.left + (int)(state->saturation * (state->square.right - state->square.left - 1) + 0.5);
    y = state->square.top + (int)((1.0 - state->value) * (state->square.bottom - state->square.top - 1) + 0.5);
    oldbrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    oldpen = (HPEN)SelectObject(hdc, GetStockObject(BLACK_PEN));
    Ellipse(hdc, x - 5, y - 5, x + 6, y + 6);
    SelectObject(hdc, GetStockObject(WHITE_PEN));
    Ellipse(hdc, x - 4, y - 4, x + 5, y + 5);
    y = state->huebar.top + (int)(state->hue / 360.0 * (state->huebar.bottom - state->huebar.top - 1) + 0.5);
    Rectangle(hdc, state->huebar.left - 2, y - 2, state->huebar.right + 2, y + 3);
    SelectObject(hdc, GetStockObject(BLACK_PEN));
    Rectangle(hdc, state->huebar.left - 3, y - 3, state->huebar.right + 3, y + 4);
    SelectObject(hdc, oldpen);
    SelectObject(hdc, oldbrush);

    /* Composite the actual RGBA brush over a checkerboard. */
    for (y = state->swatch.top; y < state->swatch.bottom; y += 6)
    {
        for (x = state->swatch.left; x < state->swatch.right; x += 6)
        {
            int background = (((x - state->swatch.left) / 6 + (y - state->swatch.top) / 6) & 1) ? 192 : 240;
            RECT cell = { x, y, x + 6, y + 6 };
            for (channel = 0; channel < 3; channel++)
            {
                rgb[channel] = (unsigned char)((state->rgba[channel] * state->rgba[3] + background * (255 - state->rgba[3]) + 127) / 255);
            }
            SetDCBrushColor(hdc, RGB(rgb[0], rgb[1], rgb[2]));
            FillRect(hdc, &cell, (HBRUSH)GetStockObject(DC_BRUSH));
        }
    }
    FrameRect(hdc, &state->swatch, GetSysColorBrush(COLOR_BTNSHADOW));
    oldfont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    SetRect(&label, state->swatch.right + 10, state->swatch.top, client.right - 8, state->swatch.top + 18);
    DrawText(hdc, "Current color", -1, &label, DT_SINGLELINE | DT_NOPREFIX);
    label.top += 18;
    label.bottom += 18;
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", state->rgba[0], state->rgba[1], state->rgba[2]);
    DrawText(hdc, hex, -1, &label, DT_SINGLELINE | DT_NOPREFIX);
    halfwidth = (client.right - COLORPICKER_MARGIN * 2) / 2;
    for (channel = 0; channel < 4; channel++)
    {
        char name[2] = { "RGBA"[channel], '\0' };
        SetRect(&label, COLORPICKER_MARGIN + (channel % 2) * halfwidth,
                state->channelstop + (channel / 2) * 28, client.right,
                state->channelstop + (channel / 2) * 28 + 23);
        DrawText(hdc, name, -1, &label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    SetRect(&label, COLORPICKER_MARGIN, state->channelstop + 60, client.right - 8, state->channelstop + 104);
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawText(hdc, "RGBA: 0-255. Click a background face to paint its nearest vertex.", -1,
             &label, DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, oldfont);
}

static LRESULT CALLBACK ColorPickerWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    ColorPickerState *state = ColorPickerGetState(hwnd);
    switch (message)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *create = (CREATESTRUCT *)lparam;
        int channel;
        state = (ColorPickerState *)calloc(1, sizeof(*state));
        if (state == NULL) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->value = 1.0;
        state->imagedirty = TRUE;
        for (channel = 0; channel < 4; channel++)
        {
            state->rgba[channel] = 255;
            state->channels[channel] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "255",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
                0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(COLORPICKER_EDIT_ID + channel), create->hInstance, NULL);
            if (state->channels[channel] == NULL) { return -1; }
            SendMessage(state->channels[channel], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            SendMessage(state->channels[channel], EM_SETLIMITTEXT, 3, 0);
        }
        return 0;
    }
    case WM_SIZE:
        if (state != NULL) { ColorPickerLayout(hwnd, state); }
        return 0;
    case WM_COMMAND:
        if (state != NULL && LOWORD(wparam) >= COLORPICKER_EDIT_ID && LOWORD(wparam) < COLORPICKER_EDIT_ID + 4)
        {
            int channel = LOWORD(wparam) - COLORPICKER_EDIT_ID;
            if (HIWORD(wparam) == EN_CHANGE) { ColorPickerReadText(hwnd, state, channel); }
            else if (HIWORD(wparam) == EN_KILLFOCUS) { ColorPickerSyncText(state, channel); }
            else if (HIWORD(wparam) == EN_SETFOCUS) { ColorPickerRevealChannel(hwnd, state, channel); }
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state != NULL)
        {
            POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            SetFocus(hwnd);
            state->dragging = PtInRect(&state->square, point) ? 1 : (PtInRect(&state->huebar, point) ? 2 : 0);
            if (state->dragging)
            {
                SetCapture(hwnd);
                ColorPickerDrag(hwnd, state, point.x, point.y);
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state != NULL && state->dragging) { ColorPickerDrag(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)); }
        return 0;
    case WM_LBUTTONUP:
        if (state != NULL && state->dragging)
        {
            ColorPickerDrag(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            state->dragging = 0;
            ReleaseCapture();
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (state != NULL) { state->dragging = 0; }
        return 0;
    case WM_MOUSEWHEEL:
        if (state != NULL && !state->dragging)
        {
            state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
            state->scroll -= 48 * (state->wheelremainder / WHEEL_DELTA);
            state->wheelremainder %= WHEEL_DELTA;
            ColorPickerLayout(hwnd, state);
        }
        return 0;
    case WM_VSCROLL:
        if (state != NULL && !state->dragging)
        {
            SCROLLINFO info;
            ZeroMemory(&info, sizeof(info));
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
            ColorPickerLayout(hwnd, state);
        }
        return 0;
    case WM_CTLCOLOREDIT:
        if (state != NULL)
        {
            int channel;
            for (channel = 0; channel < 4; channel++)
            {
                if ((HWND)lparam == state->channels[channel] && state->invalid[channel])
                {
                    SetBkColor((HDC)wparam, RGB(255, 220, 220));
                    SetDCBrushColor((HDC)wparam, RGB(255, 220, 220));
                    return (LRESULT)GetStockObject(DC_BRUSH);
                }
            }
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hwnd, &paint);
        if (state != NULL) { ColorPickerPaint(hwnd, state, hdc); }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_NCDESTROY:
        free(state);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

BOOL ColorPickerRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = ColorPickerWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = COLORPICKER_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND ColorPickerCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, COLORPICKER_CLASS, NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL, 0, 0, 1, 1, parent, NULL, instance, NULL);
}

void ColorPickerGetColor(HWND picker, unsigned char rgba[4])
{
    ColorPickerState *state = ColorPickerGetState(picker);
    int channel;
    for (channel = 0; channel < 4; channel++) { rgba[channel] = state != NULL ? state->rgba[channel] : 255; }
}

BOOL ColorPickerHandleMessage(HWND picker, MSG *message)
{
    ColorPickerState *state = ColorPickerGetState(picker);
    HWND focus = GetFocus();
    int channel;
    if (state == NULL || message->message != WM_KEYDOWN) { return FALSE; }
    for (channel = 0; channel < 4; channel++)
    {
        if (focus == state->channels[channel])
        {
            if (message->wParam == VK_RETURN)
            {
                ColorPickerSyncText(state, channel);
                SendMessage(focus, EM_SETSEL, 0, -1);
                return TRUE;
            }
            if (message->wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)
                && SendMessage(focus, EM_CANUNDO, 0, 0))
            {
                SendMessage(focus, WM_UNDO, 0, 0);
                return TRUE;
            }
        }
    }
    return FALSE;
}
