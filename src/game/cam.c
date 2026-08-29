#include <ultra64.h>
#include <math.h>
#include <bondtypes.h>
#include <boss.h>
#include <fr.h>
#include <joy.h>
#include <music.h>
#include <snd.h>
#include <str.h>
#include <options.h>
#include "cam.h"
#include "bg.h"
#include "environment.h"
#include "bgroomtrans.h"
#include "blood_animation.h"
#include "bondhead.h"
#include "bondinv.h"
#include "bondview.h"
#include "chr.h"
#include "chr_b.h"
#include "chraction.h"
#include "chrai.h"
#include "explosion.h"
#include "file.h"
#include "frametiming.h"
#include "front.h"
#include "glass.h"
#include "gun.h"
#include "initanitable.h"
#include "language.h"
#include "loadobjectmodel.h"
#include "lv.h"
#include "math_atan2f.h"
#include "matrixmath.h"
#include "model.h"
#include "mp_music.h"
#include "mpmenu.h"
#include "objecthandler.h"
#include "objective_status.h"
#include "os_extension.h"
#include "player.h"
#include "propobj.h"
#include "quaternion.h"
#include "random.h"
#include "stan.h"
#include "stanintersection.h"
#include "textrelated.h"


#if defined(VERSION_US)
    #define BONDVIEW_2ND_FONTTABLE(_param) copy_2ndfonttable
    #define BONDVIEW_1ST_FONTTABLE(_param) copy_1stfonttable
#elif defined(VERSION_JP) || defined(VERSION_EU)
    #define BONDVIEW_2ND_FONTTABLE(_param) dword_CODE_bss_jp80079CEC[_param]
    #define BONDVIEW_1ST_FONTTABLE(_param) dword_CODE_bss_jp80079Cd8[_param]
#endif

#define BONDVIEW_VIEW_TOP_OFFSET_1 0x0C
#define BONDVIEW_VIEW_TOP_OFFSET_2 0x28
#define BONDVIEW_VIEW_TOP_OFFSET_3 0x10

#define TANKUPDATEROTATION_SCALE 0.92f
#define TANKTURRETVERTICALANGLERELATED_SCALE 0.94f
#define TANK_UNKD0_SCALE 0.83f
#define CHR_OBJ_ACCEL_SPEED_FACTOR 0.5f
#define CHR_OBJ_MAXSPEED 5.0f
#define MAX_SPEED_FACTOR 0.8f
#define TANK_DAMAGE_PENTALTY_TICKS 90

#define TANK_VERT_ANGLE_FACTOR 0.0600000023842f
#define TANK_VERT_ANGLE_RAD_FACTOR 0.0799999833107f
#define MAX_AIMLOCK_SPEED_DEFAULT 0.86f

#define PLAYER_TICKEXPLODE_FACTOR 15

#define CLIPPING_CLOCK_FACTOR 0.8f
#define CLIPPING_FIELD88_FACTOR 0.19999999f
#define CLIPPING_FIELD8C_VALUE 15
#define CLIPPING_FIELD90_VALUE -4.5f

#define FULL_CROUCH_OFFSET -100.0f

#define SPEED_REGULAR_MAX  1.0f
#define SPEED_RUN_MAX      1.25f
#define SPEED_TICK_ADJUST  0.01f
#define TANK_MAX_SPEED     15.0f

#include "bondview_internal.h"

coord3d g_CamFrustumTopNormal;
f32 g_CamFrustumTopOffset;
coord3d g_CamFrustumBottomNormal;
f32 g_CamFrustumBottomOffset;
coord3d g_CamFrustumLeftNormal;
f32 g_CamFrustumLeftOffset;
coord3d g_CamFrustumRightNormal;
f32 g_CamFrustumRightOffset;
f32 g_CamFrustumNearOffset;

typedef struct CamScreenBoxCache
{
    bbox2d box;
    coord3d leftNormal;
    f32 leftOffset;
    coord3d rightNormal;
    f32 rightOffset;
    coord3d topNormal;
    f32 topOffset;
    coord3d bottomNormal;
    f32 bottomOffset;
    struct player *player;
    bool valid;
} CamScreenBoxCache;

static CamScreenBoxCache g_CamScreenBoxCache;


void camSetPlayerScreenSize(f32 width, f32 height)
{
    g_CamScreenBoxCache.valid = FALSE;
    g_CurrentPlayer->c_screenwidth = width;
    g_CurrentPlayer->c_screenheight = height;
    g_CurrentPlayer->c_halfwidth = width * 0.5f;
    g_CurrentPlayer->c_halfheight = height * 0.5f;
}


