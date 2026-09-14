static void test_modes(void)
{
    s32 i, first, enabled, cycle;
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
        for (cycle = 0; cycle < 100; cycle++) for (enabled = 0; enabled < 2; enabled++) {
            renderSetAaEnabled(enabled); renderApplySettings();
            renderApplyAaCommand(&cmd);
            if (enabled) assert(!memcmp(&cmd, &original, sizeof(cmd)));
            else {
                assert((cmd.words.w1 & AA_OTHER_BITS_MASK) == (g_AaOpaqueModes[i][1] & AA_OTHER_BITS_MASK));
                if (first) assert((cmd.words.w1 & AA_FIRST_BLENDER_MASK) == firstModes[first]);
                assert((cmd.words.w1 & (Z_CMP | Z_UPD | 0xc00)) == (original.words.w1 & (Z_CMP | Z_UPD | 0xc00)));
            }
            { Gfx again = cmd; renderApplyAaCommand(&cmd); assert(!memcmp(&cmd, &again, sizeof(cmd))); }
            /* The room builder's Off mapping is independent of the applied setting. */
            { Gfx off = renderGetAaOffCommand(cmd); Gfx expected = renderGetAaOffCommand(original);
                assert(!memcmp(&off, &expected, sizeof(off))); }
        }
    }
    for (enabled = 0; enabled < 2; enabled++) for (i = 0; i < 6; i++) {
        original.words.w0 = AA_COMMAND_WORD; original.words.w1 = preserved[i]; cmd = original;
        renderSetAaEnabled(enabled); renderApplySettings(); renderApplyAaCommand(&cmd);
        assert(!memcmp(&cmd, &original, sizeof(cmd)));
    }
    puts("AA: repeated On/Off switching, exact restoration, fog/depth and exclusions passed");
}

static void test_vi_and_save(void)
{
    s32 aa, vi, value, pixel;
    OSViMode mode;
    u32 unaffected = VI_CTRL_GAMMA_DITHER_ON | VI_CTRL_SERRATE_ON | 0x3000;
    for (aa = 0; aa < 2; aa++) for (vi = 0; vi < 2; vi++) {
        renderSetAaEnabled(aa); renderSetViFilterEnabled(vi);
        value = renderEncodeSettings();
        assert(value == (0xa0 | (aa ? 0 : 2) | (vi ? 0 : 8)));
        renderDecodeSettings(0); renderDecodeSettings(value);
        assert(renderIsAaEnabled() == aa && renderIsViFilterEnabled() == vi);
        renderApplySettings();
        assert(!renderSettingsPending());
        assert(renderUseOneCycleBackground() == !aa);
        for (pixel = 2; pixel <= 3; pixel++) {
            mode.comRegs.ctrl = unaffected | pixel | VI_CTRL_ANTIALIAS_MASK | VI_CTRL_DITHER_FILTER_ON;
            renderConfigureViMode(&mode);
            assert((mode.comRegs.ctrl & unaffected) == unaffected);
            assert((mode.comRegs.ctrl & 3) == (u32)pixel);
            assert((mode.comRegs.ctrl & VI_CTRL_ANTIALIAS_MASK) == (vi ? 0 : pixel == 2 ? 0x200u : 0x300u));
            assert(!!(mode.comRegs.ctrl & VI_CTRL_DITHER_FILTER_ON) == vi);
            assert(!!(mode.comRegs.ctrl & VI_CTRL_DIVOT_ON) == vi);
        }
    }
    /* Covers old Full/Reduced/Off, Smooth/Edges/Off, both dither values,
     * current saves, and invalid/corrupt bytes. Next save drops old dither. */
    for (value = 0; value < 256; value++) {
        renderDecodeSettings(value);
        aa = vi = TRUE;
        if ((value & 0xe0) == 0xa0 && (value & 3) < 3 && ((value >> 2) & 3) < 3) {
            aa = (value & 3) != 2;
            vi = ((value >> 2) & 3) != 2;
        }
        assert(renderIsAaEnabled() == aa && renderIsViFilterEnabled() == vi);
        assert(renderEncodeSettings() == (0xa0 | (aa ? 0 : 2) | (vi ? 0 : 8)));
    }
    /* Changing requested values cannot change the in-flight frame's state. */
    renderDecodeSettings(0); renderApplySettings();
    renderSetAaEnabled(FALSE); renderSetViFilterEnabled(FALSE);
    assert(renderSettingsPending() && !renderUseOneCycleBackground());
    mode.comRegs.ctrl = VI_CTRL_TYPE_16;
    renderConfigureViMode(&mode);
    assert(mode.comRegs.ctrl & VI_CTRL_DITHER_FILTER_ON);
    renderApplySettings();
    assert(!renderSettingsPending() && renderUseOneCycleBackground());
    renderConfigureViMode(&mode);
    assert(!(mode.comRegs.ctrl & VI_CTRL_DITHER_FILTER_ON));
    puts("VI/save: four combinations, all 256 save bytes, legacy migration and queue gating passed");
}

