#ifndef _MEMA_H_
#define _MEMA_H_

#include <ultra64.h>

void memaReset(void *heapaddr, u32 heapsize);
void memaSingleDefragPass(void);
void *memaAlloc(u32 size);
void memaFree(void *addr, s32 size);
s32 memaGetLongestFree(void);
s32 memaRealloc(s32 addr, u32 newsize, u32 oldsize);

#endif
