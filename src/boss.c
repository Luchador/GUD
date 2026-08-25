#include <os_extension.h>
#include <PR/os.h>
#include "bondview.h"
#include <bondconstants.h>
#include <bondgame.h>
#include "boss.h"
#include "deb.h"
#include "explosion.h"
#include "game/dyn.h"
#include "game/game_debug.h"
#include "game/file.h"
#include "game/file2.h"
#include "init.h"
#include "joy.h"
#include "game/lv.h"
#include "game/front.h"
#include "mema.h"
#include "memp.h"
#include "music.h"
#include "game/ob.h"
#include "ramrom.h"
#include "random.h"
#include "rmon.h"
#include "sched.h"
#include "token.h"
#include "tlb_manage.h"
#include "fr.h"
#include "game/image.h"
#include "vi.h"
#include "game/bg.h"
#include "game/debugmenu_handler.h"
#include "game/lv.h"
#include "game/ramromreplay.h"
#include "game/rsp.h"
#include "game/stan.h"
#include "game/textrelated.h"
#include "game/player.h"
#include "game/frametiming.h"
#include "PR/R4300.h"

/**
 * @brief Main game loop and initialization functions.
 */


#define CYCLES_PER_FRAME    ((u32) OS_CPU_COUNTER / 60U) // 781,250
#define INTERVAL_INTER_MATH    (CYCLES_PER_FRAME / 2U) // 390,625

// note: 3225U * 5/6 = 2687.5
#define MAIN_LOOP_TICK_INTERVAL (INTERVAL_INTER_MATH - 2688U) // 387,937

typedef union {

    struct {
        short   type;
    } gen;

    struct {
        short   type;
    } done;

    OSScMsg      app;

} GFXMsg;

// Begin forward declarations

void bossMainloop(void);

// End forward declarations

u32 g_BossDebugNoticeEntry = 0;
s32 g_DebugAndUpdateStageFlag = FALSE;
s32 g_StageNum = LEVELID_TITLE;
u32 g_CurentMMallocValue = 0x234800;
u32 g_CurentMaMallocValue = 0x4B000;

