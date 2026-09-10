# Portal properties

Enable **Portals** in Visibility, then click a visible portal in face, edge, or
vertex selection mode. The selected polygon turns gold. Picking is double-sided
and respects foreground level geometry and objects. The translucent stan
overlay does not block portal selection.
Vertex paint and vertex snapping keep their existing click behavior.

Properties shows the native portal index, polygon point count, extra-margin
status and value, and the number of connections sharing the polygon. The margin
is decoded using the game's `bgGetPortalMargin` formula and converted from BG
units to world units using the level scale. The margin is read-only.

Use the **Room 1** and **Room 2** lists and click **Apply connections** (or press
Enter with a closed list). Both endpoints are applied together, so they can be
swapped. Only existing rooms 1 through the level's final room are accepted,
within the native byte limit, and the two rooms must differ. Room 0 is unused.

Repeated clicks cycle the nearest overlapping table entries, including entries
that share one polygon but have different room connections or margins. Only the
selected entry is edited. Ctrl-click deselects it; Escape in the viewport clears
selection. Hiding portals clears portal selection. Portal geometry has no
translation, rotation, scaling or deletion controls in this change.

Each applied change is one **Change Portal Rooms** undo step and marks the
project unsaved. BG history snapshots own the portal metadata, so portal edits
and face edits can be interleaved, undone and redone, including after a save.
The compiler writes the room IDs into the BG segment's native eight-byte table
records, preserving polygon pointers, table order, flags, margin bytes, and
other metadata. Project saves and the existing ROM builder use that segment.
No new project or game-engine changes are required.

Malformed portal tables retain the existing load warning and remain uneditable;
the editor can still open the level geometry and preserve the original table.

## Verification

```sh
python3 tools/geditor/tests/portals/run.py
```

The tests use production BG loading, compilation, saving and history code with
address/undefined-behavior sanitizers. They check endpoint validation, shared
polygons, unchanged metadata, save/reload, undo after saving, rollback and mixed
face/portal edits. The production viewport ray picker is tested for double-sided
hits, occlusion, near/far clipping, hidden portals and overlapping-entry cycling.
All 256 margin encodings are compared with the game's actual decoder.
Windows rendering and an in-game check remain manual.
