#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "camprofile.h"

/* Use the real profiler and N64 declarations with a controllable Count. */
u64 osClockRate = 46875000ULL;
static u32 count;

u32 osGetCount(void)
{
    return count;
}

static void record(CamProfileCategory category, u32 ticks)
{
    u32 start = osGetCount();
    count += ticks;
    CAM_PROFILE_END(category, start);
}

static void frame(s32 activeAtStart, s32 activeAtEnd, u32 ticks)
{
    camProfileBeginFrame(activeAtStart);
    record(CAM_PROFILE_SCALE, ticks);
    g_CamProfileScaleRebuilds++;
    camProfileEndFrame(activeAtEnd);
}

static void warmup(void)
{
    u32 i;
    for (i = 0; i < CAM_PROFILE_WARMUP_FRAMES; i++)
    {
        frame(1, 1, 100000000);
        assert(camProfileGetSummary()->state == CAM_PROFILE_WARMUP);
        assert(camProfileGetSummary()->warmupFrames == i + 1);
        assert(camProfileGetSummary()->frames == 0);
    }
}

static void checkWindowAndAverages(void)
{
    const CamProfileSummary *summary;
    CamProfileSummary held;
    u32 i;

    camProfileReset();
    summary = camProfileGetSummary();
    assert(summary->state == CAM_PROFILE_WAITING);
    warmup();

    for (i = 0; i < CAM_PROFILE_CAPTURE_FRAMES; i++)
    {
        camProfileBeginFrame(1);
        assert(g_CamProfileFrame[CAM_PROFILE_SCALE].calls == 0);
        assert(g_CamProfileFrame[CAM_PROFILE_SCALE].ticks == 0);
        assert(g_CamProfileScaleRebuilds == 0);

        /* Multiple players/call sites accumulate into the same frame.
         * The baseline rebuilds on every scale request. */
        record(CAM_PROFILE_SCALE, 46875);
        record(CAM_PROFILE_SCALE, 46875);
        g_CamProfileScaleRebuilds += 2;
        record(CAM_PROFILE_FRUSTUM, 93750);
        record(CAM_PROFILE_VISIBILITY, 46875);
        record(CAM_PROFILE_VISIBILITY, 46875);
        record(CAM_PROFILE_VISIBILITY, 46875);
        camProfileEndFrame(1);
        assert(summary->frames == i + 1);
        assert(summary->state == (i + 1 == CAM_PROFILE_CAPTURE_FRAMES
                ? CAM_PROFILE_HOLD : CAM_PROFILE_CAPTURING));
    }

    assert(summary->usPerFrameTenths[CAM_PROFILE_SCALE] == 20000);
    assert(summary->usPerCallTenths[CAM_PROFILE_SCALE] == 10000);
    assert(summary->callsPerFrameTenths[CAM_PROFILE_SCALE] == 20);
    assert(summary->usPerFrameTenths[CAM_PROFILE_FRUSTUM] == 20000);
    assert(summary->usPerCallTenths[CAM_PROFILE_FRUSTUM] == 20000);
    assert(summary->callsPerFrameTenths[CAM_PROFILE_FRUSTUM] == 10);
    assert(summary->usPerFrameTenths[CAM_PROFILE_VISIBILITY] == 30000);
    assert(summary->usPerCallTenths[CAM_PROFILE_VISIBILITY] == 10000);
    assert(summary->callsPerFrameTenths[CAM_PROFILE_VISIBILITY] == 30);
    assert(summary->totalUsPerFrameTenths == 70000);
    assert(summary->scaleCalls == 600);
    assert(summary->scaleRebuilds == 600);

    held = *summary;
    for (i = 0; i < 400; i++)
    {
        frame(i % 2, i % 3, 999999);
        assert(memcmp(&held, summary, sizeof(held)) == 0);
    }
    camProfileReset();
    assert(summary->state == CAM_PROFILE_WAITING);
    assert(summary->frames == 0);
    assert(summary->totalUsPerFrameTenths == 0);
    assert(summary->scaleCalls == 0);
}

