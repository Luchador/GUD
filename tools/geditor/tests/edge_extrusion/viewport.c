#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "edittool.h"
#include "stanload.h"
#include "setupload.h"
typedef void *HWND;
typedef intptr_t LPARAM;
#define VIEWPORT_BOX_VERTICES 24
#define VIEWPORT_WM_SELECTION_CHANGED 1
#define VIEWPORT_WM_TRANSFORM_PREVIEW 2
#define VIEWPORT_WM_EXTRUDE_EDGES 3
#define VIEWPORT_WM_TRANSLATE_SELECTION 4
#define VIEWPORT_WM_ROTATE_SELECTION 5
#define VIEWPORT_WM_SCALE_SELECTION 6
#define VIEWPORT_WM_DUPLICATE_OBJECT 7
typedef struct Vertex { float x,y,z; } Vertex;
typedef struct SceneBatch { int first,count; BOOL object,secondary; } SceneBatch;
typedef struct ViewportComponent { BgDocumentVertexRef refs[2]; int corners[2]; } ViewportComponent;
typedef struct ViewportEdgeExtrusion {
    const BgDocumentEdgeRef *edges; DWORD count; double offset[3],applied[3]; BgVertex *preview;
} ViewportEdgeExtrusion;
typedef struct ViewportTranslation { double offset[3]; } ViewportTranslation;
typedef struct ViewportRotation { Rotation rotation; double pivot[3]; } ViewportRotation;
typedef struct ViewportObjectDuplicate {
    DWORD source; TransformMode mode; ViewportTranslation translation;
    ViewportRotation rotation; Scaling scaling;
} ViewportObjectDuplicate;
typedef struct ViewportState {
    EditorTool tool;
    int scenecount,batchcount,componentcount,componentcapacity,dragaxis;
    Vertex *scene,*padmarkers;
    unsigned char *hiddentris,*dragmask;
    SceneBatch *batches;
    ViewportComponent *components;
    BgDocumentVertexRef *scenevertexrefs;
    BgFaceRef *scenefacerefs;
    BOOL showbgsecondary,showbgprimary,dragmarker,dragpad,dragstan,dragscaling,dragrotation;
    BOOL dragknife,dragportal;
    BOOL dragextruding,extrudepreviewvalid,dragduplicating;
    DWORD selectedobject;
    BgDocumentEdgeRef *extrudeedges;
    int *extrudeowners;
    BgVertex *extrudepreview;
    DWORD extrudecount;
    double dragdelta,dragorigin[3];
    float (*dragvertices)[3];
    Rotation scaleaxes;
    StanFile stan;
    const SetupFile *markersetup;
    float markerlevelscale;
} ViewportState;
static HWND captured;
static unsigned commits,moves,notifications;
static unsigned duplicates;
static ViewportObjectDuplicate duplicated;
static BgDocumentEdgeRef committed;
static double offset[3];
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static HWND GetCapture(void) { return captured; }
static void ReleaseCapture(void) { captured=NULL; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void ViewportUpdateGizmo(ViewportState *state) {}
static void ViewportCancelBoxSelection(HWND hwnd,ViewportState *state) {}
static void ViewportFinishKnifeTransform(HWND hwnd,ViewportState *state,BOOL cancel) { abort(); }
static void ViewportPreviewPortalDrag(ViewportState *state,double delta) { abort(); }
static void ViewportSetSetupMarkers(HWND hwnd,ViewportState *state,const SetupFile *setup,float scale) { abort(); }
static int ViewportSelectedPadIndex(ViewportState *state) { abort(); }
static void ViewportRefreshStanOverlay(ViewportState *state) { abort(); }
static void ViewportBuildObjectSelectionBox(ViewportState *state) {}
static void ViewportClearAllSelection(ViewportState *state) { state->componentcount=0; }
static void SendMessage(HWND hwnd,int msg,int wparam,LPARAM lparam)
{
    ViewportState *state=hwnd;
    if (msg==VIEWPORT_WM_EXTRUDE_EDGES)
    {
        const ViewportEdgeExtrusion *r=(void *)lparam;
        assert(state->dragaxis==-1 && !state->dragextruding && !state->extrudepreview && !captured);
        assert(r->count==1 && r->edges); committed=r->edges[0]; memcpy(offset,r->offset,sizeof(offset)); commits++;
    }
    else if (msg==VIEWPORT_WM_TRANSLATE_SELECTION)
    { moves++; memcpy(offset,((ViewportTranslation *)lparam)->offset,sizeof(offset)); }
    else if (msg==VIEWPORT_WM_DUPLICATE_OBJECT)
    {
        assert(state->dragaxis==-1 && !state->dragduplicating && !state->dragvertices && !captured);
        duplicated=*(ViewportObjectDuplicate *)lparam; duplicates++;
    }
    else { notifications++; }
}
/* Spy on the input to the independently tested rotation math. */
void RotationAxis(Rotation *rotation,int axis,double degrees)
{ memset(rotation,0,sizeof(*rotation)); rotation->m[axis][axis]=degrees; }
#include "viewport.inc"

static void Begin(ViewportState *s,BOOL extrude,double delta)
{
    s->dragaxis=1; s->dragdelta=delta; s->dragextruding=extrude;
    s->dragvertices=calloc(s->scenecount,sizeof(*s->dragvertices)); s->dragmask=calloc(s->scenecount,1);
    assert(s->dragvertices && s->dragmask);
    for (int i=0;i<s->scenecount;i++)
    {
        s->dragvertices[i][0]=s->scene[i].x; s->dragvertices[i][1]=s->scene[i].y; s->dragvertices[i][2]=s->scene[i].z;
        s->dragmask[i]=1;
    }
    if (extrude) { assert(ViewportPrepareEdgeExtrusion(s)); }
    captured=s;
}

int main(void)
{
    /* Cached corners intentionally point into separate triangles. The first
     * triangle contains A but not B; the actual owning edge is in triangle 2. */
    BgDocumentVertexRef refs[6]={{1,2},{1,3},{1,0},{1,0},{1,1},{1,2}};
    BgFaceRef faces[2]={{12,1,0,0},{11,1,0,0}};
    Vertex vertices[6]={{0,20,0},{20,20,0},{0,0,0},{0,0,0},{20,0,0},{0,20,0}};
    Vertex saved[6]; memcpy(saved,vertices,sizeof(saved));
    unsigned char hidden[2]={0}; SceneBatch batch={0,6,FALSE,FALSE};
    ViewportState s={.tool=EDITOR_TOOL_EDGE_SELECT,.scenecount=6,.scene=vertices,.scenefacerefs=faces,
        .scenevertexrefs=refs,.hiddentris=hidden,.batches=&batch,.batchcount=1,.dragaxis=-1,.showbgprimary=TRUE};
    s.components=calloc(1,sizeof(*s.components)); assert(s.components);
    s.componentcount=s.componentcapacity=1;
    s.components[0]=(ViewportComponent){{{1,0},{1,1}},{2,4}};
    Begin(&s,TRUE,25);
    assert(s.extrudecount==1 && s.extrudeedges[0].face.faceid==11 && s.extrudeedges[0].corner==0 && s.extrudeowners[0]==1);
    ViewportEndTransform(&s,&s);
    assert(commits==1 && !moves && committed.face.faceid==11 && offset[1]==25);
    assert(!memcmp(vertices,saved,sizeof(saved)) && !s.extrudeedges && !s.dragmask && !s.dragvertices);
    /* Escape/capture loss and click-without-drag both abandon the preview. */
    Begin(&s,TRUE,25); ViewportCancelTransform(&s);
    assert(commits==1 && !captured && !s.extrudepreview && !memcmp(vertices,saved,sizeof(saved)));
    Begin(&s,TRUE,0); ViewportEndTransform(&s,&s); assert(commits==1);
    /* Ordinary translation still restores its preview and sends a move. */
    Begin(&s,FALSE,25); vertices[0].y+=25; ViewportEndTransform(&s,&s);
    assert(moves==1 && commits==1 && !memcmp(vertices,saved,sizeof(saved)));
    /* Shift-drag routes all three transforms to one duplicate commit, with
     * the source pose restored before the frame receives the request. */
    for(int mode=0;mode<3;mode++)
    {
        s.selectedobject=17; s.dragrotation=mode==1; s.dragscaling=mode==2;
        s.dragorigin[0]=10; s.dragorigin[1]=20; s.dragorigin[2]=30;
        Begin(&s,FALSE,.5); s.dragduplicating=TRUE; vertices[0].y+=10;
        ViewportEndTransform(&s,&s);
        assert(duplicates==(unsigned)mode+1 && duplicated.source==17 && duplicated.mode==(TransformMode)mode);
        assert(!memcmp(vertices,saved,sizeof(saved)) && moves==1 && commits==1);
        if(mode==0) { assert(duplicated.translation.offset[1]==.5); }
        if(mode==1) { assert(duplicated.rotation.rotation.m[1][1]==.5 && duplicated.rotation.pivot[2]==30); }
        if(mode==2) { assert(duplicated.scaling.factor[0]==1 && duplicated.scaling.factor[1]==1.5 && duplicated.scaling.pivot[2]==30); }
        Begin(&s,FALSE,.5); s.dragduplicating=TRUE; vertices[0].y+=10;
        ViewportCancelTransform(&s);
        assert(duplicates==(unsigned)mode+1 && !memcmp(vertices,saved,sizeof(saved)) && !s.dragduplicating);
        Begin(&s,FALSE,0); s.dragduplicating=TRUE; ViewportEndTransform(&s,&s);
        assert(duplicates==(unsigned)mode+1);
    }
    /* Select the new outer edge by actual face/corner, independent of sort
     * order, and preserve canonical endpoint ordering used for deselection. */
    BgDocumentEdgeRef edge={{12,1,0,0},2};
    assert(ViewportSelectBgEdges(&s,&edge,1));
    assert(s.componentcount==1 && s.components[0].refs[0].index==0 && s.components[0].refs[1].index==2);
    ViewportComponent previous=s.components[0];
    hidden[0]=1; assert(!ViewportSelectBgEdges(&s,&edge,1));
    assert(!memcmp(&previous,s.components,sizeof(previous))); hidden[0]=0;
    edge.corner=9; assert(!ViewportSelectBgEdges(&s,&edge,1));
    edge.corner=1; edge.face.faceid=999; assert(!ViewportSelectBgEdges(&s,&edge,1));
    free(s.components);
    puts("PASS: real edge ownership after draw reordering, commit payload lifetime, cancellation/no-op drags, unchanged ordinary moves, and canonical outer-edge selection.");
    puts("PASS: move/rotate/scale duplicate payloads, original pose restoration, cancellation and no-op drags.");
    return 0;
}
