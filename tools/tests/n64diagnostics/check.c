#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/n64diagnostics.c"
void test_rdp_preflight(void);

u8 cfb_16[2][320 * 240 * 2];
OSThread mainThread, g_tlbThread;
OSViMode osViModeTable[31];
u32 osTvType = 1;
u32 osMemSize = 0x400000;
static u16 guardedPixels[320 * 240 + 2];
u16 *testFramebuffer = guardedPixels + 1;
static u32 registers[TEST_REG_COUNT];
static u32 ticks;
static OSIntMask mask = 0x1234;
static OSThread *faulted;
static jmp_buf terminal;
static int armed;
static int invalidated;
static int haltRequested, freezeRequested, rejectHalt, spReads, ramReads;
static u32 fakeSp[0x2000 / 4];
static u32 fakeFifo[0x200 / 4];
static u32 fakeGpr[32];
static int rejectDump, rejectCode, dumpRuns;

OSIntMask osSetIntMask(OSIntMask next) { OSIntMask old = mask; mask = next; return old; }
u32 osGetCount(void) { return ticks; }
OSThread *__osGetCurrFaultedThread(void) { return faulted; }
void osInvalDCache(void *ptr, s32 size)
{
    assert(ptr == cfb_16 && size == 320 * 240 * 2);
    invalidated = 1;
}
u32 testRead(u32 reg)
{
    if (reg == SP_PC_REG) assert(registers[SP_STATUS_REG] & SP_STATUS_HALT);
    if (reg >= SP_DMEM_START && reg < SP_DMEM_START + 0x2000) {
        assert(haltRequested && freezeRequested);
        assert((registers[SP_STATUS_REG] & 13) == SP_STATUS_HALT);
        spReads++;
        if (rejectCode && reg >= SP_IMEM_START && reg <= SP_IMEM_START + 128)
            return fakeSp[(reg - SP_DMEM_START) / 4] ^ 1;
        return fakeSp[(reg - SP_DMEM_START) / 4];
    }
    if (reg >= TEST_REG_COUNT) {
        assert(haltRequested && freezeRequested);
        if (reg == 0x3ffff8 || reg == 0x3ffffc) {
            ramReads++;
            return 0xabcdef01;
        }
        assert(reg >= 0x1000 && reg < 0x1200 && !(reg & 3));
        ramReads++;
        return fakeFifo[(reg - 0x1000) / 4];
    }
    return registers[reg];
}
void testWrite(u32 reg, u32 value)
{
    unsigned i;
    if (reg >= SP_IMEM_START && reg <= SP_IMEM_START + 128) {
        assert(haltRequested && freezeRequested && !rejectHalt);
        assert(registers[SP_STATUS_REG] & SP_STATUS_HALT);
        fakeSp[(reg - SP_DMEM_START) / 4] = value;
        return;
    }
    if (reg == SP_STATUS_REG) {
        if (value == SP_SET_HALT) {
            haltRequested = 1;
            if (!rejectHalt) registers[reg] |= SP_STATUS_HALT;
            return;
        }
        assert(value == (SP_CLR_HALT | SP_CLR_BROKE | SP_CLR_SSTEP | SP_CLR_INTR_BREAK));
        assert(freezeRequested && haltRequested && registers[SP_PC_REG] == 0);
        dumpRuns++;
        registers[reg] = 0;
        if (rejectDump) return;
        /* Execute the terminal dumper's scalar stores and BREAK. Verify that
         * it never modifies a GPR, sends DMA, or accesses the RDP. */
        for (i = 0; i < 32; i++) {
            u32 word = fakeSp[0x1000 / 4 + i];
            u32 address = word & 0xffff, source = (word >> 16) & 31;
            assert((word >> 26) == 43 && ((word >> 21) & 31) == 0);
            assert(address == 0xf00 + i * 4 && source == i);
            fakeSp[address / 4] = fakeGpr[source];
        }
        assert(fakeSp[0x1080 / 4] == 0x0000000d);
        registers[reg] = SP_STATUS_HALT | SP_STATUS_BROKE;
        return;
    }
    if (reg == DPC_STATUS_REG) {
        assert(value == DPC_SET_FREEZE);
        freezeRequested = 1;
        registers[reg] |= 2;
        return;
    }
    registers[reg] = value;
    /* The production code's final VI enable is the test boundary. */
    if (reg == VI_CONTROL_REG && value) {
        assert(armed && invalidated && mask == OS_IM_NONE);
        assert(haltRequested && freezeRequested);
        assert(guardedPixels[0] == 0xabcd && guardedPixels[320 * 240 + 1] == 0xabcd);
        longjmp(terminal, 1);
    }
}
void osSetTimer(OSTimer *a,u64 b,u64 c,OSMesgQueue *d,OSMesg e) {(void)a;(void)b;(void)c;(void)d;(void)e;}
void osRecvMesg(OSMesgQueue *a,OSMesg *b,s32 c) {(void)a;(void)b;(void)c;}
void osCreateMesgQueue(OSMesgQueue *a,OSMesg *b,s32 c) {(void)a;(void)b;(void)c;}
void osCreateThread(OSThread *a,s32 b,void (*c)(void *),void *d,void *e,s32 f)
{(void)a;(void)b;(void)c;(void)d;(void)e;assert(f > 40 && f < 150);}
void osStartThread(OSThread *a) {(void)a;}

