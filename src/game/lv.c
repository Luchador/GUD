#include <ultra64.h>
#include <math.h>
#include <os_extension.h>
#include <PR/libaudio.h>
#include <assets/font_dl.h>
#include <bondconstants.h>
#include <deb.h>
#include <memp.h>
#include <music.h>
#include <tlb_manage.h>
#include <fr.h>
#include <snd.h>
#include <ramrom.h>
#include <random.h>
#include <joy.h>
#include <token.h>
#include "alloc_window_pieces.h"
#include "bg.h"
#include "bgroomtrans.h"
#include "bondinv.h"
#include "bondtypes.h"
#include "bondview.h"
#include "bondview_r.h"
#include "cheat.h"
#include "chr.h"
#include "chrai.h"
#include "cleanup_objects.h"
#include "ejectedcartridges.h"
#include "explosion.h"
#include "frametiming.h"
#include "front.h"
#include "glass.h"
#include "gun.h"
#include "image_bank.h"
#include "initBondDATA.h"
#include "initBondDATAdefaults.h"
#include "initcheattext.h"
#include "initexplosioncasing.h"
#include "initguards.h"
#include "inititemslots.h"
#include "initmenus.h"
#include "initmttex.h"
#include "initobjects.h"
#include "initpathtablelinks.h"
#include "initunk_005450.h"
#include "initunk_005520.h"
#include "language.h"
#include "lv.h"
#include "mp_music.h"
#include "mpmenu.h"
#include "objective.h"
#include "player.h"
#include "prop.h"
#include "propobj.h"
#include "stan.h"
#include "textrelated.h"
#include "dyntex.h"
#include "viewport.h"
#include "vtxstore.h"


u32 *ptr_font_DL;
char ramrom_data_target[0x380];
s32 record_slot_num;
u8 * address_demo_loaded;
s32 g_CurrentStageToLoad = 0;
s32 musictrack1_playing = 0;
s32 g_ControlsLockedFlag = 0;
s32 g_ClockTimer = 0;
f32 g_GlobalTimerDelta = 0;

/**
 * Accumulated video field periods since stage load i.e. 60/s on NTSC and 50/s on PAL. This remains true no
 * matter how badly the frame rate dips. Freezes while the game is paused or controls are locked.
 */
s32 g_GlobalTimer = 0;

/**
 * Set to 0 on stage load, increments by one tick per rendered *frame*. This means the rate it increases depends on the frame rate.
 */
s32 g_GlobalTickCount = 0;


/*
* Selected difficulty mode.
*/
s32 g_SelectedDifficulty = DIFFICULTY_AGENT;

/**
 * Elapsed stage time in VI-rate ticks (60/s)
 * Resets on stage load; accumulates g_ClockTimer once per frame in lvTick.
 * Used for determining end of timed MP matches.
 */
s32 g_StageElapsedTicks = 0;

s32 g_MpTime = 36000;
s32 g_MpPoint = 10;
ALSoundState * g_MpSoundStateRelated = NULL;

/**
 * Elapsed stage time in seconds (ticks / VI refresh rate). Reset on stage load.
 * Read by Action Block commands AI_IFMissionTimeLess/GreaterThan
 */
f32 g_StageElapsedSeconds = 0.0;

/**
 * VI-rate ticks since boot. Unlike the stage clock this is never reset between stage loads.
 */
s32 g_SystemPowerTimeTicks = 0;

/**
 * Number of seconds since the system has powered up.
 */
f32 g_SystemPowerTimeSeconds = 0.0;

/**
 * Stops rendering the 3D world once the pause watch fills the screen.
 */
bool g_BgRenderEnabled = TRUE;

s32 g_LastImpactTexNum = 0xFFFFFFFF;

struct LvlMpUnknown *D_800483C8 = NULL;


extern u8* _fontdlSegmentRomStart;
extern u8* _fontdlSegmentRomEnd;

