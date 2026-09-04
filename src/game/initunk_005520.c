#include <ultra64.h>
#include <memp.h>
#include "model.h"
#include "initunk_005520.h"
#include "objecthandler.h"
#include "memp.h"

#define MODEL_SPARE_SLOTS          30
#define ANIM_MODEL_SPARE_SLOTS     10

#define MODEL_SPARE_RWDATALEN      0x14
#define ANIM_MODEL_SPARE_RWDATALEN 0x8c


void modelmgrResetSlotCounts(void)
{
    g_MaxAnimModelSlots = 0;
    g_MaxModelSlots = 0;
}


void modelmgrSetLevelResetting(bool resetting) 
{
    g_ModelIsLvResetting = resetting;
}


void modelmgrAllocateModelSlots(s32 numobjs)
{
    s32 i;

    g_MaxModelSlots = numobjs + MODEL_SPARE_SLOTS;
    
    g_ModelSlots = mempAllocBytesInBank(g_MaxModelSlots * sizeof(struct ModelSlot), MEMPOOL_STAGE);

    for (i = 0; i < g_MaxModelSlots; i++)
    {
        g_ModelSlots[i].unk08 = 0;

        if (i < numobjs)
        {
            g_ModelSlots[i].unk10 = NULL;
        }
        else
        {
            g_ModelSlots[i].unk10 = mempAllocBytesInBank(MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_ModelSlots[i].unk02 = MODEL_SPARE_RWDATALEN;
        }
    }
}


void modelmgrAllocateAnimModelSlots(s32 numanimated)
{
    s32 i;

    g_MaxAnimModelSlots = numanimated + ANIM_MODEL_SPARE_SLOTS;
    g_AnimModelSlots = mempAllocBytesInBank(g_MaxAnimModelSlots * (4 + sizeof(struct AnimModelSlot)), MEMPOOL_STAGE);

    for (i = 0; i < g_MaxAnimModelSlots; i++)
    {
        g_AnimModelSlots[i].unk08 = 0;

        if (i < numanimated)
        {
            g_AnimModelSlots[i].unk10 = NULL;
        }
        else
        {
            g_AnimModelSlots[i].unk10 = mempAllocBytesInBank(ANIM_MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_AnimModelSlots[i].unk02 = ANIM_MODEL_SPARE_RWDATALEN;
        }
    }
}
