#include <ultra64.h>
#include <PR/rcp.h>
#include "frameprofile.h"

/* GUD allows two outstanding graphics tasks. Keep one spare slot; identify
 * tasks by pointer so a queued frame never borrows another frame's CPU time.
 * No task, microcode or yield-buffer layouts are changed. */
#define FRAME_PROFILE_TASKS 3
#define FRAME_PROFILE_COUNTER_MASK 0x00ffffffu
/* A 24-bit RCP counter wraps after 268.4 ms (CPU Count runs at 3/4 RCP). */
#define FRAME_PROFILE_COUNTER_RANGE 0x00c00000u

typedef struct FrameProfileTask {
    void *task;
    u32 cpuTicks;
    u32 start;
    u32 sliceStart;
    u32 rspTicks;
    u32 rspSpan;
    u32 rdpSpan;
    u32 clock;
    u32 commands;
    u32 pipe;
    u32 tmem;
    s32 started;
    s32 running;
    s32 cpuDone;
    s32 rspDone;
    s32 rdpDone;
} FrameProfileTask;

static FrameProfileTask g_FrameProfileTasks[FRAME_PROFILE_TASKS];
static FrameProfileTask *g_FrameProfileBuilding;
static u32 g_FrameProfileCpuStart;
static s32 g_FrameProfileCpuActive;
static volatile FrameBottleneck g_FrameProfileResult;

static FrameProfileTask *frameProfileFind(void *task)
{
    s32 i;
    if (task == NULL) { return NULL; }
    for (i = 0; i < FRAME_PROFILE_TASKS; i++) {
        if (g_FrameProfileTasks[i].task == task) { return &g_FrameProfileTasks[i]; }
    }
    return NULL;
}

static FrameBottleneck frameProfileClassify(const FrameProfileTask *frame)
{
    u32 rcp = frame->rspSpan > frame->rdpSpan ? frame->rspSpan : frame->rdpSpan;
    u32 lower;
    u32 upper;
    u32 maximumClock;

    if (rcp == 0 || frame->cpuTicks == 0 || frame->rspTicks == 0) {
        return FRAME_LIMIT_UNKNOWN;
    }
    /* CPU work excludes VI pacing and waiting for a free graphics task. Its
     * wall interval still includes OS preemption and shared-memory stalls.
     * Require a 10% lead before calling either side dominant. */
    if (frame->cpuTicks > rcp && frame->cpuTicks - rcp > rcp / 10) {
        return FRAME_LIMIT_CPU;
    }
    if (rcp >= FRAME_PROFILE_COUNTER_RANGE) { return FRAME_LIMIT_UNKNOWN; }
    if (frame->cpuTicks >= rcp || rcp - frame->cpuTicks <= rcp / 10) {
        return FRAME_LIMIT_MIXED;
    }

    /* Reject absent/emulated counters and possible overflow. Allow 1 ms for
     * counter-reset/task-launch skew; timestamps are scheduler boundaries. */
    maximumClock = (frame->rdpSpan * 4u + 2u) / 3u + 62500u;
    if (frame->clock == 0 || frame->clock > maximumClock
        || frame->commands > frame->clock || frame->pipe > frame->clock
        || frame->tmem > frame->clock) { return FRAME_LIMIT_UNKNOWN; }

    lower = frame->pipe > frame->tmem ? frame->pipe : frame->tmem;
    if (frame->commands > lower) { lower = frame->commands; }
    if (lower == 0) { return FRAME_LIMIT_UNKNOWN; }
    /* Counters overlap. MAX is a lower bound on DP busy time; their sum is
     * only an upper bound, never an additive measurement of rendering time.
     * Convert RCP clocks to CPU Count ticks before making comparisons. */
    upper = (frame->pipe + frame->tmem + frame->commands) * 3u / 4u;
    lower = lower * 3u / 4u;
    if (lower >= rcp - rcp / 4) { return FRAME_LIMIT_RDP; }
    /* A long SP span with a mostly idle DP points to the RSP path. The span
     * includes audio yield gaps: audio shares the same RSP as graphics. */
    if (upper <= rcp / 2 && frame->rspSpan >= rcp - rcp / 4) {
        return FRAME_LIMIT_RSP;
    }
    return FRAME_LIMIT_MIXED;
}

