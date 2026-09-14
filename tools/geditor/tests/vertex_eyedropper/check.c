#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "edittool.h"
#include "stanload.h"
typedef void *HWND;
typedef unsigned int GLuint, GLenum, UINT;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM, LRESULT;
typedef struct { int x, y; } POINT;
typedef struct { int left, top, right, bottom; } RECT;
#define WM_APP 0x8000
#include "types.inc"
enum { GL_FRONT=1, GL_BACK, GL_FRONT_AND_BACK, IDC_ARROW, IDC_CROSS,
       WM_LBUTTONDOWN, WM_LBUTTONDBLCLK, WM_KEYDOWN, WM_KEYUP,
       WM_CANCELMODE, WM_CAPTURECHANGED, WM_KILLFOCUS, VK_ESCAPE, VK_DELETE,
       MK_SHIFT=1, MK_CONTROL=2 };
#define VIEWPORT_OBJECT_NONE ((DWORD)-1)
#define GET_X_LPARAM(p) ((short)(p))
#define GET_Y_LPARAM(p) ((short)((p) >> 16))
typedef struct ViewportState {
    EditorTool tool;
    ViewportRenderMode rendermode;
    BOOL orbit, flying, vertexsnap, boxpending, contextpending, colorpick, colorsampleclick;
    BOOL cullbackfaces, showbgprimary, showbgsecondary, showobjects, showstan;
    BOOL keyw, keya, keys, keyd, keyq, keye, markerselected;
    int dragaxis, width, height, batchcount, stanopacity;
    DWORD selectedobject;
    float posx, posy, posz, yaw, pitch;
    Vertex *scene;
    SceneBatch *batches;
    BgFaceRef *scenefacerefs;
    unsigned char *hiddentris;
    ViewportTexture *texturecache;
    StanFile stan;
} ViewportState;
static ColorPickerState picker;
static BgDocument g_CurrentBgDocument;
static StanFile g_CurrentStan;
static HWND g_Viewport, g_RightPanel;
static char edittext[4][4];
static unsigned samplecount, paintcount, modechanges, selectioncalls, clears;
static int cursor;
static unsigned char painted[4];
static ViewportBgVertexHit sampled;
static BOOL failrequest;
static ColorPickerState *ColorPickerGetState(HWND hwnd) { return hwnd; }
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void SetWindowText(HWND hwnd, const char *text)
{ assert(picker.updatingtext); snprintf(hwnd, 4, "%s", text); }
static void InvalidateRect(HWND hwnd, const RECT *rect, BOOL erase) {}
static BOOL GetClientRect(HWND hwnd, RECT *rect)
{ ViewportState *s=hwnd; *rect=(RECT){0,0,s->width,s->height}; return TRUE; }
static BOOL GetCursorPos(POINT *p) { *p=(POINT){50,50}; return TRUE; }
static BOOL ScreenToClient(HWND hwnd, POINT *p) { return TRUE; }
static BOOL PtInRect(const RECT *r, POINT p)
{ return p.x>=r->left && p.x<r->right && p.y>=r->top && p.y<r->bottom; }
static int LoadCursor(void *instance, int id) { return id; }
static void SetCursor(int id) { cursor=id; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static void RightPanelGetPaintColor(HWND hwnd, unsigned char rgba[4]);
static void RightPanelSetPaintColor(HWND hwnd, const unsigned char rgba[4]);
static void ViewportEnvironmentAxes(const ViewportState *s, float *r, float *u) { abort(); }
static void ViewportEnvironmentCoordinates(const ViewportState *s, int i, BgRenderFlags f,
                                           const float *r, const float *u, float *uv) { abort(); }
#include "logic.inc"
static void RightPanelGetPaintColor(HWND hwnd, unsigned char rgba[4]) { ColorPickerGetColor(hwnd,rgba); }
static void RightPanelSetPaintColor(HWND hwnd, const unsigned char rgba[4]) { ColorPickerSetColor(hwnd,rgba); }
static LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch(msg)
    {
    case VIEWPORT_WM_SAMPLE_VERTEX:
        samplecount++; sampled=*(const ViewportBgVertexHit *)lparam;
        return !failrequest && GEditorSampleBgVertex(&sampled);
    case VIEWPORT_WM_SAMPLE_STAN: samplecount++; return !failrequest && GEditorSampleStanTile((DWORD)wparam);
    case VIEWPORT_WM_COLOR_PICK_CHANGED: modechanges++; ColorPickerSetSampling(g_RightPanel,wparam!=0); return TRUE;
    case VIEWPORT_WM_PAINT_VERTEX: paintcount++; ColorPickerGetColor(g_RightPanel,painted); return TRUE;
    default: selectioncalls++; return FALSE;
    }
}
/* Unrelated input actions are counted: a sampling click must bypass them. */
static void SetFocus(HWND hwnd) {}
static BOOL ViewportOpenModelAt(HWND h, ViewportState *s, int x, int y, WPARAM w) { selectioncalls++; return FALSE; }
static BOOL ViewportBeginTransform(HWND h, ViewportState *s, int x, int y, BOOL shift) { selectioncalls++; return FALSE; }
static BOOL ViewportTryPickMarker(HWND h, ViewportState *s, int x, int y, BOOL remove) { selectioncalls++; return FALSE; }
static BOOL ViewportTryPickPortal(HWND h, ViewportState *s, int x, int y, BOOL add, BOOL remove) { selectioncalls++; return FALSE; }
static BOOL ViewportTryPickPad(HWND h, ViewportState *s, int x, int y, BOOL remove) { selectioncalls++; return FALSE; }
static BOOL ViewportTryPickStan(HWND h, ViewportState *s, int x, int y, BOOL add, BOOL remove) { selectioncalls++; return FALSE; }
static void ViewportBeginBoxSelection(HWND h, ViewportState *s, int x, int y, BOOL add, BOOL remove) { selectioncalls++; }
static void ViewportPickComponent(HWND h, ViewportState *s, int x, int y, BOOL add, BOOL remove) { selectioncalls++; }
static void ViewportPickAt(HWND h, ViewportState *s, int x, int y, BOOL add, BOOL remove) { selectioncalls++; }
static void ViewportClearAllSelection(ViewportState *s) { clears++; }
static void ViewportCancelTransform(HWND h) { ((ViewportState *)h)->dragaxis=-1; }
static void ViewportEndFly(HWND h, ViewportState *s) { s->flying=FALSE; }
static void ViewportSetKey(ViewportState *s, WPARAM w, LPARAM l, int pressed) {}
static LRESULT Dispatch(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ViewportState *state=hwnd;
    switch(msg) {
#include "dispatch.inc"
    }
    return 0;
}
static void Click(ViewportState *s, UINT message)
{ Dispatch(s,message,0,50 | ((LPARAM)50<<16)); }
static void BrushEquals(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    unsigned char rgba[]={r,g,b,a};
    assert(!memcmp(picker.rgba,rgba,4));
    for(int i=0;i<4;i++) { assert(atoi(edittext[i])==rgba[i] && !picker.invalid[i]); }
    assert(picker.imagedirty && !picker.updatingtext);
    assert(fabs(picker.value-fmax(r,fmax(g,b))/255.0)<1e-12);
}
int main(void)
{
    /* Rendered colors intentionally differ from source, as in fullbright,
     * selection overlays and material-specific preview alpha. */
    Vertex scene[6]={ {.x=-10,.y=-10,.z=100,.r=255,.a=255},
        {.x=10,.y=-10,.z=100,.r=255,.a=255}, {.x=0,.y=10,.z=100,.r=255,.a=255} };
    for(int i=0;i<3;i++) { scene[i+3]=scene[i]; scene[i+3].z=50; }
    BgFaceRef refs[]={ {.room=1,.faceid=42}, {.room=1,.faceid=43,.layer=1} };
    BgDocumentVertex vertices[3]={ {.r=1,.g=2,.b=3,.a=4}, {.r=5,.g=6,.b=7,.a=8}, {.r=37,.g=119,.b=203,.a=73} };
    BgDocumentFace faces[]={ {.id=42,.room=1,.vertexindices={0,1,2}},
                            {.id=43,.room=1,.layer=1,.vertexindices={2,1,0}} };
    BgDocumentRoom rooms[2]={0}; rooms[1]=(BgDocumentRoom){.vertices=vertices,.vertexcount=3,.faces=faces,.facecount=2};
    g_CurrentBgDocument=(BgDocument){.rooms=rooms,.roomcount=1,.facecount=2};
    SceneBatch batches[]={ {.count=3,.renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE},
                           {.first=3,.count=3,.secondary=TRUE,.renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE} };
    unsigned char hidden[2]={0};
    GLubyte alpha=255;
    ViewportTexture texture={.name=1,.width=1,.height=1,.alpha=&alpha};
    ViewportState s={.tool=EDITOR_TOOL_VERTEX_PAINT,.dragaxis=-1,.width=101,.height=101,.yaw=180,
        .scene=scene,.batches=batches,.batchcount=1,.scenefacerefs=refs,.hiddentris=hidden,
        .texturecache=&texture,.showbgprimary=TRUE,.showbgsecondary=TRUE,.showobjects=TRUE};
    g_Viewport=&s; g_RightPanel=&picker;
    for(int i=0;i<4;i++) { picker.channels[i]=edittext[i]; picker.invalid[i]=TRUE; }
    BgDocumentVertex original[3]; memcpy(original,vertices,sizeof(vertices));
    ViewportSetColorPick(&s,TRUE); assert(s.colorpick && picker.sampling && cursor==IDC_CROSS);
    unsigned changes=modechanges;
    ViewportSetColorPick(&s,TRUE); assert(modechanges==changes);
    Click(&s,WM_LBUTTONDOWN);
    assert(samplecount==1 && !s.colorpick && !picker.sampling && cursor==IDC_ARROW);
    assert(!selectioncalls && !paintcount && !clears && sampled.face.faceid==42 && sampled.corner==2);
    BrushEquals(37,119,203,73);
    assert(!memcmp(vertices,original,sizeof(vertices)) && !g_CurrentBgDocument.dirty);
    Click(&s,WM_LBUTTONDBLCLK); assert(!selectioncalls && !paintcount && samplecount==1);
    Click(&s,WM_LBUTTONDOWN); assert(paintcount==1 && !memcmp(painted,picker.rgba,4));
    puts("PASS: exact source RGBA, controls synchronized, one-shot sampling, no selection/edit, double-click cannot paint.");

    /* Every corner, including reordered secondary faces, resolves native IDs. */
    ViewportPickRay ray={.direction={0,0,1},.mindistance=10,.maxdistance=1000};
    ViewportBgVertexHit hit;
    for(int c=0;c<3;c++)
    {
        ray.origin[0]=scene[c].x; ray.origin[1]=scene[c].y;
        assert(ViewportFindPaintTarget(&s,&ray,&hit) && hit.corner==(unsigned)c);
    }
    s.batchcount=2; ViewportSetColorPick(&s,TRUE); Click(&s,WM_LBUTTONDOWN);
    BrushEquals(1,2,3,4); assert(sampled.face.faceid==43 && sampled.corner==2);
    /* Invisible layer, texture holes and objects in front follow render picking. */
    batches[1].renderflags|=BG_RENDER_ALPHA_TEST;
    alpha=0; ViewportSetColorPick(&s,TRUE); Click(&s,WM_LBUTTONDOWN); BrushEquals(37,119,203,73);
    alpha=255; s.showbgsecondary=FALSE; ViewportSetColorPick(&s,TRUE); Click(&s,WM_LBUTTONDOWN); BrushEquals(37,119,203,73);
    s.showbgsecondary=TRUE; batches[1].object=TRUE; refs[1].faceid=BG_FACE_ID_NONE;
    ViewportSetColorPick(&s,TRUE); unsigned samples=samplecount, paints=paintcount, selections=selectioncalls;
    Click(&s,WM_LBUTTONDOWN);
    assert(samplecount==samples && paintcount==paints && selectioncalls==selections && s.colorpick);
    hidden[0]=hidden[1]=1; Click(&s,WM_LBUTTONDOWN);
    assert(samplecount==samples && s.colorpick && selectioncalls==selections);
    hidden[0]=hidden[1]=0;
    assert(ViewportSampleColorAt(&s,&s,-1,-1) && samplecount==samples && s.colorpick);
    Dispatch(&s,WM_KEYDOWN,VK_ESCAPE,0); assert(!s.colorpick && !picker.sampling && !clears);
    puts("PASS: nearest corner, reordered secondary faces, hidden geometry, texture holes, foreground occlusion, miss and Escape.");

    StanTile tile={.pointcount=3,.red=17,.green=102,.blue=238};
    for(int i=0;i<3;i++) { tile.points[i]=(StanPoint){scene[i].x,scene[i].y,25,0}; }
    g_CurrentStan=(StanFile){.tiles=&tile,.tilecount=1}; s.stan=g_CurrentStan;
    s.showstan=TRUE; s.stanopacity=44;
    ViewportSetColorPick(&s,TRUE); Click(&s,WM_LBUTTONDOWN); BrushEquals(17,102,238,73);
    assert(!g_CurrentStan.dirty && tile.red==17 && tile.green==102 && tile.blue==238);
    for(int i=0;i<3;i++) tile.points[i].z=75; /* Behind foreground object. */
    samples=samplecount; ViewportSetColorPick(&s,TRUE); Click(&s,WM_LBUTTONDOWN); assert(samplecount==samples && s.colorpick);
    s.stanopacity=0; s.showobjects=FALSE; Click(&s,WM_LBUTTONDOWN); BrushEquals(37,119,203,73);
    puts("PASS: visible Stan RGB preserves brush alpha; opacity and foreground occlusion respected.");

    samples=samplecount; paints=paintcount;
    hit=(ViewportBgVertexHit){refs[0],3}; assert(!GEditorSampleBgVertex(&hit));
    hit.corner=0; hit.face.faceid=999; assert(!GEditorSampleBgVertex(&hit));
    hit.face=refs[0]; DWORD index=faces[0].vertexindices[0]; faces[0].vertexindices[0]=99;
    assert(!GEditorSampleBgVertex(&hit)); faces[0].vertexindices[0]=index;
    assert(!GEditorSampleBgVertex(NULL) && !GEditorSampleStanTile(STAN_TILE_NONE));
    s.tool=EDITOR_TOOL_FACE_SELECT; assert(!GEditorSampleBgVertex(&hit));
    assert(!GEditorSampleStanTile(0)); ViewportSetColorPick(&s,TRUE); assert(!s.colorpick);
    s.tool=EDITOR_TOOL_VERTEX_PAINT; s.flying=TRUE; ViewportSetColorPick(&s,TRUE); assert(!s.colorpick);
    s.flying=FALSE; s.dragaxis=0; ViewportSetColorPick(&s,TRUE); assert(!s.colorpick);
    s.dragaxis=-1; s.boxpending=TRUE; ViewportSetColorPick(&s,TRUE); assert(!s.colorpick); s.boxpending=FALSE;
    failrequest=TRUE; ViewportSetColorPick(&s,TRUE); Click(&s,WM_LBUTTONDOWN); assert(s.colorpick); failrequest=FALSE;
    assert(paintcount==paints && !memcmp(vertices,original,sizeof(vertices)) && !g_CurrentBgDocument.dirty);
    for(int i=0;i<3;i++)
    {
        UINT cancel[]={WM_CANCELMODE,WM_CAPTURECHANGED,WM_KILLFOCUS};
        ViewportSetColorPick(&s,TRUE); Dispatch(&s,cancel[i],0,0); assert(!s.colorpick && !picker.sampling);
    }
    ColorPickerSetColor(&picker,(unsigned char[]){0,0,0,0}); BrushEquals(0,0,0,0);
    ColorPickerSetColor(&picker,(unsigned char[]){255,255,255,255}); BrushEquals(255,255,255,255);
    assert(isfinite(picker.hue) && isfinite(picker.saturation));
    ColorPickerSetColor(NULL,(unsigned char[]){1,2,3,4}); ColorPickerSetColor(&picker,NULL);
    BrushEquals(255,255,255,255);
    puts("PASS: stale targets, invalid tools, drag guards, failed samples, cancellation and black/white RGBA updates.");
    return 0;
}
