> Preserved source review from before the profiler patch. The initial scheduler/RDP profiler described below is now implemented; see [RcpProfiler.md](RcpProfiler.md) for measurement definitions and the hardware baseline plan. The remaining optimization candidates are unmeasured and are retained for follow-up.

**GUD graphics optimization review, 12 September 2026**

The most promising direction is to measure the RCP directly, then reduce texture traffic, avoid unnecessary render-state work, and reject more invisible geometry. F3DEX3 provides useful implementation ideas for each. A full F3DEX3 conversion deserves a separate experiment because GoldenEye has custom display-list commands and gameplay code that reads those commands.

Reviewed GUD commit `bc1927c02847a4333f7d50fae5d6cf4fb97c7fef` and F3DEX3 commit `91a85284821181af627d838e47d245718804f388`. This review includes source inspection and a decoded inventory of 25 BG source files. No game code was changed. No real-hardware timings or FPS improvements were measured. The extracted graphics microcode binaries and base ROM were absent from the checkout, so the active binary could not be disassembled or compared against the draft assembly. Some generated F3DEX3 documentation pages are older than the source checkout; implementation conclusions below use the pinned source.

**What is running today.** The CPU builds a frame list and references room/model lists. Room loading expands texture-bank commands into RDP texture commands. The scheduler runs graphics alongside audio using RSP yields. The graphics RSP program writes RDP commands through a 40 KiB RDRAM output buffer. The RDP then rasterizes the scene. These stages overlap, so shortening one stage only improves frame delivery when it shortens the limiting path or reduces contention.

| Area | Source locations | What matters for optimization |
|---|---|---|
| Frame and existing profiler | `src/boss.c`, `src/game/lv.c` | CPU time spent building the frame; counts of commands appended to the main list. |
| Room/prop submission | `src/game/bg.c`, `src/game/chrprop.c` | Portal visibility, scissors, opaque and translucent ordering, repeated matrix/state setup. |
| Model submission | `src/game/model.c` | Existing type-3 pipeline/segment cache; other model setup paths still emit state per part. |
| Texture expansion | `src/game/tex.c` | `texLoadFromGdl`, `texWriteLoadToTmemAddr`, `texWriteLoadToTmemZero`; tile-state caching and repeated loads/syncs. |
| Tasks and scheduling | `src/game/rsp.c`, `src/sched.c` | Graphics launch, audio preemption, graphics/RDP completion and RDP counter reset. |
| Microcode integration | `src/gspboot.s`, `Makefile`, `rsp/graphics/gmain.s` | Graphics binaries are included directly; the draft graphics assembly is not built. |
| GEditor compilation | `tools/geditor/src/bgcompile.c`, `modelcompile.c` | Offline control over native geometry, materials and vertex batches. |

Source anchors: [frame profiler][g-prof], [room rendering and collision][g-bg], [model rendering][g-model], [texture expansion][g-tex], [scheduler][g-sched], [task setup][g-rsp], [editor compiler][g-compile].

**The first useful patch would be RSP/RDP profiling.** The current `GFX` value is `(gdl - firstGdl)` and `BG` similarly counts commands appended around `bgSetupAndRender`. A call to a large room display list counts as one command here. Neither number measures all commands executed by the RSP, triangles rasterized, texture bytes loaded, or time spent by the RDP. Likewise, `BGRENDER` times the CPU's submission work, not GPU completion. These counters remain useful for CPU work, but they cannot establish which graphics optimization is best.

GUD already clears the four RDP performance counters in `__scExec` with `osDpSetStatus(0x3c0)` on a fresh graphics task. Read and save those counters in `__scHandleRDP` before another task is launched. Keep results attached to the completed task/frame. This gives us hardware counter data without changing the microcode. Timestamp fresh graphics launches, each yield/resume and final RSP completion to separate graphics scheduling time from audio time. Display milliseconds alongside the existing CPU values. CPU Count ticks run at 46.875 MHz on standard hardware; RCP cycles use 62.5 MHz, so the raw numbers must not share one conversion.

The first screen should expose CPU frame-build time, accumulated graphics RSP scheduling intervals, RDP completion time, raw CLK/CMD/PIPE/TMEM counters, and audio/yield timing. RSP scheduling intervals still include time blocked behind the RDP and interrupt latency; they are not pure geometry-computation time. RDP counter values overlap and should not be added together as independent costs.

F3DEX3's profiling design is a useful model for a later native-microcode profiler: record vertices, input/output triangles, clipping, DMA waits, and FIFO stalls, then transfer results through the yield buffer. Its instrumentation is implemented inside the microcode; copying only its C reader into GUD would not produce those counters. [F3DEX3 counters][f-counters]