static void frameProfilePublish(FrameProfileTask *frame)
{
    if (frame->cpuDone && frame->rspDone && frame->rdpDone) {
        g_FrameProfileResult = frameProfileClassify(frame);
        frame->task = NULL;
    }
}

void frameProfileReset(void)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    bzero(g_FrameProfileTasks, sizeof(g_FrameProfileTasks));
    g_FrameProfileBuilding = NULL;
    g_FrameProfileCpuActive = FALSE;
    g_FrameProfileResult = FRAME_LIMIT_WAITING;
    osSetIntMask(mask);
}

void frameProfileCpuBegin(void)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_FrameProfileCpuStart = osGetCount();
    g_FrameProfileCpuActive = TRUE;
    g_FrameProfileBuilding = NULL;
    osSetIntMask(mask);
}

void frameProfileTaskSubmitted(void *task)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    s32 i;
    /* Startup/legal-screen tasks have no CPU Begin/End pair. Ignore them. */
    if (task != NULL && g_FrameProfileCpuActive && g_FrameProfileBuilding == NULL
        && frameProfileFind(task) == NULL) {
        for (i = 0; i < FRAME_PROFILE_TASKS; i++) {
            FrameProfileTask *frame = &g_FrameProfileTasks[i];
            if (frame->task == NULL) {
                bzero(frame, sizeof(*frame));
                frame->task = task;
                g_FrameProfileBuilding = frame;
                break;
            }
        }
    }
    osSetIntMask(mask);
}

void frameProfileCpuEnd(void)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    FrameProfileTask *frame = g_FrameProfileBuilding;
    if (g_FrameProfileCpuActive) {
        if (frame != NULL) {
            frame->cpuTicks = osGetCount() - g_FrameProfileCpuStart;
            frame->cpuDone = TRUE;
            frameProfilePublish(frame);
        } else {
            g_FrameProfileResult = FRAME_LIMIT_UNKNOWN;
        }
    }
    g_FrameProfileBuilding = NULL;
    g_FrameProfileCpuActive = FALSE;
    osSetIntMask(mask);
}

void frameProfileRspStart(void *task)
{
    FrameProfileTask *frame = frameProfileFind(task);
    if (frame != NULL && !frame->running && !frame->rspDone) {
        u32 now = osGetCount();
        if (!frame->started) {
            frame->start = now;
            frame->started = TRUE;
        }
        frame->sliceStart = now;
        frame->running = TRUE;
    }
}

void frameProfileRspStop(void *task, s32 yielded)
{
    FrameProfileTask *frame = frameProfileFind(task);
    if (frame != NULL && frame->running) {
        u32 now = osGetCount();
        frame->rspTicks += now - frame->sliceStart;
        frame->running = FALSE;
        if (!yielded) {
            frame->rspSpan = now - frame->start;
            frame->rspDone = TRUE;
            frameProfilePublish(frame);
        }
    }
}

void frameProfileRdpDone(void *task)
{
    FrameProfileTask *frame = frameProfileFind(task);
    if (frame != NULL && frame->started && !frame->rdpDone) {
        frame->rdpSpan = osGetCount() - frame->start;
        /* Read before __scExec can reset the counters for the next task. */
        frame->clock = IO_READ(DPC_CLOCK_REG) & FRAME_PROFILE_COUNTER_MASK;
        frame->commands = IO_READ(DPC_BUFBUSY_REG) & FRAME_PROFILE_COUNTER_MASK;
        frame->pipe = IO_READ(DPC_PIPEBUSY_REG) & FRAME_PROFILE_COUNTER_MASK;
        frame->tmem = IO_READ(DPC_TMEM_REG) & FRAME_PROFILE_COUNTER_MASK;
        frame->rdpDone = TRUE;
        frameProfilePublish(frame);
    }
}

FrameBottleneck frameProfileGetBottleneck(void)
{
    /* Aligned word publication is atomic. Formatting stays on the game thread. */
    return g_FrameProfileResult;
}
