#include "n64diagnostics.h"

#if N64_LOAD_DIAGNOSTICS
#include <PR/rcp.h>

#define DIAG_WIDTH 320
#define DIAG_HEIGHT 240
#define DIAG_TIMEOUT_TICKS 468750000U /* Ten seconds at standard N64 Count rate. */
#define DIAG_THREAD_ID 6
#define DIAG_THREAD_PRIORITY 60 /* Above game, audio, scheduler and TLB pager. */

extern u8 cfb_16[][DIAG_WIDTH * DIAG_HEIGHT * 2];
extern OSThread mainThread;
extern OSThread g_tlbThread;
extern OSThread *__osGetCurrFaultedThread(void);

/* All code, strings, font data and state stay in the resident segment.
 * The fatal screen must never need the game-code pager, ROM I/O or RSP. */
static OSThread g_DiagThread;
static u64 g_DiagStack[0x1000 / sizeof(u64)];
static OSTimer g_DiagTimer;
static OSMesgQueue g_DiagQueue;
static OSMesg g_DiagMessage;
static volatile struct {
    u32 enabled;
    u32 stage;
    u32 phase;
    u32 lastProgress;
    u32 steps;
    u32 dmaSource;
    u32 dmaTarget;
    u32 dmaSize;
    s32 dmaResult;
    u32 dmaWaiting;
    u32 bank;
    u32 bytes;
    u32 allocPos;
    u32 allocEnd;
} g_Diag;

static const char *g_DiagPhases[] = {
    "BOOT", "STAGE BEGIN", "POOLS", "PLAYERS", "GFX MEMORY", "STAGE LOAD",
    "TEXTURES", "FONTS", "BG HEADER", "BG DATA", "STAN", "BG PROCESS",
    "STAGE STATE", "GUARDS", "SETUP", "FX", "PATHS", "BOND", "INTRO",
    "VIDEO", "WAIT FRAME", "TICK", "VIEW", "RENDER", "SUBMIT", "UNLOAD"
};
typedef char DiagPhaseNamesMatch[
    sizeof(g_DiagPhases) / sizeof(g_DiagPhases[0]) == N64DIAG_PHASE_COUNT ? 1 : -1];

/* Five columns, seven rows per character. Digits, uppercase letters, colon. */
static const char g_DiagAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:";
static const u8 g_DiagFont[][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}, {0,4,4,0,4,4,0}
};

static void n64DiagText(volatile u16 *fb, s32 x, s32 y, const char *text)
{
    s32 glyph, row, col;
    while (*text) {
        for (glyph = 0; g_DiagAlphabet[glyph]; glyph++) {
            if (*text == g_DiagAlphabet[glyph]) {
                for (row = 0; row < 7; row++) {
                    for (col = 0; col < 5; col++) {
                        if ((g_DiagFont[glyph][row] & (16 >> col))
                                && x + col >= 0 && x + col < DIAG_WIDTH
                                && y + row >= 0 && y + row < DIAG_HEIGHT) {
                            fb[(y + row) * DIAG_WIDTH + x + col] = 0xffff;
                        }
                    }
                }
                break;
            }
        }
        x += 6;
        text++;
    }
}

static void n64DiagHex(volatile u16 *fb, s32 x, s32 y, u32 value)
{
    char text[9];
    s32 i;
    for (i = 7; i >= 0; i--) {
        text[i] = "0123456789ABCDEF"[value & 15];
        value >>= 4;
    }
    text[8] = 0;
    n64DiagText(fb, x, y, text);
}

static void n64DiagPair(volatile u16 *fb, s32 y, const char *a, u32 av,
        const char *b, u32 bv)
{
    n64DiagText(fb, 14, y, a);
    n64DiagHex(fb, 68, y, av);
    n64DiagText(fb, 164, y, b);
    n64DiagHex(fb, 224, y, bv);
}

