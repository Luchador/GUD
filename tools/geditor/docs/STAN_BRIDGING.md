# Bridging Stan edges

Show **Stan Geometry**, switch to **Edge mode (2)**, and select two open Stan
edges on different tiles. **Shift-click** adds the second edge. Press **B** or
choose **Edge > Bridge Edges**.

When the selected edges share exactly one endpoint, the command fills the gap
with **one triangular tile**, linked to both source edges in both directions.
Separate edges create **two triangular tiles**, linked across their diagonal
and to their respective source edges. Remaining outside edges stay unlinked boundaries. Exact
native endpoints are reused, so the new surface joins the existing tile edges
without rounding cracks. Linked endpoints participate in shared-vertex editing.
Flat gaps, ramps, and vertical stair risers are supported. Using triangles also
allows the four endpoints to have different heights without a nonplanar tile.

The first resolved edge supplies the new tiles' **room**, color, special flags,
and walkable/non-walkable classification. The source tiles may belong to
different rooms. The new tiles are selected in **Face mode** after bridging;
change their room in **Properties** if necessary. Existing tile properties and
unrelated connections remain unchanged.

The edges must be open boundaries and face the gap. They can have three or four
distinct endpoints; a shared point must match exactly in native XYZ coordinates.
Both edge directions must agree with the new triangle's winding.
Edges already linked or coincident with another tile edge are unavailable
for bridging. Use **Link Tiles** for edges already sharing their endpoints.
Crossed, reversed, collapsed, or source-overlapping bridges are rejected.
Validation, allocation, display, or history failures leave the edit uncommitted.

Each bridge is one **Bridge Stan Edges** undo step. Undo restores the original
edges and selection; redo restores the bridge and selected new tiles. Save
Project stores the native links and groups tiles by room for the game's room
scans; Create ROM uses the saved Stan resource. B retains its existing BG bridge
behavior when BG edges are selected. Text inputs, floating editors, camera
flight, and active transforms retain their existing shortcut behavior.

Validation:

- `python3 tools/geditor/tests/stan_bridge/run.py`: one- and two-triangle native geometry, two-way
  crossing over flat/ramped/vertical bridges, mixed room choices, attributes,
  reciprocal links, grouped save/reload and ROM resource reads, unique IDs,
  invalid input, allocation failures, production viewport selection and
  controller rollback, and undo/save/redo.
- `python3 tools/geditor/tests/geometry_toolbar/run.py`: BG bridge regression,
  selection-aware menu availability, Stan command routing, and scoped B input.
- `python3 tools/geditor/tests/stan_topology/run.py`: existing Stan split,
  merge, bisection, links and visibility, including Depot floor/stair data.

The native tests run with address/undefined-behavior sanitizers. Window/OpenGL
calls are stubbed; check the editor interaction and cross the new bridge in-game
on the edited level after creating a ROM.
