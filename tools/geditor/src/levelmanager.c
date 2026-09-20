#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "levelmanager.h"
#include "resource.h"

static HWND g_LevelManager;
static HWND g_EnvironmentPanel;
static SetupIntroEntry *g_IntroEntries;
static DWORD g_IntroCount;
static BOOL g_IntroValid, g_IntroUpdating;
static unsigned g_IntroGeneration;
static char g_IntroSetupName[64];
static RoomStats g_RoomStats;
static int g_RoomSortColumn;
static BOOL g_RoomSortDescending;
static void LevelManagerLayout(HWND hwnd);

void LevelManagerRefreshEnvironment(const GEditorProject *project, DWORD level)
{ EnvironmentPanelRefresh(g_EnvironmentPanel, project, level); }
BOOL LevelManagerApplyEnvironment(void) { return EnvironmentPanelApply(g_EnvironmentPanel); }
BOOL LevelManagerHasEnvironmentDraft(void) { return EnvironmentPanelHasDraft(g_EnvironmentPanel); }

static const char *g_IntroAmmoNames[AMMOTYPE_MAX] = {
    [AMMO_9MM] = "9mm", [AMMO_RIFLE] = "Rifle rounds", [AMMO_SHOTGUN] = "Shotgun shells",
    [AMMO_GRENADE] = "Grenades", [AMMO_ROCKETS] = "Rockets", [AMMO_REMOTEMINE] = "Remote mines",
    [AMMO_PROXMINE] = "Proximity mines", [AMMO_TIMEDMINE] = "Timed mines", [AMMO_KNIFE] = "Throwing knives",
    [AMMO_GRENADEROUND] = "Grenade rounds", [AMMO_MAGNUM] = "Magnum rounds", [AMMO_GGUN] = "Golden Gun rounds",
    [AMMO_DARTS] = "Darts", [AMMO_EXPLOSIVEPEN] = "Explosive pen", [AMMO_BOMBCASE] = "Bomb case",
    [AMMO_FLARE] = "Flares", [AMMO_PITON] = "Pitons", [AMMO_DYNAMITE] = "Dynamite", [AMMO_BUG] = "Bugs",
    [AMMO_MICRO_CAMERA] = "Micro camera", [AMMO_GEKEY] = "GoldenEye key", [AMMO_PLASTIQUE] = "Plastique",
    [AMMO_WATCH_LASER] = "Watch laser", [AMMO_WATCH_MAGNET] = "Watch magnet", [AMMO_UNK] = "Unknown ammo (26)",
    [AMMO_CAMERA] = "Camera", [AMMO_TANK] = "Tank shells", [AMMO_TOKEN] = "Token"
};

static int LevelManagerSelectedEntry(HWND list)
{
    int row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    LVITEM item = {0}; item.mask = LVIF_PARAM; item.iItem = row;
    return row >= 0 && ListView_GetItem(list, &item) && (DWORD)item.lParam < g_IntroCount
        ? (int)item.lParam : -1;
}

static void LevelManagerButtons(HWND hwnd)
{
    for (int table = 0; table < 2; table++)
    {
        HWND list = GetDlgItem(hwnd, table ? IDC_INTRO_AMMO : IDC_INTRO_WEAPONS);
        int row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        int first = table ? IDC_INTRO_AMMO_ADD : IDC_INTRO_WEAPON_ADD;
        EnableWindow(list, g_IntroValid);
        EnableWindow(GetDlgItem(hwnd, first), g_IntroValid);
        EnableWindow(GetDlgItem(hwnd, first + 1), g_IntroValid && row >= 0);
        EnableWindow(GetDlgItem(hwnd, first + 2), g_IntroValid && row >= 0);
        EnableWindow(GetDlgItem(hwnd, first + 3), g_IntroValid && row > 0);
        EnableWindow(GetDlgItem(hwnd, first + 4), g_IntroValid && row >= 0 && row + 1 < ListView_GetItemCount(list));
    }
}

static int CALLBACK LevelManagerCompareRooms(LPARAM left, LPARAM right, LPARAM unused)
{
    DWORD a = (DWORD)left, b = (DWORD)right;
    if (!a || !b) { return !a ? (!b ? 0 : 1) : -1; } /* Unassigned stays last. */
    DWORD x = a, y = b;
    if (g_RoomSortColumn)
    {
        x = g_RoomStats.rooms[a].values[g_RoomSortColumn - 1];
        y = g_RoomStats.rooms[b].values[g_RoomSortColumn - 1];
    }
    int order = x < y ? -1 : x > y;
    if (order) { return g_RoomSortDescending ? -order : order; }
    return a < b ? -1 : a > b;
}

