#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "modeleditor.h"
#include "modeledits.h"
#include "newprops.h"
#include "modelload.h"
#include "viewport.h"
#include "uveditor.h"
#include "resource.h"
#include "browser.h"
#include "colorpicker.h"
#include "tooltoolbar.h"

typedef struct ModelEditorEntry {
    char name[MAX_PATH];
    const char *folder;
} ModelEditorEntry;

static HWND g_ModelEditor, g_ModelViewport, g_ModelBrowser;
static HWND g_ModelColorPicker, g_ModelPaintToolbar;
static char g_ModelProject[MAX_PATH];
static ModelEditorEntry *g_ModelEntries;
static int g_ModelCount;
static int g_ModelSelected = -1;
static ModelSource g_ModelSource;
static DWORD g_ModelRevision;
static BOOL g_ModelAllLods;
static BOOL g_ModelCompleting;
static BOOL g_ModelSampling;
#define MODEL_PAINT_HISTORY_LIMIT 256
static ModelVertexPaint g_ModelPaintHistory[MODEL_PAINT_HISTORY_LIMIT];
static int g_ModelPaintCount, g_ModelPaintPosition;
static void ModelEditorProperties(void);
static void ModelEditorGroups(void);
static BOOL ModelEditorFaceVisible(DWORD face)
{
    return !g_ModelSource.closestpreview || g_ModelAllLods || g_ModelSource.faces[face].closest;
}
static const int g_ModelCombos[] = { IDC_MODEL_CHARACTERS, IDC_MODEL_ITEMS, IDC_MODEL_PROPS };

/* Prefer an exact name, then the first prefix match in the sorted list.
 * Win32's combo-box searches are case-insensitive. */
static LRESULT ModelEditorFindName(HWND combo, const char *name)
{
    LRESULT row;
    if (name[0] == '\0') { return CB_ERR; }
    row = SendMessage(combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)name);
    if (row == CB_ERR) { row = SendMessage(combo, CB_FINDSTRING, (WPARAM)-1, (LPARAM)name); }
    return row;
}

static void ModelEditorCompleteName(HWND combo)
{
    char text[MAX_PATH];
    DWORD start = 0, end = 0;
    int length;
    LRESULT row;
    if (g_ModelCompleting) { return; }
    length = GetWindowText(combo, text, sizeof(text));
    SendMessage(combo, CB_GETEDITSEL, (WPARAM)&start, (LPARAM)&end);
    /* Leave selections and edits in the middle of a name alone. */
    if (!length || start != end || end != (DWORD)length) { return; }
    row = ModelEditorFindName(combo, text);
    if (row == CB_ERR) { return; }
    g_ModelCompleting = TRUE;
    SendMessage(combo, CB_SETCURSEL, row, 0);
    SendMessage(combo, CB_SETEDITSEL, 0, MAKELPARAM(length, -1));
    g_ModelCompleting = FALSE;
}

