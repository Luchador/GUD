#include "projectsettings.h"
#include "resource.h"

static HWND g_ProjectSettings;

static INT_PTR CALLBACK ProjectSettingsDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG: return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDOK)
        { DestroyWindow(hwnd); return TRUE; }
        break;
    case WM_CLOSE: DestroyWindow(hwnd); return TRUE;
    case WM_NCDESTROY: g_ProjectSettings = NULL; break;
    }
    return FALSE;
}

BOOL ProjectSettingsShow(HWND owner, HINSTANCE instance)
{
    if (!g_ProjectSettings)
    {
        g_ProjectSettings = CreateDialog(instance, MAKEINTRESOURCE(IDD_PROJECT_SETTINGS), owner, ProjectSettingsDialogProc);
        if (!g_ProjectSettings) { return FALSE; }
    }
    ShowWindow(g_ProjectSettings, SW_SHOWNORMAL);
    SetForegroundWindow(g_ProjectSettings);
    return TRUE;
}

BOOL ProjectSettingsHandleMessage(MSG *message)
{
    if (!g_ProjectSettings || !message
        || (message->hwnd != g_ProjectSettings && !IsChild(g_ProjectSettings, message->hwnd))) { return FALSE; }
    /* Dialog navigation and keys stay here instead of reaching editor tools. */
    if (!IsDialogMessage(g_ProjectSettings, message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
