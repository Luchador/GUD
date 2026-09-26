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
#define VIEWPORT_UNIFORM_SCALE_AXIS 3
#define VIEWPORT_WM_SELECTION_CHANGED 1
#define VIEWPORT_WM_TRANSFORM_PREVIEW 2
#define VIEWPORT_WM_EXTRUDE_EDGES 3
#define VIEWPORT_WM_TRANSLATE_SELECTION 4
#define VIEWPORT_WM_ROTATE_SELECTION 5
#define VIEWPORT_WM_SCALE_SELECTION 6
#define VIEWPORT_WM_DUPLICATE_OBJECT 7
#define VIEWPORT_WM_DUPLICATE_PORTALS 8
#define VIEWPORT_WM_DUPLICATE_BG_FACES 9
#define VIEWPORT_WM_PREVIEW_EDGE_EXTRUSION 8
#define VIEWPORT_OBJECT_NONE ((DWORD)-1)
typedef struct Vertex { float x,y,z; } Vertex;
typedef struct SceneBatch { int first,count; BOOL object,secondary; } SceneBatch;
typedef struct ViewportComponent { BgDocumentVertexRef refs[2]; int corners[2]; } ViewportComponent;
typedef struct ViewportEdgeExtrusion {
    const BgDocumentEdgeRef *edges; DWORD count; double offset[3],applied[3]; BgVertex *preview;
    const StanEdgeRef *stanedges; StanPoint *stanpreview;
} ViewportEdgeExtrusion;
typedef struct ViewportStanComponent { StanPointRef refs[2]; } ViewportStanComponent;
typedef struct ViewportTranslation { double offset[3]; } ViewportTranslation;
typedef struct ViewportRotation { Rotation rotation; double pivot[3]; } ViewportRotation;
typedef struct ViewportObjectDuplicate {
    DWORD source; TransformMode mode; ViewportTranslation translation;
    ViewportRotation rotation; Scaling scaling;
} ViewportObjectDuplicate;
typedef struct ViewportState {
    BOOL dragroom;
    EditorTool tool;
    int scenecount,batchcount,componentcount,componentcapacity,dragaxis;
    Vertex *scene,*padmarkers;
    unsigned char *hiddentris,*dragmask;
    SceneBatch *batches;
    ViewportComponent *components;
    BgDocumentVertexRef *scenevertexrefs;
    BgFaceRef *scenefacerefs;
    BOOL showbgsecondary,showbgprimary,dragmarker,dragpad,dragstan,dragscaling,dragrotation;
    BOOL dragknife,dragportal,dragportalduplicating;
    BOOL dragextruding,extrudepreviewvalid,dragduplicating,dragfaceduplicating;
    DWORD selectedobject;
    BgDocumentEdgeRef *extrudeedges;
    StanEdgeRef *stanextrudeedges;
    StanPoint *stanextrudepreview;
    int *extrudeowners;
    BgVertex *extrudepreview;
    DWORD extrudecount;
    double dragdelta,dragorigin[3];
    double extrudeoffset[3],gizmoposition[3];
    float (*dragvertices)[3];
    Rotation scaleaxes;
    StanFile stan;
    ViewportStanComponent *stancomponents;
    int stancomponentcount,stanopacity;
    BOOL showstan;
    DWORD *stanpointmap,*stanhiddenids,stanhiddencount;
    const SetupFile *markersetup;
    float markerlevelscale;
} ViewportState;
static HWND captured;
static unsigned commits,moves,notifications;
static unsigned duplicates;
static ViewportObjectDuplicate duplicated;
static BgDocumentEdgeRef committed;
static StanEdgeRef stancommitted;
static unsigned stancommits,previews;
static BOOL rejectpreview;
static double offset[3];
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static HWND GetCapture(void) { return captured; }
static void ReleaseCapture(void) { captured=NULL; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void ViewportUpdateGizmo(ViewportState *state) {}
static void ViewportCancelBoxSelection(HWND hwnd,ViewportState *state) {}
static void ViewportFinishKnifeTransform(HWND hwnd,ViewportState *state,BOOL cancel) { abort(); }
static void ViewportPreviewPortalDrag(ViewportState *state,double delta,const Rotation *rotation,const Scaling *scaling) { abort(); }
static void ViewportSetSetupMarkers(HWND hwnd,ViewportState *state,const SetupFile *setup,float scale) { abort(); }
static int ViewportSelectedPadIndex(ViewportState *state) { abort(); }
static void ViewportRefreshStanOverlay(ViewportState *state) {}
static void ViewportBuildObjectSelectionBox(ViewportState *state) {}
static void ViewportClearAllSelection(ViewportState *state) { state->componentcount=0; }
static intptr_t SendMessage(HWND hwnd,int msg,int wparam,LPARAM lparam)
{
    ViewportState *state=hwnd;
    if (msg==VIEWPORT_WM_EXTRUDE_EDGES)
    {
        const ViewportEdgeExtrusion *r=(void *)lparam;
        assert(state->dragaxis==-1 && !state->dragextruding && !state->extrudepreview && !captured);
        assert(r->count==1);memcpy(offset,r->offset,sizeof(offset));
        if(r->stanedges)
        { assert(!r->edges&&!state->stanextrudeedges&&!state->stanextrudepreview);stancommitted=r->stanedges[0];stancommits++; }
        else { assert(r->edges);committed=r->edges[0];commits++; }
    }
    else if(msg==VIEWPORT_WM_PREVIEW_EDGE_EXTRUSION)
    {
        ViewportEdgeExtrusion *r=(void *)lparam;previews++;
        assert(r->count==1&&r->stanedges&&r->stanpreview&&!r->edges&&!r->preview);
        if(rejectpreview)return 0;
        for(int k=0;k<3;k++)r->applied[k]=round(r->offset[k]/4)*4;
        return 1;
    }
    else if (msg==VIEWPORT_WM_TRANSLATE_SELECTION)
    { moves++; memcpy(offset,((ViewportTranslation *)lparam)->offset,sizeof(offset)); }
    else if (msg==VIEWPORT_WM_DUPLICATE_OBJECT)
    {
        assert(state->dragaxis==-1 && !state->dragduplicating && !state->dragvertices && !captured);
        duplicated=*(ViewportObjectDuplicate *)lparam; duplicates++;
    }
    else { notifications++; }
    return 1;
}
/* Spy on the input to the independently tested rotation math. */
void RotationAxis(Rotation *rotation,int axis,double degrees)
{ memset(rotation,0,sizeof(*rotation)); rotation->m[axis][axis]=degrees; }
static void ViewportCancelRoomDrag(HWND hwnd, ViewportState *state) { abort(); }
#include "viewport.inc"

static void Begin(ViewportState *s,BOOL extrude,double delta)
{
    s->dragaxis=1; s->dragdelta=delta; s->dragextruding=extrude;
    int count=s->dragstan ? (int)(s->stan.tilecount*STAN_TILE_MAX_POINTS) : s->scenecount;
    s->dragvertices=calloc(count,sizeof(*s->dragvertices)); s->dragmask=calloc(count,1);
    assert(s->dragvertices && s->dragmask);
    for (int i=0;i<count;i++)
    {
        if(s->dragstan)
        {
            const StanPoint *p=&s->stan.tiles[i/STAN_TILE_MAX_POINTS].points[i%STAN_TILE_MAX_POINTS];
            s->dragvertices[i][0]=p->x;s->dragvertices[i][1]=p->y;s->dragvertices[i][2]=p->z;
        }
        else { s->dragvertices[i][0]=s->scene[i].x; s->dragvertices[i][1]=s->scene[i].y; s->dragvertices[i][2]=s->scene[i].z; }
        s->dragmask[i]=1;
    }
    if (extrude) { assert(ViewportPrepareEdgeExtrusion(s)); }
    captured=s;
}

static void StanGesture(void)
{
    StanTile tile={.editorid=1,.pointcount=4,.points={{0,0,0,0},{0,0,20,0},{20,0,20,0},{20,0,0,0}}};
    StanTile before=tile;DWORD map[STAN_TILE_MAX_POINTS];
    for(DWORD i=0;i<STAN_TILE_MAX_POINTS;i++)map[i]=i;
    ViewportStanComponent component={.refs={{0,2},{0,3}}};
    ViewportState s={.tool=EDITOR_TOOL_EDGE_SELECT,.selectedobject=VIEWPORT_OBJECT_NONE,.dragstan=TRUE,
        .showstan=TRUE,.stanopacity=44,.stan={.tiles=&tile,.tilecount=1},.stancomponents=&component,
        .stancomponentcount=1,.stanpointmap=map,.dragaxis=-1};
    assert(ViewportShouldExtrudeEdges(&s,TRUE)&&!ViewportShouldExtrudeEdges(&s,FALSE));
    s.dragrotation=TRUE;assert(!ViewportShouldExtrudeEdges(&s,TRUE));s.dragrotation=FALSE;
    s.dragscaling=TRUE;assert(!ViewportShouldExtrudeEdges(&s,TRUE));s.dragscaling=FALSE;
    s.tool=EDITOR_TOOL_FACE_SELECT;assert(!ViewportShouldExtrudeEdges(&s,TRUE));s.tool=EDITOR_TOOL_EDGE_SELECT;
    s.selectedobject=17;assert(!ViewportShouldExtrudeEdges(&s,TRUE));s.selectedobject=VIEWPORT_OBJECT_NONE;
    s.dragstan=FALSE;assert(ViewportShouldExtrudeEdges(&s,TRUE));s.dragstan=TRUE;
    Begin(&s,TRUE,25);
    assert(s.stanextrudeedges&&s.stanextrudepreview&&!s.extrudeedges&&s.extrudecount==1);
    assert(s.stanextrudeedges[0].tile==0&&s.stanextrudeedges[0].point==2);
    ViewportPreviewEdgeExtrusion(&s,&s,25);
    assert(previews==1&&s.extrudepreviewvalid&&s.extrudeoffset[1]==24&&s.gizmoposition[1]==24);
    assert(!memcmp(&tile,&before,sizeof(tile))); /* Only new geometry is previewed. */
    rejectpreview=TRUE;ViewportPreviewEdgeExtrusion(&s,&s,-25);rejectpreview=FALSE;
    assert(!s.extrudepreviewvalid&&!s.extrudeoffset[1]&&!s.gizmoposition[1]);
    ViewportPreviewEdgeExtrusion(&s,&s,25);ViewportEndTransform(&s,&s);
    assert(stancommits==1&&stancommitted.tile==0&&stancommitted.point==2&&offset[1]==25);
    assert(!memcmp(&tile,&before,sizeof(tile))&&!captured&&!s.stanextrudeedges&&!s.stanextrudepreview);
    Begin(&s,TRUE,25);ViewportPreviewEdgeExtrusion(&s,&s,25);ViewportCancelTransform(&s);
    assert(stancommits==1&&!captured&&!s.stanextrudepreview&&!memcmp(&tile,&before,sizeof(tile)));
    Begin(&s,TRUE,0);ViewportEndTransform(&s,&s);assert(stancommits==1);
    Begin(&s,TRUE,25);ViewportPreviewEdgeExtrusion(&s,&s,25);ViewportPreviewEdgeExtrusion(&s,&s,0);
    ViewportEndTransform(&s,&s);assert(stancommits==1); /* Back to mouse-down position. */
    unsigned oldmoves=moves;
    Begin(&s,FALSE,25);tile.points[2].y+=25;ViewportEndTransform(&s,&s);
    assert(moves==oldmoves+1&&stancommits==1&&!memcmp(&tile,&before,sizeof(tile)));
    puts("PASS: Stan Shift-Move extrusion routing, quantized/invalid preview, fixed source geometry, commit payload lifetime, cancellation/no-op gestures and ordinary Stan movement.");
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
    StanGesture();
    return 0;
}