static void LevelManagerSortRooms(HWND hwnd)
{
    HWND list = GetDlgItem(hwnd, IDC_LEVEL_ROOMS_LIST);
    ListView_SortItems(list, LevelManagerCompareRooms, 0);
    HWND header = ListView_GetHeader(list);
    for (int i = 0; i <= ROOM_COUNT_COLUMNS; i++)
    {
        HDITEM item = {0}; item.mask = HDI_FORMAT;
        if (Header_GetItem(header, i, &item))
        {
            item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
            if (i == g_RoomSortColumn) { item.fmt |= g_RoomSortDescending ? HDF_SORTDOWN : HDF_SORTUP; }
            Header_SetItem(header, i, &item);
        }
    }
}

void LevelManagerRefreshRooms(const BgDocument *bg, const SetupFile *setup, const StanFile *stan)
{
    RoomStats counts = {0};
    const char *why;
    char text[384];
    if (!g_LevelManager) { return; }
    HWND list = GetDlgItem(g_LevelManager, IDC_LEVEL_ROOMS_LIST);
    if (!RoomStatsBuild(bg, setup, stan, &counts, &why))
    {
        ListView_DeleteAllItems(list); RoomStatsFree(&g_RoomStats);
        SetDlgItemText(g_LevelManager, IDC_LEVEL_ROOMS_STATUS, why);
        SetDlgItemText(g_LevelManager, IDC_LEVEL_ROOMS_TOTALS, "");
        LevelManagerLayout(g_LevelManager);
        return;
    }
    if (g_RoomStats.rooms && counts.roomcount == g_RoomStats.roomcount
        && !memcmp(counts.rooms, g_RoomStats.rooms, ((size_t)counts.roomcount + 1) * sizeof(*counts.rooms)))
    { RoomStatsFree(&counts); return; }
    LVITEM selected = {0}; selected.mask = LVIF_PARAM;
    selected.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    BOOL keepselection = selected.iItem >= 0 && ListView_GetItem(list, &selected);
    int top = ListView_GetTopIndex(list);
    BOOL visible = IsWindowVisible(list);
    if (visible) { SendMessage(list, WM_SETREDRAW, FALSE, 0); }
    ListView_DeleteAllItems(list);
    RoomStatsFree(&g_RoomStats); g_RoomStats = counts;
    /* List every BG room, including empty ones. Room zero is reserved for
     * placements / tiles without a valid BG room, and only appears if needed. */
    for (DWORD row = 1; row <= counts.roomcount + 1; row++)
    {
        DWORD room = row <= counts.roomcount ? row : 0;
        if (!room)
        {
            BOOL nonzero = FALSE;
            for (int c = 0; c < ROOM_COUNT_COLUMNS; c++) { nonzero |= counts.rooms[0].values[c] != 0; }
            if (!nonzero) { continue; }
        }
        if (room) { snprintf(text, sizeof(text), "%lu", (unsigned long)room); }
        else { strcpy(text, "Unassigned"); }
        LVITEM item = {0}; item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(list); item.pszText = text; item.lParam = room;
        int at = ListView_InsertItem(list, &item);
        if (at < 0)
        {
            ListView_DeleteAllItems(list); RoomStatsFree(&g_RoomStats);
            if (visible) { SendMessage(list, WM_SETREDRAW, TRUE, 0); }
            SetDlgItemText(g_LevelManager, IDC_LEVEL_ROOMS_STATUS, "Out of memory displaying room counts.");
            SetDlgItemText(g_LevelManager, IDC_LEVEL_ROOMS_TOTALS, "");
            LevelManagerLayout(g_LevelManager);
            return;
        }
        for (int c = 0; c < ROOM_COUNT_COLUMNS; c++)
        {
            snprintf(text, sizeof(text), "%lu", (unsigned long)counts.rooms[room].values[c]);
            ListView_SetItemText(list, at, c + 1, text);
        }
        if (keepselection && (DWORD)selected.lParam == room)
        { ListView_SetItemState(list, at, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); }
    }
    LevelManagerSortRooms(g_LevelManager);
    if (ListView_GetItemCount(list)) { ListView_EnsureVisible(list, min(top, ListView_GetItemCount(list) - 1), FALSE); }
    if (visible) { SendMessage(list, WM_SETREDRAW, TRUE, 0); }
    InvalidateRect(list, NULL, TRUE);
    SetDlgItemText(g_LevelManager, IDC_LEVEL_ROOMS_STATUS, "");
    snprintf(text, sizeof(text), "%lu rooms. Totals - Primary tris: %lu   Secondary tris: %lu   STAN tiles: %lu   Objects: %lu   Characters: %lu",
        (unsigned long)counts.roomcount, (unsigned long)counts.total.values[ROOM_PRIMARY_TRIS],
        (unsigned long)counts.total.values[ROOM_SECONDARY_TRIS], (unsigned long)counts.total.values[ROOM_STAN_TILES],
        (unsigned long)counts.total.values[ROOM_OBJECTS], (unsigned long)counts.total.values[ROOM_CHARACTERS]);
    SetDlgItemText(g_LevelManager, IDC_LEVEL_ROOMS_TOTALS, text);
    LevelManagerLayout(g_LevelManager);
}

