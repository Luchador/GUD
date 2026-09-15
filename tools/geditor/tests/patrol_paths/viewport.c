#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "setupload.h"

typedef struct ViewportState {
    const SetupFile *markersetup;
    float markerlevelscale, yaw, pitch, posx, posy, posz;
    int height, dragaxis;
    BOOL dragpad, dragrotation, dragscaling;
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
