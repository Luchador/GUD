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
static ModelSource g_ModelSource;
static DWORD g_ModelRevision;
static BOOL g_ModelAllLods;
static void ModelEditorProperties(void);
static void ModelEditorGroups(void);
static BOOL ModelEditorFaceVisible(DWORD face)
{
    return !g_ModelSource.closestpreview || g_ModelAllLods || g_ModelSource.faces[face].closest;
}
static const int g_ModelCombos[] = { IDC_MODEL_CHARACTERS, IDC_MODEL_ITEMS, IDC_MODEL_PROPS };

static void ModelEditorClearViewport(void)
{
    if (g_ModelViewport != NULL)
    {
        ViewportSetScene(g_ModelViewport, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL, TRUE);
    }
    ModelFreeSource(&g_ModelSource); g_ModelRevision = 0;
    SetWindowText(g_ModelEditor, "Model Editor");
    ModelEditorGroups(); ModelEditorProperties();
}

static BOOL ModelEditorAddFolder(const char *folder, int category)
{
    char pattern[MAX_PATH];
    WIN32_FIND_DATA found;
    HANDLE search;
    int length = snprintf(pattern, sizeof(pattern), "%s\\models\\%s\\*.gltf", g_ModelProject, folder);
    if (length < 0 || length >= (int)sizeof(pattern)) { return FALSE; }
    search = FindFirstFile(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) { return TRUE; }
    do
    {
        ModelEditorEntry *grown;
        char *extension = strrchr(found.cFileName, '.');
        LRESULT row;
        HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || extension == NULL
            || lstrcmpi(extension, ".gltf") != 0) { continue; }
        *extension = '\0';
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
    BgVertex *vertices = NULL;
    BgFaceRef *refs = NULL;
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
    ModelFreeSource(&g_ModelSource);
    if (ModelEditsReadSource(g_ModelProject, entry->name, &g_ModelSource, &g_ModelRevision, &why)
        && g_ModelSource.count <= INT_MAX / 3)
    {
        DWORD face;
        for (face = 0; face < g_ModelSource.count; face++) { count += ModelEditorFaceVisible(face); }
        vertices = malloc((size_t)max(count, 1) * 3 * sizeof(*vertices));
        tags = malloc((size_t)max(count, 1) * sizeof(*tags)); flags = malloc((size_t)max(count, 1) * sizeof(*flags));
        refs = calloc(max(count, 1), sizeof(*refs));
        if (!vertices || !tags || !flags || !refs) { free(vertices); vertices = NULL; why = "Out of memory displaying the model."; }
        else
        {
            DWORD tri = 0;
            for (face = 0; face < g_ModelSource.count; face++)
            {
                if (!ModelEditorFaceVisible(face)) { continue; }
                memcpy(vertices + tri * 3, g_ModelSource.vertices + face * 3, 3 * sizeof(*vertices));
                tags[tri] = g_ModelSource.tags[face]; flags[tri] = g_ModelSource.flags[face];
                refs[tri].faceid = face + 1; refs[tri].room = 1; tri++;
            }
        }
    }
    if (vertices != NULL)
    {
        loaded = count <= INT_MAX / 3;
        for (corner = 0; loaded && corner < count * 3; corner++)
        {
            loaded = isfinite(vertices[corner].x) && isfinite(vertices[corner].y) && isfinite(vertices[corner].z);
        }
        if (!loaded) { why = "The model has invalid geometry."; }
        else if (!ViewportSetScene(g_ModelViewport, vertices, tags, flags,
                                   refs, NULL, NULL, 0, NULL, (int)count, g_ModelProject, framecamera))
        {
            loaded = FALSE;
            why = "Not enough memory to display the model.";
        }
    }
    free(vertices); free(tags); free(flags); free(refs);
    ModelEditorGroups(); ModelEditorProperties();
    SetCursor(previous);
    if (!loaded)
    {
        ModelEditorClearViewport();
        snprintf(text, sizeof(text), "Could not load %s.\n\n%s", entry->name, why);
        SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, "The selected model could not be loaded.");
        MessageBox(g_ModelEditor, text, "Model Editor", MB_ICONERROR);
        return;
    }
    snprintf(text, sizeof(text), "Model Editor - %s", entry->name);
    SetWindowText(g_ModelEditor, text);
    snprintf(text, sizeof(text), "%lu visible triangles. Click to select faces; drag either mouse button to orbit.", (unsigned long)count);
    SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, text);
    if (framecamera) { SetFocus(g_ModelViewport); }
}

