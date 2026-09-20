/* Exercise the production dispatch/proof/cache, then independently decode
 * every draw. Texture uploads are retained but pixels are not rasterized. */
static u32 read_be(FILE *file);

static const Gfx characterOpaque[] = {
    gsDPSetTextureLOD(G_TL_LOD), gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureFilter(G_TF_BILERP), gsSPTexture(0xffff, 0xffff, 3, 0, 1),
    gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK),
    {{0x04200030, 0x04000000}}, {{0xbf000000, 0x00000a14}},
    gsSPEndDisplayList()
};

static ModelRenderData character_data(void)
{
    ModelRenderData data = prop();
    data.PropType = 7; data.flags |= MODEL_RENDER_CHARACTER;
    data.envcolour.word = 0x5a000000;
    return data;
}

static int character_rgb_input(int mux, int combined, int texel, int shade, int env, int c)
{
    if (c && mux == 11) return 255; /* proved full vertex alpha */
    if (mux == 0) return combined;
    if (mux == 1) return texel;
    if (mux == 4) return shade;
    if (mux == 5) return env;
    if (!c && mux == 6) return 256;
    assert(mux == (c ? 31 : 7) || (!c && mux == 15));
    return 0;
}

static int character_rgb_cycle(Gfx g, int cycle, int combined, int texel, int shade, int env)
{
    int a = cycle ? (g.words.w0 >> 5) & 15 : (g.words.w0 >> 20) & 15;
    int b = cycle ? (g.words.w1 >> 24) & 15 : (g.words.w1 >> 28) & 15;
    int c = cycle ? g.words.w0 & 31 : (g.words.w0 >> 15) & 31;
    int d = cycle ? (g.words.w1 >> 6) & 7 : (g.words.w1 >> 15) & 7;
    a = damage_extend(character_rgb_input(a, combined, texel, shade, env, 0));
    b = damage_extend(character_rgb_input(b, combined, texel, shade, env, 0));
    c = character_rgb_input(c, combined, texel, shade, env, 1);
    d = damage_extend(character_rgb_input(d, combined, texel, shade, env, 0));
    return (((a - b) * c + d * 256 + 128) >> 8) & 511;
}

static void character_rgb_precision(Gfx native, Gfx fast, int untextured)
{
    /* The removed blood stage multiplies by 255/256, not exact fixed-point
     * one. Quantify the precision tradeoff rather than asserting bit identity. */
    for (int tex = 0; tex < (untextured ? 1 : 256); tex++)
        for (int shade = 0; shade < 256; shade++) for (int env = 0; env < 256; env++) {
            int first = character_rgb_cycle(native, 0, 0, tex, shade, env);
            int original = damage_clamp(character_rgb_cycle(native, 1, first, tex, shade, env));
            int optimized = damage_clamp(character_rgb_cycle(fast, 1, 0, tex, shade, env));
            assert(abs(original - optimized) <= 1);
        }
}

