#ifndef _DEBUGMENU_H_
#define _DEBUGMENU_H_

#include <ultra64.h>

typedef struct {
    u8 chr;
    u8 color;
} character;

void debmenuSetPos(s32 x, s32 y);
void debmenuSetFgColour(s32 r, s32 g, s32 b, s32 a);
void debmenuSetEnvColor(s32 r, s32 g, s32 b, s32 a);
void debmenuPrintString(const unsigned char *str);
Gfx *debmenuDraw(Gfx *gdl);

#ifdef LEFTOVERDEBUG
void debmenuWriteCharAtPos(s32 x, s32 y, unsigned char c);
#endif

#endif
