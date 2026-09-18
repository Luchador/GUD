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
    renderCacheReset();
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

static int check_stream_setup(Gfx *input, int size, int type, ModelRenderData data, int damage)
{
    Gfx initial[16];
    Gfx *output, *saved, *reference, *combined;
    Snapshot *before, *after;
    int ninitial, bytes, nb, na, i, j, converted = 0;
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
        assert((a->l & (Z_CMP | Z_UPD | 4)) == (b->l & (Z_CMP | Z_UPD | 4)));
        assert(a->env == b->env && a->blend == b->blend);
        if (damage) {
            assert((b->l & 3) == G_AC_THRESHOLD && (b->blend & 255) == 128);
            assert(!(b->l & ALPHA_CVG_SEL));
        } else assert((a->l & 3) == (b->l & 3));
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

static int check_stream_material(Gfx *input, int size, int type, int zbuffer, int damage)
{
    ModelRenderData data = prop();
    data.zbufferenabled = zbuffer;
    data.envcolour.word = damage;
    return check_stream_setup(input, size, type, data, damage);
}

static int check_stream(Gfx *input, int size, int type, int zbuffer)
{
    return check_stream_material(input, size, type, zbuffer, 0);
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
    alt = modelGetOneCycleGdl(&data, src, 4, NULL);
    assert(alt != src && allocations == 1 && allocated > 0);
    assert(modelGetOneCycleGdl(&data, src, 3, NULL) == alt && allocations == 1);
    data.fogcolour.word = 0xaabbcc11;
    data.envcolour.word = 0x12345600;
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == alt && allocations == 1);
    for (i = 0; i <= 10; i++) {
        data.PropType = i;
        if (i != 9) assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src);
    }
    data = prop();
    for (i = 1; i < 128; i++) {
        data.envcolour.word = i;
        assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src);
    }
    data.envcolour.word = 0;
    assert(modelGetOneCycleGdl(&data, src, 0, NULL) == src);
    assert(modelGetOneCycleGdl(&data, src, 1, NULL) == src);
    for (i = 0; i < 2; i++) {
        Gfx *dynamic = (Gfx *)g_GfxBuffers[i];
        memcpy(dynamic, opaque, sizeof(opaque));
        assert(modelGetOneCycleGdl(&data, dynamic, 4, NULL) == dynamic);
        dynamic = (Gfx *)g_VtxBuffers[i];
        memcpy(dynamic, opaque, sizeof(opaque));
        assert(modelGetOneCycleGdl(&data, dynamic, 4, NULL) == dynamic);
    }
    for (aa = 0; aa < 2; aa++) for (vi = 0; vi < 2; vi++) {
        renderSetAaEnabled(aa); renderSetViFilterEnabled(vi); renderApplySettings();
        assert(modelGetOneCycleGdl(&data, src, 4, NULL) == (aa ? src : alt));
    }
    renderSetAaEnabled(FALSE);
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src); /* queued On state */
    renderApplySettings();
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == alt);
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
    /* Destroyed console screens clear only the instance's list. Neither
     * primary nor secondary geometry may fall back to the shared source. */
    rw.DisplayListCollisions.gdl = NULL;
    for (int type = 3; type <= 4; type++) for (int pass = 1; pass <= 3; pass++) {
        ro.DisplayList.ModelType = type; data.flags = pass; data.gdl = master;
        modelRenderNodeDlWithCache(&data, &model, &node, &cache);
        assert(data.gdl == master);
    }
    assert(ro.DisplayList.Primary == src && ro.DisplayList.Secondary == secondary);
    rw.DisplayListCollisions.gdl = src;
    data.flags = 1;

    /* Destruction filters native translucent passes in both node formats.
     * A fading prop requests both passes together; its primary shell must
     * still draw. Restoring the flags also restores a live shared instance. */
    for (int format = 0; format < 2; format++) for (int type = 3; type <= 4; type++)
    for (int pass = 1; pass <= 3; pass++) for (int destroyed = 0; destroyed < 2; destroyed++) {
        int primaryCalls = 0, secondaryCalls = 0;
        ro.DisplayList.ModelType = type;
        cache = (ModelNodeRenderCache){0}; data.gdl = master;
        data.flags = destroyed ? (pass & ~2u) | MODEL_RENDER_HIDE_TRANSLUCENT : (u32)pass;
        if (format) modelRenderNodeDlWithCache(&data, &model, &node, &cache);
        else modelRenderNodeGundl(&data, &node);
        for (Gfx *g = master; g < data.gdl; g++) if (g->words.w0 >> 24 == (u8)G_DL) {
            if (g->words.w1 == K0_TO_PHYS(secondary)) secondaryCalls++;
            else { assert(g->words.w1 == K0_TO_PHYS(alt)); primaryCalls++; }
        }
        assert(primaryCalls == !!(pass & 1));
        assert(secondaryCalls == (!destroyed && !!(pass & (type == 3 ? 1 : 2))));
        assert(!memcmp(src, opaque, sizeof(opaque)));
        if (format && type == 3 && (pass & 1)) assert(cache.type3PipelineReady == destroyed);
    }
    data.flags = 1;

    bytes = allocated - RENDER_CACHE_HEADER_SIZE;
    assert(bytes <= sizeof(saved));
    memcpy(saved, alt, bytes); oldAlt = alt;
    modelOneCycleInvalidateGdlRange(src + 1, src + 2);
    src[2].words.w1 = G_TF_POINT;
    alt = modelGetOneCycleGdl(&data, src, 4, NULL);
    assert(alt != src && alt != oldAlt && allocations == 2 && !frees);
    assert(!memcmp(saved, oldAlt, bytes)); /* queued old copy remains intact */
    modelOneCycleInvalidateGdlRange(secondary, secondary + 1);
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == alt && allocations == 2);

    reset(); failAllocation = TRUE;
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src && allocations == 1);
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src && allocations == 1);
    reset(); g_ModelOneCycleBytes = MODEL_ONE_CYCLE_BYTE_LIMIT;
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src && allocations == 0);
    reset();
    src[0].words.w0 = 0x06000000;
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src && allocations == 0);
    memcpy(src, opaque, sizeof(opaque));
    modelOneCycleInvalidateGdlRange(src, src + sizeof(opaque) / 8);
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) != src);
    reset();
    for (i = 0; i < MODEL_ONE_CYCLE_CACHE_SIZE; i++) {
        Gfx *empty = (Gfx *)(g_TestRam + 0x20000) + i;
        gSPEndDisplayList(empty);
        assert(modelGetOneCycleGdl(&data, empty, 4, NULL) == empty);
    }
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src && allocations == 0);
    puts("Cache/draw dispatch: colours, AA/VI gating, secondary/dynamic exclusions, reload retention, memory/table limits pass.");
    puts("Destroyed model passes: type-3/type-4, both node formats, fading shell and shared intact instance pass.");
}

