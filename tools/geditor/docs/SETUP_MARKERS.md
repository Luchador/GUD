# Spawn and camera viewport markers

Opening a level displays markers derived from its current setup:

- `start.glb`: the first normal-play spawn in a solo setup, or all normal-play
  spawns in a multiplayer setup. Demo-only spawns are excluded. The arrow uses
  the pad's horizontal look direction, and the base rests on the stan floor
  when it can be resolved; otherwise it retains the pad's authored height.
- `intro_camera.glb`: every fixed camera in the intro list.
- `outro_camera.glb`: cutscene cameras in the object list. Identical camera
  records used by multiple script tags share one marker. Deleted-character
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

Ordinary pad wireframes have a fixed half extent of 5 world units (10 units
across) on every level. Their positions still use the level-scale conversion.
Bound pads show their authored volumes. Scaling an ordinary pad creates bounds
from the displayed size, converting back to native level units for saving.

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
Like the other markers, the path follows Objects/Characters visibility. The
diamond artwork and sampled orange curve are viewport aids; edits are written
to the native setup records described below.

## Editing setup markers

Click a start point, swirl control, intro camera, or outro camera to select it.
The selected start turns gold (RGB 255, 210, 0), matching selected vertices;
other selected markers turn white. Markers can be picked in face, edge, and vertex
modes; clicking again cycles coincident control points. Ctrl-click removes a
selection. Picking respects foreground level geometry and object visibility.

Use W for translation or E for rotation, then drag a handle or enter values in
the existing transform panel. Scaling is disabled for these markers. Dragging
previews changes in a temporary setup copy, including the affected swirl path;
Escape cancels. Releasing the handle creates one undo step. Selection survives
the geometry rebuild and undo/redo, using a command index instead of a byte
offset that could move when other setup tables grow. Spawn or intro-camera
deletion clears selection because removing an intro command shifts the later
command indices.

- Start points edit their ordinary pad coordinates and horizontal heading.
  The game's floor placement still applies: changing Y can choose a different
  floor, but does not make the player float above it. Other setup references to
  that pad move with it. Facing-relative swirl controls follow the spawn.
- Intro/outro cameras edit their native position and yaw/pitch. World rotations
  change the camera's look direction; the game has no camera roll field. Moving
  a camera gives it an automatically resolved room pad at the new location,
  without moving an authored pad shared by other objects or cameras.
- Swirl translations invert the point's facing-relative/world-axis encoding.
  Rotations turn the neighbouring controls around the selected point, changing
  the actual curve, because swirl commands have no independent angle fields.
  Duplicate endpoint controls are handled together when finding the neighbours
  that determine the tangent. The selected point remains the pivot.

Camera positions use hundredths of world units, angles use signed 16.16
radians, and swirl offsets use signed 16.16 world units. Out-of-range edits
are rejected before writing records. Spawn pad positions alone use level scale.
Saving writes the edited raw `.set` asset. ROM creation saves the current level
first and replaces its setup resource with that asset, repacking if needed.
No new project or ROM format is introduced.

## Placing and deleting starts

Drag **Spawn Point** from the Object browser onto a surface over walkable stan.
The start is grounded on that floor, selected, and ready to move or rotate.
Dropping outside the viewport or onto empty space does not edit the setup.
Escape and capture loss cancel the browser drag.

In single-player setups, placement replaces the normal-play start, retaining
its heading and ensuring only one normal-play spawn remains. A setup without
a start receives one facing +Z. The swirl controls and curve follow the new
spawn position automatically; their relative offsets, timing and shape remain
the same. Demo starts retain their original records and pads.
Swirl room hints are reset to use the player's new collision position, so
authored hints such as Dam's do not keep pointing at the old area.

In multiplayer setups (`Ump_`), placement appends a start facing +Z. Up to 16
normal-play starts are supported, matching the game's `g_Startpad` array.
Select a start and press **Delete** to remove it; the last normal-play start
is protected. This works in face, edge and vertex selection modes.

New starts get their own pads. Replacing or deleting a start retains the old
pad for other setup references, so shared props, characters and AI targets
do not move. Intro commands and pads are written in the native setup format.
Each placement/deletion is one undo step, marks the project unsaved, and is
included in saved assets and created ROMs. Existing projects work immediately.

## Placing and deleting cameras

Drag **Intro Camera** or **Outro Camera** from the Object browser onto a level
surface. These entries are restricted to single-player setups; multiplayer
setups reject the drag with an explanation. Cameras retain the drop height
and initially face the same direction as the editor viewport. A walkable room
must be resolvable at the location. The placed camera is selected for W/E
translation and rotation. Empty-space drops and cancelled drags make no edit.

Each intro drop appends a native `INTROTYPE_CAMERA` (type 6) command. The game
counts all such commands and links them into its existing random-selection
pool, so every added camera is eligible without an engine change. New cameras
inherit the first intro camera's two caption IDs. When there is no first caption,
they use the blank `TITLE_STR_227` in the permanently loaded `LTITLE` bank.
Select an intro camera and press **Delete** to remove it. At least one must
remain. Existing spawn, swirl and other intro commands are retained.

An outro drop replaces the ending camera. Some authored missions contain
several tagged cutscene shots rather than one camera. Replacement makes every
existing shot use the new camera's position, direction and room pad, while
preserving its command slot and tag so the mission's AI camera switches still
resolve. Identical shots appear as one editable marker; subsequent transforms
update all its copies. Undo restores the previous authored shots. A setup with
no camera receives a native `CameraPos` command; this does not create new AI
camera-switch instructions or an ending sequence for that setup.

Camera placement uses new room pads, preserving existing pads referenced by
other entities. Each placement, replacement or deletion is one undo step,
marks the project unsaved, and is included in saved `.set` assets and ROMs.
Existing projects work without regeneration.

## Checks

```sh
python3 tools/geditor/tests/setup_markers/run.py
python3 tools/geditor/tests/browser_drag/run.py
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
The native edit tests also exercise start/camera/swirl transforms, private room
pad allocation and reuse, unchanged shared pads, coordinate range rejection,
save/reload, and the production setup undo/redo and rollback paths.
Spawn checks cover solo replacement and swirl translation, multiplayer
addition/deletion and limits, demo records, shared pads, absent intro lists,
saved assets, invalid positions, and undo/redo of placement and deletion.
Camera checks cover intro addition and deletion, last-camera protection,
multiplayer rejection, native positions/angles/captions, saved assets, and
undo/redo. They extract the game's actual intro link/selection code to check
that every appended camera can be chosen. Outro tests cover tagged multi-shot
replacement, synchronized transforms, deleted-guard exclusion, first-camera
creation and relocated prop cache offsets. Browser tests cover capture and
drop dispatch for spawns and both camera kinds, plus cancellation.
Windows rendering and in-game playback still need a runtime check.
