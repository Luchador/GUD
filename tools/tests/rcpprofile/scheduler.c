/* Real scheduler entry points with only OS/hardware and queueing stubbed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rcpprofile.h"
#include "scheduler_constants.inc"

typedef struct {
    struct { u32 type; void *output_buff; u32 *output_buff_size; } t;
} OSTask;
typedef struct OSScTask_s {
    struct OSScTask_s *next;
    u32 state, flags;
    OSTask list;
} OSScTask;
typedef struct {
    OSScTask *curRSPTask, *curRDPTask, *gfxListHead, *gfxListTail;
} OSSched;

static u32 ticks, counters[4], resets, starts, yields, reads, completed;
static s32 didYield;
static OSIntMask mask = 0x1234;
OSIntMask osSetIntMask(OSIntMask value) { OSIntMask old = mask; mask = value; return old; }
u32 osGetCount(void) { return ticks; }
void osWritebackDCacheAll(void) {}
void osDpSetStatus(u32 value)
{
    assert(value == (DPC_CLR_TMEM_CTR | DPC_CLR_PIPE_CTR |
                    DPC_CLR_CMD_CTR | DPC_CLR_CLOCK_CTR));
    memset(counters, 0, sizeof(counters));
    resets++;
}
void osSpTaskLoad(OSTask *t) { (void)t; }
void osSpTaskStartGo(OSTask *t) { (void)t; starts++; }
void osSpTaskYield(void) { yields++; }
s32 osSpTaskYielded(OSTask *t) { (void)t; return didYield; }
s32 osDpSetNextBuffer(void *p, u32 size) { (void)p; (void)size; return 0; }
static u32 readCounter(u32 address)
{
    assert(address >= DPC_CLOCK_REG && address <= DPC_TMEM_REG);
    assert(!((address - DPC_CLOCK_REG) & 3));
    reads++;
    return counters[(address - DPC_CLOCK_REG) / 4];
}
#define IO_READ(address) readCounter(address)

s32 __scSchedule(OSSched *sc, OSScTask **sp, OSScTask **dp, s32 state)
{ (void)sc; (void)sp; (void)dp; return state; }
s32 __scTaskComplete(OSSched *sc, OSScTask *t)
{
    RcpProfileSample s;
    (void)sc;
    if (t->list.t.type == M_GFXTASK && !(t->state & OS_SC_RCP_MASK)) {
        /* Publication must precede task release/reuse. */
        assert(rcpProfileGetSnapshot(&s));
        assert(s.rdpClock == counters[0] && reads == 4);
        completed++;
    }
    return 0;
}
void __scExec(OSSched *, OSScTask *, OSScTask *);
#include "scheduler_functions.inc"

static RcpProfileSample sample(void)
{
    RcpProfileSample s;
    assert(rcpProfileGetSnapshot(&s) && mask == 0x1234);
    return s;
}

static void runTask(s32 dpFirst)
{
    OSSched sc = {0};
    OSScTask gfx = {0}, audio = {0};
    RcpProfileSample s;
    gfx.list.t.type = M_GFXTASK;
    gfx.state = gfx.flags = OS_SC_XBUS;
    audio.list.t.type = M_AUDTASK;
    audio.state = OS_SC_NEEDS_RSP;
    rcpProfileReset();
    resets = starts = yields = reads = completed = 0;
    ticks = 100;
    __scExec(&sc, &gfx, &gfx);
    assert(resets == 1 && starts == 1);
    ticks = 130; __scYield(&sc);
    ticks = 140; __scYield(&sc); /* Repeat must retain the first request. */
    didYield = 1; ticks = 150; __scHandleRSP(&sc);
    assert((gfx.state & OS_SC_YIELDED) && sc.gfxListHead == &gfx);
    ticks = 160; __scExec(&sc, &audio, NULL);
    didYield = 0; ticks = 190; __scHandleRSP(&sc);
    counters[0] = 101; counters[1] = 102;
    counters[2] = 103; counters[3] = 104;
    ticks = 200; __scExec(&sc, &gfx, &gfx);
    assert(resets == 1 && starts == 3 && yields == 2);
    assert(!(gfx.state & (OS_SC_YIELD | OS_SC_YIELDED)));
    if (dpFirst) {
        ticks = 230; __scHandleRDP(&sc);
        assert(!rcpProfileGetSnapshot(&s));
        ticks = 250; __scHandleRSP(&sc);
    } else {
        ticks = 230; __scYield(&sc);
        ticks = 240; __scHandleRSP(&sc); /* Finishes instead of yielding. */
        assert(!rcpProfileGetSnapshot(&s));
        ticks = 260; __scHandleRDP(&sc);
    }
    s = sample();
    assert(s.rspTicks == (dpFirst ? 100U : 90U));
    assert(s.totalTicks == (dpFirst ? 150U : 160U));
    assert(s.rdpEndTicks == (dpFirst ? 130U : 160U));
    assert(s.audioTicks == 30 && s.yieldTicks == 50);
    assert(s.yieldCount == 1 && s.yieldLatencyTicks == 20);
    assert(s.rdpClock == 101 && s.rdpCommand == 102);
    assert(s.rdpPipe == 103 && s.rdpTmem == 104 && completed == 1);
    /* A fresh task at the same address must reset counters and sequence. */
    gfx.state = OS_SC_XBUS;
    ticks = 300; __scExec(&sc, &gfx, &gfx);
    assert(resets == 2 && !counters[0]);
    assert(sample().sequence == 1); /* Completed snapshot is retained. */
}

int main(void)
{
    runTask(0);
    runTask(1);
    puts("RCP scheduler hooks, yield/resume counters and completion ordering passed");
    return 0;
}
