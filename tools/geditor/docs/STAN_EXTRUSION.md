# Extruding Stan edges

Show **Stan Geometry**, select one or more open boundaries in **Edge mode (2)**,
and use **Move (W)**. Hold **Shift**, then left-click and drag a translation
gizmo axis. Each selected edge creates **two new Stan tiles with three points
each**. The source edge stays fixed while the new surface is previewed.

Release the mouse to commit. The new outer edges remain selected in Edge mode
so the next Shift-drag continues the strip. Escape, right-click, or losing mouse
capture cancels. A drag that rounds to zero in native Stan coordinates creates
nothing and adds no undo step.

The source tile supplies each new tile's room, color, special flags, and native
classification. The first new triangle links back to the source boundary in
both directions; the two new triangles link across their diagonal. Adjacent
selected edges with a shared editing vertex also link their new side edges,
including across different rooms. Coincident vertices belonging to unrelated,
unlinked tiles remain independent. Other new boundaries stay open; use Link
Tiles to connect them to existing geometry when needed.

Only open boundaries can be extruded. Existing links and coincident source
edges are rejected, including incoming-only links. Extrusions parallel to the
edge, inward/reversed floors, invalid branching selections, and positions
outside native limits are also rejected without changing the document. Vertical
risers and slopes are supported. Preview and commit use the same native
coordinate rounding and validity checks.

One drag creates one **Extrude Stan Edges** undo step, including all selected
edges. Undo/redo restores the tile records, links and edge selection together.
Save Project and Create ROM use the existing Stan pipeline, including grouping
tiles by room for the game's room scans. No project format migration is needed.

Validation:

- `python3 tools/geditor/tests/stan_extrusion/run.py`: native geometry and
  preview agreement, coordinate rounding, all six axis directions, ramps,
  connected strips, collision traversal, inherited attributes, continued
  extrusion, grouped save/reload and ROM bytes, controller history, and atomic
  allocation/rebuild/selection/commit failures.
- `python3 tools/geditor/tests/edge_extrusion/run.py`: existing BG extrusion
  plus Stan gesture routing, preview offsets, source preservation, cancellation,
  no-op drags and payload lifetime through commit.
- `python3 tools/geditor/tests/geometry_toolbar/run.py`: existing BG bridge,
  object duplication, selection queries, geometry menus and shortcut routing.

These checks use address/undefined-behavior sanitizers. Window/OpenGL calls are
stubbed; interactive preview and in-game traversal still need user validation.
