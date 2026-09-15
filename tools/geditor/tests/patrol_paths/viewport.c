#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "setupload.h"
#include "stanload.h"
#define VIEWPORT_BOX_VERTICES 24
typedef struct Vertex { float x, y, z; unsigned char r, g, b; } Vertex;
typedef struct ViewportPad { SetupPadRef ref; float position[3], previewposition[3]; BOOL occupied, path; } ViewportPad;

typedef struct ViewportState {
    const SetupFile *markersetup;
    float markerlevelscale, yaw, pitch, posx, posy, posz;
    int height, dragaxis;
    BOOL dragpad, dragrotation, dragscaling, padpreview, showobjects;
    StanFile stan;
    ViewportPad *pads;
    DWORD padcount;
    Vertex *padmarkers;
    double dragdelta, dragorigin[3];
    Rotation scaleaxes;
    SetupPadRef selectedpad;
    SetupPatrolLink *patrollinks;
    DWORD patrollinkcount;
} ViewportState;
static double vertices[16][3];
static int vertexcount;
/* Capture the actual line/arrow vertices emitted by the viewport renderer. */
#define glPushAttrib(...) ((void)0)
#define glPopAttrib(...) ((void)0)
#define glDisable(...) ((void)0)
#define glEnable(...) ((void)0)
#define glDepthFunc(...) ((void)0)
#define glDepthMask(...) ((void)0)
#define glDepthRange(...) ((void)0)
#define glLineWidth(...) ((void)0)
#define glColor3ub(...) ((void)0)
#define glBegin(...) ((void)0)
#define glEnd(...) ((void)0)
static void glVertex3dv(const double *v)
{
    assert(vertexcount < 16);
    for (int i = 0; i < 3; i++) { assert(isfinite(v[i])); }
    memcpy(vertices[vertexcount++], v, sizeof(vertices[0]));
}
#include "patrol_viewport.h"

static void Near(double a, double b) { assert(fabs(a - b) < 1e-4); }
void PatrolViewport(void)
{
    SetupPad pads[2] = {0};
    SetupFile setup = {0};
    SetupPatrolLink link = {{0, 1}, 1};
    ViewportState state = {0};
    double points[2][3];
    pads[0].pos[0] = -50; pads[1].pos[0] = 50;
    pads[0].pos[2] = pads[1].pos[2] = -500;
    setup.pads = pads; setup.padcount = 2;
    state.markersetup = &setup; state.markerlevelscale = 0.5f;
    state.patrollinks = &link; state.patrollinkcount = 1; state.height = 1000; state.dragaxis = -1;
    ViewportDrawPatrolPaths(&state);
    assert(vertexcount == 6); Near(vertices[0][0], -100); Near(vertices[1][0], 100);
    Near(vertices[2][0], 0); Near(vertices[2][2], -1000); /* tip at midpoint */
    assert(vertices[3][0] < vertices[2][0]); /* points along authored direction */
    Near(vertices[3][1], -vertices[5][1]);
    double width = fabs(vertices[3][1]);
    vertexcount = 0; link.directions = 2; ViewportDrawPatrolPaths(&state);
    assert(vertexcount == 6 && vertices[3][0] > vertices[2][0]);
    vertexcount = 0; link.directions = 3; ViewportDrawPatrolPaths(&state);
    assert(vertexcount == 10); Near(vertices[2][0], -vertices[6][0]);
    assert(vertices[2][0] > vertices[3][0] && vertices[6][0] < vertices[7][0]);
    /* Twice the distance: double world width preserves the on-screen width. */
    vertexcount = 0; link.directions = 1; state.posz = 1000; ViewportDrawPatrolPaths(&state);
    Near(fabs(vertices[3][1]), width * 2); state.posz = 0;
    /* Transient pad moves follow the gizmo and leave the setup unchanged. */
    state.dragpad = TRUE; state.selectedpad.index = 1; state.dragaxis = 1; state.dragdelta = 30;
    assert(ViewportPatrolEndpoints(&state, &link, points)); Near(points[1][1], 30); Near(pads[1].pos[1], 0);
    state.selectedpad.bound = TRUE;
    assert(ViewportPatrolEndpoints(&state, &link, points)); Near(points[1][1], 0);
    state.dragpad = FALSE;
    /* Vertical paths retain a visible arrow; coincident endpoints draw nothing. */
    pads[0].pos[0] = pads[1].pos[0] = 0; pads[0].pos[1] = -50; pads[1].pos[1] = 50;
    vertexcount = 0; ViewportDrawPatrolPaths(&state); assert(vertexcount == 6);
    assert(vertices[3][1] < vertices[2][1] && fabs(vertices[3][0]) > 0);
    pads[0] = pads[1]; vertexcount = 0; ViewportDrawPatrolPaths(&state); assert(!vertexcount);
    state.markerlevelscale = 0; assert(!ViewportPatrolEndpoints(&state, &link, points));
    state.markerlevelscale = 1; pads[0].pos[0] = NAN; assert(!ViewportPatrolEndpoints(&state, &link, points));
    puts("PASS: midpoint arrows, forward/reverse/bidirectional travel, screen sizing, vertical routes, live pad movement and degenerate endpoints.");
}