static LRESULT CALLBACK ModelEditorNameEditProc(HWND hwnd, UINT message, WPARAM wparam,
                                               LPARAM lparam, UINT_PTR id, DWORD_PTR data)
{
    /* Do not immediately put the completion back after Backspace, Delete,
     * Cut or Undo. Paste and ordinary character input still complete. */
    if ((message == WM_KEYDOWN && wparam == VK_DELETE)
        || (message == WM_CHAR && wparam == VK_BACK)
        || message == WM_CUT || message == WM_CLEAR || message == WM_UNDO || message == EM_UNDO)
    {
        BOOL previous = g_ModelCompleting;
        LRESULT result;
        g_ModelCompleting = TRUE;
        result = DefSubclassProc(hwnd, message, wparam, lparam);
        g_ModelCompleting = previous;
        return result;
    }
    if (message == WM_NCDESTROY) { RemoveWindowSubclass(hwnd, ModelEditorNameEditProc, id); }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

static void ModelEditorClearViewport(void)
{
    g_ModelPaintCount = g_ModelPaintPosition = 0;
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
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_ADD),g_ModelProject[0]!=0);
    for (category = 0; category < 3; category++)
    {
        SendDlgItemMessage(g_ModelEditor, g_ModelCombos[category], CB_RESETCONTENT, 0, 0);
    }
    free(g_ModelEntries); g_ModelEntries = NULL; g_ModelCount = 0;
    if (g_ModelProject[0] != '\0')
    {
        const char *why="";
        if (!NewPropsOpen(g_ModelProject,&why))
        { SetDlgItemText(g_ModelEditor,IDC_MODEL_STATUS,why);return; }
        complete = ModelEditorAddFolder("characters", 0)
            && ModelEditorAddFolder("guns", 1)
            && ModelEditorAddFolder("casings", 1)
            && ModelEditorAddFolder("objects", 2);
        for (int i=0;complete && i<NewPropsCount();i++)
        {
            const char *name;int j;LRESULT row;ModelEditorEntry *grown;
            NewPropsDefinition(CUSTOM_PROP_BASE+i,&name,NULL);
            for (j=0;j<g_ModelCount;j++) if (!strcmp(g_ModelEntries[j].name,name)) break;
            if (j<g_ModelCount) continue;
            grown=realloc(g_ModelEntries,((size_t)g_ModelCount+1)*sizeof(*grown));
            if (!grown) { complete=FALSE;break; }
            g_ModelEntries=grown;lstrcpyn(grown[g_ModelCount].name,name,MAX_PATH);
            grown[g_ModelCount].folder="objects";
            row=SendDlgItemMessage(g_ModelEditor,IDC_MODEL_PROPS,CB_ADDSTRING,0,(LPARAM)name);
            if (row<0) { complete=FALSE;break; }
            SendDlgItemMessage(g_ModelEditor,IDC_MODEL_PROPS,CB_SETITEMDATA,row,g_ModelCount++);
        }
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

/* Reload by asset identity: an editable selector may contain an uncommitted
 * suggestion while materials or face properties of the open model change. */
static void ModelEditorLoad(int index, BOOL framecamera)
{
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
    if (index < 0 || index >= g_ModelCount) { return; }
    g_ModelSelected = index;
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_EXPORT),TRUE);
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_IMPORT),TRUE);
    entry = &g_ModelEntries[index];
    if (framecamera) { ModelEditorClearViewport(); }
    previous = SetCursor(LoadCursor(NULL, IDC_WAIT));
    ModelFreeSource(&g_ModelSource);
    if (ModelEditsReadSource(g_ModelProject, entry->name, &g_ModelSource, &g_ModelRevision, &why)
        && g_ModelSource.count <= INT_MAX / 3)
    {
        DWORD face;
        /* Material/topology edits invalidate the old native paint offsets.
           Changing only the preview's LOD or saving keeps history usable. */
        if (g_ModelPaintCount && g_ModelRevision != (g_ModelPaintPosition
            ? g_ModelPaintHistory[g_ModelPaintPosition-1].afterRevision : g_ModelPaintHistory[0].beforeRevision))
        { g_ModelPaintCount = g_ModelPaintPosition = 0; }
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

static void ModelEditorSelect(int category, BOOL framecamera)
{
    HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
    LRESULT row = SendMessage(combo, CB_GETCURSEL, 0, 0), index;
    if (row == CB_ERR) { return; }
    index = SendMessage(combo, CB_GETITEMDATA, row, 0);
    if (index < 0 || index >= g_ModelCount) { return; }
    for (int other = 0; other < 3; other++)
    {
        if (other != category) { SendDlgItemMessage(g_ModelEditor, g_ModelCombos[other], CB_SETCURSEL, -1, 0); }
    }
    ModelEditorLoad((int)index, framecamera);
}

static void ModelEditorSetPaint(BOOL enabled)
{
    EditorTool tool = enabled ? EDITOR_TOOL_VERTEX_PAINT : EDITOR_TOOL_FACE_SELECT;
    SendMessage(g_ModelViewport, WM_CANCELMODE, 0, 0);
    ViewportSetTool(g_ModelViewport, tool);
    ToolToolbarSetTool(g_ModelPaintToolbar, tool);
    SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, enabled
        ? "Click a face to paint its nearest vertex. Drag to orbit; 4 exits painting. Save Project to keep edits."
        : "Click to select faces; drag either mouse button to orbit. 4 toggles vertex painting.");
    SetFocus(g_ModelViewport);
}

