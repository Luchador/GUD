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
#include "recentprojects.h"
#include "resource.h"
#include "viewport.h"
#include "browser.h"
#include "rightpanel.h"
#include "faceproperties.h"
#include "portalproperties.h"
#include "objectflags.h"
#include "objectproperties.h"
#include "tooltoolbar.h"
#include "uveditor.h"
#include "modeleditor.h"
#include "modeledits.h"
#include "imageedits.h"
#include "imageimport.h"
#include "uvcanvas.h"
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
static RecentProjects g_RecentProjects;
static HMENU g_RecentProjectsMenu;
/* Retain a failed metadata save even if all level assets were written. */
static BOOL g_ProjectMetadataDirty;
static DWORD g_CurrentLevelIndex = GEDITOR_NO_LEVEL;
/* Complete source segment for the selected level. Parsed viewport
   geometry is deliberately separate so selection colors and other
   display-only state can never leak into a saved background. */
static BgFile g_CurrentBg;
/* Room-aware editable geometry. Saving compiles it back into g_CurrentBg;
   the raw segment supplies preserved portal, visibility, and header data. */
static BgDocument g_CurrentBgDocument;
/* One chronological history spans document edits and viewport/UV selections. */
static EditHistory g_EditHistory;
static BOOL g_SelectionHistoryPending, g_SelectionHistoryReset, g_SelectionHistoryNavigation;
/* Setup for the selected level, including host-native parsed views.
   Editor tools can consume it without retaining the source ROM. */
static SetupFile g_CurrentSetup;
/* Editable stan document for the selected level. Kept beside the setup so
   editing tools can inspect tile IDs, rooms, links, and special types. */
static StanFile g_CurrentStan;
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
    BgRenderFlags *combinedflags;
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

    combinedflags = (BgRenderFlags *)malloc((size_t)total * sizeof(*combinedflags));
    combinedtris = (BgVertex *)malloc(
        (size_t)total * 3 * sizeof(*combinedtris));
    combinedtags = (unsigned short *)malloc(
        (size_t)total * sizeof(*combinedtags));
    /* Setup objects do not belong to BgDocument, so their zeroed references
       remain BG_FACE_ID_NONE and cannot be mistaken for editable faces. */
    combinedrefs = (BgFaceRef *)calloc((size_t)total,
                                       sizeof(*combinedrefs));

    combinedvertices = (BgDocumentVertexRef *)calloc((size_t)total * 3, sizeof(*combinedvertices));
    if (combinedtris == NULL || combinedtags == NULL || combinedflags == NULL || combinedrefs == NULL
        || combinedvertices == NULL)
    {
        free(combinedtris);
        free(combinedtags);
        free(combinedflags);
        free(combinedrefs);
        free(combinedvertices);
        *reasonout = "out of memory adding setup objects to the viewport.";
        return FALSE;
    }

    if (mesh->facecount > 0)
    {
        memcpy(combinedflags, mesh->renderflags, mesh->facecount * sizeof(*combinedflags));
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

    memcpy(combinedflags + mesh->facecount, objects->renderflags,
           (size_t)objects->tricount * sizeof(*combinedflags));
    BgDocumentRenderMeshFree(mesh);
    mesh->renderflags = combinedflags;
    mesh->vertices = combinedtris;
    mesh->tags = combinedtags;
    mesh->facerefs = combinedrefs;
    mesh->vertexrefs = combinedvertices;
    mesh->facecount = total;
    return TRUE;
}


static BOOL GEditorCanMoveSetupModel(DWORD selection)
{
    if (selection & SETUP_CHARACTER_SELECTION_BIT)
    {
        DWORD index = selection & ~SETUP_CHARACTER_SELECTION_BIT;
        return index < g_CurrentSetup.charactercount && !g_CurrentSetup.characters[index].deleted;
    }
    return selection < g_CurrentSetup.objectcount && !g_CurrentSetup.objects[selection].deleted;
}

static void GEditorRefreshTransformFields(void)
{
    double position[3];
    SetupPadRef padref;
    DWORD count, objectindex;
    BOOL hasposition = ViewportGetSelectionPosition(g_Viewport, position, &count);
    BOOL object = ViewportGetSelectedObject(g_Viewport, &objectindex);
    BOOL pad = ViewportGetSelectedPad(g_Viewport, &padref);
    SetupMarkerRef markerref;
    BOOL marker = ViewportGetSelectedMarker(g_Viewport, &markerref, NULL);
    BOOL stan = ViewportGetStanSelectionCount(g_Viewport, NULL) > 0;
    double scale = stan ? g_CurrentStan.levelscale : g_CurrentBgDocument.levelscale;
    BOOL editable = hasposition && (object ? GEditorCanMoveSetupModel(objectindex) : scale > 0);
    double precision = editable && !object && !pad && !marker ? 1.0 / scale : 0;

    Rotation frame;
    double degrees[3], pivot[3];
    unsigned int axes = 0;
    if (!ViewportIsTransforming(g_Viewport))
    {
        BOOL valid = FALSE;
        if (editable && ViewportGetTool(g_Viewport) != EDITOR_TOOL_VERTEX_PAINT)
        {
            if (marker)
            {
                valid = ViewportGetMarkerRotation(g_Viewport, &frame);
                axes = markerref.kind == SETUP_MARKER_SPAWN ? 2 : 7;
            }
            else if (object)
            {
                valid = ObjectGetSetupModelRotation(&g_CurrentSetup, objectindex, &frame);
                axes = (objectindex & SETUP_CHARACTER_SELECTION_BIT) ? 2 : 7;
            }
            else if (pad)
            {
                valid = SetupFilePadRotation(&g_CurrentSetup, &padref, &frame);
                axes = 7;
            }
            else
            {
                valid = ViewportGetGeometryRotation(g_Viewport, &frame);
                axes = 7;
            }
        }
        ViewportSetRotationFrame(g_Viewport, valid ? &frame : NULL, valid ? axes : 0);
    }
    if (!ViewportIsTransforming(g_Viewport))
    {
        Rotation scaleaxes;
        BOOL valid = editable && !marker && ViewportGetTool(g_Viewport) != EDITOR_TOOL_VERTEX_PAINT;
        RotationAxis(&scaleaxes, 0, 0);
        if (object)
        {
            valid = valid && !(objectindex & SETUP_CHARACTER_SELECTION_BIT)
                && SetupFileGetModelPad(&g_CurrentSetup, objectindex, &padref)
                && SetupFilePadRotation(&g_CurrentSetup, &padref, &scaleaxes);
        }
        else if (pad) { valid = valid && SetupFilePadRotation(&g_CurrentSetup, &padref, &scaleaxes); }
        ViewportSetScaleAxes(g_Viewport, valid ? &scaleaxes : NULL);
    }
    if (ViewportGetTransformMode(g_Viewport) == TRANSFORM_SCALE)
    {
        Scaling scaling;
        BOOL valid = ViewportGetScaling(g_Viewport, &scaling);
        RightPanelSetScaleLocal(g_RightPanel, object || pad);
        RightPanelSetTransformState(g_RightPanel, valid ? scaling.factor : NULL, count, valid, 0);
        return;
    }
    axes = (marker && markerref.kind == SETUP_MARKER_SPAWN)
        || (object && (objectindex & SETUP_CHARACTER_SELECTION_BIT)) ? 2 : 7;
    if (ViewportIsRotating(g_Viewport))
    {
        BOOL valid = ViewportGetRotation(g_Viewport, &frame, degrees, pivot);
        RightPanelSetRotationAxes(g_RightPanel, valid ? axes : 0);
        RightPanelSetTransformState(g_RightPanel, valid ? degrees : NULL, count, valid, 0);
        RightPanelSetRotationAxes(g_RightPanel, valid ? axes : 0);
    }
    else
    {
        RightPanelSetTransformState(g_RightPanel, hasposition ? position : NULL, count, editable,
                                    precision);
    }
}


