#include <ultra64.h>
#include <bondtypes.h>
#include "cheat.h"
#include "chr.h"
#include "chr_b.h"
#include "chrobjdata.h"
#include "objecthandler.h"
#include "model.h"

s32 load_body_head_if_not_loaded(s32 model)
{
    if (CitemZ_entries[model].header->RootNode == 0)
    {
        fileLoad(CitemZ_entries[model].header, CitemZ_entries[model].filename);
        return 1;
    }
    return 0;
}

// # This file is chrlv.c

/**
 * Address 0x7F0232E8 (VERSION_US)
 * Address 0x7F0235D8 (other version)
 * Not a very descriptive name for a function. What it does is load Bond's model or those
 * for spawning guards. It is not used for guards that spawn at level loading.
*/
struct Model *makeonebody(s32 body, s32 head, struct ModelFileHeader *bodyHeader, struct ModelFileHeader *headHeader, s32 sunglasses, struct Model *model)
{
    f32 scale;
    f32 pov;
    s32 opcode;
    ModelRwData_SwitchRecord *rwdata;

    scale = CitemZ_entries[body].scale * 0.10000001f;
    opcode = 0;
    pov = CitemZ_entries[body].pov;

    if (cheatIsActive(CHEAT_DK_MODE))
    {
        scale *= 0.8f;
    }

    if (bodyHeader->RootNode == 0)
    {
        fileLoad(bodyHeader, CitemZ_entries[body].filename);
    }

    modelCalculateRwDataLen(bodyHeader);

    if ((CitemZ_entries[body].hasHead == 0) && (head >= 0))
    {
        opcode = &bodyHeader->Switches[4]->Opcode;
        if (opcode != 0)
        {
            if (headHeader->RootNode == 0)
            {
                fileLoad(headHeader, CitemZ_entries[head].filename);
            }

            modelCalculateRwDataLen(headHeader);

            bodyHeader->numRecords += headHeader->numRecords;
        }
    }

    if (model == 0)
    {
        model = modelmgrInstantiateModelWithAnim(bodyHeader);
    }

    if (model != 0)
    {
        modelSetScale(model, scale);
        modelSetAnimTranslationScale(model, pov);

        if ((headHeader != 0) && (CitemZ_entries[body].hasHead == 0))
        {
            bodyHeader->numRecords -= headHeader->numRecords;
            modelAttachHead(model, opcode, headHeader);

            if ((sunglasses == 0) && ((s32) headHeader->numSwitches > 0))
            {
                if (headHeader->Switches[0] != 0)
                {
                    rwdata = &modelGetNodeRwData(model, headHeader->Switches[0])->Switch;
                    rwdata->visible = 0;
                }
            }
        }
    }

    return model;
}


Model *setup_chr_instance(int body,int head,ModelFileHeader *body_header, ModelFileHeader *head_header,int sunglasses)
{
  return makeonebody(body,head,body_header,head_header,sunglasses,0x0);
}
