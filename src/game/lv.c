#include <ultra64.h>
#include <math.h>
#include <os_extension.h>
#include <PR/libaudio.h>
#include <assets/font_dl.h>
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
#include "debugmenu_handler.h"
#include "lv.h"
#include "language.h"
#include "initcheattext.h"
#include "front.h"
#include "bondinv.h"
#include "player.h"
#include "propobj.h"
#include "cleanup_objects.h"
#include "explosion.h"
#include "chrai.h"
#include "mp_music.h"
#include "initunk_005520.h"
#include "initmttex.h"
#include "glass.h"
#include "image_bank.h"
#include "textrelated.h"
#include "initmenus.h"
#include "cheat.h"
#include "bg.h"
#include "objective.h"
#include "mpmenu.h"
#include "vtxstore.h"
#include "initunk_005450.h"
#include "initobjects.h"
#include "initguards.h"
#include "prop.h"
#include "initexplosioncasing.h"
#include "alloc_window_pieces.h"
#include "initunk_007290.h"
#include "initcheattext.h"
#include "initpathtablelinks.h"
#include "ejectedcartridges.h"
#include "inititemslots.h"
#include "initBondDATA.h"
#include "bondview.h"
#include "bondview_r.h"
#include "initBondDATAdefaults.h"
#include "viewport.h"
#include "stan.h"
#include "gun.h"
#include "debug_camera.h"
#include "mp_music.h"
#include "bgroomtrans.h"
#include "unk_092E50.h"
#include "frametiming.h"
#include "chr.h"

// bss
//CODE.bss:8008C260
u32 *ptr_font_DL;
//CODE.bss:8008C264
s32 dword_CODE_bss_8008C264;
s32 dword_CODE_bss_8008C268;
s32 dword_CODE_bss_8008C26C;
//CODE.bss:8008C270
char ramrom_data_target[0x380];
//CODE.bss:8008C5F0
s32 record_slot_num;
//CODE.bss:8008C5F4
u8 * address_demo_loaded;
//CODE.bss:8008C5F8
s32 dword_CODE_bss_8008C5F8;


// data
//D:80048360
s32 lvl_c_debug_notice_list = 0;
//D:80048364
s32 g_CurrentStageToLoad = 0;
//D:80048368
f32 D_80048368 = 1.0;
//D:8004836C
s32 musictrack1_playing = 0;
//D:80048370
s32 g_ControlsLockedFlag = 0;
//D:80048374
s32 g_ClockTimer = 0;


#if defined (BUGFIX_R1)
// addresses updated, per build\ge007.j.map
// 800483a8
f32 g_JP_GlobalTimerDelta = 0;
// 800483ac
s32 g_GlobalTimer = 0;
// 800483b0
s32 D_80048380 = 0;
// 800483b4
f32 g_GlobalTimerDelta = 0;
#else
//D:80048378
f32 g_GlobalTimerDelta = 0;
//D:8004837C
s32 g_GlobalTimer = 0;
//D:80048380
s32 D_80048380 = 0;
//D:80048384
#endif
/*
* Selected difficulty mode.
* 0x80048384
*/
s32 g_SelectedDifficulty = DIFFICULTY_AGENT;

//D:80048388
s32 D_80048388 = 0;
//D:8004838C
s32 D_8004838C = 0;
//D:80048390
s32 D_80048390 = 0;
//D:80048394
s32 D_80048394 = 0;

/**
 * Address 0x80048398.
 */
s32 g_MpTime = 0x8CA0;

/**
 * Address 0x8004839C.
 */
s32 g_MpPoint = 0xA;

/**
 * Address 0x800483A0.
 */
ALSoundState * g_MpSoundStateRelated = NULL;

/**
 * Address 0x800483A4.
 */
f32 g_CurrentMultiPlayerSec = 0.0;

//D:800483A8
s32 D_800483A8 = 0;

/**
 * Address 0x800483AC.
 */
f32 g_CurrentMultiPlayerMin = 0.0;

//D:800483B0
s32 D_800483B0 = 0;

/**
 * Address 0x800483B4.
 */
f32 g_StageTimeSec = 0;

//D:800483B8
s32 D_800483B8 = 0;

/**
 * Power on time in seconds.
 * Address 0x800483BC.
 */
f32 g_PowerOnTimeSec = 0;

/**
 * Debug variable, seems to track whether user input has changed since
 * the last time the method was entered.
 *
 * Addres 0x800483C0.
 */
s32 D_800483C0 = 1;

//D:800483C4
s32 D_800483C4 = 0xFFFFFFFF;

//D:800483C8
struct LvlMpUnknown *D_800483C8 = NULL;

/**
* Debug variable, something to do with portals.
* Address 0x800483CC.
*/
s32 g_DebugPortalsD_800483CC = 0;

/**
 * Input buffer, used in debug portal method. Might be array.
 * Address 0x800483D0.
 */
s32 g_DebugPortalsInputBuffer[5] = { 0 };

/**
 * Something debug related in the MP manage method.
 * Index to play sound effect.
 * Address 0x800483E4.
 */
s16 g_DebugMpGameSoundFxIndex = 0;

// unused address padding
s16 D_800483E6 = 0;


extern u8* _fontdlSegmentRomStart;
extern u8* _fontdlSegmentRomEnd;


// forward declarations

Gfx * lvlPortalDebug7F0BDF10(Gfx * arg0);

// end forward declarations

s32 sub_GAME_7F0BD8F0(void)
{
    return D_800483C0;
}

void sub_GAME_7F0BD8FC(s32 arg0)
{
    D_800483C0 = arg0;
}

void lvInit(void)
{
    s32 size;

    debTryAdd(&lvl_c_debug_notice_list, "lv_c_debug");
    size = (s32)&_fontdlSegmentRomEnd - (s32)&_fontdlSegmentRomStart;
    lvl_c_debug_notice_list = 1;
    ptr_font_DL = mempAllocBytesInBank(size, MEMPOOL_PERMANENT);
    romCopy(ptr_font_DL, &_fontdlSegmentRomStart, size);
}

/**
 * Unreferenced.
 */
