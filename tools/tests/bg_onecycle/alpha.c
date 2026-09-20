/* Run the real texture expander, environment LUT and AA/one-cycle walkers.
 * Every test has Auto -> Vertex -> Auto, with a reload of shared vertices. */
static Gfx *alpha_scope(Gfx *p, u32 policy)
{
    gDPNoOpTag(p++, BG_ALPHA_TAG | policy);
    gDPNoOpTag(p++, BG_ALPHA_TAG | BG_ALPHA_SYNC);
    gDPNoOpTag(p++, BG_ALPHA_TAG | BG_ALPHA_FOG);
    for (u32 slot = BG_ALPHA_BLENDER; slot <= BG_ALPHA_LAST_SLOT; slot++)
    { gDPNoOpTag(p++, BG_ALPHA_TAG | slot); }
    return p;
}

static void alpha_state_checks(Gfx *gdl, int count, int expectedFog, int onecycle,
        Gfx original, u32 originalMode)
{
    BgOneCycleState state;
    int fog = expectedFog, loads = 0, draws = 0;
    int i;
    bgOneCycleResetState(&state);
    for (i = 0; i < count; i++)
    {
        Gfx cmd = gdl[i];
        u32 op = cmd.words.w0 >> 24;
        assert(bgOneCycleReadState(&state, cmd, TRUE));
        if (op == (u8)G_SETGEOMETRYMODE && (cmd.words.w1 & G_FOG)) fog = 1;
        if (op == (u8)G_CLEARGEOMETRYMODE && (cmd.words.w1 & G_FOG)) fog = 0;
        if (op == (u8)G_VTX) { assert(fog == (loads == 1 ? 0 : expectedFog)); loads++; }
        if (op == (u8)G_TRI1 || op == (u8)G_ENDDL)
        {
            if (draws == 1)
            {
                BgOneCycleState chosen;
                assert(!fog && state.alphaSource == BG_ALPHA_VERTEX);
                assert(!bgOneCycleChooseState(&state, &chosen, FALSE, TRUE, FALSE));
                assert(state.combine.words.w0 == BG_ALPHA_COMBINE_W0(original.words.w0));
                assert(state.combine.words.w1 == BG_ALPHA_COMBINE_W1(original.words.w1));
                /* Decode the RGB/alpha field boundaries independently of the
                 * override macros: bit 15 is RGB C0, not an alpha field. */
                assert((state.combine.words.w0 & ~0x00007e00u) ==
                       (original.words.w0 & ~0x00007e00u));
                assert((state.combine.words.w1 & ~0x00fc7e3fu) ==
                       (original.words.w1 & ~0x00fc7e3fu));
                /* Independent mux decode: both cycles emit SHADE alpha. */
                assert(((state.combine.words.w0 >> 12) & 7) == ((state.combine.words.w1 >> 12) & 7));
                assert(((state.combine.words.w1 >> 21) & 7) == ((state.combine.words.w1 >> 3) & 7));
                assert(((state.combine.words.w1 >> 9) & 7) == 4 && (state.combine.words.w1 & 7) == 4);
                assert(state.low == (onecycle ? originalMode : (originalMode & ~0xcccc0000u) | G_RM_PASS));
            }
            else
            {
                assert(fog == expectedFog && state.alphaSource == BG_ALPHA_AUTO);
                assert(!memcmp(&state.combine, &original, sizeof(original)));
                assert(state.low == originalMode);
            }
            assert((state.low & (Z_CMP | Z_UPD)) == Z_CMP);
            draws++;
        }
    }
    assert(loads == 3 && draws == 4 && fog == expectedFog);
}

/* Depot's shafts inherit the TERR blender, then change only bits 3..15 to
 * translucent. Rewriting that earlier TERR command to a SURF blender for
 * AA-Off must not substitute framebuffer coverage for (1 - pixel alpha). */
