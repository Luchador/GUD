static const struct { s32 id; const char *tokens; } expected[] = {
#include "expected.inc.c"
};

static const char *reference(s32 id, s32 players)
{
    size_t i;

    if (id != LEVELID_TITLE && players >= 2)
        for (i = 0; i < ARRAYCOUNT(expected); i++)
            if (expected[i].id != LEVELID_DEFAULT && expected[i].id == id + 400)
                return expected[i].tokens;

    for (i = 0; i < ARRAYCOUNT(expected); i++)
        if (expected[i].id == id || expected[i].id == LEVELID_DEFAULT)
            return expected[i].tokens;

    assert(0);
    return NULL;
}

int main(void)
{
    size_t i, j;
    s32 id, players;
    struct LevelEntry *entry;
    const char *saved;

    /* Every former allocation, including title/special/MP/default entries,
     * must now reside directly in LevelEntry without changing its tokens. */
    assert(ARRAYCOUNT(expected) == 35);
    for (i = 0; i < ARRAYCOUNT(expected); i++)
    {
        entry = lvFindLevelInfo(expected[i].id);
        assert(entry && entry->memoryAllocationString);
        assert(!strcmp(entry->memoryAllocationString, expected[i].tokens));
    }
    for (id = -1; id <= 1000; id++)
        for (players = 0; players <= 4; players++)
            assert(!strcmp(lvGetMemoryAllocationString(id, players), reference(id, players)));

    assert(ARRAYCOUNT(g_LevelInfoTable) == LEVEL_INFO_COUNT);
    assert(lvFindLevelInfo(LEVELID_MAX) != NULL);
    for (i = 0; i < ARRAYCOUNT(g_LevelInfoTable); i++)
    {
        entry = &g_LevelInfoTable[i];
        for (j = i + 1; j < ARRAYCOUNT(g_LevelInfoTable); j++)
            assert(entry->levelID != g_LevelInfoTable[j].levelID);
    }
    {
        static const s32 stages[] = {LEVELID_BUNKER2, LEVELID_ARCHIVES, LEVELID_CAVERNS, LEVELID_FACILITY, LEVELID_EGYPT};
        char filename[256];
        for (i = 0; i < ARRAYCOUNT(stages); i++)
        {
            struct LevelEntry *solo = lvFindLevelInfo(stages[i]);
            struct LevelEntry *mp = lvFindLevelInfo(stages[i] + 400);
            assert(solo && mp && strstr(mp->levelName, "(MP)"));
            assert(!strncmp(mp->setupFileName, "Ump_setup", 9));
            assert(!strcmp(solo->bg_seg_filename, mp->bg_seg_filename));
            assert(!strcmp(solo->bg_stan_filename, mp->bg_stan_filename));
            assert(solo->levelscale == mp->levelscale);
            assert(lvFindStageInfo(stages[i], 1) == solo);
            setupName(stages[i], 1, filename);
            assert(!strcmp(filename, solo->setupFileName));
            for (players = 2; players <= 4; players++)
            {
                assert(lvFindStageInfo(stages[i], players) == mp);
                setupName(stages[i], players, filename);
                assert(!strcmp(filename, mp->setupFileName));
            }
        }
        setupName(LEVELID_TEMPLE, 2, filename);
        assert(!strcmp(filename, "Ump_setupdishZ"));
        setupName(LEVELID_TITLE, 1, filename);
        assert(!filename[0]);
        entry = lvFindLevelInfo(LEVELID_TITLE);
        assert(entry && !strcmp(entry->levelName, "Title"));
        assert(!entry->setupFileName && !entry->bg_seg_filename && !entry->bg_stan_filename);
        assert(lvFindStageInfo(LEVELID_TITLE, 4) == entry);
        assert(lvFindStageInfo(LEVELID_DAM, 4) == lvFindLevelInfo(LEVELID_DAM));
        assert(lvFindStageInfo(1000, 1) == NULL);
    }

    /* Verify the game reads the field, including fallback for an unset stage. */
    entry = lvFindLevelInfo(LEVELID_DAM);
    saved = entry->memoryAllocationString;
    entry->memoryAllocationString = "-ma123";
    assert(!strcmp(lvGetMemoryAllocationString(LEVELID_DAM, 1), "-ma123"));
    assert(!strcmp(lvGetMemoryAllocationString(LEVELID_DAM, 4), "-ma123"));
    entry->memoryAllocationString = NULL;
    assert(!strcmp(lvGetMemoryAllocationString(LEVELID_DAM, 1), reference(LEVELID_DEFAULT, 1)));
    entry->memoryAllocationString = saved;
    puts("PASS: all 35 allocations preserved; solo/MP/title/default lookups and full catalog/MP setup selection verified.");
    return 0;
}
