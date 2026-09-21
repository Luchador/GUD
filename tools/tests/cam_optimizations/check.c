static u32 randomState = 12345;
static f32 randomFloat(f32 low, f32 high)
{
    randomState = randomState * 1664525U + 1013904223U;
    return low + (f32)(randomState >> 8) / 16777216.0f * (high - low);
}

static void selectPlayer(s32 index)
{
    player_num = index;
    g_CurrentPlayer = &players[index];
}

static void getPlanes(f32 *values)
{
    memcpy(values, &g_CamFrustumTopNormal, 12);
    memcpy(values + 3, &g_CamFrustumBottomNormal, 12);
    memcpy(values + 6, &g_CamFrustumLeftNormal, 12);
    memcpy(values + 9, &g_CamFrustumRightNormal, 12);
    values[12] = g_CamFrustumTopOffset;
    values[13] = g_CamFrustumBottomOffset;
    values[14] = g_CamFrustumLeftOffset;
    values[15] = g_CamFrustumRightOffset;
    values[16] = g_CamFrustumNearOffset;
}

static void checkScaleAndFrustum(void)
{
    s32 i, j, k;
    for (i = 0; i < 12000; i++)
    {
        struct player expected;
        static Mtxf matrix;
        f32 oldPlanes[17], newPlanes[17];
        selectPlayer(i % MAX_PLAYER_COUNT);
        camInvalidatePlayerCameraScale(player_num);
        camSetPlayerScreenSize(randomFloat(80, 640), randomFloat(60, 480));
        camSetPlayerPerspective(10, randomFloat(2, 120), randomFloat(0.5f, 2.5f));
        for (j = 0; j < 4; j++) for (k = 0; k < 4; k++) matrix.m[j][k] = randomFloat(-4, 4);
        g_CurrentPlayer->viewtoworldmtxf = &matrix;

        reference_camSetPlayerCameraScale();
        expected = *g_CurrentPlayer;
        reference_camUpdateFrustumPlanes();
        getPlanes(oldPlanes);
        camSetPlayerCameraScale();
        camUpdateFrustumPlanes();
        getPlanes(newPlanes);
        assert(memcmp(&expected, g_CurrentPlayer, sizeof(expected)) == 0);
        assert(memcmp(oldPlanes, newPlanes, sizeof(oldPlanes)) == 0);
    }
}

static void checkInvalidationAndCaching(void)
{
    s32 i;
    scaleSineCalls = 0;
    for (i = 0; i < MAX_PLAYER_COUNT; i++)
    {
        selectPlayer(i);
        camInvalidatePlayerCameraScale(i);
        camSetPlayerScreenSize(320, 240);
        camSetPlayerPerspective(10, 60, 4.0f / 3.0f);
        camSetPlayerCameraScale();
    }
    assert(scaleSineCalls == 4);
    for (i = 0; i < 32; i++)
    {
        selectPlayer(i % MAX_PLAYER_COUNT);
        camSetPlayerScreenSize(320, 240);
        camSetPlayerPerspective(20, 60, 4.0f / 3.0f);
        camSetPlayerScreenPosition(i, i);
        camSetPlayerCameraScale();
    }
    assert(scaleSineCalls == 4);
    camSetPlayerScreenSize(160, 240); camSetPlayerCameraScale();
    camSetPlayerScreenSize(160, 120); camSetPlayerCameraScale();
    camSetPlayerPerspective(20, 30, 4.0f / 3.0f); camSetPlayerCameraScale();
    camSetPlayerPerspective(20, 30, 16.0f / 9.0f); camSetPlayerCameraScale();
    assert(scaleSineCalls == 8);
    /* Reusing a player slot on level load must force the first build. */
    camInvalidatePlayerCameraScale(player_num); camSetPlayerCameraScale();
    assert(scaleSineCalls == 9);
    selectPlayer(0); camSetPlayerCameraScale();
    assert(scaleSineCalls == 9);
}

