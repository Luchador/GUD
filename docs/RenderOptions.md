# Watch render options

Open the watch’s Options page and move down past Screen Ratio. The volume
sliders stay fixed; eight toggle rows scroll to keep the selection visible.
Navigation wraps through all 12 options. A green downward triangle appears
below the last visible row, above the page buttons, while more rows remain.
VI Filter is the final option.

AA and VI Filter display both **Off** and **On**, using the same text size,
positions, colours and selection behavior as Auto-Aim and Look Ahead. Press A
to select a row; left chooses Off and right chooses On. D-pad, C-buttons,
shoulder buttons and the stick work through the existing vanilla input code.
The active value is bright; the other value is dim. There are no value arrows.

| Option | On | Off |
|---|---|---|
| AA | Authored render modes and original room lists; equivalent to the former Full setting. | Non-AA modes on supported opaque surfaces, plus one-cycle rendering for eligible opaque room backgrounds. |
| VI Filter | VI edge AA, divot correction and dedithering; equivalent to the former Smooth setting. | Those filters are disabled; the resampling needed for video output remains. |

Both default to On and are saved per folder. AA affects ordinary opaque
surface, terrain and decal modes, including asset lists. Translucency,
texture-edge cutouts, particles and custom modes retain their authored AA
settings. AA Off therefore does not remove every AA operation in the scene.
VI Filter controls final video output independently.

Color Dither is no longer a setting. Each pass keeps its vanilla/authored RDP
RGB-dither and alpha-dither commands, including effects that deliberately use
noise or disable dithering. VI dedithering is a separate operation controlled
by VI Filter. The FPS counter and CPU/RSP/RDP bottleneck indicator remain.

## Save compatibility

The existing folder-options byte is retained. Bits 7..5 identify the format;
AA occupies bits 1..0 and VI occupies bits 3..2. Each field now writes 0 for On
or 2 for Off. Bit 4, formerly Color Dither Off, is cleared on the next save.

Previous Full/Reduced AA values load as On; previous Smooth/Edges VI values
load as On. Off stays Off. The former dither preference is ignored. Invalid
bytes fall back to On/On. The watch refreshes its values from the loaded
preferences, so opening the menu cannot overwrite newly loaded settings.

## One-cycle opaque backgrounds

AA Off selects an alternate primary display list built when each room loads.
Eligible opaque surfaces use one-cycle rendering, bilinear filtering (or point
filtering if authored), the primitive’s base texture tile and the original
Z compare/write behavior. Fog uses the one-cycle fog blender with `FORCE_BL`
and no framebuffer colour read. AA On selects the original room list.

Converted surfaces lose mip-level blending. Distant or oblique textures can
shimmer or alias more, especially floors. Perspective correction, UVs, vertex
colouring and geometry remain intact. Texture uploads still load the original
mip chain; the optimization targets pixel processing, not upload traffic.

Only recognized opaque surface/terrain modes and simple textured or shaded
combiners in primary room lists are converted. Cutouts, decals, translucency,
detail/sharpen combiners, animated water and custom materials retain their
existing pipelines. Secondary room lists, objects, characters, weapons, sky
and the watch are outside the one-cycle conversion. Mixed lists switch state
around eligible draws and restore the original AA-Off state at exits/calls.
State inherited from a called list is treated as unknown until established.

Conversion follows texture expansion and the environment material LUT. It
uses the same Off mapping as the AA submission pass, tracking full and partial
state writes to preserve GEditor overrides. Both list selection and command
rewriting use the applied AA setting, after pending graphics tasks drain.

Original lists remain at their original addresses for collision, bullet hits
and light fixtures. Both lists share vertices and textures, so broken lights
update the same vertex colours. The alternate adds roughly another primary
list’s worth of room memory plus inserted commands, freed on room unload. If
allocation or conversion fails, that room uses its original list. Collision
cache allocation takes priority. Room streaming should be checked alongside
stationary performance tests because the alternate consumes extra memory.

## Rendering implementation

The public AA/VI settings and their applied copies are booleans. The submission
walker handles only AA render-mode changes; it no longer examines or rewrites
SetOtherMode H commands for dithering. An already-tagged AA-Off command returns
immediately. AA On restores tagged commands to their exact authored modes.
No Reduced mode calculation or color-dither restoration metadata remains.

The walker follows Fast3D calls, branches and segments with a bounded stack
and command budget. Its 1,024-entry cache skips processed static leaf lists;
dynamic graphics/vertex pools are always scanned. AA changes, texture loads,
stage-pool initialization and background material updates invalidate the cache.
VI-only changes leave it intact because they do not change display lists.

AA restoration tags occupy unused bits 16..23 of Fast3D SetOtherMode L’s first
word. They identify the authored opaque mode and first blender so restoration
is exact even when multiple authored modes share an Off equivalent. Tags live
only in RAM and are removed before exact-command LUT processing. Original
assets and microcode are unchanged. Revisit this encoding when changing
microcode families.

The render settings module’s text remains in resident `.code`. VI retraces
start before demand paging is initialized; `renderConfigureViMode` must remain
resident. The existing linker assertion and queue-drain protections remain.

## Validation

Run from the repository root:

```sh
python3 tools/tests/render_options/run.py
python3 tools/tests/bg_onecycle/run.py
```

Either suite supports address/undefined-behavior checks, for example:

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/render_options/run.py
```

The settings suite checks all four AA/VI combinations, every possible save byte,
legacy migration, queue gating, exact AA restoration and byte-for-byte dither
preservation. It exercises real vanilla toggle drawing/input, the actual row
table, English strings and Bank Gothic metrics: all 12 rows, scrolling, value
placement/highlights, controller button masks, clamping and sound gating.
It also checks display-list nesting/cache behavior and the resident VI hook.

The one-cycle suite checks production conversion and room lifecycle code,
including fog, depth, excluded materials, partial overrides and outgoing state.
It checks 464 authored primary streams across seven levels with both primary
material LUTs. These asset checks substitute texture markers/water calls; they
do not emulate texture allocation or rasterization. Host and sanitizer checks
pass, and affected modules compile with IDO 5.3 for the US N64 build. A complete
ROM link and console verification of this menu/settings cleanup remain pending.
