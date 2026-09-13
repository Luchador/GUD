static void test_modes(void)
{
    s32 i, first, style, cycle;
    u32 firstModes[] = {0, G_RM_PASS, G_RM_FOG_SHADE_A, G_RM_FOG_PRIM_A};
    u32 preserved[] = {G_RM_AA_ZB_TEX_EDGE | G_RM_AA_ZB_TEX_EDGE2,
        G_RM_AA_ZB_XLU_SURF | G_RM_AA_ZB_XLU_SURF2,
        G_RM_AA_ZB_XLU_DECAL | G_RM_AA_ZB_XLU_DECAL2,
        G_RM_AA_PCL_SURF | G_RM_AA_PCL_SURF2,
        G_RM_ZB_CLD_SURF | G_RM_ZB_CLD_SURF2,
        G_RM_ZB_OPA_SURF | G_RM_ZB_OPA_SURF2};
    Gfx cmd, original;
    assert(sizeof(Gfx) == 8);
    for (i = 0; i < 5; i++) for (first = 0; first < 4; first++) {
        original.words.w0 = AA_COMMAND_WORD;
        original.words.w1 = first == 0 ? g_AaOpaqueModes[i][0]
            : (g_AaOpaqueModes[i][0] & AA_OTHER_BITS_MASK) | firstModes[first];
        cmd = original;
        for (cycle = 0; cycle < 100; cycle++) for (style = 2; style >= 0; style--) {
            u32 expected;
            renderSetAaStyle(style); renderApplySettings();
            renderApplyAaCommand(&cmd);
            if (style == RENDER_AA_FULL) assert(!memcmp(&cmd, &original, sizeof(cmd)));
            else {
                expected = style == RENDER_AA_REDUCED ? g_AaOpaqueModes[i][0] & ~IM_RD : g_AaOpaqueModes[i][1];
                assert((cmd.words.w1 & AA_OTHER_BITS_MASK) == (expected & AA_OTHER_BITS_MASK));
                if (first) assert((cmd.words.w1 & AA_FIRST_BLENDER_MASK) == firstModes[first]);
                assert((cmd.words.w1 & (Z_CMP | Z_UPD | 0xc00)) == (original.words.w1 & (Z_CMP | Z_UPD | 0xc00)));
            }
            { Gfx again = cmd; renderApplyAaCommand(&cmd); assert(!memcmp(&cmd, &again, sizeof(cmd))); }
        }
    }
    for (style = 0; style < 3; style++) for (i = 0; i < 6; i++) {
        original.words.w0 = AA_COMMAND_WORD; original.words.w1 = preserved[i]; cmd = original;
        renderSetAaStyle(style); renderApplySettings(); renderApplyAaCommand(&cmd);
        assert(!memcmp(&cmd, &original, sizeof(cmd)));
    }
    puts("AA: exact restoration, repeated switching, fog, depth and excluded surfaces passed");
}

static void test_vi_and_save(void)
{
    s32 aa, vi, value, pixel;
    OSViMode mode;
    u32 unaffected = VI_CTRL_GAMMA_DITHER_ON | VI_CTRL_SERRATE_ON | 0x3000;
    for (aa = 0; aa < 3; aa++) for (vi = 0; vi < 3; vi++) {
        renderSetAaStyle(aa); renderSetViFilter(vi); value = renderEncodeSettings();
        renderDecodeSettings(0); renderDecodeSettings(value);
        assert(renderGetAaStyle() == aa && renderGetViFilter() == vi);
        renderApplySettings();
        for (pixel = 2; pixel <= 3; pixel++) {
            mode.comRegs.ctrl = unaffected | pixel | VI_CTRL_ANTIALIAS_MASK | VI_CTRL_DITHER_FILTER_ON;
            renderConfigureViMode(&mode);
            assert((mode.comRegs.ctrl & unaffected) == unaffected);
            assert((mode.comRegs.ctrl & 3) == (u32)pixel);
            assert((mode.comRegs.ctrl & VI_CTRL_ANTIALIAS_MASK) == (vi == 0 ? 0 : vi == 1 ? 0x100u : pixel == 2 ? 0x200u : 0x300u));
            assert(!!(mode.comRegs.ctrl & VI_CTRL_DITHER_FILTER_ON) == (vi == 0));
            assert(!!(mode.comRegs.ctrl & VI_CTRL_DIVOT_ON) == (vi != 2));
        }
    }
    for (value = 0; value < 256; value++) {
        renderDecodeSettings(value);
        if ((value & 0xf0) != 0xa0 || (value & 3) >= 3 || ((value >> 2) & 3) >= 3)
            assert(renderGetAaStyle() == 0 && renderGetViFilter() == 0);
    }
    renderSetAaStyle(-1); renderSetViFilter(99);
    assert(renderGetAaStyle() == 0 && renderGetViFilter() == 0);
    puts("VI/save: all 9 combinations, pixel formats, and 256 legacy/invalid bytes passed");
}