static void GEditorRefreshSelectionDetails(void)
{
    SetupPadRef padref;
    SetupMarkerRef markerref;
    DWORD selectedobject, portal;
    int count = ViewportGetSelectedBgFaceCount(g_Viewport);
    BOOL objectselected = ViewportGetSelectedObject(g_Viewport, &selectedobject);
    int components = ViewportGetSelectedComponentCount(g_Viewport);
    DWORD stantile, stancount = ViewportGetStanSelectionCount(g_Viewport, &stantile);
    g_SelectionHistoryPending = TRUE;
    RightPanelSetObjectFlags(g_RightPanel,
        objectselected && selectedobject < g_CurrentSetup.objectcount
            ? &g_CurrentSetup.objects[selectedobject] : NULL,
        objectselected ? selectedobject : 0);
    UVEditorRefreshSelection(g_Viewport, &g_CurrentBgDocument);
    RightPanelSetVertexPaintMode(g_RightPanel,
        ViewportGetTool(g_Viewport) == EDITOR_TOOL_VERTEX_PAINT);
    GEditorRefreshTransformFields();
    if (ViewportGetSelectedPortal(g_Viewport, &portal))
    {
        RightPanelSetPortal(g_RightPanel, &g_CurrentBgDocument, portal);
    }
    else if (ViewportGetSelectedMarker(g_Viewport, &markerref, NULL))
    {
        RightPanelSetSetupMarker(g_RightPanel, &markerref);
    }
    else if (ViewportGetSelectedPad(g_Viewport, &padref))
    {
        RightPanelSetSetupPad(g_RightPanel, &g_CurrentSetup, &padref);
    }
    else if (stancount > 0)
    {
        RightPanelSetStanSelection(g_RightPanel, &g_CurrentStan, ViewportGetTool(g_Viewport), stancount, stantile);
    }
    else if (objectselected && selectedobject < g_CurrentSetup.objectcount)
    {
        RightPanelSetSetupObject(g_RightPanel, &g_CurrentSetup, selectedobject, g_Project.dir);
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
    else if (count > 0 && ViewportGetTool(g_Viewport) == EDITOR_TOOL_FACE_SELECT)
    {
        BgFaceRef *faces = (BgFaceRef *)malloc((size_t)count * sizeof(*faces));
        if (faces != NULL && ViewportGetSelectedBgFaces(g_Viewport, faces, count))
        {
            RightPanelSetBgFaces(g_RightPanel, &g_CurrentBgDocument, faces, (DWORD)count, g_Browser);
        }
        else { RightPanelSetBgSelectionCount(g_RightPanel, count); }
        free(faces);
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
    if (!ViewportSetScene(g_Viewport, mesh.vertices, mesh.tags, mesh.renderflags, mesh.facerefs, mesh.vertexrefs,
                          objects->objectindices,
                          (int)objectfirsttriangle, &objects->monitors,
                          (int)mesh.facecount, g_Project.dir, FALSE))
    {
        BgDocumentRenderMeshFree(&mesh);
        *reasonout = "out of memory rebuilding the viewport.";
        return FALSE;
    }
    BgDocumentRenderMeshFree(&mesh);

    ViewportSetPortals(g_Viewport, &g_CurrentBgDocument.portals);
    if (!ViewportSetStanTiles(g_Viewport,
        g_CurrentStan.data != NULL ? &g_CurrentStan : NULL))
    {
        *reasonout = "out of memory rebuilding the stan viewport.";
        return FALSE;
    }
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


static void GEditorRefreshImageViews(HWND hwnd, DWORD id, BOOL reveal)
{
    TexThumb *items = NULL;
    unsigned char *pixels = NULL;
    const char *why = "";
    DWORD count = TexLoadProjectThumbnails(g_Project.dir, &items, &pixels, &why);
    BrowserSetImages(g_Browser, items, (int)count, pixels);
    if (reveal && !BrowserRevealImage(g_Browser, id))
    {
        MessageBox(hwnd, "The image changed, but its thumbnail could not be loaded. Save Project, then reopen it.",
                   GEDITOR_TITLE, MB_ICONWARNING);
    }
    /* Model UV conversion can depend on image dimensions. Rebuild model
     * geometry as well as the GL texture cache, keeping camera/selection. */
    if (g_CurrentLevelIndex < g_Project.levelcount && !GEditorReloadCurrentObjectsAndViewport(&why))
    { MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR); }
    ModelEditorRefreshImages();
    GEditorRefreshSelectionDetails();
    GEditorRefreshHistoryMenu(hwnd);
}

static void GEditorEditImage(HWND hwnd, DWORD id, BOOL deleting)
{
    const char *why = "";
    if (g_Project.name[0] == '\0') { return; }
    if (deleting)
    {
        char warning[640];
        snprintf(warning, sizeof(warning),
            "Delete image %04lX?\r\n\r\n"
            "Some texture IDs are hard-coded in the game, including light textures. "
            "Deleting them can break lighting or other effects. Faces and models that use this image will lose its texture.\r\n\r\n"
            "The image will be removed from the browser and its BMP removed on Save Project. "
            "Its ID stays reserved as a blank ROM texture; other image IDs will not change.", (unsigned long)id);
        if (MessageBox(hwnd, warning, "Delete Image", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) { return; }
        if (!ImageEditsDelete(g_Project.dir, id, &why))
        { MessageBox(hwnd, why, "Delete Image", MB_ICONERROR); return; }
    }
    else if (!ImageReplaceShow(hwnd, g_Project.dir, id)) { return; }
    GEditorRefreshImageViews(hwnd, id, !deleting);
}

/*
 * (Re)loads the browser from the project file and extracted asset
 * folders. Works for both freshly created and reopened projects: the
 * image files supply the previews; base.z64 supplies their native metadata.
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
        lstrcpyn(levels[i].label, g_Project.levels[i].name, sizeof(levels[i].label));
    }

    BrowserSetLevels(g_Browser, g_Project.levelcount > 0 ? levels : NULL, (int)g_Project.levelcount);

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

            wsprintf(pattern, "%s\\models\\%s\\*.gltf", g_Project.dir, classes[c]);
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
                    || lstrcmpi(extension, ".gltf") != 0)
                {
                    continue;
                }

                lstrcpyn(stem, find.cFileName, sizeof(stem));
                dot = strrchr(stem, '.');
                if (dot != NULL)
                {
                    *dot = '\0';
                }

                lstrcpyn(models[modelcount].label, stem, sizeof(models[modelcount].label));
                modelcount++;
            }
            while (modelcount < 512 && FindNextFile(search, &find));

            FindClose(search);
        }

        BrowserSetModels(g_Browser, modelcount > 0 ? models : NULL, modelcount);
    }
    ModelEditorSetProject(g_Project.dir);
}

/* Creating or opening another project implicitly closes the current one. */
static void GEditorCloseProject(HWND hwnd)
{
    if (g_Project.name[0] == '\0')
    {
        return; /* Nothing open. */
    }

    SetupFileFree(&g_CurrentSetup);
    StanFileFree(&g_CurrentStan);
    ObjectGeometryFree(&g_CurrentObjects);
    EditHistoryFree(&g_EditHistory);
    BgDocumentFree(&g_CurrentBgDocument);
    BgFileFree(&g_CurrentBg);
    g_CurrentLevelIndex = GEDITOR_NO_LEVEL;
    ModelEditsReset();
    ImageEditsReset();
    ProjectClose(&g_Project);
    g_ProjectMetadataDirty = FALSE;

    ModelEditorSetProject(NULL);
    BrowserSetLevels(g_Browser, NULL, 0);
    BrowserSetImages(g_Browser, NULL, 0, NULL);
    BrowserSetModels(g_Browser, NULL, 0);
    ViewportSetScene(g_Viewport, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL, FALSE);
    ViewportSetBackgroundColor(g_Viewport, NULL);
    ViewportSetLevelFog(g_Viewport, NULL, 1.0f);
    GEditorRefreshSelectionDetails();
    GEditorRefreshHistoryMenu(hwnd);
}


/**
  * Menu command IDs. Every clickable has one and it is the number
  * that arrives in WM_COMMAND when the item is chosen.
  */
enum {
    ID_FILE_NEW_PROJECT = 40001,
    ID_FILE_OPEN_PROJECT,
    ID_FILE_SAVE_PROJECT,
    ID_FILE_IMPORT_IMAGE,
    ID_FILE_EXIT,

    ID_EDIT_UNDO,
    ID_EDIT_REDO,
    ID_VIEW_BACKFACE_CULLING,
    ID_VIEW_BG_STATISTICS,
    ID_VIEW_FOG,
    ID_VIEW_HIDE_SELECTED,
    ID_VIEW_UNHIDE_ALL,

    ID_TOOLS_CREATE_ROM,
    ID_TOOLS_UV_EDITOR,
    ID_TOOLS_MODEL_EDITOR,

    ID_FILE_RECENT_PROJECT_FIRST,
    ID_FILE_RECENT_PROJECT_LAST = ID_FILE_RECENT_PROJECT_FIRST + RECENT_PROJECTS_MAX - 1
};


/* What the New Project dialog collects. */
typedef struct NewProjectInfo {
    char name[GEDITOR_NAME_MAX];
    char location[MAX_PATH];
    char rompath[MAX_PATH];
    RomInfo rominfo;   /* filled by the dialog's Create validation */
} NewProjectInfo;


static const char *GEditorRecentProjectFilename(const char *path)
{
    const char *name = strrchr(path, '\\');
    return name != NULL ? name + 1 : path;
}


static void GEditorRefreshRecentProjectsMenu(void)
{
    DWORD index;

    if (g_RecentProjectsMenu == NULL) { return; }
    while (GetMenuItemCount(g_RecentProjectsMenu) > 0)
    {
        DeleteMenu(g_RecentProjectsMenu, 0, MF_BYPOSITION);
    }
    if (g_RecentProjects.count == 0)
    {
        AppendMenu(g_RecentProjectsMenu, MF_STRING | MF_GRAYED, 0,
                   "No recent projects");
    }
    for (index = 0; index < g_RecentProjects.count; index++)
    {
        const char *name = GEditorRecentProjectFilename(g_RecentProjects.paths[index]);
        const char *text = name;
        char label[MAX_PATH * 2];
        DWORD other;
        size_t length = 0;

        /* Identical filenames in different folders need their full paths. */
        for (other = 0; other < g_RecentProjects.count; other++)
        {
            if (other != index && lstrcmpi(name,
                GEditorRecentProjectFilename(g_RecentProjects.paths[other])) == 0)
            {
                text = g_RecentProjects.paths[index];
                break;
            }
        }
        /* Ampersands in filenames are literal, not menu mnemonics. */
        while (*text != '\0')
        {
            if (*text == '&') { label[length++] = '&'; }
            label[length++] = *text++;
        }
        label[length] = '\0';
        AppendMenu(g_RecentProjectsMenu, MF_STRING,
                   ID_FILE_RECENT_PROJECT_FIRST + index, label);
    }
}


static void GEditorRememberProject(void)
{
    RecentProjectsRemember(&g_RecentProjects, g_Project.geppath);
    GEditorRefreshRecentProjectsMenu();
}


/* Both Open Project and Recent Projects replace the current project only
   after the chosen file has been read successfully. */
static void GEditorOpenProject(HWND hwnd, const char *path)
{
    GEditorProject project;
    const char *why = "";

    if (!ProjectRead(path, &project))
    {
        char message[MAX_PATH + 128];
        snprintf(message, sizeof(message),
                 "That file is missing or is not a current GEditor project with a saved level table.\n\n%s", path);
        MessageBox(hwnd, message, GEDITOR_TITLE, MB_ICONERROR);
        return;
    }

    if (!RomExportRefreshProjectLevelMetadata(&project, &why))
    {
        char message[512];
        snprintf(message, sizeof(message), "The project needs its matching current base.z64.\n\n%s", why);
        MessageBox(hwnd, message, GEDITOR_TITLE, MB_ICONERROR);
        return;
    }
    GEditorCloseProject(hwnd);
    g_Project = project;
    GEditorRefreshProjectAssets();
    GEditorSetTitleForProject(hwnd);
    GEditorRememberProject();
}


/* Builds the top menu bar. */
static HMENU GEditorCreateMenuBar(void)
{
    HMENU menubar;
    HMENU filemenu;
    HMENU editmenu;
    HMENU viewmenu;
    HMENU toolsmenu;
    HMENU importmenu;

    menubar = CreateMenu();
    filemenu = CreatePopupMenu();
    editmenu = CreatePopupMenu();
    viewmenu = CreatePopupMenu();
    toolsmenu = CreatePopupMenu();
    importmenu = CreatePopupMenu();
    AppendMenu(importmenu, MF_STRING, ID_FILE_IMPORT_IMAGE, "Import &Image");
    g_RecentProjectsMenu = CreatePopupMenu();
    GEditorRefreshRecentProjectsMenu();

    /* MF_STRING items carry a command ID. '&' marks the Alt mnemonic. */
    AppendMenu(filemenu, MF_STRING, ID_FILE_NEW_PROJECT, "&New Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_OPEN_PROJECT, "&Open Project");
    AppendMenu(filemenu, MF_POPUP, (UINT_PTR)g_RecentProjectsMenu, "&Recent Projects");
    AppendMenu(filemenu, MF_POPUP, (UINT_PTR)importmenu, "&Import...");
    AppendMenu(filemenu, MF_STRING, ID_FILE_SAVE_PROJECT, "&Save Project");
    AppendMenu(filemenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(filemenu, MF_STRING, ID_FILE_EXIT, "E&xit");

    AppendMenu(editmenu, MF_STRING, ID_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenu(editmenu, MF_STRING, ID_EDIT_REDO, "&Redo\tCtrl+Y");

    AppendMenu(viewmenu, MF_STRING, ID_VIEW_BACKFACE_CULLING, "&Backface Culling");
    AppendMenu(viewmenu, MF_STRING | MF_CHECKED, ID_VIEW_BG_STATISTICS, "Background &Statistics");
    AppendMenu(viewmenu, MF_STRING | MF_CHECKED, ID_VIEW_FOG, "&Fog\tF");
    AppendMenu(viewmenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(viewmenu, MF_STRING, ID_VIEW_HIDE_SELECTED, "&Hide Selected\tH");
    AppendMenu(viewmenu, MF_STRING, ID_VIEW_UNHIDE_ALL, "&Unhide All\tAlt+H");

    AppendMenu(toolsmenu, MF_STRING, ID_TOOLS_UV_EDITOR, "&UV Editor");
    AppendMenu(toolsmenu, MF_STRING, ID_TOOLS_MODEL_EDITOR, "&Model Editor");
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
    HMENU menubar;
    HMENU editmenu;

    /* Saving from an owned dialog (Create ROM) still updates the main frame. */
    hwnd = GetAncestor(hwnd, GA_ROOTOWNER);
    GEditorSetTitleForProject(hwnd);
    menubar = GetMenu(hwnd);
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


static BOOL GEditorHasUnsavedChanges(void)
{
    return g_Project.name[0] != '\0'
        && (g_CurrentBgDocument.dirty || g_CurrentSetup.dirty
            || g_CurrentStan.dirty || g_ProjectMetadataDirty || ModelEditsHasUnsaved()
            || ImageEditsHasUnsaved());
}


static void GEditorSetTitleForProject(HWND hwnd)
{
    char title[GEDITOR_NAME_MAX + 32];
    const char *name = g_Project.name;

    if (g_CurrentLevelIndex < g_Project.levelcount
        && g_Project.levels[g_CurrentLevelIndex].name[0] != '\0')
    {
        name = g_Project.levels[g_CurrentLevelIndex].name;
    }

    if (name[0] != '\0')
    {
        snprintf(title, sizeof(title), "%s - %s%s", GEDITOR_TITLE, name,
                 GEditorHasUnsavedChanges() ? " *" : "");
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
 * pending model replacements are saved with the project.
 */
static BOOL GEditorSaveProject(HWND hwnd)
{
    const char *why = "";
    BOOL saved = FALSE;

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
                goto done;
            }
            bgtosave = &compiled;
        }

        /* Portals live inside this complete BG segment, so they are
           preserved by the same write rather than as a sidecar file. */
        if (!BgSaveProjectFile(g_Project.dir, bgtosave, &why))
        {
            BgFileFree(&compiled);
            MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
            goto done;
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
                goto done;
            }
            EditHistoryMarkSetupSaved(&g_EditHistory, &g_CurrentSetup);
        }

        if (g_CurrentStan.data != NULL
            && !StanSaveProjectFile(g_Project.dir, &g_CurrentStan, &why))
        {
            MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
            goto done;
        }

        EditHistoryMarkStanSaved(&g_EditHistory, &g_CurrentStan);
    }

    if (!ModelEditsSave(g_Project.dir, &why))
    {
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        goto done;
    }

    if (!ImageEditsSave(g_Project.dir, &why))
    {
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        goto done;
    }

    if (!ProjectSave(&g_Project, &why))
    {
        g_ProjectMetadataDirty = TRUE;
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        goto done;
    }

    g_ProjectMetadataDirty = FALSE;
    saved = TRUE;

done:
    GEditorRefreshHistoryMenu(hwnd);
    return saved;
}


static INT_PTR CALLBACK GEditorExitDialogProc(HWND hdlg, UINT msg,
                                             WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDYES:
        case IDNO:
        case IDCANCEL:
            EndDialog(hdlg, LOWORD(wparam));
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hdlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}


static BOOL GEditorConfirmExit(HWND hwnd)
{
    INT_PTR choice;

    if (!GEditorHasUnsavedChanges())
    {
        return TRUE;
    }

    choice = DialogBoxParam((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        MAKEINTRESOURCE(IDD_EXIT_UNSAVED), hwnd, GEditorExitDialogProc, 0);
    if (choice == IDYES)
    {
        /* Any failed write keeps the editor open so the user can retry. */
        return GEditorSaveProject(hwnd);
    }
    if (choice == IDNO)
    {
        return TRUE;
    }
    if (choice == -1)
    {
        MessageBox(hwnd, "Could not open the unsaved changes dialog.",
                   GEDITOR_TITLE, MB_ICONERROR);
    }
    return FALSE;
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


/* One snapshot contains both workspaces so a face change restores its UV
 * selection too. Data is session-local; none of it enters project assets. */
typedef struct GEditorSelectionSnapshot {
    size_t viewsize, uvsize;
} GEditorSelectionSnapshot;

static BOOL GEditorCaptureSelection(void **data, size_t *size)
{
    void *view = NULL, *uv = NULL;
    size_t viewsize = 0, uvsize = 0;
    GEditorSelectionSnapshot *snapshot;
    *data = NULL; *size = 0;
    if (!ViewportCaptureSelection(g_Viewport, &view, &viewsize)
        || !UVEditorCaptureSelection(&uv, &uvsize))
    { free(view); free(uv); return FALSE; }
    *size = sizeof(*snapshot) + viewsize + uvsize;
    snapshot = malloc(*size);
    if (!snapshot) { free(view); free(uv); return FALSE; }
    snapshot->viewsize = viewsize;
    snapshot->uvsize = uvsize;
    memcpy(snapshot + 1, view, viewsize);
    if (uvsize) { memcpy((char *)(snapshot + 1) + viewsize, uv, uvsize); }
    free(view); free(uv);
    *data = snapshot;
    return TRUE;
}

static BOOL GEditorRestoreHistorySelection(HWND hwnd)
{
    const GEditorSelectionSnapshot *snapshot = g_EditHistory.selection;
    if (!snapshot || g_EditHistory.selectionsize < sizeof(*snapshot)
        || snapshot->viewsize > g_EditHistory.selectionsize - sizeof(*snapshot)
        || snapshot->uvsize != g_EditHistory.selectionsize - sizeof(*snapshot) - snapshot->viewsize)
    { return FALSE; }
    if (!ViewportRestoreSelection(g_Viewport, snapshot + 1, snapshot->viewsize)) { return FALSE; }
    ToolToolbarSetTool(g_ToolToolbar, ViewportGetTool(g_Viewport));
    ToolToolbarSetVertexSnap(g_ToolToolbar, ViewportGetVertexSnap(g_Viewport));
    if (snapshot->uvsize && !UVEditorIsOpen() && !UVEditorShow(hwnd, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE)))
    { return FALSE; }
    GEditorRefreshSelectionDetails();
    return UVEditorRestoreSelection((const char *)(snapshot + 1) + snapshot->viewsize, snapshot->uvsize);
}

static void GEditorApplyHistoryStep(HWND hwnd, BOOL redo)
{
    const char *why = "";
    const char *restorewhy = "";
    EditHistoryAsset asset = EDIT_HISTORY_ASSET_NONE;
    EditHistoryAsset restoreasset = EDIT_HISTORY_ASSET_NONE;
    BOOL changed;

    g_SelectionHistoryNavigation = TRUE;
    ViewportCancelTransform(g_Viewport);
    UVEditorCancelInteraction();
    changed = redo
        ? EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument,
                          &g_CurrentSetup, &g_CurrentStan, &asset, &why)
        : EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument,
                          &g_CurrentSetup, &g_CurrentStan, &asset, &why);

    if (!changed)
    {
        return;
    }

    if ((asset != EDIT_HISTORY_ASSET_SELECTION
         && !((asset == EDIT_HISTORY_ASSET_SETUP || asset == EDIT_HISTORY_ASSET_STAN)
            ? GEditorReloadCurrentObjectsAndViewport(&why)
            : GEditorRebuildCurrentViewport(&why)))
        || !GEditorRestoreHistorySelection(hwnd))
    {
        /* A history step is not useful if its geometry cannot be presented.
           The inverse transfer cannot allocate here: the destination stack
           just released one entry and already owns sufficient capacity. */
        if (redo)
        {
            EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument,
                            &g_CurrentSetup, &g_CurrentStan, &restoreasset, &restorewhy);
        }
        else
        {
            EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument,
                            &g_CurrentSetup, &g_CurrentStan, &restoreasset, &restorewhy);
        }

        if (restoreasset == EDIT_HISTORY_ASSET_SETUP || restoreasset == EDIT_HISTORY_ASSET_STAN)
        {
            GEditorReloadCurrentObjectsAndViewport(&restorewhy);
        }
        else if (restoreasset == EDIT_HISTORY_ASSET_BG)
        {
            GEditorRebuildCurrentViewport(&restorewhy);
        }

        GEditorRestoreHistorySelection(hwnd);
        MessageBox(hwnd, why[0] ? why : "Could not restore the selection.", GEDITOR_TITLE, MB_ICONERROR);
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
                                  &g_CurrentSetup, &g_CurrentStan, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument,
                                &g_CurrentSetup, &g_CurrentStan);
        GEditorRebuildCurrentViewport(&restorewhy);
        free(selected);
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        GEditorRefreshHistoryMenu(hwnd);
        return;
    }

    free(selected);
    GEditorRefreshHistoryMenu(hwnd);
}


/* Commit one completed UV drag or coordinate entry through BG history.
   The canvas owns its preview; only this path changes the saved document. */
static BOOL GEditorApplyUVEdit(HWND hwnd, const UVCanvasEdit *request)
{
    EditHistoryTransaction transaction;
    const char *why = "", *restorewhy = "";
    DWORD changed;
    ZeroMemory(&transaction, sizeof(transaction));
    if (request == NULL || request->vertices == NULL || request->count == 0
        || ViewportGetTool(g_Viewport) != EDITOR_TOOL_FACE_SELECT) { return FALSE; }
    if (!EditHistoryBeginBgEdit(&g_EditHistory, &g_CurrentBgDocument,
        request->action != NULL ? request->action
            : request->count == 1 ? "Move UV Vertex" : "Move UV Vertices", &transaction, &why)) { goto fail; }
    if (!BgDocumentSetVertexUVs(&g_CurrentBgDocument, request->vertices, request->count,
                               &changed, &why)) { goto rollback; }
    if (changed == 0)
    {
        EditHistoryCancelEdit(&transaction);
        UVEditorRefreshSelection(g_Viewport, &g_CurrentBgDocument);
        return TRUE;
    }
    if (!GEditorRebuildCurrentViewport(&why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
                                  &g_CurrentStan, &transaction, &why)) { goto rollback; }
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    GEditorRebuildCurrentViewport(&restorewhy);
fail:
    EditHistoryCancelEdit(&transaction);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}


/* The panel, gizmo and vertex snaps share the same asset/history path. Drag
 * previews live only in the viewport; there is exactly one edit on release. */
static BOOL GEditorTransformMarker(HWND hwnd, const double offset[3], const Rotation *rotation)
{
    SetupMarkerRef ref;
    SetupMarker spawn;
    EditHistoryTransaction transaction = {0};
    SetupObjectGeometry objects = {0};
    const char *why = "", *restorewhy = "";
    static const char *names[] = {"Start Point", "Intro Camera", "Outro Camera", "Intro Swirl Point"};
    char action[64];
    BOOL changed;
    if (!ViewportGetSelectedMarker(g_Viewport, &ref, &spawn)) { return FALSE; }
    snprintf(action, sizeof(action), "%s %s", rotation ? "Rotate" : "Move", names[ref.kind]);
    if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup, action, &transaction, &why)) { goto fail; }
    if (!SetupFileTransformMarker(&g_CurrentSetup, &ref, &spawn, g_CurrentBgDocument.levelscale,
        offset, rotation, &changed, &why)) { goto rollback; }
    if (!changed) { EditHistoryCancelEdit(&transaction); GEditorRefreshSelectionDetails(); return TRUE; }
    if (!ObjectLoadSetupGeometry(g_Project.dir, &g_CurrentSetup, &g_CurrentStan,
            g_CurrentBgDocument.levelscale, &objects, &why)
        || !GEditorRebuildCurrentViewportWithObjects(&objects, &why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
            &g_CurrentStan, &transaction, &why)) { goto rollback; }
    ObjectGeometryFree(&g_CurrentObjects); g_CurrentObjects = objects;
    GEditorRefreshSelectionDetails(); GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    GEditorRebuildCurrentViewport(&restorewhy);
fail:
    ObjectGeometryFree(&objects); EditHistoryCancelEdit(&transaction);
    GEditorRefreshSelectionDetails(); GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}

