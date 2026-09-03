#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "objective_status.h"
#include "assets/obseg/text/LmiscE.h"
#include "language.h"
#include "PR/os.h"
#include "str.h"
#include "bondview.h"


struct objective_entry *objective_ptrs[OBJECTIVES_MAX];
OBJECTIVESTATUS objectiveStatuses[OBJECTIVES_MAX];
u32 *ptr_last_tag_entry_type16;
struct watchMenuObjectiveText *ptr_last_briefing_setup_entry_type23;
struct criteria_roomentered *ptr_last_enter_room_subobject_entry_type20;
struct criteria_deposit *ptr_last_deposit_in_room_subobject_entry_type21;
struct criteria_picture *ptr_last_photo_obj_in_room_subobject_entry_type1E;
s32 objective_count = 0xFFFFFFFF;
s32 objective_status_display_disabled = FALSE;


/**
  * Return Tag with TagID
  */
TagObjectRecord *getTagID(s32 TagID)
{
    u16              ID  = TagID;
    TagObjectRecord *tag = ptr_last_tag_entry_type16;

    while (tag)
    {
        if (tag->ID == ID)
        {
            return tag;
        }

        tag = tag->NextTag;
    }

    return NULL;
}


/**
  * Return Object with TagID  
  */
ObjectRecord *objFindByTagId(s32 TagID)
{
    TagObjectRecord *tag = getTagID(TagID);
    ObjectRecord *   obj = NULL;

    if (tag)
    {
        obj = tag->TaggedObject;
    }

    if (obj && !(obj->runtime_bitflags & RUNTIMEBITFLAG_TAGGED))
    {
        obj = NULL; //clear object
    }
    return obj;
}


u8 *get_ptr_text_for_watch_breifing_page(WATCH_BRIEFING_PAGE page)
{
    struct watchMenuObjectiveText * curentry;
    u8 * textptr;

    textptr = NULL;

    for (curentry = ptr_last_briefing_setup_entry_type23; curentry != 0; curentry = curentry->nextentry)
    {
        if (page == curentry->menu)
        {
            textptr = langGet(curentry->text);
            break;
        }
    }

    if (textptr == NULL)
    {
        if (page == 0)
        {
            textptr = langGet(getStringID(LMISC, MISC_STR_29_ERROR_LF)); //"E R R O R\n"
        }
        else if (page == 1)
        {
            textptr = langGet(getStringID(LMISC, MISC_STR_2A_NOBRIEFING_LF)); //"no briefing for this mission\n"
        }
        else
        {
            textptr = langGet(getStringID(LMISC, MISC_STR_2B_LF)); //"\n"
        }
    }

    return textptr;
}


s32 objectiveGetCount(void)
{
    return objective_count + 1;
}


u8 *objectiveGetText(s32 objectiveIndex)
{
    u8 *textptr;
    
    if ((objectiveIndex < OBJECTIVES_MAX) && (objective_ptrs[objectiveIndex] != NULL))
    {
        return langGet(objective_ptrs[objectiveIndex]->text);
    }

    return 0;
}


s32 objectiveGetDifficulty(s32 objectiveIndex)
{
    struct objective_entry * entry;

    if (objectiveIndex < OBJECTIVES_MAX)
    {
        entry = objective_ptrs[objectiveIndex];

        if (entry != NULL)
        {
            return entry->difficulty;
        }
    }

    return 0;
}


