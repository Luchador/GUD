#ifndef _BOSS_H_
#define _BOSS_H_
#include <ultra64.h>
#include <bondgame.h>

struct memallocstring
{
    s32 id;
    void *string;
};

LEVELID bossGetStageNum(void);
void bossSetLoadedStage(LEVELID stage);
void bossRunTitleStage(void);
void bossReturnTitleStage(void);

#endif