static int character_compare(Gfx *original, Gfx *alternate, ModelRenderData data, int type)
{
    Gfx initial[16];
    int n = setup_material(&data, type, initial);
    int aBytes = modelOneCycleListSize(original), bBytes = modelOneCycleListSize(alternate);
    Gfx *a = malloc(aBytes + n * 8), *b = malloc(bBytes + n * 8);
    Snapshot *before = calloc(aBytes / 8 + n, sizeof(*before));
    Snapshot *after = calloc(bBytes / 8 + n, sizeof(*after));
    int converted = 0, j = 0;
    assert(a && b && before && after && aBytes && bBytes);
    for (int i = 0; i < n; i++) a[i] = b[i] = renderGetAaOffCommand(initial[i]);
    for (int i = 0; i < aBytes / 8; i++) a[n + i] = renderGetAaOffCommand(original[i]);
    memcpy(b + n, alternate, bBytes);
    for (int i = 0; i < bBytes / 8 && j < aBytes / 8; i++)
        if (!memcmp(alternate + i, a + n + j, 8)) j++;
    assert(j == aBytes / 8); /* every source packet survives in order */
    int na = snapshots(a, aBytes / 8 + n, before), nb = snapshots(b, bBytes / 8 + n, after);
    assert(na == nb);
    for (int i = 0; i < na; i++) {
        Snapshot x = before[i], y = after[i];
        if (!memcmp(&x, &y, sizeof(x))) continue;
        assert((x.h & BG_CYCLE_MASK) == G_CYC_2CYCLE);
        assert((y.h & BG_CYCLE_MASK) == G_CYC_1CYCLE);
        assert((x.h & ~BG_CYCLE_MASK) == (y.h & ~BG_CYCLE_MASK)); /* includes mip LOD */
        assert(x.texture == y.texture && x.env == y.env && x.blend == y.blend);
        assert((x.l & (Z_CMP | Z_UPD | 7)) == (y.l & (Z_CMP | Z_UPD | 7)));
        assert((y.l & BG_FIRST_BLENDER_MASK) == (G_RM_FOG_PRIM_A));
        assert((y.l & (FORCE_BL | ALPHA_CVG_SEL)) == (FORCE_BL | ALPHA_CVG_SEL));
        assert(!(y.l & (AA_EN | IM_RD | CVG_X_ALPHA | ZMODE_DEC | ZMODE_XLU)));
        /* RGB is TEXEL0 * SHADE or just SHADE; opaque alpha is constant one. */
        assert(((y.c0 >> 5) & 15) == 1 || ((y.c0 >> 5) & 15) == 15);
        assert((y.c1 & 7) == 6);
        converted++;
    }
    assert(!memcmp(before + na - 1, after + nb - 1, sizeof(*before)));
    free(a); free(b); free(before); free(after);
    return converted;
}

