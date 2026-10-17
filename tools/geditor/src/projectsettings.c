#include "projectsettings.h"
#include "resource.h"
#include "sfxcatalog.h"
#include "sfxplayer.h"
#include "editorpath.h"
#include <commctrl.h>
#include <stdio.h>
#include <windowsx.h>

static HWND g_ProjectSettings;
static SfxCatalog g_SfxCatalog;
static BOOL g_SfxLoaded;
static char g_SfxError[384];
static RomFile g_SfxRom;
static DWORD g_PlayingSfx;
static BOOL g_PreviewLimited;
#define SFX_PLAYBACK_TIMER 1
static const GEditorProject *g_SettingsProject;
static BOOL g_LevelDragging;
static int g_LevelDragStage, g_LevelDropSlot = -1;

static void ProjectSettingsEndLevelDrag(void)
{
    g_LevelDragging = FALSE;
    if (g_LevelDropSlot >= 0)
        SendDlgItemMessage(g_ProjectSettings, IDC_PROJECT_LEVEL_SLOT_FIRST + g_LevelDropSlot, BM_SETSTATE, FALSE, 0);
    g_LevelDropSlot = -1;
    if (GetCapture() == g_ProjectSettings) ReleaseCapture();
}
static BOOL ProjectSettingsSelectedLevel(int *stage)
{
    HWND list = GetDlgItem(g_ProjectSettings, IDC_PROJECT_LEVEL_LIST);
    LVITEM item = {0}; item.mask = LVIF_PARAM;
    item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if (item.iItem < 0 || !ListView_GetItem(list, &item)) return FALSE;
    *stage = (int)item.lParam; return TRUE;
}
static void ProjectSettingsPopulateSlots(void)
{
    char text[96];
    BOOL ready = g_SettingsProject && g_SettingsProject->name[0] && g_SettingsProject->levelSlots.available;
    for (unsigned i = 0; i < LEVEL_MENU_SLOT_COUNT; i++)
    {
        const char *name = "No level";
        int stage = g_SettingsProject ? LevelSlotsGet(&g_SettingsProject->levelSlots, &g_SettingsProject->levelSlotOverrides, i) : LEVELID_NONE;
        if (stage == LEVELID_NONE && i >= LEVEL_MENU_SOLO_COUNT) name = "Random";
        if (g_SettingsProject) for (DWORD l = 0; l < g_SettingsProject->levelcount; l++)
            if (g_SettingsProject->levels[l].levelID == stage) { name = g_SettingsProject->levels[l].name; break; }
        snprintf(text, sizeof(text), "%u. %s", i < LEVEL_MENU_SOLO_COUNT ? i + 1 : i - LEVEL_MENU_SOLO_COUNT + 1, name);
        SetDlgItemText(g_ProjectSettings, IDC_PROJECT_LEVEL_SLOT_FIRST + i, text);
        EnableWindow(GetDlgItem(g_ProjectSettings, IDC_PROJECT_LEVEL_SLOT_FIRST + i), ready);
    }
    SetDlgItemText(g_ProjectSettings, IDC_PROJECT_LEVEL_STATUS, ready
        ? "Drag a level onto a box, or select a level and click a box. Use Save Project to save assignments."
        : g_SettingsProject && g_SettingsProject->name[0]
            ? "Rebuild GUD and rebase this project to enable Level Manager assignments."
            : "Open a project to manage its level menus.");
}
static void ProjectSettingsPopulateLevels(void)
{
    HWND list = GetDlgItem(g_ProjectSettings, IDC_PROJECT_LEVEL_LIST);
    ListView_DeleteAllItems(list);
    if (g_SettingsProject && g_SettingsProject->name[0])
    {
        for (DWORD i = 0; i <= g_SettingsProject->levelcount; i++)
        {
            LVITEM item = {0}; char id[24];
            item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = (int)i;
            item.pszText = i == g_SettingsProject->levelcount ? "Random (multiplayer)" : (char *)g_SettingsProject->levels[i].name;
            item.lParam = i == g_SettingsProject->levelcount ? LEVELID_NONE : g_SettingsProject->levels[i].levelID;
            int row = ListView_InsertItem(list, &item);
            snprintf(id, sizeof(id), "%ld", (long)item.lParam);
            if (row >= 0) ListView_SetItemText(list, row, 1, id);
        }
    }
    ProjectSettingsPopulateSlots();
}
static void ProjectSettingsAssignSlot(unsigned slot, int stage)
{
    ProjectLevelSlotEdit edit = {slot, stage, ""};
    if (!SendMessage(GetParent(g_ProjectSettings), PROJECTSETTINGS_WM_LEVEL_SLOT, 0, (LPARAM)&edit))
        MessageBox(g_ProjectSettings, edit.why[0] ? edit.why : "The level could not be assigned.", "Level Manager", MB_ICONWARNING);
    ProjectSettingsPopulateSlots();
}
static int ProjectSettingsSlotAt(HWND hwnd, int x, int y)
{
    POINT point = {x, y}; ClientToScreen(hwnd, &point);
    for (int i = 0; i < LEVEL_MENU_SLOT_COUNT; i++)
    {
        HWND box = GetDlgItem(hwnd, IDC_PROJECT_LEVEL_SLOT_FIRST + i); RECT bounds;
        GetWindowRect(box, &bounds);
        if (IsWindowVisible(box) && IsWindowEnabled(box) && PtInRect(&bounds, point)) return i;
    }
    return -1;
}

