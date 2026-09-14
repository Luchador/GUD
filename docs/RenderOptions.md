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
| AA | Authored render modes and original lists; equivalent to the former Full setting. | Non-AA modes on supported opaque surfaces, plus one-cycle rendering for eligible opaque room backgrounds/world props and binary-alpha secondary room cutouts. |
| VI Filter | VI edge AA, divot correction and dedithering; equivalent to the former Smooth setting. | Those filters are disabled; the resampling needed for video output remains. |

Both default to On and are saved per folder. AA affects ordinary opaque
surface, terrain and decal modes, including asset lists. Blended translucency, particles and custom modes retain their authored AA
settings. Eligible secondary room cutouts use the non-AA path described below. AA Off therefore does not remove every AA operation in the scene.
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
existing pipelines. Characters, weapons, sky and the
watch are outside this conversion. Secondary rooms have the cutout path below. World props have the separate path described
below. Mixed lists switch state
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

## One-cycle opaque models

AA Off also selects cached alternate primary lists for ordinary world props,
including crates. Model types 2, 3 and 4 are eligible when using the normal
opaque prop material (render-data `PropType` 9, with a zero low environment
byte). Type 1 models already use one-cycle. The converter uses the existing
model setup functions to determine the incoming state, then converts only
recognized opaque draws within each list. Internal decals, cutouts and
translucency retain their original pipeline and depth behavior.

The normal prop lighting/distance fog uses `G_RM_FOG_PRIM_A`. Its constant
fog-alpha blend is retained in one-cycle with `FORCE_BL`, alongside the
original per-instance fog color. Base-tile sampling, bilinear/point filtering,
texture uploads, matrices, vertices and UVs follow the background conversion's
rules. The prop combiner's added primitive alpha is retained. Source commands
and outgoing material state are preserved, including before a secondary list.

Character blood tinting, fading props, deformed/cutout prop materials and
first-person weapon materials keep their existing rendering. Secondary model
lists, nested/branching primary lists and transient frame-buffer display lists
(such as animated monitor screens) are excluded. Original model lists remain
in place for collision and bullet hits; vertex changes are shared.

An alternate is built on first eligible use and reused across instances and
AA toggles. VI does not affect list selection. A 256-entry cache distinguishes
the incoming opaque material and Z-buffer mode; its alternate allocations are
capped at 64 KiB per stage, with about 4 KiB of cache metadata on N64. Allocation
uses the room heap and falls back to the original list if memory, cache capacity
or conversion is unavailable. This avoids reserving a large buffer up front.

Re-expanding an overlapping source list invalidates its entry. Old alternate
copies remain allocated until stage reset so queued graphics tasks cannot read
freed data. Stage initialization resets this cache after the old queue has
drained and the heap has been reset. This also bounds memory spent on reloads.

For hardware comparison, use the same AA-Off/VI setting and camera position
before and after this patch. Train's crate-filled cars are a useful first test.
Check crate labels, lighting, nearby doors/windows, damaged crates and distance
fades, then toggle AA repeatedly and restart the level. Distant textures may
shimmer more, as with one-cycle backgrounds.

## One-cycle secondary room cutouts

With AA Off, eligible surfaces in secondary room lists now use one-cycle alpha
cutout rendering. This first pass targets room geometry; transparent model
lists (including prop/door glass), effects and characters keep their existing
rendering. AA On selects the original room lists, and VI remains independent.

At texture load, the base image is classified as binary-alpha only if it has
both fully transparent and fully opaque texels, with no intermediate alpha.
The check supports RGBA, IA and their paletted formats, ignores row padding and
unused palette entries, and excludes RGB/intensity-only images and special
preswapped tile banks. It uses an existing spare descriptor bit, so descriptors
do not grow. Mips and texture payloads remain unchanged; there is no per-frame
texel scan or texture-number whitelist. Imported images follow the same rule.

An Auto draw is eligible only with a recognized Z-buffered translucent-surface or
texture-edge mode, a supported texture-alpha combiner, explicit environment
alpha 255, and a verified base-tile upload/format/palette. Explicit alpha
comparison, fades, decals, partial/custom uploads and unsupported combiners
remain on their original pipeline. Soft-alpha foliage is left blended along
with tinted glass unless explicitly set to Cutout in GEditor, as described below.

Converted draws use the base tile with authored bilinear/point filtering and
an alpha threshold of 128/255 (`BG_CUTOUT_THRESHOLD`). Surviving texels write
opaque color and Z; holes write neither. Fog retains the shade-alpha blender
with `FORCE_BL`, without reading framebuffer color. Alpha coverage selection
is disabled so the comparison sees texture alpha. Dither commands are retained.
The renderer establishes the threshold before calling the alternate, which
restores alpha comparison, cycle, LOD, combiner and render mode before skipped
draws and exits/calls. Unknown state after a nested call prevents conversion.

