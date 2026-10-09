# Face copy and paste

In Face mode, select one or more background faces and press **Ctrl+C**.
Press **Ctrl+V** to paste copies at their copied positions, displaced upward
by 10 world units. This uses the same native-coordinate snapping as Move.
The copies become the selection, ready for another operation.

The Edit menu also provides **Copy** and **Paste**. Copy does not
modify the level or add an undo step. Each paste adds one undo step and saves
through the existing background/ROM pipeline.

Copies retain primary/secondary layer membership, winding, culling,
materials, texture wrapping, UVs and vertex RGBA. They receive new face and
vertex identities. Shared vertices within the copied set remain shared, while
the copies are independent of the originals.

The clipboard is a snapshot: later edits or deletion of the originals do not
alter it. Repeated paste uses the same copied positions plus the same offset;
copy the new selection first to build a stack. Copied background geometry
survives opening other levels within the same project, so you can copy in one
level and paste in another. Closing/changing projects or exiting the editor
clears it. Copying other scene data replaces it. Room renumbering clears copies
from that level; it does not clear copies brought from another level. Property inputs keep
normal Windows text copy/paste. Camera flight and active transforms cannot
copy or paste geometry.

In the source level, ordinary **Paste** keeps the original rooms. In another
level, it assigns all copies to the first selected background face's room, or
room 1 if no background face is selected. It keeps the copied world positions
plus the usual upward offset. Destination rooms must already exist.

For precise placement, right-click a background surface in Face mode and choose
**Paste Here**, directly below **Create Stan**. The copied geometry's bounding-box
center moves to the clicked point, and all copied faces go into the clicked
face's room. Existing selections do not change the destination. Orientation
is preserved; the geometry does not rotate to match the surface. A click over
empty space does not supply a paste location.

Cross-level paste preserves world-space size even if the levels have different
scales. Vertices are rounded once to the destination room's native grid. Image
IDs refer to the project's shared image catalog, so no extra texture import is
needed. Visibility commands, portals, stans and setup objects are not copied
with background faces. Add or adjust those separately as needed for the new
geometry.

Native draw groups are reused when their original state prefix remains
available. Otherwise the saved layer state is replayed and checked before
committing. Coordinate limits, incompatible inherited render state or failed
allocations reject the paste atomically, preserving geometry and clipboard.

Checks from the repository root:

```sh
python3 tools/geditor/tests/face_clipboard/run.py
python3 tools/geditor/tests/face_rooms/run.py
```

The tests exercise native Depot/Runway geometry, texture and transparency
snapshots, shared vertex identities, offset snapping, repeated paste,
compile/save/reload, undo/redo, rollback and text-control shortcut routing.
They also check transfers between Depot and Runway, different room counts and
level scales, fractional room origins, click placement/menu ordering and
compatibility with object Paste Here.
