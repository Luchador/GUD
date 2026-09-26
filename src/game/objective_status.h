#ifndef _OBJECTIVE_STATUS_H_
#define _OBJECTIVE_STATUS_H_
#include <ultra64.h>
#include <bondtypes.h>

extern struct objective_entry *objective_ptrs[OBJECTIVES_MAX];
extern  OBJECTIVESTATUS objectiveStatuses[OBJECTIVES_MAX];

/* Heads of the linked setup-record lists populated by objectiveAdd*(). */
extern u32 *g_ObjectiveTagHead;
extern struct watchMenuObjectiveText *g_ObjectiveBriefingTextHead;
extern struct criteria_roomentered *g_ObjectiveRoomEnteredCriteriaHead;
extern struct criteria_deposit *g_ObjectiveDepositInRoomCriteriaHead;
extern struct criteria_picture *g_ObjectivePhotographCriteriaHead;

extern s32 objective_count;
extern s32 objective_status_display_disabled;

void objectiveTakePictureHandler(void);
bool objectiveIsAllComplete(void);
TagObjectRecord *getTagID(s32 TagID);
ObjectRecord *objFindByTagId(s32 TagID);
u8 *objectiveGetText(s32 objectiveIndex);
s32 objectiveGetDifficulty(s32 objectiveIndex);
s32 objectiveGetCount(void);
OBJECTIVESTATUS objectiveGetStatus(s32 objectiveIndex);
void objectivestatusCheckDeposit(s32 weaponnum, s32 roomid);
void display_objective_status_text_on_status_change(void);
#endif
