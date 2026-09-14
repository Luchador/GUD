# Geometry toolbar

The Vertex, Edge and Face dropdowns sit after the existing tool icons above the
viewport. Opening a dropdown keeps the active selection and tool. Unavailable
operations are grayed out. The controls wrap when the viewport is narrowed.
Buttons support mouse click, Tab/Space/Enter, and Down to open a dropdown.

| Menu | Operations |
| --- | --- |
| Vertex | Snap to Vertex (V), Paint Vertices (4) |
| Edge | Split Edge, Bridge Edges |
| Face | Flip Face (Alt+N), Disconnect Face, Edit UVs, Hide Selected (H), Unhide All (Alt+H) |

Split Edge requires one selected BG edge and separates its endpoint vertices
between incident faces, as the right-click command does; it does not subdivide
an edge. Disconnect Face works on one or more selected BG faces.

Bridge Edges requires exactly two disjoint BG boundary edges in the same room
and layer. It adds two triangles, with winding opposite each adjoining source
edge. Interior edges, shared endpoints, and twisted or zero-area bridges are
rejected. The first edge in the selection supplies the new faces' material, culling and
draw state. Existing endpoint UVs and RGBA are retained; Edit UVs can adjust the
new surface afterward. The new faces are selected in face mode. Undo restores
the original geometry and edge selection.

Existing Edit/View menu entries, shortcuts and viewport context menus remain
available. Edge extrusion still uses Shift-drag on a Move gizmo arrow.

Validation: `python3 tools/geditor/tests/geometry_toolbar/run.py` checks bridge
geometry, native save/reload, ROM vertex batches, undo/redo and failure rollback,
as well as source edge lookup, menu availability and toolbar wrapping.
