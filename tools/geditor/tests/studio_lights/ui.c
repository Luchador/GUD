/* Production outliner, property editing and add-light callbacks. The native
 * controls/color dialog are stand-ins; this is not a Windows UI smoke test. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studiogizmo.h"
#include "resource.h"
#include "editorpath.h"
typedef intptr_t HWND,LPARAM,LRESULT,INT_PTR;
#define CALLBACK
#define LOWORD(n) ((unsigned)(n)&65535)
#define HIWORD(n) (((unsigned)(n)>>16)&65535)
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
static char text[1600][128]; static BOOL visible[1600],enabled[1600];
static HWND g_Studio=1,g_StudioViewport=2,g_StudioProperties=3,g_StudioTransform=4;
static StudioScene g_StudioScene;
static int g_StudioObject=-1,g_StudioMaterial=-1,g_StudioLight=-1,g_StudioGlobal=-1;
static StudioMaterial *g_StudioMetalnessMaterial;
static float g_StudioMetalnessBefore;
static void RenderStudioFinishMetalness(void);
static BOOL g_StudioUpdating;
static unsigned g_StudioGeneration,g_StudioLightDirty,g_StudioTransformDirty;
static int g_StudioTool=STUDIO_SCALE;
char *lstrcpyn(char *out,const char *in,int capacity) { snprintf(out,capacity,"%s",in); return out; }
#define lstrcpy strcpy
#define min(a,b) ((a)<(b)?(a):(b))
#define lstrcmpi strcmp
#define max(a,b) ((a)>(b)?(a):(b))
#define RGB(r,g,b) ((r)|((g)<<8)|((b)<<16))
#define GetRValue(x) ((x)&255)
#define GetGValue(x) (((x)>>8)&255)
#define GetBValue(x) (((x)>>16)&255)
#define TVI_ROOT ((HTREEITEM)(intptr_t)-1)
#define TVI_LAST ((HTREEITEM)(intptr_t)-2)
enum { SW_HIDE=0,SW_SHOW,WM_SETREDRAW,WM_GETFONT,LB_RESETCONTENT,LB_ADDSTRING,LB_SETHORIZONTALEXTENT,
       TBM_GETPOS,TBM_SETPOS,TB_THUMBTRACK,TB_THUMBPOSITION,TB_ENDTRACK,TB_LINEUP,CBN_SELCHANGE,CB_RESETCONTENT,CB_SETDROPPEDWIDTH,CB_GETCURSEL,CB_GETLBTEXTLEN,CB_GETLBTEXT,
       LB_SETCURSEL,CB_FINDSTRINGEXACT,CB_ADDSTRING,CB_SETCURSEL,TVIF_TEXT,TVIF_PARAM,TVE_EXPAND,
       MB_ICONERROR,BN_CLICKED,EN_CHANGE,EN_KILLFOCUS,WM_INITDIALOG,WM_COMMAND,EM_LIMITTEXT,CC_FULLOPEN=64,CC_RGBINIT=128,CB_ERR=-1 };
static HWND GetDlgItem(HWND hwnd,int id) { return id; }
static void ShowWindow(HWND hwnd,int mode) { visible[hwnd]=mode==SW_SHOW; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) { invalidated++; }
static void SetDlgItemText(HWND hwnd,int id,const char *value) { snprintf(text[id],sizeof(text[id]),"%s",value); }
static void GetDlgItemText(HWND hwnd,int id,char *out,int size) { snprintf(out,size,"%s",text[id]); }
static HDC GetDC(HWND hwnd) { return NULL; }
static HFONT SelectObject(HDC dc,HFONT font) { return NULL; }
static void ReleaseDC(HWND hwnd,HDC dc) {}
static BOOL GetTextExtentPoint32(HDC dc,const char *value,int n,SIZE *size) { return FALSE; }
static char choices[2][16][MAX_PATH]; static int choicecount[2],choicerow[2],bad_environment,refreshes;
static LRESULT SendMessage(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    int combo=hwnd==IDC_STUDIO_BASE_IMAGE ? 0 : hwnd==IDC_STUDIO_ENVIRONMENT ? 1 : -1;
    if(combo>=0)
    {
        if(message==CB_RESETCONTENT) { choicecount[combo]=0; return 0; }
        if(message==CB_ADDSTRING) { int row=choicecount[combo]++; assert(row<16); strcpy(choices[combo][row],(const char *)lparam); return row; }
        if(message==CB_SETCURSEL) { choicerow[combo]=(int)wparam; return 0; }
        if(message==CB_GETCURSEL) return choicerow[combo];
        if(message==CB_GETLBTEXTLEN) return strlen(choices[combo][wparam]);
        if(message==CB_GETLBTEXT) { strcpy((char *)lparam,choices[combo][wparam]); return 0; }
        if(message==CB_FINDSTRINGEXACT) { for(int i=0;i<choicecount[combo];i++) if(!strcmp(choices[combo][i],(const char *)lparam)) return i; return CB_ERR; }
    }
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
static BOOL StudioViewportCancelTransform(HWND hwnd) { return FALSE; }
static void StudioViewportSetTool(HWND hwnd,int tool) {}
static void StudioViewportSetScene(HWND hwnd,StudioScene *scene,BOOL frame) { invalidated++; }
static void EnableWindow(HWND hwnd,BOOL value) { enabled[hwnd]=value; }
static void CheckRadioButton(HWND hwnd,int first,int last,int value) {}
static int metalness_slider;
static LRESULT SendDlgItemMessage(HWND hwnd,int id,UINT message,WPARAM wparam,LPARAM lparam)
{
    if (id==IDC_STUDIO_METALNESS && message==TBM_SETPOS) { metalness_slider=(int)lparam; }
    return id==IDC_STUDIO_METALNESS && message==TBM_GETPOS ? metalness_slider : SendMessage(id,message,wparam,lparam);
}
static void StudioViewportRefreshImages(HWND hwnd) { refreshes++; }
BOOL TexLoadStudioEnvironment(const char *path,int limit,TexPixel **pixels,int *w,int *h)
{ *pixels=NULL; *w=2; *h=1; return !bad_environment; }
static void RenderStudioCommitLight(StudioLight *light,const StudioLight *previous);
static void RenderStudioCommitGlobalLight(StudioGlobalLight *light,const StudioGlobalLight *previous);
static void SetFocus(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *message,const char *title,int flags) { errors++; }
static BOOL ChooseColor(CHOOSECOLOR *choice)
{ if(choose_switch) g_StudioGeneration++; choice->rgbResult=RGB(17,85,204); return !choose_cancel; }
BOOL StudioSceneSave(const StudioScene *scene,const char **why)
{ if(fail_save) { *why="Test failure"; return FALSE; } saved++; return TRUE; }
static void RenderStudioNewScene(HWND hwnd) { if(!new_cancel) { strcpy(g_StudioScene.filename,"New.rnd"); StudioSceneDefaultLighting(&g_StudioScene); } }
BOOL StudioBounds(const StudioScene *scene,int selected,double lower[3],double upper[3])
{ for(int k=0;k<3;k++) { lower[k]=-2; upper[k]=2; } return scene_bounds; }
#include "ui.inc"
static void Edit(int id,const char *value)
{
    SetDlgItemText(g_StudioProperties,id,value);
    if (id>=IDC_STUDIO_POSITION_X && id<=IDC_STUDIO_SCALE_Z)
    {
        assert(RenderStudioTransformProc(g_StudioTransform,WM_COMMAND,id|(EN_CHANGE<<16),0));
        assert(RenderStudioTransformProc(g_StudioTransform,WM_COMMAND,id|(EN_KILLFOCUS<<16),0));
    }
    else if (g_StudioGlobal>=0)
    {
        assert(RenderStudioGlobalLightCommand(g_StudioProperties,id,EN_CHANGE));
        assert(RenderStudioGlobalLightCommand(g_StudioProperties,id,EN_KILLFOCUS));
    }
    else
    {
        assert(RenderStudioLightCommand(g_StudioProperties,id,EN_CHANGE));
        assert(RenderStudioLightCommand(g_StudioProperties,id,EN_KILLFOCUS));
    }
}
int main(void)
{
    new_cancel=1; RenderStudioAddLight(TRUE); assert(!g_StudioScene.filename[0] && !g_StudioScene.lights[0].enabled);
    new_cancel=0; fail_save=1; RenderStudioAddLight(TRUE); assert(!g_StudioScene.lights[0].enabled && errors==1);
    fail_save=0; scene_bounds=1; RenderStudioAddLight(FALSE);
    assert(g_StudioLight==1 && treeselected->id==-3 && nodecount==4 && !strcmp(treeselected->name,"Point Light 1"));
    assert(g_StudioScene.lights[1].position[1]==6 && g_StudioScene.lights[1].radius==16);
    assert(enabled[IDC_STUDIO_POSITION_X] && !enabled[IDC_STUDIO_ROTATION_X] && !enabled[IDC_STUDIO_SCALE_X]);
    assert(enabled[IDC_STUDIO_MOVE] && !enabled[IDC_STUDIO_ROTATE] && !enabled[IDC_STUDIO_SCALE]);
    assert(g_StudioTool==STUDIO_TRANSLATE); RenderStudioTool(STUDIO_ROTATE); assert(g_StudioTool==STUDIO_TRANSLATE);
    assert(visible[IDC_STUDIO_LIGHT_RANGE] && !visible[IDC_STUDIO_LIGHT_OUTER] && !visible[IDC_STUDIO_MATERIAL_HINT]);
    assert(!strcmp(text[IDC_STUDIO_LIGHT_RANGE_LABEL],"Radius") && viewportselection==-3 && materialcount==0);
    Edit(IDC_STUDIO_LIGHT_RANGE,"24.5"); assert(g_StudioScene.lights[1].radius==24.5);
    Edit(IDC_STUDIO_POSITION_Z,"-12"); assert(g_StudioScene.lights[1].position[2]==-12);
    fail_save=1; Edit(IDC_STUDIO_LIGHT_INTENSITY,"3"); assert(g_StudioScene.lights[1].intensity==1 && errors==2);
    fail_save=0; Edit(IDC_STUDIO_LIGHT_RANGE,"0"); assert(g_StudioScene.lights[1].radius==24.5 && errors==3);
    Edit(IDC_STUDIO_POSITION_X,"NaN"); assert(g_StudioScene.lights[1].position[0]==0);
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
    assert(enabled[IDC_STUDIO_ROTATION_X] && visible[IDC_STUDIO_LIGHT_OUTER]);
    assert(enabled[IDC_STUDIO_ROTATE] && !enabled[IDC_STUDIO_SCALE]);
    assert(!strcmp(text[IDC_STUDIO_ROTATION_LABEL],"Direction"));
    RenderStudioTool(STUDIO_ROTATE); assert(g_StudioTool==STUDIO_ROTATE); RenderStudioTool(STUDIO_SCALE); assert(g_StudioTool==STUDIO_ROTATE);
    assert(!strcmp(text[IDC_STUDIO_LIGHT_RANGE_LABEL],"Inner angle (deg)"));
    Edit(IDC_STUDIO_ROTATION_X,"1"); Edit(IDC_STUDIO_ROTATION_Y,"0");
    assert(g_StudioScene.lights[0].direction[0]==1 && g_StudioScene.lights[0].direction[1]==0);
    Edit(IDC_STUDIO_LIGHT_OUTER,"50"); Edit(IDC_STUDIO_LIGHT_RANGE,"40");
    assert(g_StudioScene.lights[0].inner==40 && g_StudioScene.lights[0].outer==50);
    RenderStudioAddLight(FALSE); assert(g_StudioLight==2 && treeselected->id==-4 && nodecount==6);
    int count=nodecount,attempts=saved; RenderStudioAddLight(FALSE); RenderStudioAddLight(TRUE);
    assert(nodecount==count && saved==attempts && g_StudioLight==2);
    StudioMaterial material={0}; strcpy(material.name,"Material"); material.shininess=32;
    StudioInstance model={0}; strcpy(model.model,"Fixture.gltf"); model.materials=&material; model.materialcount=1;
    g_StudioScene.objects=&model; g_StudioScene.count=1; RenderStudioOutliner();
    assert(nodecount==7 && treeselected->id==-4); /* Light identity survives inserted model rows. */
    RenderStudioSelect(0,0); assert(g_StudioLight==-1 && viewportselection==0 && treeselected->id==0 && materialcount==1);
    assert(visible[IDC_STUDIO_BASE_COLOR] && !visible[IDC_STUDIO_LIGHT_COLOR]);
    RenderStudioSelect(-2,-1); assert(g_StudioObject==-1 && g_StudioMaterial==-1 && materialcount==0 && viewportselection==-2);
    RenderStudioSelect(-1,-1); assert(g_StudioLight==-1 && visible[IDC_STUDIO_ENVIRONMENT]);
    RenderStudioSelect(-3,-1); StudioLight retained=g_StudioScene.lights[1],second=g_StudioScene.lights[2];
    fail_save=1; RenderStudioDeleteLight(); fail_save=0;
    assert(g_StudioLight==1 && !memcmp(&retained,&g_StudioScene.lights[1],sizeof(retained)));
    RenderStudioDeleteLight(); assert(g_StudioLight==-1 && !g_StudioScene.lights[1].enabled && nodecount==6);
    assert(!memcmp(&second,&g_StudioScene.lights[2],sizeof(second)) && viewportselection==-1);
    assert(StudioSceneLightSlot(&g_StudioScene,FALSE)==1); RenderStudioAddLight(FALSE); assert(g_StudioLight==1);
    RenderStudioSelect(-2,-1); RenderStudioDeleteLight(); assert(!g_StudioScene.lights[0].enabled);
    assert(StudioSceneLightSlot(&g_StudioScene,TRUE)==0);
    RenderStudioSelect(0,0); attempts=saved; RenderStudioDeleteLight(); assert(saved==attempts && g_StudioScene.count==1);
    g_StudioScene.lights[2].enabled=FALSE; RenderStudioSelect(-4,-1); assert(g_StudioLight==-1 && treeselected==nodes);
    RenderStudioSelect(STUDIO_SELECT_AMBIENT,-1);
    assert(g_StudioGlobal==0 && g_StudioLight==-1 && g_StudioObject==-1 && materialcount==0);
    assert(viewportselection==STUDIO_SELECT_AMBIENT && !strcmp(treeselected->name,"Ambient Light"));
    assert(visible[IDC_STUDIO_LIGHT_COLOR] && visible[IDC_STUDIO_LIGHT_INTENSITY] && !visible[IDC_STUDIO_LIGHT_RANGE]);
    assert(!visible[IDC_STUDIO_LIGHT_OUTER] && !visible[IDC_STUDIO_BASE_COLOR]);
    for(int id=IDC_STUDIO_POSITION_X;id<=IDC_STUDIO_SCALE_Z;id++) assert(!enabled[id]);
    for(int id=IDC_STUDIO_MOVE;id<=IDC_STUDIO_SCALE;id++) assert(!enabled[id]);
    int tool=g_StudioTool; RenderStudioTool(STUDIO_SCALE); assert(g_StudioTool==tool);
    Edit(IDC_STUDIO_LIGHT_INTENSITY,"0"); assert(g_StudioScene.ambient.intensity==0);
    RenderStudioGlobalLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(fabs(g_StudioScene.ambient.color[1]-1.0/3)<1e-6);
    RenderStudioOutliner(); assert(treeselected->id==STUDIO_SELECT_AMBIENT);
    attempts=saved; count=nodecount; RenderStudioDeleteLight();
    assert(saved==attempts && nodecount==count && g_StudioGlobal==0);
    StudioGlobalLight global=g_StudioScene.ambient; fail_save=1;
    Edit(IDC_STUDIO_LIGHT_INTENSITY,"2"); assert(!memcmp(&global,&g_StudioScene.ambient,sizeof(global))); fail_save=0;
    Edit(IDC_STUDIO_LIGHT_INTENSITY,"-1"); assert(!memcmp(&global,&g_StudioScene.ambient,sizeof(global)));
    Edit(IDC_STUDIO_LIGHT_INTENSITY,"NaN"); assert(!memcmp(&global,&g_StudioScene.ambient,sizeof(global)));
    RenderStudioSelect(STUDIO_SELECT_DIRECTIONAL,-1);
    assert(g_StudioGlobal==1 && viewportselection==STUDIO_SELECT_DIRECTIONAL && !strcmp(treeselected->name,"Directional Light"));
    assert(!strcmp(text[IDC_STUDIO_ROTATION_LABEL],"Direction"));
    for(int field=0;field<9;field++) assert(enabled[IDC_STUDIO_POSITION_X+field]==(field>=3 && field<6));
    for(int id=IDC_STUDIO_MOVE;id<=IDC_STUDIO_SCALE;id++) assert(!enabled[id]);
    Edit(IDC_STUDIO_ROTATION_X,"0"); Edit(IDC_STUDIO_ROTATION_Z,"0"); Edit(IDC_STUDIO_ROTATION_Y,"-7");
    assert(g_StudioScene.directional.direction[1]==-7);
    Edit(IDC_STUDIO_ROTATION_Y,"0"); assert(g_StudioScene.directional.direction[1]==-7);
    Edit(IDC_STUDIO_LIGHT_INTENSITY,"0"); assert(g_StudioScene.directional.intensity==0);
    global=g_StudioScene.directional; fail_save=1;
    Edit(IDC_STUDIO_ROTATION_X,"1"); assert(!memcmp(&global,&g_StudioScene.directional,sizeof(global)));
    RenderStudioGlobalLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(!memcmp(&global,&g_StudioScene.directional,sizeof(global))); fail_save=0;
    choose_cancel=1; RenderStudioGlobalLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(!memcmp(&global,&g_StudioScene.directional,sizeof(global))); choose_cancel=0;
    choose_switch=1; RenderStudioGlobalLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(!memcmp(&global,&g_StudioScene.directional,sizeof(global))); choose_switch=0;
    RenderStudioGlobalLightCommand(g_StudioProperties,IDC_STUDIO_LIGHT_COLOR,BN_CLICKED);
    assert(fabs(g_StudioScene.directional.color[2]-.8)<1e-6);
    attempts=saved; RenderStudioDeleteLight(); assert(saved==attempts && g_StudioGlobal==1);
    RenderStudioSelect(0,0); assert(g_StudioGlobal==-1 && visible[IDC_STUDIO_BASE_COLOR] && !visible[IDC_STUDIO_LIGHT_COLOR]);
    assert(visible[IDC_STUDIO_EMISSION_COLOR] && visible[IDC_STUDIO_METALNESS]);
    attempts=saved;
    metalness_slider=37; assert(RenderStudioMetalnessCommand(g_StudioProperties,TB_THUMBTRACK));
    assert(fabs(material.metalness-.37)<1e-6 && saved==attempts && !strcmp(text[IDC_STUDIO_METALNESS_LABEL],"Metalness: 37%"));
    metalness_slider=75; assert(RenderStudioMetalnessCommand(g_StudioProperties,TB_THUMBTRACK));
    assert(saved==attempts); assert(RenderStudioMetalnessCommand(g_StudioProperties,TB_THUMBPOSITION));
    assert(saved==attempts+1 && material.metalness==.75f && !g_StudioMetalnessMaterial);
    RenderStudioMetalnessCommand(g_StudioProperties,TB_ENDTRACK); assert(saved==attempts+1);
    fail_save=1; metalness_slider=90; RenderStudioMetalnessCommand(g_StudioProperties,TB_THUMBTRACK);
    RenderStudioMetalnessCommand(g_StudioProperties,TB_ENDTRACK); fail_save=0;
    assert(material.metalness==.75f && metalness_slider==75);
    metalness_slider=0; RenderStudioMetalnessCommand(g_StudioProperties,TB_LINEUP); assert(material.metalness==0);
    metalness_slider=100; RenderStudioMetalnessCommand(g_StudioProperties,TB_THUMBTRACK); attempts=saved;
    RenderStudioSelect(STUDIO_SELECT_AMBIENT,-1);
    assert(material.metalness==1 && saved==attempts+1 && !g_StudioMetalnessMaterial);
    assert(!visible[IDC_STUDIO_EMISSION_COLOR] && !visible[IDC_STUDIO_METALNESS]);
    assert(!RenderStudioMetalnessCommand(g_StudioProperties,TB_THUMBTRACK));
    RenderStudioSelect(0,0);
    StudioMaterial prior=material; choose_cancel=1;
    RenderStudioMaterialCommand(g_StudioProperties,IDC_STUDIO_EMISSION_COLOR,BN_CLICKED);
    assert(!memcmp(&prior,&material,sizeof(prior))); choose_cancel=0; choose_switch=1;
    RenderStudioMaterialCommand(g_StudioProperties,IDC_STUDIO_EMISSION_COLOR,BN_CLICKED);
    assert(!memcmp(&prior,&material,sizeof(prior))); choose_switch=0; fail_save=1;
    RenderStudioMaterialCommand(g_StudioProperties,IDC_STUDIO_EMISSION_COLOR,BN_CLICKED);
    assert(!memcmp(&prior,&material,sizeof(prior))); fail_save=0;
    RenderStudioMaterialCommand(g_StudioProperties,IDC_STUDIO_EMISSION_COLOR,BN_CLICKED);
    assert(fabs(material.emission[2]-.8)<1e-6 && material.base[2]==prior.base[2]);
    RenderStudioSelect(-1,-1); assert(visible[IDC_STUDIO_ENVIRONMENT] && !visible[IDC_STUDIO_METALNESS]);
    TexThumb images[2]={0}; strcpy(images[0].label,"Panorama.bmp"); strcpy(images[1].label,"Other.bmp");
    RenderStudioImageChoices(images,2); assert(choicecount[0]==3 && choicecount[1]==3);
    assert(!strcmp(choices[1][0],"None") && !strcmp(choices[1][1],"Panorama.bmp"));
    attempts=saved; choicerow[1]=1; bad_environment=1;
    assert(RenderStudioEnvironmentCommand(g_StudioProperties,IDC_STUDIO_ENVIRONMENT,CBN_SELCHANGE));
    assert(saved==attempts && !g_StudioScene.environment[0] && choicerow[1]==0); bad_environment=0;
    choicerow[1]=1; fail_save=1; RenderStudioEnvironmentCommand(g_StudioProperties,IDC_STUDIO_ENVIRONMENT,CBN_SELCHANGE);
    assert(saved==attempts && !g_StudioScene.environment[0] && choicerow[1]==0); fail_save=0;
    choicerow[1]=1; RenderStudioEnvironmentCommand(g_StudioProperties,IDC_STUDIO_ENVIRONMENT,CBN_SELCHANGE);
    assert(!strcmp(g_StudioScene.environment,"Panorama.bmp") && saved==attempts+1 && refreshes>0);
    bad_environment=1; RenderStudioImageChoices(images+1,1); assert(!strcmp(g_StudioScene.environment,"Panorama.bmp"));
    assert(choicecount[1]==3 && choicerow[1]==2 && strstr(text[IDC_STUDIO_ENVIRONMENT_HINT],"unavailable")); bad_environment=0;
    RenderStudioSelect(STUDIO_SELECT_AMBIENT,-1); assert(!visible[IDC_STUDIO_ENVIRONMENT]);
    assert(!RenderStudioEnvironmentCommand(g_StudioProperties,IDC_STUDIO_ENVIRONMENT,CBN_SELCHANGE));
    RenderStudioSelect(0,0); assert(!visible[IDC_STUDIO_ENVIRONMENT] && visible[IDC_STUDIO_METALNESS]);
    RenderStudioSelect(-1,-1); choicerow[1]=0;
    RenderStudioEnvironmentCommand(g_StudioProperties,IDC_STUDIO_ENVIRONMENT,CBN_SELCHANGE);
    assert(!g_StudioScene.environment[0] && choicerow[1]==0);
    assert(invalidated>0);
    puts("PASS: shared image catalog, scene-only selector, validation, save rollback, cache refresh, missing-image retention and None.");
    puts("PASS: emission picker/cancel/rollback, live metalness previews, one save per drag, keyboard adjustment, selection-change commit and light-control isolation.");
    puts("PASS: permanent globals, isolated controls, direction validation, zero intensity, color dialogs, non-deletion and save rollback.");
    puts("PASS: add/cancel/failure, outliner identities, material/light isolation, Transform/Properties controls, tool availability, numeric/color edits, deletion/slot reuse and rollback.");
    return 0;
}
