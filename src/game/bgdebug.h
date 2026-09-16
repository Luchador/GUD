#ifndef _BGDEBUG_H_
#define _BGDEBUG_H_

#include <ultra64.h>

#define BG_DEBUG_PRIMARY 1
#define BG_DEBUG_SECONDARY 2

extern bool g_BgDebugEnabled;

void bgDebugReset(void);
void bgDebugInput(u16 pressed, bool allowEnable);
void bgDebugBeginFrame(void);
void bgDebugRecordRoom(s32 room, s32 layer);
Gfx *bgDebugRender(Gfx *gdl);
Gfx *bgDebugDrawHud(Gfx *gdl);

#endif
