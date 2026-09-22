#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "stanload.h"
#include "edittool.h"

typedef void *HWND;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
#define VIEWPORT_OBJECT_NONE ((DWORD)-1)
#define VIEWPORT_WM_SELECTION_CHANGED 1
#define MB_OK 0
#define MB_ICONERROR 1
#define TEXT(value) value
#include "types.inc"

typedef struct ViewportState {
    EditorTool tool;
    BOOL showbgprimary, showbgsecondary, gizmovisible, vertexsnap;
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
    StanFile stan;
    unsigned char *stanselected;
    DWORD *stanpointmap, *stanhiddenids, stanhiddencount;
    BOOL showstan, showportals, padselected;
    int stanopacity;
    BgPortalFile portals;
    unsigned char portalselection[BG_MAX_PORTALS];
    DWORD selectedportal, selectedobject;
} ViewportState;

static unsigned notifications, errors, gizmos;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static void ViewportUpdateGizmo(ViewportState *state) { gizmos++; }
static void ViewportRefreshStanOverlay(ViewportState *state) {}
static void ViewportRefreshPortalColors(ViewportState *state) {}
static void ViewportClearPadSelection(ViewportState *state)
{
    state->padselected=FALSE; state->selectedportal=BG_PORTAL_INDEX_NONE;
    memset(state->portalselection,0,sizeof(state->portalselection));
}
static void ViewportClearObjectSelection(ViewportState *state) { state->selectedobject=VIEWPORT_OBJECT_NONE; }
static void ViewportSetColorPick(HWND hwnd, BOOL enabled) {}
static void ViewportCancelTransform(HWND hwnd)
{
    ViewportState *s=hwnd;
    /* Cancellation must see the old selection tool, before conversion. */
    if (s->dragaxis>=0) { assert(s->tool==EDITOR_TOOL_FACE_SELECT && s->selectedtricount==1); }
    s->dragaxis=-1;
}
static void ViewportRefreshCursor(HWND hwnd, ViewportState *state) {}
static void ViewportRedraw(HWND hwnd) {}
static HWND GetParent(HWND hwnd) { return (HWND)1; }
static void SendMessage(HWND hwnd, unsigned msg, unsigned wparam, intptr_t lparam)
{ assert(msg==VIEWPORT_WM_SELECTION_CHANGED); notifications++; }
static void MessageBox(HWND hwnd, const char *message, const char *title, unsigned flags) { errors++; }

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

static void Change(ViewportState *s, EditorTool tool)
{
    unsigned before=notifications, beforegizmo=gizmos;
    ViewportSetTool(s,tool);
    assert(s->tool==tool && notifications==before+1 && gizmos==beforegizmo+1 && !s->vertexsnap);
}

static void Face(ViewportState *s)
{
    ViewportClearAllSelection(s); s->tool=EDITOR_TOOL_FACE_SELECT;
    s->selectedtris[0]=1; s->selectedtricount=1;
    ViewportSetTriangleColor(s,0,TRUE);
}

