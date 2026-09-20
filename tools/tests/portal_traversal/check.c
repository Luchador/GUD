static bbox2d box(float left, float top, float right, float bottom)
{ bbox2d b = {.min = {left, top}, .max = {right, bottom}}; return b; }

void bgProcessPortalTraversal(s32 value, s32 room, s32 portal, s32 depth, bbox2d *parent)
{
    bbox2d saved = *parent;
    dispatched++;
    if (maximumDepth < depth) maximumDepth = depth;
    lastParent = *parent;
    if (requeueSelf) {
        bbox2d expanded = box(0, 0, 100, 100);
        requeueSelf = FALSE;
        bgQueuePortalTraversal(value, room, portal, depth + 1, expanded.f[0]);
        assert(!memcmp(&saved, parent, sizeof(saved)));
    }
    if (!traceOnly) productionProcessPortalTraversal(value, room, portal, depth, parent);
}

static void reset(void)
{
    memset(portals, 0, sizeof(portals));
    memset(g_BgRoomInfo, 0, sizeof(g_BgRoomInfo));
    memset(g_BgDrawSlots, 0, sizeof(g_BgDrawSlots));
    memset(g_BgRoomPortalOffsets, 0, sizeof(g_BgRoomPortalOffsets));
    memset(g_BgRoomPortalIndices, 0, sizeof(g_BgRoomPortalIndices));
    memset(g_PortalTraversalDepths, 0, sizeof(g_PortalTraversalDepths));
    camera = (coord3d){0,0,0};
    player.screensize = box(0,0,100,100);
    bgResetPortalQueue(); bgResetPortalVisitCounts();
    for (int i = 0; i < PORTMAX; i++) {
        g_PortalPlanes[i] = (struct PortalMetric){{1,0,0},1,1};
        portalBoxes[i] = player.screensize;
        portalVisible[i] = TRUE;
    }
    for (int i = 0; i < MAXROOMCOUNT; i++) roomVisible[i] = TRUE;
    numPortals = dispatched = peak = maximumDepth = traceOnly = requeueSelf = 0;
    legacyWrite = legacyRead = 0;
}

static int add(int a, int b)
{
    assert(numPortals < PORTMAX && a > 0 && a < MAXROOMCOUNT && b > 0 && b < MAXROOMCOUNT);
    portals[numPortals].connectedRoom1 = a;
    portals[numPortals].connectedRoom2 = b;
    return numPortals++;
}

static void adjacency(void)
{
    int offset = 0;
    for (int room = 0; room < BG_PORTAL_ROOM_COUNT; room++) {
        g_BgRoomPortalOffsets[room] = offset;
        for (int p = 0; p < numPortals; p++) {
            if (portals[p].connectedRoom1 == room || portals[p].connectedRoom2 == room)
                g_BgRoomPortalIndices[offset++] = p;
        }
    }
    g_BgRoomPortalOffsets[BG_PORTAL_ROOM_COUNT] = offset;
}

static void clear_view(int root)
{
    rootRoom = root;
    memset(g_BgRoomInfo, 0, sizeof(g_BgRoomInfo));
    bgResetPortalQueue(); bgResetPortalVisitCounts();
    legacyRead = legacyWrite = dispatched = peak = maximumDepth = 0;
    bbox2d full = player.screensize;
    bgSetRoomOnScreen(root, 0, &full, 1);
}

static void drain(void)
{
    while (g_BgPortalQueueCount) {
        assert(g_BgPortalQueueCount <= BG_PORTAL_QUEUE_LEN);
        assert(g_BgPortalQueueReadIndex >= 0 && g_BgPortalQueueReadIndex < BG_PORTAL_QUEUE_LEN);
        assert(g_BgPortalQueueWriteIndex >= 0 && g_BgPortalQueueWriteIndex < BG_PORTAL_QUEUE_LEN);
        if (peak < g_BgPortalQueueCount) peak = g_BgPortalQueueCount;
        assert(dispatched < 100000);
        assert(bgProcessNextQueuedPortal());
    }
    assert(!bgProcessNextQueuedPortal());
}

static void run(int root, int legacy)
{
    clear_view(root);
    for (int p = 0; p < numPortals; p++) {
        if (portals[p].connectedRoom1 != root && portals[p].connectedRoom2 != root) continue;
        if (legacy) legacyEnqueue(0, root, p, 1, player.screensize.f[0]);
        else bgQueuePortalTraversal(0, root, p, 1, player.screensize.f[0]);
    }
    if (legacy) { int n = 0; while (legacyNext()) assert(++n < 100000); }
    else drain();
}

