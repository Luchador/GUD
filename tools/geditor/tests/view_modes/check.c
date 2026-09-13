#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "bgload.h"
#include "bgrender.h"
#include "edittool.h"

typedef void *HWND;
typedef unsigned int GLuint, GLenum;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef intptr_t LPARAM;
#include "types.inc"

enum {
    GL_FRONT=1, GL_BACK, GL_FRONT_AND_BACK, GL_CULL_FACE, GL_TRIANGLES,
    GL_TEXTURE_2D, GL_ALPHA_TEST, GL_BLEND, GL_POINT_SMOOTH, GL_LINE_SMOOTH,
    GL_COLOR_ARRAY, GL_TEXTURE_COORD_ARRAY, GL_VERTEX_ARRAY, GL_FLOAT,
    GL_DEPTH_TEST, GL_LEQUAL, GL_POINT, GL_LINE, GL_POLYGON_OFFSET_POINT,
    GL_POLYGON_OFFSET_LINE, GL_UNSIGNED_BYTE, GL_FILL
};
#define GL_CURRENT_BIT 1
#define GL_ENABLE_BIT 2
#define GL_POLYGON_BIT 4
#define GL_POINT_BIT 8
#define GL_LINE_BIT 16
#define GL_DEPTH_BUFFER_BIT 32
#define GL_CLIENT_VERTEX_ARRAY_BIT 64
#define GL_FALSE 0

typedef struct ViewportState {
    ViewportRenderMode rendermode;
    EditorTool tool;
    BOOL orbit, cullbackfaces, showbgprimary, showbgsecondary, showobjects;
    int batchcount, bghiddentris;
    Vertex *scene;
    VertexColor *scenecolors;
    SceneBatch *batches;
    ViewportTexture *texturecache;
    unsigned char *hiddentris;
} ViewportState;

static unsigned redraws;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportRedraw(HWND hwnd) { redraws++; }
static void ViewportEnvironmentAxes(const ViewportState *s, float *r, float *u) { assert(0); }
static void ViewportEnvironmentCoordinates(const ViewportState *s, int i, BgRenderFlags f,
                                           const float *r, const float *u, float *uv) { assert(0); }

/* A small GL recorder makes draw membership and restoration observable. */
typedef struct ServerState {
    BOOL enabled[32], depthwrite;
    GLenum polygon, cull, depthfunc;
    GLubyte color[4];
    float pointsize, linewidth, offset[2];
} ServerState;
typedef struct ClientState {
    BOOL enabled[32];
    const void *vertices, *colors;
    int colorstride;
} ClientState;
static ServerState server, savedserver;
static ClientState client, savedclient;
static unsigned pushes, clientpushes, drawcount, previews;
static struct Draw { int first, count; GLenum polygon, cull; } draws[32];
static void glPushAttrib(unsigned flags)
{ assert(flags & GL_DEPTH_BUFFER_BIT); assert(flags & GL_POLYGON_BIT); assert(!pushes++); savedserver=server; }
static void glPopAttrib(void) { assert(pushes--==1); server=savedserver; }
static void glPushClientAttrib(unsigned flags) { assert(!clientpushes++); savedclient=client; }
static void glPopClientAttrib(void) { assert(clientpushes--==1); client=savedclient; }
static void glEnable(GLenum cap) { server.enabled[cap]=TRUE; }
static void glDisable(GLenum cap) { server.enabled[cap]=FALSE; }
static void glEnableClientState(GLenum cap) { client.enabled[cap]=TRUE; }
static void glDisableClientState(GLenum cap) { client.enabled[cap]=FALSE; }
static void glCullFace(GLenum face) { server.cull=face; }
static void glPolygonMode(GLenum face, GLenum mode) { server.polygon=mode; }
static void glDepthFunc(GLenum func) { server.depthfunc=func; }
static void glDepthMask(BOOL mask) { server.depthwrite=mask; }
static void glPointSize(float size) { server.pointsize=size; }
static void glLineWidth(float size) { server.linewidth=size; }
static void glPolygonOffset(float factor, float units) { server.offset[0]=factor; server.offset[1]=units; }
static void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{ server.color[0]=r; server.color[1]=g; server.color[2]=b; server.color[3]=a; }
static void glVertexPointer(int size, GLenum type, int stride, const void *pointer)
{ assert(size==3 && stride==sizeof(Vertex)); client.vertices=pointer; }
static void glColorPointer(int size, GLenum type, int stride, const void *pointer)
{ assert(size==4 && type==GL_UNSIGNED_BYTE); client.colors=pointer; client.colorstride=stride; }
static void glDrawArrays(GLenum mode, int first, int count)
{
    assert(drawcount<32 && mode==GL_TRIANGLES);
    assert(!server.enabled[GL_TEXTURE_2D] && !client.enabled[GL_COLOR_ARRAY]);
    assert(!server.enabled[GL_BLEND] && !server.enabled[GL_ALPHA_TEST]);
    assert(server.enabled[GL_DEPTH_TEST] && !server.depthwrite && server.depthfunc==GL_LEQUAL);
    draws[drawcount++]=(struct Draw){first,count,server.polygon,server.enabled[GL_CULL_FACE]?server.cull:0};
}
static void ViewportDrawExtrusionBatch(const ViewportState *s, const SceneBatch *b) { previews++; }
#include "logic.inc"

