#ifndef GUD_DOORSHADOW_H
#define GUD_DOORSHADOW_H
#include <ultra64.h>
#include <bondtypes.h>
void doorShadowReset(void);
void doorShadowInit(PropDefHeaderRecord *commands);
s32 doorShadowHasSecondary(s32 room);
void doorShadowFreeRoom(s32 room);
void doorShadowExpandRoomBounds(s32 room);
Gfx *doorShadowRenderRoom(Gfx *gdl, s32 room, s32 layer);
#endif