static void reset(void)
{
    memset((void *)&g_Diag, 0, sizeof(g_Diag));
    memset(&g_DiagStop, 0, sizeof(g_DiagStop));
    memset(&g_DiagCheck, 0, sizeof(g_DiagCheck));
    memset(g_DiagChecks, 0, sizeof(g_DiagChecks));
    memset(&g_DiagRunningCheck, 0, sizeof(g_DiagRunningCheck));
    memset(fakeGpr, 0, sizeof(fakeGpr));
    memset(fakeSp, 0, sizeof(fakeSp));
    g_DiagCheckNext = g_DiagCheckSerial = g_DiagRunningSerial = g_DiagRunningList = 0;
    rejectDump = rejectCode = dumpRuns = 0;
    memset(&g_tlbThread, 0, sizeof(g_tlbThread));
    memset(registers, 0, sizeof(registers));
    guardedPixels[0] = guardedPixels[320 * 240 + 1] = 0xabcd;
    osViModeTable[2].comRegs.ctrl = 2;
    osViModeTable[2].comRegs.width = 320;
    osViModeTable[16] = osViModeTable[30] = osViModeTable[2];
    ticks = 0;
    mask = 0x1234;
    faulted = NULL;
    armed = 1;
    invalidated = 0;
    haltRequested = freezeRequested = rejectHalt = spReads = ramReads = 0;
    osMemSize = 0x400000;
}

static void test_graphics_capture(void)
{
    unsigned i;
    reset();
    n64DiagTask(1, (void *)0x8001c340, (void *)0x80010000, 0x200,
            (void *)0x80001000, (void *)0x80001200);
    assert(mask == 0x1234 && g_Diag.taskType == 1);
    registers[SP_STATUS_REG] = 0xc0;
    registers[SP_PC_REG] = 0x1a0;
    registers[DPC_STATUS_REG] = 0x7c0;
    registers[DPC_CURRENT_REG] = 0x1080;
    registers[DPC_END_REG] = 0x1078;
    fakeSp[(0x1000 + 0x1a0) / 4] = 0x1680fffd;
    fakeSp[0x150 / 4] = 0x1000;
    fakeSp[0x154 / 4] = 0x1200;
    for (i = 0; i < sizeof(fakeFifo) / sizeof(fakeFifo[0]); i++) fakeFifo[i] = 0x12340000 + i;
    n64DiagCapture();
    assert(g_DiagStop.spStatus == 0xc0 && g_DiagStop.dpStatus == 0x7c0);
    assert(g_DiagStop.spPc == 0x1a0 && g_DiagStop.halted && g_DiagStop.spMemoryReady);
    assert(g_DiagStop.imemAddress[4] == 0x1a0 && g_DiagStop.imemWord[4] == 0x1680fffd);
    assert(g_DiagStop.dmemStart == 0x1000 && g_DiagStop.dmemEnd == 0x1200);
    assert(g_DiagStop.rdpValid == 0xffff && ramReads == 32);
    assert(g_DiagStop.rdpAddress[14] == 0x1080 && g_DiagStop.rdpWord[28] == 0x12340020);
    for (i = 0; i < DIAG_PAGES; i++) {
        n64DiagPage(testFramebuffer, i, "TEST", NULL);
        assert(guardedPixels[0] == 0xabcd && guardedPixels[320 * 240 + 1] == 0xabcd);
    }

    /* Audio replaces task metadata, but not the graphics FIFO bounds. */
    n64DiagTask(2, (void *)0x80020000, (void *)0x80030000, 0x80, NULL, NULL);
    assert(g_Diag.taskType == 2 && g_Diag.fifoStart == 0x80001000);

    reset();
    rejectHalt = 1;
    n64DiagCapture();
    assert(!g_DiagStop.halted && g_DiagStop.spPc == 0xffffffff && !spReads);
    reset();
    registers[SP_STATUS_REG] = SP_STATUS_DMA_BUSY;
    n64DiagCapture();
    assert(g_DiagStop.halted && !g_DiagStop.spMemoryReady && !spReads);

    /* A wrapped FIFO cursor must not cause out-of-bounds memory reads. */
    reset();
    n64DiagTask(1, NULL, NULL, 0, (void *)0x80001000, (void *)0x80001200);
    registers[DPC_CURRENT_REG] = 0x1000;
    n64DiagCapture();
    assert(g_DiagStop.rdpValid == 0xc000 && ramReads == 4);
    reset();
    n64DiagTask(1, NULL, NULL, 0, (void *)0x80401000, (void *)0x80401200);
    registers[DPC_CURRENT_REG] = 0x401080;
    n64DiagCapture();
    assert(!g_DiagStop.rdpValid && !ramReads);
    reset();
    n64DiagTask(1, NULL, NULL, 0, (void *)0x80001000, (void *)0x80001200);
    registers[DPC_CURRENT_REG] = 0x1080;
    registers[DPC_STATUS_REG] = DPC_STATUS_XBUS_DMEM_DMA;
    n64DiagCapture();
    assert(!g_DiagStop.rdpValid && !ramReads);
    assert(n64DiagRamRange(0xa0001000, 0x200));
    assert(!n64DiagRamRange(0x70001000, 0x200));
    assert(!n64DiagRamRange(0x80001000, 0xffffffff));
}

