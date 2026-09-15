static u32 randomState = 0x47554435;
static u32 nextRandom(void)
{
    randomState = randomState * 1664525U + 1013904223U;
    return randomState;
}

static f32 range(f32 low, f32 high)
{
    return low + (high - low) * ((nextRandom() >> 8) * (1.0f / 16777216.0f));
}

static void sameMatrix(Mtxf *a, Mtxf *b)
{
    s32 i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            /* Eliminating additions of zero may change only zero's sign. */
            assert(a->m[i][j] == b->m[i][j]);
}

static void checkMatrices(void)
{
    s32 test, i, j;
    for (test = 0; test < 20000; test++)
    {
        Mtxf input, local, expected, actual, inplace;
        coord3d pos, scale;
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                input.m[i][j] = j == 3 ? (i == 3) : range(-4.0f, 4.0f);
        for (i = 0; i < 3; i++)
        {
            input.m[3][i] = range(-30000.0f, 30000.0f);
            pos.f[i] = test % 5 == 0 ? 0.0f : range(-300.0f, 300.0f);
            scale.f[i] = test % 7 == 0 ? 0.0f : range(-3.0f, 3.0f);
        }
        matrix_4x4_set_identity_and_position(&pos, &local);
        matrix_4x4_multiply_homogeneous(&input, &local, &expected);
        matrix_4x4_multiply_translation(&input, &pos, &actual);
        sameMatrix(&expected, &actual);
        inplace = input;
        matrix_4x4_multiply_translation(&inplace, &pos, &inplace);
        sameMatrix(&expected, &inplace);

        /* Reproduce the previous hat scale/translation sequence. */
        matrix_column_1_scalar_multiply(scale.x, (f32 *)&local);
        matrix_column_2_scalar_multiply(scale.y, (f32 *)&local);
        matrix_column_3_scalar_multiply_2(scale.z, (f32 *)&local);
        matrix_4x4_multiply_homogeneous(&input, &local, &expected);
        actual = input;
        matrix_4x4_apply_scale_and_translation(&actual, &scale, &pos);
        sameMatrix(&expected, &actual);

        {
            RenderPosView oldMatrices[4], newMatrices[4];
            union ModelRoData data;
            ModelNode parent = {NULL, NULL}, node = {&data, NULL};
            Model oldModel = {oldMatrices}, newModel = {newMatrices};
            ModelRenderData render = {NULL};
            memset(oldMatrices, 0, sizeof(oldMatrices));
            memset(newMatrices, 0, sizeof(newMatrices));
            data.GroupSimple.Origin = pos;
            data.GroupSimple.Group1 = test % 4;
            if (test % 3 == 0) render.basemtx = &input;
            if (test % 3 == 1) node.Parent = &parent;
            nodeParentMatrix = &input;
            reference_process_15_subposition(&render, &oldModel, &node);
            process_15_subposition(&render, &newModel, &node);
            for (i = 0; i < 4; i++) sameMatrix(&oldMatrices[i].pos, &newMatrices[i].pos);
        }
    }
    puts("Matrices: 20,000 affine transforms, in-place compositions, hat scales and model-node paths match.");
}