static void check_character_onecycle(void)
{
    Gfx *src = (Gfx *)(g_TestRam + 0x10000), *master = (Gfx *)(g_TestRam + 0x5000);
    Vertex *vertices = (Vertex *)(g_TestRam + 0x20000), *blood = vertices + 16;
    ModelRenderData data = character_data();
    union ModelRoData ro = {0}; union ModelRwData rw = {0};
    Model model = {&rw}; ModelNode node = {&ro}; ModelNodeRenderCache state = {0};
    Gfx *alt, *old, saved[128];
    reset(); frameBytes = 0;
    memcpy(src, characterOpaque, sizeof(characterOpaque));
    memset(vertices, 255, sizeof(Vertex) * 3);
    ro.DisplayList.Primary = src; ro.DisplayList.ModelType = 3;
    ro.DisplayList.Vertices = vertices; ro.DisplayList.numVertices = 3;
    rw.DisplayListCollisions.gdl = src; rw.DisplayListCollisions.Vertices = vertices;
    alt = modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3);
    assert(alt != src && allocations == 1);
    assert(character_compare(src, alt, data, 3) == 1);
    {
        Gfx initial[16], stream[128], native = {{0}};
        Snapshot draws[16];
        int n = setup_material(&data, 3, initial);
        for (int i = 0; i < n; i++) if (initial[i].words.w0 >> 24 == 0xfc) native = initial[i];
        memcpy(stream, initial, n * 8); memcpy(stream + n, alt, modelOneCycleListSize(alt));
        assert(snapshots(stream, n + modelOneCycleListSize(alt) / 8, draws) == 2);
        Gfx fast = {{draws[0].c0, draws[0].c1}};
        character_rgb_precision(native, fast, 0);
    }
    memcpy(saved, alt, modelOneCycleListSize(alt));
    for (int type = 3; type <= 4; type++) for (int fog = 0; fog < 256; fog++) {
        data.fogcolour.word = 0xabcdef00 | fog;
        data.envcolour.word = 0x12345600;
        assert(modelGetUnbloodiedGdl(&data, src, type, NULL, vertices, 3) == alt);
        assert(character_compare(src, alt, data, type) == 1);
    }
    assert(allocations == 1 && !memcmp(saved, alt, modelOneCycleListSize(alt)));
    data.gdl = master;
    modelRenderNodeDlWithCache(&data, &model, &node, &state);
    assert(data.gdl[-1].words.w1 == K0_TO_PHYS(alt) && state.type3PipelineReady);
    /* A blood buffer must bypass even an already cached alternate. */
    memcpy(blood, vertices, sizeof(Vertex) * 3); blood[0].a = 30;
    rw.DisplayListCollisions.Vertices = blood; data.gdl = master;
    modelRenderNodeDlWithCache(&data, &model, &node, &state);
    assert(data.gdl[-1].words.w1 == K0_TO_PHYS(src));
    rw.DisplayListCollisions.Vertices = vertices;
    /* Secondary lists keep the original two-cycle material. */
    ro.DisplayList.Secondary = src; data.gdl = master; state.type3PipelineReady = FALSE;
    modelRenderNodeDlWithCache(&data, &model, &node, &state);
    assert(data.gdl[-1].words.w1 == K0_TO_PHYS(src) && !state.type3PipelineReady);
    assert(data.gdl[-2].words.w0 == 0xb900031d); /* secondary render-mode setup */
    ro.DisplayList.Secondary = NULL;
    for (int fade = 0; fade < 256; fade++) {
        data.PropType = 8; data.envcolour.word = 0x5a000000 | fade;
        assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3) == src);
    }
    data = character_data(); data.flags &= ~MODEL_RENDER_CHARACTER;
    assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3) == src);
    data = character_data(); renderSetAaEnabled(TRUE); renderApplySettings();
    assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3) == src);
    renderSetAaEnabled(FALSE); renderApplySettings();
    assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, blood, 3) == src);
    assert(modelGetUnbloodiedGdl(&data, src, 3, vertices, vertices, 3) == src);
    assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 2) == src);
    data.zbufferenabled = FALSE;
    Gfx *noZ = modelGetUnbloodiedGdl(&data, src, 4, NULL, vertices, 3);
    assert(noZ != src && noZ != alt && character_compare(src, noZ, data, 4) == 1);
    data.zbufferenabled = TRUE;
    const Gfx unsupported[] = {
        gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2),
        gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2),
        gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_DECAL2),
        gsDPSetTextureDetail(G_TD_DETAIL), gsDPSetTextureDetail(G_TD_SHARPEN),
        gsDPSetAlphaCompare(G_AC_THRESHOLD),
        gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE)
    };
    for (unsigned i = 0; i < sizeof(unsupported) / sizeof(*unsupported); i++) {
        modelOneCycleInvalidateGdlRange(src, src + sizeof(characterOpaque) / 8);
        memcpy(src, characterOpaque, sizeof(characterOpaque)); src[4] = unsupported[i];
        assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3) == src);
    }
    /* Reject unsafe vertex provenance and geometry before attempting conversion. */
    const Gfx bad[] = {
        {{0x04200030, 0x04000010}}, /* load past the declared array */
        {{0x04200030, 0x06000000}}, /* unknown vertex segment */
        {{0x0420002f, 0x04000000}}, /* wrong DMA length */
        {{0x04200030, 0x04000001}}, /* unaligned vertex */
        gsSPSetGeometryMode(G_FOG),
        {{0xbc00000c, 0x01020304}}, /* RSP point modification */
        {{0x06000000, 0x05000000}}, /* nested list */
        {{0xbf000000, 0x00000a14}} /* inherited vertex cache */
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        data.flags |= MODEL_RENDER_CHARACTER;
        modelOneCycleInvalidateGdlRange(src, src + sizeof(characterOpaque) / 8);
        memcpy(src, characterOpaque, sizeof(characterOpaque)); src[5] = bad[i];
        assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3) == src);
        if (i >= 4 && i <= 6) assert(!(data.flags & MODEL_RENDER_CHARACTER));
    }
    data.flags |= MODEL_RENDER_CHARACTER;
    memcpy(src, characterOpaque, sizeof(characterOpaque));
    vertices[1].a = 254;
    modelOneCycleInvalidateGdlRange(src, src + sizeof(characterOpaque) / 8);
    assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3) == src);
    vertices[1].a = 255;
    /* A valid segment-5 vertex load resolves through the model file. */
    src[5].words.w1 = 0x05000000;
    modelOneCycleInvalidateGdlRange(src, src + sizeof(characterOpaque) / 8);
    old = modelGetUnbloodiedGdl(&data, src, 3, vertices, vertices, 3);
    assert(old != src && character_compare(src, old, data, 3) == 1);
    memcpy(saved, old, modelOneCycleListSize(old));
    modelOneCycleInvalidateGdlRange(src, src + sizeof(characterOpaque) / 8);
    failAllocation = TRUE;
    assert(modelGetUnbloodiedGdl(&data, src, 3, vertices, vertices, 3) == src);
    assert(!memcmp(saved, old, modelOneCycleListSize(old)) && !frees);
    renderCacheRequestReclaim(); renderCacheReclaim();
    assert(!allocated && !renderCacheIsEnabled());
    assert(modelGetUnbloodiedGdl(&data, src, 3, vertices, vertices, 3) == src);
    /* Repaired untextured legs optimize and still restore the inherited
     * character equation; the same model continues fading on two cycles. */
    reset(); frameBytes = 0;
    memcpy(src, characterOpaque, 5 * 8);
    memcpy(src + 5, characterEdited, sizeof(characterEdited));
    alt = modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3);
    Gfx *repair = modelGetOneCycleGdl(&data, src, 3, NULL);
    assert(alt != src && alt != repair && character_compare(repair, alt, data, 3) == 4);
    {
        Gfx initial[16], stream[128]; Snapshot before[16], after[16];
        int n = setup_material(&data, 3, initial);
        memcpy(stream, initial, n * 8); memcpy(stream + n, repair, modelOneCycleListSize(repair));
        assert(snapshots(stream, n + modelOneCycleListSize(repair) / 8, before) == 5);
        memcpy(stream + n, alt, modelOneCycleListSize(alt));
        assert(snapshots(stream, n + modelOneCycleListSize(alt) / 8, after) == 5);
        Gfx native = {{before[1].c0, before[1].c1}}, fast = {{after[1].c0, after[1].c1}};
        character_rgb_precision(native, fast, 1);
    }
    data.PropType = 8; data.envcolour.word = 0x5a000080;
    assert(modelGetUnbloodiedGdl(&data, src, 3, NULL, vertices, 3)
            == modelGetOneCycleGdl(&data, src, 3, NULL));
    puts("Character one-cycle: mip/filter/fog/depth preservation, mixed edited materials, untouched/blood/fade dispatch, cache isolation, unsafe-vertex rejection and memory-pressure fallback pass.");
}

