# Geometry toolbar

The Vertex, Edge and Face dropdowns sit after the existing tool icons above the
viewport. Opening a dropdown keeps the active selection and tool. Unavailable
operations are grayed out. The controls wrap when the viewport is narrowed.
Buttons support mouse click, Tab/Space/Enter, and Down to open a dropdown.

| Menu | Operations |
| --- | --- |
| Vertex | Merge Vertices (M), Snap to Vertex (V), Paint Vertices (5) |
| Edge | Bisect Edge (Ctrl+Q), Split Edge, Bridge Edges (B) |
| Face | Knife (K), Flip Face (Alt+N), Disconnect Face, Edit UVs, Hide Selected (H), Unhide All (Alt+H) |

In **Face mode**, left-drag a selection box to select faces with a visible pixel
inside the box. Partly exposed faces qualify; faces fully covered within that
box do not, even if another part is visible elsewhere. The test respects the
current Primary/Secondary visibility, hidden faces, backface culling, object
occlusion, texture cutouts and decal draw order. It also applies to stan tiles
and portal faces when those are the active selection type. Shared portal room
links remain selected together.

**Shift** adds visible hits and **Ctrl** removes them. A click without dragging
keeps ordinary click selection; Escape cancels an unfinished box. Vertex and
edge boxes still select through geometry. In Wireframe mode, face boxes follow
face clicking and treat texture holes as part of the editable face; Untextured
mode uses vertex alpha without sampling images. Selection outlines, gizmos and
other editor guides do not obscure selectable faces.

The viewport renders face IDs into its back buffer and reads only the box area,
using the same clipping, depth, culling and alpha rules as the surface pass.
The ID buffer is never displayed. GL state is restored afterward, and a failed
visibility readback leaves the existing selection intact. Run the CPU lifecycle
and real OpenGL pixel regressions with:

```sh
python3 tools/geditor/tests/box_selection/run.py
python3 tools/geditor/tests/box_selection/run_visible.py
```

The pixel tests need Mesa EGL/OpenGL and GL headers on Linux; `GL_INCLUDE` can
point to an alternative include directory. Native drag/highlight behavior still
needs a Windows UI smoke test.

**Snap to Vertex** is a checked toggle available in Vertex mode (1). Press **V**
or choose the menu item to turn it on or off. Click the source vertex, then
the destination vertex. Switching selection modes turns snapping off; Room
remains 4 and Vertex Paint remains 5. There is no separate snap toolbar button.
The shortcut ignores text inputs, modified V shortcuts, and key auto-repeat;
snapping cannot be toggled during camera flight or a transform drag. Undo/Redo
restores the snap selection and toggle together.

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
operate during camera flight, transform drags, or while
editing text or working in the floating UV/model windows. Stan is not merged.

Split Edge requires one selected BG edge and separates its endpoint vertices
between incident faces, as the right-click command does; it does not subdivide
an edge. Disconnect Face separates the selected BG faces from unselected faces
while keeping their existing connections to each other. Only vertices shared
with unselected faces are copied; existing seams remain separate. The native
save optimizer can merge identical vertices again if their attributes have
not changed after disconnecting.

Press **B** to bridge the selected edges. The shortcut ignores text fields,
floating editors, camera flight, active transforms and key auto-repeat.

Bridge Edges requires exactly two BG boundary edges on different faces in the
same room and layer. Edges sharing one vertex add one triangle; disjoint edges
add two. The new winding runs opposite each adjoining source edge. Interior
edges, coincident edges, incompatible winding, and twisted or zero-area bridges are
rejected. The first edge in the selection supplies the new faces' material, culling and
draw state. Existing endpoint UVs and RGBA are retained; Edit UVs can adjust the
new surface afterward. The new faces are selected in face mode. Undo restores
the original geometry and edge selection.

For **Stan edges**, Bridge Edges also uses **B**. Edges sharing one endpoint
create one triangle; separate edges create two. The new tiles link back to
the selected source boundaries and are selected in Face mode. Different source
rooms are allowed; the new tiles inherit one source room, which can be changed in Properties. See
[Bridging Stan edges](STAN_BRIDGING.md) for details.

Existing Edit/View menu entries, shortcuts and viewport context menus remain
available. BG and Stan edge extrusion uses Shift-left-drag on a Move gizmo
arrow. Each selected Stan boundary edge creates two linked triangular tiles;
the outer edges stay selected for continued extrusion. See
[Extruding Stan edges](STAN_EXTRUSION.md).

In **Face** mode, selected **Stan tiles** support **Ctrl+C** and **Ctrl+V**
(and Edit → Copy/Paste). Pasted tiles appear **10 native units above** their
copied positions, including when Editor Settings displays world units. Copies
retain room, tile type, color and links between copied tiles; links to tiles
outside the selection become open boundaries. Each copy gets fresh identities.
The clipboard is a snapshot and is cleared when switching levels or projects.

