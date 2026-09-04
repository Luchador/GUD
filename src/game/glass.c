#include <ultra64.h>
#include <limits.h>
#include <music.h>
#include "cam.h"
#include "glass.h"
#include "image_bank.h"
#include "lv.h"
#include "math_atan2f.h"
#include "objective_status.h"
#include "player.h"
#include "random.h"


#define SHARD_HORIZ_VEL_SCALE 1.5f
#define SHARD_VERT_VEL_SCALE 3.0f
#define SHARD_ANGVEL_SCALE 0.1f

s32 SHATTERED_WINDOW_PIECES_BUFFER_LEN;
s_shattered_window_piece* ptr_shattered_window_pieces;
s32 g_NextShardNum = 0;


void glassShatterPane(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax)
{
    coord3d xnormal;
    coord3d ynormal;
    coord3d basepos;
    coord3d shardpos;
    f32 xlength;
    f32 ylength;
    f32 inverseLength;
    f32 width;
    f32 height;
    f32 shardSize;
    f32 minimumShardSize;
    f32 randomShardScale;
    f32 stepSize;
    f32 halfStep;
    f32 angle;
    s32 step;
    s32 columns;
    s32 rows;
    s32 column;
    s32 row;

    xlength = sqrtf((xaxis->z * xaxis->z) + ((xaxis->x * xaxis->x) + (xaxis->y * xaxis->y)));
    inverseLength = 1.0f / xlength;
    xnormal.x = xaxis->x * inverseLength;
    xnormal.y = xaxis->y * inverseLength;
    xnormal.z = xaxis->z * inverseLength;
    xmin *= xlength;
    xmax *= xlength;

    ylength = sqrtf((yaxis->z * yaxis->z) + ((yaxis->x * yaxis->x) + (yaxis->y * yaxis->y)));
    inverseLength = 1.0f / ylength;
    ynormal.x = yaxis->x * inverseLength;
    ynormal.y = yaxis->y * inverseLength;
    ynormal.z = yaxis->z * inverseLength;
    ymin *= ylength;
    ymax *= ylength;

    width = xmax - xmin;
    height = ymax - ymin;
    shardSize = sqrtf((width * height) / (SHATTERED_WINDOW_PIECES_BUFFER_LEN / 2));
    minimumShardSize = shardSize * 0.1f;
    randomShardScale = shardSize * (0.7f * 2.3283064e-10f);
    step = (s32) shardSize;
    stepSize = step;
    halfStep = step >> 1;

    basepos.x = pos->x + (xnormal.x * (xmin + halfStep)) + (ynormal.x * (ymin + halfStep));
    basepos.y = pos->y + (xnormal.y * (xmin + halfStep)) + (ynormal.y * (ymin + halfStep));
    basepos.z = pos->z + (xnormal.z * (xmin + halfStep)) + (ynormal.z * (ymin + halfStep));

    xnormal.x *= stepSize;
    xnormal.y *= stepSize;
    xnormal.z *= stepSize;
    ynormal.x *= stepSize;
    ynormal.y *= stepSize;
    ynormal.z *= stepSize;

    angle = atan2f(zaxis->x, zaxis->z);
    columns = (s32) (width / stepSize);
    rows = (s32) (height / stepSize);

    chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, HIT_GLASS_SMASH_SFX, NULL), pos);

    for (row = 0; row < rows; row++)
    {
        shardpos = basepos;

        for (column = 0; column < columns; column++)
        {
            glassCreateShard(&shardpos, angle, (randomGetNext() * randomShardScale) + minimumShardSize);

            shardpos.x += xnormal.x;
            shardpos.y += xnormal.y;
            shardpos.z += xnormal.z;
        }

        basepos.x += ynormal.x;
        basepos.y += ynormal.y;
        basepos.z += ynormal.z;
    }
}


/**
 * Creates a triangular shard of glass.
 */
