/* Exercise real classification and mixed secondary rendering. Texture storage
 * is represented by a descriptor; these tests do not rasterize N64 pixels. */
static void alpha_checks(void)
{
    u8 data[64], palette[8] = {0, 0, 255, 255, 128, 128, 0, 1};
    const int formats[] = {TEXFORMAT_RGBA32, TEXFORMAT_RGBA16, TEXFORMAT_IA16,
        TEXFORMAT_IA8, TEXFORMAT_IA4, TEXFORMAT_RGBA16_CI8, TEXFORMAT_RGBA16_CI4,
        TEXFORMAT_IA16_CI8, TEXFORMAT_IA16_CI4};
    int i, format;
    for (i = 0; i < (int)(sizeof(formats) / sizeof(formats[0])); i++) {
        format = formats[i];
        memset(data, 0, sizeof(data));
        if (format == TEXFORMAT_RGBA32) data[7] = 255;
        else if (format == TEXFORMAT_RGBA16 || format == TEXFORMAT_IA16) data[3] = 255;
        else if (format == TEXFORMAT_IA8) data[1] = 15;
        else if (format == TEXFORMAT_IA4 || format == TEXFORMAT_RGBA16_CI4 || format == TEXFORMAT_IA16_CI4) data[0] = 1;
        else data[1] = 1;
        assert(texHasBinaryAlpha(data, sizeof(data), format, 2, 1, palette, 4));
        assert(!texHasBinaryAlpha(data, 1, format, 2, 1, palette, 4));
        assert(!texHasBinaryAlpha(data, sizeof(data), format, 2, 0, palette, 4));
        memset(data, 0, sizeof(data));
        assert(!texHasBinaryAlpha(data, sizeof(data), format, 2, 1, palette, 4));
    }
    memset(data, 255, sizeof(data));
    assert(!texHasBinaryAlpha(data, sizeof(data), TEXFORMAT_RGBA16, 2, 1, palette, 4));
    data[1] = 0; /* RGB's stored low bit is not opacity. */
    assert(!texHasBinaryAlpha(data, sizeof(data), TEXFORMAT_RGB15, 2, 1, palette, 4));
    data[3] = 127;
    assert(!texHasBinaryAlpha(data, sizeof(data), TEXFORMAT_IA16, 2, 1, palette, 4));
    assert(!texHasBinaryAlpha(data, sizeof(data), TEXFORMAT_I8, 2, 1, palette, 4));
    memset(data, 0, sizeof(data)); data[1] = 1; data[2] = 2;
    assert(!texHasBinaryAlpha(data, sizeof(data), TEXFORMAT_IA16_CI8, 3, 1, palette, 4));
    assert(!texHasBinaryAlpha(data, sizeof(data), TEXFORMAT_RGBA16_CI8, 3, 1, palette, 2));
    data[2] = 0;
    /* Unused partial-alpha palette entries and padded pixels do not count. */
    data[7] = 2;
    assert(texHasBinaryAlpha(data, sizeof(data), TEXFORMAT_IA16_CI8, 3, 1, palette, 4));
}

