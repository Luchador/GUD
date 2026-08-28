#include <ultra64.h>
#include <memp.h>
#include "sky.h"
#include "player.h"
#include "dyntex.h"
#include "bondview.h"
#include "lv.h"
#include "environment.h"
#include "bg.h"
#include "fr.h"
#include "image_bank.h"


#define SKYABS(val) (val >= 0.0f ? (val) : -(val))

s32 g_SkyStageNum;
f32 g_SkyCloudOffset = 0;
static Mtxf g_SkyInverseRoomScaleMatrix;


void skyGetWorldPosFromScreenPos(f32 offset_x, f32 offset_y, coord3d* out) {
    Mtxf* player_mtxf;
    coord2d coords;
    f32 screen_top;

    player_mtxf = currentPlayerGetViewToWorldMtxf();
    coords.x = getPlayer_c_screenleft() + offset_x;
    screen_top = getPlayer_c_screentop();
    coords.y = envGetCurrent()->WaterConcavity + (offset_y + screen_top);
    transformAndNormalizeByLength2Dto3D(&coords, out, 100.0f);
    mtx4RotateVecInPlace(player_mtxf, out->f);
}


bool skyIsScreenCornerInSky(coord3d *corner3dpos, coord3d *dstpos, f32 *dstfrac)
{
    coord3d *eye = bondviewGetPlayerPosition();
    f32 f12 = 2.0f * corner3dpos->y / sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2] + 0.0001f);
    f32 sp2c;
    f32 f12_2;
    f32 sp24;

    if (f12 > 1.0f)
    {
        f12 = 1.0f;
    }

    *dstfrac = 1.0f - f12;

    if (corner3dpos->y == 0.0f)
    {
        sp24 = 0.01f;
    }
    else
    {
        sp24 = corner3dpos->y;
    }

    if (sp24 > 0.0f)
    {
        sp2c = (envGetCurrent()->CloudRepeat - eye->y) / sp24;
        f12_2 = sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2]) * sp2c;

        if (f12_2 > 300000)
        {
            sp2c *= 300000 / f12_2;
        }

        dstpos->x = eye->x + sp2c * corner3dpos->f[0];
        dstpos->y = eye->y + sp2c * sp24;
        dstpos->z = eye->z + sp2c * corner3dpos->f[2];

        return TRUE;
    }

    return FALSE;
}


bool skyIsCornerInWater(coord3d *corner3dpos, coord3d *dstpos, f32 *dstfrac)
{
    coord3d *eye = bondviewGetPlayerPosition();
    f32 f12 = -2.0f * corner3dpos->y / sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2] + 0.0001f);
    f32 sp2c;
    f32 f12_2;
    f32 sp24;

    if (f12 > 1.0f)
    {
        f12 = 1.0f;
    }

    *dstfrac = 1.0f - f12;

    if (corner3dpos->y == 0.0f)
    {
        sp24 = -0.01f;
    }
    else
    {
        sp24 = corner3dpos->y;
    }

    if (sp24 < 0.0f)
    {
        sp2c = (envGetCurrent()->WaterRepeat - eye->y) / sp24;
        f12_2 = sqrtf(corner3dpos->f[0] * corner3dpos->f[0] + corner3dpos->f[2] * corner3dpos->f[2]) * sp2c;

        if (f12_2 > 300000)
        {
            sp2c *= 300000 / f12_2;
        }

        dstpos->x = eye->x + sp2c * corner3dpos->f[0];
        dstpos->y = eye->y + sp2c * sp24;
        dstpos->z = eye->z + sp2c * corner3dpos->f[2];

        return TRUE;
    }

    return FALSE;
}


void skyCalculateEdgeVertex(coord3d *base, coord3d* ref, coord3d* out)
{
    f32 mult;

    mult = base->y / (base->y - ref->y);
    out->x = ((ref->x - base->x) * mult) + base->x;
    out->y = 0.0f;
    out->z = ((ref->z - base->z) * mult) + base->z;
}


f32 skyClamp(f32 a, f32 b, f32 c)
{
    if (a < b)
    {
        return b;
    }

    if (c < a)
    {
        return c;
    }

    return a;
}


f32 skyRound(f32 arg0)
{
    return (f32) (s32) (arg0 + 0.5f);
}


void skyChooseCloudVtxColour(SkyRelated18 *arg0, f32 arg1)
{
    f32 r = envGetCurrent()->Red;
    f32 g = envGetCurrent()->Green;
    f32 b = envGetCurrent()->Blue;

    arg0->r = r + envGetCurrent()->CloudRed   * (1.0f - r / 255.0f) * (1.0f - arg1);
    arg0->g = g + envGetCurrent()->CloudGreen * (1.0f - g / 255.0f) * (1.0f - arg1);
    arg0->b = b + envGetCurrent()->CloudBlue  * (1.0f - b / 255.0f) * (1.0f - arg1);

    arg0->a = 0xff;
}


void skyChooseWaterVtxColour(SkyRelated18 *arg0, f32 arg1)
{
    f32 r = envGetCurrent()->Red;
    f32 g = envGetCurrent()->Green;
    f32 b = envGetCurrent()->Blue;

    arg0->r = r + envGetCurrent()->WaterRed   * (1.0f - r / 255.0f) * (1.0f - arg1);
    arg0->g = g + envGetCurrent()->WaterGreen * (1.0f - g / 255.0f) * (1.0f - arg1);
    arg0->b = b + envGetCurrent()->WaterBlue  * (1.0f - b / 255.0f) * (1.0f - arg1);
    arg0->a = 0xff;
}


/*
* Has to do with converting a float to a 32-bits value that's later read in two 16-bits parts
* Possibly converting floats to fixed point
*/
u32 sub_GAME_7F094298(f32 arg0)
{
    u32 result;

    if (arg0 > 32767.9f) 
    { 
        arg0 = 32767.9f; 
    }

    if (arg0 < -32767.9f) 
    { 
        arg0 = -32767.9f; 
    }

    if (arg0 < 0)
    {
        result = arg0 * -65536;
        result = -result;
    }
    else
    {
        result = 65536 * arg0;
    }

    return result;
}


void skySetStageNum(s32 stagenum)
{
  g_SkyStageNum = stagenum;
}


void skyTick(void)
{
    g_SkyCloudOffset += g_ClockTimer;

    if ( g_SkyCloudOffset > 4096.0f)
    {
        g_SkyCloudOffset -= 4096.0f;
    }
}

enum SkyViewportSample
{
    SKY_SAMPLE_TOP_LEFT,
    SKY_SAMPLE_TOP_RIGHT,
    SKY_SAMPLE_BOTTOM_LEFT,
    SKY_SAMPLE_BOTTOM_RIGHT,
    SKY_SAMPLE_TOP_EDGE,
    SKY_SAMPLE_RIGHT_EDGE,
    SKY_SAMPLE_BOTTOM_EDGE,
    SKY_SAMPLE_LEFT_EDGE,
    SKY_SAMPLE_COUNT
};

typedef struct SkyPlaneSample
{
    coord3d cloudPosition;
    coord3d waterPosition;
    f32 cloudFade;
    f32 waterFade;
} SkyPlaneSample;

/*
 * Each row starts with the vertex count, followed by indices into the viewport
 * samples above. A count of -1 marks the two diagonally split masks which a
 * planar viewport cannot produce.
 *
 * The order deliberately matches the original switch statements. The render
 * code relies on this ordering when it splits each polygon into triangles.
 */
static const s8 g_SkyWaterPolygonOrder[16][6] =
{
    { 4, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_BOTTOM_RIGHT, 0 },
    { 5, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_RIGHT_EDGE,   SKY_SAMPLE_BOTTOM_EDGE },
    { 5, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_EDGE,  SKY_SAMPLE_LEFT_EDGE },
    { 4, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_LEFT_EDGE,    SKY_SAMPLE_RIGHT_EDGE,   0 },
    { 5, SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_EDGE,     SKY_SAMPLE_RIGHT_EDGE },
    { 4, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_BOTTOM_EDGE,  SKY_SAMPLE_TOP_EDGE,     0 },
    { -1, 0, 0, 0, 0, 0 },
    { 3, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_EDGE,     SKY_SAMPLE_LEFT_EDGE,    0,                       0 },
    { 5, SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_LEFT_EDGE,    SKY_SAMPLE_TOP_EDGE },
    { -1, 0, 0, 0, 0, 0 },
    { 4, SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_TOP_EDGE,     SKY_SAMPLE_BOTTOM_EDGE,  0 },
    { 3, SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_RIGHT_EDGE,   SKY_SAMPLE_TOP_EDGE,     0,                       0 },
    { 4, SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_RIGHT_EDGE,   SKY_SAMPLE_LEFT_EDGE,    0 },
    { 3, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_LEFT_EDGE,    SKY_SAMPLE_BOTTOM_EDGE,  0,                       0 },
    { 3, SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_EDGE,  SKY_SAMPLE_RIGHT_EDGE,   0,                       0 },
    { 0, 0, 0, 0, 0, 0 }
};

