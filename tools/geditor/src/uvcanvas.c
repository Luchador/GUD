#include "uvcanvas.h"

#include <windowsx.h>
#include <math.h>
#include <stdlib.h>

#define UVCANVAS_CLASS "GEditorUVCanvas"
#define UVCANVAS_UNIT_FILL RGB(48, 48, 48)
#define UVCANVAS_UNIT_EDGE RGB(160, 160, 160)
#define UVCANVAS_MIN_SCALE 4.0
#define UVCANVAS_MAX_SCALE 1000000.0

typedef struct UVCanvasState {
    /* UV coordinate at the canvas center, with U right and V up.
       One shared scale keeps a UV unit square at any window size. */
    double centeru, centerv;
    double pixelsperunit;
    int width, height;
    BOOL panning;
    POINT lastmouse;
} UVCanvasState;

static UVCanvasState *UVCanvasGetState(HWND hwnd)
{
    return (UVCanvasState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static void UVCanvasResize(HWND hwnd, UVCanvasState *state)
{
    RECT client;
    GetClientRect(hwnd, &client);
    state->width = client.right;
    state->height = client.bottom;

    /* Fit once at the first real layout. Subsequent resizes preserve the
       camera center and zoom, including a minimize/restore cycle. */
    if (state->pixelsperunit == 0.0 && state->width > 0 && state->height > 0)
    {
        int shortside = state->width < state->height ? state->width : state->height;
        state->pixelsperunit = fmax(UVCANVAS_MIN_SCALE, shortside * 0.7);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

/* Clip beyond the client edges before converting to GDI integers. Keeping
   clipped edges outside the client avoids drawing a false outline when
   zoomed far inside the square. */
static int UVCanvasClipCoordinate(double coordinate, int extent)
{
    if (coordinate < -2.0) { return -2; }
    if (coordinate > extent + 2.0) { return extent + 2; }
    return (int)floor(coordinate + 0.5);
}

static void UVCanvasDraw(HDC dc, const UVCanvasState *state)
{
    RECT client = { 0, 0, state->width, state->height };
    HGDIOBJ oldbrush, oldpen;
    double left, top, right, bottom;

    SetDCBrushColor(dc, UVCANVAS_BACKGROUND);
    FillRect(dc, &client, (HBRUSH)GetStockObject(DC_BRUSH));

    left = state->width * 0.5 - state->centeru * state->pixelsperunit;
    top = state->height * 0.5 + (state->centerv - 1.0) * state->pixelsperunit;
    right = left + state->pixelsperunit;
    bottom = top + state->pixelsperunit;
    if (right < 0.0 || bottom < 0.0 || left >= state->width || top >= state->height)
    {
        return;
    }

    oldbrush = SelectObject(dc, GetStockObject(DC_BRUSH));
    oldpen = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCBrushColor(dc, UVCANVAS_UNIT_FILL);
    SetDCPenColor(dc, UVCANVAS_UNIT_EDGE);
    Rectangle(dc, UVCanvasClipCoordinate(left, state->width),
              UVCanvasClipCoordinate(top, state->height),
              UVCanvasClipCoordinate(right, state->width) + 1,
              UVCanvasClipCoordinate(bottom, state->height) + 1);
    SelectObject(dc, oldpen);
    SelectObject(dc, oldbrush);
}

static void UVCanvasPaint(HWND hwnd, const UVCanvasState *state)
{
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(hwnd, &paint);

    if (state != NULL && state->width > 0 && state->height > 0)
    {
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP bitmap = buffer != NULL
            ? CreateCompatibleBitmap(dc, state->width, state->height) : NULL;

        /* Draw a complete frame offscreen so wheel and drag updates do not
           flash the background. Still paint if buffer allocation fails. */
        if (bitmap != NULL)
        {
            HGDIOBJ oldbitmap = SelectObject(buffer, bitmap);
            UVCanvasDraw(buffer, state);
            BitBlt(dc, 0, 0, state->width, state->height, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, oldbitmap);
            DeleteObject(bitmap);
        }
        else { UVCanvasDraw(dc, state); }
        if (buffer != NULL) { DeleteDC(buffer); }
    }
    EndPaint(hwnd, &paint);
}

static void UVCanvasPan(HWND hwnd, UVCanvasState *state, int x, int y)
{
    state->centeru -= (x - state->lastmouse.x) / state->pixelsperunit;
    state->centerv += (y - state->lastmouse.y) / state->pixelsperunit;
    state->lastmouse.x = x;
    state->lastmouse.y = y;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void UVCanvasEndPan(HWND hwnd, UVCanvasState *state)
{
    if (state == NULL || !state->panning) { return; }
    state->panning = FALSE;
    if (GetCapture() == hwnd) { ReleaseCapture(); }
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

static void UVCanvasZoom(HWND hwnd, UVCanvasState *state, WPARAM wparam, LPARAM lparam)
{
    POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
    RECT client = { 0, 0, state->width, state->height };
    double x, y, u, v, scale;

    ScreenToClient(hwnd, &mouse);
    if (state->pixelsperunit == 0.0 || !PtInRect(&client, mouse)) { return; }
    if (state->panning) { UVCanvasPan(hwnd, state, mouse.x, mouse.y); }

    x = mouse.x - state->width * 0.5;
    y = mouse.y - state->height * 0.5;
    u = state->centeru + x / state->pixelsperunit;
    v = state->centerv - y / state->pixelsperunit;

    /* Fractional wheel deltas work with high-resolution wheels too. Keep
       the UV point under the pointer fixed while changing the scale. */
    scale = state->pixelsperunit
        * pow(1.2, GET_WHEEL_DELTA_WPARAM(wparam) / (double)WHEEL_DELTA);
    scale = fmax(UVCANVAS_MIN_SCALE, fmin(UVCANVAS_MAX_SCALE, scale));
    state->centeru = u - x / scale;
    state->centerv = v + y / scale;
    state->pixelsperunit = scale;
    InvalidateRect(hwnd, NULL, FALSE);
}

static LRESULT CALLBACK UVCanvasWndProc(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam)
{
    UVCanvasState *state = UVCanvasGetState(hwnd);

    switch (message)
    {
    case WM_CREATE:
        state = (UVCanvasState *)calloc(1, sizeof(*state));
        if (state == NULL) { return -1; }
        state->centeru = state->centerv = 0.5;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;

    case WM_SIZE:
        if (state != NULL) { UVCanvasResize(hwnd, state); }
        return 0;

    case WM_PAINT:
        UVCanvasPaint(hwnd, state);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* WM_PAINT fills the complete canvas. */

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        return 0;

    case WM_MBUTTONDOWN:
        if (state != NULL && state->pixelsperunit > 0.0)
        {
            SetFocus(hwnd);
            SetCapture(hwnd);
            state->panning = GetCapture() == hwnd;
            state->lastmouse.x = GET_X_LPARAM(lparam);
            state->lastmouse.y = GET_Y_LPARAM(lparam);
            if (state->panning) { SetCursor(LoadCursor(NULL, IDC_SIZEALL)); }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (state != NULL && state->panning)
        {
            if (wparam & MK_MBUTTON)
            {
                UVCanvasPan(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            }
            else { UVCanvasEndPan(hwnd, state); }
        }
        return 0;

    case WM_MBUTTONUP:
        if (state != NULL && state->panning)
        {
            UVCanvasPan(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            UVCanvasEndPan(hwnd, state);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (state != NULL) { UVCanvasZoom(hwnd, state, wparam, lparam); }
        return 0;

    case WM_SETCURSOR:
        if (state != NULL && state->panning && LOWORD(lparam) == HTCLIENT)
        {
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            return TRUE;
        }
        break;

    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
    case WM_DESTROY:
        UVCanvasEndPan(hwnd, state);
        break;

    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        free(state);
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

HWND UVCanvasCreate(HWND parent, HINSTANCE instance)
{
    WNDCLASS wc;
    if (!GetClassInfo(instance, UVCANVAS_CLASS, &wc))
    {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = UVCanvasWndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = UVCANVAS_CLASS;
        if (!RegisterClass(&wc)) { return NULL; }
    }
    return CreateWindowEx(0, UVCANVAS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0,
        parent, NULL, instance, NULL);
}
