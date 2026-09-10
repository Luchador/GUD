# Spawn and camera viewport markers

Opening a level displays markers derived from its current setup:

- `start.glb`: the first normal-play spawn in a solo setup, or all normal-play
  spawns in a multiplayer setup. Demo-only spawns are excluded. The arrow uses
  the pad's horizontal look direction, and the base rests on the stan floor
  when it can be resolved; otherwise it retains the pad's authored height.
- `intro_camera.glb`: every fixed camera in the intro list.
- `outro_camera.glb`: every cutscene camera in the object list. Deleted-character
  tombstones also use the camera record type, but do not create camera markers.

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

## Artwork and lighting

The three GLBs are embedded in GEditor.exe from `geditorassets`; installed
executables do not need external copies. Replacing an artwork file triggers a
resource rebuild. The supplied models face local +X with +Y up. Their metre
coordinates are multiplied by `VIEWPORT_MARKER_MODEL_SCALE` (100 world units)
in `viewport.c`.

Vertex colors and authored smooth normals are retained. Uncolored vertices are
white. Ambient intensity is 0.4 and directional diffuse intensity is 0.6; the
light direction is fixed in world space. These settings are local to
`ViewportDrawSetupMarkers`, and the other viewport rendering is unaffected.
Markers are depth-tested and do not receive level fog.

The supplied `outro_camera.glb` contains coincident intro and outro meshes.
The marker loader selects its `outro_camera` mesh node, retaining ancestor
transforms, to avoid overlapping blue and red surfaces. The source GLB remains
unchanged. Keep that node name when replacing this resource.

## Checks

```sh
python3 tools/geditor/tests/setup_markers/run.py
```

This uses the production setup parser and GLB decoder with address and undefined
behavior sanitizers. It checks spawn filtering, camera coordinates and angles,
deleted-character exclusion, invalid records, multiplayer resource resolution
and saving, all three supplied models, colors, and transformed smooth normals.
Windows rendering still needs a visual runtime check.
