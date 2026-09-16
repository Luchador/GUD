# Object duplication

Select a placed object and press **Ctrl+C**, then **Ctrl+V**. The copy appears
10 world units above the copied position and becomes the selected object.
The Edit menu's **Copy** and **Paste** commands do the same thing.

Hold **Shift** when starting a drag on a Move, Rotate or Scale gizmo to
duplicate the selected object and transform its copy immediately. The original
stays visible and unchanged. Release the mouse to finish; Escape or losing
mouse capture cancels. Clicking without moving creates no copy. Each completed
paste or Shift-drag is one undoable action, including the new selection.

Copies preserve the model, native object type, visible pose, bounds and
type-specific properties. They own separate placement pads. CCTV cameras and
drone guns also receive separate look-at pads, transformed along with the copy.
Existing object/pad IDs and script/path references keep their meanings.

Duplication creates standalone placed props. It does not duplicate characters,
character equipment, attached objects, script tags or mission links. A copied
door is independent of the original door's sibling link.

The clipboard captures the object as it was when copied. Moving, deleting or
undoing the source does not change that snapshot. Repeated paste uses that
same position plus 10 units; copy the new object first to build a stack. Copying
faces replaces the object clipboard and vice versa. Switching levels/projects
clears the clipboard. Text inputs retain ordinary Windows copy/paste.

Regression checks from the repository root:

```sh
python3 tools/geditor/tests/object_duplicate/run.py
python3 tools/geditor/tests/edge_extrusion/run.py
python3 tools/geditor/tests/face_clipboard/run.py
python3 tools/geditor/tests/knife/run.py
python3 tools/geditor/tests/view_modes/run.py
python3 tools/geditor/tests/setup_lifetime/run.py
```

The duplication harness uses synthetic model assets with production setup,
placement, transform, serialization and history code. The viewport harness
checks transform payloads, preview restoration, cancellation and no-op drags.