static void check_cutoff(void)
{
    reset(); add(1,2); add(1,3);
    for (int i = 0; i < 8; i++) {
        int p = add(2,4);
        if (i & 1) portals[p].controlbytes1 = PORTALFLAG_DISABLED;
        else portalVisible[p] = FALSE;
    }
    add(3,4); add(4,5); adjacency();
    run(1, TRUE);
    assert(g_BgRoomInfo[4].portal_visit_count == 9 && !g_BgRoomInfo[4].room_rendered);
    assert(!g_BgRoomInfo[5].room_rendered);
    run(1, FALSE);
    assert(g_BgRoomInfo[4].room_rendered && g_BgRoomInfo[5].room_rendered);
    assert(peak < 20);
    puts("PASS: legacy eight-attempt cutoff hides two rooms after rejected approaches; new traversal reaches both without queue pressure.");
}

static void check_ring_and_windows(void)
{
    reset(); add(1,2); traceOnly = TRUE;
    for (int i = 0; i < 500; i++) legacyEnqueue(0,1,0,1,player.screensize.f[0]);
    assert(legacyWrite == -1); /* Stop before the legacy out-of-bounds write. */
    reset(); traceOnly = TRUE;
    for (int i = 0; i < PORTMAX; i++) {
        add(1,2);
        bgQueuePortalTraversal(0,1,i,1,player.screensize.f[0]);
        bgQueuePortalTraversal(0,2,i,1,player.screensize.f[0]);
    }
    assert(g_BgPortalQueueCount == BG_PORTAL_QUEUE_LEN);
    assert(g_BgPortalQueueReadIndex == g_BgPortalQueueWriteIndex);
    for (int i = 0; i < 2000; i++) bgQueuePortalTraversal(0,1,i % PORTMAX,2,player.screensize.f[0]);
    assert(g_BgPortalQueueCount == BG_PORTAL_QUEUE_LEN);
    drain(); assert(dispatched == PORTMAX * 2);
    assert(sizeof(g_BgPortalQueue) + sizeof(g_BgPortalQueueOrder) + sizeof(g_BgPortalQueueState) < sizeof(legacyQueue));

    /* Repeated widening and wraparound; pending updates merge in place. */
    reset(); traceOnly = TRUE; add(1,2);
    for (int i = 1; i <= 1200; i++) {
        bbox2d b = box(0,0,(float)i,100);
        bgQueuePortalTraversal(0,1,0,1,b.f[0]);
        bgQueuePortalTraversal(0,1,0,2,b.f[0]);
        assert(g_BgPortalQueueCount == 1); drain();
        assert(lastParent.max.x == i);
    }
    assert(dispatched == 1200);
    bbox2d narrow = box(10,10,20,20), wider = box(0,0,80,80);
    bgResetPortalQueue(); bgQueuePortalTraversal(0,1,0,1,narrow.f[0]);
    bgQueuePortalTraversal(0,1,0,2,wider.f[0]);
    assert(g_BgPortalQueueCount == 1); drain(); assert(lastParent.max.x == 80);
    bgResetPortalQueue(); bgQueuePortalTraversal(0,1,0,1,narrow.f[0]);
    requeueSelf = TRUE; dispatched = 0; drain(); assert(dispatched == 2 && lastParent.max.x == 100);

    /* A processed narrow window must not swallow a later useful one. */
    reset(); add(1,2); adjacency(); clear_view(1);
    portalBoxes[0] = box(70,0,90,100);
    bgQueuePortalTraversal(0,1,0,1,narrow.f[0]); drain(); assert(!g_BgRoomInfo[2].room_rendered);
    bgQueuePortalTraversal(0,1,0,2,wider.f[0]); drain(); assert(g_BgRoomInfo[2].room_rendered);
    bgResetPortalQueue(); assert(!g_BgPortalQueueCount);
    bgQueuePortalTraversal(0,1,-1,1,narrow.f[0]); bgQueuePortalTraversal(0,1,PORTMAX,1,narrow.f[0]);
    bgQueuePortalTraversal(0,3,0,1,narrow.f[0]); assert(!g_BgPortalQueueCount);
    puts("PASS: old wrap produces -1; new full ring, both directions, 1200 queue reuse steps, pending merges, expanding revisits and in-flight copies remain safe.");
}

static void check_cycles_and_specials(void)
{
    reset(); add(1,2); add(2,3); add(3,4); add(4,2); add(3,5); adjacency();
    run(1,FALSE); assert(g_BgRoomInfo[5].room_rendered && dispatched < 20);
    portals[4].controlbytes1 = PORTALFLAG_DISABLED; run(1,FALSE); assert(!g_BgRoomInfo[5].room_rendered);
    portals[4].controlbytes1 = PORTALFLAG_SPECIAL; roomVisible[5] = FALSE;
    run(1,FALSE); assert(!g_BgRoomInfo[5].room_rendered);
    roomVisible[5] = TRUE; run(1,FALSE); assert(g_BgRoomInfo[5].room_rendered);
    /* Global visibility marks a room before traversal; its normal paths still work. */
    clear_view(1); bbox2d b = box(0,0,20,20); bgSetRoomOnScreen(3,0,&b,0);
    bgQueuePortalTraversal(0,1,0,1,player.screensize.f[0]); drain(); assert(g_BgRoomInfo[5].room_rendered);
    puts("PASS: cycles terminate; disabled/special portals, room tests and script-added rooms retain their semantics.");
}