static DWORD ProjectSettingsSelectedSound(void)
{
    HWND list = GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_LIST);
    LVITEM item = {0}; item.mask = LVIF_PARAM;
    item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    return item.iItem >= 0 && ListView_GetItem(list, &item)
        && item.lParam > 0 && (DWORD)item.lParam <= g_SfxCatalog.count ? (DWORD)item.lParam : 0;
}

static void ProjectSettingsPlaybackStatus(void)
{
    char text[256];
    BOOL playing = SfxPlayerIsPlaying();
    EnableWindow(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_PLAY), g_SfxLoaded && ProjectSettingsSelectedSound());
    EnableWindow(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_STOP), SfxPlayerHasOutput());
    if (g_SfxError[0]) { SetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_STATUS, g_SfxError); }
    else
    {
        if (playing)
        {
            snprintf(text, sizeof(text), "Playing: %s%s", SfxCatalogName(g_PlayingSfx),
                g_PreviewLimited ? " (limited preview)" : "");
        }
        else
        {
            snprintf(text, sizeof(text), "%d of %lu sound effects", ListView_GetItemCount(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_LIST)),
                (unsigned long)g_SfxCatalog.count);
        }
        SetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_STATUS, text);
    }
}

static BOOL ProjectSettingsStopSound(void)
{
    const char *why = "";
    if (!SfxPlayerStop(&why))
    {
        SetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_STATUS, why);
        return FALSE;
    }
    KillTimer(g_ProjectSettings, SFX_PLAYBACK_TIMER);
    g_PlayingSfx = 0;
    ProjectSettingsPlaybackStatus();
    return TRUE;
}

static void ProjectSettingsPlaySound(void)
{
    DWORD id = ProjectSettingsSelectedSound();
    SfxPcm pcm = {0};
    const char *why = "";
    if (!id || !ProjectSettingsStopSound()) { return; }
    if (!SfxPreviewRender(&g_SfxRom, &g_SfxCatalog, id, &pcm, &why)) { goto fail; }
    g_PreviewLimited = pcm.limited;
    if (!SfxPlayerPlay(&pcm, &why)) { goto fail; }
    g_PlayingSfx = id;
    if (!SetTimer(g_ProjectSettings, SFX_PLAYBACK_TIMER, 100, NULL))
    { ProjectSettingsStopSound(); why = "Could not monitor sound preview playback."; goto fail; }
    ProjectSettingsPlaybackStatus();
    return;
fail:
    SfxPcmFree(&pcm);
    MessageBox(g_ProjectSettings, why, "Sound preview", MB_ICONERROR);
    ProjectSettingsPlaybackStatus();
}

