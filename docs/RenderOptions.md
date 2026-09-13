# Watch render options

Open the watch's Options page and move down past Screen Ratio. The volume
sliders stay fixed; eight toggle rows scroll to keep the selection on screen.
Navigation wraps through all 12 options. A green downward triangle below the
last visible row, above the page buttons, appears while more options remain
below the list. Press A to select a row, then use left/right to change it.

| Option | Values | Effect |
|---|---|---|
| AA | Full / Reduced / Off | Full preserves authored render modes. Reduced keeps coverage AA but removes the framebuffer colour read on supported opaque surfaces. Off uses their non-AA equivalents. |
| VI Filter | Smooth / Edges / Off | Smooth enables VI edge AA, divot correction and dedithering. Edges keeps edge AA/divot but disables dedithering. Off disables those three filters while retaining the VI resampling needed for video output. |

Full/Smooth remain the defaults. Both preferences are stored per save folder
through the usual folder-options save/load path. The encoding uses the existing
unused save byte and is unchanged from the profiling build, so choices saved
with that build continue to load. Saves without a valid marker use defaults.

AA affects ordinary opaque surface, terrain and decal modes, including asset
display lists. Depth behavior and first-cycle fog/pass blending are preserved.
Translucency, texture-edge cutouts, particles and custom modes retain their
authored settings. AA Off therefore does not remove every AA operation from
the scene. VI Off applies independently to final video output. Reduced can
expose internal triangle seams; Off can expose jagged edges.

The temporary AA/VI profiler, Render Stats option, task metadata, sampling and
HUD rows have been removed. The existing FPS and CPU timing panel remains.

## Implementation notes

The render settings module's text remains in resident `.code`, although its
source lives under `src/game`. VI retraces start before the main thread
initializes game demand paging, so `renderConfigureViMode` must remain resident.
A linker assertion enforces this. Included linker-script changes trigger a
relink, including when the profiler object is removed from the link.

Watch labels and values use Bank Gothic's lowercase glyphs, matching the
smaller capital-shaped letters of the existing options. Trailing newlines are
required: `textMeasure` only adds height at a newline. The down triangle uses
seven narrowing fill rectangles in the watch's 2D pixel space, at y=201..208,
between the final option and the page buttons. No font glyph or texture is
needed for it.

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

## Validation

Run from the repository root:

```sh
python3 tools/tests/render_options/run.py
```

Optional address and undefined-behavior checks:

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/render_options/run.py
```

The suite checks all nine AA/VI pairs, existing save encoding/defaults, exact
render-mode restoration, segmented/nested display lists and cache invalidation.
It exercises all 12 watch selections, wrapping and edit controls. Actual Bank
Gothic metrics and production text measurement/layout check label sizing and
clipping. Emitted fill commands are checked for triangle shape, placement and
visibility at every scroll position. It also checks profiler removal and the
resident VI callback. Hardware and final rasterization are not emulated.

The affected C files compile to MIPS assembly with IDO 5.3 and US flags. Host
and sanitizer checks pass. A full ROM build and console verification of this
menu cleanup remain outside the development workspace.
