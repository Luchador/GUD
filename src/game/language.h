#ifndef _LVL_TEXT_H_
#define _LVL_TEXT_H_
#include <ultra64.h>
#include "bondconstants.h"

void langLoadToAddr(u32 id);
u8 * langGet(s32 slotID);
void langLoadToBank(int id, u8 *target, int size);
LEVELID langGetLangBankIndexFromStagenum(LEVELID level);

#endif
