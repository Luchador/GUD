#include <ultra64.h>
#include <PR/libaudio.h>
#include <fr.h>
#include <snd.h>
#include "bondtypes.h"
#include "bondview.h"
#include "chrai.h"
#include "chr.h"
#include "cleanup.h"
#include "explosion.h"
#include "glass.h"
#include "loadobjectmodel.h"
#include "objective_status.h"
#include "player.h"
#include "propobj.h"


void cleanupGuardData(void)
{
    s32 i;

    for (i = 0; i < g_NumChrSlots; i++)
    {
        if (g_ChrSlots[i].model != NULL)
        {
            chrpropCleanupForRemoval(g_ChrSlots[i].prop);
            chrpropDelist(g_ChrSlots[i].prop);
            chrpropDisable(g_ChrSlots[i].prop);
            chrpropFree(g_ChrSlots[i].prop);
        }
    }
}


void cleanupObjectSounds(void)
{
    s32 i;

    for (i = 0; i < SFX_RELATED_LEN; i++)
    {
        if (sfx_related[i].state != NULL && sndGetPlayingState(sfx_related[i].state) != AL_STOPPED)
        {
            sndDeactivate(sfx_related[i].state);
        }
    }
}


void cleanupAlarms(void)
{
    alarmDeactivate();
    check_deactivate_gas_sound();
}


void cleanupObjects(s32 stage)
{
    u32 *obj = (u32 *)g_CurrentSetup.propDefs;

    if (obj != NULL)
    {
        while ((u8)obj[0] != PROPDEF_END)
        {
            switch ((u8)obj[0])
            {
                case PROPDEF_DOOR:
                case PROPDEF_PROP:
                case PROPDEF_KEY:
                case PROPDEF_ALARM:
                case PROPDEF_CCTV:
                case PROPDEF_MAGAZINE:
                case PROPDEF_COLLECTABLE:
                case PROPDEF_MONITOR:
                case PROPDEF_MULTI_MONITOR:
                case PROPDEF_RACK:
                case PROPDEF_AUTOGUN:
                case PROPDEF_HAT:
                case PROPDEF_AMMO:
                case PROPDEF_ARMOUR:
                case PROPDEF_GAS_RELEASING:
                case PROPDEF_VEHICLE:
                case PROPDEF_AIRCRAFT:
                case PROPDEF_UNK41:
                case PROPDEF_GLASS:
                case PROPDEF_SAFE:
                case PROPDEF_TANK:
                case PROPDEF_TINTED_GLASS:
                    objFreePermanently((ObjectRecord *)obj, 1);
                    break;
                case PROPDEF_DOOR_SCALE:
                case PROPDEF_GUARD:
                case PROPDEF_LINK:
                case PROPDEF_DEBRIS:
                case PROPDEF_UNK16:
                case PROPDEF_GUARD_ATTRIBUTE:
                case PROPDEF_SWITCH:
                case PROPDEF_TAG:
                case PROPDEF_OBJECTIVE_START:
                case PROPDEF_OBJECTIVE_END:
                case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
                case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
                case PROPDEF_OBJECTIVE_FAIL_CONDITION:
                case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
                case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
                case PROPDEF_OBJECTIVE_PHOTOGRAPH:
                case PROPDEF_OBJECTIVE_NULL:
                case PROPDEF_OBJECTIVE_ENTER_ROOM:
                case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
                case PROPDEF_OBJECTIVE_COPY_ITEM:
                case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT:
                case PROPDEF_RENAME:
                case PROPDEF_LOCK_DOOR:
                case PROPDEF_SAFE_ITEM:
                case PROPDEF_CAMERAPOS:
                    break;
            }

            obj += sizepropdef(obj);
        }
    }
}


void cleanupObjectives(void)
{
    s32 i;

    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        if (objective_ptrs[i] != NULL && (objective_ptrs[i]->unkD & 1)
                && objectiveStatuses[i] != OBJECTIVESTATUS_FAILED)
        {
            objectiveStatuses[i] = OBJECTIVESTATUS_COMPLETE;
        }
    }
}


void cleanupSFXRelated(void)
{
    if (g_TankSfxState[0] != NULL && sndGetPlayingState(g_TankSfxState[0]) != AL_STOPPED)
    {
        sndDeactivate(g_TankSfxState[0]);
    }

    if (g_TankSfxState[1] != NULL && sndGetPlayingState(g_TankSfxState[1]) != AL_STOPPED)
    {
        sndDeactivate(g_TankSfxState[1]);
    }
}


void cleanupExplosions(void)
{
    s32 i;

    g_NumExplosionEntries = 0;
    viShake(0.0f);

    if (g_ExplosionBuffer != NULL)
    {
        for (i = 0; i < EXPLOSION_BUFFER_LEN; i++)
        {
            if (g_ExplosionBuffer[i].prop != NULL)
            {
                chrpropDelist(g_ExplosionBuffer[i].prop);
                chrpropDisable(g_ExplosionBuffer[i].prop);
                chrpropFree(g_ExplosionBuffer[i].prop);
                g_ExplosionBuffer[i].prop = NULL;
            }
        }
    }

    if (g_SmokeBuffer != NULL)
    {
        for (i = 0; i < SMOKE_BUFFER_LEN; i++)
        {
            if (g_SmokeBuffer[i].prop != NULL)
            {
                chrpropDelist(g_SmokeBuffer[i].prop);
                chrpropDisable(g_SmokeBuffer[i].prop);
                chrpropFree(g_SmokeBuffer[i].prop);
                g_SmokeBuffer[i].prop = NULL;
            }
        }
    }
}


void cleanup_window_pieces(void)
{
    s32 i;

    for (i = 0; i < SHATTERED_WINDOW_PIECES_BUFFER_LEN; i++)
    {
        ptr_shattered_window_pieces[i].active = 0;
    }
}


void cleanupplayersoundrelated(void)
{
    s32 playerIndex;
    s32 handIndex;

    for (playerIndex = 0; playerIndex < getPlayerCount(); playerIndex++)
    {
        for (handIndex = 0; handIndex < 2; handIndex++)
        {
            struct hand *hand = &g_playerPointers[playerIndex]->hands[handIndex];

            if (hand->audioHandle != NULL && sndGetPlayingState(hand->audioHandle))
            {
                sndDeactivate(hand->audioHandle);
            }
        }
    }
}
