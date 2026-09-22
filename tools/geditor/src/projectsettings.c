#include "projectsettings.h"
#include "resource.h"
#include "sfxcatalog.h"
#include "editorpath.h"
#include <commctrl.h>
#include <stdio.h>

static HWND g_ProjectSettings;
static SfxCatalog g_SfxCatalog;
static BOOL g_SfxLoaded;
static char g_SfxError[384];

static void ProjectSettingsLayout(HWND hwnd)
{
    RECT client, units = {10, 10, 70, 18}, page;
    HWND tab = GetDlgItem(hwnd, IDC_PROJECT_SETTINGS_TABS);
    GetClientRect(hwnd, &client); MapDialogRect(hwnd, &units);
    int footer = max(units.top, client.bottom - units.bottom - units.top);
    MoveWindow(tab, units.left, units.top, max(0, client.right - units.left * 2), max(0, footer - units.top * 2), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDCANCEL), max(units.left, client.right - units.right - units.left),
               footer, units.right, units.bottom, TRUE);
    GetClientRect(tab, &page); TabCtrl_AdjustRect(tab, FALSE, &page);
    MapWindowPoints(tab, hwnd, (POINT *)&page, 2);
    InflateRect(&page, -units.left / 2, -units.top / 2);
    int width = max(1, page.right - page.left), gap = max(4, units.top / 2), line = units.bottom;
    int labelwidth = units.right / 2;
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_SFX_FILTER_LABEL), page.left, page.top + gap / 2, labelwidth, line, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_SFX_FILTER), page.left + labelwidth, page.top, width - labelwidth, line, TRUE);
    HWND list = GetDlgItem(hwnd, IDC_PROJECT_SFX_LIST);
    MoveWindow(list, page.left, page.top + line + gap, width, max(0, page.bottom - page.top - line * 3 - gap * 2), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_SFX_STATUS), page.left, page.bottom - line * 2, width, line * 2, TRUE);
    int available = max(1, width - GetSystemMetrics(SM_CXVSCROLL) - 8);
    ListView_SetColumnWidth(list, 0, units.right * 3 / 4);
    ListView_SetColumnWidth(list, 1, max(units.right * 2, available - units.right * 11 / 4));
    ListView_SetColumnWidth(list, 2, units.right);
    ListView_SetColumnWidth(list, 3, units.right);
    BOOL sfx = TabCtrl_GetCurSel(tab) == 0;
    for (int id = IDC_PROJECT_SFX_FILTER_LABEL; id <= IDC_PROJECT_SFX_STATUS; id++)
    { ShowWindow(GetDlgItem(hwnd, id), sfx ? SW_SHOW : SW_HIDE); }
}

static void ProjectSettingsPopulate(void)
{
    HWND list = GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_LIST);
    char filter[128], text[128];
    DWORD shown = 0;
    GetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_FILTER, filter, sizeof(filter));
    SendMessage(list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list);
    for (DWORD i = 0; i < g_SfxCatalog.count; i++)
    {
        const SfxEntry *entry = &g_SfxCatalog.entries[i];
        if (!SfxCatalogMatches(entry, filter)) { continue; }
        LVITEM item = {0};
        item.mask = LVIF_TEXT; item.iItem = (int)shown; item.pszText = text;
        snprintf(text, sizeof(text), "0x%04lX", (unsigned long)entry->id);
        int row = ListView_InsertItem(list, &item);
        if (row < 0)
        {
            lstrcpyn(g_SfxError, "Out of memory displaying sound effects.", sizeof(g_SfxError));
            break;
        }
        ListView_SetItemText(list, row, 1, (char *)SfxCatalogName(entry->id));
        if (entry->format == 0) { strcpy(text, "ADPCM"); }
        else if (entry->format == 1) { strcpy(text, "PCM 16-bit"); }
        else { snprintf(text, sizeof(text), "Type %u", entry->format); }
        ListView_SetItemText(list, row, 2, text);
        snprintf(text, sizeof(text), "%lu", (unsigned long)entry->sampleBytes);
        ListView_SetItemText(list, row, 3, text);
        shown++;
    }
    SendMessage(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, NULL, TRUE);
    if (!g_SfxLoaded || g_SfxError[0]) { SetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_STATUS, g_SfxError); }
    else
    {
        snprintf(text, sizeof(text), "%lu of %lu sound effects", (unsigned long)shown, (unsigned long)g_SfxCatalog.count);
        SetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_STATUS, text);
    }
    EnableWindow(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_FILTER), g_SfxLoaded);
    EnableWindow(list, g_SfxLoaded);
}

