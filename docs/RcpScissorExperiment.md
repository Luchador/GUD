# RCP scissor experiment

`GUD-rcp-scissor-test.patch` targets master `1641d74c` (Add RCP profiler).
It is a temporary workload comparison for the normal US build, independent of
DEBUG. Build with `make VERSION=US`.

## Controls and capture

Every stage starts at full width. In single-player gameplay, hold **L + R** and
tap **D-pad Up** to toggle between the full view and its centered half width.
Release the buttons before measuring. The profiler heading shows the selection:

| Heading | World drawing area |
|---|---|
| `S:1/1` | Full player view |
| `S:1/2` | Middle half of its width, full height: 50% of the area |

At the ordinary 320-pixel width, the half-width region is x=80 through x=239.
This is a crop, not a resized image or a narrower camera field of view. Black
areas appear on both sides, while the first-person weapon, casings, watch, HUD
and profiler continue drawing with their normal bounds. The toggle is ignored
while controls are locked, paused, in a watch transition/menu, or in an intro
camera. Multiplayer and the title screen do not run the experiment.

Use the same stationary camera in each mode. Avoid combat while taking a
baseline, or repeat the comparison to distinguish changes in enemy activity.
After switching, wait until the task number advances by **at least 70** before
recording `AVG`/`MAX`: these cover the last 64 completed tasks, and a few older
tasks can still be queued. The selection in the heading describes the frame
being built; it is not a tag on the older completed timing sample. Do not use
the readings during the transition. No live scheduler accounting is reset.

Capture **full -> half -> full** for Runway, Cradle and Jungle. The second full
capture checks whether the scene changed while taking the measurements. Use
this patch's full-width mode as the baseline, rather than the earlier photos.

## What changes

The experiment is active from sky rendering through room geometry, characters,
props, tracers, sparks, glass and explosion debris. Each emitted world scissor
is intersected with the half-width region. Room/portal visibility boxes, camera
matrices, RSP clipping parameters, vertex/triangle submissions, textures and
rendering modes are not reduced. Even a room whose scissor becomes empty keeps
its display-list submission.

The scissor cache still compares the **original** room bounds, before applying
the test. Distinct room rectangles therefore cannot collapse into one cached
command only in half-width mode. The cache is invalidated at both experiment
boundaries, and the normal player scissor is restored before weapon/UI drawing.

The sky's two single-player fill paths explicitly clip their rectangle
coordinates. Nintendo's [gDPSetScissor reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gdp/gDPSetScissor.html)
warns that fill/copy rectangles must fit inside the scissor. An empty sky fill
emits one no-op in place of its rectangle, preserving command count. Sky vertex
projection and textured triangle generation remain unchanged.

Both modes add the same full-view black color clear and the same number of
boundary commands. This prevents stale pixels outside the crop. The existing
full-size depth clear, framebuffer addresses/strides/alignment, scheduler and
microcode stay unchanged. The clear, weapons, UI and profiler impose a fixed
cost, so halving the world drawing area need not halve total task time.

The profiler keeps the same rows, placement and full-screen scissor in both
modes. Only its mode digit and measured values change. For a fixed scene, main
display-list command counts should be comparable; runtime activity can still
change geometry, audio work and animation as the frame rate changes.

## Interpreting the result

- A substantial decrease in `END` supports pixel-dependent RDP work as a
  bottleneck. A simultaneous decrease in scheduled `RSP` time is consistent
  with reduced waiting for the RDP to consume commands.
- A small decrease points toward geometry/command handling, synchronization,
  or the work outside the cropped pass. It does not by itself prove that the
  RSP is the bottleneck. The crop can also exclude an unusually cheap or
  expensive part of the scene; repeat at another camera position.
- RDP busy counters overlap each other and RSP timing. Do not add them, and
  do not read a reduction in `TMEM` as the entire savings from texturing.

## Validation

`python3 tools/tests/rcpprofile/run.py` exercises the production accounting,
scheduler and HUD plus the actual clipping, cache, fill and toggle functions.
The scissor checks cover intersecting/disjoint rectangles, exclusive versus
inclusive bounds, a non-default viewport, identical clear/command counts,
cache behavior and input gating. Native US IDO compilation is also checked
for `lv.c`, `bg.c` and `sky.c`. Hardware timings and visuals require the N64
comparison above; host checks do not emulate rasterization.

To remove the experiment before making permanent optimizations, reverse this
patch with `git apply -R GUD-rcp-scissor-test.patch` while its changes remain
intact. This leaves the restored RCP profiler in place.
