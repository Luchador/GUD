static const Gfx opaque[] = {
    gsDPSetTextureLOD(G_TL_LOD), gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP), gsDPSetColorDither(G_CD_BAYER),
    gsSPTexture(0xffff, 0xffff, 5, 0, 1),
    {{0xfd10001f, 0x00070000}}, {{0xf5100000, 0x07000000}},
    {{0xe6000000, 0}}, {{0xf3000000, 0x073ff100}},
    {{0x0400303f, 0x04000000}}, {{0xb1003210, 0x00003210}},
    gsSPEndDisplayList()
};
static const Gfx mixed[] = {
    gsDPSetTextureLOD(G_TL_LOD), gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP), gsSPTexture(0xffff, 0xffff, 5, 0, 1),
    {{0xbf000000, 0x00000a14}},
    gsDPPipeSync(), gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_DECAL2),
    {{0xbf000000, 0x00000a14}},
    gsDPPipeSync(), gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2),
    {{0xbf000000, 0x00000a14}},
    gsDPPipeSync(), gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2),
    {{0xbf000000, 0x00000a14}},
    gsDPPipeSync(), gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2),
    {{0xbf000000, 0x00000a14}}, gsSPEndDisplayList()
};

static void reset(void)
{
    modelOneCycleResetCache();
    allocated = allocations = frees = failAllocation = 0;
    renderInvalidateDisplayListCache();
    renderSetAaEnabled(FALSE);
    renderApplySettings();
}

static ModelRenderData prop(void)
{
    ModelRenderData data = {0};
    data.PropType = 9;
    data.flags = 1;
    data.zbufferenabled = TRUE;
    data.fogcolour.word = 0x31527194;
    return data;
}

static int setup_material(ModelRenderData *data, int type, Gfx *out)
{
    ModelRenderData copy = *data;
    copy.gdl = out;
    if (type == 2) modelApplyRenderModeType2(&copy);
    else if (type == 3) modelApplyRenderModeType3(&copy, TRUE);
    else modelApplyRenderModeType4(&copy, TRUE);
    assert(copy.gdl - out <= 16);
    return copy.gdl - out;
}

static int check_stream(Gfx *input, int size, int type, int zbuffer)
{
    ModelRenderData data = prop();
    Gfx initial[16];
    Gfx *output, *saved, *reference, *combined;
    Snapshot *before, *after;
    int ninitial, bytes, nb, na, i, j, converted = 0;
    data.zbufferenabled = zbuffer;
    ninitial = setup_material(&data, type, initial);
    bytes = gfxBuildOneCycleGdl(input, size, NULL, 0, initial, ninitial * 8);
    if (bytes <= 0) return 0;
    output = malloc(bytes + 8); saved = malloc(size);
    reference = malloc(size + ninitial * 8); combined = malloc(bytes + ninitial * 8);
    before = calloc(size / 8 + 16, sizeof(*before)); after = calloc(bytes / 8 + 16, sizeof(*after));
    assert(output && saved && reference && combined && before && after);
    memcpy(saved, input, size);
    memset((u8 *)output + bytes, 0xa5, 8);
    assert(gfxBuildOneCycleGdl(input, size, output, bytes, initial, ninitial * 8) == bytes);
    assert(!memcmp(saved, input, size));
    for (i = 0; i < 8; i++) assert(((u8 *)output)[bytes + i] == 0xa5);
    for (i = 0; i < ninitial; i++) reference[i] = combined[i] = renderGetAaOffCommand(initial[i]);
    for (i = 0; i < size / 8; i++) reference[ninitial + i] = renderGetAaOffCommand(input[i]);
    memcpy(combined + ninitial, output, bytes);
    /* Preserve every original packet in order, including uploads, vertices,
     * matrices, primitive colours, TRI1/TRI4 and authored surface overrides. */
    for (i = j = 0; i < bytes / 8 && j < size / 8; i++)
        if (!memcmp(output + i, reference + ninitial + j, 8)) j++;
    assert(j == size / 8);
    nb = snapshots(reference, size / 8 + ninitial, before);
    na = snapshots(combined, bytes / 8 + ninitial, after);
    assert(nb == na && nb > 0);
    for (i = 0; i < nb; i++) {
        Snapshot *a = &before[i], *b = &after[i];
        if (!memcmp(a, b, sizeof(*a))) continue;
        assert((a->h & BG_CYCLE_MASK) == G_CYC_2CYCLE);
        assert((b->h & BG_CYCLE_MASK) == G_CYC_1CYCLE);
        assert(!((a->h ^ b->h) & ~(BG_CYCLE_MASK | BG_LOD_MASK)));
        assert(a->texture == b->texture);
        assert((a->l & (Z_CMP | Z_UPD | 7)) == (b->l & (Z_CMP | Z_UPD | 7)));
        assert(!(b->l & (AA_EN | IM_RD | CVG_X_ALPHA | ZMODE_DEC | ZMODE_XLU)));
        if ((a->l & BG_FIRST_BLENDER_MASK) != (G_RM_PASS)) {
            assert((a->l & BG_FIRST_BLENDER_MASK) == (b->l & BG_FIRST_BLENDER_MASK));
            assert(b->l & FORCE_BL);
        }
        converted++;
    }
    /* Secondary draws and the next model inherit precisely the old state. */
    assert(!memcmp(before + nb - 1, after + na - 1, sizeof(*before)));
    assert(converted > 0);
    free(output); free(saved); free(reference); free(combined); free(before); free(after);
    return converted;
}

