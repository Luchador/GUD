static Gfx output[20000];

static BgDebugWalk walker(Gfx *out, s32 capacity)
{
    BgDebugWalk w;
    s32 i;
    memset(&w, 0, sizeof(w));
    w.out = out;
    w.end = out + capacity;
    w.remaining = BG_DEBUG_COMMAND_LIMIT;
    for (i = 0; i < 16; i++) w.segments[i] = 0xffffffffu;
    w.segments[0] = 0;
    w.segments[14] = 0x40000;
    w.segments[15] = 0x1000;
    return w;
}

static void decoderChecks(void)
{
    static const Gfx stream[] = {
        gsSPVertex(0x0e000010, 3, 0),
        gsDPSetCycleType(G_CYC_2CYCLE),
        gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2),
        gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA),
        gsSPSetGeometryMode(G_CULL_BACK | G_FOG | G_ZBUFFER),
        gsDPSetScissor(G_SC_NON_INTERLACE, 50, 50, 80, 80),
        gsSP1Triangle(0, 1, 2, 0),
        {{0xb1000002, 0x00000010}}, /* TRI4: (0,1,2) + 3 empty slots */
        gsSPEndDisplayList()
    };
    Gfx *root = (Gfx *)(g_TestRam + 0x1000);
    Gfx *child = (Gfx *)(g_TestRam + 0x2000);
    BgDebugWalk w = walker(output, 100);
    bgDebugCopyGeometry(&w, stream, sizeof(stream), 0);
    assert(!w.incomplete && w.out == output + 3);
    assert(output[0].words.w1 == 0x40010);
    assert(output[1].words.w0 == stream[6].words.w0 && output[1].words.w1 == stream[6].words.w1);
    assert(output[2].words.w0 == stream[7].words.w0 && output[2].words.w1 == stream[7].words.w1);
    for (int capacity = 0; capacity < 4; capacity++) {
        memset(output, 0x5a, sizeof(output));
        w = walker(output, capacity);
        bgDebugCopyGeometry(&w, stream, sizeof(stream), 0);
        assert(w.out <= output + capacity);
        assert(w.incomplete == (capacity < 3));
        assert(output[capacity].words.w0 == 0x5a5a5a5a);
    }
    /* Nested material calls can contain geometry; segment changes persist on
     * return, but neither they nor material commands reach the output. */
    root[0] = stream[0];
    gSPDisplayList(root + 1, 0x0f001000);
    root[2] = stream[6];
    gSPEndDisplayList(root + 3);
    child[0] = stream[2]; child[1] = stream[7];
    gSPSegment(child + 2, 14, 0x50000);
    child[3] = stream[0];
    gSPEndDisplayList(child + 4);
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, 4 * sizeof(Gfx), 0);
    assert(!w.incomplete && w.out == output + 4 && output[2].words.w1 == 0x50010);
    gSPBranchList(root + 1, 0x0f001000);
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, 4 * sizeof(Gfx), 0);
    assert(!w.incomplete && w.out == output + 3);
    /* Cycles, bad pointers, unsupported transforms and invalid vertex slots
     * all terminate safely and explicitly mark the overlay incomplete. */
    gSPDisplayList(child, 0x0f001000);
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, 4 * sizeof(Gfx), 0);
    assert(w.incomplete);
    gSPBranchList(child, 0x0f001000);
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, 4 * sizeof(Gfx), 0);
    assert(w.incomplete);
    gSPDisplayList(root, sizeof(g_TestRam) - 4);
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, 4 * sizeof(Gfx), 0);
    assert(w.incomplete);
    root[0] = stream[6];
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, 4 * sizeof(Gfx), 0);
    assert(w.incomplete && w.out == output);
    gSPMatrix(root, 0x8000, G_MTX_LOAD);
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, sizeof(Gfx), 0);
    assert(w.incomplete && w.out == output);
    root[0] = stream[0];
    w = walker(output, 100);
    bgDebugCopyGeometry(&w, root, sizeof(Gfx), 0);
    assert(w.incomplete); /* no terminator */
    w = walker(output, 100); w.remaining = 1;
    bgDebugCopyGeometry(&w, stream, sizeof(stream), 0);
    assert(w.incomplete);
}