/* --- TEMP visibility profiler (read via framebuffer bars) --- */
u32 g_ProfBgTickCycles;
u32 g_ProfLvlTickCycles;     /* osGetCount delta across lvTick (game logic)    */
u32 g_ProfLvlRenderCycles;   /* osGetCount delta across lvRender (DL build)   */
u32 g_ProfBgRenderCycles;       /* osGetCount delta across bgSetupAndRender       */
u32 g_ProfChrTickCycles;
u32 g_ProfChrActionCycles;
u32 g_ProfChrTickCalls;
u32 g_ProfChrOnscreenCount;
u32 g_ProfChrMagicCount;
u32 g_ProfChrActionStandCount;
u32 g_ProfChrActionMoveCount;
u32 g_ProfChrActionCombatCount;
u32 g_ProfChrActionAnimCount;
u32 g_ProfChrActionOtherCount;
s32 g_ProfChrActionActive;
s32 g_ProfChrCurrentAction;
u32 g_ProfChrAiCycles;
u32 g_ProfChrAiCalls;
u32 g_ProfChrAiCommandCount;
u32 g_ProfChrCurrentAiCommandCount;
u32 g_ProfChrAiCommandCycles[AI_CMD_COUNT];
u32 g_ProfChrAiCommandCalls[AI_CMD_COUNT];
s32 g_ProfChrLosActive;
u32 g_ProfChrLosCycles;
u32 g_ProfChrLosLineCycles;
u32 g_ProfChrLosStanCycles;
u32 g_ProfChrLosRoomCycles;
u32 g_ProfChrLosPropCycles;
u32 g_ProfChrLosRecoveryCycles;
u32 g_ProfChrLosCalls;
u32 g_ProfChrLosPasses;
u32 g_ProfChrLosStanBlocks;
u32 g_ProfChrLosPropBlocks;
u32 g_ProfChrLosTileMismatches;
u32 g_ProfChrLosInvisibleCalls;
u32 g_ProfChrLosRooms;
u32 g_ProfChrLosProps;
u32 g_ProfChrLosCandidates;
u32 g_ProfChrLosEdges;
u32 g_ProfChrLosIntersections;
u32 g_ProfChrSlowestAiCycles;
u32 g_ProfChrSlowestAiCommandCount;
s32 g_ProfChrSlowestAiChrnum;
u32 g_ProfChrStateCycles;
u32 g_ProfChrStateStandCycles;
u32 g_ProfChrStateMoveCycles;
u32 g_ProfChrStateMagicCycles;
u32 g_ProfChrStateCombatCycles;
u32 g_ProfChrStateOtherCycles;
u32 g_ProfChrAnimPosCycles;
u32 g_ProfChrAnimCycles;
u32 g_ProfChrVisibilityCycles;
u32 g_ProfChrRoomCycles;
u32 g_ProfChrRootPositionCycles;
u32 g_ProfChrRootPositionCalls;
u32 g_ProfChrPositionValidateCycles;
u32 g_ProfChrPositionValidateCalls;
u32 g_ProfChrCollisionCycles;
u32 g_ProfChrCollisionCalls;
u32 g_ProfChrCollisionLineCycles;
u32 g_ProfChrCollisionLineCalls;
u32 g_ProfChrCollisionVolumeCycles;
u32 g_ProfChrCollisionVolumeCalls;
u32 g_ProfChrStationaryVolumeSkips;
u32 g_ProfChrGroundCycles;
u32 g_ProfChrGroundCalls;
u32 g_ProfChrGroundFollowCycles;
u32 g_ProfChrGroundFollowCalls;
s32 g_ProfChrPositionActive;
u32 g_ProfChrMatrixCycles;
u32 g_ProfChrMatrixBodyCycles;
u32 g_ProfChrMatrixHitChainCycles;
u32 g_ProfChrMatrixWeaponsCycles;
u32 g_ProfChrMatrixHatCycles;
u32 g_ProfChrMatrixFinalizeCycles;
u32 g_ProfChrSlowestCycles;
s32 g_ProfChrSlowestChrnum;
s32 g_ProfChrSlowestAction;
u32 g_ProfChrSlowestActionCycles;
u32 g_ProfChrSlowestCharAiCycles;
u32 g_ProfChrSlowestStateCycles;
u32 g_ProfChrSlowestAnimPosCycles;
u32 g_ProfChrSlowestAnimCycles;
u32 g_ProfChrSlowestVisibilityCycles;
u32 g_ProfChrSlowestRoomCycles;
u32 g_ProfChrSlowestRootPositionCycles;
u32 g_ProfChrSlowestRootPositionCalls;
u32 g_ProfChrSlowestPositionValidateCycles;
u32 g_ProfChrSlowestPositionValidateCalls;
u32 g_ProfChrSlowestCollisionCycles;
u32 g_ProfChrSlowestCollisionLineCycles;
u32 g_ProfChrSlowestCollisionVolumeCycles;
u32 g_ProfChrSlowestGroundCycles;
u32 g_ProfChrSlowestGroundFollowCycles;
u32 g_ProfChrSlowestMatrixCycles;
u32 g_ProfChrSlowestMatrixBodyCycles;
u32 g_ProfChrSlowestMatrixHitChainCycles;
u32 g_ProfChrSlowestMatrixWeaponsCycles;
u32 g_ProfChrSlowestMatrixHatCycles;
u32 g_ProfChrSlowestMatrixFinalizeCycles;
u32 g_ProfChrSlowestOnscreen;
u32 g_ProfObjTickCycles;
/* --- end profiler state --- */

bool lvGetBgRenderEnabled(void)
{
    return g_BgRenderEnabled;
}


void lvSetBgRenderEnabled(bool enabled)
{
    g_BgRenderEnabled = enabled;
}


void lvInit(void)
{
    s32 size;

    size = (s32)&_fontdlSegmentRomEnd - (s32)&_fontdlSegmentRomStart;
    ptr_font_DL = mempAllocBytesInBank(size, MEMPOOL_PERMANENT);
    romCopy(ptr_font_DL, &_fontdlSegmentRomStart, size);
}


void lvlPlayMusicTrack1(MUSIC_TRACKS track)
{
    musictrack1_playing = track;
    musicTrack1Play(musictrack1_playing);
}


void lvlMusicAppendPlaySoloDeathShort(void)
{
    musictrack1_playing = (musictrack1_playing + M_SHORT_SOLO_DEATH) % NUM_MUSIC_TRACKS;

    if (musictrack1_playing == M_NONE)
    {
        musictrack1_playing = M_SHORT_SOLO_DEATH;
    }

    musicTrack1Play(musictrack1_playing);
}


void lvlMusicAppendPlayEndTheme(void)
{
    musictrack1_playing = (musictrack1_playing + M_END_SOMETHING) % NUM_MUSIC_TRACKS;

    if (musictrack1_playing == M_NONE)
    {
        musictrack1_playing = M_END_SOMETHING;
    }

    musicTrack1Play(musictrack1_playing);
}


/**
 * Stage load method.
 * Title screen is handled as a special case.
 * First half of method resets stage and player values (including mutliplayer values) to defaults.
 * Second part loads stage data (init guards, init guard heads, etc).
 **/
