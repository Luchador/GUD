# Patrol paths

## Create and edit routes

Open **Tools > Patrol Paths…**, click **New**, and add ordinary pads in travel
order. The window is modeless: the main window, viewport camera, object palette,
and other editing tools remain usable. Reopening the command focuses the same
window. **Insert Before**, **Remove**, **Up**, and **Down** edit the route order.
Check **Loop** to join the last point back to the first; otherwise the guard
reverses direction at each end. An edited route needs at least two different pads.

Click the **eyedropper** next to the pad dropdown, then click pad wireframes in the
viewport to append them to the selected route. Picking stays active for multiple
points; press **Escape** or click the eyedropper again to stop. Right-mouse flight
and wheel navigation remain available. Misses do not change the route. Picking
uses the same displayed, grounded pad boxes and occlusion as ordinary selection,
and never paints, snaps, drags, or opens a model. Bound pads are excluded.

Drag **Objects > Special > Pad** (the first button) onto a walkable floor to create
an ordinary pad. It snaps to that Stan floor, retains its tile name, becomes
selected, and appears in an already-open patrol window. Placement is undoable.
Standalone pads remain user-owned even before they are referenced by a route;
object placement cannot recycle them.

**Apply** commits one undoable setup edit and keeps the window open. **Save
Project** also applies a pending patrol draft before saving. Closing the window,
or exiting offers to apply or discard a pending draft. Switching levels/projects
offers to save the draft before leaving (using the normal project save).
Pad/object changes made while the window is open are preserved on Apply. A clean
draft follows undo/redo; a dirty draft whose source patrols or pads disappeared is
blocked with an instruction to close and reopen, preventing a stale overwrite.

The dropdown lists all ordinary, undeleted pads, including newly placed pads.
Existing native waypoint IDs are reused. When a route uses a pad without a
waypoint, Apply creates one with a singleton navigation group and reciprocal
connections along the authored route. Existing waypoint/group IDs and links are
preserved. Connections added to the navigation network remain after later route
edits, just like the original level's navigation links. Choose segments along
walkable routes; the editor does not infer shortcuts through walls. Share an
existing navigation pad to connect a new route to the level's existing network.

Select a guard and choose a route from the new **Patrol** dropdown in Properties.
Place the guard near a route point. This replaces that guard's starting behavior
with **Start patrol**, followed by the shared **Simple guard** detection loop.
The guard watches and listens for Bond and switches to normal combat when alerted.
It does not automatically resume patrolling after combat. **None** changes a
patrolling/custom guard to **Standard guard**. Viewing a custom or disabled
Action Block does not rewrite it. Use **Tools > Action Blocks** for more elaborate
behavior; routes can also be referenced by its **Start patrol** instruction.

Several guards can share one route. Generated starter blocks are reused, and
unused generated starters are removed when assignments change or routes are
applied. User-authored scripts are preserved. A route still referenced by a
guard/vehicle Action Block (even a disabled block) cannot be deleted; remove
assignments or edit those references first. Shared-script references are checked
when the base ROM supplies its shared AI catalog.

Routes retain stable native byte-sized IDs. Replaced setup tables are compacted.
Draft-only waypoints are discarded if their points/routes are removed before
Apply. Verification: `python3 tools/geditor/tests/patrol_edit/run.py` covers native
encoding, pad placement, navigation links, modeless drafts, history, save/reload,
allocation failures and repeated edits without setup growth.

## Viewport preview

The level viewport draws blue lines between the pads on each authored
guard or vehicle route. Pads belonging to a path are blue too, including
single-pad routes. Selected pads keep their white highlight. An arrow at each connection's midpoint shows travel direction.
Looping routes include the connection from their final pad back to their first.
Back-and-forth routes use double-headed arrows, since guards reverse at each end.
Shared connections draw once; routes using a connection in opposite directions
also produce a double-headed arrow.

These are patrol routes, not the general navigation graph or a simulation of a
guard's current Action Block. Connections use the patrol's waypoint-to-pad mapping.
Lines follow pad movements, including the drag preview, and refresh after undo,
redo, or level changes. Like pad markers, they remain visible when object models
are hidden and are occluded by visible level geometry. Arrowheads face the camera
and maintain an approximately constant screen size, capped for short connections.

This preview does not modify setup files, patrol behavior, or exported ROMs.
Malformed route tables are reported without preventing the rest of the level
from displaying.

Verification: `python3 tools/geditor/tests/patrol_paths/run.py`
Optionally append a project directory to check all native `.set` route tables in
its `setup` folder against an independent traversal calculation.

## Pad Preview

**View > Pad Preview** is on by default; press **P** to toggle it. With the preview
on, ordinary and bound pad origins are displayed at the height of their resolved
stan tile. Named tiles take priority over the nearest-walkable fallback, so stacked
floors and slopes use the same placement queries as the character preview. If no
usable tile is found, the pad stays at its authored position.

Pad boxes, picking, gizmos and route endpoints use the same preview positions.
Horizontal pad drags follow the floor. The coordinate fields continue to show the
authored values, and previewing never changes those values or the saved setup.
Turn the preview off to inspect or edit authored pad heights. The toggle survives
level changes during the current editor session; typing P in a text input does
not toggle it. Pad previews also refresh when stan geometry changes.