static void LevelManagerLayout(HWND hwnd)
{
    RECT client, units = {10, 10, 70, 18}, page;
    HWND tab = GetDlgItem(hwnd, IDC_LEVEL_MANAGER_TABS);
    int footer;
    GetClientRect(hwnd, &client); MapDialogRect(hwnd, &units);
    footer = max(units.top, client.bottom - units.bottom - units.top);
    MoveWindow(tab, units.left, units.top, max(0, client.right - units.left * 2), max(0, footer - units.top * 2), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDCANCEL), max(units.left, client.right - units.right - units.left),
               footer, units.right, units.bottom, TRUE);
    GetClientRect(tab, &page); TabCtrl_AdjustRect(tab, FALSE, &page);
    MapWindowPoints(tab, hwnd, (POINT *)&page, 2);
    InflateRect(&page, -units.left / 2, -units.top / 2);
    int width = max(1, page.right - page.left), gap = max(4, units.top / 2);
    int line = units.bottom, y = page.top;
    MoveWindow(GetDlgItem(hwnd, IDC_INTRO_STATUS), page.left, y, width, line, TRUE); y += line + gap;
    int section = max(1, (page.bottom - y - gap) / 2);
    for (int table = 0; table < 2; table++)
    {
        int label = table ? IDC_INTRO_AMMO_LABEL : IDC_INTRO_WEAPONS_LABEL;
        int listid = table ? IDC_INTRO_AMMO : IDC_INTRO_WEAPONS;
        int button = table ? IDC_INTRO_AMMO_ADD : IDC_INTRO_WEAPON_ADD;
        int listheight = max(line * 2, section - line * 3 - gap * 3);
        MoveWindow(GetDlgItem(hwnd, label), page.left, y, width, line, TRUE);
        MoveWindow(GetDlgItem(hwnd, label + 1), page.left, y + line, width, line, TRUE);
        HWND list = GetDlgItem(hwnd, listid);
        MoveWindow(list, page.left, y + line * 2 + gap, width, listheight, TRUE);
        int buttony = y + line * 2 + gap * 2 + listheight;
        for (int i = 0; i < 5; i++)
        { MoveWindow(GetDlgItem(hwnd, button + i), page.left + i * (units.right + gap), buttony, units.right, line, TRUE); }
        int available = max(1, width - GetSystemMetrics(SM_CXVSCROLL) - 8);
        int order = units.right / 2;
        ListView_SetColumnWidth(list, 0, order);
        ListView_SetColumnWidth(list, 1, (available - order) * (table ? 2 : 1) / (table ? 3 : 2));
        ListView_SetColumnWidth(list, 2, (available - order) / (table ? 3 : 2));
        y += section + gap;
    }
    BOOL intro = TabCtrl_GetCurSel(tab) == 1;
    for (int id = IDC_INTRO_STATUS; id <= IDC_INTRO_AMMO_DOWN; id++)
    { ShowWindow(GetDlgItem(hwnd, id), intro ? SW_SHOW : SW_HIDE); }
    BOOL rooms = TabCtrl_GetCurSel(tab) == 3;
    int helpheight = GetWindowTextLength(GetDlgItem(hwnd, IDC_LEVEL_ROOMS_STATUS)) ? line * 2 : 0;
    int totalsheight = line * 2, statusgap = helpheight ? gap : 0;
    HWND roomlist = GetDlgItem(hwnd, IDC_LEVEL_ROOMS_LIST);
    MoveWindow(GetDlgItem(hwnd, IDC_LEVEL_ROOMS_STATUS), page.left, page.top, width, helpheight, TRUE);
    MoveWindow(roomlist, page.left, page.top + helpheight + statusgap, width,
        max(1, page.bottom - page.top - helpheight - totalsheight - statusgap - gap), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_LEVEL_ROOMS_TOTALS), page.left, page.bottom - totalsheight, width, totalsheight, TRUE);
    int available = max(1, width - GetSystemMetrics(SM_CXVSCROLL) - 8), roomwidth = units.right;
    ListView_SetColumnWidth(roomlist, 0, roomwidth);
    for (int i = 1; i <= ROOM_COUNT_COLUMNS; i++)
    { ListView_SetColumnWidth(roomlist, i, max(units.right, (available - roomwidth) / ROOM_COUNT_COLUMNS)); }
    ShowWindow(GetDlgItem(hwnd, IDC_LEVEL_ROOMS_STATUS), rooms && helpheight ? SW_SHOW : SW_HIDE);
    ShowWindow(roomlist, rooms ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, IDC_LEVEL_ROOMS_TOTALS), rooms ? SW_SHOW : SW_HIDE);
    if (g_EnvironmentPanel)
    {
        MoveWindow(g_EnvironmentPanel, page.left, page.top, width, max(1, page.bottom - page.top), TRUE);
        EnvironmentPanelShow(g_EnvironmentPanel, TabCtrl_GetCurSel(tab) == 2);
    }
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static void LevelManagerItemText(char *text, size_t size, int item)
{
    const char *name = SetupIntroItemName(item);
    if (name) { snprintf(text, size, "%s", name); }
    else { snprintf(text, size, "Item %d (current)", item); }
}

