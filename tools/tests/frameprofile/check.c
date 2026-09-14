/* Included after the actual accounting module and scheduler entry points. */
static OSScTask taskA, taskB, audioTask;

static void CounterTicks(u32 wall, u32 pipe, u32 tmem, u32 commands)
{
    counters[DPC_CLOCK_REG] = wall * 4u / 3u;
    counters[DPC_PIPEBUSY_REG] = pipe * 4u / 3u;
    counters[DPC_TMEM_REG] = tmem * 4u / 3u;
    counters[DPC_BUFBUSY_REG] = commands * 4u / 3u;
}

static void Submit(OSScTask *task, u32 begin, u32 end)
{
    u32 mask = testMask;
    testNow = begin; frameProfileCpuBegin();
    testNow = end; frameProfileTaskSubmitted(task); frameProfileCpuEnd();
    assert(testMask == mask);
}

static FrameBottleneck Sample(u32 cpu, u32 rsp, u32 rdp, u32 pipe, u32 tmem, u32 commands)
{
    frameProfileReset();
    Submit(&taskA, 100, 100 + cpu);
    u32 start = 200 + cpu;
    testNow = start; frameProfileRspStart(&taskA);
    if (rsp <= rdp) {
        testNow = start + rsp; frameProfileRspStop(&taskA, FALSE);
        assert(frameProfileGetBottleneck() == FRAME_LIMIT_WAITING);
        testNow = start + rdp; CounterTicks(rdp, pipe, tmem, commands); frameProfileRdpDone(&taskA);
    } else {
        testNow = start + rdp; CounterTicks(rdp, pipe, tmem, commands); frameProfileRdpDone(&taskA);
        assert(frameProfileGetBottleneck() == FRAME_LIMIT_WAITING);
        testNow = start + rsp; frameProfileRspStop(&taskA, FALSE);
    }
    assert(testMask == 0x123401u);
    return frameProfileGetBottleneck();
}

static void Classification(void)
{
    assert(Sample(90000, 57000, 60000, 57000, 3000, 3000) == FRAME_LIMIT_CPU);
    assert(Sample(6000, 57000, 60000, 57000, 3000, 3000) == FRAME_LIMIT_RDP);
    assert(Sample(6000, 60000, 61000, 6000, 3000, 3000) == FRAME_LIMIT_RSP);
    assert(Sample(6000, 57000, 60000, 3000, 57000, 3000) == FRAME_LIMIT_RDP);
    assert(Sample(6000, 57000, 60000, 3000, 3000, 57000) == FRAME_LIMIT_RDP);
    assert(Sample(56000, 57000, 60000, 57000, 3000, 3000) == FRAME_LIMIT_MIXED);
    assert(Sample(66000, 57000, 60000, 57000, 3000, 3000) == FRAME_LIMIT_MIXED);
    /* Overlapping counters cannot be added and presented as actual DP time.
       Non-overlapping loads could still dominate, so do not assert RSP here. */
    assert(Sample(6000, 57000, 60000, 24000, 24000, 0) == FRAME_LIMIT_MIXED);
    /* 39k CPU ticks is 52k RCP clocks: compare after converting units. */
    assert(Sample(6000, 57000, 60000, 39000, 0, 0) == FRAME_LIMIT_MIXED);
    assert(Sample(6000, 60000, 59000, 6000, 3000, 3000) == FRAME_LIMIT_RSP);
    assert(Sample(6000, 30000, 60000, 6000, 3000, 3000) == FRAME_LIMIT_MIXED);
    assert(Sample(6000, 60000, 61000, 0, 0, 0) == FRAME_LIMIT_UNKNOWN);
    assert(Sample(6000, 60000, 61000, 90000, 0, 0) == FRAME_LIMIT_UNKNOWN);
    assert(Sample(6000, 0xc00000u, 0xc00000u, 6000, 0, 0) == FRAME_LIMIT_UNKNOWN);
    assert(Sample(0xf00000u, 0xc00000u, 0xc00000u, 6000, 0, 0) == FRAME_LIMIT_CPU);
    puts("PASS: CPU/RSP/RDP estimates, close cases, counter overlap, missing counters and overflow.");
}

