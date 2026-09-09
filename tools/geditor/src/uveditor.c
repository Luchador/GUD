#include "uveditor.h"
#include "resource.h"

static HWND g_UVEditor;

static void UVEditorLayout(HWND hwnd)
{
    RECT client;
    RECT units = { 8, 8, 50, 16 };
    HWND closebutton = GetDlgItem(hwnd, IDCANCEL);

    if (closebutton == NULL) { return; }
    GetClientRect(hwnd, &client);
    MapDialogRect(hwnd, &units);
    MoveWindow(closebutton,
               client.right - units.left - units.right,
               client.bottom - units.top - units.bottom,
               units.right, units.bottom, TRUE);
}

static INT_PTR CALLBACK UVEditorDialogProc(HWND hwnd, UINT message,
                                           WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        g_UVEditor = hwnd;
        UVEditorLayout(hwnd);
        return TRUE;

    case WM_SIZE:
        UVEditorLayout(hwnd);
        return TRUE;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = { 0, 0, 220, 160 };

        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
                           FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL)
        {
            DestroyWindow(hwnd);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        /* Modeless dialogs use DestroyWindow, not EndDialog or WM_QUIT. */
        DestroyWindow(hwnd);
        return TRUE;

    case WM_NCDESTROY:
        if (g_UVEditor == hwnd) { g_UVEditor = NULL; }
        break;
    }
    return FALSE;
}

BOOL UVEditorShow(HWND owner, HINSTANCE instance)
{
    if (g_UVEditor == NULL)
    {
        /* A top-level owned window always stays above its owner and follows
           its minimize/restore and destruction. CreateDialog keeps the main
           editor usable while the UV Editor is open. */
        g_UVEditor = CreateDialog(instance, MAKEINTRESOURCE(IDD_UV_EDITOR),
                                  owner, UVEditorDialogProc);
        if (g_UVEditor == NULL) { return FALSE; }
    }

    ShowWindow(g_UVEditor, SW_SHOWNORMAL);
    SetForegroundWindow(g_UVEditor);
    return TRUE;
}

BOOL UVEditorHandleMessage(MSG *message)
{
    if (g_UVEditor == NULL || message == NULL
        || (message->hwnd != g_UVEditor && !IsChild(g_UVEditor, message->hwnd)))
    {
        return FALSE;
    }

    /* Keep Tab, Escape, and Alt+F4 local to this window. Dispatch any other
       input here so the main editor's accelerators cannot consume it. */
    if (!IsDialogMessage(g_UVEditor, message))
    {
        TranslateMessage(message);
        DispatchMessage(message);
    }
    return TRUE;
}