static void integrationChecks(void)
{
    Gfx *end;
    Gfx *primary = (Gfx *)(g_TestRam + 0x1000);
    Gfx *secondary = primary + 8;
    gSPVertex(primary, 0x0e000000, 3, 0);
    gSP1Triangle(primary + 1, 0, 1, 2, 0);
    gSPEndDisplayList(primary + 2);
    memcpy(secondary, primary, 3 * sizeof(Gfx));
    g_BgData = g_TestRam;
    g_TestPlayer.outside_watch_menu = 1;
    g_TestPlayer.viewleft = 0; g_TestPlayer.viewtop = 10;
    g_TestPlayer.viewx = 320; g_TestPlayer.viewy = 220;
    g_TestPlayer.curRoomIndex = 1;
    g_BgRoomInfo[1].primaryGdl = primary;
    g_BgRoomInfo[1].primaryGdlSize = 3 * sizeof(Gfx);
    g_BgRoomInfo[1].secondaryGdl = secondary;
    g_BgRoomInfo[1].secondaryGdlSize = 3 * sizeof(Gfx);
    g_BgRoomInfo[1].vertices = (Vtx *)(g_TestRam + 0x40000);
    bgDebugReset();
    bgDebugInput(U_JPAD, FALSE); assert(!g_BgDebugEnabled);
    bgDebugInput(U_JPAD, TRUE); assert(g_BgDebugEnabled);
    bgDebugInput(0, TRUE); assert(g_BgDebugEnabled);
    bgDebugInput(U_JPAD | D_JPAD, TRUE); assert(!g_BgDebugEnabled);
    bgDebugInput(U_JPAD, TRUE);
    bgDebugInput(D_JPAD, FALSE); assert(!g_BgDebugEnabled);
    bgDebugInput(U_JPAD, TRUE);
    bgDebugBeginFrame();
    /* Loading without submitting must not enter the overlay's room list. */
    bgRenderRoomPrimary(output, 1); assert(!g_BgDebugRoomCount);
    bgRenderRoomSecondary(output, 1); assert(!g_BgDebugRoomCount && g_TestLoads == 1);
    g_BgRoomInfo[1].unloadAge = 1;
    bgRenderRoomPrimary(output, 1);
    assert(g_BgDebugRoomCount == 1 && g_BgDebugSubmitted[1] == BG_DEBUG_PRIMARY);
    bgRenderRoomSecondary(output, 1);
    assert(g_BgDebugRoomCount == 1 && g_BgDebugSubmitted[1] == 3);
    bgDebugRecordRoom(-1, 1); bgDebugRecordRoom(MAXROOMCOUNT, 1);
    assert(g_BgDebugRoomCount == 1);
    g_TestGfxEnd = output + 5000;
    g_TestMatrices = 0;
    end = bgDebugRender(output);
    assert(end > output && !g_BgDebugIncomplete && g_TestMatrices == 1 && g_TestLoads == 1);
    bool mode = FALSE, combine = FALSE, scissor = FALSE;
    int vertices = 0, triangles = 0;
    for (Gfx *p = output; p < end; p++) {
        u32 op = p->words.w0 >> 24;
        if (op == (u8)G_VTX) vertices++;
        if (op == (u8)G_TRI1) triangles++;
        if (op == (u8)G_SETOTHERMODE_L && ((p->words.w0 >> 8) & 255) == G_MDSFT_RENDERMODE) {
            assert(p->words.w1 == (G_RM_XLU_SURF | G_RM_XLU_SURF2)); mode = TRUE;
        }
        if (op == (u8)G_SETCOMBINE) {
            Gfx expected = gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            assert(!memcmp(p, &expected, sizeof(Gfx))); combine = TRUE;
        }
        if (op == (u8)G_SETSCISSOR) {
            Gfx expected = gsDPSetScissor(G_SC_NON_INTERLACE, 0, 10, 320, 230);
            assert(!memcmp(p, &expected, sizeof(Gfx))); scissor = TRUE;
        }
    }
    assert(mode && combine && scissor && vertices == 2 && triangles == 2);
    g_TestVisibilityStats = (BgVisibilityStats){20, 5, 1, 12, 12, FALSE};
    bgDebugDrawHud(end);
    assert(strstr(g_TestLabels[0], "1 ROOMS") && !strcmp(g_TestLabels[1], "1"));
    assert(!strcmp(g_TestLabels[2], "PORTAL PEAK: 20  VISIBLE: 5"));
    assert(!strcmp(g_TestLabels[3], "WAIT: 1  FIRST ROOM: 12"));
    assert(!strcmp(g_TestLabels[4], "ALLOC FAIL: ROOM 12"));
    g_TestLabelCount = 0;
    g_TestVisibilityStats = (BgVisibilityStats){2, 1, 0, -1, -1, FALSE};
    bgDebugDrawHud(end);
    assert(!strcmp(g_TestLabels[3], "WAIT: NONE"));
    assert(!strcmp(g_TestLabels[4], "ALLOC FAIL: NONE  CACHE: OFF"));
    for (int capacity = 0; capacity < BG_DEBUG_GFX_RESERVE + 80; capacity++) {
        g_TestGfxEnd = output + capacity;
        g_BgDebugIncomplete = FALSE;
        end = bgDebugRender(output);
        assert(end <= g_TestGfxEnd);
        if (capacity < BG_DEBUG_GFX_RESERVE + 32) assert(end == output && g_BgDebugIncomplete);
    }
    g_TestGfxEnd = output + 5000;
    g_TestPlayer.pause_state = 1; assert(bgDebugRender(output) == output);
    g_TestPlayer.pause_state = 0;
    g_TestStage = LEVELID_TITLE; assert(bgDebugRender(output) == output); g_TestStage = 0;
    g_TestPlayerCount = 2; assert(bgDebugRender(output) == output); g_TestPlayerCount = 1;
    bgDebugInput(D_JPAD, FALSE);
    bgDebugBeginFrame();
    assert(bgDebugRender(output) == output && bgDebugDrawHud(output) == output);
    bgRenderRoomPrimary(output, 1); assert(!g_BgDebugRoomCount);
    bgDebugInput(U_JPAD, TRUE); bgDebugBeginFrame();
    assert(!g_BgDebugSubmitted[1]);
    bgDebugReset(); assert(!g_BgDebugEnabled);
}

