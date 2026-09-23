# Face environment mapping

Select background faces and open **Advanced > Environment mapping**.

| Choice | Behavior |
| --- | --- |
| Auto (preserve original) | Retains the authored texture-coordinate behavior. |
| Off | Uses the saved UV coordinates on supported background faces. |
| Spherical | Enables native N64 spherical texture-coordinate generation. |
| Linear | Enables native N64 linear texture-coordinate generation. |
| Mixed / Keep current | Indicates a mixed selection and leaves each face unchanged. |

Spherical and Linear use the triangle's normal and the assigned base image as
a reflection lookup texture. Moving the camera changes the sampled part of that
image. They do not capture reflections of surrounding level geometry. Linear
describes the coordinate projection, not texture filtering.

The normal comes from triangle winding and is shared by all three corners.
This produces a flat, faceted reflection; it does not add smooth vertex normals.
A flat triangle consequently samples the same lookup coordinates at each corner.
The generated coordinate range fits the base image dimensions, including
rectangular images. Existing texture wrapping and detail settings still apply.

Generated mapping displays the image without painted RGB tint. Original RGB,
alpha and UV values remain in the document and saved BG. Auto restores the
authored material, and Off uses the original UVs and colors on ordinary faces.
Fog participation, alpha presets, opacity, transparency, decals and geometry
layers remain independent settings, subject to their existing restrictions.

The viewport updates coordinates when the camera moves. It also updates the
flat normal during corner moves, rotation and scaling. Painting changes the
stored colors without changing the generated normal or tinting the reflection.
Multiple selection, mixed values, undo/redo, copy/paste, saving and ROM export
retain the choice.

## Supported geometry

Explicit choices support ordinary background geometry. Materials with native
lighting, custom matrix/light commands or nested display lists must retain
Auto. Spherical and Linear additionally require a static base image: animated
water and light-fixture textures are rejected. A selection containing an
unsupported face is rejected before any face changes.

Assigning an animated/light-fixture image to a mapped face is rejected. Removing
the texture also returns environment mapping to Auto. Auto is always available
to remove an override.

## Build and export

Rebuild **GEditor and GUD**, then **rebase the project onto the newly built GUD
ROM** before exporting. Use a clean GUD build when applying this patch. A ROM
based on an older GUD loader cannot resolve the new environment metadata.

No project version change is needed. Per-face metadata preserves the original
material and a packed signed normal. The compiler emits a private three-vertex
load for each generated triangle. During room loading, GUD replaces only those
private RAM copies' RGB bytes with normals. Their alpha, position and UV bytes
are unchanged. Saved vertex data keeps its original painted colors.

Native RSP lighting and texture generation run with the current player's LookAt
axes. A scoped white ambient light keeps shade RGB neutral; geometry flags,
texture scale and the ordinary background lights are restored on scope exit.
The room-loader pass runs after texture expansion and fog/alpha resolution and
before optional one-cycle conversion. Explicit environment choices are excluded
from automatic one-cycle conversion and texture batching.

Generated faces cost three private vertices plus display-list commands and RSP
lighting work. They require no additional texture image, per-frame vertex
rewriting or per-frame display-list allocation.

## Validation

```
python3 tools/geditor/tests/face_environment/run.py
python3 tools/geditor/tests/uniform_scale/run.py
python3 tools/geditor/tests/face_fog/run.py
python3 tools/geditor/tests/face_alpha/run.py
```

The environment suite covers numerical spherical/linear projections, camera
rotation, live deformation, painting, both room passes, independent fog/opacity,
history, copy/paste, unsupported texture edits, exact Auto restoration, twenty
save/reload cycles and ROM-export cleanup. It feeds compiled synthetic and native
Runway streams into the production texture dispatcher, fog/alpha resolver and
environment resolver. Runtime checks cover private normals, unchanged neighbors,
rectangular image scales, geometry/light restoration, fog on/off, AA on/off and
one-cycle protection. Texture upload is stubbed; the tests do not rasterize N64
pixels. Windows GUI and emulator/hardware visual checks remain necessary.
