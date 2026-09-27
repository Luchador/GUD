#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderstudio.h"
#include "studioviewport.h"
#include "studioscene.h"
#include "studiogizmo.h"
#include "browser.h"

static HWND g_Studio, g_StudioViewport, g_StudioProperties, g_StudioTransform;
static StudioScene g_StudioScene;
static int g_StudioObject=-1,g_StudioMaterial=-1;
static BOOL g_StudioUpdating,g_StudioDragArmed,g_StudioDragging;
static unsigned g_StudioGeneration,g_StudioTransformDirty;
static int g_StudioTool=STUDIO_TRANSLATE;
static BOOL g_StudioLoading,g_StudioTriedInitialScene;
static POINT g_StudioDragPress;
static char g_StudioDragModel[MAX_PATH];
static void RenderStudioNewScene(HWND hwnd);
static char g_StudioProject[MAX_PATH];
static const char g_StudioNavigation[] = "W: move    E: rotate    R: scale    Z: frame    Drag handles to transform    Ctrl+S: save";

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

static void RenderStudioFillList(HWND list, const StudioFileEntry *entries, DWORD count,
    const char *select, const char **why)
{
    char previous[MAX_PATH] = "";
    LRESULT row = SendMessage(list, LB_GETCURSEL, 0, 0);
    if (select) { lstrcpyn(previous, select, sizeof(previous)); }
    else if (row != LB_ERR && SendMessage(list, LB_GETTEXTLEN, row, 0) < MAX_PATH)
    { SendMessage(list, LB_GETTEXT, row, (LPARAM)previous); }
    SendMessage(list, WM_SETREDRAW, FALSE, 0);
    SendMessage(list, LB_RESETCONTENT, 0, 0);
    int widest = 0; HDC dc = GetDC(list);
    HFONT oldfont = dc ? SelectObject(dc, (HFONT)SendMessage(list, WM_GETFONT, 0, 0)) : NULL;
    for (DWORD i = 0; i < count; i++)
    {
        LRESULT added = SendMessage(list, LB_ADDSTRING, 0, (LPARAM)entries[i].filename);
        if (added == LB_ERR || added == LB_ERRSPACE)
        { *why = "Not enough memory to list all studio files."; break; }
        if (dc)
        {
            SIZE size;
            if (GetTextExtentPoint32(dc, entries[i].filename, (int)strlen(entries[i].filename), &size))
                widest = max(widest, size.cx + 8);
        }
    }
    if (dc) { if (oldfont) { SelectObject(dc, oldfont); } ReleaseDC(list, dc); }
    SendMessage(list, LB_SETHORIZONTALEXTENT, widest, 0);
    if (previous[0])
    {
        row = SendMessage(list, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)previous);
        if (row != LB_ERR) { SendMessage(list, LB_SETCURSEL, row, 0); }
    }
    SendMessage(list, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(list, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
}

static StudioMaterial *RenderStudioMaterial(void)
{
    if (g_StudioObject<0 || (DWORD)g_StudioObject>=g_StudioScene.count) { return NULL; }
    StudioInstance *o=&g_StudioScene.objects[g_StudioObject];
    return g_StudioMaterial>=0 && (DWORD)g_StudioMaterial<o->materialcount ? &o->materials[g_StudioMaterial] : NULL;
}

static void RenderStudioProperties(void)
{
    StudioMaterial *m=RenderStudioMaterial(); char text[64];
    g_StudioUpdating=TRUE;
    for (int id=IDC_STUDIO_BASE_LABEL;id<=IDC_STUDIO_SHININESS;id++)
        ShowWindow(GetDlgItem(g_StudioProperties,id),m ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(g_StudioProperties,IDC_STUDIO_MATERIAL_HINT),m ? SW_HIDE : SW_SHOW);
    if (m)
    {
        snprintf(text,sizeof(text),"%.6g",m->intensity); SetDlgItemText(g_StudioProperties,IDC_STUDIO_INTENSITY,text);
        snprintf(text,sizeof(text),"%.6g",m->shininess); SetDlgItemText(g_StudioProperties,IDC_STUDIO_SHININESS,text);
        HWND image=GetDlgItem(g_StudioProperties,IDC_STUDIO_BASE_IMAGE);
        LRESULT row=m->image[0] ? SendMessage(image,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)m->image) : 0;
        if (row==CB_ERR) { row=SendMessage(image,CB_ADDSTRING,0,(LPARAM)m->image); }
        SendMessage(image,CB_SETCURSEL,row,0);
        InvalidateRect(GetDlgItem(g_StudioProperties,IDC_STUDIO_BASE_COLOR),NULL,TRUE);
        InvalidateRect(GetDlgItem(g_StudioProperties,IDC_STUDIO_SPEC_COLOR),NULL,TRUE);
    }
    g_StudioUpdating=FALSE;
}