static void compareVisibility(PropRecord *prop, coord3d *pos, f32 radius)
{
    bool expected, actual;
    s32 oldRooms, oldBoxes;
    roomQueries = boxQueries = 0;
    g_CamScreenBoxCache.valid = FALSE;
    expected = reference_camIsPosOnScreen(prop, pos, radius);
    oldRooms = roomQueries; oldBoxes = boxQueries;
    roomQueries = boxQueries = matrixQueries = 0;
    g_CamScreenBoxCache.valid = FALSE;
    actual = camIsPosOnScreen(prop, pos, radius);
    assert(actual == expected);
    assert(roomQueries == oldRooms && boxQueries == oldBoxes);
    assert(matrixQueries <= 1);
}

static void checkVisibility(void)
{
    s32 i;
    static Mtxf matrix = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    PropRecord prop;
    coord3d pos;
    f32 boundaries[] = {-32000, -4100, -4000, -600, 0, 600, 4000, 4100, 32000};
    selectPlayer(0);
    camSetPlayerScreenSize(320, 240);
    camSetPlayerScreenPosition(0, 0);
    camSetPlayerPerspective(10, 60, 4.0f / 3.0f);
    camSetPlayerCameraScale();
    g_CurrentPlayer->viewtoworldmtxf = &matrix;
    camUpdateFrustumPlanes();
    memset(&prop, 0, sizeof(prop));
    memset(prop.rooms, 0xff, sizeof(prop.rooms));
    worldToView = matrix;
    for (i = 0; i < 100000; i++)
    {
        prop.stan = (i & 1) ? &prop : NULL;
        prop.type = (i & 2) ? PROP_TYPE_VIEWER : 2;
        prop.obj = (i & 4) ? &prop : NULL;
        prop.rooms[0] = (i & 8) ? 0 : 1;
        prop.rooms[1] = (i & 16) ? 1 : 0xff;
        roomRendered = (i & 32) != 0;
        useScreenBox = (i & 64) != 0;
        g_CurrentEnvironment.FogEnabled = (i & 128) != 0;
        g_ScaledFarFogIntensity = randomFloat(2000, 32000);
        g_CurrentPlayer->c_lodscalez = randomFloat(0.2f, 5);
        roomBox.min.x = randomFloat(0, 120); roomBox.min.y = randomFloat(0, 100);
        roomBox.max.x = randomFloat(200, 320); roomBox.max.y = randomFloat(140, 240);
        pos.x = randomFloat(-40000, 40000); pos.y = randomFloat(-40000, 40000);
        pos.z = randomFloat(-40000, 40000);
        cameraPos.x = randomFloat(-1000, 1000); cameraPos.y = randomFloat(-1000, 1000);
        cameraPos.z = randomFloat(-1000, 1000);
        worldToView.m[0][0] = randomFloat(-1, 1);
        worldToView.m[0][1] = randomFloat(-1, 1);
        worldToView.m[0][2] = randomFloat(-1, 1);
        compareVisibility(&prop, &pos, i % 5 == 0 ? 0 : randomFloat(1, 500));
    }
    /* Exercise equality and adjacent float values at fog/far limits. */
    worldToView = matrix;
    worldToView.m[0][0] = 0; worldToView.m[0][2] = -1;
    memset(&cameraPos, 0, sizeof(cameraPos));
    g_CurrentPlayer->c_lodscalez = 1;
    g_ScaledFarFogIntensity = 4000;
    g_CurrentEnvironment.FogEnabled = TRUE;
    prop.rooms[0] = 1; prop.rooms[1] = 0xff;
    roomRendered = TRUE; useScreenBox = FALSE;
    pos.x = pos.y = 0;
    for (i = 0; i < (s32)(sizeof(boundaries) / sizeof(boundaries[0])); i++)
    {
        s32 side;
        for (side = -1; side <= 1; side++)
        {
            pos.z = side ? nextafterf(boundaries[i], side < 0 ? -INFINITY : INFINITY) : boundaries[i];
            compareVisibility(&prop, &pos, 0);
            compareVisibility(&prop, &pos, 100);
            g_CurrentEnvironment.FogEnabled = FALSE;
            compareVisibility(&prop, &pos, 100);
            g_CurrentEnvironment.FogEnabled = TRUE;
        }
    }
}

int main(void)
{
    checkScaleAndFrustum();
    checkInvalidationAndCaching();
    checkVisibility();
    puts("cam_optimizations: 12000 bit-exact scale/frustum comparisons; 100081 visibility comparisons; cache lifetime and skipped scale work passed");
    return 0;
}
