#include <ultra64.h>
#include <bondtypes.h>
#include "fx.h"
#include "image_bank.h"
#include "lv.h"


#define BULLET_SPARKS_MAX 20
#define BULLET_MOVING_SPARKS_MAX 50

s_bullet_spark g_BulletSparkArray[BULLET_SPARKS_MAX];
s_moving_bullet_spark g_MovingBulletSparkArray[BULLET_MOVING_SPARKS_MAX];

static Vtx g_BulletSparkVertexTemplate;

struct rgba_u8 g_BulletSparkColors[8] = {
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0xC8, 0xFF },
    { 0xFF, 0x00, 0x00, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0 },
    { 0 }
};


void fxResetBulletSparks(void)
{
    s32 i;
    s32 start_index;

    start_index = 0;

    for (i = start_index; (i < BULLET_SPARKS_MAX); i++)
    {
        g_BulletSparkArray[i].unk0C = 0;
        g_BulletSparkArray[i].lifetime = 0;
        g_BulletSparkArray[i].age = 0;
    }
}


void fxInitBulletSparks(s_bullet_spark *spark, coord3d *arg1, s32 arg2, f32 arg3, s16 arg4)
{
    f32 angle;

    angle = randomGetNext();
    angle *= (1.0f / M_U32_MAX_VALUE_F);
    angle *= M_TAU_F;

    spark->age = 0;
    spark->unk06 = arg4;

    if (arg2 == 4)
    {
        spark->lifetime = 1;
        spark->unk08 = 1.0f;
        spark->unk0C = flareimage2;
    }
    else if (arg2 == 1)
    {
        spark->lifetime = 11;
        spark->unk08 = 0.5f;
        spark->unk0C = explosion_smokeimages;
    }
    else if (arg2 == 3)
    {
        spark->lifetime = 9;
        spark->unk08 = 0.5f;
        spark->unk0C = scattered_explosions;
    }
    else if (arg2 == 6)
    {
        spark->lifetime = 100;
        spark->unk08 = 0.0f;
        spark->unk0C = flareimage2;
    }
    else
    {
        spark->lifetime = 11;
        spark->unk08 = 0.5f;
        spark->unk0C = explosion_smokeimages;
    }

    spark->unk28 = g_BulletSparkColors[arg2].r;
    spark->unk29 = g_BulletSparkColors[arg2].g;
    spark->unk2A = g_BulletSparkColors[arg2].b;
    spark->unk2B = g_BulletSparkColors[arg2].a;

    spark->unk10 = arg1->x;
    spark->unk14 = arg1->y;
    spark->unk18 = arg1->z;

    arg3 *= 1.0f + ((f32)randomGetNext() * (1.0f / M_U32_MAX_VALUE_F) * 0.25f);
    arg3 *= M_SQRT2_F;
    spark->unk24 = arg3;

    spark->unk1c = cosf(angle) * arg3;
    spark->unk20 = sinf(angle) * arg3;
}


s_bullet_spark *fxCreateBulletspark(coord3d *arg0, s32 arg1, f32 arg2, s16 arg3)
{
    s_bullet_spark *ptr;

    for (ptr = &g_BulletSparkArray[0]; ptr < &g_BulletSparkArray[BULLET_SPARKS_MAX]; ptr++)
    {
        if (ptr->lifetime == 0)
        {
            fxInitBulletSparks(ptr, arg0, arg1, arg2, arg3);
            return ptr;
        }
    }

    return NULL;
}


void fxUpdateBulletSparks(void)
{
    s_bullet_spark *thing = &g_BulletSparkArray[0]; \
    s_bullet_spark *end = g_BulletSparkArray + BULLET_SPARKS_MAX;

    for (; thing < end; thing++)
    {
        if (thing->lifetime > 0)
        {
            thing->age += g_ClockTimer;

            if (thing->age >= 0 && thing->age >= thing->lifetime)
            {
                thing->lifetime = 0;
            } 
        }
    }
}


