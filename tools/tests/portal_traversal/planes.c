static void build_geometry(void)
{
    for (int p = 0; p < numPortals; p++) bgCalcPortalPlane(p, &g_PortalPlanes[p]);
    for (int p = 0; p < numPortals; p++) bgOrderPortal(p);
    for (int r = 0; r < g_MaxNumRooms; r++) bgExpandRoomToPortals(r);
    bgBuildPortalCache();
}

static void bounds(int room, coord3d lo, coord3d hi)
{ g_BgRoomInfo[room].minbounds = lo; g_BgRoomInfo[room].maxbounds = hi; }

static void horizontal(int p, float y)
{
    geometry[p].numPoints = 4;
    geometry[p].points[0] = (coord3d){-488,y,-323};
    geometry[p].points[1] = (coord3d){488,y,-323};
    geometry[p].points[2] = (coord3d){488,y,-122};
    geometry[p].points[3] = (coord3d){-488,y,-122};
    portals[p].portal = (Portal *)&geometry[p];
}

/* Reproduce the exact revised Control portal and room bounds. Projected
 * rectangles are controlled so this isolates the failing side test. */
static void check_control_connection(int p)
{
    assert(g_BgPortalPlaneCullMasks[p] == 0);
    for (int side = 0; side < 2; side++) {
        int from = side ? 67 : 32, to = side ? 32 : 67;
        camera = (coord3d){0,side ? 430 : 240,-200};
        clear_view(from);
        legacyProcess(0,from,p,1,&player.screensize);
        assert(!g_BgRoomInfo[to].room_rendered);
        clear_view(from);
        productionProcessPortalTraversal(0,from,p,1,&player.screensize);
        assert(g_BgRoomInfo[to].room_rendered);
        assert(dispatched == 0); /* The failure needs no queue pressure. */
    }
}

static void check_special_ranges(void)
{
    const int counts[] = {0,1,57,105,141,199,200};
    PortalData before[PORTMAX + 1];
    for (unsigned int c = 0; c < ARRAYCOUNT(counts); c++) {
        int count = counts[c];
        reset();
        for (int p = 0; p < count; p++) {
            portals[p].portal = (Portal *)&geometry[p];
            portals[p].controlbytes1 = PORTALFLAG_DISABLED;
        }
        /* Following bytes stand in for visibility cells and polygon data. */
        memset(&portals[count + 1], 0xa5, (PORTMAX - count) * sizeof(*portals));
        memcpy(before,portals,sizeof(before));
        for (int level = LEVELID_CONTROL; level <= LEVELID_JUNGLE; level++) {
            memcpy(portals,before,sizeof(before));
            g_CurrentBgLevelId = level;
            bgMarkSpecialPortals();
            assert(!memcmp(before + count,portals + count,(PORTMAX + 1 - count) * sizeof(*portals)));
            for (int p = 0; p < count; p++) {
                int special = 0;
                for (unsigned int i = 0; i < ARRAYCOUNT(specialportalarray); i++) {
                    if (specialportalarray[i].levelid != level) continue;
                    const u8 *ranges = specialportalarray[i].portallist;
                    for (unsigned int r = 0; r + 1 < sizeof(specialportalarray[i].portallist) && ranges[r] != 255; r += 2)
                        if (p >= ranges[r] && p <= ranges[r + 1]) special = PORTALFLAG_SPECIAL;
                }
                assert(portals[p].controlbytes1 == (PORTALFLAG_DISABLED | special));
            }
        }
    }
    g_CurrentBgLevelId = 0;
    puts("PASS: legacy special-portal ranges stop at the actual table end, including 105 portals; following data is untouched.");
}

