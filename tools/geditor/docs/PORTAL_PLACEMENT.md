# Portal creation and editing

Drag **Portal** from **Objects > Special** into the viewport. Choose the two
rooms to connect and the initial orientation, then click **Create**. Room
numbers are decimal. The new portal is always **3 x 3 meters**, centered where
it was dropped. A background hit supplies the first room; an empty-space drop
places the center in front of the camera. Creation enables portal visibility,
switches to Face/Move mode and selects the portal.

| Initial plane | Width axis | Height/depth axis |
| --- | --- | --- |
| XY, vertical | X | Y |
| YZ, vertical | Z | Y |
| XZ, horizontal | X | Z |

The creation dialog has no position or size fields. Use the viewport to shape
and place both new and existing portals:

- **Vertex (1):** select cyan corner dots and move the selected vertices.
- **Edge (2):** select and move perimeter edges. Fan-triangulation diagonals
  are not selectable edges.
- **Face (3):** select and move whole portal polygons.
- **Move (W):** drag an axis arrow, or enter coordinates in the existing
  position fields. The fields show the average selected component position.
- **Shift-click** adds components or faces; **Ctrl-click** removes them.
  Vertex/edge box selection also works. Select a portal component first to
  keep a marquee on portals; otherwise the marquee prefers background hits.
- **Snap to Vertex (V):** click a portal corner, then a background vertex.
  Only the portal corner moves. A missed target keeps the source selected;
  a successful snap clears it, ready for another pair. Portals and Stan do
  not steal the destination click. Press V again to leave snapping.
- **Escape** cancels a live drag; a second Escape clears the selection.

Translation, snapping and component selection participate in **Undo/Redo**.
Live previews do not modify the document until the drag is released. Moving
several selected edges translates their shared vertices only once. Rotation,
scaling, extrusion and other BG topology operations remain separate tools;
this portal extension adds translation and vertex snapping.

Portals are ordered polygons. Keep the finished shape flat and convex, with
no collapsed edges or crossing perimeter edges. Individual corner edits may
pass through temporary nonplanar shapes while you position the other corners.
Place the finished polygon at the opening through which the linked rooms
should see each other. Portals affect visibility; they do not add walkable
Stan geometry.

Properties shows the active portal entry's room links. In Face mode, repeated
clicks on coincident portals cycle their entries. Some native entries share
one polygon: changing that geometry updates all its connections together,
while room-link edits still affect only the active table entry.

**Extra margin (world units)** is editable. Enter a nonnegative value and press
Enter or **Apply margin**. Escape restores the current value; 0 removes the
extra margin. The value is rounded to the closest amount GoldenEye can store
(ties round upward), and the text box shows the stored result after applying.
A margin extends the visibility opening on both sides of the portal plane; it
does not move the portal's vertices. Margin edits affect only the active table
entry, independently of pending room-link edits, and support undo/redo.

For Depot portal 105, try **110 world units**: this stores a margin of 24 native
background units (about 109.85044 world units). Save the project and create a
ROM to test the railing from both floors and while moving on the stairs. The
viewport continues to draw the portal polygon, not its expanded visibility
volume.

Saving writes portal geometry and links into the native BG included by Create
ROM. Existing indices, polygon addresses and flags are preserved; margins are
preserved unless explicitly edited.
Native coordinates are floats rather than integer BG vertex coordinates.
Exact native values are retained in history so Undo after saving restores the
original coordinates without accumulating scale-conversion rounding.

The native limit remains 199 portals. Additions append to the table and normal
repeat saves reuse their storage. Undo/save/redo across portal additions is
supported; replacing additions after saving can leave unused metadata to keep
original addresses stable. No tank connections are added automatically to
Depot's rooms 20–24.

## Validation

Run from the repository root:

```
python3 tools/geditor/tests/portal_margin/run.py
python3 tools/geditor/tests/portal_editing/run.py
python3 tools/geditor/tests/portal_placement/run.py
python3 tools/geditor/tests/portals/run.py
python3 tools/geditor/tests/vertex_snap/run.py
python3 tools/geditor/tests/selection_history/run.py
python3 tools/geditor/tests/box_selection/run.py
```

Native tests cover component picks, modifiers, shared geometry, preview
restoration, controller rollback, snapping, native BG persistence, byte-exact
undo/save/redo, original metadata, 3–8-point polygons and capacity limits.
Window messages are stubbed; Windows UI and real N64 visibility need manual
checks. Try creating a portal, moving an edge, snapping its corners, saving
and reopening, then exporting a ROM and viewing the opening from both rooms.
