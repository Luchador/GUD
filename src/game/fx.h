#ifndef GAME_FX_H
#define GAME_FX_H

#include <ultra64.h>
#include <bondtypes.h>

typedef struct BulletSpark {
    s32 age;
    s16 lifetime;
    s16 room;
    f32 framesPerTick;
    struct sImageTableEntry *imageFrames;
    coord3d position;
    f32 rotationCosRadius;
    f32 rotationSinRadius;
    f32 radius;
    rgba_u8 color;
} BulletSpark;

typedef struct MovingBulletSpark
{
    BulletSpark spark;
    coord3d velocity;
    f32 minimumY;
} MovingBulletSpark;

void fxResetAllSparks(void);
void fxUpdateAllSparks(void);
void fxRenderAllSparks(Gfx **gdl, s32 zbufferMode);
BulletSpark *fxCreateBulletSpark(coord3d *position, s32 sparkType, s16 room);
BulletSpark *fxCreateHitPuff(coord3d *position, s32 effectType, f32 size, s16 room);

#endif
