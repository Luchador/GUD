#include <ultra64.h>
#include <boss.h>
#include "objective.h"
#include "objective_status.h"


/*
 * Clear setup references for the next stage. Keep the previous mission's
 * objective count and statuses when returning to the title screen.
 */
void objectiveReset(void)
{
    s32 i;

    if (bossGetStageNum() != LEVELID_TITLE)
    {
        objective_count = -1;

        for (i = 0; i < OBJECTIVES_MAX; i++)
        {
            objectiveStatuses[i] = OBJECTIVESTATUS_INCOMPLETE;
        }
    }


    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        objective_ptrs[i] = NULL;
    }

    g_ObjectiveTagHead = NULL;
    g_ObjectiveBriefingTextHead = NULL;
    g_ObjectiveRoomEnteredCriteriaHead = NULL;
    g_ObjectiveDepositInRoomCriteriaHead = NULL;
    g_ObjectivePhotographCriteriaHead = NULL;
}


/* Setup records are prepended to their linked lists as they are loaded. */
void objectiveAddTag(struct TagObjectRecord *tag)
{
    tag->NextTag = g_ObjectiveTagHead;
    g_ObjectiveTagHead = tag;
}


void objectiveAddBriefingText(struct setup_objective_text *briefing)
{
    briefing->next = g_ObjectiveBriefingTextHead;
    g_ObjectiveBriefingTextHead = briefing;
}


void objectiveAddEntry(struct objective_entry *objective)
{
    objective_ptrs[objective->menu] = objective;

    if (objective_count < objective->menu)
    {
        objective_count = objective->menu;
    }
}


void objectiveAddRoomEnteredCriteria(struct criteria_roomentered *criteria)
{
    criteria->next = g_ObjectiveRoomEnteredCriteriaHead;
    g_ObjectiveRoomEnteredCriteriaHead = criteria;
}


void objectiveAddDepositInRoomCriteria(struct criteria_deposit *criteria)
{
    criteria->next = g_ObjectiveDepositInRoomCriteriaHead;
    g_ObjectiveDepositInRoomCriteriaHead = criteria;
}


void objectiveAddPhotographCriteria(struct criteria_picture *criteria)
{
    criteria->next = g_ObjectivePhotographCriteriaHead;
    g_ObjectivePhotographCriteriaHead = criteria;
}
