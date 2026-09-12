# Temporary N64 load diagnostics

## Restore full rendering (12F)

`GUD-n64-restore-rendering.patch` targets master `ced387d1`
(Color buffer alignment fix). Use the normal US build.

### 11A hardware result

The user reports no crashes while viewing and manipulating the controller,
exiting and loading multiple levels, or viewing pause-menu items including
the PP7 and Sniper Rifle. The aligned color framebuffers therefore pass the
same controller configuration that previously stalled with misaligned
buffers. This strongly supports framebuffer alignment as a cause of the
observed hangs. Full world rendering and normal controller depth comparison
were still bypassed, so those paths need verification before the hardware
investigation is considered complete.

### What 12F changes

All rendering-isolation switches now default to zero. This restores:

* Normal sky/cloud/water rendering and room display-list submission.
* World props, characters, effects, and the ordinary weapon/watch draw path.
* Normal interpolated controller depth, depth comparison, and depth writes.

The fill-only RDP probe remains disabled. `N64_DIAG_RESTORE_WEAPONS=0` only
disables its special HUD-isolation path; with `N64_DIAG_HUD_ONLY=0`, weapons
and the watch render through their ordinary path. The framebuffer alignment,
compile/link guards and preflight alignment checks remain. The watchdog,
six-page capture and existing RSP/RDP profiler remain enabled, independently
of DEBUG. Crash pages identify this configuration as **12F**.

### Hardware test

1. Apply against `ced387d1` and run `make VERSION=US`.
2. Check that room geometry, characters and sky are visible in the emulator.
3. Run the new ROM on N64. Test Runway and Cradle first, including the
   controller fade/manipulation/re-entry, pause-menu item models and combat.
4. Switch levels and try the other previously profiled scenes: Surface 1,
   Jungle and Streets. A short split-screen test also exercises shared depth
   storage and viewport changes.
5. If it stalls, send all six 12F pages and the matching ELF/map. If it passes,
   the next cleanup can remove the temporary isolation scaffolding and turn
   off the expensive command preflight before collecting performance results.

Validation: all 14 consumers of the diagnostic configuration compile with
normal US IDO 5.3. The diagnostic/preflight and fill-probe host suites pass.
The restored `gunfire`, `sky` and `bondview` objects have identical code,
read-only data, data and BSS to builds with diagnostics disabled; `lvRender`
also has identical compiled instructions. This confirms that the temporary
rendering overrides are absent while retaining the diagnostic infrastructure.
No full ROM build, emulator run or N64 test was performed here.

## Framebuffer alignment fix (11A)

`GUD-n64-framebuffer-alignment.patch` targets master `8e52a1cc`
(Depth test 2). Use the normal US build. The controller rendering settings
remain those of 10R: constant primitive depth, comparison disabled, ordinary
depth-write enables retained. World rendering remains bypassed.

### 10R hardware result and concrete defect

The controller appeared for about half a second before the watchdog fired.
All six `20260912_130412.jpg` through `20260912_130450.jpg` photos show 10R.
The matching `GUD(10).elf` places `osRecvMesg` at `7000D240`; main PC
`7000D2A8` is inside that wait. SP PC `1C4` is the FIFO-space wait, with
pre-capture SP status `C0` and DP status `760`. The CPU exception fields are
zero. This remains an RCP stall, not a captured CPU exception.

Page 4's matched-task snapshot reports depth image `0023E540`, color image
`003DA150`, width `140` (320), mode `0C19286C`, zero Z-compare draw commands,
and `136` (310) depth-write-enabled GBI draw commands. These are command-walk
observations, not proof that those writes completed or the exact stalled
primitive's state.

The color image is only 16-byte aligned. Nintendo's
[gDPSetColorImage reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gdp/gDPSetColorImage.html)
requires **64-byte alignment for both color and depth images**. The captured
depth address satisfies that requirement; the color address does not.

The ELF/map confirms the source of this defect:

| Allocation | 10R address/size | After alignment fix |
|---|---|---|
| Stacks | `803AB400`, size `9550` | Unchanged |
| First color framebuffer | `803B4950` | `803B4980` |
| Second color framebuffer | `803DA150` | `803DA180` |
| End of both framebuffers | `803FF950` | `803FF980` |

Commit `12b520e7` removed the unused `sp_debug[0x6B0]` stack. Previously that
stack made the total stack size `9C00`, placing the following framebuffers at
`803B5000` by coincidence. The old normal US build enabled `LEFTOVERDEBUG`,
so this dependency also affected normal builds. The linker never explicitly
aligned the framebuffer section. Preserve the unused-stack removal and fix
that implicit layout dependency instead.

### What 11A changes

* Align `.cfb` to 64 bytes in the linker script. The 48-byte padding leaves
  both framebuffers below `80400000` in the original 4 MiB region.
* Assert framebuffer alignment and the 4 MiB upper bound at link time, and
  assert the per-frame stride is a multiple of 64 at compile time.
* Reject known misaligned color/depth image addresses in the existing
  preflight. Check resolved addresses, including segment bases; unknown
  segments remain unknown. This is not a general image-bounds validator.
* Change the current diagnostic title to **11A**. Do not change the
  controller modes, allocation, geometry, or rendering-isolation switches.

This fixes a demonstrated hardware requirement violation. Whether it accounts
for every observed hang still requires a hardware test. The delayed failure
is consistent with the controller fade reaching a depth-writing pass, but
these captures do not prove that exact transition caused the stall.

### Hardware test and validation

Apply against `8e52a1cc`, then run `make VERSION=US`. Run the newly generated
ROM. Open the controller page, let the fade finish, manipulate it, leave and
reopen it. If it stalls, send all six 11A pages and the matching ELF/map.
If it passes, restore normal controller depth and world rendering in the
next controlled test; this patch deliberately retains the current isolation.

Validation: normal US IDO 5.3 compilation of `cfb`, `stacks`, diagnostics and
preflight; diagnostic/preflight and fill-probe host suites; GNU ld layout
test using the production stack/framebuffer script fragment and host fixtures
derived from the native objects' symbols, sizes and alignment. The layout
test confirms the addresses above and that deliberately misaligned or
overflowing placements fail the new assertions. No full ROM build, emulator
run or N64 test was performed here.

## Controller depth-comparison test (10R)

`GUD-n64-controller-depth-compare.patch` targets master `75e4bff4`
(Depth buffer test). Use the normal US build, not DEBUG. This is a diagnostic
experiment; correct world rendering is still bypassed.

### 09P hardware result

The controller page still stalled. All six `20260912_124125.jpg` through
`20260912_124205.jpg` photos show **09P**, and `GUD(9).elf` contains the
primitive-depth commands in `watchRenderController` plus the restoration to
pixel depth after both controller draws.

| Evidence | Interpretation |
|---|---|
| Main PC/RA `7000D0C8`, no fault thread | Main is waiting inside `osRecvMesg` (`7000D060` in this ELF), not a captured CPU exception. |
| SP PC `1B8`, pre-capture status `C0`, DP status `760` | The graphics microcode is waiting for FIFO space while the RDP remains busy. |
| DP current `002C5000`, write/end `002C4FE0`, pending output `B0` bytes | The pending output crosses the read cursor. Preserve the existing FIFO guard. |
| RSP saved mode low `0C19287C` | `G_ZS_PRIM` (`04`), `Z_CMP` (`10`) and `Z_UPD` (`20`) are all set. This is producer state, which may be ahead of the stalled RDP primitive. |
| RAM input `0021A848..0021A85F` | Matches the controller asset at `4C70..4C87`: `B100001C 0000F3DE`, `04F00100 05001DD8`, `B1003322 45040310`. |

