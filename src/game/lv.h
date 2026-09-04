#ifndef _LVL_H_
#define _LVL_H_
#include <ultra64.h>
#include <bondconstants.h>
#include "bondtypes.h"

#define DEFAULT_AGENT_AI_ACCURACY_MODIFIER 0.6f
#define DEFAULT_AGENT_AI_DAMAGE_MODIFIER 0.5f
#define DEFAULT_AGENT_SOLO_AMMO_MULTIPLIER 2.0f
#define DEFAULT_AGENT_AI_REACTION_SPEED 0.2f

#define DEFAULT_SECRET_AGENT_AI_ACCURACY_MODIFIER 0.75f
#define DEFAULT_SECRET_AGENT_AI_DAMAGE_MODIFIER 0.75f
#define DEFAULT_SECRET_AGENT_SOLO_AMMO_MULTIPLIER 1.5f
#define DEFAULT_SECRET_AGENT_AI_REACTION_SPEED 0.5f

#define DEFAULT_00_AGENT_AI_ACCURACY_MODIFIER 1.0f
#define DEFAULT_00_AGENT_AI_DAMAGE_MODIFIER 1.0f
#define DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER 1.0f
#define DEFAULT_00_AGENT_AI_REACTION_SPEED 1.0f

#define DEFAULT_007_AI_ACCURACY_MODIFIER DEFAULT_00_AGENT_AI_ACCURACY_MODIFIER
#define DEFAULT_007_AI_DAMAGE_MODIFIER DEFAULT_00_AGENT_AI_DAMAGE_MODIFIER
#define DEFAULT_007_SOLO_AMMO_MULTIPLIER DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER
#define DEFAULT_007_AI_REACTION_SPEED DEFAULT_00_AGENT_AI_REACTION_SPEED

struct LvlMpUnknown {
    union {
        struct {
            u8 unk_0;
            u8 unk_1;
            u8 unk_2;
            u8 unk_3;
        };
        struct {
            u32 unk_0_0 : 8;
            u32 unk_1_3 : 3;
            u32 unk_1_0 : 3;
        };
        u32 word;
    };
};

struct levelentry
{
    s32 levelID;
    void *bg_seg_filename;
    void *bg_stan_filename;
    f32 levelscale;
    f32 visibility;
};

extern struct levelentry g_LevelInfoTable[];
extern stagesetup *g_ptrStageSetupFile;
extern char *setup_text_pointers[];

extern s32 g_LastImpactTexNum;
extern struct LvlMpUnknown *D_800483C8;

extern s32 g_ClockTimer;
extern s32 g_GlobalTimer;
extern f32 g_GlobalTimerDelta;
extern s32 g_GlobalTickCount;
extern u32 *ptr_font_DL;
extern char ramrom_data_target[0x380];
extern s32 record_slot_num;
extern u8 * address_demo_loaded;

// TEMP
extern u32 g_ProfBgTickCycles;
extern u32 g_ProfLvlTickCycles;
extern u32 g_ProfLvlRenderCycles;
extern u32 g_ProfBgRenderCycles;
extern u32 g_ProfChrTickCycles;
extern u32 g_ProfChrActionCycles;
extern u32 g_ProfObjTickCycles;
extern u32 g_ProfGfxCommands;
extern u32 g_ProfBgGfxCommands;

void lvInit(void);
Gfx * lvRender(Gfx *);
void lvSetSelectedDifficulty(DIFFICULTY diff);
void lvTick(void);
void lvlViewMoveTick(void);
void lvlUnloadStageTextData(void);
DIFFICULTY lvGetSelectedDifficulty(void);
void lvlStageLoad(s32 stage);
void lvSetMpTime(s32 timelimit);
void lvSetMpPoint(s32 pointlimit);
void lvSetControlsLockedFlag(bool locked);
s32 lvGetControlsLockedFlag(void);
void lvlSetMultipliersForDifficulty(void);
s32 lvlGetCurrentStageToLoad(void);
bool lvGetBgRenderEnabled(void);
void lvSetBgRenderEnabled(bool enabled);
f32 lvGetStageElapsedSeconds(void);
f32 lvGetSystemPowerTimeSeconds(void);
Gfx *lvDrawFrameRateDisplay(Gfx *gdl);

#endif