void lvlPlayRandomMusicTrack1(void)
{
    musictrack1_playing = randomGetNext() % 0x3dU + M_INTRO;
    musicTrack1Play(musictrack1_playing);
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
 * Unreferenced.
 */
void lvlMusicPlayStageTrackOrRandom(void)
{
    lvlPlayMusicTrack1(getmusictrack_or_randomtrack(g_CurrentStageToLoad));
}

/**
 * Stage load method.
 * Title screen is handled as a special case.
 * First half of method resets stage and player values (including mutliplayer values) to defaults.
 * Second part loads stage data (init guards, init guard heads, etc).
 *
 * NTSC Address: 0x7F0BDAB0.
 * NTSC-J Address: 7F0BE660.
 * PAL Address: 7F0BCE60.
 **/
void lvlStageLoad(s32 stage)
{
    s32 i;
    struct player_data *player_data;

    g_CurrentStageToLoad = stage;

    // this if block pushes where g_CurrentStageToLoad gets loaded to the
    // top of the method. Maybe a debug log about which level is loaded.
    if(0)
    {
        #ifdef DEBUG
        // removed
        #endif
    }

    D_800483C0 = 1;
    g_ControlsLockedFlag = 0;
    g_ClockTimer = 1;

#ifdef VERSION_US
    g_GlobalTimerDelta = 1.0f;
#endif
#if defined(VERSION_JP) || defined(VERSION_EU)
    g_JP_GlobalTimerDelta = 1.0f;
#endif

    D_80048380 = 0;
    g_GlobalTimer = 0;

#if defined(VERSION_JP)
    g_GlobalTimerDelta = 1.f;
#endif
#if defined(VERSION_EU)
    g_GlobalTimerDelta = 1.20000004768f;
#endif

    D_80048388 = 0;
    D_8004838C = 0;
    D_80048390 = 0;
    D_80048394 = 0;
    g_CurrentMultiPlayerSec = 0.0f;
    D_800483B0 = 0;
    g_StageTimeSec = 0.0f;
    g_MpSoundStateRelated = 0;

    sndSetScalerApplyVolumeAllSfxSlot(1.0f);
    musicTrack1ApplySeqpVol(VOLUME_MAX);
    musicTrack2ApplySeqpVol(VOLUME_MAX);
    musicTrack3ApplySeqpVol(VOLUME_MAX);
    sub_GAME_7F0C1364();
    modelmgrSetLevelResetting(TRUE);
    set_mt_tex_alloc();
#ifdef VERSION_EU
    bullet_moving_sparks_reset();
#else
    bullet_sparks_reset_all();
#endif
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

        if ((g_CurrentStageToLoad != LEVELID_TITLE) && (D_80048394 == 0) && (g_ClockTimer > 0))
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

        load_bg_file(g_CurrentStageToLoad);
        skySetStageNum(g_CurrentStageToLoad);

        // HACK: This method call is wrong. The function takes one argument, but the asm calls it without
        // any arguments here.
        init_watch_at_start_of_stage();

        sub_GAME_7F0C11FC(stage);

        for (i=0; i<4; i++)
        {
            s32 s3;
            player_data = (struct player_data *)&g_playerPlayerData[i];

            if (getPlayerCount() == 1)
            {
                // s4 variable
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

                lvlSetMpTime(get_mp_timelimit());
                lvlSetMpPoint(get_mp_pointlimit());
                copy_aim_settings_to_playerdata();
            }

            // g_playerPlayerData s4 variable
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
            player_data->distance_traveled = 0.0f; // one kind of float zero
            player_data->body_armor_pickups = 0.f; // a different kind of float zero

            // g_playerPlayerData s2, different than above
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
    sub_GAME_7F007290();
    initCheatTextBuffer();

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        disableOnscreenCheatText();
    }
    else
    {
        s32 s0;

        init_path_table_links();
        init_ejected_cartridges();

        for (s0 = 0; s0 < getPlayerCount(); s0++)
        {
            set_cur_player(s0);
            reinit_gunheld_totaltime();
            init_player_BONDdata_stats();
            init_player_BONDdata();
            bondviewLoadSetupIntroSection();
            bondviewPlayerSpawnRelated();
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
    D_80048368 = 1.0f;
    lvlSetControlsLockedFlag(0);
}



s32 lvlGetCurrentStageToLoad(void)
{
    return g_CurrentStageToLoad;
}


#ifdef NONMATCHING
/**
 * Debug method. Something to do with portals. Button press
 * on controller 1 and 2 are used for control flow.
 *
 * address 0x7F0BDF10.
 *
 * https://decomp.me/scratch/C83v8 90.47%
 *
 * decomp status:
 * - compiles: yes
 * - stack resize: wrong
 * - identical instructions: no
 * - identical registers: fail
 */
Gfx * lvlPortalDebug7F0BDF10(Gfx * arg0)
{
    s32 temp_v1;
    s32 sp20;
    s32 i;

    sp20 = 0;

    if (arg0 != 0)
    {
        arg0 = bgDebugRemoved7F0B9DE4(arg0, g_DebugPortalsD_800483CC, -1);

        for (i = 0; i < 4; i++)
        {
            g_DebugPortalsInputBuffer[i] = g_DebugPortalsInputBuffer[i+1];
        }

        temp_v1 = joyGetButtons(PLAYER_1, A_BUTTON) | joyGetButtons(PLAYER_2, A_BUTTON);

        if (g_DebugPortalsInputBuffer[3] != temp_v1)
        {
            D_800483C0 ^= 1;
        }

        if (g_DebugPortalsInputBuffer[0] != g_DebugPortalsInputBuffer[1])
        {
            D_800483C0 ^= 1;
        }

        g_DebugPortalsInputBuffer[4] = temp_v1;

        bgRemoved7F0B9DF4(temp_v1 ? g_DebugPortalsD_800483CC : -1);

        return arg0;
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, L_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, L_JPAD))
    {
        g_DebugPortalsD_800483CC--;
        sp20 = 1;
        if (g_DebugPortalsD_800483CC < 0)
        {
            g_DebugPortalsD_800483CC = 0;
        }
    }

    if (joyGetButtonsPressedThisFrame(PLAYER_1, R_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, R_JPAD))
    {
        g_DebugPortalsD_800483CC++;
        sp20 = 1;
    }

    if (
        (joyGetButtons(PLAYER_1, R_TRIG) | joyGetButtons(PLAYER_2, R_TRIG))
        && (joyGetButtons(PLAYER_1, L_TRIG) | joyGetButtons(PLAYER_2, L_TRIG)))
    {
        if (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD))
        {
            bgSwapConnectedRooms(g_DebugPortalsD_800483CC);
        }
    }
    else if (joyGetButtons(PLAYER_1, R_TRIG) | joyGetButtons(PLAYER_2, R_TRIG))
    {
        if (
            (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, D_JPAD))
            && (bgGetDataPortalsControlBytes1Bit1(g_DebugPortalsD_800483CC) == 0))
        {
            bgToggleDataPortalsContrlBytes1Bit1(g_DebugPortalsD_800483CC, 0);
            sp20 = 1;
        }

        if (
            (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, U_JPAD))
            && (bgGetDataPortalsControlBytes1Bit1(g_DebugPortalsD_800483CC) != 0))
        {
            bgToggleDataPortalsContrlBytes1Bit1(g_DebugPortalsD_800483CC, 1);
            sp20 = 1;
        }
    }
    else if (joyGetButtons(PLAYER_1, L_TRIG) | joyGetButtons(PLAYER_2, L_TRIG))
    {
        if (
            (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, D_JPAD))
            && bgGetDataPortalsControlBytes1Bit2(g_DebugPortalsD_800483CC))
        {
            bgClearDataPortalsControlBytes1Low2Bits(g_DebugPortalsD_800483CC);
            sp20 = 1;
        }

        if (
            (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, U_JPAD))
            && (bgGetDataPortalsControlBytes1Bit2(g_DebugPortalsD_800483CC) == 0))
        {
            bgSetDataPortalsControlBytes1Bit2(g_DebugPortalsD_800483CC);
            sp20 = 1;
        }
    }
    else
    {
        if (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, D_JPAD))
        {
            sub_GAME_7F0B9A7C(g_DebugPortalsD_800483CC);
            sp20 = 1;
        }

        if (joyGetButtonsPressedThisFrame(PLAYER_1, U_JPAD) | joyGetButtonsPressedThisFrame(PLAYER_2, U_JPAD))
        {
            sub_GAME_7F0B9A2C(g_DebugPortalsD_800483CC);
            sp20 = 1;
        }
    }

    if (sp20)
    {
        // removed?
    }

    return 0;

}
#else
GLOBAL_ASM(
.text
glabel lvlPortalDebug7F0BDF10
/* 0F2A40 7F0BDF10 27BDFFD8 */  addiu $sp, $sp, -0x28
/* 0F2A44 7F0BDF14 AFBF001C */  sw    $ra, 0x1c($sp)
/* 0F2A48 7F0BDF18 AFB00018 */  sw    $s0, 0x18($sp)
/* 0F2A4C 7F0BDF1C 1080003B */  beqz  $a0, .L7F0BE00C
/* 0F2A50 7F0BDF20 AFA00020 */   sw    $zero, 0x20($sp)
/* 0F2A54 7F0BDF24 3C058005 */  lui   $a1, %hi(g_DebugPortalsD_800483CC)
/* 0F2A58 7F0BDF28 8CA583CC */  lw    $a1, %lo(g_DebugPortalsD_800483CC)($a1)
/* 0F2A5C 7F0BDF2C 0FC2E779 */  jal   bgDebugRemoved7F0B9DE4
/* 0F2A60 7F0BDF30 2406FFFF */   li    $a2, -1
/* 0F2A64 7F0BDF34 3C0E8005 */  lui   $t6, %hi(g_DebugPortalsInputBuffer + 0x4)
/* 0F2A68 7F0BDF38 8DCE83D4 */  lw    $t6, %lo(g_DebugPortalsInputBuffer + 0x4)($t6)
/* 0F2A6C 7F0BDF3C 3C0F8005 */  lui   $t7, %hi(g_DebugPortalsInputBuffer + 0x8)
/* 0F2A70 7F0BDF40 8DEF83D8 */  lw    $t7, %lo(g_DebugPortalsInputBuffer + 0x8)($t7)
/* 0F2A74 7F0BDF44 3C018005 */  lui   $at, %hi(g_DebugPortalsInputBuffer + 0x0)
/* 0F2A78 7F0BDF48 3C188005 */  lui   $t8, %hi(g_DebugPortalsInputBuffer + 0xc)
/* 0F2A7C 7F0BDF4C AC2E83D0 */  sw    $t6, %lo(g_DebugPortalsInputBuffer + 0x0)($at)
/* 0F2A80 7F0BDF50 8F1883DC */  lw    $t8, %lo(g_DebugPortalsInputBuffer + 0xc)($t8)
/* 0F2A84 7F0BDF54 3C018005 */  lui   $at, %hi(g_DebugPortalsInputBuffer + 0x4)
/* 0F2A88 7F0BDF58 AC2F83D4 */  sw    $t7, %lo(g_DebugPortalsInputBuffer + 0x4)($at)
/* 0F2A8C 7F0BDF5C 3C038005 */  lui   $v1, %hi(g_DebugPortalsInputBuffer + 0x10)
/* 0F2A90 7F0BDF60 3C018005 */  lui   $at, %hi(g_DebugPortalsInputBuffer + 0x8)
/* 0F2A94 7F0BDF64 8C6383E0 */  lw    $v1, %lo(g_DebugPortalsInputBuffer + 0x10)($v1)
/* 0F2A98 7F0BDF68 AC3883D8 */  sw    $t8, %lo(g_DebugPortalsInputBuffer + 0x8)($at)
/* 0F2A9C 7F0BDF6C 3C018005 */  lui   $at, %hi(g_DebugPortalsInputBuffer + 0xc)
/* 0F2AA0 7F0BDF70 AFA20028 */  sw    $v0, 0x28($sp)
/* 0F2AA4 7F0BDF74 00002025 */  move  $a0, $zero
/* 0F2AA8 7F0BDF78 34058000 */  li    $a1, 32768
/* 0F2AAC 7F0BDF7C 0C0030C3 */  jal   joyGetButtons
/* 0F2AB0 7F0BDF80 AC2383DC */   sw    $v1, %lo(g_DebugPortalsInputBuffer + 0xc)($at)
/* 0F2AB4 7F0BDF84 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2AB8 7F0BDF88 24040001 */  li    $a0, 1
/* 0F2ABC 7F0BDF8C 0C0030C3 */  jal   joyGetButtons
/* 0F2AC0 7F0BDF90 34058000 */   li    $a1, 32768
/* 0F2AC4 7F0BDF94 3C198005 */  lui   $t9, %hi(g_DebugPortalsInputBuffer + 0xc)
/* 0F2AC8 7F0BDF98 8F3983DC */  lw    $t9, %lo(g_DebugPortalsInputBuffer + 0xc)($t9)
/* 0F2ACC 7F0BDF9C 00501825 */  or    $v1, $v0, $s0
/* 0F2AD0 7F0BDFA0 3C0B8005 */  lui   $t3, %hi(g_DebugPortalsInputBuffer + 0x0)
/* 0F2AD4 7F0BDFA4 13230006 */  beq   $t9, $v1, .L7F0BDFC0
/* 0F2AD8 7F0BDFA8 3C0C8005 */   lui   $t4, %hi(g_DebugPortalsInputBuffer + 0x4)
/* 0F2ADC 7F0BDFAC 3C028005 */  lui   $v0, %hi(D_800483C0)
/* 0F2AE0 7F0BDFB0 244283C0 */  addiu $v0, %lo(D_800483C0) # addiu $v0, $v0, -0x7c40
/* 0F2AE4 7F0BDFB4 8C490000 */  lw    $t1, ($v0)
/* 0F2AE8 7F0BDFB8 392A0001 */  xori  $t2, $t1, 1
/* 0F2AEC 7F0BDFBC AC4A0000 */  sw    $t2, ($v0)
.L7F0BDFC0:
/* 0F2AF0 7F0BDFC0 8D6B83D0 */  lw    $t3, %lo(g_DebugPortalsInputBuffer + 0x0)($t3)
/* 0F2AF4 7F0BDFC4 8D8C83D4 */  lw    $t4, %lo(g_DebugPortalsInputBuffer + 0x4)($t4)
/* 0F2AF8 7F0BDFC8 3C028005 */  lui   $v0, %hi(D_800483C0)
/* 0F2AFC 7F0BDFCC 244283C0 */  addiu $v0, %lo(D_800483C0) # addiu $v0, $v0, -0x7c40
/* 0F2B00 7F0BDFD0 116C0004 */  beq   $t3, $t4, .L7F0BDFE4
/* 0F2B04 7F0BDFD4 3C018005 */   lui   $at, %hi(g_DebugPortalsInputBuffer + 0x10)
/* 0F2B08 7F0BDFD8 8C4E0000 */  lw    $t6, ($v0)
/* 0F2B0C 7F0BDFDC 39CF0001 */  xori  $t7, $t6, 1
/* 0F2B10 7F0BDFE0 AC4F0000 */  sw    $t7, ($v0)
.L7F0BDFE4:
/* 0F2B14 7F0BDFE4 10600004 */  beqz  $v1, .L7F0BDFF8
/* 0F2B18 7F0BDFE8 AC2383E0 */   sw    $v1, %lo(g_DebugPortalsInputBuffer + 0x10)($at)
/* 0F2B1C 7F0BDFEC 3C048005 */  lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2B20 7F0BDFF0 10000002 */  b     .L7F0BDFFC
/* 0F2B24 7F0BDFF4 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
.L7F0BDFF8:
/* 0F2B28 7F0BDFF8 2404FFFF */  li    $a0, -1
.L7F0BDFFC:
/* 0F2B2C 7F0BDFFC 0FC2E77D */  jal   bgRemoved7F0B9DF4
/* 0F2B30 7F0BE000 00000000 */   nop
/* 0F2B34 7F0BE004 100000BC */  b     .L7F0BE2F8
/* 0F2B38 7F0BE008 8FA20028 */   lw    $v0, 0x28($sp)
.L7F0BE00C:
/* 0F2B3C 7F0BE00C 00002025 */  move  $a0, $zero
/* 0F2B40 7F0BE010 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2B44 7F0BE014 24050200 */   li    $a1, 512
/* 0F2B48 7F0BE018 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2B4C 7F0BE01C 24040001 */  li    $a0, 1
/* 0F2B50 7F0BE020 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2B54 7F0BE024 24050200 */   li    $a1, 512
/* 0F2B58 7F0BE028 0050C025 */  or    $t8, $v0, $s0
/* 0F2B5C 7F0BE02C 1300000B */  beqz  $t8, .L7F0BE05C
/* 0F2B60 7F0BE030 24050100 */   li    $a1, 256
/* 0F2B64 7F0BE034 3C048005 */  lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2B68 7F0BE038 8C8483CC */  lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2B6C 7F0BE03C 24190001 */  li    $t9, 1
/* 0F2B70 7F0BE040 3C018005 */  lui   $at, %hi(g_DebugPortalsD_800483CC)
/* 0F2B74 7F0BE044 2484FFFF */  addiu $a0, $a0, -1
/* 0F2B78 7F0BE048 AC2483CC */  sw    $a0, %lo(g_DebugPortalsD_800483CC)($at)
/* 0F2B7C 7F0BE04C 04810003 */  bgez  $a0, .L7F0BE05C
/* 0F2B80 7F0BE050 AFB90020 */   sw    $t9, 0x20($sp)
/* 0F2B84 7F0BE054 3C018005 */  lui   $at, %hi(g_DebugPortalsD_800483CC)
/* 0F2B88 7F0BE058 AC2083CC */  sw    $zero, %lo(g_DebugPortalsD_800483CC)($at)
.L7F0BE05C:
/* 0F2B8C 7F0BE05C 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2B90 7F0BE060 00002025 */   move  $a0, $zero
/* 0F2B94 7F0BE064 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2B98 7F0BE068 24040001 */  li    $a0, 1
/* 0F2B9C 7F0BE06C 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2BA0 7F0BE070 24050100 */   li    $a1, 256
/* 0F2BA4 7F0BE074 00504025 */  or    $t0, $v0, $s0
/* 0F2BA8 7F0BE078 11000008 */  beqz  $t0, .L7F0BE09C
/* 0F2BAC 7F0BE07C 24050010 */   li    $a1, 16
/* 0F2BB0 7F0BE080 3C048005 */  lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2BB4 7F0BE084 8C8483CC */  lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2BB8 7F0BE088 24090001 */  li    $t1, 1
/* 0F2BBC 7F0BE08C 3C018005 */  lui   $at, %hi(g_DebugPortalsD_800483CC)
/* 0F2BC0 7F0BE090 24840001 */  addiu $a0, $a0, 1
/* 0F2BC4 7F0BE094 AC2483CC */  sw    $a0, %lo(g_DebugPortalsD_800483CC)($at)
/* 0F2BC8 7F0BE098 AFA90020 */  sw    $t1, 0x20($sp)
.L7F0BE09C:
/* 0F2BCC 7F0BE09C 0C0030C3 */  jal   joyGetButtons
/* 0F2BD0 7F0BE0A0 00002025 */   move  $a0, $zero
/* 0F2BD4 7F0BE0A4 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2BD8 7F0BE0A8 24040001 */  li    $a0, 1
/* 0F2BDC 7F0BE0AC 0C0030C3 */  jal   joyGetButtons
/* 0F2BE0 7F0BE0B0 24050010 */   li    $a1, 16
/* 0F2BE4 7F0BE0B4 00505025 */  or    $t2, $v0, $s0
/* 0F2BE8 7F0BE0B8 11400012 */  beqz  $t2, .L7F0BE104
/* 0F2BEC 7F0BE0BC 00002025 */   move  $a0, $zero
/* 0F2BF0 7F0BE0C0 0C0030C3 */  jal   joyGetButtons
/* 0F2BF4 7F0BE0C4 24050020 */   li    $a1, 32
/* 0F2BF8 7F0BE0C8 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2BFC 7F0BE0CC 24040001 */  li    $a0, 1
/* 0F2C00 7F0BE0D0 0C0030C3 */  jal   joyGetButtons
/* 0F2C04 7F0BE0D4 24050020 */   li    $a1, 32
/* 0F2C08 7F0BE0D8 00505825 */  or    $t3, $v0, $s0
/* 0F2C0C 7F0BE0DC 11600009 */  beqz  $t3, .L7F0BE104
/* 0F2C10 7F0BE0E0 00002025 */   move  $a0, $zero
/* 0F2C14 7F0BE0E4 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2C18 7F0BE0E8 24050400 */   li    $a1, 1024
/* 0F2C1C 7F0BE0EC 10400081 */  beqz  $v0, .L7F0BE2F4
/* 0F2C20 7F0BE0F0 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2C24 7F0BE0F4 0FC2E6D9 */  jal   bgSwapConnectedRooms
/* 0F2C28 7F0BE0F8 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2C2C 7F0BE0FC 1000007E */  b     .L7F0BE2F8
/* 0F2C30 7F0BE100 00001025 */   move  $v0, $zero
.L7F0BE104:
/* 0F2C34 7F0BE104 00002025 */  move  $a0, $zero
/* 0F2C38 7F0BE108 0C0030C3 */  jal   joyGetButtons
/* 0F2C3C 7F0BE10C 24050010 */   li    $a1, 16
/* 0F2C40 7F0BE110 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2C44 7F0BE114 24040001 */  li    $a0, 1
/* 0F2C48 7F0BE118 0C0030C3 */  jal   joyGetButtons
/* 0F2C4C 7F0BE11C 24050010 */   li    $a1, 16
/* 0F2C50 7F0BE120 00506025 */  or    $t4, $v0, $s0
/* 0F2C54 7F0BE124 11800029 */  beqz  $t4, .L7F0BE1CC
/* 0F2C58 7F0BE128 00002025 */   move  $a0, $zero
/* 0F2C5C 7F0BE12C 00002025 */  move  $a0, $zero
/* 0F2C60 7F0BE130 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2C64 7F0BE134 24050400 */   li    $a1, 1024
/* 0F2C68 7F0BE138 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2C6C 7F0BE13C 24040001 */  li    $a0, 1
/* 0F2C70 7F0BE140 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2C74 7F0BE144 24050400 */   li    $a1, 1024
/* 0F2C78 7F0BE148 00506825 */  or    $t5, $v0, $s0
/* 0F2C7C 7F0BE14C 11A0000A */  beqz  $t5, .L7F0BE178
/* 0F2C80 7F0BE150 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2C84 7F0BE154 0FC2E6B9 */  jal   bgGetDataPortalsControlBytes1Bit1
/* 0F2C88 7F0BE158 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2C8C 7F0BE15C 14400006 */  bnez  $v0, .L7F0BE178
/* 0F2C90 7F0BE160 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2C94 7F0BE164 240E0001 */  li    $t6, 1
/* 0F2C98 7F0BE168 AFAE0020 */  sw    $t6, 0x20($sp)
/* 0F2C9C 7F0BE16C 8C8483CC */  lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2CA0 7F0BE170 0FC2E76F */  jal   bgToggleDataPortalsContrlBytes1Bit1
/* 0F2CA4 7F0BE174 00002825 */   move  $a1, $zero
.L7F0BE178:
/* 0F2CA8 7F0BE178 00002025 */  move  $a0, $zero
/* 0F2CAC 7F0BE17C 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2CB0 7F0BE180 24050800 */   li    $a1, 2048
/* 0F2CB4 7F0BE184 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2CB8 7F0BE188 24040001 */  li    $a0, 1
/* 0F2CBC 7F0BE18C 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2CC0 7F0BE190 24050800 */   li    $a1, 2048
/* 0F2CC4 7F0BE194 00507825 */  or    $t7, $v0, $s0
/* 0F2CC8 7F0BE198 11E00056 */  beqz  $t7, .L7F0BE2F4
/* 0F2CCC 7F0BE19C 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2CD0 7F0BE1A0 0FC2E6B9 */  jal   bgGetDataPortalsControlBytes1Bit1
/* 0F2CD4 7F0BE1A4 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2CD8 7F0BE1A8 10400052 */  beqz  $v0, .L7F0BE2F4
/* 0F2CDC 7F0BE1AC 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2CE0 7F0BE1B0 24180001 */  li    $t8, 1
/* 0F2CE4 7F0BE1B4 AFB80020 */  sw    $t8, 0x20($sp)
/* 0F2CE8 7F0BE1B8 8C8483CC */  lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2CEC 7F0BE1BC 0FC2E76F */  jal   bgToggleDataPortalsContrlBytes1Bit1
/* 0F2CF0 7F0BE1C0 24050001 */   li    $a1, 1
/* 0F2CF4 7F0BE1C4 1000004C */  b     .L7F0BE2F8
/* 0F2CF8 7F0BE1C8 00001025 */   move  $v0, $zero
.L7F0BE1CC:
/* 0F2CFC 7F0BE1CC 0C0030C3 */  jal   joyGetButtons
/* 0F2D00 7F0BE1D0 24050020 */   li    $a1, 32
/* 0F2D04 7F0BE1D4 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2D08 7F0BE1D8 24040001 */  li    $a0, 1
/* 0F2D0C 7F0BE1DC 0C0030C3 */  jal   joyGetButtons
/* 0F2D10 7F0BE1E0 24050020 */   li    $a1, 32
/* 0F2D14 7F0BE1E4 0050C825 */  or    $t9, $v0, $s0
/* 0F2D18 7F0BE1E8 13200027 */  beqz  $t9, .L7F0BE288
/* 0F2D1C 7F0BE1EC 00002025 */   move  $a0, $zero
/* 0F2D20 7F0BE1F0 00002025 */  move  $a0, $zero
/* 0F2D24 7F0BE1F4 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2D28 7F0BE1F8 24050400 */   li    $a1, 1024
/* 0F2D2C 7F0BE1FC 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2D30 7F0BE200 24040001 */  li    $a0, 1
/* 0F2D34 7F0BE204 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2D38 7F0BE208 24050400 */   li    $a1, 1024
/* 0F2D3C 7F0BE20C 00504025 */  or    $t0, $v0, $s0
/* 0F2D40 7F0BE210 11000009 */  beqz  $t0, .L7F0BE238
/* 0F2D44 7F0BE214 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2D48 7F0BE218 0FC2E6C1 */  jal   bgGetDataPortalsControlBytes1Bit2
/* 0F2D4C 7F0BE21C 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2D50 7F0BE220 10400005 */  beqz  $v0, .L7F0BE238
/* 0F2D54 7F0BE224 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2D58 7F0BE228 24090001 */  li    $t1, 1
/* 0F2D5C 7F0BE22C AFA90020 */  sw    $t1, 0x20($sp)
/* 0F2D60 7F0BE230 0FC2E6D1 */  jal   bgClearDataPortalsControlBytes1Low2Bits
/* 0F2D64 7F0BE234 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
.L7F0BE238:
/* 0F2D68 7F0BE238 00002025 */  move  $a0, $zero
/* 0F2D6C 7F0BE23C 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2D70 7F0BE240 24050800 */   li    $a1, 2048
/* 0F2D74 7F0BE244 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2D78 7F0BE248 24040001 */  li    $a0, 1
/* 0F2D7C 7F0BE24C 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2D80 7F0BE250 24050800 */   li    $a1, 2048
/* 0F2D84 7F0BE254 00505025 */  or    $t2, $v0, $s0
/* 0F2D88 7F0BE258 11400026 */  beqz  $t2, .L7F0BE2F4
/* 0F2D8C 7F0BE25C 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2D90 7F0BE260 0FC2E6C1 */  jal   bgGetDataPortalsControlBytes1Bit2
/* 0F2D94 7F0BE264 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2D98 7F0BE268 14400022 */  bnez  $v0, .L7F0BE2F4
/* 0F2D9C 7F0BE26C 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2DA0 7F0BE270 240B0001 */  li    $t3, 1
/* 0F2DA4 7F0BE274 AFAB0020 */  sw    $t3, 0x20($sp)
/* 0F2DA8 7F0BE278 0FC2E6C9 */  jal   bgSetDataPortalsControlBytes1Bit2
/* 0F2DAC 7F0BE27C 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
/* 0F2DB0 7F0BE280 1000001D */  b     .L7F0BE2F8
/* 0F2DB4 7F0BE284 00001025 */   move  $v0, $zero
.L7F0BE288:
/* 0F2DB8 7F0BE288 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2DBC 7F0BE28C 24050400 */   li    $a1, 1024
/* 0F2DC0 7F0BE290 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2DC4 7F0BE294 24040001 */  li    $a0, 1
/* 0F2DC8 7F0BE298 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2DCC 7F0BE29C 24050400 */   li    $a1, 1024
/* 0F2DD0 7F0BE2A0 00506025 */  or    $t4, $v0, $s0
/* 0F2DD4 7F0BE2A4 11800005 */  beqz  $t4, .L7F0BE2BC
/* 0F2DD8 7F0BE2A8 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2DDC 7F0BE2AC 240D0001 */  li    $t5, 1
/* 0F2DE0 7F0BE2B0 AFAD0020 */  sw    $t5, 0x20($sp)
/* 0F2DE4 7F0BE2B4 0FC2E69F */  jal   sub_GAME_7F0B9A7C
/* 0F2DE8 7F0BE2B8 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
.L7F0BE2BC:
/* 0F2DEC 7F0BE2BC 00002025 */  move  $a0, $zero
/* 0F2DF0 7F0BE2C0 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2DF4 7F0BE2C4 24050800 */   li    $a1, 2048
/* 0F2DF8 7F0BE2C8 3050FFFF */  andi  $s0, $v0, 0xffff
/* 0F2DFC 7F0BE2CC 24040001 */  li    $a0, 1
/* 0F2E00 7F0BE2D0 0C0030EB */  jal   joyGetButtonsPressedThisFrame
/* 0F2E04 7F0BE2D4 24050800 */   li    $a1, 2048
/* 0F2E08 7F0BE2D8 00507025 */  or    $t6, $v0, $s0
/* 0F2E0C 7F0BE2DC 11C00005 */  beqz  $t6, .L7F0BE2F4
/* 0F2E10 7F0BE2E0 3C048005 */   lui   $a0, %hi(g_DebugPortalsD_800483CC)
/* 0F2E14 7F0BE2E4 240F0001 */  li    $t7, 1
/* 0F2E18 7F0BE2E8 AFAF0020 */  sw    $t7, 0x20($sp)
/* 0F2E1C 7F0BE2EC 0FC2E68B */  jal   sub_GAME_7F0B9A2C
/* 0F2E20 7F0BE2F0 8C8483CC */   lw    $a0, %lo(g_DebugPortalsD_800483CC)($a0)
.L7F0BE2F4:
/* 0F2E24 7F0BE2F4 00001025 */  move  $v0, $zero
.L7F0BE2F8:
/* 0F2E28 7F0BE2F8 8FBF001C */  lw    $ra, 0x1c($sp)
/* 0F2E2C 7F0BE2FC 8FB00018 */  lw    $s0, 0x18($sp)
/* 0F2E30 7F0BE300 27BD0028 */  addiu $sp, $sp, 0x28
/* 0F2E34 7F0BE304 03E00008 */  jr    $ra
/* 0F2E38 7F0BE308 00000000 */   nop
)
#endif



