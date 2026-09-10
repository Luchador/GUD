# Spawn and camera viewport markers

Opening a level displays markers derived from its current setup:

- `start.glb`: the first normal-play spawn in a solo setup, or all normal-play
  spawns in a multiplayer setup. Demo-only spawns are excluded. The arrow uses
  the pad's horizontal look direction, and the base rests on the stan floor
  when it can be resolved; otherwise it retains the pad's authored height.
- `intro_camera.glb`: every fixed camera in the intro list.
- `outro_camera.glb`: every cutscene camera in the object list. Deleted-character
  tombstones also use the camera record type, but do not create camera markers.
- `intro_spline_point.glb`: each control point in the solo intro swirl path.
  An orange curve shows the camera's travel between the points.

Camera positions and yaw/pitch use the game's conversions: signed positions
divided by 100, angles divided by 65536. Camera positions are already gameplay
coordinates and must not be divided by the level scale. Spawn pads do require
the level-scale conversion. A camera's pad identifies its room; its position
comes from the camera record itself.

The Object/Characters visibility control also hides these markers. They are
viewport aids, not additional game objects: displaying them does not dirty the
project, alter its setup or models, change ROM output, or add to BG statistics.
They refresh when a level is opened or its setup geometry is rebuilt, including
undo/redo. Existing current-format projects work without being recreated.

Complex, Temple, Basement, Stack, Library, and Caves have placeholder solo setup
names in the native level table. When the named file is missing, GEditor loads
the corresponding `Ump_setup*.set` file and retains that actual name for saving.
It does not substitute multiplayer data for solo missions or damaged files.

When a level opens, the viewport camera starts 200 world units (two metres)
above the first normal-play spawn marker and faces that spawn's horizontal
direction. This uses the marker's grounded position in both solo and multiplayer
setups. If no spawn is available, the existing view of the level bounds remains.
This placement runs only when opening a level; rebuilding geometry, editing,
and undo/redo preserve the user's viewpoint. Model-viewer orbit cameras are
unaffected.

## Artwork and lighting

The four GLBs are embedded in GEditor.exe from `geditorassets`; installed
executables do not need external copies. Replacing an artwork file triggers a
resource rebuild. The supplied models face local +X with +Y up. Their metre
coordinates are multiplied by `VIEWPORT_MARKER_MODEL_SCALE` (100 world units)
in `viewport.c`.

Vertex colors and authored smooth normals are retained. Uncolored vertices are
white. Ambient intensity is 0.4 and directional diffuse intensity is 0.6; the
light direction is fixed in world space. These settings are local to
`ViewportDrawSetupMarkers`, and the other viewport rendering is unaffected.
Markers are depth-tested and do not receive level fog.

The marker loader displays each GLB's active scene, including node transforms.
Only include the intended artwork when exporting the resource from Blender.

## Intro swirl path

The first contiguous block of `INTROTYPE_SWIRL` commands describes the solo
camera path. Its offsets and tangent scales are signed 16.16 values. Bit 0 ends
the block and does not produce a diamond. Bit 1 rotates an offset with Bond's
spawn-facing direction; otherwise the offset uses world axes. Bit 2 affects
camera aim, not its position. The pad field is a room reference, not the control
point's location.

The game adds these offsets to Bond's smoothed camera position. This static
preview uses the grounded spawn position plus the default starting eye height
of 175 world units (`bviewPlayerBeginLife`). It does not simulate intro animation,
head bob, cheats, or subsequent player movement. Only pad positions receive the
level-scale conversion; the swirl offsets and eye height are already world units.

The solid line uses the game's `coord3dCubicSplineInterp` formula and each
segment's tangent scale, with 32 samples per segment. Playback starts at control
point 1 and ends at the penultimate point. The first and last points supply
tangents; dashed orange guides connect these to the traveled curve. Coincident
controls remain separate records, as authored. The diamonds retain the model's
vertex colors and lighting, while the unlit line takes its orange color from
the diamond model's vertex colors and is two pixels wide.

The spline model's local +X arrow follows the tangent in the direction of
camera travel. End controls inherit the adjacent traveled point's direction.
Zero-tension and repeated points use nearby movement on the curve when the
derivative vanishes; a wholly stationary path uses a consistent +X direction.
Models remain upright relative to world Y, using world Z as the up reference
for vertical tangents so the orientation remains valid.

Multiplayer's procedural orbit is not an authored swirl path and is not shown.
A setup with no swirl or no solo spawn has no swirl preview. Malformed paths
produce a warning without hiding the other spawn/camera markers. Path buffers
are rebuilt on level load and setup refresh, and freed when the scene closes.
Like the other markers, the path follows Objects/Characters visibility and is
never written into a project or ROM.

## Checks

```sh
python3 tools/geditor/tests/setup_markers/run.py
```

This uses the production setup parser and GLB decoder with address and undefined
behavior sanitizers. It checks spawn filtering, camera coordinates and angles,
deleted-character exclusion, invalid records, multiplayer resource resolution
and saving, all four supplied models, colors, and transformed smooth normals.
Swirl checks cover signed offsets, facing-relative and world-axis positions,
eye height, segment tension, curve endpoints, tangent-only controls, terminal
records, invalid input, and all 20 authored mission swirl paths.
Orientation checks include tangents, end controls, vertical sections, repeated
points, zero tension, and stationary paths. The supplied arrow model is also
decoded in full, including both of its mesh nodes.
Windows rendering still needs a visual runtime check.
