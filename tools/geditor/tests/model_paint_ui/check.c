#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "edittool.h"
#include "resource.h"
typedef void *HWND, *HBITMAP;
typedef unsigned UINT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef struct { int left,top,right,bottom; } RECT;
typedef struct { UINT cbSize,fMask; int nMin,nMax; UINT nPage; int nPos; } SCROLLINFO;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
#define TOOLTOOLBAR_HEIGHT 40
#define WM_APP 0x8000
#define COLORPICKER_MODEL_HEIGHT 204
enum { TOOLTOOLBAR_MENU_COUNT=3, GWLP_USERDATA=1, WM_KEYDOWN, BM_CLICK, VK_CONTROL,
       VK_MENU, VK_SHIFT, VK_RETURN, VK_DOWN, VK_ESCAPE, WM_CANCELMODE,
       VK_NUMPAD1=0x61, VK_NUMPAD5=0x65 };
enum { SIF_RANGE=1,SIF_PAGE=2,SIF_POS=4,SIF_DISABLENOSCROLL=8,SB_VERT=1,
       SWP_NOZORDER=1,SWP_NOACTIVATE=2,SWP_NOCOPYBITS=4,SWP_NOREDRAW=8,
       RDW_INVALIDATE=1,RDW_ERASE=2,RDW_FRAME=4,RDW_ALLCHILDREN=8,
       LB_SETITEMHEIGHT=1,CB_SETDROPPEDWIDTH=2,IDCANCEL=2 };
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define ZeroMemory(p,n) memset(p,0,n)
#define lstrcmpi strcasecmp
#include "types.inc"
static ToolToolbarState toolbar;
static HWND g_ModelEditor=(HWND)1, g_ModelPaintToolbar=(HWND)2,
    g_ModelViewport=(HWND)2000, g_ModelColorPicker=(HWND)2001;
