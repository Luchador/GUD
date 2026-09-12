#ifndef _RCPPROFILE_H_
#define _RCPPROFILE_H_

#include <ultra64.h>

/* One completed graphics task. CPU timestamps are osGetCount ticks; the
 * four DPC counters are RCP clocks. These quantities must not be added. */
typedef struct RcpProfileSample_s {
    u32 sequence;
    u32 rspTicks;
    u32 audioTicks;
    u32 yieldTicks;
    u32 yieldCount;
    u32 yieldLatencyTicks;
    u32 rdpEndTicks;
    u32 totalTicks;
    u32 rdpClock;
    u32 rdpCommand;
    u32 rdpPipe;
    u32 rdpTmem;
    u32 counterRangeExceeded;
    u32 averageTicks;
    u32 maximumTicks;
    u32 historyCount;
} RcpProfileSample;

/* Scheduler-thread writers. GUD submits one combined SP/DP graphics task at
 * a time. Keep this module in step with any future scheduler/task changes. */
void rcpProfileReset(void);
void rcpProfileRspStart(const void *task, s32 type, s32 resumed, u32 now);
void rcpProfileYieldRequested(const void *task, u32 now);
void rcpProfileRspDone(const void *task, u32 now, s32 yielded);
void rcpProfileRdpDone(const void *task, u32 now, u32 clock, u32 command,
        u32 pipe, u32 tmem);

/* Game-thread reader: copies a completed sample under a short interrupt mask. */
s32 rcpProfileGetSnapshot(RcpProfileSample *sample);
u32 rcpProfileCpuTenthsMs(u32 ticks);
u32 rcpProfileRdpTenthsMs(u32 clocks);

#endif