static void check_character_assets(const char *path)
{
    FILE *file = fopen(path, "rb");
    int lists = 0, converted = 0, packets = 0, marineLists = 0, marinePackets = 0;
    Gfx *src = (Gfx *)(g_TestRam + 0x60000);
    void *base = g_TestRam + 0x10000;
    assert(file);
    while (fgetc(file) != EOF) {
        fseek(file, -1, SEEK_CUR);
        int length = read_be(file), type = read_be(file), count = read_be(file);
        int offset = read_be(file), bytes = read_be(file);
        char name[128];
        assert(length < sizeof(name) && count * 8 < 0x10000 && offset + bytes < 0x40000);
        assert(bytes > 0 && !(bytes & 15));
        assert(fread(name, 1, length, file) == length); name[length] = 0;
        Vertex *vertices = (Vertex *)((u8 *)base + offset);
        assert(fread(vertices, 1, bytes, file) == bytes);
        for (int i = 0; i < count; i++) {
            src[i].words.w0 = read_be(file); src[i].words.w1 = read_be(file);
        }
        reset(); frameBytes = 0;
        ModelRenderData data = character_data();
        Gfx *alt = modelGetUnbloodiedGdl(&data, src, type, base, vertices, bytes / 16);
        int gain = character_compare(src, alt, data, type);
        lists++; converted += gain > 0; packets += gain;
        if (!strcmp(name, "armourguard")) { marineLists++; marinePackets += gain; assert(gain > 0); }
    }
    fclose(file);
    assert(lists > 1000 && converted > 1000 && marineLists == 25 && marinePackets == 131);
    printf("Character assets: %d/%d body/head lists, %d triangle packets eligible; CarmourguardZ %d/25 lists and all %d triangle packets convert.\n",
            converted, lists, packets, marineLists, marinePackets);
}
