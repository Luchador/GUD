/* Independent integer oracle for the RDP alpha combiner. In particular,
 * 384..511 clamp to ZERO, not 255. Floating-point min(1, a+b) missed this. */
static int glass_ext9(int value)
{ return (value & 384) == 384 ? value - 512 : value; }
static int glass_clamp9(int value)
{ return value < 256 ? value : value < 384 ? 255 : 0; }
static int glass_equation(int a, int b, int c, int d)
{
    c = c >= 256 ? c - 512 : c;
    return (((glass_ext9(a) - glass_ext9(b)) * c
        + glass_ext9(d) * 256 + 128) >> 8) & 511;
}
static int glass_alpha(Snapshot s, int texture, int shade, int tint)
{
    int combined = 0;
    for (int cycle = 0; cycle < 2; cycle++)
    {
        int add[] = {combined, texture, texture, tint, shade, 255, 256, 0};
        int mul[] = {0, texture, texture, tint, shade, 255, 0, 0};
        int a = cycle ? (s.c1 >> 21) & 7 : (s.c0 >> 12) & 7;
        int b = cycle ? (s.c1 >> 3) & 7 : (s.c1 >> 12) & 7;
        int c = cycle ? (s.c1 >> 18) & 7 : (s.c0 >> 9) & 7;
        int d = cycle ? s.c1 & 7 : (s.c1 >> 9) & 7;
        combined = glass_equation(add[a], add[b], mul[c], add[d]);
    }
    return glass_clamp9(combined);
}
static void check_tinted_glass(void)
{
    Gfx output[32], *primary = (Gfx *)(g_TestRam + 0x10000);
    Snapshot states[2], fixed = {0}, legacy;
    memcpy(primary, opaque, sizeof(opaque));
    for (int type = 3; type <= 4; type++) for (int secondary = 0; secondary < 2; secondary++)
    for (int z = 0; z < 2; z++) for (int tint = 0; tint <= 255; tint += 51)
    {
        ModelRenderData data = prop();
        data.flags = 3 | MODEL_RENDER_TINTED_GLASS;
        data.envcolour.word = tint << 8; data.zbufferenabled = z; data.gdl = output;
        if (type == 3) modelApplyRenderModeType3(&data, !secondary);
        else modelApplyRenderModeType4(&data, !secondary);
        gSPEndDisplayList(data.gdl++);
        assert(data.gdl - output < 32 && snapshots(output, data.gdl - output, states) == 1);
        fixed = states[0];
        assert(((fixed.h >> G_MDSFT_CYCLETYPE) & 3) == 1 && !(fixed.l & 3));
        assert(!!(fixed.l & Z_CMP) == z);
        if (secondary) assert(!(fixed.l & Z_UPD) && (fixed.l & FORCE_BL));
        else if (z) assert(fixed.l & Z_UPD);
        int foundPrimitive = 0;
        for (Gfx *g = output; g < data.gdl; g++) if (g->words.w0 >> 24 == 0xfa)
        { assert((g->words.w1 & 255) == (unsigned)tint); foundPrimitive++; }
        assert(foundPrimitive == 1);
        assert(modelGetOneCycleGdl(&data, primary, type, g_TestRam) == primary);
    }
    /* Reproduce the uploaded window: both texture and vertex alpha are 255.
     * With the old shader it is opaque already, then vanishes at tint 130. */
    ModelRenderData data = prop(); data.gdl = output;
    modelApplyRenderModeType4(&data, FALSE); gSPEndDisplayList(data.gdl++);
    assert(snapshots(output, data.gdl - output, states) == 1); legacy = states[0];
    assert(glass_alpha(legacy, 255, 255, 129) == 255);
    assert(glass_alpha(legacy, 255, 255, 130) == 0);
    assert(glass_alpha(legacy, 255, 255, 255) == 0);
    for (int texture = 0; texture <= 255; texture++) for (int shade = 0; shade <= 255; shade++)
    {
        int last = -1;
        for (int tint = 0; tint <= 255; tint++)
        {
            int alpha = glass_alpha(fixed, texture, shade, tint);
            double base = texture / 255.0 * shade / 255.0;
            double expected = 255 * (base + (1 - base) * tint / 255.0);
            assert(alpha >= last && alpha <= 255);
            assert(alpha - expected < 2 && expected - alpha < 2);
            if (tint == 255) assert(alpha == 255);
            last = alpha;
        }
    }
    /* Stock 2m..6m setup distances and a nonzero nearby tint. */
    assert(glassOpacityAtDistance(100, 200, 600, 0) == 0);
    assert(glassOpacityAtDistance(200, 200, 600, 0) == 0);
    assert(glassOpacityAtDistance(400, 200, 600, 0) == 127);
    assert(glassOpacityAtDistance(600, 200, 600, 0) == 255);
    assert(glassOpacityAtDistance(900, 200, 600, 0) == 255);
    assert(glassOpacityAtDistance(100, 200, 600, .25f) == 63);
    puts("Tinted glass: legacy disappearance reproduced; all 16,777,216 alpha combinations are bounded and monotonic, with opaque endpoint, two-cycle state and cache exclusion.");
}