static StudioInstance *RenderStudioObject(void)
{
    return g_StudioObject>=0 && (DWORD)g_StudioObject<g_StudioScene.count ? &g_StudioScene.objects[g_StudioObject] : NULL;
}

static void RenderStudioTransformPanel(void)
{
    StudioInstance *o=RenderStudioObject(); char text[64]; g_StudioUpdating=TRUE;
    for (int field=0;field<9;field++)
    {
        int id=IDC_STUDIO_POSITION_X+field; text[0]=0;
        if (o)
        {
            const double *values=field<3 ? o->transform.position : field<6 ? o->transform.rotation : o->transform.scale;
            snprintf(text,sizeof(text),"%.9g",values[field%3]);
        }
        SetDlgItemText(g_StudioTransform,id,text); EnableWindow(GetDlgItem(g_StudioTransform,id),o!=NULL);
    }
    CheckRadioButton(g_StudioTransform,IDC_STUDIO_MOVE,IDC_STUDIO_SCALE,IDC_STUDIO_MOVE+g_StudioTool);
    g_StudioTransformDirty=0; g_StudioUpdating=FALSE;
}

static void RenderStudioTool(int tool)
{
    StudioViewportSetTool(g_StudioViewport,tool); g_StudioTool=tool;
    CheckRadioButton(g_StudioTransform,IDC_STUDIO_MOVE,IDC_STUDIO_SCALE,IDC_STUDIO_MOVE+tool);
}

static void RenderStudioCommitTransform(const StudioTransform *previous)
{
    StudioInstance *o=RenderStudioObject(); const char *why="";
    if (!o) { return; }
    if (!StudioTransformValid(&o->transform) || !StudioSceneSave(&g_StudioScene,&why))
    {
        o->transform=*previous;
        MessageBox(g_Studio,why[0] ? why : "Enter finite position/rotation values within +/-1 billion and scale values from 0.0001 to 10000.","Transform",MB_ICONERROR);
    }
    else { SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,"Transform saved."); }
    StudioViewportSetScene(g_StudioViewport,&g_StudioScene,FALSE); RenderStudioTransformPanel();
}

static INT_PTR CALLBACK RenderStudioTransformProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    if (message==WM_INITDIALOG)
    {
        for (int id=IDC_STUDIO_POSITION_X;id<=IDC_STUDIO_SCALE_Z;id++) { SendDlgItemMessage(hwnd,id,EM_LIMITTEXT,48,0); }
        return TRUE;
    }
    if (message==WM_COMMAND && !g_StudioUpdating)
    {
        int id=LOWORD(wparam),code=HIWORD(wparam),field=id-IDC_STUDIO_POSITION_X;
        if (id>=IDC_STUDIO_MOVE && id<=IDC_STUDIO_SCALE && code==BN_CLICKED)
        { RenderStudioTool(id-IDC_STUDIO_MOVE); SetFocus(g_StudioViewport); return TRUE; }
        StudioInstance *o=RenderStudioObject();
        if (o && field>=0 && field<9)
        {
            if (code==EN_CHANGE) { g_StudioTransformDirty|=1u<<field; return TRUE; }
            if (code==EN_KILLFOCUS && (g_StudioTransformDirty&(1u<<field)))
            {
                char text[64],*end; GetDlgItemText(hwnd,id,text,sizeof(text)); double value=strtod(text,&end); BOOL parsed=end!=text;
                while (*end==' ' || *end=='\t') { end++; }
                if (!parsed || *end || !isfinite(value))
                { RenderStudioTransformPanel(); SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,"Enter a valid transform value."); return TRUE; }
                StudioTransform previous=o->transform;
                double *values=field<3 ? o->transform.position : field<6 ? o->transform.rotation : o->transform.scale;
                values[field%3]=value;
                RenderStudioCommitTransform(&previous); return TRUE;
            }
        }
    }
    return FALSE;
}

static void RenderStudioSaveScene(void)
{
    const char *why="";
    /* End text editing first so File > Save includes the current field. */
    StudioViewportCommitTransform(g_StudioViewport);
    SetFocus(g_StudioViewport);
    if (!g_StudioScene.filename[0]) { return; }
    if (!StudioSceneSave(&g_StudioScene,&why)) { MessageBox(g_Studio,why,"Save Scene",MB_ICONERROR); }
    else { SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,"Scene saved."); }
}

