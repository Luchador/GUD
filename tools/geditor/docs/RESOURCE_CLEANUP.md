# Save and export cleanup

Save Project refreshes ordinary and bound pads' native STAN names against the
collision data being saved. Create ROM repeats this for every setup with a
unique level/STAN pairing, including unopened levels and multiplayer setups.
The exporter first resolves resource aliases, then uses the selected STAN bytes
and the level's scale. It preserves the runtime's existing placement choice;
it does not move pads, choose a different floor, or remove authored pads.
Unresolved pads retain their existing names and positions and are reported.

Background files are rebuilt from reachable data: the room table, active portal
connections, shared polygon records, visibility commands and their polygon
references, and the three native streams per room. Retired portal tables,
orphan polygons, old stream allocations and trailing slot bytes are discarded.
Polygon aliases, portal order, room IDs, flags, margins, and native geometry
remain intact. Visibility operands and stream pointers are relocated together.
Empty rooms retain their bounds vertices, which the engine needs for portals.

Compaction operates on serialization copies. Live selections, portal identities
and undo/redo snapshots continue to use their original offsets. A fresh load
uses the compact file's offsets. Repeated saves and exports are stable.

The [batch compiler](TEXTURE_BATCHING.md) also packs static vertex loads, reuses
exact native vertex attributes, and removes redundant state writes. Transparent
and decal triangle order is preserved. Empty-room bounds remain unchanged.

STAN cleanup retains all tiles, the terminator and the recognized `unstric`
footer. It discards old bytes beyond that footer when its four pointer fields
are NULL. Unknown footer extensions remain intact. Untouched stock placeholder
resources that do not contain native STAN are preserved.

ROM repacking uses the compact setup, BG and STAN lengths, aligned to 16 bytes,
instead of retaining their previous slot capacities. File-table aliases and
the resource-segment manifest range are updated; reclaimed trailing space is
zeroed. Later exports can reuse that cleared padding without crossing another
manifest segment. The N64 ROM retains its supported capacity, so its overall `.z64` size
need not decrease. The source ROM and project assets are not changed by export.

## Verification

```sh
python3 tools/geditor/tests/resource_cleanup/run.py [local-GUD-ROM.z64]
python3 tools/geditor/tests/setup_compaction/run.py [local-GUD-ROM.z64]
python3 tools/geditor/tests/project_rebase/run.py
```

The optional ROM test compares every resource across two exports, verifies
native BG streams, portal aliases and script polygons, and independently checks
that setup content changes only in pad names. The tests run with address and
undefined-behavior sanitizers. Windows interaction and in-game load timing still
require runtime checks.