static BOOL ModelEditorVertexColor(const ViewportBgVertexHit *hit, BOOL sample)
{
    DWORD corner;
    unsigned char rgba[4];
    const BgVertex *vertex;
    const char *why = "";
    ModelVertexPaint change;
    if (!hit || g_ModelSelected < 0 || hit->corner >= 3 || !hit->face.faceid
        || hit->face.faceid > g_ModelSource.count) { return FALSE; }
    corner = (hit->face.faceid - 1) * 3 + hit->corner;
    vertex = &g_ModelSource.vertices[corner];
    if (sample)
    {
        rgba[0] = vertex->r; rgba[1] = vertex->g; rgba[2] = vertex->b; rgba[3] = vertex->a;
        ColorPickerSetColor(g_ModelColorPicker, rgba);
        SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, "Vertex RGBA sampled. Click another vertex to paint it.");
        return TRUE;
    }
    ColorPickerGetColor(g_ModelColorPicker, rgba);
    if (vertex->r == rgba[0] && vertex->g == rgba[1] && vertex->b == rgba[2] && vertex->a == rgba[3])
    { return TRUE; }
    if (!ModelEditsSetVertexColor(g_ModelProject, g_ModelEntries[g_ModelSelected].name,
                                 g_ModelRevision, corner, rgba, &change, &why))
    {
        SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, why);
        return FALSE;
    }
    if (!memcmp(change.before, change.after, 4)) { return TRUE; }
    if (g_ModelPaintPosition == MODEL_PAINT_HISTORY_LIMIT)
    {
        memmove(g_ModelPaintHistory, g_ModelPaintHistory+1,
                (MODEL_PAINT_HISTORY_LIMIT-1)*sizeof(*g_ModelPaintHistory));
        g_ModelPaintPosition--;
    }
    g_ModelPaintHistory[g_ModelPaintPosition++] = change;
    g_ModelPaintCount = g_ModelPaintPosition; /* A new stroke replaces redo. */
    ModelEditorLoad(g_ModelSelected, FALSE);
    SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, "Vertex RGBA painted. Save Project to keep the model changes.");
    SendMessage(GetWindow(g_ModelEditor, GW_OWNER), MODELEDITOR_CHANGED, 0, 0);
    return TRUE;
}

static void ModelEditorUndoPaint(BOOL redo)
{
    const char *why = "";
    int position = redo ? g_ModelPaintPosition : g_ModelPaintPosition-1;
    if (g_ModelSelected < 0 || position < 0 || position >= g_ModelPaintCount) { return; }
    SendMessage(g_ModelViewport, WM_CANCELMODE, 0, 0);
    if (!ModelEditsRestoreVertexColor(g_ModelProject, g_ModelEntries[g_ModelSelected].name,
                                      &g_ModelPaintHistory[position], redo, &why))
    {
        g_ModelPaintCount = g_ModelPaintPosition = 0;
        SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, why);
        return;
    }
    g_ModelPaintPosition += redo ? 1 : -1;
    ModelEditorLoad(g_ModelSelected, FALSE);
    SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS, redo
        ? "Vertex paint redone. Save Project to keep model changes."
        : "Vertex paint undone. Save Project to keep model changes.");
    SendMessage(GetWindow(g_ModelEditor, GW_OWNER), MODELEDITOR_CHANGED, 0, 0);
}

static BOOL ModelEditorPaintKey(MSG *message)
{
    if (message->message != WM_KEYDOWN) { return FALSE; }
    /* Handle sampling before IsDialogMessage translates Escape to Close. */
    if (message->wParam == VK_ESCAPE && g_ModelSampling)
    {
        SendMessage(g_ModelViewport, WM_CANCELMODE, 0, 0);
        return TRUE;
    }
    if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)
        && (message->wParam == 'Z' || message->wParam == 'Y'))
    {
        char classname[32] = "";
        GetClassName(message->hwnd, classname, sizeof(classname));
        if (lstrcmpi(classname, "Edit") != 0)
        {
            ModelEditorUndoPaint(message->wParam == 'Y' || (GetKeyState(VK_SHIFT) & 0x8000));
            return TRUE;
        }
    }
    return FALSE;
}

static void ModelEditorAcceptName(int category)
{
    HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
    char text[MAX_PATH];
    LRESULT row;
    GetWindowText(combo, text, sizeof(text));
    row = ModelEditorFindName(combo, text);
    if (row == CB_ERR)
    {
        SetDlgItemText(g_ModelEditor, IDC_MODEL_STATUS,
            "No matching model. Type the beginning of a model name, then press Enter.");
        return;
    }
    /* Closing the popup can generate combo notifications. Commit only once. */
    g_ModelCompleting = TRUE;
    SendMessage(combo, CB_SHOWDROPDOWN, FALSE, 0);
    SendMessage(combo, CB_SETCURSEL, row, 0);
    g_ModelCompleting = FALSE;
    ModelEditorSelect(category, TRUE);
}

