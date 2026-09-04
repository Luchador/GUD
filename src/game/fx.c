#include <ultra64.h>
#include <bondtypes.h>
#include <random.h>
#include "bg.h"
#include "bgroomtrans.h"
#include "bondview.h"
#include "cam.h"
#include "dyn.h"
#include "fx.h"
#include "gbi_extension.h"
#include "image_bank.h"
#include "lv.h"
#include "matrixmath.h"
#include "tex.h"


#define BULLET_SPARKS_MAX 20
#define BULLET_MOVING_SPARKS_MAX 50

typedef struct BulletSparkStyle
{
    f32 size;
    rgba_u8 color;
} BulletSparkStyle;

static BulletSpark g_BulletSparks[BULLET_SPARKS_MAX];
static MovingBulletSpark g_MovingBulletSparks[BULLET_MOVING_SPARKS_MAX];

static Vtx g_BulletSparkVertexTemplate;

static const BulletSparkStyle g_BulletSparkStyles[8] = {
    /* SPARK_WHITE      */ { 26.0f, { 0xFF, 0xFF, 0xFF, 0xFF } },
    /* SPARK_STANDARD   */ { 26.0f, { 0xFF, 0xFF, 0xC8, 0xFF } },
    /* SPARK_LASER      */ { 26.0f, { 0xFF, 0x00, 0x00, 0xFF } },
    /* SPARK_WATCHLASER */ { 20.0f, { 0xFF, 0xFF, 0xFF, 0xFF } },
    { 26.0f, { 0xFF, 0xFF, 0xFF, 0xFF } },
    { 26.0f, { 0xFF, 0xFF, 0xFF, 0xFF } },
    { 26.0f, { 0 } },
    { 26.0f, { 0 } }
};


static void fxResetBulletSparks(void)
{
    s32 i;

    for (i = 0; i < BULLET_SPARKS_MAX; i++)
    {
        g_BulletSparks[i].imageFrames = NULL;
        g_BulletSparks[i].lifetime = 0;
        g_BulletSparks[i].age = 0;
    }
}


static void fxInitBulletSpark(BulletSpark *spark, coord3d *position, s32 effectType, f32 size, const rgba_u8 *color, s16 room)
{
    f32 rotationAngle;

    rotationAngle = randomGetNext();
    rotationAngle *= (1.0f / M_U32_MAX_VALUE_F);
    rotationAngle *= M_TAU_F;

    spark->age = 0;
    spark->room = room;

    if (effectType == 4)
    {
        spark->lifetime = 1;
        spark->framesPerTick = 1.0f;
        spark->imageFrames = flareimage2;
    }
    else if (effectType == 1)
    {
        spark->lifetime = 11;
        spark->framesPerTick = 0.5f;
        spark->imageFrames = explosion_smokeimages;
    }
    else if (effectType == 3)
    {
        spark->lifetime = 9;
        spark->framesPerTick = 0.5f;
        spark->imageFrames = scattered_explosions;
    }
    else if (effectType == 6)
    {
        spark->lifetime = 100;
        spark->framesPerTick = 0.0f;
        spark->imageFrames = flareimage2;
    }
    else
    {
        spark->lifetime = 11;
        spark->framesPerTick = 0.5f;
        spark->imageFrames = explosion_smokeimages;
    }

    spark->color = *color;

    spark->position = *position;

    size *= 1.0f + ((f32)randomGetNext() * (1.0f / M_U32_MAX_VALUE_F) * 0.25f);
    size *= M_SQRT2_F;
    spark->radius = size;

    spark->rotationCosRadius = cosf(rotationAngle) * size;
    spark->rotationSinRadius = sinf(rotationAngle) * size;
}


static BulletSpark *fxCreateSpark(coord3d *position, s32 effectType, f32 size, const rgba_u8 *color, s16 room)
{
    BulletSpark *spark;

    for (spark = &g_BulletSparks[0]; spark < &g_BulletSparks[BULLET_SPARKS_MAX]; spark++)
    {
        if (spark->lifetime == 0)
        {
            fxInitBulletSpark(spark, position, effectType, size, color, room);
            return spark;
        }
    }

    return NULL;
}


BulletSpark *fxCreateBulletSpark(coord3d *position, s32 sparkType, s16 room)
{
    const BulletSparkStyle *style = &g_BulletSparkStyles[sparkType];

    return fxCreateSpark(position, sparkType, style->size, &style->color, room);
}