static void checkAutoAim(void)
{
    StandTile tiles[3] = {{0}, {1}, {2}};
    PropRecord player = {{{{0, 0, 0}}}, &tiles[0]};
    PropRecord target = {{{{1, 2, 3}}}, &tiles[1]};
    s32 test, accepted = 0, rejected = 0;
    playerData.prop = &player;
    for (test = 0; test < 30000; test++)
    {
        coord3d aim;
        f32 xbounds[2], ybounds[2], oldScore, newScore;
        coord2d oldScreen = {{{-1234, -5678}}}, newScreen = oldScreen;
        s32 oldLos, oldSolid;
        playerCount = test % 4 + 1;
        autoAimX = test % 3 != 0;
        playerData.c_screenwidth = test % 2 ? 320.0f : 159.0f;
        playerData.c_screenheight = test % 4 ? 111.0f : 240.0f;
        playerData.c_screenleft = test % 2 ? 0.0f : 160.0f;
        playerData.c_screentop = test % 3 ? 0.0f : 120.0f;
        playerData.c_halfwidth = playerData.c_screenwidth * 0.5f;
        playerData.c_halfheight = playerData.c_screenheight * 0.5f;
        if (test % 11 == 0) playerData.c_halfwidth += 0.25f;
        playerData.c_recipscalex = range(100.0f, 1500.0f);
        playerData.c_recipscaley = range(100.0f, 1500.0f);
        crosshairX = playerData.c_screenleft + 0.5f * playerData.c_screenwidth;
        crosshairY = playerData.c_screentop + 0.5f * playerData.c_screenheight;
        /* Gameplay sets both auto-aim axes together. For the synthetic X-off
         * helper checks, keep the crosshair centered to avoid the baseline's
         * uninitialized horizontal_tolerance branch outside that contract. */
        difficulty = test % 2 ? 0.2f : 1.5f;
        aim.x = range(-100.0f, 100.0f);
        aim.y = range(-150.0f, 150.0f);
        aim.z = -range(0.01f, 5000.0f);
        xbounds[0] = aim.x - range(0.0f, 100.0f);
        xbounds[1] = aim.x + range(0.0f, 100.0f);
        ybounds[0] = aim.y - range(0.0f, 200.0f);
        ybounds[1] = aim.y + range(0.0f, 200.0f);
        if (test % 17 == 0)
        {
            aim.x = 0.0f;
            xbounds[0] = nextafterf(0.0f, -1.0f);
            xbounds[1] = nextafterf(0.0f, 1.0f);
        }
        losResult = test % 5 != 0;
        losTile = &tiles[test % 7 == 0 ? 2 : 1];
        losCalls = solidTrace = 0;
        oldScore = reference_chrpropScoreAutoAimTarget(&target, &aim, xbounds, ybounds, &oldScreen);
        oldLos = losCalls; oldSolid = solidTrace;
        losCalls = solidTrace = 0;
        newScore = chrpropScoreAutoAimTarget(&target, &aim, xbounds, ybounds, &newScreen);
        assert(memcmp(&oldScore, &newScore, sizeof(f32)) == 0);
        assert(memcmp(&oldScreen, &newScreen, sizeof(oldScreen)) == 0);
        assert(oldLos == losCalls && oldSolid == solidTrace);
        if (newScore > -1.0f) accepted++; else rejected++;
    }
    assert(accepted > 100 && rejected > 100);
    puts("Auto-aim: 30,000 scores and screen coordinates match bit-for-bit; LOS and collision toggles match.");
}

static void checkProps(void)
{
    s32 test, room, block, i, count;
    s32 rooms[25];
    s16 expected[MAX_PROPS + 1];
    for (test = 0; test < 2000; test++)
    {
        for (room = 0; room < 256; room++) RoomPropListBlockIndices[room] = -1;
        for (room = 0; room < 16; room++)
        {
            RoomPropListBlockIndices[room] = room * 2;
            for (block = room * 2; block < room * 2 + 2; block++)
            {
                for (i = 0; i < 15; i++)
                    RoomPropListBlocks[block].propnums[i] = nextRandom() % (MAX_PROPS + 1) - 1;
                RoomPropListBlocks[block].propnums[15] = block % 2 ? -1 : block + 1;
            }
        }
        RoomPropListBlocks[0].propnums[0] = 0;
        RoomPropListBlocks[0].propnums[1] = 31;
        RoomPropListBlocks[0].propnums[2] = 32;
        RoomPropListBlocks[0].propnums[3] = MAX_PROPS - 1;
        count = test % 25;
        for (i = 0; i < count; i++) rooms[i] = nextRandom() % 20;
        rooms[count] = -1;
        memset(propResults, 0xa5, sizeof(propResults));
        reference_roomGetProps(rooms);
        memcpy(expected, propResults, sizeof(expected));
        memset(propResults, 0xa5, sizeof(propResults));
        roomGetProps(rooms);
        assert(memcmp(expected, propResults, sizeof(expected)) == 0);
    }
    puts("Room props: 2,000 fragmented, overlapping and repeated-room queries retain exact output order.");
}