Hold **Shift** when pressing a Move, Rotate or Scale gizmo with Stan tiles
selected to duplicate and transform them. This includes the uniform-scale
center handle. The originals stay in place, and the copies are selected on
release. A complete gesture is one Undo action; Escape, right-click, capture
loss or a drag ending at its starting value creates no copies. Duplication
does not replace the clipboard. Transforms outside the native coordinate
range, or those collapsing a tile to a line/point, are rejected atomically.

In **Scale** mode, the white cube at the gizmo center scales all three axes
together around the existing selection pivot. It turns gold on hover. Hold the
left mouse button and drag right or up to enlarge, or left or down to shrink.
This works for objects and selected faces or edges, including rotated objects.
The colored axis handles still scale one axis at a time.

A 90-pixel horizontal or vertical drag increases the scale by 100%. Combined
horizontal and vertical movement adds together; opposite directions cancel.
Scaling uses 1% steps and stops at 1% of the starting size. Releasing the button
commits through the existing undo history; Escape or right-click cancels the
drag. Returning to the starting position makes no edit. Shift-dragging the
center cube on an object creates a uniformly scaled copy.

Uniform-scale validation: `python3 tools/geditor/tests/uniform_scale/run.py`
checks the shipped handle geometry, center picking/highlighting, object and
face/edge selections, rotated axes, preview/commit agreement, cancellation,
no-op drags and duplication with AddressSanitizer and UndefinedBehaviorSanitizer.
Window and OpenGL calls are stubbed in this host test.

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

**Pick Point** sets the position with one click on a background surface. The
default plane lies parallel to the X/Z axes (normal 0, 1, 0). Use **Move (W)** or
**Rotate (E)** to adjust the plane with the existing gizmos, or enter coordinates
and angles in the transform panel. The preview and gizmo stay centered on the
picked position. Scaling is unavailable for the infinite cutting plane.

The selected faces stay selected while the knife is open; moving and rotating
the preview does not edit geometry or create undo steps. Camera flight remains
available. A missed pick leaves picking active. **Cancel Pick** or Escape cancels
point picking. Escape during a gizmo drag restores the plane before that drag;
otherwise Escape closes the dialog. Losing focus or capture also cancels a drag.
Changing the selection, active selection tool, level, or undo state closes the
dialog, so its preview cannot refer to an obsolete selection.

Hold **Ctrl** during a rotation drag to snap its angle to **10-degree intervals**.
This applies to the knife, background geometry, Stan, props, characters, pads,
markers, and UV rotation rings. Ctrl can be held before grabbing a handle or
pressed/released during the drag. The snap is relative to the drag's starting
orientation. Releasing Ctrl restores normal precision; typed angles remain exact.

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
The same suite checks single-point placement, preview transforms/cancellation,
scene isolation and Ctrl snapping in both 3D and UV drags with window calls stubbed.

## Bisect Edge

Select one background or Stan edge in Edge mode (`2`), then choose
**Edge > Bisect Edge** or press **Ctrl+Q**. One triangle becomes two triangles,
with a new vertex at the selected edge's midpoint. All triangles sharing that
source edge are divided together; a shared edge between two triangles creates
four triangles. Background UVs and RGBA are averaged at the midpoint, and
faces retain their room, layer, material, winding and draw state. Coincident
background edges with separate vertex identities (such as UV seams) remain
independent.

Stan uses its existing linked vertex identities to find incident tiles.
Triangular tiles become two triangles. Larger convex tiles become two convex
polygons, cut from the edge midpoint to an opposite perimeter vertex. Internal
and external links are updated, including incoming and one-way connections.
Room, tile color, flags, and walkable/non-walkable classification are preserved.
Unlinked overlapping tiles and floors at other heights remain separate.

Positions, UVs and colors round to native integer precision. A midpoint that
collapses a triangle or an invalid Stan boundary connection rejects the entire
edit. The operation supports undo/redo, Save Project, and Create ROM. The two
new background edge halves stay selected; for Stan, the first half is selected.
The shortcut leaves text fields, floating editors, camera flight and active
transforms alone, and runs only once per key press.

Validation: `python3 tools/geditor/tests/bisect_edge/run.py` covers background
subdivision, attributes, seams, native save/reload, history, rollback and Ctrl+Q.
`python3 tools/geditor/tests/stan_topology/run.py` covers Stan bisection, linked
neighbors, both-way walking, native ID flags, room regrouping and export, and
Depot's actual floor/stair link. `geometry_toolbar/run.py` checks menu routing.

Stan clipboard validation: `python3 tools/geditor/tests/stan_clipboard/run.py`
checks native links, attributes, save/reload, coordinate scale, history and
failure rollback. `uniform_scale/run.py` also checks Stan duplicate gestures.