OBJECTIVESTATUS objectiveGetStatus(s32 objectiveIndex)
{
    struct objective_entry *entry;
    MissionObjectiveRecord *condition;
    OBJECTIVESTATUS conditionStatus;
    OBJECTIVESTATUS objectiveStatus = OBJECTIVESTATUS_COMPLETE;

    if (objectiveIndex < 0 || objectiveIndex >= OBJECTIVES_MAX)
    {
        return objectiveStatus;
    }

    entry = objective_ptrs[objectiveIndex];

    if (entry == NULL)
    {
        return objectiveStatuses[objectiveIndex];
    }

    condition = (MissionObjectiveRecord *)entry;

    while (condition->type != PROPDEF_OBJECTIVE_END)
    {
        conditionStatus = OBJECTIVESTATUS_COMPLETE;

        switch (condition->type)
        {
            case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
            {
                ObjectRecord *obj = objFindByTagId(condition->ObjRefID);

                if (obj && obj->prop && objIsHealthy(obj))
                {
                    conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                }
                break;
            }

            case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
                if (!chrHasStageFlag(NULL, condition->ObjRefID))
                {
                    conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                }
                break;

            case PROPDEF_OBJECTIVE_FAIL_CONDITION:
                if (chrHasStageFlag(NULL, condition->ObjRefID))
                {
                    conditionStatus = OBJECTIVESTATUS_FAILED;
                }
                break;

            case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
            {
                ObjectRecord *obj = objFindByTagId(condition->ObjRefID);

                if (!obj || !obj->prop || !objIsHealthy(obj))
                {
                    conditionStatus = OBJECTIVESTATUS_FAILED;
                }
                else if (!bondinvHasPropInInv(obj->prop))
                {
                    conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                }
                break;
            }

            case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
            {
                ObjectRecord *obj = objFindByTagId(condition->ObjRefID);

                if (obj && obj->prop && bondinvHasPropInInv(obj->prop))
                {
                    conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                }
                break;
            }

            case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            {
                ObjectRecord *obj = objFindByTagId(condition->ObjRefID);

                if (!condition->TextID)
                {
                    if (!obj || !obj->prop || !objIsHealthy(obj))
                    {
                        conditionStatus = OBJECTIVESTATUS_FAILED;
                    }
                    else
                    {
                        conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                    }
                }
                break;
            }

            case PROPDEF_OBJECTIVE_ENTER_ROOM:
                if (!condition->TextID)
                {
                    conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                }
                break;

            case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
                if (!condition->MinDificulty)
                {
                    conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                }
                break;

            case PROPDEF_OBJECTIVE_COPY_ITEM:
                if (!get_keyanalyzer_flag())
                {
                    conditionStatus = OBJECTIVESTATUS_INCOMPLETE;
                }
                break;

            case PROPDEF_OBJECTIVE_START:
            case PROPDEF_OBJECTIVE_NULL:
                break;

            default:
                break;
        }

        if (conditionStatus == OBJECTIVESTATUS_FAILED)
        {
            objectiveStatus = OBJECTIVESTATUS_FAILED;
        }
        else if (conditionStatus == OBJECTIVESTATUS_INCOMPLETE
                && objectiveStatus == OBJECTIVESTATUS_COMPLETE)
        {
            objectiveStatus = OBJECTIVESTATUS_INCOMPLETE;
        }

        condition = (MissionObjectiveRecord *)((PropDefHeaderRecord *)condition + sizepropdef((PropDefHeaderRecord *)condition));
    }

    return objectiveStatus;
}


bool objectiveIsAllComplete(void)
{
    DIFFICULTY currentDifficulty = lvGetSelectedDifficulty();
    s32 objectiveIndex;

    for (objectiveIndex = 0; objectiveIndex < objectiveGetCount(); objectiveIndex++)
    {
        if (objectiveGetDifficulty(objectiveIndex) <= currentDifficulty
                && objectiveGetStatus(objectiveIndex) != OBJECTIVESTATUS_COMPLETE)
        {
            return FALSE;
        }
    }

    return TRUE;
}


