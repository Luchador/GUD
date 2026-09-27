/* Exercise the actual viewport menu, including clicked-edge selection and
 * the frame's read-only eligibility query before the action is offered. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bgdocument.h"
#include "stanload.h"
#include "edittool.h"
typedef void *HWND;
typedef void *HMENU;
typedef unsigned int UINT;
typedef intptr_t LPARAM;
typedef struct { int x,y; } POINT;
typedef struct { int unused; } ViewportObjectPaste,ViewportPickRay;
typedef struct ViewportState {
    BOOL orbit,flying,vertexsnap,boxpending;
    int dragaxis;
    EditorTool tool;
    BgFaceRef *scenefacerefs;
    BOOL showstan;
    int stanopacity;
    StanFile stan;
    unsigned char *stanselected;
} ViewportState;
enum { MF_STRING=0,MF_ENABLED=0,MF_GRAYED=1,MF_CHECKED=8,MF_SEPARATOR=0x800,
       TPM_RETURNCMD=1,TPM_NONOTIFY=2,TPM_RIGHTBUTTON=4,
       VIEWPORT_WM_CAN_PASTE_OBJECT=80,VIEWPORT_WM_PASTE_OBJECT_HERE=81,
       VIEWPORT_WM_SPLIT_EDGE=51,VIEWPORT_WM_DISCONNECT_FACES=52,VIEWPORT_WM_MARK_SEAM=78,
       VIEWPORT_WM_LINK_STAN_TILES=71,VIEWPORT_WM_SPLIT_STAN_EDGE=75,VIEWPORT_WM_LINK_STAN_EDGE=76,
       VIEWPORT_WM_CREATE_DOOR_SHADOW=100,VIEWPORT_WM_CAN_REVERSE_EDGE=111,VIEWPORT_WM_REVERSE_EDGE=112,
       VIEWPORT_WM_STAN_TYPE_CHANGED=114 };
static BOOL hit=TRUE,pickstan,eligible=TRUE;
static BgDocumentEdgeRef selected;
static int menus,queries,actions,choice=6;
static UINT items,disabled,checked;
static DWORD hittile=STAN_TILE_NONE;
static double scandistance=20;
static unsigned typechanges;
static StanTileType senttype;
static HWND GetParent(HWND hwnd) { return hwnd; }
static intptr_t SendMessage(HWND hwnd,UINT msg,UINT wparam,LPARAM lparam)
{
    if(msg==VIEWPORT_WM_CAN_PASTE_OBJECT)return FALSE;
    if(msg==VIEWPORT_WM_CAN_REVERSE_EDGE) { assert(selected.face.faceid==42 && selected.corner==2);queries++;return eligible; }
    if(msg==VIEWPORT_WM_STAN_TYPE_CHANGED) { assert(!lparam);senttype=wparam;typechanges++;return TRUE; }
    assert(msg==VIEWPORT_WM_REVERSE_EDGE && !wparam && !lparam);actions++;return TRUE;
}
static BOOL ViewportObjectPasteTarget(HWND h,const ViewportState *s,int x,int y,ViewportObjectPaste *t) { return FALSE; }
static BOOL ViewportTryPickStan(HWND h,ViewportState *s,int x,int y,BOOL a,BOOL b) { return pickstan; }
static BOOL ViewportGetSelectedStanEdge(HWND h,StanEdgeRef *e) { *e=(StanEdgeRef){0};return TRUE; }
static BOOL ViewportFindContextEdge(const ViewportState *s,int x,int y,BgDocumentEdgeRef *e)
{ *e=(BgDocumentEdgeRef){{42,1,0,0},2};return hit; }
static BOOL ViewportSelectBgEdges(HWND h,const BgDocumentEdgeRef *e,DWORD count) { assert(count==1);selected=*e;return TRUE; }
static DWORD ViewportGetStanSelectionCount(HWND h,DWORD *tiles)
{
    ViewportState *s=h;DWORD count=0;
    for(DWORD i=0;i<s->stan.tilecount;i++)count+=s->stanselected[i]!=0;
    return count;
}
static DWORD ViewportGetSelectedBgFaceCount(HWND h) { return 1; }
static BOOL ViewportBuildPickRay(HWND h,const ViewportState *s,int x,int y,ViewportPickRay *r) { return TRUE; }
static DWORD ViewportFindPickedStan(const ViewportState *s,const ViewportPickRay *r,double *d) { *d=10;return hittile; }
static double ViewportSceneHitDistance(const ViewportState *s,const ViewportPickRay *r) { return scandistance; }
static double ViewportCoplanarPickTolerance(double d) { return .001; }
static BOOL ViewportSelectStanTiles(HWND h,const DWORD *tiles,DWORD count)
{
    ViewportState *s=h;assert(count==1 && tiles[0]<s->stan.tilecount);
    memset(s->stanselected,0,s->stan.tilecount);s->stanselected[tiles[0]]=1;return TRUE;
}
static int ViewportFindPickedTriangle(const ViewportState *s,const ViewportPickRay *r,BOOL a,BOOL b,double *d) { return -1; }
static BOOL ViewportSelectBgFaces(HWND h,const BgFaceRef *f,DWORD count) { return FALSE; }
static HMENU CreatePopupMenu(void) { menus++;items=disabled=checked=0;return (HMENU)1; }
static BOOL AppendMenu(HMENU menu,UINT flags,UINT id,const char *label)
{
    if(flags==MF_SEPARATOR)return TRUE;
    assert(id<10);items|=1u<<id;if(flags&MF_GRAYED)disabled|=1u<<id;if(flags&MF_CHECKED)checked|=1u<<id;
    if(id==6)assert(!strcmp(label,"Reverse Edge\tT"));
    return TRUE;
}
static void ClientToScreen(HWND h,POINT *p) {}
static UINT TrackPopupMenu(HMENU m,UINT flags,int x,int y,int reserved,HWND h,void *rect)
{ return choice && (items&(1u<<choice)) && !(disabled&(1u<<choice)) ? choice : 0; }
static void DestroyMenu(HMENU m) {}
#include "context.inc"
int main(void)
{
    ViewportState s={.dragaxis=-1,.tool=EDITOR_TOOL_EDGE_SELECT};
    ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(menus==1 && queries==1 && actions==1 && (items&(1u<<6)) && !(disabled&(1u<<6)));
    eligible=FALSE;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(menus==2 && queries==2 && actions==1 && (disabled&(1u<<6)));
    eligible=TRUE;choice=0;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(menus==3 && actions==1);choice=6;
    hit=FALSE;ViewportShowGeometryContextMenu(&s,&s,50,50);assert(menus==3);hit=TRUE;
    pickstan=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(menus==4 && actions==1 && !(items&(1u<<6)));pickstan=FALSE;
    s.tool=EDITOR_TOOL_FACE_SELECT;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(menus==5 && !(items&(1u<<6)));s.tool=EDITOR_TOOL_EDGE_SELECT;
    s.flying=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,50);s.flying=FALSE;
    s.orbit=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,50);s.orbit=FALSE;
    s.vertexsnap=TRUE;ViewportShowGeometryContextMenu(&s,&s,50,50);s.vertexsnap=FALSE;
    s.dragaxis=0;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(menus==5 && actions==1);
    s.dragaxis=-1;s.tool=EDITOR_TOOL_FACE_SELECT;s.showstan=TRUE;s.stanopacity=44;
    StanTile tiles[3]={{.special=STAN_TYPE_LADDER},{.special=STAN_TYPE_LADDER},{.special=STAN_TYPE_NORMAL}};
    unsigned char selectedtiles[3]={1,1,0};s.stan=(StanFile){.tiles=tiles,.tilecount=3};s.stanselected=selectedtiles;
    hittile=0;choice=9;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(typechanges==1 && senttype==STAN_TYPE_FORCED_CROUCH && selectedtiles[0] && selectedtiles[1]);
    assert(checked==(1u<<8) && (items&(1u<<1))); /* pair keeps Link Stan Tiles */
    hittile=2;choice=8;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(typechanges==2 && senttype==STAN_TYPE_LADDER && !selectedtiles[0] && !selectedtiles[1] && selectedtiles[2]);
    assert(checked==(1u<<7) && !(items&(1u<<1))); /* right-click selects an unselected tile */
    selectedtiles[0]=selectedtiles[1]=1;hittile=1;choice=7;
    ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(typechanges==3 && senttype==STAN_TYPE_NORMAL && !checked && !(items&(1u<<1)));
    assert(selectedtiles[0] && selectedtiles[1] && selectedtiles[2]); /* mixed group */
    selectedtiles[0]=selectedtiles[1]=0;tiles[2].special=2;choice=0;hittile=2;
    ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(!checked && tiles[2].special==2 && typechanges==3); /* cancel preserves unknown type */
    selectedtiles[0]=selectedtiles[1]=selectedtiles[2]=0;scandistance=5;
    ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(!(items&((1u<<7)|(1u<<8)|(1u<<9))) && !ViewportGetStanSelectionCount(&s,NULL)); /* occluded tile */
    scandistance=20;s.stanopacity=0;ViewportShowGeometryContextMenu(&s,&s,50,50);
    assert(!(items&(1u<<7)));s.stanopacity=44;s.showstan=FALSE;
    ViewportShowGeometryContextMenu(&s,&s,50,50);assert(!(items&(1u<<7)) && typechanges==3);
    puts("PASS: right-click selects its BG edge, offers Reverse Edge with T, queries eligibility, disables invalid flips, and respects cancellation/tool/gesture scope.");
    puts("PASS: stan context types, clicked tile, retained multi-selection, common/mixed/unknown checks, native commands, cancellation and occlusion/visibility.");
    return 0;
}