static void vertex_alpha_surface_override_checks(void)
{
    Gfx raw[96], expanded[128], alternate[256], *p;
    Gfx *runtime = (Gfx *)(g_TestRam + 0x30000);
    Gfx *submittedAlternate = (Gfx *)(g_TestRam + 0x40000);
    int fog, onecycle, terrain, neighbor, layer, z, toggle, size, bytes;
    for (fog = 0; fog < 2; fog++)
    for (onecycle = 0; onecycle < 2; onecycle++)
    for (terrain = 0; terrain < 2; terrain++)
    for (neighbor = 0; neighbor < 2; neighbor++)
    for (layer = 0; layer < 2; layer++)
    for (z = 0; z < 2; z++)
    {
        u32 original = onecycle
            ? (terrain ? G_RM_AA_ZB_OPA_TERR : G_RM_AA_ZB_OPA_SURF)
            : G_RM_PASS | (terrain ? G_RM_AA_ZB_OPA_TERR2 : G_RM_AA_ZB_OPA_SURF2);
        u32 finalShift = onecycle ? 18 : 16;
        u32 expected = onecycle ? G_RM_AA_ZB_XLU_SURF : G_RM_PASS | G_RM_AA_ZB_XLU_SURF2;
        enum CCRMLUT lut = fog ? (layer ? CCRMLUT_SECONDARY_ADDFOG : CCRMLUT_PRIMARY_ADDFOG)
            : (layer ? CCRMLUT_SECONDARY : CCRMLUT_PRIMARY);
        if (!z) { original &= ~(Z_CMP | Z_UPD); expected &= ~(Z_CMP | Z_UPD | ZMODE_XLU); }
        g_TestEnvironment.FogEnabled = fog;
        p = raw;
        gDPPipeSync(p++);
        gDPSetCycleType(p++, onecycle ? G_CYC_1CYCLE : G_CYC_2CYCLE);
        gDPSetRenderMode(p++, original, 0);
        gSPTexture(p++, 0xffff, 0xffff, 0, 0, 0);
        gDPSetCombineMode(p++, G_CC_SHADE, G_CC_SHADE);
        if (neighbor) { gSP1Triangle(p++, 0, 1, 2, 0); }
        gDPPipeSync(p++);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 3, 13, expected & 0xfff8);
        /* TERR already has 1MA: the editor need not write this field. */
        if (!terrain) { gSPSetOtherMode(p++, G_SETOTHERMODE_L, finalShift, 2, 0); }
        gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(BG_SURFACE_BLEND, original));
        p = alpha_scope(p, BG_ALPHA_VERTEX);
        gDPSetCombineMode(p++, G_CC_SHADE, G_CC_SHADE);
        p->words.w0 = 0x04200030; p++->words.w1 = 0x0e000000;
        gSP1Triangle(p++, 0, 1, 2, 0);
        p = alpha_scope(p, BG_ALPHA_AUTO);
        gDPSetCombineMode(p++, G_CC_SHADE, G_CC_SHADE);
        gDPPipeSync(p++);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 3, 13, original & 0xfff8);
        if (!terrain) { gSPSetOtherMode(p++, G_SETOTHERMODE_L, finalShift, 2, original & (3u << finalShift)); }
        gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(BG_SURFACE_AUTO, 0));
        if (neighbor) { gSP1Triangle(p++, 0, 1, 2, 0); }
        gSPEndDisplayList(p++);
        size = texLoadFromGdl(raw, (p - raw) * 8, expanded, NULL);
        bgApplyDynamicCCRMLUT(expanded, expanded + size / 8, lut);
        bytes = layer ? bgBuildCutoutGdl(expanded, size, alternate, sizeof(alternate))
                      : bgBuildOneCycleGdl(expanded, size, alternate, sizeof(alternate));
        assert((bytes > 0) == (neighbor && !onecycle));
        if (bytes > 0) { memcpy(submittedAlternate, alternate, bytes); }
        memcpy(runtime, expanded, size);
        for (toggle = 0; toggle < 7; toggle++)
        {
            int i, draws = 0, activeFog = fog;
            BgOneCycleState state;
            renderSetAaEnabled(!(toggle & 1)); renderApplySettings();
            assert(renderApplyDisplayListSettings(runtime, runtime + size / 8));
            if (bytes > 0 && (toggle & 1))
            { assert(renderApplyDisplayListSettings(submittedAlternate, submittedAlternate + bytes / 8)); }
            /* Exercise both the submitted original and the cached alternate. */
            for (int variant = 0; variant < 2; variant++)
            {
                Gfx *list = variant ? submittedAlternate : runtime;
                int count = variant ? bytes / 8 : size / 8;
                if (variant && (bytes <= 0 || !(toggle & 1))) { continue; }
                draws = 0; activeFog = fog;
                bgOneCycleResetState(&state);
                for (i = 0; i < count; i++)
                {
                    u32 op = list[i].words.w0 >> 24;
                    assert(bgOneCycleReadState(&state, list[i], layer));
                    if (op == (u8)G_SETGEOMETRYMODE && (list[i].words.w1 & G_FOG)) activeFog = 1;
                    if (op == (u8)G_CLEARGEOMETRYMODE && (list[i].words.w1 & G_FOG)) activeFog = 0;
                    if (op == (u8)G_VTX) { assert(!activeFog); }
                    if (op != (u8)G_TRI1 || state.alphaSource != BG_ALPHA_VERTEX) { continue; }
                    assert(state.low == expected);
                    assert((state.high & BG_CYCLE_MASK) == (onecycle ? G_CYC_1CYCLE : G_CYC_2CYCLE));
                    assert(state.combine.words.w0 == 0xfcffffff && state.combine.words.w1 == 0xfffe793c);
                    draws++;
                }
                assert(draws == 1 && activeFog == fog && state.alphaSource == BG_ALPHA_AUTO);
            }
        }
        assert(!memcmp(runtime, expanded, size));
    }
    puts("PASS vertex-alpha surface overrides: TERR/SURF inheritance, both cycles/layers, mixed optimized draws and repeated AA toggles.");
}

