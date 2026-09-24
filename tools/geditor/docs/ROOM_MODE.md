# Room mode

The main viewport modes are Vertex (1), Edge (2), Face (3), Room (4), and
Vertex Paint (5). The room button uses `room_select_off/on.png`. Model Editor
painting also uses 5. The Vertex Snap toolbar button, Vertex menu entry, and
V shortcut are removed. Numeric shortcuts leave edit fields and combo boxes alone.

Press 4 and click a background face, object, or character. The whole room is
selected, including both background layers, its Stan tiles, connected portal
polygons, characters, objects, and Door Shadows. Hidden items and disabled
visibility layers remain part of the move. Click empty space or Ctrl-click
to clear the room selection. Switching modes clears the room selection.

Drag a translation arrow or enter a world position in the Transform panel.
The position is the center of the selected room geometry's bounding box.
For a small height correction, the Y field accepts expressions such as
`current value + 20`. Escape cancels a drag. Rotation, scaling, duplication,
and component-edit operations are not room operations.

Pads move with their room, including unused placement pads, spawn pads,
patrol waypoints and bound pads. Membership follows the same authored Stan
floor lookup used for model placement, so overlapping floors remain distinct.
Shared pads move once and retain their placement flags, dimensions, links and
indices. Door Shadow origins move with their explicit room ID.

A room move uses one displacement rounded to native Stan precision for every
asset. Background vertices retain their room-local coordinates; the room
origin moves. Portal aliases retain their shared geometry and move once.
Neighboring rooms' background and Stan geometry stay in place, including
Stan endpoints linked across the room boundary. Connection records are retained.

The edit is one **Move Room** Undo/Redo action across BG, setup and Stan.
Cancel or a failed edit restores all three documents. Save Project and Create
ROM use their ordinary native asset paths. No runtime patch or rebase is needed.

Validation: `python3 tools/geditor/tests/room_mode/run.py` exercises room
membership, both background layers, object/BG clicks, live preview and cancel,
native rounding, adjacent and stacked collision floors, shared placement pads,
Door Shadows, shared portals, persistence, Undo/Redo and rollback. Existing
selection-history, geometry-toolbar and model-paint UI tests cover selection
restoration, unchanged edge tools and the new shortcuts.
