# Face fog participation

Select background faces and open **Properties > Advanced > Fog participation**.

| Choice | Behavior |
| --- | --- |
| Auto (preserve original) | Uses the authored material and the game's normal fog conversion. |
| On | Participates in the level's fog, even if the original material skipped it. |
| Off | Disables fog for these faces. |
| Mixed / Keep current | Indicates a mixed selection; leaves each setting unchanged. |

The choice is independent of transparency, opacity, texture, decal, and geometry
layer. It supports multiple selections, undo/redo, copy/paste, room transfers,
project saving and ROM export. Changing fog does not replace the stored alpha
preset, colors, texture settings or authored render commands. Auto restores
that authored pipeline.

The viewport now checks the material's fog blender as well as geometry fog and
alpha overrides. It can show native materials that skip fog, rather than applying
fog to every background face. **View > Fog** and the level's environment settings
still control whether fog is visible. On does not invent fog in a fogless level.

## Runway example

In the supplied Runway BG, faces **1609, 1610 and 1611** belong to **room 12** and
use image **046A** with a one-cycle material. The native fog LUT converts supported
two-cycle render modes; it does not promote this one-cycle material. That leaves
these faces without the neighboring geometry's distance fog.

Select those faces and set **Fog participation > On**. At runtime, when the level
has fog, their material is promoted to two cycles: the first combiner evaluates
the original color equation and the second passes that result through. The
first blender applies fog; the second retains the original surface blending.
Original sampling, opacity, depth, culling and coverage settings are preserved.
The surrounding pipeline is restored before following faces and at list end.

Promotion costs an additional RDP cycle on the affected faces. Auto and Off keep
an authored one-cycle material in one cycle. Explicit fog choices are protected
from the AA-off one-cycle optimizer so it cannot undo the requested result.

## Alpha and supported materials

N64 fog and painted vertex alpha use the same vertex channel. Fog On therefore
cannot be combined with **Vertex alpha** or **Texture x vertex alpha**. The editor
rejects that combination before editing any faces. Use Auto or Off with painted
alpha, or use a texture/constant alpha preset when fog is required. Custom Auto
alpha equations that consume vertex alpha also require an independent preset.

On and Off require an explicit ordinary one- or two-cycle surface pipeline.
One-cycle promotion supports standard shaded textures and untextured SHADE
materials. Custom one-cycle RGB equations stay unsupported; the editor reports
that instead of changing their appearance. Animated water and unknown texture
markers also stay unsupported because they inject their own rendering state.
Auto is always available to remove
the fog override. Standard Auto alpha is translated to environment alpha during
promotion, consistent with the existing BG alpha behavior.

## Build and persistence

Rebuild **GEditor and GUD**, then **rebase the project onto the rebuilt GUD ROM**
before exporting. Older editors and game loaders do not understand the new fog
metadata. There is no project format version change.

Fog metadata is stored per face alongside the authored material. The compiler
emits joint alpha/fog scopes before vertex loads, so shared vertices are reloaded
when their fog participation changes. Runtime slots set fog geometry, cycle type
and both blender stages after native LUT conversion. Saving recreates those
slots instead of retaining obsolete copies. Returning to Auto preserves the
original one/two-cycle choice and combiner.

Validation:

```
python3 tools/geditor/tests/face_fog/run.py [path/to/bg_run_all_p.seg]
python3 tools/geditor/tests/face_alpha/run.py
python3 tools/geditor/tests/bg_transparency/run.py
python3 tools/geditor/tests/face_detail/run.py
python3 tools/geditor/tests/face_rooms/run.py
python3 tools/tests/bg_onecycle/run.py
```

The optional BG argument checks the reported face IDs in the supplied file.
Without it, tests use Runway geometry from the repository. Tests cover preview,
restoring Auto, alpha conflicts, unchanged neighboring materials, history,
copy/paste, repeated edits/saves, native one-cycle promotion, independently
evaluated RGB/alpha equations, fog/no-fog environments and AA toggles. They do
not replace visual testing on Windows and an emulator or N64.