static void Background(void)
{
    /* Two adjacent triangles, a coincident room, a disconnected seam,
       a hidden duplicate, a secondary duplicate and an object batch. */
    BgDocumentVertexRef refs[]={
        {1,0},{1,1},{1,2}, {1,2},{1,1},{1,3}, {2,0},{2,1},{2,2},
        {1,4},{1,5},{1,6}, {1,0},{1,1},{1,2}, {1,0},{1,1},{1,2}, {1,0},{1,1},{1,2}};
    Vertex vertices[21]={0}; VertexColor colors[21]={0}; BgFaceRef faces[7]={0};
    unsigned char selected[7]={0}, hidden[7]={0}; SceneBatch batches[7]={0};
    ViewportState s={.tool=EDITOR_TOOL_FACE_SELECT,.showbgprimary=TRUE,.dragaxis=-1,
        .scene=vertices,.scenecolors=colors,.scenecount=21,.scenevertexrefs=refs,.scenefacerefs=faces,
        .selectedtris=selected,.hiddentris=hidden,.batches=batches,.batchcount=7};
    for (int i=0; i<7; i++)
    { faces[i].faceid=i+1; faces[i].room=refs[i*3].room; batches[i].first=i*3; batches[i].count=3; }
    for (int i=0; i<21; i++) { colors[i]=(VertexColor){50,60,70}; }
    hidden[4]=1; batches[5].secondary=TRUE; batches[6].object=TRUE;
    Face(&s); s.dragaxis=0; s.vertexsnap=TRUE;
    Change(&s,EDITOR_TOOL_VERTEX_SELECT);
    assert(s.componentcount==3 && !s.selectedtricount && vertices[0].r==50 && s.dragaxis==-1);
    for (int i=0; i<3; i++)
    {
        assert(s.components[i].refs[0].room==1 && s.components[i].refs[0].index==(DWORD)i);
        for (int e=0; e<2; e++)
            assert(!memcmp(&s.components[i].refs[e],&refs[s.components[i].corners[e]],sizeof(refs[0])));
    }
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.componentcount==3);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.selectedtricount==1 && selected[0] && vertices[0].r!=50);
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.componentcount==3);
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.componentcount==3);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.selectedtricount==1 && selected[0]);
    /* Shared edge/vertices are selected once, in either direction. */
    selected[1]=1; s.selectedtricount++;
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.componentcount==5);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.selectedtricount==2);
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.componentcount==4);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.selectedtricount==2);
    Face(&s); Change(&s,EDITOR_TOOL_VERTEX_SELECT); s.componentcount=2;
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.componentcount==1);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(!s.selectedtricount);
    Face(&s); Change(&s,EDITOR_TOOL_EDGE_SELECT); s.componentcount=2;
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(!s.selectedtricount); /* All 3 endpoints, missing edge. */
    Face(&s); Change(&s,EDITOR_TOOL_VERTEX_SELECT); s.showbgsecondary=TRUE;
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.selectedtricount==2 && selected[0] && selected[5]);
    s.showbgsecondary=FALSE;
    /* Cached corners may be hidden; identity must still find visible shared corners. */
    Face(&s); Change(&s,EDITOR_TOOL_VERTEX_SELECT); hidden[0]=1;
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.componentcount==1);
    for (int e=0; e<2; e++) { assert(s.components[0].corners[e]/3==1); }
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.componentcount==2);
    hidden[0]=0;
    Face(&s); Change(&s,EDITOR_TOOL_VERTEX_PAINT); assert(!s.selectedtricount && !s.componentcount);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(!s.selectedtricount);
    unsigned before=notifications;
    ViewportSetTool(&s,EDITOR_TOOL_FACE_SELECT); ViewportSetTool(&s,EDITOR_TOOL_COUNT); ViewportSetTool(NULL,0);
    assert(notifications==before);
    s.selectedobject=4; s.padselected=TRUE;
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(!s.padselected && s.selectedobject==VIEWPORT_OBJECT_NONE);
    free(s.components); free(s.stancomponents);
    puts("PASS: all BG conversions, partial boundaries, shared identities, rooms/seams, hidden layers and tool routing.");
}

static void Tiles(void)
{
    StanTile tiles[3]={{.pointcount=4,.editorid=1},{.pointcount=3,.editorid=2},{.pointcount=4,.editorid=3}};
    DWORD map[3*STAN_TILE_MAX_POINTS], hiddenid=3;
    unsigned char selected[3]={1,0,0};
    for (unsigned i=0; i<3*STAN_TILE_MAX_POINTS; i++) { map[i]=i; }
    map[STAN_TILE_MAX_POINTS]=1; map[STAN_TILE_MAX_POINTS+1]=2; /* Linked shared edge. */
    for (int i=0; i<4; i++) { map[2*STAN_TILE_MAX_POINTS+i]=i; }
    ViewportState s={.tool=EDITOR_TOOL_FACE_SELECT,.dragaxis=-1,.showstan=TRUE,.stanopacity=50,
        .stan={.tiles=tiles,.tilecount=3},.stanpointmap=map,.stanselected=selected,
        .stanhiddenids=&hiddenid,.stanhiddencount=1};
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.stancomponentcount==4 && !selected[0]);
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.stancomponentcount==4); /* No quad fan diagonal. */
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(selected[0] && !selected[1] && !selected[2]);
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.stancomponentcount==4);
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.stancomponentcount==4);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(selected[0] && !selected[1]);
    selected[1]=1;
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.stancomponentcount==6);
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.stancomponentcount==5);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(selected[0] && selected[1] && !selected[2]);
    selected[1]=0;
    Change(&s,EDITOR_TOOL_EDGE_SELECT); s.stancomponentcount=3;
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(!selected[0]);
    selected[0]=1;
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); s.stancomponentcount=3;
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(!selected[0]);
    selected[0]=1; s.stanopacity=0;
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(!s.stancomponentcount);
    free(s.components); free(s.stancomponents);
    puts("PASS: STAN polygon boundaries, canonical linked points, partial selections and visibility.");
}

