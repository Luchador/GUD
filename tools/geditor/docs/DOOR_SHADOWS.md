# Door Shadows

In face mode, select two adjacent background triangles forming a planar,
convex quadrilateral. Right-click and choose **Create Door Shadow**. The faces
are replaced by a selectable Door Shadow object with the same texture and UVs.
Creating it is one undo step that restores both faces and the previous setup.

Select the new object to edit its properties:

- **Pick door**: click a door in the viewport to link it. Escape cancels picking.
  **Unlink door** removes the link. Unlinked, deleted or unavailable doors leave
  the shadow closed.
- **Light expands toward**: choose **+X**, **-X**, **+Z** or **-Z**. The selected
  axis must run between two opposite edges. At 0% the surface is entirely dark;
  at 100% it is entirely light. Intermediate values move the dividing line.
- **Light RGB / Dark RGB**: choose the vertex color tint for each region. White
  retains the texture's brightness; lower values darken or color it. These
  replace the source triangles' painted RGB. The source material and opacity
  remain in use.
- **Preview door opening**: scrub from closed to open without changing the
  linked door or saving an artificial opening amount. In game the shadow uses
  the door's actual opening divided by its full travel, including partial
  openings, closing and reversals. Sliding and swinging travel are supported.

The original two triangles are clipped independently. New boundary vertices
interpolate each triangle's UVs and alpha, so the texture remains stationary
and even a UV seam along the original diagonal is preserved. In game, vertices
use coordinates centered on the shadow with up to 64 steps per original room
unit. Each axis uses the finest power-of-two scale that fits the signed 16-bit
vertex range, so large surfaces reduce precision safely. A model matrix places
the surface back in the world; its outer corners stay fixed during animation.
UVs and alpha are evaluated at the emitted, rounded local positions to avoid
texture drift. Texture coordinates still use the native 1/32-texel precision.

The source faces must share a room, layer and static material/render state.
Water, light-fixture textures, environment mapping and native lighting are not
supported for conversion. The object stays at the converted surface's position;
ordinary model move/rotate/scale and copy/paste do not apply. To reshape it,
undo its creation and edit the background faces first. Delete removes the
object; it does not recreate the old background faces.

There may be up to 64 live Door Shadows per setup. They render in their original
room and geometry layer, with the level's existing fog and material settings.
They add no collision or gameplay prop. Room statistics count them as objects
in their stored room. The editor's Objects / Characters visibility toggle also
controls their preview.

Save Project writes both the edited BG and setup. As with other BG edits, a BG
resource shared by several setups changes for all of them; the new Door Shadow
belongs to the setup in which it was created.

## Build and export

Apply this change after the environment-mapping controls (`3cd2212`). Rebuild
GEditor and perform a clean GUD build, then rebase the project onto that GUD ROM
before exporting. The new native setup record requires this runtime. Export
checks the ROM's versioned `DSHD` feature entry and reports an older base ROM.
Use the updated editor for setups containing Door Shadows.

The subsequent **Door Shadow precision** patch applies on top of the initial
Door Shadow implementation. It requires a clean GUD build and project rebase
onto that new ROM before export. Existing Door Shadows automatically use the
finer coordinates; their saved format and editor controls are unchanged, so
rebuilding GEditor or recreating the objects is unnecessary for this update.

## Implementation and checks

`src/doorshadowformat.h` defines the fixed 1,024-byte type-49 command. Door
links use stable native setup-command indices, surviving compaction and
save/reload. Deleted command slots can be recycled without shifting other
references. The room renderer caches the expanded static material list and
allocates 18 transient vertices and one 64-byte matrix per visible shadow;
unused triangles degenerate. Both allocations are included in the frame-buffer
budget check. The room's matrix and vertex segment are restored after drawing.
Cache data is released with the room and reset before loading another level.

The portable clipping implementation is shared by the editor and game. Run:

```sh
python3 tools/geditor/tests/door_shadow/run.py
# Optional read-only regression on a pair of native project files:
python3 tools/geditor/tests/door_shadow/run.py path/to/bg.seg path/to/setup.set
```

These tests cover all directions and opening fractions, area and UV continuity,
conversion rejection, properties, save/reload, combined BG/setup undo and redo,
command recycling, and the production renderer with texture expansion/fog LUTs,
room cache lifecycle and allocation failures. Runtime precision checks exercise
sub-unit door motion in all four directions, UV anchoring after rounding,
coordinates near the signed-16-bit limits, vertical/sloping surfaces, level and
render scales, room-origin changes, multiple shadows, matrix restoration, and
insufficient matrix/vertex memory. They decode matrices produced by the game's
actual fixed-point converter. Host tests do not rasterize an N64 frame;
emulator visual verification is still required.