/* Independent alpha-mux evaluation. The RDP carries nine-bit intermediates:
 * 0x100..0x17f clamp high, 0x180..0x1ff clamp low. A floating-point saturating
 * sum would fill the console's holes and would miss a broken conversion. */
static int damage_extend(int value)
{
    return (value & 0x180) == 0x180 ? value - 512 : value;
}
static int damage_clamp(int value)
{
    return value < 256 ? value : value < 384 ? 255 : 0;
}
static int damage_input(int mux, int combined, int shade, int env)
{
    switch (mux) {
    case 0: return combined;
    case 4: return shade;
    case 5: return env;
    case 6: return 256;
    case 7: return 0;
    default: assert(0); return 0;
    }
}
static int damage_alpha(Gfx combine, int cycle, int combined, int shade, int env)
{
    u32 w0 = combine.words.w0, w1 = combine.words.w1;
    int a = cycle ? (w1 >> 21) & 7 : (w0 >> 12) & 7;
    int b = cycle ? (w1 >> 3) & 7 : (w1 >> 12) & 7;
    int c = cycle ? (w1 >> 18) & 7 : (w0 >> 9) & 7;
    int d = cycle ? w1 & 7 : (w1 >> 9) & 7;
    /* The supported alpha equations multiply by shade or zero only. */
    assert(c == 4 || c == 7);
    a = damage_extend(damage_input(a, combined, shade, env));
    b = damage_extend(damage_input(b, combined, shade, env));
    c = damage_input(c, combined, shade, env);
    d = damage_extend(damage_input(d, combined, shade, env));
    return ((((a - b) * c + d * 256 + 128) >> 8) & 511);
}