static void compareTraversal(s32 kind, s32 portalnum, s32 room)
{
    struct Trace expected[8192];
    bbox2d box = {.f = {{0, 0}, {320, 240}}};
    s32 oldTrace, oldRoom, oldBudget;
    u8 oldDepths[PORTMAX];
    traceCount = 0;
    memset(g_PortalTraversalDepths, 0, sizeof(g_PortalTraversalDepths));
    g_BgCurrentRoom = room;
    g_RoomLoadBudget = -1;
    if (kind == 0) reference_bgTick();
    if (kind == 1) reference_bgDetermineVisibleRooms();
    if (kind == 2) reference_bgProcessPortalTraversal(12, room, portalnum, 3, &box);
    oldTrace = traceCount; oldRoom = g_BgCurrentRoom; oldBudget = g_RoomLoadBudget;
    memcpy(expected, trace, traceCount * sizeof(*trace));
    memcpy(oldDepths, g_PortalTraversalDepths, sizeof(oldDepths));
    traceCount = 0;
    memset(g_PortalTraversalDepths, 0, sizeof(g_PortalTraversalDepths));
    g_BgCurrentRoom = room;
    g_RoomLoadBudget = -1;
    if (kind == 0) bgTick();
    if (kind == 1) bgDetermineVisibleRooms();
    if (kind == 2) bgProcessPortalTraversal(12, room, portalnum, 3, &box);
    assert(oldTrace == traceCount);
    assert(memcmp(expected, trace, traceCount * sizeof(*trace)) == 0);
    assert(oldRoom == g_BgCurrentRoom && oldBudget == g_RoomLoadBudget);
    assert(memcmp(oldDepths, g_PortalTraversalDepths, sizeof(oldDepths)) == 0);
}

