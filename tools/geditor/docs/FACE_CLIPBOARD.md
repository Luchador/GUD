# Face copy and paste

In Face mode, select one or more background faces and press **Ctrl+C**.
Press **Ctrl+V** to paste copies at their copied positions, displaced upward
by 10 world units. This uses the same native-coordinate snapping as Move.
The copies become the selection, ready for another operation.

The Edit menu also provides **Copy Faces** and **Paste Faces**. Copy does not
modify the level or add an undo step. Each paste adds one undo step and saves
through the existing background/ROM pipeline.

Copies retain room and primary/secondary layer membership, winding, culling,
materials, texture wrapping, UVs and vertex RGBA. They receive new face and
vertex identities. Shared vertices within the copied set remain shared, while
the copies are independent of the originals.

The clipboard is a snapshot: later edits or deletion of the originals do not
alter it. Repeated paste uses the same copied positions plus the same offset;
copy the new selection first to build a stack. The clipboard is local to the
open level and clears when changing levels or projects. Property inputs keep
normal Windows text copy/paste. Camera flight and active transforms cannot
copy or paste geometry.

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