static void vertex_alpha_checks(void)
{
    Gfx raw[64], expanded[128], autoState[3], *p;
    Gfx *runtime = (Gfx *)(g_TestRam + 0x30000);
    Gfx combiners[] = {
        gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
        gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA),
        gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2)
    };
    int fog, authoredFog, onecycle, material, layer, aa, bytes, i, requests;
    for (fog = 0; fog < 2; fog++)
    for (authoredFog = -1; authoredFog < 2; authoredFog++)
    for (onecycle = 0; onecycle < 2; onecycle++)
    for (material = 0; material < 3; material++)
    for (layer = 0; layer < 2; layer++)
    {
        enum CCRMLUT lut = fog ? (layer ? CCRMLUT_SECONDARY_ADDFOG : CCRMLUT_PRIMARY_ADDFOG)
            : (layer ? CCRMLUT_SECONDARY : CCRMLUT_PRIMARY);
        int expectedFog = authoredFog < 0 ? fog : authoredFog;
        g_TestEnvironment.FogEnabled = fog;
        p = raw;
        gDPPipeSync(p++);
        gDPSetCycleType(p++, onecycle ? G_CYC_1CYCLE : G_CYC_2CYCLE);
        gDPSetRenderMode(p++, onecycle ? G_RM_AA_ZB_XLU_SURF : G_RM_PASS,
            onecycle ? G_RM_NOOP2 : G_RM_AA_ZB_XLU_SURF2);
        if (authoredFog >= 0)
        {
            if (authoredFog) { gSPSetGeometryMode(p++, G_FOG); }
            else { gSPClearGeometryMode(p++, G_FOG); }
        }
        gSPTexture(p++, 0xffff, 0xffff, 0, 0, material != 0);
        *p++ = combiners[material];
        p->words.w0 = 0x04200030; p++->words.w1 = 0x0e000000;
        gSP1Triangle(p++, 0, 1, 2, 0);
        p = alpha_scope(p, BG_ALPHA_VERTEX);
        *p++ = combiners[material];
        p->words.w0 = 0x04200030; p++->words.w1 = 0x0e000000;
        gSP1Triangle(p++, 0, 1, 2, 0);
        p = alpha_scope(p, BG_ALPHA_AUTO);
        *p++ = combiners[material];
        p->words.w0 = 0x04200030; p++->words.w1 = 0x0e000000;
        gSP1Triangle(p++, 0, 1, 2, 0);
        gSPEndDisplayList(p++);
        requests = g_TestTextureRequests;
        bytes = texLoadFromGdl(raw, (p - raw) * 8, expanded, NULL);
        assert(bytes == (p - raw) * 8 && !memcmp(raw, expanded, bytes));
        assert(requests == g_TestTextureRequests);
        autoState[0] = raw[2]; autoState[1] = combiners[material];
        gSPEndDisplayList(&autoState[2]);
        bgApplyDynamicCCRMLUT(autoState, NULL, lut);
        bgApplyDynamicCCRMLUT(expanded, expanded + bytes / 8, lut);
        alpha_state_checks(expanded, bytes / 8, expectedFog, onecycle, autoState[1], autoState[0].words.w1);
        /* No binary-alpha or opaque optimization may consume the vertex A. */
        for (i = 0; i < 2; i++)
        {
            int converted = i ? bgBuildCutoutGdl(expanded, bytes, NULL, 0)
                              : bgBuildOneCycleGdl(expanded, bytes, NULL, 0);
            assert(converted == 0);
        }
        memcpy(runtime, expanded, bytes);
        for (aa = 0; aa < 3; aa++)
        {
            renderSetAaEnabled(aa != 1); renderApplySettings();
            assert(renderApplyDisplayListSettings(runtime, runtime + bytes / 8));
            alpha_state_checks(runtime, bytes / 8, expectedFog, onecycle, autoState[1], autoState[0].words.w1);
        }
        if (!onecycle && !material)
        {
            BgOneCycleState state;
            int optimized, draws = 0, activeFog = expectedFog;
            /* Eligible opaque neighbors must still optimize, with the vertex
             * alpha draw staying two-cycle and restoring fog before them. */
            gDPSetRenderMode(&raw[2], G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
            bytes = texLoadFromGdl(raw, (p - raw) * 8, expanded, NULL);
            bgApplyDynamicCCRMLUT(expanded, expanded + bytes / 8, lut);
            optimized = bgBuildOneCycleGdl(expanded, bytes, runtime, 4096);
            assert(optimized > 0);
            bgOneCycleResetState(&state);
            for (i = 0; i < optimized / 8; i++)
            {
                Gfx cmd = runtime[i]; u32 op = cmd.words.w0 >> 24;
                assert(bgOneCycleReadState(&state, cmd, TRUE));
                if (op == (u8)G_SETGEOMETRYMODE && (cmd.words.w1 & G_FOG)) activeFog = 1;
                if (op == (u8)G_CLEARGEOMETRYMODE && (cmd.words.w1 & G_FOG)) activeFog = 0;
                if (op == (u8)G_TRI1)
                {
                    assert((state.high & BG_CYCLE_MASK) == (draws == 1 ? G_CYC_2CYCLE : G_CYC_1CYCLE));
                    assert(activeFog == (draws == 1 ? 0 : expectedFog));
                    if (draws == 1)
                    {
                        assert(state.alphaSource == BG_ALPHA_VERTEX);
                        assert(state.combine.words.w0 == BG_ALPHA_COMBINE_W0(autoState[1].words.w0));
                        assert(state.combine.words.w1 == BG_ALPHA_COMBINE_W1(autoState[1].words.w1));
                    }
                    draws++;
                }
            }
            assert(draws == 3 && state.alphaSource == BG_ALPHA_AUTO);
            assert((state.high & BG_CYCLE_MASK) == G_CYC_2CYCLE && activeFog == expectedFog);
        }
    }
    puts("PASS vertex alpha: fog/no-fog LUTs, explicit fog restoration, RGB retention, both cycles/layers, texture-marker dispatch and AA/one-cycle safety.");
    vertex_alpha_surface_override_checks();
}