/**
 * Graphics render method.
 * Also sets player max ammo if infinite ammo cheat is enabled.
 *
 * Address 0x7F0BE30C (VERSION_US).
 */

Gfx* lvlRender(Gfx* DL)
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

            if (get_debug_render_raster() == DEB_MOVE_VIEW)
            {
                DL = sub_GAME_7F091580(DL);
            }

            if (get_debug_render_raster() == DEB_STAN_VIEW)
            {
                DL = stanRenderDebugStanView(DL);
            }

            if (get_debug_render_raster() == DEB_BOND_VIEW)
            {
                DL = bondviewRenderDebugBondView(DL);
            }

            DL = viSetupScreensForNumPlayers(DL);
            DL = skyRender(DL);
            bgRoomVisibilityRelated();
            determing_type_of_object_and_detection();
            chraiUpdateOnscreenPropCount();
            chrpropUpdateAutoaimTarget();
            chraiCheckUseHeldItems();

            if (bond_pressed_reload_activate() && bond_interact_object())
            {
                attempt_reload_item_in_hand(GUNRIGHT);
                attempt_reload_item_in_hand(GUNLEFT);
            }

            sub_GAME_7F03D0D4();
            DL = bgLevelRender(DL);

            if (get_debug_portal_flag())
            {
                DL = lvlPortalDebug7F0BDF10(DL);
            }

            if (get_debug_stan_problems_flag())
            {
                DL = sub_GAME_7F0B303C(DL);
            }

            if (get_debug_stanhit_flag())
            {
                DL = sub_GAME_7F0B3034(DL);
                DL = write_stan_tiles_in_yellow(DL);
            }

            if (get_debug_stanregion_flag())
            {
                DL = sub_GAME_7F0B3034(DL);
                DL = sub_GAME_7F0B312C(DL, -0x7FC0);
            }

            if (tokenFind(1, "-stanshow_"))
            {
                StandTilePoint *tile1 = stanMatchTileName(tokenFind(1, "-stanshow_"));
                if (tile1)
                {
                    DL = sub_GAME_7F0B3034(DL);
                    DL = sub_GAME_7F0B3024(DL, tile1, 0xFF0000FF);
                }
            }

            if (tokenFind(2, "-stanshow_"))
            {
                StandTilePoint *tile2 = stanMatchTileName(tokenFind(2, "-stanshow_"));
                if (tile2)
                {
                    DL = sub_GAME_7F0B3034(DL);
                    DL = sub_GAME_7F0B3024(DL, tile2, 0xFF00FF);
                }
            }

            if (tokenFind(3, "-stanshow_"))
            {
                StandTilePoint *tile3 = stanMatchTileName(tokenFind(3, "-stanshow_"));
                if (tile3)
                {
                    DL = sub_GAME_7F0B3034(DL);
                    DL = sub_GAME_7F0B3024(DL, tile3, 0xFFFF);
                }
            }

            setanimationdebugflag(getDebugMode() == DEB_SELANIM);
            DL = weaponRenderTracers(DL);

