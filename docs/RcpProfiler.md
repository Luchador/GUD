# RSP/RDP profiler

> Historical reference: the RCP profiler, its scheduler/HUD hooks, linker
> entries and tests, and the GFX/BG command counters have been removed after
> hardware profiling. FPS and the existing CPU timing rows remain. The
> implementation and test instructions below describe the profiling build
> at commit `03bec2a4`, not the current build. Keep these measurement definitions
> when interpreting earlier captures or restoring instrumentation from Git.
> The [scissor experiment results](RcpScissorExperiment.md#hardware-results)
> record the Jungle comparison that followed this profiling pass.

`GUD-rcp-profiler-restored.patch` targets master `aa1f9b09` (Remove diagnostics).
It reconnects the retained accounting module to the scheduler, resident linker
sections and on-screen display. The framebuffer/BG alignment fixes and the
interrupt-mask correction remain. No crash-investigation code is restored.
Build with `make VERSION=US`; the profiler is independent of DEBUG.

The existing FPS/CPU panel now includes an RCP block. It is enabled wherever
`lvDrawFrameRateDisplay` is called, once per full rendered gameplay frame,
including split-screen. No USB cartridge, logging connection, or replacement
microcode is required. Build and run GUD normally.

All RCP times are **milliseconds**, rounded to 0.1 ms. The CPU rows above it
retain their existing `K` CPU Count units. `#` identifies the latest **completed
graphics task**; every RCP value below that number belongs to that task. The
CPU rows describe the frame currently being built and may be a different frame.
At startup the RCP panel waits for its first completed task.

| Label | Meaning |
|---|---|
| `RSP` | Graphics RSP scheduled time, summed across all execution slices. Excludes yielded pauses; includes FIFO/RDP waits and completion-message latency. This is not a pure geometry-work counter. |
| `AUD` | RSP audio scheduled time overlapping this graphics task's lifetime. Audio outside that interval is excluded. |
| `GAP` | Time from acknowledged graphics yields to their resumes, including audio and scheduling overhead. This overlaps `AUD`; do not add them. |
| `YIELDS` | Number of actual graphics yields, not yield requests. |
| `YLAG` | Longest request-to-yield-acknowledgment time in this task. A task finishing instead of yielding does not increase this value. |
| `DPEND` | Wall time from the first graphics RSP launch to handling its RDP completion message. Includes waiting for RSP input and audio interruptions. |
| `CLK` | `DPC_CLOCK_REG`, converted from RCP clocks. |
| `CMD` | `DPC_BUFBUSY_REG` command-buffer busy counter, converted from RCP clocks. |
| `PIPE` | `DPC_PIPEBUSY_REG` pipeline busy counter, converted from RCP clocks. |
| `TMEM` | `DPC_TMEM_REG` texture-memory load counter, converted from RCP clocks. |
| `END` | Wall time from first graphics launch until both SP and DP completion messages have been handled. Excludes time waiting in the submission queue and waiting to present at a VI retrace. |
| `AVG`, `MAX` | Average and maximum `END` over the last 64 completed graphics tasks, or all available samples during startup. The window continues across level changes; wait at least 64 tasks after settling into a test view. |

RDP counters overlap each other and the RSP execution interval. **Do not add
RSP and RDP values to estimate frame time.** High RSP time can result from
waiting behind the RDP. High `TMEM` suggests investigating texture traffic;
high `PIPE` suggests investigating raster work, overdraw, modes, and memory
contention. These are leads for controlled experiments, not definitive causes.

The four DPC registers have 24 significant bits. `COUNTER RANGE!` means the
launch-to-DP-completion interval is at least about 268.4 ms, long enough that
a counter may no longer represent the whole task. Treat those counter readings
as inconclusive. The warning is conservative: individual units may have been
busy for much less than the full interval. Timestamp arithmetic handles a
32-bit Count wrap provided an individual interval is shorter than one wrap
(about 91.6 seconds).

## Implementation and measurement limits

- `src/sched.c` timestamps graphics/audio starts, yield requests, SP completions,
  and DP completion. It reads DPC registers before another task can reset them.
  The existing counter reset remains on fresh graphics starts, never resumes.
- `src/rcpprofile.c` holds one in-flight graphics sample plus a completed
  snapshot. It publishes only after both SP and DP finish, regardless of message
  order. Audio running when DP finishes is clipped to the graphics interval.
  No `OSScTask`, `OSSched`, microcode, or yield-buffer layout changes are needed.
- This matches GUD's current single combined SP/DP graphics-task scheduling.
  If independently pipelined graphics tasks or DP-only tasks are introduced,
  extend the accounting before relying on its results.
- The game thread copies the snapshot with interrupts briefly masked, then
  formats and draws it outside the mask. A 64-entry history stores task latency.
- Conversion uses standard N64 clocks: CPU Count at 46.875 MHz and RCP at
  62.5 MHz, as documented in `include/PR/os.h`. DPC registers and masks are in
  `include/PR/rcp.h`. This is intended for standard real N64 hardware.
- The profiler measures scheduler notification boundaries, not exact hardware
  instruction boundaries. Task-load work before launch is excluded; the small
  start-accounting call and interrupt/message latency are included.
- The text itself consumes CPU, RSP, RDP and texture-load work. Keep the same
  overlay for before/after comparisons. `GFX`/`BG` CPU counters still count
  commands appended to the main list, not commands inside referenced lists;
  `GFX` is sampled before this overlay is appended.
- Each text row checks remaining master display-list space and is omitted if
  necessary, leaving space for the final full-sync/end commands. If rows are
  missing in a crowded view, check the stage's `-mgfx` budget before recording
  a comparison; changing overlay coverage changes its cost.
- Emulator counter support varies. Zero or implausible emulator readings are
  not evidence of zero hardware cost. No real-hardware performance claims are
  made by this patch.

## Hardware baseline to collect

The temporary [scissor experiment](RcpScissorExperiment.md) compares full and
half-width world rasterization within one ROM, using the same camera and
geometry submissions. Use its full-width mode as the matching baseline.

Use the same ROM settings, video mode, player count, camera position, and enemy
activity before and after a change. Let loading settle and the 64-task history
fill, then take repeated screenshots or a short capture showing both panels.
Record the ROM commit and whether the console has an Expansion Pak.

1. Frigate bow/interior: static room and prop submission.
2. Train corridor with guards: animation, character geometry, audio yields.
3. Jungle: texture traffic and foliage/overdraw.
4. Open Runway view: visibility and longer draw distances.
5. Monitors, glass, water, bullet marks, and sky: rendering correctness checks.
6. Two- and four-player views: whole-frame accounting and audio continuity.

Check that the task number advances; yields increase in suitable busy views;
`RSP <= END` and `DPEND <= END` before display rounding; and that audio remains
clean through level transitions and heavy scenes. Expect `END` to differ from
the displayed FPS interval because frame building, queueing and presentation
also affect delivery.

The earlier source findings, candidate priorities, compatibility constraints,
and source links are preserved in [GraphicsOptimizationReview.md](GraphicsOptimizationReview.md).
After collecting these baselines, choose one candidate, make one change, and
compare identical views. Native microcode counters for vertices, triangles,
clipping, DMA waits, and FIFO stalls remain a later step requiring a buildable,
verified GoldenEye microcode baseline.

## Validation

Run the host accounting tests without a ROM or N64 toolchain:

```sh
python3 tools/tests/rcpprofile/run.py
```

They compile the production module and cover both completion orders, task
reuse, multiple audio yields, repeated yield requests, completion instead of
yielding, audio overlap at DP completion, timer wrap, counter masking/range,
rolling-window eviction, rounding, and interrupt-mask restoration.

The restoration also tests the actual scheduler entry points with OS/hardware
shims: fresh-task counter resets, counter retention through yield/resume,
both SP/DP completion orders, and publication before task release. A HUD test
checks the actual display routines' formatted values, full-screen placement,
and command-buffer capacity boundaries while reserving the final sync/end.
These host checks do not emulate RSP microcode or RDP rasterization.

The changed `rcpprofile.c`, `sched.c`, and `game/lv.c` also compile with the
repository's IDO 5.3 toolchain as MIPS-II objects. A complete ROM link and
real-console display/timing validation still require the extracted ROM assets
and hardware; they were not available during development of this patch.
