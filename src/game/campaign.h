#ifndef GUD_CAMPAIGN_H
#define GUD_CAMPAIGN_H

#include "../levelids.h"

/* Array order controls campaign/menu order. number_text is presentation only:
 * a mission number on a header, or a part number on a playable row. Neither
 * may be used to address saves, cheats, level resources or photographs. */
struct mission_folder_setup {
    const char *number_text;
    unsigned short folder_text_preset;
    unsigned short icon_text_preset;
    LEVELID stage_id;
    int type;
    LEVEL_SOLO_SLOT save_slot;
    const char *briefing_name_ptr;
    int photo_switch;
    short cheat_times[3];
    int minimum_difficulty;
    int is_bonus;
};

extern struct mission_folder_setup mission_folder_setup_entries[];
extern const int g_CampaignEntryCount;

int campaignGetMissionCount(void);
int campaignGetEntryByOrder(int order);
int campaignGetEntryBySaveSlot(LEVEL_SOLO_SLOT slot);
int campaignGetEntryByLevel(LEVELID level);
int campaignGetOrderBySaveSlot(LEVEL_SOLO_SLOT slot);
LEVEL_SOLO_SLOT campaignGetSaveSlotByOrder(int order);
int campaignGetPhotoOrder(int photo);
int campaignIsBonus(LEVEL_SOLO_SLOT slot);
int campaignIsPrerequisite(LEVEL_SOLO_SLOT slot, LEVEL_SOLO_SLOT prior);
LEVEL_SOLO_SLOT campaignGetFirstMain(void);
LEVEL_SOLO_SLOT campaignGetPreviousMain(LEVEL_SOLO_SLOT slot);
LEVEL_SOLO_SLOT campaignGetNextMain(LEVEL_SOLO_SLOT slot);
LEVEL_SOLO_SLOT campaignGetLastMain(void);
int campaignGetCheatTime(LEVEL_SOLO_SLOT slot, int difficulty);

#endif
