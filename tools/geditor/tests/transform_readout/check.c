#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "editorunits.h"

typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct ViewportState {
    BOOL thumbnail, orbit, dragscaling, dragrotation, dragextruding, extrudepreviewvalid;
    int dragaxis, width, height;
    double dragdelta, coordinatescale, extrudeoffset[3];
} ViewportState;
#include "logic.inc"

static void Expect(const ViewportState *state, const char *expected)
{
    char text[96];
    assert(ViewportTransformReadoutText(state, text));
    if (strcmp(text, expected)) { fprintf(stderr, "Expected '%s', got '%s'\n", expected, text); }
    assert(!strcmp(text, expected));
}

int main(void)
{
    ViewportState state = {.dragaxis=0, .width=800, .height=600};
    state.coordinatescale = EditorUnitsFactor(EDITOR_UNITS_NATIVE, .25);
    state.dragdelta = EditorUnitsSnap(41.1, state.coordinatescale);
    Expect(&state, "Move X: +10 units");
    state.dragaxis = 2; state.dragdelta = EditorUnitsSnap(-41.1, state.coordinatescale);
    Expect(&state, "Move Z: -10 units");
    state.coordinatescale = EditorUnitsFactor(EDITOR_UNITS_WORLD, .25);
    state.dragdelta = EditorUnitsSnap(-41.1, state.coordinatescale);
    Expect(&state, "Move Z: -41 units");
    state.dragdelta = -.00000001; Expect(&state, "Move Z: +0 units");
    state.dragdelta = 1.125; Expect(&state, "Move Z: +1.125 units");
    state.dragdelta = 2.25; Expect(&state, "Move Z: +2.25 units");

    /* Native extrusion can round the requested displacement during preview. */
    state.dragaxis = 1; state.dragextruding = state.extrudepreviewvalid = TRUE;
    state.dragdelta = 8; state.extrudeoffset[1] = 7.5;
    Expect(&state, "Move Y: +7.5 units");
    state.extrudepreviewvalid = FALSE; Expect(&state, "Move Y: +0 units");
    state.dragextruding = FALSE;

    state.dragrotation = TRUE; state.dragdelta = 45;
    Expect(&state, "Rotate Y: +45 deg");
    state.dragdelta = -370; Expect(&state, "Rotate Y: -370 deg");
    state.dragdelta = -0.; Expect(&state, "Rotate Y: +0 deg");
    state.dragrotation = FALSE; state.dragscaling = TRUE; state.dragaxis = 0;
    state.dragdelta = .25; Expect(&state, "Scale X: 1.25x (+25%)");
    state.dragdelta = -.25; Expect(&state, "Scale X: 0.75x (-25%)");
    state.dragdelta = -.99; Expect(&state, "Scale X: 0.01x (-99%)");
    state.dragaxis = 3; state.dragdelta = 1; Expect(&state, "Scale XYZ: 2.00x (+100%)");
    state.dragdelta = -0.; Expect(&state, "Scale XYZ: 1.00x (+0%)");

    char text[96];
    state.dragaxis = -1; assert(!ViewportTransformReadoutText(&state, text)); /* release/cancel */
    state.dragaxis = 0; state.thumbnail = TRUE; assert(!ViewportTransformReadoutText(&state, text));
    state.thumbnail = FALSE; state.orbit = TRUE; assert(!ViewportTransformReadoutText(&state, text));
    state.orbit = FALSE; state.dragdelta = NAN; assert(!ViewportTransformReadoutText(&state, text));

    RECT bounds;
    const double center[2] = {400, 300}, right[2] = {790, 300}, top[2] = {400, 5};
    assert(ViewportTransformReadoutBounds(&state, center, 180, 26, &bounds));
    assert(bounds.left > center[0] && bounds.bottom < center[1]);
    assert(ViewportTransformReadoutBounds(&state, right, 180, 26, &bounds));
    assert(bounds.right < right[0]);
    assert(ViewportTransformReadoutBounds(&state, top, 180, 26, &bounds));
    assert(bounds.top > top[1]);
    const double extremes[] = {-1e30, 0, 400, 600, 800, 1e30};
    for (int x=0; x<6; x++) for (int y=0; y<6; y++) {
        const double anchor[2] = {extremes[x], extremes[y]};
        assert(ViewportTransformReadoutBounds(&state, anchor, 180, 26, &bounds));
        assert(bounds.left >= 8 && bounds.top >= 8 && bounds.right <= 792 && bounds.bottom <= 592);
        assert(bounds.right - bounds.left == 180 && bounds.bottom - bounds.top == 26);
    }
    const double invalid[2] = {NAN, 0};
    assert(!ViewportTransformReadoutBounds(&state, invalid, 180, 26, &bounds));
    state.width = 100;
    assert(!ViewportTransformReadoutBounds(&state, center, 180, 26, &bounds));
    puts("PASS: transform deltas, unit conversion, snapping, extrusion rounding, scale ratios, cancellation and edge placement.");
    return 0;
}