static BOOL GEditorTranslateSelection(HWND hwnd, const double offset[3], BOOL snap)
{
    if (ViewportGetSelectedMarker(g_Viewport, NULL, NULL)) { return GEditorTransformMarker(hwnd, offset, NULL); }
    EditHistoryTransaction transaction;
    double applied[3];
    SetupObjectGeometry objects;
    BgDocumentVertexRef *vertices = NULL;
    StanPointRef *stanpoints = NULL;
    DWORD count = 0, moved = 0, objectindex;
    SetupPadRef padref;
    BOOL pad = ViewportGetSelectedPad(g_Viewport, &padref);
    BOOL stan = ViewportGetStanSelectionCount(g_Viewport, NULL) > 0;
    BOOL object = ViewportGetSelectedObject(g_Viewport, &objectindex);
    BOOL character = object && (objectindex & SETUP_CHARACTER_SELECTION_BIT);
    EditorTool tool = ViewportGetTool(g_Viewport);
    const char *why = "", *restorewhy = "";
    const char *action = pad ? (padref.bound ? "Move Bound Pad" : "Move Pad")
        : stan ? (tool == EDITOR_TOOL_VERTEX_SELECT ? "Move Stan Vertices"
        : tool == EDITOR_TOOL_EDGE_SELECT ? "Move Stan Edges" : "Move Stan Tiles")
        : object ? (character ? "Move Character" : "Move Object") : tool == EDITOR_TOOL_VERTEX_SELECT
        ? "Move BG Vertices" : tool == EDITOR_TOOL_EDGE_SELECT ? "Move BG Edges" : "Move BG Faces";
    int axis;
    if (snap) { action = stan ? "Snap Stan Vertex" : "Snap BG Vertex"; }
    ZeroMemory(&transaction, sizeof(transaction));
    ZeroMemory(&objects, sizeof(objects));
    for (axis=0; axis<3; axis++) { applied[axis]=0; }
    if (tool == EDITOR_TOOL_VERTEX_PAINT || (object && !GEditorCanMoveSetupModel(objectindex))) { return FALSE; }
    if (offset[0]==0 && offset[1]==0 && offset[2]==0) { return TRUE; }
    if (pad)
    {
        BOOL changed;
        if (!EditHistoryBeginSetupEdit(&g_EditHistory,&g_CurrentSetup,action,&transaction,&why)) { goto fail; }
        if (!SetupFileTranslatePad(&g_CurrentSetup,&padref,g_CurrentBgDocument.levelscale,
                                   offset,&changed,&why)) { goto rollback; }
        moved = changed ? 1 : 0;
        if (changed && !ObjectLoadSetupGeometry(g_Project.dir,&g_CurrentSetup,&g_CurrentStan,
                    g_CurrentBgDocument.levelscale,&objects,&why)) { goto rollback; }
    }
    else if (stan)
    {
        stanpoints = ViewportGetMoveStanPoints(g_Viewport, &count);
        if (stanpoints == NULL) { why="There are no editable selected stan points."; goto fail; }
        if (!EditHistoryBeginStanEdit(&g_EditHistory,&g_CurrentStan,action,&transaction,&why)) { goto fail; }
        if (!StanTranslatePoints(&g_CurrentStan,stanpoints,count,offset,&moved,&why)) { goto rollback; }
    }
    else if (object)
    {
        if (!EditHistoryBeginSetupEdit(&g_EditHistory,&g_CurrentSetup,action,&transaction,&why)) { goto fail; }
        if (!ObjectTranslateSetupModel(g_Project.dir,&g_CurrentSetup,&g_CurrentStan,
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
        free(vertices); free(stanpoints);
        return TRUE;
    }
    if (snap && transaction.asset == EDIT_HISTORY_ASSET_BG)
    {
        BgFaceRef *collapsed = NULL;
        DWORD collapsedcount, deleted;
        BOOL ok;

        /* Test the actual quantized positions before rebuilding the viewport.
           Deletion belongs to this same transaction, so one Undo restores both. */
        if (!BgDocumentFindCollapsedFaces(&transaction.beforebg, &g_CurrentBgDocument,
                                          &collapsed, &collapsedcount, &why)) { goto rollback; }
        if (collapsedcount > 0)
        {
            char warning[320];
            snprintf(warning, sizeof(warning),
                "This snap would collapse %lu background triangle%s to a line or point.\n\n"
                "Click OK to snap and delete the collapsed triangle%s.\n"
                "Click Cancel to keep the geometry unchanged.",
                (unsigned long)collapsedcount, collapsedcount == 1 ? "" : "s",
                collapsedcount == 1 ? "" : "s");
            if (MessageBox(hwnd, warning, GEDITOR_TITLE, MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            {
                free(collapsed);
                EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument,
                                        &g_CurrentSetup, &g_CurrentStan);
                free(vertices);
                free(stanpoints);
                return FALSE;
            }
            ok = BgDocumentDeleteFaces(&g_CurrentBgDocument, collapsed, collapsedcount,
                                        &deleted, &why);
            free(collapsed);
            if (!ok) { goto rollback; }
            lstrcpyn(transaction.action, "Snap BG Vertex and Delete Faces", sizeof(transaction.action));
        }
    }
    if (!(stan ? GEditorReloadCurrentObjectsAndViewport(&why)
        : (object || pad) ? GEditorRebuildCurrentViewportWithObjects(&objects,&why)
                 : GEditorRebuildCurrentViewport(&why))
        || !EditHistoryCommitEdit(&g_EditHistory,&g_CurrentBgDocument,
                                  &g_CurrentSetup, &g_CurrentStan,&transaction,&why)) { goto rollback; }
    if (object || pad)
    {
        ObjectGeometryFree(&g_CurrentObjects);
        g_CurrentObjects=objects;
    }
    free(vertices); free(stanpoints);
    GEditorRefreshSelectionDetails();
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction,&g_CurrentBgDocument,&g_CurrentSetup, &g_CurrentStan);
    if (stan) { GEditorReloadCurrentObjectsAndViewport(&restorewhy); }
    else { GEditorRebuildCurrentViewport(&restorewhy); }
fail:
    EditHistoryCancelEdit(&transaction);
    ObjectGeometryFree(&objects);
    free(vertices); free(stanpoints);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd,why,GEDITOR_TITLE,MB_ICONERROR);
    return FALSE;
}


static BOOL GEditorTransformSelection(HWND hwnd, const ViewportRotation *request,
                                      const Scaling *scaling)
{
    if (ViewportGetSelectedMarker(g_Viewport, NULL, NULL))
    { return !scaling && request && GEditorTransformMarker(hwnd, NULL, &request->rotation); }
    EditHistoryTransaction transaction;
    const Rotation *rotation = request ? &request->rotation : NULL;
    const double *pivot = request ? request->pivot : scaling->pivot;
    SetupObjectGeometry objects;
    BgDocumentVertexRef *vertices = NULL;
    StanPointRef *stanpoints = NULL;
    DWORD count = 0, moved = 0, objectindex;
    SetupPadRef padref;
    BOOL pad = ViewportGetSelectedPad(g_Viewport, &padref);
    BOOL stan = ViewportGetStanSelectionCount(g_Viewport, NULL) > 0;
    BOOL object = ViewportGetSelectedObject(g_Viewport, &objectindex);
    BOOL character = object && (objectindex & SETUP_CHARACTER_SELECTION_BIT);
    EditorTool tool = ViewportGetTool(g_Viewport);
    const char *why = "", *restorewhy = "";
    const char *target = pad         ? "Pad"
                         : stan      ? (tool == EDITOR_TOOL_VERTEX_SELECT ? "Stan Vertices"
                                        : tool == EDITOR_TOOL_EDGE_SELECT ? "Stan Edges"
                                                                          : "Stan Faces")
                         : character ? "Character"
                         : object    ? "Object"
                         : tool == EDITOR_TOOL_VERTEX_SELECT ? "BG Vertices"
                         : tool == EDITOR_TOOL_EDGE_SELECT   ? "BG Edges"
                                                             : "BG Faces";
    char action[64];
    snprintf(action, sizeof(action), "%s %s", scaling ? "Scale" : "Rotate", target);
    ZeroMemory(&transaction, sizeof(transaction));
    ZeroMemory(&objects, sizeof(objects));
    if (tool == EDITOR_TOOL_VERTEX_PAINT ||
        (scaling ? !ScalingValid(scaling) : !RotationValid(rotation)) || (scaling && character) ||
        (object && !GEditorCanMoveSetupModel(objectindex)))
    {
        return FALSE;
    }
    if (scaling)
    {
        if (scaling->factor[0] == 1 && scaling->factor[1] == 1 && scaling->factor[2] == 1)
        {
            return TRUE;
        }
    }
    else
    {
        int i, j;
        double difference = 0;
        for (i = 0; i < 3; i++)
        {
            for (j = 0; j < 3; j++)
            {
                difference += fabs(rotation->m[i][j] - (i == j));
            }
        }
        if (difference < 1e-8)
        {
            return TRUE;
        }
    }
    if (pad)
    {
        BOOL changed;
        if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup, action, &transaction, &why))
        {
            goto fail;
        }
        changed = scaling != NULL;
        if (!(scaling ? SetupFileScalePad(&g_CurrentSetup, &padref, g_CurrentBgDocument.levelscale, scaling, &why)
                      : SetupFileRotatePad(&g_CurrentSetup, &padref, rotation, &changed, &why)))
        {
            goto rollback;
        }
        moved = changed ? 1 : 0;
        if (changed && !ObjectLoadSetupGeometry(g_Project.dir, &g_CurrentSetup, &g_CurrentStan,
                                                g_CurrentBgDocument.levelscale, &objects, &why))
        {
            goto rollback;
        }
    }
    else if (stan)
    {
        stanpoints = ViewportGetMoveStanPoints(g_Viewport, &count);
        if (stanpoints == NULL)
        {
            why = "There are no editable selected stan points.";
            goto fail;
        }
        if (tool == EDITOR_TOOL_VERTEX_SELECT && count < 2)
        {
            why = "Select at least two vertices to rotate or scale.";
            goto fail;
        }
        if (!EditHistoryBeginStanEdit(&g_EditHistory, &g_CurrentStan, action, &transaction, &why))
        {
            goto fail;
        }
        if (!(scaling ? StanScalePoints(&g_CurrentStan, stanpoints, count, scaling, &moved, &why)
                      : StanRotatePoints(&g_CurrentStan, stanpoints, count, rotation, pivot, &moved,
                                         &why)))
        {
            goto rollback;
        }
    }
    else if (object)
    {
        if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup, action, &transaction, &why))
        {
            goto fail;
        }
        if (!(scaling ? ObjectScaleSetupModel(g_Project.dir, &g_CurrentSetup, &g_CurrentStan,
                                              g_CurrentBgDocument.levelscale, &g_CurrentObjects,
                                              objectindex, scaling, &objects, &why)
                      : ObjectRotateSetupModel(g_Project.dir, &g_CurrentSetup, &g_CurrentStan,
                                               g_CurrentBgDocument.levelscale, &g_CurrentObjects,
                                               objectindex, rotation, pivot, &objects, &why)))
        {
            goto rollback;
        }
        moved = 1;
    }
    else
    {
        vertices = ViewportGetMoveVertices(g_Viewport, &count);
        if (vertices == NULL)
        {
            why = "There are no editable selected vertices.";
            goto fail;
        }
        if (tool == EDITOR_TOOL_VERTEX_SELECT && count < 2)
        {
            why = "Select at least two vertices to rotate or scale.";
            goto fail;
        }
        if (!EditHistoryBeginBgEdit(&g_EditHistory, &g_CurrentBgDocument, action, &transaction,
                                    &why))
        {
            goto fail;
        }
        if (!(scaling ? BgDocumentScaleVertices(&g_CurrentBgDocument, vertices, count, scaling,
                                                &moved, &why)
                      : BgDocumentRotateVertices(&g_CurrentBgDocument, vertices, count, rotation,
                                                 pivot, &moved, &why)))
        {
            goto rollback;
        }
    }
    if (moved == 0)
    {
        EditHistoryCancelEdit(&transaction);
        free(vertices);
        free(stanpoints);
        return TRUE;
    }
    if (scaling && transaction.asset == EDIT_HISTORY_ASSET_BG)
    {
        BgFaceRef *collapsed = NULL;
        DWORD collapsedcount, deleted;
        BOOL ok;

        /* Test the actual quantized positions before rebuilding the viewport.
           Deletion belongs to this same transaction, so one Undo restores both. */
        if (!BgDocumentFindCollapsedFaces(&transaction.beforebg, &g_CurrentBgDocument, &collapsed,
                                          &collapsedcount, &why))
        {
            goto rollback;
        }
        if (collapsedcount > 0)
        {
            char warning[320];
            snprintf(warning, sizeof(warning),
                     "This scale would collapse %lu background triangle%s to a line or point.\n\n"
                     "Click OK to scale and delete the collapsed triangle%s.\n"
                     "Click Cancel to keep the geometry unchanged.",
                     (unsigned long)collapsedcount, collapsedcount == 1 ? "" : "s",
                     collapsedcount == 1 ? "" : "s");
            if (MessageBox(hwnd, warning, GEDITOR_TITLE, MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            {
                free(collapsed);
                EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup,
                                        &g_CurrentStan);
                free(vertices);
                free(stanpoints);
                return FALSE;
            }
            ok = BgDocumentDeleteFaces(&g_CurrentBgDocument, collapsed, collapsedcount, &deleted,
                                       &why);
            free(collapsed);
            if (!ok)
            {
                goto rollback;
            }
            lstrcpyn(transaction.action, "Scale BG and Delete Faces", sizeof(transaction.action));
        }
    }
    if (!(stan              ? GEditorReloadCurrentObjectsAndViewport(&why)
          : (object || pad) ? GEditorRebuildCurrentViewportWithObjects(&objects, &why)
                            : GEditorRebuildCurrentViewport(&why)) ||
        !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
                               &g_CurrentStan, &transaction, &why))
    {
        goto rollback;
    }
    if (object || pad)
    {
        ObjectGeometryFree(&g_CurrentObjects);
        g_CurrentObjects = objects;
    }
    free(vertices);
    free(stanpoints);
    if (pad && scaling)
    {
        ViewportSelectPad(g_Viewport, &padref);
    }
    GEditorRefreshSelectionDetails();
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    if (stan)
    {
        GEditorReloadCurrentObjectsAndViewport(&restorewhy);
    }
    else
    {
        GEditorRebuildCurrentViewport(&restorewhy);
    }