void ProjectSettingsRefresh(const char *projectdir)
{
    if (!g_ProjectSettings) { return; }
    RomFile rom = {0};
    char path[MAX_PATH];
    const char *why = "Open a project to list its sound effects.";
    SfxCatalogFree(&g_SfxCatalog);
    g_SfxLoaded = FALSE;
    if (projectdir && projectdir[0])
    {
        if (!EditorPathJoin(path, sizeof(path), projectdir, "base.z64")) { why = "The project ROM path is too long."; }
        else if (RomLoad(path, &rom, &why)) { g_SfxLoaded = SfxCatalogReadRom(&rom, &g_SfxCatalog, &why); }
    }
    lstrcpyn(g_SfxError, why, sizeof(g_SfxError));
    RomFree(&rom);
    SetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_FILTER, "");
    ProjectSettingsPopulate();
}

static INT_PTR CALLBACK ProjectSettingsDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG: return TRUE;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) { ProjectSettingsLayout(hwnd); }
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = {0, 0, 430, 260};
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case WM_NOTIFY:
        if (((NMHDR *)lparam)->idFrom == IDC_PROJECT_SETTINGS_TABS && ((NMHDR *)lparam)->code == TCN_SELCHANGE)
        { ProjectSettingsLayout(hwnd); return TRUE; }
        break;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_PROJECT_SFX_FILTER && HIWORD(wparam) == EN_CHANGE && g_ProjectSettings)
        { ProjectSettingsPopulate(); return TRUE; }
        if (LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDOK)
        { DestroyWindow(hwnd); return TRUE; }
        break;
    case WM_CLOSE: DestroyWindow(hwnd); return TRUE;
    case WM_NCDESTROY:
        SfxCatalogFree(&g_SfxCatalog); g_SfxLoaded = FALSE; g_ProjectSettings = NULL;
        break;
    }
    return FALSE;
}

BOOL ProjectSettingsShow(HWND owner, HINSTANCE instance, const char *projectdir)
{
    if (!g_ProjectSettings)
    {
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        g_ProjectSettings = CreateDialog(instance, MAKEINTRESOURCE(IDD_PROJECT_SETTINGS), owner, ProjectSettingsDialogProc);
        if (!g_ProjectSettings) { return FALSE; }
        const char *tabs[] = {"SFX", "Music"};
        for (int i = 0; i < 2; i++)
        {
            TCITEM item = {0}; item.mask = TCIF_TEXT; item.pszText = (char *)tabs[i];
            if (TabCtrl_InsertItem(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SETTINGS_TABS), i, &item) < 0)
            { DestroyWindow(g_ProjectSettings); return FALSE; }
        }
        HWND list = GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_LIST);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        const char *columns[] = {"ID", "Name", "Format", "Sample bytes"};
        for (int i = 0; i < 4; i++)
        {
            LVCOLUMN column = {0}; column.mask = LVCF_TEXT | LVCF_FMT;
            column.pszText = (char *)columns[i]; column.fmt = i == 3 ? LVCFMT_RIGHT : LVCFMT_LEFT;
            if (ListView_InsertColumn(list, i, &column) < 0) { DestroyWindow(g_ProjectSettings); return FALSE; }
        }
        SendDlgItemMessage(g_ProjectSettings, IDC_PROJECT_SFX_FILTER, EM_SETLIMITTEXT, 127, 0);
        TabCtrl_SetCurSel(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SETTINGS_TABS), 0);
    }
    ProjectSettingsRefresh(projectdir);
    ProjectSettingsLayout(g_ProjectSettings);
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
