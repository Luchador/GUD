#include <ultra64.h>
#include <bondconstants.h>
#include <memp.h>
#include "alloc_window_pieces.h"
#include "glass.h"


void alloc_shattered_window_pieces(void)
{
    s32 i;
    s32 level = lvlGetCurrentStageToLoad();

    SHATTERED_WINDOW_PIECES_BUFFER_LEN = (200 / getPlayerCount());

    ptr_shattered_window_pieces = mempAllocBytesInBank(((SHATTERED_WINDOW_PIECES_BUFFER_LEN * sizeof(s_shattered_window_piece)) + 0xF) & ~0xF, MEMPOOL_STAGE);

    for(i=0; i<SHATTERED_WINDOW_PIECES_BUFFER_LEN; i++)
    {
        ptr_shattered_window_pieces[i].active = 0;
    }

    g_NextShardNum = 0;
}
