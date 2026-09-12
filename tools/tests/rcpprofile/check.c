#include <assert.h>
#include <stdio.h>
#include "rcpprofile.h"

static int gfx, audio, unrelated;
static OSIntMask currentMask = 0x1234;

OSIntMask osSetIntMask(OSIntMask mask)
{
    OSIntMask old = currentMask;
    currentMask = mask;
    return old;
}

static RcpProfileSample snapshot(void)
{
    RcpProfileSample sample;
    assert(rcpProfileGetSnapshot(&sample));
    assert(currentMask == 0x1234);
    return sample;
}

static void finish(u32 start, u32 duration)
{
    rcpProfileRspStart(&gfx, M_GFXTASK, 0, start);
    rcpProfileRspDone(&gfx, start + duration, 0);
    rcpProfileRdpDone(&gfx, start + duration, 100, 50, 60, 20);
}

static void test_completion_orders(void)
{
    RcpProfileSample s;
    rcpProfileReset();
    assert(!rcpProfileGetSnapshot(&s));
    assert(currentMask == 0x1234);
    rcpProfileRspStart(&gfx, M_GFXTASK, 0, 100);
    rcpProfileRspDone(&gfx, 200, 0);
    assert(!rcpProfileGetSnapshot(&s));
    rcpProfileRdpDone(&unrelated, 250, 1, 2, 3, 4);
    assert(!rcpProfileGetSnapshot(&s));
    rcpProfileRdpDone(&gfx, 300, 101, 102, 103, 104);
    s = snapshot();
    assert(s.sequence == 1 && s.rspTicks == 100 && s.totalTicks == 200);
    assert(s.rdpEndTicks == 200 && s.rdpClock == 101 && s.rdpCommand == 102);
    assert(s.rdpPipe == 103 && s.rdpTmem == 104 && !s.audioTicks);

    /* Reuse the same OSScTask address; retain old sample until both finish. */
    rcpProfileRspStart(&gfx, M_GFXTASK, 0, 400);
    rcpProfileRdpDone(&gfx, 500, 201, 202, 203, 204);
    s = snapshot();
    assert(s.sequence == 1 && s.rdpClock == 101);
    rcpProfileRspDone(&gfx, 550, 0);
    s = snapshot();
    assert(s.sequence == 2 && s.rspTicks == 150 && s.totalTicks == 150);
    assert(s.rdpEndTicks == 100 && s.rdpClock == 201);
    assert(s.averageTicks == 175 && s.maximumTicks == 200 && s.historyCount == 2);
}

static void test_audio_yields(void)
{
    RcpProfileSample s;
    rcpProfileReset();
    /* Audio outside the graphics interval is excluded. */
    rcpProfileRspStart(&audio, M_AUDTASK, 0, 0);
    rcpProfileRspDone(&audio, 90, 0);
    rcpProfileRspStart(&gfx, M_GFXTASK, 0, 100);
    rcpProfileYieldRequested(&gfx, 130);
    rcpProfileYieldRequested(&gfx, 140); /* Preserve the first request. */
    rcpProfileRspDone(&gfx, 150, 1);
    rcpProfileRspStart(&audio, M_AUDTASK, 0, 160);
    rcpProfileRspDone(&audio, 190, 0);
    rcpProfileRspStart(&gfx, M_GFXTASK, 1, 200);
    rcpProfileYieldRequested(&gfx, 220);
    rcpProfileRspDone(&gfx, 250, 1);
    rcpProfileRspStart(&audio, M_AUDTASK, 0, 260);
    rcpProfileRspDone(&audio, 280, 0);
    rcpProfileRspStart(&gfx, M_GFXTASK, 1, 300);
    rcpProfileYieldRequested(&gfx, 310);
    rcpProfileRspDone(&gfx, 320, 0); /* Completed instead of yielding. */
    rcpProfileRspStart(&audio, M_AUDTASK, 0, 325);
    rcpProfileRdpDone(&gfx, 350, 10, 20, 30, 40);
    s = snapshot();
    assert(s.rspTicks == 120 && s.yieldTicks == 100 && s.yieldCount == 2);
    assert(s.yieldLatencyTicks == 30 && s.audioTicks == 75);
    assert(s.totalTicks == 250 && s.rdpClock == 10);
    rcpProfileRspDone(&audio, 400, 0); /* Must not modify published result. */
    assert(snapshot().audioTicks == 75);
    finish(500, 100);
    s = snapshot();
    assert(s.sequence == 2 && !s.yieldCount && !s.yieldTicks && !s.audioTicks);
}

static void test_wrap_counters_history(void)
{
    RcpProfileSample s;
    u32 i;
    rcpProfileReset();
    finish(0xfffffff0U, 100);
    s = snapshot();
    assert(s.rspTicks == 100 && s.totalTicks == 100);
    rcpProfileRspStart(&gfx, M_GFXTASK, 0, 1000);
    rcpProfileRspDone(&gfx, 1000 + 0xc00000, 0);
    rcpProfileRdpDone(&gfx, 1000 + 0xc00000, 0xff123456U,
            0xff234567U, 0xff345678U, 0xff456789U);
    s = snapshot();
    assert(s.counterRangeExceeded && s.rdpClock == 0x123456);
    assert(s.rdpCommand == 0x234567 && s.rdpPipe == 0x345678 && s.rdpTmem == 0x456789);
    for (i = 0; i < 64; i++) {
        finish(i * 1000, 10 + i);
    }
    s = snapshot();
    assert(s.historyCount == 64 && s.maximumTicks == 73 && s.averageTicks == 41);
    assert(!s.counterRangeExceeded);
    rcpProfileReset();
    assert(!rcpProfileGetSnapshot(&s));
}

static void test_conversions(void)
{
    u32 values[] = {0, 2343, 2344, 46875, 781250, 46875000, 0xffffffffU};
    u32 i;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        assert(rcpProfileCpuTenthsMs(values[i]) ==
                ((u64)values[i] * 10000 + 23437500) / 46875000);
    }
    assert(rcpProfileRdpTenthsMs(3124) == 0);
    assert(rcpProfileRdpTenthsMs(3125) == 1);
    assert(rcpProfileRdpTenthsMs(62500) == 10);
    assert(rcpProfileRdpTenthsMs(0xffffff) == 2684);
}

int main(void)
{
    test_completion_orders();
    test_audio_yields();
    test_wrap_counters_history();
    test_conversions();
    puts("RCP profiler accounting tests passed");
    return 0;
}
