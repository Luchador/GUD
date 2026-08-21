
#include <ultra64.h>
#include "debugmenu_handler.h"
#include "lv.h"
#include "initcheattext.h"
#include "front.h"
#include "ramromreplay.h"
#include <ramrom.h>
#include <macro.h>
#include "file.h"
#include "file2.h"
#include <random.h>
#include "joy.h"
#include "frametiming.h"


struct ramrom_struct
{
    ramromfilestructure *fdata;
    s32 locked;
};

struct ramrom_blockbuf
{
    s8 stick_x;
    s8 stick_y;
    u8 button_low;
    u8 button_high;
};

struct ramrom_seed
{
    u8 speedframes;
    u8 count;
    u8 randseed;
    u8 check;
};

//move me to better home
extern u32* ramrom_Dam_1;
extern u32* ramrom_Dam_2;
extern u32* ramrom_Facility_1;
extern u32* ramrom_Facility_2;
extern u32* ramrom_Facility_3;
extern u32* ramrom_Runway_1;
extern u32* ramrom_Runway_2;
extern u32* ramrom_BunkerI_1;
extern u32* ramrom_BunkerI_2;
extern u32* ramrom_Silo_1;
extern u32* ramrom_Silo_2;
extern u32* ramrom_Frigate_1;
extern u32* ramrom_Frigate_2;
extern u32* ramrom_Train;

extern u64 g_randomSeed;
extern u64 g_chrObjRandomSeed;

struct ramrom_struct ramrom_table[] = {
    {&ramrom_Dam_1, 0},
    {&ramrom_Dam_2, 0},
    {&ramrom_Facility_1, 0},
    {&ramrom_Facility_2, 0},
    {&ramrom_Facility_3, 0},
    {&ramrom_Runway_1, 0},
    {&ramrom_Runway_2, 0},
    {&ramrom_BunkerI_1, 0},
    {&ramrom_BunkerI_2, 0},
    {&ramrom_Silo_1, 0},
    {&ramrom_Silo_2, 0},
    {&ramrom_Frigate_1, 0},
    {&ramrom_Frigate_2, 0},
    {&ramrom_Train, 0},
    {0,0}
};

ramromfilestructure *g_ActiveDemoFile = 0;
struct ramrom_seed *ramrom_blkbuf_2 = NULL;
struct ramrom_blockbuf *ramrom_blkbuf_3 = NULL;
bool g_IsDemoPlaying = FALSE;
bool g_DemoHasStarted = FALSE;
bool g_NotSureWhatToCallThis = FALSE;
s32 recording_ramrom_flag = 0;
s32 ramrom_demo_related_6 = 0;
s32 g_ramromRecordFlag = 0;


void ramromFadeToTitle(void);
s32 ramromTickDemo(struct contsample *arg0, s32 arg1);
void ramromCopyGameSettingBeforePlayback(ramromfilestructure *state);
void ramromRestoreGameSettings(ramromfilestructure *state);


void clear_ramrom_block_buffer_heading_ptrs(void)
{
    g_ActiveDemoFile = 0;
    ramrom_blkbuf_2 = 0;
    ramrom_blkbuf_3 = 0;
}


s32 ramromGetIsDemoPlaying(void)
{
    return g_IsDemoPlaying;
}


s32 interface_menu0B_runstage(void)
{
    return g_NotSureWhatToCallThis;
}