static void ProjectSettingsLayout(HWND hwnd)
{
    RECT client, units = {10, 10, 70, 18}, page;
    HWND tab = GetDlgItem(hwnd, IDC_PROJECT_SETTINGS_TABS);
    GetClientRect(hwnd, &client); MapDialogRect(hwnd, &units);
    int footer = max(units.top, client.bottom - units.bottom - units.top);
    /* The tab's background sits behind its sibling page controls. The
     * resource's WS_CLIPSIBLINGS prevents themed repaints from erasing them. */
    SetWindowPos(tab, HWND_BOTTOM, units.left, units.top,
        max(0, client.right - units.left * 2), max(0, footer - units.top * 2), SWP_NOACTIVATE);
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
    MoveWindow(list, page.left, page.top + line + gap, width, max(0, page.bottom - page.top - line * 4 - gap * 3), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_SFX_PLAY), page.left, page.bottom - line * 3 - gap, units.right, line, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_SFX_STOP), page.left + units.right + gap, page.bottom - line * 3 - gap, units.right, line, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_SFX_STATUS), page.left, page.bottom - line * 2, width, line * 2, TRUE);
    int available = max(1, width - GetSystemMetrics(SM_CXVSCROLL) - 8);
    ListView_SetColumnWidth(list, 0, units.right * 3 / 4);
    ListView_SetColumnWidth(list, 1, max(units.right * 2, available - units.right * 11 / 4));
    ListView_SetColumnWidth(list, 2, units.right);
    ListView_SetColumnWidth(list, 3, units.right);
    BOOL sfx = TabCtrl_GetCurSel(tab) == 1;
    for (int id = IDC_PROJECT_SFX_FILTER_LABEL; id <= IDC_PROJECT_SFX_STOP; id++)
    { ShowWindow(GetDlgItem(hwnd, id), sfx ? SW_SHOW : SW_HIDE); }
    BOOL levels = TabCtrl_GetCurSel(tab) == 0;
    int leftwidth = width * 28 / 100, gridleft = page.left + leftwidth + gap;
    int gridwidth = page.right - gridleft;
    int rowheight = max(line, (page.bottom - page.top - line * 4 - gap * 10) / 7);
    int multiTop = page.top + line + gap + 4 * (rowheight + gap);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_LEVEL_LABEL), page.left, page.top, leftwidth, line, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_LEVEL_LIST), page.left, page.top + line + gap, leftwidth,
        max(0, page.bottom - page.top - line * 3 - gap), TRUE);
    ListView_SetColumnWidth(GetDlgItem(hwnd, IDC_PROJECT_LEVEL_LIST), 0, max(1, leftwidth - 48 - GetSystemMetrics(SM_CXVSCROLL)));
    ListView_SetColumnWidth(GetDlgItem(hwnd, IDC_PROJECT_LEVEL_LIST), 1, 48);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_SOLO_LABEL), gridleft, page.top, gridwidth, line, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_MULTI_LABEL), gridleft, multiTop, gridwidth, line, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PROJECT_LEVEL_STATUS), page.left, page.bottom - line * 2, width, line * 2, TRUE);
    for (int id = IDC_PROJECT_LEVEL_LIST; id <= IDC_PROJECT_LEVEL_STATUS; id++)
        ShowWindow(GetDlgItem(hwnd, id), levels ? SW_SHOW : SW_HIDE);
    for (int i = 0; i < LEVEL_MENU_SLOT_COUNT; i++)
    {
        int slot = i < LEVEL_MENU_SOLO_COUNT ? i : i - LEVEL_MENU_SOLO_COUNT;
        int columns = i < LEVEL_MENU_SOLO_COUNT ? 5 : 4;
        int top = i < LEVEL_MENU_SOLO_COUNT ? page.top : multiTop;
        int cellwidth = (gridwidth - gap * (columns - 1)) / columns;
        HWND box = GetDlgItem(hwnd, IDC_PROJECT_LEVEL_SLOT_FIRST + i);
        MoveWindow(box, gridleft + (slot % columns) * (cellwidth + gap),
            top + line + gap + (slot / columns) * (rowheight + gap), cellwidth, rowheight, TRUE);
        ShowWindow(box, levels ? SW_SHOW : SW_HIDE);
    }
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
        item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = (int)shown; item.pszText = text; item.lParam = entry->id;
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
    EnableWindow(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_FILTER), g_SfxLoaded);
    EnableWindow(list, g_SfxLoaded);
    ProjectSettingsPlaybackStatus();
}

