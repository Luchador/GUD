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

Duplication creates standalone placed props or characters. Script tags,
mission links and attached props are not duplicated. A copied door is
independent of the original door's sibling link.

## Characters and Paste Here

Select a character and press **Ctrl+C**, then right-click a background triangle
and choose **Paste Here**. The character keeps its facing direction and stands
on the Stan floor at the clicked X/Z position; the clicked height chooses the
floor when rooms overlap. A position with no valid Stan placement is rejected
without leaving a partial copy. Props use the same menu item to contact the
clicked surface, including walls and ceilings.

Character copies preserve the full native guard settings, including body,
head, flags and starting AI list. Each copy gets a new character ID and a
separate pad. Its initial right/left weapons, concealed weapons, difficulty
variants, hats and guard attributes are copied and assigned to the new ID.
Deleted equipment is omitted. Shared Action Blocks and references to other
characters/pads remain shared; explicit IDs inside scripts are not rewritten.

**Ctrl+V** also works for characters, placing the copy 10 world units along +X
from the snapshot, grounded on Stan. **Shift-drag** can duplicate and move or
rotate a character using its existing transform rules. Characters cannot be
scaled. Copying and pasting uses the same snapshot, selection and undo behavior
as props.

The clipboard captures the object as it was when copied. Moving, deleting or
undoing the source does not change that snapshot. Repeated paste uses that
same position plus 10 units; copy the new object first to build a stack. Copying
faces replaces the object clipboard and vice versa. Switching levels/projects
clears the clipboard. Text inputs retain ordinary Windows copy/paste.

Regression checks from the repository root:

```sh
python3 tools/geditor/tests/object_duplicate/run.py
python3 tools/geditor/tests/character_weapons/run.py
python3 tools/geditor/tests/edge_extrusion/run.py
python3 tools/geditor/tests/face_clipboard/run.py
python3 tools/geditor/tests/knife/run.py
python3 tools/geditor/tests/view_modes/run.py
python3 tools/geditor/tests/setup_lifetime/run.py
```

The duplication harness uses synthetic model assets with production setup,
placement, transform, serialization and history code. The viewport harness
checks transform payloads, preview restoration, cancellation and no-op drags.
