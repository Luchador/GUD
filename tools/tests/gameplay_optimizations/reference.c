/* Frozen pre-optimization functions, renamed, with trailing whitespace normalized. run.py adapts N64 pointer casts for host execution. */
void reference_bgTick(void)
{
    PortalData *portal;
    Portal *next;
    coord3d *cameraPosition;
    coord3d *cameraGroundPosition;
    u8 *specialPortalFlags;
    s32 room;
    s32 portalnum;
    s32 lastportal;
    s32 depth;
    s32 maxdepth;
    s32 offset;
    s32 cammode;

    lastportal = -1;

    g_BgGlobalVisAddedRoomCount = 0;

    if (get_player_position_in_shuffled(get_cur_playernum()) == 0)
    {
        bgRoomsTickUnload();
    }

    cammode = bviewGetCameraMode();

    g_RoomLoadBudget = 3;

    switch (cammode)
    {
        case CAMERAMODE_INTRO:
        case CAMERAMODE_FADESWIRL:
        case CAMERAMODE_SWIRL:
        case CAMERAMODE_POSEND:
        case CAMERAMODE_MP:
            g_RoomLoadBudget = 0xc8;
            break;
        case CAMERAMODE_FP:
        case CAMERAMODE_DEATH_CAM_SP:
        case CAMERAMODE_DEATH_CAM_MP:
        case CAMERAMODE_FP_NOINPUT:
            break;
    }

    room = bondviewGetPlayerRoom();
    g_BgCurrentRoom = room;

    cameraPosition = bondviewGetPlayerPosition();
    cameraGroundPosition = bondviewGetPlayerGroundPosition();

    for (depth = 0, maxdepth = 11; depth != maxdepth; depth++)
    {
        for (portalnum = 0; g_BgPortals[portalnum].portal != NULL; portalnum++)
        {

            if (g_PortalIsVertical[portalnum] != 0)
            {
                continue;
            }

            if (portalnum == lastportal)
            {
                continue;
            }

            if (((room == g_BgPortals[portalnum].connectedRoom1 || room == g_BgPortals[portalnum].connectedRoom2) && bgTestLineIntersectsPortal(portalnum, cameraPosition, cameraGroundPosition)))
            {
                lastportal = portalnum;

                room = (room ^ g_BgPortals[portalnum].connectedRoom1) ^ g_BgPortals[portalnum].connectedRoom2;

                break;
            }
        }

        if (g_BgPortals[portalnum].portal == NULL)
        {
            break;
        }
    }

    g_BgCurrentRoom = room;

    reference_bgDetermineVisibleRooms();
}

void reference_bgProcessPortalTraversal(s32 value, s32 roomnum, s32 portalnum, s32 depth, bbox2d *parentbox)
{
    bbox2d screenbox;
    coord3d *playerpos;
    s32 otherroom;
    struct PortalMetric metric;
    f32 playermetric;
    f32 portalmetric;
    s32 i;

    if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_DISABLED)
    {
        return;
    }

    i = (s32) &g_PortalTraversalDepths[portalnum];

    playerpos = bondviewGetPlayerPosition();
    metric = g_PortalPlanes[portalnum];
    playermetric = ((metric.normal.z * playerpos->z) + ((metric.normal.x * playerpos->x) + (metric.normal.y * playerpos->y))) * g_LevelScale;
    portalmetric = bgGetPortalMargin(portalnum);

    if (roomnum == g_BgPortals[portalnum].connectedRoom1)
    {
        otherroom = g_BgPortals[portalnum].connectedRoom2;

        if (metric.max <= (playermetric - portalmetric))
        {
            return;
        }
    }
    else
    {
        otherroom = g_BgPortals[portalnum].connectedRoom1;

        if ((playermetric + portalmetric) <= metric.min)
        {
            return;
        }
    }

    if (((metric.min - portalmetric) < playermetric) && (playermetric < (metric.max + portalmetric)))
    {
        screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
        screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
        screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
        screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
    }
    else
    {
        if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL)
        {
            if (!bgGetPortalScreenBbox(portalnum, &screenbox))
            {
                return;
            }

            otherroom = (g_BgPortals[portalnum].connectedRoom1 ^ g_BgPortals[portalnum].connectedRoom2) ^ roomnum;

            if (!bgIsRoomOnScreen(otherroom, (struct rectbbox *) &screenbox))
            {
                return;
            }

            screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
            screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
            screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
            screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
        }
        else
        {
            if (!bgGetPortalScreenBbox(portalnum, &screenbox))
            {
                return;
            }

            bgRectIntersect(&screenbox, parentbox);
            bgRectIntersect(&screenbox, &g_CurrentPlayer->screensize);
        }

        if ((screenbox.max.x <= screenbox.min.x) || (screenbox.max.y <= screenbox.min.y))
        {
            return;
        }
    }

    *((u8 *) i) = depth;

    if ((screenbox.min.x < screenbox.max.x) && (screenbox.min.y < screenbox.max.y))
    {
        if (bgSetRoomOnScreen(otherroom, depth, &screenbox, g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL))
        {
            return;
        }
    }
    else
    {
        return;
    }

    for (i = 0; g_BgPortals[i].portal != NULL; i++)
    {
        if (i != portalnum)
        {
            if ((otherroom == g_BgPortals[i].connectedRoom1) || (otherroom == g_BgPortals[i].connectedRoom2))
            {
                bgQueuePortalTraversal(value, otherroom, i, depth + 1, &screenbox);
            }
        }
    }

    return;
}

