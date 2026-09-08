#define COBJMACROS
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>   /* IFileOpenDialog: the modern folder picker */
#include <shlobj.h>
#include <windowsx.h>  /* GET_X_LPARAM */     /* SHGetFolderPath: default to Documents */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "project.h"
#include "resource.h"
#include "viewport.h"
#include "browser.h"
#include "rightpanel.h"
#include "tooltoolbar.h"
#include "rom.h"
#include "romexport.h"
#include "bgload.h"
#include "bgdocument.h"
#include "bghistory.h"
#include "setupload.h"
#include "stanload.h"
#include "texload.h"
#include "modelload.h"
#include "objectload.h"

#define GEDITOR_CLASS  "GEditorWindow"
#define GEDITOR_TITLE  "GEditor"
#define GEDITOR_WIDTH  1920
#define GEDITOR_HEIGHT  1080
#define GEDITOR_NO_LEVEL ((DWORD)-1)

static HWND g_Viewport;
static HWND g_ToolToolbar;
static HWND g_Browser;
static HWND g_RightPanel;

/* Full-height side columns, each separated from the viewport by a
   user-draggable splitter gutter. */
#define GEDITOR_SPLITTER_W    5
#define GEDITOR_BROWSER_MIN 160
#define GEDITOR_RIGHTPANEL_MIN 180
#define GEDITOR_VIEWPORT_MIN 320
static int  g_BrowserWidth = 280;
static int  g_RightPanelWidth = 260;
static BOOL g_DraggingBrowserSplitter = FALSE;
static BOOL g_DraggingRightSplitter = FALSE;
static GEditorProject g_Project;
static DWORD g_CurrentLevelIndex = GEDITOR_NO_LEVEL;
/* Complete source segment for the selected level. Parsed viewport
   geometry is deliberately separate so selection colors and other
   display-only state can never leak into a saved background. */
static BgFile g_CurrentBg;
/* Room-aware editable geometry. Saving compiles it back into g_CurrentBg;
   the raw segment supplies preserved portal, visibility, and header data. */
static BgDocument g_CurrentBgDocument;
/* One chronological history spans both BG and setup edits. */
static EditHistory g_EditHistory;
/* Setup for the selected level, including host-native parsed views.
   Editor tools can consume it without retaining the source ROM. */
static SetupFile g_CurrentSetup;
/* Decoded stan for the selected level. Kept beside the setup so later
   editing tools can inspect tile IDs, rooms, links, and special types. */
static StanFile g_CurrentStan;
/* Portal records decoded from the selected level's saved BG segment. */
static BgPortalFile g_CurrentPortals;
/* Setup model geometry is retained so BG-only undo/redo can rebuild the scene
   without reloading every model from disk. Setup edits regenerate this layer. */
static SetupObjectGeometry g_CurrentObjects;

static void GEditorSetTitleForProject(HWND hwnd);
static void GEditorRefreshHistoryMenu(HWND hwnd);


static BOOL GEditorAppendObjectGeometry(BgDocumentRenderMesh *mesh,
                                        const SetupObjectGeometry *objects,
                                        const char **reasonout)
{
    DWORD total;
    BgVertex *combinedtris;
    unsigned short *combinedtags;
    BgFaceRef *combinedrefs;
    BgDocumentVertexRef *combinedvertices;

    *reasonout = "";
    if (objects == NULL || objects->tricount == 0)
    {
        return TRUE;
    }

    total = mesh->facecount + objects->tricount;
    if (total < mesh->facecount || total < objects->tricount)
    {
        *reasonout = "too many setup triangles for the viewport.";
        return FALSE;
    }

    combinedtris = (BgVertex *)malloc(
        (size_t)total * 3 * sizeof(*combinedtris));
    combinedtags = (unsigned short *)malloc(
        (size_t)total * sizeof(*combinedtags));
    /* Setup objects do not belong to BgDocument, so their zeroed references
       remain BG_FACE_ID_NONE and cannot be mistaken for editable faces. */
    combinedrefs = (BgFaceRef *)calloc((size_t)total,
                                       sizeof(*combinedrefs));

    combinedvertices = (BgDocumentVertexRef *)calloc((size_t)total * 3, sizeof(*combinedvertices));
    if (combinedtris == NULL || combinedtags == NULL || combinedrefs == NULL
        || combinedvertices == NULL)
    {
        free(combinedtris);
        free(combinedtags);
        free(combinedrefs);
        free(combinedvertices);
        *reasonout = "out of memory adding setup objects to the viewport.";
        return FALSE;
    }

    if (mesh->facecount > 0)
    {
        memcpy(combinedvertices, mesh->vertexrefs,
               (size_t)mesh->facecount * 3 * sizeof(*combinedvertices));
        memcpy(combinedtris, mesh->vertices,
               (size_t)mesh->facecount * 3 * sizeof(*combinedtris));
        memcpy(combinedtags, mesh->tags,
               (size_t)mesh->facecount * sizeof(*combinedtags));
        memcpy(combinedrefs, mesh->facerefs,
               (size_t)mesh->facecount * sizeof(*combinedrefs));
    }
    memcpy(combinedtris + mesh->facecount * 3, objects->tris,
           (size_t)objects->tricount * 3 * sizeof(*combinedtris));
    memcpy(combinedtags + mesh->facecount, objects->tritags,
           (size_t)objects->tricount * sizeof(*combinedtags));

    BgDocumentRenderMeshFree(mesh);
    mesh->vertices = combinedtris;
    mesh->tags = combinedtags;
    mesh->facerefs = combinedrefs;
    mesh->vertexrefs = combinedvertices;
    mesh->facecount = total;
    return TRUE;
}


static void GEditorRefreshTransformFields(void)
{
    double position[3];
    DWORD count, objectindex;
    BOOL hasposition = ViewportGetSelectionPosition(g_Viewport, position, &count);
    BOOL object = ViewportGetSelectedObject(g_Viewport, &objectindex);
    BOOL editable = hasposition && (object ? objectindex < g_CurrentSetup.objectcount
                                          : g_CurrentBgDocument.levelscale > 0.0f);
    double precision = editable && !object ? 1.0 / g_CurrentBgDocument.levelscale : 0;

    RightPanelSetTransformState(g_RightPanel, hasposition ? position : NULL,
                                count, editable, precision);
}


static void GEditorRefreshSelectionDetails(void)
{
    BgFaceRef selected;
    DWORD selectedobject;
    int count = ViewportGetSelectedBgFaceCount(g_Viewport);
    BOOL objectselected = ViewportGetSelectedObject(g_Viewport, &selectedobject);
    int components = ViewportGetSelectedComponentCount(g_Viewport);
    RightPanelSetVertexPaintMode(g_RightPanel,
        ViewportGetTool(g_Viewport) == EDITOR_TOOL_VERTEX_PAINT);
    GEditorRefreshTransformFields();
    if (objectselected && selectedobject < g_CurrentSetup.objectcount)
    {
        RightPanelSetSetupObject(g_RightPanel,
            &g_CurrentSetup.objects[selectedobject], selectedobject);
    }
    else if (objectselected && (selectedobject & SETUP_CHARACTER_SELECTION_BIT)
        && (selectedobject & ~SETUP_CHARACTER_SELECTION_BIT) < g_CurrentSetup.charactercount)
    {
        DWORD index = selectedobject & ~SETUP_CHARACTER_SELECTION_BIT;
        RightPanelSetSetupCharacter(g_RightPanel, &g_CurrentSetup.characters[index]);
    }
    else if (components > 0)
    {
        RightPanelSetBgComponentSelection(g_RightPanel,
            ViewportGetTool(g_Viewport) == EDITOR_TOOL_EDGE_SELECT, components);
    }
    else if (count == 1 && ViewportGetSingleSelectedBgFace(g_Viewport, &selected))
    {
        RightPanelSetBgTriangle(g_RightPanel, &g_CurrentBgDocument, &selected);
    }
    else
    {
        RightPanelSetBgSelectionCount(g_RightPanel, count);
    }
}


static BOOL GEditorRebuildCurrentViewportWithObjects(
    const SetupObjectGeometry *objects, const char **reasonout)
{
    BgDocumentRenderMesh mesh;
    DWORD objectfirsttriangle;

    if (!BgDocumentBuildRenderMesh(&g_CurrentBgDocument, &mesh, reasonout))
    {
        return FALSE;
    }
    objectfirsttriangle = mesh.facecount;
    if (!GEditorAppendObjectGeometry(&mesh, objects, reasonout))
    {
        BgDocumentRenderMeshFree(&mesh);
        return FALSE;
    }
    if (!ViewportSetScene(g_Viewport, mesh.vertices, mesh.tags, mesh.facerefs, mesh.vertexrefs,
                          objects->objectindices,
                          (int)objectfirsttriangle,
                          (int)mesh.facecount, g_Project.dir, FALSE))
    {
        BgDocumentRenderMeshFree(&mesh);
        *reasonout = "out of memory rebuilding the viewport.";
        return FALSE;
    }
    BgDocumentRenderMeshFree(&mesh);

    ViewportSetPortals(g_Viewport,
        g_CurrentPortals.portals != NULL ? &g_CurrentPortals : NULL);
    ViewportSetStanTiles(g_Viewport,
        g_CurrentStan.data != NULL ? &g_CurrentStan : NULL);
    ViewportSetSetupPads(g_Viewport,
        g_CurrentSetup.data != NULL ? &g_CurrentSetup : NULL,
        g_CurrentBgDocument.levelscale,
        objects->occupiedpads,
        objects->occupiedboundpads);
    GEditorRefreshSelectionDetails();
    return TRUE;
}