static void RenderStudioSelect(int object, int material)
{
    g_StudioObject=object>=0 && (DWORD)object<g_StudioScene.count ? object : -1;
    g_StudioMaterial=-1; HWND list=GetDlgItem(g_Studio,IDC_STUDIO_MATERIALS);
    g_StudioUpdating=TRUE; SendMessage(list,WM_SETREDRAW,FALSE,0); SendMessage(list,LB_RESETCONTENT,0,0);
    int widest=0; HDC dc=GetDC(list); HFONT font=(HFONT)SendMessage(list,WM_GETFONT,0,0),old=dc ? SelectObject(dc,font) : NULL;
    if (g_StudioObject>=0)
    {
        StudioInstance *o=&g_StudioScene.objects[g_StudioObject];
        for (DWORD i=0;i<o->materialcount;i++)
        {
            SendMessage(list,LB_ADDSTRING,0,(LPARAM)o->materials[i].name);
            SIZE size; if (dc && GetTextExtentPoint32(dc,o->materials[i].name,(int)strlen(o->materials[i].name),&size)) { widest=max(widest,size.cx+8); }
        }
        g_StudioMaterial=material>=0 && (DWORD)material<o->materialcount ? material : o->materialcount ? 0 : -1;
    }
    if (dc) { if (old) { SelectObject(dc,old); } ReleaseDC(list,dc); }
    SendMessage(list,LB_SETHORIZONTALEXTENT,widest,0); SendMessage(list,LB_SETCURSEL,g_StudioMaterial,0);
    SendMessage(list,WM_SETREDRAW,TRUE,0); InvalidateRect(list,NULL,TRUE);
    HWND tree=GetDlgItem(g_Studio,IDC_STUDIO_OUTLINER);
    HTREEITEM root=TreeView_GetRoot(tree),item=TreeView_GetChild(tree,root),choice=root;
    for (int i=0;item;item=TreeView_GetNextSibling(tree,item),i++) if (i==g_StudioObject) { choice=item; break; }
    TreeView_SelectItem(tree,choice); g_StudioUpdating=FALSE;
    StudioViewportSelect(g_StudioViewport,g_StudioObject); RenderStudioProperties(); RenderStudioTransformPanel();
}

static void RenderStudioOutliner(void)
{
    HWND tree=GetDlgItem(g_Studio,IDC_STUDIO_OUTLINER); TVINSERTSTRUCT insert={0}; char label[MAX_PATH+48];
    g_StudioUpdating=TRUE; TreeView_DeleteAllItems(tree);
    if (g_StudioScene.filename[0])
    {
        insert.hParent=TVI_ROOT; insert.hInsertAfter=TVI_LAST; insert.item.mask=TVIF_TEXT | TVIF_PARAM;
        insert.item.pszText=g_StudioScene.filename; insert.item.lParam=-1;
        HTREEITEM root=TreeView_InsertItem(tree,&insert); insert.hParent=root;
        for (DWORD i=0;i<g_StudioScene.count;i++)
        {
            snprintf(label,sizeof(label),"%s (%lu)%s",g_StudioScene.objects[i].model,(unsigned long)i+1,g_StudioScene.objects[i].asset ? "" : " - unavailable");
            insert.item.pszText=label; insert.item.lParam=i; TreeView_InsertItem(tree,&insert);
        }
        TreeView_Expand(tree,root,TVE_EXPAND);
    }
    g_StudioUpdating=FALSE; RenderStudioSelect(g_StudioObject,g_StudioMaterial);
}

static BOOL RenderStudioLoadScene(const char *filename)
{
    const char *why="";
    if (g_StudioLoading) { return FALSE; } g_StudioLoading=TRUE;
    StudioViewportCancelTransform(g_StudioViewport);
    if (!StudioSceneLoad(g_StudioProject,filename,&g_StudioScene,&why))
    {
        SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,why);
        MessageBox(g_Studio,why,"Open Scene",MB_ICONERROR);
        LRESULT row=SendDlgItemMessage(g_Studio,IDC_STUDIO_SCENE,LB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)g_StudioScene.filename);
        SendDlgItemMessage(g_Studio,IDC_STUDIO_SCENE,LB_SETCURSEL,row,0); g_StudioLoading=FALSE; return FALSE;
    }
    g_StudioGeneration++;
    g_StudioObject=g_StudioScene.count ? 0 : -1; g_StudioMaterial=-1;
    StudioViewportRefreshImages(g_StudioViewport); StudioViewportSetScene(g_StudioViewport,&g_StudioScene,TRUE);
    RenderStudioOutliner(); SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,why[0] ? why : g_StudioNavigation); g_StudioLoading=FALSE; return TRUE;
}