**The original BG assets already use fairly full batches.** I decoded the compressed room arrays in all 25 BG C source files, stopped each list at ENDDL, and validated vertex ranges and every triangle index. Every triangle in this inventory references the latest vertex load. The table counts each stored primary/secondary list once across the whole level. It is not a visible-frame workload and does not include props, characters, runtime texture expansion, or user project files outside the repository.

| Level | Stored triangles | Vertex loads | Mean vertices per load, maximum 16 | Mean triangles per TRI4, maximum 4 |
|---|---:|---:|---:|---:|
| Frigate | 16,677 | 1,837 | 15.33 | 3.55 |
| Train | 9,358 | 1,073 | 15.21 | 3.59 |
| Jungle | 6,265 | 707 | 15.02 | 3.55 |
| Dam | 12,400 | 1,477 | 14.71 | 3.54 |
| Archives | 10,957 | 1,350 | 15.27 | 3.58 |
| Runway | 2,991 | 334 | 15.09 | 3.62 |

Across the full inventory there are 24,192 vertex loads, 366,215 loaded vertices, 57,044 TRI4 commands and 2,119 TRI1 commands. Filling existing batches more tightly therefore looks like a modest opportunity for original BGs. Larger caches could still reduce reloads and batch boundaries; these averages do not measure vertex reuse across batches. GEditor-created or heavily edited geometry could behave differently and should receive the same audit.

**Priority candidates.** The ranking is my engineering assessment of the inspected code, not a measured ranking of hardware gains.

| Priority | Candidate | Expected saving | First bounded experiment |
|---|---|---|---|
| 1 | Skip setup for empty prop passes and avoid proven redundant projection loads | CPU command generation, matrix DMA and RSP matrix work | Add per-room pass-presence information during the existing room-list build; preserve all draw ordering. |
| 2 | Consolidate texture synchronization and redundant material state | RDP command work and stalls; some RSP traffic | Optimize one controlled non-paletted texture-load path with an explicit synchronization contract. |
| 3 | Reuse texture contents across repeated object draws | TMEM load time, RDRAM traffic, RSP/RDP commands | Start with an opaque object/material whose texture and palette are immutable and whose intervening draws are understood. |
| 4 | Add conservative occlusion within visible rooms | Geometry submission and/or RDP overdraw | Author a large solid occluder in one test room; compare counts and timings with occlusion disabled. |
| 5 | Tune native microcode dispatch and DMA scheduling | RSP execution and transfer waits | Establish a buildable, verified GoldenEye microcode baseline, then change one hot path. |
| 6 | Increase vertex-cache reach and change batch generation | Vertex reloads, transformations and display-list fetches | Compare exported workloads at multiple cache sizes before committing to a new triangle encoding. |

**Empty-pass and state work is unusually concrete here.** `bgRender` changes projection matrices around the pre-BG prop pass, room BG and post-BG prop pass, even when a pass will emit no geometry. It also emits projection setup in the reverse traversal for translucent props. The per-room linked lists already exist, making it practical to record whether relevant passes have work without rescanning all props. A pass mask must retain the Cuba special case, render-late/render-post-BG flags, and scissor restoration semantics.

There is also existing model state caching in `modelRenderNodeDlWithCache`; any extension should build on it. Cache only state whose current value is known. Nested asset lists can change state without calling the CPU-side setter, so a broad cache keyed only on the last C function call would be incorrect. These changes are attractive because they preserve the current ordering and geometry format. [Room/prop submission][g-props] [Model rendering][g-model]

**Texture traffic deserves more attention than another global geometry-mode cleanup.** `texLoadFromGdl` already suppresses some duplicate setup while expanding a stored list, and `texTrySetTileState`/`texTrySetTileSize` cache descriptors during that process. They do not establish that texture contents remain resident in TMEM across arbitrary later object draws. The expanded room/model lists still execute their embedded texture loads each time they are drawn.

The current loader emits load syncs and pipe syncs around load blocks and palettes. F3DEX3's GBI documents a material-setup arrangement with synchronization at the draw-to-state-change boundary and distinct load tiles for multiple uploads. Borrowing that arrangement is an RDP-side optimization and can be investigated with GoldenEye's existing microcode. GoldenEye often reuses tile 7 for texture and palette loading, so removing syncs without restructuring those dependencies would be unsafe. Keep CI palettes, mip chains, detail textures and changing light-fixture materials in the validation set. [F3DEX3 GBI, texture synchronization][f-gbi]

For repeated draws, a CPU-managed material system or a native microcode texture-load suppression feature could help. The identity must cover the actual TMEM contents and layout, palettes, subregions and changes to source data. Texture ID alone is insufficient. F3DEX3 uses repeated material-list location as its shortcut and provides explicit invalidation when segment remapping changes the source image. GoldenEye's water and light-fixture paths make such invalidation important. [F3DEX3 GBI, repeated materials][f-gbi]