void lvlStageLoad(s32 stage)
{
    s32 i;
    struct player_data *player_data;

    g_CurrentStageToLoad = stage;
    g_BgRenderEnabled = TRUE;
    g_ControlsLockedFlag = 0;
    g_ClockTimer = 1;
    g_GlobalTimerDelta = 1.0f;
    g_GlobalTickCount = 0;
    g_GlobalTimer = 0;
    g_StageElapsedTicks = 0;
    g_StageElapsedSeconds = 0.0f;
    g_MpSoundStateRelated = 0;

    sndSetScalerApplyVolumeAllSfxSlot(1.0f);
    musicTrack1ApplySeqpVol(VOLUME_MAX);
    musicTrack2ApplySeqpVol(VOLUME_MAX);
    musicTrack3ApplySeqpVol(VOLUME_MAX);
    sub_GAME_7F0C1364();
    modelmgrSetLevelResetting(TRUE);
    set_mt_tex_alloc();
    bullet_sparks_reset_all();
    texReset();
    load_font_tables();

    /* If title screen, initialize screen and folder setup.
    * Otherwise:
    * - enable cheats for player
    * - init watch
    * - reset some player values, like view height
    * - reset multiplayer stats
    */
    if (stage == LEVELID_TITLE)
    {
        init_menus_or_reset();
    }
    else
    {
        g_NewCheatUnlocked = 0;

        if ((g_CurrentStageToLoad != LEVELID_TITLE) && (g_StageElapsedTicks == 0) && (g_ClockTimer > 0))
        {
            if (g_AppendCheatSinglePlayer != 0)
            {
                s32 s0 = 1;

                for (s0 = 1; s0 != CHEAT_INVALID; s0++)
                {
                    if (g_CheatActivated[s0] && cheatIsEnemyRockets(s0))
                    {
                        cheatButtonTurnOnCheatForPlayers(s0);
                    }
                }
            }
        }

        bgLoadFile(g_CurrentStageToLoad);
        skySetStageNum(g_CurrentStageToLoad);

        optionsWatchInit();

        sub_GAME_7F0C11FC(stage);

        for (i=0; i<4; i++)
        {
            s32 s3;
            player_data = (struct player_data *)&g_playerPlayerData[i];

            if (getPlayerCount() == 1)
            {
                player_data->autoaim = 0;
                player_data->sight = 0;
                player_data->handicap = 1.0f;
                player_data->player_perspective_height = 1.0f;
            }
            else
            {

                // why is this looping from g_playerPlayerData again, this inner block
                // gets executed 16 times in multiplayer.
                for (s3 = 0; s3 < 4; s3++)
                {
                    if (get_scenario() == SCENARIO_LTK)
                    {
                        g_playerPlayerData[s3].handicap = 200.0f;
                    }
                    else
                    {
                        g_playerPlayerData[s3].handicap = get_player_mp_handicap(s3);
                    }

                    g_playerPlayerData[s3].player_perspective_height = get_player_mp_char_height(s3);
                }

                lvSetMpTime(get_mp_timelimit());
                lvSetMpPoint(get_mp_pointlimit());
                copy_aim_settings_to_playerdata();
            }

            player_data->time_other_players_on_screen = 0;
            player_data->damage_to_backside = 0;
            player_data->min_time_between_kills = S32_MAX;
            player_data->max_time_between_kills = 0;
            player_data->most_killed_one_life = 0;
            player_data->most_killed_one_time = 0;
            player_data->longest_inning = 0;
            player_data->shortest_inning = S32_MAX;
            player_data->order_out_in_yolt = 0;
            player_data->flag_counter = 0;
            player_data->distance_traveled = 0.0f;
            player_data->body_armor_pickups = 0.0f;

            for (s3 = 0; s3 < 4; s3++)
            {
                player_data->kill_counts[s3] = 0;
            }
        }
    }

    something_with_stage_objectives();
    mpwatchUnpauseGame();
    sub_GAME_7F09B820();
    initModelHitEntryFreeList();
    modelmgrResetSlotCounts();
    init_load_objpos_table();
    reinit_between_menus();
    init_sound_effects_registers();
    init_guards();
    bodiesReset(stage);
    proplvreset2(stage);
    alloc_explosion_smoke_casing_scorch_impact_buffers();
    alloc_shattered_window_pieces();
    
    g_DyntexWaterScrollS = 0.0f;
    g_DyntexWaterScrollT = 0.0f;
    g_DyntexWaterBlendPhase = 0.0f;

    initCheatTextBuffer();

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        disableOnscreenCheatText();
    }
    else
    {
        s32 player;

        init_path_table_links();
        init_ejected_cartridges();

        for (player = 0; player < getPlayerCount(); player++)
        {
            set_cur_player(player);
            reinit_gunheld_totaltime();
            init_player_BONDdata_stats();
            init_player_BONDdata();
            bondviewLoadSetupIntroSection();
            bviewPlayerBeginLife();
            sets_a_bunch_of_BONDdata_values_to_default();
            disableOnscreenCheatText();
        }

        set_cur_player(0);
    }

    /**
     * Leave stage load allocation mode.
     * From this point on, model creation should try to reuse existing slots.
     */
    modelmgrSetLevelResetting(FALSE);

    zbufDeallocate();
    viSetVideoMode(MD_NORMAL);
    lvSetControlsLockedFlag(FALSE);
}


s32 lvlGetCurrentStageToLoad(void)
{
    return g_CurrentStageToLoad;
}


/**
 * Sets the modifier values for the level being loaded.
 * This covers the enemy accuracy, reaction speed, and similar values.
 */
void lvlSetMultipliersForDifficulty(void)
{
    if (g_SelectedDifficulty == DIFFICULTY_AGENT)
    {
        f32 armorDiff = currentPlayerGetHealth() + currentPlayerGetArmor();
        f32 damageMultiplier = 1.0f;

        if (armorDiff <= 0.125f)
        {
            damageMultiplier = 0.5f;
        }
        else if (armorDiff <= 0.6f)
        {
            damageMultiplier = (((armorDiff - 0.125f) * 0.5f) / 0.47500002f) + 0.5f;
        }

        g_CctvAlarmDelayMult = 2.0f;
        g_CctvTakenDamageMult = 2.0f;
        g_AutogunPendingDamageTick = (0.5f * damageMultiplier);
        g_AutogunDamageScalar = (0.5f * damageMultiplier);
        g_AutogunTakenDamageMult = 2.0f;
        g_AiAccuracyModifier = DEFAULT_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = (DEFAULT_AGENT_AI_DAMAGE_MODIFIER * damageMultiplier);
        g_AiHealthModifier = 2.0f;
        g_SpExplosionDamageMult = (f32) (0.25f * damageMultiplier);
        difficulty = 1.5f;
        g_SoloAmmoMultiplier = DEFAULT_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_SECRET)
    {
        g_CctvAlarmDelayMult = 1.0f;
        g_CctvTakenDamageMult = 1.0f;
        g_AutogunPendingDamageTick = 0.75f;
        g_AutogunDamageScalar = 0.75f;
        g_AutogunTakenDamageMult = 1.0f;
        g_AiAccuracyModifier = DEFAULT_SECRET_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_SECRET_AGENT_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 0.75f;

        difficulty = 0.75f;

        g_SoloAmmoMultiplier = DEFAULT_SECRET_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_SECRET_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_00)
    {
        g_CctvAlarmDelayMult = 1.0f;
        g_CctvTakenDamageMult = 1.0f;
        g_AutogunPendingDamageTick = 1.0f;
        g_AutogunDamageScalar = 1.0f;
        g_AutogunTakenDamageMult = 1.0f;
        g_AiAccuracyModifier = DEFAULT_00_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_00_AGENT_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 1.0f;

        difficulty = 0.2f;

        g_SoloAmmoMultiplier = DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_00_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_007)
    {
        g_CctvAlarmDelayMult = 1.0f;
        g_CctvTakenDamageMult = 1.0f;
        g_AutogunPendingDamageTick = 1.0f;
        g_AutogunDamageScalar = 1.0f;
        g_AutogunTakenDamageMult = 1.0f;
        g_AiAccuracyModifier = DEFAULT_007_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_007_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 1.0f;
        difficulty = 1.0f;
        g_SoloAmmoMultiplier = DEFAULT_007_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_007_AI_REACTION_SPEED;
    }
}