static void RenderStudioCommitMaterial(StudioMaterial *material, const StudioMaterial *previous)
{
    const char *why="";
    if (!StudioMaterialValid(material) || !StudioSceneSave(&g_StudioScene,&why))
    {
        *material=*previous;
        MessageBox(g_Studio,why[0] ? why : "Enter a finite intensity from 0 to 1 and shininess from 1 to 128.","Material",MB_ICONERROR);
    }
    else { SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,"Scene saved."); InvalidateRect(g_StudioViewport,NULL,FALSE); }
    RenderStudioProperties();
}

static INT_PTR CALLBACK RenderStudioPropertiesProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    StudioMaterial *m=RenderStudioMaterial();
    switch (message)
    {
    case WM_INITDIALOG:
        SendDlgItemMessage(hwnd,IDC_STUDIO_INTENSITY,EM_LIMITTEXT,32,0);
        SendDlgItemMessage(hwnd,IDC_STUDIO_SHININESS,EM_LIMITTEXT,32,0); return TRUE;
    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT *draw=(const DRAWITEMSTRUCT *)lparam;
        if (!m || (draw->CtlID!=IDC_STUDIO_BASE_COLOR && draw->CtlID!=IDC_STUDIO_SPEC_COLOR)) { break; }
        const float *color=draw->CtlID==IDC_STUDIO_BASE_COLOR ? m->base : m->specular;
        HBRUSH brush=CreateSolidBrush(RGB((int)(color[0]*255+.5f),(int)(color[1]*255+.5f),(int)(color[2]*255+.5f)));
        FillRect(draw->hDC,&draw->rcItem,brush); DeleteObject(brush); RECT rect=draw->rcItem;
        DrawEdge(draw->hDC,&rect,(draw->itemState&ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED,BF_RECT);
        if (draw->itemState&ODS_FOCUS) { InflateRect(&rect,-3,-3); DrawFocusRect(draw->hDC,&rect); } return TRUE;
    }
    case WM_COMMAND:
    {
        int id=LOWORD(wparam),code=HIWORD(wparam); if (g_StudioUpdating || !m) { break; }
        StudioMaterial previous=*m;
        if ((id==IDC_STUDIO_BASE_COLOR || id==IDC_STUDIO_SPEC_COLOR) && code==BN_CLICKED)
        {
            static COLORREF custom[16]; float *color=id==IDC_STUDIO_BASE_COLOR ? m->base : m->specular;
            unsigned generation=g_StudioGeneration;
            CHOOSECOLOR choice={0}; choice.lStructSize=sizeof(choice); choice.hwndOwner=g_Studio;
            choice.rgbResult=RGB((int)(color[0]*255+.5f),(int)(color[1]*255+.5f),(int)(color[2]*255+.5f));
            choice.lpCustColors=custom; choice.Flags=CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&choice) && generation==g_StudioGeneration && g_Studio)
            { color[0]=GetRValue(choice.rgbResult)/255.f; color[1]=GetGValue(choice.rgbResult)/255.f; color[2]=GetBValue(choice.rgbResult)/255.f; RenderStudioCommitMaterial(m,&previous); }
            return TRUE;
        }
        if (id==IDC_STUDIO_BASE_IMAGE && code==CBN_SELCHANGE)
        {
            LRESULT row=SendDlgItemMessage(hwnd,id,CB_GETCURSEL,0,0);
            if (row==0) { m->image[0]=0; }
            else if (row>0 && SendDlgItemMessage(hwnd,id,CB_GETLBTEXTLEN,row,0)<MAX_PATH)
                SendDlgItemMessage(hwnd,id,CB_GETLBTEXT,row,(LPARAM)m->image);
            RenderStudioCommitMaterial(m,&previous); return TRUE;
        }
        if ((id==IDC_STUDIO_INTENSITY || id==IDC_STUDIO_SHININESS) && code==EN_KILLFOCUS)
        {
            char text[64],*end; GetDlgItemText(hwnd,id,text,sizeof(text)); double value=strtod(text,&end); BOOL parsed=end!=text;
            while (*end==' ' || *end=='\t') { end++; }
            if (!parsed || *end || !isfinite(value))
            { RenderStudioProperties(); SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,"Enter a valid material value."); return TRUE; }
            if (id==IDC_STUDIO_INTENSITY) { m->intensity=(float)value; } else { m->shininess=(float)value; }
            if (memcmp(m,&previous,sizeof(previous))) { RenderStudioCommitMaterial(m,&previous); }
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

