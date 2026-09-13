#ifndef _RENDERPROFILE_H_
#define _RENDERPROFILE_H_
#include <ultra64.h>
#include <bondtypes.h>
#include <sched.h>

enum RenderProfileMetric {
    RENDER_PROFILE_GFX,
    RENDER_PROFILE_CMD,
    RENDER_PROFILE_PIPE,
    RENDER_PROFILE_TMEM,
    RENDER_PROFILE_AA_CPU,
    RENDER_PROFILE_METRICS
};

typedef struct RenderProfileTask {
    u32 epoch;
    u32 startCount;
    u32 aaCpuCount;
    bool eligible;
} RenderProfileTask;

typedef struct RenderProfileStats {
    u32 average[RENDER_PROFILE_METRICS];
    u32 maximum[RENDER_PROFILE_METRICS];
    u32 samples;
    u32 warming;
    u32 collected;
    u32 rejected;
    bool aaError;
} RenderProfileStats;

bool renderProfileEnabled(void);
void renderProfileSetEnabled(bool enabled);
bool renderProfileGameplayActive(void);
void renderProfilePrepareTask(OSScTask *task, u32 aaCpuCount, bool aaOk);
void renderProfileTaskStart(OSScTask *task);
void renderProfileTaskDone(OSScTask *task);
void renderProfileRead(RenderProfileStats *stats);
u32 renderProfileToUsec(u32 ticks, s32 metric);

#endif