static void check_planes(void)
{
    reset(); add(67,32); horizontal(0,329);
    bounds(32,(coord3d){-488,158,-942},(coord3d){826,524,-122});
    bounds(67,(coord3d){-614,0,-540},(coord3d){488,524,-42});
    build_geometry();
    assert(portals[0].connectedRoom1 == 67 && portals[0].connectedRoom2 == 32);
    check_control_connection(0);
    /* Reversing the authored polygon cannot fix a misleading center. */
    coord3d swap = geometry[0].points[0];
    geometry[0].points[0] = geometry[0].points[2]; geometry[0].points[2] = swap;
    build_geometry(); check_control_connection(0);

    camera.y = 240;
    portalBoxes[0] = box(20,30,80,90);
    run(32,FALSE);
    assert(g_BgRoomsScheduledToBeDrawn == 2);
    assert(!memcmp(&g_BgDrawSlots[1].bbox,&portalBoxes[0],sizeof(bbox2d)));
    portalVisible[0] = FALSE; run(32,FALSE); assert(!g_BgRoomInfo[67].room_rendered);
    portalVisible[0] = TRUE; portals[0].controlbytes1 = PORTALFLAG_DISABLED;
    run(32,FALSE); assert(!g_BgRoomInfo[67].room_rendered);
    portals[0].controlbytes1 = 0;
    clear_view(32);
    bbox2d other = box(0,0,10,10);
    bgQueuePortalTraversal(0,32,0,1,other.f[0]); drain();
    assert(!g_BgRoomInfo[67].room_rendered);

    reset(); add(1,2); horizontal(0,100);
    bounds(1,(coord3d){-488,0,-323},(coord3d){488,100,-122});
    bounds(2,(coord3d){-488,100,-323},(coord3d){488,200,-122});
    build_geometry();
    assert(g_BgPortalPlaneCullMasks[0] == 3);
    camera.y = 50; run(1,FALSE); assert(g_BgRoomInfo[2].room_rendered);
    camera.y = 150; run(1,FALSE); assert(!g_BgRoomInfo[2].room_rendered);
    run(2,FALSE); assert(g_BgRoomInfo[1].room_rendered);
    camera.y = 50; run(2,FALSE); assert(!g_BgRoomInfo[1].room_rendered);

    /* One ambiguous destination only disables its own incoming side test. */
    g_BgRoomInfo[1].maxbounds.y = 150;
    build_geometry(); assert(g_BgPortalPlaneCullMasks[0] == BG_PORTAL_CULL_FROM_ROOM1);
    portals[0].controlbytes1 |= PORTALFLAG_FORCE_SIDE_CULL;
    bgBuildPortalCache(); assert(g_BgPortalPlaneCullMasks[0] == 3);
    camera.y = 50; clear_view(2);
    productionProcessPortalTraversal(0,2,0,1,&player.screensize);
    assert(!g_BgRoomInfo[1].room_rendered);
    camera.y = 150; clear_view(2);
    productionProcessPortalTraversal(0,2,0,1,&player.screensize);
    assert(g_BgRoomInfo[1].room_rendered);
    /* Door toggles preserve the authored bit and still close the opening. */
    bgToggleDataPortalsContrlBytes1Bit1(0,0);
    assert(portals[0].controlbytes1 == (PORTALFLAG_FORCE_SIDE_CULL | PORTALFLAG_DISABLED));
    clear_view(2); productionProcessPortalTraversal(0,2,0,1,&player.screensize);
    assert(!g_BgRoomInfo[1].room_rendered);
    bgToggleDataPortalsContrlBytes1Bit1(0,1);
    assert(portals[0].controlbytes1 == PORTALFLAG_FORCE_SIDE_CULL);
    /* Existing positive margin still allows a camera close to the plane. */
    portals[0].controlbytes2 = 0x18; /* Four native units. */
    camera.y = 99; clear_view(2);
    productionProcessPortalTraversal(0,2,0,1,&player.screensize);
    assert(g_BgRoomInfo[1].room_rendered);
    portals[0].controlbytes2 = 0;
    camera.y = 150; portalVisible[0] = FALSE; clear_view(2);
    productionProcessPortalTraversal(0,2,0,1,&player.screensize);
    assert(!g_BgRoomInfo[1].room_rendered);
    portalVisible[0] = TRUE; clear_view(2);
    bbox2d separate = box(110,110,120,120);
    productionProcessPortalTraversal(0,2,0,1,&separate);
    assert(!g_BgRoomInfo[1].room_rendered);
    portals[0].controlbytes1 &= ~PORTALFLAG_FORCE_SIDE_CULL;
    bgBuildPortalCache(); assert(g_BgPortalPlaneCullMasks[0] == BG_PORTAL_CULL_FROM_ROOM1);
    /* Another opening below the plane also counts, even without BG there. */
    add(2,3); horizontal(1,60);
    bounds(3,(coord3d){-488,0,-323},(coord3d){488,60,-122});
    build_geometry(); assert(g_BgPortalPlaneCullMasks[0] == 0);

    /* Verify oblique-plane extrema against all eight AABB corners. */
    for (int trial = 0; trial < 1000; trial++) {
        struct PortalMetric metric;
        metric.min = metric.max = (int)(random32() % 101) - 50;
        for (int a = 0; a < 3; a++) {
            metric.normal.f[a] = ((int)(random32() % 31) - 15) / 16.0f;
            g_BgRoomInfo[1].minbounds.f[a] = -(int)(random32() % 100);
            g_BgRoomInfo[1].maxbounds.f[a] = random32() % 100;
        }
        for (int positive = 0; positive < 2; positive++) {
            int all = TRUE;
            for (int c = 0; c < 8; c++) {
                float distance = 0;
                for (int a = 0; a < 3; a++)
                    distance += metric.normal.f[a] * ((c & (1 << a))
                        ? g_BgRoomInfo[1].maxbounds.f[a] : g_BgRoomInfo[1].minbounds.f[a]);
                all &= positive ? distance >= metric.min : distance <= metric.max;
            }
            assert(bgRoomFitsPortalSide(1,&metric,positive) == all);
        }
    }
    puts("PASS: Control 32/67 fails in both directions with the legacy plane test; corrected traversal preserves clipping, closed/off-screen rejection and proven side culling.");
    check_special_ranges();
}