static void RenderStudioImageChoices(const TexThumb *images,DWORD count)
{
    HWND combo=GetDlgItem(g_StudioProperties,IDC_STUDIO_BASE_IMAGE);
    g_StudioUpdating=TRUE; SendMessage(combo,CB_RESETCONTENT,0,0); SendMessage(combo,CB_ADDSTRING,0,(LPARAM)"None");
    for (DWORD i=0;i<count;i++) { SendMessage(combo,CB_ADDSTRING,0,(LPARAM)images[i].label); }
    SendMessage(combo,CB_SETDROPPEDWIDTH,360,0); g_StudioUpdating=FALSE; RenderStudioProperties();
}

static void RenderStudioDropModel(const char *filename,POINT point)
{
    const char *why=""; double position[3];
    if (WindowFromPoint(point)!=g_StudioViewport || !StudioViewportDropPoint(g_StudioViewport,point,position)) { return; }
    if (!g_StudioScene.filename[0]) { RenderStudioNewScene(g_Studio); if (!g_StudioScene.filename[0]) { return; } }
    if (!StudioSceneAddModel(&g_StudioScene,filename,position,&why)) { MessageBox(g_Studio,why,"Add Model",MB_ICONERROR); return; }
    StudioInstance *o=&g_StudioScene.objects[g_StudioScene.count-1];
    /* Drop the model's base at the ground-plane hit, regardless of its origin. */
    o->transform.position[0]-=(o->asset->lower[0]+o->asset->upper[0])*0.5;
    o->transform.position[1]-=o->asset->lower[1]; o->transform.position[2]-=(o->asset->lower[2]+o->asset->upper[2])*0.5;
    if (!StudioSceneSave(&g_StudioScene,&why))
    { StudioSceneRemove(&g_StudioScene,g_StudioScene.count-1); MessageBox(g_Studio,why,"Add Model",MB_ICONERROR); return; }
    g_StudioObject=(int)g_StudioScene.count-1; g_StudioMaterial=0;
    StudioViewportSetScene(g_StudioViewport,&g_StudioScene,g_StudioScene.count==1); RenderStudioOutliner();
    SetDlgItemText(g_Studio,IDC_STUDIO_STATUS,"Model instance added. Scene saved."); SetFocus(g_StudioViewport);
}