void camSetPlayerScreenPosition(f32 left, f32 top)
{
    g_CamScreenBoxCache.valid = FALSE;
    g_CurrentPlayer->c_screenleft = left;
    g_CurrentPlayer->c_screentop = top;
}


void camSetPlayerPerspective(f32 near, f32 fovy, f32 aspect)
{
    g_CamScreenBoxCache.valid = FALSE;
    g_CurrentPlayer->c_perspnear = near;
    g_CurrentPlayer->c_perspfovy = fovy;
    g_CurrentPlayer->c_perspaspect = aspect;
}


void camSetPlayerCameraScale(void)
{
	f32 fVar4;
	f32 tmp;
	f32 fVar5;
	f32 fVar2;

    g_CamScreenBoxCache.valid = FALSE;

	g_CurrentPlayer->c_scaley = sinf(mDegToHalfRad(g_CurrentPlayer->c_perspfovy)) / (cosf(mDegToHalfRad(g_CurrentPlayer->c_perspfovy)) * g_CurrentPlayer->c_halfheight);
	g_CurrentPlayer->c_scalex = (g_CurrentPlayer->c_scaley * g_CurrentPlayer->c_perspaspect * g_CurrentPlayer->c_halfheight) / g_CurrentPlayer->c_halfwidth;

	g_CurrentPlayer->c_recipscalex = 1.0f / g_CurrentPlayer->c_scalex;
	g_CurrentPlayer->c_recipscaley = 1.0f / g_CurrentPlayer->c_scaley;

    g_CurrentPlayer->c_scalelod = g_CurrentPlayer->c_scaley;
    g_CurrentPlayer->c_scalelod60 = sinf(DegToRad(30)) / (cosf(DegToRad(30)) * 120.0f);
	g_CurrentPlayer->c_lodscalez = g_CurrentPlayer->c_scalelod / g_CurrentPlayer->c_scalelod60;
	tmp = (g_CurrentPlayer->c_lodscalez * M_U16_MAX_VALUE_F);

	if (tmp > M_U32_MAX_VALUE_F) 
    {
		g_CurrentPlayer->c_lodscalezu32 = -1;
	} 
    else 
    {
		g_CurrentPlayer->c_lodscalezu32 = tmp;
	}

	fVar2 = g_CurrentPlayer->c_halfheight * g_CurrentPlayer->c_scaley;
	fVar4 = 1.0f / sqrtf(fVar2 * fVar2 + 1.0f);
	g_CurrentPlayer->c_cameratopnorm.x = 0;
	g_CurrentPlayer->c_cameratopnorm.y = fVar4;
	g_CurrentPlayer->c_cameratopnorm.z = fVar2 * fVar4;

	fVar5 = -g_CurrentPlayer->c_halfwidth * g_CurrentPlayer->c_scalex;
	fVar4 = 1.0f / sqrtf(fVar5 * fVar5 + 1.0f);
	g_CurrentPlayer->c_cameraleftnorm.x = -fVar4;
	g_CurrentPlayer->c_cameraleftnorm.y = 0;
	g_CurrentPlayer->c_cameraleftnorm.z = -fVar5 * fVar4;
}


/**
 * Transforms a 2D screen coordinate to a 3D world coordinate
 *
 * 'out' looks to be a vector which probably has the length 'length'
 * It starts from the middle of the screenn.
 */
