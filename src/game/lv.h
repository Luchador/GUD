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
typedef enum ChrCollisionProfileScope {
    CHR_COLLISION_PROFILE_NONE,
    CHR_COLLISION_PROFILE_NAV_SWEEP,
    CHR_COLLISION_PROFILE_MOVE
} ChrCollisionProfileScope;

extern u32 g_ProfBgTickCycles;
extern u32 g_ProfLvlTickCycles;
extern u32 g_ProfLvlRenderCycles;
extern u32 g_ProfBgRenderCycles;
extern u32 g_ProfChrTickCycles;
extern u32 g_ProfChrActionCycles;
extern u32 g_ProfObjTickCycles;
extern u32 g_ProfGfxCommands;
extern u32 g_ProfBgGfxCommands;
extern u32 g_ProfChrNavCycles;
extern u32 g_ProfChrNavCalls;
extern u32 g_ProfChrMoveCycles;
extern u32 g_ProfChrMoveCalls;
extern u32 g_ProfChrRoomCycles;
extern u32 g_ProfChrRoomCalls;
extern u32 g_ProfChrNavSweepCycles;
extern u32 g_ProfChrNavSweepCalls;
extern ChrCollisionProfileScope g_ProfChrCollisionScope;
extern u32 g_ProfChrNavSweepLineCycles;
extern u32 g_ProfChrNavSweepLineCalls;
extern u32 g_ProfChrNavSweepVolumeCycles;
extern u32 g_ProfChrNavSweepVolumeCalls;
extern u32 g_ProfChrNavLineTileCycles;
extern u32 g_ProfChrNavLineQueryCycles;
extern u32 g_ProfChrNavLineFilterCycles;
extern u32 g_ProfChrNavLineBoundsCycles;
extern u32 g_ProfChrNavLineIntersectionTestCycles;
extern u32 g_ProfChrNavLineHitProcessingCycles;
extern u32 g_ProfChrNavLineCandidateProps;
extern u32 g_ProfChrNavLineZeroEdgeProps;
extern u32 g_ProfChrNavLineTestedEdges;
extern u32 g_ProfChrNavLineAabbPassedEdges;
extern u32 g_ProfChrNavLineIntersectingEdges;
extern u32 g_ProfChrMoveLineCycles;
extern u32 g_ProfChrMoveLineCalls;
extern u32 g_ProfChrMoveVolumeCycles;
extern u32 g_ProfChrMoveVolumeCalls;

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