/* Independent monotone room/window relaxation: no ring, visit counts or
 * per-portal queue records. Used with authored graph edges and synthetic
 * projections to check closure; it does not reproduce an in-game camera. */
static void oracle(int root, int *seen, bbox2d *windows)
{
    int changed = TRUE, passes = 0;
    memset(seen,0,MAXROOMCOUNT*sizeof(*seen));
    seen[root] = TRUE; windows[root] = player.screensize;
    while (changed) {
        changed = FALSE; assert(++passes < 10000);
        for (int p = 0; p < numPortals; p++) for (int side = 0; side < 2; side++) {
            int from = side ? portals[p].connectedRoom2 : portals[p].connectedRoom1;
            int to = side ? portals[p].connectedRoom1 : portals[p].connectedRoom2;
            if (!seen[from] || (portals[p].controlbytes1 & PORTALFLAG_DISABLED)) continue;
            float low = g_PortalPlanes[p].min, high = g_PortalPlanes[p].max;
            if ((!side && high <= 0) || (side && low >= 0)) continue;
            bbox2d b;
            if (low < 0 && high > 0) b = player.screensize;
            else {
                if (!portalVisible[p]) continue;
                b = portalBoxes[p];
                if (b.min.x < windows[from].min.x) b.min.x = windows[from].min.x;
                if (b.min.y < windows[from].min.y) b.min.y = windows[from].min.y;
                if (b.max.x > windows[from].max.x) b.max.x = windows[from].max.x;
                if (b.max.y > windows[from].max.y) b.max.y = windows[from].max.y;
            }
            if (b.min.x >= b.max.x || b.min.y >= b.max.y) continue;
            if (!seen[to]) { seen[to] = TRUE; windows[to] = b; changed = TRUE; }
            else {
                bbox2d *w = &windows[to];
                if (b.min.x < w->min.x) { w->min.x = b.min.x; changed = TRUE; }
                if (b.min.y < w->min.y) { w->min.y = b.min.y; changed = TRUE; }
                if (b.max.x > w->max.x) { w->max.x = b.max.x; changed = TRUE; }
                if (b.max.y > w->max.y) { w->max.y = b.max.y; changed = TRUE; }
            }
        }
    }
}

static u32 seed = 12345;
static u32 random32(void) { seed = seed * 1664525u + 1013904223u; return seed; }

static void check_native(const char *path)
{
    int a,b,maxroom = 0,total = 0, maxpeak = 0,maxdepth = 0;
    int seen[MAXROOMCOUNT]; bbox2d windows[MAXROOMCOUNT];
    FILE *f = fopen(path,"r"); assert(f); reset();
    while (fscanf(f,"%d %d",&a,&b) == 2) {
        add(a,b); if(maxroom<a)maxroom=a;if(maxroom<b)maxroom=b;
    }
    fclose(f); adjacency();
    for (int root = 1; root <= maxroom; root++) for (int trial = 0; trial < 21; trial++) {
        for (int p = 0; p < numPortals; p++) {
            portals[p].controlbytes1 = trial && random32()%13 == 0 ? PORTALFLAG_DISABLED : 0;
            portalVisible[p] = !trial || random32()%13 != 0;
            float x = random32()%7*10, y = random32()%7*10;
            portalBoxes[p] = trial ? box(x,y,100,100) : player.screensize;
            g_PortalPlanes[p].min = (random32() >> 16) & 1 ? 1 : -1;
            g_PortalPlanes[p].max = g_PortalPlanes[p].min;
            if (!trial) {g_PortalPlanes[p].min=-1;g_PortalPlanes[p].max=1;}
        }
        oracle(root,seen,windows); run(root,FALSE);
        for (int r = 1; r <= maxroom; r++) assert(seen[r] == !!g_BgRoomInfo[r].room_rendered);
        for (int i = 0; i < g_BgRoomsScheduledToBeDrawn; i++) {
            int r = g_BgDrawSlots[i].roomid;
            assert(!memcmp(&windows[r],&g_BgDrawSlots[i].bbox,sizeof(bbox2d)));
        }
        if(maxpeak<peak)maxpeak=peak;if(maxdepth<maximumDepth)maxdepth=maximumDepth;
        total++;
    }
    printf("PASS: %s: %d portals, %d camera-room/aperture cases match independent closure; peak queue %d, depth %d.\n",path,numPortals,total,maxpeak,maxdepth);
}

int main(int argc, char **argv)
{
    if (argc == 2) check_native(argv[1]);
    else { check_cutoff(); check_ring_and_windows(); check_cycles_and_specials(); }
    return 0;
}