struct memallocstring memallocstringtable[] = {
{ LEVELID_DAM,          "-ml0 -me0 -mgfx70  -mvtx50 -mt625 -ma275"},
{ LEVELID_FACILITY,     "-ml0 -me0 -mgfx70  -mvtx50 -mt650 -ma160"},
{ LEVELID_RUNWAY,       "-ml0 -me0 -mgfx100 -mvtx50 -mt610 -ma300"},
{ LEVELID_SURFACE,      "-ml0 -me0 -mgfx70  -mvtx50 -mt600 -ma300"},
{ LEVELID_BUNKER1,      "-ml0 -me0 -mgfx100 -mvtx50 -mt700 -ma150"},
{ LEVELID_SILO,         "-ml0 -me0 -mgfx70  -mvtx50 -mt660 -ma150"},
{ LEVELID_FRIGATE,      "-ml0 -me0 -mgfx70  -mvtx50 -mt750 -ma225"},
{ LEVELID_SURFACE2,     "-ml0 -me0 -mgfx100 -mvtx50 -mt550 -ma350"},
{ LEVELID_BUNKER2,      "-ml0 -me0 -mgfx100 -mvtx50 -mt725 -ma150"},
{ LEVELID_STATUE,       "-ml0 -me0 -mgfx70  -mvtx50 -mt750 -ma220"},
{ LEVELID_ARCHIVES,     "-ml0 -me0 -mgfx70  -mvtx50 -mt600 -ma250"},
{ LEVELID_STREETS,      "-ml0 -me0 -mgfx60  -mvtx40 -mt635 -ma290"},
{ LEVELID_DEPOT,        "-ml0 -me0 -mgfx60  -mvtx50 -mt710 -ma300"},
{ LEVELID_TRAIN,        "-ml0 -me0 -mgfx100 -mvtx50 -mt600 -ma200"},
{ LEVELID_JUNGLE,       "-ml0 -me0 -mgfx70  -mvtx50 -mt500 -ma200"},
{ LEVELID_CONTROL,      "-ml0 -me0 -mgfx70  -mvtx50 -mt671 -ma200"},
{ LEVELID_CRADLE,       "-ml0 -me0 -mgfx100 -mvtx50 -mt650 -ma250"},
{ LEVELID_CAVERNS,      "-ml0 -me0 -mgfx100 -mvtx50 -mt530 -ma250"},
{ LEVELID_AZTEC,        "-ml0 -me0 -mgfx60  -mvtx40 -mt855 -ma135"},
{ LEVELID_EGYPT,        "-ml0 -me0 -mgfx100 -mvtx50 -mt600 -ma250"},
{ LEVELID_CITADEL,      "-ml0 -me0 -mgfx100 -mvtx50 -mt650 -ma150"},
{ LEVELID_CUBA,         "-ml0 -me0 -mgfx100 -mvtx50 -mt300 -ma300"},
{ LEVELID_TITLE,        "-ml0 -me0 -mgfx80 -mvtx20 -mt646 -ma001"},
{ 0x5B,                 "-ml0 -me0 -mgfx60 -mvtx20 -mt500 -ma001"},
{ 0x63,                 "-ml0 -me0 -mgfx60 -mvtx20 -mt500 -ma001"},
{ LEVELID_TEMPLE,       "-ml0 -me0 -mgfx130 -mvtx100 -mt390 -ma100"},
{ LEVELID_COMPLEX,      "-ml0 -me0 -mgfx130 -mvtx100 -mt400 -ma100"},
{ LEVELID_LIBRARY,      "-ml0 -me0 -mgfx130 -mvtx100 -mt400 -ma100"},
{ LEVELID_BASEMENT,     "-ml0 -me0 -mgfx130 -mvtx100 -mt400 -ma100"},
{ LEVELID_STACK,        "-ml0 -me0 -mgfx130 -mvtx100 -mt400 -ma100"},
{ LEVELID_CAVES,        "-ml0 -me0 -mgfx130 -mvtx100 -mt400 -ma300"},
{ LEVELID_BUNKER2_MP ,  "-ml0 -me0 -mgfx130 -mvtx100 -mt550 -ma170"},
{ LEVELID_ARCHIVES_MP , "-ml0 -me0 -mgfx80  -mvtx100 -mt550 -ma250"},
{ LEVELID_CAVERNS_MP ,  "-ml0 -me0 -mgfx130 -mvtx100 -mt440 -ma220"},
{ LEVELID_FACILITY_MP , "-ml0 -me0 -mgfx90  -mvtx100 -mt550 -ma230"},
{ LEVELID_EGYPT_MP ,    "-ml0 -me0 -mgfx110 -mvtx100 -mt350 -ma400"},
{ LEVELID_DEFAULT,      "-ml0 -me0 -mgfx100 -mvtx50 -mt700 -ma400"},
{ 0x0, },
{ 0x0, },
{ 0x0, }
};

s32 g_MainStageNum = LEVELID_NONE;

OSScMsg g_bossGfxDoneMsg = { OS_SC_DONE_MSG };


// extern declarations
extern struct player *g_CurrentPlayer;

void bossInitMainthreadData(void)
{
    OSMesg bossmsg;
    OSTimer bosstimer;
    OSMesgQueue bossmq;
    u32 start;
    s32 i;

    debInit();
    romCreateMesgQueue();
    tlbmanageEstablishManagementTable();
    image_entries_load();
    viInit();
    viInitVideoSettings();
    g_DebugAndUpdateStageFlag = rmonGetToken();
    obInit();
    joyInit();
    osCreateMesgQueue(&bossmq, &bossmsg, 1);

    for (i = 0; i != MAXCONTROLLERS; i++)
    {
        osSetTimer(&bosstimer, OS_USEC_TO_CYCLES(100000), 0, &bossmq, &bossmsg);
        osRecvMesg(&bossmq, &bossmsg, OS_MESG_BLOCK);

        if (i == 1)
        {
            joyCheckStatusThreadSafe();
        }
        else if (i >= 2)
        {
            joyConsumeSamplesWrapper();
        }
    }

    if (tokenFind(1, "-level_") == 0)
    {
        g_DebugAndUpdateStageFlag = 1;
    }

    if (g_DebugAndUpdateStageFlag != 0)
    {
        tokenSetString("          -ml0 -me0 -mgfx100 -mvtx50 -mt700 -ma400");
    }

    if (tokenFind(1, "-m") != 0)
    {
        g_CurentMMallocValue = (s32) (strtol(tokenFind(1, "-m"), 0, 0) << 0xa);
    }

    start = (PHYS_TO_K0(osVirtualToPhysical(&_bssSegmentEnd)));

    mempCheckMemflagTokens(start, ((u32)tlbmanageGetTlbAllocatedBlock() - (u32)start));
    mempResetBank(MEMPOOL_PERMANENT);
    langInit();
    lvInit();
    playerSetDefaultPerspectiveAndHeight();
    store_osgetcount();
    explosionInitScaleMtx();
    casingsInit();
    alloc_load_expand_ani_table();
    init_weapon_animation_groups_maybe();
    reset_counter_rand_body_head();
    init_player_gait_object();
    initGameData();
    fileResetRamRomSave();
    ramromClearPlaybackPointers();
}


