# Model one-cycle regression

## Opaque world characters

With **Anti-Aliasing Off** in the existing watch options, untouched opaque
character parts may use a one-cycle combiner. AA On retains the original
two-cycle character path. No new setting or ROM asset conversion is required.
This extends the current model cache and shares its 64 KiB allocation budget;
the extra per-entry vertex provenance costs 3 KiB of static cache metadata.

The new path keeps texture mip-level selection (the stock blood combiner
already samples one mip), texture filtering, fog/room lighting, depth and
authored culling. It removes the blood-tint stage only when the node still
uses its authored vertices and every referenced vertex has alpha 255.
Blood-stained parts, faded characters, translucent/cutout/decal materials,
detail/sharpen materials and unsupported display-list state retain two cycles.
Unchanged parts of a wounded guard can still qualify. GEditor-style untextured
parts retain the existing blood/death-fade repair on either path.

The removed stage multiplies by 255/256 rather than exact fixed-point one.
An exhaustive scalar RGB comparison bounds the combiner difference to one
8-bit channel level, before fog/dithering/framebuffer quantization. This is a
small precision tradeoff, not a claim of bit-identical final pixels.

Proofs and copies are cached per source list, depth mode and vertex provenance;
blood-buffer checks happen before lookup. All copies restore the original
outgoing pipeline and leave per-instance colours in the caller. The character
pass explicitly disables RSP fog, which otherwise overwrites vertex alpha.
An unsupported part that can alter RSP state, a secondary list or a special
draw stops further character conversion until the next character pass.
Source replacement retires copies without freeing graphics still in flight.
Allocation failure, full tables and memory-pressure reclaim use the existing
renderer, including its mandatory material repair.

Fixtures exercise 1,068 authored body/head lists: 1,054 lists and 7,393 triangle
packets qualify. CarmourguardZ converts all 25 lists / 131 triangle packets
before blood or fading. These are coverage counts, not frame-rate estimates.
First-person hands remain covered by the existing weapon tests.

For in-game comparison, use AA Off and a repeatable camera position facing
several guards. Check close/distant textures, room lighting/fog, wounded limbs,
death fades, untextured legs and held equipment. Compare against the previous
build with the same settings. This patch targets RDP pixel work; animation,
RSP vertex transforms and AI costs remain, so 30 FPS is not guaranteed.

Hardware references:
- [Nintendo blender documentation](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-07.html): one-cycle fog with opaque, non-AA rendering.
- [Nintendo tile selection documentation](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-07.html): LOD tile selection versus two-cycle trilinear interpolation.
- [libdragon mipmap modes](https://libdragon.dev/ref/rdpq__mode_8h.html): nearest-mip selection and interpolated mip modes.

## Regression coverage

Validated against GUD master `8090527` (LOD chr skips): the five changed game
translation units compile with IDO 5.3 for NTSC, the model suite passes normally
and under ASan/UBSan, and the BG one-cycle/cutout, character LOD and LOD-skip
suites pass. The BG harness also needed its allocator mock updated to the
current shared render-cache API.

The separate `render_options` suite fails its settings-save assertion in
`test_vi_and_save` on both this patch and the unmodified `8090527` sources.
Its settings implementation and tests are unchanged here. A complete ROM
build, hardware/emulator visual comparison and FPS measurement were not run.

Character material regressions also exercise GEditor-style switches between
textured and untextured legs, followed by another body part. They independently
decode the combiner and evaluate all 256 fade values against all 256 blood/vertex
alpha values. Both AA settings, live/dying instances sharing one model, native
segment-5 addresses, source immutability, outgoing combiner restoration,
unhandled-state cache invalidation, and frame-owned allocation fallback after
memory-pressure reclaim are covered. These tests do not render pixels on N64.

Run from the repository root:

```sh
python3 tools/tests/model_onecycle/run.py
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/model_onecycle/run.py
```

The harness compiles the production model material setup, alternate-list cache,
both model-node draw paths, shared converter and AA submission walker. It checks
opaque fog/depth state, internal decal/cutout/translucent exclusions, exact
outgoing state, per-instance colors, secondary calls, transient-list exclusion,
AA/VI queue gating, cache reuse, source replacement, pending-copy retention,
stage reset, allocation failure and both cache limits.

Damaged-prop checks independently evaluate the original two-cycle and emitted
one-cycle alpha muxes using nine-bit RDP arithmetic for all 32,768 combinations
of eligible environment alpha and vertex alpha. They verify the same alpha
and zero/nonzero hole mask, the base-texture/shade RGB mux, fog/depth state,
all four native damage levels, intact/damaged cache isolation, per-instance
damage/fog values, mixed-list restoration and unsupported-material fallback.
AA On must retain the original setup and list.

It also walks the repository's real prop models and checks their primary
display lists, including Train's wooden and metal crates. Both console2 and
console3 must convert all 20 body triangle packets at every native damage
level. Multi-byte GBI words
and pointers are adapted for the host. Asset texture markers become representative
upload commands; texture allocation, actual TMEM contents and rasterization are
not emulated. Hardware testing is still needed for appearance and frame rate.

Held-weapon tests cover the production first-person opt-in, room-light colour
and all 256 blend amounts, primary/secondary dispatch, mixed materials and
watch/casing/fading exclusions. Fixtures also walk gun assets and the unarmed
hand, including switch/LOD/BSP children. Unsupported model types retain their
existing paths.

Native segment-5 list tests resolve addresses through the actual node base,
check both draw-node formats, preserve segmented fallback, distinguish two
models with equal offsets, and exercise intact/damaged/weapon cache separation.
Reloads invalidate CPU address ranges while pending copies remain intact.
The host GBI adapter preserves segmented addresses instead of pretending every
input list pointer is already a RAM pointer.
