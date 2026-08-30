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
extern u32 g_ProfChrTickCalls;
extern u32 g_ProfChrOnscreenCount;
extern u32 g_ProfChrMagicCount;
extern u32 g_ProfChrActionStandCount;
extern u32 g_ProfChrActionMoveCount;
extern u32 g_ProfChrActionCombatCount;
extern u32 g_ProfChrActionAnimCount;
extern u32 g_ProfChrActionOtherCount;
extern s32 g_ProfChrActionActive;
extern s32 g_ProfChrCurrentAction;
extern u32 g_ProfChrAiCycles;
extern u32 g_ProfChrAiCalls;
extern u32 g_ProfChrAiCommandCount;
extern u32 g_ProfChrCurrentAiCommandCount;
extern u32 g_ProfChrAiCommandCycles[AI_CMD_COUNT];
extern u32 g_ProfChrAiCommandCalls[AI_CMD_COUNT];
extern s32 g_ProfChrLosActive;
extern u32 g_ProfChrLosCycles;
extern u32 g_ProfChrLosLineCycles;
extern u32 g_ProfChrLosStanCycles;
extern u32 g_ProfChrLosRoomCycles;
extern u32 g_ProfChrLosPropCycles;
extern u32 g_ProfChrLosRecoveryCycles;
extern u32 g_ProfChrLosCalls;
extern u32 g_ProfChrLosPasses;
extern u32 g_ProfChrLosStanBlocks;
extern u32 g_ProfChrLosPropBlocks;
extern u32 g_ProfChrLosTileMismatches;
extern u32 g_ProfChrLosInvisibleCalls;
extern u32 g_ProfChrLosRooms;
extern u32 g_ProfChrLosProps;
extern u32 g_ProfChrLosCandidates;
extern u32 g_ProfChrLosEdges;
extern u32 g_ProfChrLosIntersections;
extern u32 g_ProfChrSlowestAiCycles;
extern u32 g_ProfChrSlowestAiCommandCount;
extern s32 g_ProfChrSlowestAiChrnum;
extern u32 g_ProfChrStateCycles;
extern u32 g_ProfChrStateStandCycles;
extern u32 g_ProfChrStateMoveCycles;
extern u32 g_ProfChrStateMagicCycles;
extern u32 g_ProfChrStateCombatCycles;
extern u32 g_ProfChrStateOtherCycles;
extern u32 g_ProfChrAnimPosCycles;
extern u32 g_ProfChrAnimCycles;
extern u32 g_ProfChrVisibilityCycles;
extern u32 g_ProfChrRoomCycles;
extern u32 g_ProfChrRootPositionCycles;
extern u32 g_ProfChrRootPositionCalls;
extern u32 g_ProfChrPositionValidateCycles;
extern u32 g_ProfChrPositionValidateCalls;
extern u32 g_ProfChrCollisionCycles;
extern u32 g_ProfChrCollisionCalls;
extern u32 g_ProfChrCollisionLineCycles;
extern u32 g_ProfChrCollisionLineCalls;
extern u32 g_ProfChrCollisionVolumeCycles;
extern u32 g_ProfChrCollisionVolumeCalls;
extern u32 g_ProfChrStationaryVolumeSkips;
extern u32 g_ProfChrGroundCycles;
extern u32 g_ProfChrGroundCalls;
extern u32 g_ProfChrGroundFollowCycles;
extern u32 g_ProfChrGroundFollowCalls;
extern s32 g_ProfChrPositionActive;
extern u32 g_ProfChrMatrixCycles;
extern u32 g_ProfChrSlowestCycles;
extern s32 g_ProfChrSlowestChrnum;
extern s32 g_ProfChrSlowestAction;
extern u32 g_ProfObjTickCycles;

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
