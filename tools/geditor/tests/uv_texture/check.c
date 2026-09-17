#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *HWND;
typedef void *HDC;
typedef struct { int x,y; } POINT;
typedef struct { int left,top,right,bottom; } RECT;
#define WM_APP 0x8000
#define MB_ICONERROR 16
#define IDCANCEL 2
#define SW_HIDE 0
#define SW_SHOW 1
#define COLORONCOLOR 3
#define DIB_RGB_COLORS 0
#define SRCCOPY 0
#define RGB(r,g,b) ((unsigned)(r) | ((unsigned)(g)<<8) | ((unsigned)(b)<<16))
#define GetRValue(c) ((c)&255)
#define GetGValue(c) (((c)>>8)&255)
#define GetBValue(c) (((c)>>16)&255)
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))
#define lstrcpy strcpy
#define UVCANVAS_UNIT_FILL RGB(48,48,48)
#include "uvcanvas.h"
#include "resource.h"
#include "types.inc"
typedef struct { BITMAPINFOHEADER bmiHeader; DWORD color; } BITMAPINFO;

static UVCanvasState state;
static HWND g_UVCanvas=&state, g_UVEditor=(HWND)1;
static RECT client={0,0,1170,877}, controls[1200], imagebox;
static int selectioncount=2, loads, imagewidth=3, imageheight=2, draws;
static EditorTool tool=EDITOR_TOOL_FACE_SELECT;
static BOOL available=TRUE, sliderenabled;
static char statustext[80];
static TexPixel fixture[]={{255,0,0,255},{0,255,0,255},{0,0,255,255},
                           {255,255,0,255},{255,0,255,0},{0,255,255,128}};
static UVCanvasState *UVCanvasGetState(HWND hwnd) { return hwnd; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static HWND GetDlgItem(HWND hwnd,int id) { return (HWND)(intptr_t)id; }
static void SetDlgItemText(HWND hwnd,int id,const char *text)
{ if(id==IDC_UV_TEXTURE_STATUS)snprintf(statustext,sizeof(statustext),"%s",text); }
static void EnableWindow(HWND hwnd,BOOL enabled)
{ if((intptr_t)hwnd==IDC_UV_TEXTURE_OPACITY)sliderenabled=enabled; }
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags) { abort(); }
static int SetStretchBltMode(HDC dc,int mode) { return COLORONCOLOR; }
static void StretchDIBits(HDC dc,int x,int y,int w,int h,int sx,int sy,int sw,int sh,
                         const unsigned char *pixels,const BITMAPINFO *info,int usage,int op)
{
    draws++; imagebox=(RECT){x,y,x+w,y+h};
    assert(sx==0 && sy==0 && sw==imagewidth && sh==imageheight);
    assert(info->bmiHeader.biHeight==imageheight && info->bmiHeader.biWidth==imagewidth);
    assert(info->bmiHeader.biBitCount==32 && pixels==state.texturebitmap);
}
#include "canvas.inc"

static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return selectioncount; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd,BgFaceRef *refs,int count)
{
    for(int i=0;i<count;i++)refs[i]=(BgFaceRef){.room=i+1,.faceid=i+1};
    return TRUE;
}
static BOOL ViewportGetTextureSize(HWND hwnd,unsigned short id,int *w,int *h)
{ *w=imagewidth; *h=imageheight; return available; }
BOOL TexLoadProjectImage(const char *dir,DWORD id,TexPixel *out,int *w,int *h)
{
    assert(!strcmp(dir,"project") && id==0x558); loads++;
    if(!available)return FALSE;
    memcpy(out,fixture,sizeof(fixture)); *w=imagewidth; *h=imageheight; return TRUE;
}
BOOL UVCanvasSetTriangles(HWND hwnd,UVCanvasTriangle *triangles,int count)
{ free(state.triangles); state.triangles=triangles; state.trianglecount=count; return TRUE; }
static void GetClientRect(HWND hwnd,RECT *r) { *r=client; }
static void MapDialogRect(HWND hwnd,RECT *r)
{ r->left=r->left*3/2; r->right=r->right*3/2; r->top=r->top*13/8; r->bottom=r->bottom*13/8; }
static void MoveWindow(HWND hwnd,int x,int y,int w,int h,BOOL paint)
{
    assert(w>=0 && h>=0);
    if(hwnd==g_UVCanvas) { state.width=w; state.height=h; return; }
    int id=(int)(intptr_t)hwnd; assert(id>=0 && id<1200);
    controls[id]=(RECT){x,y,x+w,y+h};
}
static void ShowWindow(HWND hwnd,int show) {}
#include "editor.inc"