s32 ramromTickDemo(struct contsample *inputsample, s32 arg1)
{
    s32 var_a3;
    s32 var_a0;
    struct ramrom_blockbuf *temp_v0;
    u8 var_t0;
    s32 temp_a2;
    s32 temp_t2;

    var_t0 = 0;
    temp_a2 = (s32) g_ActiveDemoFile->size_cmds;
    temp_t2 = ramrom_blkbuf_2->count;

    for (var_a3 = 0; var_a3 < temp_t2; var_a3++)
    {
        arg1 = (s32) (arg1 + 1) % CONTSAMPLE_LEN;

        for (var_a0 = 0; var_a0 < MAXCONTROLLERS; var_a0++)
        {
            if (var_a0 < temp_a2)
            {
                temp_v0 = ramrom_blkbuf_3 + (var_a3 * temp_a2) + var_a0;

                inputsample->pads[arg1 * 4 + var_a0].stick_x = temp_v0->stick_x;
                inputsample->pads[arg1 * 4 + var_a0].stick_y = temp_v0->stick_y;
                inputsample->pads[arg1 * 4 + var_a0].button = (temp_v0->button_high << 8) | temp_v0->button_low;

                var_t0 += (u8)((u8)temp_v0->stick_x + (u8)temp_v0->stick_y + temp_v0->button_low + temp_v0->button_high);
            }
            else
            {
                inputsample->pads[arg1 * 4 + var_a0].stick_x = 0;
                inputsample->pads[arg1 * 4 + var_a0].stick_y = 0;
                inputsample->pads[arg1 * 4 + var_a0].button = 0;
            }
        }
    }

    if (ramrom_blkbuf_2->randseed != (u8)g_randomSeed)
    {
        ramromFadeToTitle();
    }

    var_t0 += (u8)((u8)ramrom_blkbuf_2->speedframes + (u8)ramrom_blkbuf_2->count + ramrom_blkbuf_2->randseed);

    if (ramrom_blkbuf_2->check != var_t0)
    {
        ramromFadeToTitle();
    }
    
    joySetContDataIndex(0);
    
    if (joyGetButtonsPressedThisFrame(PLAYER_1, 0xFFFFU) != 0)
    {
        ramromFadeToTitle();
        prev_keypresses = TRUE;
    }
    
    joySetContDataIndex(1);

    return arg1;
}


void iterate_ramrom_entries_handle_camera_out(void)
{
    s32 temp_v1;
    s32 var_a3;
    
    ramrom_blkbuf_2 = romCopyAligned(ramrom_data_target + 0x1F8, address_demo_loaded, sizeof(struct ramrom_seed));

    var_a3 = ramrom_blkbuf_2->count;

    if (var_a3 > 0)
    {
        ramrom_blkbuf_3 = romCopyAligned(
            ramrom_data_target + 0x21E,
            address_demo_loaded + 4,
            g_ActiveDemoFile->size_cmds * sizeof(struct ramrom_blockbuf) * ramrom_blkbuf_2->count);
    }

    var_a3 = ramrom_blkbuf_2->count;

    if (var_a3 == 0 && ramrom_blkbuf_2->speedframes == 0)
    {
        ramromFadeToTitle();
    }
    else
    {
        // 5 is ??
        address_demo_loaded += align_addr_even((g_ActiveDemoFile->size_cmds * sizeof(struct ramrom_blockbuf) * ramrom_blkbuf_2->count) + 5);
    }

    updateFrameCounters(ramrom_blkbuf_2->speedframes);

    temp_v1 = g_ActiveDemoFile->totaltime_ms - 60;

    if ((g_GlobalTimer >= temp_v1) && ((g_GlobalTimer - g_ClockTimer) < temp_v1))
    {
        ramromFadeToTitle();
    }
}


void ramromCopyGameSettingBeforePlayback(ramromfilestructure *state)
{
    state->randomseed = g_randomSeed;
    state->randomizer = g_chrObjRandomSeed;
    state->mode = gamemode;
    state->numplayers = selected_num_players;
    state->scenario = scenario;
    state->mpstage_sel = MP_stage_selected;
    state->gamelength = game_length;
    state->mp_weapon_set = getMPWeaponSet();
    state->mp_char[0] = player_char[0];
    state->mp_char[1] = player_char[1];
    state->mp_char[2] = player_char[2];
    state->mp_char[3] = player_char[3];
    state->mp_handi[0] = player_handicap[0];
    state->mp_handi[1] = player_handicap[1];
    state->mp_handi[2] = player_handicap[2];
    state->mp_handi[3] = player_handicap[3];
    state->mp_contstyle[0] = controlstyle_player[0];
    state->mp_contstyle[1] = controlstyle_player[1];
    state->mp_contstyle[2] = controlstyle_player[2];
    state->mp_contstyle[3] = controlstyle_player[3];
    state->aim_option = aim_sight_adjustment;
    state->mp_flags[0] = get_players_team_or_scenario_item_flag(0);
    state->mp_flags[1] = get_players_team_or_scenario_item_flag(1);
    state->mp_flags[2] = get_players_team_or_scenario_item_flag(2);
    state->mp_flags[3] = get_players_team_or_scenario_item_flag(3);
}