static void ModelEditorGroups(void)
{
    HWND combo = GetDlgItem(g_ModelEditor, IDC_MODEL_GROUP);
    LRESULT old = SendMessage(combo, CB_GETCURSEL, 0, 0);
    DWORD face;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (face = 0; face < g_ModelSource.count; face++)
    {
        DWORD earlier;
        char text[96];
        LRESULT row;
        if (!ModelEditorFaceVisible(face)) { continue; }
        for (earlier = 0; earlier < face; earlier++)
        {
            if (ModelEditorFaceVisible(earlier)
                && g_ModelSource.faces[earlier].list == g_ModelSource.faces[face].list
                && g_ModelSource.tags[earlier] == g_ModelSource.tags[face]) { break; }
        }
        if (earlier < face) { continue; }
        snprintf(text, sizeof(text), "Part %lu / texture 0x%03X%s",
            (unsigned long)g_ModelSource.faces[face].list + 1, BG_TEX_ID(g_ModelSource.tags[face]),
            BG_TRI_IS_SECONDARY(g_ModelSource.tags[face]) ? " (secondary)" : "");
        row = SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
        if (row >= 0) { SendMessage(combo, CB_SETITEMDATA, row, face); }
    }
    if (old < 0 || old >= SendMessage(combo, CB_GETCOUNT, 0, 0)) { old = 0; }
    SendMessage(combo, CB_SETCURSEL, old, 0);
    EnableWindow(combo, g_ModelSource.count != 0);
    EnableWindow(GetDlgItem(g_ModelEditor, IDC_MODEL_SELECT_GROUP), g_ModelSource.count != 0);
    EnableWindow(GetDlgItem(g_ModelEditor, IDC_MODEL_SELECT_ALL), g_ModelSource.count != 0);
    EnableWindow(GetDlgItem(g_ModelEditor, IDC_MODEL_LODS), g_ModelSource.closestpreview);
}
static int ModelEditorCulling(const ModelSourceFace *face)
{
    if ((face->state.geometryknown & 0x3000) != 0x3000) { return 4; }
    return (face->state.geometrymode & 0x3000) >> 12;
}
static int ModelEditorSurface(BgRenderFlags flags)
{
    if (flags & BG_RENDER_DECAL) { return 3; }
    if ((flags & (BG_RENDER_ALPHA_TEST | BG_RENDER_BLEND)) == (BG_RENDER_ALPHA_TEST | BG_RENDER_BLEND)) { return 4; }
    if (flags & BG_RENDER_ALPHA_TEST) { return 1; }
    return flags & BG_RENDER_BLEND ? 2 : 0;
}
static void ModelEditorProperties(void)
{
    static const char *culls[] = {"Disabled (two-sided)", "Cull front faces", "Cull back faces", "Cull both sides", "Inherited", "Mixed"};
    static const char *surfaces[] = {"Opaque", "Cutout", "Alpha blend", "Custom / decal", "Cutout + blend", "Mixed"};
    int count = g_ModelViewport ? ViewportGetSelectedBgFaceCount(g_ModelViewport) : 0, cull = -1, surface = -1, i;
    int depthtest = -1, depthwrite = -1;
    BOOL cullingeditable = TRUE;
    BgFaceRef *refs = count ? malloc((size_t)count * sizeof(*refs)) : NULL;
    char text[240];
    if (refs && ViewportGetSelectedBgFaces(g_ModelViewport, refs, count))
    {
        for (i = 0; i < count; i++)
        {
            DWORD face = refs[i].faceid - 1;
            int c, r, test, write;
            if (face >= g_ModelSource.count) { continue; }
            c = ModelEditorCulling(&g_ModelSource.faces[face]); r = ModelEditorSurface(g_ModelSource.flags[face]);
            cullingeditable &= c != 4;
            test = !!(g_ModelSource.flags[face] & BG_RENDER_DEPTH_TEST);
            write = !!(g_ModelSource.flags[face] & BG_RENDER_DEPTH_WRITE);
            depthtest = depthtest < 0 ? test : depthtest == test ? depthtest : 2;
            depthwrite = depthwrite < 0 ? write : depthwrite == write ? depthwrite : 2;
            cull = cull < 0 ? c : cull == c ? cull : 5;
            surface = surface < 0 ? r : surface == r ? surface : 5;
        }
    }
    free(refs);
    snprintf(text, sizeof(text), "%d face%s selected", count, count == 1 ? "" : "s");
    SetDlgItemText(g_ModelEditor, IDC_MODEL_SELECTION, text);
    if (cull >= 0 && surface >= 0)
    {
        static const char *depth[] = {"off", "on", "mixed"};
        snprintf(text, sizeof(text), "Culling: %s\r\nRender mode: %s\r\nDepth test: %s; write: %s",
            culls[cull], surfaces[surface], depth[depthtest], depth[depthwrite]);
    }
    else { lstrcpyn(text, "Select faces to inspect their settings.", sizeof(text)); }
    SetDlgItemText(g_ModelEditor, IDC_MODEL_CURRENT, text);
    SendDlgItemMessage(g_ModelEditor, IDC_MODEL_CULL, CB_SETCURSEL, 0, 0);
    SendDlgItemMessage(g_ModelEditor, IDC_MODEL_SURFACE, CB_SETCURSEL, 0, 0);
    EnableWindow(GetDlgItem(g_ModelEditor, IDC_MODEL_CULL), cull >= 0 && cullingeditable);
    EnableWindow(GetDlgItem(g_ModelEditor, IDC_MODEL_SURFACE), cull >= 0);
    EnableWindow(GetDlgItem(g_ModelEditor, IDC_MODEL_APPLY), FALSE);
}
static void ModelEditorSelectGroup(BOOL all)
{
    DWORD face, count = 0, groupface = 0;
    LRESULT row = SendDlgItemMessage(g_ModelEditor, IDC_MODEL_GROUP, CB_GETCURSEL, 0, 0);
    BgFaceRef *refs = calloc(g_ModelSource.count, sizeof(*refs));
    if (!refs) { return; }
    if (!all)
    {
        LRESULT id = row < 0 ? -1 : SendDlgItemMessage(g_ModelEditor, IDC_MODEL_GROUP, CB_GETITEMDATA, row, 0);
        if (id < 0 || (DWORD)id >= g_ModelSource.count) { free(refs); return; }
        groupface = (DWORD)id;
    }
    for (face = 0; face < g_ModelSource.count; face++)
    {
        if (ModelEditorFaceVisible(face) && (all ||
            (g_ModelSource.faces[face].list == g_ModelSource.faces[groupface].list
             && g_ModelSource.tags[face] == g_ModelSource.tags[groupface])))
        { refs[count].faceid = face + 1; refs[count++].room = 1; }
    }
    if (count) { ViewportSelectBgFaces(g_ModelViewport, refs, count); }
    free(refs);
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

static void ModelEditorApplyProperties(void)
{
    int count = ViewportGetSelectedBgFaceCount(g_ModelViewport), i;
    int cull = (int)SendDlgItemMessage(g_ModelEditor, IDC_MODEL_CULL, CB_GETCURSEL, 0, 0) - 1;
    int surface = (int)SendDlgItemMessage(g_ModelEditor, IDC_MODEL_SURFACE, CB_GETCURSEL, 0, 0) - 1;
    BgFaceRef *refs;
    DWORD *faces;
    const char *why = "";
    BOOL ok = FALSE;
    if (g_ModelSelected < 0 || count < 1 || (cull < 0 && surface < 0)) { return; }
    refs = malloc((size_t)count * sizeof(*refs)); faces = malloc((size_t)count * sizeof(*faces));
    if (!refs || !faces) { why = "Out of memory editing model properties."; }
    else if (ViewportGetSelectedBgFaces(g_ModelViewport, refs, count))
    {
        for (i = 0; i < count; i++) { faces[i] = refs[i].faceid - 1; }
        ok = ModelEditsSetProperties(g_ModelProject, g_ModelEntries[g_ModelSelected].name,
            g_ModelRevision, faces, count, cull, surface, &why);
    }
    free(refs); free(faces);
    if (!ok) { MessageBox(g_ModelEditor, why, "Model Editor", MB_ICONERROR); return; }
    ModelEditorRefreshImages();
    SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, "Face properties updated. Save Project to keep the native model changes.");
    SendMessage(GetWindow(g_ModelEditor, GW_OWNER), MODELEDITOR_CHANGED, 0, 0);
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
    int category, margin, column, bottom, panelx;
    RECT panel = {0, 0, 192, 0};
    GetClientRect(hwnd, &client);
    MapDialogRect(hwnd, &units);
    MapDialogRect(hwnd, &panel);
    panelx = max(0, client.right - panel.right);
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
        MoveWindow(g_ModelViewport, 0, units.top, panelx, max(0, bottom - units.top), TRUE);
    }
    {
        static const struct {int id, x, y, w, h;} controls[] = {
            {IDC_MODEL_SELECTION,8,16,168,18}, {IDC_MODEL_GROUP_LABEL,8,38,168,12},
            {IDC_MODEL_GROUP,8,52,168,180}, {IDC_MODEL_SELECT_GROUP,8,74,80,18},
            {IDC_MODEL_SELECT_ALL,96,74,80,18}, {IDC_MODEL_CURRENT,8,102,168,54},
            {IDC_MODEL_CULL_LABEL,8,164,168,12}, {IDC_MODEL_CULL,8,178,168,100},
            {IDC_MODEL_SURFACE_LABEL,8,204,168,12}, {IDC_MODEL_SURFACE,8,218,168,100},
            {IDC_MODEL_APPLY,8,244,168,20}, {IDC_MODEL_LODS,8,278,168,16},
            {IDC_MODEL_HINT,8,304,168,80}
        };
        size_t i;
        MoveWindow(GetDlgItem(hwnd, IDC_MODEL_PROPERTIES), panelx, units.top,
            panel.right - margin, max(0, bottom - units.top), TRUE);
        for (i = 0; i < sizeof(controls) / sizeof(controls[0]); i++)
        {
            RECT r = {controls[i].x, controls[i].y, controls[i].w, controls[i].h};
            MapDialogRect(hwnd, &r);
            MoveWindow(GetDlgItem(hwnd, controls[i].id), panelx + r.left, units.top + r.top, r.right, r.bottom, TRUE);
        }
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
        {
            static const char *culls[] = {"Keep current", "Disabled (two-sided)", "Cull back faces", "Cull front faces"};
            static const char *surfaces[] = {"Keep current", "Opaque", "Cutout", "Alpha blend"};
            int i;
            for (i = 0; i < 4; i++)
            {
                SendDlgItemMessage(hwnd, IDC_MODEL_CULL, CB_ADDSTRING, 0, (LPARAM)culls[i]);
                SendDlgItemMessage(hwnd, IDC_MODEL_SURFACE, CB_ADDSTRING, 0, (LPARAM)surfaces[i]);
            }
        }
        g_ModelAllLods = FALSE;
        return TRUE;
    case WM_SIZE:
        ModelEditorLayout(hwnd);
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = { 0, 0, 660, 520 };
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
                           FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case VIEWPORT_WM_SELECTION_CHANGED:
        ModelEditorProperties(); return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_MODEL_SELECT_GROUP || LOWORD(wparam) == IDC_MODEL_SELECT_ALL)
        { ModelEditorSelectGroup(LOWORD(wparam) == IDC_MODEL_SELECT_ALL); return TRUE; }
        if (LOWORD(wparam) == IDC_MODEL_LODS)
        {
            g_ModelAllLods = IsDlgButtonChecked(hwnd, IDC_MODEL_LODS) == BST_CHECKED;
            ModelEditorRefreshImages(); return TRUE;
        }
        if (LOWORD(wparam) == IDC_MODEL_APPLY) { ModelEditorApplyProperties(); return TRUE; }
        if ((LOWORD(wparam) == IDC_MODEL_CULL || LOWORD(wparam) == IDC_MODEL_SURFACE) && HIWORD(wparam) == CBN_SELCHANGE)
        {
            BOOL edit = SendDlgItemMessage(hwnd, IDC_MODEL_CULL, CB_GETCURSEL, 0, 0) > 0
                || SendDlgItemMessage(hwnd, IDC_MODEL_SURFACE, CB_GETCURSEL, 0, 0) > 0;
            EnableWindow(GetDlgItem(hwnd, IDC_MODEL_APPLY), edit && ViewportGetSelectedBgFaceCount(g_ModelViewport) > 0);
            return TRUE;
        }
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
        ModelFreeSource(&g_ModelSource); g_ModelRevision = 0;
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

