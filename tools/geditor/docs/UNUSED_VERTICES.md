# Automatic unused-vertex cleanup

Save Project and Create ROM remove unreferenced vertices from populated BG
rooms automatically. Saving also cleans an older project file when no new edit
has been made. ROM creation cleans all selected BG resources, including levels
not opened during the current session, after resolving shared resource aliases.

Cleanup operates on a copy of the saved data. It preserves live vertex indices,
viewport and UV selections, and undo/redo snapshots. It checks both primary and
secondary triangles, compacts each room's vertex block, and updates vertex loads
and triangle cache indices together. Surviving native positions, UVs, colors,
flags and vertex sharing are unchanged. It never merges coincident vertices.

Room IDs stay unchanged. The vertex pass clears removed records and excludes
them from the block length. The subsequent [resource cleanup](RESOURCE_CLEANUP.md)
packs live streams and portal metadata and relocates their addresses in the
saved copy. This is particularly important because GoldenEye computes room
bounds from every vertex in the block, even if no triangles use it. In the Depot
regression, six orphaned vertices changed room 42's center and made the engine
reject the portal between rooms 41 and 42 in both directions.

Empty rooms retain their bounds-only vertices, since the game uses those bounds
for portal orientation. Real point triangles are preserved, including a harmless
coincident cache-padding vertex where required by TRI4's zero-slot convention.
Rooms containing nested display lists or unsupported vertex-based cull/branch
commands are left intact rather than guessing at their additional references.

Run `python3 tools/geditor/tests/unused_vertices/run.py` for native BG checks,
the Depot orphan regression, mixed-layer references, point triangles, empty
rooms, repeated cleanup and input preservation. Optional arguments are additional
uncompressed `.seg` files to check.