void PadPreview(void)
{
    SetupPad pad = {0};
    SetupBoundPad bound = {0};
    SetupFile setup = {.pads=&pad, .padcount=1, .boundpads=&bound, .boundpadcount=1};
    StanTile tiles[2] = {0};
    ViewportPad views[2] = {{.ref={0,FALSE},.occupied=TRUE,.path=TRUE}, {.ref={0,TRUE}}};
    Vertex boxes[48] = {0};
    ViewportState state = {.markersetup=&setup, .markerlevelscale=.5f, .padpreview=TRUE,
        .dragaxis=-1, .pads=views, .padcount=2, .padmarkers=boxes, .showobjects=TRUE};
    SetupPadRef ref = {0,FALSE};
    double position[3];
    pad.pos[0]=10; pad.pos[1]=300; pad.pos[2]=10; strcpy(pad.stanname,"p1a"); bound.pad=pad;
    for (int t=0;t<2;t++)
    {
        StanTile *tile=&tiles[t]; tile->id=(t+1)*256; tile->pointcount=3;
        tile->extreme[0]=0; tile->extreme[1]=1; tile->extreme[2]=2;
        tile->points[0].y=tile->points[1].y=t*100;
        tile->points[1].z=200; tile->points[2].x=200; tile->points[2].y=t*100+100;
        for (int axis=0;axis<3;axis++) { views[t].position[axis]=views[t].previewposition[axis]=pad.pos[axis]*2; }
    }
    state.stan.tiles=tiles; state.stan.tilecount=2; state.stan.levelscale=1;
    for (int i=0;i<48;i++) { boxes[i].y=600+(i&1 ? 5 : -5); }
    ViewportRefreshPadPreview(&state);
    Near(views[0].previewposition[1],10); Near(views[1].previewposition[1],10);
    Near(boxes[0].y,5); Near(boxes[1].y,15);
    assert(ViewportPadPosition(&state,&ref,TRUE,position)); Near(position[1],10);
    assert(ViewportPadPosition(&state,&ref,FALSE,position)); Near(position[1],600);
    /* Named lower floor wins despite the authored height being above both. */
    strcpy(pad.stanname,"p2a"); ViewportRefreshPadPreview(&state); Near(views[0].previewposition[1],110);
    state.padpreview=FALSE; ViewportRefreshPadPreview(&state); Near(boxes[0].y,595);
    state.padpreview=TRUE; ViewportRefreshPadPreview(&state); Near(boxes[0].y,105);
    /* Path pads stay blue and visible even if occupied; selection stays white. */
    state.selectedpad.index=SETUP_PAD_INDEX_NONE; ViewportRefreshPadColors(&state);
    assert(boxes[0].r==64 && boxes[0].g==128 && boxes[0].b==255 && ViewportPadVisible(&state,0));
    assert(boxes[24].r==255 && boxes[24].g==48 && boxes[24].b==48);
    state.selectedpad=ref; ViewportRefreshPadColors(&state); assert(boxes[0].r==255 && boxes[0].g==255 && boxes[0].b==255);
    /* Horizontal movement follows the slope while authored Y remains unchanged. */
    state.dragpad=TRUE; state.dragaxis=0; state.dragdelta=40;
    assert(ViewportPadPosition(&state,&ref,TRUE,position)); Near(position[0],60); Near(position[1],130);
    assert(ViewportPadPosition(&state,&ref,FALSE,position)); Near(position[0],60); Near(position[1],600);
    state.dragaxis=-1; state.padpreview=FALSE; ViewportRefreshPadPreview(&state); Near(boxes[0].y,595);
    /* A missing stan retains the authored height. */
    state.padpreview=TRUE; state.stan.tilecount=0; ViewportRefreshPadPreview(&state); Near(views[0].previewposition[1],600);
    assert(pad.pos[0]==10 && pad.pos[1]==300 && pad.pos[2]==10 && !setup.dirty);
    puts("PASS: blue path pads, occupied/selected colors, named stacked floors, slopes, bound pads, preview toggle after a drag, authored coordinates and missing-stan fallback.");
}