void reference_bgDetermineVisibleRooms(void)
{
    f32 screenbounds[4];
    s32 temp_a0;
    s32 temp_v1;
    s32 i;
    u32 prof_t0 = 0;

    bgUpdateCurrentPlayerScreenMinMax();

    screenbounds[0] = g_CurrentPlayer->screensize.min.x;
    screenbounds[1] = g_CurrentPlayer->screensize.min.y;
    screenbounds[2] = g_CurrentPlayer->screensize.max.x;
    screenbounds[3] = g_CurrentPlayer->screensize.max.y;

    bgResetPortalVisitCounts();

    for (i = 0; i < MAXROOMCOUNT; i++)
    {
        g_BgRoomInfo[i].room_rendered = 0;
        g_BgRoomInfo[i].room_loaded_mask = 0;
    }

    for (i = 0; i < PORTMAX; i++)
    {
        g_PortalTraversalDepths[i] = 0;
    }

    bgResetPortalQueue();

    for (i = 0; i < PORTMAX; i++)
    {
        g_PortalCameraCache[i].count = -1;
    }

    bgExecuteGlobalVisCommands((GlobalVisCommand *)g_BgGlobalVisCommands);

    /**
     * If the level is Cradle, or has no portals, skip the portal occlusion culling algorithm. Just add every room in the player's
     * screen bounds to the list of rooms to draw.
     */
    if ((levelentry_index == LEVEL_INDEX_CRAD) || (g_BgPortals->portal == NULL))
    {
        if (levelentry_index == LEVEL_INDEX_CRAD)
        {
            bgSetRoomOnScreen(9, 0, &g_CurrentPlayer->screensize, 1);
        }

        for (i = 1; i < g_MaxNumRooms; i++)
        {
            if (bgIsRoomOnScreen(i, &g_CurrentPlayer->screensize) != 0)
            {
                bgSetRoomOnScreen(i, 0, &g_CurrentPlayer->screensize, 1);
            }
        }
    }
    else
    {
        bgSetRoomOnScreen(g_BgCurrentRoom, 0, &g_CurrentPlayer->screensize, 1);

        for (i = 0; g_BgPortals[i].portal != NULL; i++)
        {
            if ((g_BgCurrentRoom == g_BgPortals[i].connectedRoom1) || (g_BgCurrentRoom == g_BgPortals[i].connectedRoom2))
            {
                bgQueuePortalTraversal(0, g_BgCurrentRoom, i, 1, screenbounds);
            }
        }

        while (bgProcessNextQueuedPortal())
        {
            // empty
        }
    }
}

s32 reference_bgGetConnectedRooms(s32 roomIndex, s32* list, s32 max)
{
    s32 len = 0;
    s32 i;
    s32 p;
    s32 connectedRoom1;
    s32 connectedRoom2;

    for (p = 0; g_BgPortals[p].portal != NULL; p++)
    {
        connectedRoom1 = g_BgPortals[p].connectedRoom1;
        connectedRoom2 = g_BgPortals[p].connectedRoom2;

        if (connectedRoom1 == roomIndex)
        {
            connectedRoom1 = connectedRoom2;
            connectedRoom2 = roomIndex;
        }

        if (connectedRoom2 == roomIndex)
        {
            for (i = 0; i < len; i++)
            {
                if (list[i] == connectedRoom1)
                {
                    goto end;
                }
            }

            list[len] = connectedRoom1;
            len++;

            if (len >= max)
            {
                return len;
            }
end:
            if (1);
        }
    }

    return len;
}