/**
 * Graphics render method.
 * Also sets player max ammo if infinite ammo cheat is enabled.
 */
Gfx* lvRender(Gfx* DL)
{
    gSPSegment(DL++, SPSEGMENT_PHYSICAL, NULL);
    gSPSegment(DL++, SPSEGMENT_UNKNOWN, osVirtualToPhysical(ptr_font_DL));

    gSPDisplayList(DL++, &dlFastPipelineSetup);
    gSPDisplayList(DL++, &dlZBufferGeometry);

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        DL = viClearZBufCurrentPlayer(DL);
        DL = viSetupCurrentPlayerView(DL);
        gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0, (s16)viGetX(), (s16)viGetY());
        DL = menu_jump_constructor_handler(DL);
    }
    else
    {
        s32 i;
        s32 pcount;

        pcount = getPlayerCount();

        gSPClipRatio(DL++, FRUSTRATIO_2);

        for(i = 0; i < pcount; i++)
        {
            set_cur_player(get_nth_player_from_shuffled(i));

            viSetViewSize(g_CurrentPlayer->viewx, g_CurrentPlayer->viewy);
            viSetViewPosition(g_CurrentPlayer->viewleft, g_CurrentPlayer->viewtop);
            viSetFovY(g_CurrentPlayer->fovy);
            viSetAspect(g_CurrentPlayer->aspect);

            DL = viClearZBufCurrentPlayer(DL);
            DL = viSetupCurrentPlayerView(DL);
            DL = bviewRenderCameraView(DL);
            DL = viSetupScreensForNumPlayers(DL);
            DL = skyRender(DL);

            
            { /* TEMP profiler */
                u32 prof_t = osGetCount();
                bgTick();
                g_ProfBgTickCycles = osGetCount() - prof_t;
            }

            propsTick();

            chraiUpdateOnscreenPropCount();
            chrpropUpdateAutoaimTarget();
            chraiCheckUseHeldItems();

            if (bond_pressed_reload_activate() && bond_interact_object())
            {
                attempt_reload_item_in_hand(GUNRIGHT);
                attempt_reload_item_in_hand(GUNLEFT);
            }

            propsTickPlayer();

            { /* TEMP profiler */
                u32 prof_t = osGetCount();
                 DL = bgSetupAndRender(DL);
                g_ProfBgRenderCycles = osGetCount() - prof_t;
            }
            
            DL = weaponRenderTracers(DL);

            bullet_sparks_render_all(&DL, ZBUF_SURFACE);

            DL = glassRenderShards(DL);
            DL = explosionRenderFlyingParticles(DL);

            if (cheatIsActive(CHEAT_INFINITE_AMMO))
            {
                set_max_ammo_for_cur_player();
            }

            DL = bondviewRenderPlayerView(DL);
            DL = mp_watch_menu_display(DL);
        }
    }

    gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());

    return DL;
}


/**
 * Per-frame game tick called once per frame from the main loop in boss.c 
 * Advances the clock variables the game runs on: g_ClockTimer, g_GlobalTimerDelta, g_GlobalTimer, and g_GlobalTickCount.
 * Updates characters, sky, bullet casings, bullet sparks, explosion debris.
 * Handles multiplayer states (timers, end of match warning alarm, and concluding the match)
 */
