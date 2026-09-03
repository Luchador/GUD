#ifndef _INIT_H_
#define _INIT_H_
#include <ultra64.h>

extern OSThread rmonThread;
extern OSThread idleThread;
extern OSThread mainThread;
extern OSThread shedThread;
extern OSMesgQueue gfxFrameMsgQ;
extern OSMesg gfxFrameMsgBuf[32];
extern OSMesgQueue *sched_cmdQ;

extern u32 piStatusReg;
extern u32 piDomAddr;

void init(void);
void *setSPToEnd(u8 *stack, u32 size);

#endif
