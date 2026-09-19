#ifndef _BOSS_H_
#define _BOSS_H_
#include <ultra64.h>
#include <bondgame.h>

LEVELID bossGetStageNum(void);
void bossSetLoadedStage(LEVELID stage);
void bossRunTitleStage(void);
void bossReturnTitleStage(void);

#endif
