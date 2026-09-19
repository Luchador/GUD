#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *HWND;
typedef void *HDC;
typedef void *HGDIOBJ;
typedef void *HBRUSH;
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
#define DC_BRUSH 1
#define DC_PEN 2
#define HOLLOW_BRUSH 3
#define RGB(r,g,b) ((unsigned)(r) | ((unsigned)(g)<<8) | ((unsigned)(b)<<16))
#define GetRValue(c) ((c)&255)
#define GetGValue(c) (((c)>>8)&255)
#define GetBValue(c) (((c)>>16)&255)
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))
#define lstrcpy strcpy
#define UVCANVAS_UNIT_FILL RGB(48,48,48)
#define UVCANVAS_UNIT_EDGE RGB(160,160,160)
#define UVCANVAS_LIMIT_EDGE RGB(255,80,80)
#include "uvcanvas.h"
#include "resource.h"
#include "types.inc"
typedef struct { BITMAPINFOHEADER bmiHeader; DWORD color; } BITMAPINFO;

static UVCanvasState state;
static HWND g_UVCanvas=&state, g_UVEditor=(HWND)1;
static RECT client={0,0,1170,877}, controls[1300];
static unsigned char *screenpixels;
static unsigned pencolor;
static POINT linestart, lines[4][2];
static int linecount;
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
static HGDIOBJ GetStockObject(int id) { return (HGDIOBJ)(intptr_t)id; }
static HGDIOBJ SelectObject(HDC dc,HGDIOBJ object) { return object; }
static void SetDCBrushColor(HDC dc,unsigned color) {}
static void SetDCPenColor(HDC dc,unsigned color) { pencolor=color; }
static void FillRect(HDC dc,const RECT *rect,HBRUSH brush) {}
static void Rectangle(HDC dc,int left,int top,int right,int bottom) {}
static void UVCanvasDrawTriangles(HDC dc,const UVCanvasState *canvas) {}
static void UVCanvasDrawTools(HDC dc,const UVCanvasState *canvas) {}
static void MoveToEx(HDC dc,int x,int y,void *old) { linestart=(POINT){x,y}; }
static void LineTo(HDC dc,int x,int y)
{
    assert(pencolor==UVCANVAS_LIMIT_EDGE && linecount<4);
    lines[linecount][0]=linestart; lines[linecount++][1]=(POINT){x,y};
}
static void StretchDIBits(HDC dc,int x,int y,int w,int h,int sx,int sy,int sw,int sh,
                         const unsigned char *pixels,const BITMAPINFO *info,int usage,int op)
{
    draws++;
    assert(x==0 && y==0 && w==state.width && h==state.height);
    assert(sx==0 && sy==0 && sw==w && sh==h);
    assert(info->bmiHeader.biHeight==-h && info->bmiHeader.biWidth==w);
    assert(info->bmiHeader.biBitCount==32);
    free(screenpixels); screenpixels=malloc((size_t)w*h*4); assert(screenpixels);
    memcpy(screenpixels,pixels,(size_t)w*h*4);
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
    int id=(int)(intptr_t)hwnd; assert(id>=0 && id<1300);
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

static void CheckScreenPixels(void)
{
    /* Reference samples in unwrapped native texel coordinates; remainder
       after flooring handles the negative tiles independently of the renderer. */
    const double left=state.width*.5-state.centeru*state.pixelsperunit;
    const double bottom=state.height*.5+state.centerv*state.pixelsperunit;
    for(int y=0;y<state.height;y++)for(int x=0;x<state.width;x++)
    {
        double u=(x+.5-left)/state.pixelsperunit;
        double v=(bottom-y-.5)/state.pixelsperunit;
        int s=(int)floor(u*imagewidth),t=(int)floor(v*imageheight);
        s=(s%imagewidth+imagewidth)%imagewidth;
        t=(t%imageheight+imageheight)%imageheight;
        TexPixel pixel=fixture[t*imagewidth+s];
        BOOL unit=u>=0 && u<1 && v>=0 && v<1;
        double alpha=pixel.a/255.0*state.textureopacity/100.0*(unit?1:.5);
        int background=unit?48:40;
        const unsigned char channels[]={pixel.b,pixel.g,pixel.r};
        const unsigned char *actual=screenpixels+((size_t)y*state.width+x)*4;
        for(int c=0;c<3;c++)
        {
            int expected=(int)floor(channels[c]*alpha+background*(1-alpha)+.5);
            assert(actual[c]==expected);
        }
        assert(actual[3]==0);
    }
}

static void PixelsAndPlacement(void)
{
    UVCanvasTriangle *triangles=state.triangles;
    state.draghandle=3; state.values[0]=.25;
    UVCanvasSetTextureOpacity(g_UVCanvas,100);
    /* Cache preserves native row zero (V=0); repeats blend at half opacity. */
    assert(state.texturebitmap[0]==0 && state.texturebitmap[1]==0 && state.texturebitmap[2]==255);
    assert(state.texturebitmap[12]==0 && state.texturebitmap[13]==255 && state.texturebitmap[14]==255);
    assert(state.texturebitmap[16]==48 && state.texturebitmap[17]==48 && state.texturebitmap[18]==48);
    assert(state.texturebitmap[20]==152 && state.texturebitmap[21]==152 && state.texturebitmap[22]==24);
    assert(state.texturebitmap[24]==20 && state.texturebitmap[25]==20 && state.texturebitmap[26]==148);
    assert(state.texturebitmap[40]==40 && state.texturebitmap[41]==40 && state.texturebitmap[42]==40);
    UVCanvasSetTextureOpacity(g_UVCanvas,50);
    assert(state.texturebitmap[2]==152 && state.texturebitmap[1]==24);
    UVCanvasSetTextureOpacity(g_UVCanvas,-10);
    assert(state.textureopacity==0 && state.texturebitmap[2]==48);
    UVCanvasDrawTexture(NULL,&state); assert(!draws);
    UVCanvasSetTextureOpacity(g_UVCanvas,120); assert(state.textureopacity==100);
    assert(state.triangles==triangles && state.draghandle==3 && state.values[0]==.25);
    state.width=240; state.height=180;
    const double cameras[][3]={{.5,.5,60},{-.25,-.75,60},{2.25,3.75,60},
        {.25,.75,4},{.9999,.0001,1000000},{-.0001,1.0001,1000000},
        {-1000000.25,1000000.75,13.25},{.325,.175,87.25}};
    const int opacities[]={1,37,50,100};
    for(unsigned n=0;n<sizeof(cameras)/sizeof(cameras[0]);n++)
    {
        state.centeru=cameras[n][0]; state.centerv=cameras[n][1]; state.pixelsperunit=cameras[n][2];
        for(unsigned a=0;a<sizeof(opacities)/sizeof(opacities[0]);a++)
        {
            UVCanvasSetTextureOpacity(g_UVCanvas,opacities[a]);
            int before=draws; linecount=0;
            UVCanvasDraw(NULL,&state);
            assert(draws==before+1); /* Always one blit, even with 2700 visible tiles. */
            CheckScreenPixels();
        }
    }
    UVCanvasSetTextureOpacity(g_UVCanvas,0);
    int before=draws; UVCanvasDrawTexture(NULL,&state); assert(draws==before);
    state.width=0; UVCanvasSetTextureOpacity(g_UVCanvas,50);
    UVCanvasDrawTexture(NULL,&state); assert(draws==before);
    TexPixel *invalid=malloc(sizeof(*invalid)); assert(invalid);
    assert(!UVCanvasSetTexture(g_UVCanvas,invalid,0,1) && !state.texture && !state.texturebitmap);
}

static void NativeLimits(void)
{
    UVCanvasTriangle *saved=state.triangles;
    int savedcount=state.trianglecount;
    UVCanvasTriangle faces[2]={{.width=32,.height=64},{.width=128,.height=16}};
    state.triangles=faces; state.trianglecount=1;
    state.width=800; state.height=600;
    state.centeru=state.centerv=0; state.pixelsperunit=8;
    linecount=0; UVCanvasDraw(NULL,&state);
    assert(linecount==4);
    assert(lines[0][0].x==144 && lines[0][0].y==428);
    assert(lines[0][1].x==656 && lines[0][1].y==428);
    assert(lines[2][0].x==656 && lines[2][0].y==172);
    /* Mixed texture sizes: intersect U and V independently, without an image. */
    state.trianglecount=2; UVCanvasSetTextureOpacity(g_UVCanvas,0);
    linecount=0; UVCanvasDraw(NULL,&state); assert(linecount==4);
    assert(lines[0][0].x==336 && lines[0][0].y==428);
    assert(lines[0][1].x==464 && lines[2][0].y==172);
    state.trianglecount=1;
    state.centeru=32; state.pixelsperunit=100000;
    linecount=0; UVCanvasDrawLimits(NULL,&state); assert(linecount==1);
    /* Positive maximum is 32767, not 32768. Edges clip, never clamp inward. */
    assert(lines[0][0].x==302 && lines[0][1].x==302);
    assert(lines[0][0].y==599 && lines[0][1].y==0);
    state.centeru=-32;
    linecount=0; UVCanvasDrawLimits(NULL,&state); assert(linecount==1);
    assert(lines[0][0].x==400 && lines[0][1].x==400);
    state.pixelsperunit=1000000;
    const double centers[]={0,32,1000000};
    for(unsigned n=0;n<sizeof(centers)/sizeof(centers[0]);n++)
    {
        state.centeru=centers[n]; linecount=0;
        UVCanvasDrawLimits(NULL,&state); assert(linecount==0);
    }
    state.trianglecount=0;
    linecount=0; UVCanvasDrawLimits(NULL,&state); assert(linecount==0);
    state.triangles=saved; state.trianglecount=savedcount;
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
    Selection(); PixelsAndPlacement(); NativeLimits(); Layout();
    UVCanvasSetTexture(g_UVCanvas,NULL,0,0); free(state.triangles);
    free(screenpixels);
    puts("PASS: shared/mixed/untextured selections, missing/replaced images, RGBA/opacity, dim repeats, V-up orientation, pan/zoom, native limits/clipping, live-drag isolation and layouts.");
    return 0;
}