static void check_damage(void)
{
    const int levels[] = {150, 200, 250, 255};
    ModelRenderData data = prop();
    Gfx *src = (Gfx *)(g_TestRam + 0x10000), *intact, *damaged, *other;
    Gfx setup[16], result[128], combined[144], saved[128];
    Snapshot states[128];
    Gfx original, converted;
    int nsetup, size, nstates, bytes, damagedBytes, stage, type, z, i;
    reset(); memcpy(src, opaque, sizeof(opaque));
    intact = modelGetOneCycleGdl(&data, src, 4, NULL); assert(intact != src);
    bytes = g_ModelOneCycleBytes;
    data.envcolour.word = 150;
    damaged = modelGetOneCycleGdl(&data, src, 4, NULL);
    assert(damaged != src && damaged != intact && allocations == 2);
    damagedBytes = g_ModelOneCycleBytes - bytes;
    assert(bytes + damagedBytes <= sizeof(saved));
    memcpy(saved, intact, bytes);
    memcpy((u8 *)saved + bytes, damaged, damagedBytes);
    for (stage = 0; stage < 4; stage++) {
        data.envcolour.word = levels[stage]; data.fogcolour.word = 0x1745ab80 + stage;
        assert(modelGetOneCycleGdl(&data, src, 3, NULL) == damaged && allocations == 2);
        assert(!memcmp(saved, intact, bytes)); /* No per-instance mutation. */
        assert(!memcmp((u8 *)saved + bytes, damaged, damagedBytes));
        for (type = 3; type <= 4; type++) for (z = 0; z < 2; z++)
            assert(check_stream_material(src, sizeof(opaque), type, z, levels[stage]) == 1);
    }
    data.envcolour.word = 150;
    data.zbufferenabled = FALSE; other = modelGetOneCycleGdl(&data, src, 4, NULL);
    assert(other != damaged && other != src);
    data.zbufferenabled = TRUE;
    assert(modelGetOneCycleGdl(&data, src, 2, NULL) == src);
    data.PropType = 5; assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src); data.PropType = 9;
    for (i = 0; i < 2; i++) {
        renderSetViFilterEnabled(i); renderApplySettings();
        assert(modelGetOneCycleGdl(&data, src, 4, NULL) == damaged);
    }
    renderSetAaEnabled(TRUE);
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == damaged); /* Change is still queued. */
    renderApplySettings(); assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src);
    nsetup = setup_material(&data, 4, setup);
    for (i = 0; i < nsetup; i++) assert(setup[i].words.w0 >> 24 != (u8)G_SETBLENDCOLOR);
    renderSetAaEnabled(FALSE); renderApplySettings();
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == damaged);

    nsetup = setup_material(&data, 4, setup);
    size = gfxBuildOneCycleGdl(src, sizeof(opaque), result, sizeof(result), setup, nsetup * 8);
    assert(size > 0);
    memcpy(combined, setup, nsetup * 8); memcpy(combined + nsetup, result, size);
    nstates = snapshots(combined, nsetup + size / 8, states); assert(nstates == 2);
    original.words.w0 = states[1].c0; original.words.w1 = states[1].c1;
    converted.words.w0 = states[0].c0; converted.words.w1 = states[0].c1;
    /* Decode the actual emitted RGB mux too: TEXEL0 * SHADE, no TEXEL1. */
    assert(((converted.words.w0 >> 5) & 15) == G_CCMUX_TEXEL0);
    assert(((converted.words.w1 >> 24) & 15) == (G_CCMUX_0 & 15));
    assert((converted.words.w0 & 31) == G_CCMUX_SHADE);
    assert(((converted.words.w1 >> 6) & 7) == (G_CCMUX_0 & 7));
    for (int env = 128; env <= 255; env++) for (int shade = 0; shade <= 255; shade++) {
        int first = damage_alpha(original, 0, 0, shade, env);
        int old = damage_clamp(damage_alpha(original, 1, first, shade, env));
        int now = damage_clamp(damage_alpha(converted, 1, 0, shade, env));
        assert(old == now && (!old || old >= 128));
        assert((old != 0) == (now >= 128));
    }
    /* An internal secondary/glass equation or render mode must not become a
     * damage cutout. Each rejected draw and the final state are restored. */
    {
        Gfx mixedDamage[64]; Gfx *g = mixedDamage;
        for (i = 0; i < (int)sizeof(opaque) / 8 - 1; i++) *g++ = opaque[i];
        gDPPipeSync(g++); gDPSetRenderMode(g++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_XLU_SURF2);
        gSP1Triangle(g++, 0, 1, 2, 0);
        gDPSetRenderMode(g++, G_RM_FOG_PRIM_A, G_RM_AA_ZB_TEX_EDGE2);
        gDPSetCombineLERP(g++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, SHADE, ENVIRONMENT, TEXEL0, 0,
                COMBINED, 0, SHADE, 0, 1, 0, PRIMITIVE, COMBINED);
        gSP1Triangle(g++, 0, 1, 2, 0); gSPEndDisplayList(g++);
        assert(check_stream_material(mixedDamage, (g - mixedDamage) * 8, 4, TRUE, 150) == 1);
    }
    /* Explicit alpha tests, unsuitable thresholds, room lists and custom
     * partial overrides are not enough to qualify as the damage material. */
    for (i = 0; i < 5; i++) {
        Gfx initial[20]; int n = setup_material(&data, 4, initial);
        if (i == 0) { gDPSetBlendColor(&initial[n], 0, 0, 0, 64); n++; }
        if (i == 1) { gDPSetAlphaCompare(&initial[n], G_AC_DITHER); n++; }
        if (i == 2) { gDPSetEnvColor(&initial[n], 0, 0, 0, 127); n++; }
        if (i == 3) { gDPSetRenderMode(&initial[n], G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_DECAL2); n++; }
        if (i == 4) { gDPSetCombineMode(&initial[n], G_CC_TRILERP, G_CC_MODULATEIFADEA); n++; }
        assert(gfxBuildOneCycleGdl(src, sizeof(opaque), NULL, 0, initial, n * 8) == 0);
    }
    memcpy(combined, setup, nsetup * 8); memcpy(combined + nsetup, src, sizeof(opaque));
    assert(bgBuildOneCycleGdl(combined, nsetup * 8 + sizeof(opaque), NULL, 0) == 0);
    modelOneCycleInvalidateGdlRange(src, src + 1);
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) != damaged && !frees);
    assert(!memcmp(saved, intact, bytes));
    reset(); failAllocation = TRUE;
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src); reset();
    puts("Damaged props: exact nine-bit alpha/hole mask, RGB mux, fog/depth, all damage stages, fallback, instance/cache isolation and AA/VI switching pass.");
}