#if defined(VERSION_EU)
            bullet_moving_sparks_update(&DL, ZBUF_SURFACE);
#else /* VERSION_US, VERSION_JP, unspecified */
            bullet_sparks_render_all(&DL, ZBUF_SURFACE);
#endif
            DL = glassRenderShards(DL);
            DL = explosionRenderFlyingParticles(DL);

            if (

#if defined(BUGFIX_R1)
                cheatIsActive(CHEAT_INFINITE_AMMO) != 0
                && (
                    (getCurrentPlayerWeaponId(GUNRIGHT) != ITEM_WATCHLASER)
                    || (g_CurrentPlayer->trigger_down == 0)
                )
#else /* VERSION_US */
                cheatIsActive(CHEAT_INFINITE_AMMO) != 0
#endif
                )
            {
                set_max_ammo_for_cur_player();
            }

            if (get_debug_render_raster() == DEB_BOND_VIEW)
            {
                DL = maybe_mp_interface(DL);
            }
            else
            {
                DL = bondviewRemoved7F08BCB8(DL);
            }

            DL = mp_watch_menu_display(DL);
        }
    }

    gDPSetScissor(DL++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());

    return DL;
}


/**
 * Sets the modifier values for the level being loaded.
 * This covers the enemy accuracy, reaction speed, and similar values.
 *
 * address 0x7F0BE8D0
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

        F_80030B14 = 2.0f;
        F_80030B18 = 2.0f;
        g_AutogunPendingDamageTick = (0.5f * damageMultiplier);
        g_AutogunDamageScalar = (0.5f * damageMultiplier);
        F_80030B24 = 2.0f;
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
        F_80030B14 = 1.0f;
        F_80030B18 = 1.0f;
        g_AutogunPendingDamageTick = 0.75f;
        g_AutogunDamageScalar = 0.75f;
        F_80030B24 = 1.0f;
        g_AiAccuracyModifier = DEFAULT_SECRET_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_SECRET_AGENT_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 0.75f;

#if defined(BUGFIX_R1)
        if (j_text_trigger)
        {
            difficulty = 1.1f;
        }
        else
        {
            difficulty = 0.75f;
        }
#else
        // VERSION_US
        difficulty = 0.75f;
#endif

        g_SoloAmmoMultiplier = DEFAULT_SECRET_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_SECRET_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_00)
    {
        F_80030B14 = 1.0f;
        F_80030B18 = 1.0f;
        g_AutogunPendingDamageTick = 1.0f;
        g_AutogunDamageScalar = 1.0f;
        F_80030B24 = 1.0f;
        g_AiAccuracyModifier = DEFAULT_00_AGENT_AI_ACCURACY_MODIFIER;
        g_AiDamageModifier = DEFAULT_00_AGENT_AI_DAMAGE_MODIFIER;
        g_AiHealthModifier = 1.0f;
        g_SpExplosionDamageMult = 1.0f;

#if defined(BUGFIX_R1)
        if (j_text_trigger)
        {
            difficulty = 0.75f;
        }
        else
        {
            difficulty = 0.2f;
        }
#else
        // VERSION_US
        difficulty = 0.2f;
#endif

        g_SoloAmmoMultiplier = DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER;
        g_AiReactionSpeed = DEFAULT_00_AGENT_AI_REACTION_SPEED;
    }
    else if (g_SelectedDifficulty == DIFFICULTY_007)
    {
        F_80030B14 = 1.0f;
        F_80030B18 = 1.0f;
        g_AutogunPendingDamageTick = 1.0f;
        g_AutogunDamageScalar = 1.0f;
        F_80030B24 = 1.0f;
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
 * Multiplayer method. Manages a lot of stuff.
 * Tracks you-only-live-twice kills/deaths.
 * Lots of debug code.
 *
 * Address: 0x7F0BEB88 (NTSC).
 * Address: 0x7F0BF7AC (NTSC-J).
 * Address: 0x7F0BDFAC (PAL).
 */
