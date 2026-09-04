#include <ultra64.h>
#include <memp.h>
#include "model.h"
#include "initunk_005520.h"
#include "objecthandler.h"
#include "memp.h"

#define MODEL_SPARE_RWDATALEN      0x14
#define ANIM_MODEL_SPARE_RWDATALEN 0x8c


void modelmgrResetSlotCounts(void)
{
    g_AnimatedModelSlotCount = 0;
    g_ModelSlotCount = 0;
}


void modelmgrSetLevelResetting(bool resetting) 
{
    g_ModelIsLvResetting = resetting;
}


void modelmgrAllocateModelSlots(s32 modelCount)
{
    s32 i;

    g_ModelSlotCount = modelCount + MODEL_SPARE_SLOT_COUNT;
    
    g_ModelSlots = mempAllocBytesInBank(g_ModelSlotCount * sizeof(ModelSlot), MEMPOOL_STAGE);

    for (i = 0; i < g_ModelSlotCount; i++)
    {
        g_ModelSlots[i].obj = NULL;

        if (i < modelCount)
        {
            g_ModelSlots[i].datas = NULL;
        }
        else
        {
            g_ModelSlots[i].datas = mempAllocBytesInBank(MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_ModelSlots[i].rwdatalen = MODEL_SPARE_RWDATALEN;
        }
    }
}


void modelmgrAllocateAnimModelSlots(s32 animatedModelCount)
{
    s32 i;

    g_AnimatedModelSlotCount = animatedModelCount + ANIM_MODEL_SPARE_SLOT_COUNT;
    g_AnimatedModelSlots = mempAllocBytesInBank(
        g_AnimatedModelSlotCount * ANIM_MODEL_ALLOCATION_SIZE, MEMPOOL_STAGE);

    for (i = 0; i < g_AnimatedModelSlotCount; i++)
    {
        g_AnimatedModelSlots[i].obj = NULL;

        if (i < animatedModelCount)
        {
            g_AnimatedModelSlots[i].datas = NULL;
        }
        else
        {
            g_AnimatedModelSlots[i].datas = mempAllocBytesInBank(ANIM_MODEL_SPARE_RWDATALEN * sizeof(u32), MEMPOOL_STAGE);
            g_AnimatedModelSlots[i].rwdatalen = ANIM_MODEL_SPARE_RWDATALEN;
        }
    }
}