Together with 08Z passing, 09P failing deprioritizes the controller's
interpolated Z/deltaZ values as a necessary cause. It still does not prove an
invalid depth allocation, a broken RDP, or a defect in the original microcode.
The raw FIFO window is reused ring memory, not a trustworthy chronological
trace of the exact primitive that first stopped the RDP.

### What 10R changes

Keep 09P's constant primitive depth, geometry, textures, draw calls, allocation,
clear and sync commands. Keep selecting the controller's usual Z-enabled
render modes. After building both body/button command ranges, clear only
`Z_CMP` in each complete render-mode setter in that newly generated range.
`Z_UPD` remains enabled wherever the ordinary mode enables it. Blending,
coverage, cycle type, depth source and the other mode bits are preserved.

The helper recognizes GoldenEye's `B900031D` render-mode command and a full
`EF` other-mode setter. It does not rewrite model asset memory, walk arbitrary
memory, or change other command payloads. Source inspection finds 13 controller
lists, all model type 3, with no nested display-list calls or B9/EF setters.
Their C0 texture expansion path also does not emit render-mode setters.
Both animated-buttons paths are included. A model-not-ready return occurs
before any override or scan. The extra CPU scan can change timing.

`N64_DIAG_CONTROLLER_NO_ZCOMPARE=1` applies only with diagnostics enabled,
probe off, HUD isolation and weapon restoration on, and primitive depth on.
The title becomes **10R**. Set this new flag to 0 for the 09P rendering control;
the added address diagnostics remain. To repeat 08Z, also set primitive depth
to 0 and retain `N64_DIAG_CONTROLLER_NO_ZBUFFER=1`.

### Depth information on page 4

The preflight now records image addresses using the same segmented-address
translation as the graphics microcode. The new fields belong to the matched
running task, unless the page explicitly says its list was rejected.

| Field | Meaning |
|---|---|
| Z IMG / COLOR | Physical depth/color image addresses at the last depth-enabled GBI draw encountered by the walk. |
| WIDTH / Z MODE | Color-image width (also the depth-buffer stride), and mode low at that draw. |
| Z CMP / Z UPD | Counts of GBI draw commands with comparison/writes enabled. TRI4 counts as one command; this does not count pixels or prove writes completed. |
| Z CMD / Z DRAW | Address of the depth-image setter used by that draw, and the draw command itself. |

Later non-Z HUD commands do not replace that snapshot. `FFFFFFFF` denotes
unknown/unseen state. The preflight can only count draws once both depth-enable
bits are known. A partial walk is incomplete. These fields are CPU command-stream
evidence, not RDP register readback or proof of allocation ownership, buffer
bounds, cache coherence, clipping or memory lifetime. Existing preflight hazard
checks are unchanged; the new fields do not add a submission gate.

### Hardware test

1. Apply against `75e4bff4`, then run `make VERSION=US`.
2. Check the controller screen in the emulator. Wrong overlap/occlusion is
   expected with comparison disabled and constant depth.
3. Run the newly built ROM on N64. Open the controller page, let its fade
   complete (opaque passes retain Z writes), manipulate it, then leave and
   return. Also try the second controller layout if available.
4. If it stalls, send all six **10R** pages and the matching ELF/map. Page 4
   should have zero Z CMP commands in this isolation configuration. Nonzero
   Z CMP means the intended comparison was not fully isolated and needs tracing.

If it passes, investigate depth comparison/read access and rejection-related
state/timing next; an opaque pass can then complete with its depth-write enable
retained. If it fails with zero Z CMP, comparison is not required for that
failure. Inspect the captured target addresses/stride and investigate depth
writes or other preserved controller state. Neither outcome is a finished fix.

Validation: normal US IDO 5.3 compilation of gunfire, diagnostics and preflight;
existing diagnostic/preflight and fill-probe host suites, including exact-bit
masking and depth snapshot retention/unknown-state checks. The new flag off
reproduces 09P's gunfire code/data/BSS. No full ROM, emulator or N64 run here.

## Controller primitive-depth test (09P)

`GUD-n64-controller-primitive-depth.patch` targets master `a8551f7e`
(Zbuffer test). Use the normal US build. This remains an investigation, not
a confirmed crash fix.

### 08Z hardware result

The user reports that the controller is visible and can be manipulated,
left, reopened and manipulated again without a crash. The previous 07W
controller page stalled with its Z-enabled model render modes. The 08Z
change leaves the Z-buffer allocation, address setup and clear active, as
well as the controller's model loading, transformations and triangle output.
It selects the existing non-Z RDP render modes for the controller.

That is strong evidence for the depth-enabled drawing path. It does not
establish a defective allocation or identify the exact hardware hazard.
Disabling depth access also changes rejection, work and timing. Emulator
success still cannot establish whether the same stream is safe on hardware.

Source review found the normal buffer allocation includes 64 bytes of
alignment slack, rounds its address to 64 bytes, and uses a 16-bit clear with
NOOP render mode and pipe synchronization. `zbufInit` sets the depth image
after a pipe sync. In the ordinary 320x240 single-player configuration, the
nominal dimensions agree with the framebuffer. These checks do not prove
the actual runtime addresses or memory lifetime are correct.

### What 09P changes

Re-enable the controller's original Z-enabled render modes, while selecting
`G_ZS_PRIM` with `gDPSetPrimDepth(0x4000, 0)`. This makes the RDP use a fixed,
valid depth and deltaZ instead of the interpolated per-pixel depth generated
from the triangles. Normal model render-mode selection still controls depth
comparison and writes: translucent passes compare, opaque passes can also
write. Both the controller body and separately animated buttons share the
override. Geometry-mode Z stays enabled, so the RSP still generates the
triangle depth coefficients; the RDP ignores those coefficients for depth
comparison while the override is active.

The existing depth image, allocation, clear, textures and transformations
remain. Pipe syncs bracket the override. The routine restores `G_ZS_PIXEL`
after both draws, including when separate button animation is disabled.
The initial model-not-ready return occurs before any override is emitted.
The extra synchronization and resulting timing are part of this test.

`N64_DIAG_CONTROLLER_PRIMITIVE_Z=1` takes precedence over
`N64_DIAG_CONTROLLER_NO_ZBUFFER` only in the existing weapon/watch isolation
configuration. No world/room rendering is restored. The diagnostic title is
**09P**. Set the new switch to 0 to repeat **08Z**, leaving the no-Z switch at
1. Setting both controller switches to 0 restores the original **07W** test.

1. Apply against `a8551f7e` and run `make VERSION=US`.
2. In the emulator, confirm the controller still draws. All its parts now
   share one depth, so overlap/occlusion can be wrong; visual correctness is
   not the pass criterion for this test.
3. Test the newly built ROM on N64: open the controller page, wait for its
   transition to finish, manipulate it, then leave and reopen it.
4. Report whether it stays responsive. On a stall, photograph all six
   **09P** pages and send the matching ELF/map.

If it passes, the controller's Z-enabled modes can complete with a fixed
depth. Prioritize interpolated Z/deltaZ, projection/viewport depth setup,
depth-dependent rejection and synchronization/timing differences. It would
not prove a microcode defect by itself. If it fails, the original triangle
Z/deltaZ values are not required to reproduce the failure; prioritize depth
image address/lifetime and Z-enabled render-state interactions next.