static void assetChecks(const char *path)
{
    FILE *file = fopen(path, "rb");
    u32 size, expected, words[2];
    s32 lists = 0;
    Gfx *src = (Gfx *)(g_TestRam + 0x1000);
    assert(file);
    while (fread(words, 4, 2, file) == 2) {
        size = __builtin_bswap32(words[0]); expected = __builtin_bswap32(words[1]);
        assert(size < 0x30000);
        assert(fread(src, 1, size, file) == size);
        for (u32 i = 0; i < size / sizeof(Gfx); i++) {
            src[i].words.w0 = __builtin_bswap32(src[i].words.w0);
            src[i].words.w1 = __builtin_bswap32(src[i].words.w1);
        }
        BgDebugWalk w = walker(output, 20000);
        bgDebugCopyGeometry(&w, src, size, 0);
        if (w.incomplete || w.out - output != expected) {
            fprintf(stderr, "%s list %d: incomplete %d, emitted %ld, expected %u\n",
                    path, lists, w.incomplete, (long)(w.out - output), expected);
            abort();
        }
        lists++;
    }
    fclose(file);
    printf("PASS: %s (%d native room streams)\n", path, lists);
}

int main(int argc, char **argv)
{
    decoderChecks();
    integrationChecks();
    if (argc > 1) assetChecks(argv[1]);
    else puts("PASS: decoder, nested lists, pipeline, input, submission tracking and capacity limits.");
    return 0;
}