static const s8 g_SkyCloudPolygonOrder[16][6] =
{
    { 0, 0, 0, 0, 0, 0 },
    { 3, SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_EDGE,  SKY_SAMPLE_RIGHT_EDGE,   0,                       0 },
    { 3, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_LEFT_EDGE,    SKY_SAMPLE_BOTTOM_EDGE,  0,                       0 },
    { 4, SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_RIGHT_EDGE,   SKY_SAMPLE_LEFT_EDGE,    0 },
    { 3, SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_RIGHT_EDGE,   SKY_SAMPLE_TOP_EDGE,     0,                       0 },
    { 4, SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_TOP_EDGE,     SKY_SAMPLE_BOTTOM_EDGE,  0 },
    { -1, 0, 0, 0, 0, 0 },
    { 5, SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_LEFT_EDGE,    SKY_SAMPLE_TOP_EDGE },
    { 3, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_EDGE,     SKY_SAMPLE_LEFT_EDGE,    0,                       0 },
    { -1, 0, 0, 0, 0, 0 },
    { 4, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_BOTTOM_EDGE,  SKY_SAMPLE_TOP_EDGE,     0 },
    { 5, SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_EDGE,     SKY_SAMPLE_RIGHT_EDGE },
    { 4, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_LEFT_EDGE,    SKY_SAMPLE_RIGHT_EDGE,   0 },
    { 5, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_RIGHT, SKY_SAMPLE_BOTTOM_EDGE,  SKY_SAMPLE_LEFT_EDGE },
    { 5, SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_RIGHT_EDGE,   SKY_SAMPLE_BOTTOM_EDGE },
    { 4, SKY_SAMPLE_TOP_LEFT,     SKY_SAMPLE_TOP_RIGHT,    SKY_SAMPLE_BOTTOM_LEFT,  SKY_SAMPLE_BOTTOM_RIGHT, 0 }
};

static void skySetWaterVertex(SkyRelated18 *vertex, coord3d *position, f32 fade, f32 roomScale)
{
    vertex->unk00 = position->x * roomScale;
    vertex->unk04 = position->y * roomScale;
    vertex->unk08 = position->z * roomScale;
    vertex->unk0c = position->x;
    vertex->unk10 = position->z + g_SkyCloudOffset;
    skyChooseWaterVtxColour(vertex, fade);
}

static void skySetCloudVertex(SkyRelated18 *vertex, coord3d *position, f32 fade, f32 roomScale)
{
    vertex->unk00 = position->x * roomScale;
    vertex->unk04 = position->y * roomScale;
    vertex->unk08 = position->z * roomScale;
    vertex->unk0c = position->x * 0.1f;
    vertex->unk10 = position->z * 0.1f + g_SkyCloudOffset;
    skyChooseCloudVtxColour(vertex, fade);
}

static s32 skyBuildWaterPolygon(SkyRelated18 *vertices, SkyPlaneSample *samples, s32 horizonMask, f32 roomScale)
{
    const s8 *order = g_SkyWaterPolygonOrder[horizonMask];
    s32 vertexCount = order[0];
    s32 i;

    for (i = 0; i < vertexCount; i++)
    {
        SkyPlaneSample *sample = &samples[order[i + 1]];
        skySetWaterVertex(&vertices[i], &sample->waterPosition, sample->waterFade, roomScale);
    }

    return vertexCount;
}

static s32 skyBuildCloudPolygon(SkyRelated18 *vertices, SkyPlaneSample *samples, s32 horizonMask, f32 roomScale)
{
    const s8 *order = g_SkyCloudPolygonOrder[horizonMask];
    s32 vertexCount = order[0];
    s32 i;

    for (i = 0; i < vertexCount; i++)
    {
        SkyPlaneSample *sample = &samples[order[i + 1]];
        skySetCloudVertex(&vertices[i], &sample->cloudPosition, sample->cloudFade, roomScale);
    }

    return vertexCount;
}

static Gfx *skyRenderSolidBackground(Gfx *gdl, struct CurrentEnvironmentRecord *env)
{
    if (getPlayerCount() == 1)
    {
        gDPSetCycleType(gdl++, G_CYC_FILL);
        gdl = viSetFillColor(gdl, env->Red, env->Green, env->Blue);
        gDPFillRectangle(gdl++, viGetViewLeft(), viGetViewTop(),
                viGetViewLeft() + viGetViewWidth() - 1,
                viGetViewTop() + viGetViewHeight() - 1);
    }
    else
    {
        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_FILL);
        gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
        gDPFillRectangle(gdl++,
                g_CurrentPlayer->viewleft, g_CurrentPlayer->viewtop,
                g_CurrentPlayer->viewleft + g_CurrentPlayer->viewx - 1,
                g_CurrentPlayer->viewtop + g_CurrentPlayer->viewy - 1);
    }

    gDPPipeSync(gdl++);
    return gdl;
}

