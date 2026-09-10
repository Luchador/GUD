#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "modeleditor.h"
#include "modeledits.h"
#include "modelload.h"
#include "viewport.h"
#include "uveditor.h"
#include "resource.h"

typedef struct ModelEditorEntry {
    char name[MAX_PATH];
    const char *folder;
} ModelEditorEntry;

static HWND g_ModelEditor, g_ModelViewport;
static char g_ModelProject[MAX_PATH];
static ModelEditorEntry *g_ModelEntries;
static int g_ModelCount;
static int g_ModelSelected = -1;
static const int g_ModelCombos[] = { IDC_MODEL_CHARACTERS, IDC_MODEL_ITEMS, IDC_MODEL_PROPS };

static void ModelEditorClearViewport(void)
{
    if (g_ModelViewport != NULL)
    {
        ViewportSetScene(g_ModelViewport, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, TRUE);
    }
    SetWindowText(g_ModelEditor, "Model Editor");
}

static BOOL ModelEditorAddFolder(const char *folder, int category)
{
    char pattern[MAX_PATH];
    WIN32_FIND_DATA found;
    HANDLE search;
    int length = snprintf(pattern, sizeof(pattern), "%s\\models\\%s\\*.*", g_ModelProject, folder);
    if (length < 0 || length >= (int)sizeof(pattern)) { return FALSE; }
    search = FindFirstFile(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) { return TRUE; }
    do
    {
        ModelEditorEntry *grown;
        char *extension = strrchr(found.cFileName, '.');
        int index;
        LRESULT row;
        HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || extension == NULL
            || (lstrcmpi(extension, ".gltf") != 0 && lstrcmpi(extension, ".ply") != 0)) { continue; }
        *extension = '\0';
        for (index = 0; index < g_ModelCount; index++)
        {
            if (!strcmp(g_ModelEntries[index].folder, folder)
                && lstrcmpi(g_ModelEntries[index].name, found.cFileName) == 0) { break; }
        }
        if (index != g_ModelCount) { continue; } /* glTF and PLY copies share one row. */
        grown = realloc(g_ModelEntries, ((size_t)g_ModelCount + 1) * sizeof(*grown));
        if (grown == NULL) { FindClose(search); return FALSE; }
        g_ModelEntries = grown;
        lstrcpyn(grown[g_ModelCount].name, found.cFileName, MAX_PATH);
        grown[g_ModelCount].folder = folder;
        row = SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)found.cFileName);
        if (row == CB_ERR || row == CB_ERRSPACE) { FindClose(search); return FALSE; }
        SendMessage(combo, CB_SETITEMDATA, row, g_ModelCount++);
    }
    while (FindNextFile(search, &found));
    FindClose(search);
    return TRUE;
}

void ModelEditorSetProject(const char *projectdir)
{
    int category;
    BOOL complete = TRUE;
    lstrcpyn(g_ModelProject, projectdir != NULL ? projectdir : "", sizeof(g_ModelProject));
    if (g_ModelEditor == NULL) { return; }
    ModelEditorClearViewport();
    g_ModelSelected = -1;
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_EXPORT),FALSE);
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_IMPORT),FALSE);
    for (category = 0; category < 3; category++)
    {
        SendDlgItemMessage(g_ModelEditor, g_ModelCombos[category], CB_RESETCONTENT, 0, 0);
    }
    free(g_ModelEntries); g_ModelEntries = NULL; g_ModelCount = 0;
    if (g_ModelProject[0] != '\0')
    {
        complete = ModelEditorAddFolder("characters", 0)
            && ModelEditorAddFolder("guns", 1)
            && ModelEditorAddFolder("casings", 1)
            && ModelEditorAddFolder("objects", 2);
    }
    for (category = 0; category < 3; category++)
    {
        HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
        EnableWindow(combo, SendMessage(combo, CB_GETCOUNT, 0, 0) > 0);
    }
    SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, !complete
        ? "Could not list every model: check the project path or available memory."
        : g_ModelProject[0] == '\0' ? "Open or create a project to browse its models."
        : g_ModelCount == 0 ? "No models were found in this project."
        : "Choose a model. Drag either mouse button to orbit; scroll to dolly.");
}

