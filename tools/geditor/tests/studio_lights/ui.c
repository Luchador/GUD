/* Production outliner, property editing and add-light callbacks. The native
 * controls/color dialog are stand-ins; this is not a Windows UI smoke test. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studioscene.h"
#include "resource.h"
typedef intptr_t HWND,LPARAM,LRESULT;
typedef uintptr_t WPARAM;
typedef unsigned UINT,COLORREF;
typedef void *HDC,*HFONT;
typedef struct { int cx,cy; } SIZE;
typedef struct TreeNode *HTREEITEM;
typedef struct { int mask; HTREEITEM hItem; char *pszText; LPARAM lParam; } TVITEM;
typedef struct { HTREEITEM hParent,hInsertAfter; TVITEM item; } TVINSERTSTRUCT;
typedef struct { size_t lStructSize; HWND hwndOwner; COLORREF rgbResult,*lpCustColors; unsigned Flags; } CHOOSECOLOR;
struct TreeNode { LPARAM id; char name[MAX_PATH+48]; } nodes[16];
static int nodecount,selectedrow,materialcount,viewportselection,errors,saved,invalidated;
static int fail_save,choose_cancel,choose_switch,new_cancel,scene_bounds;
static HTREEITEM treeselected;
static char text[1600][128]; static BOOL visible[1600];
static HWND g_Studio=1,g_StudioViewport=2,g_StudioProperties=3;
static StudioScene g_StudioScene;
static int g_StudioObject=-1,g_StudioMaterial=-1,g_StudioLight=-1;
static BOOL g_StudioUpdating;
static unsigned g_StudioGeneration,g_StudioLightDirty;
#define lstrcpy strcpy
#define max(a,b) ((a)>(b)?(a):(b))
#define RGB(r,g,b) ((r)|((g)<<8)|((b)<<16))
#define GetRValue(x) ((x)&255)
#define GetGValue(x) (((x)>>8)&255)
#define GetBValue(x) (((x)>>16)&255)
#define TVI_ROOT ((HTREEITEM)(intptr_t)-1)
#define TVI_LAST ((HTREEITEM)(intptr_t)-2)
enum { SW_HIDE=0,SW_SHOW,WM_SETREDRAW,WM_GETFONT,LB_RESETCONTENT,LB_ADDSTRING,LB_SETHORIZONTALEXTENT,
       LB_SETCURSEL,CB_FINDSTRINGEXACT,CB_ADDSTRING,CB_SETCURSEL,TVIF_TEXT,TVIF_PARAM,TVE_EXPAND,
       MB_ICONERROR,BN_CLICKED,EN_CHANGE,EN_KILLFOCUS,CC_FULLOPEN=64,CC_RGBINIT=128,CB_ERR=-1 };
static HWND GetDlgItem(HWND hwnd,int id) { return id; }
static void ShowWindow(HWND hwnd,int mode) { visible[hwnd]=mode==SW_SHOW; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) { invalidated++; }
static void SetDlgItemText(HWND hwnd,int id,const char *value) { snprintf(text[id],sizeof(text[id]),"%s",value); }
static void GetDlgItemText(HWND hwnd,int id,char *out,int size) { snprintf(out,size,"%s",text[id]); }
static HDC GetDC(HWND hwnd) { return NULL; }
static HFONT SelectObject(HDC dc,HFONT font) { return NULL; }
static void ReleaseDC(HWND hwnd,HDC dc) {}
static BOOL GetTextExtentPoint32(HDC dc,const char *value,int n,SIZE *size) { return FALSE; }
static LRESULT SendMessage(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    if (message==LB_RESETCONTENT) { materialcount=0; }
    if (message==LB_ADDSTRING) { return materialcount++; }
    if (message==LB_SETCURSEL) { selectedrow=(int)wparam; }
    return 0;
}
static HTREEITEM TreeView_GetRoot(HWND hwnd) { return nodecount ? nodes : NULL; }
static HTREEITEM TreeView_GetChild(HWND hwnd,HTREEITEM item) { return nodecount>1 ? nodes+1 : NULL; }
static HTREEITEM TreeView_GetNextSibling(HWND hwnd,HTREEITEM item) { return item+1<nodes+nodecount ? item+1 : NULL; }
static BOOL TreeView_GetItem(HWND hwnd,TVITEM *item) { item->lParam=item->hItem->id; return TRUE; }
static void TreeView_SelectItem(HWND hwnd,HTREEITEM item) { assert(g_StudioUpdating); treeselected=item; }
static void TreeView_DeleteAllItems(HWND hwnd) { nodecount=0; }
static HTREEITEM TreeView_InsertItem(HWND hwnd,TVINSERTSTRUCT *insert)
{
    assert(nodecount<16); HTREEITEM node=nodes+nodecount++; node->id=insert->item.lParam;
    snprintf(node->name,sizeof(node->name),"%s",insert->item.pszText); return node;
}
static void TreeView_Expand(HWND hwnd,HTREEITEM item,int how) {}
static void StudioViewportSelect(HWND hwnd,int selected) { viewportselection=selected; }
static void RenderStudioTransformPanel(void) {}
static void SetFocus(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *message,const char *title,int flags) { errors++; }
static BOOL ChooseColor(CHOOSECOLOR *choice)
{ if(choose_switch) g_StudioGeneration++; choice->rgbResult=RGB(17,85,204); return !choose_cancel; }
BOOL StudioSceneSave(const StudioScene *scene,const char **why)
{ if(fail_save) { *why="Test failure"; return FALSE; } saved++; return TRUE; }
static void RenderStudioNewScene(HWND hwnd) { if(!new_cancel) strcpy(g_StudioScene.filename,"New.rnd"); }
static BOOL StudioBounds(const StudioScene *scene,int selected,double lower[3],double upper[3])
{ for(int k=0;k<3;k++) { lower[k]=-2; upper[k]=2; } return scene_bounds; }
#include "ui.inc"
static void Edit(int id,const char *value)
{
    SetDlgItemText(g_StudioProperties,id,value);
    assert(RenderStudioLightCommand(g_StudioProperties,id,EN_CHANGE));
    assert(RenderStudioLightCommand(g_StudioProperties,id,EN_KILLFOCUS));
}
int main(void)
{
    new_cancel=1; RenderStudioAddLight(TRUE); assert(!g_StudioScene.filename[0] && !g_StudioScene.lights[0].enabled);
    new_cancel=0; fail_save=1; RenderStudioAddLight(TRUE); assert(!g_StudioScene.lights[0].enabled && errors==1);
    fail_save=0; scene_bounds=1; RenderStudioAddLight(FALSE);
    assert(g_StudioLight==1 && treeselected->id==-3 && nodecount==2 && !strcmp(treeselected->name,"Point Light 1"));
    assert(g_StudioScene.lights[1].position[1]==6 && g_StudioScene.lights[1].radius==16);
    assert(visible[IDC_STUDIO_LIGHT_POSITION_X] && !visible[IDC_STUDIO_LIGHT_DIRECTION_X]);
    assert(visible[IDC_STUDIO_LIGHT_RANGE] && !visible[IDC_STUDIO_LIGHT_OUTER] && !visible[IDC_STUDIO_MATERIAL_HINT]);
    assert(!strcmp(text[IDC_STUDIO_LIGHT_RANGE_LABEL],"Radius") && viewportselection==-1 && materialcount==0);
    Edit(IDC_STUDIO_LIGHT_RANGE,"24.5"); assert(g_StudioScene.lights[1].radius==24.5);
    Edit(IDC_STUDIO_LIGHT_POSITION_Z,"-12"); assert(g_StudioScene.lights[1].position[2]==-12);
    fail_save=1; Edit(IDC_STUDIO_LIGHT_INTENSITY,"3"); assert(g_StudioScene.lights[1].intensity==1 && errors==2);
    fail_save=0; Edit(IDC_STUDIO_LIGHT_RANGE,"0"); assert(g_StudioScene.lights[1].radius==24.5 && errors==3);
    Edit(IDC_STUDIO_LIGHT_POSITION_X,"NaN"); assert(g_StudioScene.lights[1].position[0]==0);
    RenderStudioLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(fabs(g_StudioScene.lights[1].color[2]-.8)<1e-6);
    StudioLight before=g_StudioScene.lights[1]; choose_cancel=1;
    RenderStudioLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(!memcmp(&before,&g_StudioScene.lights[1],sizeof(before))); choose_cancel=0;
    choose_switch=1; g_StudioScene.lights[1].color[0]=1;
    RenderStudioLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(g_StudioScene.lights[1].color[0]==1); choose_switch=0;
    RenderStudioAddLight(TRUE);
    assert(g_StudioLight==0 && treeselected->id==-2 && !strcmp(treeselected->name,"Spotlight"));
    assert(visible[IDC_STUDIO_LIGHT_DIRECTION_X] && visible[IDC_STUDIO_LIGHT_OUTER]);
    assert(!strcmp(text[IDC_STUDIO_LIGHT_RANGE_LABEL],"Inner angle (deg)"));
    Edit(IDC_STUDIO_LIGHT_DIRECTION_X,"1"); Edit(IDC_STUDIO_LIGHT_DIRECTION_Y,"0");
    assert(g_StudioScene.lights[0].direction[0]==1 && g_StudioScene.lights[0].direction[1]==0);
    Edit(IDC_STUDIO_LIGHT_OUTER,"50"); Edit(IDC_STUDIO_LIGHT_RANGE,"40");
    assert(g_StudioScene.lights[0].inner==40 && g_StudioScene.lights[0].outer==50);
    RenderStudioAddLight(FALSE); assert(g_StudioLight==2 && treeselected->id==-4 && nodecount==4);
    int count=nodecount,attempts=saved; RenderStudioAddLight(FALSE); RenderStudioAddLight(TRUE);
    assert(nodecount==count && saved==attempts && g_StudioLight==2);
    StudioMaterial material={0}; strcpy(material.name,"Material");
    StudioInstance model={0}; strcpy(model.model,"Fixture.gltf"); model.materials=&material; model.materialcount=1;
    g_StudioScene.objects=&model; g_StudioScene.count=1; RenderStudioOutliner();
    assert(nodecount==5 && treeselected->id==-4); /* Light identity survives inserted model rows. */
    RenderStudioSelect(0,0); assert(g_StudioLight==-1 && viewportselection==0 && treeselected->id==0 && materialcount==1);
    assert(visible[IDC_STUDIO_BASE_COLOR] && !visible[IDC_STUDIO_LIGHT_COLOR]);
    RenderStudioSelect(-2,-1); assert(g_StudioObject==-1 && g_StudioMaterial==-1 && materialcount==0 && viewportselection==-1);
    RenderStudioSelect(-1,-1); assert(g_StudioLight==-1 && visible[IDC_STUDIO_MATERIAL_HINT]);
    g_StudioScene.lights[2].enabled=FALSE; RenderStudioSelect(-4,-1); assert(g_StudioLight==-1 && treeselected==nodes);
    assert(invalidated>0);
    puts("PASS: add/cancel/failure, outliner identities, material/light isolation, property visibility, numeric/color edits and rollback.");
    return 0;
}