static void test_lists(void)
{
    Gfx *root = (Gfx *)g_TestRam;
    Gfx *leaf = (Gfx *)(g_TestRam + 0x10000);
    Gfx *dynamic = (Gfx *)(g_TestRam + 0x2000);
    Gfx *nested = (Gfx *)(g_TestRam + 0x18000);
    Gfx original;
    u32 savedCacheSlot;
    memset(g_TestRam, 0, sizeof(g_TestRam));
    gSPSegment(root, 5, 0x10000);
    gSPDisplayList(root + 1, 0x05000000);
    gSPDisplayList(root + 2, 0x00002000);
    gSPEndDisplayList(root + 3);
    gDPSetRenderMode(leaf, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
    original = *leaf;
    gSPEndDisplayList(leaf + 1);
    *dynamic = original; gSPEndDisplayList(dynamic + 1);
    renderSetAaStyle(RENDER_AA_OFF); renderApplySettings();
    assert(renderApplyAa(root, root + 4));
    assert(!(leaf->words.w1 & (AA_EN | IM_RD)));
    assert(!(dynamic->words.w1 & AA_EN));
    savedCacheSlot = ((u32)leaf >> 3) & (AA_LEAF_CACHE_SIZE - 1);
    assert(g_AaLeafCache[savedCacheSlot] == leaf);
    *dynamic = original; /* Dynamic lists must be processed every frame. */
    assert(renderApplyAa(root, root + 4));
    assert(!(dynamic->words.w1 & AA_EN));
    renderSetAaStyle(RENDER_AA_FULL); assert(renderSettingsPending()); renderApplySettings();
    assert(renderApplyAa(root, root + 4));
    assert(!memcmp(leaf, &original, sizeof(original)));
    renderSetAaStyle(RENDER_AA_REDUCED); renderApplySettings();
    assert(renderApplyAa(root, root + 4));
    renderRestoreAaGdl(leaf, leaf + 2);
    assert(!memcmp(leaf, &original, sizeof(original)));
    assert(renderApplyAa(root, root + 4));
    assert((leaf->words.w1 & AA_EN) && !(leaf->words.w1 & IM_RD));
    /* Branch lists replace the current stack frame and segmented addresses
     * set by a child remain visible to the parent. */
    gSPDisplayList(root, 0x18000);
    gSPDisplayList(root + 1, 0x05000000);
    gSPEndDisplayList(root + 2);
    gSPSegment(nested, 5, 0x10000);
    gSPBranchList(nested + 1, 0x10000);
    renderInvalidateAaCache();
    assert(renderApplyAa(root, root + 3));
    /* Reload into the same address, as texLoadFromGdl does. */
    renderInvalidateAaCache(); *leaf = original;
    assert(renderApplyAa(root, root + 3)); assert(!(leaf->words.w1 & IM_RD));
    gSPDisplayList(root, 0x0f100000); /* physical address equals RAM size */
    assert(!renderApplyAa(root, root + 1));
    gSPDisplayList(root, 0x10003); assert(!renderApplyAa(root, root + 1));
    gSPDisplayList(root, 0x18000); gSPDisplayList(nested, 0x18000);
    renderInvalidateAaCache(); assert(!renderApplyAa(root, root + 1));
    puts("Display lists: segments, nesting, branches, cache reuse, reload and bounds passed");
}

static void test_watch(void)
{
    s32 direction, i, before;
    g_WatchGameOptionsIndex = 0;
    for (direction = 0; direction < 2; direction++) for (i = 0; i < GAME_OPTIONS_INDEX_COUNT * 3; i++) {
        before = g_WatchGameOptionsIndex;
        g_TestButtons = direction == 0 ? D_JPAD : U_CBUTTONS;
        g_TestActive = 1;
        if (before == 0) game_options_music_volume_navigation();
        else if (before == 1) game_options_fx_volume_navigation();
        else watchNavToggleOptions();
        assert(g_WatchGameOptionsIndex == (u32)((before + (direction == 0 ? 1 : GAME_OPTIONS_INDEX_COUNT - 1)) % GAME_OPTIONS_INDEX_COUNT));
        assert(!g_TestActive);
        watchScrollGameOptions();
        assert(g_WatchFirstToggleOption >= 0 && g_WatchFirstToggleOption <= 2);
        if (g_WatchGameOptionsIndex >= 2) {
            assert((s32)g_WatchGameOptionsIndex - 2 >= g_WatchFirstToggleOption);
            assert((s32)g_WatchGameOptionsIndex - 2 < g_WatchFirstToggleOption + WATCH_VISIBLE_TOGGLE_OPTIONS);
        } else assert(g_WatchFirstToggleOption == 0);
    }
    g_TestButtons = 0; g_TestStick = -1; before = g_WatchGameOptionsIndex;
    watchNavToggleOptions(); assert(g_WatchGameOptionsIndex == (u32)(before + 1));
    puts("Watch: every row reachable, wraparound, volume navigation and scrolling passed");
}

static void test_watch_editing(void)
{
    Gfx dummy[256];
    renderDecodeSettings(0);
    g_TestButtons = R_JPAD;
    watchDrawRenderOption(dummy, 155, GAME_OPTIONS_INDEX_AA, 1);
    assert(renderGetAaStyle() == RENDER_AA_FULL);
    watchDrawRenderOption(dummy, 155, GAME_OPTIONS_INDEX_AA, 2);
    assert(renderGetAaStyle() == RENDER_AA_REDUCED);
    watchDrawRenderOption(dummy, 155, GAME_OPTIONS_INDEX_AA, 2);
    watchDrawRenderOption(dummy, 155, GAME_OPTIONS_INDEX_AA, 2);
    assert(renderGetAaStyle() == RENDER_AA_OFF);
    watchDrawRenderOption(dummy, 170, GAME_OPTIONS_INDEX_VI_FILTER, 2);
    assert(renderGetViFilter() == RENDER_VI_EDGES);
    g_TestButtons = L_CBUTTONS;
    watchDrawRenderOption(dummy, 170, GAME_OPTIONS_INDEX_VI_FILTER, 2);
    assert(renderGetViFilter() == RENDER_VI_SMOOTH);
    g_TestButtons = 0;
    puts("Watch editing: selection gating, both directions and value limits passed");
}

static bool test_drew(const char *text)
{
    s32 i;
    for (i = 0; i < g_TestDrawCount; i++) if (!strcmp(g_TestDrawText[i], text)) return TRUE;
    return FALSE;
}

static void test_triangle(Gfx *start, Gfx *end, bool expected)
{
    Gfx *cmd;
    s32 count = 0;
    s32 previousWidth = 0;
    assert(end < start + 256);
    for (cmd = start; cmd < end; cmd++) {
        if ((cmd->words.w0 >> 24) == (u8)G_FILLRECT) {
            s32 left = (cmd->words.w1 >> 14) & 1023;
            s32 top = (cmd->words.w1 >> 2) & 1023;
            s32 right = (cmd->words.w0 >> 14) & 1023;
            s32 bottom = (cmd->words.w0 >> 2) & 1023;
            assert(expected && left < right && bottom == top + 1);
            assert(top > YOFFSET_1 + (WATCH_VISIBLE_TOGGLE_OPTIONS - 1) * YINC + 10);
            assert(bottom < 216); /* Top edge of the watch page rectangles. */
            assert(left + right == 321); /* Centered in the 320-pixel view. */
            if (count) assert(right - left == previousWidth - 2);
            previousWidth = right - left;
            count++;
        }
    }
    assert(count == (expected ? 7 : 0));
    if (expected) assert(previousWidth == 1);
}

static void test_watch_text(void)
{
    Gfx list[256];
    s32 selected, aa, vi, active, height, width;
    test_load_font();
    assert(g_TestFont.chars['a' - 33].height < g_TestFont.chars['A' - 33].height);
    assert(g_TestFont.chars['v' - 33].height == g_TestFont.chars['s' - 33].height);
    assert(GAME_OPTIONS_INDEX_COUNT == 12);
    /* Reproduce the original bug through the real text measurement code. */
    textMeasure(&height, &width, "aa", ptrFontBankGothicChars, ptrFontBankGothic, 10);
    assert(width > 0 && height == 0);
    textMeasure(&height, &width, "aa\n", ptrFontBankGothicChars, ptrFontBankGothic, 10);
    assert(width > 0 && height == 10);
    g_TestButtons = 0; g_TestStick = 0;
    /* Exercise actual scrolling, labels, all values, both arrows and outlines. */
    for (aa = 0; aa < 3; aa++) for (vi = 0; vi < 3; vi++)
    for (active = 0; active < 2; active++) for (selected = 0; selected < GAME_OPTIONS_INDEX_COUNT; selected++) {
        Gfx *end;
        renderSetAaStyle(aa); renderSetViFilter(vi);
        g_WatchGameOptionsIndex = selected;
        watch_item_is_actively_selected = active;
        g_TestDrawCount = 0;
        end = watchDrawToggleOptions(list);
        test_triangle(list, end, g_WatchFirstToggleOption < 2);
        if (g_WatchFirstToggleOption >= 1) assert(test_drew("aa\n"));
        if (g_WatchFirstToggleOption >= 2) assert(test_drew("vi filter\n"));
    }
    g_TestDrawCount = 0;
    puts("Watch text: production measurement/render bounds passed for small-cap labels, values, arrows and outlines");
}

int main(void)
{
    test_modes(); test_vi_and_save(); test_lists(); test_watch(); test_watch_text(); test_watch_editing();
    return 0;
}
