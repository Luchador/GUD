#include <ultra64.h>
#include <PR/os.h>
#include "tlb_manage.h"
#include "crash.h"
#include "thread_config.h"

/**
 * @file deb_video.c
 * This file contains code to handle tlb paging. 
 * 
 * Additionally, it contains unused/removed debug code, and code for indy.
 * The function addresses seem to indicate these methods are all in the same file ...
 */


/**
 * Copied from n64devkit\ultra\usr\src\pr\demos\fault\crash.c
 */
#define MSG_FAULT    0x10

#define TLB_MESSAGE_QUEUE_SIZE      1

/*
-----------------------------------------------------------------
| ADDIU     | ADD Immediate Unsigned word                       |
|-----------|---------------------------------------------------|
|001001 (9) |   rs    |   rt    |          immediate            |
------6----------5---------5-------------------16----------------
 Format:  ADDIU rt, rs, immediate
 Purpose: To add a constant to a 32-bit integer.
 Descrip: rt = rs + immediate

-----------------------------------------------------------------
| SW        | Store Word                                        |
|-----------|---------------------------------------------------|
|101011 (43)|  base   |   rt    |             offset            |
------6----------5---------5-------------------16----------------
 Format:  SW rt, offset(base)
 Purpose: To store a word to memory.
 Descrip: word[base+offset] = rt

-----------------------------------------------------------------
| JR        | Jump Register                                     |
|-----------|---------------------------------------------------|
|  000000   |   rs    |     0000 0000 0000 000      |001000 (8) |
------6----------5------------------15--------------------6------
 Format:  JR rs
 Purpose: To branch to an instruction address in a register.
*/

/**
 * Add immediate unsigned word. Upper 6 bits out of 32.
 */
#define MIPS_OP_BITS_ADDIU 0x09

/**
 * Store word. Upper 6 bits out of 32.
 */
#define MIPS_OP_BITS_SW 0x2b

/**
 * Jump to register. Upper 6 bits out of 32.
 */
#define MIPS_OP_BITS_JR 0x00

/**
 * Five bits defining register source or destination, for stack pointer register.
 */
#define MIPS_REG_SOURCE_BITS_SP 0x1d

/**
 * Five bits defining register source or destination, for return address register.
 */
#define MIPS_REG_SOURCE_BITS_RA 0x1f

/**
 * Builds 32 bit word to perform ADDIU.
 * 
 * No error checking or input size validation are performed here.
 * 
 * @param source_r: source register number (5 bits).
 * @param dest_r: destination register number (5 bits).
 * @param imm: 16 bit immediate value to add.
 */
#define MIPS_INSTR_ADDIU(source_r, dest_r, imm) ((MIPS_OP_BITS_ADDIU << 26) | (source_r << 21) | (dest_r << 16) | imm)

/**
 * Builds 32 bit word to perform SW.
 * 
 * No error checking or input size validation are performed here.
 * 
 * @param val_r: source register holding value to store (5 bits).
 * @param dest_r: register holding destination base address (5 bits).
 * @param offset: 16 bit offset from base address to store value at.
 */
#define MIPS_INSTR_SW(val_r, dest_r, offset) ((MIPS_OP_BITS_SW << 26) | (dest_r << 21) | (val_r << 16) | offset)

/**
 * Builds 32 bit word to perform SW.
 * 
 * No error checking or input size validation are performed here.
 * 
 * Note: lower 6 bits of JR instruction are always 0x08.
 * 
 * @param dest_r: register holding destination address (5 bits).
 */
#define MIPS_INSTR_JR(dest_r) ((MIPS_OP_BITS_JR << 26) | (dest_r << 21) | 8)

/**
 * Any MIPS 32-bit instruction matching $sp increment
 * 
 * 0x27BD0000 = # addiu $sp, $sp <any_immediate>
 */
#define MIPS_ADDIU_SP_SP_ANY        MIPS_INSTR_ADDIU(MIPS_REG_SOURCE_BITS_SP, MIPS_REG_SOURCE_BITS_SP, 0)
#define MIPS_ADDIU_SP_SP_ANY_MASK   0xFFFF0000

/**
 * Any MIPS 32-bit instruction matching sw into $sp
 * 
 * 0xAFA00000 = sw <any_register>, 0x[any_offset]($sp)
 */
#define MIPS_SW_SP_ANY       MIPS_INSTR_SW(0, MIPS_REG_SOURCE_BITS_SP, 0)
#define MIPS_SW_SP_ANY_MASK  0xFFE00000

/**
 * Any MIPS 32-bit instruction (should only be one...) matching jump to return address.
 * 
 * 0x03E00008 = jr ra
 */
#define MIPS_JR_RA MIPS_INSTR_JR(MIPS_REG_SOURCE_BITS_RA)

OSThread g_tlbThread;

/**
 * Stack for the fault/TLB-pager thread (g_tlbThread / crashMain).
 *
 * Sized from measurement of worst-case call depth from
 * crashMain() is 0x158 bytes (crashMain -> tlbmanage
 * TranslateLoadRomFromTlbAddress -> romCopy -> doRomCopy -> osPiStartDma ->
 * osSendMesg -> osStartThread; no indirect calls anywhere in the reachable
 * subgraph). Retail shipped with an
 * effective allowance of 0x4F0, so 0x800 is ~6x the measured bound 
 * and 1.6x the retail-proven allowance. Resized down from original 0x2300.
 */
char g_tlbThreadStack[0x800];

OSMesgQueue g_faultMesgQ;
OSMesg *g_faultMesgBuf;

// Begin forward declarations.

void crashMain(void* arg0);

// End forward declarations.

void crashInit(void)
{
    osCreateMesgQueue(&g_faultMesgQ, (OSMesg *)&g_faultMesgBuf, TLB_MESSAGE_QUEUE_SIZE);
    osCreateThread(&g_tlbThread, TLB_THREAD_ID, &crashMain, NULL, &g_tlbThreadStack[sizeof(g_tlbThreadStack)], TLB_THREAD_PRIORITY);
    osStartThread(&g_tlbThread);
}

extern OSThread *__osRunQueue;

void crashMain(void* arg0)
{
    OSMesg msg = 0;
    OSIntMask mask;
    static OSThread *curr;
    /**
     * Target generates 5 separate dereferences of g_faultedThreadPtr,
     * only way I can get that to happen is with a pointer to a pointer.
     */
    //OSThread **ppfaultedThread = &g_faultedThreadPtr;

    osSetEventMesg(OS_EVENT_FAULT, &g_faultMesgQ, (OSMesg)MSG_FAULT);

    while (1)
    {
        osRecvMesg(&g_faultMesgQ, &msg, OS_MESG_BLOCK);
        mask = osSetIntMask(OS_IM_NONE);
        curr = __osGetCurrFaultedThread();
        //*ppfaultedThread = curr;

        if (curr == NULL)
        {
            continue;
        }

        else if (((curr->context.cause & 0x7c) == 8) && ((curr->context.badvaddr & 0xFFC00000) == (u32)0x7F000000))
        {
            tlbmanageTranslateLoadRomFromTlbAddress(curr->context.badvaddr);
            curr->state = OS_STATE_RUNNABLE | OS_STATE_WAITING;;
            curr->flags = OS_READ;

            __osEnqueueThread(&__osRunQueue, curr);
            osSetIntMask(mask);
            osYieldThread();
        }
        else
        {
            break;
        }
    }

    osSetIntMask(mask);

    // infinite loop
    while (1) {}

}