void fxRenderBulletSpark(s_bullet_spark *thing, Gfx *gdlarg, s32 zbufferMode)
{
    Vtx vtx;
    Mtxf *mtx;
    Gfx *gdl;
    Vtx *vertices;
    f32 z;
    f32 y;
    f32 x;
    f32 s0[3];
    f32 s1[3];
    f32 s2[3];
    f32 s3[3];
    s32 frame;
    s32 room;
    struct coord3d *roompos;
    
    if (thing->lifetime <= 0)
    {
        return;
    }
    
    if (thing->age < 0)
    {
        return;
    }
    
    if (!camIsPosInScreen((coord3d *) (&thing->unk10), *((f32 *) (&thing->unk24))))
    {
        return;
    }

    vtx = g_BulletSparkVertexTemplate;
    mtx = currentPlayerGetViewToWorldMtxf();
    gdl = *((Gfx **) gdlarg);
    vertices = dynAllocateVertices(4);
    room = thing->unk06;
    roompos = getRoomPositionByIndex(room);
    vtx.v.cn[0] = ((u8 *) thing)[0x28];
    vtx.v.cn[1] = ((u8 *) thing)[0x29];
    vtx.v.cn[2] = ((u8 *) thing)[0x2a];
    vtx.v.cn[3] = ((u8 *) thing)[0x2b];
    frame = (s32) (((f32) thing->age) * (*(&thing->unk08)));
    
    x = *((f32 *) (&thing->unk10));
    y = *((f32 *) (&thing->unk14));
    z = *((f32 *) (&thing->unk18));
    
    s0[0] = mtx->m[0][0] * thing->unk1c;
    s0[1] = mtx->m[0][1] * thing->unk1c;
    s0[2] = mtx->m[0][2] * thing->unk1c;
    s1[0] = mtx->m[0][0] * thing->unk20;
    s1[1] = mtx->m[0][1] * thing->unk20;
    s1[2] = mtx->m[0][2] * thing->unk20;
    s2[0] = mtx->m[1][0] * thing->unk1c;
    s2[1] = mtx->m[1][1] * thing->unk1c;
    s2[2] = mtx->m[1][2] * thing->unk1c;
    s3[0] = mtx->m[1][0] * thing->unk20;
    s3[1] = mtx->m[1][1] * thing->unk20;
    s3[2] = mtx->m[1][2] * thing->unk20;

    vertices[0] = vtx;
    vertices[1] = vtx;
    vertices[2] = vtx;
    vertices[3] = vtx;
    vertices[0].v.ob[0] = (((x - s0[0]) - s3[0]) * bgGetRoomScale()) - roompos->f[0];
    vertices[0].v.ob[1] = (((y - s0[1]) - s3[1]) * bgGetRoomScale()) - roompos->f[1];
    vertices[0].v.ob[2] = (((z - s0[2]) - s3[2]) * bgGetRoomScale()) - roompos->f[2];
    vertices[0].v.tc[0] = ((struct sImageTableEntry *) (((u8 *) thing->unk0C) + (frame * 12)))->width << 5;
    vertices[0].v.tc[1] = 0;
    vertices[1].v.ob[0] = (((x + s1[0]) - s2[0]) * bgGetRoomScale()) - roompos->f[0];
    vertices[1].v.ob[1] = (((y + s1[1]) - s2[1]) * bgGetRoomScale()) - roompos->f[1];
    vertices[1].v.ob[2] = (((z + s1[2]) - s2[2]) * bgGetRoomScale()) - roompos->f[2];
    vertices[1].v.tc[0] = 0;
    vertices[1].v.tc[1] = 0;
    vertices[2].v.ob[0] = (((x + s0[0]) + s3[0]) * bgGetRoomScale()) - roompos->f[0];
    vertices[2].v.ob[1] = (((y + s0[1]) + s3[1]) * bgGetRoomScale()) - roompos->f[1];
    vertices[2].v.ob[2] = (((z + s0[2]) + s3[2]) * bgGetRoomScale()) - roompos->f[2];
    vertices[2].v.tc[0] = 0;
    vertices[2].v.tc[1] = ((struct sImageTableEntry *) (((u8 *) thing->unk0C) + (frame * 12)))->height << 5;
    vertices[3].v.ob[0] = (((x - s1[0]) + s2[0]) * bgGetRoomScale()) - roompos->f[0];
    vertices[3].v.ob[1] = (((y - s1[1]) + s2[1]) * bgGetRoomScale()) - roompos->f[1];
    vertices[3].v.ob[2] = (((z - s1[2]) + s2[2]) * bgGetRoomScale()) - roompos->f[2];
    vertices[3].v.tc[0] = ((struct sImageTableEntry *) (((u8 *) thing->unk0C) + (frame * 12)))->width << 5;
    vertices[3].v.tc[1] = ((struct sImageTableEntry *) (((u8 *) thing->unk0C) + (frame * 12)))->height << 5;
    
    gSPSetGeometryMode(gdl++, G_CULL_BACK);
    gSPMatrix(gdl++, osVirtualToPhysical((void *) camGetPlayerProjViewMtx()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gdl = applyRoomMatrixToDisplayList(gdl, room);
    texSelect(&gdl, (struct sImageTableEntry *) (((u8 *) thing->unk0C) + (frame * 12)), 4, zbufferMode, 2);
    gSPVertex(gdl++, osVirtualToPhysical(vertices), 4, 0);
    gSP2Triangles(gdl++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPMatrix(gdl++, osVirtualToPhysical(camGetPlayerProjMtx()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    *((Gfx **) gdlarg) = gdl;
}


void fxRenderBulletSparks(Gfx *gdl, s32 zbufferMode)
{

    s_bullet_spark *thing = &g_BulletSparkArray[0]; \
    s_bullet_spark *end = g_BulletSparkArray + BULLET_SPARKS_MAX;

    for (; (thing < end); thing++)
    {
        fxRenderBulletSpark(thing, gdl, zbufferMode);
    }
}


void fxResetMovingSparks(void)
{
    s_moving_bullet_spark *ptr;

    ptr = g_MovingBulletSparkArray;

    for (ptr = &g_MovingBulletSparkArray[0]; ptr < &g_MovingBulletSparkArray[BULLET_MOVING_SPARKS_MAX]; ptr++)
    {
        ptr->unk00.lifetime = 0;
    }
}


void fxUpdateMovingSparks(void)
{
    s_moving_bullet_spark *ptr;
    s_moving_bullet_spark *end;

    ptr = &g_MovingBulletSparkArray[0]; end = &g_MovingBulletSparkArray[BULLET_MOVING_SPARKS_MAX];

    while (ptr < end)
    {
        if (ptr->unk00.lifetime > 0)
        {
            ptr->unk00.age += g_ClockTimer;
            if (ptr->unk00.age >= 0)
            {
                if (ptr->unk00.lifetime > ptr->unk00.age)
                {
                    sub_GAME_7F057D88(&ptr->unk00.unk10, &ptr->velocity, g_GlobalTimerDelta);
                    if (ptr->unk00.unk14 < ptr->unk38)
                    {
                        ptr->unk00.lifetime = 0;
                    }
                }
                else
                {
                    ptr->unk00.lifetime = 0;
                }
            }
        }
        ptr++;
    }
}


void fxRenderMovingSparks(Gfx *arg0, s32 zbufferMode)
{
    s32 max_index;
    s_moving_bullet_spark *ptr;

    max_index = BULLET_MOVING_SPARKS_MAX;

    for (ptr = &g_MovingBulletSparkArray[0]; ptr < (&g_MovingBulletSparkArray[max_index]); ptr++)
    {
        fxRenderBulletSpark(&ptr->unk00, arg0, zbufferMode);
    }

}


void fxResetAllSparks(void)
{
    fxResetBulletSparks();
    fxResetMovingSparks();
}


void fxUpdateAllSparks(void)
{
    fxUpdateBulletSparks();

    // responsible for updating bullet sparks and dust clouds that spawn when shooting at other players
    // these are 2D and always facing the camera
    fxUpdateMovingSparks();
}


void fxRenderAllSparks(Gfx **gdl, s32 zbufferMode)
{
    fxRenderBulletSparks(gdl, zbufferMode);
    fxRenderMovingSparks(gdl, zbufferMode);
}