static void n64DiagStop(const char *reason, OSThread *fault)
{
    volatile u16 *fb;
    OSViMode *mode;
    u32 spStatus, dpStatus, piStatus, spPc, dpCurrent, dpEnd;
    u32 i;

    /* Terminal takeover only. Capture evidence before halting graphics. Keep
     * interrupts disabled so no thread, VI manager or cache writeback replaces
     * this screen. Direct VI setup also works if the scheduler has faulted. */
    osSetIntMask(OS_IM_NONE);
    spStatus = IO_READ(SP_STATUS_REG);
    dpStatus = IO_READ(DPC_STATUS_REG);
    piStatus = IO_READ(PI_STATUS_REG);
    spPc = IO_READ(SP_PC_REG);
    dpCurrent = IO_READ(DPC_CURRENT_REG);
    dpEnd = IO_READ(DPC_END_REG);
    IO_WRITE(SP_STATUS_REG, SP_SET_HALT);
    IO_WRITE(DPC_STATUS_REG, DPC_SET_FREEZE);
    IO_WRITE(VI_CONTROL_REG, 0);

    osInvalDCache(cfb_16, DIAG_WIDTH * DIAG_HEIGHT * 2);
    fb = (volatile u16 *)PHYS_TO_K1(K0_TO_PHYS(cfb_16));
    for (i = 0; i < DIAG_WIDTH * DIAG_HEIGHT; i++) {
        fb[i] = 0x0001;
    }

    n64DiagText(fb, 14, 16, "GUD N64 DIAGNOSTIC 01");
    n64DiagText(fb, 14, 28, reason);
    n64DiagText(fb, 14, 40, "PHASE:");
    n64DiagText(fb, 62, 40, g_Diag.phase < N64DIAG_PHASE_COUNT
            ? g_DiagPhases[g_Diag.phase] : "UNKNOWN");
    n64DiagPair(fb, 52, "STAGE", g_Diag.stage, "STEPS", g_Diag.steps);
    n64DiagPair(fb, 64, "THREAD", fault ? (u32)fault->id : 0xffffffff,
            "CAUSE", fault ? fault->context.cause : 0);
    n64DiagPair(fb, 76, "PC", fault ? fault->context.pc : 0,
            "RA", fault ? (u32)fault->context.ra : 0);
    n64DiagPair(fb, 88, "BADVADDR", fault ? fault->context.badvaddr : 0,
            "FPCSR", fault ? fault->context.fpcsr : 0);
    n64DiagPair(fb, 100, "SP", fault ? (u32)fault->context.sp : 0,
            "SR", fault ? fault->context.sr : 0);
    n64DiagPair(fb, 112, "MAIN PC", mainThread.context.pc,
            "MAIN RA", (u32)mainThread.context.ra);
    n64DiagPair(fb, 124, "TLB PC", g_tlbThread.context.pc,
            "TLB RA", (u32)g_tlbThread.context.ra);
    n64DiagPair(fb, 136, "SPSTAT", spStatus, "DPSTAT", dpStatus);
    n64DiagPair(fb, 148, "PISTAT", piStatus, "SP PC", spPc);
    n64DiagPair(fb, 160, "DP CUR", dpCurrent, "DP END", dpEnd);
    n64DiagPair(fb, 172, "ROM", g_Diag.dmaSource, "SIZE", g_Diag.dmaSize);
    n64DiagPair(fb, 184, "RAM", g_Diag.dmaTarget, "DMA WAIT", g_Diag.dmaWaiting);
    n64DiagPair(fb, 196, "BANK", g_Diag.bank, "ALLOC", g_Diag.bytes);
    n64DiagPair(fb, 208, "POOL POS", g_Diag.allocPos, "POOL END", g_Diag.allocEnd);
    n64DiagText(fb, 14, 222, "PHOTO SCREEN THEN RESET CONSOLE");

    /* osTvType uses 0=PAL, 1=NTSC, 2=MPAL in this SDK. */
    mode = &osViModeTable[osTvType == 2 ? OS_VI_MPAL_LAN1
            : osTvType == 0 ? OS_VI_PAL_LAN1 : OS_VI_NTSC_LAN1];
    /* Equivalent base mode programming to __osViSwapContext, without waiting
     * for a VI interrupt or touching any game-owned video state. */
    IO_WRITE(VI_ORIGIN_REG, K0_TO_PHYS(cfb_16) + mode->fldRegs[0].origin);
    IO_WRITE(VI_WIDTH_REG, mode->comRegs.width);
    IO_WRITE(VI_BURST_REG, mode->comRegs.burst);
    IO_WRITE(VI_V_SYNC_REG, mode->comRegs.vSync);
    IO_WRITE(VI_H_SYNC_REG, mode->comRegs.hSync);
    IO_WRITE(VI_LEAP_REG, mode->comRegs.leap);
    IO_WRITE(VI_H_START_REG, mode->comRegs.hStart);
    IO_WRITE(VI_V_START_REG, mode->fldRegs[0].vStart);
    IO_WRITE(VI_V_BURST_REG, mode->fldRegs[0].vBurst);
    IO_WRITE(VI_INTR_REG, mode->fldRegs[0].vIntr);
    IO_WRITE(VI_X_SCALE_REG, mode->comRegs.xScale);
    IO_WRITE(VI_Y_SCALE_REG, mode->fldRegs[0].yScale);
    IO_WRITE(VI_CONTROL_REG, mode->comRegs.ctrl);
    for (;;) {}
}

