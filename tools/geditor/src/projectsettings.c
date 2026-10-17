#include "projectsettings.h"
#include "resource.h"
#include "sfxcatalog.h"
#include "sfxplayer.h"
#include "editorpath.h"
#include <commctrl.h>
#include <stdio.h>
#include <windowsx.h>
#include <stdlib.h>
#include "texload.h"
#include "briefing.h"
#include "../../../src/game/levelmenuwallet.h"

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

static int g_AssetSlot;
static BOOL g_AssetLoading,g_AssetDraft;
static TexThumb *g_AssetImages;
static unsigned char *g_AssetPixels;
static DWORD g_AssetImageCount;
static unsigned g_AssetDefaultImages[60];
static void ProjectSettingsPopulateSlots(void);
static void ProjectSettingsLoadInspector(void);
static void ProjectSettingsSelectSlot(int slot);
static void ProjectSettingsLoadAssetImages(void);
static void ProjectSettingsDrawAsset(const DRAWITEMSTRUCT *item);

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
        if(g_SettingsProject) {
            LevelMenuAsset a;LevelAssetsGet(&g_SettingsProject->levelSlots,&g_SettingsProject->levelSlotOverrides,i,&a);
            if(a.flags&LEVEL_ASSET_BIT(LEVEL_ASSET_MENU_NAME))snprintf(text,sizeof(text),"%u. %s",i<20?i+1:i-19,a.names[0]);
        }
        SetDlgItemText(g_ProjectSettings, IDC_PROJECT_LEVEL_SLOT_FIRST + i, text);
        SendDlgItemMessage(g_ProjectSettings,IDC_PROJECT_LEVEL_SLOT_FIRST+i,BM_SETCHECK,i==(unsigned)g_AssetSlot?BST_CHECKED:BST_UNCHECKED,0);
        EnableWindow(GetDlgItem(g_ProjectSettings, IDC_PROJECT_LEVEL_SLOT_FIRST + i), ready);
    }
    SetDlgItemText(g_ProjectSettings, IDC_PROJECT_LEVEL_STATUS, ready
        ? "Drag levels onto boxes. Click a box to edit its assets. Save Project saves assignments and asset settings."
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
    if(!ProjectSettingsApply())return;
    ProjectLevelSlotEdit edit = {slot, stage, ""};
    if (!SendMessage(GetParent(g_ProjectSettings), PROJECTSETTINGS_WM_LEVEL_SLOT, 0, (LPARAM)&edit))
        MessageBox(g_ProjectSettings, edit.why[0] ? edit.why : "The level could not be assigned.", "Level Manager", MB_ICONWARNING);
    ProjectSettingsSelectSlot((int)slot);
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
    int leftwidth = width * 19 / 100, inspectorwidth=width*29/100, gridleft = page.left + leftwidth + gap;
    int inspectorleft=page.right-inspectorwidth;
    int gridwidth = inspectorleft-gap-gridleft;
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
    int top=page.top,control=max(20,line*5/6),label=max(14,line*5/9);
    int row=control+label+max(3,gap*3/5);
    MoveWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_TITLE),inspectorleft,top,inspectorwidth,line,TRUE);top+=line+gap;
    MoveWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSIGN_LEVEL),inspectorleft,top,inspectorwidth,line,TRUE);top+=line+gap;
    for(int f=0;f<7;f++) {
        int fieldtop=top+f*row,preview=f<3?control*2:0;
        MoveWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_LABEL_FIRST+f),inspectorleft,fieldtop,inspectorwidth,label,TRUE);
        MoveWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_FIELD_FIRST+f),inspectorleft,fieldtop+label,
            inspectorwidth-preview-(preview?gap:0),f<3||f==6?control*12:control,TRUE);
        if(f<3)MoveWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_PREVIEW_FIRST+f),page.right-preview,fieldtop+label,preview,control,TRUE);
    }
    top+=7*row;
    for(int i=0;i<3;i++)MoveWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_APPLY+i),inspectorleft+i*(inspectorwidth/3),top,inspectorwidth/3-gap,line,TRUE);
    MoveWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_HINT),inspectorleft,top+line+gap,inspectorwidth,max(0,page.bottom-top-line*3-gap),TRUE);
    ShowWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_TITLE),levels?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSIGN_LEVEL),levels?SW_SHOW:SW_HIDE);
    for(int f=0;f<7;f++) {
        ShowWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_LABEL_FIRST+f),levels?SW_SHOW:SW_HIDE);
        ShowWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_FIELD_FIRST+f),levels?SW_SHOW:SW_HIDE);
    }
    for(int i=0;i<3;i++)ShowWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_PREVIEW_FIRST+i),levels?SW_SHOW:SW_HIDE);
    for(int i=0;i<4;i++)ShowWindow(GetDlgItem(hwnd,IDC_PROJECT_ASSET_APPLY+i),levels?SW_SHOW:SW_HIDE);

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
    if(project && g_AssetDraft && !ProjectSettingsApply())return;
    g_AssetDraft=FALSE;
    ProjectSettingsEndLevelDrag();
    g_SettingsProject = project;
    const char *projectdir = project && project->name[0] ? project->dir : NULL;
    ProjectSettingsPopulateLevels();
    ProjectSettingsLoadAssetImages();
    ProjectSettingsLoadInspector();
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
        RECT minimum = {0, 0, 760, 440};
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
    case WM_DRAWITEM:
        if(wparam>=IDC_PROJECT_ASSET_PREVIEW_FIRST && wparam<IDC_PROJECT_ASSET_PREVIEW_FIRST+3)
        { ProjectSettingsDrawAsset((const DRAWITEMSTRUCT *)lparam);return TRUE; }
        break;
    case WM_COMMAND:
        if(LOWORD(wparam)>=IDC_PROJECT_ASSET_FIELD_FIRST && LOWORD(wparam)<IDC_PROJECT_ASSET_FIELD_FIRST+7
            && !g_AssetLoading && (HIWORD(wparam)==EN_CHANGE || HIWORD(wparam)==CBN_SELCHANGE))
        {
            g_AssetDraft=TRUE;SendMessage(GetParent(hwnd),PROJECTSETTINGS_WM_DRAFT,0,0);
            for(int i=0;i<3;i++)InvalidateRect(GetDlgItem(hwnd,IDC_PROJECT_ASSET_PREVIEW_FIRST+i),NULL,TRUE);
            return TRUE;
        }
        if(LOWORD(wparam)==IDC_PROJECT_ASSIGN_LEVEL) {
            int stage;if(ProjectSettingsSelectedLevel(&stage))ProjectSettingsAssignSlot(g_AssetSlot,stage);return TRUE;
        }
        if(LOWORD(wparam)==IDC_PROJECT_ASSET_APPLY) {ProjectSettingsApply();return TRUE;}
        if(LOWORD(wparam)==IDC_PROJECT_ASSET_REVERT) {ProjectSettingsLoadInspector();return TRUE;}
        if(LOWORD(wparam)==IDC_PROJECT_ASSET_DEFAULTS) {
            ProjectLevelAssetEdit edit={0};edit.slot=g_AssetSlot;edit.why="";
            if(!SendMessage(GetParent(hwnd),PROJECTSETTINGS_WM_LEVEL_ASSETS,0,(LPARAM)&edit))MessageBox(hwnd,edit.why,"Level Manager",MB_ICONWARNING);
            else {ProjectSettingsLoadInspector();ProjectSettingsPopulateSlots();}return TRUE;
        }
        if (LOWORD(wparam) >= IDC_PROJECT_LEVEL_SLOT_FIRST
            && LOWORD(wparam) < IDC_PROJECT_LEVEL_SLOT_FIRST + LEVEL_MENU_SLOT_COUNT && HIWORD(wparam) == BN_CLICKED)
        {
            ProjectSettingsSelectSlot(LOWORD(wparam)-IDC_PROJECT_LEVEL_SLOT_FIRST);
            return TRUE;
        }
        if (LOWORD(wparam) == IDC_PROJECT_SFX_FILTER && HIWORD(wparam) == EN_CHANGE && g_ProjectSettings)
        { ProjectSettingsStopSound(); ProjectSettingsPopulate(); return TRUE; }
        if (LOWORD(wparam) == IDC_PROJECT_SFX_PLAY) { ProjectSettingsPlaySound(); return TRUE; }
        if (LOWORD(wparam) == IDC_PROJECT_SFX_STOP) { ProjectSettingsStopSound(); return TRUE; }
        if (LOWORD(wparam) == IDOK && GetFocus() == GetDlgItem(hwnd, IDC_PROJECT_SFX_LIST))
        { ProjectSettingsPlaySound(); return TRUE; }
        if (LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDOK)
        { if (ProjectSettingsApply() && ProjectSettingsStopSound()) { DestroyWindow(hwnd); } return TRUE; }
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
    case WM_CLOSE: if (ProjectSettingsApply() && ProjectSettingsStopSound()) { DestroyWindow(hwnd); } return TRUE;
    case WM_NCDESTROY:
        ProjectSettingsEndLevelDrag(); g_SettingsProject = NULL;g_AssetDraft=FALSE;
        free(g_AssetImages);free(g_AssetPixels);g_AssetImages=NULL;g_AssetPixels=NULL;g_AssetImageCount=0;
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
            HWND box = CreateWindowEx(0, "BUTTON", "", WS_CHILD | WS_TABSTOP | BS_RADIOBUTTON | BS_PUSHLIKE | BS_MULTILINE,
                0, 0, 0, 0, g_ProjectSettings, (HMENU)(INT_PTR)(IDC_PROJECT_LEVEL_SLOT_FIRST + i), instance, NULL);
            if (!box) { DestroyWindow(g_ProjectSettings); return FALSE; }
            SendMessage(box, WM_SETFONT, SendMessage(g_ProjectSettings, WM_GETFONT, 0, 0), FALSE);
        }
        const char *labels[]={"Menu icon (68 x 44)","Briefing image: upper half (128 x 64)","Briefing image: lower half (128 x 64)","Menu name","Mission name","Part name","Menu briefing source"};
        for(int f=0;f<7;f++) {
            HWND label=CreateWindowEx(0,"STATIC",labels[f],WS_CHILD,0,0,0,0,g_ProjectSettings,(HMENU)(INT_PTR)(IDC_PROJECT_ASSET_LABEL_FIRST+f),instance,NULL);
            HWND field=CreateWindowEx(f>=3&&f<6?WS_EX_CLIENTEDGE:0,f>=3&&f<6?"EDIT":"COMBOBOX","",
                WS_CHILD|WS_TABSTOP|(f>=3&&f<6?ES_AUTOHSCROLL:CBS_DROPDOWNLIST|WS_VSCROLL),0,0,0,0,g_ProjectSettings,(HMENU)(INT_PTR)(IDC_PROJECT_ASSET_FIELD_FIRST+f),instance,NULL);
            if(!label||!field){DestroyWindow(g_ProjectSettings);return FALSE;}
            SendMessage(label,WM_SETFONT,SendMessage(g_ProjectSettings,WM_GETFONT,0,0),FALSE);
            SendMessage(field,WM_SETFONT,SendMessage(g_ProjectSettings,WM_GETFONT,0,0),FALSE);
            if(f>=3&&f<6)SendMessage(field,EM_SETLIMITTEXT,31,0);
            if(f<3&&!CreateWindowEx(0,"STATIC","",WS_CHILD|SS_OWNERDRAW,0,0,0,0,g_ProjectSettings,(HMENU)(INT_PTR)(IDC_PROJECT_ASSET_PREVIEW_FIRST+f),instance,NULL))
            {DestroyWindow(g_ProjectSettings);return FALSE;}
        }
        const int ids[]={IDC_PROJECT_ASSET_TITLE,IDC_PROJECT_ASSIGN_LEVEL,IDC_PROJECT_ASSET_APPLY,IDC_PROJECT_ASSET_REVERT,IDC_PROJECT_ASSET_DEFAULTS,IDC_PROJECT_ASSET_HINT};
        const char *texts[]={"","Assign selected level","Apply","Revert","Defaults",""};
        for(int i=0;i<6;i++) {
            BOOL button=i>0&&i<5;
            HWND control=CreateWindowEx(0,button?"BUTTON":"STATIC",texts[i],WS_CHILD|(button?WS_TABSTOP|BS_PUSHBUTTON:0),0,0,0,0,
                g_ProjectSettings,(HMENU)(INT_PTR)ids[i],instance,NULL);
            if(!control){DestroyWindow(g_ProjectSettings);return FALSE;}
            SendMessage(control,WM_SETFONT,SendMessage(g_ProjectSettings,WM_GETFONT,0,0),FALSE);
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

static int AssetComboValue(int field)
{
    HWND control=GetDlgItem(g_ProjectSettings,IDC_PROJECT_ASSET_FIELD_FIRST+field);
    LRESULT row=SendMessage(control,CB_GETCURSEL,0,0);
    return row==CB_ERR ? -2 : (int)SendMessage(control,CB_GETITEMDATA,row,0);
}
static void AssetComboAdd(HWND control,const char *label,int value)
{
    LRESULT row=SendMessage(control,CB_ADDSTRING,0,(LPARAM)label);
    if(row>=0)SendMessage(control,CB_SETITEMDATA,row,value);
}
static void AssetComboSelect(int field,int value)
{
    HWND control=GetDlgItem(g_ProjectSettings,IDC_PROJECT_ASSET_FIELD_FIRST+field);
    int count=(int)SendMessage(control,CB_GETCOUNT,0,0);
    for(int i=0;i<count;i++)if((int)SendMessage(control,CB_GETITEMDATA,i,0)==value)
    {SendMessage(control,CB_SETCURSEL,i,0);return;}
    char label[80];snprintf(label,sizeof(label),"Unavailable: %s%04X",field<3?"image ":"briefing ",value);
    AssetComboAdd(control,label,value);SendMessage(control,CB_SETCURSEL,count,0);
}
static void ProjectSettingsLoadAssetImages(void)
{
    const char *why="";RomFile rom={0};char path[MAX_PATH];DWORD offset,span;
    free(g_AssetImages);free(g_AssetPixels);g_AssetImages=NULL;g_AssetPixels=NULL;g_AssetImageCount=0;
    for(int i=0;i<60;i++)g_AssetDefaultImages[i]=(unsigned)-1;
    if(!g_SettingsProject||!g_SettingsProject->name[0])return;
    g_AssetImageCount=TexLoadProjectThumbnails(g_SettingsProject->dir,&g_AssetImages,&g_AssetPixels,&why);
    if(EditorPathJoin(path,sizeof(path),g_SettingsProject->dir,"base.z64")&&RomLoad(path,&rom,&why)) {
        if(RomFindFile(&rom,"PwalletbondZ",&offset,&span,&why)) {
            LevelMenuAsset empty[20]={{0}};int order[20];for(int i=0;i<20;i++)order[i]=i;
            if(!levelWalletPatch(rom.data+offset,span,43,84,empty,order,g_AssetDefaultImages))
                for(int i=0;i<60;i++)g_AssetDefaultImages[i]=(unsigned)-1;
        }
        RomFree(&rom);
    }
}
static void ProjectSettingsLoadInspector(void)
{
    LevelMenuAsset asset={0};char text[128];
    BOOL ready=g_SettingsProject&&g_SettingsProject->name[0]&&g_SettingsProject->levelSlots.assetsAvailable;
    g_AssetLoading=TRUE;g_AssetDraft=FALSE;
    if(g_SettingsProject)LevelAssetsGet(&g_SettingsProject->levelSlots,&g_SettingsProject->levelSlotOverrides,g_AssetSlot,&asset);
    snprintf(text,sizeof(text),"%s - slot %d",g_AssetSlot<20?"Single player":"Multiplayer",g_AssetSlot<20?g_AssetSlot+1:g_AssetSlot-19);
    SetDlgItemText(g_ProjectSettings,IDC_PROJECT_ASSET_TITLE,text);
    for(int f=0;f<7;f++) {
        HWND field=GetDlgItem(g_ProjectSettings,IDC_PROJECT_ASSET_FIELD_FIRST+f);
        BOOL supported=g_AssetSlot<20 || f==0 || f==3;
        EnableWindow(field,ready&&supported);
        if(f>=3&&f<6) {
            SetWindowText(field,asset.flags&LEVEL_ASSET_BIT(f)?asset.names[f-3]:"");
            SendMessageW(field,EM_SETCUEBANNER,TRUE,(LPARAM)L"Default (leave blank)");
        } else {
            SendMessage(field,CB_RESETCONTENT,0,0);
            if(f<3) {
                unsigned defaultImage=g_AssetSlot<20?g_AssetDefaultImages[g_AssetSlot*3+f]:(unsigned)-1;
                if(defaultImage<4096)snprintf(text,sizeof(text),"Default (%04X)",defaultImage);else strcpy(text,"Default");
                AssetComboAdd(field,text,-2);
                for(DWORD i=0;i<g_AssetImageCount;i++) {
                    TexThumb *image=&g_AssetImages[i];unsigned id=(unsigned)strtoul(image->label,NULL,16);
                    if(!image->info.valid||image->info.mipmaps||id>=4096
                        ||!LEVEL_ASSET_IMAGE_COMPATIBLE(image->info.format,image->imagewidth,image->imageheight,f))continue;
                    snprintf(text,sizeof(text),"%04X - %s",id,TexInfoFormatName(image->info.format));AssetComboAdd(field,text,(int)id);
                }
                AssetComboSelect(f,asset.flags&LEVEL_ASSET_BIT(f)?asset.images[f]:-2);
            } else {
                AssetComboAdd(field,"Assigned level's briefing",-2);AssetComboAdd(field,"No briefing",-1);
                if(g_SettingsProject)for(DWORD i=0;i<g_SettingsProject->levelcount;i++) {
                    const RomLevel *level=&g_SettingsProject->levels[i];const char *bank,*brief;
                    if(!BriefingNames(level->levelID,&bank,&brief))continue;
                    snprintf(text,sizeof(text),"%s (%s)",level->name,brief);AssetComboAdd(field,text,level->levelID);
                }
                AssetComboSelect(f,asset.flags&LEVEL_ASSET_BIT(f)?asset.briefingStage:-2);
            }
        }
    }
    EnableWindow(GetDlgItem(g_ProjectSettings,IDC_PROJECT_ASSIGN_LEVEL),g_SettingsProject&&g_SettingsProject->levelSlots.available);
    for(int i=0;i<3;i++)EnableWindow(GetDlgItem(g_ProjectSettings,IDC_PROJECT_ASSET_APPLY+i),ready);
    SetDlgItemText(g_ProjectSettings,IDC_PROJECT_ASSET_HINT,ready
        ? "Names: up to 31 characters. Blank uses the default. Edit briefing text in Level Settings > Briefing for the chosen source."
        : "Rebuild GUD and rebase this project to customize menu assets.");
    for(int i=0;i<3;i++)InvalidateRect(GetDlgItem(g_ProjectSettings,IDC_PROJECT_ASSET_PREVIEW_FIRST+i),NULL,TRUE);
    g_AssetLoading=FALSE;
    SendMessage(GetParent(g_ProjectSettings),PROJECTSETTINGS_WM_DRAFT,0,0);
}
BOOL ProjectSettingsHasDraft(void) {return g_ProjectSettings&&g_AssetDraft;}
BOOL ProjectSettingsApply(void)
{
    if(!g_ProjectSettings||!g_AssetDraft)return TRUE;
    ProjectLevelAssetEdit edit={0};edit.slot=g_AssetSlot;edit.why="";
    for(int f=0;f<7;f++) {
        if(g_AssetSlot>=20&&f!=0&&f!=3)continue;
        if(f>=3&&f<6) {
            GetDlgItemText(g_ProjectSettings,IDC_PROJECT_ASSET_FIELD_FIRST+f,edit.value.names[f-3],32);
            if(edit.value.names[f-3][0])edit.value.flags|=LEVEL_ASSET_BIT(f);
        } else {
            int value=AssetComboValue(f);
            if(value!=-2) {
                edit.value.flags|=LEVEL_ASSET_BIT(f);
                if(f<3)edit.value.images[f]=value;else edit.value.briefingStage=value;
            }
        }
    }
    if(!SendMessage(GetParent(g_ProjectSettings),PROJECTSETTINGS_WM_LEVEL_ASSETS,0,(LPARAM)&edit)) {
        MessageBox(g_ProjectSettings,edit.why[0]?edit.why:"The asset settings could not be applied.","Level Manager",MB_ICONWARNING);return FALSE;
    }
    g_AssetDraft=FALSE;SendMessage(GetParent(g_ProjectSettings),PROJECTSETTINGS_WM_DRAFT,0,0);
    ProjectSettingsPopulateSlots();return TRUE;
}
static void ProjectSettingsSelectSlot(int slot)
{
    if(slot<0||slot>=32)return;
    if(ProjectSettingsApply()) {g_AssetSlot=slot;ProjectSettingsLoadInspector();}
    ProjectSettingsPopulateSlots();
}
static void ProjectSettingsDrawAsset(const DRAWITEMSTRUCT *item)
{
    int f=(int)item->CtlID-IDC_PROJECT_ASSET_PREVIEW_FIRST;
    int id=AssetComboValue(f);RECT box=item->rcItem;
    FillRect(item->hDC,&box,GetSysColorBrush(COLOR_WINDOW));
    if(id==-2&&g_AssetSlot<20)id=(int)g_AssetDefaultImages[g_AssetSlot*3+f];
    for(DWORD i=0;i<g_AssetImageCount;i++)if(id>=0&&(unsigned)strtoul(g_AssetImages[i].label,NULL,16)==(unsigned)id) {
        TexThumb *image=&g_AssetImages[i];BITMAPINFO info={0};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=TEX_THUMB_MAX;info.bmiHeader.biHeight=-TEX_THUMB_MAX;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        int height=box.bottom-box.top,width=image->h?image->w*height/image->h:0;
        if(width>box.right-box.left){width=box.right-box.left;height=image->w?image->h*width/image->w:0;}
        StretchDIBits(item->hDC,box.left+(box.right-box.left-width)/2,box.top,width,height,0,0,image->w,image->h,
            g_AssetPixels+image->pixeloffset,&info,DIB_RGB_COLORS,SRCCOPY);return;
    }
}
