#include <ultra64.h>
#include <bondconstants.h>
#include <memp.h>
#include "chr.h"
#include "chr_b.h"
#include "chrsetup.h"
#include "loadobjectmodel.h"
#include "aicommands2.h"
#include "dyn.h"
#include "memp.h"
#include <macro.h>


ChrRecord global_action_block_temp_buffer = { 0 };

WeaponProjectileModels weapon_models_for_weapon_load = {
    PROJECTILES_TYPE_KNIFE, PROJECTILES_TYPE_GRENADE,
    PROJECTILES_TYPE_REMOTE_MINE, PROJECTILES_TYPE_PROX_MINE,
    PROJECTILES_TYPE_TIMED_MINE, PROJECTILES_TYPE_ROCKET_ROUND,
    PROJECTILES_TYPE_GLAUNCH_ROUND, 0xFFFFFFFF
};

void init_obj_register_difficulty_vals(void)
{
    objectiveregisters1 = 0;
    g_AiAccuracyModifier = 1.0f;
    g_AiDamageModifier = 1.0f;
    g_AiHealthModifier = 1.0f;
    g_AiReactionSpeed = 1.0f;
    g_SeenBondRecentlyGuardCount = 0;
}


/** Allocates the phantom ChrRecords that host BACKGROUND AI lists.
 * Setup ailist IDs >= 0x1000 are level-logic scripts that run with no real
 * guard attached; the AI interpreter still needs a chr context, so this
 * allocates one blank record per background list from the stage pool
 * (copied from global_action_block_temp_buffer) and publishes the array as
 * g_ActiveChrs/g_ActiveChrsCount. Called once per setup load.
 */
void allocBackgroundAiChrs(void)
{
    s32 i;
    s32 count;

    g_ActiveChrs = NULL;
    g_ActiveChrsCount = 0;

    if (!g_CurrentSetup.ailists) 
    { 
        return; 
    }

    count = 0;

    // Count the number of background AI lists
    i = 0;

    while (g_CurrentSetup.ailists[i].ailist)
    {
        if (g_CurrentSetup.ailists[i].ID >= 0x1000)
        {
            g_ActiveChrsCount++;
        }

        i++;
    }

    if (g_ActiveChrsCount > 0)
    {
        ChrRecord tmp;

        // Allocate BG chrs
        g_ActiveChrs = mempAllocBytesInBank(ALIGN16_a(g_ActiveChrsCount * sizeof(ChrRecord)), MEMPOOL_STAGE);

        // Initialise BG chrs
        i = 0;

        while (g_CurrentSetup.ailists[i].ailist)
        {
            if (g_CurrentSetup.ailists[i].ID >= 0x1000)
            {
                tmp = global_action_block_temp_buffer;

                g_ActiveChrs[count] = tmp;

                g_ActiveChrs[count].chrnum = 0xFE;
                g_ActiveChrs[count].ailist = g_CurrentSetup.ailists[i].ailist;
                g_ActiveChrs[count].aioffset = 0;
                g_ActiveChrs[count].aireturnlist = -1;
                g_ActiveChrs[count].actiontype = ACT_NULL;
                count++;
            }

            i++;
        }
    }
}
