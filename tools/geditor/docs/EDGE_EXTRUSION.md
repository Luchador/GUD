# Extruding background edges

In **Edge** mode with the **Move** tool (`W`), select one or more background
edges, hold **Shift**, then left-drag an axis handle. Each selected edge grows
one quad (two triangles). Release to commit; Escape, right-click, or losing
mouse capture cancels the preview. The new outer edges remain selected for
continued extrusion. Undo/redo treats the entire drag as one edit.

The source triangle supplies the room, primary/secondary layer, texture,
material, backface culling, vertex colors, and native draw state. A shared
edge uses its picked face where available; after a rebuild, the first visible
face containing both endpoint identities is used. Edges at UV seams retain
separate identities.

The existing UV mapping continues across a coplanar extrusion. When the quad
folds away from the source plane, its UVs retain the source mapping's scale.
An unmapped edge receives a basic mapping, preserving the original face's UVs.
Connected extruded endpoints share vertices when their source identities and
resulting UVs agree.

Positions snap to the level's native BG precision. A drag that rounds to zero
adds nothing. Zero-length edges, axis-parallel extrusions that have no area,
and positions or UVs outside native limits are rejected without changing the
geometry. Rotation, scale, stan edges, and setup objects keep their existing
transform behavior.

Save Project and Create ROM use the normal BG asset pipeline; no project
format migration is needed. This feature creates visual BG geometry. It does
not create stan collision tiles or portals.

Run the focused geometry, save/ROM, transaction, and viewport tests with:

```sh
python3 tools/geditor/tests/edge_extrusion/run.py
```