static BOOL GEditorRebuildCurrentViewport(const char **reasonout)
{
    return GEditorRebuildCurrentViewportWithObjects(&g_CurrentObjects,
                                                     reasonout);
}


static BOOL GEditorReloadCurrentObjectsAndViewport(const char **reasonout)
{
    SetupObjectGeometry objects;

    ZeroMemory(&objects, sizeof(objects));
    if (g_CurrentSetup.data != NULL
        && !ObjectLoadSetupGeometry(g_Project.dir, &g_CurrentSetup,
                                    &g_CurrentStan,
                                    g_CurrentBgDocument.levelscale,
                                    &objects, reasonout))
    {
        return FALSE;
    }

    if (!GEditorRebuildCurrentViewportWithObjects(&objects, reasonout))
    {
        ObjectGeometryFree(&objects);
        return FALSE;
    }

    ObjectGeometryFree(&g_CurrentObjects);
    g_CurrentObjects = objects;
    return TRUE;
}


/**
  * The one place a project gets closed, however the user asks for it:
  * the Close Project menu item, or implicitly when creating or opening
  * another project.
  */
/*
 * (Re)loads the browser from the project file and extracted asset
 * folders. Works for both freshly created and reopened projects: the
 * files on disk are the source of truth, no ROM needed.
 */
static void GEditorRefreshProjectAssets(void)
{
    BrowserLevelItem levels[ROM_MAX_LEVELS];
    TexThumb *items = NULL;
    unsigned char *pixels = NULL;
    const char *why = "";
    DWORD count;
    DWORD i;

    for (i = 0; i < g_Project.levelcount; i++)
    {
        wsprintf(levels[i].label, "%s  (%s)",
                 g_Project.levels[i].name,
                 g_Project.levels[i].world);
    }

    BrowserSetLevels(g_Browser,
                     g_Project.levelcount > 0 ? levels : NULL,
                     (int)g_Project.levelcount);

    count = TexLoadProjectThumbnails(g_Project.dir, &items, &pixels, &why);

    /* count==0 hands back no allocations; passing the NULLs through
       clears the section, which is the right display for "none". */
    BrowserSetImages(g_Browser, items, (int)count, pixels);

    /* Models: enumerate the four class folders into plain rows. */
    {
        static const char *classes[] = { "characters", "guns", "objects", "casings" };
        BrowserLevelItem models[512];
        int modelcount = 0;
        int c;

        for (c = 0; c < 4 && modelcount < 512; c++)
        {
            char pattern[MAX_PATH];
            WIN32_FIND_DATA find;
            HANDLE search;

            wsprintf(pattern, "%s\\models\\%s\\*.*", g_Project.dir, classes[c]);
            search = FindFirstFile(pattern, &find);

            if (search == INVALID_HANDLE_VALUE)
            {
                continue;
            }

            do
            {
                char stem[64];
                char *dot;
                const char *extension = strrchr(find.cFileName, '.');

                if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    || extension == NULL
                    || (lstrcmpi(extension, ".gltf") != 0
                        && lstrcmpi(extension, ".ply") != 0))
                {
                    continue;
                }

                lstrcpyn(stem, find.cFileName, sizeof(stem));
                dot = strrchr(stem, '.');
                if (dot != NULL)
                {
                    *dot = '\0';
                }

                wsprintf(models[modelcount].label, "%s  (%s)", stem, classes[c]);
                modelcount++;
            }
            while (modelcount < 512 && FindNextFile(search, &find));

            FindClose(search);
        }

        BrowserSetModels(g_Browser, modelcount > 0 ? models : NULL, modelcount);
    }
}

static void GEditorCloseProject(HWND hwnd)
{
    if (g_Project.name[0] == '\0')
    {
        return; /* Nothing open. */
    }

    SetupFileFree(&g_CurrentSetup);
    StanFileFree(&g_CurrentStan);
    BgPortalFileFree(&g_CurrentPortals);
    ObjectGeometryFree(&g_CurrentObjects);
    EditHistoryFree(&g_EditHistory);
    BgDocumentFree(&g_CurrentBgDocument);
    BgFileFree(&g_CurrentBg);
    g_CurrentLevelIndex = GEDITOR_NO_LEVEL;
    ProjectClose(&g_Project);

    BrowserSetLevels(g_Browser, NULL, 0);
    BrowserSetImages(g_Browser, NULL, 0, NULL);
    BrowserSetModels(g_Browser, NULL, 0);
    ViewportSetScene(g_Viewport, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, FALSE);
    GEditorRefreshSelectionDetails();
    GEditorRefreshHistoryMenu(hwnd);
    GEditorSetTitleForProject(hwnd);
}


/**
  * Menu command IDs. Every clickable has one and it is the number
  * that arrives in WM_COMMAND when the item is chosen.
  */
enum {
    ID_FILE_NEW_PROJECT = 40001,
    ID_FILE_OPEN_PROJECT,
    ID_FILE_SAVE_PROJECT,
    ID_FILE_CLOSE_PROJECT,
    ID_FILE_EXIT,

    ID_EDIT_UNDO,
    ID_EDIT_REDO,
    ID_VIEW_BACKFACE_CULLING,

    ID_TOOLS_CREATE_ROM
};


/* What the New Project dialog collects. */
typedef struct NewProjectInfo {
    char name[GEDITOR_NAME_MAX];
    char location[MAX_PATH];
    char rompath[MAX_PATH];
    RomInfo rominfo;   /* filled by the dialog's Create validation */
} NewProjectInfo;


/*
 * Builds the top menu bar.
 */