void transformAndNormalizeByLength2Dto3D(coord2d *in, coord3d *out, f32 length)
{
    f32 norm;
    f32 x;
    f32 y;
    f32 z;

    y = (g_CurrentPlayer->c_halfheight - (in->y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    x = ((in->x - g_CurrentPlayer->c_screenleft) - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    z = -1.0f;
    norm = length / sqrtf((x * x) + (y * y) + (z * z));
    out->x = (x * norm);
    out->y = (y * norm);
    out->z = (-1.0f * norm);
}


void transform3Dto2DCoords(coord3d *in, coord2d *out)
{
    f32 inv_z = (1.0f / in->z);
    out->y = (in->y * inv_z * g_CurrentPlayer->c_recipscaley) + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
    out->x = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - (in->x * inv_z * g_CurrentPlayer->c_recipscalex);
}


void transform3Dto2DWithZScaling(coord3d *in, coord3d *out)
{
	f32 inv_z;

	if (in->z == 0.0f)
    {
		inv_z = -100000000000000000000.0f;
	} 
    else
    {
		inv_z = 1.0f / in->z;
	}

	out->y = in->y * inv_z * g_CurrentPlayer->c_recipscaley + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
	out->x = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - in->x * inv_z * g_CurrentPlayer->c_recipscalex;
}


void divide3DCoordinates(coord3d *in, f32 divisor, coord3d *out)
{
	out->y = in->y * (1.0f / divisor) * g_CurrentPlayer->c_recipscaley;
	out->x = in->x * (1.0f / divisor) * g_CurrentPlayer->c_recipscalex;
}


void currentPlayerSetMatrix10C4(Mtx *matrix)
{
    g_CurrentPlayer->field_10C4 = matrix;
}


void currentPlayerSetMatrix10C8(Mtx *matrix)
{
    g_CurrentPlayer->field_10C8 = matrix;
}


Mtx *currentPlayerGetMatrix10C8(void)
{
    return g_CurrentPlayer->field_10C8;
}


void camSetPlayerProjMtx(Mtx *matrix)
{
    g_CurrentPlayer->projmatrix = matrix;
}


Mtx *camGetPlayerProjMtx(void)
{
    return g_CurrentPlayer->projmatrix;
}


void camSetPlayerProjViewMtx(Mtx *mtx)
{
    g_CurrentPlayer->projViewMtx = mtx;
}


Mtx *camGetPlayerProjViewMtx(void)
{
    return g_CurrentPlayer->projViewMtx;
}


void *currentPlayerSetMatrix10CC(Mtxf *matrix)
{
    g_CurrentPlayer->field_10E8 = g_CurrentPlayer->field_10CC;
    g_CurrentPlayer->field_10CC = matrix;
}


Mtxf *camGetWorldToScreenMtxf(void)
{
    return g_CurrentPlayer->field_10CC;
}


void currentPlayerSetProjectionMatrixF(Mtxf *matrix)
{
    g_CurrentPlayer->projmatrixf = matrix;
}


Mtxf *currentPlayerGetProjectionMatrixF(void)
{
    return g_CurrentPlayer->projmatrixf;
}


void currentPlayerSetViewToWorldMtxf(Mtxf *matrix)
{
    g_CurrentPlayer->field_10EC = g_CurrentPlayer->viewtoworldmtxf;
    g_CurrentPlayer->viewtoworldmtxf = matrix;
}


Mtxf *currentPlayerGetViewToWorldMtxf(void)
{
    return g_CurrentPlayer->viewtoworldmtxf;
}


Mtxf *currentPlayerGetMatrix10EC(void)
{
    return g_CurrentPlayer->field_10EC;
}


void sub_GAME_7F078464(s32 arg0)
{
    g_CurrentPlayer->field_10E4 = arg0;
}


s32 sub_GAME_7F078474(void)
{
    return g_CurrentPlayer->field_10E4;
}


f32 getPlayer_c_lodscalez(void)
{
    return g_CurrentPlayer->c_lodscalez;
}


f32 getPlayer_c_screenwidth(void)
{
    return g_CurrentPlayer->c_screenwidth;
}


f32 getPlayer_c_screenheight(void)
{
    return g_CurrentPlayer->c_screenheight;
}


f32 getPlayer_c_screenleft(void)
{
    return g_CurrentPlayer->c_screenleft;
}


f32 getPlayer_c_screentop(void)
{
    return g_CurrentPlayer->c_screentop;
}


f32 getPlayer_c_perspaspect(void)
{
    return g_CurrentPlayer->c_perspaspect;
}


/**
 * Update the world space frustum planes used for object visibility tests.
 */
void camUpdateFrustumPlanes()
{
    f32 h_div;
    f32 h2;
    f32 h;
    f32 nh_div;
    f32 nh2_div;
    f32 h2_div;

    g_CamScreenBoxCache.valid = FALSE;

    h = g_CurrentPlayer->c_halfheight * g_CurrentPlayer->c_scaley;
    h_div = 1.0f / sqrtf((h * h) + 1.0f);
    h *= h_div;
    nh_div = -h_div;

    g_CamFrustumTopNormal.x = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][0]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumTopNormal.y = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][1]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumTopNormal.z = (-nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][2]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumTopOffset = (g_CamFrustumTopNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                          + (g_CamFrustumTopNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                          + (g_CamFrustumTopNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumBottomNormal.x = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][0]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumBottomNormal.y = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][1]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumBottomNormal.z = (nh_div * g_CurrentPlayer->viewtoworldmtxf->m[1][2]) + (h * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumBottomOffset = (g_CamFrustumBottomNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                             + (g_CamFrustumBottomNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                             + (g_CamFrustumBottomNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    h2 = (-g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    h2_div = 1.0f / sqrtf((h2 * h2) + 1.0f);
    h2 *= h2_div;
    nh2_div = -h2_div;

    g_CamFrustumLeftNormal.x = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][0]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumLeftNormal.y = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][1]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumLeftNormal.z = (nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][2]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumLeftOffset = (g_CamFrustumLeftNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                           + (g_CamFrustumLeftNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                           + (g_CamFrustumLeftNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumRightNormal.x = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][0]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][0]);
    g_CamFrustumRightNormal.y = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][1]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][1]);
    g_CamFrustumRightNormal.z = (-nh2_div * g_CurrentPlayer->viewtoworldmtxf->m[0][2]) - (h2 * g_CurrentPlayer->viewtoworldmtxf->m[2][2]);

    g_CamFrustumRightOffset = (g_CamFrustumRightNormal.x * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                            + (g_CamFrustumRightNormal.y * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                            + (g_CamFrustumRightNormal.z * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);

    g_CamFrustumNearOffset = (g_CurrentPlayer->viewtoworldmtxf->m[2][0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0])
                           + (g_CurrentPlayer->viewtoworldmtxf->m[2][1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1])
                           + (g_CurrentPlayer->viewtoworldmtxf->m[2][2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2]);
}


/**
 * Check if the 3D coordinate is within the screen
 *
 * Takes dot product of some position and compares each to an associated scalar value.
 * Returns 0 if the dot product exceeds the scalar amount, 1 otherwise.
 *
 * @param pos: Applies dot product of this position against g_CurrentPlayer->viewtoworldmtxf
 * and four coords starting at g_CamFrustumLeftNormal.
 *
 * @param margin: Value added to g_CamFrustumNearOffset to compare g_CurrentPlayer->viewtoworldmtxf,
 * and the four values starting at g_CamFrustumLeftOffset.
 */
bool camIsPosInScreen(coord3d *pos, f32 margin)
{
    if (g_CamFrustumNearOffset + margin < (g_CurrentPlayer->viewtoworldmtxf->m[2][0] * pos->f[0]) + (g_CurrentPlayer->viewtoworldmtxf->m[2][1] * pos->f[1]) + (g_CurrentPlayer->viewtoworldmtxf->m[2][2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumLeftOffset + margin < (g_CamFrustumLeftNormal.f[0] * pos->f[0]) + (g_CamFrustumLeftNormal.f[1] * pos->f[1]) + (g_CamFrustumLeftNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumRightOffset + margin < (g_CamFrustumRightNormal.f[0] * pos->f[0]) + (g_CamFrustumRightNormal.f[1] * pos->f[1]) + (g_CamFrustumRightNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumTopOffset + margin < (g_CamFrustumTopNormal.f[0] * pos->f[0]) + (g_CamFrustumTopNormal.f[1] * pos->f[1]) + (g_CamFrustumTopNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    if (g_CamFrustumBottomOffset + margin < (g_CamFrustumBottomNormal.f[0] * pos->f[0]) + (g_CamFrustumBottomNormal.f[1] * pos->f[1]) + (g_CamFrustumBottomNormal.f[2] * pos->f[2]))
    {
        return FALSE;
    }

    return TRUE;
}


static void camPrepareScreenBoxCache(bbox2d *box)
{
    f32 leftinvlen;
    f32 xslope;
    f32 yslope;
    f32 rightinvlen;
    f32 topinvlen;
    f32 bottominvlen;
    f32 leftneginvlen;
    f32 rightneginvlen;
    f32 topneginvlen;
    f32 bottomneginvlen;

    xslope = (box->min.x - g_CurrentPlayer->c_screenleft - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;

    leftinvlen = 1.0f / sqrtf(xslope * xslope + 1.0f);
    xslope *= leftinvlen;
    leftneginvlen = -leftinvlen;

    g_CamScreenBoxCache.leftNormal.f[0] = leftneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][0] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    g_CamScreenBoxCache.leftNormal.f[1] = leftneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][1] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    g_CamScreenBoxCache.leftNormal.f[2] = leftneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][2] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    g_CamScreenBoxCache.leftOffset = g_CamScreenBoxCache.leftNormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + g_CamScreenBoxCache.leftNormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + g_CamScreenBoxCache.leftNormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    xslope = -(box->max.x - g_CurrentPlayer->c_screenleft - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    rightinvlen = 1.0f / sqrtf(xslope * xslope + 1.0f);
    xslope *= rightinvlen;
    rightneginvlen = -rightinvlen;

    g_CamScreenBoxCache.rightNormal.f[0] = -rightneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][0] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    g_CamScreenBoxCache.rightNormal.f[1] = -rightneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][1] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    g_CamScreenBoxCache.rightNormal.f[2] = -rightneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[0][2] - xslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    g_CamScreenBoxCache.rightOffset = g_CamScreenBoxCache.rightNormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + g_CamScreenBoxCache.rightNormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + g_CamScreenBoxCache.rightNormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    yslope = (g_CurrentPlayer->c_halfheight - (box->min.y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    topinvlen = 1.0f / sqrtf(yslope * yslope + 1.0f);
    yslope *= topinvlen;
    topneginvlen = -topinvlen;

    g_CamScreenBoxCache.topNormal.f[0] = -topneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][0] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    g_CamScreenBoxCache.topNormal.f[1] = -topneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][1] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    g_CamScreenBoxCache.topNormal.f[2] = -topneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][2] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    g_CamScreenBoxCache.topOffset = g_CamScreenBoxCache.topNormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + g_CamScreenBoxCache.topNormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + g_CamScreenBoxCache.topNormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    yslope = -(g_CurrentPlayer->c_halfheight - (box->max.y - g_CurrentPlayer->c_screentop)) * g_CurrentPlayer->c_scaley;
    bottominvlen = 1.0f / sqrtf(yslope * yslope + 1.0f);
    yslope *= bottominvlen;
    bottomneginvlen = -bottominvlen;

    g_CamScreenBoxCache.bottomNormal.f[0] = bottomneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][0] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][0];
    g_CamScreenBoxCache.bottomNormal.f[1] = bottomneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][1] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][1];
    g_CamScreenBoxCache.bottomNormal.f[2] = bottomneginvlen * g_CurrentPlayer->viewtoworldmtxf->m[1][2] + yslope * g_CurrentPlayer->viewtoworldmtxf->m[2][2];

    g_CamScreenBoxCache.bottomOffset = g_CamScreenBoxCache.bottomNormal.f[0] * g_CurrentPlayer->viewtoworldmtxf->m[3][0] + g_CamScreenBoxCache.bottomNormal.f[1] * g_CurrentPlayer->viewtoworldmtxf->m[3][1] + g_CamScreenBoxCache.bottomNormal.f[2] * g_CurrentPlayer->viewtoworldmtxf->m[3][2];

    g_CamScreenBoxCache.box = *box;
    g_CamScreenBoxCache.player = g_CurrentPlayer;
    g_CamScreenBoxCache.valid = TRUE;
}


/**
 * Similar to the above function but checks if the 3D point is within an arbitrary box instead of the whole screen.
 * The box's world-space planes are cached because props in the same room use the same portal scissor box.
 *
 * @param pos: 3D coordinate in absolute world space.
 *
 * @param margin: is a slack in world units applied as a sphere around the point. The point is rejected only
 * if it is more than 'margin' outside a box plane.
 *
 * @param box: screen space rectangle with 'min' being the top-left corner and 'max' the bottom-right corner.
 */
bool camIsPosInScreenBox(coord3d *pos, f32 margin, bbox2d *box)
{
    if (g_CamFrustumNearOffset + margin < g_CurrentPlayer->viewtoworldmtxf->m[2][0] * pos->f[0] + g_CurrentPlayer->viewtoworldmtxf->m[2][1] * pos->f[1] + g_CurrentPlayer->viewtoworldmtxf->m[2][2] * pos->f[2])
    {
        return FALSE;
    }

    if (!g_CamScreenBoxCache.valid
            || g_CamScreenBoxCache.player != g_CurrentPlayer
            || g_CamScreenBoxCache.box.min.x != box->min.x
            || g_CamScreenBoxCache.box.min.y != box->min.y
            || g_CamScreenBoxCache.box.max.x != box->max.x
            || g_CamScreenBoxCache.box.max.y != box->max.y)
    {
        camPrepareScreenBoxCache(box);
    }

    if (g_CamScreenBoxCache.leftOffset + margin < g_CamScreenBoxCache.leftNormal.f[0] * pos->f[0] + g_CamScreenBoxCache.leftNormal.f[1] * pos->f[1] + g_CamScreenBoxCache.leftNormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    if (g_CamScreenBoxCache.rightOffset + margin < g_CamScreenBoxCache.rightNormal.f[0] * pos->f[0] + g_CamScreenBoxCache.rightNormal.f[1] * pos->f[1] + g_CamScreenBoxCache.rightNormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    if (g_CamScreenBoxCache.topOffset + margin < g_CamScreenBoxCache.topNormal.f[0] * pos->f[0] + g_CamScreenBoxCache.topNormal.f[1] * pos->f[1] + g_CamScreenBoxCache.topNormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    if (g_CamScreenBoxCache.bottomOffset + margin < g_CamScreenBoxCache.bottomNormal.f[0] * pos->f[0] + g_CamScreenBoxCache.bottomNormal.f[1] * pos->f[1] + g_CamScreenBoxCache.bottomNormal.f[2] * pos->f[2])
    {
        return FALSE;
    }

    return TRUE;
}


void camSetPlayerFrozenCam(bool isFrozen)
{
    g_CurrentPlayer->frozencam = isFrozen;
}

bool camIsPosInObjFadeDistance(coord3d *coord, f32 arg1)
{
    bool result = TRUE;
    NearFogRecord *nearFogRecord = envGetNearFogValues();
    coord3d diff;
    f32 distSquared;

    if (nearFogRecord != NULL)
    {
        coord3d *campos = bondviewGetPlayerPosition();
        Mtxf *mtx = camGetWorldToScreenMtxf();

        diff.x = coord->x - campos->x;
        diff.y = coord->y - campos->y;
        diff.z = coord->z - campos->z;

        distSquared = diff.f[0] * mtx->m[0][0] + diff.f[1] * mtx->m[0][1] + diff.f[2] * mtx->m[0][2];

        if (distSquared > nearFogRecord->MaxObfuscationRange)
        {
            f32 scalez = getPlayer_c_lodscalez();

            distSquared = ((distSquared - nearFogRecord->MaxObfuscationRange) * 100 / arg1 + nearFogRecord->MaxObfuscationRange) * scalez;

            if (distSquared >= nearFogRecord->MaxVisRange)
            {
                result = FALSE;
            }
        }
    }

    return result;
}


bool camIsPosOnScreen(PropRecord *prop, coord3d *pos, f32 modelInstSize, bool applyFogCull)
{
    s32 room_ids[8];
    s32 *rooms;
    s32 roomnum;
    bool result;
    bool singleRoom;
    bbox2d bbox;

    result = FALSE;

    /**
     * GUD: Optimize props in one room. Previously all props had their IDs copied into a temp s32 array, searched those rooms for one being rendered,
     * called getPropCombinedRoomsBBox2D() which copied the samr room IDs again, then searched for each room's portal-scissor rectangle.
     * Now a prop with one room uses prop->rooms[0] directly.
     */
    singleRoom = prop->stan != NULL
        && (prop->type != PROP_TYPE_VIEWER || prop->obj != NULL)
        && prop->rooms[0] != 0xff
        && prop->rooms[1] == 0xff;

    if (singleRoom)
    {
        roomnum = prop->rooms[0];
    }
    else
    {
        chraiGetPropRoomIds(prop, room_ids);
        rooms = room_ids;
        roomnum = *rooms;
    }

    while (roomnum >= 0)
    {
        if (bgIsRoomRendered(roomnum))
        {
            if (envPositionIsVisibleThroughFog(pos, modelInstSize) && (!applyFogCull || camIsPosInObjFadeDistance(pos, modelInstSize)))
            {
                if ((singleRoom ? bgGet2dBboxByRoomId(roomnum, &bbox) : getPropCombinedRoomsBBox2D(prop, &bbox)) != 0)
                {
                    result = camIsPosInScreenBox(pos, modelInstSize, &bbox);
                }
                else
                {
                    result = camIsPosInScreen(pos, modelInstSize);
                }

                if (result)
                {
                    coord3d *campos = bondviewGetPlayerPosition();
                    f32 xdiff = pos->x - campos->x;
                    f32 ydiff = pos->y - campos->y;
                    f32 zdiff = pos->z - campos->z;

                    /**
                     * If farther than 32000 units, consider it off screen.
                     */
                    if (xdiff * xdiff + ydiff * ydiff + zdiff * zdiff > 32000 * 32000)
                    {
                        result = FALSE;
                    }
                }
            }

            break;
        }

        if (singleRoom)
        {
            roomnum = -1;
        }
        else
        {
            rooms++;
            roomnum = *rooms;
        }
        result = FALSE;
    }

    return result;
}
