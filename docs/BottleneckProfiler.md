# Compact frame bottleneck profiler

> Historical reference: this profiler has been removed after bottleneck
> investigation. The FPS counter and background X-ray remain. The label,
> CPU/task accounting, scheduler hooks, DP-counter reads, module, linker
> entries/assertions and profiler tests are removed. The ordinary scheduler's
> pre-existing DP counter reset is retained. The implementation and validation
> instructions below describe the earlier profiling build, not the current one.
> FPS-only checks now run with `python3 tools/tests/frame_rate/run.py`.

The existing FPS counter is retained. The old BGTICK, LVTICK, LVRENDER,
BGRENDER, OBJTICK, CHRTICK and CHRACT rows and their timers are removed.
One line below FPS estimates the limiting stage:

| Display | Interpretation |
|---|---|
| `LIMIT: CPU` | Preparing/submitting the frame took clearly longer than its graphics task. |
| `LIMIT: RSP` | The RSP path dominated while the RDP was mostly idle. This can include audio work on the shared RSP. |
| `LIMIT: RDP` | RDP processing or command/texture traffic occupied most of the graphics task's lifetime. RSP time may include waiting on that work. |
| `LIMIT: MIXED` | CPU and graphics times are close, or the counters do not clearly separate RSP and RDP pressure. |
| `LIMIT: --` | Waiting for the first completed frame after a stage change. |
| `LIMIT: ?` | Timings/counters are unavailable, implausible, or outside the safe counter range. |

This is a per-frame **estimate**, not a measurement of the RSP's exact stall
instructions. CPU, RSP and RDP work overlap and share memory bandwidth. The
label identifies the apparent limiting work stage; VI pacing and frame caps
can still limit FPS even when all three have spare capacity.

The overlay uses the latest **completed** frame. The frame currently being
built has not finished on the hardware yet, so its result cannot appear in
its own display list. There is no rolling average or deliberate update delay.
The label may change rapidly in variable scenes. Split-screen gets one result
for the whole frame, and stage changes clear the previous result.

## Measurement

- CPU timing starts after frame pacing and task-availability checks. It covers
  input consumption, ticks, all players' rendering preparation, the overlay,
  submission and the post-submit defragmentation pass. This is main-thread wall
  time, including preemption, synchronous loads and shared-memory stalls; it
  is not an instruction-retirement counter.
- Each CPU sample is attached to its `OSScTask` before submission. Three fixed
  slots cover GUD's maximum of two outstanding graphics tasks. Publication
  waits for CPU end, final SP completion and DP completion in any order.
- Graphics time runs from first SP launch until both SP and DP finish. Queue
  waits and VI presentation are excluded. SP slices track actual scheduling;
  the total SP span also includes audio yield gaps. Audio never resets the
  graphics task's RDP counters, and a resumed graphics task retains them.
- At DP completion, the scheduler reads CLK/CMD/PIPE/TMEM before another task
  can reset them. The counters are 24-bit RCP clocks at 62.5 MHz; CPU Count is
  46.875 MHz. A graphics interval of 268.4 ms or longer makes DP classification
  inconclusive because the counters may have wrapped. A clear CPU lead can
  still be reported without relying on DP counters.
- The rules require a CPU lead greater than 10% of graphics duration for CPU,
  or CPU time below 90% of graphics duration before distinguishing RSP/RDP.
  Otherwise the result is MIXED. Missing or inconsistent DP counters produce
  `?` rather than a spurious RSP result.
- Counter intervals overlap. Their maximum provides a conservative lower
  bound on DP busy time; their sum is used only as an upper bound. At least
  75% DP occupancy on one counter suggests RDP. An upper bound at most 50%,
  together with an SP span at least 75% of graphics duration, suggests RSP.
  Other cases are MIXED. These thresholds are diagnostic heuristics, not
  calibrated performance guarantees.

All profiler code and state use resident linker sections. Scheduler callbacks
have linker assertions preventing accidental placement in demand-paged game
code. The implementation makes no changes to microcode, task layouts, queue
policy, rendering options or frame pacing. The guarded text renderer reserves
space for the final full-sync/end commands and can omit a row if space is low.

Counter meanings and clock constants follow the bundled SDK headers and
[Nintendo's DP interface documentation](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/n64man/os/osDpSetNextBuffer.htm).

## Validation and hardware check

Run `python3 tools/tests/frameprofile/run.py` for host tests of the production
accounting module, scheduler entry points and HUD. Coverage includes queued
frames, both completion orders, CPU finishing last, audio yields/resumes,
completion instead of yielding, counter reset ordering, task reuse, timer
wrap, missing/overflowing counters, close cases, interrupt-mask restoration,
full-screen layout and display-list space guards.

The affected eight C files compile with the repository's IDO 5.3 toolchain as
MIPS-II N64 objects. A complete ROM link and console execution have not been
performed in this environment because extracted ROM assets are unavailable.

Build the usual US ROM and check the legal screen/audio first, then inspect
spawn views in Train, Frigate, Depot and Jungle. Compare the estimate with
controlled changes such as the existing AA setting while holding the camera
still. Lower RDP demand may move an RDP result toward MIXED or RSP. Also check
split-screen and level transitions. Emulator DP-counter support varies;
`LIMIT: ?` is preferable to treating zero counters as free rendering.