static void checkInterruptedCapture(void)
{
    const CamProfileSummary *summary = camProfileGetSummary();
    s32 start;
    s32 end;

    for (start = 0; start <= 1; start++)
    {
        for (end = 0; end <= 1; end++)
        {
            if (start && end) continue;
            camProfileReset();
            warmup();
            frame(1, 1, 46875);
            assert(summary->frames == 1);
            frame(start, end, 46875);
            assert(summary->state == CAM_PROFILE_WAITING);
            assert(summary->warmupFrames == 0);
            assert(summary->frames == 0);
            assert(summary->totalUsPerFrameTenths == 0);
            frame(1, 1, 46875);
            assert(summary->warmupFrames == 1);
        }
    }

    camProfileReset();
    camProfileEndFrame(1);
    assert(summary->warmupFrames == 0);
    frame(1, 1, 0);
    camProfileEndFrame(1);
    assert(summary->warmupFrames == 1);
}

static void checkCounterWrapAndWideTotals(void)
{
    const CamProfileSummary *summary = camProfileGetSummary();
    u32 i;

    camProfileReset();
    camProfileBeginFrame(1);
    count = 0xfffffff0U;
    record(CAM_PROFILE_SCALE, 32);
    assert(count == 16);
    assert(g_CamProfileFrame[CAM_PROFILE_SCALE].ticks == 32);
    assert(g_CamProfileFrame[CAM_PROFILE_SCALE].calls == 1);

    camProfileReset();
    warmup();
    for (i = 0; i < CAM_PROFILE_CAPTURE_FRAMES; i++)
    {
        /* 12 billion ticks in the completed window exceeds a u32. */
        frame(1, 1, 40000000);
    }
    assert(summary->usPerFrameTenths[CAM_PROFILE_SCALE] == 8533333);
    assert(summary->usPerCallTenths[CAM_PROFILE_SCALE] == 8533333);
    assert(summary->usPerCallTenths[CAM_PROFILE_VISIBILITY] == 0);
    assert(summary->callsPerFrameTenths[CAM_PROFILE_VISIBILITY] == 0);
}

static void checkRoundingAndRebuildCounts(void)
{
    const CamProfileSummary *summary = camProfileGetSummary();
    u32 i;

    camProfileReset();
    warmup();
    for (i = 0; i < CAM_PROFILE_CAPTURE_FRAMES; i++)
    {
        camProfileBeginFrame(1);
        record(CAM_PROFILE_SCALE, 2);
        record(CAM_PROFILE_SCALE, 3);
        /* Future cached calls still count as requests and are timed. */
        g_CamProfileScaleRebuilds++;
        if (i % 2 == 0) record(CAM_PROFILE_VISIBILITY, 46875);
        camProfileEndFrame(1);
    }
    assert(summary->usPerFrameTenths[CAM_PROFILE_SCALE] == 1);
    assert(summary->usPerCallTenths[CAM_PROFILE_SCALE] == 1);
    assert(summary->scaleRebuilds == 300);
    assert(summary->scaleCalls == 600);
    assert(summary->callsPerFrameTenths[CAM_PROFILE_VISIBILITY] == 5);
    assert(summary->usPerFrameTenths[CAM_PROFILE_VISIBILITY] == 5000);
    assert(summary->usPerCallTenths[CAM_PROFILE_VISIBILITY] == 10000);

    camProfileReset();
    warmup();
    osClockRate = 0;
    frame(1, 1, 46875);
    assert(summary->totalUsPerFrameTenths == 0);
}

int main(void)
{
    checkWindowAndAverages();
    checkInterruptedCapture();
    checkCounterWrapAndWideTotals();
    checkRoundingAndRebuildCounts();
    puts("cam_profile: capture, interruption, hold, Count wrap and averages passed");
    return 0;
}