static void test_register_capture(void)
{
    unsigned i;
    reset();
    n64DiagTask(1, NULL, (void *)0x80001000, 0x100, NULL, NULL);
    registers[SP_PC_REG] = 0x1b8;
    for (i = 1; i < 32; i++) fakeGpr[i] = 0xa0000000 + i;
    fakeGpr[26] = 0x1080;
    fakeGpr[27] = 0x6b0;
    for (i = 0; i < 6; i++) fakeSp[0x6a0 / 4 + i] = 0xb0000000 + i;
    n64DiagCapture();
    assert(dumpRuns == 1 && g_DiagStop.registersValid);
    assert(g_DiagStop.spPc == 0x1b8); /* Captured PC predates the dumper. */
    assert(!memcmp(g_DiagStop.registers, fakeGpr, sizeof(fakeGpr)));
    assert(g_DiagStop.inputValid == 0x707 && ramReads == 6);
    for (i = 0; i < 6; i++) {
        assert(g_DiagStop.inputSpWord[i] == 0xb0000000 + i);
        assert(g_DiagStop.inputRamWord[i] == fakeFifo[0x70 / 4 + i]);
    }
    assert(g_DiagStop.inputRamAddress[2] == 0x1080);
    assert(g_DiagStop.inputSpAddress[2] == 0x6b0);
    reset();
    rejectDump = 1;
    n64DiagCapture();
    assert(dumpRuns == 1 && !g_DiagStop.registersValid && !g_DiagStop.inputValid);
    assert(registers[SP_STATUS_REG] & SP_STATUS_HALT);
    reset();
    rejectCode = 1;
    n64DiagCapture();
    assert(!dumpRuns && !g_DiagStop.registersValid);
    reset();
    rejectHalt = 1;
    n64DiagCapture();
    assert(!dumpRuns);
    reset();
    registers[SP_STATUS_REG] = SP_STATUS_DMA_BUSY;
    n64DiagCapture();
    assert(!dumpRuns);
    reset();
    n64DiagTask(1, NULL, NULL, 0, NULL, NULL);
    fakeGpr[26] = 0x400008; /* crosses installed-RAM boundary */
    fakeGpr[27] = 0x7e0; /* last two buffered words, then outside input buffer */
    n64DiagCapture();
    assert(g_DiagStop.inputValid == 0x301 && ramReads == 2);
}