bool reference_bgRoomsSharePortal(s32 room1, s32 room2)
{
    s32 i;

    for (i = 0; g_BgPortals[i].portal != NULL; i++)
    {
        s32 v0 = g_BgPortals[i].connectedRoom1;
        s32 v1 = g_BgPortals[i].connectedRoom2;

        if (v0 == room1 && v1 == room2)
        {
            return TRUE;
        }

        if (v1 == room1 && v0 == room2)
        {
            return TRUE;
        }
    }
    return FALSE;
}

s32 reference_bgGetPortalBetweenRooms(s32 room1, s32 room2, coord3d *arg2, coord3d *arg3)
{
    s32 bFoundPortal = FALSE;
    s32 i;
    s32 portalIndex = -1;

    for (i = 0; g_BgPortals[i].portal != NULL; i++)
    {
        if (((g_BgPortals[i].connectedRoom1 == room1) && (g_BgPortals[i].connectedRoom2 == room2)) ||
            ((g_BgPortals[i].connectedRoom1 == room2) && (g_BgPortals[i].connectedRoom2 == room1)))
        {
            bFoundPortal = TRUE;

            if (bgTestLineIntersectsPortal(i, arg2, arg3) != 0)
            {
                portalIndex = i;
            }
        }
    }

    return portalIndex;
}

void reference_bgGetRoomsIntersectingBbox(coord3d *bbmin, coord3d *bbmax, s32 *room_list, s32 *count, s32 max_count)
{
    Portal *portal_pts;
    f32 v;
    s32 cur_room;
    coord3d scaled_bbmin;
    coord3d scaled_bbmax;
    s32 cur_count;
    s32 i;
    s32 j;
    s32 k;
    s32 pad;
    s32 saved_count;
    coord3d portal_min;
    coord3d portal_max;
    s32 portal_idx;
    s32 *p;
    s32 other_room;

    cur_count = *count;
    i = 0;
    scaled_bbmin.x = bbmin->x * g_LevelScale;
    scaled_bbmin.y = bbmin->y * g_LevelScale;
    scaled_bbmin.z = bbmin->z * g_LevelScale;
    scaled_bbmax.x = bbmax->x * g_LevelScale;
    scaled_bbmax.y = bbmax->y * g_LevelScale;
    scaled_bbmax.z = bbmax->z * g_LevelScale;
    saved_count = cur_count;

    while (1)
    {
        if (i < cur_count)
        {
            p = (s32 *)((u8 *)room_list + (i << 2)); do {
            cur_room = *p;
            portal_idx = 0;

            if (g_BgPortals[0].portal != ((void *) 0))
            {
                do
                {
                    if ((g_BgPortals[portal_idx].controlbytes1 & 1) || ((cur_room != g_BgPortals[portal_idx].connectedRoom1) && (cur_room != g_BgPortals[portal_idx].connectedRoom2)))
                    {
                        goto next_portal;
                    }

                    portal_min = *(coord3d *) &D_80044904;
                    portal_max = *(coord3d *) &D_80044910;
                    portal_pts = g_BgPortals[portal_idx].portal;

                    for (j = 0; j < portal_pts->numPoints; j++)
                    {
                        for (k = 0; k < 3; k++)
                        {
                            v = (&portal_pts->point)[j].f[k];

                            if (v < portal_min.f[k])
                            {
                                portal_min.f[k] = v;
                            }

                            if (portal_max.f[k] < v)
                            {
                                portal_max.f[k] = v;
                            }

                            portal_pts = g_BgPortals[portal_idx].portal;
                        }

                        if (portal_pts->numPoints);
                    }

                if (bgIsBboxOverlapping(&portal_min, &portal_max, &scaled_bbmin, &scaled_bbmax))
                {
                    if (cur_room == g_BgPortals[portal_idx].connectedRoom1)
                    {
                        other_room = g_BgPortals[portal_idx].connectedRoom2;
                    }
                    else
                    {
                        other_room = g_BgPortals[portal_idx].connectedRoom1;
                    }

                    for (k = 0; k < cur_count; k++)
                    {
                        if (room_list[k] == other_room)
                        {
                            break;
                        }
                    }

                    if (k == cur_count)
                    {
                        if (cur_count < max_count)
                        {
                            room_list[cur_count] = other_room;
                            cur_count++;
                        }

                        if (cur_count >= max_count)
                        {
                            *count = cur_count;
                            return;
                        }
                    }
                }

next_portal:
                portal_idx++;
                }
                while (g_BgPortals[portal_idx].portal != NULL);
            }

            i++;
            p++;

            } while (i < saved_count);
        }

        if (cur_count == saved_count)
        {
            break;
        }

        saved_count = cur_count;
    }
    *count = cur_count;
}

