# Face room assignment

In Face mode, the Properties panel's **Room** field lists every existing room,
including empty rooms. Choose a room from the list, or type its number and press
**Enter**. **Escape** restores the current value. A selection spanning multiple
rooms displays **Mixed**; choosing a number moves all selected faces there.

Moving faces preserves world positions, UVs, vertex colors, face winding,
backface culling, material settings and primary/secondary layers. Vertices shared
by the moved faces remain shared; they are copied into the destination room.
The selected faces remain selected, and the move supports Undo and Redo.

Original room slots and vertex records remain in place, even when all their
faces are moved away. The compiler retains a valid primary display list for an
empty room. Room numbers, portals, Stan geometry, pads and object assignments
are not changed by this operation.

For Depot's tank, select its faces in room 22 (Select > Select Room can expand
the selection), then set Room to **23**. Save with **Ctrl+S**. **Ctrl+T** opens or
activates the UV Editor. Both shortcuts also work from the floating editors.

Transfers fail without changing the document if native coordinates cannot
represent the same positions in the destination, allocation fails, or inherited
render state conflicts with the destination pipeline.

Regression check: `python3 tools/geditor/tests/face_rooms/run.py`. This covers
Depot's actual 64 tank faces, native save/reload with room 22 empty, material
preservation, shared vertices, undo/redo, failure rollback and room-field input.