static void ColorsAndSwitching(void)
{
    Vertex vertices[3]={{.r=23,.g=45,.b=67,.a=0}, {.r=89,.g=101,.b=123,.a=97},
                        {.r=145,.g=167,.b=189,.a=255}};
    VertexColor colors[3]={{23,45,67},{89,101,123},{145,167,189}};
    ViewportState s={.scene=vertices,.scenecolors=colors};
    Vertex original[3]; memcpy(original,vertices,sizeof(original));
    assert(ViewportGetRenderMode(&s)==VIEWPORT_RENDER_NORMAL);
    assert(ViewportGetRenderMode(NULL)==VIEWPORT_RENDER_NORMAL);
    for (int i=0; i<3; i++)
    {
        ViewportSetFullbrightColor(&vertices[i],FALSE);
        assert(vertices[i].fullbright[0]==255 && vertices[i].fullbright[1]==255
            && vertices[i].fullbright[2]==255 && vertices[i].fullbright[3]==original[i].a);
        assert(!memcmp(&vertices[i].r,&original[i].r,4));
    }
    for (int mode=VIEWPORT_RENDER_WIREFRAME; mode<=VIEWPORT_RENDER_UNTEXTURED; mode++)
    {
        ViewportSetRenderMode(&s,(ViewportRenderMode)mode);
        unsigned before=redraws;
        assert(ViewportGetRenderMode(&s)==(ViewportRenderMode)mode);
        ViewportSetRenderMode(&s,(ViewportRenderMode)mode); assert(redraws==before);
        ViewportSceneColorPointer(&s,vertices);
        assert(client.colors==(mode==VIEWPORT_RENDER_FULLBRIGHT ? vertices[0].fullbright : &vertices[0].r));
        assert(client.colorstride==sizeof(Vertex));
        for (int i=0; i<3; i++) { assert(!memcmp(&vertices[i].r,&original[i].r,4)); }
    }
    unsigned before=redraws;
    ViewportSetRenderMode(&s,(ViewportRenderMode)-1);
    ViewportSetRenderMode(&s,(ViewportRenderMode)4);
    ViewportSetRenderMode(NULL,VIEWPORT_RENDER_NORMAL);
    s.orbit=TRUE; ViewportSetRenderMode(&s,VIEWPORT_RENDER_NORMAL); s.orbit=FALSE;
    assert(redraws==before && s.rendermode==VIEWPORT_RENDER_UNTEXTURED);
    ViewportSetTriangleColor(&s,0,TRUE);
    for (int i=0; i<3; i++)
    { assert(vertices[i].fullbright[0]==0 && vertices[i].fullbright[1]==255 && vertices[i].fullbright[3]==original[i].a); }
    ViewportSetTriangleColor(&s,0,FALSE);
    ViewportSetRenderMode(&s,VIEWPORT_RENDER_NORMAL);
    ViewportSceneColorPointer(&s,vertices);
    assert(client.colors==&vertices[0].r);
    for (int i=0; i<3; i++) { assert(!memcmp(&vertices[i].r,&original[i].r,4)); }
    /* Editing vertex alpha refreshes the display alpha too. */
    vertices[1].a=42; ViewportSetTriangleColor(&s,0,FALSE);
    assert(vertices[1].fullbright[3]==42 && vertices[1].fullbright[0]==255);
    puts("PASS: display colors preserve authored RGB/alpha, selection and mode switching.");
}