void glassCreateShard(coord3d* pos, f32 rotX, f32 shard_size)
{
    /**
     * Horizontal X velocity component with randomness. Range: [-1.0, +1.0]
     */
    f32 randSymmetricX = (2.0f * (randomGetNext() * (1.0f / (f32)UINT_MAX))) - 1.0f;

    /**
     * Vertical velocity component. Range: [ -0.12, +1.0]
     * Biased so that most shards get an upward push but some have a chance of getting a small downward push.
     */
    f32 randBiasedY = (randomGetNext() * (1.0f / (f32)UINT_MAX) * 1.12f) - .12f;
    
    /**
     * Horizontal Z velocity component with randomness. Range: [-1.0, +1.0]
     */
    f32 randSymmetricZ = (2.0f * (randomGetNext() * (1.0f / (f32)UINT_MAX))) - 1.0f;

    u8 alpha; // Shard opacity

    ptr_shattered_window_pieces[g_NextShardNum].active = 1;
    ptr_shattered_window_pieces[g_NextShardNum].pos.x = pos->x;
    ptr_shattered_window_pieces[g_NextShardNum].pos.y = pos->y;
    ptr_shattered_window_pieces[g_NextShardNum].pos.z = pos->z;

    ptr_shattered_window_pieces[g_NextShardNum].velocity.x= randSymmetricX * SHARD_HORIZ_VEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].velocity.y = randBiasedY * SHARD_VERT_VEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].velocity.z = randSymmetricZ * SHARD_HORIZ_VEL_SCALE;

    ptr_shattered_window_pieces[g_NextShardNum].v1x = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v1y = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v1z = 0;

    ptr_shattered_window_pieces[g_NextShardNum].v2x = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v2y = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * -shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v2z = 0;

    ptr_shattered_window_pieces[g_NextShardNum].v3x = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * -shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v3y = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * -shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v3z = 0;
    
    ptr_shattered_window_pieces[g_NextShardNum].v1s = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v1t = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v2s = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v2t = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v3s = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v3t = 0;

    /**
     * Create a colored gradient over the verts for environment mapping.
     */
    ptr_shattered_window_pieces[g_NextShardNum].v1r = 5;
    ptr_shattered_window_pieces[g_NextShardNum].v1g = 5;
    ptr_shattered_window_pieces[g_NextShardNum].v1b = 0x7E;
    ptr_shattered_window_pieces[g_NextShardNum].v2r = 5;
    ptr_shattered_window_pieces[g_NextShardNum].v2g = 0xFB;
    ptr_shattered_window_pieces[g_NextShardNum].v2b = 0x7E;
    ptr_shattered_window_pieces[g_NextShardNum].v3r = 0xFB;
    ptr_shattered_window_pieces[g_NextShardNum].v3g = 0xFB;
    ptr_shattered_window_pieces[g_NextShardNum].v3b = 0x7E;
    ptr_shattered_window_pieces[g_NextShardNum].v3a = 0xFF;

    alpha = ptr_shattered_window_pieces[g_NextShardNum].v3a;
    ptr_shattered_window_pieces[g_NextShardNum].v2a = alpha;
    ptr_shattered_window_pieces[g_NextShardNum].v1a = alpha;

    ptr_shattered_window_pieces[g_NextShardNum].rot.x = rotX;
    ptr_shattered_window_pieces[g_NextShardNum].rot.y = 0.0f;
    ptr_shattered_window_pieces[g_NextShardNum].rot.z = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * M_TAU_F;

    /**
     * Impart a random angular velocity on each shard piece.
     */
    ptr_shattered_window_pieces[g_NextShardNum].angvel.x = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * SHARD_ANGVEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].angvel.y = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * SHARD_ANGVEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].angvel.z = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * SHARD_ANGVEL_SCALE;

    g_NextShardNum++;

    if (g_NextShardNum >= SHATTERED_WINDOW_PIECES_BUFFER_LEN)
    {
        g_NextShardNum = 0;
    }
}