void lvlManageMpGame(void)
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
        D_80048380 += 1;
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
    if ((g_CurrentStageToLoad != LEVELID_TITLE) && (D_80048394 == 0) && (g_ClockTimer > 0))
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

            for (i=0; i<getPlayerCount(); i++)
            {
                if (g_playerPointers[i]->bonddead != FALSE)
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
            current_time = D_80048394;
            sp180 = g_ClockTimer + D_80048394;

#ifdef VERSION_EU
            if ((D_80048394 < (g_MpTime - 0xBB8)) && (sp180 >= (g_MpTime - 0xBB8)))
#else
            if ((D_80048394 < (g_MpTime - 0xE10)) && (sp180 >= (g_MpTime - 0xE10)))
#endif
            {
                for (i = 0; i < getPlayerCount(); i++)
                {
                    set_cur_player(i);
#ifdef VERSION_US
                    HUDMESSAGEBOTTOM("One minute left");
#else
                    HUDMESSAGEBOTTOM(langGet(0xB044));
#endif
                }
            }

            // sound alarm when game is about to end (10 seconds before end)
#ifdef VERSION_EU
            if ((sp180 >= (g_MpTime - 0x1F4)) && (g_MpSoundStateRelated == 0) && (lvlGetControlsLockedFlag() == 0))
#else
            if ((sp180 >= (g_MpTime - 0x258)) && (g_MpSoundStateRelated == 0) && (lvlGetControlsLockedFlag() == 0))
#endif
            {
                sndPlaySfx(g_musicSfxBufferPtr, ALARM1_SFX, &g_MpSoundStateRelated);
            }

            // stop alarm
            if (lvlGetControlsLockedFlag() != 0)
            {
                if ((g_MpSoundStateRelated != NULL) && (sndGetPlayingState(g_MpSoundStateRelated) != 0))
                {
                    sndDeactivate(g_MpSoundStateRelated);
                }
            }

            if ((current_time < g_MpTime) && (sp180 >= g_MpTime))
            {
                mpCalculateAwards(0);
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
                if (g_playerPointers[i]->bonddead != FALSE &&
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
                    mpCalculateAwards(0);
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
                    if (g_playerPointers[j]->bonddead == 0)
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
                mpCalculateAwards(0);
            }
            else if (killed_total >= player_count - 1)
            {
                mpwatchSetStopPlayFlag();
            }
        } // end YOLT

        if (0)
        {
            char debug_buf[268];
            sprintf(debug_buf, "setdetail %d %d %d %d %d %d %d %d %d", 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
    }

    D_80048394 = D_80048394 + g_ClockTimer;
#ifdef VERSION_EU
    g_CurrentMultiPlayerSec = (f32) (D_80048394) / 50.0f;
#else
    g_CurrentMultiPlayerSec = (f32) (D_80048394) / 60.0f;
#endif
    D_800483A8 = D_800483A8 + g_ClockTimer;
#ifdef VERSION_EU
    g_CurrentMultiPlayerMin = (f32) (D_800483A8) / 50.0f;
#else
    g_CurrentMultiPlayerMin = (f32) (D_800483A8) / 60.0f;
#endif

    if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON))
    {
        D_80048388 = 0;
        D_80048390 = 0;
    }
    else
    {
        D_80048390 = D_80048390 + g_ClockTimer;

#ifdef VERSION_EU
        if (D_80048390 >= 0x5DC)
#else
        if (D_80048390 >= 0x708)
#endif
        {
            D_80048388 = 1;
        }
    }

    if (D_80048388 != 0)
    {
        D_8004838C += g_ClockTimer;
    }
    else
    {
        D_800483B0 = D_800483B0 + g_ClockTimer;
#ifdef VERSION_EU
        g_StageTimeSec = (f32) (D_800483B0) / 50.0f;
#else
        g_StageTimeSec = (f32) (D_800483B0) / 60.0f;
#endif
        D_800483B8 = D_800483B8 + g_ClockTimer;
#ifdef VERSION_EU
        g_PowerOnTimeSec = (f32) (D_800483B8) / 50.0f;
#else
        g_PowerOnTimeSec = (f32) (D_800483B8) / 60.0f;
#endif
    }

    viSetUseZBuf(1);

    if (g_CurrentStageToLoad == LEVELID_TITLE)
    {
        cheat_buttons_mp_related();
        menu_init();
        langTick();
    }
    else
    {
        sub_GAME_7F09BBBC();
        lvlSetMultipliersForDifficulty();
        updateRoomStatusFlags();
        sub_GAME_7F092E50();
        skyTick();
#ifdef VERSION_EU
        bullet_moving_spark_create();
#else
        bullet_sparks_update_all();
#endif
        update_bullet_casings();
        update_broken_windows();
        explosionUpdateFlyingParticles();
        chrpropTick();
        reset_all_music_slots();
        langTick();

        if ((get_debug_joy2detailedit_flag() != 0) && (D_800483C8 == 0))
        {
            s32 i;
            D_800483C8 = (struct LvlMpUnknown *)mempAllocBytesInBank(0x3000, MEMPOOL_STAGE);
            if (D_800483C8 != 0)
            {
                for (i=0; i<3000; i++)
                {
                    D_800483C8[i].unk_0 = 0xff;
                    D_800483C8[i].unk_1 = (D_800483C8[i].unk_1 & 0xFF1F) | 0x20;
                    D_800483C8[i].unk_1 = (D_800483C8[i].unk_1 & 0xFFE3) | 4;
                }
            }
        }

        if (get_debug_portal_flag() != 0)
        {
            lvlPortalDebug7F0BDF10(0);
        }

        switch (getDebugMode())
        {
            case 4:
            {
                if (joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS))
                {
                    sub_GAME_7F0AF630(-1);
                    debugStanView(0, 0, 0);
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS))
                {
                    sub_GAME_7F0AF630(1);
                    debugStanView(0, 0, 0);
                }
            }
            break;

            case 8:
            {
                if (joyGetButtonsPressedThisFrame(PLAYER_1, L_CBUTTONS))
                {
                    chrDecrementAnimationTablePointerCount();
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, R_CBUTTONS))
                {
                    chrIncrementAnimationTablePointerCount();
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, L_TRIG))
                {
                    chrToggleD_8002C90C();
                }

                sub_GAME_7F022EE0(joyGetButtons(PLAYER_1, R_TRIG) != 0);
            }
            break;
        }

    }
    {
        struct ALBank * sfx;
        s16 sound_index;
        s16 *sound_index_ptr;
        switch (getDebugMode())
        {
            case 0x38:
            {
                s32 sp30;
                s32 sp2C;
                sp30 = viGetHorizontalOffset();
                sp2C = viGet800232A0();
                if (joyGetButtons(PLAYER_1, D_CBUTTONS))
                {
                    sp2C += 1;
                }
                if (joyGetButtons(PLAYER_1, U_CBUTTONS))
                {
                    sp2C += -1;
                }
                if (joyGetButtons(PLAYER_1, R_CBUTTONS))
                {
                    sp30 += 1;
                }
                if (joyGetButtons(PLAYER_1, L_CBUTTONS))
                {
                    sp30 += -1;
                }
                viSetHorizontalOffset(sp30);
                viSet800232A0(sp2C);
            }
            break;

            case 0xc:
            {
                if (joyGetButtonsPressedThisFrame(PLAYER_1, (L_JPAD | L_CBUTTONS)))
                {
                    lvlMusicAppendPlayEndTheme();
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, (R_JPAD | R_CBUTTONS)))
                {
                    lvlMusicAppendPlaySoloDeathShort();
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, D_JPAD))
                {
                    musicTrack1Stop();
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, B_BUTTON))
                {
                    musicTrack1SaveCurrentVolumeAsTrackDefault();
                }
            }
            break;
            case 0xd:
            {
#ifdef VERSION_US
                sound_index_ptr = &g_DebugMpGameSoundFxIndex;

                if (joyGetButtonsPressedThisFrame(PLAYER_1, (D_JPAD | L_JPAD | L_TRIG | L_CBUTTONS)))
                {
                    sound_index = g_DebugMpGameSoundFxIndex - 1;\
                    sfx = g_musicSfxBufferPtr;\
                    *sound_index_ptr = sound_index;
                    sndPlaySfx(sfx, sound_index, NULL);
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, (U_JPAD | R_JPAD | R_TRIG | R_CBUTTONS)))
                {
                    sound_index = g_DebugMpGameSoundFxIndex + 1;\
                    sfx = g_musicSfxBufferPtr;\
                    *sound_index_ptr = sound_index;
                    sndPlaySfx(sfx, sound_index, NULL);
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS))
                {
                    sndDeactivateAllSfxByFlag_1();
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS))
                {
                    sound_index = g_DebugMpGameSoundFxIndex;\
                    sfx = g_musicSfxBufferPtr;
                    sndPlaySfx(sfx, sound_index, NULL);
                }