void ProjectSettingsRefresh(const GEditorProject *project)
{
    if (!g_ProjectSettings) { return; }
    ProjectSettingsEndLevelDrag();
    g_SettingsProject = project;
    const char *projectdir = project && project->name[0] ? project->dir : NULL;
    ProjectSettingsPopulateLevels();
    char path[MAX_PATH];
    const char *why = "Open a project to list its sound effects.";
    ProjectSettingsStopSound();
    RomFree(&g_SfxRom);
    SfxCatalogFree(&g_SfxCatalog);
    g_SfxLoaded = FALSE;
    if (projectdir && projectdir[0])
    {
        if (!EditorPathJoin(path, sizeof(path), projectdir, "base.z64")) { why = "The project ROM path is too long."; }
        else if (RomLoad(path, &g_SfxRom, &why)) { g_SfxLoaded = SfxCatalogReadRom(&g_SfxRom, &g_SfxCatalog, &why); }
    }
    lstrcpyn(g_SfxError, why, sizeof(g_SfxError));
    if (!g_SfxLoaded) { RomFree(&g_SfxRom); }
    SetDlgItemText(g_ProjectSettings, IDC_PROJECT_SFX_FILTER, "");
    ProjectSettingsPopulate();
}

static INT_PTR CALLBACK ProjectSettingsDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG: return TRUE;
    case WM_TIMER:
        if (wparam == SFX_PLAYBACK_TIMER && !SfxPlayerIsPlaying()) { ProjectSettingsStopSound(); }
        return TRUE;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) { ProjectSettingsLayout(hwnd); }
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = {0, 0, 560, 360};
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case WM_NOTIFY:
        if (((NMHDR *)lparam)->idFrom == IDC_PROJECT_SETTINGS_TABS && ((NMHDR *)lparam)->code == TCN_SELCHANGE)
        { ProjectSettingsEndLevelDrag(); ProjectSettingsStopSound(); ProjectSettingsLayout(hwnd); return TRUE; }
        if (((NMHDR *)lparam)->idFrom == IDC_PROJECT_LEVEL_LIST && ((NMHDR *)lparam)->code == LVN_BEGINDRAG)
        {
            if (g_SettingsProject && g_SettingsProject->levelSlots.available && ProjectSettingsSelectedLevel(&g_LevelDragStage))
            {
                ProjectSettingsEndLevelDrag(); SetCapture(hwnd); g_LevelDragging = GetCapture() == hwnd;
            }
            return TRUE;
        }
        if (((NMHDR *)lparam)->idFrom == IDC_PROJECT_SFX_LIST)
        {
            if (((NMHDR *)lparam)->code == LVN_ITEMCHANGED) { ProjectSettingsPlaybackStatus(); }
            if (((NMHDR *)lparam)->code == NM_DBLCLK && ((NMITEMACTIVATE *)lparam)->iItem >= 0)
            { ProjectSettingsPlaySound(); return TRUE; }
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wparam) >= IDC_PROJECT_LEVEL_SLOT_FIRST
            && LOWORD(wparam) < IDC_PROJECT_LEVEL_SLOT_FIRST + LEVEL_MENU_SLOT_COUNT && HIWORD(wparam) == BN_CLICKED)
        {
            int stage;
            if (ProjectSettingsSelectedLevel(&stage)) ProjectSettingsAssignSlot(LOWORD(wparam) - IDC_PROJECT_LEVEL_SLOT_FIRST, stage);
            return TRUE;
        }
        if (LOWORD(wparam) == IDC_PROJECT_SFX_FILTER && HIWORD(wparam) == EN_CHANGE && g_ProjectSettings)
        { ProjectSettingsStopSound(); ProjectSettingsPopulate(); return TRUE; }
        if (LOWORD(wparam) == IDC_PROJECT_SFX_PLAY) { ProjectSettingsPlaySound(); return TRUE; }
        if (LOWORD(wparam) == IDC_PROJECT_SFX_STOP) { ProjectSettingsStopSound(); return TRUE; }
        if (LOWORD(wparam) == IDOK && GetFocus() == GetDlgItem(hwnd, IDC_PROJECT_SFX_LIST))
        { ProjectSettingsPlaySound(); return TRUE; }
        if (LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDOK)
        { if (ProjectSettingsStopSound()) { DestroyWindow(hwnd); } return TRUE; }
        break;
    case WM_MOUSEMOVE:
        if (g_LevelDragging)
        {
            if (!(wparam & MK_LBUTTON)) { ProjectSettingsEndLevelDrag(); return TRUE; }
            int slot = ProjectSettingsSlotAt(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            if (g_LevelDropSlot >= 0) SendDlgItemMessage(hwnd, IDC_PROJECT_LEVEL_SLOT_FIRST + g_LevelDropSlot, BM_SETSTATE, FALSE, 0);
            g_LevelDropSlot = slot;
            if (slot >= 0) SendDlgItemMessage(hwnd, IDC_PROJECT_LEVEL_SLOT_FIRST + slot, BM_SETSTATE, TRUE, 0);
            SetCursor(LoadCursor(NULL, slot >= 0 ? IDC_HAND : IDC_NO));
            return TRUE;
        }
        break;
    case WM_LBUTTONUP:
        if (g_LevelDragging)
        {
            int slot = ProjectSettingsSlotAt(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            int stage = g_LevelDragStage; ProjectSettingsEndLevelDrag();
            if (slot >= 0) ProjectSettingsAssignSlot((unsigned)slot, stage);
            return TRUE;
        }
        break;
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED: ProjectSettingsEndLevelDrag(); break;
    case WM_CLOSE: if (ProjectSettingsStopSound()) { DestroyWindow(hwnd); } return TRUE;
    case WM_NCDESTROY:
        ProjectSettingsEndLevelDrag(); g_SettingsProject = NULL;
        SfxPlayerStop(NULL); KillTimer(hwnd, SFX_PLAYBACK_TIMER); g_PlayingSfx = 0;
        RomFree(&g_SfxRom);
        SfxCatalogFree(&g_SfxCatalog); g_SfxLoaded = FALSE; g_ProjectSettings = NULL;
        break;
    }
    return FALSE;
}