void LevelManagerRefresh(const SetupFile *setup, const char *levelname)
{
    if (!g_LevelManager) { return; }
    char title[128];
    snprintf(title, sizeof(title), "Level Settings%s%s", levelname ? " - " : "", levelname ? levelname : "");
    SetWindowText(g_LevelManager, title);
    SetupIntroEntry *entries = NULL; DWORD count = 0;
    const char *why = "Open a level with a setup to edit starting equipment.";
    BOOL valid = setup && setup->data && SetupFileGetIntroEquipment(setup, &entries, &count, &why);
    const char *name = setup ? setup->name : "";
    g_IntroGeneration++;
    if (valid && g_IntroValid && !strcmp(name, g_IntroSetupName) && count == g_IntroCount
        && (!count || !memcmp(entries, g_IntroEntries, count * sizeof(*entries))))
    { free(entries); return; }
    BOOL samelevel = !strcmp(name, g_IntroSetupName);
    int selected[2];
    BOOL visible[2];
    g_IntroUpdating = TRUE;
    for (int table = 0; table < 2; table++)
    {
        HWND list = GetDlgItem(g_LevelManager, table ? IDC_INTRO_AMMO : IDC_INTRO_WEAPONS);
        selected[table] = samelevel ? ListView_GetNextItem(list, -1, LVNI_SELECTED) : -1;
        visible[table] = IsWindowVisible(list);
        if (visible[table]) { SendMessage(list, WM_SETREDRAW, FALSE, 0); }
        ListView_DeleteAllItems(list);
    }
    free(g_IntroEntries); g_IntroEntries = entries; g_IntroCount = count; g_IntroValid = valid;
    lstrcpyn(g_IntroSetupName, name, sizeof(g_IntroSetupName));
    char text[192];
    if (valid) { snprintf(text, sizeof(text), "Changes are included when you save the project."); }
    else { snprintf(text, sizeof(text), "%s", why); }
    SetDlgItemText(g_LevelManager, IDC_INTRO_STATUS, text);
    for (DWORD i = 0; i < count; i++)
    {
        const SetupIntroEntry *entry = &entries[i];
        BOOL ammo = entry->type == SETUP_INTRO_AMMO;
        HWND list = GetDlgItem(g_LevelManager, ammo ? IDC_INTRO_AMMO : IDC_INTRO_WEAPONS);
        int row = ListView_GetItemCount(list);
        snprintf(text, sizeof(text), "%d", row + 1);
        LVITEM item = {0}; item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = row; item.pszText = text; item.lParam = i;
        if (ListView_InsertItem(list, &item) < 0) { continue; }
        if (ammo)
        {
            int type = entry->value[0];
            if (type > AMMO_NONE && type < AMMOTYPE_MAX && g_IntroAmmoNames[type])
            { snprintf(text, sizeof(text), "%s", g_IntroAmmoNames[type]); }
            else { snprintf(text, sizeof(text), "Ammo %d (current)", type); }
        }
        else { LevelManagerItemText(text, sizeof(text), entry->value[0]); }
        ListView_SetItemText(list, row, 1, text);
        if (ammo) { snprintf(text, sizeof(text), "%ld", (long)entry->value[1]); }
        else { LevelManagerItemText(text, sizeof(text), entry->value[1]); }
        ListView_SetItemText(list, row, 2, text);
    }
    for (int table = 0; table < 2; table++)
    {
        HWND list = GetDlgItem(g_LevelManager, table ? IDC_INTRO_AMMO : IDC_INTRO_WEAPONS);
        int row = min(selected[table], ListView_GetItemCount(list) - 1);
        if (row >= 0) { ListView_SetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); }
        if (visible[table]) { SendMessage(list, WM_SETREDRAW, TRUE, 0); }
        InvalidateRect(list, NULL, TRUE);
    }
    g_IntroUpdating = FALSE; LevelManagerButtons(g_LevelManager);
}