static BOOL ModelEditorNameKey(MSG *message)
{
    if (message->message != WM_KEYDOWN || message->wParam != VK_RETURN) { return FALSE; }
    for (int category = 0; category < 3; category++)
    {
        HWND combo = GetDlgItem(g_ModelEditor, g_ModelCombos[category]);
        if (message->hwnd == combo || IsChild(combo, message->hwnd))
        {
            ModelEditorAcceptName(category);
            return TRUE;
        }
    }
    return FALSE;
}

static void ModelEditorGroups(void)
{
    HWND list=GetDlgItem(g_ModelEditor,IDC_MODEL_MATERIAL_LIST);
    LRESULT old=SendMessage(list,LB_GETCURSEL,0,0),top=SendMessage(list,LB_GETTOPINDEX,0,0);
    DWORD slot;
    SendMessage(list,WM_SETREDRAW,FALSE,0);
    SendMessage(list,LB_RESETCONTENT,0,0);
    for (slot=0;slot<g_ModelSource.materials.count;slot++)
        SendMessage(list,LB_ADDSTRING,0,(LPARAM)g_ModelSource.materials.slots[slot].name);
    if (old>=0 && (DWORD)old<g_ModelSource.materials.count) SendMessage(list,LB_SETCURSEL,old,0);
    if (top>=0) SendMessage(list,LB_SETTOPINDEX,top,0);
    SendMessage(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,NULL,TRUE);
    EnableWindow(list,g_ModelSource.materials.count!=0);
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_SELECT_ALL),g_ModelSource.count!=0);
    EnableWindow(GetDlgItem(g_ModelEditor,IDC_MODEL_LODS),g_ModelSource.closestpreview);
}