BOOL ModelEditorOpenModel(HWND owner, HINSTANCE instance, const char *projectdir,
                          const char *name, const char **reasonout)
{
    int category;
    if (reasonout) { *reasonout = ""; }
    if (name == NULL || name[0] == '\0')
    {
        if (reasonout) { *reasonout = "This object has no model to open."; }
        return FALSE;
    }
    if (!ModelEditorShow(owner, instance, projectdir))
    {
        if (reasonout) { *reasonout = "Could not open the Model Editor window."; }
        return FALSE;
    }
    for (category = 0; category < 3; category++)
    {
        HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
        LRESULT row, count = SendMessage(combo, CB_GETCOUNT, 0, 0);
        for (row = 0; row < count; row++)
        {
            /* The combo is sorted; its rows are not indices into the asset list. */
            LRESULT index = SendMessage(combo, CB_GETITEMDATA, row, 0);
            if (index < 0 || index >= g_ModelCount
                || lstrcmpi(g_ModelEntries[index].name, name) != 0) { continue; }
            SendMessage(combo, CB_SETCURSEL, row, 0);
            if (index != g_ModelSelected || g_ModelSource.vertices == NULL)
            { ModelEditorSelect(category, TRUE); }
            else { SetFocus(g_ModelViewport); }
            return TRUE;
        }
    }
    if (reasonout) { *reasonout = "The object's model was not found in this project's model list."; }
    return FALSE;
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
