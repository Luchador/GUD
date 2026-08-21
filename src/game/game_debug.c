#include <ultra64.h>
#include <deb.h>
#include "ob.h"
#include <memp.h>
#include "game_debug.h"


void reset_mem_bank_5(void)
{
    obBlankResourcesInBank5();
    mempResetBank(MEMPOOL_ME);
    obBlankResourcesLoadedInBank(MEMPOOL_ME);
}


void sub_GAME_7F0D1A7C(void)
{
    mempNullNextEntryInBank(MEMPOOL_ME);
    obBlankResourcesLoadedInBank(MEMPOOL_ME);
}





