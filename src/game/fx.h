#ifndef GAME_FX_H
#define GAME_FX_H

#include <ultra64.h>

typedef struct s_bullet_spark {
    s32 age;
    s16 lifetime;
    s16 unk06;
    f32 unk08;
    void* unk0C;
    f32 unk10;
    f32 unk14;
    f32 unk18;
    f32 unk1c;
    f32 unk20;
    f32 unk24;
    u8 unk28;
    u8 unk29;
    u8 unk2A;
    u8 unk2B;
} s_bullet_spark;

typedef struct s_moving_bullet_spark
{
    s_bullet_spark unk00;
    coord3d velocity;
    f32 unk38;
} s_moving_bullet_spark;

void bullet_sparks_reset_all(void);
void bullet_sparks_update_all(void);
void bullet_sparks_render_all(Gfx **gdl, s32 zbufferMode);
s_bullet_spark *bullet_spark_create(coord3d *arg0, s32 arg1, f32 arg2, s16 arg3);

#endif
