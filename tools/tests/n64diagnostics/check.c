#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/n64diagnostics.c"

u8 cfb_16[2][320 * 240 * 2];
OSThread mainThread, g_tlbThread;
OSViMode osViModeTable[31];
u32 osTvType = 1;
static u16 guardedPixels[320 * 240 + 2];
u16 *testFramebuffer = guardedPixels + 1;
static u32 registers[TEST_REG_COUNT];
static u32 ticks;
static OSIntMask mask = 0x1234;
static OSThread *faulted;
static jmp_buf terminal;
static int armed;
static int invalidated;

OSIntMask osSetIntMask(OSIntMask next) { OSIntMask old = mask; mask = next; return old; }
u32 osGetCount(void) { return ticks; }
OSThread *__osGetCurrFaultedThread(void) { return faulted; }
void osInvalDCache(void *ptr, s32 size)
{
    assert(ptr == cfb_16 && size == 320 * 240 * 2);
    invalidated = 1;
}
u32 testRead(u32 reg) { return registers[reg]; }
void testWrite(u32 reg, u32 value)
{
    registers[reg] = value;
    /* The production code's final VI enable is the test boundary. */
    if (reg == VI_CONTROL_REG && value) {
        assert(armed && invalidated && mask == OS_IM_NONE);
        assert(registers[SP_STATUS_REG] == SP_SET_HALT);
        assert(registers[DPC_STATUS_REG] == DPC_SET_FREEZE);
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

static void write_preview(const char *path)
{
    FILE *f;
    unsigned i;
    reset();
    ticks = 12345;
    n64DiagStage(0x16);
    n64DiagStep(N64DIAG_BG_DATA);
    n64DiagDmaStart((void *)0x80100000, (void *)0x00802400, 0x580);
    n64DiagAllocation(4, 0x580, (void *)0x80100000, (void *)0x80200000);
    mainThread.id = 3;
    mainThread.context.pc = 0x7f012340;
    mainThread.context.ra = 0x7f011100;
    mainThread.context.cause = 16;
    mainThread.context.badvaddr = 0x12345678;
    if (!setjmp(terminal)) n64DiagFault(&mainThread);
    f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n320 240\n255\n");
    for (i = 0; i < 320 * 240; i++) {
        unsigned char v = testFramebuffer[i] == 0xffff ? 255 : 0;
        fputc(v, f); fputc(v, f); fputc(v, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    test_progress_and_watchdog();
    test_fault_classification();
    test_dma();
    if (argc == 2) write_preview(argv[1]);
    puts("N64 diagnostic watchdog, DMA, fault and framebuffer tests passed");
    return 0;
}