static void check_geometry(const char *path)
{
    FILE *file = fopen(path,"r"); assert(file); reset();
    int rooms, room, count;
    assert(fscanf(file,"%d",&rooms) == 1 && rooms < MAXROOMCOUNT);
    g_MaxNumRooms = rooms + 1;
    for (int i = 0; i < rooms; i++) {
        coord3d lo,hi;
        assert(fscanf(file,"%d %f %f %f %f %f %f",&room,&lo.x,&lo.y,&lo.z,&hi.x,&hi.y,&hi.z) == 7);
        assert(room > 0 && room < g_MaxNumRooms); bounds(room,lo,hi);
    }
    assert(fscanf(file,"%d",&count) == 1 && count < PORTMAX);
    for (int p = 0; p < count; p++) {
        int a,b,flags,margin,points;
        assert(fscanf(file,"%d %d %d %d %d",&a,&b,&flags,&margin,&points) == 5);
        add(a,b); portals[p].controlbytes1 = flags; portals[p].controlbytes2 = margin;
        assert(points >= 3 && points <= 8); geometry[p].numPoints = points;
        for (int v = 0; v < points; v++) {
            coord3d *point = &geometry[p].points[v];
            assert(fscanf(file,"%f %f %f",&point->x,&point->y,&point->z) == 3);
        }
        portals[p].portal = (Portal *)&geometry[p];
    }
    fclose(file); build_geometry();
    g_CurrentBgLevelId = LEVELID_CONTROL; bgMarkSpecialPortals();
    if (count > 87 && ((portals[87].connectedRoom1 == 32 && portals[87].connectedRoom2 == 67)
        || (portals[87].connectedRoom1 == 67 && portals[87].connectedRoom2 == 32))) {
        check_control_connection(87);
        puts("PASS: supplied Control portal 87 bounds/order reproduce and fix both missing-room directions.");
    }
    if (rooms == 62 && count == 76) {
        for (int p = 68; p <= 69; p++) {
            assert((portals[p].connectedRoom1 == 1 && portals[p].connectedRoom2 == 50)
                || (portals[p].connectedRoom1 == 50 && portals[p].connectedRoom2 == 1));
            assert(g_BgPortalPlaneCullMasks[p] == 0);
            camera = (coord3d){-300,390,-500}; clear_view(50);
            productionProcessPortalTraversal(0,50,p,1,&player.screensize);
            assert(g_BgRoomInfo[1].room_rendered);
            portals[p].controlbytes1 |= PORTALFLAG_FORCE_SIDE_CULL; bgBuildPortalCache();
            assert(g_BgPortalPlaneCullMasks[p] == 3);
            clear_view(50); productionProcessPortalTraversal(0,50,p,1,&player.screensize);
            assert(!g_BgRoomInfo[1].room_rendered);
            camera = (coord3d){0,390,200}; clear_view(50);
            productionProcessPortalTraversal(0,50,p,1,&player.screensize);
            assert(g_BgRoomInfo[1].room_rendered); /* Bow-facing window view. */
            camera = (coord3d){0,390,-100}; clear_view(1);
            productionProcessPortalTraversal(0,1,p,1,&player.screensize);
            assert(g_BgRoomInfo[50].room_rendered); /* From inside the bridge. */
            portals[p].controlbytes1 &= ~PORTALFLAG_FORCE_SIDE_CULL; bgBuildPortalCache();
        }
        puts("PASS: Frigate windows 68/69 reject room 61's side with the flag; bow and bridge views remain eligible.");
    }
    int maxpeak = 0;
    for (int root = 1; root <= rooms; root++) {
        int seen[MAXROOMCOUNT]; bbox2d windows[MAXROOMCOUNT];
        bgGetRoomCenter(root,&camera);
        oracle(root,seen,windows); run(root,FALSE);
        for (int r = 1; r <= rooms; r++) assert(seen[r] == !!g_BgRoomInfo[r].room_rendered);
        if (peak > maxpeak) maxpeak = peak;
    }
    printf("PASS: supplied BG: %d rooms, %d portals; all room-center/full-aperture cases match closure; peak queue %d.\n",rooms,count,maxpeak);
}
