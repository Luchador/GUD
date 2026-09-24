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
- **Delete:** in Face mode, remove the selected portal connections. Shift-click
  selects several for a single deletion. Undo restores them and their selection;
  Redo removes them again. Vertex/Edge modes do not delete portal components.
- **Ctrl+C / Ctrl+V:** in Face mode, copy the selected portal connections and
  paste independent copies 10 world units above their copied positions, matching
  object/face paste. Shift-click can select several portals for one copy. The
  copies become selected and portal visibility is enabled. **Edit > Copy/Paste**
  performs the same commands.
- **Move (W):** drag an axis arrow, or enter coordinates in the existing
  position fields. The fields show the average selected component position.
- **Shift-click** adds components or faces; **Ctrl-click** removes them.
  Vertex/edge box selection also works. Select a portal component first to
  keep a marquee on portals; otherwise the marquee prefers background hits.
- **Escape** cancels a live drag; a second Escape clears the selection.

Translation and component selection participate in **Undo/Redo**.
Live previews do not modify the document until the drag is released. Moving
several selected edges translates their shared vertices only once. Rotation,
scaling, extrusion and other BG topology operations remain separate tools;
portal components support translation.

Portal copies retain their shape, winding, room links, flags and margin. Entries
that share a polygon within the copied group continue to share the new polygon;
the pasted group never shares geometry with the originals. Each paste is one
Undo/Redo step. Copy itself does not modify the level. Repeated paste starts
from the snapshot, so editing or deleting the source does not change it.
After moving a copy to another opening, update its Room 1/Room 2 links as needed.

Copying portals, objects or background faces replaces the previous scene
clipboard. It clears when changing levels or projects. Text fields keep their
normal text clipboard. Portal copy/paste requires Face mode with no active
transform, knife operation or camera flight. Invalid coordinates, allocation
failure or exceeding the 199-portal limit leaves the level and clipboard intact.
Global visibility scripts are retained and are not duplicated with portals.

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
Deletion likewise removes only selected table entries, not every connection
sharing their polygon. Surviving entries are renumbered. If a level's global
visibility script references a polygon, its last connection cannot be deleted
until the script is updated; the editor explains this and leaves the selection
unchanged. Depot's empty global visibility script does not impose this restriction.

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
ROM. Deletion compacts the table; surviving polygon addresses, flags and margins
are preserved unless explicitly edited.
Native coordinates are floats rather than integer BG vertex coordinates.
Exact native values are retained in history so Undo after saving restores the
original coordinates without accumulating scale-conversion rounding.

The native limit remains 199 portals. Additions append to the table and normal
repeat saves reuse their storage. Deletion frees portal capacity. Undo/save/redo
across portal additions and deletions is supported; saved edits can leave unused metadata to keep
original addresses stable. No tank connections are added automatically to
Depot's rooms 20–24.

## Validation

Run from the repository root:

```
python3 tools/geditor/tests/portal_clipboard/run.py
python3 tools/geditor/tests/portal_margin/run.py
python3 tools/geditor/tests/portal_deletion/run.py
python3 tools/geditor/tests/portal_editing/run.py
python3 tools/geditor/tests/portal_placement/run.py
python3 tools/geditor/tests/portals/run.py
python3 tools/geditor/tests/selection_history/run.py
python3 tools/geditor/tests/box_selection/run.py
```

Native tests cover component picks, modifiers, shared geometry, preview
restoration, controller rollback, native BG persistence, byte-exact
undo/save/redo, original metadata, 3–8-point polygons and capacity limits.
Window messages are stubbed; Windows UI and real N64 visibility need manual
checks. Try creating a portal, moving an edge, positioning its corners, saving
and reopening, then exporting a ROM and viewing the opening from both rooms.