#else
                sound_index_ptr = &g_DebugMpGameSoundFxIndex;

                if (joyGetButtonsPressedThisFrame(PLAYER_1, (D_JPAD | L_JPAD | L_TRIG | L_CBUTTONS)))
                {
                    sound_index = g_DebugMpGameSoundFxIndex - 1;\
                    sfx = g_musicSfxBufferPtr;\
                    *sound_index_ptr = sound_index;
                    sndPlaySfx(sfx, sound_index, NULL);
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, (U_JPAD | R_JPAD | R_TRIG | R_CBUTTONS)))
                {
                    sound_index = g_DebugMpGameSoundFxIndex + 1;\
                    sfx = g_musicSfxBufferPtr;\
                    *sound_index_ptr = sound_index;
                    sndPlaySfx(sfx, sound_index, NULL);
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, D_CBUTTONS))
                {
                    sndDeactivateAllSfxByFlag_1();
                }

                if (joyGetButtonsPressedThisFrame(PLAYER_1, U_CBUTTONS))
                {
                    sound_index = g_DebugMpGameSoundFxIndex;\
                    sfx = g_musicSfxBufferPtr;
                    sndPlaySfx(sfx, sound_index, NULL);
                }
#endif
            }
            break;

            default:
            break;
        }
    }
}



