#ifndef GUD_CAMPROFILE_H
#define GUD_CAMPROFILE_H

#include <ultra64.h>

/* Temporary camera profiler. Keep the timing boundaries when optimizing. */
#define CAM_PROFILE_WARMUP_FRAMES 60
#define CAM_PROFILE_CAPTURE_FRAMES 300
#define CAM_PROFILE_BUILD_LABEL "OPTIMIZED"

typedef enum CamProfileCategory
{
    CAM_PROFILE_SCALE,
    CAM_PROFILE_FRUSTUM,
    CAM_PROFILE_VISIBILITY,
    CAM_PROFILE_COUNT
} CamProfileCategory;

typedef enum CamProfileState
{
    CAM_PROFILE_WAITING,
    CAM_PROFILE_WARMUP,
    CAM_PROFILE_CAPTURING,
    CAM_PROFILE_HOLD
} CamProfileState;

typedef struct CamProfileFrame
{
    u32 ticks;
    u32 calls;
} CamProfileFrame;

typedef struct CamProfileSummary
{
    CamProfileState state;
    u32 warmupFrames;
    u32 frames;
    u32 usPerFrameTenths[CAM_PROFILE_COUNT];
    u32 usPerCallTenths[CAM_PROFILE_COUNT];
    u32 callsPerFrameTenths[CAM_PROFILE_COUNT];
    u32 totalUsPerFrameTenths;
    u32 scaleCalls;
    u32 scaleRebuilds;
} CamProfileSummary;

extern CamProfileFrame g_CamProfileFrame[CAM_PROFILE_COUNT];
extern u32 g_CamProfileScaleRebuilds;

/* Start with osGetCount() before any early-out; end on every return path.
 * Unsigned subtraction handles a Count wrap. Accumulation is outside the
 * timed interval, and the three intervals do not contain one another. */
#define CAM_PROFILE_END(category, start) do { \
    u32 camProfileElapsed = (u32)(osGetCount() - (start)); \
    g_CamProfileFrame[(category)].ticks += camProfileElapsed; \
    g_CamProfileFrame[(category)].calls++; \
} while (0)

void camProfileReset(void);
void camProfileBeginFrame(s32 gameplayActive);
void camProfileEndFrame(s32 gameplayActive);
const CamProfileSummary *camProfileGetSummary(void);

#endif
