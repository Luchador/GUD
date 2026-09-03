#include <ultra64.h>
#include <PR/R4300.h>
#include <PR/os.h>
#include <PR/os_internal.h>
#include <PR/rcp.h>
#include <bondgame.h>
#include "boot.h"
#include "sched.h"
#include "rmon.h"
#include "tlb_resolve.h"
#include "tlb_hardwire.h"
#include "init.h"
#include "thread_config.h"


/**
 * @file init.c
 * This file contains the initial non bootstrap code ran.
 *
 * In particular, it:
 *   - loads the main data payload
 *   - starts idle and rmon loops
 *   - starts main loop
 */


#define NUM_FIELDS  1

#define MAXSP 7

#define MAXCODESIZE (0x101000 - 0x1050) 


/**
 * These 3 values (likely more as u32 piDeviceType = DEVICE_TYPE_CART could be 4x u8)
 * piDeviceType is completely unused but the most logical value here
 * piStatusReg, and piDomAddr are explicitly stored as 32bit values
*/
u32 piDeviceType = DEVICE_TYPE_CART;
u32 piStatusReg = PI_CLR_INTR;
u32 piDomAddr = PI_DOM1_ADDR2;

OSThread rmonThread;
OSThread idleThread;
OSThread mainThread;
OSThread shedThread;
OSMesgQueue gfxFrameMsgQ;
OSMesg gfxFrameMsgBuf[32];
OSMesgQueue *sched_cmdQ;

void mainproc(void *args);


/**
 * init - The real main entry point, called from boot.s
 * Loads the data segment from ROM
 * Installs the TLB miss handler
 * Then starts the main thread
 */
void init(void)
{
    u32 flags;
    u32 csegmentSize;
    u32 residentCodeSize;
    s32 i;
    s32 *dest;
    s32 *src;
    s32 *stack_pointer;

    csegmentSize = (u8 *)&_csegmentSegmentRomEnd - (u8 *)&_csegmentSegmentRomStart;
    osInvalDCache(&_csegmentSegmentStart, csegmentSize);
    osPiRawStartDma(OS_READ, &_csegmentSegmentRomStart, &_csegmentSegmentStart, csegmentSize);
    while (osPiGetStatus() & PI_STATUS_DMA_BUSY)
    {
    }

    residentCodeSize = (u32)&_gameSegmentRomStart - (u32)&_codeSegmentRomStart;
    if (residentCodeSize > MAXCODESIZE)
    {
        osPiRawStartDma(OS_READ, &_alt_startSegmentRomStart, &_alt_startSegmentStart, residentCodeSize - MAXCODESIZE);
        while ((osPiGetStatus() & PI_STATUS_DMA_BUSY))
        {
        }
    }

    osInitialize();

    // This sets up TLB CONTEXT to allow the TLB miss handler to work
    initTLBPrepareContext();

    // Copy the TLB miss handler to proper place
    src = &resolve_TLBaddress_for_InvalidHit;
    dest = (s32 *) K0BASE;
    while (dest < (s32 *) XUT_VEC)
    {
        *dest++ = *src++;
    }

    // Refresh Cache
    osWritebackDCacheAll();
    osInvalICache((void *) K0BASE, ICACHE_SIZE);

    // Cleanup TLB
    for (i = 2; i < NTLBENTRIES + 1; i++)
    {
        osUnmapTLB(i);
    }

    // Setup floating point register
    flags = __osGetFpcCsr();
    flags |= FPCSR_EI; // enable inexact operation
    flags |= FPCSR_EO; // enable overflow
    flags |= FPCSR_EZ; // enable division by zero
    flags |= FPCSR_EV; // enable invalid operation
    __osSetFpcCsr(flags);

    stack_pointer = setSPToEnd(sp_main, sizeof(sp_main));
    osCreateThread(&mainThread, MAIN_THREAD_ID, &mainproc, NULL, stack_pointer, MAIN_THREAD_PRIORITY);
    osStartThread(&mainThread);
}


/**
 * setSPToEnd - set stack pointer to end of stack
 * @param stack
 * @param size
 * @return stack+size-8
 */
void *setSPToEnd(u8 *stack, u32 size)
{
    return stack + size - 8;
}

/**
 * piStatusRegReset - set PI status register to trigger a reset
 * This is unused and is leftover from Indy debug,
 */
void piStatusRegReset(void)
{
    piStatusReg = PI_SET_RESET;
    piDomAddr = PI_DOM1_ADDR2;
}

/**
 * 1318	70000718
 * idleproc - infinite loop
 * @param arg doesn't matter as this is infinite loop
 */
void idleproc(void *arg)
{
    for (;;);
}


/**
 * idleCreateThread - creates an empty thread;
 */
void idleCreateThread(void)
{
    osCreateThread(&idleThread, IDLE_THREAD_ID, idleproc, NULL, setSPToEnd(&sp_idle, sizeof(sp_idle)), IDLE_THREAD_PRIORITY);
    osStartThread(&idleThread);
}

/**
 * 1390	70000790
 * rmonCreateThread - remote communication thread
 * rmonMain now forcably returns INDI_NOT_DETECTED (1)
 */
void rmonCreateThread(void)
{
    osCreateThread(&rmonThread, RMON_THREAD_ID, rmonMain, NULL, setSPToEnd(&sp_rmon, sizeof(sp_rmon)), RMON_THREAD_PRIORITY);
    osStartThread(&rmonThread);
}

/**
 * 13EC	700007EC
 * schedulerInitThread - create scheduler thread based on TV type
 */
void schedulerInitThread(void)
{
    osCreateMesgQueue(&gfxFrameMsgQ, &gfxFrameMsgBuf, 32);
    if (osTvType == OS_TV_MPAL)
    {
        osCreateScheduler(&os_scheduler, &shedThread, OS_VI_MPAL_LAN1, NUM_FIELDS);
    }
    else
    {
        osCreateScheduler(&os_scheduler, &shedThread, OS_VI_NTSC_LAN1, NUM_FIELDS);
    }

    osScAddClient(&os_scheduler, &gfxClient, &gfxFrameMsgQ, NULL);
    sched_cmdQ = osScGetCmdQ(&os_scheduler);
}

/**
 * mainproc - main game setup and loop
 * calls command line parser, debug console setup, etc.
 * @param args
 */
void mainproc(void *args)
{
    idleCreateThread();
    piCreateManager();
    rmonCreateThread();

    if (tokenReadIo())
    {
        osStopThread(RMON_THREAD_ID);
    }

    osSetThreadPri(RMON_THREAD_ID, MAIN_THREAD_PRIORITY);
    /**
     * Timers are initialized via:
     * schedulerInitThread -> osCreateScheduler -> osCreateViManager -> __osTimerServicesInit
     */
    schedulerInitThread();
    bossEntry();
}
