#include "campaign.h"

static int campaignIsMission(int entry)
{
    return entry >= 0 && entry < g_CampaignEntryCount
        && mission_folder_setup_entries[entry].stage_id != LEVELID_NONE
        && mission_folder_setup_entries[entry].save_slot >= 0
        && mission_folder_setup_entries[entry].save_slot < SP_LEVEL_MAX;
}

int campaignGetMissionCount(void)
{
    int entry, count = 0;
    for (entry = 0; entry < g_CampaignEntryCount; entry++)
        if (campaignIsMission(entry)) count++;
    return count;
}

int campaignGetEntryByOrder(int order)
{
    int entry;
    if (order < 0) return -1;
    for (entry = 0; entry < g_CampaignEntryCount; entry++)
        if (campaignIsMission(entry) && order-- == 0) return entry;
    return -1;
}

int campaignGetEntryBySaveSlot(LEVEL_SOLO_SLOT slot)
{
    int entry;
    if (slot < 0 || slot >= SP_LEVEL_MAX) return -1;
    for (entry = 0; entry < g_CampaignEntryCount; entry++)
        if (campaignIsMission(entry) && mission_folder_setup_entries[entry].save_slot == slot) return entry;
    return -1;
}

int campaignGetEntryByLevel(LEVELID level)
{
    int entry;
    for (entry = 0; entry < g_CampaignEntryCount; entry++)
        if (campaignIsMission(entry) && mission_folder_setup_entries[entry].stage_id == level) return entry;
    return -1;
}

int campaignGetOrderBySaveSlot(LEVEL_SOLO_SLOT slot)
{
    int entry, order = 0;
    if (slot < 0 || slot >= SP_LEVEL_MAX) return -1;
    for (entry = 0; entry < g_CampaignEntryCount; entry++)
        if (campaignIsMission(entry))
        {
            if (mission_folder_setup_entries[entry].save_slot == slot) return order;
            order++;
        }
    return -1;
}

LEVEL_SOLO_SLOT campaignGetSaveSlotByOrder(int order)
{
    int entry = campaignGetEntryByOrder(order);
    return entry < 0 ? SP_LEVEL_NONE : mission_folder_setup_entries[entry].save_slot;
}

int campaignGetPhotoOrder(int photo)
{
    int entry, order = 0;
    for (entry = 0; entry < g_CampaignEntryCount; entry++)
        if (campaignIsMission(entry))
        {
            if (mission_folder_setup_entries[entry].photo_switch == photo) return order;
            order++;
        }
    return -1;
}

int campaignIsBonus(LEVEL_SOLO_SLOT slot)
{
    int entry = campaignGetEntryBySaveSlot(slot);
    return entry >= 0 && mission_folder_setup_entries[entry].is_bonus;
}

int campaignIsPrerequisite(LEVEL_SOLO_SLOT slot, LEVEL_SOLO_SLOT prior)
{
    int order = campaignGetOrderBySaveSlot(slot);
    int priorOrder = campaignGetOrderBySaveSlot(prior);
    if (order < 0 || priorOrder < 0 || slot == prior) return 0;
    if (campaignIsBonus(slot)) return !campaignIsBonus(prior) || priorOrder < order;
    return !campaignIsBonus(prior) && priorOrder < order;
}

LEVEL_SOLO_SLOT campaignGetFirstMain(void)
{
    int entry;
    for (entry = 0; entry < g_CampaignEntryCount; entry++)
        if (campaignIsMission(entry) && !mission_folder_setup_entries[entry].is_bonus)
            return mission_folder_setup_entries[entry].save_slot;
    return SP_LEVEL_NONE;
}

LEVEL_SOLO_SLOT campaignGetPreviousMain(LEVEL_SOLO_SLOT slot)
{
    int entry = campaignGetEntryBySaveSlot(slot);
    while (--entry >= 0)
        if (campaignIsMission(entry) && !mission_folder_setup_entries[entry].is_bonus)
            return mission_folder_setup_entries[entry].save_slot;
    return SP_LEVEL_NONE;
}

LEVEL_SOLO_SLOT campaignGetNextMain(LEVEL_SOLO_SLOT slot)
{
    int entry = campaignGetEntryBySaveSlot(slot);
    if (entry < 0) return SP_LEVEL_NONE;
    while (++entry < g_CampaignEntryCount)
        if (campaignIsMission(entry) && !mission_folder_setup_entries[entry].is_bonus)
            return mission_folder_setup_entries[entry].save_slot;
    return SP_LEVEL_NONE;
}

LEVEL_SOLO_SLOT campaignGetLastMain(void)
{
    int entry;
    for (entry = g_CampaignEntryCount - 1; entry >= 0; entry--)
        if (campaignIsMission(entry) && !mission_folder_setup_entries[entry].is_bonus)
            return mission_folder_setup_entries[entry].save_slot;
    return SP_LEVEL_NONE;
}

int campaignGetCheatTime(LEVEL_SOLO_SLOT slot, int difficulty)
{
    int entry = campaignGetEntryBySaveSlot(slot);
    if (entry < 0 || difficulty < 0 || difficulty >= 3) return 0;
    return mission_folder_setup_entries[entry].cheat_times[difficulty];
}
