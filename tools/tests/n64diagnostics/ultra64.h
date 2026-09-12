/* Host-only types and services; not included by the N64 build. */
#ifndef DIAG_TEST_ULTRA64_H
#define DIAG_TEST_ULTRA64_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef u32 OSIntMask;
typedef void *OSMesg;
typedef struct { int unused; } OSTimer;
typedef struct { int unused; } OSMesgQueue;
typedef struct {
    u32 pc, cause, badvaddr, fpcsr, sr;
    u64 sp, ra, a0;
} TestContext;
typedef struct {
    s32 id;
    u32 flags;
    TestContext context;
} OSThread;
typedef struct {
    struct { u32 ctrl, width, burst, vSync, hSync, leap, hStart, xScale; } comRegs;
    struct { u32 origin, vStart, vBurst, vIntr, yScale; } fldRegs[2];
} OSViMode;
extern OSViMode osViModeTable[31];
extern u32 osTvType;
extern u32 osMemSize;
#define OS_VI_NTSC_LAN1 2
#define OS_VI_PAL_LAN1 16
#define OS_VI_MPAL_LAN1 30
#define OS_FLAG_FAULT 2
#define OS_IM_NONE 1
#define OS_MESG_BLOCK 1
#define OS_USEC_TO_CYCLES(n) ((u64)(n) * 46875 / 1000)
#define TRUE 1
#define FALSE 0
OSIntMask osSetIntMask(OSIntMask mask);
u32 osGetCount(void);
void osInvalDCache(void *ptr, s32 size);
void osSetTimer(OSTimer *timer, u64 first, u64 interval, OSMesgQueue *queue, OSMesg msg);
void osRecvMesg(OSMesgQueue *queue, OSMesg *msg, s32 flags);
void osCreateMesgQueue(OSMesgQueue *queue, OSMesg *msg, s32 count);
void osCreateThread(OSThread *thread, s32 id, void (*fn)(void *), void *arg, void *stack, s32 priority);
void osStartThread(OSThread *thread);
#endif
