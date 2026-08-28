/*
 * GEditor - GoldenEye level editor for GUD.
 *
 * Step 1: the shell. One resizable window with the standard system
 * buttons and a working close. Everything later (menus, viewport,
 * asset panes) is added onto this skeleton.
 */

#include <windows.h>

#define GEDITOR_CLASS  "GEditorWindow"
#define GEDITOR_TITLE  "GEditor"
#define GEDITOR_WIDTH  1024
#define GEDITOR_HEIGHT  720

/*
 * Window procedure: Windows calls this for every message aimed at our
 * window. We handle the ones we care about and hand the rest to
 * DefWindowProc, which supplies the standard behaviour (dragging,
 * resizing, the min/max/close buttons, and so on).
 */
static LRESULT CALLBACK GEditorWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        /* The window is gone; ask the message loop to stop. Without
           this the process keeps running after the window closes. */
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR cmdline, int showcmd)
{
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;

    /* 1. Describe the kind of window we want, and register it. */
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = GEditorWndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); /* paints the blank client area */
    wc.lpszClassName = GEDITOR_CLASS;

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, "RegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    /* 2. Create one instance of it. WS_OVERLAPPEDWINDOW is the combination
          that gives a title bar, a resizable frame, and the standard
          minimise / maximise / close buttons. */
    hwnd = CreateWindowEx(
        0,
        GEDITOR_CLASS,
        GEDITOR_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,       /* let Windows place it */
        GEDITOR_WIDTH, GEDITOR_HEIGHT,
        NULL, NULL, hinstance, NULL);

    if (hwnd == NULL)
    {
        MessageBox(NULL, "CreateWindowEx failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, showcmd);
    UpdateWindow(hwnd);

    /* 3. The message loop. GetMessage blocks until something happens,
          returns 0 when WM_QUIT arrives, and -1 on error. */
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
