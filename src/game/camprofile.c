#include "camprofile.h"

CamProfileFrame g_CamProfileFrame[CAM_PROFILE_COUNT];
u32 g_CamProfileScaleRebuilds;

static CamProfileSummary g_CamProfileSummary;
static u64 g_CamProfileTotalTicks[CAM_PROFILE_COUNT];
static u32 g_CamProfileTotalCalls[CAM_PROFILE_COUNT];
static u32 g_CamProfileTotalRebuilds;
static s32 g_CamProfileFrameStarted;
static s32 g_CamProfileFrameWasGameplay;

void camProfileReset(void)
{
    CamProfileSummary empty = {0};
    s32 i;

    g_CamProfileSummary = empty;
    g_CamProfileTotalRebuilds = 0;
    g_CamProfileScaleRebuilds = 0;
    g_CamProfileFrameStarted = 0;
    g_CamProfileFrameWasGameplay = 0;

    for (i = 0; i < CAM_PROFILE_COUNT; i++)
    {
        g_CamProfileFrame[i].ticks = 0;
        g_CamProfileFrame[i].calls = 0;
        g_CamProfileTotalTicks[i] = 0;
        g_CamProfileTotalCalls[i] = 0;
    }
}

/* Called once before lvTick, including the viewport/movement work that
 * happens between lvTick and lvRender. All players share this frame. */
void camProfileBeginFrame(s32 gameplayActive)
{
    s32 i;

    for (i = 0; i < CAM_PROFILE_COUNT; i++)
    {
        g_CamProfileFrame[i].ticks = 0;
        g_CamProfileFrame[i].calls = 0;
    }
    g_CamProfileScaleRebuilds = 0;
    g_CamProfileFrameStarted = 1;
    g_CamProfileFrameWasGameplay = gameplayActive;
}

static u32 camProfileAverageUsTenths(u64 ticks, u32 samples)
{
    u64 divisor;

    if (samples == 0 || osClockRate == 0)
    {
        return 0;
    }

    /* osClockRate is initialized to the CP0 Count frequency (46.875 MHz
     * on N64). Round once after summing, retaining sub-microsecond costs. */
    divisor = osClockRate * samples;
    return (u32)((ticks * 10000000ULL + divisor / 2) / divisor);
}

static void camProfileUpdateSummary(void)
{
    u64 totalTicks = 0;
    u32 frames = g_CamProfileSummary.frames;
    s32 i;

    for (i = 0; i < CAM_PROFILE_COUNT; i++)
    {
        g_CamProfileSummary.usPerFrameTenths[i] =
            camProfileAverageUsTenths(g_CamProfileTotalTicks[i], frames);
        g_CamProfileSummary.usPerCallTenths[i] =
            camProfileAverageUsTenths(g_CamProfileTotalTicks[i], g_CamProfileTotalCalls[i]);
        g_CamProfileSummary.callsPerFrameTenths[i] =
            (u32)(((u64)g_CamProfileTotalCalls[i] * 10 + frames / 2) / frames);
        totalTicks += g_CamProfileTotalTicks[i];
    }

    g_CamProfileSummary.totalUsPerFrameTenths = camProfileAverageUsTenths(totalTicks, frames);
    g_CamProfileSummary.scaleCalls = g_CamProfileTotalCalls[CAM_PROFILE_SCALE];
    g_CamProfileSummary.scaleRebuilds = g_CamProfileTotalRebuilds;
}

/* Finish independently of HUD drawing: a full display list must not drop
 * samples or prevent the capture from completing. */
void camProfileEndFrame(s32 gameplayActive)
{
    s32 i;

    if (!g_CamProfileFrameStarted)
    {
        return;
    }
    g_CamProfileFrameStarted = 0;

    if (g_CamProfileSummary.state == CAM_PROFILE_HOLD)
    {
        return;
    }

    /* Require uninterrupted normal gameplay at both frame boundaries.
     * Intros, pause transitions and death restart an unfinished capture. */
    if (!g_CamProfileFrameWasGameplay || !gameplayActive)
    {
        camProfileReset();
        return;
    }

    if (g_CamProfileSummary.warmupFrames < CAM_PROFILE_WARMUP_FRAMES)
    {
        g_CamProfileSummary.warmupFrames++;
        g_CamProfileSummary.state = CAM_PROFILE_WARMUP;
        return;
    }

    g_CamProfileSummary.state = CAM_PROFILE_CAPTURING;
    g_CamProfileSummary.frames++;

    for (i = 0; i < CAM_PROFILE_COUNT; i++)
    {
        g_CamProfileTotalTicks[i] += g_CamProfileFrame[i].ticks;
        g_CamProfileTotalCalls[i] += g_CamProfileFrame[i].calls;
    }
    g_CamProfileTotalRebuilds += g_CamProfileScaleRebuilds;

    /* Keep conversions out of camera code and amortize display updates.
     * Timing remains enabled after HOLD, keeping instrumentation consistent. */
    if (g_CamProfileSummary.frames == 1
            || g_CamProfileSummary.frames % 30 == 0
            || g_CamProfileSummary.frames == CAM_PROFILE_CAPTURE_FRAMES)
    {
        camProfileUpdateSummary();
    }

    if (g_CamProfileSummary.frames == CAM_PROFILE_CAPTURE_FRAMES)
    {
        g_CamProfileSummary.state = CAM_PROFILE_HOLD;
    }
}

const CamProfileSummary *camProfileGetSummary(void)
{
    return &g_CamProfileSummary;
}