static void skyProjectVertices(SkyRelated18 *vertices, SkyRelated38 *projected, s32 vertexCount, f32 roomScale, bool moveInteriorVerticesUp)
{
    Mtxf worldToClip;
    Mtxf scaledWorldToClip;
    s32 i;

    matrix_4x4_multiply(currentPlayerGetProjectionMatrixF(), camGetWorldToScreenMtxf(), &worldToClip);
    guScaleF(g_SkyInverseRoomScaleMatrix.m, 1.0f / roomScale, 1.0f / roomScale, 1.0f / roomScale);
    matrix_4x4_multiply(&worldToClip, &g_SkyInverseRoomScaleMatrix, &scaledWorldToClip);

    for (i = 0; i < vertexCount; i++)
    {
        skyProjectVertex(&vertices[i], &scaledWorldToClip, 130, 65535.0f, 65535.0f, &projected[i]);

        projected[i].unk28 = skyClamp(projected[i].unk28,
                getPlayer_c_screenleft() * 4.0f,
                (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f);
        projected[i].unk2c = skyClamp(projected[i].unk2c,
                getPlayer_c_screentop() * 4.0f,
                (getPlayer_c_screentop() + getPlayer_c_screenheight()) * 4.0f - 1.0f);

        if (moveInteriorVerticesUp
                && projected[i].unk2c > getPlayer_c_screentop() * 4.0f + 4.0f
                && projected[i].unk2c < (getPlayer_c_screentop() + getPlayer_c_screenheight()) * 4.0f - 4.0f)
        {
            projected[i].unk2c -= 4.0f;
        }
    }
}

static Gfx *skyRenderWaterPolygon(Gfx *gdl, SkyRelated18 *vertices, s32 vertexCount,
        f32 roomScale, bool closeHorizonSeam, struct CurrentEnvironmentRecord *env)
{
    SkyRelated38 projected[5];
    s32 i;

    skyProjectVertices(vertices, projected, vertexCount, roomScale, TRUE);

    if (!env->IsWater)
    {
        f32 minX = 1279.0f;
        f32 maxX = 0.0f;
        f32 minY = 959.0f;
        f32 maxY = 0.0f;

        for (i = 0; i < vertexCount; i++)
        {
            if (projected[i].unk28 < minX) { minX = projected[i].unk28; }
            if (projected[i].unk28 > maxX) { maxX = projected[i].unk28; }
            if (projected[i].unk2c < minY) { minY = projected[i].unk2c; }
            if (projected[i].unk2c > maxY) { maxY = projected[i].unk2c; }
        }

        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_FILL);
        gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
        gDPSetTexturePersp(gdl++, G_TP_NONE);
        gDPFillRectangle(gdl++, (s32)(minX * 0.25f), (s32)(minY * 0.25f),
                (s32)(maxX * 0.25f), (s32)(maxY * 0.25f));
        gDPPipeSync(gdl++);
        gDPSetTexturePersp(gdl++, G_TP_PERSP);
        return gdl;
    }

    gDPPipeSync(gdl++);
    texSelect(&gdl, &skywaterimages[env->WaterImageId], 1, 0, 2);
    gdl = dyntexConfigureTwoLayerWater(gdl, FALSE);
    gDPSetRenderMode(gdl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

    if (vertexCount == 4)
    {
        gdl = skyRenderTri(gdl, &projected[0], &projected[1], &projected[3], 130.0f, TRUE);

        if (closeHorizonSeam)
        {
            for (i = 0; i < 4; i++)
            {
                projected[i].unk2c++;
            }
        }

        gdl = skyRenderTri(gdl, &projected[3], &projected[2], &projected[0], 130.0f, TRUE);
    }
    else if (vertexCount == 5)
    {
        gdl = skyRenderTri(gdl, &projected[0], &projected[1], &projected[2], 130.0f, TRUE);
        gdl = skyRenderTri(gdl, &projected[0], &projected[2], &projected[3], 130.0f, TRUE);
        gdl = skyRenderTri(gdl, &projected[0], &projected[3], &projected[4], 130.0f, TRUE);
    }
    else if (vertexCount == 3)
    {
        gdl = skyRenderTri(gdl, &projected[0], &projected[1], &projected[2], 130.0f, TRUE);
    }

    return gdl;
}

static Gfx *skyRenderCloudPolygon(Gfx *gdl, SkyRelated18 *vertices, s32 vertexCount, f32 roomScale, s32 horizonMask, f32 leftHorizonY, f32 rightHorizonY, struct CurrentEnvironmentRecord *env)
{
    SkyRelated38 projected[5];

    gDPPipeSync(gdl++);
    texSelect(&gdl, &skywaterimages[env->SkyImageId], 1, 0, 2);
    gDPSetEnvColor(gdl++, env->Red, env->Green, env->Blue, 0xff);
    gDPSetCombineLERP(gdl++,
            SHADE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, SHADE,
            SHADE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, SHADE);

    skyProjectVertices(vertices, projected, vertexCount, roomScale, FALSE);

    if (vertexCount == 4)
    {
        if (horizonMask == 12)
        {
            if (rightHorizonY < leftHorizonY)
            {
                if (projected[3].unk2c >= projected[1].unk2c + 4.0f)
                {
                    projected[0].unk28 = getPlayer_c_screenleft() * 4.0f;
                    projected[0].unk2c = getPlayer_c_screentop() * 4.0f;
                    projected[1].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;
                    projected[1].unk2c = getPlayer_c_screentop() * 4.0f;
                    projected[2].unk28 = getPlayer_c_screenleft() * 4.0f;
                    projected[3].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;
                    gdl = skyRenderFull(gdl, &projected[0], &projected[1], &projected[2], &projected[3], 130.0f);
                }
                else
                {
                    gdl = skyRenderTri(gdl, &projected[0], &projected[1], &projected[2], 130.0f, TRUE);
                }
            }
            else if (projected[2].unk2c >= projected[0].unk2c + 4.0f)
            {
                projected[0].unk28 = getPlayer_c_screenleft() * 4.0f;
                projected[0].unk2c = getPlayer_c_screentop() * 4.0f;
                projected[1].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;
                projected[1].unk2c = getPlayer_c_screentop() * 4.0f;
                projected[2].unk28 = getPlayer_c_screenleft() * 4.0f;
                projected[3].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;
                gdl = skyRenderFull(gdl, &projected[1], &projected[0], &projected[3], &projected[2], 130.0f);
            }
            else
            {
                gdl = skyRenderTri(gdl, &projected[1], &projected[0], &projected[3], 130.0f, TRUE);
            }
        }
        else
        {
            gdl = skyRenderTri(gdl, &projected[0], &projected[1], &projected[3], 130.0f, TRUE);
            gdl = skyRenderTri(gdl, &projected[3], &projected[2], &projected[0], 130.0f, TRUE);
        }
    }
    else if (vertexCount == 5)
    {
        gdl = skyRenderTri(gdl, &projected[0], &projected[1], &projected[2], 130.0f, TRUE);
        gdl = skyRenderTri(gdl, &projected[0], &projected[2], &projected[3], 130.0f, TRUE);
        gdl = skyRenderTri(gdl, &projected[0], &projected[3], &projected[4], 130.0f, TRUE);
    }
    else if (vertexCount == 3)
    {
        gdl = skyRenderTri(gdl, &projected[0], &projected[1], &projected[2], 130.0f, TRUE);
    }

    return gdl;
}


Gfx *skyRender(Gfx *gdl)
{
    coord3d viewRays[4];
    coord3d horizonRay;
    SkyPlaneSample samples[SKY_SAMPLE_COUNT];
    SkyRelated18 vertices[5];
    struct CurrentEnvironmentRecord *env;
    f32 roomScale;
    f32 leftHorizonY = 0.0f;
    f32 rightHorizonY = 0.0f;
    s32 cornerInSky[4];
    s32 horizonMask;
    s32 vertexCount;
    s32 i;

    roomScale = bgGetRoomScale() / 30.0f;
    env = envGetCurrent();

    if (!env->Clouds)
    {
        return skyRenderSolidBackground(gdl, env);
    }

    gdl = viSetFillColor(gdl, env->Red, env->Green, env->Blue);

    skyGetWorldPosFromScreenPos(0.0f, 0.0f, &viewRays[SKY_SAMPLE_TOP_LEFT]);
    skyGetWorldPosFromScreenPos(getPlayer_c_screenwidth() - 0.1f, 0.0f, &viewRays[SKY_SAMPLE_TOP_RIGHT]);
    skyGetWorldPosFromScreenPos(0.0f, getPlayer_c_screenheight() - 0.1f, &viewRays[SKY_SAMPLE_BOTTOM_LEFT]);
    skyGetWorldPosFromScreenPos(getPlayer_c_screenwidth() - 0.1f, getPlayer_c_screenheight() - 0.1f, &viewRays[SKY_SAMPLE_BOTTOM_RIGHT]);

    for (i = 0; i < 4; i++)
    {
        cornerInSky[i] = skyIsScreenCornerInSky(&viewRays[i], &samples[i].cloudPosition, &samples[i].cloudFade);
    }

    for (i = 0; i < 4; i++)
    {
        skyIsCornerInWater(&viewRays[i], &samples[i].waterPosition, &samples[i].waterFade);
    }

    if (cornerInSky[SKY_SAMPLE_TOP_LEFT] != cornerInSky[SKY_SAMPLE_BOTTOM_LEFT])
    {
        leftHorizonY = getPlayer_c_screentop() + getPlayer_c_screenheight() * (viewRays[SKY_SAMPLE_TOP_LEFT].y / (viewRays[SKY_SAMPLE_TOP_LEFT].y - viewRays[SKY_SAMPLE_BOTTOM_LEFT].y));
        skyCalculateEdgeVertex(&viewRays[SKY_SAMPLE_TOP_LEFT], &viewRays[SKY_SAMPLE_BOTTOM_LEFT], &horizonRay);
        skyIsScreenCornerInSky(&horizonRay, &samples[SKY_SAMPLE_LEFT_EDGE].cloudPosition, &samples[SKY_SAMPLE_LEFT_EDGE].cloudFade);
        skyIsCornerInWater(&horizonRay, &samples[SKY_SAMPLE_LEFT_EDGE].waterPosition, &samples[SKY_SAMPLE_LEFT_EDGE].waterFade);
    }

    if (cornerInSky[SKY_SAMPLE_TOP_RIGHT] != cornerInSky[SKY_SAMPLE_BOTTOM_RIGHT])
    {
        rightHorizonY = getPlayer_c_screentop() + getPlayer_c_screenheight() * (viewRays[SKY_SAMPLE_TOP_RIGHT].y / (viewRays[SKY_SAMPLE_TOP_RIGHT].y - viewRays[SKY_SAMPLE_BOTTOM_RIGHT].y));
        skyCalculateEdgeVertex(&viewRays[SKY_SAMPLE_TOP_RIGHT], &viewRays[SKY_SAMPLE_BOTTOM_RIGHT], &horizonRay);
        skyIsScreenCornerInSky(&horizonRay, &samples[SKY_SAMPLE_RIGHT_EDGE].cloudPosition, &samples[SKY_SAMPLE_RIGHT_EDGE].cloudFade);
        skyIsCornerInWater(&horizonRay, &samples[SKY_SAMPLE_RIGHT_EDGE].waterPosition, &samples[SKY_SAMPLE_RIGHT_EDGE].waterFade);
    }

    if (cornerInSky[SKY_SAMPLE_TOP_LEFT] != cornerInSky[SKY_SAMPLE_TOP_RIGHT])
    {
        skyCalculateEdgeVertex(&viewRays[SKY_SAMPLE_TOP_LEFT], &viewRays[SKY_SAMPLE_TOP_RIGHT], &horizonRay);
        skyIsScreenCornerInSky(&horizonRay, &samples[SKY_SAMPLE_TOP_EDGE].cloudPosition, &samples[SKY_SAMPLE_TOP_EDGE].cloudFade);
        skyIsCornerInWater(&horizonRay, &samples[SKY_SAMPLE_TOP_EDGE].waterPosition, &samples[SKY_SAMPLE_TOP_EDGE].waterFade);
    }

    if (cornerInSky[SKY_SAMPLE_BOTTOM_LEFT] != cornerInSky[SKY_SAMPLE_BOTTOM_RIGHT])
    {
        skyCalculateEdgeVertex(&viewRays[SKY_SAMPLE_BOTTOM_LEFT], &viewRays[SKY_SAMPLE_BOTTOM_RIGHT], &horizonRay);
        skyIsScreenCornerInSky(&horizonRay, &samples[SKY_SAMPLE_BOTTOM_EDGE].cloudPosition, &samples[SKY_SAMPLE_BOTTOM_EDGE].cloudFade);
        skyIsCornerInWater(&horizonRay, &samples[SKY_SAMPLE_BOTTOM_EDGE].waterPosition, &samples[SKY_SAMPLE_BOTTOM_EDGE].waterFade);
    }

    horizonMask = (cornerInSky[SKY_SAMPLE_TOP_LEFT] << 3)
            | (cornerInSky[SKY_SAMPLE_TOP_RIGHT] << 2)
            | (cornerInSky[SKY_SAMPLE_BOTTOM_LEFT] << 1)
            | cornerInSky[SKY_SAMPLE_BOTTOM_RIGHT];

    vertexCount = skyBuildWaterPolygon(vertices, samples, horizonMask, roomScale);

    if (vertexCount < 0)
    {
        return gdl;
    }

    if (vertexCount > 0)
    {
        gdl = skyRenderWaterPolygon(gdl, vertices, vertexCount, roomScale, horizonMask == 12, env);
    }

    vertexCount = skyBuildCloudPolygon(vertices, samples, horizonMask, roomScale);

    if (vertexCount <= 0)
    {
        return gdl;
    }

    return skyRenderCloudPolygon(gdl, vertices, vertexCount, roomScale, horizonMask, leftHorizonY, rightHorizonY, env);
}


void skyProjectVertex(SkyRelated18 *arg0, Mtxf *arg1, u16 arg2, f32 arg3, f32 arg4, SkyRelated38 *arg5)
{
    f32 sp68[4];
    f32 sp64;
    f32 sp60;
    f32 f22;
    f32 f0;
    f32 sp48[4];
    f32 sp38[4];
    f32 mult;

    mult = arg2 / 65536.0f;

    sp68[0] = (arg0->unk00 * arg1->m[0][0] + arg0->unk04 * arg1->m[1][0] + arg0->unk08 * arg1->m[2][0]) + arg1->m[3][0];
    sp68[1] = (arg0->unk00 * arg1->m[0][1] + arg0->unk04 * arg1->m[1][1] + arg0->unk08 * arg1->m[2][1]) + arg1->m[3][1];
    sp68[2] = (arg0->unk00 * arg1->m[0][2] + arg0->unk04 * arg1->m[1][2] + arg0->unk08 * arg1->m[2][2]) + arg1->m[3][2];
    sp68[3] = (arg0->unk00 * arg1->m[0][3] + arg0->unk04 * arg1->m[1][3] + arg0->unk08 * arg1->m[2][3]) + arg1->m[3][3];

    sp60 = arg0->unk0c * (arg3 / 65536.0f);
    sp64 = arg0->unk10 * (arg4 / 65536.0f);

    if (sp68[3] == 0.0f)
    {
        f22 = 32767.0f;
    }
    else
    {
        f22 = 1.0f / (sp68[3] * mult);
    }

    f0 = f22;

    if (f0 < 0.0f) 
    { 
        f0 = 32767.0f; 
    }

    sp48[0] = sp68[0] * f0 * mult;
    sp48[1] = sp68[1] * f0 * mult;
    sp48[2] = sp68[2] * f0 * mult;
    sp48[3] = sp68[3] * f0 * mult;

    sp38[0] = sp48[0] * (getPlayer_c_screenwidth() * 2) + (getPlayer_c_screenwidth() * 2 + getPlayer_c_screenleft() * 4);
    sp38[1] = -sp48[1] * (getPlayer_c_screenheight() * 2) + (getPlayer_c_screenheight() * 2 + getPlayer_c_screentop() * 4);

    sp38[2] = sp48[2] * 511.0f + 511.0f;
    sp38[3] = sp48[3] * 0 + 0;

    sp38[0] = skyClamp(sp38[0], -4090.0f, 4090.0f);
    sp38[1] = skyClamp(sp38[1], -4090.0f, 4090.0f);
    sp38[2] = skyClamp(sp38[2], 0.0f, 32767.0f);
    sp38[3] = skyClamp(sp38[3], 0.0f, 32767.0f);

    arg5->unk00 = sp68[0];
    arg5->unk04 = sp68[1];
    arg5->unk08 = sp68[2];
    arg5->unk0c = sp68[3];
    arg5->unk20 = sp60;
    arg5->unk24 = sp64;
    arg5->unk28 = sp38[0];
    arg5->unk2c = sp38[1] - envGetCurrent()->WaterConcavity * 4.0f;
    arg5->unk30 = sp38[2];
    arg5->unk34 = f22;

    arg5->r = arg0->r;
    arg5->g = arg0->g;
    arg5->b = arg0->b;
    arg5->a = arg0->a;
}


bool skyVerticesAreTheSame(SkyRelated38 *arg0, SkyRelated38 *arg1)
{
    f32 f0;
    f32 f1;

    f0 = arg0->unk28 - arg1->unk28;
    f1 = arg0->unk2c - arg1->unk2c;

    return sqrtf((f0 * f0) + (f1 * f1)) < 1.0f ? TRUE : FALSE;
}


Gfx *skyRenderTri(Gfx *gdl, SkyRelated38 *arg1, SkyRelated38 *arg2, SkyRelated38 *arg3, f32 arg4, bool textured)
{
    SkyRelated38 *sp484;
    SkyRelated38 *sp480;
    SkyRelated38 *sp47c;
    s32 i;
    f32 sp474;
    f32 sp470;
    f32 sp46c;
    f32 sp468;
    f32 sp464;
    f32 sp460;

    f32 sp45c[1];
    f32 sp458[1];
    f32 sp454[1];
    f32 sp450[1];
    f32 sp44c[1];
    f32 sp448[1];

    f32 sp444;
    f32 sp440;

    f32 sp43c[1];
    f32 sp438[1];
    f32 sp434[1];
    f32 sp430[1];
    f32 sp42c[1];
    f32 sp428[1];
    f32 sp424[1];
    f32 sp420[1];

    f32 sp41c;
    f32 sp418;
    f32 sp414;
    f32 sp410;
    f32 sp40c;
    f32 sp408;
    f32 sp404;
    f32 sp400;

    f32 sp3fc[1];
    f32 sp3f8[1];
    f32 sp3f4[1];
    f32 sp3f0[1];
    f32 sp3ec[1];
    f32 sp3e8[1];
    f32 sp3e4[1];
    f32 sp3e0[1];

    f32 sp3dc[1];
    f32 sp3d8[1];
    f32 sp3d4[1];
    f32 sp3d0[1];
    f32 sp3cc[1];
    f32 sp3c8[1];
    f32 sp3c4[1];
    f32 sp3c0[1];

    f32 sp3bc[1];
    f32 sp3b8[1];
    f32 sp3b4[1];
    f32 sp3b0[1];
    f32 sp3ac[1];
    f32 sp3a8[1];
    f32 sp3a4[1];
    f32 sp3a0[1];
    f32 sp39c[1];
    f32 sp398[1];
    f32 sp394[1];
    f32 sp390[1];
    f32 sp38c[1];
    f32 sp388[1];
    f32 sp384[1];
    f32 sp380[1];
    f32 sp37c;
    f32 sp378;
    f32 sp374[1];
    f32 sp370[1];
    f32 sp36c[1];
    f32 sp368;
    f32 sp364[1];
    f32 sp360[1];
    f32 sp35c[1];
    f32 sp358[1];
    f32 sp354[1];
    f32 sp350[1];
    f32 sp34c[1];
    f32 sp348[1];
    f32 sp344[1];
    f32 sp340[1];
    f32 sp33c[1];
    f32 sp338[1];
    f32 sp334[1];
    f32 sp330[1];
    f32 sp310[8];
    f32 sp2f0[8];
    f32 sp2d0[8];
    f32 sp2b0[8];
    f32 sp290[8];
    f32 sp270[8];
    f32 sp250[8];
    f32 sp230[8];
    f32 sp210[8];
    f32 f2;
    f32 sp208[1];
    f32 sp204[1];
    f32 sp200[1];
    u32 stack[4];
    f32 sp1d0[8];
    f32 sp1b0[8];
    u32 stack2;
    f32 sp1a8[1];
    f32 sp1a4[1];
    f32 sp1a0[1];
    SkyRelated38 *swap1;
    SkyRelated38 *swap2;
    SkyRelated38 *swap3;
    f32 sp190[1];
    u32 stack3;

    if (skyVerticesAreTheSame(arg1, arg2) || skyVerticesAreTheSame(arg2, arg3) || skyVerticesAreTheSame(arg3, arg1))
    {
        return gdl;
    }

    sp378 = arg4 / 65536.0f;

    sp474 = arg2->unk28 - arg1->unk28;
    sp470 = arg2->unk2c - arg1->unk2c;
    sp46c = arg3->unk28 - arg1->unk28;
    sp468 = arg3->unk2c - arg1->unk2c;

    sp444 = ((sp46c * sp470) - (sp474 * sp468)) / 65536.0f;

    if (sp444 == 0.0f)
    {
        return gdl;
    }

    sp440 = 1.0f / sp444;

    sp484 = arg1;
    sp480 = arg2;
    sp47c = arg3;

    if (sp480->unk2c < sp484->unk2c)
    {
        swap1 = sp480;
        sp480 = sp484;
        sp484 = swap1;

        sp444 *= -1.0f;
        sp440 *= -1.0f;
    }

    if (sp47c->unk2c < sp480->unk2c)
    {
        swap2 = sp480;
        sp480 = sp47c;
        sp47c = swap2;

        sp444 *= -1.0f;
        sp440 *= -1.0f;
    }

    if (sp480->unk2c < sp484->unk2c)
    {
        swap3 = sp480;
        sp480 = sp484;
        sp484 = swap3;

        sp444 *= -1.0f;
        sp440 *= -1.0f;
    }

    sp420[0] = sp480->unk28 * 0.25f;
    sp424[0] = 0.0f;
    sp428[0] = sp484->unk28 * 0.25f;
    sp42c[0] = 0.0f;
    sp430[0] = sp484->unk28 * 0.25f;
    sp434[0] = 0.0f;
    sp43c[0] = 0.0f;
    sp438[0] = 0.0f;

    sp448[0] = sp47c->unk28;
    sp44c[0] = sp47c->unk2c;
    sp450[0] = sp480->unk28;
    sp454[0] = sp480->unk2c;
    sp458[0] = sp484->unk28;
    sp45c[0] = sp484->unk2c;

    sp474 = sp450[0] - sp458[0];
    sp470 = sp454[0] - sp45c[0];
    sp46c = sp448[0] - sp458[0];
    sp468 = sp44c[0] - sp45c[0];
    sp464 = sp448[0] - sp450[0];
    sp460 = sp44c[0] - sp454[0];

    sp3fc[0] = 0.0f;
    sp3f8[0] = 0.0f;
    sp3dc[0] = 0.0f;
    sp3d8[0] = 0.0f;
    sp3a0[0] = 0.0f;
    sp3a8[0] = 0.0f;
    sp3b0[0] = 0.0f;
    sp3bc[0] = 0.0f;
    sp3b8[0] = 0.0f;
    sp380[0] = 0.0f;
    sp388[0] = 0.0f;
    sp390[0] = 0.0f;
    sp39c[0] = 0.0f;
    sp398[0] = 0.0f;

    sp3e0[0] = sp464 / 4.0f;
    sp3e4[0] = sp460 / 4.0f;
    sp3e8[0] = sp474 / 4.0f;
    sp3ec[0] = sp470 / 4.0f;
    sp3f0[0] = sp46c / 4.0f;
    sp3f4[0] = sp468 / 4.0f;

    sp3c0[0] = sp464 * 4.0f;
    sp3c4[0] = sp460 * 4.0f;
    sp3c8[0] = sp474 * 4.0f;
    sp3cc[0] = sp470 * 4.0f;
    sp3d0[0] = sp46c * 4.0f;
    sp3d4[0] = sp468 * 4.0f;

    sp3a4[0] = 4.0f / sp460;
    sp3ac[0] = 4.0f / sp470;
    sp3b4[0] = 4.0f / sp468;

    sp384[0] = sp464 / sp460;
    sp38c[0] = sp474 / sp470;
    sp394[0] = sp46c / sp468;

    sp384[0] = skyClamp(sp384[0], -1878.0f, 1877.0f);
    sp38c[0] = skyClamp(sp38c[0], -1878.0f, 1877.0f);
    sp394[0] = skyClamp(sp394[0], -1878.0f, 1877.0f);

    f2 = (sp484->unk2c * 0.25f);
    sp37c = f2 - (s32) f2;
    sp408 = sp428[0] - skyRound(sp38c[0] * 8192.0f) * (1.0f / 8192.0f) * sp37c;
    sp410 = sp430[0] - skyRound(sp394[0] * 8192.0f) * (1.0f / 8192.0f) * sp37c;

    gImmp1(gdl++, G_RDPHALF_1, (textured ? (G_TRI_SHADE_TXTR << 24) : (G_TRI_FILL << 24))
            | (sp444 < 0.0f ? 0x00800000 : 0)
            | (s32) sp47c->unk2c);
    gImmp1(gdl++, G_RDPHALF_CONT, (s32) sp480->unk2c << 16 | (s32) sp484->unk2c);

    gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(sp480->unk28 * 0.25f));
    gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(sp384[0]));

    gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(sp410));
    gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(sp394[0]));

    gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(sp408));
    gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(sp38c[0]));

    if (!textured)
    {
        return gdl;
    }

    sp36c[0] = sp484->unk0c * sp378;
    sp370[0] = sp480->unk0c * sp378;
    sp374[0] = sp47c->unk0c * sp378;

    sp368 = sp36c[0];

    if (sp370[0] < sp368) { sp368 = sp370[0]; }
    if (sp374[0] < sp368) { sp368 = sp374[0]; }

    sp368 *= 0.5f;

    sp35c[0] = sp484->unk34 * sp368;
    sp360[0] = sp480->unk34 * sp368;
    sp364[0] = sp47c->unk34 * sp368;

    sp338[0] = sp35c[0] * sp484->unk20;
    sp33c[0] = sp35c[0] * sp484->unk24;
    sp340[0] = sp35c[0] * 32767.0f;
    sp344[0] = sp360[0] * sp480->unk20;
    sp348[0] = sp360[0] * sp480->unk24;
    sp34c[0] = sp360[0] * 32767.0f;
    sp350[0] = sp364[0] * sp47c->unk20;
    sp354[0] = sp364[0] * sp47c->unk24;
    sp358[0] = sp364[0] * 32767.0f;

    sp330[0] = SKYABS(sp338[0]);
    sp334[0] = SKYABS(sp33c[0]);

    if (sp330[0] < SKYABS(sp344[0])) { sp330[0] = SKYABS(sp344[0]); }
    if (sp334[0] < SKYABS(sp348[0])) { sp334[0] = SKYABS(sp348[0]); }
    if (sp330[0] < SKYABS(sp350[0])) { sp330[0] = SKYABS(sp350[0]); }
    if (sp334[0] < SKYABS(sp354[0])) { sp334[0] = SKYABS(sp354[0]); }

    sp310[0] = sp484->r + 0.5f;
    sp310[1] = sp484->g + 0.5f;
    sp310[2] = sp484->b + 0.5f;
    sp310[3] = sp484->a + 0.5f;

    sp2f0[0] = sp480->r + 0.5f;
    sp2f0[1] = sp480->g + 0.5f;
    sp2f0[2] = sp480->b + 0.5f;
    sp2f0[3] = sp480->a + 0.5f;

    sp2d0[0] = sp47c->r + 0.5f;
    sp2d0[1] = sp47c->g + 0.5f;
    sp2d0[2] = sp47c->b + 0.5f;
    sp2d0[3] = sp47c->a + 0.5f;

    sp310[4] = sp338[0]; sp310[5] = sp33c[0]; sp310[6] = sp340[0];
    sp2f0[4] = sp344[0]; sp2f0[5] = sp348[0]; sp2f0[6] = sp34c[0];
    sp2d0[4] = sp350[0]; sp2d0[5] = sp354[0]; sp2d0[6] = sp358[0];

    sp310[7] = sp484->unk30;
    sp2f0[7] = sp480->unk30;
    sp2d0[7] = sp47c->unk30;

    for (i = 0; i < 8; i++)
    {
        sp2b0[i] = sp2f0[i] - sp310[i];
        sp290[i] = sp2d0[i] - sp310[i];
    }

    for (i = 0; i < 8; i++)
    {
        sp250[i] = (sp290[i] * sp3cc[0] - sp2b0[i] * sp3d4[0]) / 65536.0f;
        sp270[i] = (sp2b0[i] * sp3d0[0] - sp290[i] * sp3c8[0]) / 65536.0f;
        sp290[i] = sp250[i] * sp440;
        sp2b0[i] = sp270[i] * sp440;
        sp230[i] = sp2b0[i] + sp290[i] * sp394[0];
        sp210[i] = sp310[i] - sp230[i] * sp37c;
    }

    {
        u32 sp168;
        u32 sp164;
        u32 sp160;
        u32 sp15c;
        u32 sp158;
        u32 sp154;
        u32 sp150;
        u32 sp14c;
        u32 sp148;
        u32 sp144;
        u32 sp140;
        u32 sp13c;
        u32 sp138;
        u32 sp134;
        u32 sp130;
        u32 sp12c;

        sp168 = sub_GAME_7F094298(sp210[0]);
        sp164 = sub_GAME_7F094298(sp210[1]);
        sp160 = sub_GAME_7F094298(sp210[2]);
        sp15c = sub_GAME_7F094298(sp210[3]);

        sp158 = sub_GAME_7F094298(sp290[0]);
        sp154 = sub_GAME_7F094298(sp290[1]);
        sp150 = sub_GAME_7F094298(sp290[2]);
        sp14c = sub_GAME_7F094298(sp290[3]);

        sp138 = sub_GAME_7F094298(sp2b0[0]);
        sp134 = sub_GAME_7F094298(sp2b0[1]);
        sp130 = sub_GAME_7F094298(sp2b0[2]);
        sp12c = sub_GAME_7F094298(sp2b0[3]);

        sp148 = sub_GAME_7F094298(sp230[0]);
        sp144 = sub_GAME_7F094298(sp230[1]);
        sp140 = sub_GAME_7F094298(sp230[2]);
        sp13c = sub_GAME_7F094298(sp230[3]);

        gImmp1(gdl++, G_RDPHALF_1, (sp168 & 0xffff0000) | (sp164 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp160 & 0xffff0000) | (sp15c & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp158 & 0xffff0000) | (sp154 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp150 & 0xffff0000) | (sp14c & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp168 & 0x0000ffff) << 16 | (sp164 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp160 & 0x0000ffff) << 16 | (sp15c & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (sp158 & 0x0000ffff) << 16 | (sp154 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp150 & 0x0000ffff) << 16 | (sp14c & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (sp148 & 0xffff0000) | (sp144 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp140 & 0xffff0000) | (sp13c & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp138 & 0xffff0000) | (sp134 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp130 & 0xffff0000) | (sp12c & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp148 & 0x0000ffff) << 16 | (sp144 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp140 & 0x0000ffff) << 16 | (sp13c & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (sp138 & 0x0000ffff) << 16 | (sp134 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp130 & 0x0000ffff) << 16 | (sp12c & 0x0000ffff));
    }

    sp200[0] = sp330[0] / 32.0f;
    sp204[0] = sp334[0] / 32.0f;
    sp208[0] = sp368 / 32.0f;

    for (i = 0; i < 8; i++)
    {
        sp1d0[i] = SKYABS(sp290[i]) / 32.0f;
        sp1b0[i] = SKYABS(sp2b0[i]) / 32.0f;
    }

    sp1a0[0] = sp200[0] + (2 * sp1d0[4]) + sp1b0[4];
    sp1a4[0] = sp204[0] + (2 * sp1d0[5]) + sp1b0[5];
    sp1a8[0] = sp208[0] + (2 * sp1d0[6]) + sp1b0[6];

    if (sp1a0[0] < sp1a4[0]) { sp1a0[0] = sp1a4[0]; }
    if (sp1a0[0] < sp1a8[0]) { sp1a0[0] = sp1a8[0]; }

    sp1a0[0] /= 1024.0f;

    if (sp1a0[0] > 1.0f)
    {
        sp190[0] = 1.0f / sp1a0[0];
    }
    else
    {
        sp190[0] = 1.0f;
    }

    {
        u32 spe8;
        u32 spe4;
        u32 spe0;
        u32 spdc;
        u32 spd8;
        u32 spd4;
        u32 spd0;
        u32 spcc;
        u32 spc8;
        u32 spc4;
        u32 spc0;
        u32 spbc;
        u32 spb8;
        u32 spb4;
        u32 spb0;
        u32 spac;

        spe8 = sub_GAME_7F094298(sp210[4] * sp190[0]);
        spe4 = sub_GAME_7F094298(sp210[5] * sp190[0]);
        spe0 = sub_GAME_7F094298(sp210[6] * sp190[0]);
        spdc = 0;

        spd8 = sub_GAME_7F094298(sp290[4] * sp190[0]);
        spd4 = sub_GAME_7F094298(sp290[5] * sp190[0]);
        spd0 = sub_GAME_7F094298(sp290[6] * sp190[0]);
        spcc = 0;

        spb8 = sub_GAME_7F094298(sp2b0[4] * sp190[0]);
        spb4 = sub_GAME_7F094298(sp2b0[5] * sp190[0]);
        spb0 = sub_GAME_7F094298(sp2b0[6] * sp190[0]);
        spac = 0;

        spc8 = sub_GAME_7F094298(sp230[4] * sp190[0]);
        spc4 = sub_GAME_7F094298(sp230[5] * sp190[0]);
        spc0 = sub_GAME_7F094298(sp230[6] * sp190[0]);
        spbc = 0;

        gImmp1(gdl++, G_RDPHALF_1, (spe8 & 0xffff0000) | (spe4 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spe0 & 0xffff0000) | (spdc & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spd8 & 0xffff0000) | (spd4 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spd0 & 0xffff0000) | (spcc & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spe8 & 0x0000ffff) << 16 | (spe4 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (spe0 & 0x0000ffff) << 16 | (spdc & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (spd8 & 0x0000ffff) << 16 | (spd4 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (spd0 & 0x0000ffff) << 16 | (spcc & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (spc8 & 0xffff0000) | (spc4 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spc0 & 0xffff0000) | (spbc & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spb8 & 0xffff0000) | (spb4 & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spb0 & 0xffff0000) | (spac & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spc8 & 0x0000ffff) << 16 | (spc4 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (spc0 & 0x0000ffff) << 16 | (spbc & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (spb8 & 0x0000ffff) << 16 | (spb4 & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_2, (spb0 & 0x0000ffff) << 16 | (spac & 0x0000ffff));
    }

    return gdl;
}


Gfx *skyRenderFull(Gfx *gdl, SkyRelated38 *arg1, SkyRelated38 *arg2, SkyRelated38 *arg3, SkyRelated38 *arg4, f32 arg5)
{
    SkyRelated38 *sp4cc;
    SkyRelated38 *sp4c8;
    SkyRelated38 *sp4c4;
    s32 i;
    u32 stack;
    f32 sp4b8;
    f32 sp4b4;
    f32 sp4b0;
    f32 sp4ac;
    f32 sp4a8;
    f32 sp4a4;
    f32 sp4a0[1];
    f32 sp49c[1];
    f32 sp498[1];
    f32 sp494[1];
    f32 sp490[1];
    f32 sp48c[1];
    f32 sp488;
    f32 sp484;
    f32 sp480[1];
    f32 sp47c[1];
    f32 sp478[1];
    f32 sp474[1];
    f32 sp470[1];
    f32 sp46c[1];
    f32 sp468[1];
    f32 sp464[1];
    SkyRelated38 *swap1;
    SkyRelated38 *swap2;
    SkyRelated38 *swap3;
    f32 sp454[1];
    u32 stack07;
    f32 sp44c[1];
    u32 stack08;
    u32 stack09;
    f32 sp440[1];
    f32 sp43c[1];
    f32 sp438[1];
    f32 sp434[1];
    f32 sp430[1];
    f32 sp42c[1];
    f32 sp428[1];
    f32 sp424[1];
    f32 sp420[1];
    f32 sp41c[1];
    f32 sp418[1];
    f32 sp414[1];
    f32 sp410[1];
    f32 sp40c[1];
    f32 sp408[1];
    f32 sp404[1];
    f32 sp400[1];
    f32 sp3fc[1];
    f32 sp3f8[1];
    f32 sp3f4[1];
    f32 sp3f0[1];
    f32 sp3ec[1];
    f32 sp3e8[1];
    f32 sp3e4[1];
    f32 sp3e0[1];
    f32 sp3dc[1];
    f32 sp3d8[1];
    f32 sp3d4[1];
    f32 sp3d0[1];
    f32 sp3cc[1];
    f32 sp3c8[1];
    f32 sp3c4[1];
    f32 sp3c0;
    f32 sp3bc[1];
    f32 sp3b8[1];
    f32 sp3b4[1];
    f32 sp3b0[1];
    f32 sp3ac;
    f32 sp3a8[1];
    f32 sp3a4[1];
    f32 sp3a0[1];
    f32 sp39c[1];
    f32 sp398[1];
    f32 sp394[1];
    f32 sp390[1];
    f32 sp38c[1];
    f32 sp388[1];
    f32 sp384[1];
    f32 sp380[1];
    f32 sp37c[1];
    f32 sp378[1];
    f32 sp374[1];
    f32 sp370[1];
    f32 sp36c[1];
    f32 sp368[1];
    f32 sp364[1];
    f32 sp354[4];
    f32 sp334[8];
    f32 sp314[8];
    f32 sp2f4[8];
    f32 sp2d4[8];
    f32 sp2b4[8];
    f32 sp294[8];
    f32 sp274[8];
    f32 sp254[8];
    u32 stack10;
    u32 stack11;
    u32 stack12;
    u32 stack13;
    u32 stack00;
    f32 sp23c[1];
    f32 sp238[1];
    f32 sp234[1];
    f32 sp214[8];
    f32 sp1f4[8];
    u32 stack03;
    u32 stack04;
    u32 stack05;
    u32 stack06;
    u32 stack15;
    f32 sp1dc[1];
    f32 sp1d8[1];
    f32 sp1d4[1];
    u32 stack01;
    u32 stack02;
    u32 stack14;
    f32 sp1c4[1];
    u32 stack16;

    if (skyVerticesAreTheSame(arg1, arg2)
            || skyVerticesAreTheSame(arg2, arg3)
            || skyVerticesAreTheSame(arg3, arg1)
            || skyVerticesAreTheSame(arg4, arg1)
            || skyVerticesAreTheSame(arg4, arg2)
            || skyVerticesAreTheSame(arg4, arg3))
    {
        return gdl;
    }

    sp3c0 = arg5 / 65536.0f;

    sp4b8 = arg2->unk28 - arg1->unk28;
    sp4b4 = arg2->unk2c - arg1->unk2c;
    sp4b0 = arg3->unk28 - arg1->unk28;
    sp4ac = arg3->unk2c - arg1->unk2c;

    sp488 = ((sp4b0 * sp4b4) - (sp4b8 * sp4ac)) / 65536.0f;

    sp484 = 1.0f / sp488;

    sp4cc = arg1;
    sp4c8 = arg2;
    sp4c4 = arg3;

    if (sp4c8->unk2c < sp4cc->unk2c)
    {
        swap1 = sp4c8;
        sp4c8 = sp4cc;
        sp4cc = swap1;

        sp488 *= -1.0f;
        sp484 *= -1.0f;
    }

    if (sp4c4->unk2c < sp4c8->unk2c)
    {
        swap2 = sp4c8;
        sp4c8 = sp4c4;
        sp4c4 = swap2;

        sp488 *= -1.0f;
        sp484 *= -1.0f;
    }

    if (sp4c8->unk2c < sp4cc->unk2c)
    {
        swap3 = sp4c8;
        sp4c8 = sp4cc;
        sp4cc = swap3;

        sp488 *= -1.0f;
        sp484 *= -1.0f;
    }

    sp464[0] = sp4c8->unk28 * 0.25f;
    sp468[0] = 0.0f;
    sp46c[0] = sp4cc->unk28 * 0.25f;
    sp470[0] = 0.0f;
    sp474[0] = sp4cc->unk28 * 0.25f;
    sp478[0] = 0.0f;
    sp480[0] = 0.0f;
    sp47c[0] = 0.0f;

    sp48c[0] = sp4c4->unk28;
    sp490[0] = sp4c4->unk2c;
    sp494[0] = sp4c8->unk28;
    sp498[0] = sp4c8->unk2c;
    sp49c[0] = sp4cc->unk28;
    sp4a0[0] = sp4cc->unk2c;

    sp4b8 = sp494[0] - sp49c[0];
    sp4b4 = sp498[0] - sp4a0[0];
    sp4b0 = sp48c[0] - sp49c[0];
    sp4ac = sp490[0] - sp4a0[0];
    sp4a8 = sp48c[0] - sp494[0];
    sp4a4 = sp490[0] - sp498[0];

    sp440[0] = 0.0f;
    sp43c[0] = 0.0f;
    sp420[0] = 0.0f;
    sp41c[0] = 0.0f;
    sp3e4[0] = 0.0f;
    sp3ec[0] = 0.0f;
    sp3f4[0] = 0.0f;
    sp400[0] = 0.0f;
    sp3fc[0] = 0.0f;
    sp3c4[0] = 0.0f;
    sp3cc[0] = 0.0f;
    sp3d4[0] = 0.0f;
    sp3e0[0] = 0.0f;
    sp3dc[0] = 0.0f;

    sp424[0] = sp4a8 / 4.0f;
    sp428[0] = sp4a4 / 4.0f;
    sp42c[0] = sp4b8 / 4.0f;
    sp430[0] = sp4b4 / 4.0f;
    sp434[0] = sp4b0 / 4.0f;
    sp438[0] = sp4ac / 4.0f;

    sp404[0] = sp4a8 * 4.0f;
    sp408[0] = sp4a4 * 4.0f;
    sp40c[0] = sp4b8 * 4.0f;
    sp410[0] = sp4b4 * 4.0f;
    sp414[0] = sp4b0 * 4.0f;
    sp418[0] = sp4ac * 4.0f;

    sp3e8[0] = 4.0f / sp4a4;
    sp3f0[0] = 4.0f / sp4b4;
    sp3f8[0] = 4.0f / sp4ac;

    sp3c8[0] = sp4a8 / sp4a4;
    sp3d0[0] = sp4b8 / sp4b4;
    sp3d8[0] = sp4b0 / sp4ac;

    sp3c8[0] = skyClamp(sp3c8[0], -1878.0f, 1877.0f);
    sp3d0[0] = skyClamp(sp3d0[0], -1878.0f, 1877.0f);
    sp3d8[0] = skyClamp(sp3d8[0], -1878.0f, 1877.0f);

    sp44c[0] = sp46c[0];
    sp454[0] = sp474[0];

    if (arg1->unk28 < arg2->unk28)
    {
        f32 sp1bc;

        if (arg3->unk2c - arg4->unk2c < 1.0f)
        {
            sp1bc = -1878.0f;
        }
        else
        {
            sp1bc = -(getPlayer_c_screenwidth() - 0.25f) / ((arg3->unk2c - arg4->unk2c) / 4.0f);
        }

        gImmp1(gdl++, G_RDPHALF_1, (G_TRI_SHADE_TXTR << 24) | 0x00800000 | (s32) arg3->unk2c);
        gImmp1(gdl++, G_RDPHALF_CONT, (s32) arg4->unk2c << 16 | (s32) arg1->unk2c);

        gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(getPlayer_c_screenleft() + getPlayer_c_screenwidth() - 0.25f));
        gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(sp1bc));

        gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(getPlayer_c_screenleft()));
        gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(0.0f));

        gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(getPlayer_c_screenleft() + getPlayer_c_screenwidth() - 0.25f));
        gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(0.0f));
    }
    else
    {
        f32 sp198;

        if (arg3->unk2c - arg4->unk2c < 1.0f)
        {
            sp198 = 1877.0f;
        }
        else
        {
            sp198 = (getPlayer_c_screenwidth() - 0.25f) / ((arg3->unk2c - arg4->unk2c) / 4.0f);
        }

        gImmp1(gdl++, G_RDPHALF_1, 0xce000000 | (s32) arg3->unk2c);
        gImmp1(gdl++, G_RDPHALF_CONT, (s32) arg4->unk2c << 16 | (s32) arg1->unk2c);

        gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(getPlayer_c_screenleft()));
        gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(sp198));

        gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(getPlayer_c_screenleft() + getPlayer_c_screenwidth() - 0.25f));
        gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(0.0f));

        gImmp1(gdl++, G_RDPHALF_1, sub_GAME_7F094298(getPlayer_c_screenleft()));
        gImmp1(gdl++, G_RDPHALF_CONT, sub_GAME_7F094298(0.0f));
    }

    sp3b0[0] = sp4cc->unk0c * sp3c0;
    sp3b4[0] = sp4c8->unk0c * sp3c0;
    sp3b8[0] = sp4c4->unk0c * sp3c0;
    sp3bc[0] = arg4->unk0c * sp3c0;

    sp3ac = sp3b0[0];

    if (sp3b4[0] < sp3ac) { sp3ac = sp3b4[0]; }
    if (sp3b8[0] < sp3ac) { sp3ac = sp3b8[0]; }
    if (sp3bc[0] < sp3ac) { sp3ac = sp3bc[0]; }

    sp3ac *= 0.5f;

    sp39c[0] = sp4cc->unk34 * sp3ac;
    sp3a0[0] = sp4c8->unk34 * sp3ac;
    sp3a4[0] = sp4c4->unk34 * sp3ac;
    sp3a8[0] = arg4->unk34 * sp3ac;

    sp36c[0] = sp39c[0] * sp4cc->unk20;
    sp370[0] = sp39c[0] * sp4cc->unk24;
    sp374[0] = sp39c[0] * 32767.0f;
    sp378[0] = sp3a0[0] * sp4c8->unk20;
    sp37c[0] = sp3a0[0] * sp4c8->unk24;
    sp380[0] = sp3a0[0] * 32767.0f;
    sp384[0] = sp3a4[0] * sp4c4->unk20;
    sp388[0] = sp3a4[0] * sp4c4->unk24;
    sp38c[0] = sp3a4[0] * 32767.0f;
    sp390[0] = sp3a8[0] * arg4->unk20;
    sp394[0] = sp3a8[0] * arg4->unk24;
    sp398[0] = sp3a8[0] * 32767.0f;

    sp364[0] = SKYABS(sp36c[0]);
    sp368[0] = SKYABS(sp370[0]);

    if (sp364[0] < SKYABS(sp378[0])) { sp364[0] = SKYABS(sp378[0]); }
    if (sp368[0] < SKYABS(sp37c[0])) { sp368[0] = SKYABS(sp37c[0]); }
    if (sp364[0] < SKYABS(sp384[0])) { sp364[0] = SKYABS(sp384[0]); }
    if (sp368[0] < SKYABS(sp388[0])) { sp368[0] = SKYABS(sp388[0]); }
    if (sp364[0] < SKYABS(sp390[0])) { sp364[0] = SKYABS(sp390[0]); }
    if (sp368[0] < SKYABS(sp394[0])) { sp368[0] = SKYABS(sp394[0]); }

    sp354[0] = sp36c[0]; sp354[1] = sp370[0]; sp354[2] = sp374[0];
    sp334[0] = sp378[0]; sp334[1] = sp37c[0]; sp334[2] = sp380[0];
    sp314[0] = sp384[0]; sp314[1] = sp388[0]; sp314[2] = sp38c[0];

    sp354[3] = sp4cc->unk30;
    sp334[3] = sp4c8->unk30;
    sp314[3] = sp4c4->unk30;

    for (i = 0; i < 4; i++)
    {
        sp2f4[i] = sp334[i] - sp354[i];
        sp2d4[i] = sp314[i] - sp354[i];
    }

    for (i = 0; i < 4; i++)
    {
        sp294[i] = ((sp2d4[i] * sp410[0]) - (sp2f4[i] * sp418[0])) / 65536.0f;
        sp2b4[i] = ((sp2f4[i] * sp414[0]) - (sp2d4[i] * sp40c[0])) / 65536.0f;
        sp2d4[i] = sp294[i] * sp484;
        sp2f4[i] = sp2b4[i] * sp484;
        sp274[i] = sp2b4[i] * sp484;
        sp254[i] = sp354[i];
    }

    {
        f32 mult = arg4->unk2c / arg3->unk2c;

        f32 sp170 = arg4->r + ((arg1->r - arg3->r) * mult);
        f32 sp16c = arg4->g + ((arg1->g - arg3->g) * mult);
        f32 sp168 = arg4->b + ((arg1->b - arg3->b) * mult);
        f32 sp164 = arg4->a + ((arg1->a - arg3->a) * mult);

        u32 sp160 = arg1->r * 65536.0f;
        u32 sp15c = arg1->g * 65536.0f;
        u32 sp158 = arg1->b * 65536.0f;
        u32 sp154 = arg1->a * 65536.0f;

        u32 sp150 = sub_GAME_7F094298((sp170 - arg1->r) / ((arg2->unk28 - arg1->unk28) * 0.25f));
        u32 sp14c = sub_GAME_7F094298((sp16c - arg1->g) / ((arg2->unk28 - arg1->unk28) * 0.25f));
        u32 sp148 = sub_GAME_7F094298((sp168 - arg1->b) / ((arg2->unk28 - arg1->unk28) * 0.25f));
        u32 sp144 = sub_GAME_7F094298((sp164 - arg1->a) / ((arg2->unk28 - arg1->unk28) * 0.25f));

        u32 sp140;
        u32 sp13c;
        u32 sp138;
        u32 sp134;
        u32 sp130;
        u32 sp12c;
        u32 sp128;
        u32 sp124;

        sp140 = sp130 = sub_GAME_7F094298((arg3->r - arg1->r) / ((arg3->unk2c - arg1->unk2c) * 0.25f));
        sp13c = sp12c = sub_GAME_7F094298((arg3->g - arg1->g) / ((arg3->unk2c - arg1->unk2c) * 0.25f));
        sp138 = sp128 = sub_GAME_7F094298((arg3->b - arg1->b) / ((arg3->unk2c - arg1->unk2c) * 0.25f));
        sp124 = sp134 = sub_GAME_7F094298((arg3->a - arg1->a) / ((arg3->unk2c - arg1->unk2c) * 0.25f));

        gImmp1(gdl++, G_RDPHALF_1, (sp160 & 0xffff0000) | (sp15c & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp158 & 0xffff0000) | (sp154 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp150 & 0xffff0000) | (sp14c & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp148 & 0xffff0000) | (sp144 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp160 & 0x0000ffff) << 16 | (sp15c & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp158 & 0x0000ffff) << 16 | (sp154 & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (sp150 & 0x0000ffff) << 16 | (sp14c & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp148 & 0x0000ffff) << 16 | (sp144 & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (sp140 & 0xffff0000) | (sp13c & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp138 & 0xffff0000) | (sp134 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp130 & 0xffff0000) | (sp12c & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (sp128 & 0xffff0000) | (sp124 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (sp140 & 0x0000ffff) << 16 | (sp13c & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp138 & 0x0000ffff) << 16 | (sp134 & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (sp130 & 0x0000ffff) << 16 | (sp12c & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (sp128 & 0x0000ffff) << 16 | (sp124 & 0x0000ffff));
    }

    sp234[0] = sp364[0] / 32.0f;
    sp238[0] = sp368[0] / 32.0f;
    sp23c[0] = sp3ac / 32.0f;

    for (i = 0; i < 4; i++)
    {
        sp214[i] = SKYABS(sp2d4[i]) / 32.0f;
        sp1f4[i] = SKYABS(sp2f4[i]) / 32.0f;
    }

    sp1d4[0] = sp234[0] + (2 * sp214[0]) + sp1f4[0];
    sp1d8[0] = sp238[0] + (2 * sp214[1]) + sp1f4[1];
    sp1dc[0] = sp23c[0] + (2 * sp214[2]) + sp1f4[2];

    if (sp1d4[0] < sp1d8[0]) { sp1d4[0] = sp1d8[0]; }
    if (sp1d4[0] < sp1dc[0]) { sp1d4[0] = sp1dc[0]; }

    sp1d4[0] *= (1.0f / 1024.0f);

    if (sp1d4[0] > 1.0f)
    {
        sp1c4[0] = 1.0f / sp1d4[0];
    }
    else
    {
        sp1c4[0] = 1.0f;
    }

    {
        u32 spe0;
        u32 spdc;
        u32 spd8;
        u32 spd4;
        u32 spd0;
        u32 spcc;
        u32 spc8;
        u32 spc4;
        u32 spc0;
        u32 spbc;
        u32 spb8;
        u32 spb4;
        u32 spb0;
        u32 spac;
        u32 spa8;
        u32 spa4;

        spe0 = sub_GAME_7F094298(sp254[0] * sp1c4[0]);
        spdc = sub_GAME_7F094298(sp254[1] * sp1c4[0]);
        spd8 = sub_GAME_7F094298(sp254[2] * sp1c4[0]);
        spd4 = sub_GAME_7F094298(sp254[3] * sp1c4[0]);

        spd0 = sub_GAME_7F094298(sp2d4[0] * sp1c4[0]);
        spcc = sub_GAME_7F094298(sp2d4[1] * sp1c4[0]);
        spc8 = sub_GAME_7F094298(sp2d4[2] * sp1c4[0]);
        spc4 = sub_GAME_7F094298(sp2d4[3] * sp1c4[0]);

        spb0 = sub_GAME_7F094298(sp2f4[0] * sp1c4[0]);
        spac = sub_GAME_7F094298(sp2f4[1] * sp1c4[0]);
        spa8 = sub_GAME_7F094298(sp2f4[2] * sp1c4[0]);
        spa4 = sub_GAME_7F094298(sp2f4[3] * sp1c4[0]);

        spc0 = sub_GAME_7F094298(sp274[0] * sp1c4[0]);
        spbc = sub_GAME_7F094298(sp274[1] * sp1c4[0]);
        spb8 = sub_GAME_7F094298(sp274[2] * sp1c4[0]);
        spb4 = sub_GAME_7F094298(sp274[3] * sp1c4[0]);

        gImmp1(gdl++, G_RDPHALF_1, (spe0 & 0xffff0000) | (spdc & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spd8 & 0xffff0000) | (spd4 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spd0 & 0xffff0000) | (spcc & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spc8 & 0xffff0000) | (spc4 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spe0 & 0x0000ffff) << 16 | (spdc & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (spd8 & 0x0000ffff) << 16 | (spd4 & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (spd0 & 0x0000ffff) << 16 | (spcc & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (spc8 & 0x0000ffff) << 16 | (spc4 & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (spc0 & 0xffff0000) | (spbc & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spb8 & 0xffff0000) | (spb4 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spb0 & 0xffff0000) | (spac & 0xffff0000) >> 16);
        gImmp1(gdl++, G_RDPHALF_CONT, (spa8 & 0xffff0000) | (spa4 & 0xffff0000) >> 16);

        gImmp1(gdl++, G_RDPHALF_1, (spc0 & 0x0000ffff) << 16 | (spbc & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_CONT, (spb8 & 0x0000ffff) << 16 | (spb4 & 0x0000ffff));

        gImmp1(gdl++, G_RDPHALF_1, (spb0 & 0x0000ffff) << 16 | (spac & 0x0000ffff));
        gImmp1(gdl++, G_RDPHALF_2, (spa8 & 0x0000ffff) << 16 | (spa4 & 0x0000ffff));
    }

    return gdl;
}
