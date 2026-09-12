# N64 hardware fixes

The September 2026 hardware investigation is complete. After restoring full
rendering, the user confirmed that Runway, Cradle, other levels, and the
watch menu run on real N64 without the earlier hangs.

## Color framebuffer alignment

The failing build placed the two color framebuffers at `803B4950` and
`803DA150`. Both were only 16-byte aligned. Nintendo's
[image-command reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gdp/gDPSetColorImage.html)
requires 64-byte alignment for color and depth images.

Removing the unused `sp_debug[0x6B0]` stack had moved the framebuffers, which
follow the stacks in the linker script. Their previous alignment depended
on the stack allocation's size. The old normal US build also included that
stack; this was not limited to the DEBUG build.

The permanent fix is explicit `.cfb ALIGN(64)` placement in `ge007.ld`.
With the current stack layout, the buffers start at `803B4980` and
`803DA180`, and end at `803FF980`, within the original 4 MiB RDRAM region.
Keep the linker assertions for alignment and the memory limit, the
per-frame stride assertion in `src/cfb.c`, and its `src/fr.h` build dependency.
Do not restore the unused stack or disable ordinary depth rendering.

## Other correctness fixes retained

* `bgLoadFile` uses an explicitly aligned temporary header buffer for ROM
  DMA. Its destination meets the PI alignment requirement and covers whole
  data-cache lines.
* `crashMain` restores the previous interrupt mask when a fault event has
  no faulted thread. Keep this restoration before its `continue`.

## Investigation cleanup

`GUD-n64-diagnostic-cleanup.patch` targets master `337bd7bd` (Restore rendering).
The temporary watchdog/thread, six-page crash display, stage/DMA/allocation
tracking, RSP/FIFO snapshots, GBI command preflight, fill-only probe, and
render-isolation switches are removed, together with their tests and linker
entries. The existing fault/TLB handler and ordinary rendering remain.

The CPU profiler code, RCP profiler module/tests, and optimization notes are
preserved. See [RcpProfiler.md](RcpProfiler.md) and
[GraphicsOptimizationReview.md](GraphicsOptimizationReview.md) for the earlier
profiling work. Removing the per-task command walk and watchdog eliminates
their overhead from future performance measurements.

At the cleanup baseline, the RCP module was present but its scheduler/HUD
connections and explicit linker entries were absent. The subsequent
`GUD-rcp-profiler-restored.patch` reconnects them without restoring the
crash-investigation code; see [RcpProfiler.md](RcpProfiler.md).

## Cleanup validation

Normal US IDO 5.3 compilation passed for all edited C files and the retained
framebuffer, stack and RCP profiler modules. Thirteen native objects have
identical code, read-only data, data and BSS to current master compiled with
diagnostics disabled. The RCP profiler accounting tests pass, and the
preprocessed linker script has no remaining diagnostic object references.
No full ROM build, emulator run or hardware test of this cleanup was performed.
