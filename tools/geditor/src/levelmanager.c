#include <windows.h>
#include <commctrl.h>
#include "levelmanager.h"
#include "resource.h"

static HWND g_LevelManager;

static void LevelManagerLayout(HWND hwnd)
{
    RECT client, units = {10, 10, 70, 18};
    int footer;
    GetClientRect(hwnd, &client);
    MapDialogRect(hwnd, &units);
    footer = max(units.top, client.bottom - units.bottom - units.top);
    MoveWindow(GetDlgItem(hwnd, IDC_LEVEL_MANAGER_TABS), units.left, units.top,
               max(0, client.right - units.left * 2), max(0, footer - units.top * 2), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDCANCEL), max(units.left, client.right - units.right - units.left),
               footer, units.right, units.bottom, TRUE);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static INT_PTR CALLBACK LevelManagerDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) { LevelManagerLayout(hwnd); }
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = {0, 0, 320, 220};
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
                           FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL) { DestroyWindow(hwnd); return TRUE; }
        if (LOWORD(wparam) == IDOK) { return TRUE; }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return TRUE;
    case WM_NCDESTROY:
        g_LevelManager = NULL;
        break;
    }
    return FALSE;
}

BOOL LevelManagerShow(HWND owner, HINSTANCE instance)
{
    if (!g_LevelManager)
    {
        static const char *labels[] = {"Stage Options", "Environment", "Rooms"};
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TAB_CLASSES};
        if (!InitCommonControlsEx(&controls)) { return FALSE; }
        /* Reuse one owned, modeless window; Windows closes it with GEditor. */
        g_LevelManager = CreateDialog(instance, MAKEINTRESOURCE(IDD_LEVEL_MANAGER), owner, LevelManagerDialogProc);
        if (!g_LevelManager) { return FALSE; }
        for (int i = 0; i < 3; i++)
        {
            TCITEM item = {0};
            item.mask = TCIF_TEXT;
            item.pszText = (LPSTR)labels[i];
            if (TabCtrl_InsertItem(GetDlgItem(g_LevelManager, IDC_LEVEL_MANAGER_TABS), i, &item) == -1)
            { DestroyWindow(g_LevelManager); return FALSE; }
        }
        /* All three pages are intentionally empty until their controls are added. */
        TabCtrl_SetCurSel(GetDlgItem(g_LevelManager, IDC_LEVEL_MANAGER_TABS), 0);
        LevelManagerLayout(g_LevelManager);
    }
    ShowWindow(g_LevelManager, SW_SHOWNORMAL);
    SetForegroundWindow(g_LevelManager);
    return TRUE;
}

BOOL LevelManagerHandleMessage(MSG *message)
{
    if (!g_LevelManager || !message
        || (message->hwnd != g_LevelManager && !IsChild(g_LevelManager, message->hwnd))) { return FALSE; }
    /* Keep dialog navigation and keys inside this window, ahead of editor shortcuts. */
    if (!IsDialogMessage(g_LevelManager, message))
    { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