static HMENU GEditorCreateMenuBar(void)
{
    HMENU menubar;
    HMENU filemenu;
    HMENU editmenu;
    HMENU viewmenu;
    HMENU toolsmenu;

    menubar = CreateMenu();
    filemenu = CreatePopupMenu();
    editmenu = CreatePopupMenu();
    viewmenu = CreatePopupMenu();
    toolsmenu = CreatePopupMenu();

    /* MF_STRING items carry a command ID. '&' marks the Alt mnemonic. */
    AppendMenu(filemenu, MF_STRING, ID_FILE_NEW_PROJECT, "&New Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_OPEN_PROJECT, "&Open Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_SAVE_PROJECT, "&Save Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_CLOSE_PROJECT, "&Close Project");
    AppendMenu(filemenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(filemenu, MF_STRING, ID_FILE_EXIT, "E&xit");

    AppendMenu(editmenu, MF_STRING, ID_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenu(editmenu, MF_STRING, ID_EDIT_REDO, "&Redo\tCtrl+Y");

    AppendMenu(viewmenu, MF_STRING, ID_VIEW_BACKFACE_CULLING, "&Backface Culling");

    AppendMenu(toolsmenu, MF_STRING, ID_TOOLS_CREATE_ROM, "&Create ROM...");

    AppendMenu(menubar, MF_POPUP, (UINT_PTR)filemenu, "&File");
    AppendMenu(menubar, MF_POPUP, (UINT_PTR)editmenu, "&Edit");
    AppendMenu(menubar, MF_POPUP, (UINT_PTR)viewmenu, "&View");
    AppendMenu(menubar, MF_POPUP, (UINT_PTR)toolsmenu, "&Tools");

    return menubar;
}


static HACCEL GEditorCreateAccelerators(void)
{
    ACCEL entries[3];

    ZeroMemory(entries, sizeof(entries));
    entries[0].fVirt = FVIRTKEY | FCONTROL;
    entries[0].key = 'Z';
    entries[0].cmd = ID_EDIT_UNDO;
    entries[1].fVirt = FVIRTKEY | FCONTROL;
    entries[1].key = 'Y';
    entries[1].cmd = ID_EDIT_REDO;
    entries[2].fVirt = FVIRTKEY | FCONTROL | FSHIFT;
    entries[2].key = 'Z';
    entries[2].cmd = ID_EDIT_REDO;
    return CreateAcceleratorTable(entries, 3);
}


static void GEditorUpdateHistoryMenu(HMENU menu)
{
    const char *undoaction;
    const char *redoaction;
    char label[128];

    if (GetMenuState(menu, ID_EDIT_UNDO, MF_BYCOMMAND) == (UINT)-1)
    {
        return;
    }

    undoaction = EditHistoryGetUndoAction(&g_EditHistory);
    redoaction = EditHistoryGetRedoAction(&g_EditHistory);

    if (undoaction[0] != '\0')
    {
        snprintf(label, sizeof(label), "&Undo %s\tCtrl+Z", undoaction);
    }
    else
    {
        snprintf(label, sizeof(label), "&Undo\tCtrl+Z");
    }
    ModifyMenu(menu, ID_EDIT_UNDO, MF_BYCOMMAND | MF_STRING,
               ID_EDIT_UNDO, label);
    EnableMenuItem(menu, ID_EDIT_UNDO, MF_BYCOMMAND
        | (EditHistoryCanUndo(&g_EditHistory) ? MF_ENABLED : MF_GRAYED));

    if (redoaction[0] != '\0')
    {
        snprintf(label, sizeof(label), "&Redo %s\tCtrl+Y", redoaction);
    }
    else
    {
        snprintf(label, sizeof(label), "&Redo\tCtrl+Y");
    }
    ModifyMenu(menu, ID_EDIT_REDO, MF_BYCOMMAND | MF_STRING,
               ID_EDIT_REDO, label);
    EnableMenuItem(menu, ID_EDIT_REDO, MF_BYCOMMAND
        | (EditHistoryCanRedo(&g_EditHistory) ? MF_ENABLED : MF_GRAYED));
}


static void GEditorRefreshHistoryMenu(HWND hwnd)
{
    HMENU menubar = GetMenu(hwnd);
    HMENU editmenu;

    if (menubar == NULL)
    {
        return;
    }

    editmenu = GetSubMenu(menubar, 1);
    if (editmenu != NULL)
    {
        GEditorUpdateHistoryMenu(editmenu);
        DrawMenuBar(hwnd);
    }
}


/*
 * Returns TRUE if name is usable as a project name. The rules are the
 * ones Windows imposes on file names, because the name becomes part of
 * the .gep file name. There must be text and the text can't use reserved characters.
 */
static BOOL GEditorNameIsValid(const char *name, const char **reasonout)
{
    static const char reserved[] = "\\/:*?\"<>|";
    const char *p;

    for (p = name; *p == ' ' || *p == '\t'; p++)
    {
        /* skip leading blanks */
    }

    if (*p == '\0')
    {
        *reasonout = "Enter a project name.";
        return FALSE;
    }

    for (p = name; *p != '\0'; p++)
    {
        if (strchr(reserved, *p) != NULL)
        {
            *reasonout = "A name cannot contain \\ / : * ? \" < > or |";
            return FALSE;
        }
    }

    *reasonout = "";
    return TRUE;
}


/* TRUE if path names an existing directory. */
static BOOL GEditorDirectoryExists(const char *path)
{
    DWORD attrs = GetFileAttributes(path);

    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}


/*
 * Folder picker. Fills pathout and returns TRUE if a folder was chosen.
 */
static BOOL GEditorPromptForFolder(HWND owner, const WCHAR *title,
                                   char *pathout, int pathmax)
{
    IFileOpenDialog *dlg = NULL;
    IShellItem *item = NULL;
    PWSTR wpath = NULL;
    DWORD opts = 0;
    BOOL ok = FALSE;

    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IFileOpenDialog, (void **)&dlg)))
    {
        return FALSE;
    }

    if (SUCCEEDED(IFileOpenDialog_GetOptions(dlg, &opts)))
    {
        /* FORCEFILESYSTEM refuses virtual folders (Libraries, This PC)
           that have no path we could write files into. */
        IFileOpenDialog_SetOptions(dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    IFileOpenDialog_SetTitle(dlg, title);

    if (SUCCEEDED(IFileOpenDialog_Show(dlg, owner))
        && SUCCEEDED(IFileOpenDialog_GetResult(dlg, &item)))
    {
        /* The shell speaks UTF-16; the rest of GEditor is ANSI. */
        if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &wpath)))
        {
            ok = WideCharToMultiByte(CP_ACP, 0, wpath, -1, pathout, pathmax, NULL, NULL) > 0;
            CoTaskMemFree(wpath);
        }
        IShellItem_Release(item);
    }

    IFileOpenDialog_Release(dlg);
    return ok;
}


/*
 * Ask the user for a GUD ROM.
 */
static BOOL GEditorPromptForRom(HWND hwnd, char *pathout, DWORD pathmax)
{
    OPENFILENAME ofn;

    /* The dialog uses this buffer as the initial contents of the File
       name box, so it must be a valid (here, empty) string. */
    pathout[0] = '\0';

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;            /* modal to our window */
    ofn.lpstrFile   = pathout;
    ofn.nMaxFile    = pathmax;
    ofn.lpstrTitle  = "Select GUD ROM";
    ofn.lpstrDefExt = "z64";           /* appended if the user types no extension */

    /* Filter: pairs of "description\0pattern\0", terminated by an extra
       \0. The literal below already ends in one implicit terminator, so
       the explicit \0 at the end supplies the required double null. */
    ofn.lpstrFilter = "N64 ROMs (*.z64)\0*.z64\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;              /* 1-based: start on the .gep filter */

    ofn.Flags = OFN_EXPLORER          /* Modern Explorer-style dialog. */
              | OFN_FILEMUSTEXIST     /* Refuse names that do not exist. */
              | OFN_PATHMUSTEXIST
              | OFN_HIDEREADONLY      /* Hide the vestigial read-only box. */
              | OFN_NOCHANGEDIR;      /* Do not move our working directory. */

    return GetOpenFileName(&ofn);
}


/*
 * Re-checks the edit box and updates the two things that depend on it:
 * whether Create Project is clickable, and what the warning line says.
 * Called once when the dialog opens and again on every keystroke, so
 * the button is never enabled for a name we would reject.
 */
static void GEditorUpdateNameValidity(HWND hdlg)
{
    char name[GEDITOR_NAME_MAX];
    char location[MAX_PATH];
    const char *reason;
    BOOL valid;

    GetDlgItemText(hdlg, IDC_PROJECT_NAME, name, sizeof(name));
    valid = GEditorNameIsValid(name, &reason);

    /* The name check reports first; the location check only runs on
       a good name, so the warning line shows one problem at a time. */
    if (valid)
    {
        GetDlgItemText(hdlg, IDC_PROJECT_LOCATION, location, sizeof(location));

        if (location[0] == '\0')
        {
            reason = "Choose a location for the project.";
            valid = FALSE;
        }
        else if (!GEditorDirectoryExists(location))
        {
            reason = "That location does not exist.";
            valid = FALSE;
        }
    }

    if (valid)
    {
        char rompath[MAX_PATH];
        DWORD attrs;

        GetDlgItemText(hdlg, IDC_PROJECT_ROM, rompath, sizeof(rompath));
        attrs = GetFileAttributes(rompath);

        if (rompath[0] == '\0')
        {
            reason = "Choose the GUD ROM to extract assets from.";
            valid = FALSE;
        }
        else if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            reason = "That ROM file does not exist.";
            valid = FALSE;
        }
    }

    EnableWindow(GetDlgItem(hdlg, IDC_CREATE_PROJECT), valid);
    SetDlgItemText(hdlg, IDC_NAME_WARNING, reason);
}