An immediate-repeat texture audit found only 17 identical consecutive texture-use commands across the stored BG lists; this does not count reuse across room calls or repeated props. I would test repeated objects first rather than assume the interiors of original room lists contain many duplicate loads.

**Occlusion is the strongest editor-assisted candidate.** GoldenEye already uses portals and scissors, and opaque props are already traversed near-to-far within their room list. We should measure the geometry left over after those systems. A room can still contain a wall or other large solid obstruction that hides much of the submitted content.

GEditor could export conservative occluder quads placed inside opaque walls. Initially, a CPU test could reject whole prop bounds or separately compiled BG clusters when completely hidden. A native RSP occlusion-plane implementation could later reject individual triangles. Treat doors, windows, cutouts and breakable geometry according to their actual opaque coverage and current state. Keep collision geometry available even when its rendering is rejected.

F3DEX3 offers both an occlusion-enabled configuration and a faster vertex path without it, so the extra vertex work need only be accepted where it pays. Its reported gains in other games are evidence that the approach can work, not estimates for GoldenEye. I would compare a Frigate or Archives obstruction with an open Runway view to test both benefit and overhead. [F3DEX3 configuration][f-config]

**The RSP work needs a verified starting point.** `src/gspboot.s` includes `bin/gspboot.text.bin` and `bin/gspboot.data.bin`; `rspGfxTaskStart` selects those symbols. The Makefile builds the RSP boot program, but has no corresponding graphics-source build dependency. `rsp/graphics/gmain.s` contains missing initialization references, inconsistent/duplicate labels and incomplete instructions. Editing it currently cannot change the shipped graphics microcode. [Binary linkage][g-link] [Build rules][g-make] [Draft graphics assembly][g-asm]

Before native microcode optimization, reconstruct or obtain complete matching graphics source, assemble its text/data and overlays, and compare with the extracted baseline binaries. Add explicit build dependencies on the generated outputs. Matching here is a validation tool for the new build path; it is not a proposal to restore GUD's old ROM-matching policy.

The best assembly targets to inspect next are command dispatch, TRI4 decoding, small RDP command forwarding, triangle coefficient calculation and DMA overlap. F3DEX3 interleaves scalar/vector work, pays attention to instruction alignment, and uses two RDP staging buffers. GoldenEye's draft suggests a blocking flush through one staging area and fixed 320-byte display-list fetches. Those two observations need confirmation against the actual binary before being treated as active performance problems. Display-list length hints and asynchronous staging are plausible experiments after that confirmation. A larger RDRAM FIFO alone will not fix a rasterizer that cannot consume commands fast enough. [F3DEX3 assembly][f-asm]

F3DEX3's published hand-counted examples show small RDP command handling falling from 14 to 4 cycles and a drawn triangle path from 172 to 149 cycles relative to F3DEX2. Unlit vertex-pair processing remains 54 cycles in both the compared F3DEX2 and F3DEX3_NOC paths. These are not GoldenEye measurements, and they strengthen the case for examining dispatch/triangle work rather than assuming its lighting rewrite will speed up GoldenEye's largely vertex-colored content. [Performance results][f-performance]

**A full F3DEX3 port has specific compatibility costs.** GoldenEye's TRI4 uses four-bit indices; simply allocating more vertex slots cannot make that command address them. Its stored lists use the older GBI encoding, so recompiling C with a replacement header would leave embedded binary commands unchanged. The sky renderer also generates raw RDP triangle coefficients using `G_RDPHALF_1`/`G_RDPHALF_CONT`, a path requiring explicit adaptation. [GoldenEye GBI extensions][g-gbi] [Sky renderer][g-sky]

`bgBuildRoomVtxBounds` and `bgTestRayIntersectionInRoom` decode native VTX/TRI1/TRI4 commands and use the most recent vertex batch for bullet intersection work. A wider cache, partial cache reloads, reordered lists or new triangle commands must update those consumers, or use a separate canonical collision representation. GEditor's import, validation, export and ROM-building paths must agree with the chosen format.

There is a concrete task-memory mismatch too: GUD allocates `g_gfxYieldBuf` using `OS_YIELD_GFX_DATA_SIZE = 0xBA0`, whereas F3DEX3 writes a `0xC00`-byte yield image. A direct replacement would need 96 more bytes and appropriate alignment. Audit initial data sizes, task flags, FIFO format and audio preemption as part of any port. [GUD task sizes][g-task] [F3DEX3 task sizes][f-task]