BOOL ProjectSettingsShow(HWND owner, HINSTANCE instance, const GEditorProject *project)
{
    if (!g_ProjectSettings)
    {
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        g_ProjectSettings = CreateDialog(instance, MAKEINTRESOURCE(IDD_PROJECT_SETTINGS), owner, ProjectSettingsDialogProc);
        if (!g_ProjectSettings) { return FALSE; }
        const char *tabs[] = {"Level Manager", "SFX", "Music"};
        for (int i = 0; i < 3; i++)
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
        list = GetDlgItem(g_ProjectSettings, IDC_PROJECT_LEVEL_LIST);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        for (int i = 0; i < 2; i++)
        {
            LVCOLUMN column = {0}; column.mask = LVCF_TEXT; column.pszText = i ? "ID" : "Level";
            if (ListView_InsertColumn(list, i, &column) < 0) { DestroyWindow(g_ProjectSettings); return FALSE; }
        }
        for (int i = 0; i < LEVEL_MENU_SLOT_COUNT; i++)
        {
            HWND box = CreateWindowEx(0, "BUTTON", "", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | BS_MULTILINE,
                0, 0, 0, 0, g_ProjectSettings, (HMENU)(INT_PTR)(IDC_PROJECT_LEVEL_SLOT_FIRST + i), instance, NULL);
            if (!box) { DestroyWindow(g_ProjectSettings); return FALSE; }
            SendMessage(box, WM_SETFONT, SendMessage(g_ProjectSettings, WM_GETFONT, 0, 0), FALSE);
        }
        TabCtrl_SetCurSel(GetDlgItem(g_ProjectSettings, IDC_PROJECT_SETTINGS_TABS), 0);
    }
    ProjectSettingsRefresh(project);
    ProjectSettingsLayout(g_ProjectSettings);
    ShowWindow(g_ProjectSettings, SW_SHOWNORMAL);
    SetForegroundWindow(g_ProjectSettings);
    return TRUE;
}

BOOL ProjectSettingsHandleMessage(MSG *message)
{
    if (!g_ProjectSettings || !message
        || (message->hwnd != g_ProjectSettings && !IsChild(g_ProjectSettings, message->hwnd))) { return FALSE; }
    if (message->message == WM_KEYDOWN && message->wParam == VK_ESCAPE && g_LevelDragging)
    { ProjectSettingsEndLevelDrag(); return TRUE; }
    if (message->message == WM_KEYDOWN && message->wParam == 'S'
        && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) return FALSE;
    /* The dialog's default button is Close; Enter in the sound list previews
     * the selected ID instead of invoking that default button. */
    if (message->message == WM_KEYDOWN && message->wParam == VK_RETURN
        && message->hwnd == GetDlgItem(g_ProjectSettings, IDC_PROJECT_SFX_LIST))
    { ProjectSettingsPlaySound(); return TRUE; }
    /* Dialog navigation and keys stay here instead of reaching editor tools. */
    if (!IsDialogMessage(g_ProjectSettings, message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