fail:
    EditHistoryCancelEdit(&transaction);
    ObjectGeometryFree(&objects);
    free(vertices);
    free(stanpoints);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}


static BOOL GEditorSetFaceProperties(HWND hwnd, const BgFacePropertiesEdit *edit)
{
    EditHistoryTransaction transaction = {0};
    BgFaceRef *faces = NULL;
    int count = ViewportGetSelectedBgFaceCount(g_Viewport);
    BOOL changed;
    const char *why = "", *restorewhy = "";
    const char *action;
    if (edit == NULL || edit->fields == 0 || count <= 0
        || ViewportGetTool(g_Viewport) != EDITOR_TOOL_FACE_SELECT
        || ViewportIsTransforming(g_Viewport)) { return FALSE; }
    faces = (BgFaceRef *)malloc((size_t)count * sizeof(*faces));
    if (faces == NULL)
    {
        why = "Out of memory reading the BG selection.";
        goto fail;
    }
    if (!ViewportGetSelectedBgFaces(g_Viewport, faces, count))
    {
        why = "The selected BG faces could not be read.";
        goto fail;
    }
    action = edit->fields == BG_FACE_PROPERTY_CULL ? "Change BG Backface Culling"
        : "Change BG Texture Wrapping";
    if (!EditHistoryBeginBgEdit(&g_EditHistory, &g_CurrentBgDocument,
                                action, &transaction, &why)) { goto fail; }
    if (!BgDocumentSetFaceProperties(&g_CurrentBgDocument, faces, (DWORD)count,
                                     edit, &changed, &why)) { goto fail; }
    free(faces);
    faces = NULL;
    if (!changed)
    {
        EditHistoryCancelEdit(&transaction);
        return TRUE;
    }
    /* The existing mesh builder and compiler consume these per-face flags.
       Rebuilding also preserves selection by stable face identity. */
    if (!GEditorRebuildCurrentViewport(&why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument,
                                  &g_CurrentSetup, &g_CurrentStan, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument,
                                &g_CurrentSetup, &g_CurrentStan);
        GEditorRebuildCurrentViewport(&restorewhy);
        goto fail;
    }
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
fail:
    EditHistoryCancelEdit(&transaction);
    free(faces);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}