static void checkPortals(void)
{
    s32 test, portalnum, room, i, axis;
    for (test = 0; test < 1000; test++)
    {
        s32 count = test % (PORTMAX + 1);
        g_MaxNumRooms = MAXROOMCOUNT;
        g_BgRenderMode = BGLOADTYPE_ROOMS;
        g_LevelScale = range(0.01f, 3.0f);
        for (axis = 0; axis < 3; axis++)
        {
            cameraPosition.f[axis] = test % 3 ? range(-20.0f, 20.0f) : 0.0f;
            cameraGround.f[axis] = range(-50.0f, 50.0f);
        }
        playerData.screensize.min.x = playerData.screensize.min.y = 0.0f;
        playerData.screensize.max.x = 320.0f; playerData.screensize.max.y = 240.0f;
        for (portalnum = 0; portalnum < count; portalnum++)
        {
            PortalData *data = &portalData[portalnum];
            data->portal = (Portal *)&portalGeometry[portalnum];
            data->connectedRoom1 = nextRandom() % 256;
            data->connectedRoom2 = test % 4 ? nextRandom() % 256 : data->connectedRoom1;
            data->controlbytes1 = nextRandom() % 4;
            portalGeometry[portalnum].numPoints = nextRandom() % 8 + 1;
            for (i = 0; i < portalGeometry[portalnum].numPoints; i++)
                for (axis = 0; axis < 3; axis++)
                    portalGeometry[portalnum].points[i].f[axis] = range(-100.0f, 100.0f);
            g_PortalIsVertical[portalnum] = nextRandom() % 2;
            lineHits[portalnum] = nextRandom() % 3;
            portalVisible[portalnum] = nextRandom() % 2;
            portalScreen[portalnum] = playerData.screensize;
            if (portalnum % 3 == 0)
            {
                portalScreen[portalnum].min.x = range(-100.0f, 400.0f);
                portalScreen[portalnum].max.y = range(-100.0f, 400.0f);
            }
            g_PortalPlanes[portalnum].normal.x = range(-1.0f, 1.0f);
            g_PortalPlanes[portalnum].normal.y = range(-1.0f, 1.0f);
            g_PortalPlanes[portalnum].normal.z = range(-1.0f, 1.0f);
            g_PortalPlanes[portalnum].min = range(-10.0f, 0.0f);
            g_PortalPlanes[portalnum].max = range(0.0f, 10.0f);
            portalMargin[portalnum] = range(0.0f, 2.0f);
        }
        portalData[count].portal = NULL;
        bgBuildPortalCache();
        /* Door flags and endpoint direction can change without rebuilding. */
        for (portalnum = 0; portalnum < count; portalnum++)
        {
            u8 tmp = portalData[portalnum].connectedRoom1;
            portalData[portalnum].connectedRoom1 = portalData[portalnum].connectedRoom2;
            portalData[portalnum].connectedRoom2 = tmp;
            portalData[portalnum].controlbytes1 = nextRandom() % 4;
        }
        for (room = -1; room <= 256; room++)
        {
            const u8 *cursor, *end;
            s32 oldCount, newCount;
            s32 other = nextRandom() % 258 - 1;
            cursor = bgGetRoomPortalList(room, &end);
            for (portalnum = 0; portalnum < count; portalnum++)
                if (portalData[portalnum].connectedRoom1 == room || portalData[portalnum].connectedRoom2 == room)
                { assert(cursor < end && *cursor == portalnum); cursor++; }
            assert(cursor == end);
            assert(reference_bgRoomsSharePortal(room, other) == bgRoomsSharePortal(room, other));
            traceCount = 0;
            oldCount = reference_bgGetPortalBetweenRooms(room, other, &cameraPosition, &cameraGround);
            newCount = bgGetPortalBetweenRooms(room, other, &cameraPosition, &cameraGround);
            assert(oldCount == newCount);
        }
        for (i = 0; i < 30; i++)
        {
            coord3d min, max;
            s32 expected[8], actual[8], oldCount, newCount, limit;
            for (axis = 0; axis < 3; axis++)
            { min.f[axis] = range(-100.0f, 0.0f); max.f[axis] = range(0.0f, 100.0f); }
            limit = i % 8;
            oldCount = newCount = limit ? nextRandom() % (limit + 1) : 0;
            for (axis = 0; axis < 8; axis++) expected[axis] = actual[axis] = axis < oldCount ? nextRandom() % 256 : -999;
            reference_bgGetRoomsIntersectingBbox(&min, &max, expected, &oldCount, limit);
            bgGetRoomsIntersectingBbox(&min, &max, actual, &newCount, limit);
            assert(oldCount == newCount && memcmp(expected, actual, sizeof(actual)) == 0);
        }
        for (room = 0; room < 256; room++)
        { roomVisible[room] = nextRandom() % 2; roomFull[room] = nextRandom() % 3 == 0; }
        cameraMode = test % 9; currentPlayer = test % 4;
        cameraRoom = nextRandom() % MAXROOMCOUNT;
        levelentry_index = test % 6 == 0 ? LEVEL_INDEX_CRAD : 0;
        compareTraversal(0, 0, cameraRoom);
        compareTraversal(1, 0, cameraRoom);
        for (portalnum = 0; portalnum < count; portalnum++)
            compareTraversal(2, portalnum, portalData[portalnum].connectedRoom1);
    }
    g_BgRenderMode = BGLOADTYPE_SINGLE_DL;
    bgBuildPortalCache();
    for (room = 0; room <= BG_PORTAL_ROOM_COUNT; room++) assert(g_BgRoomPortalOffsets[room] == 0);
    puts("Portals: 1,000 graphs through the 200-portal limit; 30,000 bounded room queries and traversal traces match.");
}

int main(void)
{
    checkMatrices();
    checkAutoAim();
    checkProps();
    checkPortals();
    return 0;
}