static void PairingAndWrap(void)
{
    frameProfileReset();
    Submit(&taskA, 100, 3100);
    testNow = 4000; frameProfileRspStart(&taskA);
    testNow = 5000; frameProfileCpuBegin(); /* Building B while A renders. */
    testNow = 64000; frameProfileRspStop(&taskA, FALSE);
    testNow = 65000; CounterTicks(61000, 58000, 0, 0); frameProfileRdpDone(&taskA);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_RDP);
    testNow = 95000; frameProfileTaskSubmitted(&taskB); frameProfileCpuEnd();
    testNow = 96000; frameProfileRspStart(&taskB);
    testNow = 126000; CounterTicks(30000, 27000, 0, 0); frameProfileRdpDone(&taskB);
    testNow = 126100; frameProfileRspStop(&taskB, FALSE);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_CPU);

    frameProfileReset();
    testNow = 0xfffff000u; frameProfileCpuBegin();
    testNow = 0xfffff100u; frameProfileTaskSubmitted(&taskA);
    testNow = 0xfffff200u; frameProfileRspStart(&taskA);
    testNow += 6000; frameProfileRspStop(&taskA, FALSE);
    testNow += 300; CounterTicks(6300, 6000, 0, 0); frameProfileRdpDone(&taskA);
    /* Hardware can finish before post-submit CPU work. Publish only when all
       three measurements for this particular task are available. */
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_WAITING);
    testNow += 4000; frameProfileCpuEnd();
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_CPU);
    Submit(&taskA, testNow + 100, testNow + 400); /* Reuse the same task structure. */
    testNow += 100; frameProfileRspStart(&taskA);
    testNow += 9000; frameProfileRspStop(&taskA, FALSE);
    testNow += 300; CounterTicks(9300, 9000, 0, 0); frameProfileRdpDone(&taskA);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_RDP);
    frameProfileReset();
    frameProfileTaskSubmitted(&taskA); frameProfileRspStart(&taskA);
    frameProfileRspStop(&taskA, FALSE); frameProfileRdpDone(&taskA); frameProfileCpuEnd();
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_WAITING); /* Boot/stale events. */
    assert(testMask == 0x123401u);
    puts("PASS: queued frame pairing, both completion orders, late CPU completion, pointer reuse and timer wrap.");
}

static void InitTask(OSScTask *task, int type)
{
    memset(task, 0, sizeof(*task));
    task->list.t.type = type;
    task->flags = type == M_GFXTASK ? OS_SC_NEEDS_RSP | OS_SC_NEEDS_RDP : OS_SC_NEEDS_RSP;
    task->state = task->flags;
}

static void Scheduler(void)
{
    OSSched sc = {0};
    frameProfileReset(); resets = 0; reads = 0;
    InitTask(&taskA, M_GFXTASK); InitTask(&audioTask, M_AUDTASK);
    Submit(&taskA, 100, 3100);
    testNow = 4000; __scExec(&sc, &taskA, &taskA);
    assert(resets == 1 && sc.curRSPTask == &taskA && sc.curRDPTask == &taskA);
    CounterTicks(30000, 1000, 0, 0);
    testNow = 34000; __scYield(&sc); yielded = TRUE; __scHandleRSP(&sc);
    assert(taskA.state & OS_SC_YIELDED);
    assert(sc.gfxListHead == &taskA && reads == 0);
    __scExec(&sc, &audioTask, NULL);
    testNow = 64000; yielded = FALSE; __scHandleRSP(&sc);
    sc.gfxListHead = sc.gfxListTail = NULL;
    __scExec(&sc, &taskA, &taskA);
    assert(resets == 1 && counters[DPC_CLOCK_REG] == 40000); /* No reset on audio/resume. */
    testNow = 94000; __scHandleRSP(&sc);
    FrameProfileTask *frame = frameProfileFind(&taskA);
    assert(frame && frame->rspTicks == 60000 && frame->rspSpan == 90000);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_WAITING);
    testNow = 94300; CounterTicks(90300, 9000, 3000, 0); __scHandleRDP(&sc);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_RSP && reads == 4);
    assert(releasedResult == FRAME_LIMIT_RSP);

    frameProfileReset();
    InitTask(&taskA, M_GFXTASK); InitTask(&taskB, M_GFXTASK);
    Submit(&taskA, 100000, 103000);
    testNow = 104000; __scExec(&sc, &taskA, &taskA);
    Submit(&taskB, 105000, 108000);
    testNow = 164000; __scYield(&sc); yielded = FALSE; __scHandleRSP(&sc);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_WAITING); /* Finished instead of yielding. */
    nextTask = &taskB;
    testNow = 165000; CounterTicks(61000, 58000, 0, 0); __scHandleRDP(&sc);
    /* __scExec just reset B's counters. A must already be published. */
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_RDP && counters[DPC_PIPEBUSY_REG] == 0);
    assert(sc.curRSPTask == &taskB && sc.curRDPTask == &taskB);
    testNow = 190000; CounterTicks(25000, 1000, 0, 0); __scHandleRDP(&sc);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_RDP); /* B still awaits SP. */
    testNow = 190100; __scHandleRSP(&sc);
    assert(frameProfileGetBottleneck() == FRAME_LIMIT_RSP && releasedResult == FRAME_LIMIT_RSP);
    puts("PASS: production scheduler hooks, audio yields, resume counters and publication before task release/reset.");
}

int main(void)
{
    Classification(); PairingAndWrap(); Scheduler();
    return 0;
}
