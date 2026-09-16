# Deleting stan tiles

Show Stan Geometry, switch to Face mode, select one or more stan tiles, and
press **Delete** while the viewport has focus. Shift-click adds tiles and
Control-click removes them from the selection. Vertex/Edge mode does not delete
whole tiles. Text fields keep their normal Delete behavior.

Deletion is one undoable edit. It removes the selected collision polygons,
clears their selection, and refreshes pad placement and object grounding/shading.
Undo restores both the tiles and their selection; redo removes them again.
Save Project writes the edited `.stan` resource, which Create ROM reads normally.

Surviving tiles retain their IDs, coordinates, colors, flags, and room ordering.
Native records are compacted, edge links are relocated, and links to removed
tiles become solid boundary edges. Header tile pointers and the terminator/footer
are retained in a valid layout. Setup pad names are unchanged; a pad whose named
tile was deleted uses the existing runtime placement fallback.

At least one stan tile must remain in the level, as required by the current
loader. Deleting every tile in an individual room is allowed. The operation
fails without changing the file if references are invalid or allocation fails.

Validation: `python3 tools/geditor/tests/stan_deletion/run.py` checks mixed-size
tiles, every nonempty survivor combination in a linked fixture, first/last
removals, header pointers, native save/reload, ROM resource reads, allocation and
viewport rebuild failures, selection/history, and the repository's Depot tiles.