/**
 * Assumes a debug mode is present, and handles debug edit intro, debug stan edit, debug bond "view."
 * By default, the DEB_BOND_VIEW path is chosen without debug info.
 * This updates the player viewport(s), and handles player movement.
 *
 * Multiplayer:
 * Updates distance_traveled and possibly (depending on scenario) have_token_or_goldengun.
 *
 * US Address 0x7F0BF800.
 * EU address 7F0BEC44.
 */
void lvlViewMoveTick(void)
{
    s8 local_player_number;
    s32 padding;
    f32 temp_f0;
    f32 temp_f2;

    local_player_number = get_cur_playernum();
    cheat_buttons_mp_related();

    switch (get_debug_freeze_processing())
    {
        case 0:
        {
            if ((getDebugMode() == DEB_MOVE_VIEW) || ((getDebugMode() == DEB_INTRO_EDIT) && (D_80036ABC < 0)))
            {
                debugFreeCamera(joyGetStickX(local_player_number), joyGetStickY(local_player_number), joyGetButtons(local_player_number, ANY_BUTTON));
            }
            else
            {
                debugFreeCamera(joyGetStickX(local_player_number), joyGetStickY(local_player_number), 0);
            }
        }
        break;

        case 1:
        {
            if (getDebugMode() == DEB_STAN_VIEW)
            {
                debugStanView(joyGetStickX(local_player_number), joyGetStickY(local_player_number), joyGetButtons(local_player_number, ANY_BUTTON));
            }
            else
            {
                debugStanView(joyGetStickX(local_player_number), joyGetStickY(local_player_number), 0);
            }
        }
        break;

        case 2:
        {
            if (getDebugMode() == DEB_BOND_VIEW)
            {
                bondviewMovePlayerUpdateViewport(joyGetStickX(local_player_number), joyGetStickY(local_player_number), joyGetButtons(local_player_number, ANY_BUTTON));
            }
            else
            {
                bondviewMovePlayerUpdateViewport(joyGetStickX(local_player_number), joyGetStickY(local_player_number), 0);
            }

            mpwatchMenuTick();
        }
        break;
    }

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

                if(1);

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
    cleanup_REMOVED_();
    cleanupAlarms();
    cleanupObjects(g_CurrentStageToLoad);
    cleanupObjectives();
    cleanupSFXRelated();
    cleanupplayersoundrelated();
    set_missionstate_zero();
    cleanup_rooms();
}


