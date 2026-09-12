#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "bgdocument.h"
#include "stanload.h"
#include "edittool.h"

typedef void *HWND;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef intptr_t LPARAM;
#define VIEWPORT_OBJECT_NONE ((DWORD)-1)
#define VIEWPORT_WM_SELECTION_CHANGED 1
#include "types.inc"

typedef struct ViewportState {
    EditorTool tool;
    BOOL orbit, flying, boxpending, showbgprimary, showbgsecondary, gizmovisible;
    int dragaxis, hoveraxis, scenecount, batchcount, selectedtricount;
    int componentcount, componentcapacity, stancomponentcount, stancomponentcapacity;
    Vertex *scene;
    VertexColor *scenecolors;
    BgDocumentVertexRef *scenevertexrefs;
    BgFaceRef *scenefacerefs;
    SceneBatch *batches;
    unsigned char *selectedtris, *hiddentris;
    ViewportComponent *components;
    ViewportStanComponent *stancomponents;
    DWORD selectedobject;
    BOOL padselected;
} ViewportState;

static unsigned notifications;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportUpdateGizmo(ViewportState *state) {}
static void ViewportRefreshStanOverlay(ViewportState *state) {}
static void ViewportClearPadSelection(ViewportState *state) { state->padselected=FALSE; }
static void ViewportClearObjectSelection(ViewportState *state) { state->selectedobject=VIEWPORT_OBJECT_NONE; }
static void ViewportClearStanSelection(ViewportState *state) { state->stancomponentcount=0; }
static void InvalidateRect(HWND hwnd, const void *rect, BOOL erase) {}
static HWND GetParent(HWND hwnd) { return (HWND)1; }
static unsigned command;
static void SendMessage(HWND hwnd, unsigned msg, unsigned wparam, LPARAM lparam)
{ if (msg==VIEWPORT_WM_SELECTION_CHANGED) { notifications++; } else { command=wparam; } }

