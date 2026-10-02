#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "setupload.h"
typedef void *HWND;
typedef struct MSG MSG;
typedef uintptr_t ULONG_PTR;
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#include "visibilitypanel.h"
#include "layout_types.inc"
#include "layout.inc"

static void Check(const RightPanelState *state, int height)
{
    assert(state->outlinerbottom >= 0 && state->outlinerbottom <= state->topheight);
    assert(state->topheight <= max(0, height - RIGHTPANEL_SPLITTER_H));
    int minimum = RIGHTPANEL_OUTLINER_TOP + RIGHTPANEL_OUTLINER_MIN_H
        + 2 * RIGHTPANEL_SECTION_GAP + 2 * RIGHTPANEL_SPLITTER_H + RIGHTPANEL_TRANSFORM_MIN_H;
    if (height >= minimum) {
        assert(state->outlinerbottom >= RIGHTPANEL_OUTLINER_TOP + RIGHTPANEL_OUTLINER_MIN_H + RIGHTPANEL_SECTION_GAP);
        assert(RightPanelTransformTop(state) + RIGHTPANEL_TRANSFORM_MIN_H <= state->topheight);
    }
    if (height >= minimum + RIGHTPANEL_BOTTOM_MIN) {
        assert(height - state->topheight - RIGHTPANEL_SPLITTER_H >= RIGHTPANEL_BOTTOM_MIN);
    }
}

int main(void)
{
    RightPanelState state = {0};
    state.preferredoutlinerbottom = RIGHTPANEL_OUTLINER_TOP + RIGHTPANEL_INITIAL_OUTLINER_H + RIGHTPANEL_SECTION_GAP;
    state.preferredtopheight = state.preferredoutlinerbottom + RIGHTPANEL_SPLITTER_H
        + RIGHTPANEL_SECTION_GAP + RIGHTPANEL_TRANSFORM_MIN_H;
    /* Initial 1px child creation and subsequent window resizes must not destroy
     * the default/user-requested sizes. */
    RightPanelClampLayout(&state, 1); Check(&state, 1);
    RightPanelClampLayout(&state, 1100); Check(&state, 1100);
    assert(state.outlinerbottom - RIGHTPANEL_SECTION_GAP - RIGHTPANEL_OUTLINER_TOP == 280);
    int original = state.outlinerbottom, lower = state.topheight;
    for (int height = 0; height <= 1200; height++) {
        RightPanelClampLayout(&state, height); Check(&state, height);
    }
    RightPanelClampLayout(&state, 1100);
    assert(state.outlinerbottom == original && state.topheight == lower);

    /* Both dividers are independently hittable; control rows are not handles. */
    for (int offset = 0; offset < RIGHTPANEL_SPLITTER_H; offset++) {
        assert(RightPanelInSplitter(&state, original + offset) == RIGHTPANEL_SPLITTER_OUTLINER);
        assert(RightPanelInSplitter(&state, lower + offset) == RIGHTPANEL_SPLITTER_PROPERTIES);
    }
    assert(RightPanelInSplitter(&state, original - 1) == RIGHTPANEL_SPLITTER_NONE);
    assert(RightPanelInSplitter(&state, original + RIGHTPANEL_SPLITTER_H) == RIGHTPANEL_SPLITTER_NONE);
    assert(RightPanelInSplitter(&state, lower + RIGHTPANEL_SPLITTER_H) == RIGHTPANEL_SPLITTER_NONE);

    state.draggingsplitter = RIGHTPANEL_SPLITTER_OUTLINER; state.splitteroffset = 3;
    RightPanelDragSplitter(&state, original + 80 + 3, 1100); Check(&state, 1100);
    assert(state.outlinerbottom == original + 80 && state.topheight == lower + 80);
    RightPanelDragSplitter(&state, original - 40 + 3, 1100); Check(&state, 1100);
    assert(state.outlinerbottom == original - 40 && state.topheight == lower - 40);

    state.draggingsplitter = RIGHTPANEL_SPLITTER_PROPERTIES; state.splitteroffset = 1;
    RightPanelDragSplitter(&state, lower + 30 + 1, 1100); Check(&state, 1100);
    assert(state.outlinerbottom == original - 40 && state.topheight == lower + 30);
    int savedoutliner = state.outlinerbottom, savedlower = state.topheight;
    RightPanelClampLayout(&state, 400); Check(&state, 400);
    RightPanelClampLayout(&state, 1100);
    assert(state.outlinerbottom == savedoutliner && state.topheight == savedlower);

    /* Dragging beyond either edge clamps safely and leaves Transform usable. */
    for (int divider = RIGHTPANEL_SPLITTER_OUTLINER; divider <= RIGHTPANEL_SPLITTER_PROPERTIES; divider++) {
        state.draggingsplitter = divider;
        RightPanelDragSplitter(&state, -1000, 1100); Check(&state, 1100);
        RightPanelDragSplitter(&state, 10000, 1100); Check(&state, 1100);
    }
    puts("PASS: larger outliner, two draggable dividers, minimum panel sizes and resize restoration.");
    return 0;
}