static INT_PTR CALLBACK GEditorNewProjectProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
    NewProjectInfo *info;

    switch (msg)
    {
    case WM_INITDIALOG:
        /* Remember where to write the chosen name. */
        SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)lparam);

        SendDlgItemMessage(hdlg, IDC_PROJECT_NAME, EM_LIMITTEXT, GEDITOR_NAME_MAX - 1, 0);
        SetDlgItemText(hdlg, IDC_PROJECT_NAME, "GE Project");

        /* Default the location to the user's Documents folder. */
        {
            char docs[MAX_PATH];

            SendDlgItemMessage(hdlg, IDC_PROJECT_LOCATION, EM_LIMITTEXT, MAX_PATH - 1, 0);
            if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, docs))
                || SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, docs)))
            {
                SetDlgItemText(hdlg, IDC_PROJECT_LOCATION, docs);
            }
        }

        /* Select the default so typing replaces it. */
        SendDlgItemMessage(hdlg, IDC_PROJECT_NAME, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(hdlg, IDC_PROJECT_NAME));

        GEditorUpdateNameValidity(hdlg);
        return FALSE; /* FALSE because we set the focus ourselves */

    case WM_CTLCOLORSTATIC:
        /* Paint the warning line red. lparam is the control's HWND. */
        if (GetDlgCtrlID((HWND)lparam) == IDC_NAME_WARNING)
        {
            HDC hdc = (HDC)wparam;
            SetTextColor(hdc, RGB(192, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
        }
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_PROJECT_NAME:
        case IDC_PROJECT_LOCATION:
        case IDC_PROJECT_ROM:
            /* EN_CHANGE arrives on every edit, so validation is live
               for both boxes - including a path pasted into Location. */
            if (HIWORD(wparam) == EN_CHANGE)
            {
                GEditorUpdateNameValidity(hdlg);
            }
            return TRUE;

        case IDC_BROWSE_LOCATION:
        {
            char folder[MAX_PATH];

            if (GEditorPromptForFolder(hdlg, L"Choose Project Location",
                                       folder, sizeof(folder)))
            {
                /* Setting the text fires EN_CHANGE, which revalidates. */
                SetDlgItemText(hdlg, IDC_PROJECT_LOCATION, folder);
            }
            return TRUE;
        }

        case IDC_BROWSE_ROM:
        {
            char folder[MAX_PATH];

            if (GEditorPromptForRom(hdlg, folder, sizeof(folder)))
            {
                /* Setting the text fires EN_CHANGE, which revalidates. */
                SetDlgItemText(hdlg, IDC_PROJECT_ROM, folder);
            }
            return TRUE;
        }

        case IDC_CREATE_PROJECT:
        {
            char rompath[MAX_PATH];
            const char *reason;

            /* The expensive checks run once, here - reading the whole
               ROM and walking the manifest. A failure reports on the
               warning line and keeps the dialog open for correction.
               On success the parsed RomInfo (including the level list)
               rides out to the caller inside NewProjectInfo. */
            info = (NewProjectInfo *)GetWindowLongPtr(hdlg, DWLP_USER);
            GetDlgItemText(hdlg, IDC_PROJECT_ROM, rompath, sizeof(rompath));
            if (!RomValidate(rompath, &info->rominfo, &reason))
            {
                SetDlgItemText(hdlg, IDC_NAME_WARNING, reason);
                return TRUE;
            }
        }
            /* fall through to the original create path */
            info = (NewProjectInfo *)GetWindowLongPtr(hdlg, DWLP_USER);
            GetDlgItemText(hdlg, IDC_PROJECT_NAME, info->name, sizeof(info->name));
            GetDlgItemText(hdlg, IDC_PROJECT_LOCATION, info->location, sizeof(info->location));
            GetDlgItemText(hdlg, IDC_PROJECT_ROM, info->rompath, sizeof(info->rompath));
            EndDialog(hdlg, IDOK);
            return TRUE;

        case IDCANCEL:
            /* Escape key and the title bar's close button. */
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}


/*
 * Shows the New Project dialog. Fills nameout (GEDITOR_NAME_MAX chars)
 * and returns TRUE if the user created a project.
 */
static BOOL GEditorPromptForNewProject(HWND hwnd, NewProjectInfo *info)
{
    INT_PTR result;

    ZeroMemory(info, sizeof(*info));

    result = DialogBoxParam(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(IDD_NEW_PROJECT),
                            hwnd,
                            GEditorNewProjectProc,
                            (LPARAM)info);

    if (result == -1)
    {
        MessageBox(hwnd, "Dialog resource missing (build problem).",
                   GEDITOR_TITLE, MB_ICONERROR);
    }

    return (result == IDOK);
}


/*
 * Ask the user for a .gep project file.
 */
static BOOL GEditorPromptForProject(HWND hwnd, char *pathout, DWORD pathmax)
{
    OPENFILENAME ofn;

    /* The dialog uses this buffer as the initial contents of the File
       name box, so it must be a valid (here, empty) string. */
    pathout[0] = '\0';

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;            /* modal to our window */
    ofn.lpstrFile   = pathout;
    ofn.nMaxFile    = pathmax;
    ofn.lpstrTitle  = "Open Project";
    ofn.lpstrDefExt = "gep";           /* appended if the user types no extension */

    /* Filter: pairs of "description\0pattern\0", terminated by an extra
       \0. The literal below already ends in one implicit terminator, so
       the explicit \0 at the end supplies the required double null. */
    ofn.lpstrFilter = "GEditor Projects (*.gep)\0*.gep\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;              /* 1-based: start on the .gep filter */

    ofn.Flags = OFN_EXPLORER          /* Modern Explorer-style dialog. */
              | OFN_FILEMUSTEXIST     /* Refuse names that do not exist. */
              | OFN_PATHMUSTEXIST
              | OFN_HIDEREADONLY      /* Hide the vestigial read-only box. */
              | OFN_NOCHANGEDIR;      /* Do not move our working directory. */

    return GetOpenFileName(&ofn);
}


static void GEditorSetTitleForProject(HWND hwnd)
{
    char title[GEDITOR_NAME_MAX + 32];
    
    if (g_Project.name[0] != '\0')
    {
        snprintf(title, sizeof(title), "%s - %s", GEDITOR_TITLE, g_Project.name);
    }
    else
    {
        snprintf(title, sizeof(title), "%s", GEDITOR_TITLE);
    }

    SetWindowText(hwnd, title);
}


/*
 * Saves only the project's metadata and the resource files belonging
 * to the currently open level. Other extracted levels and shared
 * image/model assets remain untouched.
 */
static BOOL GEditorSaveProject(HWND hwnd)
{
    const char *why = "";

    if (g_Project.name[0] == '\0')
    {
        return FALSE;
    }

    if (g_CurrentLevelIndex < g_Project.levelcount)
    {
        BgFile compiled;
        const BgFile *bgtosave = &g_CurrentBg;

        ZeroMemory(&compiled, sizeof(compiled));

        if (g_CurrentBgDocument.dirty)
        {
            if (!BgDocumentCompile(&g_CurrentBgDocument, &g_CurrentBg,
                                   &compiled, &why))
            {
                MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
                return FALSE;
            }
            bgtosave = &compiled;
        }

        /* Portals live inside this complete BG segment, so they are
           preserved by the same write rather than as a sidecar file. */
        if (!BgSaveProjectFile(g_Project.dir, bgtosave, &why))
        {
            BgFileFree(&compiled);
            MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
            return FALSE;
        }

        if (compiled.data != NULL)
        {
            BgFileFree(&g_CurrentBg);
            g_CurrentBg = compiled;
            ZeroMemory(&compiled, sizeof(compiled));
        }
        EditHistoryMarkBgSaved(&g_EditHistory, &g_CurrentBgDocument);

        if (g_CurrentSetup.data != NULL)
        {
            if (!SetupSaveProjectFile(g_Project.dir, &g_CurrentSetup, &why))
            {
                MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
                GEditorRefreshHistoryMenu(hwnd);
                return FALSE;
            }
            EditHistoryMarkSetupSaved(&g_EditHistory, &g_CurrentSetup);
        }

        if (g_CurrentStan.data != NULL
            && !StanSaveProjectFile(g_Project.dir, &g_CurrentStan, &why))
        {
            MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
            GEditorRefreshHistoryMenu(hwnd);
            return FALSE;
        }

        GEditorRefreshHistoryMenu(hwnd);
    }

    if (!ProjectSave(&g_Project, &why))
    {
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return FALSE;
    }

    return TRUE;
}


typedef struct CreateRomInfo {
    const GEditorProject *project;
} CreateRomInfo;


/* Live validation for the Create ROM dialog. The conversion button is
 * reachable only when its filename and destination are both usable. */
static void GEditorUpdateRomExportValidity(HWND hdlg)
{
    char name[ROM_EXPORT_NAME_MAX];
    char directory[MAX_PATH];
    char outputpath[MAX_PATH];
    const char *reason = "";
    BOOL valid;

    GetDlgItemText(hdlg, IDC_ROM_NAME, name, sizeof(name));
    GetDlgItemText(hdlg, IDC_ROM_OUTPUT_DIR, directory, sizeof(directory));

    valid = RomExportDestinationIsValid(&g_Project, directory, name,
                                        outputpath, sizeof(outputpath),
                                        &reason);

    EnableWindow(GetDlgItem(hdlg, IDC_CREATE_ROM), valid);
    SetDlgItemText(hdlg, IDC_ROM_WARNING, reason);
}


static INT_PTR CALLBACK GEditorCreateRomProc(HWND hdlg, UINT msg,
                                             WPARAM wparam, LPARAM lparam)
{
    CreateRomInfo *info;

    switch (msg)
    {
    case WM_INITDIALOG:
        info = (CreateRomInfo *)lparam;
        SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)info);

        SendDlgItemMessage(hdlg, IDC_ROM_NAME, EM_LIMITTEXT,
                           ROM_EXPORT_NAME_MAX - 1, 0);
        SendDlgItemMessage(hdlg, IDC_ROM_OUTPUT_DIR, EM_LIMITTEXT,
                           MAX_PATH - 1, 0);
        SetDlgItemText(hdlg, IDC_ROM_NAME, info->project->name);
        SetDlgItemText(hdlg, IDC_ROM_OUTPUT_DIR, info->project->dir);
        GEditorUpdateRomExportValidity(hdlg);
        return TRUE;

    case WM_CTLCOLORSTATIC:
        if (GetDlgCtrlID((HWND)lparam) == IDC_ROM_WARNING)
        {
            HDC hdc = (HDC)wparam;
            SetTextColor(hdc, RGB(192, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
        }
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_ROM_NAME:
        case IDC_ROM_OUTPUT_DIR:
            if (HIWORD(wparam) == EN_CHANGE)
            {
                GEditorUpdateRomExportValidity(hdlg);
            }
            return TRUE;

        case IDC_BROWSE_ROM_OUTPUT:
        {
            char folder[MAX_PATH];

            if (GEditorPromptForFolder(hdlg, L"Choose ROM Output Directory",
                                       folder, sizeof(folder)))
            {
                SetDlgItemText(hdlg, IDC_ROM_OUTPUT_DIR, folder);
            }
            return TRUE;
        }

        case IDC_CREATE_ROM:
        {
            char name[ROM_EXPORT_NAME_MAX];
            char directory[MAX_PATH];
            char outputpath[MAX_PATH];
            char message[MAX_PATH + 64];
            const char *reason = "";
            DWORD attrs;

            info = (CreateRomInfo *)GetWindowLongPtr(hdlg, DWLP_USER);
            GetDlgItemText(hdlg, IDC_ROM_NAME, name, sizeof(name));
            GetDlgItemText(hdlg, IDC_ROM_OUTPUT_DIR, directory,
                           sizeof(directory));

            if (!RomExportDestinationIsValid(info->project, directory, name,
                                             outputpath, sizeof(outputpath),
                                             &reason))
            {
                SetDlgItemText(hdlg, IDC_ROM_WARNING, reason);
                return TRUE;
            }

            attrs = GetFileAttributes(outputpath);
            if (attrs != INVALID_FILE_ATTRIBUTES)
            {
                if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                {
                    MessageBox(hdlg, "A directory already uses that ROM name.",
                               GEDITOR_TITLE, MB_ICONERROR);
                    return TRUE;
                }

                if (MessageBox(hdlg,
                        "That ROM already exists. Overwrite it?",
                        GEDITOR_TITLE,
                        MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
                {
                    return TRUE;
                }
            }

            /* Export consumes the project files, so first flush the
             * currently open level's retained raw buffers to them. */
            if (!GEditorSaveProject(hdlg))
            {
                return TRUE;
            }

            if (!RomExportCreate(info->project, name, directory,
                                 outputpath, sizeof(outputpath), &reason))
            {
                MessageBox(hdlg, reason, GEDITOR_TITLE, MB_ICONERROR);
                return TRUE;
            }

            snprintf(message, sizeof(message), "ROM created successfully:\n%s",
                     outputpath);
            MessageBox(hdlg, message, GEDITOR_TITLE, MB_ICONINFORMATION);
            EndDialog(hdlg, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}


static BOOL GEditorEnsureProjectBaseRom(HWND hwnd)
{
    char path[MAX_PATH];
    const char *reason = "";
    RomFile rom;
    BOOL ok;

    if (RomExportHasProjectBase(&g_Project))
    {
        return TRUE;
    }

    if (MessageBox(hwnd,
            "This project predates ROM export and has no base.z64. "
            "Select the GUD ROM used to create it. GEditor will retain "
            "a project copy and will only ask once.",
            GEDITOR_TITLE, MB_ICONINFORMATION | MB_OKCANCEL) != IDOK)
    {
        return FALSE;
    }

    if (!GEditorPromptForRom(hwnd, path, sizeof(path)))
    {
        return FALSE;
    }

    if (!RomLoad(path, &rom, &reason))
    {
        MessageBox(hwnd, reason, GEDITOR_TITLE, MB_ICONERROR);
        return FALSE;
    }

    ok = RomExportStoreProjectBase(&g_Project, &rom, &reason);
    RomFree(&rom);

    if (!ok)
    {
        MessageBox(hwnd, reason, GEDITOR_TITLE, MB_ICONERROR);
    }

    return ok;
}


static void GEditorPromptForRomExport(HWND hwnd)
{
    CreateRomInfo info;
    INT_PTR result;

    info.project = &g_Project;
    result = DialogBoxParam(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(IDD_CREATE_ROM), hwnd,
                            GEditorCreateRomProc, (LPARAM)&info);

    if (result == -1)
    {
        MessageBox(hwnd, "Create ROM dialog resource missing (build problem).",
                   GEDITOR_TITLE, MB_ICONERROR);
    }
}


static void GEditorLayout(HWND hwnd)
{
    RECT rc;
    int sidewidth;
    int viewportleft;
    int viewportright;
    int panelleft;
    int toolbarheight;
    int viewportwidth;

    GetClientRect(hwnd, &rc);

    /* Space available to the two sidebars after reserving a useful
       viewport and both splitter gutters. */
    sidewidth = rc.right - GEDITOR_VIEWPORT_MIN - GEDITOR_SPLITTER_W * 2;

    if (sidewidth < GEDITOR_BROWSER_MIN + GEDITOR_RIGHTPANEL_MIN)
    {
        /* On an unusually narrow window, share what remains instead of
           allowing a negative viewport width or overlapping children. */
        if (sidewidth < 0)
        {
            sidewidth = 0;
        }

        g_BrowserWidth = sidewidth * GEDITOR_BROWSER_MIN
                       / (GEDITOR_BROWSER_MIN + GEDITOR_RIGHTPANEL_MIN);
        g_RightPanelWidth = sidewidth - g_BrowserWidth;
    }
    else
    {
        if (g_BrowserWidth < GEDITOR_BROWSER_MIN)
        {
            g_BrowserWidth = GEDITOR_BROWSER_MIN;
        }
        if (g_RightPanelWidth < GEDITOR_RIGHTPANEL_MIN)
        {
            g_RightPanelWidth = GEDITOR_RIGHTPANEL_MIN;
        }

        if (g_DraggingBrowserSplitter)
        {
            int maxbrowser = sidewidth - g_RightPanelWidth;

            if (g_BrowserWidth > maxbrowser)
            {
                g_BrowserWidth = maxbrowser;
            }
        }
        else
        {
            int maxpanel = sidewidth - g_BrowserWidth;

            if (g_RightPanelWidth > maxpanel)
            {
                g_RightPanelWidth = maxpanel;
            }
        }
    }

    viewportleft = g_BrowserWidth + GEDITOR_SPLITTER_W;
    panelleft = rc.right - g_RightPanelWidth;
    viewportright = panelleft - GEDITOR_SPLITTER_W;
    viewportwidth = viewportright > viewportleft ? viewportright - viewportleft : 0;
    toolbarheight = rc.bottom < TOOLTOOLBAR_HEIGHT ? rc.bottom : TOOLTOOLBAR_HEIGHT;

    if (g_Browser != NULL)
    {
        MoveWindow(g_Browser, 0, 0, g_BrowserWidth, rc.bottom, TRUE);
    }

    if (g_ToolToolbar != NULL)
    {
        MoveWindow(g_ToolToolbar, viewportleft, 0,
                   viewportwidth, toolbarheight, TRUE);
    }

    if (g_Viewport != NULL)
    {
        MoveWindow(g_Viewport, viewportleft, toolbarheight,
                   viewportwidth, rc.bottom - toolbarheight, TRUE);
    }

    if (g_RightPanel != NULL)
    {
        MoveWindow(g_RightPanel, panelleft, 0,
                   g_RightPanelWidth, rc.bottom, TRUE);
    }

    /* The strips between the three children are bare main-window
       client area; the mouse handlers below treat them as gutters. */
}

static BOOL GEditorInBrowserSplitter(int x)
{
    return x >= g_BrowserWidth && x < g_BrowserWidth + GEDITOR_SPLITTER_W;
}

static BOOL GEditorInRightSplitter(HWND hwnd, int x)
{
    RECT client;
    int splitterleft;

    GetClientRect(hwnd, &client);
    splitterleft = client.right - g_RightPanelWidth - GEDITOR_SPLITTER_W;
    return x >= splitterleft && x < splitterleft + GEDITOR_SPLITTER_W;
}


static void GEditorApplyHistoryStep(HWND hwnd, BOOL redo)
{
    const char *why = "";
    const char *restorewhy = "";
    EditHistoryAsset asset = EDIT_HISTORY_ASSET_NONE;
    EditHistoryAsset restoreasset = EDIT_HISTORY_ASSET_NONE;
    BOOL changed;

    changed = redo
        ? EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument,
                          &g_CurrentSetup, &asset, &why)
        : EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument,
                          &g_CurrentSetup, &asset, &why);

    if (!changed)
    {
        return;
    }

    if (!(asset == EDIT_HISTORY_ASSET_SETUP
            ? GEditorReloadCurrentObjectsAndViewport(&why)
            : GEditorRebuildCurrentViewport(&why)))
    {
        /* A history step is not useful if its geometry cannot be presented.
           The inverse transfer cannot allocate here: the destination stack
           just released one entry and already owns sufficient capacity. */
        if (redo)
        {
            EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument,
                            &g_CurrentSetup, &restoreasset, &restorewhy);
        }
        else
        {
            EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument,
                            &g_CurrentSetup, &restoreasset, &restorewhy);
        }

        if (restoreasset == EDIT_HISTORY_ASSET_SETUP)
        {
            GEditorReloadCurrentObjectsAndViewport(&restorewhy);
        }
        else if (restoreasset == EDIT_HISTORY_ASSET_BG)
        {
            GEditorRebuildCurrentViewport(&restorewhy);
        }

        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    }

    GEditorRefreshHistoryMenu(hwnd);
}


static void GEditorDeleteSelectedBgFaces(HWND hwnd)
{
    EditHistoryTransaction transaction;
    BgFaceRef *selected;
    const char *why = "";
    const char *restorewhy = "";
    DWORD deleted = 0;
    int count = ViewportGetSelectedBgFaceCount(g_Viewport);
    const char *action = count == 1 ? "Delete BG Face" : "Delete BG Faces";

    if (count <= 0)
    {
        return;
    }

    selected = (BgFaceRef *)malloc((size_t)count * sizeof(*selected));
    if (selected == NULL)
    {
        MessageBox(hwnd, "Out of memory reading the BG selection.",
                   GEDITOR_TITLE, MB_ICONERROR);
        return;
    }

    if (!ViewportGetSelectedBgFaces(g_Viewport, selected, count)
        || !EditHistoryBeginBgEdit(&g_EditHistory, &g_CurrentBgDocument,
                                   action, &transaction, &why))
    {
        free(selected);
        if (why[0] == '\0')
        {
            why = "The selected BG faces could not be read.";
        }
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return;
    }

    if (!BgDocumentDeleteFaces(&g_CurrentBgDocument, selected,
                               (DWORD)count, &deleted, &why)
        || deleted != (DWORD)count
        || !GEditorRebuildCurrentViewport(&why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument,
                                  &g_CurrentSetup, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument,
                                &g_CurrentSetup);
        GEditorRebuildCurrentViewport(&restorewhy);
        free(selected);
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        GEditorRefreshHistoryMenu(hwnd);
        return;
    }

    free(selected);
    GEditorRefreshHistoryMenu(hwnd);
}


/* Both the panel and gizmo commit through the same asset/history path. Drag
 * previews live only in the viewport; there is exactly one edit on release. */
static BOOL GEditorTranslateSelection(HWND hwnd, const double offset[3])
{
    EditHistoryTransaction transaction;
    double applied[3];
    SetupObjectGeometry objects;
    BgDocumentVertexRef *vertices = NULL;
    DWORD count = 0, moved = 0, objectindex;
    BOOL object = ViewportGetSelectedObject(g_Viewport, &objectindex);
    EditorTool tool = ViewportGetTool(g_Viewport);
    const char *why = "", *restorewhy = "";
    const char *action = object ? "Move Object" : tool == EDITOR_TOOL_VERTEX_SELECT
        ? "Move BG Vertices" : tool == EDITOR_TOOL_EDGE_SELECT ? "Move BG Edges" : "Move BG Faces";
    int axis;
    ZeroMemory(&transaction, sizeof(transaction));
    ZeroMemory(&objects, sizeof(objects));
    for (axis=0; axis<3; axis++) { applied[axis]=0; }
    if (tool == EDITOR_TOOL_VERTEX_PAINT || (object && objectindex >= g_CurrentSetup.objectcount)) { return FALSE; }
    if (offset[0]==0 && offset[1]==0 && offset[2]==0) { return TRUE; }
    if (object)
    {
        if (!EditHistoryBeginSetupEdit(&g_EditHistory,&g_CurrentSetup,action,&transaction,&why)) { goto fail; }
        if (!ObjectTranslateSetupObject(g_Project.dir,&g_CurrentSetup,&g_CurrentStan,
                g_CurrentBgDocument.levelscale,&g_CurrentObjects,objectindex,offset,&objects,&why)) { goto rollback; }
        moved = 1;
        for (axis=0; axis<3; axis++) { applied[axis]=offset[axis]; }
    }
    else
    {
        vertices=ViewportGetMoveVertices(g_Viewport,&count);
        if (vertices == NULL) { why="There are no editable selected vertices."; goto fail; }
        if (!EditHistoryBeginBgEdit(&g_EditHistory,&g_CurrentBgDocument,action,&transaction,&why)) { goto fail; }
        if (!BgDocumentTranslateVertices(&g_CurrentBgDocument,vertices,count,offset,applied,&moved,&why)) { goto rollback; }
    }
    if (moved == 0)
    {
        EditHistoryCancelEdit(&transaction);
        free(vertices);
        return TRUE;
    }
    if (!(object ? GEditorRebuildCurrentViewportWithObjects(&objects,&why)
                 : GEditorRebuildCurrentViewport(&why))
        || !EditHistoryCommitEdit(&g_EditHistory,&g_CurrentBgDocument,
                                  &g_CurrentSetup,&transaction,&why)) { goto rollback; }
    if (object)
    {
        ObjectGeometryFree(&g_CurrentObjects);
        g_CurrentObjects=objects;
    }
    free(vertices);
    GEditorRefreshSelectionDetails();
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction,&g_CurrentBgDocument,&g_CurrentSetup);
    GEditorRebuildCurrentViewport(&restorewhy);
fail:
    EditHistoryCancelEdit(&transaction);
    ObjectGeometryFree(&objects);
    free(vertices);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd,why,GEDITOR_TITLE,MB_ICONERROR);
    return FALSE;
}