void lvlSetControlsLockedFlag(s32 arg0)
{
    #if defined(BUGFIX_R1)
    if ((arg0 != 0) && (g_ControlsLockedFlag == 0))
    {
        joyRumblePakStop();
    }
    #endif

    g_ControlsLockedFlag = arg0;
}


s32 lvlGetControlsLockedFlag(void)
{
    return g_ControlsLockedFlag;
}


DIFFICULTY lvlGetSelectedDifficulty(void)
{
    return g_SelectedDifficulty;
}


void lvlSetSelectedDifficulty(DIFFICULTY arg0)
{
    g_SelectedDifficulty = arg0;
}

void lvlSetMpTime(s32 arg0)
{
    g_MpTime = arg0;
}


void lvlSetMpPoint(s32 arg0)
{
    g_MpPoint = arg0;
}


f32 lvlGetCurrentMultiPlayerSec(void)
{
    return g_CurrentMultiPlayerSec;
}


f32 lvlGetCurrentMultiPlayerMin(void)
{
    return g_CurrentMultiPlayerMin;
}


/**
 * Unreferenced.
 */
f32 lvlGetStageTimeSec(void)
{
    return g_StageTimeSec;
}


/**
 * Unreferenced.
 */
f32 lvlGetPowerOnTimeSec(void)
{
    return g_PowerOnTimeSec;
}