static void Picking(void)
{
    Vertex vertices[3]={{.x=-1,.y=-1,.z=10,.a=255}, {.x=1,.y=-1,.z=10,.a=255},
                        {.x=0,.y=1,.z=10,.a=255}};
    GLubyte alpha=0;
    ViewportTexture texture={.name=1,.width=1,.height=1,.alpha=&alpha};
    SceneBatch batch={.count=3,.renderflags=BG_RENDER_ALPHA_TEST};
    unsigned char hidden=0;
    ViewportState s={.scene=vertices,.texturecache=&texture,.hiddentris=&hidden};
    ViewportPickRay ray={.direction={0,0,1},.mindistance=0,.maxdistance=100};
    double distance;
    for (int mode=VIEWPORT_RENDER_NORMAL; mode<=VIEWPORT_RENDER_UNTEXTURED; mode++)
    {
        s.rendermode=(ViewportRenderMode)mode;
        assert(ViewportRayBatchTriangleDistance(&s,&batch,&ray,0,&distance)==(mode==VIEWPORT_RENDER_UNTEXTURED));
    }
    assert(fabs(distance-10)<1e-6);
    hidden=1; assert(!ViewportRayBatchTriangleDistance(&s,&batch,&ray,0,&distance)); hidden=0;
    for (int i=0; i<3; i++) { vertices[i].a=0; }
    assert(!ViewportRayBatchTriangleDistance(&s,&batch,&ray,0,&distance));
    batch.renderflags=BG_RENDER_BLEND;
    assert(!ViewportRayBatchTriangleDistance(&s,&batch,&ray,0,&distance));
    for (int i=0; i<3; i++) { vertices[i].a=80; }
    assert(ViewportRayBatchTriangleDistance(&s,&batch,&ray,0,&distance));
    s.rendermode=VIEWPORT_RENDER_NORMAL;
    assert(!ViewportRayBatchTriangleDistance(&s,&batch,&ray,0,&distance));
    alpha=255; assert(ViewportRayBatchTriangleDistance(&s,&batch,&ray,0,&distance));
    puts("PASS: untextured picking ignores texture holes, preserves vertex transparency/hidden faces.");
}

static void Overlay(ViewportState *s, unsigned expected)
{
    ServerState before=server; ClientState clientbefore=client;
    drawcount=previews=0; ViewportDrawBgToolOverlay(s);
    assert(drawcount==expected && !pushes && !clientpushes);
    assert(!memcmp(&server,&before,sizeof(server)) && !memcmp(&client,&clientbefore,sizeof(client)));
}

static void Wireframe(void)
{
    Vertex vertices[15]={0}; unsigned char hidden[5]={0,1,0,0,0};
    SceneBatch batches[]={
        {.first=0,.count=6,.cullbackfaces=TRUE}, /* Primary: one hidden triangle. */
        {.first=6,.count=3,.secondary=TRUE},
        {.first=9,.count=3,.object=TRUE,.renderflags=BG_RENDER_CULL_EXPLICIT|BG_RENDER_CULL_FRONT},
        {.first=12,.count=3,.object=TRUE}}; /* A character uses the same object draw path. */
    ViewportState s={.scene=vertices,.batches=batches,.batchcount=4,.tool=EDITOR_TOOL_FACE_SELECT,
        .showobjects=TRUE,.showbgprimary=TRUE,.showbgsecondary=TRUE,.cullbackfaces=TRUE,
        .hiddentris=hidden,.bghiddentris=1};
    server.polygon=GL_FILL; server.depthwrite=TRUE; server.enabled[GL_TEXTURE_2D]=TRUE;
    client.enabled[GL_COLOR_ARRAY]=TRUE;
    Overlay(&s,0);
    s.rendermode=VIEWPORT_RENDER_WIREFRAME; Overlay(&s,4);
    assert(previews==4);
    for (unsigned i=0; i<4; i++)
    { assert(draws[i].polygon==GL_LINE && draws[i].count==3 && draws[i].first==batches[i].first); }
    assert(draws[0].cull==GL_BACK && draws[2].cull==GL_FRONT);
    s.tool=EDITOR_TOOL_EDGE_SELECT; Overlay(&s,4); /* No duplicate BG wire pass. */
    s.rendermode=VIEWPORT_RENDER_NORMAL; Overlay(&s,2); assert(!previews);
    s.rendermode=VIEWPORT_RENDER_WIREFRAME; s.tool=EDITOR_TOOL_VERTEX_SELECT; Overlay(&s,6);
    assert(draws[4].polygon==GL_POINT && draws[5].polygon==GL_POINT && previews==4);
    s.tool=EDITOR_TOOL_FACE_SELECT; s.showobjects=FALSE; Overlay(&s,2);
    s.showbgsecondary=FALSE; Overlay(&s,1);
    s.showbgprimary=FALSE; Overlay(&s,0);
    puts("PASS: wireframe covers BG/objects once, respects visibility/culling and restores GL state.");
}

