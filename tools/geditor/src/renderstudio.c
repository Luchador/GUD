#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderstudio.h"
#include "studioviewport.h"
#include "studioscene.h"

static HWND g_Studio, g_StudioViewport;
static char g_StudioProject[MAX_PATH];
static const char g_StudioNavigation[] = "Left/right drag: orbit    Middle drag: pan    Wheel: zoom";

/* Group boxes normally leave their interiors to the parent's background.
 * WS_CLIPCHILDREN protects our GL viewport, but also prevents the parent from
 * clearing old borders inside the resized groups. Erase each group itself,
 * excluding its content control so native controls and GL never get painted over. */
static LRESULT CALLBACK RenderStudioGroupProc(HWND hwnd, UINT message, WPARAM wparam,
    LPARAM lparam, UINT_PTR subclass, DWORD_PTR contentid)
{
    if (message == WM_ERASEBKGND)
    {
        HDC dc = (HDC)wparam; RECT client, content;
        HWND child = GetDlgItem(GetParent(hwnd), (int)contentid);
        int saved = SaveDC(dc);
        GetClientRect(hwnd, &client);
        if (child && IsWindowVisible(child))
        {
            GetWindowRect(child, &content);
            MapWindowPoints(NULL, hwnd, (POINT *)&content, 2);
            ExcludeClipRect(dc, content.left, content.top, content.right, content.bottom);
        }
        FillRect(dc, &client, GetSysColorBrush(COLOR_3DFACE));
        if (saved) { RestoreDC(dc, saved); }
        return 1;
    }
    if (message == WM_NCDESTROY) { RemoveWindowSubclass(hwnd, RenderStudioGroupProc, subclass); }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

static void RenderStudioRefreshScenes(const char *select)
{
    HWND list = GetDlgItem(g_Studio, IDC_STUDIO_SCENE);
    StudioSceneEntry *entries = NULL; DWORD count = 0;
    const char *why = ""; char previous[MAX_PATH] = "";
    LRESULT row = SendMessage(list, LB_GETCURSEL, 0, 0);
    if (select) { lstrcpyn(previous, select, sizeof(previous)); }
    else if (row != LB_ERR && SendMessage(list, LB_GETTEXTLEN, row, 0) < MAX_PATH)
    { SendMessage(list, LB_GETTEXT, row, (LPARAM)previous); }
    if (!StudioSceneList(g_StudioProject, &entries, &count, &why))
    { SetDlgItemText(g_Studio, IDC_STUDIO_STATUS, why); return; }
    SendMessage(list, WM_SETREDRAW, FALSE, 0);
    SendMessage(list, LB_RESETCONTENT, 0, 0);
    int widest = 0; HDC dc = GetDC(list);
    HFONT oldfont = dc ? SelectObject(dc, (HFONT)SendMessage(list, WM_GETFONT, 0, 0)) : NULL;
    for (DWORD i = 0; i < count; i++)
    {
        LRESULT added = SendMessage(list, LB_ADDSTRING, 0, (LPARAM)entries[i].filename);
        if (added == LB_ERR || added == LB_ERRSPACE)
        { why = "Not enough memory to list all studio scenes."; break; }
        if (dc)
        {
            SIZE size;
            if (GetTextExtentPoint32(dc, entries[i].filename, (int)strlen(entries[i].filename), &size))
                widest = max(widest, size.cx + 8);
        }
    }
    if (dc) { if (oldfont) { SelectObject(dc, oldfont); } ReleaseDC(list, dc); }
    free(entries);
    SendMessage(list, LB_SETHORIZONTALEXTENT, widest, 0);
    if (previous[0])
    {
        row = SendMessage(list, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)previous);
        if (row != LB_ERR) { SendMessage(list, LB_SETCURSEL, row, 0); }
    }
    SendMessage(list, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(list, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
    SetDlgItemText(g_Studio, IDC_STUDIO_STATUS, why[0] ? why : g_StudioNavigation);
}

typedef struct StudioNewScene {
    char projectdir[MAX_PATH], filename[MAX_PATH];
} StudioNewScene;

static INT_PTR CALLBACK RenderStudioNewSceneProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    StudioNewScene *state = (StudioNewScene *)GetWindowLongPtr(dialog, DWLP_USER);
    switch (message)
    {
    case WM_INITDIALOG:
        SetWindowLongPtr(dialog, DWLP_USER, lparam);
        SendDlgItemMessage(dialog, IDC_STUDIO_SCENE_NAME, EM_LIMITTEXT, STUDIO_SCENE_NAME_MAX + 3, 0);
        EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
        SetFocus(GetDlgItem(dialog, IDC_STUDIO_SCENE_NAME));
        return FALSE;
    case WM_CLOSE: EndDialog(dialog, IDCANCEL); return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
        if (LOWORD(wparam) == IDC_STUDIO_SCENE_NAME && HIWORD(wparam) == EN_CHANGE)
        {
            EnableWindow(GetDlgItem(dialog, IDOK), GetWindowTextLength(GetDlgItem(dialog, IDC_STUDIO_SCENE_NAME)) > 0);
            SetDlgItemText(dialog, IDC_STUDIO_SCENE_ERROR, "");
            return TRUE;
        }
        if (LOWORD(wparam) == IDOK && state)
        {
            char name[STUDIO_SCENE_NAME_MAX + 4]; const char *why = "";
            GetDlgItemText(dialog, IDC_STUDIO_SCENE_NAME, name, sizeof(name));
            if (strcmp(state->projectdir, g_StudioProject))
                why = "The project changed. Cancel and create the scene in the current project.";
            else if (StudioSceneCreate(state->projectdir, name, state->filename, &why))
            { EndDialog(dialog, IDOK); return TRUE; }
            SetDlgItemText(dialog, IDC_STUDIO_SCENE_ERROR, why);
            SetFocus(GetDlgItem(dialog, IDC_STUDIO_SCENE_NAME));
            SendDlgItemMessage(dialog, IDC_STUDIO_SCENE_NAME, EM_SETSEL, 0, -1);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void RenderStudioNewScene(HWND hwnd)
{
    StudioNewScene state = {{0}, {0}};
    if (!g_StudioProject[0]) { return; }
    lstrcpyn(state.projectdir, g_StudioProject, sizeof(state.projectdir));
    INT_PTR result = DialogBoxParam((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        MAKEINTRESOURCE(IDD_STUDIO_NEW_SCENE), hwnd, RenderStudioNewSceneProc, (LPARAM)&state);
    if (result == IDOK && g_Studio && !strcmp(state.projectdir, g_StudioProject))
        RenderStudioRefreshScenes(state.filename);
    else if (result == -1)
        MessageBox(hwnd, "Could not open the New Scene dialog.", "Render Studio", MB_ICONERROR);
}

static void RenderStudioPlace(HWND control, int x, int y, int width, int height)
{
    SetWindowPos(control, NULL, x, y, max(0, width), max(0, height),
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
}

static void RenderStudioPanel(HWND hwnd, int group, int content,
    int x, int y, int width, int height, int margin, int heading)
{
    RenderStudioPlace(GetDlgItem(hwnd, group), x, y, width, height);
    RenderStudioPlace(GetDlgItem(hwnd, content), x + margin, y + heading,
        width - margin * 2, height - heading - margin);
}

static void RenderStudioLayout(HWND hwnd)
{
    RECT client, units = {8, 16, 144, 0}, panel = {0, 0, 168, 88};
    int margin, height, left, right, rightx, scenesize, imagesize, outline;
    GetClientRect(hwnd, &client); MapDialogRect(hwnd, &units); MapDialogRect(hwnd, &panel);
    margin = units.left; left = units.right; right = panel.right;
    height = max(0, client.bottom - units.top - margin * 3);
    rightx = client.right - margin - right;
    scenesize = min(panel.bottom, height / 3);
    imagesize = max(0, (height - scenesize - margin * 2) / 2);
    outline = max(0, (height - margin) / 2);
    RenderStudioPanel(hwnd, IDC_STUDIO_SCENE_PANEL, IDC_STUDIO_SCENE, margin, margin,
        left, scenesize, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_IMAGES_PANEL, IDC_STUDIO_IMAGES, margin, margin * 2 + scenesize,
        left, imagesize, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_MODELS_PANEL, IDC_STUDIO_MODELS, margin, margin * 3 + scenesize + imagesize,
        left, height - scenesize - imagesize - margin * 2, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_OUTLINER_PANEL, IDC_STUDIO_OUTLINER, rightx, margin,
        right, outline, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_PROPERTIES_PANEL, IDC_STUDIO_PROPERTIES, rightx, margin * 2 + outline,
        right, height - outline - margin, margin, units.top);
    if (g_StudioViewport)
        RenderStudioPlace(g_StudioViewport, left + margin * 2, margin,
            rightx - left - margin * 3, height);
    RenderStudioPlace(GetDlgItem(hwnd, IDC_STUDIO_STATUS), margin, height + margin * 2,
        client.right - margin * 2, units.top);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void RenderStudioSetProject(const GEditorProject *project)
{
    char title[GEDITOR_NAME_MAX + 32];
    const char *dir = project ? project->dir : "";
    BOOL changed;
    if (!g_Studio) { return; }
    changed = strcmp(g_StudioProject, dir) != 0;
    lstrcpyn(g_StudioProject, dir, sizeof(g_StudioProject));
    snprintf(title, sizeof(title), "Render Studio%s%s", dir[0] ? " - " : "", dir[0] ? project->name : "");
    SetWindowText(g_Studio, title);
    if (!changed) { return; }
    /* Scene state belongs to this project, never to the selected game level. */
    StudioViewportReset(g_StudioViewport);
    SendDlgItemMessage(g_Studio, IDC_STUDIO_SCENE, LB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(g_Studio, IDC_STUDIO_IMAGES, LB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(g_Studio, IDC_STUDIO_MODELS, LB_RESETCONTENT, 0, 0);
    TreeView_DeleteAllItems(GetDlgItem(g_Studio, IDC_STUDIO_OUTLINER));
    if (dir[0])
    {
        TVINSERTSTRUCT root = {0};
        root.hParent = TVI_ROOT; root.hInsertAfter = TVI_LAST;
        root.item.mask = TVIF_TEXT; root.item.pszText = "Scene";
        TreeView_InsertItem(GetDlgItem(g_Studio, IDC_STUDIO_OUTLINER), &root);
    }
    RenderStudioRefreshScenes(NULL);
    SetDlgItemText(g_Studio, IDC_STUDIO_PROPERTIES, "Nothing selected.");
    EnableWindow(GetDlgItem(g_Studio, IDC_STUDIO_SCENE), dir[0] != 0);
    EnableWindow(GetDlgItem(g_Studio, IDC_STUDIO_IMAGES), dir[0] != 0);
    EnableWindow(GetDlgItem(g_Studio, IDC_STUDIO_MODELS), dir[0] != 0);
    EnableWindow(GetDlgItem(g_Studio, IDC_STUDIO_OUTLINER), dir[0] != 0);
}

static INT_PTR CALLBACK RenderStudioProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        static const int panels[][2] = {
            {IDC_STUDIO_SCENE_PANEL, IDC_STUDIO_SCENE}, {IDC_STUDIO_IMAGES_PANEL, IDC_STUDIO_IMAGES},
            {IDC_STUDIO_MODELS_PANEL, IDC_STUDIO_MODELS}, {IDC_STUDIO_OUTLINER_PANEL, IDC_STUDIO_OUTLINER},
            {IDC_STUDIO_PROPERTIES_PANEL, IDC_STUDIO_PROPERTIES}};
        g_Studio = hwnd;
        for (size_t i = 0; i < sizeof(panels) / sizeof(*panels); i++)
            SetWindowSubclass(GetDlgItem(hwnd, panels[i][0]), RenderStudioGroupProc, 1, panels[i][1]);
        return TRUE;
    }
    case WM_SIZE: RenderStudioLayout(hwnd); return TRUE;
    case WM_ACTIVATE:
        if (LOWORD(wparam) != WA_INACTIVE && g_StudioProject[0]) { RenderStudioRefreshScenes(NULL); }
        break; /* Let the dialog manager restore keyboard focus normally. */
    case WM_INITMENUPOPUP:
        EnableMenuItem((HMENU)wparam, ID_STUDIO_NEW_SCENE, MF_BYCOMMAND | (g_StudioProject[0] ? MF_ENABLED : MF_GRAYED));
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = {0, 0, 640, 320};
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
            GetMenu(hwnd) != NULL, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_STUDIO_NEW_SCENE) { RenderStudioNewScene(hwnd); return TRUE; }
        if (LOWORD(wparam) == IDCANCEL) { DestroyWindow(hwnd); return TRUE; }
        if (LOWORD(wparam) == IDOK) { return TRUE; }
        break;
    case WM_CLOSE: DestroyWindow(hwnd); return TRUE;
    case WM_NCDESTROY:
        g_Studio = g_StudioViewport = NULL; g_StudioProject[0] = '\0';
        break;
    }
    return FALSE;
}

BOOL RenderStudioShow(HWND owner, HINSTANCE instance, const GEditorProject *project, const char **why)
{
    *why = "";
    if (!project || !project->name[0]) { *why = "Open a project before opening Render Studio."; return FALSE; }
    if (!ProjectEnsureStudioFolders(project->dir, why)) { return FALSE; }
    if (!g_Studio)
    {
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TREEVIEW_CLASSES};
        if (!InitCommonControlsEx(&controls)) { *why = "Could not initialize the scene outliner."; return FALSE; }
        g_Studio = CreateDialog(instance, MAKEINTRESOURCE(IDD_RENDER_STUDIO), owner, RenderStudioProc);
        if (!g_Studio) { *why = "Could not open the Render Studio window."; return FALSE; }
        g_StudioViewport = StudioViewportCreate(g_Studio, instance);
        if (!g_StudioViewport)
        {
            DestroyWindow(g_Studio);
            *why = "Could not initialize Render Studio's OpenGL viewport.";
            return FALSE;
        }
        RenderStudioLayout(g_Studio);
    }
    RenderStudioSetProject(project);
    RenderStudioRefreshScenes(NULL);
    ShowWindow(g_Studio, IsIconic(g_Studio) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(g_Studio);
    return TRUE;
}

void RenderStudioClose(void) { if (g_Studio) { DestroyWindow(g_Studio); } }

BOOL RenderStudioHandleMessage(MSG *message)
{
    BOOL own;
    if (!g_Studio || !message) { return FALSE; }
    own = message->hwnd == g_Studio || IsChild(g_Studio, message->hwnd);
    if (message->message == WM_MOUSEWHEEL && (!GetCapture() || GetCapture() == g_StudioViewport))
    {
        POINT point = {GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam)};
        HWND target = WindowFromPoint(point);
        if (target == g_StudioViewport)
        { SendMessage(g_StudioViewport, WM_MOUSEWHEEL, message->wParam, message->lParam); return TRUE; }
        if (own && target != g_Studio && !IsChild(g_Studio, target))
        {
            HWND owner = GetWindow(g_Studio, GW_OWNER);
            if (target == owner || IsChild(owner, target))
            { SendMessage(owner, WM_MOUSEWHEEL, message->wParam, message->lParam); return TRUE; }
            return FALSE; /* Let another floating editor route its wheel input. */
        }
        if (own && (target == GetDlgItem(g_Studio, IDC_STUDIO_SCENE)
            || target == GetDlgItem(g_Studio, IDC_STUDIO_IMAGES)
            || target == GetDlgItem(g_Studio, IDC_STUDIO_MODELS)
            || target == GetDlgItem(g_Studio, IDC_STUDIO_OUTLINER)))
        { SendMessage(target, WM_MOUSEWHEEL, message->wParam, message->lParam); return TRUE; }
    }
    if (!own) { return FALSE; }
    /* The main window remains usable; studio input never reaches its game
     * selection, transform or save accelerators. */
    if (!IsDialogMessage(g_Studio, message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