void update_broken_windows(void)
{
    f32 var_f0;
    s32 i;
    s32 j;

    if (g_ClockTimer < 15) {
        var_f0 = (f32)g_ClockTimer;
    } else {
        var_f0 = 15.0f;
    }
    if (SHATTERED_WINDOW_PIECES_BUFFER_LEN > 0) {
        i = 0;
        do {
            if (ptr_shattered_window_pieces[i].active > 0) {
                ptr_shattered_window_pieces[i].active += (s32)var_f0;
                j = 0;
                ptr_shattered_window_pieces[i].rot.x += ptr_shattered_window_pieces[i].angvel.x * var_f0;
                ptr_shattered_window_pieces[i].rot.y += ptr_shattered_window_pieces[i].angvel.y * var_f0;
                ptr_shattered_window_pieces[i].rot.z += ptr_shattered_window_pieces[i].angvel.z * var_f0;
                ptr_shattered_window_pieces[i].pos.x += ptr_shattered_window_pieces[i].velocity.x * var_f0;
                ptr_shattered_window_pieces[i].pos.z += ptr_shattered_window_pieces[i].velocity.z * var_f0;
                if ((s32)var_f0 > 0) {
                    do {
                        ptr_shattered_window_pieces[i].pos.y += ptr_shattered_window_pieces[i].velocity.y;
                        ptr_shattered_window_pieces[i].velocity.y -= 0.1f;
                        j++;
                    } while (j < (s32)var_f0);
                }
                if (ptr_shattered_window_pieces[i].active >= 0x96) {
                    ptr_shattered_window_pieces[i].active = 0;
                }
                if (ptr_shattered_window_pieces[i].pos.y < -30000.0f ||
                    ptr_shattered_window_pieces[i].pos.y > 30000.0f) {
                    ptr_shattered_window_pieces[i].active = 0;
                }
            }
            i++;
        } while (i < SHATTERED_WINDOW_PIECES_BUFFER_LEN);
    }
}


Gfx *glassRenderShards(Gfx *gdl)
{
    Mtxf mtxf;
    s32 i;
    Mtx *mtx;

    texSelect(&gdl, glassoverlayimage + 1, 2, 1, 2);

    gSPTexture(gdl++, 0x0D80, 0x0D80, 2, G_TX_RENDERTILE, 1);
    gDPSetCycleType(gdl++, G_CYC_2CYCLE);
    gDPSetTextureLOD(gdl++, G_TL_LOD);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gSPSetGeometryMode(gdl++, G_LIGHTING | G_TEXTURE_GEN);
    gSPMatrix(gdl++, osVirtualToPhysical(camGetPlayerProjViewMtx()), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    #define WINDOW_PIECE(_i) ((s_shattered_window_piece*)((u8*)ptr_shattered_window_pieces + ((_i) * sizeof(s_shattered_window_piece))))

    for (i = 0; i < SHATTERED_WINDOW_PIECES_BUFFER_LEN; i++) {
        if (WINDOW_PIECE(i)->active > 0) {
            mtx = dynAllocateMatrix();

            matrix_4x4_set_position_and_rotation_around_xyz(&WINDOW_PIECE(i)->pos, &WINDOW_PIECE(i)->rot, &mtxf);

            mtxf.m[3][0] -= g_CurrentPlayer->current_model_pos.x;
            mtxf.m[3][1] -= g_CurrentPlayer->current_model_pos.y;
            mtxf.m[3][2] -= g_CurrentPlayer->current_model_pos.z;

            matrix_4x4_f32_to_s32(&mtxf, (Mtxf *)mtx);

            gSPMatrix(gdl++, osVirtualToPhysical(mtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPVertex(gdl++, osVirtualToPhysical(&WINDOW_PIECE(i)->v1x), 3, 0);
            gSP1Triangle(gdl++, 0, 1, 2, 0);
        }
    }
    #undef WINDOW_PIECE

    gSPClearGeometryMode(gdl++, G_LIGHTING | G_TEXTURE_GEN);
    gSPMatrix(gdl++, (u32)camGetPlayerProjMtx(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gdl++, (u32)currentPlayerGetMatrix10C8(), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    return gdl;
}
