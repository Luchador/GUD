#ifndef GAME_HUD_H
#define GAME_HUD_H

#include <ultra64.h>
#include <bondtypes.h>

void hudDrawImage(Gfx **gdlptr, f32 *position, f32 *halfSize, s32 textureWidth, s32 textureHeight, s32 swapAxes, s32 flipS, s32 flipT, s32 red, s32 green, s32 blue, s32 alpha, s32 mipmapped, s32 highlight);
void hudMakeDamageSegments(struct damage_display_val *segments, s32 numSegments, s32 isArmour, f32 healthValue);
Gfx *hudBuildGaugeBar(Gfx *gdl, uintptr_t vtxaddr, s32 numvertices);

#endif