static BOOL GEditorDropBgTexture(HWND hwnd, const BrowserImageDrop *request)
{
    EditHistoryTransaction transaction;
    BgFaceRef hit;
    BgFaceRef *faces = NULL;
    BOOL selected, changed;
    int count, width, height;
    const char *why = "";
    const char *restorewhy = "";
    const char *action;

    if (request == NULL || request->textureid > BG_TEX_NONE
        || g_CurrentBgDocument.rooms == NULL
        || WindowFromPoint(request->screen) != g_Viewport
        || !ViewportGetTextureDropFace(g_Viewport, request->screen, &hit, &selected))
    {
        return FALSE;
    }
    if (request->textureid != BG_TEX_NONE
        && !TexGetProjectImageSize(g_Project.dir, request->textureid, &width, &height))
    {
        why = "The dragged image is no longer available in this project.";
        goto fail;
    }
    count = selected ? ViewportGetSelectedBgFaceCount(g_Viewport) : 1;
    if (count <= 0) { return FALSE; }
    faces = (BgFaceRef *)malloc((size_t)count * sizeof(*faces));
    if (faces == NULL)
    {
        why = "Out of memory reading the BG selection.";
        goto fail;
    }
    if (selected)
    {
        if (!ViewportGetSelectedBgFaces(g_Viewport, faces, count))
        {
            why = "The selected BG faces could not be read.";
            goto fail;
        }
    }
    else { faces[0] = hit; }

    action = request->textureid == BG_TEX_NONE
        ? (count == 1 ? "Remove BG Texture" : "Remove BG Textures")
        : (count == 1 ? "Apply BG Texture" : "Apply BG Textures");
    if (!EditHistoryBeginBgEdit(&g_EditHistory, &g_CurrentBgDocument,
                                action, &transaction, &why)) { goto fail; }
    if (!BgDocumentSetFaceTexture(&g_CurrentBgDocument, faces, (DWORD)count,
                                  request->textureid, &changed, &why))
    {
        EditHistoryCancelEdit(&transaction); /* validation precedes mutation */
        goto fail;
    }
    free(faces);
    faces = NULL;
    if (!changed)
    {
        EditHistoryCancelEdit(&transaction);
        SetFocus(g_Viewport);
        return TRUE;
    }
    /* Rebatch by the new texture IDs, preserving stable face selection and
       camera position. The existing BG compiler writes the edited markers
       into the saved room streams, also used by Create ROM. */
    if (!GEditorRebuildCurrentViewport(&why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument,
                                  &g_CurrentSetup, &g_CurrentStan, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument,
                                &g_CurrentSetup, &g_CurrentStan);
        GEditorRebuildCurrentViewport(&restorewhy);
        goto fail;
    }
    GEditorRefreshHistoryMenu(hwnd);
    SetFocus(g_Viewport);
    return TRUE;

fail:
    free(faces);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
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
                               &g_CurrentSetup, &g_CurrentStan, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
        GEditorRefreshHistoryMenu(hwnd);
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return FALSE;
    }
    ViewportRefreshBgVertexColor(g_Viewport, &g_CurrentBgDocument, &hit);
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
}


static BOOL GEditorPaintStanTile(HWND hwnd, DWORD tile)
{
    EditHistoryTransaction transaction;
    unsigned char rgba[4];
    BOOL changed;
    const char *why = "";
    if (ViewportGetTool(g_Viewport) != EDITOR_TOOL_VERTEX_PAINT) { return FALSE; }
    RightPanelGetPaintColor(g_RightPanel, rgba);
    if (!EditHistoryBeginStanEdit(&g_EditHistory,&g_CurrentStan,"Paint Stan Tile",&transaction,&why)) { goto fail; }
    if (!StanPaintTile(&g_CurrentStan,tile,rgba,&changed,&why)) { goto rollback; }
    if (!changed) { EditHistoryCancelEdit(&transaction); return TRUE; }
    if (!ViewportSetStanTiles(g_Viewport,&g_CurrentStan))
    {
        why="out of memory updating the stan viewport.";
        goto rollback;
    }
    if (!EditHistoryCommitEdit(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,
                              &g_CurrentStan,&transaction,&why)) { goto rollback; }
    GEditorRefreshSelectionDetails();
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    ViewportSetStanTiles(g_Viewport,&g_CurrentStan);
fail:
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd,why,GEDITOR_TITLE,MB_ICONERROR);
    return FALSE;
}


static void GEditorDeleteSelectedObject(HWND hwnd, DWORD objectindex)
{
    EditHistoryTransaction transaction;
    const char *why = "";
    const char *restorewhy = "";
    BOOL character = (objectindex & SETUP_CHARACTER_SELECTION_BIT) != 0;

    if (!GEditorCanMoveSetupModel(objectindex))
    {
        return;
    }

    if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup,
                                   character ? "Delete Character" : "Delete Object", &transaction, &why))
    {
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        return;
    }

    if (!(character ? SetupFileDeleteCharacter(&g_CurrentSetup, objectindex & ~SETUP_CHARACTER_SELECTION_BIT, &why)
                    : SetupFileDeleteObject(&g_CurrentSetup, objectindex, &why))
        || !GEditorReloadCurrentObjectsAndViewport(&why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument,
                                  &g_CurrentSetup, &g_CurrentStan, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument,
                                &g_CurrentSetup, &g_CurrentStan);
        GEditorReloadCurrentObjectsAndViewport(&restorewhy);
        MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
        GEditorRefreshHistoryMenu(hwnd);
        return;
    }

    GEditorRefreshHistoryMenu(hwnd);
}


static BOOL GEditorSetObjectFlag(HWND hwnd, const ObjectFlagEdit *edit)
{
    EditHistoryTransaction transaction = {0};
    const char *why = "", *restorewhy = "";
    DWORD selected; BOOL changed;
    if (!edit || !ViewportGetSelectedObject(g_Viewport, &selected) || selected != edit->objectindex
        || selected >= g_CurrentSetup.objectcount) { return FALSE; }
    ViewportCancelTransform(g_Viewport);
    if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup,
        "Change Object Flag", &transaction, &why)) { goto fail; }
    if (!SetupFileSetObjectFlag(&g_CurrentSetup, selected, edit->bank, edit->mask, edit->enabled,
        &changed, &why)) { goto fail; }
    if (!changed) { EditHistoryCancelEdit(&transaction); return TRUE; }
    if (!GEditorReloadCurrentObjectsAndViewport(&why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
            &g_CurrentStan, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
        GEditorReloadCurrentObjectsAndViewport(&restorewhy);
        ViewportSelectSetupModel(g_Viewport, selected);
        goto fail;
    }
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
fail:
    EditHistoryCancelEdit(&transaction);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}

static BOOL GEditorSetPortalRooms(HWND hwnd, const PortalPropertiesEdit *edit)
{
    EditHistoryTransaction transaction = {0};
    const char *why = "";
    DWORD index; BOOL changed;
    if (!edit || !ViewportGetSelectedPortal(g_Viewport, &index) || index != edit->portal) { return FALSE; }
    ViewportCancelTransform(g_Viewport);
    if (!EditHistoryBeginBgEdit(&g_EditHistory, &g_CurrentBgDocument, "Change Portal Rooms", &transaction, &why)) { goto fail; }
    if (!BgDocumentSetPortalRooms(&g_CurrentBgDocument, index, edit->room1, edit->room2, &changed, &why)) { goto fail; }
    if (!changed) { EditHistoryCancelEdit(&transaction); return TRUE; }
    if (!EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
        &g_CurrentStan, &transaction, &why))
    {
        EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
        goto fail;
    }
    ViewportSetPortals(g_Viewport, &g_CurrentBgDocument.portals);
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
fail:
    EditHistoryCancelEdit(&transaction);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}

static BOOL GEditorEditSetupMarker(HWND hwnd, SetupMarkerKind kind, const double *position, const double *look)
{
    EditHistoryTransaction transaction = {0};
    SetupObjectGeometry objects = {0};
    SetupMarkerRef selected, previous;
    BOOL hadmarker = ViewportGetSelectedMarker(g_Viewport, &previous, NULL);
    const char *why = "", *restorewhy = "";
    const char *action = kind == SETUP_MARKER_SPAWN
        ? (position ? (strncmp(g_CurrentSetup.name, "Ump_", 4) == 0 ? "Add Spawn Point" : "Place Player Start") : "Delete Spawn Point")
        : kind == SETUP_MARKER_INTRO ? (position ? "Add Intro Camera" : "Delete Intro Camera") : "Replace Outro Camera";
    BOOL edited;
    if (!position && (!hadmarker || previous.kind != kind
        || (kind != SETUP_MARKER_SPAWN && kind != SETUP_MARKER_INTRO))) { return FALSE; }
    if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup, action, &transaction, &why)) { goto fail; }
    if (position)
    {
        edited = kind == SETUP_MARKER_SPAWN
            ? SetupFilePlaceSpawn(&g_CurrentSetup, g_CurrentBgDocument.levelscale, position, &selected, &why)
            : SetupFilePlaceCamera(&g_CurrentSetup, kind, g_CurrentBgDocument.levelscale, position, look, &selected, &why);
    }
    else
    {
        edited = kind == SETUP_MARKER_SPAWN ? SetupFileDeleteSpawn(&g_CurrentSetup, &previous, &why)
            : SetupFileDeleteIntroCamera(&g_CurrentSetup, &previous, &why);
    }
    if (!edited) { goto rollback; }
    if (!ObjectLoadSetupGeometry(g_Project.dir, &g_CurrentSetup, &g_CurrentStan,
            g_CurrentBgDocument.levelscale, &objects, &why)
        || !GEditorRebuildCurrentViewportWithObjects(&objects, &why)
        || !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
            &g_CurrentStan, &transaction, &why)) { goto rollback; }
    ObjectGeometryFree(&g_CurrentObjects); g_CurrentObjects = objects;
    /* Removing an intro record changes later command indices. Explicitly
       select the placed marker, or clear the deleted marker's old identity. */
    if (position)
    {
        RightPanelShowObjects(g_RightPanel);
        ViewportSetTool(g_Viewport, EDITOR_TOOL_FACE_SELECT);
        ToolToolbarSetTool(g_ToolToolbar, ViewportGetTool(g_Viewport));
        ViewportSetTransformMode(g_Viewport, TRANSFORM_MOVE);
        RightPanelSetTransformMode(g_RightPanel, TRANSFORM_MOVE);
    }
    ViewportSelectSetupMarker(g_Viewport, position ? &selected : NULL);
    GEditorRefreshHistoryMenu(hwnd);
    SetFocus(g_Viewport);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    GEditorRebuildCurrentViewport(&restorewhy);
    if (hadmarker) { ViewportSelectSetupMarker(g_Viewport, &previous); }
