
#include <ultra64.h>
#include <os_extension.h>
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


struct ramrom_demo_entry
{
    ramromfilestructure *fdata;
    s32 locked;
};

struct ramrom_inputframe
{
    s8 stick_x;
    s8 stick_y;
    u8 button_low;
    u8 button_high;
};

struct ramrom_blockheader
{
    u8 speedframes;
    u8 count;
    u8 randseed;
    u8 check;
};

extern u64 g_randomSeed;
extern u64 g_chrObjRandomSeed;

struct ramrom_demo_entry ramrom_table[] = {
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

ramromfilestructure *g_ActiveDemoFile = NULL;
struct ramrom_blockheader *g_DemoBlockHeader = NULL;
struct ramrom_inputframe *g_DemoInputCursor = NULL;
bool g_IsDemoPlaying = FALSE;
bool g_DemoHasStarted = FALSE;
bool g_DemoLaunchPending = FALSE;


// Forward declarations

void ramromFadeToTitle(void);
s32 ramromTickDemo(struct contsample *inputsample, s32 sampleRingIndex);
void ramromCopyGameSettingBeforePlayback(ramromfilestructure *state);
void ramromRestoreGameSettings(ramromfilestructure *state);

// End forward declarations


void ramromClearPlaybackPointers(void)
{
    g_ActiveDemoFile = NULL;
    g_DemoBlockHeader = NULL;
    g_DemoInputCursor = NULL;
}


s32 ramromGetIsDemoPlaying(void)
{
    return g_IsDemoPlaying;
}


s32 ramromIsDemoLaunchPending(void)
{
    return g_DemoLaunchPending;
}


s32 ramromTickDemo(struct contsample *inputsample, s32 sampleRingIndex)
{
    s32 frameIndex;
    s32 controllerIndex;
    struct ramrom_inputframe *inputframe;

    /** 
     * Wrapping u8 sum of all input bytes + header fields must match blockheader->check. Used to ensure
     * the demo is not corrupted.
     */
    u8 blockChecksum;

    s32 numRecordedControllers;
    s32 framesInBlock;

    blockChecksum = 0;
    numRecordedControllers = (s32) g_ActiveDemoFile->size_cmds;
    framesInBlock = g_DemoBlockHeader->count;

    for (frameIndex = 0; frameIndex < framesInBlock; frameIndex++)
    {
        sampleRingIndex = (s32) (sampleRingIndex + 1) % CONTSAMPLE_LEN;

        for (controllerIndex = 0; controllerIndex < MAXCONTROLLERS; controllerIndex++)
        {
            if (controllerIndex < numRecordedControllers)
            {
                inputframe = g_DemoInputCursor + (frameIndex * numRecordedControllers) + controllerIndex;

                inputsample->pads[sampleRingIndex * 4 + controllerIndex].stick_x = inputframe->stick_x;
                inputsample->pads[sampleRingIndex * 4 + controllerIndex].stick_y = inputframe->stick_y;
                inputsample->pads[sampleRingIndex * 4 + controllerIndex].button = (inputframe->button_high << 8) | inputframe->button_low;

                blockChecksum += (u8)((u8)inputframe->stick_x + (u8)inputframe->stick_y + inputframe->button_low + inputframe->button_high);
            }
            else
            {
                inputsample->pads[sampleRingIndex * 4 + controllerIndex].stick_x = 0;
                inputsample->pads[sampleRingIndex * 4 + controllerIndex].stick_y = 0;
                inputsample->pads[sampleRingIndex * 4 + controllerIndex].button = 0;
            }
        }
    }

    if (g_DemoBlockHeader->randseed != (u8)g_randomSeed)
    {
        ramromFadeToTitle();
    }

    blockChecksum += (u8)((u8)g_DemoBlockHeader->speedframes + (u8)g_DemoBlockHeader->count + g_DemoBlockHeader->randseed);

    if (g_DemoBlockHeader->check != blockChecksum)
    {
        ramromFadeToTitle();
    }
    
    joySetContDataIndex(0);
    
    if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON))
    {
        ramromFadeToTitle();
        prev_keypresses = TRUE;
    }
    
    joySetContDataIndex(1);

    return sampleRingIndex;
}


void ramromAdvanceDemoStream(void)
{
    s32 demoEndTicks;
    s32 framesInBlock;
    
    g_DemoBlockHeader = romCopyAligned(ramrom_data_target + 504, address_demo_loaded, sizeof(struct ramrom_blockheader));

    framesInBlock = g_DemoBlockHeader->count;

    if (framesInBlock > 0)
    {
        g_DemoInputCursor = romCopyAligned(ramrom_data_target + 542, address_demo_loaded + 4, g_ActiveDemoFile->size_cmds * sizeof(struct ramrom_inputframe) * g_DemoBlockHeader->count);
    }

    framesInBlock = g_DemoBlockHeader->count;

    // No more frames are loading, demo has ended.
    if (framesInBlock == 0 && g_DemoBlockHeader->speedframes == 0)
    {
        ramromFadeToTitle();
    }
    else
    {
        address_demo_loaded += align_addr_even((g_ActiveDemoFile->size_cmds * sizeof(struct ramrom_inputframe) * g_DemoBlockHeader->count) + sizeof(struct ramrom_blockheader) + 1);
    }

    updateFrameCounters(g_DemoBlockHeader->speedframes);

    // Cut off 60 ticks early to account for the fade.
    demoEndTicks = g_ActiveDemoFile->totaltime_ms - 60;

    if ((g_GlobalTimer >= demoEndTicks) && ((g_GlobalTimer - g_ClockTimer) < demoEndTicks))
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
    if (g_DemoLaunchPending)
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
        g_DemoLaunchPending = FALSE;
    }
}


void ramromStartPlayDemo(ramromfilestructure *demofile)
{
    address_demo_loaded = demofile;
    g_ActiveDemoFile = romCopyAligned(&ramrom_data_target, address_demo_loaded, sizeof(struct ramromfilestructure));
    address_demo_loaded += sizeof(ramromfilestructure);
    g_DemoLaunchPending = TRUE;
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


u32 ramromGetActiveDemoSlot(void)
{
    if (ramromGetIsDemoPlaying())
    {
        return g_ActiveDemoFile->slotnum;

    }

    return 0;
}
