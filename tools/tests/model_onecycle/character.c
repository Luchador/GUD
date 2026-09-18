/* GEditor material edits must retain the renderer's character blood/fade
 * equations. The leg switches below match CarmourguardZ's SHADE -> textured
 * transition; the same file can be shared by live and dying instances. */
static const Gfx characterEdited[] = {
    {{0x04200030, 0x04000000}}, {{0xbf000000, 0x00000a14}},
    gsDPPipeSync(), gsSPTexture(0xffff, 0xffff, 0, 0, 0),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    {{0xbf000000, 0x00000a14}},
    gsDPPipeSync(), gsSPTexture(0xffff, 0xffff, 0, 0, 1),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
    {{0xbf000000, 0x00000a14}},
    gsDPPipeSync(), gsSPTexture(0xffff, 0xffff, 0, 0, 0),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_PASS2),
    {{0xbf000000, 0x00000a14}}, gsSPEndDisplayList()
};

static int character_alpha_input(int mux, int combined, int texel, int shade, int env)
{
    if (mux == 1) return texel;
    return damage_input(mux, combined, shade, env);
}
static int character_alpha_cycle(Gfx g, int cycle, int combined, int texel, int shade, int env)
{
    u32 a = cycle ? (g.words.w1 >> 21) & 7 : (g.words.w0 >> 12) & 7;
    u32 b = cycle ? (g.words.w1 >> 3) & 7 : (g.words.w1 >> 12) & 7;
    u32 c = cycle ? (g.words.w1 >> 18) & 7 : (g.words.w0 >> 9) & 7;
    u32 d = cycle ? g.words.w1 & 7 : (g.words.w1 >> 9) & 7;
    /* Alpha C maps 0 to LOD fraction; none of these equations use it. */
    assert(c != 0);
    int av = damage_extend(character_alpha_input(a, combined, texel, shade, env));
    int bv = damage_extend(character_alpha_input(b, combined, texel, shade, env));
    int cv = character_alpha_input(c, combined, texel, shade, env);
    int dv = damage_extend(character_alpha_input(d, combined, texel, shade, env));
    return (((av - bv) * cv + dv * 256 + 128) >> 8) & 511;
}
static int character_alpha(Gfx g, int texel, int shade, int env)
{
    int first = character_alpha_cycle(g, 0, 0, texel, shade, env);
    return damage_clamp(character_alpha_cycle(g, 1, first, texel, shade, env));
}

static void character_equations(Gfx *list, int bytes, ModelRenderData data)
{
    Gfx initial[16], stream[256], inherited = {{0}}, shade = {{0}};
    Snapshot states[128];
    int n = setup_material(&data, 3, initial), count, fading = data.PropType == 8;
    for (int i = 0; i < n; i++) if (initial[i].words.w0 >> 24 == 0xfc) inherited = initial[i];
    memcpy(stream, initial, n * 8); memcpy(stream + n, list, bytes);
    count = snapshots(stream, n + bytes / 8, states);
    assert(count == 5); /* four triangles, then outgoing state */
    for (int i = 0; i < count; i++) {
        Gfx combine = {{states[i].c0, states[i].c1}};
        assert((states[i].h & BG_CYCLE_MASK) == G_CYC_2CYCLE);
        assert((states[i].l & (Z_CMP | Z_UPD)) == (data.zbufferenabled ? (fading ? Z_CMP : Z_CMP | Z_UPD) : 0));
        if (i == 1 || i == 3) {
            assert(!(states[i].texture & 1));
            shade = combine;
            /* No texture inputs: RGB cycle 0 is (1 - ENV) * shade A + ENV;
             * cycle 1 multiplies by vertex RGB, retaining native blood tint. */
            assert(((combine.words.w0 >> 20) & 15) == 6);
            assert(((combine.words.w1 >> 28) & 15) == 5);
            assert(((combine.words.w0 >> 15) & 31) == 11);
            assert(((combine.words.w1 >> 15) & 7) == 5);
            assert(((combine.words.w0 >> 5) & 15) == 0);
            assert((combine.words.w0 & 31) == 4);
            for (int alpha = 0; alpha < 256; alpha++) for (int blood = 0; blood < 256; blood++) {
                assert(character_alpha(combine, 0, blood, alpha) == (fading ? alpha : 255));
                assert(character_alpha(combine, 255, blood, alpha) == (fading ? alpha : 255));
            }
        } else {
            assert(!memcmp(&combine, &inherited, 8));
        }
    }
    assert(shade.words.w0);
    /* Textured faces must fade with texture alpha, independent of blood A. */
    if (fading) for (int alpha = 0; alpha < 256; alpha++) {
        assert(character_alpha(inherited, 0, 127, alpha) == 0);
        assert(character_alpha(inherited, 255, 127, alpha) == ((255 * alpha + 128) >> 8));
    }
}