static void LevelManagerAddChoice(HWND combo, const char *name, int value, int selected)
{
    int row = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)name);
    if (row < 0) { return; }
    SendMessage(combo, CB_SETITEMDATA, row, value);
    if (value == selected) { SendMessage(combo, CB_SETCURSEL, row, 0); }
}

static BOOL LevelManagerParseQuantity(const char *text, LONG *quantity)
{
    char *end; unsigned long value;
    while (isspace((unsigned char)*text)) { text++; }
    if (!isdigit((unsigned char)*text)) { return FALSE; }
    errno = 0; value = strtoul(text, &end, 10);
    while (isspace((unsigned char)*end)) { end++; }
    if (errno == ERANGE || *end || value > 2147483647UL) { return FALSE; }
    *quantity = (LONG)value; return TRUE;
}

static INT_PTR CALLBACK IntroEquipmentDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    SetupIntroEntry *entry = (SetupIntroEntry *)GetWindowLongPtr(hwnd, DWLP_USER);
    if (message == WM_INITDIALOG)
    {
        entry = (SetupIntroEntry *)lparam; SetWindowLongPtr(hwnd, DWLP_USER, lparam);
        BOOL ammo = entry->type == SETUP_INTRO_AMMO;
        SetWindowText(hwnd, ammo ? "Starting Ammo" : "Starting Weapons");
        ShowWindow(GetDlgItem(hwnd, IDC_INTRO_QUANTITY), ammo ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(hwnd, IDC_INTRO_SECOND), ammo ? SW_HIDE : SW_SHOW);
        if (ammo)
        {
            char text[32]; snprintf(text, sizeof(text), "%ld", (long)entry->value[1]);
            SetDlgItemText(hwnd, IDC_INTRO_FIRST_LABEL, "Ammo type");
            SetDlgItemText(hwnd, IDC_INTRO_SECOND_LABEL, "Quantity");
            SetDlgItemText(hwnd, IDC_INTRO_QUANTITY, text);
            SendDlgItemMessage(hwnd, IDC_INTRO_QUANTITY, EM_LIMITTEXT, 10, 0);
            for (int type = 1; type < AMMOTYPE_MAX; type++)
            { if (g_IntroAmmoNames[type]) { LevelManagerAddChoice(GetDlgItem(hwnd, IDC_INTRO_FIRST), g_IntroAmmoNames[type], type, entry->value[0]); } }
        }
        else
        {
            DWORD count; const SetupIntroChoice *choices = SetupIntroItemChoices(&count);
            for (DWORD i = 0; i < count; i++)
            {
                if (choices[i].id >= 0)
                { LevelManagerAddChoice(GetDlgItem(hwnd, IDC_INTRO_FIRST), choices[i].name, choices[i].id, entry->value[0]); }
                LevelManagerAddChoice(GetDlgItem(hwnd, IDC_INTRO_SECOND), choices[i].name, choices[i].id, entry->value[1]);
            }
        }
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == IDCANCEL) { EndDialog(hwnd, IDCANCEL); return TRUE; }
    if (message == WM_CLOSE) { EndDialog(hwnd, IDCANCEL); return TRUE; }
    if (message == WM_COMMAND && LOWORD(wparam) == IDOK && entry)
    {
        HWND first = GetDlgItem(hwnd, IDC_INTRO_FIRST), second = GetDlgItem(hwnd, IDC_INTRO_SECOND);
        int a = (int)SendMessage(first, CB_GETCURSEL, 0, 0);
        LONG value;
        if (a == CB_ERR) { MessageBox(hwnd, "Choose a weapon or ammo type.", "Starting Equipment", MB_ICONERROR); return TRUE; }
        if (entry->type == SETUP_INTRO_AMMO)
        {
            char text[64]; GetDlgItemText(hwnd, IDC_INTRO_QUANTITY, text, sizeof(text));
            if (!LevelManagerParseQuantity(text, &value))
            { MessageBox(hwnd, "Enter a whole-number quantity from 0 to 2147483647.", "Starting Ammo", MB_ICONERROR); SetFocus(GetDlgItem(hwnd, IDC_INTRO_QUANTITY)); return TRUE; }
        }
        else
        {
            int b = (int)SendMessage(second, CB_GETCURSEL, 0, 0);
            if (b == CB_ERR) { MessageBox(hwnd, "Choose a left-hand weapon or None.", "Starting Weapons", MB_ICONERROR); return TRUE; }
            value = (LONG)SendMessage(second, CB_GETITEMDATA, b, 0);
        }
        entry->value[0] = (LONG)SendMessage(first, CB_GETITEMDATA, a, 0); entry->value[1] = value;
        EndDialog(hwnd, IDOK); return TRUE;
    }
    return FALSE;
}