static void check_cache_and_dispatch(void)
{
    ModelRenderData data = prop();
    Gfx *src = (Gfx *)(g_TestRam + 0x10000), *alt, *oldAlt;
    Gfx *master = (Gfx *)(g_TestRam + 0x5000);
    Gfx *secondary = (Gfx *)(g_TestRam + 0x12000);
    Gfx saved[128];
    union ModelRoData ro = {0};
    union ModelRwData rw = {0};
    ModelNode node = {&ro};
    Model model = {&rw};
    ModelNodeRenderCache cache = {0};
    int bytes, i, aa, vi;
    reset();
    memcpy(src, opaque, sizeof(opaque));
    alt = modelGetOneCycleGdl(&data, src, 4);
    assert(alt != src && allocations == 1 && allocated > 0);
    assert(modelGetOneCycleGdl(&data, src, 3) == alt && allocations == 1);
    data.fogcolour.word = 0xaabbcc11;
    data.envcolour.word = 0x12345600;
    assert(modelGetOneCycleGdl(&data, src, 4) == alt && allocations == 1);
    for (i = 0; i <= 10; i++) {
        data.PropType = i;
        if (i != 9) assert(modelGetOneCycleGdl(&data, src, 4) == src);
    }
    data = prop();
    for (i = 1; i < 256; i++) {
        data.envcolour.word = i;
        assert(modelGetOneCycleGdl(&data, src, 4) == src);
    }
    data.envcolour.word = 0;
    assert(modelGetOneCycleGdl(&data, src, 0) == src);
    assert(modelGetOneCycleGdl(&data, src, 1) == src);
    for (i = 0; i < 2; i++) {
        Gfx *dynamic = (Gfx *)g_GfxBuffers[i];
        memcpy(dynamic, opaque, sizeof(opaque));
        assert(modelGetOneCycleGdl(&data, dynamic, 4) == dynamic);
        dynamic = (Gfx *)g_VtxBuffers[i];
        memcpy(dynamic, opaque, sizeof(opaque));
        assert(modelGetOneCycleGdl(&data, dynamic, 4) == dynamic);
    }
    for (aa = 0; aa < 2; aa++) for (vi = 0; vi < 2; vi++) {
        renderSetAaEnabled(aa); renderSetViFilterEnabled(vi); renderApplySettings();
        assert(modelGetOneCycleGdl(&data, src, 4) == (aa ? src : alt));
    }
    renderSetAaEnabled(FALSE);
    assert(modelGetOneCycleGdl(&data, src, 4) == src); /* queued On state */
    renderApplySettings();
    assert(modelGetOneCycleGdl(&data, src, 4) == alt);
    assert(allocations == 1);

    /* Both node formats select the alternate only for primary geometry. */
    gSPEndDisplayList(secondary);
    ro.DisplayList.Primary = src; ro.DisplayList.Secondary = secondary;
    ro.DisplayList.BaseAddr = g_TestRam + 0x14000;
    ro.DisplayList.ModelType = 3;
    data.gdl = master;
    modelRenderNodeGundl(&data, &node);
    i = 0;
    for (Gfx *g = master; g < data.gdl; g++) if (g->words.w0 >> 24 == (u8)G_DL) {
        assert(g->words.w1 == K0_TO_PHYS(i++ ? secondary : alt));
    }
    assert(i == 2);
    assert(renderApplyDisplayListSettings(master, data.gdl));
    rw.DisplayListCollisions.gdl = src;
    rw.DisplayListCollisions.Vertices = g_TestRam + 0x15000;
    data.gdl = master;
    modelRenderNodeDlWithCache(&data, &model, &node, &cache);
    i = 0;
    for (Gfx *g = master; g < data.gdl; g++) if (g->words.w0 >> 24 == (u8)G_DL) {
        assert(g->words.w1 == K0_TO_PHYS(i++ ? secondary : alt));
    }
    assert(i == 2 && !cache.type3PipelineReady);
    ro.DisplayList.ModelType = 4; data.flags = 2; data.gdl = master;
    modelRenderNodeDlWithCache(&data, &model, &node, &cache);
    assert(data.gdl[-1].words.w1 == K0_TO_PHYS(secondary));
    data.flags = 1;

    bytes = allocated;
    assert(bytes <= sizeof(saved));
    memcpy(saved, alt, bytes); oldAlt = alt;
    modelOneCycleInvalidateGdlRange(src + 1, src + 2);
    src[2].words.w1 = G_TF_POINT;
    alt = modelGetOneCycleGdl(&data, src, 4);
    assert(alt != src && alt != oldAlt && allocations == 2 && !frees);
    assert(!memcmp(saved, oldAlt, bytes)); /* queued old copy remains intact */
    modelOneCycleInvalidateGdlRange(secondary, secondary + 1);
    assert(modelGetOneCycleGdl(&data, src, 4) == alt && allocations == 2);

    reset(); failAllocation = TRUE;
    assert(modelGetOneCycleGdl(&data, src, 4) == src && allocations == 1);
    assert(modelGetOneCycleGdl(&data, src, 4) == src && allocations == 1);
    reset(); g_ModelOneCycleBytes = MODEL_ONE_CYCLE_BYTE_LIMIT;
    assert(modelGetOneCycleGdl(&data, src, 4) == src && allocations == 0);
    reset();
    src[0].words.w0 = 0x06000000;
    assert(modelGetOneCycleGdl(&data, src, 4) == src && allocations == 0);
    memcpy(src, opaque, sizeof(opaque));
    modelOneCycleInvalidateGdlRange(src, src + sizeof(opaque) / 8);
    assert(modelGetOneCycleGdl(&data, src, 4) != src);
    reset();
    for (i = 0; i < MODEL_ONE_CYCLE_CACHE_SIZE; i++) {
        Gfx *empty = (Gfx *)(g_TestRam + 0x20000) + i;
        gSPEndDisplayList(empty);
        assert(modelGetOneCycleGdl(&data, empty, 4) == empty);
    }
    assert(modelGetOneCycleGdl(&data, src, 4) == src && allocations == 0);
    puts("Cache/draw dispatch: colours, AA/VI gating, secondary/dynamic exclusions, reload retention, memory/table limits pass.");
}