static int make_cutout(Gfx *src, int fog)
{
    Gfx *p = src;
    gDPPipeSync(p++);
    gDPSetCycleType(p++, G_CYC_2CYCLE);
    gDPSetRenderMode(p++, fog ? G_RM_FOG_SHADE_A : G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    gDPSetCombineLERP(p++, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0,
            COMBINED, 0, SHADE, 0, COMBINED, 0, ENVIRONMENT, 0);
    gDPSetTextureLOD(p++, G_TL_LOD);
    gDPSetTextureDetail(p++, G_TD_CLAMP);
    gDPSetTextureFilter(p++, G_TF_BILERP);
    gDPSetTextureLUT(p++, G_TT_NONE);
    gSPTexture(p++, 0xffff, 0xffff, 5, 0, 1);
    gDPSetTextureImage(p++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0x123450);
    gDPSetTile(p++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(p++);
    gDPLoadBlock(p++, 7, 0, 0, 1023, 0);
    gDPPipeSync(p++);
    gDPSetTile(p++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, 0, 0, 0, 5, 0, 0, 5, 0);
    gDPSetEnvColor(p++, 0, 0, 0, 255);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gSPEndDisplayList(p++);
    return (p - src) * 8;
}

static void override_checks(void)
{
    Gfx src[128], out[256], *p;
    Snapshot before[32], after[32];
    int size, bytes, fog, i, n;
    const Gfx tag = gsDPNoOpTag(BG_SURFACE_TAG_VALUE(BG_SURFACE_BLEND, G_RM_AA_ZB_XLU_SURF2));
    assert(BG_SURFACE_IS_MARKER(tag.words.w0, tag.words.w1));
    assert(BG_SURFACE_TAG_POLICY(tag.words.w1) == BG_SURFACE_BLEND);
    for (fog = 0; fog < 2; fog++) {
        size = make_cutout(src, fog);
        src[0] = tag;
        /* An explicit Blend overrides automatic binary-alpha classification. */
        assert(bgBuildCutoutGdl(src, size, NULL, 0) == 0);
        /* Native Opaque remains eligible for the opaque one-cycle path. */
        gDPNoOpTag(&src[0], BG_SURFACE_TAG_VALUE(BG_SURFACE_OPAQUE, src[2].words.w1));
        src[2].words.w1 = (fog ? G_RM_FOG_SHADE_A : G_RM_PASS) | G_RM_AA_ZB_OPA_SURF2;
        bytes = bgBuildCutoutGdl(src, size, out, sizeof(out)); assert(bytes > 0);
        snapshots(out, bytes / 8, after);
        assert((after[0].h & (3u << 20)) == G_CYC_1CYCLE && !(after[0].l & 3));
        /* Explicit Cutout can threshold soft alpha; Auto cannot assume that. */
        size = make_cutout(src, fog);
        g_TestTexture.hasBinaryAlpha = 0;
        src[2].words.w1 = (fog ? G_RM_FOG_SHADE_A : G_RM_PASS) | G_RM_AA_ZB_TEX_EDGE2;
        assert(bgBuildCutoutGdl(src, size, NULL, 0) == 0);
        gDPNoOpTag(&src[0], BG_SURFACE_TAG_VALUE(BG_SURFACE_CUTOUT, G_RM_AA_ZB_XLU_SURF2));
        bytes = bgBuildCutoutGdl(src, size, out, sizeof(out)); assert(bytes > 0);
        snapshots(out, bytes / 8, after);
        assert((after[0].h & (3u << 20)) == G_CYC_1CYCLE && (after[0].l & 3) == G_AC_THRESHOLD);
        src[9].words.w1 = 0x777770; /* A choice does not bypass upload validation. */
        assert(bgBuildCutoutGdl(src, size, NULL, 0) == 0);
        g_TestTexture.hasBinaryAlpha = 1;

        /* Auto -> explicit Blend -> Auto, without any native mode change. */
        size = make_cutout(src, fog); p = src + size / 8 - 1;
        *p++ = tag;
        gSP1Triangle(p++, 0, 1, 2, 0);
        gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(BG_SURFACE_AUTO, 0));
        gSP1Triangle(p++, 0, 1, 2, 0);
        gSPEndDisplayList(p++);
        size = (p - src) * 8;
        bytes = bgBuildCutoutGdl(src, size, out, sizeof(out)); assert(bytes > 0);
        n = snapshots(src, size / 8, before); assert(n == 4);
        assert(snapshots(out, bytes / 8, after) == n);
        for (i = 0; i < n; i++) {
            if (i == 0 || i == 2) assert((after[i].l & 3) == G_AC_THRESHOLD);
            else assert(!memcmp(&before[i], &after[i], sizeof(Snapshot)));
        }
        /* Both settings preserve metadata and the protected blended draw. */
        for (i = 0; i < 2; i++) {
            Gfx *ram = (Gfx *)(g_TestRam + 0x20000);
            int j, tags = 0;
            memcpy(ram, out, bytes);
            renderSetAaEnabled(i); renderApplySettings(); renderInvalidateDisplayListCache();
            assert(renderApplyDisplayListSettings(ram, ram + bytes / 8));
            for (j = 0; j < bytes / 8; j++) if (BG_SURFACE_IS_MARKER(out[j].words.w0, out[j].words.w1)) {
                assert(!memcmp(out + j, ram + j, sizeof(Gfx))); tags++;
            }
            assert(tags == 2);
            snapshots(ram, bytes / 8, after);
            assert(!memcmp(&before[1], &after[1], sizeof(Snapshot)));
        }
    }
    texture_marker_checks();
    puts("BG overrides: explicit Blend/Opaque/Cutout, Auto resets, mixed draws, tagged no-op loading and AA preservation pass");
}

static void cutout_combiner_checks(void)
{
    const Gfx authored[] = {
        gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEI2),
        gsDPSetCombineMode(G_CC_MODULATEI, G_CC_MODULATEI),
        gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
        gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
        gsDPSetCombineLERP(TEXEL0,0,SHADE,0,0,0,0,PRIMITIVE,
                          TEXEL0,0,SHADE,0,0,0,0,PRIMITIVE)
    };
    const Gfx cutout[] = {
        gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATEIA2),
        gsDPSetCombineMode(G_CC_MODULATEIA, G_CC_MODULATEIA)
    };
    for (int layer=0;layer<2;layer++) for (int fog=0;fog<2;fog++)
    for (unsigned int m=0;m<sizeof(authored)/sizeof(authored[0]);m++)
    {
        Gfx raw[32], expanded[64], expected[3], *p=raw;
        enum CCRMLUT lut=fog?(layer?CCRMLUT_SECONDARY_ADDFOG:CCRMLUT_PRIMARY_ADDFOG)
            :(layer?CCRMLUT_SECONDARY:CCRMLUT_PRIMARY);
        g_TestEnvironment.FogEnabled=fog;
        for (int scope=0;scope<3;scope++)
        {
            gDPPipeSync(p++);
            gDPNoOpTag(p++,BG_SURFACE_TAG_VALUE(scope==1?BG_SURFACE_CUTOUT:BG_SURFACE_AUTO,0));
            *p++=authored[m];
            gSP1Triangle(p++,0,1,2,0);
            expected[scope]=scope==1 && m<2?cutout[m]:authored[m];
            bgApplyDynamicCCRMLUT(&expected[scope],&expected[scope]+1,lut);
        }
        gSPEndDisplayList(p++);
        int bytes=texLoadFromGdl(raw,(p-raw)*8,expanded,NULL), draws=0;
        bgApplyDynamicCCRMLUT(expanded,expanded+bytes/8,lut);
        BgOneCycleState state; bgOneCycleResetState(&state);
        for (int i=0;i<bytes/8;i++)
        {
            assert(bgOneCycleReadState(&state,expanded[i],layer));
            if (expanded[i].words.w0>>24==(u8)G_TRI1)
            { assert(!memcmp(&state.combine,&expected[draws++],sizeof(Gfx))); }
        }
        assert(draws==3);
    }
    /* A soft-alpha texture using I2 becomes eligible only under explicit
     * Cutout. Verify the real LUT -> one-cycle path and AA toggles. */
    for (int fog=0;fog<2;fog++)
    {
        Gfx src[128], out[256]; Snapshot states[8];
        int size=make_cutout(src,fog);
        gDPNoOpTag(&src[0],BG_SURFACE_TAG_VALUE(BG_SURFACE_CUTOUT,0));
        src[2].words.w1=G_RM_PASS|G_RM_AA_ZB_TEX_EDGE2;
        src[3]=authored[0];
        g_TestTexture.hasBinaryAlpha=0;
        bgApplyDynamicCCRMLUT(src,src+size/8,fog?CCRMLUT_SECONDARY_ADDFOG:CCRMLUT_SECONDARY);
        int bytes=bgBuildCutoutGdl(src,size,out,sizeof(out)); assert(bytes>0);
        for (int aa=0;aa<2;aa++)
        {
            Gfx *runtime=(Gfx *)(g_TestRam+0x20000);
            memcpy(runtime,out,bytes);
            renderSetAaEnabled(aa); renderApplySettings(); renderInvalidateDisplayListCache();
            assert(renderApplyDisplayListSettings(runtime,runtime+bytes/8));
            snapshots(runtime,bytes/8,states);
            assert((states[0].h&(3u<<20))==G_CYC_1CYCLE && (states[0].l&3)==G_AC_THRESHOLD);
        }
    }
    g_TestTexture.hasBinaryAlpha=1;
    puts("Cutout combiner: texture alpha, custom/SHADE preservation, fog, both layers, Auto restoration and AA/one-cycle conversion pass");
}