static void Selection(void)
{
    BgDocumentVertex vertices[3]={{.id=1},{.id=2,.s=96},{.id=3,.t=64}};
    BgDocumentFace faces[2]={{.id=1,.room=1,.textureid=0x558,.vertexindices={0,1,2}},
                            {.id=2,.room=2,.textureid=0x558,.vertexindices={0,1,2}}};
    BgDocumentRoom rooms[3]={[1]={.faces=faces,.facecount=1,.vertices=vertices,.vertexcount=3},
                            [2]={.faces=faces+1,.facecount=1,.vertices=vertices,.vertexcount=3}};
    BgDocument doc={.rooms=rooms,.roomcount=2,.facecount=2,.levelscale=1};
    state.textureopacity=50;
    UVEditorRefreshSelection(NULL,&doc,"project");
    assert(loads==1 && state.texture && state.trianglecount==2 && sliderenabled && !doc.dirty);
    assert(!strcmp(statustext,"Image 0558 (3 x 2)"));
    assert(state.triangles[0].uv[1][0]==1 && state.triangles[0].uv[2][1]==1);
    faces[1].textureid=0x123;
    UVEditorRefreshSelection(NULL,&doc,"project");
    assert(loads==1 && !state.texture && !sliderenabled && !strcmp(statustext,"Mixed textures"));
    faces[1].textureid=BG_TEX_NONE;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(!state.texture && loads==1);
    faces[0].textureid=BG_TEX_NONE;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(!strcmp(statustext,"No texture"));
    faces[0].textureid=faces[1].textureid=0x558;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(state.texture);
    available=FALSE;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(!state.texture && !sliderenabled);
    assert(!strcmp(statustext,"Image 0558 unavailable"));
    available=TRUE; fixture[0].r=77; /* Reimport/replacement with the same ID. */
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(state.texture[0].r==77);
    fixture[0].r=255;
    selectioncount=0;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(!state.texture && !state.trianglecount);
    selectioncount=2;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(state.texture);
    tool=EDITOR_TOOL_VERTEX_SELECT;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(!state.texture && !state.trianglecount);
    tool=EDITOR_TOOL_FACE_SELECT;
    UVEditorRefreshSelection(NULL,&doc,"project"); assert(state.texture);
    assert(!doc.dirty && vertices[1].s==96 && vertices[2].t==64);
}

static void PixelsAndPlacement(void)
{
    UVCanvasTriangle *triangles=state.triangles;
    state.draghandle=3; state.values[0]=.25;
    UVCanvasSetTextureOpacity(g_UVCanvas,100);
    /* Native row zero remains the bottom row of the positive-height DIB. */
    assert(state.texturebitmap[0]==0 && state.texturebitmap[1]==0 && state.texturebitmap[2]==255);
    assert(state.texturebitmap[12]==0 && state.texturebitmap[13]==255 && state.texturebitmap[14]==255);
    assert(state.texturebitmap[16]==48 && state.texturebitmap[17]==48 && state.texturebitmap[18]==48);
    assert(state.texturebitmap[20]==152 && state.texturebitmap[21]==152 && state.texturebitmap[22]==24);
    UVCanvasSetTextureOpacity(g_UVCanvas,50);
    assert(state.texturebitmap[2]==152 && state.texturebitmap[1]==24);
    UVCanvasSetTextureOpacity(g_UVCanvas,-10);
    assert(state.textureopacity==0 && state.texturebitmap[2]==48);
    UVCanvasDrawTexture(NULL,&state,100,100,500,500); assert(!draws);
    UVCanvasSetTextureOpacity(g_UVCanvas,120); assert(state.textureopacity==100);
    assert(state.triangles==triangles && state.draghandle==3 && state.values[0]==.25);
    state.width=800; state.height=600;
    for(int high=0;high<2;high++)
    {
        state.pixelsperunit=high?1000000:400;
        state.centeru=high?.9999:.25; state.centerv=high?.0001:.75;
        const double tl[2]={0,1},br[2]={1,0}; double a[2],b[2];
        UVCanvasProject(&state,tl,a); UVCanvasProject(&state,br,b);
        UVCanvasDrawTexture(NULL,&state,a[0],a[1],b[0],b[1]);
        assert(imagebox.left==(int)floor(a[0]+.5) && imagebox.top==(int)floor(a[1]+.5));
        assert(imagebox.right==(int)floor(b[0]+.5) && imagebox.bottom==(int)floor(b[1]+.5));
        if(high)assert(imagebox.left<0 && imagebox.top<0 && imagebox.right-imagebox.left==1000000);
    }
    assert(draws==2);
    TexPixel *invalid=malloc(sizeof(*invalid)); assert(invalid);
    assert(!UVCanvasSetTexture(g_UVCanvas,invalid,0,1) && !state.texture && !state.texturebitmap);
}

static void Layout(void)
{
    const int sizes[][2]={{510,390},{1170,877},{1800,1100}};
    const int preview[]={IDC_UV_TEXTURE_LABEL,IDC_UV_TEXTURE_STATUS,
                         IDC_UV_TEXTURE_OPACITY_LABEL,IDC_UV_TEXTURE_OPACITY,IDCANCEL};
    for(unsigned n=0;n<sizeof(sizes)/sizeof(sizes[0]);n++)
    {
        client.right=sizes[n][0]; client.bottom=sizes[n][1]; UVEditorLayout(g_UVEditor);
        for(unsigned i=0;i<sizeof(preview)/sizeof(preview[0]);i++)
        {
            RECT r=controls[preview[i]];
            assert(r.left>=state.width && r.right<=client.right && r.top>=0 && r.bottom<=client.bottom);
            if(i)assert(controls[preview[i-1]].bottom<=r.top);
        }
        assert(controls[IDC_UV_HINT].bottom<=controls[IDC_UV_TEXTURE_LABEL].top);
    }
}

int main(void)
{
    Selection(); PixelsAndPlacement(); Layout();
    UVCanvasSetTexture(g_UVCanvas,NULL,0,0); free(state.triangles);
    puts("PASS: shared/mixed/untextured selections, missing/replaced images, RGBA/opacity, V-up orientation, pan/high zoom, live-drag isolation and minimum/default/large layouts.");
    return 0;
}