static LRESULT CALLBACK RenderStudioModelDragProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR subclass,DWORD_PTR unused)
{
    if (message==WM_LBUTTONDOWN)
    {
        LRESULT result=DefSubclassProc(hwnd,message,wparam,lparam);
        LRESULT hit=SendMessage(hwnd,LB_ITEMFROMPOINT,0,lparam);
        LRESULT length=SendMessage(hwnd,LB_GETTEXTLEN,LOWORD(hit),0);
        if (!HIWORD(hit) && length>0 && length<MAX_PATH)
        {
            g_StudioDragPress=(POINT){GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
            SendMessage(hwnd,LB_GETTEXT,LOWORD(hit),(LPARAM)g_StudioDragModel); g_StudioDragArmed=TRUE; g_StudioDragging=FALSE;
            /* The native list box normally captured during DefSubclassProc.
             * Capturing it again sends WM_CAPTURECHANGED back to this callback,
             * which cancels the drag we just armed. Keep its existing capture. */
            if (GetCapture()!=hwnd) { SetCapture(hwnd); }
        }
        return result;
    }
    if (message==WM_MOUSEMOVE && g_StudioDragArmed)
    {
        POINT point={GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        if (abs(point.x-g_StudioDragPress.x)>GetSystemMetrics(SM_CXDRAG) || abs(point.y-g_StudioDragPress.y)>GetSystemMetrics(SM_CYDRAG)) { g_StudioDragging=TRUE; }
        if (g_StudioDragging)
        { ClientToScreen(hwnd,&point); SetCursor(LoadCursor(NULL,WindowFromPoint(point)==g_StudioViewport ? IDC_CROSS : IDC_NO)); return 0; }
    }
    if (message==WM_LBUTTONUP && g_StudioDragArmed)
    {
        BOOL drop=g_StudioDragging; char name[MAX_PATH]; POINT point={GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        lstrcpyn(name,g_StudioDragModel,sizeof(name)); ClientToScreen(hwnd,&point);
        g_StudioDragArmed=g_StudioDragging=FALSE;
        LRESULT result=DefSubclassProc(hwnd,message,wparam,lparam);
        if (GetCapture()==hwnd) { ReleaseCapture(); } SetCursor(LoadCursor(NULL,IDC_ARROW));
        if (drop) { RenderStudioDropModel(name,point); } return result;
    }
    if (message==WM_CANCELMODE || message==WM_CAPTURECHANGED || (message==WM_KEYDOWN && wparam==VK_ESCAPE))
    {
        BOOL armed=g_StudioDragArmed; g_StudioDragArmed=g_StudioDragging=FALSE;
        if (GetCapture()==hwnd) { ReleaseCapture(); }
        if (message==WM_KEYDOWN && armed) { return 0; }
    }
    if (message==WM_NCDESTROY) { RemoveWindowSubclass(hwnd,RenderStudioModelDragProc,subclass); }
    return DefSubclassProc(hwnd,message,wparam,lparam);
}

static void RenderStudioRefreshScenes(const char *select)
{
    StudioSceneEntry *entries = NULL; DWORD count = 0; const char *why = "";
    if (StudioSceneList(g_StudioProject, &entries, &count, &why))
        RenderStudioFillList(GetDlgItem(g_Studio, IDC_STUDIO_SCENE), entries, count, select, &why);
    SetDlgItemText(g_Studio, IDC_STUDIO_STATUS, why[0] ? why : g_StudioNavigation);
    if (!why[0] && count && (select || (!g_StudioScene.filename[0] && !g_StudioTriedInitialScene)))
    {
        g_StudioTriedInitialScene=TRUE;
        const char *filename=select ? select : entries[0].filename;
        LRESULT row=SendDlgItemMessage(g_Studio,IDC_STUDIO_SCENE,LB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)filename);
        SendDlgItemMessage(g_Studio,IDC_STUDIO_SCENE,LB_SETCURSEL,row,0); RenderStudioLoadScene(filename);
    }
    free(entries);
}

static void RenderStudioRefreshAssets(void)
{
    StudioFileEntry *models = NULL; TexThumb *images = NULL; unsigned char *pixels = NULL;
    DWORD count = 0; const char *why = "", *modelwhy = "";
    if (StudioFileList(g_StudioProject, "studio\\models", ".gltf", &models, &count, &modelwhy))
        RenderStudioFillList(GetDlgItem(g_Studio, IDC_STUDIO_MODELS), models, count, NULL, &modelwhy);
    free(models);
    if (StudioLoadImages(g_StudioProject, &images, &pixels, &count, &why))
    {
        RenderStudioImageChoices(images,count);
        BrowserSetImages(GetDlgItem(g_Studio, IDC_STUDIO_IMAGES), images, (int)count, pixels);
        StudioViewportRefreshImages(g_StudioViewport);
    }
    if (modelwhy[0] || why[0])
        SetDlgItemText(g_Studio, IDC_STUDIO_STATUS, modelwhy[0] ? modelwhy : why);
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
    RECT client, units = {8, 16, 144, 0}, panel = {0, 0, 168, 88}, properties={0,0,0,184}, transform={0,0,0,112};
    int margin, height, left, right, rightx, scenesize, imagesize, outline;
    GetClientRect(hwnd, &client); MapDialogRect(hwnd, &units); MapDialogRect(hwnd, &panel); MapDialogRect(hwnd,&properties); MapDialogRect(hwnd,&transform);
    margin = units.left; left = units.right; right = panel.right;
    height = max(0, client.bottom - units.top - margin * 3);
    rightx = client.right - margin - right;
    scenesize = min(panel.bottom, height / 3);
    imagesize = max(0, (height - scenesize - margin * 2) / 2);
    outline = max(0, (height - properties.bottom - transform.bottom - margin * 3) / 2);
    RenderStudioPanel(hwnd, IDC_STUDIO_SCENE_PANEL, IDC_STUDIO_SCENE, margin, margin,
        left, scenesize, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_IMAGES_PANEL, IDC_STUDIO_IMAGES, margin, margin * 2 + scenesize,
        left, imagesize, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_MODELS_PANEL, IDC_STUDIO_MODELS, margin, margin * 3 + scenesize + imagesize,
        left, height - scenesize - imagesize - margin * 2, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_OUTLINER_PANEL, IDC_STUDIO_OUTLINER, rightx, margin,
        right, outline, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_TRANSFORM_PANEL, IDC_STUDIO_TRANSFORM, rightx, margin * 2 + outline,
        right, transform.bottom, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_MATERIALS_PANEL, IDC_STUDIO_MATERIALS, rightx, margin * 3 + outline + transform.bottom,
        right, outline, margin, units.top);
    RenderStudioPanel(hwnd, IDC_STUDIO_PROPERTIES_PANEL, IDC_STUDIO_PROPERTIES, rightx, margin * 4 + outline * 2 + transform.bottom,
        right, height - outline * 2 - transform.bottom - margin * 3, margin, units.top);
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
    StudioViewportCancelTransform(g_StudioViewport);
    g_StudioGeneration++; g_StudioTriedInitialScene=FALSE; StudioSceneFree(&g_StudioScene); g_StudioObject=g_StudioMaterial=-1;
    StudioViewportSetScene(g_StudioViewport,&g_StudioScene,TRUE);
    SendDlgItemMessage(g_Studio, IDC_STUDIO_SCENE, LB_RESETCONTENT, 0, 0);
    BrowserSetImages(GetDlgItem(g_Studio, IDC_STUDIO_IMAGES), NULL, 0, NULL);
    SendDlgItemMessage(g_Studio, IDC_STUDIO_MODELS, LB_RESETCONTENT, 0, 0);
    RenderStudioOutliner();
    RenderStudioRefreshScenes(NULL);
    RenderStudioRefreshAssets();
    RenderStudioProperties();
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
            {IDC_STUDIO_PROPERTIES_PANEL, IDC_STUDIO_PROPERTIES}, {IDC_STUDIO_MATERIALS_PANEL,IDC_STUDIO_MATERIALS},
            {IDC_STUDIO_TRANSFORM_PANEL,IDC_STUDIO_TRANSFORM}};
        g_Studio = hwnd;
        for (size_t i = 0; i < sizeof(panels) / sizeof(*panels); i++)
            SetWindowSubclass(GetDlgItem(hwnd, panels[i][0]), RenderStudioGroupProc, 1, panels[i][1]);
        return TRUE;
    }
    case WM_SIZE: RenderStudioLayout(hwnd); return TRUE;
    case WM_ACTIVATE:
        if (LOWORD(wparam) != WA_INACTIVE && g_StudioProject[0] && !g_StudioLoading)
        { RenderStudioRefreshScenes(NULL); RenderStudioRefreshAssets(); }
        break; /* Let the dialog manager restore keyboard focus normally. */
    case WM_INITMENUPOPUP:
        EnableMenuItem((HMENU)wparam, ID_STUDIO_NEW_SCENE, MF_BYCOMMAND | (g_StudioProject[0] ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem((HMENU)wparam, ID_STUDIO_SAVE_SCENE, MF_BYCOMMAND | (g_StudioScene.filename[0] ? MF_ENABLED : MF_GRAYED));
        return TRUE;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = {0, 0, 640, 480};
        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
            GetMenu(hwnd) != NULL, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_STUDIO_SAVE_SCENE) { RenderStudioSaveScene(); return TRUE; }
        if (LOWORD(wparam) == ID_STUDIO_NEW_SCENE) { RenderStudioNewScene(hwnd); return TRUE; }
        if (LOWORD(wparam) == IDCANCEL) { DestroyWindow(hwnd); return TRUE; }
        if (LOWORD(wparam) == IDOK) { SetFocus(GetDlgItem(g_Studio,IDC_STUDIO_MATERIALS)); return TRUE; }
        if (LOWORD(wparam)==IDC_STUDIO_SCENE && HIWORD(wparam)==LBN_SELCHANGE)
        {
            char filename[MAX_PATH]; LRESULT row=SendDlgItemMessage(hwnd,IDC_STUDIO_SCENE,LB_GETCURSEL,0,0);
            if (row!=LB_ERR && SendDlgItemMessage(hwnd,IDC_STUDIO_SCENE,LB_GETTEXTLEN,row,0)<MAX_PATH)
            { SendDlgItemMessage(hwnd,IDC_STUDIO_SCENE,LB_GETTEXT,row,(LPARAM)filename); RenderStudioLoadScene(filename); }
            return TRUE;
        }
        if (LOWORD(wparam)==IDC_STUDIO_MATERIALS && HIWORD(wparam)==LBN_SELCHANGE && !g_StudioUpdating)
        { g_StudioMaterial=(int)SendDlgItemMessage(hwnd,IDC_STUDIO_MATERIALS,LB_GETCURSEL,0,0); RenderStudioProperties(); return TRUE; }
        break;
    case WM_NOTIFY:
        if (((NMHDR *)lparam)->idFrom==IDC_STUDIO_OUTLINER && ((NMHDR *)lparam)->code==TVN_SELCHANGED && !g_StudioUpdating)
        { RenderStudioSelect((int)((NMTREEVIEW *)lparam)->itemNew.lParam,-1); return TRUE; }
        break;
    case STUDIO_WM_TRANSFORM:
        if (wparam==1) { RenderStudioCommitTransform((const StudioTransform *)lparam); }
        else { RenderStudioTransformPanel(); StudioViewportSetScene(g_StudioViewport,&g_StudioScene,FALSE); }
        return TRUE;
    case STUDIO_WM_SELECT: RenderStudioSelect((int)(INT_PTR)wparam,(int)lparam); return TRUE;
    case WM_CLOSE: StudioViewportCancelTransform(g_StudioViewport); DestroyWindow(hwnd); return TRUE;
    case WM_NCDESTROY:
        g_StudioGeneration++; g_StudioTriedInitialScene=FALSE; StudioSceneFree(&g_StudioScene);
        g_Studio = g_StudioViewport = g_StudioProperties = g_StudioTransform = NULL; g_StudioTool=STUDIO_TRANSLATE; g_StudioProject[0] = '\0';
        g_StudioObject=g_StudioMaterial=-1; g_StudioDragArmed=g_StudioDragging=FALSE;
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
        DestroyWindow(GetDlgItem(g_Studio, IDC_STUDIO_IMAGES));
        if (!BrowserCreateImagePanel(g_Studio, instance, IDC_STUDIO_IMAGES))
        {
            DestroyWindow(g_Studio);
            *why = "Could not initialize Render Studio's image browser.";
            return FALSE;
        }
        DestroyWindow(GetDlgItem(g_Studio,IDC_STUDIO_PROPERTIES));
        g_StudioProperties=CreateDialog(instance,MAKEINTRESOURCE(IDD_STUDIO_PROPERTIES),g_Studio,RenderStudioPropertiesProc);
        if (!g_StudioProperties)
        { DestroyWindow(g_Studio); *why="Could not create the studio material properties panel."; return FALSE; }
        SetWindowLongPtr(g_StudioProperties,GWLP_ID,IDC_STUDIO_PROPERTIES); ShowWindow(g_StudioProperties,SW_SHOW);
        DestroyWindow(GetDlgItem(g_Studio,IDC_STUDIO_TRANSFORM));
        g_StudioTransform=CreateDialog(instance,MAKEINTRESOURCE(IDD_STUDIO_TRANSFORM),g_Studio,RenderStudioTransformProc);
        if (!g_StudioTransform)
        { DestroyWindow(g_Studio); *why="Could not create the Transform panel."; return FALSE; }
        SetWindowLongPtr(g_StudioTransform,GWLP_ID,IDC_STUDIO_TRANSFORM); ShowWindow(g_StudioTransform,SW_SHOW);
        SetWindowSubclass(GetDlgItem(g_Studio,IDC_STUDIO_MODELS),RenderStudioModelDragProc,2,0);
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
    RenderStudioRefreshAssets();
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
            || target == GetDlgItem(g_Studio, IDC_STUDIO_OUTLINER)
            || target == GetDlgItem(g_Studio, IDC_STUDIO_MATERIALS)))
        { SendMessage(target, WM_MOUSEWHEEL, message->wParam, message->lParam); return TRUE; }
    }
    if (!own) { return FALSE; }
    if (message->message==WM_KEYDOWN)
    {
        if (message->wParam==VK_ESCAPE && StudioViewportCancelTransform(g_StudioViewport)) { return TRUE; }
        if (message->wParam=='S' && (GetKeyState(VK_CONTROL)&0x8000)) { RenderStudioSaveScene(); return TRUE; }
        char kind[32]=""; GetClassName(GetFocus(),kind,sizeof(kind));
        if (!(GetKeyState(VK_CONTROL)&0x8000) && !(GetKeyState(VK_MENU)&0x8000)
            && lstrcmpi(kind,"Edit") && lstrcmpi(kind,"ComboBox"))
        {
            int tool=message->wParam=='W' ? STUDIO_TRANSLATE : message->wParam=='E' ? STUDIO_ROTATE : message->wParam=='R' ? STUDIO_SCALE : -1;
            if (tool>=0) { RenderStudioTool(tool); return TRUE; }
        }
    }
    if (message->message==WM_KEYDOWN && message->wParam==VK_ESCAPE && g_StudioDragArmed)
    { SendDlgItemMessage(g_Studio,IDC_STUDIO_MODELS,WM_KEYDOWN,VK_ESCAPE,0); return TRUE; }
    /* The main window remains usable; studio input never reaches its game
     * selection, transform or save accelerators. */
    if (!IsDialogMessage(g_Studio, message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
