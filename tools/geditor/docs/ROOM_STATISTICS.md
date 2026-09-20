# Room statistics in Level Settings

Open **Settings > Level Settings > Rooms** to see every authored background
room, including empty rooms. The table lists **Primary tris**, **Secondary
tris**, **STAN tiles**, **Objects**, and **Characters**. Click a column heading
to sort numerically; count columns initially show the largest counts first.
The footer totals include the Unassigned row when it is present.

Counts come from the current editable level documents, including unsaved edits,
hidden geometry and all authored difficulty variants. They refresh after edits,
undo/redo, level changes and reopening Level Settings. Selection-only changes do
not repeat the spatial queries. Inspecting counts never changes the level or its
dirty state.

- Primary and secondary triangle counts use each room's current BG faces and
  layer. These are background triangles, not model triangles.
- STAN tiles use their authored room IDs. Room 0 is reserved; tiles with room 0
  or a room absent from the BG appear under **Unassigned**.
- Each live placed object or character counts once, using its placement pad's
  resolved STAN room. The resolver uses the current pad position and tile data,
  including the existing STAN reference for stacked floors. Doors use their
  direct bound-pad index; other bound placements use the encoded pad index.
- Deleted models and carried/contained object records are excluded. Attached
  monitors inherit their owner's placement room. An unresolved pad, missing
  pad/STAN, invalid position, or broken attachment appears under **Unassigned**.
  Objects spanning room boundaries count in their placement room once.

These are authored placement counts, not live gameplay counts: spawning, guard
movement, carried equipment and runtime difficulty selection can change what
is present while playing.

Run the native checks and Windows build from the repository root:

```sh
python3 tools/geditor/tests/room_stats/run.py
python3 tools/geditor/tests/selection_history/run.py
python3 tools/geditor/tests/intro_equipment/run.py
make -C tools/geditor/src
```

The room test optionally accepts an extracted Depot audit directory containing
`rom-depot.seg`, `rom-depot.stan`, and `setup/UsetupdepoZ.set`. It validates totals
against those native assets. Synthetic tests cover layers, empty rooms, sorting,
stacked floors, stale pad references, doors/bound pads, attached monitors/cycles,
Unassigned records, live changes and allocation failures under ASan/UBSan.
