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
#include "newprops.h"
#include "modelload.h"
#include "viewport.h"
#include "uveditor.h"
#include "resource.h"
#include "browser.h"

typedef struct ModelEditorEntry {
    char name[MAX_PATH];
    const char *folder;
} ModelEditorEntry;

static HWND g_ModelEditor, g_ModelViewport, g_ModelBrowser;
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
    MoveWindow(GetDlgItem(hwnd,IDC_MODEL_ADD),margin*3+units.right*2,margin*2+units.bottom*2,units.right+margin,units.bottom,TRUE);
    if (g_ModelViewport != NULL)
    {
        MoveWindow(g_ModelViewport, 0, units.top, panelx, max(0, bottom - units.top), TRUE);
    }
    {
        static const struct {int id,x,y,w,h;} controls[] = {
            {IDC_MODEL_SELECTION,8,16,98,12},{IDC_MODEL_SELECT_ALL,114,12,62,18},
            {IDC_MODEL_CURRENT,8,38,168,36},
            {IDC_MODEL_CULL_LABEL,8,80,168,12},{IDC_MODEL_CULL,8,94,168,100},
            {IDC_MODEL_SURFACE_LABEL,8,118,168,12},{IDC_MODEL_SURFACE,8,132,168,100},
            {IDC_MODEL_APPLY,8,158,168,20},{IDC_MODEL_LODS,8,188,168,16}
        };
        RECT dimensions={8,16,168,212},row={0,0,0,44};
        int facey,materialheight;
        size_t i;
        MapDialogRect(hwnd,&dimensions);MapDialogRect(hwnd,&row);
        facey=bottom-dimensions.bottom;materialheight=max(0,facey-units.top-margin);
        MoveWindow(GetDlgItem(hwnd,IDC_MODEL_MATERIALS),panelx,units.top,panel.right-margin,materialheight,TRUE);
        MoveWindow(GetDlgItem(hwnd,IDC_MODEL_MATERIAL_LIST),panelx+dimensions.left,units.top+dimensions.top,
            dimensions.right,max(0,materialheight-dimensions.top*3),TRUE);
        MoveWindow(GetDlgItem(hwnd,IDC_MODEL_HINT),panelx+dimensions.left,units.top+materialheight-dimensions.top*2,
            dimensions.right,dimensions.top*2-margin,TRUE);
        SendDlgItemMessage(hwnd,IDC_MODEL_MATERIAL_LIST,LB_SETITEMHEIGHT,0,row.bottom);
        MoveWindow(GetDlgItem(hwnd,IDC_MODEL_PROPERTIES),panelx,facey,panel.right-margin,dimensions.bottom,TRUE);
        for (i=0;i<sizeof(controls)/sizeof(controls[0]);i++)
        {
            RECT r={controls[i].x,controls[i].y,controls[i].w,controls[i].h};
            MapDialogRect(hwnd,&r);
            MoveWindow(GetDlgItem(hwnd,controls[i].id),panelx+r.left,facey+r.top,r.right,r.bottom,TRUE);
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
