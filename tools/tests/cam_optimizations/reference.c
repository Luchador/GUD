/* Pre-optimization reference, without profiler hooks or retired prop fading.
 * Included by run.py for differential checks against the live source. */
void reference_camSetPlayerCameraScale(void)
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

void reference_camUpdateFrustumPlanes()
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

s32 reference_envPositionIsVisibleThroughFog(coord3d *pos, f32 range)
{
    coord3d sp24;
    f32 ff;
    coord3d *player_pos;
    Mtxf *player_mtx;

    if (!g_CurrentEnvironment.FogEnabled)
    {
        return TRUE;
    }

    player_pos = bondviewGetPlayerPosition();
    player_mtx = camGetWorldToViewMtxf();

    sp24.f[0] = pos->f[0] - player_pos->f[0];
    sp24.f[1] = pos->f[1] - player_pos->f[1];
    sp24.f[2] = pos->f[2] - player_pos->f[2];

    ff = (((sp24.f[0] * player_mtx->m[0][0]) + (sp24.f[1] * player_mtx->m[0][1]) + (sp24.f[2] * player_mtx->m[0][2])));

    if (ff > (g_ScaledFarFogIntensity + range))
    {
        return FALSE;
    }

    return TRUE;
}

bool reference_camIsPosOnScreen(PropRecord *prop, coord3d *pos, f32 modelInstSize)
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
            if (reference_envPositionIsVisibleThroughFog(pos, modelInstSize))
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