static void secondary_fog_checks(void)
{
    /* Secondary geometry can use any ordinary surface. Its fog mapping must
     * agree with primary geometry, including opaque bases edited to Cutout. */
    for (Gfx *pair = DL_LUT_PRIMARY_ADDFOG; pair->words.w0; pair += 2) {
        Gfx mode = *pair;
        bgApplyDynamicCCRMLUT(&mode, &mode + 1, CCRMLUT_SECONDARY_ADDFOG);
        assert(!memcmp(&mode, pair + 1, sizeof(mode)));
    }
    for (int fog = 0; fog < 2; fog++) for (int opaque = 0; opaque < 2; opaque++) {
        Gfx src[128], out[256], *p;
        Snapshot states[8];
        const u32 base = opaque ? G_RM_AA_ZB_OPA_SURF2 : G_RM_AA_ZB_OPA_TERR2;
        const u32 first = fog ? G_RM_FOG_SHADE_A : G_RM_PASS;
        const Gfx combine = gsDPSetCombineMode(G_CC_MODULATEIFADEA, G_CC_MODULATEIFADEA);
        int size = make_cutout(src, 0);
        g_TestEnvironment.FogEnabled = fog;
        gDPSetRenderMode(&src[2], G_RM_PASS, base);
        /* Cradle rooms 20/35: opaque-terrain command, partial Cutout writes,
         * then a full opaque reset and more Cutout writes in the same scope.
         * Neither partial packet can match a full render-mode LUT entry. */
        p = src + size / 8 - 2;
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 3, 13, 0x3078);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 16, 2, 0x10000);
        gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(BG_SURFACE_CUTOUT, base));
        gDPSetCombineMode(p++, G_CC_TRILERP, G_CC_MODULATEI2);
        gSP1Triangle(p++, 0, 1, 2, 0);
        gDPPipeSync(p++);
        gDPSetRenderMode(p++, G_RM_PASS, base);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 3, 13, 0x3078);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 16, 2, 0x10000);
        gSP1Triangle(p++, 0, 1, 2, 0);
        gDPPipeSync(p++);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 3, 13, base & 0xfff8);
        gSPSetOtherMode(p++, G_SETOTHERMODE_L, 16, 2, base & 0x30000);
        gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(BG_SURFACE_AUTO, 0));
        gDPSetCombineMode(p++, G_CC_TRILERP, G_CC_MODULATEI2);
        gSP1Triangle(p++, 0, 1, 2, 0);
        gSPEndDisplayList(p++);
        size = (p - src) * sizeof(Gfx);
        bgApplyDynamicCCRMLUT(src, p, fog ? CCRMLUT_SECONDARY_ADDFOG : CCRMLUT_SECONDARY);
        assert(snapshots(src, size / 8, states) == 4);
        for (int i = 0; i < 3; i++) {
            assert(states[i].l == (first | (i < 2 ? G_RM_AA_ZB_TEX_EDGE2 : base)));
        }
        /* Test both the original two-cycle list and the AA-Off alternate.
         * The alternate keeps texture alpha for rejection and shade alpha
         * for fog, with no framebuffer colour bleeding through solid texels. */
        int bytes = bgBuildCutoutGdl(src, size, out, sizeof(out));
        assert(bytes > 0 && snapshots(out, bytes / 8, states) == 4);
        for (int i = 0; i < 2; i++) {
            assert((states[i].h & BG_CYCLE_MASK) == G_CYC_1CYCLE);
            assert((states[i].l & 3) == G_AC_THRESHOLD);
            assert(!(states[i].l & (CVG_X_ALPHA | ALPHA_CVG_SEL)));
            assert((states[i].l & (Z_CMP | Z_UPD)) == (Z_CMP | Z_UPD));
            assert(states[i].c0 == combine.words.w0 && states[i].c1 == combine.words.w1);
            if (fog) assert((states[i].l & BG_FIRST_BLENDER_MASK) == first && (states[i].l & FORCE_BL));
            else assert(!(states[i].l & FORCE_BL));
        }
        assert(!(states[2].l & 3)); /* Auto restores ordinary opaque rendering. */
        for (int aa = 0; aa < 2; aa++) {
            Gfx *runtime = (Gfx *)(g_TestRam + 0x20000);
            int count = (aa ? size : bytes) / 8;
            memcpy(runtime, aa ? src : out, count * sizeof(Gfx));
            renderSetAaEnabled(aa); renderApplySettings(); renderInvalidateDisplayListCache();
            assert(renderApplyDisplayListSettings(runtime, runtime + count));
            assert(snapshots(runtime, count, states) == 4);
            for (int i = 0; i < 2; i++) {
                if (fog) assert((states[i].l & BG_FIRST_BLENDER_MASK) == first);
                assert((states[i].l & 3) == (aa ? G_AC_NONE : G_AC_THRESHOLD));
            }
        }
    }
    puts("Secondary fog: opaque bases, partial Cutout overrides, repeated resets, Auto, texture alpha, fog on/off and both AA paths pass");
}