/**
 * Begin infinite main program loop.
 */
void bossEntry(void)
{
    bossInitMainthreadData();
    rspAllocateBuffers();
    musicSeqPlayerInit();

    while(1)
    {
       bossMainloop();
    }
}

/**
 * Main program loop.
 */
void bossMainloop(void)
{
    const unsigned char *tokenFindLevel;
    GFXMsg *localGfxFrameMsg;
    OSScMsg localGfxDoneMsg;
    s32 stringIndex;
    Gfx *gdl;
    Gfx *firstGdl;
    u32 nowCount;
    s32 i;
    s32 count;
    s8 joyStickXPos;
    s8 joyStickYPos;
    u16 joyButtons;
    struct player *localPlayer;
    s32 localSelectedNumPlayers;
    u32 pendingGfx = 0;
    s32 freeGfx;
    s32 mainTickElapsed;
    s32 rspReplyMsg;

    reset_mem_bank_5();

    if (tokenFind(1, "-level_") != NULL)
    {
        tokenFindLevel = (const unsigned char *)tokenFind(1, "-level_");

        // quick hack strltolon, converts the two digit ASCII level to 32bit int
        // eg, "09" = 48*10 + 57 - 528 = 9
        g_StageNum = (((s32)(tokenFindLevel[0] * 10) + (s32)tokenFindLevel[1]) - 0x210);
    }

    if (g_StageNum != LEVELID_TITLE)
    {
        fileValidateSaves();
        fileSetCurrentFolder(FOLDER1);
        set_selected_difficulty(DIFFICULTY_AGENT);
        set_solo_and_ptr_briefing(g_StageNum);

        if (tokenFind(1, "-hard"))
        {
            // convert ASCII difficulty value to int in set difficulty calls eg '1' = 49, 49-48 = 1
            set_selected_difficulty(*(const unsigned char*)tokenFind(1, "-hard") - '0');
            lvSetSelectedDifficulty(*(const unsigned char*)tokenFind(1, "-hard") - '0');
        }
    }

    nowCount = osGetCount();
    randomSetSeed(nowCount);

    // Infinite game loop.
    while (1)
    {
        localGfxFrameMsg = NULL;
        localGfxDoneMsg = g_bossGfxDoneMsg;
        pendingGfx = 0;

        ramromInitDemo(g_StageNum, lvGetSelectedDifficulty());

        if (g_DebugAndUpdateStageFlag)
        {
            stringIndex = -1;

            if (g_StageNum != LEVELID_TITLE && get_selected_num_players() >= 2)
            {
                stringIndex = 0;

                while(memallocstringtable[stringIndex].id)
                {
                    if (memallocstringtable[stringIndex].id == (g_StageNum + 400))
                    {
                        break;
                    }

                    stringIndex++;
                }

                if (memallocstringtable[stringIndex].id == 0)
                {
                    stringIndex = -1;
                }
            }

            if (stringIndex < 0)
            {
                stringIndex = 0;

                while(memallocstringtable[stringIndex].id)
                {
                    if (memallocstringtable[stringIndex].id == g_StageNum)
                    {
                        break;
                    }

                    stringIndex++;
                }
            }

            tokenSetString(memallocstringtable[stringIndex].string);
        }

        mempResetBank(MEMPOOL_STAGE);
        obBlankResourcesLoadedInBank(MEMPOOL_STAGE);

        if (tokenFind(1, "-ma"))
        {
            g_CurentMaMallocValue = (s32) (strtol(tokenFind(1, "-ma"), NULL, 0) * 1024);
        }

        memaReset(mempAllocBytesInBank(g_CurentMaMallocValue, MEMPOOL_STAGE), g_CurentMaMallocValue);
        reset_play_data_ptrs();

        localSelectedNumPlayers = 0;

        if (g_StageNum != LEVELID_TITLE)
        {
            localSelectedNumPlayers = 1;

            if (get_selected_num_players() >= 2)
            {
                localSelectedNumPlayers = get_selected_num_players();
            }
        }

        init_player_data_ptrs_construct_viewports(localSelectedNumPlayers);
        dynInitMemory();
        joyCheckStatusThreadSafe();
        lvlStageLoad(g_StageNum);
        viInitBuffers();
        waitForNextFrame();

        while (g_MainStageNum < 0 || pendingGfx != 0)
        {
            osRecvMesg(&gfxFrameMsgQ, (OSMesg *)&localGfxFrameMsg, OS_MESG_BLOCK);

            switch (localGfxFrameMsg->gen.type)
            {
                case (OS_SC_RETRACE_MSG):
                {
                    mainTickElapsed = (u32) (osGetCount() - copy_of_osgetcount_value_1);

                    if (mainTickElapsed < MAIN_LOOP_TICK_INTERVAL)
                    {
                        // nothing to do.
                    }
                    else
                    {
                        if (g_MainStageNum < 0 && pendingGfx < 2U)
                        {
                            if (ramromGetIsDemoPlaying())
                            {
                                ramromAdvanceDemoStream();
                            }
                            else
                            {
                                waitForNextFrame();
                            }

                            joyConsumeSamplesWrapper();
                            permit_stderr(0);

                            gdl = firstGdl = dynGetMasterDisplayList();

                            // Primary game tick function.

                            { /* TEMP profiler */
                                u32 prof_t = osGetCount();
                                lvTick();
                                g_ProfLvlTickCycles = osGetCount() - prof_t;
                            }

                            shuffle_player_ids();

                            if (g_StageNum != LEVELID_TITLE)
                            {
                                for (i = 0; i < getPlayerCount(); i++)
                                {
                                    set_cur_player(get_nth_player_from_shuffled(i));

                                    localPlayer = g_CurrentPlayer;
                                    viSetViewSize(localPlayer->viewx, localPlayer->viewy);

                                    localPlayer = g_CurrentPlayer;
                                    viSetViewPosition(localPlayer->viewleft, localPlayer->viewtop);

                                    lvlViewMoveTick();
                                }
                            }

                            // Primary game rendering function.
                            
                            { /* TEMP profiler */
                                u32 prof_t = osGetCount();
                                gdl = lvRender(gdl);
                                g_ProfLvlRenderCycles = osGetCount() - prof_t;
                            }

                            gdl = lvDrawFrameRateDisplay(gdl);
                            
                            gDPFullSync(gdl++);
                            gSPEndDisplayList(gdl++);

                            freeGfx = dynGetFreeGfx2(gdl);
                            dynSwapBuffers();
                            video_related_8();

                            rspReplyMsg = (s32)(&localGfxDoneMsg);
                            rspGfxTaskStart(firstGdl, gdl, 0, (s32*)rspReplyMsg);

                            pendingGfx++;
                            memaSingleDefragPass();
                        }
                    }
                }
                break;

                case (OS_SC_DONE_MSG):
                    pendingGfx--;
                    break;

                case OS_SC_PRE_NMI_MSG: // message when the console's power is being cut off
                    pendingGfx = 4U;    // this stops any further graphics processing
                    break;
            }
        }

        lvlUnloadStageTextData();
        ramromStopDemoPlayback();
        mempNullNextEntryInBank(MEMPOOL_STAGE);
        obBlankResourcesLoadedInBank(MEMPOOL_STAGE);

        g_StageNum = g_MainStageNum;
        g_MainStageNum = LEVELID_NONE;
    }

    sub_GAME_7F0D1A7C();
}


/**
 * 7530    70006930
 *     run title [0x5A->loaded stage#]; fry AT
 *     redirect to 70006950: A0=0x5A
 */
void bossRunTitleStage(void) {
    bossSetLoadedStage(LEVELID_TITLE);
}

/**
 * 7550    70006950
 *     A0->loaded stage# [800242FC]; fry AT
 *     0x5A jumps to folder select
 *     0x5B
 *     0x63
 */
void bossSetLoadedStage(LEVELID stage){
    g_MainStageNum = stage;
}

/**
 * 755C    7000695C
 *     V0= stage# [800241A8]
 */
LEVELID bossGetStageNum() {
    return g_StageNum;
}

/**
 * 7568    70006968
 *     return to title screen from stage
 */
void bossReturnTitleStage(void) {
#ifdef BUGFIX_R1
    display_objective_status_text_on_status_change();
    objectivestatusDisableStatusDisplay();
#endif
    if ((bossGetStageNum() != LEVELID_CUBA) && (objectiveIsAllComplete() != 0x0)) {
        end_of_mission_briefing();
    }
    bossRunTitleStage();
}