static BOOL GEditorPaintBgVertex(HWND hwnd, const ViewportBgVertexHit *request)
{
    EditHistoryTransaction transaction;
    ViewportBgVertexHit hit;
    unsigned char rgba[4];
    BOOL changed;
    const char *why = "";

    if (request == NULL || ViewportGetTool(g_Viewport) != EDITOR_TOOL_VERTEX_PAINT)
    {
        return FALSE;
    }
    hit = *request;
    RightPanelGetPaintColor(g_RightPanel, rgba);
    if (!EditHistoryBeginBgEdit(&g_EditHistory, &g_CurrentBgDocument,
                                "Paint BG Vertex", &transaction, &why))
    {
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return FALSE;
    }
    if (!BgDocumentPaintVertex(&g_CurrentBgDocument, &hit.face, hit.corner,
                               rgba, &changed, &why))
    {
        EditHistoryCancelEdit(&transaction);
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return FALSE;
    }
    if (!changed)
    {
        /* Repainting the same color must preserve redo and the saved state. */
        EditHistoryCancelEdit(&transaction);
        return TRUE;
    }
    if (!EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument,
                               &g_CurrentSetup, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup);
        GEditorRefreshHistoryMenu(hwnd);
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return FALSE;
    }
    ViewportRefreshBgVertexColor(g_Viewport, &g_CurrentBgDocument, &hit);
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
}