static void cutout_checks(void)
{
    Gfx src[256], saved[256], out[512], *p;
    Snapshot before[128], after[128];
    int size, bytes, n, i, j, fog, allocs, frees;
    const u32 modes[] = {G_RM_AA_ZB_XLU_SURF2, G_RM_ZB_XLU_SURF2, G_RM_AA_ZB_TEX_EDGE2};
    alpha_checks();
    memset(&g_TestTexture, 0, sizeof(g_TestTexture));
    g_TestTexture.width = g_TestTexture.height = 32;
    g_TestTexture.gbiformat = G_IM_FMT_RGBA;
    g_TestTexture.depth = G_IM_SIZ_16b;
    g_TestTexture.hasBinaryAlpha = 1;
    cutout_combiner_checks();
    secondary_fog_checks();
    override_checks();
    for (fog = 0; fog < 2; fog++) for (j = 0; j < 3; j++) {
        size = make_cutout(src, fog);
        src[2].words.w1 = (fog ? G_RM_FOG_SHADE_A : G_RM_PASS) | modes[j];
        memcpy(saved, src, size);
        bytes = bgBuildCutoutGdl(src, size, NULL, 0);
        assert(bytes > 0);
        assert(bgBuildCutoutGdl(src, size, out, bytes - 8) == -1);
        assert(bgBuildCutoutGdl(src, size, out, bytes) == bytes);
        assert(memcmp(src, saved, size) == 0);
        n = snapshots(out, bytes / 8, after);
        assert(n == 2);
        assert((after[0].h & (3u << 20)) == G_CYC_1CYCLE);
        assert(!(after[0].h & (1u << 16)));
        assert((after[0].l & 3) == G_AC_THRESHOLD);
        assert((after[0].l & (Z_CMP | Z_UPD)) == (Z_CMP | Z_UPD));
        assert(!(after[0].l & (AA_EN | IM_RD | CVG_X_ALPHA | ALPHA_CVG_SEL | ZMODE_DEC)));
        if (fog) assert((after[0].l & 0xcccc0000u) == (G_RM_FOG_SHADE_A) && (after[0].l & FORCE_BL));
        snapshots(src, size / 8, before);
        assert(memcmp(&after[1], &before[1], sizeof(Snapshot)) == 0);
        /* The submission walker must preserve the non-AA threshold mode. */
        memcpy(g_TestRam + 0x20000, out, bytes);
        renderSetAaEnabled(FALSE); renderApplySettings(); renderInvalidateDisplayListCache();
        assert(renderApplyDisplayListSettings((Gfx *)(g_TestRam + 0x20000), (Gfx *)(g_TestRam + 0x20000 + bytes)));
        assert(memcmp(g_TestRam + 0x20000, out, bytes) == 0);
    }
    /* Fail closed for soft textures, glass/fades, non-alpha combiners, unknown
     * images, partial uploads, alternate tiles, TLUT mismatches and decals. */
    for (i = 0; i < 14; i++) {
        size = make_cutout(src, 1);
        if (i == 0) g_TestTexture.hasBinaryAlpha = 0;
        if (i == 1) src[15].words.w1 = 127;
        if (i == 2) src[15].words.w0 = 0;
        if (i == 3) gDPSetCombineMode(&src[3], G_CC_TRILERP, G_CC_MODULATEIA2); /* Vertex alpha. */
        if (i == 4) src[9].words.w1 = 0x777770;
        if (i == 5) src[12].words.w1 = 0x07001000;
        if (i == 6) src[10].words.w0 |= 1;
        if (i == 7) src[14].words.w0 |= 1;
        if (i == 8) gDPSetTextureLUT(&src[7], G_TT_RGBA16);
        if (i == 9) gDPSetAlphaCompare(&src[0], G_AC_DITHER);
        if (i == 10) gDPSetBlendColor(&src[0], 0, 0, 0, 64);
        if (i == 11) src[2].words.w1 = G_RM_FOG_SHADE_A | G_RM_AA_ZB_XLU_DECAL2;
        if (i == 12) src[14].words.w0 ^= 1u << 19;
        if (i == 13) src[8].words.w0 |= 1u << 8;
        assert(bgBuildCutoutGdl(src, size, NULL, 0) == 0);
        g_TestTexture.hasBinaryAlpha = 1;
    }
    /* The real CI upload pattern uses the same image pointer for its palette,
     * and integer TLUT coordinates are encoded at bits 14 and 2. */
    for (i = 0; i < 4; i++) {
        size = make_cutout(src, 1);
        g_TestTexture.gbiformat = G_IM_FMT_CI;
        g_TestTexture.depth = G_IM_SIZ_4b;
        g_TestTexture.lutmodeindex = G_TT_RGBA16 >> G_MDSFT_TEXTLUT;
        g_TestTexture.unk0a = 15;
        gDPSetTextureLUT(&src[7], G_TT_RGBA16);
        gDPSetTextureImage(&src[9], G_IM_FMT_CI, G_IM_SIZ_16b, 1, 0x123450);
        gDPSetTile(&src[14], G_IM_FMT_CI, G_IM_SIZ_4b, 2, 0, 0, 0, 0, 5, 0, 0, 5, 0);
        p = src + 16;
        gDPSetTile(p++, 0, 0, 1, 0x100, 7, 0, 0, 0, 0, 0, 0, 0);
        /* len=1024: uls=16, ult=1008, count=16. */
        p->words.w0 = 0xf0000000 | (16 << 14) | (1008 << 2);
        p++->words.w1 = 0x07000000 | (31 << 14) | (1008 << 2);
        gSP1Triangle(p++, 0, 1, 2, 0);
        gSPEndDisplayList(p++);
        if (i == 1) src[17].words.w0 = 0; /* Missing palette. */
        if (i == 2) src[17].words.w1 -= 1 << 14; /* Partial palette. */
        if (i == 3) src[14].words.w1 |= 1 << 20; /* Wrong palette bank. */
        bytes = bgBuildCutoutGdl(src, (p - src) * 8, out, sizeof(out));
        assert(i == 0 ? bytes > 0 : bytes == 0);
    }
    g_TestTexture.gbiformat = G_IM_FMT_RGBA;
    g_TestTexture.depth = G_IM_SIZ_16b;
    g_TestTexture.lutmodeindex = 0;
    size = make_cutout(src, 1); p = src + size / 8 - 1;
    gDPPipeSync(p++);
    gDPSetEnvColor(p++, 0, 0, 0, 100); /* Tinted glass after foliage. */
    gSP1Triangle(p++, 0, 1, 2, 0);
    gDPSetEnvColor(p++, 0, 0, 0, 255);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gSPDisplayList(p++, 0x100);
    gSP1Triangle(p++, 0, 1, 2, 0); /* Child state unknown, never converted. */
    gSPEndDisplayList(p++);
    size = (p - src) * 8;
    bytes = bgBuildCutoutGdl(src, size, out, sizeof(out)); assert(bytes > 0);
    n = snapshots(src, size / 8, before);
    assert(snapshots(out, bytes / 8, after) == n);
    for (i = 0; i < n; i++) {
        if (i == 0 || i == 2) assert((after[i].l & 3) == G_AC_THRESHOLD);
        else assert(memcmp(&before[i], &after[i], sizeof(Snapshot)) == 0);
    }
    /* Actual secondary allocation, AA/VI selection and release paths. */
    size = make_cutout(src, 1);
    memset(g_BgRoomInfo, 0, sizeof(g_BgRoomInfo));
    g_BgRoomInfo[1].secondaryGdl = src;
    g_BgRoomInfo[1].secondaryGdlSize = size;
    g_BgRoomInfo[1].unloadAge = 1;
    allocs = g_TestAllocCount; frees = g_TestFreeCount;
    g_TestAllocFail = 1; bgBuildRoomOneCycleGdl(1);
    assert(!g_BgOneCycleRooms[1].secondaryGdl);
    renderSetAaEnabled(FALSE); renderApplySettings();
    assert(bgRenderRoomSecondary(out, 1) == out + 2);
    assert(out[1].words.w1 == OS_K0_TO_PHYSICAL(src));
    g_TestAllocFail = 0; bgBuildRoomOneCycleGdl(1);
    assert(g_TestAllocCount == allocs + 2);
    assert(g_BgOneCycleRooms[1].secondaryGdl);
    renderSetAaEnabled(TRUE); renderApplySettings();
    renderSetAaEnabled(FALSE); /* Pending preference must not select the copy. */
    assert(bgRenderRoomSecondary(out, 1) == out + 2);
    assert(out[1].words.w1 == OS_K0_TO_PHYSICAL(src));
    for (i = 0; i < 2; i++) {
        renderSetViFilterEnabled(i); renderApplySettings();
        assert(bgRenderRoomSecondary(out, 1) == out + 5);
        assert(out[3].words.w1 == BG_CUTOUT_THRESHOLD);
        assert(out[4].words.w1 == OS_K0_TO_PHYSICAL(g_BgOneCycleRooms[1].secondaryGdl));
    }
    bgFreeRoomData(1);
    assert(g_TestFreeCount == frees + 1);
    assert(!g_BgOneCycleRooms[1].secondaryGdl && !g_BgOneCycleRooms[1].secondarySize);
    puts("Cutouts: binary-alpha formats, glass/fades, fog, depth, state transitions, AA/VI and room lifecycle pass");
}