static void test_task_check_matching(void)
{
    N64RdpCheck result = {0};
    unsigned i;
    reset();
    result.status = N64RDP_PARTIAL;
    result.commands = 12;
    result.reason = "CONDITIONAL CULL LIST";
    n64DiagSaveCheck((void *)0x80001000, 0x100, &result);
    result.status = N64RDP_CHECKED;
    result.commands = 20;
    n64DiagSaveCheck((void *)0x80002000, 0x200, &result);
    n64DiagTask(1, NULL, (void *)0x80001000, 0x100, NULL, NULL);
    assert(g_DiagRunningCheck.status == N64RDP_PARTIAL && g_DiagRunningCheck.commands == 12);
    assert(g_DiagRunningList == 0x80001000 && g_DiagRunningSerial == 1);
    assert(g_DiagCheckList == 0x80002000 && g_DiagCheck.commands == 20);
    n64DiagTask(2, NULL, (void *)0x80003000, 0x80, NULL, NULL);
    assert(g_DiagRunningSerial == 1);
    n64DiagTask(1, NULL, (void *)0x80001000, 0x100, NULL, NULL);
    assert(g_DiagRunningCheck.commands == 12); /* Resume after audio. */
    result.commands = 30;
    n64DiagSaveCheck((void *)0x80001000, 0x100, &result);
    n64DiagTask(1, NULL, (void *)0x80001000, 0x100, NULL, NULL);
    assert(g_DiagRunningCheck.commands == 30 && g_DiagRunningSerial == 3);
    n64DiagTask(1, NULL, (void *)0x80001000, 0x108, NULL, NULL);
    assert(!g_DiagRunningCheck.status && !g_DiagRunningCheck.commands && !g_DiagRunningSerial);
    for (i = 0; i < DIAG_CHECK_SLOTS + 1; i++)
        n64DiagSaveCheck((void *)0x80002000, 0x200, &result);
    n64DiagTask(1, NULL, (void *)0x80001000, 0x100, NULL, NULL);
    assert(!g_DiagRunningCheck.status); /* Eviction must not borrow another task. */
    assert(mask == 0x1234);
}

static void test_progress_and_watchdog(void)
{
    reset();
    n64DiagInit();
    ticks = 0xfffffff0U;
    n64DiagStage(0x16);
    assert(g_Diag.stage == 0x16 && mask == 0x1234);
    n64DiagPoll(ticks + DIAG_TIMEOUT_TICKS - 1);
    if (!setjmp(terminal)) {
        n64DiagPoll(ticks + DIAG_TIMEOUT_TICKS);
        assert(!"watchdog did not take over");
    }
    reset();
    n64DiagInit();
    ticks = DIAG_TIMEOUT_TICKS - 1;
    n64DiagStep(N64DIAG_SETUP);
    n64DiagPoll(DIAG_TIMEOUT_TICKS + 1);
    assert(g_Diag.phase == N64DIAG_SETUP && !invalidated);
}

static void test_fault_classification(void)
{
    OSThread gameFault;
    reset();
    memset(&gameFault, 0, sizeof(gameFault));
    gameFault.flags = OS_FLAG_FAULT;
    gameFault.context.cause = 8;
    gameFault.context.badvaddr = 0x7f001000;
    faulted = &gameFault;
    n64DiagPoll(0); /* Valid game paging must continue normally. */
    assert(!invalidated);
    gameFault.context.cause = 16; /* Address error. */
    if (!setjmp(terminal)) {
        n64DiagPoll(0);
        assert(!"CPU fault was not reported");
    }
    reset();
    g_tlbThread.flags = OS_FLAG_FAULT;
    g_tlbThread.context.cause = 8;
    g_tlbThread.context.badvaddr = 0x7f001000;
    faulted = &g_tlbThread;
    if (!setjmp(terminal)) {
        n64DiagPoll(0);
        assert(!"pager fault was not reported");
    }
}

static void test_dma(void)
{
    static const u32 requests[][3] = {
        {0x80001004,0x1000,64}, {0x80001000,0x1001,64},
        {0x80001000,0x1000,0}, {0x80001000,0x1000,63},
        {0x80001000,0x1000,0x1000002}
    };
    volatile unsigned i;
    reset();
    n64DiagDmaStart((void *)0x80001008, (void *)0x1002, 64);
    assert(g_Diag.dmaWaiting == 1 && mask == 0x1234);
    n64DiagDmaResult(0);
    n64DiagDmaDone();
    assert(!g_Diag.dmaWaiting && !invalidated);
    for (i = 0; i < sizeof(requests) / sizeof(requests[0]); i++) {
        reset();
        if (!setjmp(terminal)) {
            n64DiagDmaStart((void *)(uintptr_t)requests[i][0],
                    (void *)(uintptr_t)requests[i][1], requests[i][2]);
            assert(!"invalid DMA was accepted");
        }
    }
    reset();
    if (!setjmp(terminal)) {
        n64DiagDmaResult(-1);
        assert(!"failed queue submission was accepted");
    }
}

