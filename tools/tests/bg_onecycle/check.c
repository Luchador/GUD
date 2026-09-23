/* Decode draw-time state independently of the converter's state tracker. */
typedef struct { u32 h, l, c0, c1, texture, env, blend; } Snapshot;
static int is_draw(Gfx g)
{
    u32 op = g.words.w0 >> 24;
    return op == 0xbf || op == 0xb1 || op == 0xe4 || op == 0xe5
            || op == 0xf6 || op == 0x06 || op == 0xbe || op == 0xb8 || op == 0xb5;
}
static int snapshots(Gfx *gdl, int count, Snapshot *states)
{
    Snapshot s = {0};
    int i, n = 0;
    for (i = 0; i < count; i++) {
        u32 op = gdl[i].words.w0 >> 24, data = gdl[i].words.w1;
        if (op == 0xba || op == 0xb9) {
            unsigned shift = (gdl[i].words.w0 >> 8) & 255, bits = gdl[i].words.w0 & 255;
            u32 mask = (u32)((((uint64_t)1 << bits) - 1) << shift);
            u32 *word = op == 0xba ? &s.h : &s.l;
            *word = (*word & ~mask) | (data & mask);
        } else if (op == 0xef) {
            s.h = gdl[i].words.w0 & 0xffffff;
            s.l = data;
        } else if (op == 0xfc) {
            s.c0 = gdl[i].words.w0; s.c1 = data;
        } else if (op == 0xfb) {
            s.env = data;
        } else if (op == 0xf9) {
            s.blend = data;
        } else if (op == 0xbb) {
            s.texture = gdl[i].words.w0;
        }
        if (is_draw(gdl[i])) states[n++] = s;
    }
    return n;
}

static const Gfx standard[] = {
    gsDPPipeSync(), gsDPSetCycleType(G_CYC_2CYCLE),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    gsDPSetTextureLOD(G_TL_LOD), gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP), gsDPSetColorDither(G_CD_BAYER),
    gsSPTexture(0xffff, 0xffff, 5, 0, 1),
    /* Expanded upload packets and vertex commands must survive verbatim. */
    {{0xfd10001f, 0x00123450}}, {{0xf5100000, 0x07000000}},
    {{0xe6000000, 0}}, {{0xf3000000, 0x073ff100}},
    {{0x0400303f, 0x0e000000}}, {{0xb1003210, 0x00003210}},
    gsSPEndDisplayList()
};

static int check_stream(Gfx *input, int size, int run_walker)
{
    int bytes, i, j = 0, converted = 0;
    Gfx *output, *saved, *reference;
    Snapshot *before, *after;
    int nb, na;
    for (i = 0; i < size / 8; i++) {
        u32 op = input[i].words.w0 >> 24;
        if (op == 0xb8 || (op == 6 && ((input[i].words.w0 >> 16) & 255))) {
            size = (i + 1) * 8;
            break;
        }
    }
    bytes = bgBuildOneCycleGdl(input, size, NULL, 0);
    if (!bytes) return 0;
    assert(bytes > 0);
    output = malloc(bytes + 8); saved = malloc(size); reference = malloc(size);
    before = calloc(size / 8, sizeof(*before)); after = calloc(bytes / 8, sizeof(*after));
    assert(output && saved && reference && before && after);
    memcpy(saved, input, size); memcpy(reference, input, size);
    for (i = 0; i < size / 8; i++) reference[i] = renderGetAaOffCommand(reference[i]);
    memset((u8 *)output + bytes, 0xa5, 8);
    assert(bgBuildOneCycleGdl(input, size, output, bytes) == bytes);
    assert(memcmp(input, saved, size) == 0);
    for (i = 0; i < 8; i++) assert(((u8 *)output)[bytes + i] == 0xa5);
    /* Every packet remains ordered; only render modes receive AA-Off mapping. */
    for (i = 0; i < bytes / 8 && j < size / 8; i++)
        if (memcmp(&output[i], &reference[j], 8) == 0) j++;
    assert(j == size / 8);
    if (run_walker) {
        Gfx *ramReference = (Gfx *)(g_TestRam + 0x10000);
        Gfx *ramOutput = (Gfx *)(g_TestRam + 0x20000);
        assert(size < 0x10000 && bytes < 0x10000);
        memcpy(ramReference, reference, size); memcpy(ramOutput, output, bytes);
        renderInvalidateDisplayListCache();
        renderSetAaEnabled(FALSE); renderApplySettings();
        assert(renderApplyDisplayListSettings(ramReference, ramReference + size / 8));
        assert(renderApplyDisplayListSettings(ramOutput, ramOutput + bytes / 8));
        memcpy(reference, ramReference, size); memcpy(output, ramOutput, bytes);
    }
    nb = snapshots(reference, size / 8, before);
    na = snapshots(output, bytes / 8, after);
    assert(nb == na);
    for (i = 0; i < nb; i++) {
        Snapshot *a = &before[i], *b = &after[i];
        if (memcmp(a, b, sizeof(*a)) == 0) continue;
        if ((b->h & (3u << 20)) != G_CYC_1CYCLE)
            fprintf(stderr, "draw %d/%d walker %d: %08x %08x %08x %08x -> %08x %08x %08x %08x\n",
                    i, nb, run_walker, a->h, a->l, a->c0, a->c1, b->h, b->l, b->c0, b->c1);
        assert((a->h & (3u << 20)) == G_CYC_2CYCLE);
        assert((b->h & (3u << 20)) == G_CYC_1CYCLE);
        assert(((a->h ^ b->h) & ~((3u << 20) | (1u << 16))) == 0);
        assert(a->texture == b->texture);
        assert((a->l & (Z_CMP | Z_UPD | 7)) == (b->l & (Z_CMP | Z_UPD | 7)));
        assert(!(b->l & (AA_EN | IM_RD | CVG_X_ALPHA | ZMODE_DEC | ZMODE_XLU)));
        if ((a->l & 0xcccc0000u) == (G_RM_FOG_SHADE_A)) {
            assert((b->l & 0xcccc0000u) == (G_RM_FOG_SHADE_A));
            assert(b->l & FORCE_BL);
        }
        converted++;
    }
    assert(converted > 0);
    /* The caller receives precisely the original outgoing state. */
    assert(memcmp(&before[nb - 1], &after[na - 1], sizeof(*before)) == 0);
    free(output); free(saved); free(reference); free(before); free(after);
    return converted;
}

