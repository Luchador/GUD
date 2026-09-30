#include "editorsettings.h"
#include "resource.h"

#define EDITOR_SETTINGS_KEY "Software\\GUD\\GEditor\\Editor Settings"
static EditorCoordinateUnits g_CoordinateUnits = EDITOR_UNITS_NATIVE;
static EditorTheme g_EditorTheme = EDITOR_THEME_DARK;
static HWND g_EditorSettings;

void EditorSettingsLoad(void)
{
    HKEY key;
    DWORD value, type = 0, size = sizeof(value);
    g_CoordinateUnits = EDITOR_UNITS_NATIVE;
    g_EditorTheme = EDITOR_THEME_DARK;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, EDITOR_SETTINGS_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) { return; }
    if (RegQueryValueExA(key, "Coordinate units", NULL, &type, (BYTE *)&value, &size) == ERROR_SUCCESS
        && type == REG_DWORD && size == sizeof(value) && value <= EDITOR_UNITS_WORLD)
    { g_CoordinateUnits = (EditorCoordinateUnits)value; }
    type = 0; size = sizeof(value);
    if (RegQueryValueExA(key, "Theme", NULL, &type, (BYTE *)&value, &size) == ERROR_SUCCESS
        && type == REG_DWORD && size == sizeof(value) && value <= EDITOR_THEME_DARK)
    { g_EditorTheme = (EditorTheme)value; }
    RegCloseKey(key);
}

EditorCoordinateUnits EditorSettingsGetUnits(void) { return g_CoordinateUnits; }

void EditorSettingsSetUnits(EditorCoordinateUnits units)
{
    HKEY key;
    DWORD value = (DWORD)units;
    if ((units != EDITOR_UNITS_NATIVE && units != EDITOR_UNITS_WORLD) || units == g_CoordinateUnits) { return; }
    g_CoordinateUnits = units;
    /* Like recent projects, an unavailable preference store does not block
     * editing. The choice still applies for this session. */
    if (RegCreateKeyExA(HKEY_CURRENT_USER, EDITOR_SETTINGS_KEY, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExA(key, "Coordinate units", 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
        RegCloseKey(key);
    }
}

EditorTheme EditorSettingsGetTheme(void) { return g_EditorTheme; }

void EditorSettingsSetTheme(EditorTheme theme)
{
    HKEY key;
    DWORD value = (DWORD)theme;
    if ((theme != EDITOR_THEME_LIGHT && theme != EDITOR_THEME_DARK) || theme == g_EditorTheme) { return; }
    g_EditorTheme = theme;
    ThemeSet(theme);
    if (RegCreateKeyExA(HKEY_CURRENT_USER, EDITOR_SETTINGS_KEY, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExA(key, "Theme", 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
        RegCloseKey(key);
    }
}

static INT_PTR CALLBACK EditorSettingsProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        SendDlgItemMessage(hwnd, IDC_EDITOR_THEME, CB_ADDSTRING, 0, (LPARAM)"Light");
        SendDlgItemMessage(hwnd, IDC_EDITOR_THEME, CB_ADDSTRING, 0, (LPARAM)"Dark");
        SendDlgItemMessage(hwnd, IDC_EDITOR_THEME, CB_SETCURSEL, g_EditorTheme, 0);
        SendDlgItemMessage(hwnd, IDC_EDITOR_UNITS, CB_ADDSTRING, 0, (LPARAM)"Native level units");
        SendDlgItemMessage(hwnd, IDC_EDITOR_UNITS, CB_ADDSTRING, 0, (LPARAM)"World units");
        SendDlgItemMessage(hwnd, IDC_EDITOR_UNITS, CB_SETCURSEL, g_CoordinateUnits, 0);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_EDITOR_THEME && HIWORD(wparam) == CBN_SELCHANGE)
        {
            EditorSettingsSetTheme((EditorTheme)SendDlgItemMessage(hwnd, IDC_EDITOR_THEME, CB_GETCURSEL, 0, 0));
            return TRUE;
        }
        if (LOWORD(wparam) == IDC_EDITOR_UNITS && HIWORD(wparam) == CBN_SELCHANGE)
        {
            LRESULT units = SendDlgItemMessage(hwnd, IDC_EDITOR_UNITS, CB_GETCURSEL, 0, 0);
            if (units == EDITOR_UNITS_NATIVE || units == EDITOR_UNITS_WORLD)
            { SendMessage(GetParent(hwnd), EDITORSETTINGS_WM_UNITS_CHANGED, (WPARAM)units, 0); }
            return TRUE;
        }
        if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)
        { DestroyWindow(hwnd); return TRUE; }
        break;
    case WM_CLOSE: DestroyWindow(hwnd); return TRUE;
    case WM_DESTROY: g_EditorSettings = NULL; return TRUE;
    }
    return FALSE;
}

BOOL EditorSettingsShow(HWND owner, HINSTANCE instance)
{
    if (!g_EditorSettings)
    { g_EditorSettings = CreateDialog(instance, MAKEINTRESOURCE(IDD_EDITOR_SETTINGS), owner, EditorSettingsProc); }
    if (!g_EditorSettings) { return FALSE; }
    ShowWindow(g_EditorSettings, SW_SHOWNORMAL);
    SetForegroundWindow(g_EditorSettings);
    return TRUE;
}

void EditorSettingsClose(void) { if (g_EditorSettings) { DestroyWindow(g_EditorSettings); } }

BOOL EditorSettingsHandleMessage(MSG *message)
{
    if (!g_EditorSettings || !message
        || (message->hwnd != g_EditorSettings && !IsChild(g_EditorSettings, message->hwnd))) { return FALSE; }
    if (!IsDialogMessage(g_EditorSettings, message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