BulletSpark *fxCreateHitPuff(coord3d *position, s32 effectType, f32 size, s16 room)
{
    return fxCreateSpark(position, effectType, size, &g_BulletSparkStyles[effectType].color, room);
}


static void fxUpdateBulletSparks(void)
{
    BulletSpark *spark;
    BulletSpark *end = g_BulletSparks + BULLET_SPARKS_MAX;

    for (spark = &g_BulletSparks[0]; spark < end; spark++)
    {
        if (spark->lifetime > 0)
        {
            spark->age += g_ClockTimer;

            if (spark->age >= spark->lifetime)
            {
                spark->lifetime = 0;
            }
        }
    }
}


static void fxRenderBulletSpark(BulletSpark *spark, Gfx **gdl, s32 zbufferMode)
{
    Vtx vertexTemplate;
    Mtxf *viewToWorld;
    Gfx *displayList;
    Vtx *vertices;
    f32 positionX;
    f32 positionY;
    f32 positionZ;
    f32 rightCosOffset[3];
    f32 rightSinOffset[3];
    f32 upCosOffset[3];
    f32 upSinOffset[3];
    f32 roomScale;
    s32 frameIndex;
    sImageTableEntry *imageFrame;
    coord3d *roomPosition;

    if (spark->lifetime <= 0)
    {
        return;
    }

    if (spark->age < 0)
    {
        return;
    }

    if (!camIsPosInScreen(&spark->position, spark->radius))
    {
        return;
    }

    vertexTemplate = g_BulletSparkVertexTemplate;
    viewToWorld = currentPlayerGetViewToWorldMtxf();
    displayList = *gdl;
    vertices = dynAllocateVertices(4);
    roomPosition = getRoomPositionByIndex(spark->room);
    roomScale = bgGetRoomScale();

    vertexTemplate.v.cn[0] = spark->color.r;
    vertexTemplate.v.cn[1] = spark->color.g;
    vertexTemplate.v.cn[2] = spark->color.b;
    vertexTemplate.v.cn[3] = spark->color.a;

    frameIndex = (s32)((f32)spark->age * spark->framesPerTick);
    imageFrame = &spark->imageFrames[frameIndex];

    positionX = spark->position.x;
    positionY = spark->position.y;
    positionZ = spark->position.z;

    rightCosOffset[0] = viewToWorld->m[0][0] * spark->rotationCosRadius;
    rightCosOffset[1] = viewToWorld->m[0][1] * spark->rotationCosRadius;
    rightCosOffset[2] = viewToWorld->m[0][2] * spark->rotationCosRadius;
    rightSinOffset[0] = viewToWorld->m[0][0] * spark->rotationSinRadius;
    rightSinOffset[1] = viewToWorld->m[0][1] * spark->rotationSinRadius;
    rightSinOffset[2] = viewToWorld->m[0][2] * spark->rotationSinRadius;
    upCosOffset[0] = viewToWorld->m[1][0] * spark->rotationCosRadius;
    upCosOffset[1] = viewToWorld->m[1][1] * spark->rotationCosRadius;
    upCosOffset[2] = viewToWorld->m[1][2] * spark->rotationCosRadius;
    upSinOffset[0] = viewToWorld->m[1][0] * spark->rotationSinRadius;
    upSinOffset[1] = viewToWorld->m[1][1] * spark->rotationSinRadius;
    upSinOffset[2] = viewToWorld->m[1][2] * spark->rotationSinRadius;

    vertices[0] = vertexTemplate;
    vertices[1] = vertexTemplate;
    vertices[2] = vertexTemplate;
    vertices[3] = vertexTemplate;
    vertices[0].v.ob[0] = (((positionX - rightCosOffset[0]) - upSinOffset[0]) * roomScale) - roomPosition->x;
    vertices[0].v.ob[1] = (((positionY - rightCosOffset[1]) - upSinOffset[1]) * roomScale) - roomPosition->y;
    vertices[0].v.ob[2] = (((positionZ - rightCosOffset[2]) - upSinOffset[2]) * roomScale) - roomPosition->z;
    vertices[0].v.tc[0] = imageFrame->width << 5;
    vertices[0].v.tc[1] = 0;
    vertices[1].v.ob[0] = (((positionX + rightSinOffset[0]) - upCosOffset[0]) * roomScale) - roomPosition->x;
    vertices[1].v.ob[1] = (((positionY + rightSinOffset[1]) - upCosOffset[1]) * roomScale) - roomPosition->y;
    vertices[1].v.ob[2] = (((positionZ + rightSinOffset[2]) - upCosOffset[2]) * roomScale) - roomPosition->z;
    vertices[1].v.tc[0] = 0;
    vertices[1].v.tc[1] = 0;
    vertices[2].v.ob[0] = (((positionX + rightCosOffset[0]) + upSinOffset[0]) * roomScale) - roomPosition->x;
    vertices[2].v.ob[1] = (((positionY + rightCosOffset[1]) + upSinOffset[1]) * roomScale) - roomPosition->y;
    vertices[2].v.ob[2] = (((positionZ + rightCosOffset[2]) + upSinOffset[2]) * roomScale) - roomPosition->z;
    vertices[2].v.tc[0] = 0;
    vertices[2].v.tc[1] = imageFrame->height << 5;
    vertices[3].v.ob[0] = (((positionX - rightSinOffset[0]) + upCosOffset[0]) * roomScale) - roomPosition->x;
    vertices[3].v.ob[1] = (((positionY - rightSinOffset[1]) + upCosOffset[1]) * roomScale) - roomPosition->y;
    vertices[3].v.ob[2] = (((positionZ - rightSinOffset[2]) + upCosOffset[2]) * roomScale) - roomPosition->z;
    vertices[3].v.tc[0] = imageFrame->width << 5;
    vertices[3].v.tc[1] = imageFrame->height << 5;

    gSPSetGeometryMode(displayList++, G_CULL_BACK);
    gSPMatrix(displayList++, osVirtualToPhysical((void *)camGetPlayerProjViewMtx()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    displayList = applyRoomMatrixToDisplayList(displayList, spark->room);
    texSelect(&displayList, imageFrame, 4, zbufferMode, 2);
    gSPVertex(displayList++, osVirtualToPhysical(vertices), 4, 0);
    gSP2Triangles(displayList++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPMatrix(displayList++, osVirtualToPhysical(camGetPlayerProjMtx()), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    *gdl = displayList;
}


static void fxRenderBulletSparks(Gfx **gdl, s32 zbufferMode)
{
    BulletSpark *spark;
    BulletSpark *end = g_BulletSparks + BULLET_SPARKS_MAX;

    for (spark = &g_BulletSparks[0]; spark < end; spark++)
    {
        fxRenderBulletSpark(spark, gdl, zbufferMode);
    }
}


static void fxResetMovingSparks(void)
{
    MovingBulletSpark *movingSpark;

    for (movingSpark = &g_MovingBulletSparks[0]; movingSpark < &g_MovingBulletSparks[BULLET_MOVING_SPARKS_MAX]; movingSpark++)
    {
        movingSpark->spark.lifetime = 0;
    }
}


static void fxUpdateMovingSparks(void)
{
    MovingBulletSpark *movingSpark;
    MovingBulletSpark *end = &g_MovingBulletSparks[BULLET_MOVING_SPARKS_MAX];

    for (movingSpark = &g_MovingBulletSparks[0]; movingSpark < end; movingSpark++)
    {
        if (movingSpark->spark.lifetime > 0)
        {
            movingSpark->spark.age += g_ClockTimer;

            if (movingSpark->spark.age >= 0)
            {
                if (movingSpark->spark.age < movingSpark->spark.lifetime)
                {
                    sub_GAME_7F057D88(&movingSpark->spark.position, &movingSpark->velocity, g_GlobalTimerDelta);

                    if (movingSpark->spark.position.y < movingSpark->minimumY)
                    {
                        movingSpark->spark.lifetime = 0;
                    }
                }
                else
                {
                    movingSpark->spark.lifetime = 0;
                }
            }
        }
    }
}


static void fxRenderMovingSparks(Gfx **gdl, s32 zbufferMode)
{
    MovingBulletSpark *movingSpark;
    MovingBulletSpark *end = &g_MovingBulletSparks[BULLET_MOVING_SPARKS_MAX];

    for (movingSpark = &g_MovingBulletSparks[0]; movingSpark < end; movingSpark++)
    {
        fxRenderBulletSpark(&movingSpark->spark, gdl, zbufferMode);
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
    fxUpdateMovingSparks();
}


void fxRenderAllSparks(Gfx **gdl, s32 zbufferMode)
{
    fxRenderBulletSparks(gdl, zbufferMode);
    fxRenderMovingSparks(gdl, zbufferMode);
}
