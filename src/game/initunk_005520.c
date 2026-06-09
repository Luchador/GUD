#include <ultra64.h>
#include <memp.h>
#include "model.h"
#include "initunk_005520.h"
#include "objecthandler.h"
#include "memp.h"


void modelmgrResetSlotCounts(void)
{
    g_NumAnimModelSlots = 0;
    g_NumModelSlots = 0;
}


void modelmgrSetLevelResetting(bool resetting) 
{
    g_ModelIsLvResetting = resetting;
}

//this may be a file split

/**
 * NTSC address 0x7F005540.
*/
void sub_GAME_7F005540(s32 arg0)
{
    s32 temp_t6;
    s32 var_s1;

    g_NumModelSlots = arg0 + 30;
    
    g_ModelSlots = mempAllocBytesInBank(g_NumModelSlots * sizeof(struct ptr_1_s), MEMPOOL_STAGE);

    for (var_s1 = 0; var_s1 < g_NumModelSlots; var_s1++)
    {
        g_ModelSlots[var_s1].unk08 = 0;

        if (var_s1 < arg0)
        {
            g_ModelSlots[var_s1].unk10 = NULL;
        }
        else
        {
            g_ModelSlots[var_s1].unk10 = mempAllocBytesInBank(0x50, MEMPOOL_STAGE);
            g_ModelSlots[var_s1].unk02 = 0x14;
        }
    }
}


/**
 * NTSC address 0x7F005540.
*/
void sub_GAME_7F005624(s32 arg0)
{
    s32 temp_t6;
    s32 var_s1;

    g_NumAnimModelSlots = arg0 + 10;

    // mips2c says: g_AnimModelSlots = mempAllocBytesInBank(temp_t6 * 0xC0, 4);
    // however, the pointer is incremented by 0xbc in the loop below.
    g_AnimModelSlots = mempAllocBytesInBank(g_NumAnimModelSlots * (4 + sizeof(struct ptr_0_s)), MEMPOOL_STAGE);

    for (var_s1 = 0; var_s1 < g_NumAnimModelSlots; var_s1++)
    {
        g_AnimModelSlots[var_s1].unk08 = 0;

        if (var_s1 < arg0)
        {
            g_AnimModelSlots[var_s1].unk10 = NULL;
        }
        else
        {
            g_AnimModelSlots[var_s1].unk10 = mempAllocBytesInBank(0x230, MEMPOOL_STAGE);
            g_AnimModelSlots[var_s1].unk02 = 0x8c;
        }
    }
}