Viewport Y/depth conventions and clipping also change in F3DEX3. In GUD, inspect the viewports in `src/fr.c`, CPU projection work in `src/game/sky.c`, fog/culling distances, decals and first-person geometry together. Far-plane clipping is removed in F3DEX3, and near clipping differs; these are observable behavior changes that need GoldenEye-specific decisions. [F3DEX3 porting guide][f-porting] [Clipping tradeoffs][f-tradeoffs]

**Lower-priority ideas are still worth retaining.** Triangle snakes would save less command bandwidth relative to GoldenEye's TRI4 than relative to F3DEX2's TRI2; F3DEX3's own current performance notes describe an RSP-time tradeoff. Keep TRI4 as the comparison point. Packed normals, point lights and specular shading are visual capabilities, with additional processing costs, and are not immediate optimizations for the present baked-color rendering path. [Performance results][f-performance]

RSP-driven framebuffer/Z clears and an explicit early command flush are reasonable later scheduling experiments. GUD currently clears Z using RDP fill mode and reuses Z storage across multiplayer views. RSP writes would need correct ordering with prior RDP accesses and must not erase depth still in use. Test that separately from geometry changes. Selective one-cycle materials are another option where the authored combiner, fog and mip/detail filtering permit it; replacing all two-cycle materials would alter GoldenEye's appearance. The existing water scroller already edits tile offsets instead of rewriting every vertex, so a generic UV-scroll feature has a less obvious payoff here.

**How to validate on the EverDrive/N64 setup.** Use an on-screen profiling page and a repeatable camera position/path. Warm room and texture loads before taking steady-state measurements, then separately test transitions. Hold resolution, VI settings, player count, audio and scene content constant. Record a few hundred frames and show median and worst-case milliseconds as well as FPS. Capture a small results table per experiment; USB logging is unnecessary.

| Test view | Main question |
|---|---|
| Frigate bow and a room with a large obstruction | Is the remaining delay geometry processing, texture traffic or pixel work? Does occlusion help? |
| Train corridor with the same nearby guards | Does a graphics improvement affect the frame limit while character work stays constant? |
| Jungle foliage/combat | Do transparency, texture edges and two-cycle pixel work dominate? |
| Open Runway view | What overhead does a feature add when there is little occlusion? |
| Monitors, glass, water, bullet marks and sky | Are texture state, custom sky commands and decal depth behavior preserved? |
| Two/four-player views with active audio | Are yield/resume, shared Z-buffer use and per-player state correct? |

Measure CPU build, graphics scheduling intervals, RDP counters and completed-frame time together. A drop in command count without a frame-time reduction is still useful diagnostic evidence, but it is not proof of an FPS gain. When instrumented microcode becomes available, distinguish actual geometry time from FIFO backpressure before changing arithmetic. The editor's desktop OpenGL timing cannot predict these N64 costs.

The proposed implementation sequence is: add hardware timing; use that evidence to select an empty-pass/state or texture-loader patch; establish the native microcode build baseline; then test one measured RSP hot path or an editor-authored occluder. A full F3DEX3 backend can be evaluated after a converter and compatibility test scene exist. Keep one experimental change per comparison so its effect is identifiable.

**Reproducing the static inventory.** The accompanying audit bundle includes `audit_gud_bg.py` and its JSON output. Run `python3 audit_gud_bg.py /path/to/GUD > gud-bg-static-audit.json` against the pinned GUD commit. It reads embedded compressed room arrays, validates their decompression and indices, and writes counts without modifying the repository. It is a source-asset inventory rather than an emulator or runtime display-list profiler.

[g-prof]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/game/lv.c
[g-bg]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/game/bg.c
[g-model]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/game/model.c
[g-tex]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/game/tex.c
[g-sched]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/sched.c
[g-rsp]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/game/rsp.c
[g-compile]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/tools/geditor/src/bgcompile.c
[g-props]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/game/chrprop.c
[g-link]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/gspboot.s
[g-make]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/Makefile
[g-asm]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/rsp/graphics/gmain.s
[g-gbi]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/include/gbi_extension.h
[g-sky]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/src/game/sky.c
[g-task]: https://github.com/Luchador/GUD/blob/bc1927c02847a4333f7d50fae5d6cf4fb97c7fef/include/PR/sptask.h
[f-counters]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/docs/Code/Counters.md
[f-gbi]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/gbi.h
[f-config]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/docs/Documentation/Configuration.md
[f-asm]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/f3dex3.s
[f-performance]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/docs/Documentation/Performance.md
[f-task]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/rsp/rsp_defs.inc
[f-porting]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/docs/Documentation/Porting%20your%20Romhack.md
[f-tradeoffs]: https://github.com/HackerN64/F3DEX3/blob/91a85284821181af627d838e47d245718804f388/docs/Documentation/Design%20Tradeoffs.md
