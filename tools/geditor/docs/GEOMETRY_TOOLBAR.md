# Geometry toolbar

The Vertex, Edge and Face dropdowns sit after the existing tool icons above the
viewport. Opening a dropdown keeps the active selection and tool. Unavailable
operations are grayed out. The controls wrap when the viewport is narrowed.
Buttons support mouse click, Tab/Space/Enter, and Down to open a dropdown.

| Menu | Operations |
| --- | --- |
| Vertex | Merge Vertices (M), Snap to Vertex (V), Paint Vertices (4) |
| Edge | Split Edge, Bridge Edges (B) |
| Face | Flip Face (Alt+N), Disconnect Face, Edit UVs, Hide Selected (H), Unhide All (Alt+H) |

Merge Vertices requires at least two selected BG vertices in vertex mode,
within one room. It creates one shared vertex at their average position and
averages RGBA and UV coordinates as well, rounded to native integer precision.
Each distinct vertex has equal weight; all faces using those vertices follow
the merge, across both BG layers. The removed vertex records are compacted.
The survivor keeps the lowest-index selected vertex's ID and native flag.
Differing attributes are allowed; choosing suitable merges is up to the user.

If triangles collapse, OK merges and removes those faces; Cancel leaves the
geometry unchanged. Existing degenerate faces are retained. The surviving
vertex stays selected when it has a visible incident face; otherwise selection
clears. Undo restores the geometry and original selection together. M does not
operate during camera flight, transform drags or vertex snapping, or while
editing text or working in the floating UV/model windows. Stan is not merged.

Split Edge requires one selected BG edge and separates its endpoint vertices
between incident faces, as the right-click command does; it does not subdivide
an edge. Disconnect Face works on one or more selected BG faces.

Press **B** to bridge the selected edges. The shortcut ignores text fields,
floating editors, camera flight, active transforms and key auto-repeat.

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

Merge validation: `python3 tools/geditor/tests/merge_vertices/run.py` checks
averaging, shared native vertices, collapsed triangles, Cancel, undo/redo,
failure rollback, survivor selection and the M shortcut.
