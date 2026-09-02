/**
  * GEditor content browser panel.
  *
  */

#include <windows.h>

#include "browser.h"

#define BROWSER_CLASS    "GEditorBrowser"
#define BROWSER_HEADER_H 26

static void BrowserPaint(HWND hwnd, HDC hdc)
{
    RECT rc;
    RECT header;
    RECT body;
    HFONT font;
    HFONT oldfont;

    GetClientRect(hwnd, &rc);

    /* Header band. */
    header = rc;
    header.bottom = BROWSER_HEADER_H;
    FillRect(hdc, &header, GetSysColorBrush(COLOR_BTNFACE));

    /* Body. */
    body = rc;
    body.top = BROWSER_HEADER_H;
    FillRect(hdc, &body, GetSysColorBrush(COLOR_WINDOW));

    /* Text: the stock GUI font, not the default device font (which is
       the ancient bitmap System font). Selected in, restored after -
       GDI objects are checked out and returned like library books. */
    font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    oldfont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    header.left += 8;
    SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
    DrawText(hdc, "Content Browser", -1, &header, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    body.left += 8;
    body.top += 8;
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawText(hdc, "Project assets will appear here.", -1, &body,
             DT_SINGLELINE | DT_TOP | DT_LEFT);

    SelectObject(hdc, oldfont);
}


static LRESULT CALLBACK BrowserWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        BrowserPaint(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        /* WM_PAINT covers every pixel; skipping the erase avoids the
           flash-of-background on resize, as in the viewport. */
        return 1;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}


BOOL BrowserRegisterClass(HINSTANCE hinstance)
{
    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = BrowserWndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; /* painted in WM_PAINT */
    wc.lpszClassName = BROWSER_CLASS;

    return RegisterClass(&wc) != 0;
}


HWND BrowserCreate(HWND parent, HINSTANCE hinstance)
{
    return CreateWindowEx(
        0,
        BROWSER_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 16, 16, /* placeholder; the parent's layout positions it */
        parent, NULL, hinstance, NULL);
}