static void check_first_person(void)
{
    ModelRenderData data = prop();
    void *base = g_TestRam + 0x20000, *otherBase = g_TestRam + 0x30000;
    Gfx *segmented = (Gfx *)(uintptr_t)0x05001000;
    Gfx *src = (Gfx *)((u8 *)base + 0x1000), *otherSrc = (Gfx *)((u8 *)otherBase + 0x1000);
    Gfx *secondary = (Gfx *)(uintptr_t)0x05002000;
    Gfx *master = (Gfx *)(g_TestRam + 0x5000), setup[16], saved[128];
    Gfx *alt, *other, *world, *damaged;
    union ModelRoData ro = {0}; union ModelRwData rw = {0};
    ModelNode node = {&ro}; Model model = {&rw}; ModelNodeRenderCache cache = {0};
    int bytes, n, i, calls, count;
    reset(); memcpy(src, opaque, sizeof(opaque)); memcpy(otherSrc, opaque, sizeof(opaque));
    data.zbufferenabled = FALSE;
    /* The same segmented address must resolve separately in each asset. */
    world = modelGetOneCycleGdl(&data, segmented, 3, base);
    assert(world != segmented);
    data.envcolour.word = 150;
    damaged = modelGetOneCycleGdl(&data, segmented, 3, base);
    assert(damaged != segmented && damaged != world);
    data.PropType = PROP_TYPE_WEAPON;
    data.envcolour.word = 0x31415980;
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) == segmented); /* watch/casings */
    data.flags |= MODEL_RENDER_FIRST_PERSON;
    alt = modelGetOneCycleGdl(&data, segmented, 3, base);
    assert(alt != segmented && alt != world && alt != damaged);
    assert(modelGetOneCycleGdl(&data, src, 4, base) == alt); /* RAM alias, same setup */
    other = modelGetOneCycleGdl(&data, segmented, 3, otherBase);
    assert(other != segmented && other != alt);
    count = allocations; bytes = 0;
    while (alt[bytes++].words.w0 >> 24 != (u8)G_ENDDL) assert(bytes < 128);
    memcpy(saved, alt, bytes * 8);

    for (i = 0; i < 256; i++) {
        data.envcolour.word = 0x31415900 | i; /* room-light blend, not damage alpha */
        assert(modelGetOneCycleGdl(&data, segmented, 3, base) == alt && allocations == count);
        assert(!memcmp(saved, alt, bytes * 8));
        n = setup_material(&data, 3, setup);
        calls = 0;
        for (int j = 0; j < n; j++) if (setup[j].words.w0 >> 24 == (u8)G_SETFOGCOLOR) {
            assert(setup[j].words.w1 == data.envcolour.word); calls++;
        }
        assert(calls == 1);
    }
    for (i = 0; i < bytes; i++) assert(alt[i].words.w0 >> 24 != (u8)G_SETFOGCOLOR);
    for (int type = 2; type <= 4; type++) for (int z = 0; z < 2; z++) {
        data.zbufferenabled = z;
        assert(check_stream_setup(src, sizeof(opaque), type, data, 0) == 1);
        assert(check_stream_setup((Gfx *)mixed, sizeof(mixed), type, data, 0) == 2);
    }
    data.zbufferenabled = FALSE;
    /* Both production node formats must submit the converted primary and the
     * original secondary, with the segment base still available to the RSP. */
    gSPEndDisplayList((Gfx *)((u8 *)base + 0x2000));
    ro.DisplayList.Primary = segmented; ro.DisplayList.Secondary = secondary;
    ro.DisplayList.BaseAddr = base; ro.DisplayList.ModelType = 3;
    rw.DisplayListCollisions.gdl = segmented; rw.DisplayListCollisions.Vertices = (u8 *)base + 0x3000;
    for (int format = 0; format < 2; format++) {
        data.gdl = master;
        if (format) modelRenderNodeDlWithCache(&data, &model, &node, &cache);
        else modelRenderNodeGundl(&data, &node);
        calls = 0;
        for (Gfx *g = master; g < data.gdl; g++) if (g->words.w0 >> 24 == (u8)G_DL)
            assert(g->words.w1 == (calls++ ? 0x05002000 : K0_TO_PHYS(alt)));
        assert(calls == 2 && renderApplyDisplayListSettings(master, data.gdl));
    }
    data.PropType = PROP_TYPE_PLAYER; /* faded/custom material remains excluded */
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) == segmented);
    data.PropType = PROP_TYPE_WEAPON;
    assert(modelGetOneCycleGdl(&data, segmented, 0, base) == segmented);
    assert(modelGetOneCycleGdl(&data, segmented, 3, NULL) == segmented);
    assert(modelGetOneCycleGdl(&data, segmented, 3, g_TestRam + osMemSize - 8) == segmented);
    assert(modelGetOneCycleGdl(&data, (Gfx *)(uintptr_t)0x06001000, 3, base) == (Gfx *)(uintptr_t)0x06001000);
    assert(modelGetOneCycleGdl(&data, (Gfx *)(uintptr_t)0x05000000, 3, g_GfxBuffers[0]) == (Gfx *)(uintptr_t)0x05000000);
    renderSetViFilterEnabled(FALSE); renderApplySettings();
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) == alt);
    renderSetAaEnabled(TRUE);
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) == alt); /* queued */
    renderApplySettings();
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) == segmented);
    renderSetAaEnabled(FALSE); renderApplySettings();
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) == alt);

    /* Texture expansion invalidates RAM ranges on a weapon-buffer reload.
     * Submitted old copies survive; a replacement gets a new conversion. */
    modelOneCycleInvalidateGdlRange(src, src + sizeof(opaque) / 8);
    src[2].words.w1 = G_TF_POINT;
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) != alt && !frees);
    assert(!memcmp(saved, alt, bytes * 8));
    assert(modelGetOneCycleGdl(&data, segmented, 3, otherBase) == other);
    reset(); failAllocation = TRUE;
    assert(modelGetOneCycleGdl(&data, segmented, 3, base) == segmented);
    reset();
    puts("First-person weapons: native segmented addresses, lighting, mixed materials, draw dispatch, AA/VI, instance isolation and reload/failure fallback pass.");
}