static void ModelEditorDrawMaterial(const DRAWITEMSTRUCT *draw)
{
    DWORD slot=draw->itemID;
    RECT r=draw->rcItem,thumbrect,textrect;
    TexThumb thumb={0};unsigned char pixels[TEX_THUMB_MAX*TEX_THUMB_MAX*4];
    BITMAPINFO bmi={0};
    BOOL selected=(draw->itemState & ODS_SELECTED)!=0;
    char caption[192],image[64];WCHAR label[192];
    int pad=6,side=max(16,r.bottom-r.top-pad*2);
    if (slot>=g_ModelSource.materials.count) return;
    const ModelMaterialSlot *material=&g_ModelSource.materials.slots[slot];
    FillRect(draw->hDC,&r,GetSysColorBrush(selected?COLOR_HIGHLIGHT:COLOR_WINDOW));
    SetBkMode(draw->hDC,TRANSPARENT);
    SetTextColor(draw->hDC,GetSysColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT));
    thumbrect=r;thumbrect.left+=pad;thumbrect.top+=pad;
    thumbrect.right=thumbrect.left+side;thumbrect.bottom=thumbrect.top+side;
    FillRect(draw->hDC,&thumbrect,GetSysColorBrush(COLOR_BTNFACE));
    if (BrowserCopyImageThumbnail(g_ModelBrowser,material->texture,&thumb,pixels))
    {
        int width=side,height=side,x,y;
        if (thumb.w>thumb.h) height=max(1,side*thumb.h/thumb.w);
        else width=max(1,side*thumb.w/thumb.h);
        x=thumbrect.left+(side-width)/2;y=thumbrect.top+(side-height)/2;
        bmi.bmiHeader.biSize=sizeof(bmi.bmiHeader);bmi.bmiHeader.biWidth=TEX_THUMB_MAX;
        bmi.bmiHeader.biHeight=-TEX_THUMB_MAX;bmi.bmiHeader.biPlanes=1;
        bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
        StretchDIBits(draw->hDC,x,y,width,height,0,0,thumb.w,thumb.h,pixels,&bmi,DIB_RGB_COLORS,SRCCOPY);
    }
    FrameRect(draw->hDC,&thumbrect,GetSysColorBrush(COLOR_3DSHADOW));
    textrect=r;textrect.left=thumbrect.right+pad;textrect.right-=pad;textrect.top+=pad;
    textrect.bottom=textrect.top+(r.bottom-r.top)/2;
    snprintf(caption,sizeof(caption),"%lu. %s",(unsigned long)slot+1,material->name);
    if (MultiByteToWideChar(CP_UTF8,0,caption,-1,label,sizeof(label)/sizeof(label[0])))
        DrawTextW(draw->hDC,label,-1,&textrect,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    textrect.top=textrect.bottom;textrect.bottom=r.bottom-pad;
    if (material->texture==BG_TEX_NONE) strcpy(image,"No Texture");
    else snprintf(image,sizeof(image),"Image %04lX",(unsigned long)material->texture);
    DrawText(draw->hDC,image,-1,&textrect,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    if (draw->itemState & ODS_FOCUS) DrawFocusRect(draw->hDC,&r);
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
    DWORD face,count=0;
    LRESULT slot=SendDlgItemMessage(g_ModelEditor,IDC_MODEL_MATERIAL_LIST,LB_GETCURSEL,0,0);
    BgFaceRef *refs=calloc(g_ModelSource.count?g_ModelSource.count:1,sizeof(*refs));
    if (!refs) return;
    for (face=0;face<g_ModelSource.count;face++)
        if (ModelEditorFaceVisible(face) && (all || (slot>=0 && g_ModelSource.materials.faces[face].slot==(DWORD)slot)))
        { refs[count].faceid=face+1;refs[count++].room=1; }
    if (count) ViewportSelectBgFaces(g_ModelViewport,refs,count);
    free(refs);
}

void ModelEditorSetImageBrowser(HWND browser) { g_ModelBrowser=browser; }
BOOL ModelEditorCanAssignImages(void)
{
    return g_ModelEditor && IsWindowVisible(g_ModelEditor) && g_ModelSelected>=0
        && g_ModelSource.materials.count!=0;
}
BOOL ModelEditorDropImage(DWORD texture,POINT screen)
{
    HWND list;
    RECT client;
    LRESULT hit;
    const char *why="";
    BOOL ok;
    if (!ModelEditorCanAssignImages()) return FALSE;
    list=GetDlgItem(g_ModelEditor,IDC_MODEL_MATERIAL_LIST);
    if (WindowFromPoint(screen)!=list) return FALSE;
    ScreenToClient(list,&screen);GetClientRect(list,&client);
    if (!PtInRect(&client,screen)) return FALSE; /* Scrollbar and border are not slots. */
    hit=SendMessage(list,LB_ITEMFROMPOINT,0,MAKELPARAM(screen.x,screen.y));
    if (HIWORD(hit) || (DWORD)LOWORD(hit)>=g_ModelSource.materials.count) return FALSE;
    { RECT item;
      if (SendMessage(list,LB_GETITEMRECT,LOWORD(hit),(LPARAM)&item)==LB_ERR || !PtInRect(&item,screen)) return FALSE; }
    SendMessage(list,LB_SETCURSEL,LOWORD(hit),0);
    ok=ModelEditsSetMaterial(g_ModelProject,g_ModelEntries[g_ModelSelected].name,
        g_ModelRevision,LOWORD(hit),texture,&why);
    if (!ok) { MessageBox(g_ModelEditor,why,"Assign Material Image",MB_ICONERROR);return TRUE; }
    ModelEditorRefreshImages();ModelEditorSelectGroup(FALSE);
    SetDlgItemText(g_ModelEditor,IDC_MODEL_STATUS,"Material image updated. Save Project to keep the assignment.");
    SendMessage(GetWindow(g_ModelEditor,GW_OWNER),MODELEDITOR_CHANGED,0,0);
    return TRUE;
}

void ModelEditorRefreshImages(void)
{
    if (g_ModelEditor == NULL || g_ModelSelected < 0) { return; }
    ModelEditorLoad(g_ModelSelected, FALSE);
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
    ModelEditorLoad(g_ModelSelected, TRUE);
    snprintf(message,sizeof(message),"Imported all LODs: %lu to %lu tris. Save Project to keep the replacement.",
        (unsigned long)before,(unsigned long)after);
    SetDlgItemText(g_ModelEditor,IDC_MODEL_STATUS,message);
    SendMessage(GetWindow(g_ModelEditor,GW_OWNER),MODELEDITOR_CHANGED,0,0);
}

static INT_PTR CALLBACK ModelEditorNewPropDialog(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    char *name=(char *)GetWindowLongPtr(hwnd,DWLP_USER);
    if (message==WM_INITDIALOG)
    {
        SetWindowLongPtr(hwnd,DWLP_USER,lparam);
        SendDlgItemMessage(hwnd,IDC_NEW_PROP_NAME,EM_SETLIMITTEXT,63,0);
        SetDlgItemText(hwnd,IDC_NEW_PROP_NAME,(const char *)lparam);return TRUE;
    }
    if (message==WM_COMMAND && LOWORD(wparam)==IDOK)
    { GetDlgItemText(hwnd,IDC_NEW_PROP_NAME,name,64);EndDialog(hwnd,IDOK);return TRUE; }
    if ((message==WM_COMMAND && LOWORD(wparam)==IDCANCEL) || message==WM_CLOSE)
    { EndDialog(hwnd,IDCANCEL);return TRUE; }
    return FALSE;
}

static void ModelEditorAddProp(void)
{
    OPENFILENAME ofn={0};char path[MAX_PATH]="",name[64],stem[64],text[160];
    const char *base,*why="";char *dot;DWORD triangles;HCURSOR cursor;BOOL ok;
    HWND owner=GetWindow(g_ModelEditor,GW_OWNER);
    HINSTANCE instance=(HINSTANCE)GetWindowLongPtr(g_ModelEditor,GWLP_HINSTANCE);
    if (!g_ModelProject[0]) return;
    ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=g_ModelEditor;ofn.lpstrFile=path;ofn.nMaxFile=sizeof(path);
    ofn.lpstrTitle="Add a new prop model";ofn.lpstrFilter="glTF models (*.glb;*.gltf)\0*.glb;*.gltf\0\0";
    ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    if (!GetOpenFileName(&ofn)) return;
    base=strrchr(path,'\\');base=base?base+1:path;lstrcpyn(stem,base,sizeof(stem));
    dot=strrchr(stem,'.');if (dot) *dot=0;
    if (stem[0]=='P' && strlen(stem)>1 && stem[strlen(stem)-1]=='Z') lstrcpyn(name,stem,sizeof(name));
    else snprintf(name,sizeof(name),"P%.60sZ",stem);
    if (DialogBoxParam(instance,MAKEINTRESOURCE(IDD_ADD_PROP_MODEL),g_ModelEditor,ModelEditorNewPropDialog,(LPARAM)name)!=IDOK) return;
    cursor=SetCursor(LoadCursor(NULL,IDC_WAIT));
    ok=NewPropsImport(g_ModelProject,name,path,FALSE,&triangles,&why);SetCursor(cursor);
    if (!ok) { MessageBox(g_ModelEditor,why,"Add Prop Model",MB_ICONERROR);return; }
    SendMessage(owner,MODELEDITOR_CHANGED,1,0); /* Refresh browser and new IDs before selecting. */
    ModelEditorSetProject(g_ModelProject);
    ModelEditorOpenModel(owner,instance,g_ModelProject,name,&why);
    snprintf(text,sizeof(text),"Added %s (%lu triangles). Drag it from Models into the level; Save Project to keep it.",name,(unsigned long)triangles);
    SetDlgItemText(g_ModelEditor,IDC_MODEL_STATUS,text);
}

/* Move every control before repainting. Copying pixels or painting a group
 * box while its siblings still occupy their old positions leaves fragments
 * of labels and borders behind during a resize. */
static void ModelEditorPlaceControl(HWND control, int x, int y, int width, int height)
{
    SetWindowPos(control, NULL, x, y, width, height,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
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
        ModelEditorPlaceControl(GetDlgItem(hwnd, labels[category]), x, margin, column, units.bottom);
        ModelEditorPlaceControl(GetDlgItem(hwnd, g_ModelCombos[category]), x, margin + units.bottom,
                                column, units.bottom * 12);
    }
    ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_EXPORT),margin,margin*2+units.bottom*2,units.right,units.bottom);
    ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_IMPORT),margin*2+units.right,margin*2+units.bottom*2,units.right,units.bottom);
    ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_ADD),margin*3+units.right*2,margin*2+units.bottom*2,units.right+margin,units.bottom);
    if (g_ModelPaintToolbar)
        ModelEditorPlaceControl(g_ModelPaintToolbar, margin*5+units.right*3,
                                units.top-TOOLTOOLBAR_HEIGHT-margin/2, TOOLTOOLBAR_HEIGHT, TOOLTOOLBAR_HEIGHT);
    if (g_ModelViewport != NULL)
    {
        ModelEditorPlaceControl(g_ModelViewport, 0, units.top, panelx, max(0, bottom - units.top));
    }
    {
        static const struct {int id,x,y,w,h;} controls[] = {
            {IDC_MODEL_SELECTION,8,16,98,12},{IDC_MODEL_SELECT_ALL,114,12,62,18},
            {IDC_MODEL_CURRENT,8,36,168,30},
            {IDC_MODEL_CULL_LABEL,8,70,80,12},{IDC_MODEL_CULL,8,84,80,100},
            {IDC_MODEL_SURFACE_LABEL,96,70,80,12},{IDC_MODEL_SURFACE,96,84,80,100},
            {IDC_MODEL_APPLY,8,110,168,20},{IDC_MODEL_LODS,8,138,168,16}
        };
        RECT dimensions={8,16,168,156},row={0,0,0,44};
        int facey,colory,colorheight,materialheight;
        size_t i;
        MapDialogRect(hwnd,&dimensions);MapDialogRect(hwnd,&row);
        facey=bottom-dimensions.bottom;
        colorheight=COLORPICKER_MODEL_HEIGHT+dimensions.top+margin;
        colory=facey-margin-colorheight;
        materialheight=max(0,colory-units.top-margin);
        ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_MATERIALS),panelx,units.top,panel.right-margin,materialheight);
        ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_MATERIAL_LIST),panelx+dimensions.left,units.top+dimensions.top,
            dimensions.right,max(0,materialheight-dimensions.top*3));
        ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_HINT),panelx+dimensions.left,units.top+materialheight-dimensions.top*2,
            dimensions.right,dimensions.top*2-margin);
        SendDlgItemMessage(hwnd,IDC_MODEL_MATERIAL_LIST,LB_SETITEMHEIGHT,0,row.bottom);
        ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_COLORS),panelx,colory,panel.right-margin,colorheight);
        if (g_ModelColorPicker)
            ModelEditorPlaceControl(g_ModelColorPicker,panelx+dimensions.left,colory+dimensions.top,
                                    dimensions.right,COLORPICKER_MODEL_HEIGHT);
        ModelEditorPlaceControl(GetDlgItem(hwnd,IDC_MODEL_PROPERTIES),panelx,facey,panel.right-margin,dimensions.bottom);
        for (i=0;i<sizeof(controls)/sizeof(controls[0]);i++)
        {
            RECT r={controls[i].x,controls[i].y,controls[i].w,controls[i].h};
            MapDialogRect(hwnd,&r);
            ModelEditorPlaceControl(GetDlgItem(hwnd,controls[i].id),panelx+r.left,facey+r.top,r.right,r.bottom);
        }
        SendDlgItemMessage(hwnd,IDC_MODEL_CULL,CB_SETDROPPEDWIDTH,dimensions.right,0);
    }

    ModelEditorPlaceControl(GetDlgItem(hwnd, IDC_MODEL_STATUS), margin, bottom + margin,
                            max(0, client.right - units.right - margin * 3), units.bottom);
    ModelEditorPlaceControl(GetDlgItem(hwnd, IDCANCEL), max(0, client.right - units.right - margin),
                            bottom + margin, units.right, units.bottom);
    /* The dialog clips child windows, so invalidate their full contents and
     * borders too once the entire layout is in its final position. */
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

