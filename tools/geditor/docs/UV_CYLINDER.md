# UV coordinates and cylindrical mapping

In the UV editor's **Move** mode, U and V show the arithmetic mean of the
selected UV vertices. A shared native vertex is counted once. Type a coordinate
and press **Enter** in that box to set that axis on every selected vertex;
the other axis stays unchanged. For example, entering `0.5` in U aligns the
selection vertically in UV space. Dragging the move handles still translates
the group. Rotate and Scale retain their existing angle/factor fields.

Coordinates are normalized: one unit spans the texture's width or height.
Changes are rounded to GoldenEye's native 1/32-texel precision. An entry outside
the signed 16-bit S/T range rejects the entire edit. Invalid text in the other
box does not block a coordinate entry. An unchanged entry adds no undo step.

When the selected faces share one texture, the canvas shows it behind their
UVs. The **Opacity** slider controls the preview inside the outlined 0-1 tile;
repeats outside it use half that opacity. The image repeats in both directions,
including negative coordinates, and stays aligned while panning or zooming.
At 0% both the main image and its repeats disappear.

The **red border** marks the native UV limits, not the 0-1 tile: tiling outside
0-1 is valid. For a texture of width W and height H, the supported range is
`-32768/(32*W)` through `32767/(32*W)` in U, and the equivalent using H in V.
For example, a 32x32 texture permits almost 32 repeats in either direction.
Zoom out or pan towards a limit to see its border. For mixed texture sizes,
the border encloses the range valid for every displayed face. It remains
visible without a shared texture preview and with opacity set to 0%.

Select the background faces making up one cylinder, open the UV editor, and
click **Cylindrical** beside the planar projection buttons. It maps all faces
shown in the UV editor, independently of the UV vertex selection.

The mapper estimates an axis from vertex positions and face normals, including
rotated cylinders and cylinders that are wider than they are tall. The side
faces wrap once around U; V covers the cylinder's height. The seam follows a
rim column where possible. Selected end caps receive planar UVs in the same
texture tile, overlapping the side layout. This is a starting mapping for a
single, approximately circular cylinder; selecting several objects or an
incomplete side wall can give poor results. Rotate, scale or move the resulting
UVs as needed.

Mapping creates native vertex copies where the seam, caps, or different texture
sizes require different UV coordinates. Copies retain position, color and flags.
Unselected faces keep their original UVs. Face identities, materials and room
assignments are preserved. Both coordinate entries and cylindrical mapping use
one ordinary undo/redo step and save through the existing background compiler.
No game rendering code or project file format changes are required.

## Verification

Run `python3 tools/geditor/tests/uv_texture/run.py` for texture selection,
alpha/opacity, negative repeats, pan/zoom alignment, and native range borders
with mixed dimensions and offscreen clipping.

Run `python3 tools/geditor/tests/uv_cylinder/run.py` for the focused native tests:

- 792 cylinder cases covering every side count from 6 through 16, three aspect
  ratios and orientations, optional caps, duplicated vertices and integer grid
  rounding; side spans, seam wrapping and invalid input are checked.
- Per-face native UV edits preserve unselected faces, geometry, colors and
  materials; equal UV variants share copies; repeated mapping adds no vertices.
- Actual editor transactions cover native compile/save/reload, undo/redo and
  rollback on allocation, rebuild or history failures.
- Actual UV field/canvas functions cover displayed means, focused-axis Enter,
  mixed texture dimensions, bounds, invalid input, unchanged entries and the
  existing rotate/scale controls.

The tests use AddressSanitizer and UndefinedBehaviorSanitizer. Selection-history
regressions and a complete MinGW Windows build with warnings treated as errors
also passed. Interactive Windows viewport testing remains a manual check.