static void check_pressure_reclaim(void)
{
    ModelRenderData data = prop();
    Gfx *src = (Gfx *)(g_TestRam + 0x10000), *old, *replacement;
    Gfx saved[128];
    int bytes;
    reset(); memcpy(src, opaque, sizeof(opaque));
    old = modelGetOneCycleGdl(&data, src, 4, NULL);
    assert(old != src);
    bytes = g_ModelOneCycleBytes;
    assert(bytes <= sizeof(saved)); memcpy(saved, old, bytes);
    modelOneCycleInvalidateGdlRange(src, src + sizeof(opaque) / 8);
    src[2].words.w1 = G_TF_POINT;
    replacement = modelGetOneCycleGdl(&data, src, 4, NULL);
    assert(replacement != src && replacement != old && allocations == 2);
    renderCacheRequestReclaim();
    assert(renderCacheReclaimPending() && !frees);
    assert(!memcmp(saved, old, bytes));
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src);
    assert(allocations == 2 && !frees);
    /* Simulate boss's safe point after every queued graphics task completes. */
    renderCacheReclaim();
    assert(frees == 2 && allocated == 0 && g_ModelOneCycleBytes == 0);
    assert(!renderCacheReclaimPending() && !renderCacheIsEnabled());
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) == src && allocations == 2);
    renderCacheReclaim(); assert(frees == 2);
    reset();
    assert(modelGetOneCycleGdl(&data, src, 4, NULL) != src);
    puts("Memory pressure: live and retired model copies survive until drain, then reclaim completely; native fallback and stage reset pass.");
}