void ramromRestoreGameSettings(ramromfilestructure *state)
{
    g_randomSeed = state->randomseed;
    g_chrObjRandomSeed = state->randomizer;
    gamemode = state->mode;
    selected_num_players = state->numplayers;
    scenario = state->scenario;
    MP_stage_selected = state->mpstage_sel;
    game_length = state->gamelength;
    setMPWeaponSet(state->mp_weapon_set);
    player_char[0] = state->mp_char[0];
    player_char[1] = state->mp_char[1];
    player_char[2] = state->mp_char[2];
    player_char[3] = state->mp_char[3];
    player_handicap[0] = state->mp_handi[0];
    player_handicap[1] = state->mp_handi[1];
    player_handicap[2] = state->mp_handi[2];
    player_handicap[3] = state->mp_handi[3];
    controlstyle_player[0] = state->mp_contstyle[0];
    controlstyle_player[1] = state->mp_contstyle[1];
    controlstyle_player[2] = state->mp_contstyle[2];
    controlstyle_player[3] = state->mp_contstyle[3];
    aim_sight_adjustment = state->aim_option;
    set_players_team_or_scenario_item_flag(0, state->mp_flags[0]);
    set_players_team_or_scenario_item_flag(1, state->mp_flags[1]);
    set_players_team_or_scenario_item_flag(2, state->mp_flags[2]);
    set_players_team_or_scenario_item_flag(3, state->mp_flags[3]);
}


void ramromInitDemo(enum LEVELID arg0, enum DIFFICULTY arg1)
{
    if (g_NotSureWhatToCallThis)
    {
        set_selected_difficulty(g_ActiveDemoFile->difficulty);
        set_solo_and_ptr_briefing(g_ActiveDemoFile->stagenum);
        set_selected_foldernum_and_copy_demo_eeprom(&g_ActiveDemoFile->savefile);
        ramromCopyGameSettingBeforePlayback((ramromfilestructure *) (ramrom_data_target + 272));
        ramromRestoreGameSettings(g_ActiveDemoFile);
        g_IsDemoPlaying = TRUE;
        g_DemoHasStarted = TRUE;
        joySetPlaybackFunc(ramromTickDemo, g_ActiveDemoFile->size_cmds);
        joySetContDataIndex(1);
        g_NotSureWhatToCallThis = FALSE;
    }
}


void ramromStartPlayDemo(ramromfilestructure *demofile)
{
    address_demo_loaded = demofile;
    g_ActiveDemoFile = romCopyAligned(&ramrom_data_target, address_demo_loaded, sizeof(struct ramromfilestructure));
    address_demo_loaded += sizeof(ramromfilestructure);
    g_NotSureWhatToCallThis = TRUE;
    set_solo_and_ptr_briefing(g_ActiveDemoFile->stagenum);
    set_selected_difficulty(g_ActiveDemoFile->difficulty);
    frontChangeMenu(MENU_RUN_STAGE, 1);
}


void ramromFadeToTitle(void)
{
    if (bondviewGetCameraMode() != CAMERAMODE_FADE_TO_TITLE)
    {
        bondviewSetCameraMode(CAMERAMODE_FADE_TO_TITLE);
    }
}


void ramromStopDemoPlayback(void)
{
    if (g_DemoHasStarted)
    {
        ramromRestoreGameSettings(ramrom_data_target + 272);
        joySetPlaybackFunc(0, -1);
        joySetContDataIndex(0);
        g_DemoHasStarted = FALSE;
        g_IsDemoPlaying = FALSE;
    }
}


void ramromSelectDemoAndPlay(void)
{
    s32 numUnlockedDemos;
    s32 highestUnlockedStage;

    numUnlockedDemos = 0;
    highestUnlockedStage = fileGetHighestStageUnlockedAnyFolder();

    /**
     * Prevent demo spoilers by only playing demos that take place in unlocked stages.
     */
    while(ramrom_table[numUnlockedDemos].fdata != NULL && highestUnlockedStage >= ramrom_table[numUnlockedDemos].locked)
    {
        numUnlockedDemos++;
    }

    ramromStartPlayDemo(ramrom_table[randomGetNext() % numUnlockedDemos].fdata);
}


u32 check_ramrom_flags(void)
{
    if (ramromGetIsDemoPlaying())
    {
        return g_ActiveDemoFile->slotnum;

    }

    return 0;
}
