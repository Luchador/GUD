# Sun and Moon

Apply this patch to GUD commit `78e6971` (Retain models patch), then rebuild both
GUD and GEditor with your usual commands. This changes the engine's environment
record layout, so **rebase your project onto the newly built ROM** before using
the new settings. During rebase, leave **Keep existing base images** enabled to
retain your project images, including 0AA4 and 0AA5. Keep project models when
resolving a model conflict if those contain your optimized hands.

In **Level Settings > Environment**, choose the desired environment variant,
then choose **Sun / Moon** in the Section dropdown. Set the fields and click
**Apply**. Save the project and use Create ROM as usual.

| Setting | Meaning |
| --- | --- |
| Sun / Moon | None (0), Sun (1, image 0AA4), or Moon (2, image 0AA5). |
| Diameter (degrees) | Full angular diameter, greater than 0 and up to 90. |
| Tint red/green/blue | 0–255 each. White (255, 255, 255) preserves the image. |
| Direction X/Y/Z | World direction; +Y is up. Length is ignored. Must be nonzero when enabled. |

The first activation from the empty ROM defaults supplies 5 degrees, white tint,
and direction `(0, 0.5, -1)`. This places the disc approximately 26.6 degrees
above the -Z horizon. Change the direction to place it elsewhere. All existing
levels start with None selected.

The sun/moon is at infinite distance: walking, camera translation and level
scale do not move it or change its angular size. Looking around changes where
it appears on screen; zoom changes its apparent pixel size. It is not a HUD
image locked to one screen position.

Rendering uses the IA8 image intensity and alpha, multiplied by the RGB tint.
The uploaded 64×64 images each occupy 4 KiB of N64 texture memory. Keep these
slots as IA8 images that fit TMEM (row width rounded up to 8 texels, times height,
at most 4096 bytes). Missing images, unsupported formats, oversized images or
failed texture loads are skipped safely in-game.

The game draws each disc with one oversized triangle, removing the internal
diagonal of the old two-triangle quad. Perspective UVs preserve the original
angular size and image orientation. The triangle follows the horizon and uses
the RDP scissor at viewport edges, so clipping does not turn it into a triangle
fan. AA settings do not change this path. Keep a fully transparent outer texel
border on both images: clamping that border hides the unused triangle area.
The supplied 0AA4 and 0AA5 images already meet this requirement.

For this triangle-only update, rebuild GUD, rebase the project onto that ROM,
and create a playable ROM. GEditor and the environment record layout are
unchanged; the editor preview uses the same underlying image projection.

The disc draws after the sky/cloud background and before level geometry, without
reading or writing depth. Buildings and terrain therefore cover it normally.
The horizon clips its lower portion; the existing Horizon Y offset moves it
with the rest of the sky. It works with Clouds turned off and in each player's
viewport. This feature does not add a light source, water reflection, lens flare
or cloud occlusion. GEditor previews it in the level viewport after Apply.

The new `EnvironmentRecord.SkyBody` contains `Type`, `AngularSize`, RGB bytes,
a reserved byte, and a `coord3d Direction`. It appends 24 bytes: native ENVT rows
are now 112 bytes. GEditor still reads/exports 88-byte and legacy 104-byte ROMs
without altering their layouts. Sun/Moon controls are disabled for those ROMs
until rebase. The new settings participate in existing project save/reopen,
field-level rebase conflict handling and ROM export. Alternate sky transitions
switch the body settings to the alternate record at the completed endpoint.

## Checks

- IDO 5.3 compilation of sky, environment and ROM manifest code.
- Full MinGW GEditor build.
- Environment field serialization, all three native layouts, reserved-byte
  preservation, old-format refusal, rebase and UI apply/reset tests.
- Shared projection tests for angular size, camera translation, direction
  magnitude, zenith, horizon/viewport clipping, zoom and aspect ratios.
- Production engine-adapter tests for texture IDs, IA8/tint, perspective UVs,
  missing images and offscreen early exits; exactly one native RDP triangle,
  signed offscreen Y packing, scissor restoration, and projective UV coverage
  across horizon/viewport clipping, including a tilted horizon.
- Existing project rebase/ROM-export regression suite with the supplied ROM.

Run the focused host tests from the repository root:

```sh
python3 tools/geditor/tests/environment/run.py
python3 tools/geditor/tests/cloud_preview/run.py
python3 tools/geditor/tests/sky_body/run.py
python3 tools/geditor/tests/project_rebase/run.py
```

Final visual verification in ares or on N64 is still needed. Check rotation and
movement, edges of the screen, the horizon, terrain occlusion, tint, zoom and a
second level with the other image selected.