static u32 read_be(FILE *file)
{
    u8 b[4]; assert(fread(b, 1, 4, file) == 4);
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        reset();
        for (int type = 2; type <= 4; type++) for (int z = 0; z < 2; z++) {
            assert(check_stream((Gfx *)opaque, sizeof(opaque), type, z) == 1);
            assert(check_stream((Gfx *)mixed, sizeof(mixed), type, z) == 2);
        }
        check_cache_and_dispatch();
        check_damage();
        check_first_person();
        check_pressure_reclaim();
        check_character_materials();
        puts("Model states: TRI1/TRI4, opaque fog lighting, depth, decal/cutout/translucent fallback and outgoing state pass.");
    } else {
        FILE *file = fopen(argv[1], "rb");
        int lists = 0, converted = 0, crateLists = 0, cratePackets = 0, trainCrates = 0;
        int consoleBodies = 0, weaponLists = 0, weaponPackets = 0;
        reset();
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
            if (!strncmp(name, "gun/", 4) || !strncmp(name, "chr/", 4)) {
                ModelRenderData data = prop();
                data.PropType = PROP_TYPE_WEAPON; data.flags |= MODEL_RENDER_FIRST_PERSON;
                data.zbufferenabled = FALSE; data.envcolour.word = 0x31415980;
                gained = nested ? 0 : check_stream_setup(gdl, count * 8, type, data, 0);
                weaponLists++; weaponPackets += gained;
                free(gdl); continue;
            }
            gained = nested ? 0 : check_stream(gdl, count * 8, type, TRUE);
            converted += gained;
            if ((!strcmp(name, "console2") || !strcmp(name, "console3")) && count > 20) {
                for (int level = 150; level <= 250; level += 50)
                    assert(check_stream_material(gdl, count * 8, type, TRUE, level) == 20);
                assert(check_stream_material(gdl, count * 8, type, TRUE, 255) == 20);
                printf("%s: all 20 body triangle packets convert at every damage stage.\n", name);
                consoleBodies++;
            }
            if (strstr(name, "crate")) { crateLists++; cratePackets += gained; }
            if (!strcmp(name, "wood_sm_crate4") || !strcmp(name, "metal_crate3")) {
                assert(gained > 0);
                printf("Train %s: %d opaque triangle packets converted.\n", name, gained);
                trainCrates++;
            }
            free(gdl); lists++;
        }
        fclose(file);
        assert(lists > 100 && converted > 100 && crateLists > 10 && cratePackets > 20 && trainCrates == 2 && consoleBodies == 2);
        printf("Prop assets: %d primary lists checked; %d triangle packets converted, including %d packets across %d crate lists.\n",
                lists, converted, cratePackets, crateLists);
        assert(weaponLists > 400 && weaponPackets > 1000);
        printf("Weapon/hand assets: %d primary lists checked; %d triangle packets converted.\n", weaponLists, weaponPackets);
    }
    return 0;
}
