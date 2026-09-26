static void swapMissions(LEVEL_SOLO_SLOT a, LEVEL_SOLO_SLOT b)
{
    int ai = campaignGetEntryBySaveSlot(a), bi = campaignGetEntryBySaveSlot(b);
    struct mission_folder_setup row = mission_folder_setup_entries[ai];
    mission_folder_setup_entries[ai] = mission_folder_setup_entries[bi];
    mission_folder_setup_entries[bi] = row;
}

int main(void)
{
    unsigned random = 1234567;
    int sample, slot, difficulty, portal;
    LEVEL_SOLO_SLOT highest;
    DIFFICULTY highestDifficulty;
    struct LevelEntry original[LEVEL_INFO_COUNT];

    assert(!strcmp(levelIdName(LEVELID_SILO), "LEVELID_SILO"));
    assert(levelIdName(255) == NULL);
    assert(campaignGetMissionCount() == 20);
    assert(campaignGetEntryByLevel(LEVELID_SILO) == campaignGetEntryBySaveSlot(SP_LEVEL_SILO));
    assert(campaignGetEntryByLevel(LEVELID_TITLE) == -1);
    assert(campaignGetEntryBySaveSlot(SP_LEVEL_NONE) == -1);
    assert(campaignGetSaveSlotByOrder(-1) == SP_LEVEL_NONE);
    assert(campaignGetSaveSlotByOrder(20) == SP_LEVEL_NONE);

    /* Include sparse progress, all difficulty prefixes and arbitrary saves. */
    for (sample = 0; sample < 2048; sample++)
    {
        for (slot = 0; slot < SP_LEVEL_MAX; slot++)
            for (difficulty = 0; difficulty < 3; difficulty++)
            {
                random = random * 1664525u + 1013904223u;
                saves[0].completed[slot][difficulty] = sample < 64
                    ? slot < sample / 3 && difficulty <= sample % 3 : (random >> 28) != 0;
            }
        for (slot = 0; slot < SP_LEVEL_MAX; slot++)
        {
            for (difficulty = 0; difficulty < 4; difficulty++)
                assert(fileIsStageUnlockedAtDifficulty(0, slot, difficulty)
                    == legacyIsStageUnlocked(0, slot, difficulty));
            cheatWrittenSlot = SP_LEVEL_NONE;
            assert(cheatButtonUnlockMission(slot) == (legacyCheatUnlockSlot(slot) != SP_LEVEL_NONE));
            assert(cheatWrittenSlot == legacyCheatUnlockSlot(slot));
        }
    }
    puts("PASS: 163,840 original-campaign unlock decisions and 40,960 button-code checks match the old rules.");

    memset(saves, 0, sizeof(saves));
    swapMissions(SP_LEVEL_DAM, SP_LEVEL_SILO);
    swapMissions(SP_LEVEL_FACILITY, SP_LEVEL_CRADLE);
    mission_folder_setup_entries[campaignGetEntryBySaveSlot(SP_LEVEL_SILO)].number_text = "99";
    mission_folder_setup_entries[0].number_text = "42";
    assert(campaignGetSaveSlotByOrder(0) == SP_LEVEL_SILO);
    assert(campaignGetSaveSlotByOrder(1) == SP_LEVEL_CRADLE);
    assert(campaignGetOrderBySaveSlot(SP_LEVEL_DAM) == 5);
    assert(campaignGetNextMain(SP_LEVEL_SILO) == SP_LEVEL_CRADLE);
    assert(campaignGetLastMain() == SP_LEVEL_FACILITY);
    assert(campaignGetPhotoOrder(SW_BRIEF6) == 0);
    assert(campaignGetPhotoOrder(SW_BRIEF1) == 5);
    assert(campaignGetCheatTime(SP_LEVEL_SILO, DIFFICULTY_AGENT) == 180);
    assert(campaignGetCheatTime(SP_LEVEL_DAM, DIFFICULTY_SECRET) == 160);
    assert(fileIsStageUnlockedAtDifficulty(0, SP_LEVEL_SILO, DIFFICULTY_AGENT) == STAGESTATUS_UNLOCKED);
    assert(fileIsStageUnlockedAtDifficulty(0, SP_LEVEL_DAM, DIFFICULTY_AGENT) == STAGESTATUS_LOCKED);
    saves[0].completed[SP_LEVEL_SILO][DIFFICULTY_AGENT] = 1;
    assert(fileIsStageUnlockedAtDifficulty(0, SP_LEVEL_CRADLE, DIFFICULTY_AGENT) == STAGESTATUS_UNLOCKED);
    assert(fileGetHighestStageUnlockedAnyFolder() == SP_LEVEL_CRADLE);
    fileGetHighestStageDifficultyCompletedForFolder(0, &highest, &highestDifficulty);
    assert(highest == SP_LEVEL_SILO && highestDifficulty == DIFFICULTY_AGENT);
    assert(saves[0].completed[SP_LEVEL_SILO][DIFFICULTY_AGENT] && !saves[0].completed[SP_LEVEL_DAM][DIFFICULTY_AGENT]);
    swapMissions(SP_LEVEL_AZTEC, SP_LEVEL_CRADLE);
    assert(campaignGetNextMain(SP_LEVEL_SILO) == SP_LEVEL_RUNWAY);
    assert(fileIsStageUnlockedAtDifficulty(0, SP_LEVEL_RUNWAY, DIFFICULTY_AGENT) == STAGESTATUS_UNLOCKED);
    assert(fileIsStageUnlockedAtDifficulty(0, SP_LEVEL_AZTEC, DIFFICULTY_SECRET) == STAGESTATUS_LOCKED);
    cheatWrittenSlot = SP_LEVEL_NONE;
    assert(cheatButtonUnlockMission(SP_LEVEL_SURFACE1));
    assert(cheatWrittenSlot == SP_LEVEL_RUNWAY);
    swapMissions(SP_LEVEL_AZTEC, SP_LEVEL_SILO);
    assert(campaignGetSaveSlotByOrder(0) == SP_LEVEL_AZTEC);
    assert(campaignGetFirstMain() == SP_LEVEL_SILO);
    assert(campaignIsPrerequisite(SP_LEVEL_AZTEC, SP_LEVEL_FACILITY));
    assert(!campaignIsPrerequisite(SP_LEVEL_SILO, SP_LEVEL_AZTEC));
    missing_saves = 1;
    assert(fileGetHighestStageUnlockedForFolder(0) == SP_LEVEL_SILO);
    assert(fileGetHighestStageUnlockedAnyFolder() == SP_LEVEL_SILO);
    assert(fileIsStageUnlockedAtDifficulty(0, SP_LEVEL_SILO, DIFFICULTY_AGENT) == STAGESTATUS_UNLOCKED);
    assert(fileIsStageUnlockedAtDifficulty(0, SP_LEVEL_AZTEC, DIFFICULTY_SECRET) == STAGESTATUS_LOCKED);
    missing_saves = 0;
    puts("PASS: reordered campaign, bonus placement, labels, saves, photographs, cheat times and next-stage selection.");

    memcpy(original, g_LevelInfoTable, sizeof(original));
    for (slot = 0; slot < LEVEL_INFO_COUNT; slot++)
        g_LevelInfoTable[slot] = original[LEVEL_INFO_COUNT - 1 - slot];
    for (slot = 0; slot < LEVEL_INFO_COUNT; slot++)
    {
        struct LevelEntry *row = lvFindLevelInfo(original[slot].levelID);
        assert(row && row->levelName == original[slot].levelName);
        assert(row->memoryAllocationString == original[slot].memoryAllocationString);
    }
    assert(strstr(lvGetMemoryAllocationString(LEVELID_SILO, 1), "-mt660"));
    assert(strstr(lvGetMemoryAllocationString(LEVELID_DAM, 1), "-mt625"));
    assert(strstr(lvGetMemoryAllocationString(LEVELID_BUNKER2, 4), "-mt550"));
    assert(lvFindStageInfo(LEVELID_TITLE, 4)->levelID == LEVELID_TITLE);

    g_CurrentBgLevelId = LEVELID_CONTROL;
    bgMarkSpecialPortals();
    for (portal = 0; portal < 128; portal++)
    {
        int expected = (portal >= 0x2c && portal <= 0x2e) || (portal >= 0x32 && portal <= 0x37)
            || (portal >= 0x3e && portal <= 0x3f) || (portal >= 0x4e && portal <= 0x56)
            || (portal >= 0x59 && portal <= 0x5d) || (portal >= 0x72 && portal <= 0x76)
            || (portal >= 0x79 && portal <= 0x7a);
        assert(g_BgPortals[portal * 8 + 6] == (expected ? 2 : 0));
    }
    memset(g_BgPortals, 0, sizeof(g_BgPortals));
    g_CurrentBgLevelId = LEVELID_JUNGLE;
    bgMarkSpecialPortals();
    for (portal = 0; portal < 128; portal++) assert(g_BgPortals[portal * 8 + 6] == (portal <= 0x3a ? 2 : 0));
    memset(g_BgPortals, 0, sizeof(g_BgPortals));
    g_CurrentBgLevelId = LEVELID_SILO;
    bgMarkSpecialPortals();
    for (portal = 0; portal < 128; portal++) assert(!g_BgPortals[portal * 8 + 6]);
    puts("PASS: catalog permutation preserves resources, memory budgets, MP/Title selection and level-specific portals.");
    return 0;
}