static void LevelManagerEdit(HWND hwnd, BOOL ammo, SetupIntroAction action)
{
    if (!g_IntroValid || g_IntroUpdating) { return; }
    HWND list = GetDlgItem(hwnd, ammo ? IDC_INTRO_AMMO : IDC_INTRO_WEAPONS);
    int index = LevelManagerSelectedEntry(list), row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    DWORD choicecount; const SetupWeaponChoice *choices = SetupWeaponChoices(&choicecount);
    int defaultitem = choicecount > 1 ? choices[1].item : 0;
    SetupIntroEdit edit = {.action = action, .entry = {0, ammo ? SETUP_INTRO_AMMO : SETUP_INTRO_WEAPON, {ammo ? AMMO_9MM : defaultitem, ammo ? 0 : -1}}};
    if (action != SETUP_INTRO_ADD)
    { if (index < 0) { return; } edit.entry = g_IntroEntries[index]; }
    if (action == SETUP_INTRO_ADD || action == SETUP_INTRO_UPDATE)
    {
        unsigned generation = g_IntroGeneration;
        INT_PTR result = DialogBoxParam((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            MAKEINTRESOURCE(IDD_INTRO_EQUIPMENT), hwnd, IntroEquipmentDialogProc, (LPARAM)&edit.entry);
        if (result == -1) { MessageBox(hwnd, "The starting-equipment dialog could not be opened.", "Level Settings", MB_ICONERROR); }
        if (result != IDOK) { return; }
        if (generation != g_IntroGeneration)
        { MessageBox(hwnd, "The level was refreshed while this entry was open. Please edit the entry again.", "Level Settings", MB_ICONINFORMATION); return; }
    }
    if (!SendMessage(GetWindow(hwnd, GW_OWNER), LEVELMANAGER_WM_INTRO_EDIT, 0, (LPARAM)&edit)) { return; }
    if (action == SETUP_INTRO_ADD) { row = ListView_GetItemCount(list) - 1; }
    else if (action == SETUP_INTRO_UP) { row--; }
    else if (action == SETUP_INTRO_DOWN) { row++; }
    row = max(0, min(row, ListView_GetItemCount(list) - 1));
    ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (ListView_GetItemCount(list))
    { ListView_SetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); ListView_EnsureVisible(list, row, FALSE); }
    LevelManagerButtons(hwnd); SetFocus(list);
}

static void LevelManagerFrameRoom(HWND hwnd, int row)
{
    LVITEM item = {0}; item.mask = LVIF_PARAM; item.iItem = row;
    if (row < 0 || !ListView_GetItem(GetDlgItem(hwnd, IDC_LEVEL_ROOMS_LIST), &item)
        || item.lParam <= 0 || (DWORD)item.lParam > g_RoomStats.roomcount) { return; }
    /* Use the stored room ID, since column sorting changes row indices. */
    SendMessage(GetWindow(hwnd, GW_OWNER), LEVELMANAGER_WM_FRAME_ROOM, (WPARAM)item.lParam, 0);
}

