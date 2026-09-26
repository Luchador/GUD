#ifndef _OBJECTIVE_H_
#define _OBJECTIVE_H_
#include <ultra64.h>

void objectiveReset(void);

void objectiveAddTag(struct TagObjectRecord *tag);
void objectiveAddBriefingText(struct setup_objective_text *briefing);
void objectiveAddEntry(struct objective_entry *objective);
void objectiveAddRoomEnteredCriteria(struct criteria_roomentered *criteria);
void objectiveAddDepositInRoomCriteria(struct criteria_deposit *criteria);
void objectiveAddPhotographCriteria(struct criteria_picture *criteria);

#endif