static void unit_checks(void)
{
    Gfx src[128], out[256], *p;
    Snapshot states[64];
    int size, i, j;
    const u32 excluded[] = {G_RM_AA_ZB_TEX_EDGE2, G_RM_AA_ZB_XLU_SURF2,
        G_RM_AA_ZB_OPA_DECAL2, G_RM_AA_ZB_OPA_INTER2, G_RM_AA_ZB_XLU_DECAL2};
    memcpy(src, standard, sizeof(standard));
    assert(check_stream(src, sizeof(standard), 1) == 1);
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            memcpy(src, standard, sizeof(standard));
            src[2].words.w1 = (i ? G_RM_FOG_SHADE_A : G_RM_PASS) | G_RM_AA_ZB_OPA_SURF2;
            if (j) gDPSetTextureFilter(&src[6], G_TF_POINT);
            assert(check_stream(src, sizeof(standard), 1) == 1);
        }
    }
    /* Partial editor overrides, inherited combiners and returns from water. */
    for (i = 0; i < (int)(sizeof(excluded) / sizeof(excluded[0])); i++) {
        memcpy(src, standard, sizeof(standard)); p = src + sizeof(standard) / 8 - 1;
        gDPPipeSync(p++);
        gDPSetRenderMode(p++, G_RM_PASS, excluded[i]);
        gSP1Triangle(p++, 0, 1, 2, 0);
        gDPPipeSync(p++);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 3, 29, G_RM_PASS | G_RM_AA_ZB_OPA_SURF2);
        gSP1Triangle(p++, 0, 1, 2, 0);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 10, 2, ZMODE_DEC);
        gSP1Triangle(p++, 0, 1, 2, 0);
        gSPEndDisplayList(p++);
        assert(check_stream(src, (p - src) * 8, 1) == 2);
    }
    memcpy(src, standard, sizeof(standard)); p = src + sizeof(standard) / 8 - 1;
    gDPSetCombineMode(p++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gDPSetCombineMode(p++, G_CC_TRILERP, G_CC_MODULATEIA2);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gSPCullDisplayList(p++, 0, 3);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gSPDisplayList(p++, 0x100);
    gSP1Triangle(p++, 0, 1, 2, 0); /* Inherited child state is unknown: skip. */
    gSPEndDisplayList(p++);
    assert(check_stream(src, (p - src) * 8, 0) == 3);

    /* Bounds, unsupported pipelines, explicit alpha compare and missing state. */
    memcpy(src, standard, sizeof(standard));
    size = bgBuildOneCycleGdl(src, sizeof(standard), NULL, 0);
    assert(bgBuildOneCycleGdl(src, sizeof(standard), out, size - 8) == -1);
    assert(bgBuildOneCycleGdl(src, sizeof(standard) - 8, out, sizeof(out)) == -1);
    assert(bgBuildOneCycleGdl(src, 7, out, sizeof(out)) == -1);
    assert(bgBuildOneCycleGdl(src, sizeof(standard), src, sizeof(src)) == -1);
    assert(bgBuildOneCycleGdl(NULL, sizeof(standard), out, sizeof(out)) == -1);
    for (i = 0; i < 3; i++) {
        memcpy(src, standard, sizeof(standard));
        gDPSetCycleType(&src[1], i == 0 ? G_CYC_1CYCLE : i == 1 ? G_CYC_COPY : G_CYC_FILL);
        assert(bgBuildOneCycleGdl(src, sizeof(standard), NULL, 0) == 0);
    }
    memcpy(src, standard, sizeof(standard)); gDPSetAlphaCompare(&src[0], G_AC_THRESHOLD);
    assert(bgBuildOneCycleGdl(src, sizeof(standard), NULL, 0) == 0);
    memcpy(src, standard, sizeof(standard)); gDPSetTextureDetail(&src[5], G_TD_DETAIL);
    assert(bgBuildOneCycleGdl(src, sizeof(standard), NULL, 0) == 0);
    memcpy(src, standard, sizeof(standard)); src[2].words.w0 = 0;
    assert(bgBuildOneCycleGdl(src, sizeof(standard), NULL, 0) == 0);

    /* Actual room allocation/free code and delayed AA application. */
    memcpy(src, standard, sizeof(standard));
    g_BgRoomInfo[1].primaryGdl = src; g_BgRoomInfo[1].primaryGdlSize = sizeof(standard);
    g_BgRoomInfo[1].unloadAge = 1;
    g_TestAllocFail = 1; bgBuildRoomOneCycleGdl(1);
    assert(!g_BgOneCycleRooms[1].gdl);
    g_TestAllocFail = 0; bgBuildRoomOneCycleGdl(1);
    assert(g_BgOneCycleRooms[1].gdl && g_TestAllocCount == 2);
    renderSetAaEnabled(TRUE); renderApplySettings();
    renderSetAaEnabled(FALSE);
    assert(!renderUseOneCycle());
    assert(bgRenderRoomPrimary(out, 1) == out + 2);
    assert(out[1].words.w1 == OS_K0_TO_PHYSICAL(src));
    renderApplySettings();
    assert(renderUseOneCycle());
    bgRenderRoomPrimary(out, 1);
    assert(out[1].words.w1 == OS_K0_TO_PHYSICAL(g_BgOneCycleRooms[1].gdl));
    renderSetAaEnabled(TRUE); renderApplySettings();
    bgRenderRoomPrimary(out, 1);
    assert(out[1].words.w1 == OS_K0_TO_PHYSICAL(src));
    bgFreeRoomData(1);
    assert(!g_BgOneCycleRooms[1].gdl && !g_BgOneCycleRooms[1].size && g_TestFreeCount == 1);
    assert(memcmp(src, standard, sizeof(standard)) == 0);
    /* Sanity check that the independent decoder sees the authored cycle. */
    snapshots(src, sizeof(standard) / 8, states);
    assert((states[0].h & (3u << 20)) == G_CYC_2CYCLE);
    puts("One-cycle: mixed materials, fog, depth, AA walker, bounds, state restoration and room lifecycle pass");
}

