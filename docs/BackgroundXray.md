# Background x-ray debug view

During ordinary single-player gameplay, press **D-pad Up** to enable the live
background x-ray overlay. Press **D-pad Down** to disable it. It starts disabled
when a level loads. The watch and cutscenes hide the overlay, and Up in those
views does not enable it. D-pad movement bindings are otherwise unchanged.

The normal scene remains visible. Submitted rooms are overlaid in translucent
colors, with the player's current room much fainter. The overlay ignores depth,
backface culling, textures/texture alpha, fog and portal scissors, so enclosing
walls cannot completely hide the background geometry behind them. Normal camera
clipping and the viewport boundaries still apply.

The HUD shows the number of rooms submitted this frame, the current room number,
and a color-matched legend of up to 18 submitted room IDs (decimal, as in GEditor).
Colors stay stable as rooms enter and leave the list; the palette repeats every
12 room IDs. `+N ROOMS (LEGEND)` means additional rooms are included in the overlay
but did not fit in the legend.

## What it measures

This visualizes **background geometry submitted by the CPU**, including both
primary and secondary display lists. A room is recorded only after its normal
render path actually submits a list. Merely loaded or preloaded rooms are not
added. The normal portal traversal, load budget, room list and room scissors are
unchanged. The overlay neither loads extra rooms nor expands gameplay visibility.

Geometry shown through a wall, outside a portal scissor, or facing away may
produce no pixels in the normal pass, but its commands/vertices still reach the
graphics task. This distinction is useful when investigating unnecessary room
submissions; colored surfaces alone do not prove that the RDP was shading their
hidden pixels. Objects, characters, effects, sky and the first-person weapon
remain part of the normal scene, but are **not** duplicated by the background
x-ray overlay. Split-screen and single-display-list title backgrounds are not
supported by this debug view.

The FPS counter and bottleneck readout remain visible. The extra CPU decoding,
vertex processing and translucent overdraw affect those measurements: turn the
overlay off to measure normal gameplay performance.

## Memory and rendering

The overlay is appended at the end of the normal frame, before the FPS display.
It copies only vertex loads and TRI1/TRI4 commands into spare master-display-list
space, reuses existing room vertices/matrices, and leaves space for HUD text and
task termination. It allocates no room cache, vertex pool storage or heap memory,
and never edits the authored or one-cycle room display lists.

Nested lists and segment address changes are decoded on the CPU; material commands
cannot restore texture, depth, fog or scissor state inside the overlay. Limits on
addresses, output space, nesting and commands prevent unbounded traversal.
Unsupported geometry/transform commands, malformed lists or insufficient space
show **XRAY INCOMPLETE**. That frame's overlay must not be treated as a complete
record of submitted geometry. If necessary, turn slightly or move to a less busy
view to reduce the debug display-list demand.

## Validation

Run `python3 tools/tests/bg_debug/run.py` for native Fast3D decoding, nested calls
and branches, invalid input, pipeline checks, input toggles, actual submission
tracking, title/watch/multiplayer gating and display-list capacity checks. The
harness runs with address/undefined-behavior sanitizers and also scans primary
and secondary streams from the included background source assets. Texture marker
expansion and actual RSP/RDP rasterization are not emulated by these tests.

The changed runtime files can be compiled with:

```sh
make build/u/src/game/bgdebug.o build/u/src/game/bg.o build/u/src/game/lv.o
```

On hardware or an emulator, check Depot and another busy level with AA both On
and Off: compare Up/Down from the same position, move between rooms, open/close
the watch, and reload a level. Confirm that the normal view returns immediately
and that hidden background surfaces and the matching room IDs become visible
when x-ray is enabled.
