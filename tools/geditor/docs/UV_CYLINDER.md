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