void reference_roomGetProps(s32 *rooms)
{
    s16 *writeptr = g_RoomPropQueryIndices;
    s32 room;
    s32 i;
    s32 j;

    room = *rooms;

    // Iterate rooms
    while (room >= 0)
    {
        // Find the chunk to start at
        s32 chunkindex = RoomPropListBlockIndices[room];

        // Iterate the chunks
        while (chunkindex >= 0)
        {
            // Iterate the propnums within each chunk
            for (i = 0; i < 15; i++)
            {
                s32 propnum = RoomPropListBlocks[chunkindex].propnums[i];

                if (propnum >= 0)
                {
                    // Check if it's in the list already
                    s16 *ptr = g_RoomPropQueryIndices;

                    while (ptr < writeptr)
                    {
                        if (*ptr == propnum) { break; }
                        ptr++;
                    }

                    if (ptr == writeptr)
                    {
                        // Prop is not in the list, so insert it
                        *writeptr = propnum;
                        writeptr++;
                    }
                }
            }

            chunkindex = RoomPropListBlocks[chunkindex].propnums[15];
        }

        rooms++;
        room = *rooms;
    }

    *writeptr = -1;
    writeptr++;
}

f32 reference_chrpropScoreAutoAimTarget(PropRecord *targetprop, coord3d *aimpos, f32 *world_xbounds, f32 *world_ybounds, coord2d *out_screen)
{
    f32 aim_screen[2];
    coord3d testpos;
    f32 screen_left_edge[2];
    f32 screen_right_edge[2];
    f32 screen_top_edge[2];
    f32 screen_bottom_edge[2];
    f32 crosshair_x;
    f32 crosshair_y;
    f32 autoaim_top;
    f32 autoaim_bottom;
    f32 autoaim_left;
    f32 autoaim_right;
    f32 score;
    bool passes_horizontal_check;
    f32 horizontal_tolerance;
    PropRecord *playerprop;
    StandTile* line_stan;
    f32 player_los_height;

    /**
     * Define a central auto-aim acceptance region.
     * The sweet spot is 65% vertically in favor of the top of the screen and 50% horizontally.
     */
    autoaim_top = getPlayer_c_screentop() + getPlayer_c_screenheight() * 0.175f;
    autoaim_bottom = getPlayer_c_screentop() + getPlayer_c_screenheight() * 0.825f;
    autoaim_left = getPlayer_c_screenleft() + getPlayer_c_screenwidth() * 0.25f;
    autoaim_right = getPlayer_c_screenleft() + getPlayer_c_screenwidth() * 0.75f;

    score = -2.0f;

    transform3Dto2DCoords(aimpos, (coord3d*)aim_screen);
    testpos.x = world_xbounds[0];
    testpos.y = aimpos->y;
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_left_edge);
    testpos.x = world_xbounds[1];
    testpos.y = aimpos->y;
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_right_edge);
    testpos.x = aimpos->x;
    testpos.y = world_ybounds[1];
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_top_edge);
    testpos.x = aimpos->x;
    testpos.y = world_ybounds[0];
    testpos.z = aimpos->z;
    transform3Dto2DCoords(&testpos, (coord3d*)screen_bottom_edge);

    if (screen_bottom_edge[1] >= autoaim_top && autoaim_bottom >= screen_top_edge[1])
    {
        passes_horizontal_check = FALSE;
        get_bullet_angle(&crosshair_x, &crosshair_y);
        screen_left_edge[0] = floorFloat(screen_left_edge[0]);
        screen_right_edge[0] = ceilFloat(screen_right_edge[0]);

        if (currentPlayerGetXAutoAimEnabledRedirect())
        {
            if (screen_left_edge[0] <= autoaim_right && autoaim_left <= screen_right_edge[0])
            {
                horizontal_tolerance = (screen_right_edge[0] - screen_left_edge[0]) * 1.5f;

                if (getPlayerCount() == 1)
                {
                    horizontal_tolerance = horizontal_tolerance * difficulty;
                }

                passes_horizontal_check = getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() >= (screen_left_edge[0] + screen_right_edge[0]) * 0.5f - horizontal_tolerance
                    && getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() <= (screen_left_edge[0] + screen_right_edge[0]) * 0.5f + horizontal_tolerance
                    && autoaim_left <= aim_screen[0]
                    && autoaim_right >= aim_screen[0];
            }
        }
        else
        {
            passes_horizontal_check = screen_left_edge[0] <= crosshair_x && crosshair_x <= screen_right_edge[0];
        }

        if (passes_horizontal_check)
        {
            playerprop = getCurrentPlayerProp();
            line_stan = playerprop->stan;
            player_los_height = bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer);
            bviewSetPlayerSolid(playerprop, FALSE);

            // Can auto-aim see the target?
            if ((stanTestLineUnobstructed(&line_stan, playerprop->pos.f[0], playerprop->pos.f[2], targetprop->pos.f[0], targetprop->pos.f[2], CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, player_los_height, player_los_height, 0.0f, 1.0f) != 0))
            {
                if (line_stan == targetprop->stan)
                {
                    f32 clamped_screen_y = aim_screen[1];

                    if (clamped_screen_y < autoaim_top)
                    {
                        clamped_screen_y = autoaim_top;
                    }
                    else if (clamped_screen_y > autoaim_bottom)
                    {
                        clamped_screen_y = autoaim_bottom;
                    }

                    out_screen->y = clamped_screen_y;

                    if (currentPlayerGetXAutoAimEnabledRedirect())
                    {
                        f32 clamped_screen_x = aim_screen[0];

                        if (clamped_screen_x < autoaim_left)
                        {
                            clamped_screen_x = autoaim_left;
                        }
                        else if (clamped_screen_x > autoaim_right)
                        {
                            clamped_screen_x = autoaim_right;
                        }

                        out_screen->x = clamped_screen_x;
                    }

                    /** If the screen's center x-coord overlaps the target's horizontal span, give it the best possible score of 1.0.
                     *  If this happens, this function's caller, chrpropUpdateAutoaimTarget, treats this as the winning prop and stops searching.
                     */
                    if (getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() >= screen_left_edge[0] && getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() <= screen_right_edge[0])
                    {
                        score = 1.0f;
                    }
                    // If the target is towards the left side of the screen, penalize it based on how far towards the left.
                    else if (getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth() >= screen_left_edge[0])
                    {
                        score = 1.0f - ((getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth()) - screen_right_edge[0]) / horizontal_tolerance;
                    }
                    // If the target is towards the right side of the screen, penalize it based on how far towards the right.
                    else
                    {
                        score = 1.0f - (screen_left_edge[0] - (getPlayer_c_screenleft() + 0.5f * getPlayer_c_screenwidth())) / horizontal_tolerance;
                    }
                }
            }

            bviewSetPlayerSolid(playerprop, TRUE);
        }
    }

    return score;
}

void reference_process_15_subposition(ModelRenderData* arg0, Model *model, ModelNode *node)
{
    union ModelRoData *rodata = node->Data;
    Mtxf *sp68;
    Mtxf sp28;
    s32 mtxindex = rodata->GroupSimple.Group1;
    RenderPosView *matrices = model->render_pos;

    if (node->Parent)
    {
        sp68 = modelFindNodeMtx(model, node->Parent, 0);
    }
    else
    {
        sp68 = arg0->basemtx;
    }

    if (sp68)
    {
        matrix_4x4_set_identity_and_position(&rodata->GroupSimple.Origin, &sp28);
        matrix_4x4_multiply_homogeneous(sp68, &sp28, &matrices[mtxindex]);
    }
    else
    {
        matrix_4x4_set_identity_and_position(&rodata->GroupSimple.Origin, &matrices[mtxindex]);
    }
}