static void cutout_asset_checks(const char *path)
{
    FILE *file = fopen(path, "rb");
    int rooms = 0, converted = 0, i, lut, binary;
    assert(file);
    memset(&g_TestTexture, 0, sizeof(g_TestTexture));
    g_TestTexture.width = g_TestTexture.height = 32;
    g_TestTexture.gbiformat = G_IM_FMT_RGBA;
    g_TestTexture.depth = G_IM_SIZ_16b;
    while (fgetc(file) != EOF) {
        int size, bytes, nb, na, j;
        Gfx *original, *source, *out;
        Snapshot *before, *after;
        fseek(file, -1, SEEK_CUR); size = read_be(file);
        assert(size > 0 && size % 8 == 0);
        original = malloc(size); source = malloc(size); assert(original && source);
        for (i = 0; i < size / 8; i++) {
            original[i].words.w0 = read_be(file); original[i].words.w1 = read_be(file);
        }
        for (lut = 0; lut < 2; lut++) for (binary = 0; binary < 2; binary++) {
            Gfx *table = lut ? DL_LUT_SECONDARY_ADDFOG : DL_LUT_SECONDARY;
            memcpy(source, original, size);
            g_TestTexture.hasBinaryAlpha = binary;
            for (i = 0; i < size / 8; i++) {
                Gfx *pair;
                for (pair = table; pair->words.w0; pair += 2)
                    if (memcmp(&source[i], pair, 8) == 0) source[i] = pair[1];
            }
            bytes = bgBuildCutoutGdl(source, size, NULL, 0);
            assert(bytes >= 0);
            if (!bytes) continue;
            out = malloc(bytes); before = calloc(size / 8, sizeof(*before));
            after = calloc(bytes / 8, sizeof(*after)); assert(out && before && after);
            assert(bgBuildCutoutGdl(source, size, out, bytes) == bytes);
            for (i = 0; i < size / 8; i++) source[i] = renderGetAaOffCommand(source[i]);
            /* Original packets are retained, in order, up to ENDDL. */
            for (i = j = 0; i < bytes / 8 && j < size / 8; i++) {
                if (!memcmp(out + i, source + j, 8)) {
                    if (source[j++].words.w0 >> 24 == (u8)G_ENDDL) break;
                }
            }
            assert(j && source[j - 1].words.w0 >> 24 == (u8)G_ENDDL);
            nb = snapshots(source, j, before); na = snapshots(out, bytes / 8, after);
            assert(nb == na);
            for (i = 0; i < nb; i++) {
                if (!memcmp(before + i, after + i, sizeof(*before))) continue;
                assert((before[i].h & (3u << 20)) == G_CYC_2CYCLE);
                assert((after[i].h & (3u << 20)) == G_CYC_1CYCLE);
                assert(!(after[i].l & (AA_EN | IM_RD)));
                if ((after[i].l & 3) == G_AC_THRESHOLD) {
                    assert(binary && (before[i].env & 255) == 255);
                    assert((after[i].l & (Z_CMP | Z_UPD)) == (Z_CMP | Z_UPD));
                    converted++;
                }
            }
            assert(!memcmp(before + nb - 1, after + na - 1, sizeof(*before)));
            free(out); free(before); free(after);
        }
        rooms++; free(original); free(source);
    }
    fclose(file);
    assert(rooms > 0 && converted > 0);
    printf("%s: %d secondary streams checked, %d cutout candidate packets with synthetic binary-alpha uploads\n",
            path, rooms, converted);
}