fail:
    ObjectGeometryFree(&objects); EditHistoryCancelEdit(&transaction);
    GEditorRefreshSelectionDetails(); GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}

static BOOL GEditorDropSetupMarker(HWND hwnd, const BrowserObjectDrop *request)
{
    double position[3], look[3];
    float point[3], height;
    DWORD tile;
    SetupMarkerKind kind;
    if (!request || g_CurrentLevelIndex == GEDITOR_NO_LEVEL || !g_CurrentSetup.data
        || WindowFromPoint(request->screen) != g_Viewport) { return FALSE; }
    switch (request->type)
    {
    case BROWSER_OBJECT_SPAWN: kind = SETUP_MARKER_SPAWN; break;
    case BROWSER_OBJECT_INTRO_CAMERA: kind = SETUP_MARKER_INTRO; break;
    case BROWSER_OBJECT_OUTRO_CAMERA: kind = SETUP_MARKER_OUTRO; break;
    default: return FALSE;
    }
    if (kind != SETUP_MARKER_SPAWN && strncmp(g_CurrentSetup.name, "Ump_", 4) == 0) { return FALSE; }
    if (!ViewportGetModelDropPosition(g_Viewport, request->screen, position)
        || !ViewportGetCameraDirection(g_Viewport, look)) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { point[axis] = (float)position[axis]; }
    tile = StanResolvePadTile(&g_CurrentStan, "", point);
    if (tile == STAN_TILE_NONE || !StanGetTileHeight(&g_CurrentStan, tile, point[0], point[2], &height))
    {
        MessageBox(hwnd, kind == SETUP_MARKER_SPAWN ? "Place the spawn point over a walkable floor."
            : "Place the camera over a walkable area so the game can identify its room.",
            GEDITOR_TITLE, MB_ICONINFORMATION);
        return FALSE;
    }
    if (kind == SETUP_MARKER_SPAWN) { position[1] = height; }
    return GEditorEditSetupMarker(hwnd, kind, position, look);
}


typedef enum
{
    GEDITOR_PLACE_MODEL, GEDITOR_PLACE_DOOR, GEDITOR_PLACE_GLASS,
    GEDITOR_PLACE_CCTV, GEDITOR_PLACE_ALARM
} GEditorPlacementKind;

static BOOL GEditorDropModel(HWND hwnd, const BrowserModelDrop *request, GEditorPlacementKind kind)
{
    EditHistoryTransaction transaction;
    SetupObjectGeometry objects = {0};
    const char *why = "", *restorewhy = "";
    double position[3], look[3];
    DWORD selection, triangle;
    BOOL character, found = FALSE, added;
    BOOL door = kind == GEDITOR_PLACE_DOOR, glass = kind == GEDITOR_PLACE_GLASS;
    BOOL cctv = kind == GEDITOR_PLACE_CCTV, alarm = kind == GEDITOR_PLACE_ALARM;
    int modelid;

    if (request == NULL || g_CurrentLevelIndex == GEDITOR_NO_LEVEL || g_CurrentSetup.data == NULL ||
        WindowFromPoint(request->screen) != g_Viewport ||
        !ObjectResolvePlaceableModel(request->name, &character, &modelid) ||
        !ViewportGetModelDropPosition(g_Viewport, request->screen, position))
    {
        return FALSE;
    }
    if ((door || glass || cctv || alarm) && !ViewportGetCameraDirection(g_Viewport, look)) { return FALSE; }
    if (cctv || alarm)
    {
        /* Pull the mount a little towards the viewer so an exact wall hit
         * resolves on the room side of a stan boundary. Keep the drop height. */
        double length = hypot(look[0], look[2]);
        if (length > 1e-8)
        { position[0] -= look[0] / length; position[2] -= look[2] / length; }
    }
    if (door || glass)
    {
        float point[3], height;
        DWORD tile;
        for (int axis = 0; axis < 3; axis++) { point[axis] = (float)position[axis]; }
        tile = StanResolvePadTile(&g_CurrentStan, "", point);
        if (tile == STAN_TILE_NONE || !StanGetTileHeight(&g_CurrentStan, tile, point[0], point[2], &height))
        {
            MessageBox(hwnd, glass ? "Place the glass over a walkable floor." : "Place the door over a walkable floor.",
                GEDITOR_TITLE, MB_ICONINFORMATION);
            return FALSE;
        }
        position[1] = height;
    }
    if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup,
                                   door ? "Add Door" : glass ? "Add Glass" : cctv ? "Add CCTV Camera"
                                       : alarm ? "Add Alarm" : character ? "Add Character" : "Add Prop",
                                   &transaction, &why))
    {
        goto fail;
    }
    if (door)
    {
        added = SetupFileAddDoor(&g_CurrentSetup, modelid, g_CurrentBgDocument.levelscale,
            position, look, &selection, &why);
    }
    else if (glass)
    {
        added = SetupFileAddGlass(&g_CurrentSetup, modelid, g_CurrentBgDocument.levelscale,
            position, look, &selection, &why);
    }
    else if (cctv)
    {
        added = SetupFileAddCctv(&g_CurrentSetup, modelid, g_CurrentBgDocument.levelscale,
            position, look, &selection, &why);
    }
    else if (alarm)
    {
        added = SetupFileAddAlarm(&g_CurrentSetup, modelid, g_CurrentBgDocument.levelscale,
            position, look, &selection, &why);
    }
    else
    {
        added = SetupFileAddModel(&g_CurrentSetup, character, modelid, g_CurrentBgDocument.levelscale,
            position, &selection, &why);
    }
    if (!added ||
        !ObjectLoadSetupGeometry(g_Project.dir, &g_CurrentSetup, &g_CurrentStan,
                                 g_CurrentBgDocument.levelscale, &objects, &why))
    {
        goto rollback;
    }
    /* The loader can skip unavailable models or invalid stan placements.
       Never commit an invisible instance that the user cannot manipulate. */
    for (triangle = 0; triangle < objects.tricount; triangle++)
    {
        if (objects.objectindices[triangle] == selection)
        {
            found = TRUE;
            break;
        }
    }
    if (!found)
    {
        why = "The model could not be placed here. Choose a surface over walkable stan "
              "and ensure its project model files are available.";
        goto rollback;
    }
    if (!GEditorRebuildCurrentViewportWithObjects(&objects, &why) ||
        !EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
                               &g_CurrentStan, &transaction, &why))
    {
        goto rollback;
    }
    ObjectGeometryFree(&g_CurrentObjects);
    g_CurrentObjects = objects;
    RightPanelShowObjects(g_RightPanel);
    ViewportSetTool(g_Viewport, EDITOR_TOOL_FACE_SELECT);
    ToolToolbarSetTool(g_ToolToolbar, ViewportGetTool(g_Viewport));
    if (door || glass)
    {
        ViewportSetTransformMode(g_Viewport, TRANSFORM_MOVE);
        RightPanelSetTransformMode(g_RightPanel, TRANSFORM_MOVE);
    }
    ViewportSelectSetupModel(g_Viewport, selection);
    GEditorRefreshHistoryMenu(hwnd);
    SetFocus(g_Viewport);
    return TRUE;

rollback:
    EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    ObjectGeometryFree(&objects);
    GEditorReloadCurrentObjectsAndViewport(&restorewhy);
