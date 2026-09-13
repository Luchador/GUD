# Watch render options and AA profiler

Open the watch's Options page and move down past Screen Ratio. The volume
sliders stay fixed; the eight visible toggle rows scroll to keep the selection
on screen. The footer shows the visible range. Navigation wraps through all
13 options. Press A to select a row, then use left/right to change its value,
as with the existing options.

| Option | Values | Effect |
|---|---|---|
| OPAQUE AA | Full / Reduced / Off | Full preserves authored render modes. Reduced keeps coverage AA but removes the framebuffer colour read on supported opaque surfaces. Off uses their non-AA equivalents. |
| VI FILTER | Smooth / Edges / Off | Smooth enables VI edge AA, divot correction and dedithering. Edges keeps edge AA/divot but disables dedithering. Off disables those three filters while retaining the VI resampling needed for video output. |
| RENDER STATS | Off / On | Shows a gameplay timing panel below the existing FPS and CPU panel. |

Full/Smooth are the defaults. AA and VI preferences are stored per save folder,
through the usual folder-options save/load path. They occupy the existing
unused save byte, with a format marker; saves without a valid marker use the
defaults. The profiler setting is not saved and starts Off each boot.

Opaque AA affects the supported ordinary opaque surface, terrain and decal
render modes, including modes in asset display lists. It preserves their
depth behavior and first-cycle fog/pass blending. Translucency, texture-edge
cutouts, particles and custom modes retain their authored settings. Thus
Opaque AA Off is deliberately scoped; it does not remove every AA operation
from the scene. VI Off applies to the final video output independently.
Reduced can expose internal triangle seams, and Off can expose jagged edges.

## Taking measurements on N64

1. Enable Render Stats. Begin with Opaque AA Full and VI Filter Smooth.
2. Stand at the chosen spawn point with the same view, screen mode, player
   count and activity for every comparison. Close the watch completely.
3. Wait through 30 eligible warmup tasks and the first 60-task sample window.
   Record the panel after it says `60 FRAMES: AVG / MAX MS`.
4. Capture several windows. Return to the watch, change one option, close it
   and repeat. Opening the watch or changing a setting resets the window.
5. First compare Full, Reduced and Off with VI Smooth held constant. Then
   compare Smooth, Edges and Off with one opaque-AA setting held constant.
   Full/Off isolates VI filtering; Off/Off shows the combined change.

The panel heading records the selected AA/VI pair. Values are milliseconds,
shown as average / maximum over the most recently completed block of 60
eligible graphics tasks. They update once per block, rather than every frame.
Level changes, paused gameplay and watch transitions discard the old window
and restart warmup. Pausing any player excludes multiplayer frames.

| Row | Measurement |
|---|---|
| GFX | Wall time from the first graphics RSP start hook to handling that task's DP completion message. Includes RSP input delays, audio yields, memory stalls and notification latency. Excludes time waiting in the submission queue or for presentation. |
| DP CMD | Command-buffer activity counter: RDP clocks while its command FIFO has work. |
| DP PIPE | Pipeline activity counter: RDP clocks while the pipeline is not stalled waiting for memory. |
| DP TEX | Texture-memory load activity counter. |
| AA CPU | CPU wall time spent walking display lists and applying the AA policy before submission. Includes any preemption during that interval. |

Use GFX to compare graphics-task latency; it is not pure RDP execution time
or the displayed frame interval. The DP counters overlap and must not be
added together. VI changes affect memory traffic and output filtering;
there is no separate VI-time counter in this panel. AA CPU is outside GFX
and makes the cost of applying the settings visible. Overall FPS still
depends on CPU work, queueing and presentation as well as graphics.

The full graphics task includes the HUD and profiler text. Keep the profiler
enabled in every measured run. Rows are omitted if the master display list
lacks room, using the existing overlay's capacity guard; compare captures
with the same rows visible. This patch makes no measured speedup claim.

DP counters are 24-bit. Tasks lasting 250 ms or more are skipped to avoid
misreporting wrapped counters; `LONG TASKS SKIPPED` reports the count since
the last reset. `AA DISPLAY LIST ERROR` means the display-list walk failed
validation or exceeded its bounds; that task is excluded. Do not use a run
showing that error as evidence for an AA setting. Emulator counter support
varies, so real-console measurements are the intended result.

## Implementation notes

The render settings and profiler modules' text is linked into resident
`.code`, even though their source files live under `src/game`. VI retraces
start before `bossInitMainthreadData` initializes game demand paging. Their
callbacks, and the scheduler's profiler hooks, must not enter the paged
`0x7f...` game segment. Linker assertions enforce residency of those entry
points. Changes to included linker scripts now also trigger a relink.

Watch strings include trailing newlines, matching the existing language-bank
labels. `textMeasure` only adds height at a newline; omitting it creates a
zero-height clipping rectangle and hides the label, value, arrow or footer.

The game drains pending graphics tasks before applying AA/VI changes because
asset display lists are shared between frames. The submission pass follows
Fast3D display-list calls, branches and segments. A bounded stack and command
budget prevent an invalid list from hanging the walker. A 1,024-entry cache
skips static leaf lists already processed; dynamic graphics/vertex pools are
always scanned. Texture loads, stage-pool initialization and background
render-mode LUT updates invalidate that cache.

Changed render-mode commands retain a small restoration tag in the unused
bits 16..23 of Fast3D SetOtherMode's first word. No commands are added and no
microcode is changed. The tag identifies the original opaque mode, allowing
exact restoration even when two authored modes share the same Off equivalent.
It is removed before the game's exact-command LUT processing, and never
written to assets on disk. Already-correct commands are not rewritten.
This encoding depends on GUD's Fast3D command format; revisit it if switching
microcode families.

Task metadata is captured at submission, with an epoch that prevents already
queued tasks from entering a new sampling window. Scheduler hooks retain the
first start and the DP counters across audio yields. The counters are read
before another graphics task resets them. Accounting assumes GUD's combined
SP/DP graphics tasks; independently pipelined or DP-only graphics tasks would
need different accounting. Interrupt masking protects shared snapshots.

Count timestamps use 46.875 MHz and DP counters use 62.5 MHz, matching the
standard N64 clocks in `include/PR/os.h`. Counter meanings and filter behavior
are described in Nintendo's
[osDpGetCounters documentation](https://ultra64.ca/files/documentation/online-manuals/man/n64man/os/osDpGetCounters.html)
and the
[VI programming chapter](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro29/29-01.htm).

## Validation

Run the host regression suite from the repository root:

```sh
python3 tools/tests/render_options/run.py
```

Optional address and undefined-behavior checks:

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/render_options/run.py
```

The suite exercises production AA transformations and restoration, segmented
and nested lists, cache invalidation, all nine AA/VI pairs, legacy-save
defaults, watch navigation and edit controls, timing windows, stale-task
rejection, audio-yield resumes and counter conversion/wrap handling. It also
checks resident callback placement and runs the actual watch page drawing,
label layout and text measurement with all option combinations. The final
rasterizer is replaced with a check that rejects empty or off-screen clipping
rectangles. Font metrics and hardware calls are shimmed; these checks do not
emulate rendering or a console boot.

The affected C files were compiled to MIPS assembly with the repository's
IDO 5.3 compiler and US build flags. Host tests and sanitizer checks passed.
A full ROM build and on-console visual/timing validation were not available
in the development workspace. On hardware, check scrolling and all nine
combinations, switching back to Full/Smooth, folder reload persistence,
fog, glass, foliage, decals, sky and multiplayer views before keeping the
display options in a release.