static u32 read_be(FILE *file)
{
    u8 b[4]; assert(fread(b, 1, 4, file) == 4);
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        for (int type = 2; type <= 4; type++) for (int z = 0; z < 2; z++) {
            assert(check_stream((Gfx *)opaque, sizeof(opaque), type, z) == 1);
            assert(check_stream((Gfx *)mixed, sizeof(mixed), type, z) == 2);
        }
        check_cache_and_dispatch();
        puts("Model states: TRI1/TRI4, opaque fog lighting, depth, decal/cutout/translucent fallback and outgoing state pass.");
    } else {
        FILE *file = fopen(argv[1], "rb");
        int lists = 0, converted = 0, crateLists = 0, cratePackets = 0, trainCrates = 0;
        assert(file);
        while (fgetc(file) != EOF) {
            int length, type, count, gained, nested = 0;
            char name[128];
            Gfx *gdl;
            fseek(file, -1, SEEK_CUR);
            length = read_be(file); type = read_be(file); count = read_be(file);
            assert(length < sizeof(name) && count > 0 && count < 16384);
            assert(fread(name, 1, length, file) == length); name[length] = 0;
            gdl = malloc(count * 8); assert(gdl);
            for (int i = 0; i < count; i++) {
                gdl[i].words.w0 = read_be(file); gdl[i].words.w1 = read_be(file);
                if (gdl[i].words.w0 >> 24 == (u8)G_DL) nested = 1;
            }
            gained = nested ? 0 : check_stream(gdl, count * 8, type, TRUE);
            converted += gained;
            if (strstr(name, "crate")) { crateLists++; cratePackets += gained; }
            if (!strcmp(name, "wood_sm_crate4") || !strcmp(name, "metal_crate3")) {
                assert(gained > 0);
                printf("Train %s: %d opaque triangle packets converted.\n", name, gained);
                trainCrates++;
            }
            free(gdl); lists++;
        }
        fclose(file);
        assert(lists > 100 && converted > 100 && crateLists > 10 && cratePackets > 20 && trainCrates == 2);
        printf("Prop assets: %d primary lists checked; %d triangle packets converted, including %d packets across %d crate lists.\n",
                lists, converted, cratePackets, crateLists);
    }
    return 0;
}