static u32 read_be(FILE *file)
{
    u8 b[4]; assert(fread(b, 1, 4, file) == 4);
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}
static void cutout_checks(void);
static void vertex_alpha_checks(void);
static void fog_participation_checks(void);
static void environment_checks(void);
static void cutout_asset_checks(const char *path);
int main(int argc, char **argv)
{
    if (argc == 1) { unit_checks(); cutout_checks(); vertex_alpha_checks(); fog_participation_checks(); environment_checks(); }
    else if (argc == 3) cutout_asset_checks(argv[1]);
    else {
        FILE *file = fopen(argv[1], "rb");
        int rooms = 0, converted = 0, i, lut;
        assert(file);
        while (fgetc(file) != EOF) {
            int size;
            Gfx *gdl, *original;
            fseek(file, -1, SEEK_CUR); size = read_be(file);
            assert(size > 0 && size % 8 == 0);
            gdl = malloc(size); original = malloc(size); assert(gdl && original);
            for (i = 0; i < size / 8; i++) {
                original[i].words.w0 = read_be(file); original[i].words.w1 = read_be(file);
            }
            for (lut = 0; lut < 2; lut++) {
                Gfx *table = lut ? DL_LUT_PRIMARY_ADDFOG : DL_LUT_PRIMARY;
                memcpy(gdl, original, size);
                for (i = 0; i < size / 8; i++) {
                    Gfx *pair;
                    for (pair = table; pair->words.w0; pair += 2)
                        if (memcmp(&gdl[i], pair, 8) == 0) gdl[i] = pair[1];
                }
                converted += check_stream(gdl, size, 0);
            }
            rooms++; free(gdl); free(original);
        }
        fclose(file);
        assert(rooms && converted);
        printf("%s: %d primary streams, %d converted triangle packets across fog/no-fog LUTs\n", argv[1], rooms, converted);
    }
    return 0;
}