fail:
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    GEditorRefreshHistoryMenu(hwnd);
    return FALSE;
}
static BOOL GEditorSetObjectProperty(HWND hwnd, const SetupObjectPropertyEdit *edit)
{
    EditHistoryTransaction transaction = {0};
    SetupObjectGeometry objects = {0};
    const char *why = "", *restorewhy = "";
    DWORD selected;
    BOOL changed = FALSE, model;
    const char *action;
    if (!edit || !ViewportGetSelectedObject(g_Viewport, &selected)
        || selected != edit->objectindex || selected >= g_CurrentSetup.objectcount) { return FALSE; }
    model = edit->property == SETUP_OBJECT_MODEL;
    switch (edit->property)
    {
    case SETUP_OBJECT_MODEL: action = "Change Object Model"; break;
    case SETUP_OBJECT_HEALTH: action = "Change Object Health"; break;
    case SETUP_OBJECT_KEY_FLAGS: action = "Change Key Unlock Flags"; break;
    case SETUP_OBJECT_AMMO_TYPE: action = "Change Ammo Type"; break;
    case SETUP_OBJECT_AMMO_QUANTITY: action = "Change Ammo Quantity"; break;
    case SETUP_OBJECT_DOOR_TRAVEL: action = "Change Door Travel"; break;
    case SETUP_OBJECT_DOOR_CLEARANCE: action = "Change Door Collision Clearance"; break;
    case SETUP_OBJECT_DOOR_ACCEL: action = "Change Door Acceleration"; break;
    case SETUP_OBJECT_DOOR_DECEL: action = "Change Door Deceleration"; break;
    case SETUP_OBJECT_DOOR_SPEED: action = "Change Door Speed"; break;
    case SETUP_OBJECT_DOOR_CLOSE_DELAY: action = "Change Door Close Delay"; break;
    case SETUP_OBJECT_DOOR_TYPE: action = "Change Door Type"; break;
    case SETUP_OBJECT_DOOR_SOUND: action = "Change Door Sounds"; break;
    case SETUP_OBJECT_DOOR_FLAGS: action = "Change Door Flags"; break;
    case SETUP_OBJECT_DOOR_KEY_FLAGS: action = "Change Door Key Requirements"; break;
    case SETUP_OBJECT_CCTV_LOOK_PAD: action = "Change CCTV Look-at Pad"; break;
    case SETUP_OBJECT_CCTV_SWEEP_MIN: action = "Change CCTV Sweep Minimum"; break;
    case SETUP_OBJECT_CCTV_SWEEP_MAX: action = "Change CCTV Sweep Maximum"; break;
    case SETUP_OBJECT_CCTV_SPEED: action = "Change CCTV Turn Speed"; break;
    case SETUP_OBJECT_CCTV_RANGE: action = "Change CCTV Detection Range"; break;
    case SETUP_OBJECT_DRONE_AIM_PAD: action = "Change Drone Gun Aim Pad"; break;
    case SETUP_OBJECT_DRONE_YAW_MIN: action = "Change Drone Gun Aim Minimum"; break;
    case SETUP_OBJECT_DRONE_YAW_MAX: action = "Change Drone Gun Aim Maximum"; break;
    case SETUP_OBJECT_DRONE_SPEED: action = "Change Drone Gun Tracking Speed"; break;
    case SETUP_OBJECT_DRONE_RANGE: action = "Change Drone Gun Detection Range"; break;
    default: return FALSE;
    }
    ViewportCancelTransform(g_Viewport);
    if (!EditHistoryBeginSetupEdit(&g_EditHistory, &g_CurrentSetup,
        action, &transaction, &why)) { goto fail; }
    if (!SetupFileSetObjectProperty(&g_CurrentSetup, edit, &changed, &why)) { goto rollback; }
    if (!changed) { EditHistoryCancelEdit(&transaction); return TRUE; }
    if (model)
    {
        DWORD count = 0;
        unsigned short *tags = NULL;
        BgRenderFlags *flags = NULL;
        float scale;
        /* The full scene loader skips missing models to keep damaged projects
         * inspectable. An explicit model edit must instead fail and roll back. */
        BgVertex *mesh = ModelLoadProjectGeometry(g_Project.dir, (int)edit->value,
            &count, &tags, &flags, &scale, &why);
        BOOL loaded = mesh != NULL;
        free(mesh); free(tags); free(flags);
        if (!loaded)
        {
            if (!why[0]) { why = "The selected model could not be loaded."; }
            goto rollback;
        }
    }
    if (model)
    {
        if (!ObjectLoadSetupGeometry(g_Project.dir, &g_CurrentSetup, &g_CurrentStan,
                g_CurrentBgDocument.levelscale, &objects, &why)
            || !GEditorRebuildCurrentViewportWithObjects(&objects, &why)) { goto rollback; }
    }
    if (!EditHistoryCommitEdit(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup,
                               &g_CurrentStan, &transaction, &why)) { goto rollback; }
    if (model)
    {
        ObjectGeometryFree(&g_CurrentObjects);
        g_CurrentObjects = objects;
    }
    GEditorRefreshHistoryMenu(hwnd);
    return TRUE;
rollback:
    EditHistoryRollbackEdit(&transaction, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    if (model)
    {
        GEditorRebuildCurrentViewport(&restorewhy);
        ViewportSelectSetupModel(g_Viewport, selected);
    }
fail:
    ObjectGeometryFree(&objects);
    EditHistoryCancelEdit(&transaction);
    GEditorRefreshHistoryMenu(hwnd);
    MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
    return FALSE;
}

static LRESULT GEditorDispatchMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
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

    case RIGHTPANEL_WM_STAN_OPACITY:
        ViewportSetStanOpacity(g_Viewport, (int)wparam);
        return 0;

    case VIEWPORT_WM_TRANSFORM_PREVIEW:
        GEditorRefreshTransformFields();
        return 0;

    case VIEWPORT_WM_SNAP_PICK:
        ViewportSnapVertexAt(g_Viewport, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;

    case UVEDITOR_WM_SELECTION_CHANGED:
        g_SelectionHistoryPending = TRUE;
        return 0;

    case VIEWPORT_WM_SELECTION_CHANGED:
        GEditorRefreshSelectionDetails();
        return 0;

    case OBJECTPROPERTIES_WM_CHANGED:
    {
        BOOL ok = GEditorSetObjectProperty(hwnd, (const SetupObjectPropertyEdit *)lparam);
        GEditorRefreshSelectionDetails();
        /* Aim guides read the target pad from the live setup on each draw. */
        if (ok) { ViewportRedraw(g_Viewport); }
        return ok;
    }

    case OBJECTFLAGS_WM_CHANGED:
    {
        BOOL ok = GEditorSetObjectFlag(hwnd, (const ObjectFlagEdit *)lparam);
        GEditorRefreshSelectionDetails();
        return ok;
    }

    case PORTALPROPERTIES_WM_CHANGED:
    {
        BOOL ok = GEditorSetPortalRooms(hwnd, (const PortalPropertiesEdit *)lparam);
        GEditorRefreshSelectionDetails();
        return ok;
    }

    case FACEPROPERTIES_WM_CHANGED:
    {
        BOOL ok = GEditorSetFaceProperties(hwnd, (const BgFacePropertiesEdit *)lparam);
        GEditorRefreshSelectionDetails();
        return ok;
    }

    case BROWSER_WM_IMAGE_DELETE:
    case BROWSER_WM_IMAGE_REPLACE:
        GEditorEditImage(hwnd, (DWORD)wparam, msg == BROWSER_WM_IMAGE_DELETE);
        return 0;

    case FACEPROPERTIES_WM_REVEAL_IMAGE:
        return BrowserRevealImage(g_Browser, (DWORD)wparam);

    case UVEDITOR_WM_APPLY:
        return GEditorApplyUVEdit(hwnd, (const UVCanvasEdit *)lparam);

    case UVEDITOR_WM_HISTORY:
        GEditorApplyHistoryStep(hwnd, (BOOL)wparam);
        return 0;

    case EDITTOOL_WM_SELECT:
        if (wparam < EDITOR_TOOL_COUNT)
        {
            ViewportSetTool(g_Viewport, (EditorTool)wparam);
            ToolToolbarSetTool(g_ToolToolbar, ViewportGetTool(g_Viewport));
            SetFocus(g_Viewport);
        }
        return 0;

    case EDITTOOL_WM_TOGGLE_VERTEX_SNAP:
        if (ViewportGetTool(g_Viewport) == EDITOR_TOOL_VERTEX_SELECT)
        {
            ViewportSetVertexSnap(g_Viewport, !ViewportGetVertexSnap(g_Viewport));
            ToolToolbarSetVertexSnap(g_ToolToolbar, ViewportGetVertexSnap(g_Viewport));
            SetFocus(g_Viewport);
        }
        return 0;

    case RIGHTPANEL_WM_TRANSFORM_MODE:
        if (wparam > TRANSFORM_SCALE) { return 0; }
        ViewportSetTransformMode(g_Viewport, (TransformMode)wparam);
        RightPanelSetTransformMode(g_RightPanel, (TransformMode)wparam);
        GEditorRefreshTransformFields();
        SetFocus(g_Viewport);
        return 0;

    case RIGHTPANEL_WM_SET_SCALE:
    {
        const RightPanelPosition *input = (const RightPanelPosition *)lparam;
        Scaling scaling;
        BOOL ok = FALSE;
        int axis;
        if (input && input->axismask && !(input->axismask & ~7u)
            && ViewportGetScaling(g_Viewport, &scaling))
        {
            for (axis = 0; axis < 3; axis++)
            {
                scaling.factor[axis] = input->axismask & (1u << axis) ? input->position[axis] : 1;
            }
            ok = GEditorTransformSelection(hwnd, NULL, &scaling);
        }
        GEditorRefreshTransformFields();
        return ok;
    }
    case VIEWPORT_WM_SCALE_SELECTION:
    {
        BOOL ok = lparam && GEditorTransformSelection(hwnd, NULL, (const Scaling *)lparam);
        GEditorRefreshTransformFields();
        return ok;
    }

    case RIGHTPANEL_WM_SET_ROTATION:
    {
        const RightPanelPosition *input=(const RightPanelPosition *)lparam;
        Rotation old,target;ViewportRotation request;double degrees[3];BOOL ok=FALSE;int axis;
        if(input && input->axismask && !(input->axismask&~7u)
            && ViewportGetRotation(g_Viewport,&old,degrees,request.pivot)){
            for(axis=0;axis<3;axis++)if(input->axismask&(1u<<axis))degrees[axis]=input->position[axis];
            if(isfinite(degrees[0])&&isfinite(degrees[1])&&isfinite(degrees[2])){
                RotationEuler(&target,degrees);RotationDifference(&request.rotation,&target,&old);
                ok=GEditorTransformSelection(hwnd,&request,NULL);
            }
        }
        GEditorRefreshTransformFields();return ok;
    }
    case VIEWPORT_WM_ROTATE_SELECTION:
    {
        BOOL ok=lparam && GEditorTransformSelection(hwnd,(const ViewportRotation *)lparam,NULL);
        GEditorRefreshTransformFields();return ok;
    }

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
            if (axis == 3) { result = GEditorTranslateSelection(hwnd, offset, FALSE); }
        }
        /* Refresh even for a no-op or rejected edit: display actual asset coordinates. */
        GEditorRefreshTransformFields();
        return result;
    }

    case VIEWPORT_WM_TRANSLATE_SELECTION:
    case VIEWPORT_WM_SNAP_VERTEX:
    {
        const ViewportTranslation *request = (const ViewportTranslation *)lparam;
        return request != NULL && GEditorTranslateSelection(hwnd, request->offset,
                                                            msg == VIEWPORT_WM_SNAP_VERTEX);
    }

    case VIEWPORT_WM_PAINT_STAN:
        return GEditorPaintStanTile(hwnd, (DWORD)wparam);

    case VIEWPORT_WM_PAINT_VERTEX:
        return GEditorPaintBgVertex(hwnd, (const ViewportBgVertexHit *)lparam);

    case VIEWPORT_WM_DELETE_SELECTION:
    {
        DWORD selectedobject;
        SetupMarkerRef marker;

        if (ViewportGetTool(g_Viewport) == EDITOR_TOOL_VERTEX_PAINT) { return 0; }
        if (ViewportGetSelectedMarker(g_Viewport, &marker, NULL))
        {
            if (marker.kind == SETUP_MARKER_SPAWN || marker.kind == SETUP_MARKER_INTRO)
            { GEditorEditSetupMarker(hwnd, marker.kind, NULL, NULL); }
        }
        else if (ViewportGetSelectedObject(g_Viewport, &selectedobject))
        {
            GEditorDeleteSelectedObject(hwnd, selectedobject);
        }
        else if (ViewportGetTool(g_Viewport) == EDITOR_TOOL_FACE_SELECT)
        {
            GEditorDeleteSelectedBgFaces(hwnd);
        }
        return 0;
    }

    case BROWSER_WM_OBJECT_DRAG_BEGIN:
        if ((wparam != BROWSER_OBJECT_SPAWN && wparam != BROWSER_OBJECT_INTRO_CAMERA && wparam != BROWSER_OBJECT_OUTRO_CAMERA
                && wparam != BROWSER_OBJECT_DOOR && wparam != BROWSER_OBJECT_GLASS
                && wparam != BROWSER_OBJECT_CCTV && wparam != BROWSER_OBJECT_ALARM)
            || g_CurrentLevelIndex == GEDITOR_NO_LEVEL || !g_CurrentSetup.data) { return FALSE; }
        if ((wparam == BROWSER_OBJECT_INTRO_CAMERA || wparam == BROWSER_OBJECT_OUTRO_CAMERA)
            && strncmp(g_CurrentSetup.name, "Ump_", 4) == 0)
        {
            MessageBox(hwnd, "Intro and outro cameras can only be placed in single-player levels.",
                GEDITOR_TITLE, MB_ICONINFORMATION);
            return FALSE;
        }
        ViewportCancelTransform(g_Viewport);
        return TRUE;

    case BROWSER_WM_OBJECT_DROP:
    {
        const BrowserObjectDrop *drop = (const BrowserObjectDrop *)lparam;
        if (drop && (drop->type == BROWSER_OBJECT_DOOR || drop->type == BROWSER_OBJECT_GLASS
            || drop->type == BROWSER_OBJECT_CCTV || drop->type == BROWSER_OBJECT_ALARM))
        {
            BrowserModelDrop model = {"", drop->screen};
            GEditorPlacementKind kind;
            const char *name;
            switch (drop->type)
            {
            case BROWSER_OBJECT_GLASS: kind = GEDITOR_PLACE_GLASS; name = SETUP_DEFAULT_GLASS_MODEL; break;
            case BROWSER_OBJECT_CCTV: kind = GEDITOR_PLACE_CCTV; name = SETUP_DEFAULT_CCTV_MODEL; break;
            case BROWSER_OBJECT_ALARM: kind = GEDITOR_PLACE_ALARM; name = SETUP_DEFAULT_ALARM_MODEL; break;
            default: kind = GEDITOR_PLACE_DOOR; name = SETUP_DEFAULT_DOOR_MODEL; break;
            }
            lstrcpyn(model.name, name, sizeof(model.name));
            return GEditorDropModel(hwnd, &model, kind);
        }
        return GEditorDropSetupMarker(hwnd, drop);
    }

    case BROWSER_WM_MODEL_DRAG_BEGIN:
    {
        BOOL character;
        int modelid;
        if (g_CurrentLevelIndex == GEDITOR_NO_LEVEL || g_CurrentSetup.data == NULL
            || !ObjectResolvePlaceableModel((const char *)lparam, &character, &modelid))
        { return FALSE; }
        ViewportCancelTransform(g_Viewport);
        return TRUE;
    }

    case BROWSER_WM_MODEL_DROP:
        return GEditorDropModel(hwnd, (const BrowserModelDrop *)lparam, GEDITOR_PLACE_MODEL);

    case BROWSER_WM_IMAGE_DRAG_BEGIN:
        if (g_CurrentBgDocument.rooms == NULL
            || ViewportGetTool(g_Viewport) != EDITOR_TOOL_FACE_SELECT)
        {
            return FALSE;
        }
        ViewportCancelTransform(g_Viewport);
        return TRUE;

    case BROWSER_WM_IMAGE_DROP:
        return GEditorDropBgTexture(hwnd, (const BrowserImageDrop *)lparam);

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
        const char *bgwhy = "";
        const char *setupwhy = "";
        const char *objectwhy = "";
        const char *stanwhy = "";
        BOOL setupLoaded;
        BOOL objectsLoaded = FALSE;
        BOOL stanLoaded;
        DWORD objectfirsttriangle;

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

        if (!ViewportSetScene(g_Viewport, mesh.vertices, mesh.tags, mesh.renderflags,
                              mesh.facerefs, mesh.vertexrefs,
                              objectsLoaded ? objects.objectindices : NULL,
                              (int)objectfirsttriangle, objectsLoaded ? &objects.monitors : NULL,
                              (int)mesh.facecount,
                              g_Project.dir, TRUE))
        {
            BgDocumentRenderMeshFree(&mesh);
            ObjectGeometryFree(&objects);
            SetupFileFree(&setup);
            StanFileFree(&stan);
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

        ViewportSetPortals(g_Viewport, &g_CurrentBgDocument.portals);
        if (g_CurrentBgDocument.portalwarning)
        { MessageBox(hwnd, g_CurrentBgDocument.portalwarning, GEDITOR_TITLE, MB_ICONWARNING); }

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
            ViewportMoveCameraToSpawn(g_Viewport);
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

        EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
        g_SelectionHistoryReset = TRUE;
        g_CurrentLevelIndex = index;

        ViewportSetBackgroundColor(g_Viewport, level->hasbackgroundcolor ? level->backgroundcolor : NULL);
        ViewportSetLevelFog(g_Viewport, level->hasbackgroundcolor ? &level->fog : NULL, level->renderScale);

        GEditorRefreshHistoryMenu(hwnd);

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
           states matter, so they can never be stale. Saving requires
           an open project. */
        EnableMenuItem((HMENU)wparam, ID_FILE_SAVE_PROJECT, MF_BYCOMMAND | (g_Project.name[0] != '\0' ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem((HMENU)wparam, ID_FILE_IMPORT_IMAGE, MF_BYCOMMAND | (g_Project.name[0] != '\0' ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem((HMENU)wparam, ID_TOOLS_CREATE_ROM, MF_BYCOMMAND | (g_Project.name[0] != '\0' ? MF_ENABLED : MF_GRAYED));
        GEditorUpdateHistoryMenu((HMENU)wparam);
        CheckMenuItem((HMENU)wparam, ID_VIEW_BACKFACE_CULLING, MF_BYCOMMAND | (ViewportGetBackfaceCulling(g_Viewport) ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem((HMENU)wparam, ID_VIEW_BG_STATISTICS, MF_BYCOMMAND | (ViewportGetBgStatisticsVisible(g_Viewport) ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem((HMENU)wparam, ID_VIEW_FOG, MF_BYCOMMAND | (ViewportGetFogVisible(g_Viewport) ? MF_CHECKED : MF_UNCHECKED));
        EnableMenuItem((HMENU)wparam, ID_VIEW_HIDE_SELECTED, MF_BYCOMMAND |
            (ViewportGetTool(g_Viewport) == EDITOR_TOOL_FACE_SELECT && ViewportGetSelectedBgFaceCount(g_Viewport) > 0
                ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem((HMENU)wparam, ID_VIEW_UNHIDE_ALL, MF_BYCOMMAND |
            (ViewportHasHiddenBgFaces(g_Viewport) ? MF_ENABLED : MF_GRAYED));
        return 0;

    case MODELEDITOR_CHANGED:
    {
        const char *why="";
        if (g_CurrentLevelIndex < g_Project.levelcount && !GEditorReloadCurrentObjectsAndViewport(&why))
        { MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR); }
        GEditorRefreshSelectionDetails();
        GEditorRefreshHistoryMenu(hwnd);
        return 0;
    }

    case WM_COMMAND:
        ViewportCancelTransform(g_Viewport);
        if (LOWORD(wparam) >= ID_FILE_RECENT_PROJECT_FIRST
            && LOWORD(wparam) <= ID_FILE_RECENT_PROJECT_LAST)
        {
            DWORD index = LOWORD(wparam) - ID_FILE_RECENT_PROJECT_FIRST;
            if (index < g_RecentProjects.count)
            {
                GEditorOpenProject(hwnd, g_RecentProjects.paths[index]);
            }
            return 0;
        }
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
                        GEditorRememberProject();
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
                    GEditorOpenProject(hwnd, path);
                }
                return 0;
            }

            case ID_FILE_SAVE_PROJECT:
                GEditorSaveProject(hwnd);
                return 0;

            case ID_FILE_IMPORT_IMAGE:
            {
                DWORD id;
                if (g_Project.name[0] != '\0' && ImageImportShow(hwnd, g_Project.dir, &id))
                {
                    GEditorRefreshImageViews(hwnd, id, TRUE);
                }
                return 0;
            }

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

            case ID_VIEW_BG_STATISTICS:
                ViewportSetBgStatisticsVisible(g_Viewport,
                    !ViewportGetBgStatisticsVisible(g_Viewport));
                return 0;

            case ID_VIEW_FOG:
                ViewportSetFogVisible(g_Viewport, !ViewportGetFogVisible(g_Viewport));
                return 0;

            case ID_VIEW_HIDE_SELECTED:
                if (!ViewportHideSelectedBgFaces(g_Viewport))
                { MessageBox(hwnd, "Not enough memory to hide the selected faces.", GEDITOR_TITLE, MB_ICONERROR); }
                return 0;

            case ID_VIEW_UNHIDE_ALL:
                ViewportUnhideAllBgFaces(g_Viewport);
                return 0;

            case ID_TOOLS_UV_EDITOR:
                if (!UVEditorShow(hwnd, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE)))
                {
                    MessageBox(hwnd, "Could not open the UV Editor window.",
                               GEDITOR_TITLE, MB_ICONERROR);
                }
                else
                {
                    UVEditorRefreshSelection(g_Viewport, &g_CurrentBgDocument);
                }
                return 0;

            case ID_TOOLS_MODEL_EDITOR:
                if (!ModelEditorShow(hwnd, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), g_Project.dir))
                {
                    MessageBox(hwnd, "Could not open the Model Editor window.", GEDITOR_TITLE, MB_ICONERROR);
                }
                return 0;

            case ID_TOOLS_CREATE_ROM:
                GEditorPromptForRomExport(hwnd);
                return 0;

            case ID_FILE_EXIT:
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }

        break; /* anything else falls through to DefWindowProc */

    case WM_CLOSE:
        ViewportCancelTransform(g_Viewport);
        if (GEditorConfirmExit(hwnd))
        {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        /* The window is gone; ask the message loop to stop. Without
           this the process keeps running after the window closes. */
        SetupFileFree(&g_CurrentSetup);
        StanFileFree(&g_CurrentStan);
        ObjectGeometryFree(&g_CurrentObjects);
        EditHistoryFree(&g_EditHistory);
        BgDocumentFree(&g_CurrentBgDocument);
        BgFileFree(&g_CurrentBg);
        ImageEditsReset();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}


/* Selection notifications are synchronous and can nest during asset rebuilds.
 * Finish only the outer frame message: one click/marquee gets one entry, while
 * an edit and its automatic reselection stay together in the edit's entry. */
static LRESULT CALLBACK GEditorWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    static unsigned int depth;
    ULONGLONG revision = g_EditHistory.currentstaterevision;
    LRESULT result;
    depth++;
    result = GEditorDispatchMessage(hwnd, msg, wparam, lparam);
    if (depth == 1)
    {
        if (g_EditHistory.currentstaterevision && (g_SelectionHistoryPending
            || g_SelectionHistoryReset || revision != g_EditHistory.currentstaterevision))
        {
            void *selection = NULL;
            size_t size = 0;
            const char *why = "out of memory capturing the selection.";
            BOOL record = !g_SelectionHistoryReset && !g_SelectionHistoryNavigation
                && revision == g_EditHistory.currentstaterevision;
            if (!GEditorCaptureSelection(&selection, &size)
                || !EditHistorySetSelection(&g_EditHistory, selection, size, record, &why))
            {
                /* Do not leave an untracked selection masquerading as the
                 * current undo state if memory is exhausted. */
                GEditorRestoreHistorySelection(hwnd);
                MessageBox(hwnd, why, GEDITOR_TITLE, MB_ICONERROR);
            }
            free(selection);
            GEditorRefreshHistoryMenu(hwnd);
        }
        g_SelectionHistoryPending = FALSE;
        g_SelectionHistoryReset = FALSE;
        g_SelectionHistoryNavigation = FALSE;
    }
    depth--;
    return result;
}

/* W/E/R are editor shortcuts only outside camera flight. Native edit fields
   retain their text input, including the E in scientific notation. */
static BOOL GEditorHandleTransformHotkey(HWND frame, const MSG *message)
{
    char classname[32] = "";
    TransformMode mode;

    if (message == NULL || g_Viewport == NULL || message->message != WM_KEYDOWN
        || (message->wParam != 'W' && message->wParam != 'E' && message->wParam != 'R')
        || ViewportIsFlying(g_Viewport)
        || (message->hwnd != frame && !IsChild(frame, message->hwnd))
        || (GetKeyState(VK_CONTROL) & 0x8000)
        || (GetKeyState(VK_MENU) & 0x8000)
        || (GetKeyState(VK_SHIFT) & 0x8000)) { return FALSE; }
    GetClassName(message->hwnd, classname, sizeof(classname));
    if (lstrcmpi(classname, "Edit") == 0 || lstrcmpi(classname, "ComboBox") == 0
        || lstrcmpi(classname, "ComboLBox") == 0) { return FALSE; }

    /* Do not repeatedly cancel/restart previews while a key is held. */
    if (message->lParam & ((LPARAM)1 << 30)) { return TRUE; }
    mode = message->wParam == 'E' ? TRANSFORM_ROTATE : message->wParam == 'R' ? TRANSFORM_SCALE : TRANSFORM_MOVE;
    if (ViewportGetTransformMode(g_Viewport) != mode)
    {
        SendMessage(frame, RIGHTPANEL_WM_TRANSFORM_MODE, mode, 0);
    }
    return TRUE;
}

/* Keep H as text in input fields and keep visibility shortcuts scoped to
   the main editor, so typing in the UV editor cannot hide main-view faces. */
static BOOL GEditorHandleVisibilityHotkey(HWND frame, const MSG *message)
{
    char classname[32] = "";
    BOOL unhide;
    if (message == NULL || g_Viewport == NULL
        || (message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN)
        || message->wParam != 'H' || ViewportIsFlying(g_Viewport)
        || (message->hwnd != frame && !IsChild(frame, message->hwnd))
        || (GetKeyState(VK_CONTROL) & 0x8000)
        || (GetKeyState(VK_SHIFT) & 0x8000)) { return FALSE; }
    GetClassName(message->hwnd, classname, sizeof(classname));
    if (lstrcmpi(classname, "Edit") == 0 || lstrcmpi(classname, "ComboBox") == 0
        || lstrcmpi(classname, "ComboLBox") == 0) { return FALSE; }
    unhide = (GetKeyState(VK_MENU) & 0x8000) != 0;
    SendMessage(frame, WM_COMMAND, unhide ? ID_VIEW_UNHIDE_ALL : ID_VIEW_HIDE_SELECTED, 0);
    return TRUE;
}

/* F also works during camera flight, but remains text in property inputs.
   Consume auto-repeat so holding F does not flicker between modes. */
static BOOL GEditorHandleFogHotkey(HWND frame, const MSG *message)
{
    char classname[32] = "";
    if (message == NULL || g_Viewport == NULL || message->message != WM_KEYDOWN
        || message->wParam != 'F'
        || (message->hwnd != frame && !IsChild(frame, message->hwnd))
        || (GetKeyState(VK_CONTROL) & 0x8000)
        || (GetKeyState(VK_MENU) & 0x8000)
        || (GetKeyState(VK_SHIFT) & 0x8000)) { return FALSE; }
    GetClassName(message->hwnd, classname, sizeof(classname));
    if (lstrcmpi(classname, "Edit") == 0 || lstrcmpi(classname, "ComboBox") == 0
        || lstrcmpi(classname, "ComboLBox") == 0) { return FALSE; }
    if (!(message->lParam & ((LPARAM)1 << 30)))
    { SendMessage(frame, WM_COMMAND, ID_VIEW_FOG, 0); }
    return TRUE;
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

    RecentProjectsLoad(&g_RecentProjects);
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
                if (!ModelEditorHandleMessage(&msg)
                    && !UVEditorHandleMessage(&msg)
                    && !GEditorHandleFogHotkey(hwnd, &msg)
                    && !GEditorHandleVisibilityHotkey(hwnd, &msg)
                    && !GEditorHandleTransformHotkey(hwnd, &msg)
                    && !RightPanelHandleMessage(g_RightPanel, &msg)
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
            /* Idle: block until input or an animated-monitor repaint timer. */
            if (GetMessage(&msg, NULL, 0, 0) <= 0)
            {
                break;
            }
            if (!ModelEditorHandleMessage(&msg)
                && !UVEditorHandleMessage(&msg)
                && !GEditorHandleFogHotkey(hwnd, &msg)
                && !GEditorHandleVisibilityHotkey(hwnd, &msg)
                && !GEditorHandleTransformHotkey(hwnd, &msg)
                && !RightPanelHandleMessage(g_RightPanel, &msg)
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
