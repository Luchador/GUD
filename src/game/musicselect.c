#include <ultra64.h>
#include "musicselect.h"
#include "bondconstants.h"
#include "lv.h"
#include "random.h"

s16 random_tracks[] = {
    M_TRAIN,
    M_DEPOT,
    M_MPTHEME,
    M_CITADEL,
    M_FACILITY,
    M_CONTROL,
    M_DAM,
    M_FRIGATE,
    M_ARCHIVES,
    M_SILO,
    M_MPTHEME2,
    M_STREETS,
    M_BUNKER1,
    M_BUNKER2,
    M_STATUE,
    M_ELEVATOR_CONTROL,
    M_CRADLE,
    M_ELEVATOR_WC,
    M_EGYPTIAN,
    M_AZTEC,
    M_WATERCAVERNS,
    M_SURFACE2,
    M_TRAINX,
    M_FACILITYX,
    M_DEPOTX,
    M_CONTROLX,
    M_WATERCAVERNSX,
    M_DAMX,
    M_FRIGATEX,
    M_ARCHIVESX,
    M_SILOX,
    M_STREETSX,
    M_BUNKER1X,
    M_BUNKER2X,
    M_JUNGLEX,
    M_STATUEX,
    M_AZTECX,
    M_EGYPTX,
    M_CRADLEX,
    M_RUNWAY,
    M_MPTHEME3,
    M_RUNWAYX,
    M_SURFACE1,
    M_NONE
};


/**
 * Returns the stage's assigned music track, or a random track
 * from random_tracks[] if the stage's main_music entry is -1. Also returns
 * a random track if the stage is not in g_LevelInfoTable.
 */
s32 musicGetMainTrackOrRandom(s32 stageID)
{
    struct LevelEntry *levelInfo;

    levelInfo = lvFindLevelInfo(stageID);

    if (levelInfo != NULL && levelInfo->main_music != -1)
    {
        return levelInfo->main_music;
    }

    /**
     * Stage not in g_LevelInfoTable or listed as random.
     * Select a random entry from random_tracks[].
     */
    {
        s32 count = 0;

        while (random_tracks[count] != M_NONE)
        {
            count++;
        }

        return random_tracks[randomGetNext() % count];
    }
}


s32 musicGetAmbientTrackForStage(s32 stageID)
{
    struct LevelEntry *levelInfo;

    levelInfo = lvFindLevelInfo(stageID);

    return levelInfo != NULL ? levelInfo->bg_sound : -1;
}


s32 musicGetXTrackForStage(s32 stageID)
{
    struct LevelEntry *levelInfo;

    levelInfo = lvFindLevelInfo(stageID);

    return levelInfo != NULL ? levelInfo->xtrack : -1;
}
