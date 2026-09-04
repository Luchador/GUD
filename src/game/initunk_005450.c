#include <ultra64.h>
#include "bondtypes.h"
#include "initunk_005450.h"
#include "objecthandler.h"

/**
 * Called by stage load.
 * 
 * Initializes a fixed pool of ModelHitEntry records used while building per-model hit/collision traversal lists.
 * The pool is threaded as a doubly linked free list and consumed by objecthandler.c.
 */
void initModelHitEntryFreeList(void)
{
    s32 i;

    g_ModelHitFreeList = g_ModelHitEntries;
    g_ModelHitEntries[0].prev = NULL;

    for (i = 0; i < MODEL_HIT_ENTRY_POOL_SIZE - 1; i++)
    {
        g_ModelHitEntries[i].next = &g_ModelHitEntries[i + 1];
        g_ModelHitEntries[i + 1].prev = &g_ModelHitEntries[i];
    }

    g_ModelHitEntries[MODEL_HIT_ENTRY_POOL_SIZE - 1].next = NULL;
}