void lvTick(void)
{
    tlbmanageResetCurrentEntriesCount();

    if (g_ControlsLockedFlag != 0)
    {
        g_ClockTimer = 0;
    }
    else if (checkGamePaused() != 0)
    {
        g_ClockTimer = 0;
    }
    else
    {
        g_ClockTimer = speedgraphframes;
        g_GlobalTickCount += 1;
    }

#ifdef VERSION_US
    g_GlobalTimerDelta = (f32) g_ClockTimer;
#else
    g_JP_GlobalTimerDelta = (f32) g_ClockTimer;
#ifdef VERSION_EU
    g_GlobalTimerDelta = g_JP_GlobalTimerDelta * 1.2f;
#else
    g_GlobalTimerDelta = g_JP_GlobalTimerDelta;
#endif
#endif
    g_GlobalTimer += g_ClockTimer;

    if ((g_CurrentStageToLoad != LEVELID_TITLE) && (g_StageElapsedTicks == 0) && (g_ClockTimer > 0))
    {
        if (g_AppendCheatSinglePlayer != 0)
        {
            s32 i;
            for (i = 1; i != CHEAT_INVALID; i++)
            {
                if (g_CheatActivated[i] && !cheatIsEnemyRockets(i))
                {
                    cheatButtonTurnOnCheatForPlayers(i);
                }
            }
        }
    }

    if ((getPlayerCount() >= 2) && (g_CurrentStageToLoad != LEVELID_TITLE))
    {
        if (get_mission_state() == MISSION_STATE_6)
        {
            s32 i;
            s32 mp_alive_count;
            s32 mp_player_field424_count;

            mp_alive_count = 0;
            mp_player_field424_count = 0;

            for (i = 0; i < getPlayerCount(); i++)
            {
                if (g_playerPointers[i]->bondstate == BONDSTATE_JUST_DIED || g_playerPointers[i]->bondstate == BONDSTATE_DEAD)
                {
                    mp_alive_count++;
                    if (g_playerPointers[i]->redbloodfinished)
                    {
                        mp_player_field424_count++;
                    }
                }
            }

            if ((mp_alive_count > 0) && (mp_alive_count == mp_player_field424_count))
            {
                set_missionstate(MISSION_STATE_1);
            }
        }

        if (g_MpTime > 0)
        {
            s32 current_time;
            s32 sp180;
            s32 i;
            current_time = g_StageElapsedTicks;
            sp180 = g_ClockTimer + g_StageElapsedTicks;

            if ((g_StageElapsedTicks < (g_MpTime - 3600)) && (sp180 >= (g_MpTime - 3600)))
            {
                for (i = 0; i < getPlayerCount(); i++)
                {
                    set_cur_player(i);
                    HUDMESSAGEBOTTOM("One minute left");
                }
            }

            // sound alarm when game is about to end (10 seconds before end)
            if ((sp180 >= (g_MpTime - 600)) && (g_MpSoundStateRelated == 0) && (lvGetControlsLockedFlag() == 0))
            {
                sndPlaySfx(g_musicSfxBufferPtr, ALARM1_SFX, &g_MpSoundStateRelated);
            }

            // stop alarm
            if (lvGetControlsLockedFlag())
            {
                if ((g_MpSoundStateRelated != NULL) && (sndGetPlayingState(g_MpSoundStateRelated) != 0))
                {
                    sndDeactivate(g_MpSoundStateRelated);
                }
            }

            if ((current_time < g_MpTime) && (sp180 >= g_MpTime))
            {
                mpCalculateAwards(FALSE);
            }
        }

        // when playing with a kill limit, g_MpPoint is not zero
        if ((g_MpPoint > 0) && (g_ClockTimer != 0))
        {
            s32 var_player_count1;
            s32 i;
            s32 mp_player_currently_in_dying_animation;
            s32 mp_players_over_point_limit;

            var_player_count1 = getPlayerCount();
            mp_player_currently_in_dying_animation = 0;
            mp_players_over_point_limit = 0;

            for (i = 0; i < var_player_count1; i++)
            {
                if ((g_playerPointers[i]->bondstate == BONDSTATE_JUST_DIED || g_playerPointers[i]->bondstate == BONDSTATE_DEAD) &&
                    (g_playerPointers[i]->redbloodfinished == FALSE || g_playerPointers[i]->deathanimfinished == FALSE || g_playerPointers[i]->colourfadetimemax60 >= 0.0f))
                {
                    mp_player_currently_in_dying_animation++;
                }

                if (get_points_for_mp_player(i) >= g_MpPoint)
                {
                    // counts players over kill limit
                    mp_players_over_point_limit++;
                }
            }

            if (mp_players_over_point_limit > 0)
            {
                if (mp_player_currently_in_dying_animation == 0)
                {
                    // end game after dying players are finished dying
                    mpCalculateAwards(FALSE);
                }
                else
                {
                    // this will cause the game to freeze players, to stop them from moving once game ended
                    mpwatchSetStopPlayFlag();
                }
            }
        }


        // YOLT scenario: end-of-game tracking.
        if ((get_scenario() == SCENARIO_YOLT) && (g_ClockTimer != 0))
        {
            s32 player_count;
            s32 killed_count;
            s32 not_dead_count;
            s32 fully_dead_total;
            s32 killed_total;
            s32 i;
            s32 j;

            player_count = getPlayerCount();
            killed_total = 0;
            fully_dead_total = 0;

            for (i = 0; i < player_count; i++)
            {
                killed_count = 0;
                not_dead_count = 0;

                for (j = 0; j < player_count; j++)
                {
                    if (g_playerPointers[j]->bondstate == BONDSTATE_ALIVE)
                    {
                        not_dead_count++;
                    }
                    killed_count += g_playerPlayerData[j].kill_counts[i];
                }

                if (killed_count >= 2)
                {
                    if (g_playerPlayerData[i].order_out_in_yolt == 0)
                    {
                        g_playerPlayerData[i].order_out_in_yolt = (u8) (not_dead_count + 1);
                    }

                    killed_total++;

                    if (g_playerPointers[i]->redbloodfinished
                        && g_playerPointers[i]->deathanimfinished
                        && g_playerPointers[i]->colourfadetimemax60 < 0.0f)
                    {
                        fully_dead_total++;
                    }
                }
            }

            if (fully_dead_total >= player_count - 1)
            {
                mpCalculateAwards(FALSE);
            }
            else if (killed_total >= player_count - 1)
            {
                mpwatchSetStopPlayFlag();
            }
        } // end YOLT
    }

    g_StageElapsedTicks = g_StageElapsedTicks + g_ClockTimer;
    g_StageElapsedSeconds = (f32) (g_StageElapsedTicks) / VI_REFRESH_RATE_F;
    g_SystemPowerTimeTicks = g_SystemPowerTimeTicks + g_ClockTimer;
    g_SystemPowerTimeSeconds = (f32) (g_SystemPowerTimeTicks) / VI_REFRESH_RATE_F;

    viSetUseZBuf(1);

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        cheatButtonSampleInput();
        menu_init();
        langTick();
    }
    else
    {
        sub_GAME_7F09BBBC();
        lvlSetMultipliersForDifficulty();
        updateRoomStatusFlags();
        dyntexWaterController();
        skyTick();
        bullet_sparks_update_all();
        update_bullet_casings();
        update_broken_windows();
        explosionUpdateFlyingParticles();
        chrpropTick();
        reset_all_music_slots();
        langTick();
    }
}


/**
 * Assumes a debug mode is present, and handles debug edit intro, debug stan edit, debug bond "view."
 * This updates the player viewport(s), and handles player movement.
 *
 * Multiplayer:
 * Updates distance_traveled and, depending on scenario, have_token_or_goldengun.
 */
void lvlViewMoveTick(void)
{
    s8 local_player_number;
    f32 temp_f0;
    f32 temp_f2;

    local_player_number = get_cur_playernum();
    cheatButtonSampleInput();

    bondviewMovePlayerUpdateViewport(joyGetStickX(local_player_number), joyGetStickY(local_player_number), joyGetButtons(local_player_number, ANY_BUTTON));

    mpwatchMenuTick();

    temp_f0 = g_CurrentPlayer->prop->pos.x - g_CurrentPlayer->bondprevpos.x;
    temp_f2 = g_CurrentPlayer->prop->pos.z - g_CurrentPlayer->bondprevpos.z;

    g_playerPerm->distance_traveled += sqrtf((temp_f0 * temp_f0) + (temp_f2 * temp_f2));

    if (get_scenario() == SCENARIO_TLD)
    {
        if (bondinvIsAliveWithFlag())
        {
            if (getCurrentPlayerWeaponId(GUNRIGHT) != ITEM_TOKEN)
            {
                currentPlayerEquipWeaponWrapper(GUNRIGHT, ITEM_TOKEN);

                if (g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state == GUN_ANIM_STATE_FIRE)
                {
                    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_SWITCH_LOWER;
                }
            }

            g_playerPerm->flag_counter += g_ClockTimer;
            g_playerPerm->have_token_or_goldengun = 1;
        }
        else
        {
            g_playerPerm->have_token_or_goldengun = 0;
        }

        return;
    }

    if (get_scenario() == SCENARIO_MWTGG)
    {
        if (bondinvHasGoldenGun())
        {
            g_playerPerm->have_token_or_goldengun = 1;
        }
        else
        {
            g_playerPerm->have_token_or_goldengun = 0;
        }

        return;
    }
}