static void Portals(void)
{
    BgPortal portals[2]={{.pointcount=4,.geometryoffset=16},{.pointcount=4,.geometryoffset=16}};
    ViewportState s={.tool=EDITOR_TOOL_FACE_SELECT,.dragaxis=-1,.showportals=TRUE,
        .portals={.portals=portals,.portalcount=2},.portalselection={0,1},.selectedportal=1};
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.portalselection[0]==15 && !s.portalselection[1]);
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.portalselection[0]==15);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.portalselection[0]==1);
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.portalselection[0]==15);
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.portalselection[0]==15);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.portalselection[0]==1);
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); s.portalselection[0]=9; /* Closing edge: vertices 3 and 0. */
    Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(s.portalselection[0]==8);
    Change(&s,EDITOR_TOOL_VERTEX_SELECT); assert(s.portalselection[0]==9);
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(!s.portalselection[0] && s.selectedportal==BG_PORTAL_INDEX_NONE);
    s.portalselection[0]=1; Change(&s,EDITOR_TOOL_EDGE_SELECT);
    s.portalselection[0]=7; /* All vertices touched, one boundary edge missing. */
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(!s.portalselection[0]);
    s.tool=EDITOR_TOOL_VERTEX_SELECT; s.portalselection[0]=3; s.portalselection[1]=12;
    Change(&s,EDITOR_TOOL_FACE_SELECT); assert(s.portalselection[0]==1); /* Combine shared room links. */
    s.showportals=FALSE; Change(&s,EDITOR_TOOL_EDGE_SELECT); assert(!s.portalselection[0]);
    puts("PASS: portal conversions, closing edges, shared geometry masks and complete-boundary requirements.");
}

static void AtomicFailure(void)
{
    Vertex vertices[3]={0}; VertexColor colors[3]={0};
    BgDocumentVertexRef refs[]={{1,0},{1,1},{1,2}};
    BgFaceRef face={.room=1,.faceid=1}; SceneBatch batch={.count=3};
    unsigned char selected=0, stanselected=0;
    StanTile tile={.pointcount=4}; DWORD map[STAN_TILE_MAX_POINTS];
    BgPortal portal={.pointcount=4};
    for (unsigned i=0; i<STAN_TILE_MAX_POINTS; i++) { map[i]=i; }
    ViewportState s={.tool=EDITOR_TOOL_FACE_SELECT,.dragaxis=-1,.showbgprimary=TRUE,.showstan=TRUE,
        .stanopacity=50,.showportals=TRUE,.scene=vertices,.scenecolors=colors,.scenecount=3,
        .scenevertexrefs=refs,.scenefacerefs=&face,.selectedtris=&selected,.batches=&batch,.batchcount=1,
        .stan={.tiles=&tile,.tilecount=1},.stanpointmap=map,.stanselected=&stanselected,
        .portals={.portals=&portal,.portalcount=1}};
    for (int from=0; from<3; from++) for (int to=0; to<3; to++)
    {
        if (from==to) { continue; }
        int failures=0, successes=0;
        for (int budget=0; budget<9; budget++)
        {
            Face(&s); stanselected=1; s.portalselection[0]=1; s.selectedportal=0;
            if (from!=EDITOR_TOOL_FACE_SELECT) { Change(&s,from); }
            unsigned before=notifications, beforeerrors=errors;
            int bgcount=s.componentcount, stancount=s.stancomponentcount;
            unsigned char oldportal=s.portalselection[0]; s.vertexsnap=TRUE;
            ViewportComponent *oldbg=s.components; ViewportStanComponent *oldstan=s.stancomponents;
            allocations=budget; ViewportSetTool(&s,to); allocations=-1;
            if (s.tool==(EditorTool)from)
            {
                failures++; assert(notifications==before && errors==beforeerrors+1 && s.vertexsnap);
                assert(s.components==oldbg && s.stancomponents==oldstan);
                assert(s.componentcount==bgcount && s.stancomponentcount==stancount);
                assert(s.portalselection[0]==oldportal);
                if (from==EDITOR_TOOL_FACE_SELECT) { assert(selected && stanselected); }
            }
            else
            {
                successes++; assert(s.tool==(EditorTool)to && notifications==before+1 && errors==beforeerrors);
                if (to==EDITOR_TOOL_FACE_SELECT) { assert(selected && stanselected && s.portalselection[0]==1); }
                else { assert(s.componentcount==3 && s.stancomponentcount==4 && s.portalselection[0]==15); }
            }
        }
        assert(failures && successes);
    }
    free(s.components); free(s.stancomponents);
    puts("PASS: allocation failures preserve all domains and mode; successful switches notify history once.");
}

int main(void) { Background(); Tiles(); Portals(); AtomicFailure(); return 0; }