Nintendo documents this depth-source choice and the primitive register
format in [gDPSetDepthSource](https://ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSetDepthSource.html)
and [gDPSetPrimDepth](https://ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSetPrimDepth.html).

Validation: the changed functions compile with US IDO 5.3. Native instruction
inspection confirms the primitive depth/source commands, enabled model Z
flag and restoration to pixel depth. None of the controller asset's 13 lists
overwrites the depth-source bit. With the new switch off, `gunfire.o` and
`n64diagnostics.o` reproduce master 08Z code/data/BSS exactly. Both existing
diagnostic/preflight and fill-probe host suites pass. No full ROM, emulator
or hardware test was performed here.

## Controller depth-access comparison (08Z)

`GUD-n64-controller-zbuffer-isolation.patch` targets master `aa9a80ab`
(Restore weapon and watch rendering). It uses the normal US build. This is
an isolation test, not a confirmed fix or a replacement for correct depth
buffering in the finished game.

### 07W hardware result and matching files

The user can see and fire the first-person weapon, create visible casings,
open the watch, and display the PP7 inventory model without a crash. Opening
the watch controller page triggers the watchdog. `GUD(8).elf`/map and the six
`20260912_120346.jpg` through `20260912_120424.jpg` photographs match **07W**.

| Observation | Meaning |
|---|---|
| Main PC/RA `0x7000D0C8` | Inside `osRecvMesg`, whose ELF entry is `0x7000D060`: the main thread is waiting. The watchdog reports no captured CPU exception. |
| SP PC `0x1B8`, SP status `0xC0` before capture | Graphics microcode is in its FIFO free-space wait. The later halt is diagnostic capture, not an original SP break. |
| DP current `0x002C5000`, producer write/end `0x002C4FC0`, pending bytes `0xB0` | The pending 176-byte output would cross the RDP read cursor. Waiting is correct; do not relax the FIFO full/empty guard. |
| Captured RAM input `0x0021A918..0x0021A92F` | Matches controller asset commands at file offsets `0x4C70..0x4C87`: `B100001C 0000F3DE`, `04F00100 05001DD8`, `B1003322 45040310`. |
| Controller asset in the uploaded ELF | Byte-identical to current `assets/obseg/gun/GjoypadZ.bin`. The input match identifies drawing of the loaded controller, not a loader exception. |
| Saved RSP other-mode low `0xC41049D8` | Includes depth comparison. It is the producer's saved state and can be ahead of the RDP's actual stalled primitive. |

The raw FIFO window may contain reused ring-buffer data. Neither the current
TRI4 input nor the saved other-mode register identifies the exact command
that originally stopped the RDP. Passing the known-hazard preflight is also
not proof of complete command-stream correctness.

### Why test depth access

`watchRenderController` explicitly sets `renderdata.zbufferenabled = TRUE`.
The working PP7 preview in `watchRenderItemModel`, first-person gun drawing,
and casing drawing explicitly disable depth buffering. The controller and
PP7 use the same hand-model loader and model draw machinery. This makes
depth access a concrete comparison, while still leaving controller-specific
textures, geometry, state transitions and timing as other candidates.

`N64_DIAG_CONTROLLER_NO_ZBUFFER=1` selects the existing non-Z model render
modes only within `watchRenderController`. Both the main controller and its
separately animated buttons use the same renderdata. Loading, vertex data,
matrices, texture setup, transparency/colour selection and draw calls remain
in place. No geometry-mode Z bit is removed. The common Z-buffer allocation,
address setup and clear remain, as do the world bypass and 07W weapon setup.

The switch takes effect only with diagnostics enabled, the fill probe off,
HUD isolation on, and weapons restored. The diagnostic title is then **08Z**.
It has no effect on the ordinary renderer outside that isolation setup.

1. Apply against `aa9a80ab`, then build with `make VERSION=US`.
2. Check that the controller still appears in the emulator. Surfaces and
   buttons may overlap incorrectly without depth testing; this is expected.
3. Run that newly built ROM on N64. Load Runway, open the watch controller
   screen, let its transition finish, and exercise the stick/buttons. Also
   leave and reopen the page. Check Cradle if Runway stays stable.
4. Report whether the controller appears and whether the page still stalls.
   On a stall, send all six **08Z** pages and that ROM's matching ELF/map.

If 08Z works, the controller can render through this path without depth
access; investigate depth-buffer state/address/lifetime and the Z-enabled
render modes next. It does not by itself prove that the Z-buffer allocation
is wrong, nor establish that every original world crash has the same cause.
If 08Z still stalls, depth access by the controller is not required for that
failure; retain the model-specific texture/geometry/state investigation.

Set `N64_DIAG_CONTROLLER_NO_ZBUFFER=0` and rebuild to repeat the **07W**
control. The Makefile now rebuilds `gunfire.o` when diagnostic switches change.

Validation: US IDO 5.3 compilation succeeds. Native function comparison finds
only `watchRenderController` changed in `gunfire.o`; its depth-enable store
becomes zero. With the switch off, `gunfire.o` and `n64diagnostics.o` have
identical code/data/BSS to master. Existing diagnostic/preflight and fill-probe
host suites pass. No full ROM, emulator or hardware run was performed here.

## Weapon/watch drawing after the successful HUD test (07W)

`GUD-n64-weapon-isolation.patch` targets master `76664039` (HUDtest).
It keeps the normal US build and is an isolation test, not a confirmed fix.

### 06H hardware result

The user reports no crash with the HUD, ammo counter, health and armor gauges
visible. Pausing, watch-menu navigation, firing and audible guard gunfire
also worked. `GUD(7).elf`/map match the intended **06H** configuration:

| Item | ELF evidence |
|---|---|
| Diagnostic title | `N64 DIAG 06H` at `0x8002263C`. |
| World/effects bypass | `lvRender` at `0x7F0AF6A0` contains the no-push branch construction at `0x7F0AF9F8`. |
| Weapon/watch bypass | `bondviewRenderPlayerView` at `0x7F08164C` contains the no-push branch construction at `0x7F0816F4`. |
| Fill-only probe | `n64RdpProbeBuild` is absent; `rspGfxTaskStart` has the ordinary submission path. |
| Solid sky | `skyRender` is still the 0x60-byte bypass implementation. |

This demonstrates that the common frame setup and clears, HUD texture/text
drawing and gauge triangles can complete together in the tested situation.
It does not certify every texture, triangle, synchronization or timing case.
The successful pause/menu interaction does not undo the compiled branch
around `bondviewRenderWatch`; its CPU work still ran during 06H.

### What 07W changes

`N64_DIAG_RESTORE_WEAPONS` defaults to 1 in `src/n64diagnostics.h`. With HUD
isolation active and the fill probe off, the weapon/watch branch becomes an
RSP no-op. Its reserved command slot is retained. The RSP now executes the
normal casing, first-person weapon/beam and watch drawing commands. The
world/effects branch in `lvRender` is unchanged, so rooms, props, characters,
world tracers, sparks, glass and explosion debris remain invisible. Their
CPU work continues, as in 06H.

The world branch also skips `bgSetupAndRender`'s shared state commands.
Before the restored weapon range, `bgSetupWeaponDiagnostic` recreates its
light/look-at setup, player scissor, environment fog parameters/alpha dither,
fog-disable state, projection matrix and player model-view matrix. It emits
no room, prop or world-effect draw calls. Weapon/model renderers retain their
own cycle, combiner and render-mode setup. This added state setup is part of
the test, so a failure alone would not prove that a particular gun asset is
bad. Frozen intro-camera views still take their original HUD-only path and
do not execute the restored weapon range until normal player view begins.

1. Apply the patch against `76664039` and run `make VERSION=US`.
2. In the emulator, confirm the weapon is visible once player control begins,
   with HUD/profiler text over the solid background and no world or guards.
3. On N64, test Runway and Cradle: reach player control, aim, fire, reload,
   then pause and switch watch pages. Report which stages were tested and
   whether any failure occurs at loading, weapon appearance, firing or pause.
4. If it stalls, send the **07W** diagnostic pages and matching ELF/map.

If 07W succeeds, world/prop/character/effect drawing and its interaction with
the rest of the frame become the next focus. If it fails, split the restored
setup, weapon, casing and watch paths further before blaming an asset.
Both results still allow timing-dependent interactions.

To repeat 06H, set `N64_DIAG_RESTORE_WEAPONS=0` and rebuild. Leave
`N64_DIAG_HUD_ONLY=1` and `N64_DIAG_RDP_PROBE=0`. The new switch is ignored
when HUD isolation is off or the fill-only probe is on. The earlier room-list
and solid-sky switches retain their existing settings.

Validation: US IDO 5.3 compilation succeeded for the changed code and the
unchanged world-bypass caller; the three existing `bg.c` floating-point
warnings also occur on master. Both diagnostic/preflight and fill-probe host
suites pass. Native instruction/relocation inspection confirms the world
branch remains, the weapon branch is removed, and setup precedes all restored
draw calls. With `N64_DIAG_RESTORE_WEAPONS=0`, `bg.c`, `bondview.c` and the
diagnostic module have identical code/data/BSS to master 06H. No full ROM,
emulator or N64 test of 07W was performed here.

## HUD and frame-setup isolation after the successful 05P test (06H)

`GUD-n64-hud-isolation.patch` targets master `e27f805e` (Latest diagnostics).
It uses the normal US build. This is the next isolation test, not a crash fix.

### Hardware result to preserve

The user corrected the latest test report: photos `20260912_105625.jpg`
through `20260912_105705.jpg` came from an older **04BS** ROM. They must not
be interpreted using `GUD(6).elf`/map, which contain the **05P** title and an
active call to `n64RdpProbeBuild` in `rspGfxTaskStart`.

With the new ROM, Runway loaded, the screen reached green with a moving white
marker, and Bond could move and fire. A guard's gunfire was audible, with no
crash reported. Green begins at submission 240, after repeated tasks whose
fill/sync output exceeds the 40 KiB FIFO. This shows that stage work, audio
and repeated FIFO-wrap workloads can coexist successfully in this test.
It does not prove all audio-yield timings, buffer lifetimes or ordinary scene
commands are safe. The earlier sky-only and room-drawing bypasses still
failed; concentrate next on remaining scene commands and their render state.
Do not change the FIFO free-space guard on the strength of these results.

### What 06H submits

The patch defaults `N64_DIAG_RDP_PROBE` to 0 and adds `N64_DIAG_HUD_ONLY=1`.
It submits the ordinary frame list again, with two no-push display-list
branches per unfrozen player view:

- In `lvRender`, jump over the list generated by `bgSetupAndRender` and the
  following tracers, sparks, glass and explosion-debris draws. This includes
  props, characters and world effects, as well as their world render state.
- In `bondviewRenderPlayerView`, jump over casing, first-person weapon and
  watch drawing, including UI drawn on the watch itself. Intro-camera views
  already omit these draws and still follow their normal HUD path.

Both ranges are still generated in their original buffer. Their CPU calls,
room loading, render-time updates and dynamic allocations continue. Nothing
rewinds the display-list pointer or overwrites the skipped commands. Each
branch is patched to the end of its range before the ordinary preflight,
cache writeback and submission. It does not push an RSP return address.

Normal pipeline setup, Z-buffer clear, framebuffer setup and the solid sky
clear remain, followed by HUD text, ammo, sights, gauges, fades, timers and
the existing profiler. HUD gauges can use triangles: this separates drawing
paths, not every triangle opcode from every rectangle opcode. The earlier
room-list and solid-sky bypass switches remain enabled. GFX/profiler command
counts still include CPU-generated commands that the RSP skips.

1. Apply against `e27f805e` and run `make VERSION=US`.
2. In the emulator, load Runway. Expect the ordinary HUD/profiler over the
   solid background, with no world, characters or first-person weapon.
   There should be no green probe screen or scrolling probe marker.
3. Test that same ROM on N64: reach player control, move, aim and fire, then
   try Cradle. The watch's visual UI is deliberately skipped during this
   test, so use ordinary gameplay for the first comparison.
4. Report whether each stage loads and whether it fails later when aiming,
   firing or taking damage. If it stalls, photograph the **06H** diagnostic
   pages and send that ROM's matching ELF/map.

If 06H passes, reintroduce world and weapon draws separately next; their
commands, state changes and timing remain candidates. If 06H fails, the
common frame setup, clears and remaining overlays are the next group to
split. Neither outcome alone identifies a bad texture or primitive.

To repeat the successful control, set `N64_DIAG_RDP_PROBE=1`; it takes
precedence and disables both branches, restoring the **05P** test. To return
to the earlier **04BS** configuration, set both `N64_DIAG_RDP_PROBE` and
`N64_DIAG_HUD_ONLY` to 0. To restore all normal drawing with diagnostics,
also set `N64_DIAG_SKIP_BG_GDLS` and `N64_DIAG_SOLID_SKY` to 0.

Validation: the affected render functions, submission path, diagnostics and
probe object compile with IDO 5.3 for US. The existing watchdog/preflight and
fill-probe host suites pass. Native instruction inspection confirms the
render-time CPU calls remain and both branch targets use the returned end
pointer. With `N64_DIAG_HUD_ONLY=0` and the probe off, `lv.c` and `bondview.c`
code/data match current master with the same probe setting. The patch adds
the missing diagnostic-header dependency for `bondview.o`. No complete ROM,
emulator or N64 run of 06H was performed here.

## Fill-only submission and FIFO-wrap probe (05P)

`GUD-n64-rdp-probe.patch` targets master `7767ff96` (Room geometry test).
The latest 04BS hardware test still stalls with both room display lists and
sky/water drawing omitted. This next test replaces gameplay graphics-task
input with a small, known fill-only display list, then increases its output
past the size of the existing FIFO. It is not a confirmed crash fix.

The normal US build is used; DEBUG is not required. Enable/disable with
`N64_DIAG_RDP_PROBE` in `src/n64diagnostics.h` (default 1). Menus still draw
normally. Once a level loads, the CPU continues its existing stage work and
builds its usual frame, but `rspGfxTaskStart` submits the probe instead. Its
framebuffer, scheduler, audio yielding, microcode, cache writeback, FIFO and
completion messages use the normal path. There is one private probe list per
existing graphics-task slot; the slots are not overwritten together.

| Screen | Submitted workload |
|---|---|
| Blue | First 120 gameplay submissions: a full-screen clear and a small moving white marker. The short command stream does not force a FIFO wrap within the task. |
| Amber | Next 120 submissions: the same clear/marker plus 24 calls to a block containing 128 sync/fill pairs. Those pairs alone produce 49,152 RDP bytes, exceeding the 40 KiB FIFO within one task. |
| Green | Continues the same wrap workload after reaching 240 submissions. The white marker keeps moving while frames progress. |

All drawing uses 16-bit RGBA FILL with NOOP render mode, a full-screen
scissor starting at zero, and no texture loads, vertices, triangles, image
reads or Z-buffer writes. Every small repeated fill is synchronized and has
a four-pixel-aligned left edge. The calls have a stack depth of one and only
the top-level list emits FullSync. Segment 0 is explicitly zeroed. Other-mode
commands update the microcode's saved state for audio yield/resume. Private
arrays include padding for the microcode's input read-ahead.

1. Apply against `7767ff96`, then build normally with `make VERSION=US`.
2. Check in the emulator that menus work and loading a level produces
   blue, then amber, then green with the moving marker. The gameplay view
   and profiler overlay are intentionally absent during this probe.
3. Repeat the failing stage load on N64 and wait for green or a diagnostic.
   Report the level, visible colour progression and whether the marker stops.
4. If it stalls, send pages 1-3 (title **05P**) and this build's ELF/map.
   Page 2's GDL SIZE distinguishes the running short list (`0x78`) from the
   wrap list (`0x138`). The last visible colour may belong to the preceding
   completed frame, so it is not sufficient by itself to identify the task.

A short-list failure removes scene-command execution and within-task FIFO
wrap as requirements for that failure, but does not eliminate initial FIFO
setup or stage-loading side effects. A wrap-list failure after short frames
complete puts additional focus on FIFO transfer/ownership and shared memory.
Reaching green demonstrates that repeated wrap workloads can complete in
this configuration; it does not prove the normal workload's timing, graphics
state or buffer lifetimes are correct. Restore `N64_DIAG_RDP_PROBE=0` for the
04BS control. To restore normal scene drawing too, set both earlier bypass
switches (`N64_DIAG_SKIP_BG_GDLS`, `N64_DIAG_SOLID_SKY`) to 0.

### Evidence from the supplied 04BS build

Photos `20260912_063531.jpg` through `20260912_063610.jpg` show Runway
(stage `0x23`). In `GUD(5).elf`, neither room-rendering function emits its
room display-list call, and `skyRender` uses the solid-background bypass.

| Observation | Interpretation |
|---|---|
| WAIT FRAME, MAIN PC `0x7000D0C8` | `osRecvMesg + 0x68` in this ELF; no CPU exception is reported. |
| SP PC `0x1BC`, instruction `0x0277A022` | Still in the FIFO free-space loop. |
| DP STATUS `0x760`, CURRENT `0x002BF230`, END/WRITE `0x002BF228` | Same class of stalled graphics progress with both bypasses active. |
| R18 `8`, R19 `0x002BF228`, R23 `0x002BF230` | The pending write would end exactly at CURRENT; the guard correctly withholds it to preserve the ring's full/empty distinction. |
| Matching CHECK/RSP GDL `0x800D5780`, preflight status 1 | The running list passed the implemented checks. This does not validate all RDP timing/state hazards. |
| Input near RDRAM `0x000D7D30`: SetTile, LoadSync, LoadBlock | The producer is now forwarding texture-load commands in the main list, rather than the earlier room TRI4 input. The corresponding DMEM and RDRAM words agree. |

The current FIFO window includes texture-rectangle and texture-load state.
As before, reused FIFO memory and the producer's current command do not
identify the earlier command responsible for the stall. No bad texture or
bad font is established by these photographs. Graphics microcode text is
still byte-identical to the earlier supplied builds.

Validation: the probe, submission hook and diagnostic display compile with
IDO 5.3 for US. The host probe test uses the repository's actual GBI encoders
with 32-bit command words, walks every command including nested calls, runs
the production preflight, checks both phase boundaries, confirms output
exceeds the FIFO size, and checks separate task buffers/read-ahead space.
The existing diagnostic tests also pass. With the probe disabled, `rsp.c`
code/data match master and the probe object has no runtime code/data.
This is not a full ROM/emulator run or a real-hardware validation.

## Room-drawing isolation after 04S

`GUD-n64-room-isolation.patch` applies to master `7153e175` (More diagnostics),
which includes the solid-sky test. It changes the normal US build and does not
require DEBUG. This is an investigation switch, not a confirmed crash fix.

`N64_DIAG_SKIP_BG_GDLS` defaults to 1 in `src/n64diagnostics.h`. It omits only
the primary and secondary room display-list calls in `bgRenderRoomPrimary`
and `bgRenderRoomSecondary`. Room loading, texture expansion, matrices, vertex
segment setup, room aging, visibility and collision still run. Characters,
props, weapons and the HUD retain their drawing paths. The existing solid-sky
bypass remains enabled so this test changes one additional part of rendering.
Expect invisible level walls/floors and objects against a solid background.

1. Apply the patch to `7153e175` and run `make VERSION=US`.
2. Confirm the missing room geometry in the emulator before testing the ROM
   on N64. Test both Runway and Cradle, including intro and player control.
3. Report whether each level reaches player control. If it stalls, keep the
   matching ELF/map and photograph pages 1-3 initially. The screen title will
   say **04BS** (room lists skipped and solid sky). Capture pages 4-6 too if
   the first pages show a different failure or a preflight error.
4. Set `N64_DIAG_SKIP_BG_GDLS` to 0 and rebuild for the 04S control. This
   restores room drawing. To restore both room and sky/water drawing, also
   set `N64_DIAG_SOLID_SKY` to 0. Setting `N64_LOAD_DIAGNOSTICS` to 0 disables
   both bypasses. Header dependencies already rebuild the affected objects.

If this still stalls, room display-list execution is not required for that
failure; investigate the remaining draws and shared RDP/FIFO handling. If it
loads, compare with the 04S control and narrow the room draw path next. A pass
does not prove bad room data: the bypass also changes graphics state, command
volume and timing. Room loading itself has not been eliminated by this test.

### Evidence from the supplied 04S build

`GUD(4).elf` confirms that `skyRender` calls the solid-background path, with
the sky/water triangle path omitted. The user reports that Runway and Cradle
both still stall. The photographed six-page sequence
`20260912_020657.jpg` through `20260912_020736.jpg` shows Cradle (stage `0x29`).
The additional UUID-named photo repeats page 6's values; it is not treated as
an independent Runway register capture.

| Observation | Interpretation |
|---|---|
| WAIT FRAME, MAIN PC `0x7000D0C8` | Matches `osRecvMesg + 0x68` in the supplied ELF; no CPU exception is reported. |
| SP PC `0x1B8`, word `0x40175000` | The same `mfc0 $23,DPC_CURRENT` FIFO free-space loop. |
| DP STATUS `0x760`, CURRENT `0x002BE8F0`, END/WRITE `0x002BE8D8` | Graphics remains stalled with the solid-sky bypass active. CURRENT above END is possible during a circular-buffer wrap. |
| R18 `0xB0`, R19 `0x002BE8D8`, R23 `0x002BE8F0` | A 176-byte write would end at `0x002BE988`, crossing CURRENT; the producer correctly withholds it. This R23 agrees with the separate DP register read. |
| Preflight status 1, matching CHECK/RSP GDL `0x800C8F80` | The running task passed the implemented checks, not a complete hardware correctness test. |

The RDRAM input is:

```
B6000000 00003000
04F00100 0E000DC0
B1007632 64542010
```

It matches Cradle room 5's primary stream in the ELF. The segment symbol is
`0x00AF4AA0`, the stream offset is `0x262C4`, and the sequence starts at stream
offset `0x1D8`. Texture token `C0080002 00000556` immediately precedes it.
The DMEM TRI4 copy `B1000763 00645420` is consistent with one triangle having
been consumed. These observations locate the producer; they do not identify
the RDP primitive that caused the stall or establish texture `0x556` as bad.

The raw FIFO window now contains plausible SetTile/SetTileSize mipmap state
through `0x002BE8D0`, then zeros beginning at END. It is still reused ring
memory, not a chronological trace. Neither those zeros nor the visible tile
commands are proof of an invalid submitted command. The graphics microcode
text remains identical to the earlier supplied builds.

Validation: both changed production C units compile with IDO 5.3 for US;
the existing diagnostic host tests pass. The room switch disabled produces
the same code/data sections as master for `bg.c`. A full ROM run and N64
validation remain external to this check.

## Solid-sky isolation test (04S)

The separate `GUD-n64-sky-isolation.patch` targets commit `abe660a3`
(More diagnostics), which already contains Diagnostic 04. It changes the
normal US build; no DEBUG build is required.

With `N64_DIAG_SOLID_SKY` set to 1 in `src/n64diagnostics.h`, `skyRender`
uses the existing solid-background path instead of rendering clouds/water.
The test explicitly synchronizes and selects NOOP render mode before the
fill. Room geometry, props and characters still use their normal paths.
Diagnostic screen titles say **04S** so the tested variant can be identified.
This is a controlled bypass, not a proposed permanent fix.

1. Apply this patch, then run `make VERSION=US`.
2. Check the ROM in the emulator: outdoor sky/water should become a solid
   background. Test Runway and Cradle on N64, including intro and player control.
3. If either still freezes, photograph the six 04S pages and keep the matching
   ELF/map. State which level and when it froze.
4. Set `N64_DIAG_SOLID_SKY` to 0 and rebuild to restore normal sky/water while
   retaining Diagnostic 04. Repeat the same failing load as the A/B control.

If the bypass works and restoring the sky reproduces the hang, focus on the
cloud/water command generation and its graphics-state transitions. A pass
still does not establish the exact bad command: the bypass also reduces
command volume and changes timing. A continued hang means the removed
cloud/water drawing is not required to reproduce that failure; inspect the
remaining rendering and FIFO handling next. Disabling all load diagnostics
also disables this bypass.

## Findings from the Diagnostic 04 hardware capture

The September 12 photos `20260912_014607.jpg` through `20260912_014646.jpg`
and matching `GUD(3).elf` / `GUD(3).map` show Runway (stage `0x23`):

| Observation | Interpretation |
|---|---|
| WAIT FRAME; MAIN PC `0x7000D0C8` | The supplied ELF resolves this to `osRecvMesg + 0x68`; no CPU exception is reported. |
| Halted SP PC `0x1BC`, word `0x0277A022` | `sub $20,$19,$23` in the same FIFO space-check loop as earlier captures. |
| DP STATUS `0x760`, CURRENT `0x002BE800`, END `0x002BE7A0` | The RDP is busy while graphics progress has stopped. These registers are captured before the diagnostic freezes the RDP. |
| R18 `0xB0`, R19 `0x002BE7A0` | The pending RSP output block is 176 bytes; it would end at `0x002BE850`, crossing CURRENT. The FIFO guard is withholding that write. |
| CHECK GDL and RSP GDL both `0x800D57B0`, status 1 | The check now belongs to the running task. It completed without finding one of the implemented hazards. |
| REG OK 1 and input VALID `0x707` | The scalar dump completed and all six input-word reads passed their address checks. This is not a whole-state correctness test. |

The exact three-command sequence read from RDRAM is:

```
04D000E0 0E002470
B1009652 87534310
B100AAA9 DCB9CB7A
```

It occurs uniquely in the supplied ELF, in Runway room 13's primary display
list. The commands are at raw stream offsets `0x560`, `0x568`, and `0x570`.
`bg_run_all_p_seg` is at ELF symbol address `0x009FBF00`; that room's primary
stream begins at segment offset `0x17014`. The first command loads 14
vertices from room vertex offset `0x2470`; the other two are TRI4 commands.
GoldenEye consumes TRI4 indices by shifting and rewriting its DMEM copy,
so the displayed `B1000000 00000000` entries do not by themselves indicate
cache corruption. The vertex command and both original TRI4 commands match
the embedded asset.

This locates the RSP producer, not the RDP command responsible for the stall.
The RSP can be many primitives ahead, and the raw circular FIFO window is
still not a command trace. Do not label room 13 defective from this capture.
Also, dumped R23 (`0x72`) is not coherent with the separately captured DP
CURRENT and loop arithmetic; do not use that one register as a fresh RDP
address or claim that REG OK validates every register's timing.
Graphics microcode text remains byte-identical to the previous supplied ELFs.

This patch gathers evidence for the real-console level-load crash that persists
after the BG header alignment fix. It does not claim to fix the remaining crash.
This consolidated Diagnostic 04 patch targets GUD commit `58435db0` (RDP check).
That commit contains the checker source files but not the earlier diagnostic
integration. This patch includes the missing hooks, linker entries, build
dependencies, displays and tests as well as Diagnostic 04's register capture.
Apply this patch directly to that HEAD; older diagnostic patches are not needed.
The hardware investigation targets the normal US build; `DEBUG` is not needed.

## Run the hardware test

Apply the patch and build normally. Diagnostics are enabled by default in
`src/n64diagnostics.h`; no USB cartridge or emulator debugger is needed.

1. Try the newly built ROM in GUD first. A preflight hazard can be caught there
   even if the emulator previously tolerated the bad graphics input. If it
   passes, copy the ROM to the EverDrive and repeat the failing level load.
2. If it freezes, wait at least 15 seconds for the ten-second watchdog.
3. Photograph **all six pages**. They cycle automatically every eight seconds
   and repeat until reset; no controller input is needed. Keep the matching
   `build/u/GUD.map` and `build/u/GUD.elf` from this exact build.
4. Send the photographs, ELF and map, along with the level and whether failure occurs
   before the intro, during the intro, or at player control. If no diagnostic
   appears, report that and whether sound continues.

The screen takes over permanently and requires a console reset. Its small font,
framebuffer drawing, and video setup all live in resident code; it requires
neither the game's fonts/textures nor a working RSP/RDP rendering task.
The optional scalar-register capture briefly runs a terminal RSP store program;
if it cannot run, the remaining diagnostic pages are still drawn by the CPU.

## Findings from the Diagnostic 03 capture

The September 12 photos `20260912_011228.jpg` through `20260912_011251.jpg`
and matching `GUD(2).elf` / `GUD(2).map` show the same graphics-progress stall:

| Observation | Interpretation |
|---|---|
| Stage `0x29`, phase WAIT FRAME | Cradle has reached the frame-completion wait. |
| MAIN PC `0x7000C808` | `osRecvMesg + 0x68`, after yielding the CPU; no CPU exception is reported. |
| Halted SP PC `0x1B8`, instruction `0x40175000` | `mfc0 $23, DPC_CURRENT` in the graphics FIFO free-space loop. |
| DP STATUS `0x760` | Command, pipeline and DMA busy, with START/END valid; the diagnostic had not yet set FREEZE. |
| FIFO `0x802BDCA0..0x802C7CA0`, WRITE/END `0x002BE7A0`, CURRENT `0x002BE800` | A 40 KiB circular buffer; CURRENT above END is possible during a wrap, not proof of bad bounds. |
| Preflight status 1, `0x1150` (4,432) commands | The last CPU check reached END without finding an implemented hazard. |
| CHECK GDL `0x800EDAE0`, RSP GDL `0x800D4AE0` | The displayed check describes a different, queued frame. It cannot establish the running frame's complete check result. |

The microcode text and initial data are byte-identical across all three supplied
ELFs. IMEM is loaded at `0x1080`, so SP PC `0x1B8` corresponds to text offset
`0x138`; ignoring this offset would identify the wrong instruction.
No specific malformed RDP command is proved by the raw FIFO window. It contains
triangle coefficients as well as possible command headers, and is not a trace.
The evidence points to an RDP progress failure with the RSP waiting behind it;
the triggering command or hardware condition remains unidentified.

## Diagnostic 04: match the task and capture RSP input

Page 4 now retains the preflight result for the graphics task selected by the
scheduler, matched by list pointer and size. Four recent checks cover the two
graphics task slots and queued frames; the newest matching record wins when a
buffer is reused. The result survives an audio interruption and graphics
resume. A missing/evicted match reports status 0, never a different task's
successful check. `LATEST` shows the most recently checked list for comparison;
`SERIAL` identifies the matched check. If the RSP is running audio, the retained
result describes the last graphics task. A preflight rejection still displays
the rejected list immediately.

Page 5 records all 32 scalar RSP registers and the original halted PC. After
the original hardware/IMEM/FIFO snapshot, a 33-instruction program stores the
GPRs to DMEM `0xF00..0xF7F` and executes BREAK. It uses no scratch register,
DMA, or RDP command; the RDP stays frozen. The program is read back before
execution, and its completion wait is bounded. If the RSP cannot halt, has
outstanding DMA, rejects the injected code, or fails to complete, `VALID` is 0.
The CPU still draws the pages. This technique is used only at terminal takeover
and destroys the stopped task's IMEM; resuming the game is not supported.
Normal gameplay still uses the original unmodified graphics microcode.

Page 6 captures the two input entries preceding R26/R27 and the entry at those
input cursors. Its RDRAM reads use the uncached alias and installed-RAM bounds;
DMEM reads stay in GoldenEye's `0x6A0..0x7DF` input buffer. `VALID` bits 0..2
mark the three RDRAM rows, bits 8..10 the DMEM rows (`0x707` means all six rows).
Rows without a valid bit must be ignored. R18/R19 show the pending FIFO transfer
size and destination when the RSP is stopped in the observed wait loop. R28 is
the remaining input byte count. For other stop locations these registers may
have other temporary uses; their labels are not universal task-state fields.

DMEM and RDRAM input words can differ legitimately: TRI4 rewrites its buffered
command, and input DMA can be ahead of the current instruction. A mismatch
alone does not prove a cache bug. RSP input progress is also ahead of RDP
execution. These captures are intended to locate the producer's code/list and
check its FIFO request, not to declare that its latest command caused the stall.

## What the screen means

All numbers on this screen are **hexadecimal**.

| Readout | Meaning |
|---|---|
| `CPU FAULT` | A fatal CPU exception, including a fault inside the TLB pager. Normal recoverable game-code TLB misses still use the existing pager. |
| `NO MAIN PROGRESS FOR 10 SECONDS` | The main thread has not reached another checkpoint for ten seconds. This is a stalled-progress report, not proof of a CPU exception or a particular RCP fault. |
| `INVALID ROM DMA` | A `romCopy` request violates the PI requirements checked here: RAM destination multiple of 8, ROM source multiple of 2, positive even size no greater than 16 MiB. The screen appears before submitting that request. |
| `ROM DMA QUEUE FAILED` | `osPiStartDma` rejected a ROM-copy submission. The old code would ignore the return value and wait for a completion message that might never arrive. |
| `PHASE` | Last coarse operation entered: pools, players, textures, fonts, BG header/data, STAN, setup, guards, paths, Bond initialization, intro, video, or frame processing. It describes where to start looking, not a proven cause. |
| `STAGE`, `STEPS` | Level ID and total number of checkpoint updates. |
| `THREAD`, `CAUSE`, `PC`, `RA`, `BADVADDR`, `FPCSR`, `SP`, `SR` | Faulted thread and saved CPU exception context. `THREAD` is `FFFFFFFF` and exception fields are zero for a watchdog/DMA report without a fault context. |
| `MAIN PC/RA`, `TLB PC/RA` | Last saved main-thread and pager program counter/return address. Useful for a watchdog stall; these are saved contexts, not a stack trace. |
| `SPSTAT`, `DPSTAT`, `PISTAT`, `DP CUR/END` | Hardware status and command addresses captured **before** the diagnostic halts/freezes graphics. |
| `SP PC` | Program counter read **after confirming RSP halt**. `FFFFFFFF` means halt was not confirmed. Diagnostic 01 read this register while the RSP could still be running; that value must not be treated as a reliable stopped instruction. |
| `ROM`, `RAM`, `SIZE`, `DMA WAIT` | Most recent synchronous ROM-copy request. WAIT is 1 after submission begins and 0 after its completion is received. This may be a pager read; audio's separate sample-DMA path is not included. |
| `BANK`, `ALLOC`, `POOL POS/END` | Most recent memory-pool allocation/resize request. ALLOC is requested bytes (new total size for resize), not necessarily a failed allocation. |

Thread IDs used by GUD include 2 scheduler, 3 main/game, 4 audio, 5 TLB pager,
and 6 this diagnostic watchdog. Exception code is `(CAUSE >> 2) & 31`. PC and
RA must be resolved against the map/ELF from this build, since instrumentation
changes code addresses. A branch-delay exception is indicated by CAUSE bit 31.

Page 2 captures the halted RSP. `HALTED` confirms the halt bit; `SP MEM OK`
also requires no outstanding SP DMA. A bounded poll permits the report to
continue if either condition never arrives. IMEM words and graphics DMEM
fields are unavailable unless `SP MEM OK` is 1. The eight instruction words
span PC minus 16 bytes through PC plus 12 bytes, wrapping within IMEM; they
identify the actual loaded instructions, including overlays or corruption.

`TYPE`, `UCODE`, `GDL` and `GDL SIZE` describe the most recent task prepared
for RSP start: type 1 is graphics, type 2 audio. `FIFO BASE/END` retain the
most recent graphics output buffer even if audio has since started. For a
graphics task, `WRITE`, `DMEM S/E` and `MODE HI/LO` read GoldenEye microcode's
output cursor, FIFO bounds and saved other-mode words at DMEM offsets 0x128,
0x150/0x154 and 0x118/0x11C. These fields are not decoded as graphics state
for an audio task. `MAIN A0/TLB A0` are saved registers; at the `osRecvMesg`
wait point they do not reliably identify the queue: `__osEnqueueAndYield`
saves callee-saved registers, but does not refresh A0. These can be stale.

Page 3 contains sixteen raw 64-bit entries from DP CURRENT minus 112 bytes
through DP CURRENT plus 8 bytes. The row at CURRENT is row 14, counting from
zero. `VALID` bit N says whether row N was read; unavailable rows show
`FFFFFFFF FFFFFFFF`. Reads use uncached RDRAM and stay inside the recorded
graphics FIFO and installed RAM. An XBUS command source is not read as RDRAM.
The window does not wrap across the FIFO boundary. CURRENT can be ahead of
the command currently executing, entries can be coefficients rather than
command headers, and a pending ring-buffer wrap can put END below CURRENT.
This is evidence to decode, not identification of an offending command.

## Findings from the Diagnostic 02 capture

The three September 12 photos and matching `GUD(1).elf` / `GUD(1).map`
confirm a graphics-progress stall on Cradle, rather than a reported CPU exception.
The saved CPU PC `0x7000BA98` is again `osRecvMesg + 0x68`.

The halted RSP PC `0x1B8` contains `mfc0 $23, DPC_CURRENT`. The eight captured
instructions match the supplied ELF's graphics microcode. This is the FIFO
space-check loop, which prevents the RSP overwriting commands still needed
by the RDP. FIFO base/end are `0x802BDCA0` / `0x802C7CA0` (40 KiB), its write
cursor is `0x002BE658`, and DP CURRENT is `0x002BE690`. The RSP needs more
space before it can submit its next block. The apparent reversal of CURRENT
and END can occur during circular-buffer wrap and is not by itself a bad bound.

This identifies where graphics progress stops, not the command that triggered
it. The raw window is not a command trace: commands have varying lengths,
the RDP prefetches, and FIFO memory is reused. Triangle coefficients can look
like opcodes, and older commands may already have been overwritten. Neither
these photos nor a static ELF prove which RDP command caused the stall.

## Diagnostic 03: check commands before submission

Before each graphics task is queued, `n64DiagCheckGfx` walks the CPU-built GBI
stream, including pushed/branched display lists and segment-address changes.
It checks selected RDP hardware hazards: unsafe fill/copy state, 4-bit tile or
palette loads, reversed palette indices, and the misaligned texture-load
condition. It also checks list/RSP DMA addresses, other-mode masks, and the
length/termination of raw sky triangles and texture rectangles. It reads
through the CPU cache because the display lists have not yet been written back.
It does not insert rendering commands or change the scheduler's completion rules.

If a hazard is found, graphics submission stops and page 4 appears immediately.
It gives the reason, physical command address, two GBI words, enclosing call-site
address, mode words, and texture/framebuffer information. `CHECK GDL` identifies
the inspected master list. In Diagnostic 04 the saved result is matched to the
running graphics task, as described above. The page remains CPU preflight state,
not a new RDP-register snapshot.
Texture and framebuffer sizes use RDP encoding: 0=4b, 1=8b, 2=16b, 3=32b.
`FFFFFFFF` means the checker has not established that field.

| Status | Meaning |
|---|---|
| 0 | No matching completed check for the running graphics task. |
| 1 | Reached the list end without finding the hazards implemented here. |
| 2 | Partial inspection: unknown/conditional control flow or the bounded walk limit. The task is still submitted. |
| 3 | Potential hardware hazard or malformed input; stopped before submission. |

A status of 1 is not proof of valid rendering. This is not a full RSP emulator
or RDP validator. It does not calculate visibility, triangle coefficients,
texture memory contents, synchronization correctness, or cache coherency. A
hazard detected at an input triangle might have been avoided by RSP culling;
confirm the reported command before calling it the original crash cause.
The walk has a 200,000-command budget and a 16-entry call stack. CULLDL and
unsupported commands stop inspection instead of inventing a branch outcome.
This adds CPU overhead and changes memory layout, so it is only for diagnosis.

The fill/copy restrictions are described in Nintendo's
[gDPSetCycleType manual](https://ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSetCycleType.html).
The additional load and copy-scissor checks follow the hardware conditions
recorded by [libdragon's RDP validator](https://github.com/DragonMinded/libdragon/blob/c4a7e119eff1cfad07adcfa892a2910c40d8bdb8/src/rdpq/rdpq_debug.c).
Only a subset is implemented here; libdragon itself is not linked into GUD.

## Findings from the first US hardware capture

The supplied ELF/map resolve both saved CPU PCs, `0x7000B258`, to
`osRecvMesg + 0x68`, immediately after its `__osEnqueueAndYield` call. This
is consistent with the game waiting for graphics messages and the pager
waiting for faults. It is not a CPU exception location.

The reported stage was Cradle (0x29), phase WAIT FRAME, SP status 0xC0 and
DP status 0x7C0. These support investigating graphics completion, but are
only one hardware snapshot. The initial graphics microcode image maps the
reported running SP PC of 0x300 to an AND in the other-mode update handler,
not a wait loop. Because Diagnostic 01 did not halt before reading PC, this
does not establish that the RSP was stuck there. Diagnostic 02 fixes that
capture ordering and collects live instructions and FIFO data that an ELF
cannot supply. The remaining game defect has not yet been identified.

## Scope and limits

- The watchdog runs every 250 ms above the game, scheduler, audio and pager
  threads, but below the PI/SI/VI managers. It starts at `bossEntry`, after OS
  timers and the scheduler are initialized.
- Checkpoints refresh on actual loading/frame progress. Receiving another VI
  message alone does not refresh them, so an RSP/RDP stall that prevents new
  frame submission can time out.
- DMA/allocation metadata is captured under a short interrupt mask. ROM reads
  do not refresh the main-thread watchdog, so repeated paging cannot hide a
  complete lack of application progress.
- The terminal renderer disables interrupts, captures hardware status, halts
  the RSP, freezes RDP command processing, then captures halted PC/instructions
  and a bounded raw FIFO window before drawing through an uncached alias
  of the existing framebuffer. It programs a base 320-pixel VI mode directly,
  so a stopped scheduler or black loading screen does not prevent display.
- It cannot guarantee recovery from disabled-interrupt hangs, corrupted
  resident code/data, or hardware/bus failures that prevent its own execution.
  A long legitimate operation with no checkpoints could also time out.
- The two diagnostic modules total about 19 KiB of resident code/data including
  a 4 KiB watchdog stack, plus hooks (about 3.2 KiB more than Diagnostic 03).
  Its timing and memory-layout effects make this a diagnostic build, not a
  performance baseline. If the crash disappears, that is useful evidence of
  a timing/layout-sensitive failure, not confirmation of a fix.
- The existing crash handler now restores its saved interrupt mask on the
  no-fault-thread path before waiting again. The ordinary TLB recovery path
  and game scheduling rules are otherwise preserved.

Set `N64_LOAD_DIAGNOSTICS` to `0` in `src/n64diagnostics.h` and rebuild to remove
the diagnostic code and calls. The Makefile explicitly tracks that header for
all consumers. Do this before returning to performance measurements.

## Validation

```sh
python3 tools/tests/n64diagnostics/run.py
```

The host tests execute the production code with OS/register shims. They cover
watchdog timing and Count wraparound, progress reset, recoverable TLB misses,
fatal/pager faults, invalid DMA requests, failed DMA queue submission, captured
metadata, framebuffer bounds on all six pages, and the terminal graphics-stop
sequence. They also check halted-PC ordering, unavailable SP memory, captured
IMEM/FIFO contents, preservation of graphics FIFO metadata across audio tasks,
and rejection of FIFO reads outside the buffer/installed RAM or in XBUS mode.
Diagnostic 04 also tests task matching with queued frames, audio/resume,
pointer reuse, differing sizes and record eviction. A host model executes the
injected SW/BREAK words, verifies GPR preservation and bounded DMEM writes, and
exercises code-readback failure, timeout, refused halt, busy DMA and input bounds.
This is not a hardware verification of the register dumper.
The preflight tests cover unsafe and legal fill/copy modes, texture-load
conditions, segmented pushed/branched lists, DMA bounds, sky-triangle and
texture-rectangle framing, missing terminators, partial inspection and a
self-branching list hitting the command budget. An optional PPM output path
produces previews using simulated values.
The submit-hook test verifies that an invalid master list goes directly to
the preflight page. The three static pipeline/geometry lists extracted from
the supplied ELF also pass the checker.

Diagnostic 04's changed production C unit compiles with IDO 5.3 for US. With
`N64_LOAD_DIAGNOSTICS=0` it also compiles without diagnostic function symbols.
Linker lists explicitly place the diagnostic text/data/rodata/BSS in resident
sections. A complete ROM build and real-console validation still need the
extracted assets and hardware that are not available in this workspace.
