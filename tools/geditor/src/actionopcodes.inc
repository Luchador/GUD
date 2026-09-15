/* Native command layouts from src/aicommands.def; checked by tests/action_blocks/run.py. */
    {"GotoNext", "Continue at next section", 2, 1, {{"LABEL", 1, 1, ACTION_LABEL}}}, /* 00 */
    {"GotoFirst", "Continue at first section", 2, 1, {{"LABEL", 1, 1, ACTION_LABEL}}}, /* 01 */
    {"Label", "Section", 2, 1, {{"ID", 1, 1, ACTION_NUMBER}}}, /* 02 */
    {"Yield", "Wait for next AI update", 1, 0, {{0}}}, /* 03 */
    {"EndList", "End of block", 1, 0, {{0}}}, /* 04 */
    {"SetChrAiList", "Switch character behavior", 4, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"AI_LIST_ID", 2, 2, ACTION_BLOCK}}}, /* 05 */
    {"SetReturnAiList", "Set return behavior", 3, 1, {{"AI_LIST_ID", 1, 2, ACTION_BLOCK}}}, /* 06 */
    {"Return", "Switch to return behavior", 1, 0, {{0}}}, /* 07 */
    {"Stop", "Stop", 1, 0, {{0}}}, /* 08 */
    {"Kneel", "Kneel", 1, 0, {{0}}}, /* 09 */
    {"PlayAnimation", "Play animation", 9, 5, {{"ANIMATION_ID", 1, 2, ACTION_NUMBER}, {"START_TIME30", 3, 2, ACTION_SECONDS30}, {"END_TIME30", 5, 2, ACTION_SECONDS30}, {"BITFIELD", 7, 1, ACTION_MASK}, {"INTERPOL_TIME60", 8, 1, ACTION_SECONDS60}}}, /* 0A */
    {"IFPlayingAnimation", "If playing animation", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 0B */
    {"PointAtBond", "Point at Bond", 1, 0, {{0}}}, /* 0C */
    {"LookSurprised", "Look surprised", 1, 0, {{0}}}, /* 0D */
    {"TRYSidestepping", "Try to sidestep", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 0E */
    {"TRYSideHopping", "Try to hop sideways", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 0F */
    {"TRYSideRunning", "Try to run sideways", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 10 */
    {"TRYFiringWalk", "Try to fire while walking", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 11 */
    {"TRYFiringRun", "Try to fire while running", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 12 */
    {"TRYFiringRoll", "Try to fire while rolling", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 13 */
    {"TRYFireOrAimAtTarget", "Try to fire or aim at a target", 6, 3, {{"BITFIELD", 1, 2, ACTION_MASK}, {"TARGET", 3, 2, ACTION_NUMBER}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 14 */
    {"TRYFireOrAimAtTargetKneel", "Try to kneel and fire or aim at a target", 6, 3, {{"BITFIELD", 1, 2, ACTION_MASK}, {"TARGET", 3, 2, ACTION_NUMBER}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 15 */
    {"TRYFireOrAimAtTargetUpdate", "Try to update firing target", 6, 3, {{"BITFIELD", 1, 2, ACTION_MASK}, {"TARGET", 3, 2, ACTION_NUMBER}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 16 */
    {"TRYFacingTarget", "Try to face a target", 6, 3, {{"BITFIELD", 1, 2, ACTION_MASK}, {"TARGET", 3, 2, ACTION_NUMBER}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 17 */
    {"HitChrWithItem", "Damage a character with an item", 4, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"PART_NUM", 2, 1, ACTION_NUMBER}, {"ITEM_NUM", 3, 1, ACTION_NUMBER}}}, /* 18 */
    {"ChrHitChr", "Have one character damage another", 4, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"CHR_NUM_TARGET", 2, 1, ACTION_CHARACTER}, {"PART_NUM", 3, 1, ACTION_NUMBER}}}, /* 19 */
    {"TRYThrowingGrenade", "Try to throw a grenade", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 1A */
    {"TRYDroppingItem", "Try to drop an item", 5, 3, {{"PROP_NUM", 1, 2, ACTION_NUMBER}, {"ITEM_NUM", 3, 1, ACTION_NUMBER}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* 1B */
    {"RunToPad", "Run to pad", 3, 1, {{"PAD", 1, 2, ACTION_PAD}}}, /* 1C */
    {"RunToPadPreset", "Run to pad preset", 1, 0, {{0}}}, /* 1D */
    {"WalkToPad", "Walk to pad", 3, 1, {{"PAD", 1, 2, ACTION_PAD}}}, /* 1E */
    {"SprintToPad", "Sprint to pad", 3, 1, {{"PAD", 1, 2, ACTION_PAD}}}, /* 1F */
    {"StartPatrol", "Start patrol", 2, 1, {{"PATH_NUM", 1, 1, ACTION_NUMBER}}}, /* 20 */
    {"Surrender", "Surrender", 1, 0, {{0}}}, /* 21 */
    {"RemoveMe", "Remove this character", 1, 0, {{0}}}, /* 22 */
    {"ChrRemoveInstant", "Character remove instant", 2, 1, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}}}, /* 23 */
    {"TRYTriggeringAlarmAtPad", "Try triggering alarm at pad", 4, 2, {{"PAD", 1, 2, ACTION_PAD}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 24 */
    {"AlarmOn", "Alarm on", 1, 0, {{0}}}, /* 25 */
    {"AlarmOff", "Alarm off", 1, 0, {{0}}}, /* 26 */
    {"TRYRunFromBond", "Try run from Bond", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 27 */
    {"TRYRunToBond", "Try run to Bond's position", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 28 */
    {"TRYWalkToBond", "Try walk to Bond's position", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 29 */
    {"TRYSprintToBond", "Try sprint to Bond's position", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 2A */
    {"TRYFindCover", "Try find cover", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 2B */
    {"TRYRunToChr", "Try run to character", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 2C */
    {"TRYWalkToChr", "Try walk to character", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 2D */
    {"TRYSprintToChr", "Try sprint to character", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 2E */
    {"IFImOnPatrolOrStopped", "If this character is on patrol or stopped", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 2F */
    {"IFChrDyingOrDead", "If character dying or dead", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 30 */
    {"IFChrDoesNotExist", "If character does not exist", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 31 */
    {"IFISeeBond", "If this character has stopped moving or is patrolling", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 32 */
    {"SetNewRandom", "Draw a new random value", 1, 0, {{0}}}, /* 33 */
    {"IFRandomLessThan", "If stored random value is less than", 3, 2, {{"BYTE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 34 */
    {"IFRandomGreaterThan", "If stored random value is greater than", 3, 2, {{"BYTE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 35 */
    {"IFICanHearAlarm", "If ican hear alarm", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 36 */
    {"IFAlarmIsOn", "If alarm is on", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 37 */
    {"IFGasIsLeaking", "If gas is leaking", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 38 */
    {"IFIHeardBond", "If iheard Bond", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 39 */
    {"IFISeeSomeoneShot", "If isee someone shot", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 3A */
    {"IFISeeSomeoneDie", "If isee someone die", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 3B */
    {"IFICouldSeeBond", "If icould see Bond", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 3C */
    {"IFICouldSeeBondsStan", "If icould see bonds stan", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 3D */
    {"IFIWasShotRecently", "If iwas shot recently", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 3E */
    {"IFIHeardBondRecently", "If iheard Bond recently", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 3F */
    {"IFImInRoomWithChr", "If this character is in room with character", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 40 */
    {"IFIveNotBeenSeen", "If ive not been seen", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 41 */
    {"IFImOnScreen", "If this character is on screen", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 42 */
    {"IFMyRoomIsOnScreen", "If this character's room is on screen", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 43 */
    {"IFRoomWithPadIsOnScreen", "If room with pad is on screen", 4, 2, {{"PAD", 1, 2, ACTION_PAD}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 44 */
    {"IFImTargetedByBond", "If Bond is targeting this character", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 45 */
    {"IFBondMissedMe", "If Bond narrowly missed this character", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 46 */
    {"IFMyAngleToBondLessThan", "If this character's angle to Bond less than", 3, 2, {{"ANGLE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 47 */
    {"IFMyAngleToBondGreaterThan", "If this character's angle to Bond greater than", 3, 2, {{"ANGLE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 48 */
    {"IFMyAngleFromBondLessThan", "If this character's angle from Bond less than", 3, 2, {{"ANGLE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 49 */
    {"IFMyAngleFromBondGreaterThan", "If this character's angle from Bond greater than", 3, 2, {{"ANGLE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 4A */
    {"IFMyDistanceToBondLessThanDecimeter", "If distance to Bond is less than", 4, 2, {{"DISTANCE", 1, 2, ACTION_METERS}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 4B */
    {"IFMyDistanceToBondGreaterThanDecimeter", "If distance to Bond is greater than", 4, 2, {{"DISTANCE", 1, 2, ACTION_METERS}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 4C */
    {"IFChrDistanceToPadLessThanDecimeter", "If character distance to pad less than", 7, 4, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"DISTANCE", 2, 2, ACTION_METERS}, {"PAD", 4, 2, ACTION_PAD}, {"GOTOLABEL", 6, 1, ACTION_LABEL}}}, /* 4D */
    {"IFChrDistanceToPadGreaterThanDecimeter", "If character distance to pad greater than", 7, 4, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"DISTANCE", 2, 2, ACTION_METERS}, {"PAD", 4, 2, ACTION_PAD}, {"GOTOLABEL", 6, 1, ACTION_LABEL}}}, /* 4E */
    {"IFMyDistanceToChrLessThanDecimeter", "If this character's distance to character less than", 5, 3, {{"DISTANCE", 1, 2, ACTION_METERS}, {"CHR_NUM", 3, 1, ACTION_CHARACTER}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* 4F */
    {"IFMyDistanceToChrGreaterThanDecimeter", "If this character's distance to character greater than", 5, 3, {{"DISTANCE", 1, 2, ACTION_METERS}, {"CHR_NUM", 3, 1, ACTION_CHARACTER}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* 50 */
    {"TRYSettingMyPresetToChrWithinDistanceDecimeter", "Try to choose a nearby character as the preset", 4, 2, {{"DISTANCE", 1, 2, ACTION_METERS}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 51 */
    {"IFBondDistanceToPadLessThanDecimeter", "If Bond distance to pad less than", 6, 3, {{"DISTANCE", 1, 2, ACTION_METERS}, {"PAD", 3, 2, ACTION_PAD}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 52 */
    {"IFBondDistanceToPadGreaterThanDecimeter", "If Bond distance to pad greater than", 6, 3, {{"DISTANCE", 1, 2, ACTION_METERS}, {"PAD", 3, 2, ACTION_PAD}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 53 */
    {"IFChrInRoomWithPad", "If character in room with pad", 5, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"PAD", 2, 2, ACTION_PAD}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* 54 */
    {"IFBondInRoomWithPad", "If Bond in room with pad", 4, 2, {{"PAD", 1, 2, ACTION_PAD}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 55 */
    {"IFBondCollectedObject", "If Bond collected object", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 56 */
    {"IFKeyDropped", "If key dropped", 3, 2, {{"KEY_ID", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 57 */
    {"IFItemIsAttachedToObject", "If item is attached to object", 4, 3, {{"ITEM_NUM", 1, 1, ACTION_NUMBER}, {"OBJECT_TAG", 2, 1, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 58 */
    {"IFBondHasItemEquipped", "If Bond has item equipped", 3, 2, {{"ITEM_NUM", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 59 */
    {"IFObjectExists", "If object exists", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 5A */
    {"IFObjectNotDestroyed", "If object not destroyed", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 5B */
    {"IFObjectWasActivated", "If object was activated", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 5C */
    {"IFBondUsedGadgetOnObject", "If Bond used gadget on object", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 5D */
    {"ActivateObject", "Activate object", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* 5E */
    {"DestroyObject", "Destroy object", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* 5F */
    {"DropObject", "Drop object", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* 60 */
    {"ChrDropAllConcealedItems", "Character drop all concealed items", 2, 1, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}}}, /* 61 */
    {"ChrDropAllHeldItems", "Character drop all held items", 2, 1, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}}}, /* 62 */
    {"BondCollectObject", "Bond collect object", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* 63 */
    {"ChrEquipObject", "Character equip object", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"CHR_NUM", 2, 1, ACTION_CHARACTER}}}, /* 64 */
    {"MoveObject", "Move object", 4, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"PAD", 2, 2, ACTION_PAD}}}, /* 65 */
    {"DoorOpen", "Door open", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* 66 */
    {"DoorClose", "Door close", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* 67 */
    {"IFDoorStateEqual", "If door state equal", 4, 3, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"DOOR_STATE", 2, 1, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 68 */
    {"IFDoorHasBeenOpenedBefore", "If door has been opened before", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 69 */
    {"DoorSetLock", "Door set lock", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"LOCK_FLAG", 2, 1, ACTION_MASK}}}, /* 6A */
    {"DoorUnsetLock", "Door unset lock", 3, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"LOCK_FLAG", 2, 1, ACTION_MASK}}}, /* 6B */
    {"IFDoorLockEqual", "If door lock equal", 4, 3, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"LOCK_FLAG", 2, 1, ACTION_MASK}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 6C */
    {"IFObjectiveNumComplete", "If objective num complete", 3, 2, {{"OBJ_NUM", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 6D */
    {"TRYUnknown6e", "Unknown test 6E", 3, 2, {{"UNKNOWN_FLAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 6E */
    {"TRYUnknown6f", "Unknown test 6F", 3, 2, {{"UNKNOWN_FLAG", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 6F */
    {"IFGameDifficultyLessThan", "If game difficulty less than", 3, 2, {{"DIFICULTY_ID", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 70 */
    {"IFGameDifficultyGreaterThan", "If game difficulty greater than", 3, 2, {{"DIFICULTY_ID", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 71 */
    {"IFMissionTimeLessThan", "If mission time less than", 4, 2, {{"SECONDS", 1, 2, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 72 */
    {"IFMissionTimeGreaterThan", "If mission time greater than", 4, 2, {{"SECONDS", 1, 2, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 73 */
    {"IFSystemPowerTimeLessThan", "If system power time less than", 4, 2, {{"MINUTES", 1, 2, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 74 */
    {"IFSystemPowerTimeGreaterThan", "If system power time greater than", 4, 2, {{"MINUTES", 1, 2, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 75 */
    {"IFLevelIdLessThan", "If level id less than", 3, 2, {{"LEVEL_ID", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 76 */
    {"IFLevelIdGreaterThan", "If level id greater than", 3, 2, {{"LEVEL_ID", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 77 */
    {"IFMyNumArghsLessThan", "If this character's num arghs less than", 3, 2, {{"HIT_NUM", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 78 */
    {"IFMyNumArghsGreaterThan", "If this character's num arghs greater than", 3, 2, {{"HIT_NUM", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 79 */
    {"IFMyNumCloseArghsLessThan", "If this character's num close arghs less than", 3, 2, {{"MISSED_NUM", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 7A */
    {"IFMyNumCloseArghsGreaterThan", "If this character's num close arghs greater than", 3, 2, {{"MISSED_NUM", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 7B */
    {"IFChrHealthLessThan", "If character health less than", 4, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"HEALTH", 2, 1, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 7C */
    {"IFChrHealthGreaterThan", "If character health greater than", 4, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"HEALTH", 2, 1, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 7D */
    {"IFChrWasDamagedSinceLastCheck", "If character was damaged since last check", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 7E */
    {"IFBondHealthLessThan", "If Bond health less than", 3, 2, {{"HEALTH", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 7F */
    {"IFBondHealthGreaterThan", "If Bond health greater than", 3, 2, {{"HEALTH", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 80 */
    {"SetMyMorale", "Set this character's morale", 2, 1, {{"CHRBYTE", 1, 1, ACTION_NUMBER}}}, /* 81 */
    {"AddToMyMorale", "Add to this character's morale", 2, 1, {{"CHRBYTE", 1, 1, ACTION_NUMBER}}}, /* 82 */
    {"SubtractFromMyMorale", "Subtract from this character's morale", 2, 1, {{"CHRBYTE", 1, 1, ACTION_NUMBER}}}, /* 83 */
    {"IFMyMoraleLessThan", "If this character's morale less than", 3, 2, {{"CHRBYTE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 84 */
    {"IFMyMoraleLessThanRandom", "If this character's morale less than random", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 85 */
    {"SetMyAlertness", "Set this character's alertness", 2, 1, {{"CHRBYTE", 1, 1, ACTION_NUMBER}}}, /* 86 */
    {"AddToMyAlertness", "Add to this character's alertness", 2, 1, {{"CHRBYTE", 1, 1, ACTION_NUMBER}}}, /* 87 */
    {"SubtractFromMyAlertness", "Subtract from this character's alertness", 2, 1, {{"CHRBYTE", 1, 1, ACTION_NUMBER}}}, /* 88 */
    {"IFMyAlertnessLessThan", "If this character's alertness less than", 3, 2, {{"CHRBYTE", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 89 */
    {"IFMyAlertnessLessThanRandom", "If this character's alertness less than random", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* 8A */
    {"SetMyHearingScale", "Set this character's hearing scale", 3, 1, {{"HEARING_SCALE", 1, 2, ACTION_NUMBER}}}, /* 8B */
    {"SetMyVisionRange", "Set this character's vision range", 2, 1, {{"VISION_RANGE", 1, 1, ACTION_NUMBER}}}, /* 8C */
    {"SetMyGrenadeProbability", "Set this character's grenade probability", 2, 1, {{"GRENADE_PROB", 1, 1, ACTION_NUMBER}}}, /* 8D */
    {"SetMyChrNum", "Change this character's ID", 2, 1, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}}}, /* 8E */
    {"SetMyHealthTotal", "Set this character's health total", 3, 1, {{"HEALTH", 1, 2, ACTION_NUMBER}}}, /* 8F */
    {"SetMyArmour", "Set this character's armour", 3, 1, {{"AMOUNT", 1, 2, ACTION_NUMBER}}}, /* 90 */
    {"SetMySpeedRating", "Set this character's speed rating", 2, 1, {{"SPEED_RATING", 1, 1, ACTION_NUMBER}}}, /* 91 */
    {"SetMyArghRating", "Set this character's argh rating", 2, 1, {{"ARGH_RATING", 1, 1, ACTION_NUMBER}}}, /* 92 */
    {"SetMyAccuracyRating", "Set this character's accuracy rating", 2, 1, {{"ACCURACY_RATING", 1, 1, ACTION_NUMBER}}}, /* 93 */
    {"SetMyFlags2", "Set this character's flags2", 2, 1, {{"BITS", 1, 1, ACTION_MASK}}}, /* 94 */
    {"UnsetMyFlags2", "Unset this character's flags2", 2, 1, {{"BITS", 1, 1, ACTION_MASK}}}, /* 95 */
    {"IFMyFlags2Has", "If this character's flags2 has", 3, 2, {{"BITS", 1, 1, ACTION_MASK}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* 96 */
    {"SetChrBitfield", "Set character bitfield", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"BITS", 2, 1, ACTION_MASK}}}, /* 97 */
    {"UnsetChrBitfield", "Unset character bitfield", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"BITS", 2, 1, ACTION_MASK}}}, /* 98 */
    {"IFChrBitfieldHas", "If character bitfield has", 4, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"BITS", 2, 1, ACTION_MASK}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* 99 */
    {"SetObjectiveBitfield", "Set objective bitfield", 5, 1, {{"BITFIELD", 1, 4, ACTION_MASK}}}, /* 9A */
    {"UnsetObjectiveBitfield", "Unset objective bitfield", 5, 1, {{"BITFIELD", 1, 4, ACTION_MASK}}}, /* 9B */
    {"IFObjectiveBitfieldHas", "If objective bitfield has", 6, 2, {{"BITS", 1, 4, ACTION_MASK}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 9C */
    {"SetMychrflags", "Set this character's flags", 5, 1, {{"CHRFLAGS", 1, 4, ACTION_MASK}}}, /* 9D */
    {"UnsetMychrflags", "Clear this character's flags", 5, 1, {{"CHRFLAGS", 1, 4, ACTION_MASK}}}, /* 9E */
    {"IFMychrflagsHas", "If this character has flags", 6, 2, {{"CHRFLAGS", 1, 4, ACTION_MASK}, {"GOTOLABEL", 5, 1, ACTION_LABEL}}}, /* 9F */
    {"SetChrchrflags", "Set character flags", 6, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"CHRFLAGS", 2, 4, ACTION_MASK}}}, /* A0 */
    {"UnsetChrchrflags", "Clear character flags", 6, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"CHRFLAGS", 2, 4, ACTION_MASK}}}, /* A1 */
    {"IFChrchrflagsHas", "If character has flags", 7, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"CHRFLAGS", 2, 4, ACTION_MASK}, {"GOTOLABEL", 6, 1, ACTION_LABEL}}}, /* A2 */
    {"SetObjectFlags", "Set object flags", 6, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"BITFIELD", 2, 4, ACTION_MASK}}}, /* A3 */
    {"UnsetObjectFlags", "Unset object flags", 6, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"BITFIELD", 2, 4, ACTION_MASK}}}, /* A4 */
    {"IFObjectFlagsHas", "If object flags has", 7, 3, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"BITS", 2, 4, ACTION_MASK}, {"GOTOLABEL", 6, 1, ACTION_LABEL}}}, /* A5 */
    {"SetObjectFlags2", "Set object flags2", 6, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"BITS", 2, 4, ACTION_MASK}}}, /* A6 */
    {"UnsetObjectFlags2", "Unset object flags2", 6, 2, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"BITS", 2, 4, ACTION_MASK}}}, /* A7 */
    {"IFObjectFlags2Has", "If object flags2 has", 7, 3, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"BITFIELD", 2, 4, ACTION_MASK}, {"GOTOLABEL", 6, 1, ACTION_LABEL}}}, /* A8 */
    {"SetMyChrPreset", "Set this character's character preset", 2, 1, {{"CHR_PRESET", 1, 1, ACTION_CHARACTER}}}, /* A9 */
    {"SetChrChrPreset", "Set character character preset", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"CHR_PRESET", 2, 1, ACTION_CHARACTER}}}, /* AA */
    {"SetMyPadPreset", "Set this character's pad preset", 3, 1, {{"PAD_PRESET", 1, 2, ACTION_PAD}}}, /* AB */
    {"SetChrPadPreset", "Set character pad preset", 4, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"PAD_PRESET", 2, 2, ACTION_PAD}}}, /* AC */
    {"PRINT", "Legacy debug text (not displayed)", 0, 0, {{0}}}, /* AD */
    {"MyTimerStart", "Start this character's timer", 1, 0, {{0}}}, /* AE */
    {"MyTimerReset", "Reset this character's timer", 1, 0, {{0}}}, /* AF */
    {"MyTimerPause", "Pause this character's timer", 1, 0, {{0}}}, /* B0 */
    {"MyTimerResume", "Resume this character's timer", 1, 0, {{0}}}, /* B1 */
    {"IFMyTimerIsNotRunning", "If this character's timer is not running", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* B2 */
    {"IFMyTimerLessThanTicks", "If this character's timer less than ticks", 5, 2, {{"TICKS", 1, 3, ACTION_NUMBER}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* B3 */
    {"IFMyTimerGreaterThanTicks", "If this character's timer greater than ticks", 5, 2, {{"TICKS", 1, 3, ACTION_NUMBER}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* B4 */
    {"HudCountdownShow", "Hud countdown show", 1, 0, {{0}}}, /* B5 */
    {"HudCountdownHide", "Hud countdown hide", 1, 0, {{0}}}, /* B6 */
    {"HudCountdownSet", "Hud countdown set", 3, 1, {{"SECONDS", 1, 2, ACTION_NUMBER}}}, /* B7 */
    {"HudCountdownStop", "Hud countdown stop", 1, 0, {{0}}}, /* B8 */
    {"HudCountdownStart", "Hud countdown start", 1, 0, {{0}}}, /* B9 */
    {"IFHudCountdownIsNotRunning", "If HUD countdown is not running", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* BA */
    {"IFHudCountdownLessThan", "If HUD countdown less than", 4, 2, {{"SECONDS", 1, 2, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* BB */
    {"IFHudCountdownGreaterThan", "If HUD countdown greater than", 4, 2, {{"SECONDS", 1, 2, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* BC */
    {"TRYSpawningChrAtPad", "Try spawning character at pad", 12, 6, {{"BODY_NUM", 1, 1, ACTION_NUMBER}, {"HEAD_NUM", 2, 1, ACTION_NUMBER}, {"PAD", 3, 2, ACTION_PAD}, {"AI_LIST_ID", 5, 2, ACTION_BLOCK}, {"BITFIELD", 7, 4, ACTION_MASK}, {"GOTOLABEL", 11, 1, ACTION_LABEL}}}, /* BD */
    {"TRYSpawningChrNextToChr", "Try spawning character next to character", 11, 6, {{"BODY_NUM", 1, 1, ACTION_NUMBER}, {"HEAD_NUM", 2, 1, ACTION_NUMBER}, {"CHR_NUM_TARGET", 3, 1, ACTION_CHARACTER}, {"AI_LIST_ID", 4, 2, ACTION_BLOCK}, {"BITFIELD", 6, 4, ACTION_MASK}, {"GOTOLABEL", 10, 1, ACTION_LABEL}}}, /* BE */
    {"TRYGiveMeItem", "Try give this character item", 9, 4, {{"PROP_NUM", 1, 2, ACTION_NUMBER}, {"ITEM_NUM", 3, 1, ACTION_NUMBER}, {"PROPFLAG", 4, 4, ACTION_NUMBER}, {"GOTOLABEL", 8, 1, ACTION_LABEL}}}, /* BF */
    {"TRYGiveMeHat", "Try give this character hat", 8, 3, {{"PROP_NUM", 1, 2, ACTION_NUMBER}, {"PROP_BITFIELD", 3, 4, ACTION_NUMBER}, {"GOTOLABEL", 7, 1, ACTION_LABEL}}}, /* C0 */
    {"TRYCloningChr", "Try cloning character", 5, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"AI_LIST_ID", 2, 2, ACTION_BLOCK}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* C1 */
    {"TextPrintBottom", "Text print bottom", 3, 1, {{"TEXT_SLOT", 1, 2, ACTION_NUMBER}}}, /* C2 */
    {"TextPrintTop", "Text print top", 3, 1, {{"TEXT_SLOT", 1, 2, ACTION_NUMBER}}}, /* C3 */
    {"SfxPlay", "Sfx play", 4, 2, {{"SOUND_NUM", 1, 2, ACTION_NUMBER}, {"CHANNEL_NUM", 3, 1, ACTION_NUMBER}}}, /* C4 */
    {"SfxEmitFromObject", "Sfx emit from object", 5, 3, {{"CHANNEL_NUM", 1, 1, ACTION_NUMBER}, {"OBJECT_TAG", 2, 1, ACTION_NUMBER}, {"VOL_DECAY_TIME60", 3, 2, ACTION_SECONDS60}}}, /* C5 */
    {"SfxEmitFromPad", "Sfx emit from pad", 6, 3, {{"CHANNEL_NUM", 1, 1, ACTION_NUMBER}, {"PAD", 2, 2, ACTION_PAD}, {"VOL_DECAY_TIME60", 4, 2, ACTION_SECONDS60}}}, /* C6 */
    {"SfxSetChannelVolume", "Sfx set channel volume", 6, 3, {{"CHANNEL_NUM", 1, 1, ACTION_NUMBER}, {"TARGET_VOLUME", 2, 2, ACTION_NUMBER}, {"TRANSITION_TIME60", 4, 2, ACTION_SECONDS60}}}, /* C7 */
    {"SfxFadeChannelVolume", "Sfx fade channel volume", 6, 3, {{"CHANNEL_NUM", 1, 1, ACTION_NUMBER}, {"TARGET_VOLUME", 2, 2, ACTION_NUMBER}, {"TRANSITION_TIME60", 4, 2, ACTION_SECONDS60}}}, /* C8 */
    {"SfxStopChannel", "Sfx stop channel", 2, 1, {{"CHANNEL_NUM", 1, 1, ACTION_NUMBER}}}, /* C9 */
    {"IFSfxChannelVolumeLessThan", "If sfx channel volume less than", 5, 3, {{"CHANNEL_NUM", 1, 1, ACTION_NUMBER}, {"VOLUME", 2, 2, ACTION_NUMBER}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* CA */
    {"VehicleStartPath", "Vehicle start path", 2, 1, {{"PATH_NUM", 1, 1, ACTION_NUMBER}}}, /* CB */
    {"VehicleSpeed", "Vehicle speed", 5, 2, {{"TOP_SPEED", 1, 2, ACTION_NUMBER}, {"ACCELERATION_TIME60", 3, 2, ACTION_SECONDS60}}}, /* CC */
    {"AircraftRotorSpeed", "Aircraft rotor speed", 5, 2, {{"ROTOR_SPEED", 1, 2, ACTION_NUMBER}, {"ACCELERATION_TIME60", 3, 2, ACTION_SECONDS60}}}, /* CD */
    {"IFCameraIsInIntro", "If camera is in intro", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* CE */
    {"IFCameraIsInBondSwirl", "If camera is in Bond swirl", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* CF */
    {"TvChangeScreenBank", "Tv change screen bank", 4, 3, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"SCREEN_INDEX", 2, 1, ACTION_NUMBER}, {"SCREEN_BANK", 3, 1, ACTION_NUMBER}}}, /* D0 */
    {"IFBondInTank", "If Bond in tank", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* D1 */
    {"EndLevel", "End level", 1, 0, {{0}}}, /* D2 */
    {"CameraReturnToBond", "Camera return to Bond", 1, 0, {{0}}}, /* D3 */
    {"CameraLookAtBondFromPad", "Camera look at Bond from pad", 3, 1, {{"PAD", 1, 2, ACTION_PAD}}}, /* D4 */
    {"CameraSwitch", "Camera switch", 6, 3, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"LOOK_AT_BOND_FLAG", 2, 2, ACTION_NUMBER}, {"UNUSED_FLAG", 4, 2, ACTION_NUMBER}}}, /* D5 */
    {"IFBondYPosLessThan", "If Bond ypos less than", 4, 2, {{"Y_POS", 1, 2, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* D6 */
    {"BondDisableControl", "Bond disable control", 2, 1, {{"BITFIELD", 1, 1, ACTION_MASK}}}, /* D7 */
    {"BondEnableControl", "Bond enable control", 1, 0, {{0}}}, /* D8 */
    {"TRYTeleportingChrToPad", "Try teleporting character to pad", 5, 3, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"PAD", 2, 2, ACTION_PAD}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* D9 */
    {"ScreenFadeToBlack", "Screen fade to black", 1, 0, {{0}}}, /* DA */
    {"ScreenFadeFromBlack", "Screen fade from black", 1, 0, {{0}}}, /* DB */
    {"IFScreenFadeCompleted", "If screen fade completed", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* DC */
    {"HideAllChrs", "Hide all chrs", 1, 0, {{0}}}, /* DD */
    {"ShowAllChrs", "Show all chrs", 1, 0, {{0}}}, /* DE */
    {"DoorOpenInstant", "Door open instant", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* DF */
    {"ChrRemoveItemInHand", "Character remove item in hand", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"HAND_INDEX", 2, 1, ACTION_NUMBER}}}, /* E0 */
    {"IfNumberOfActivePlayersLessThan", "If number of active players less than", 3, 2, {{"NUMBER", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* E1 */
    {"IFBondItemTotalAmmoLessThan", "If Bond item total ammo less than", 4, 3, {{"ITEM_NUM", 1, 1, ACTION_NUMBER}, {"AMMO_TOTAL", 2, 1, ACTION_NUMBER}, {"GOTOLABEL", 3, 1, ACTION_LABEL}}}, /* E2 */
    {"BondEquipItem", "Bond equip item", 2, 1, {{"ITEM_NUM", 1, 1, ACTION_NUMBER}}}, /* E3 */
    {"BondEquipItemCinema", "Bond equip item cinema", 2, 1, {{"ITEM_NUM", 1, 1, ACTION_NUMBER}}}, /* E4 */
    {"BondSetLockedVelocity", "Bond set locked velocity", 3, 2, {{"X_SPEED60", 1, 1, ACTION_NUMBER}, {"Z_SPEED60", 2, 1, ACTION_NUMBER}}}, /* E5 */
    {"IFObjectInRoomWithPad", "If object in room with pad", 5, 3, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}, {"PAD", 2, 2, ACTION_PAD}, {"GOTOLABEL", 4, 1, ACTION_LABEL}}}, /* E6 */
    {"IFImFiringAndLockedForward", "If this character is firing and locked forward", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* E7 */
    {"IFImFiring", "If this character is firing", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* E8 */
    {"SwitchSky", "Switch sky", 1, 0, {{0}}}, /* E9 */
    {"TriggerFadeAndExitLevelOnButtonPress", "Trigger fade and exit level on button press", 1, 0, {{0}}}, /* EA */
    {"IFBondIsDead", "If Bond is dead", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* EB */
    {"BondDisableDamageAndPickups", "Bond disable damage and pickups", 1, 0, {{0}}}, /* EC */
    {"BondHideWeapons", "Bond hide weapons", 1, 0, {{0}}}, /* ED */
    {"CameraOrbitPad", "Camera orbit pad", 13, 6, {{"LAT_DISTANCE", 1, 2, ACTION_NUMBER}, {"VERT_DISTANCE", 3, 2, ACTION_NUMBER}, {"ORBIT_SPEED60", 5, 2, ACTION_NUMBER}, {"PAD", 7, 2, ACTION_PAD}, {"Y_POS_OFFSET", 9, 2, ACTION_NUMBER}, {"INITIAL_ROTATION", 11, 2, ACTION_NUMBER}}}, /* EE */
    {"CreditsRoll", "Credits roll", 1, 0, {{0}}}, /* EF */
    {"IFCreditsHasCompleted", "If credits has completed", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* F0 */
    {"IFObjectiveAllCompleted", "If objective all completed", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* F1 */
    {"IFFolderActorIsEqual", "If folder actor is equal", 3, 2, {{"BOND_ACTOR_INDEX", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* F2 */
    {"IFBondDamageAndPickupsDisabled", "If Bond damage and pickups disabled", 2, 1, {{"GOTOLABEL", 1, 1, ACTION_LABEL}}}, /* F3 */
    {"MusicPlaySlot", "Music play slot", 4, 3, {{"MUSIC_SLOT", 1, 1, ACTION_NUMBER}, {"SECONDS_STOPPED_DURATION", 2, 1, ACTION_NUMBER}, {"SECONDS_TOTAL_DURATION", 3, 1, ACTION_NUMBER}}}, /* F4 */
    {"MusicStopSlot", "Music stop slot", 2, 1, {{"MUSIC_SLOT", 1, 1, ACTION_NUMBER}}}, /* F5 */
    {"TriggerExplosionsAroundBond", "Trigger explosions around Bond", 1, 0, {{0}}}, /* F6 */
    {"IFKilledCiviliansGreaterThan", "If killed civilians greater than", 3, 2, {{"CIVILIANS_KILLED", 1, 1, ACTION_NUMBER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* F7 */
    {"IFChrWasShotSinceLastCheck", "If character was shot since last check", 3, 2, {{"CHR_NUM", 1, 1, ACTION_CHARACTER}, {"GOTOLABEL", 2, 1, ACTION_LABEL}}}, /* F8 */
    {"BondKilledInAction", "Bond killed in action", 1, 0, {{0}}}, /* F9 */
    {"RaiseArms", "Raise arms", 1, 0, {{0}}}, /* FA */
    {"GasLeakAndFadeFog", "Gas leak and fade fog", 1, 0, {{0}}}, /* FB */
    {"ObjectRocketLaunch", "Object rocket launch", 2, 1, {{"OBJECT_TAG", 1, 1, ACTION_NUMBER}}}, /* FC */