static INT_PTR CALLBACK LevelManagerDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG: return TRUE;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) { LevelManagerLayout(hwnd); }
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = {0, 0, 540, 350};
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case WM_NOTIFY:
    {
        NMHDR *notice = (NMHDR *)lparam;
        if (notice->idFrom == IDC_LEVEL_MANAGER_TABS && notice->code == TCN_SELCHANGING)
        { SetWindowLongPtr(hwnd, DWLP_MSGRESULT, !LevelManagerApplyEnvironment()); return TRUE; }
        if (notice->idFrom == IDC_LEVEL_MANAGER_TABS && notice->code == TCN_SELCHANGE) { LevelManagerLayout(hwnd); }
        if (notice->idFrom == IDC_LEVEL_ROOMS_LIST && notice->code == NM_DBLCLK)
        { LevelManagerFrameRoom(hwnd, ((NMITEMACTIVATE *)notice)->iItem); }
        if (notice->idFrom == IDC_LEVEL_ROOMS_LIST && notice->code == LVN_COLUMNCLICK)
        {
            int column = ((NMLISTVIEW *)notice)->iSubItem;
            if (column >= 0 && column <= ROOM_COUNT_COLUMNS)
            {
                g_RoomSortDescending = column == g_RoomSortColumn ? !g_RoomSortDescending : column != 0;
                g_RoomSortColumn = column;
                LevelManagerSortRooms(hwnd);
            }
        }
        if (notice->idFrom == IDC_INTRO_WEAPONS || notice->idFrom == IDC_INTRO_AMMO)
        {
            BOOL ammo = notice->idFrom == IDC_INTRO_AMMO;
            if (notice->code == LVN_ITEMCHANGED && !g_IntroUpdating) { LevelManagerButtons(hwnd); }
            if (notice->code == NM_DBLCLK) { LevelManagerEdit(hwnd, ammo, SETUP_INTRO_UPDATE); }
            if (notice->code == LVN_KEYDOWN && ((NMLVKEYDOWN *)notice)->wVKey == VK_DELETE)
            { LevelManagerEdit(hwnd, ammo, SETUP_INTRO_REMOVE); }
        }
        return TRUE;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wparam);
        if (id == IDCANCEL)
        {
            if (LevelManagerApplyEnvironment()) { EnvironmentPanelShow(g_EnvironmentPanel, FALSE); DestroyWindow(hwnd); }
            return TRUE;
        }
        if (id == IDOK)
        {
            HWND focus = GetFocus();
            if (focus == GetDlgItem(hwnd, IDC_INTRO_WEAPONS) || focus == GetDlgItem(hwnd, IDC_INTRO_AMMO))
            { LevelManagerEdit(hwnd, focus == GetDlgItem(hwnd, IDC_INTRO_AMMO), SETUP_INTRO_UPDATE); }
            if (TabCtrl_GetCurSel(GetDlgItem(hwnd, IDC_LEVEL_MANAGER_TABS)) == 2) { LevelManagerApplyEnvironment(); }
            return TRUE;
        }
        if (HIWORD(wparam) == BN_CLICKED)
        {
            BOOL ammo = id >= IDC_INTRO_AMMO_ADD && id <= IDC_INTRO_AMMO_DOWN;
            int action = id - (ammo ? IDC_INTRO_AMMO_ADD : IDC_INTRO_WEAPON_ADD);
            if (action >= 0 && action <= 4) { LevelManagerEdit(hwnd, ammo, (SetupIntroAction)action); return TRUE; }
        }
        break;
    }
    case WM_CLOSE:
        if (LevelManagerApplyEnvironment()) { EnvironmentPanelShow(g_EnvironmentPanel, FALSE); DestroyWindow(hwnd); }
        return TRUE;
    case WM_NCDESTROY:
        RoomStatsFree(&g_RoomStats); g_RoomSortColumn = 0; g_RoomSortDescending = FALSE;
        free(g_IntroEntries); g_IntroEntries = NULL; g_IntroCount = 0;
        g_IntroValid = FALSE; g_IntroSetupName[0] = 0; g_LevelManager = NULL; g_EnvironmentPanel = NULL;
        break;
    }
    return FALSE;
}