static void check_character_materials(void)
{
    Gfx *src = (Gfx *)(g_TestRam + 0x10000), *stock = (Gfx *)(g_TestRam + 0x12000);
    Gfx *master = (Gfx *)(g_TestRam + 0x5000), *alive, *fading, *copy;
    Gfx snapshot[128], *segmented = (Gfx *)(uintptr_t)0x05000000;
    ModelRenderData data = prop();
    int size = sizeof(characterEdited) + 16;
    reset(); frameBytes = 0;
    memcpy(src, characterEdited, sizeof(characterEdited));
    memcpy(stock, opaque, sizeof(opaque));
    assert(!modelGdlPreservesType3Pipeline(&data, segmented, src));
    assert(modelGdlPreservesType3Pipeline(&data, stock, NULL));
    data.PropType = 7;
    alive = modelGetOneCycleGdl(&data, segmented, 3, src);
    assert(alive != src && alive != segmented);
    data.PropType = 8; data.envcolour.word = 0x5a000080;
    fading = modelGetOneCycleGdl(&data, segmented, 3, src);
    assert(fading != src && fading != segmented && fading != alive);
    assert(allocations == 2);
    memcpy(snapshot, fading, size);
    for (int aa = 0; aa < 2; aa++) for (int z = 0; z < 2; z++) for (int type = 3; type <= 4; type++) {
        renderSetAaEnabled(aa); renderApplySettings(); data.zbufferenabled = z;
        for (int alpha = 0; alpha < 256; alpha++) {
            data.envcolour.word = 0x5a000000 | alpha;
            data.PropType = 8;
            assert(modelGetOneCycleGdl(&data, src, type, NULL) == fading);
            data.PropType = 7;
            assert(modelGetOneCycleGdl(&data, src, type, NULL) == alive);
        }
        data.PropType = 8;
        character_equations(fading, size, data);
        data.PropType = 7;
        character_equations(alive, size, data);
    }
    assert(allocations == 2 && !frameBytes);
    assert(!memcmp(snapshot, fading, size));
    assert(!memcmp(src, characterEdited, sizeof(characterEdited)));
    /* Both node draw paths use the repair. The repaired list restores
     * the inherited combiner, preserving the cache win for following stock
     * parts without repeating their complete pipeline setup. Segment-5 lookup must not alias another file. */
    for (int format = 0; format < 2; format++) {
        union ModelRoData ro = {0}; union ModelRwData rw = {0};
        ModelNode node = {&ro}; Model model = {&rw}; ModelNodeRenderCache cache = {0};
        ro.DisplayList.Primary = segmented; ro.DisplayList.BaseAddr = src; ro.DisplayList.ModelType = 3;
        rw.DisplayListCollisions.gdl = segmented; rw.DisplayListCollisions.Vertices = g_TestRam + 0x15000;
        data.PropType = 8; data.gdl = master;
        if (format) {
            modelRenderNodeDlWithCache(&data, &model, &node, &cache);
            assert(cache.type3PipelineReady);
        } else modelRenderNodeGundl(&data, &node);
        assert(data.gdl[-1].words.w1 == K0_TO_PHYS(fading));
        if (format) {
            Gfx *start = data.gdl;
            ro.DisplayList.BaseAddr = stock;
            modelRenderNodeDlWithCache(&data, &model, &node, &cache);
            int setters = 0;
            for (Gfx *g = start; g < data.gdl; g++) setters += g->words.w0 >> 24 == 0xfc;
            assert(setters == 0 && cache.type3PipelineReady);
            assert(modelGetOneCycleGdl(&data, segmented, 3, stock) == segmented);
        }
    }
    /* Replacing material data invalidates metadata and copies without freeing
     * an in-flight version. Allocation failure and cache reclaim still fade. */
    modelOneCycleInvalidateGdlRange(src, src + sizeof(characterEdited) / 8);
    failAllocation = 1; data.PropType = 8;
    copy = modelGetOneCycleGdl(&data, src, 3, NULL);
    assert(copy != fading && frameBytes && !frees);
    character_equations(copy, size, data);
    assert(!memcmp(snapshot, fading, size));
    renderCacheRequestReclaim(); renderCacheReclaim(); frameBytes = 0;
    copy = modelGetOneCycleGdl(&data, src, 3, NULL);
    assert(frameBytes && !renderCacheIsEnabled());
    character_equations(copy, size, data);
    /* Stock lists and unrelated prop equations do not get character copies. */
    reset(); frameBytes = 0; data = prop(); renderSetAaEnabled(TRUE); renderApplySettings();
    assert(modelGetOneCycleGdl(&data, src, 3, NULL) == src && !allocations && !frameBytes);
    data.PropType = 8;
    assert(modelGetOneCycleGdl(&data, stock, 3, NULL) == stock && !allocations && !frameBytes);
    /* Unhandled state overrides and nested/invalid lists must not claim the
     * old cached character pipeline is still valid. */
    {
        const Gfx overrides[] = {
            gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE),
            gsDPSetRenderMode(G_RM_FOG_PRIM_A, G_RM_AA_ZB_OPA_SURF2),
            gsDPSetCycleType(G_CYC_1CYCLE), gsDPSetEnvColor(1, 2, 3, 4),
            {{0x06000000, 0x05000000}}
        };
        for (unsigned i = 0; i < sizeof(overrides) / sizeof(*overrides); i++) {
            modelOneCycleInvalidateGdlRange(src, src + 2);
            src[0] = overrides[i]; gSPEndDisplayList(src + 1);
            assert(!modelGdlPreservesType3Pipeline(&data, src, NULL));
        }
        assert(!modelGdlPreservesType3Pipeline(&data, segmented, NULL));
    }
    puts("Characters: mixed textured/untextured death fade, all alpha/blood values, live/dead sharing, AA modes, dispatch, state restoration, invalidation and memory-pressure fallback pass.");
}