void display_objective_status_text_on_status_change(void) 
{
    s32 i;
    s32 availableindex = 0;
    char buffer[50] = "";
    enum OBJECTIVESTATUS status;

    for (i = 0; i <= objective_count; i++)
    {
        status = objectiveGetStatus(i);

        if (objectiveStatuses[i] != status)
        {
            objectiveStatuses[i] = status;
    
            if (objectiveGetDifficulty(i) <= lvGetSelectedDifficulty())
            {
                sprintf(&buffer, "%s %c: ", langGet(getStringID(LMISC, MISC_STR_2C_OBJECTIVE)), availableindex + 0x61);

                if (status == OBJECTIVESTATUS_COMPLETE)
                {
                    strcat(&buffer, langGet(getStringID(LMISC, MISC_STR_2D_COMPLETED_LF)));
                }
                else if (status == OBJECTIVESTATUS_INCOMPLETE)
                {
                    strcat(&buffer, langGet(getStringID(LMISC, MISC_STR_2E_INCOMPLETE_LF)));
                }
                else if (status == OBJECTIVESTATUS_FAILED)
                {
                    strcat(&buffer, langGet(getStringID(LMISC, MISC_STR_2F_FAILED_LF)));
                }

                hudmsgBottomShow(&buffer);
            }
        }
        if (objectiveGetDifficulty(i) <= lvGetSelectedDifficulty())
        {
            availableindex++;
        }
    }
}


/**
 * Iterate list of ptr_last_enter_room_subobject_entry_type20, if it's the same
 * room as the argument then set the `flag` property.
*/
void objectivestatusCheckRoomEntered(s32 roomid)
{
    struct criteria_roomentered *var_v0;
    struct PadRecord* setupPad;
    struct StandTile *stan;
    
    var_v0 = ptr_last_enter_room_subobject_entry_type20;
    while (var_v0 != NULL)
    {
        if (var_v0->status == 0)
        {
            if (isNotBoundPad(var_v0->pad))
            {
                setupPad = &g_CurrentSetup.pads[var_v0->pad];
            }
            else
            {
                setupPad = (struct PadRecord*)&g_CurrentSetup.boundpads[getBoundPadNum((s32)var_v0->pad)];
            }

            stan = setupPad->stan;

            if ((stan != NULL) && (roomid == stan->room))
            {
                var_v0->status = 1;
            }
        }

        var_v0 = var_v0->next;
    }
}


void objectivestatusCheckDeposit(s32 weaponnum, s32 roomid)
{
    struct criteria_deposit *dep;
    s32 padid;
    PadRecord *pad;

    for (dep = ptr_last_deposit_in_room_subobject_entry_type21; dep != NULL; dep = dep->next)
    {
        if (dep->flag == 0 && weaponnum == dep->weaponnum)
        {
            padid = dep->padid;

            if (isNotBoundPad((u32)padid))
            {
                pad = &g_CurrentSetup.pads[padid];
            }
            else
            {
                pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padid)];
            }

            if (pad->stan != NULL && roomid == pad->stan->room)
            {
                dep->flag = 1;
            }
        }
    }
}


void objectiveTakePictureHandler(void)
{
    ObjectRecord *target_object;
    PropRecord *target_prop;
    struct coord3d sp84;
    struct coord2d sp7C;
    struct coord2d sp74;
    struct rectbbox sp64;
    struct criteria_picture *criteria;

    criteria = ptr_last_photo_obj_in_room_subobject_entry_type1E;

    for (; criteria != NULL; criteria = criteria->next)
    {
        if (criteria->flag == 0)
        {
            target_object = objFindByTagId(criteria->tag_id);
            
            if (target_object != NULL)
            {
                target_prop = target_object->prop;
                
                if ((target_prop != NULL) && (target_prop->flags & PROPFLAG_ONSCREEN) && (target_prop->zDepth >= 0.0f) && (objIsHealthy(target_object)) && (objGetOnscreenRenderBounds(target_object->prop, &sp84, &sp7C, &sp74) != 0))
                {
                    projectRectCornersTo2D(&sp84, &sp7C, &sp74, &sp64.left, &sp64.right);
                    
                    if (getPlayer_c_screenleft() < sp64.left
                        && sp64.right < getPlayer_c_screenleft() + getPlayer_c_screenwidth()
                        && getPlayer_c_screentop() < sp64.up
                        && sp64.down < getPlayer_c_screentop() + getPlayer_c_screenheight())
                    {
                        criteria->flag = 1;
                    }
                }
            }
        }
    }
}