static int allocations=-1;
static BOOL FailAllocation(void)
{ if (allocations==0) { return TRUE; } if (allocations>0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t size) { return FailAllocation() ? NULL : malloc(size); }
static void *TestCalloc(size_t count, size_t size) { return FailAllocation() ? NULL : calloc(count,size); }
#define malloc TestMalloc
#define calloc TestCalloc
#include "logic.inc"
#undef malloc
#undef calloc

static void Seed(ViewportState *state, EditorTool tool)
{
    ViewportClearAllSelection(state); state->tool=tool;
    if (tool==EDITOR_TOOL_FACE_SELECT)
    { state->selectedtris[0]=1; state->selectedtricount=1; ViewportSetTriangleColor(state,0,TRUE); }
    else
    {
        free(state->components); state->components=calloc(1,sizeof(*state->components));
        assert(state->components); state->componentcount=state->componentcapacity=1;
        state->components[0]=(ViewportComponent){.refs={{1,1},{1,0}},.corners={1,0}};
        if (tool==EDITOR_TOOL_VERTEX_SELECT)
        { state->components[0].refs[1]=state->components[0].refs[0]; state->components[0].corners[1]=1; }
    }
}

static void Geometry(void)
{
    /* Four triangles form a chain. A fifth touches only one vertex. Other
       faces are coincident but disconnected, hidden, objects, or secondary. */
    BgDocumentVertexRef refs[30]={
        {1,0},{1,1},{1,2}, {1,1},{1,3},{1,2}, {1,2},{1,3},{1,4}, {1,3},{1,4},{1,5},
        {1,0},{1,6},{1,7}, {2,0},{2,1},{2,2}, {1,8},{1,9},{1,10},
        {1,4},{1,5},{1,11}, {0,0},{0,1},{0,2}, {1,3},{1,4},{1,12}};
    Vertex vertices[30]={0}; VertexColor colors[30]={0}; BgFaceRef faces[10]={0};
    unsigned char selected[10]={0}, hidden[10]={0}; SceneBatch batches[10]={0};
    ViewportState s={.tool=EDITOR_TOOL_VERTEX_SELECT,.showbgprimary=TRUE,.dragaxis=-1,
        .scene=vertices,.scenecolors=colors,.scenecount=30,.scenevertexrefs=refs,.scenefacerefs=faces,
        .selectedtris=selected,.hiddentris=hidden,.batches=batches,.batchcount=10,
        .selectedobject=VIEWPORT_OBJECT_NONE};
    for (int i=0; i<10; i++)
    { faces[i].faceid=i+1; batches[i].first=i*3; batches[i].count=3; }
    for (int i=0; i<30; i++) { vertices[i].z=1000000; } /* Beyond camera bounds. */
    hidden[7]=1; faces[8].faceid=BG_FACE_ID_NONE; batches[8].object=TRUE; batches[9].secondary=TRUE;
    for (int tool=EDITOR_TOOL_VERTEX_SELECT; tool<=EDITOR_TOOL_FACE_SELECT; tool++)
    {
        const int counts[]={14,18,7};
        ViewportClearAllSelection(&s); s.tool=tool; s.selectedobject=5; s.padselected=TRUE;
        assert(!ViewportCanSelectBackground(&s,TRUE) && ViewportCanSelectBackground(&s,FALSE));
        unsigned before=notifications;
        assert(ViewportSelectBackground(&s,FALSE));
        assert(notifications==before+1 && s.selectedobject==VIEWPORT_OBJECT_NONE && !s.padselected);
        assert((tool==EDITOR_TOOL_FACE_SELECT?s.selectedtricount:s.componentcount)==counts[tool]);
        assert(ViewportSelectBackground(&s,FALSE)); /* No duplicates on repetition. */
        assert((tool==EDITOR_TOOL_FACE_SELECT?s.selectedtricount:s.componentcount)==counts[tool]);
        if (tool==EDITOR_TOOL_FACE_SELECT) { assert(!selected[7] && !selected[8] && !selected[9]); }
        else
        {
            for (int i=0; i<s.componentcount; i++)
            {
                const ViewportComponent *c=&s.components[i];
                for (int end=0; end<(tool==EDITOR_TOOL_EDGE_SELECT?2:1); end++)
                {
                    assert(!memcmp(&c->refs[end],&refs[c->corners[end]],sizeof(c->refs[end])));
                    assert(c->refs[end].room && c->refs[end].index!=11 && c->refs[end].index!=12);
                }
            }
        }
    }
    Seed(&s,EDITOR_TOOL_VERTEX_SELECT);
    assert(ViewportSelectBackground(&s,TRUE) && s.componentcount==4);
    assert(ViewportSelectBackground(&s,TRUE) && s.componentcount==8);
    assert(ViewportSelectBackground(&s,TRUE) && s.componentcount==8);
    Seed(&s,EDITOR_TOOL_EDGE_SELECT);
    assert(ViewportSelectBackground(&s,TRUE) && s.componentcount==6);
    assert(ViewportSelectBackground(&s,TRUE) && s.componentcount==11);
    Seed(&s,EDITOR_TOOL_FACE_SELECT);
    for (int count=2; count<=4; count++)
    { assert(ViewportSelectBackground(&s,TRUE) && s.selectedtricount==count); }
    assert(ViewportSelectBackground(&s,TRUE) && s.selectedtricount==4 && !selected[4]);
    s.showbgsecondary=TRUE;
    assert(ViewportSelectBackground(&s,TRUE) && s.selectedtricount==5 && selected[9]);
    s.tool=EDITOR_TOOL_EDGE_SELECT;
    assert(ViewportSelectBackground(&s,FALSE) && s.componentcount==20);
    /* A shared source vertex still grows if its cached corner is on a hidden
       face but another visible face exposes it. Hidden-only endpoints don't. */
    Seed(&s,EDITOR_TOOL_VERTEX_SELECT); hidden[0]=1;
    assert(ViewportSelectBackground(&s,TRUE) && s.componentcount==3);
    hidden[0]=0; s.showbgsecondary=FALSE;
    /* Every allocation failure must preserve selection and send no history notification. */
    for (int budget=0; budget<7; budget++)
    {
        Seed(&s,EDITOR_TOOL_EDGE_SELECT);
        ViewportComponent old=s.components[0]; unsigned before=notifications;
        allocations=budget; BOOL ok=ViewportSelectBackground(&s,TRUE); allocations=-1;
        if (!ok) { assert(s.componentcount==1 && !memcmp(&old,s.components,sizeof(old)) && notifications==before); }
        else { assert(s.componentcount==6 && notifications==before+1); }
    }
    Seed(&s,EDITOR_TOOL_FACE_SELECT);
    for (int budget=0; budget<3; budget++)
    {
        unsigned before=notifications; allocations=budget;
        assert(!ViewportSelectBackground(&s,TRUE)); allocations=-1;
        assert(s.selectedtricount==1 && selected[0] && !selected[1] && notifications==before);
    }
    s.flying=TRUE; assert(!ViewportCanSelectBackground(&s,FALSE)); s.flying=FALSE;
    s.orbit=TRUE; assert(!ViewportCanSelectBackground(&s,FALSE)); s.orbit=FALSE;
    s.dragaxis=0; assert(!ViewportCanSelectBackground(&s,TRUE)); s.dragaxis=-1;
    s.boxpending=TRUE; assert(!ViewportCanSelectBackground(&s,FALSE)); s.boxpending=FALSE;
    s.tool=EDITOR_TOOL_VERTEX_PAINT; assert(!ViewportCanSelectBackground(&s,FALSE));
    free(s.components); free(s.stancomponents);
    puts("PASS: per-mode counts, one-ring growth, hidden/layer filters, source identity, and atomic failure.");
}

static void SameMaterial(void)
{
    Vertex vertices[30]={0}; VertexColor colors[30]={0};
    BgDocumentVertexRef refs[30]={0}; BgFaceRef faces[10]={0};
    unsigned char selected[10]={0}, hidden[10]={0};
    SceneBatch batches[]={
        {.first=0, .count=6, .textureid=17}, /* Two faces in one draw batch. */
        {.first=6, .count=3, .textureid=23},
        {.first=9, .count=3, .textureid=17, .secondary=TRUE, .renderflags=BG_RENDER_BLEND},
        {.first=12, .count=3, .textureid=17, .cullbackfaces=TRUE},
        {.first=15, .count=3, .textureid=17},
        {.first=18, .count=3, .textureid=17, .object=TRUE},
        {.first=21, .count=6, .textureid=BG_TEX_NONE},
        {.first=27, .count=3, .textureid=17}
    };
    ViewportState s={.tool=EDITOR_TOOL_FACE_SELECT,.showbgprimary=TRUE,.dragaxis=-1,
        .scene=vertices,.scenecolors=colors,.scenecount=30,.scenevertexrefs=refs,.scenefacerefs=faces,
        .selectedtris=selected,.hiddentris=hidden,.batches=batches,.batchcount=8,
        .selectedobject=VIEWPORT_OBJECT_NONE};
    for (int i=0; i<10; i++) { faces[i].faceid=i+1; faces[i].room=i/3+1; }
    for (int i=0; i<30; i++) { vertices[i].z=1000000; } /* Includes off-screen geometry. */
    hidden[5]=1; faces[6].faceid=faces[9].faceid=BG_FACE_ID_NONE;
    unsigned before=notifications;
    assert(!ViewportCanSelectSameMaterial(NULL) && !ViewportCanSelectSameMaterial(&s));
    ViewportSelectSameMaterial(&s); assert(notifications==before && !s.selectedtricount);
    Seed(&s,EDITOR_TOOL_FACE_SELECT);
    assert(ViewportCanSelectSameMaterial(&s));
    ViewportSelectSameMaterial(&s);
    assert(s.selectedtricount==3 && selected[0] && selected[1] && selected[4]);
    assert(!selected[2] && !selected[3] && !selected[5] && !selected[6] && !selected[9]);
    assert(notifications==before+1); /* Standard selection/history notification. */
    assert(ViewportCanSelectSameMaterial(&s)); /* Multiple faces, one texture. */
    ViewportSelectSameMaterial(&s); assert(s.selectedtricount==3); /* No duplicates. */
    selected[2]=1; s.selectedtricount++;
    before=notifications;
    assert(!ViewportCanSelectSameMaterial(&s));
    ViewportSelectSameMaterial(&s);
    assert(s.selectedtricount==4 && selected[2] && notifications==before);
    selected[2]=0; s.selectedtricount--;
    /* Hidden layer selections still count when checking for mixed textures. */
    selected[3]=1; s.selectedtricount++; batches[2].textureid=23;
    assert(!ViewportCanSelectSameMaterial(&s));
    batches[2].textureid=17; assert(ViewportCanSelectSameMaterial(&s));
    s.showbgsecondary=TRUE;
    ViewportSelectSameMaterial(&s);
    assert(s.selectedtricount==4 && selected[3]); /* Different render flags still match. */
    s.showbgsecondary=FALSE;
    ViewportSelectSameMaterial(&s); assert(s.selectedtricount==3 && !selected[3]);
    ViewportClearAllSelection(&s); selected[7]=1; s.selectedtricount=1;
    assert(ViewportCanSelectSameMaterial(&s));
    ViewportSelectSameMaterial(&s); assert(s.selectedtricount==2 && selected[7] && selected[8]);
    Seed(&s,EDITOR_TOOL_FACE_SELECT);
    s.flying=TRUE; assert(!ViewportCanSelectSameMaterial(&s)); s.flying=FALSE;
    s.orbit=TRUE; assert(!ViewportCanSelectSameMaterial(&s)); s.orbit=FALSE;
    s.dragaxis=0; assert(!ViewportCanSelectSameMaterial(&s)); s.dragaxis=-1;
    s.boxpending=TRUE; assert(!ViewportCanSelectSameMaterial(&s)); s.boxpending=FALSE;
    s.showbgprimary=FALSE; assert(!ViewportCanSelectSameMaterial(&s)); s.showbgprimary=TRUE;
    s.tool=EDITOR_TOOL_VERTEX_SELECT; assert(!ViewportCanSelectSameMaterial(&s));
    s.tool=EDITOR_TOOL_EDGE_SELECT; assert(!ViewportCanSelectSameMaterial(&s));
    s.tool=EDITOR_TOOL_VERTEX_PAINT; assert(!ViewportCanSelectSameMaterial(&s));
    s.tool=EDITOR_TOOL_FACE_SELECT; ViewportClearAllSelection(&s); s.selectedobject=0;
    assert(!ViewportCanSelectSameMaterial(&s));
    puts("PASS: same-material selection, mixed-texture rejection, room/batch/layer boundaries, untextured faces and input guards.");
}

typedef struct { HWND hwnd; unsigned message, wParam; LPARAM lParam; } MSG;
#define WM_KEYDOWN 2
#define WM_COMMAND 3
#define VK_CONTROL 0
#define VK_MENU 1
#define VK_SHIFT 2
#define ID_SELECT_ALL 10
#define ID_SELECT_GROW 11
static HWND g_Viewport=(HWND)2;
static int keys[3]; static BOOL flying;
static const char *classname="Viewport";
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL IsChild(HWND frame, HWND hwnd) { return hwnd==(HWND)2 || hwnd==(HWND)3; }
static int GetKeyState(int key) { return keys[key]; }
static void GetClassName(HWND hwnd, char *out, int size) { snprintf(out,size,"%s",classname); }
#define lstrcmpi strcasecmp
#include "hotkeys.inc"

static void Hotkeys(void)
{
    MSG msg={.hwnd=(HWND)2,.message=WM_KEYDOWN,.wParam='Q'};
    assert(GEditorHandleSelectionHotkey((HWND)1,&msg) && command==ID_SELECT_GROW);
    command=0; msg.lParam=(LPARAM)1<<30;
    assert(GEditorHandleSelectionHotkey((HWND)1,&msg) && !command); msg.lParam=0;
    flying=TRUE; assert(!GEditorHandleSelectionHotkey((HWND)1,&msg)); flying=FALSE;
    keys[VK_CONTROL]=0x8000; assert(!GEditorHandleSelectionHotkey((HWND)1,&msg));
    msg.wParam='A'; assert(GEditorHandleSelectionHotkey((HWND)1,&msg) && command==ID_SELECT_ALL);
    keys[VK_SHIFT]=0x8000; assert(!GEditorHandleSelectionHotkey((HWND)1,&msg)); keys[VK_SHIFT]=0;
    keys[VK_MENU]=0x8000; assert(!GEditorHandleSelectionHotkey((HWND)1,&msg)); keys[VK_MENU]=0;
    const char *inputs[]={"Edit","ComboBox","ComboLBox"};
    for (unsigned i=0; i<3; i++) { classname=inputs[i]; assert(!GEditorHandleSelectionHotkey((HWND)1,&msg)); }
    classname="Viewport"; msg.hwnd=(HWND)4; assert(!GEditorHandleSelectionHotkey((HWND)1,&msg));
    msg.hwnd=(HWND)2; keys[VK_CONTROL]=0; assert(!GEditorHandleSelectionHotkey((HWND)1,&msg));
    puts("PASS: Q/Ctrl+A routing, repeat suppression, text fields, camera flight and window scope.");
}

int main(void) { Geometry(); SameMaterial(); Hotkeys(); return 0; }