static void test_authored_dither(void)
{
    Gfx *root = (Gfx *)g_TestRam;
    Gfx *leaf = (Gfx *)(g_TestRam + 0x10000);
    Gfx *dynamic = (Gfx *)(g_TestRam + 0x2000);
    Gfx original[4];
    u32 modes[] = {G_CD_MAGICSQ, G_CD_BAYER, G_CD_NOISE, G_CD_DISABLE};
    s32 aa, vi, type;
    for (aa = 0; aa < 2; aa++) for (vi = 0; vi < 2; vi++) for (type = 0; type < 4; type++) {
        gDPSetColorDither(root, modes[type]);
        gSPDisplayList(root + 1, 0x10000);
        gSPDisplayList(root + 2, 0x2000);
        gSPEndDisplayList(root + 3);
        gDPSetRenderMode(leaf, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
        gDPSetColorDither(leaf + 1, modes[type]);
        gDPSetAlphaDither(leaf + 2, G_AD_NOISE);
        gSPEndDisplayList(leaf + 3);
        gSPSetOtherMode(dynamic, G_SETOTHERMODE_H, 0, 32,
                G_AD_NOISE | G_TF_BILERP | G_CYC_2CYCLE | G_TP_PERSP | modes[type]);
        gSPEndDisplayList(dynamic + 1);
        original[0] = *root; original[1] = leaf[1]; original[2] = leaf[2]; original[3] = *dynamic;
        renderSetAaEnabled(aa); renderSetViFilterEnabled(vi); renderApplySettings();
        renderInvalidateDisplayListCache();
        assert(renderApplyDisplayListSettings(root, root + 4));
        assert(renderApplyDisplayListSettings(root, root + 4)); /* Cached + dynamic. */
        renderRestoreDisplayListSettings(root, root + 4);
        renderRestoreDisplayListSettings(leaf, leaf + 4);
        renderRestoreDisplayListSettings(dynamic, dynamic + 2);
        assert(!memcmp(root, original, 8));
        assert(!memcmp(leaf + 1, original + 1, 16));
        assert(!memcmp(dynamic, original + 3, 8));
    }
    puts("Dither: authored RGB/alpha dither and complete H commands remain byte-for-byte intact");
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
    renderInvalidateDisplayListCache();
    gSPSegment(root, 5, 0x10000);
    gSPDisplayList(root + 1, 0x05000000);
    gSPDisplayList(root + 2, 0x00002000);
    gSPEndDisplayList(root + 3);
    gDPSetRenderMode(leaf, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
    original = *leaf;
    gSPEndDisplayList(leaf + 1);
    *dynamic = original; gSPEndDisplayList(dynamic + 1);
    renderSetAaEnabled(FALSE); renderApplySettings();
    assert(renderApplyDisplayListSettings(root, root + 4));
    assert(!(leaf->words.w1 & (AA_EN | IM_RD)));
    assert(!(dynamic->words.w1 & AA_EN));
    savedCacheSlot = ((u32)leaf >> 3) & (RENDER_LEAF_CACHE_SIZE - 1);
    assert(g_RenderLeafCache[savedCacheSlot] == leaf);
    renderSetViFilterEnabled(!renderIsViFilterEnabled()); renderApplySettings();
    assert(g_RenderLeafCache[savedCacheSlot] == leaf);
    *dynamic = original; /* Dynamic lists must be processed every frame. */
    assert(renderApplyDisplayListSettings(root, root + 4));
    assert(!(dynamic->words.w1 & AA_EN));
    renderSetAaEnabled(TRUE); assert(renderSettingsPending()); renderApplySettings();
    assert(renderApplyDisplayListSettings(root, root + 4));
    assert(!memcmp(leaf, &original, sizeof(original)));
    renderSetAaEnabled(FALSE); renderApplySettings();
    assert(renderApplyDisplayListSettings(root, root + 4));
    renderRestoreDisplayListSettings(leaf, leaf + 2);
    assert(!memcmp(leaf, &original, sizeof(original)));
    assert(renderApplyDisplayListSettings(root, root + 4));
    assert(!(leaf->words.w1 & (AA_EN | IM_RD)));
    /* Branch lists replace the current stack frame and segmented addresses
     * set by a child remain visible to the parent. */
    gSPDisplayList(root, 0x18000);
    gSPDisplayList(root + 1, 0x05000000);
    gSPEndDisplayList(root + 2);
    gSPSegment(nested, 5, 0x10000);
    gSPBranchList(nested + 1, 0x10000);
    renderInvalidateDisplayListCache();
    assert(renderApplyDisplayListSettings(root, root + 3));
    /* Reload into the same address, as texLoadFromGdl does. */
    renderInvalidateDisplayListCache(); *leaf = original;
    assert(renderApplyDisplayListSettings(root, root + 3)); assert(!(leaf->words.w1 & IM_RD));
    gSPDisplayList(root, 0x0f100000); /* physical address equals RAM size */
    assert(!renderApplyDisplayListSettings(root, root + 1));
    gSPDisplayList(root, 0x10003); assert(!renderApplyDisplayListSettings(root, root + 1));
    gSPDisplayList(root, 0x18000); gSPDisplayList(nested, 0x18000);
    renderInvalidateDisplayListCache(); assert(!renderApplyDisplayListSettings(root, root + 1));
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

static void draw_watch_frame(Gfx *gdl)
{
    g_TestDrawCount = 0;
    watchDrawToggleOptions(gdl);
}

static void test_watch_editing(void)
{
    Gfx dummy[256];
    s32 index, input, count;
    u32 left[] = {L_JPAD, L_CBUTTONS, L_TRIG, 0};
    u32 right[] = {R_JPAD, R_CBUTTONS, R_TRIG, 0};
    for (index = GAME_OPTIONS_INDEX_AA; index <= GAME_OPTIONS_INDEX_VI_FILTER; index++) {
        for (input = 0; input < 4; input++) {
            renderDecodeSettings(0); renderApplySettings();
            g_WatchGameOptionsIndex = index;
            watch_item_is_actively_selected = FALSE;
            g_TestButtons = left[input]; g_TestStick = input == 3 ? -2 : 0;
            count = g_TestChooseCount;
            draw_watch_frame(dummy);
            assert(renderIsAaEnabled() && renderIsViFilterEnabled());
            assert(g_TestChooseCount == count);
            watch_item_is_actively_selected = TRUE;
            draw_watch_frame(dummy);
            assert((index == GAME_OPTIONS_INDEX_AA ? renderIsAaEnabled() : renderIsViFilterEnabled()) == FALSE);
            assert(g_TestChooseCount == count + 1 && g_TestStick == 0);
            assert(renderSettingsPending());
            if (index == GAME_OPTIONS_INDEX_AA) assert(!renderUseOneCycleBackground());
            draw_watch_frame(dummy); /* Left at Off is clamped, not wrapped. */
            assert(g_TestChooseCount == count + 1);
            g_TestButtons = right[input]; g_TestStick = input == 3 ? 2 : 0;
            draw_watch_frame(dummy);
            assert(renderIsAaEnabled() && renderIsViFilterEnabled());
            assert(g_TestChooseCount == count + 2 && g_TestStick == 0);
            draw_watch_frame(dummy); /* Right at On is also clamped. */
            assert(g_TestChooseCount == count + 2);
            /* Save loading must override the watch's previously drawn value. */
            renderDecodeSettings(0xaa);
            g_TestButtons = 0; g_TestStick = 0;
            draw_watch_frame(dummy);
            assert(!renderIsAaEnabled() && !renderIsViFilterEnabled());
        }
    }
    puts("Watch editing: vanilla D-pad/C-button/trigger/stick controls, sound gating and save sync passed");
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
    s32 selected, aa, vi, active, height, width, index, i;
    test_load_font();
    assert(g_TestFont.chars['a' - 33].height < g_TestFont.chars['A' - 33].height);
    assert(g_TestFont.chars['v' - 33].height == g_TestFont.chars['s' - 33].height);
    assert(GAME_OPTIONS_INDEX_COUNT == 12);
    assert(sizeof(g_GameOptionEntries) / sizeof(g_GameOptionEntries[0]) == GAME_OPTIONS_INDEX_COUNT - 2);
    textMeasure(&height, &width, "aa", ptrFontBankGothicChars, ptrFontBankGothic, 10);
    assert(width > 0 && height == 0);
    textMeasure(&height, &width, "aa\n", ptrFontBankGothicChars, ptrFontBankGothic, 10);
    assert(width > 0 && height == 10);
    g_TestButtons = 0; g_TestStick = 0;
    for (aa = 0; aa < 2; aa++) for (vi = 0; vi < 2; vi++)
    for (active = 0; active < 2; active++) for (selected = 0; selected < GAME_OPTIONS_INDEX_COUNT; selected++) {
        Gfx *end;
        renderSetAaEnabled(aa); renderSetViFilterEnabled(vi);
        g_WatchGameOptionsIndex = selected;
        watch_item_is_actively_selected = active;
        g_TestDrawCount = 0;
        end = watchDrawToggleOptions(list);
        test_triangle(list, end, g_WatchFirstToggleOption < 2);
        assert(!test_drew("<\n") && !test_drew(">\n") && !test_drew("color dither\n"));
        for (index = GAME_OPTIONS_INDEX_AA; index <= GAME_OPTIONS_INDEX_VI_FILTER; index++) {
            const char *label = index == GAME_OPTIONS_INDEX_AA ? "aa\n" : "vi filter\n";
            s32 value = index == GAME_OPTIONS_INDEX_AA ? aa : vi;
            bool visible = index - 2 < g_WatchFirstToggleOption + WATCH_VISIBLE_TOGGLE_OPTIONS;
            assert(test_drew(label) == visible);
            if (!visible) continue;
            for (i = 0; i < g_TestDrawCount; i++) if (!strcmp(g_TestDrawText[i], label)) break;
            assert(i + 2 < g_TestDrawCount);
            assert(!strcmp(g_TestDrawText[i + 1], "off\n"));
            assert(!strcmp(g_TestDrawText[i + 2], "on\n"));
            assert(g_TestDraw[i + 1].x + g_TestDraw[i + 1].width / 2 == 200);
            assert(g_TestDraw[i + 2].x + g_TestDraw[i + 2].width / 2 == 250);
            assert(g_TestDraw[i].x == XOFFSET_1);
            assert(g_TestDraw[i].y == YOFFSET_1 + (index - 2 - g_WatchFirstToggleOption) * YINC);
            assert(g_TestDraw[i + 1].y == g_TestDraw[i].y && g_TestDraw[i + 2].y == g_TestDraw[i].y);
            assert(g_TestDraw[i].outlined == (index == selected && active));
            assert(g_TestDraw[i + 1 + value].colour == (index == selected && active ? 0xa0ffa0f0u : 0x00ff00b0u));
            assert(g_TestDraw[i + 2 - value].colour == 0x00800080u);
        }
    }
    g_TestDrawCount = 0;
    puts("Watch layout: 12 rows, vanilla Off/On placement/highlights, font size and scroll triangle passed");
}

int main(void)
{
    test_modes(); test_vi_and_save(); test_authored_dither(); test_lists();
    test_watch(); test_watch_text(); test_watch_editing();
    return 0;
}
