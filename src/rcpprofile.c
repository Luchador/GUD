#include "rcpprofile.h"

#define RCP_PROFILE_HISTORY 64
#define RCP_COUNTER_MASK 0x00ffffff
/* 2^24 RCP clocks expressed in CPU Count ticks (3/4 of the RCP clock). */
#define RCP_COUNTER_RANGE_TICKS 0x00c00000

static struct {
    const void *gfxTask;
    const void *audioTask;
    u32 start;
    u32 rspStart;
    u32 audioStart;
    u32 pauseStart;
    u32 requestStart;
    s32 rspRunning;
    s32 paused;
    s32 yieldPending;
    s32 rspDone;
    s32 rdpDone;
    s32 valid;
    u32 sequence;
    u32 history[RCP_PROFILE_HISTORY];
    u32 historyNext;
    u32 historyCount;
    u64 historySum;
    RcpProfileSample active;
    RcpProfileSample latest;
} g_RcpProfile;

void rcpProfileReset(void)
{
    bzero(&g_RcpProfile, sizeof(g_RcpProfile));
}

static void rcpProfilePublish(u32 now)
{
    RcpProfileSample *sample = &g_RcpProfile.active;
    u32 i;

    if (!g_RcpProfile.rspDone || !g_RcpProfile.rdpDone) {
        return;
    }

    /* Audio may still be running after graphics SP completion. Only charge
     * the part before this graphics task's final completion notification. */
    if (g_RcpProfile.audioTask) {
        sample->audioTicks += now - g_RcpProfile.audioStart;
    }
    sample->totalTicks = now - g_RcpProfile.start;
    g_RcpProfile.historySum -= g_RcpProfile.history[g_RcpProfile.historyNext];
    g_RcpProfile.history[g_RcpProfile.historyNext] = sample->totalTicks;
    g_RcpProfile.historySum += sample->totalTicks;
    g_RcpProfile.historyNext = (g_RcpProfile.historyNext + 1) % RCP_PROFILE_HISTORY;
    if (g_RcpProfile.historyCount < RCP_PROFILE_HISTORY) {
        g_RcpProfile.historyCount++;
    }
    sample->historyCount = g_RcpProfile.historyCount;
    sample->averageTicks = g_RcpProfile.historySum / sample->historyCount;
    sample->maximumTicks = 0;
    for (i = 0; i < sample->historyCount; i++) {
        if (g_RcpProfile.history[i] > sample->maximumTicks) {
            sample->maximumTicks = g_RcpProfile.history[i];
        }
    }
    g_RcpProfile.latest = *sample;
    g_RcpProfile.valid = TRUE;
    g_RcpProfile.gfxTask = NULL;
}

void rcpProfileRspStart(const void *task, s32 type, s32 resumed, u32 now)
{
    if (type == M_AUDTASK) {
        g_RcpProfile.audioTask = task;
        g_RcpProfile.audioStart = now;
    } else if (type == M_GFXTASK) {
        if (!resumed) {
            bzero(&g_RcpProfile.active, sizeof(g_RcpProfile.active));
            g_RcpProfile.gfxTask = task;
            g_RcpProfile.start = now;
            g_RcpProfile.paused = FALSE;
            g_RcpProfile.yieldPending = FALSE;
            g_RcpProfile.rspDone = FALSE;
            g_RcpProfile.rdpDone = FALSE;
            g_RcpProfile.active.sequence = ++g_RcpProfile.sequence;
        }
        if (task == g_RcpProfile.gfxTask) {
            if (g_RcpProfile.paused) {
                g_RcpProfile.active.yieldTicks += now - g_RcpProfile.pauseStart;
                g_RcpProfile.paused = FALSE;
            }
            g_RcpProfile.rspStart = now;
            g_RcpProfile.rspRunning = TRUE;
        }
    }
}

void rcpProfileYieldRequested(const void *task, u32 now)
{
    if (task == g_RcpProfile.gfxTask && g_RcpProfile.rspRunning
            && !g_RcpProfile.yieldPending) {
        g_RcpProfile.yieldPending = TRUE;
        g_RcpProfile.requestStart = now;
    }
}

void rcpProfileRspDone(const void *task, u32 now, s32 yielded)
{
    u32 latency;

    if (task == g_RcpProfile.audioTask) {
        if (g_RcpProfile.gfxTask) {
            g_RcpProfile.active.audioTicks += now - g_RcpProfile.audioStart;
        }
        g_RcpProfile.audioTask = NULL;
    } else if (task == g_RcpProfile.gfxTask && g_RcpProfile.rspRunning) {
        g_RcpProfile.active.rspTicks += now - g_RcpProfile.rspStart;
        g_RcpProfile.rspRunning = FALSE;
        if (yielded) {
            g_RcpProfile.active.yieldCount++;
            g_RcpProfile.paused = TRUE;
            g_RcpProfile.pauseStart = now;
            if (g_RcpProfile.yieldPending) {
                latency = now - g_RcpProfile.requestStart;
                if (latency > g_RcpProfile.active.yieldLatencyTicks) {
                    g_RcpProfile.active.yieldLatencyTicks = latency;
                }
            }
        } else {
            g_RcpProfile.rspDone = TRUE;
        }
        g_RcpProfile.yieldPending = FALSE;
        rcpProfilePublish(now);
    }
}

void rcpProfileRdpDone(const void *task, u32 now, u32 clock, u32 command,
        u32 pipe, u32 tmem)
{
    if (task != g_RcpProfile.gfxTask || g_RcpProfile.rdpDone) {
        return;
    }
    g_RcpProfile.active.rdpEndTicks = now - g_RcpProfile.start;
    g_RcpProfile.active.rdpClock = clock & RCP_COUNTER_MASK;
    g_RcpProfile.active.rdpCommand = command & RCP_COUNTER_MASK;
    g_RcpProfile.active.rdpPipe = pipe & RCP_COUNTER_MASK;
    g_RcpProfile.active.rdpTmem = tmem & RCP_COUNTER_MASK;
    /* Conservative warning: the wall interval can exceed a counter's range
     * even when that particular unit was not busy for the entire interval. */
    g_RcpProfile.active.counterRangeExceeded =
        g_RcpProfile.active.rdpEndTicks >= RCP_COUNTER_RANGE_TICKS;
    g_RcpProfile.rdpDone = TRUE;
    rcpProfilePublish(now);
}

s32 rcpProfileGetSnapshot(RcpProfileSample *sample)
{
    OSIntMask mask;
    s32 valid;

    mask = osSetIntMask(OS_IM_NONE);
    valid = g_RcpProfile.valid;
    if (valid) {
        *sample = g_RcpProfile.latest;
    }
    osSetIntMask(mask);
    return valid;
}

u32 rcpProfileCpuTenthsMs(u32 ticks)
{
    /* Standard N64: CPU Count 46.875 MHz. Split to avoid 32-bit overflow. */
    return (ticks / 46875) * 10 + ((ticks % 46875) * 10 + 23437) / 46875;
}

u32 rcpProfileRdpTenthsMs(u32 clocks)
{
    /* RCP 62.5 MHz, rounded to the nearest 0.1 ms. */
    return ((clocks & RCP_COUNTER_MASK) + 3125) / 6250;
}
