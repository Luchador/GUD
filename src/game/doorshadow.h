#ifndef GUD_DOORSHADOW_H
#define GUD_DOORSHADOW_H
#include <ultra64.h>
#include <bondtypes.h>
void doorShadowReset(void);
void doorShadowInit(PropDefHeaderRecord *commands);
s32 doorShadowHasRoom(s32 room);
s32 doorShadowHasSecondary(s32 room);
void doorShadowFreeRoom(s32 room);
void doorShadowClearRenderCaches(void);
void doorShadowExpandRoomBounds(s32 room);
/* Same forward-ray and BG-coordinate contract as background triangle tests. */
bool doorShadowTestHit(s32 room, coord3d *from, coord3d *to, coord3d *dir, HitThing *hit);
/* Follow the owning room pass, including its two-cycle cache fallback. */
Gfx *doorShadowRenderRoom(Gfx *gdl, s32 room, s32 layer, bool oneCycle);
#endif