BOOL LevelManagerShow(HWND owner, HINSTANCE instance, const SetupFile *setup, const char *levelname)
{
    if (!g_LevelManager)
    {
        static const char *labels[] = {"Stage Options", "Intro", "Environment", "Rooms"};
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES};
        if (!InitCommonControlsEx(&controls)) { return FALSE; }
        g_LevelManager = CreateDialog(instance, MAKEINTRESOURCE(IDD_LEVEL_MANAGER), owner, LevelManagerDialogProc);
        if (!g_LevelManager) { return FALSE; }
        for (int i = 0; i < 4; i++)
        {
            TCITEM item = {0}; item.mask = TCIF_TEXT; item.pszText = (LPSTR)labels[i];
            if (TabCtrl_InsertItem(GetDlgItem(g_LevelManager, IDC_LEVEL_MANAGER_TABS), i, &item) == -1)
            { DestroyWindow(g_LevelManager); return FALSE; }
        }
        for (int table = 0; table < 2; table++)
        {
            HWND list = GetDlgItem(g_LevelManager, table ? IDC_INTRO_AMMO : IDC_INTRO_WEAPONS);
            const char *columns[] = {"#", table ? "Ammo type" : "Right-hand weapon", table ? "Quantity" : "Left-hand weapon"};
            ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
            for (int i = 0; i < 3; i++)
            {
                LVCOLUMN column = {0}; column.mask = LVCF_TEXT; column.pszText = (LPSTR)columns[i];
                if (ListView_InsertColumn(list, i, &column) == -1) { DestroyWindow(g_LevelManager); return FALSE; }
            }
        }
        HWND roomlist = GetDlgItem(g_LevelManager, IDC_LEVEL_ROOMS_LIST);
        const char *roomcolumns[] = {"Room", "Primary tris", "Secondary tris", "STAN tiles", "Objects", "Characters"};
        ListView_SetExtendedListViewStyle(roomlist, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        for (int i = 0; i <= ROOM_COUNT_COLUMNS; i++)
        {
            LVCOLUMN column = {0}; column.mask = LVCF_TEXT | LVCF_FMT;
            column.pszText = (LPSTR)roomcolumns[i]; column.fmt = i ? LVCFMT_RIGHT : LVCFMT_LEFT;
            if (ListView_InsertColumn(roomlist, i, &column) == -1) { DestroyWindow(g_LevelManager); return FALSE; }
        }
        g_EnvironmentPanel = EnvironmentPanelCreate(g_LevelManager, instance);
        if (!g_EnvironmentPanel) { DestroyWindow(g_LevelManager); return FALSE; }
        TabCtrl_SetCurSel(GetDlgItem(g_LevelManager, IDC_LEVEL_MANAGER_TABS), 0);
        LevelManagerLayout(g_LevelManager);
        LevelManagerButtons(g_LevelManager);
    }
    LevelManagerRefresh(setup, levelname);
    ShowWindow(g_LevelManager, SW_SHOWNORMAL); SetForegroundWindow(g_LevelManager);
    return TRUE;
}

BOOL LevelManagerHandleMessage(MSG *message)
{
    if (!g_LevelManager || !message
        || (message->hwnd != g_LevelManager && !IsChild(g_LevelManager, message->hwnd))) { return FALSE; }
    if (message->message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000))
    {
        if (g_EnvironmentPanel && IsChild(g_EnvironmentPanel, message->hwnd)
            && (message->wParam == 'Z' || message->wParam == 'Y'))
        {
            /* Let environment edit boxes undo their text, without undoing
             * an unrelated geometry edit behind this settings form. */
            TranslateMessage(message); DispatchMessage(message); return TRUE;
        }
        if (message->wParam == 'Z' || message->wParam == 'Y' || message->wParam == 'S')
        {
            if (!(message->lParam & ((LPARAM)1 << 30)))
            {
                BOOL redo = message->wParam == 'Y' || (GetKeyState(VK_SHIFT) & 0x8000);
                SendMessage(GetWindow(g_LevelManager, GW_OWNER), message->wParam == 'S' ? LEVELMANAGER_WM_SAVE : LEVELMANAGER_WM_HISTORY, redo, 0);
            }
            return TRUE;
        }
    }
    /* Keep dialog navigation and keys inside this window, ahead of editor shortcuts. */
    if (!IsDialogMessage(g_LevelManager, message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