void n64DiagFault(OSThread *thread)
{
    n64DiagStop("CPU FAULT", thread);
}

void n64DiagStep(enum N64DiagPhase phase)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_Diag.phase = phase;
    g_Diag.lastProgress = osGetCount();
    g_Diag.steps++;
    osSetIntMask(mask);
}

void n64DiagStage(s32 stage)
{
    g_Diag.stage = stage;
    n64DiagStep(N64DIAG_STAGE_BEGIN);
}

void n64DiagDmaStart(void *target, void *source, u32 size)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_Diag.dmaSource = (u32)source;
    g_Diag.dmaTarget = (u32)target;
    g_Diag.dmaSize = size;
    g_Diag.dmaWaiting = 1;
    g_Diag.dmaResult = 0;
    osSetIntMask(mask);
    if (((u32)target & 7) || ((u32)source & 1) || !size || (size & 1)
            || size > 0x1000000) {
        n64DiagStop("INVALID ROM DMA", NULL);
    }
}

void n64DiagDmaResult(s32 result)
{
    g_Diag.dmaResult = result;
    if (result != 0) {
        n64DiagStop("ROM DMA QUEUE FAILED", NULL);
    }
}

void n64DiagDmaDone(void)
{
    g_Diag.dmaWaiting = 0;
}

void n64DiagAllocation(u32 bank, u32 bytes, void *pos, void *end)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_Diag.bank = bank;
    g_Diag.bytes = bytes;
    g_Diag.allocPos = (u32)pos;
    g_Diag.allocEnd = (u32)end;
    osSetIntMask(mask);
}

static void n64DiagPoll(u32 now)
{
    OSThread *fault = __osGetCurrFaultedThread();
    /* Normal game-code TLB misses are handled by crashMain. A fault in the
     * pager itself cannot be handled by that same stopped thread. */
    if (fault && (fault->flags & OS_FLAG_FAULT)
            && (fault == &g_tlbThread || (fault->context.cause & 0x7c) != 8
                || (fault->context.badvaddr & 0xffc00000) != 0x7f000000)) {
        n64DiagFault(fault);
    }
    if (g_Diag.enabled && (u32)(now - g_Diag.lastProgress) >= DIAG_TIMEOUT_TICKS) {
        n64DiagStop("NO MAIN PROGRESS FOR 10 SECONDS", NULL);
    }
}

static void n64DiagMain(void *arg)
{
    for (;;) {
        osSetTimer(&g_DiagTimer, OS_USEC_TO_CYCLES(250000), 0,
                &g_DiagQueue, (OSMesg)1);
        osRecvMesg(&g_DiagQueue, NULL, OS_MESG_BLOCK);
        n64DiagPoll(osGetCount());
    }
}

void n64DiagInit(void)
{
    g_Diag.enabled = TRUE;
    n64DiagStep(N64DIAG_BOOT);
    osCreateMesgQueue(&g_DiagQueue, &g_DiagMessage, 1);
    osCreateThread(&g_DiagThread, DIAG_THREAD_ID, n64DiagMain, NULL,
            g_DiagStack + sizeof(g_DiagStack) / sizeof(g_DiagStack[0]),
            DIAG_THREAD_PRIORITY);
    osStartThread(&g_DiagThread);
}
#endif