static void ModelEditorSelect(int category, BOOL framecamera)
{
    HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
    LRESULT row = SendMessage(combo, CB_GETCURSEL, 0, 0);
    LRESULT index;
    const ModelEditorEntry *entry;
    BgVertex *vertices;
    unsigned short *tags = NULL;
    BgRenderFlags *flags = NULL;
    DWORD count = 0, corner;
    const char *why = "";
    char text[MAX_PATH + 128];
    HCURSOR previous;
    BOOL loaded = FALSE;
    int other;

    if (row == CB_ERR) { return; }
    index = SendMessage(combo, CB_GETITEMDATA, row, 0);
    if (index < 0 || index >= g_ModelCount) { return; }
    g_ModelSelected = (int)index;
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_EXPORT),TRUE);
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_IMPORT),TRUE);
    entry = &g_ModelEntries[index];
    for (other = 0; other < 3; other++)
    {
        if (other != category) { SendDlgItemMessage(g_ModelEditor, g_ModelCombos[other], CB_SETCURSEL, -1, 0); }
    }
    if (framecamera) { ModelEditorClearViewport(); }
    previous = SetCursor(LoadCursor(NULL, IDC_WAIT));
    vertices = ModelLoadProjectNamedGeometry(g_ModelProject, entry->folder, entry->name,
                                             &count, &tags, &flags, &why);
    if (vertices != NULL)
    {
        loaded = count <= INT_MAX / 3;
        for (corner = 0; loaded && corner < count * 3; corner++)
        {
            loaded = isfinite(vertices[corner].x) && isfinite(vertices[corner].y) && isfinite(vertices[corner].z);
        }
        if (!loaded) { why = "The model has invalid geometry."; }
        else if (!ViewportSetScene(g_ModelViewport, vertices, tags, flags,
                                   NULL, NULL, NULL, 0, (int)count, g_ModelProject, framecamera))
        {
            loaded = FALSE;
            why = "Not enough memory to display the model.";
        }
    }
    free(vertices); free(tags); free(flags);
    SetCursor(previous);
    if (!loaded)
    {
        snprintf(text, sizeof(text), "Could not load %s.\n\n%s", entry->name, why);
        SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, "The selected model could not be loaded.");
        MessageBox(g_ModelEditor, text, "Model Editor", MB_ICONERROR);
        return;
    }
    snprintf(text, sizeof(text), "Model Editor - %s", entry->name);
    SetWindowText(g_ModelEditor, text);
    snprintf(text, sizeof(text), "%lu triangles. Drag either mouse button to orbit; scroll to dolly.", (unsigned long)count);
    SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, text);
    if (framecamera) { SetFocus(g_ModelViewport); }
}

void ModelEditorRefreshImages(void)
{
    int category;
    if (g_ModelEditor == NULL || g_ModelSelected < 0) { return; }
    for (category = 0; category < 3; category++)
    {
        if (SendDlgItemMessage(g_ModelEditor, g_ModelCombos[category], CB_GETCURSEL, 0, 0) != CB_ERR)
        { ModelEditorSelect(category, FALSE); break; }
    }
}

static void ModelEditorTransfer(BOOL importing)
{
    OPENFILENAME ofn;
    char path[MAX_PATH] = "", message[256];
    const ModelEditorEntry *entry;
    const char *why="";
    DWORD before=0, after=0;
    BOOL ok;
    HCURSOR previous;
    int category;
    if (g_ModelSelected < 0 || g_ModelSelected >= g_ModelCount) { return; }
    entry=&g_ModelEntries[g_ModelSelected];
    if (!importing) { snprintf(path,sizeof(path),"%s.gltf",entry->name); }
    ZeroMemory(&ofn,sizeof(ofn)); ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=g_ModelEditor;
    ofn.lpstrFile=path;ofn.nMaxFile=sizeof(path);ofn.lpstrDefExt="gltf";
    ofn.lpstrTitle=importing ? "Import replacement model" : "Export model for Blender";
    ofn.lpstrFilter=importing ? "glTF models (*.glb;*.gltf)\0*.glb;*.gltf\0\0" : "glTF model (*.gltf)\0*.gltf\0\0";
    ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(importing ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);
    if (!(importing ? GetOpenFileName(&ofn) : GetSaveFileName(&ofn))) { return; }
    previous=SetCursor(LoadCursor(NULL,IDC_WAIT));
    ok=importing ? ModelEditsImport(g_ModelProject,entry->name,path,&before,&after,&why)
                 : ModelEditsExport(g_ModelProject,entry->name,path,&why);
    SetCursor(previous);
    if (!ok) { MessageBox(g_ModelEditor,why,"Model Editor",MB_ICONERROR);return; }
    if (!importing)
    {
        SetDlgItemText(g_ModelEditor,IDC_MODEL_STATUS,
            "Exported all LODs. In Blender enable Custom Properties, Attributes, UVs and Vertex Colors.");
        return;
    }
    for (category=0;category<3;category++)
    {
        if (SendDlgItemMessage(g_ModelEditor,g_ModelCombos[category],CB_GETCURSEL,0,0)!=CB_ERR)
        { ModelEditorSelect(category, TRUE);break; }
    }
    snprintf(message,sizeof(message),"Imported all LODs: %lu to %lu tris. Save Project to keep the replacement.",
        (unsigned long)before,(unsigned long)after);
    SetDlgItemText(g_ModelEditor,IDC_MODEL_STATUS,message);
    SendMessage(GetWindow(g_ModelEditor,GW_OWNER),MODELEDITOR_CHANGED,0,0);
}