static INT_PTR CALLBACK ModelEditorDialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        g_ModelEditor = hwnd;
        g_ModelCompleting = FALSE;
        for (int category = 0; category < 3; category++)
        {
            HWND combo = GetDlgItem(hwnd, g_ModelCombos[category]);
            COMBOBOXINFO info = {0};
            info.cbSize = sizeof(info);
            SendMessage(combo, CB_LIMITTEXT, MAX_PATH - 1, 0);
            if (GetComboBoxInfo(combo, &info))
            { SetWindowSubclass(info.hwndItem, ModelEditorNameEditProc, 1, 0); }
        }
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
    case WM_MEASUREITEM:
        if (((MEASUREITEMSTRUCT *)lparam)->CtlID==IDC_MODEL_MATERIAL_LIST)
        { ((MEASUREITEMSTRUCT *)lparam)->itemHeight=66;return TRUE; }
        break;
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT *)lparam)->CtlID==IDC_MODEL_MATERIAL_LIST)
        { ModelEditorDrawMaterial((const DRAWITEMSTRUCT *)lparam);return TRUE; }
        break;
    case WM_SIZE:
        ModelEditorLayout(hwnd);
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = { 0, 0, 660, 540 };
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
                           FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case VIEWPORT_WM_SELECTION_CHANGED:
        ModelEditorProperties(); return TRUE;
    case EDITTOOL_WM_SELECT:
        if (wparam == EDITOR_TOOL_VERTEX_PAINT)
            ModelEditorSetPaint(ViewportGetTool(g_ModelViewport) != EDITOR_TOOL_VERTEX_PAINT);
        return TRUE;
    case COLORPICKER_WM_PICK_COLOR:
        ModelEditorSetPaint(TRUE);
        ViewportSetColorPick(g_ModelViewport, TRUE);
        SetDlgItemText(hwnd, IDC_MODEL_STATUS, "Click a face to sample its nearest vertex's RGBA. Esc cancels.");
        return TRUE;
    case VIEWPORT_WM_COLOR_PICK_CHANGED:
        g_ModelSampling = (BOOL)wparam;
        ColorPickerSetSampling(g_ModelColorPicker, (BOOL)wparam); return TRUE;
    case VIEWPORT_WM_SAMPLE_VERTEX:
    case VIEWPORT_WM_PAINT_VERTEX:
        SetWindowLongPtr(hwnd, DWLP_MSGRESULT,
            ModelEditorVertexColor((const ViewportBgVertexHit *)lparam, message == VIEWPORT_WM_SAMPLE_VERTEX));
        return TRUE;
    case WM_COMMAND:
        if ((LOWORD(wparam)==IDC_MODEL_MATERIAL_LIST && HIWORD(wparam)==LBN_SELCHANGE)
            || LOWORD(wparam)==IDC_MODEL_SELECT_ALL)
        { ModelEditorSelectGroup(LOWORD(wparam)==IDC_MODEL_SELECT_ALL); return TRUE; }
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
        if (LOWORD(wparam)==IDC_MODEL_ADD) { ModelEditorAddProp();return TRUE; }
        if (LOWORD(wparam) >= IDC_MODEL_CHARACTERS && LOWORD(wparam) <= IDC_MODEL_PROPS)
        {
            if (HIWORD(wparam) == CBN_EDITCHANGE)
            { ModelEditorCompleteName((HWND)lparam); }
            else if (HIWORD(wparam) == CBN_SELENDOK && !g_ModelCompleting)
            { ModelEditorSelect(LOWORD(wparam) - IDC_MODEL_CHARACTERS, TRUE); }
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
        g_ModelColorPicker = NULL; g_ModelPaintToolbar = NULL;
        g_ModelSampling = FALSE;
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
        g_ModelColorPicker = ColorPickerCreateModel(g_ModelEditor, instance);
        g_ModelPaintToolbar = ToolToolbarCreatePaint(g_ModelEditor, instance);
        if (!g_ModelViewport || !g_ModelColorPicker || !g_ModelPaintToolbar)
        { DestroyWindow(g_ModelEditor); return FALSE; }
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
    if (ColorPickerHandleMessage(g_ModelColorPicker, message)
        || ToolToolbarHandleMessage(g_ModelPaintToolbar, message)) { return TRUE; }
    if (ModelEditorPaintKey(message)) { return TRUE; }
    /* Consume Enter in the combo's edit child before dialog navigation can
     * treat it as a default-button click. Typing itself never loads a model. */
    if (ModelEditorNameKey(message)) { return TRUE; }
    /* Project shortcuts are shared with the main frame's accelerators. */
    if (message->message == WM_KEYDOWN
        && (message->wParam == 'S' || message->wParam == 'T')
        && (GetKeyState(VK_CONTROL) & 0x8000)
        && !(GetKeyState(VK_MENU) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000))
    { return FALSE; }

    /* Keep dialog navigation and keyboard shortcuts inside this window. */
    if (!IsDialogMessage(g_ModelEditor, message))
    {
        TranslateMessage(message);
        DispatchMessage(message);
    }
    return TRUE;
}