void lvlUnloadStageTextData(void)
{
    if (g_MpSoundStateRelated != NULL)
    {
        if (sndGetPlayingState(g_MpSoundStateRelated) != AL_STOPPED)
        {
            sndDeactivate(g_MpSoundStateRelated);
        }
    }

    if (g_CurrentStageToLoad != LEVELID_TITLE)
    {
        langClearBank(langGetLangBankIndexFromStagenum(g_CurrentStageToLoad));
        set_favorite_weapon_for_every_player();
    }

    cheatDisableAllCheats();
    cleanupGuardData();
    cleanupObjectSounds();
    cleanupExplosions();
    cleanup_window_pieces();
    cleanupAlarms();
    cleanupObjects(g_CurrentStageToLoad);
    cleanupObjectives();
    cleanupSFXRelated();
    cleanupplayersoundrelated();
    set_missionstate_zero();
    bgCleanupRooms();
}


void lvSetControlsLockedFlag(bool locked)
{
    // Integrate R1 fix for Rumble Pak.
    if ((locked) && (g_ControlsLockedFlag == FALSE))
    {
        joyRumblePakStop();
    }

    g_ControlsLockedFlag = locked;
}


s32 lvGetControlsLockedFlag(void)
{
    return g_ControlsLockedFlag;
}


DIFFICULTY lvGetSelectedDifficulty(void)
{
    return g_SelectedDifficulty;
}


void lvSetSelectedDifficulty(DIFFICULTY diff)
{
    g_SelectedDifficulty = diff;
}


void lvSetMpTime(s32 timelimit)
{
    g_MpTime = timelimit;
}


void lvSetMpPoint(s32 pointlimit)
{
    g_MpPoint = pointlimit;
}


f32 lvGetStageElapsedSeconds(void)
{
    return g_StageElapsedSeconds;
}


f32 lvGetSystemPowerTimeSeconds(void)
{
    return g_SystemPowerTimeSeconds;
}