static void ModelEditorLayout(HWND hwnd)
{
    static const int labels[] = { IDC_MODEL_CHARACTERS_LABEL, IDC_MODEL_ITEMS_LABEL, IDC_MODEL_PROPS_LABEL };
    RECT client, units = { 8, 80, 88, 18 };
    int category, margin, column, bottom;
    GetClientRect(hwnd, &client);
    MapDialogRect(hwnd, &units);
    margin = units.left;
    column = max(0, (client.right - margin * 4) / 3);
    bottom = max(units.top, client.bottom - units.bottom - margin * 2);
    for (category = 0; category < 3; category++)
    {
        int x = margin + category * (column + margin);
        MoveWindow(GetDlgItem(hwnd, labels[category]), x, margin, column, units.bottom, TRUE);
        MoveWindow(GetDlgItem(hwnd, g_ModelCombos[category]), x, margin + units.bottom,
                   column, units.bottom * 12, TRUE);
    }
    MoveWindow(GetDlgItem(hwnd,IDC_MODEL_EXPORT),margin,margin*2+units.bottom*2,units.right,units.bottom,TRUE);
    MoveWindow(GetDlgItem(hwnd,IDC_MODEL_IMPORT),margin*2+units.right,margin*2+units.bottom*2,units.right,units.bottom,TRUE);
    if (g_ModelViewport != NULL)
    {
        MoveWindow(g_ModelViewport, 0, units.top, client.right, max(0, bottom - units.top), TRUE);
    }
    MoveWindow(GetDlgItem(hwnd, IDC_MODEL_STATUS), margin, bottom + margin,
               max(0, client.right - units.right - margin * 3), units.bottom, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDCANCEL), max(0, client.right - units.right - margin),
               bottom + margin, units.right, units.bottom, TRUE);
}

static INT_PTR CALLBACK ModelEditorDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        g_ModelEditor = hwnd;
        return TRUE;
    case WM_SIZE:
        ModelEditorLayout(hwnd);
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = { 0, 0, 480, 240 };
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
                           FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam)==IDC_MODEL_EXPORT || LOWORD(wparam)==IDC_MODEL_IMPORT)
        { ModelEditorTransfer(LOWORD(wparam)==IDC_MODEL_IMPORT);return TRUE; }
        if (LOWORD(wparam) >= IDC_MODEL_CHARACTERS && LOWORD(wparam) <= IDC_MODEL_PROPS
            && HIWORD(wparam) == CBN_SELCHANGE)
        {
            ModelEditorSelect(LOWORD(wparam) - IDC_MODEL_CHARACTERS, TRUE);
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL) { DestroyWindow(hwnd); return TRUE; }
        if (LOWORD(wparam) == IDOK) { return TRUE; }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return TRUE;
    case WM_NCDESTROY:
        free(g_ModelEntries); g_ModelEntries = NULL; g_ModelCount = 0; g_ModelSelected = -1;
        g_ModelViewport = NULL; g_ModelEditor = NULL;
        break;
    }
    return FALSE;
}

BOOL ModelEditorShow(HWND owner, HINSTANCE instance, const char *projectdir)
{
    if (g_ModelEditor == NULL)
    {
        /* An owned, modeless window stays above GEditor and closes with it. */
        g_ModelEditor = CreateDialog(instance, MAKEINTRESOURCE(IDD_MODEL_EDITOR), owner, ModelEditorDialogProc);
        if (g_ModelEditor == NULL) { return FALSE; }
        g_ModelViewport = ViewportCreateOrbit(g_ModelEditor, instance);
        if (g_ModelViewport == NULL) { DestroyWindow(g_ModelEditor); return FALSE; }
        ModelEditorLayout(g_ModelEditor);
        ModelEditorSetProject(projectdir);
    }
    else if (lstrcmp(g_ModelProject, projectdir != NULL ? projectdir : "") != 0)
    {
        ModelEditorSetProject(projectdir);
    }
    ShowWindow(g_ModelEditor, SW_SHOWNORMAL);
    SetForegroundWindow(g_ModelEditor);
    return TRUE;
}

BOOL ModelEditorHandleMessage(MSG *message)
{
    BOOL ownmessage;
    if (g_ModelEditor == NULL || message == NULL) { return FALSE; }
    ownmessage = message->hwnd == g_ModelEditor || IsChild(g_ModelEditor, message->hwnd);
    if (message->message == WM_MOUSEWHEEL && (GetCapture() == NULL || GetCapture() == g_ModelViewport))
    {
        POINT mouse = { GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam) };
        HWND target = WindowFromPoint(mouse);
        if (target == g_ModelViewport)
        {
            SendMessage(g_ModelViewport, WM_MOUSEWHEEL, message->wParam, message->lParam);
            return TRUE;
        }
        if (ownmessage && target != g_ModelEditor && !IsChild(g_ModelEditor, target))
        {
            HWND owner = GetWindow(g_ModelEditor, GW_OWNER);
            if (UVEditorHandleMessage(message)) { return TRUE; }
            if (target == owner || IsChild(owner, target))
            { SendMessage(owner, WM_MOUSEWHEEL, message->wParam, message->lParam); }
            return TRUE;
        }
    }
    if (!ownmessage) { return FALSE; }
    /* Keep dialog navigation and keyboard shortcuts inside this window. */
    if (!IsDialogMessage(g_ModelEditor, message))
    {
        TranslateMessage(message);
        DispatchMessage(message);
    }
    return TRUE;
}