typedef struct MSG { HWND hwnd; unsigned message; uintptr_t wParam; LPARAM lParam; } MSG;
enum { WM_KEYDOWN=256, WM_COMMAND=273, VK_CONTROL=17, VK_MENU=18, VK_SHIFT=16, ID_VIEW_RENDER_NORMAL=100 };
static HWND g_Viewport=(HWND)2;
static const char *classname="GEditorViewport";
static BOOL control=TRUE, alt, shift;
static unsigned command, sent;
static BOOL IsChild(HWND parent, HWND child) { return parent==(HWND)1 && child==(HWND)2; }
static short GetKeyState(int key) { return (key==VK_CONTROL?control:key==VK_MENU?alt:shift)?(short)0x8000:0; }
static void GetClassName(HWND hwnd, char *out, int size) { snprintf(out,size,"%s",classname); }
#define lstrcmpi strcasecmp
static void SendMessage(HWND hwnd, unsigned message, uintptr_t wparam, LPARAM lparam)
{ assert(hwnd==(HWND)1 && message==WM_COMMAND); command=(unsigned)wparam; sent++; }
#include "hotkeys.inc"

static void Hotkeys(void)
{
    MSG msg={.hwnd=g_Viewport,.message=WM_KEYDOWN};
    for (unsigned key='1'; key<='4'; key++)
    {
        msg.wParam=key;
        assert(GEditorHandleRenderModeHotkey((HWND)1,&msg));
        assert(command==ID_VIEW_RENDER_NORMAL+key-'1');
    }
    unsigned before=sent;
    msg.lParam=(LPARAM)1<<30; assert(GEditorHandleRenderModeHotkey((HWND)1,&msg) && sent==before); msg.lParam=0;
    const char *inputs[]={"Edit","ComboBox","ComboLBox"};
    for (unsigned i=0; i<3; i++)
    { classname=inputs[i]; assert(!GEditorHandleRenderModeHotkey((HWND)1,&msg)); }
    classname="GEditorViewport";
    msg.hwnd=(HWND)3; assert(!GEditorHandleRenderModeHotkey((HWND)1,&msg)); msg.hwnd=g_Viewport;
    control=FALSE; assert(!GEditorHandleRenderModeHotkey((HWND)1,&msg)); control=TRUE;
    alt=TRUE; assert(!GEditorHandleRenderModeHotkey((HWND)1,&msg)); alt=FALSE;
    shift=TRUE; assert(!GEditorHandleRenderModeHotkey((HWND)1,&msg)); shift=FALSE;
    msg.wParam='5'; assert(!GEditorHandleRenderModeHotkey((HWND)1,&msg));
    assert(!GEditorHandleRenderModeHotkey((HWND)1,NULL) && sent==before);
    puts("PASS: Ctrl+1..4 routing, repeat suppression, text controls and floating editor isolation.");
}

int main(void)
{
    ColorsAndSwitching(); Picking(); Wireframe(); Hotkeys();
    return 0;
}