Gfx *lvDrawFrameRateDisplay(Gfx *gdl)
{
    s32 i;
    s32 topAiCommand1;
    s32 topAiCommand2;
    u32 topAiCycles1;
    u32 topAiCycles2;
    u32 topAiCalls1;
    u32 topAiCalls2;
    u32 aiCommandCycles;
    u32 chrOther;
    u32 chrProfiled;
    u32 matrixOther;
    u32 matrixProfiled;
    u32 slowOther;
    u32 slowMatrixOther;
    u32 slowMatrixProfiled;
    u32 slowProfiled;
    u32 lvlOther;
    u32 sub;

    static u32 fpsWindowStart = 0;
    static u32 fpsFrameCount = 0;
    static char fpsText[8] = "--";
    static u32 color = 0xFFFFFFFF;
    
    u32 now = osGetCount();
    s32 x;
    s32 y;
    s32 screenwidth;

    fpsFrameCount++;

    if (fpsWindowStart == 0)
    {
        fpsWindowStart = now;
    }
    else if ((u32)(now - fpsWindowStart) >= (u32)OS_USEC_TO_CYCLES(1000000))
    {
        /* Color code the readout: green ~60, yellow ~30..59, red below. */
        if(fpsFrameCount > 50)
        {
            color = 0x22FF22FF;
        }
        else if(fpsFrameCount > 25)
        {
            color = 0xFFFF22FF;
        }
        else
        {
            color = 0xFF0000FF;
        }

        sprintf(fpsText, "%d", fpsFrameCount);
        fpsFrameCount = 0;
        fpsWindowStart = now;
    }

    x = viGetViewLeft() + 14;
    y = viGetViewTop() + 2;
    screenwidth = (s32) viGetX();

    gdl = gfxSetup2DTextureMode(gdl);
    gdl = textRender(gdl, &x, &y, fpsText, ptrFontBankGothicChars, ptrFontBankGothic, color, screenwidth, viGetY(), 0, 0);

    { /* TEMP profiler readouts: name + raw osGetCount cycles per frame */
        static char profText[22][48];
        static const u32 profColor[22] = {
            0x00FFFFFF,  /* bgTick   - cyan      */
            0x4040FFFF,  /* lvTick   - blue      */
            0xFF3030FF,  /* lvRender- red       */
            0xFF8C00FF,  /* bg       - orange    */
            0xFFFF30FF,  /* objTick  - yellow     */
            0xB43CFFFF,  /* chrTick  - violet     */
            0xB43CFFFF,  /* character counts      */
            0xC060FFFF,  /* action populations    */
            0x30FF30FF,  /* chrAction - green     */
            0x30FF30FF,  /* AI/action state       */
            0x60FF60FF,  /* state categories      */
            0x30FFFFFF,  /* AI calls/commands      */
            0x30FFFFFF,  /* top AI command         */
            0x40FFFFFF,  /* anim/visibility/rooms  */
            0x40FFFFFF,  /* slow chr action detail  */
            0x40FFFFFF,  /* slow chr anim detail    */
            0x40FFFFFF,  /* slow chr root/validation*/
            0x40FFFFFF,  /* slow chr collision      */
            0x40FFFFFF,  /* slow chr remaining work */
            0x40FFFFFF,  /* slow chr matrix detail  */
            0xFF80FFFF,  /* slowest character      */
        };

        topAiCommand1 = -1;
        topAiCommand2 = -1;
        topAiCycles1 = 0;
        topAiCycles2 = 0;
        topAiCalls1 = 0;
        topAiCalls2 = 0;

        for (i = 0; i < AI_CMD_COUNT; i++)
        {
            aiCommandCycles = g_ProfChrAiCommandCycles[i];

            if (aiCommandCycles > topAiCycles1)
            {
                topAiCommand2 = topAiCommand1;
                topAiCycles2 = topAiCycles1;
                topAiCalls2 = topAiCalls1;
                topAiCommand1 = i;
                topAiCycles1 = aiCommandCycles;
                topAiCalls1 = g_ProfChrAiCommandCalls[i];
            }
            else if (aiCommandCycles > topAiCycles2)
            {
                topAiCommand2 = i;
                topAiCycles2 = aiCommandCycles;
                topAiCalls2 = g_ProfChrAiCommandCalls[i];
            }
        }

        chrProfiled = g_ProfChrActionCycles + g_ProfChrAnimPosCycles + g_ProfChrMatrixCycles;
        chrOther = (g_ProfChrTickCycles > chrProfiled) ? (g_ProfChrTickCycles - chrProfiled) : 0;
        matrixProfiled = g_ProfChrMatrixBodyCycles + g_ProfChrMatrixHitChainCycles + g_ProfChrMatrixWeaponsCycles + g_ProfChrMatrixHatCycles + g_ProfChrMatrixFinalizeCycles;
        matrixOther = (g_ProfChrMatrixCycles > matrixProfiled) ? (g_ProfChrMatrixCycles - matrixProfiled) : 0;
        slowProfiled = g_ProfChrSlowestActionCycles + g_ProfChrSlowestAnimPosCycles + g_ProfChrSlowestMatrixCycles;
        slowOther = (g_ProfChrSlowestCycles > slowProfiled) ? (g_ProfChrSlowestCycles - slowProfiled) : 0;
        slowMatrixProfiled = g_ProfChrSlowestMatrixBodyCycles + g_ProfChrSlowestMatrixHitChainCycles + g_ProfChrSlowestMatrixWeaponsCycles + g_ProfChrSlowestMatrixHatCycles + g_ProfChrSlowestMatrixFinalizeCycles;
        slowMatrixOther = (g_ProfChrSlowestMatrixCycles > slowMatrixProfiled) ? (g_ProfChrSlowestMatrixCycles - slowMatrixProfiled) : 0;

        sub = g_ProfBgTickCycles + g_ProfBgRenderCycles + g_ProfChrTickCycles + g_ProfObjTickCycles;  /* full chrTick */
        lvlOther = (g_ProfLvlRenderCycles > sub) ? (g_ProfLvlRenderCycles - sub) : 0;

        sprintf(profText[0], "BGTICK:%4uK",      (g_ProfBgTickCycles + 500) / 1000);
        sprintf(profText[1], "LVTICK:%4uK",      (g_ProfLvlTickCycles   + 500) / 1000);
        sprintf(profText[2], "LVRENDER:%4uK",    (lvlOther + 500) / 1000);
        sprintf(profText[3], "BGRENDER:%4uK",     (g_ProfBgRenderCycles        + 500) / 1000);
        sprintf(profText[4], "OBJTICK:%4uK",       (g_ProfObjTickCycles + 500) / 1000);
        sprintf(profText[5], "CHRTICK:%4uK",      (g_ProfChrTickCycles   + 500) / 1000);
        sprintf(profText[6], "CHR:%u ON:%u", g_ProfChrTickCalls, g_ProfChrOnscreenCount);
        sprintf(profText[7], "ACT S/M/G/C/A/O:%u/%u/%u/%u/%u/%u", g_ProfChrActionStandCount, g_ProfChrActionMoveCount, g_ProfChrMagicCount, g_ProfChrActionCombatCount, g_ProfChrActionAnimCount, g_ProfChrActionOtherCount);
        sprintf(profText[8], "CHRACT:%4uK", (g_ProfChrActionCycles + 500) / 1000);
        sprintf(profText[9], "AI/ST:%3u/%3uK", (g_ProfChrAiCycles + 500) / 1000, (g_ProfChrStateCycles + 500) / 1000);
        sprintf(profText[10], "S/M/G/C/O:%u/%u/%u/%u/%uK", (g_ProfChrStateStandCycles + 500) / 1000, (g_ProfChrStateMoveCycles + 500) / 1000, (g_ProfChrStateMagicCycles + 500) / 1000, (g_ProfChrStateCombatCycles + 500) / 1000, (g_ProfChrStateOtherCycles + 500) / 1000);
        sprintf(profText[11], "AI:%u CMD:%u", g_ProfChrAiCalls, g_ProfChrAiCommandCount);
        sprintf(profText[12], "TOP C%d:%uKx%u C%d:%uKx%u", topAiCommand1, (topAiCycles1 + 500) / 1000, topAiCalls1, topAiCommand2, (topAiCycles2 + 500) / 1000, topAiCalls2);
        sprintf(profText[13], "AN/VI/RM/MX/OT:%u/%u/%u/%u/%uK", (g_ProfChrAnimCycles + 500) / 1000, (g_ProfChrVisibilityCycles + 500) / 1000, (g_ProfChrRoomCycles + 500) / 1000, (g_ProfChrMatrixCycles + 500) / 1000, (chrOther + 500) / 1000);
        sprintf(profText[14], "MX B/H/W/T/F/O:%u/%u/%u/%u/%u/%uK", (g_ProfChrMatrixBodyCycles + 500) / 1000, (g_ProfChrMatrixHitChainCycles + 500) / 1000, (g_ProfChrMatrixWeaponsCycles + 500) / 1000, (g_ProfChrMatrixHatCycles + 500) / 1000, (g_ProfChrMatrixFinalizeCycles + 500) / 1000, (matrixOther + 500) / 1000);
        sprintf(profText[15], "SC A/AI/ST:%u/%u/%uK", (g_ProfChrSlowestActionCycles + 500) / 1000, (g_ProfChrSlowestCharAiCycles + 500) / 1000, (g_ProfChrSlowestStateCycles + 500) / 1000);
        sprintf(profText[16], "SC AP/AN/VI/RM:%u/%u/%u/%uK", (g_ProfChrSlowestAnimPosCycles + 500) / 1000, (g_ProfChrSlowestAnimCycles + 500) / 1000, (g_ProfChrSlowestVisibilityCycles + 500) / 1000, (g_ProfChrSlowestRoomCycles + 500) / 1000);
        sprintf(profText[17], "SC RT/PV:%uKx%u/%uKx%u", (g_ProfChrSlowestRootPositionCycles + 500) / 1000, g_ProfChrSlowestRootPositionCalls, (g_ProfChrSlowestPositionValidateCycles + 500) / 1000, g_ProfChrSlowestPositionValidateCalls);
        sprintf(profText[18], "SC CD/LN/VL:%u/%u/%uK", (g_ProfChrSlowestCollisionCycles + 500) / 1000, (g_ProfChrSlowestCollisionLineCycles + 500) / 1000, (g_ProfChrSlowestCollisionVolumeCycles + 500) / 1000);
        sprintf(profText[19], "SC GD/GF/MX/OT:%u/%u/%u/%uK", (g_ProfChrSlowestGroundCycles + 500) / 1000, (g_ProfChrSlowestGroundFollowCycles + 500) / 1000, (g_ProfChrSlowestMatrixCycles + 500) / 1000, (slowOther + 500) / 1000);
        sprintf(profText[20], "SCMX B/H/W/T/F/O:%u/%u/%u/%u/%u/%uK", (g_ProfChrSlowestMatrixBodyCycles + 500) / 1000, (g_ProfChrSlowestMatrixHitChainCycles + 500) / 1000, (g_ProfChrSlowestMatrixWeaponsCycles + 500) / 1000, (g_ProfChrSlowestMatrixHatCycles + 500) / 1000, (g_ProfChrSlowestMatrixFinalizeCycles + 500) / 1000, (slowMatrixOther + 500) / 1000);
        sprintf(profText[21], "SLOW C%d A%d:%uK ON:%u", g_ProfChrSlowestChrnum, g_ProfChrSlowestAction, (g_ProfChrSlowestCycles + 500) / 1000, g_ProfChrSlowestOnscreen);

        g_ProfChrTickCycles = 0;
        g_ProfChrActionCycles = 0;
        g_ProfChrTickCalls = 0;
        g_ProfChrOnscreenCount = 0;
        g_ProfChrMagicCount = 0;
        g_ProfChrActionStandCount = 0;
        g_ProfChrActionMoveCount = 0;
        g_ProfChrActionCombatCount = 0;
        g_ProfChrActionAnimCount = 0;
        g_ProfChrActionOtherCount = 0;
        g_ProfChrActionActive = 0;
        g_ProfChrCurrentAction = ACT_NULL;
        g_ProfChrAiCycles = 0;
        g_ProfChrAiCalls = 0;
        g_ProfChrAiCommandCount = 0;
        g_ProfChrCurrentAiCommandCount = 0;
        g_ProfChrLosActive = 0;
        g_ProfChrLosCycles = 0;
        g_ProfChrLosLineCycles = 0;
        g_ProfChrLosStanCycles = 0;
        g_ProfChrLosRoomCycles = 0;
        g_ProfChrLosPropCycles = 0;
        g_ProfChrLosRecoveryCycles = 0;
        g_ProfChrLosCalls = 0;
        g_ProfChrLosPasses = 0;
        g_ProfChrLosStanBlocks = 0;
        g_ProfChrLosPropBlocks = 0;
        g_ProfChrLosTileMismatches = 0;
        g_ProfChrLosInvisibleCalls = 0;
        g_ProfChrLosRooms = 0;
        g_ProfChrLosProps = 0;
        g_ProfChrLosCandidates = 0;
        g_ProfChrLosEdges = 0;
        g_ProfChrLosIntersections = 0;
        g_ProfChrSlowestAiCycles = 0;
        g_ProfChrSlowestAiCommandCount = 0;
        g_ProfChrSlowestAiChrnum = -1;
        g_ProfChrStateCycles = 0;
        g_ProfChrStateStandCycles = 0;
        g_ProfChrStateMoveCycles = 0;
        g_ProfChrStateMagicCycles = 0;
        g_ProfChrStateCombatCycles = 0;
        g_ProfChrStateOtherCycles = 0;
        g_ProfChrAnimPosCycles = 0;
        g_ProfChrAnimCycles = 0;
        g_ProfChrVisibilityCycles = 0;
        g_ProfChrRoomCycles = 0;
        g_ProfChrRootPositionCycles = 0;
        g_ProfChrRootPositionCalls = 0;
        g_ProfChrPositionValidateCycles = 0;
        g_ProfChrPositionValidateCalls = 0;
        g_ProfChrCollisionCycles = 0;
        g_ProfChrCollisionCalls = 0;
        g_ProfChrCollisionLineCycles = 0;
        g_ProfChrCollisionLineCalls = 0;
        g_ProfChrCollisionVolumeCycles = 0;
        g_ProfChrCollisionVolumeCalls = 0;
        g_ProfChrStationaryVolumeSkips = 0;
        g_ProfChrGroundCycles = 0;
        g_ProfChrGroundCalls = 0;
        g_ProfChrGroundFollowCycles = 0;
        g_ProfChrGroundFollowCalls = 0;
        g_ProfChrPositionActive = 0;
        g_ProfChrMatrixCycles = 0;
        g_ProfChrMatrixBodyCycles = 0;
        g_ProfChrMatrixHitChainCycles = 0;
        g_ProfChrMatrixWeaponsCycles = 0;
        g_ProfChrMatrixHatCycles = 0;
        g_ProfChrMatrixFinalizeCycles = 0;
        g_ProfChrSlowestCycles = 0;
        g_ProfChrSlowestChrnum = -1;
        g_ProfChrSlowestAction = ACT_NULL;
        g_ProfChrSlowestActionCycles = 0;
        g_ProfChrSlowestCharAiCycles = 0;
        g_ProfChrSlowestStateCycles = 0;
        g_ProfChrSlowestAnimPosCycles = 0;
        g_ProfChrSlowestAnimCycles = 0;
        g_ProfChrSlowestVisibilityCycles = 0;
        g_ProfChrSlowestRoomCycles = 0;
        g_ProfChrSlowestRootPositionCycles = 0;
        g_ProfChrSlowestRootPositionCalls = 0;
        g_ProfChrSlowestPositionValidateCycles = 0;
        g_ProfChrSlowestPositionValidateCalls = 0;
        g_ProfChrSlowestCollisionCycles = 0;
        g_ProfChrSlowestCollisionLineCycles = 0;
        g_ProfChrSlowestCollisionVolumeCycles = 0;
        g_ProfChrSlowestGroundCycles = 0;
        g_ProfChrSlowestGroundFollowCycles = 0;
        g_ProfChrSlowestMatrixCycles = 0;
        g_ProfChrSlowestMatrixBodyCycles = 0;
        g_ProfChrSlowestMatrixHitChainCycles = 0;
        g_ProfChrSlowestMatrixWeaponsCycles = 0;
        g_ProfChrSlowestMatrixHatCycles = 0;
        g_ProfChrSlowestMatrixFinalizeCycles = 0;
        g_ProfChrSlowestOnscreen = 0;
        g_ProfObjTickCycles = 0;

        for (i = 0; i < AI_CMD_COUNT; i++)
        {
            g_ProfChrAiCommandCycles[i] = 0;
            g_ProfChrAiCommandCalls[i] = 0;
        }

        for (i = 0; i < 22; i++)
        {
            x = viGetViewLeft() + 14;
            y = viGetViewTop() + 14 + (i * 10);
            gdl = textRender(gdl, &x, &y, profText[i], ptrFontBankGothicChars, ptrFontBankGothic, profColor[i], screenwidth, viGetY(), 0, 0);
        }
    }

    return gdl;
}
