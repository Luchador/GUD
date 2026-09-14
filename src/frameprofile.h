#ifndef FRAMEPROFILE_H
#define FRAMEPROFILE_H

#include <ultra64.h>

/* Estimated limiting stage of the latest fully completed gameplay frame.
 * MIXED avoids assigning RSP waits on the RDP to geometry processing. */
typedef enum FrameBottleneck {
    FRAME_LIMIT_WAITING,
    FRAME_LIMIT_UNKNOWN,
    FRAME_LIMIT_MIXED,
    FRAME_LIMIT_CPU,
    FRAME_LIMIT_RSP,
    FRAME_LIMIT_RDP
} FrameBottleneck;

/* Main-thread hooks. Begin after frame pacing; end after submission/defrag. */
void frameProfileReset(void);
void frameProfileCpuBegin(void);
void frameProfileTaskSubmitted(void *task);
void frameProfileCpuEnd(void);
FrameBottleneck frameProfileGetBottleneck(void);

/* Scheduler hooks: all code and storage must remain resident. */
void frameProfileRspStart(void *task);
void frameProfileRspStop(void *task, s32 yielded);
void frameProfileRdpDone(void *task);

#endif
