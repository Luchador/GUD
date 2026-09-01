#include <ultra64.h>
#include <memp.h>
#include "game_debug.h"
#include "ob.h"


void resetMemBank5(void)
{
    obBlankResourcesInBank5();
    mempResetBank(MEMPOOL_ME);
    obBlankResourcesLoadedInBank(MEMPOOL_ME);
}


void disableMemBank5(void)
{
    mempNullNextEntryInBank(MEMPOOL_ME);
    obBlankResourcesLoadedInBank(MEMPOOL_ME);
}
