#ifndef GAME_HUD_H
#define GAME_HUD_H

#include <ultra64.h>
#include <bondtypes.h>

void hudMakeDamageSegments(struct damage_display_val *segments, s32 numSegments, s32 isArmour, f32 healthValue);

Gfx *buildGaugeBarDL(Gfx *gdl, uintptr_t vtxaddr, s32 numvertices);

void sub_GAME_7F0A33F8(struct WatchVertex *vtx, s32 numverts, f32 scale, s32 arg3);

Gfx *draw_watch_background(Gfx *gdl, struct WatchVertex *watchVerts, s32 unusedArg, s32 drawFan);

struct WatchVertex *setup_watch_rectangles(struct WatchVertex *vtx, s32 startx, s32 startz, s32 width, s32 height, s32 horizontalOffset, s32 verticalOffset);

#endif
