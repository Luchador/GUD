# Geometry toolbar

The Vertex, Edge and Face dropdowns sit after the existing tool icons above the
viewport. Opening a dropdown keeps the active selection and tool. Unavailable
operations are grayed out. The controls wrap when the viewport is narrowed.
Buttons support mouse click, Tab/Space/Enter, and Down to open a dropdown.

| Menu | Operations |
| --- | --- |
| Vertex | Merge Vertices (M), Snap to Vertex (V), Paint Vertices (4) |
| Edge | Split Edge, Bridge Edges (B) |
| Face | Knife (K), Flip Face (Alt+N), Disconnect Face, Edit UVs, Hide Selected (H), Unhide All (Alt+H) |

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

## Knife

Select background faces in Face mode, then choose **Face > Knife** or press
**K**. The floating dialog edits a plane position in world units and a direction
vector, interpreted as the plane's perpendicular normal. The initial plane is
horizontal through the selection center. A red, two-sided plane previews the
cut at 40% opacity; it is depth-tested against the scene. The preview's finite
size does not limit the actual cut.

**Pick Points** temporarily reserves left clicks for background surface hits.
The first click sets the plane position. The direction from the first point to
the second sets the normal. The selected faces stay selected, and camera flight
remains available. Misses and coincident points leave picking active. **Cancel
Pick** or Escape restores the previous plane. Escape outside picking closes
the dialog. Changing the selection, active tool, level, or undo state also closes
it, so the preview cannot refer to an obsolete selection.

Press **Knife** to bisect the selected faces. Both sides are kept, triangulated,
and selected together. They retain their room, layer, texture, detail settings,
culling and draw state. New vertices interpolate UVs and RGBA along their source
edges. Adjacent selected faces share intersections on shared source edges;
authored UV/color seams remain separate. Unselected faces are untouched.

Coordinates and attributes round to native integer precision. Intersections
which round onto an existing endpoint reuse it. Fragments narrower than the
native grid may collapse; zero-area fragments and unused new vertices are
omitted. A cut which would invert geometry is rejected without changing the
document. Coplanar faces and cuts which only touch an edge/vertex are unchanged.
Undo/redo includes the complete operation and selection.

Knife validation: `python3 tools/geditor/tests/knife/run.py` covers interpolation,
shared edges and seams, Depot-style thin railing, multiple rooms/layers and
detail materials, allocation failures, native save/reload, and edit history.