static void GEditorDeleteSelectedObject(HWND hwnd, DWORD objectindex)
{
    EditHistoryTransaction transaction;
    const char *why = "";
    const char *restorewhy = "";

    if (objectindex >= g_CurrentSetup.objectcount
        || g_CurrentSetup.objects[objectindex].deleted)
    {
        return;
    }

    if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup,
                                   "Delete Object", &transaction, &why))
    {
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return;
    }

    if (!SetupFileDeleteObject(&g_CurrentSetup, objectindex, &why)
        || !GEditorReloadCurrentObjectsAndViewport(&why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument,
                                  &g_CurrentSetup, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument,
                                &g_CurrentSetup);
        GEditorReloadCurrentObjectsAndViewport(&restorewhy);
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        GEditorRefreshHistoryMenu(hwnd);
        return;
    }

    GEditorRefreshHistoryMenu(hwnd);
}


static LRESULT CALLBACK GEditorWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;

        g_Browser = BrowserCreate(hwnd, cs->hInstance);
        if (g_Browser == NULL)
        {
            return -1;
        }

        g_RightPanel = RightPanelCreate(hwnd, cs->hInstance);
        if (g_RightPanel == NULL)
        {
            return -1;
        }

        g_Viewport = ViewportCreate(hwnd, cs->hInstance);
        if (g_Viewport == NULL)
        {
            return -1;
        }
        g_ToolToolbar = ToolToolbarCreate(hwnd, cs->hInstance);
        if (g_ToolToolbar == NULL)
        {
            return -1;
        }
        ToolToolbarSetTool(g_ToolToolbar, ViewportGetTool(g_Viewport));
        GEditorRefreshHistoryMenu(hwnd);
        return 0;
    }

    case WM_SIZE:
        GEditorLayout(hwnd);
        return 0;

    case RIGHTPANEL_WM_VISIBILITY_CHANGED:
    {
        DWORD visibility = (DWORD)wparam;

        ViewportSetGeometryVisibility(
            g_Viewport,
            (visibility & RIGHTPANEL_SHOW_BG_PRIMARY) != 0,
            (visibility & RIGHTPANEL_SHOW_BG_SECONDARY) != 0,
            (visibility & RIGHTPANEL_SHOW_STAN) != 0,
            (visibility & RIGHTPANEL_SHOW_PORTALS) != 0,
            (visibility & RIGHTPANEL_SHOW_OBJECTS) != 0);
        return 0;
    }

    case VIEWPORT_WM_TRANSFORM_PREVIEW:
        GEditorRefreshTransformFields();
        return 0;

    case VIEWPORT_WM_SELECTION_CHANGED:
        GEditorRefreshSelectionDetails();
        return 0;

    case EDITTOOL_WM_SELECT:
        if (wparam < EDITOR_TOOL_COUNT)
        {
            ViewportSetTool(g_Viewport, (EditorTool)wparam);
            ToolToolbarSetTool(g_ToolToolbar, ViewportGetTool(g_Viewport));
            SetFocus(g_Viewport);
        }
        return 0;

    case RIGHTPANEL_WM_SET_POSITION:
    {
        const RightPanelPosition *request = (const RightPanelPosition *)lparam;
        double position[3], offset[3] = {0, 0, 0};
        DWORD count;
        BOOL result = FALSE;
        int axis;

        if (request != NULL && !(request->axismask & ~7u)
            && ViewportGetSelectionPosition(g_Viewport, position, &count))
        {
            for (axis = 0; axis < 3; axis++)
            {
                if (request->axismask & (1u << axis))
                {
                    offset[axis] = request->position[axis] - position[axis];
                    if (!isfinite(offset[axis])) { break; }
                }
            }
            if (axis == 3) { result = GEditorTranslateSelection(hwnd, offset); }
        }
        /* Refresh even for a no-op or rejected edit: display actual asset coordinates. */
        GEditorRefreshTransformFields();
        return result;
    }

    case VIEWPORT_WM_TRANSLATE_SELECTION:
    {
        const ViewportTranslation *request = (const ViewportTranslation *)lparam;
        return request != NULL && GEditorTranslateSelection(hwnd,request->offset);
    }

    case VIEWPORT_WM_PAINT_VERTEX:
        return GEditorPaintBgVertex(hwnd, (const ViewportBgVertexHit *)lparam);

    case VIEWPORT_WM_DELETE_SELECTION:
    {
        DWORD selectedobject;

        if (ViewportGetTool(g_Viewport) != EDITOR_TOOL_FACE_SELECT) { return 0; }
        if (ViewportGetSelectedObject(g_Viewport, &selectedobject))
        {
            GEditorDeleteSelectedObject(hwnd, selectedobject);
        }
        else
        {
            GEditorDeleteSelectedBgFaces(hwnd);
        }
        return 0;
    }

    case BROWSER_WM_LEVEL_OPEN:
    {
        DWORD index = (DWORD)wparam;
        const RomLevel *level;
        BgFile bg;
        BgDocument document;
        BgDocumentRenderMesh mesh;
        SetupFile setup;
        SetupObjectGeometry objects;
        StanFile stan;
        BgPortalFile portals;
        const char *bgwhy = "";
        const char *setupwhy = "";
        const char *objectwhy = "";
        const char *stanwhy = "";
        const char *portalwhy = "";
        BOOL setupLoaded;
        BOOL objectsLoaded = FALSE;
        BOOL stanLoaded;
        BOOL portalsLoaded;
        DWORD objectfirsttriangle;
        char title[256];

        if (index >= g_Project.levelcount)
        {
            MessageBox(hwnd, "That level is not present in this project.",
                       GEDITOR_TITLE, MB_ICONERROR);
            return 0;
        }

        level = &g_Project.levels[index];

        if (!BgLoadProjectFile(g_Project.dir, level->bgname, &bg, &bgwhy))
        {
            MessageBox(hwnd, bgwhy, GEDITOR_TITLE, MB_ICONERROR);
            return 0;
        }

        if (!BgDocumentLoad(bg.data, bg.size, level->levelscale,
                            &document, &bgwhy))
        {
            BgFileFree(&bg);
            MessageBox(hwnd, bgwhy, GEDITOR_TITLE, MB_ICONERROR);
            return 0;
        }

        if (!BgDocumentBuildRenderMesh(&document, &mesh, &bgwhy))
        {
            BgDocumentFree(&document);
            BgFileFree(&bg);
            MessageBox(hwnd, bgwhy, GEDITOR_TITLE, MB_ICONERROR);
            return 0;
        }
        objectfirsttriangle = mesh.facecount;

        portalsLoaded = BgLoadPortals(bg.data, bg.size, level->levelscale,
                                      &portals, &portalwhy);

        setupLoaded = SetupLoadProjectFile(g_Project.dir, level->setupname,
                                           &setup, &setupwhy);
        /* Placement needs the collision tiles before object geometry is
           built, regardless of whether the stan overlay is visible. */
        stanLoaded = StanLoadProjectFile(g_Project.dir, level->stanname,
                                         level->levelscale, &stan,
                                         &stanwhy);
        ZeroMemory(&objects, sizeof(objects));

        if (setupLoaded)
        {
            objectsLoaded = ObjectLoadSetupGeometry(g_Project.dir, &setup,
                stanLoaded ? &stan : NULL, level->levelscale, &objects, &objectwhy);

            if (objectsLoaded && objects.tricount > 0)
            {
                if (!GEditorAppendObjectGeometry(&mesh, &objects, &objectwhy))
                {
                    objectsLoaded = FALSE;
                }
            }
        }

        if (!ViewportSetScene(g_Viewport, mesh.vertices, mesh.tags,
                              mesh.facerefs, mesh.vertexrefs,
                              objectsLoaded ? objects.objectindices : NULL,
                              (int)objectfirsttriangle,
                              (int)mesh.facecount,
                              g_Project.dir, TRUE))
        {
            BgDocumentRenderMeshFree(&mesh);
            ObjectGeometryFree(&objects);
            SetupFileFree(&setup);
            StanFileFree(&stan);
            BgPortalFileFree(&portals);
            BgDocumentFree(&document);
            BgFileFree(&bg);
            MessageBox(hwnd, "Out of memory loading the level viewport.",
                       GEDITOR_TITLE, MB_ICONERROR);
            return 0;
        }
        BgDocumentRenderMeshFree(&mesh); /* the viewport copied all arrays */

        BgFileFree(&g_CurrentBg);
        g_CurrentBg = bg;
        BgDocumentFree(&g_CurrentBgDocument);
        g_CurrentBgDocument = document;
        GEditorRefreshSelectionDetails();

        BgPortalFileFree(&g_CurrentPortals);
        if (portalsLoaded)
        {
            g_CurrentPortals = portals;
            ViewportSetPortals(g_Viewport, &g_CurrentPortals);
        }
        else
        {
            ViewportSetPortals(g_Viewport, NULL);
            MessageBox(hwnd, portalwhy, GEDITOR_TITLE, MB_ICONWARNING);
        }

        StanFileFree(&g_CurrentStan);
        if (stanLoaded)
        {
            g_CurrentStan = stan;
            ViewportSetStanTiles(g_Viewport, &g_CurrentStan);
        }
        else
        {
            ViewportSetStanTiles(g_Viewport, NULL);
            /* As with unfinished setup entries, keep the useful BG
               open even when this level has no project stan. */
            MessageBox(hwnd, stanwhy, GEDITOR_TITLE, MB_ICONWARNING);
        }

        SetupFileFree(&g_CurrentSetup);
        if (setupLoaded)
        {
            g_CurrentSetup = setup;
            ViewportSetSetupPads(g_Viewport, &g_CurrentSetup,
                                 level->levelscale,
                                 objectsLoaded ? objects.occupiedpads : NULL,
                                 objectsLoaded ? objects.occupiedboundpads : NULL);
        }
        else
        {
            ViewportSetSetupPads(g_Viewport, NULL, level->levelscale,
                                 NULL, NULL);
            /* Some unfinished/test level-table entries name setup
               resources which are not present in obseg. Their BG is
               still useful, so open it and report only the setup gap. */
            MessageBox(hwnd, setupwhy, GEDITOR_TITLE, MB_ICONWARNING);
        }

        if (setupLoaded && !objectsLoaded)
        {
            MessageBox(hwnd, objectwhy, GEDITOR_TITLE, MB_ICONWARNING);
        }

        ObjectGeometryFree(&g_CurrentObjects);
        if (objectsLoaded)
        {
            g_CurrentObjects = objects;
            ZeroMemory(&objects, sizeof(objects));
        }
        ObjectGeometryFree(&objects);

        EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument,
                         &g_CurrentSetup);
        GEditorRefreshHistoryMenu(hwnd);
        g_CurrentLevelIndex = index;

        wsprintf(title, "%s - %s", GEDITOR_TITLE, (const char *)lparam);
        SetWindowText(hwnd, title);
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        /*
         * Win32 delivers the wheel to the keyboard-focus window, which
         * is usually this frame - not the panel under the pointer.
         * Route it by position so the browser scrolls whenever the
         * cursor is over it, matching what hands expect.
         */
        POINT p;
        RECT rc;

        p.x = GET_X_LPARAM(lparam);
        p.y = GET_Y_LPARAM(lparam);
        GetWindowRect(g_Browser, &rc);

        if (PtInRect(&rc, p))
        {
            SendMessage(g_Browser, WM_MOUSEWHEEL, wparam, lparam);
            return 0;
        }
        GetWindowRect(g_RightPanel, &rc);
        if (PtInRect(&rc, p))
        {
            SendMessage(g_RightPanel, WM_MOUSEWHEEL, wparam, lparam);
            return 0;
        }
        break;
    }

    case WM_SETCURSOR:
        /* Mouse messages over the children go to the children, so the
           main window only hears about the cursor when it is over its
           own client area - which is exactly the splitter gutter. */
        if (LOWORD(lparam) == HTCLIENT)
        {
            POINT p;

            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            if (GEditorInBrowserSplitter(p.x)
                || GEditorInRightSplitter(hwnd, p.x))
            {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONDOWN:
        if (GEditorInBrowserSplitter(GET_X_LPARAM(lparam)))
        {
            g_DraggingBrowserSplitter = TRUE;
            SetCapture(hwnd);
        }
        else if (GEditorInRightSplitter(hwnd, GET_X_LPARAM(lparam)))
        {
            g_DraggingRightSplitter = TRUE;
            SetCapture(hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (g_DraggingBrowserSplitter)
        {
            g_BrowserWidth = GET_X_LPARAM(lparam) - GEDITOR_SPLITTER_W / 2;
            GEditorLayout(hwnd);
        }
        else if (g_DraggingRightSplitter)
        {
            RECT client;

            GetClientRect(hwnd, &client);
            g_RightPanelWidth = client.right - GET_X_LPARAM(lparam)
                              - GEDITOR_SPLITTER_W / 2;
            GEditorLayout(hwnd);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_DraggingBrowserSplitter || g_DraggingRightSplitter)
        {
            g_DraggingBrowserSplitter = FALSE;
            g_DraggingRightSplitter = FALSE;
            ReleaseCapture();
        }
        return 0;

    case WM_CAPTURECHANGED:
        /* Capture stolen (Alt+Tab, a dialog): abandon the drag the same
           way the viewport abandons flight. */
        g_DraggingBrowserSplitter = FALSE;
        g_DraggingRightSplitter = FALSE;
        return 0;

    case WM_INITMENUPOPUP:
        /* Sent just before a drop-down opens - the one moment the item
           states matter, so they can never be stale. Save and Close
           are only clickable while a project is open. */
        EnableMenuItem((HMENU)wparam, ID_FILE_SAVE_PROJECT, MF_BYCOMMAND | (g_Project.name[0] != '\0' ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem((HMENU)wparam, ID_FILE_CLOSE_PROJECT, MF_BYCOMMAND | (g_Project.name[0] != '\0' ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem((HMENU)wparam, ID_TOOLS_CREATE_ROM, MF_BYCOMMAND | (g_Project.name[0] != '\0' ? MF_ENABLED : MF_GRAYED));
        GEditorUpdateHistoryMenu((HMENU)wparam);
        CheckMenuItem((HMENU)wparam, ID_VIEW_BACKFACE_CULLING, MF_BYCOMMAND | (ViewportGetBackfaceCulling(g_Viewport) ? MF_CHECKED : MF_UNCHECKED));
        return 0;

    case WM_COMMAND:
        ViewportCancelTransform(g_Viewport);
        switch (LOWORD(wparam))
        {
            /**
             * Create a new project.
             */
            case ID_FILE_NEW_PROJECT:
            {
                NewProjectInfo info;

                if (GEditorPromptForNewProject(hwnd, &info))
                {
                    GEditorCloseProject(hwnd); /* one project at a time */

                    const char *why = "";

                    if (ProjectCreate(info.name, info.location, &info.rominfo,
                                      &g_Project, &why))
                    {
                        RomFile rom;
                        const char *romwhy = "";

                        /* Read the ROM once to extract editable assets
                           and retain a validated base for later exports. */
                        if (RomLoad(info.rompath, &rom, &romwhy))
                        {
                            const char *assetwhy = "";

                            if (!RomExportStoreProjectBase(&g_Project, &rom,
                                                          &assetwhy))
                            {
                                MessageBox(hwnd, assetwhy, GEDITOR_TITLE,
                                           MB_ICONWARNING);
                            }

                            /* One-time extraction into the project:
                               its asset folders become the editable
                               copies used whenever it is opened. */
                            if (TexExtractImages(&rom, g_Project.dir, &assetwhy) == 0)
                            {
                                MessageBox(hwnd, assetwhy, GEDITOR_TITLE, MB_ICONWARNING);
                            }

                            if (ModelExtractAll(&rom, g_Project.dir, &assetwhy) == 0)
                            {
                                MessageBox(hwnd, assetwhy, GEDITOR_TITLE, MB_ICONWARNING);
                            }

                            if (BgExtractAll(&rom, g_Project.dir, &assetwhy) == 0)
                            {
                                MessageBox(hwnd, assetwhy, GEDITOR_TITLE, MB_ICONWARNING);
                            }

                            if (SetupExtractAll(&rom, g_Project.dir, &assetwhy) == 0)
                            {
                                MessageBox(hwnd, assetwhy, GEDITOR_TITLE, MB_ICONWARNING);
                            }

                            if (StanExtractAll(&rom, g_Project.dir, &assetwhy) == 0)
                            {
                                MessageBox(hwnd, assetwhy, GEDITOR_TITLE, MB_ICONWARNING);
                            }

                            RomFree(&rom);
                        }
                        else
                        {
                            MessageBox(hwnd, romwhy, GEDITOR_TITLE, MB_ICONERROR);
                        }

                        GEditorRefreshProjectAssets();
                        GEditorSetTitleForProject(hwnd);
                    }
                    else
                    {
                        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
                    }
                }
                return 0;
            }

            case ID_FILE_OPEN_PROJECT:
            {
                char path[MAX_PATH];
 
                if (GEditorPromptForProject(hwnd, path, sizeof(path)))
                {
                    GEditorCloseProject(hwnd); /* one project at a time */

                    if (ProjectRead(path, &g_Project))
                    {
                        GEditorRefreshProjectAssets();
                        GEditorSetTitleForProject(hwnd);
                    }
                    else
                    {
                        MessageBox(hwnd, "That file is not a readable GEditor project.", GEDITOR_TITLE, MB_ICONERROR);
                    }
                }
                return 0;
            }

            case ID_FILE_CLOSE_PROJECT:
                GEditorCloseProject(hwnd);
                return 0;

            case ID_FILE_SAVE_PROJECT:
                GEditorSaveProject(hwnd);
                return 0;

            case ID_EDIT_UNDO:
                GEditorApplyHistoryStep(hwnd, FALSE);
                return 0;

            case ID_EDIT_REDO:
                GEditorApplyHistoryStep(hwnd, TRUE);
                return 0;

            case ID_VIEW_BACKFACE_CULLING:
                ViewportSetBackfaceCulling(g_Viewport,
                    !ViewportGetBackfaceCulling(g_Viewport));
                return 0;

            case ID_TOOLS_CREATE_ROM:
                if (GEditorEnsureProjectBaseRom(hwnd))
                {
                    GEditorPromptForRomExport(hwnd);
                }
                return 0;

            case ID_FILE_EXIT:
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }

        break; /* anything else falls through to DefWindowProc */

    case WM_DESTROY:
        /* The window is gone; ask the message loop to stop. Without
           this the process keeps running after the window closes. */
        SetupFileFree(&g_CurrentSetup);
        StanFileFree(&g_CurrentStan);
        BgPortalFileFree(&g_CurrentPortals);
        ObjectGeometryFree(&g_CurrentObjects);
        EditHistoryFree(&g_EditHistory);
        BgDocumentFree(&g_CurrentBgDocument);
        BgFileFree(&g_CurrentBg);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR cmdline, int showcmd)
{
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;
    HMENU menubar;
    HACCEL accelerators;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = GEditorWndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_GEDITOR));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); /* Paints the blank client area. */
    wc.lpszClassName = GEDITOR_CLASS;

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, "RegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    if (!BrowserRegisterClass(hinstance))
    {
        MessageBox(NULL, "BrowserRegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    if (!RightPanelRegisterClass(hinstance))
    {
        MessageBox(NULL, "RightPanelRegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    if (!ViewportRegisterClass(hinstance))
    {
        MessageBox(NULL, "ViewportRegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    if (!ToolToolbarRegisterClass(hinstance))
    {
        MessageBox(NULL, "ToolToolbarRegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    menubar = GEditorCreateMenuBar();

    hwnd = CreateWindowEx(0, GEDITOR_CLASS, GEDITOR_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, GEDITOR_WIDTH, GEDITOR_HEIGHT, NULL, menubar, hinstance, NULL);

    if (hwnd == NULL)
    {
        MessageBox(NULL, "CreateWindowEx failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, showcmd);
    UpdateWindow(hwnd);
    accelerators = GEditorCreateAccelerators();

    /**
     * The message loop. GetMessage blocks until something happens, returns 0 when WM_QUIT arrives, and -1 on error. 
     */
    for (;;)
    {
        if (g_Viewport != NULL && ViewportIsFlying(g_Viewport))
        {
            /* Flying: drain the queue, then render one frame ourselves.
               WM_PAINT and WM_TIMER are the lowest-priority messages
               Windows has, and a fast mouse floods the queue faster
               than they surface - the cause of the skipping camera.
               With vsync on, SwapBuffers inside ViewportFlyFrame
               blocks until the monitor is ready, pacing this loop to
               the refresh rate. */
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    if (accelerators != NULL)
                    {
                        DestroyAcceleratorTable(accelerators);
                    }
                    CoUninitialize();
                    return (int)msg.wParam;
                }
                if (!RightPanelHandleMessage(g_RightPanel, &msg)
                    && !ToolToolbarHandleMessage(g_ToolToolbar, &msg)
                    && (accelerators == NULL
                        || !TranslateAccelerator(hwnd, accelerators, &msg)))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            ViewportFlyFrame(g_Viewport);
        }
        else
        {
            /* Idle: block. The editor uses no CPU, exactly as before. */
            if (GetMessage(&msg, NULL, 0, 0) <= 0)
            {
                break;
            }
            if (!RightPanelHandleMessage(g_RightPanel, &msg)
                && !ToolToolbarHandleMessage(g_ToolToolbar, &msg)
                && (accelerators == NULL
                    || !TranslateAccelerator(hwnd, accelerators, &msg)))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    if (accelerators != NULL)
    {
        DestroyAcceleratorTable(accelerators);
    }
    CoUninitialize();
    return (int)msg.wParam;
}
