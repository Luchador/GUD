#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "renderstudio.h"
#include "studioviewport.h"

static HWND g_Studio, g_StudioViewport;
static char g_StudioProject[MAX_PATH];

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
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
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
    SetDlgItemText(g_Studio, IDC_STUDIO_SCENE, dir[0] ? "Empty scene" : "Open a project to use Render Studio.");
    SetDlgItemText(g_Studio, IDC_STUDIO_PROPERTIES, "Nothing selected.");
    EnableWindow(GetDlgItem(g_Studio, IDC_STUDIO_IMAGES), dir[0] != 0);
    EnableWindow(GetDlgItem(g_Studio, IDC_STUDIO_MODELS), dir[0] != 0);
    EnableWindow(GetDlgItem(g_Studio, IDC_STUDIO_OUTLINER), dir[0] != 0);
}

static INT_PTR CALLBACK RenderStudioProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG: g_Studio = hwnd; return TRUE;
    case WM_SIZE: RenderStudioLayout(hwnd); return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = {0, 0, 640, 320};
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
        if (own && (target == GetDlgItem(g_Studio, IDC_STUDIO_IMAGES)
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