static void test_preflight_stop(void)
{
    u16 expected[320 * 240];
    reset();
    if (!setjmp(terminal)) {
        n64DiagCheckGfx((void *)0x80001001, 8);
        assert(!"misaligned list was accepted for submission");
    }
    assert(g_DiagCheck.status == N64RDP_HAZARD);
    assert(g_DiagCheckList == 0x80001001 && g_DiagCheck.address == 0x1001);
    memcpy(expected, testFramebuffer, sizeof(expected));
    n64DiagPage(testFramebuffer, 3, "GBI PREFLIGHT HAZARD", NULL);
    assert(!memcmp(expected, testFramebuffer, sizeof(expected)));
}

static void write_preview(const char *path)
{
    FILE *f;
    unsigned i, page;
    char filename[1024];
    reset();
    ticks = 12345;
    n64DiagStage(0x29);
    n64DiagStep(N64DIAG_WAIT_FRAME);
    n64DiagDmaStart((void *)0x80100000, (void *)0x00802400, 0x580);
    n64DiagAllocation(4, 0x580, (void *)0x80100000, (void *)0x80200000);
    mainThread.id = 3;
    mainThread.context.pc = 0x7f012340;
    mainThread.context.ra = 0x7f011100;
    mainThread.context.cause = 16;
    mainThread.context.badvaddr = 0x12345678;
    n64DiagTask(1, (void *)0x8001c340, (void *)0x80010000, 0x200,
            (void *)0x80001000, (void *)0x80001200);
    registers[SP_STATUS_REG] = 0xc0;
    registers[DPC_STATUS_REG] = 0x7c0;
    registers[SP_PC_REG] = 0x1a0;
    registers[DPC_CURRENT_REG] = 0x1080;
    for (i = 1; i < 32; i++) fakeGpr[i] = 0x12340000 + i;
    fakeGpr[18] = 0x70;
    fakeGpr[19] = 0x1060;
    fakeGpr[26] = 0x1080;
    fakeGpr[27] = 0x6b0;
    fakeGpr[28] = 0x80;
    fakeSp[0x6a0 / 4] = 0x04f00100;
    fakeSp[0x6a4 / 4] = 0x00200000;
    if (!setjmp(terminal)) n64DiagFault(&mainThread);
    g_DiagCheck.status = N64RDP_HAZARD;
    g_DiagCheck.reason = "INVALID RSP DMA ADDRESS";
    g_DiagCheck.address = 0x00123458;
    g_DiagCheck.word0 = 0x04f00100;
    g_DiagCheck.word1 = 0x00200004;
    g_DiagCheck.caller = 0x000c7200;
    g_DiagCheck.commands = 1234;
    g_DiagCheck.texture = 0x00198000;
    g_DiagCheck.textureSize = 2;
    g_DiagCheck.colorSize = 2;
    g_DiagCheck.modeHi = 0x00992c2f;
    g_DiagCheck.modeLo = 0xc8102878;
    g_DiagCheckList = 0x800c7160;
    g_DiagRunningCheck = g_DiagCheck;
    g_DiagRunningCheck.status = N64RDP_CHECKED;
    g_DiagRunningCheck.reason = "CHECKED KNOWN HAZARDS";
    g_DiagRunningList = g_DiagCheckList;
    g_DiagRunningSerial = 123;
    g_DiagCheck.status = N64RDP_CHECKED;
    g_DiagCheckList = 0x800d7160;
    for (page = 0; page < DIAG_PAGES; page++) {
        if (page) {
            assert(snprintf(filename, sizeof(filename), "%s.page%u.ppm", path, page + 1) < (int)sizeof(filename));
        } else {
            assert(snprintf(filename, sizeof(filename), "%s", path) < (int)sizeof(filename));
        }
        n64DiagPage(testFramebuffer, page, "NO MAIN PROGRESS FOR 10 SECONDS", NULL);
        f = fopen(filename, "wb");
        assert(f);
        fprintf(f, "P6\n320 240\n255\n");
        for (i = 0; i < 320 * 240; i++) {
            unsigned char v = testFramebuffer[i] == 0xffff ? 255 : 0;
            fputc(v, f); fputc(v, f); fputc(v, f);
        }
        fclose(f);
    }
}

int main(int argc, char **argv)
{
    test_progress_and_watchdog();
    test_fault_classification();
    test_dma();
    test_graphics_capture();
    test_register_capture();
    test_task_check_matching();
    test_preflight_stop();
    test_rdp_preflight();
    if (argc == 2) write_preview(argv[1]);
    puts("N64 watchdog, DMA, fault, RSP/FIFO, task matching, scalar capture, GBI preflight and six-page framebuffer tests passed");
    return 0;
}