Optional secondary copies are allocated and freed with their rooms, after
collision bounds and primary copies. They add roughly the original secondary
list size plus transition commands; each failed allocation falls back to that
room's original list. Geometry, collision lists, bullet hits and light vertices
remain shared/unchanged.

For a hardware comparison, keep AA Off, VI and the camera position fixed across
builds. Try Jungle foliage and Runway/Depot fencing, including close views,
distant edges and overlaps with props. Check glass, moving doors, fog, broken
lights, AA toggles and room streaming. Hard edges and lost mip blending can
make thin/distant details disappear or shimmer. The existing FPS/bottleneck
indicator remains; no new watch option or profiler was added.

The host suite checks classification, RGBA/CI uploads, mixed cutout/glass
transitions, fog/depth, state restoration, allocation fallback, unloading and
AA/VI selection. It also checks 271 authored secondary streams across seven
levels with both environment LUTs, using representative binary/soft-alpha
texture metadata and upload packets. Original texture payloads are unavailable
here, so those asset checks establish candidate coverage, not which actual
images qualify or their appearance. Console validation remains necessary.

Host and address/undefined-behavior checks pass. Affected code compiles with
IDO 5.3 for the US N64 target. The texture module compilation uses a source
image-catalog fixture in place of the unavailable generated ROM-offset table;
a complete ROM build is blocked by missing base-ROM/texture assets here.

## Authoring background-face transparency in GEditor

Select background faces and use the Transparency dropdown in their Properties
panel. Choices apply to the whole selection and participate in undo/redo:

| Choice | Behavior |
|---|---|
| Auto | Retain the original material and allow eligible AA-Off optimizations. Returning from an explicit choice restores the material captured before the first override. |
| Opaque | Use an opaque surface, with the ordinary one-cycle optimization when eligible. |
| Cutout | Use texture alpha to discard holes and draw the remaining pixels as an opaque surface. Supported secondary-room materials use one-cycle thresholding with AA Off, even when the image has intermediate alpha. |
| Translucent (alpha blend) | Preserve blended transparency, including for binary-alpha images that Auto would otherwise convert to cutout. |

Explicit choices take priority over automatic classification. They are per
face: the same texture can be used by cutout and blended faces in one room.
The inspector shows the common choice, or Mixed when choices differ. Auto also
shows the underlying native transparency type. Mixed / Keep current leaves
the existing choices alone. Custom or unresolved native pipelines remain
read-only, as before.

AA On uses the authored native mode for each explicit choice; AA Off uses the
optimized version when supported. Explicit Blend stays blended with either
setting. Cutout does not guarantee one-cycle conversion: unsupported uploads,
combiners and other pipelines keep their native texture-edge mode. VI is
independent. The editor preview reflects the authored material, not the game's
automatic AA-Off conversion or distance filtering.

This control applies to background geometry and keeps faces in their current
primary/secondary layer. Put glass and other blended room geometry in the
secondary layer for the usual draw order. Secondary model lists remain outside
the new one-cycle cutout path; their existing model editing controls are unchanged.

Unmarked assets and older projects load as Auto. Previous files do not record
whether a native blend mode was an explicit editing decision, so reselect
Translucent (alpha blend) on faces that must remain blended. Selecting that
choice records an override even when the native mode is already blended.

The override and pre-override surface bits are stored in standard Fast3D
`gDPNoOpTag` packets, alongside the native partial render-mode commands. The
shared contract is `src/bgtransparency.h`. Texture expansion recognizes these
tags and passes them through without treating them as texture requests or
changing light-fixture boundaries. The metadata survives project save/reload
and native background compilation for ROM export, without a project format
change. Use the updated editor and game code together.

Tests cover mixed selections across rooms/layers, same-mode locks, repeated
overrides, Auto restoration in both cycle modes, save/reload/recompilation,
undo/redo and allocation failure. Runtime tests cover explicit Blend/Cutout/
Opaque precedence, neighboring Auto draws, AA preservation and actual texture
expander dispatch, including ordinary texture markers that resemble the tag.
Run `python3 tools/geditor/tests/bg_transparency/run.py` and the background
one-cycle suite below. Host sanitizer checks and US N64 object compilation
pass; a full ROM build and Windows viewport/console validation remain pending.

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
python3 tools/tests/model_onecycle/run.py
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

The model suite checks the production material setup, converter, cache and both
model-node submission paths. It covers incoming/outgoing state, AA/VI switching,
per-instance colors, dynamic and secondary exclusions, source replacement and
memory limits. It also checks 443 authored primary prop lists, including both
crate models placed in Train. Model and background sanitizer checks pass, along
with render-option and bullet-hit regressions; affected US N64 objects compile.
Model appearance and performance still require a console comparison.
