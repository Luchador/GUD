#ifndef _OBJECTIVE_STATUS_H_
#define _OBJECTIVE_STATUS_H_
#include <ultra64.h>
#include <bondtypes.h>

extern struct objective_entry *objective_ptrs[OBJECTIVES_MAX];
extern  OBJECTIVESTATUS objectiveStatuses[OBJECTIVES_MAX];

extern u32 *ptr_last_tag_entry_type16;
extern struct watchMenuObjectiveText *ptr_last_briefing_setup_entry_type23;
extern struct criteria_roomentered *ptr_last_enter_room_subobject_entry_type20;
extern struct criteria_deposit *ptr_last_deposit_in_room_subobject_entry_type21;
extern struct criteria_picture *ptr_last_photo_obj_in_room_subobject_entry_type1E;

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