static const int g_ModelCombos[]={IDC_MODEL_CHARACTERS,IDC_MODEL_ITEMS,IDC_MODEL_PROPS};
static RECT positions[4000], client;
static double scalex,scaley;
static int toggles,control,shift,alt;
static EditorTool lasttool;
static int snaptoggles;
static int undos,redos,cancels;
static BOOL g_ModelSampling;
static void ModelEditorUndo(BOOL redo) { if(redo) redos++; else undos++; }
static const char *focusclass="GEditorViewport";
static BOOL IsChild(HWND parent, HWND child)
{ return parent==(HWND)1 ? (uintptr_t)child>1 && (uintptr_t)child<4000 : parent==(HWND)2 && child==(HWND)3; }
static HWND GetParent(HWND hwnd) { return g_ModelEditor; }
static intptr_t GetWindowLongPtr(HWND hwnd,int index) { return (intptr_t)&toolbar; }
static int GetKeyState(int key) { return (key==VK_CONTROL?control:key==VK_SHIFT?shift:alt)?0x8000:0; }
static void GetClassName(HWND hwnd,char *name,int size) { snprintf(name,size,"%s",focusclass); }
static int GetDlgCtrlID(HWND hwnd) { return 0; }
static BOOL IsDialogMessage(HWND hwnd,MSG *message) { return FALSE; }
static void SendMessage(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    if(message==WM_CANCELMODE) { assert(hwnd==g_ModelViewport);g_ModelSampling=FALSE;cancels++;return; }
    if(message==EDITTOOL_WM_TOGGLE_VERTEX_SNAP) { snaptoggles++; return; }
    assert(message==EDITTOOL_WM_SELECT && wparam<EDITOR_TOOL_COUNT); lasttool=(EditorTool)wparam; toggles++;
}
static void GetClientRect(HWND hwnd,RECT *rect)
{
    if(hwnd==g_ModelEditor) *rect=client;
    else { *rect=positions[(uintptr_t)hwnd];rect->right-=rect->left+17;rect->bottom-=rect->top;rect->left=rect->top=0; }
}
static void MapDialogRect(HWND hwnd,RECT *r)
{ r->left=lround(r->left*scalex);r->right=lround(r->right*scalex);r->top=lround(r->top*scaley);r->bottom=lround(r->bottom*scaley); }
static HWND GetDlgItem(HWND hwnd,int id) { return (HWND)(uintptr_t)id; }
static void SetRect(RECT *r,int x,int y,int right,int bottom) { *r=(RECT){x,y,right,bottom}; }
static void MoveWindow(HWND hwnd,int x,int y,int w,int h,BOOL redraw)
{ assert(w>=0 && h>=0);positions[(uintptr_t)hwnd]=(RECT){x,y,x+w,y+h}; }
static void SetWindowPos(HWND hwnd,HWND after,int x,int y,int w,int h,UINT flags)
{ assert(flags & SWP_NOCOPYBITS);MoveWindow(hwnd,x,y,w,h,FALSE); }
static void SetScrollInfo(HWND hwnd,int bar,SCROLLINFO *info,BOOL redraw) {}
static void InvalidateRect(HWND hwnd,const RECT *r,BOOL erase) {}
static void RedrawWindow(HWND hwnd,const RECT *r,void *region,UINT flags) { assert(flags & RDW_ALLCHILDREN); }
static void SendDlgItemMessage(HWND hwnd,int id,UINT msg,WPARAM wparam,LPARAM lparam) {}
#include "logic.inc"
static BOOL Inside(RECT inner,RECT outer)
{ return inner.left>=outer.left && inner.top>=outer.top && inner.right<=outer.right && inner.bottom<=outer.bottom; }
int main(void)
{
    ColorPickerState picker={0};picker.model=TRUE;picker.pickbutton=(HWND)3000;
    for(int i=0;i<4;i++) picker.channels[i]=(HWND)(uintptr_t)(3001+i);
    for(int scale=0;scale<3;scale++) for(int height=660;height<=900;height+=20)
    {
        scalex=1.25+scale*.25;scaley=1.5+scale*.25;
        client=(RECT){0,0,lround(660*scalex),lround(height*scaley)};
        ModelEditorLayout(g_ModelEditor);
        assert(Inside(positions[IDC_MODEL_UV],client));
        assert(positions[IDC_MODEL_ADD].right<positions[IDC_MODEL_UV].left);
        assert(positions[IDC_MODEL_UV].right<positions[(uintptr_t)g_ModelPaintToolbar].left);
        RECT materials=positions[IDC_MODEL_MATERIALS], colors=positions[IDC_MODEL_COLORS], faces=positions[IDC_MODEL_PROPERTIES];
        assert(materials.bottom<colors.top && colors.bottom<faces.top);
        assert(Inside(materials,client) && Inside(colors,client) && Inside(faces,client));
        for (int axis=0;axis<2;axis++)
        {
            RECT wrap=positions[axis?IDC_MODEL_WRAP_V:IDC_MODEL_WRAP_U];
            wrap.bottom=wrap.top+lround(18*scaley); /* Closed combo, excluding popup. */
            assert(Inside(wrap,faces));
            assert(wrap.top>positions[IDC_MODEL_CURRENT].bottom);
            assert(wrap.bottom<positions[IDC_MODEL_APPLY].top);
        }
        assert(positions[IDC_MODEL_WRAP_U].right<positions[IDC_MODEL_WRAP_V].left);
        assert(Inside(positions[IDC_MODEL_MATERIAL_LIST],materials));
        assert(Inside(positions[IDC_MODEL_UNTEXTURED],materials));
        assert(Inside(positions[IDC_MODEL_SEPARATE_LODS],materials));
        assert(Inside(positions[IDC_MODEL_SHARED],materials));
        assert(positions[IDC_MODEL_MATERIAL_LIST].bottom<positions[IDC_MODEL_SEPARATE_LODS].top);
        assert(positions[IDC_MODEL_SEPARATE_LODS].bottom<positions[IDC_MODEL_UNTEXTURED].top);
        assert(positions[IDC_MODEL_UNTEXTURED].bottom<positions[IDC_MODEL_SHARED].top);
        assert(positions[IDC_MODEL_SHARED].bottom<=positions[IDC_MODEL_HINT].top);
        RECT lod=positions[IDC_MODEL_LODS];lod.bottom=lod.top+lround(18*scaley);
        assert(Inside(lod,faces) && lod.top>positions[IDC_MODEL_APPLY].bottom);
        assert(positions[IDC_MODEL_MATERIAL_LIST].bottom-positions[IDC_MODEL_MATERIAL_LIST].top>=44*scaley);
        assert(Inside(positions[(uintptr_t)g_ModelColorPicker],colors));
        ColorPickerLayout(g_ModelColorPicker,&picker);
        RECT bounds;GetClientRect(g_ModelColorPicker,&bounds);
        assert(Inside(picker.square,bounds) && Inside(picker.huebar,bounds) && Inside(picker.swatch,bounds));
        for(int i=0;i<4;i++)
        {
            RECT field=positions[3001+i];
            assert(Inside(field,bounds) && field.right-field.left>=40 && field.left>picker.huebar.right);
            if(i) assert(field.top>=positions[3000+i].bottom);
            assert(field.bottom<picker.swatch.top);
        }
        assert(picker.scroll==0);
    }
    toolbar.paintonly=TRUE;
    MSG message={g_ModelViewport,WM_KEYDOWN,'5',0};
    assert(ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && toggles==1);
    message.lParam=1L<<30;assert(ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && toggles==1);
    message.lParam=0;message.wParam=VK_NUMPAD5;
    assert(ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && toggles==2);
    for(int i=0;i<3;i++)
    {
        focusclass=i==0?"Edit":i==1?"ComboBox":"ComboLBox";
        assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && toggles==2);
    }
    focusclass="GEditorViewport";message.wParam='5';
    control=1;assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));control=0;
    alt=1;assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));alt=0;
    shift=1;assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));shift=0;
    message.wParam='1';assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));
    message.wParam='V';assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));
    message.hwnd=(HWND)5000;message.wParam='5';assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));
    assert(toggles==2);
    toolbar.paintonly=FALSE; message.hwnd=g_ModelViewport;
    for(int i=0;i<5;i++) {
        message.wParam='1'+i;
        assert(ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && lasttool==(EditorTool)i);
        message.wParam=VK_NUMPAD1+i;
        assert(ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && lasttool==(EditorTool)i);
    }
    assert(EDITOR_TOOL_ROOM_SELECT==3 && EDITOR_TOOL_VERTEX_PAINT==4);
    for(int i=0;i<3;i++) {
        focusclass=i==0 ? "Edit" : i==1 ? "ComboBox" : "ComboLBox";
        message.wParam='4'; assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));
    }
    focusclass="GEditorViewport";
    message.wParam='V'; toolbar.tool=EDITOR_TOOL_VERTEX_SELECT;
    assert(ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && snaptoggles==1);
    message.lParam=(LPARAM)1<<30;
    assert(ToolToolbarHandleMessage(g_ModelPaintToolbar,&message) && snaptoggles==1);
    message.lParam=0;
    for(int i=0;i<3;i++) {
        focusclass=i==0 ? "Edit" : i==1 ? "ComboBox" : "ComboLBox";
        assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));
    }
    focusclass="GEditorViewport";
    control=1;assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));control=0;
    alt=1;assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));alt=0;
    shift=1;assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));shift=0;
    for(int i=EDITOR_TOOL_EDGE_SELECT;i<EDITOR_TOOL_COUNT;i++) {
        toolbar.tool=(EditorTool)i;
        assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));
    }
    assert(snaptoggles==1);
    message.wParam='6'; assert(!ToolToolbarHandleMessage(g_ModelPaintToolbar,&message));
    message.hwnd=g_ModelViewport;message.wParam=VK_ESCAPE;g_ModelSampling=TRUE;
    assert(ModelEditorPaintKey(&message) && !g_ModelSampling && cancels==1);
    assert(!ModelEditorPaintKey(&message)); /* Normal dialog Escape remains available. */
    message.wParam='Z';assert(!ModelEditorPaintKey(&message));
    control=1;assert(ModelEditorPaintKey(&message) && undos==1);
    shift=1;assert(ModelEditorPaintKey(&message) && redos==1);shift=0;
    message.wParam='Y';assert(ModelEditorPaintKey(&message) && redos==2);
    focusclass="Edit";assert(!ModelEditorPaintKey(&message) && redos==2);
    focusclass="GEditorViewport";alt=1;assert(!ModelEditorPaintKey(&message));
    puts("PASS model panels/visible RGBA across font scales and window sizes; paint toggle, text focus, modifiers, sampling Escape and undo/redo shortcuts.");
    return 0;
}
