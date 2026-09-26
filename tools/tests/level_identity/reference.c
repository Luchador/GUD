/* Pre-refactor unlock rules, frozen for behavior comparison. */
static LEVEL_SOLO_SLOT legacyCheatUnlockSlot(int max)
{
    int stagenum;
    int unlock_count = 0;
    if (selected_folder_num >= 0 && selected_folder_num < 4)
    {
        for (stagenum = 0; stagenum < max; stagenum++)
            if (fileIsStageUnlockedAtDifficulty(selected_folder_num, stagenum, DIFFICULTY_AGENT) == 3)
                unlock_count++;
        if (max - 1 == unlock_count) return max - 1;
    }
    return SP_LEVEL_NONE;
}

STAGESTATUS legacyIsStageUnlocked(s32 foldernum, LEVEL_SOLO_SLOT levelid, DIFFICULTY difficulty)
{
    save_data* save;
    s32 i;

    if (( fileIsFolderValid(foldernum)) && (levelid >= SP_LEVEL_DAM && levelid < SP_LEVEL_MAX) && (difficulty >= DIFFICULTY_AGENT && difficulty < DIFFICULTY_MAX))
    {
        save = fileGetSaveForFoldernum(foldernum);

        if (save)
        {
            if ( fileGetSaveStageCompletedForDifficulty(save, levelid, difficulty))
            {
                return STAGESTATUS_COMPLETED; //found on first try, stage has been completed and a time saved.
            }

            if ((levelid == SP_LEVEL_AZTEC && difficulty < DIFFICULTY_SECRET) ||
                (levelid == SP_LEVEL_EGYPT && difficulty < DIFFICULTY_00))
            {
                return STAGESTATUS_LOCKED; //we cant possibly have a completed bonus stage below each set dificulty
            }

            //still cant find it, do a search (this is probably how a cheat can unlock stages without having to actualy do them all)
            for (i = difficulty; i < DIFFICULTY_MAX ; i++)
            {
                LEVEL_SOLO_SLOT istage;
                for (istage = SP_LEVEL_DAM; istage < levelid; istage++)
                {
                    if (! fileGetSaveStageCompletedForDifficulty(save, istage, i))
                    {
                        break;
                    }
                }
                //if the first uncomplete stage is not less than current
                if (levelid <= istage)
                {
                    return STAGESTATUS_UNLOCKED;
                }
            }

            // if we still cant find it
            if ((difficulty < DIFFICULTY_007) && (levelid < SP_LEVEL_AZTEC))
            {
                for (i = difficulty; i < DIFFICULTY_MAX; i++)
                {
                    if ( fileGetSaveStageCompletedForDifficulty(save, levelid - 1, i))
                    {
                        return STAGESTATUS_UNLOCKED;
                    }
                }
            }

            if (difficulty < DIFFICULTY_007)
            {
                for (i = SP_LEVEL_DAM; i < SP_LEVEL_AZTEC; i++)
                {
                    if (! fileGetSaveStageCompletedForDifficulty(save, i, DIFFICULTY_AGENT))
                    {
                        break;
                    }
                }
                //this cant actually fire an it?
                if (i >= SP_LEVEL_AZTEC)
                {
                    for (i = DIFFICULTY_AGENT; i < difficulty; i++)
                    {
                        if (! fileGetSaveStageCompletedForDifficulty(save, levelid, i))
                        {
                            break;
                        }
                    }

                    if (difficulty <= i)
                    {
                        return STAGESTATUS_UNLOCKED;
                    }
                }
            }// difficulty < DIFFICULTY_007
        }// save

        // no save, current level is dam, its unlocked.
        if (levelid == SP_LEVEL_DAM)
        {
            return STAGESTATUS_UNLOCKED;
        }
    }

    // After all that the stage is not unlocked
    return STAGESTATUS_LOCKED;
}
