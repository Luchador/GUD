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
    assert(ARRAYCOUNT(expected) == 37);
    for (i = 0; i < ARRAYCOUNT(expected); i++)
    {
        entry = lvFindLevelInfo(expected[i].id);
        assert(entry && entry->memoryAllocationString);
        assert(!strcmp(entry->memoryAllocationString, expected[i].tokens));
    }
    for (id = -1; id <= 1000; id++)
        for (players = 0; players <= 4; players++)
            assert(!strcmp(lvGetMemoryAllocationString(id, players), reference(id, players)));

    /* Allocation-only rows must not displace the BG catalog/placeholder or
     * introduce duplicate IDs that would shadow another stage's settings. */
    assert(STAGES_MAX == 38);
    assert(g_LevelInfoTable[STAGES_MAX - 1].levelID == LEVELID_MAX);
    for (i = 0; i < ARRAYCOUNT(g_LevelInfoTable); i++)
    {
        entry = &g_LevelInfoTable[i];
        assert((entry->bg_seg_filename != NULL) == (i < STAGES_MAX));
        for (j = i + 1; j < ARRAYCOUNT(g_LevelInfoTable); j++)
            assert(entry->levelID != g_LevelInfoTable[j].levelID);
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
    puts("PASS: all 37 allocations preserved; solo/MP/title/default lookups and BG catalog bounds verified.");
    return 0;
}
