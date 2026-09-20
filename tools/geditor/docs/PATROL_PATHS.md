# Patrol paths

## Create and edit routes

Open **Tools > Patrol Paths…**, click **New**, and add navigation pads in travel
order. **Insert Before**, **Remove**, **Up**, and **Down** edit that order. Check
**Loop** to join the last point back to the first; otherwise the guard reverses
direction at each end. An edited route needs at least two different pads.
**Apply and Close** saves the draft into the current level and refreshes the
viewport. **Cancel** discards the draft. Applying is one undoable setup edit.

Select a pad in the viewport to read its **Pad index** in Properties. If that pad
belongs to the navigation network, it is preselected in the Patrol Paths window.
The pad dropdown lists both pad IDs and their native waypoint indices. Routes
use existing navigation waypoints, not object placement pads or bound pads;
this tool does not create a new navigation network. Choose points along a
walkable corridor rather than drawing a straight line through obstacles.
Existing pad translation controls move route points; moving a shared pad also
changes every other reference to it.

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

Routes retain stable native byte-sized IDs. The editor keeps the waypoint and
navigation-group tables intact and compacts replaced route/script data so edits
do not continually enlarge the setup. Paths and assignments survive Save Project,
undo/redo, and Create ROM. Reload the level in-game to use its new starting AI.

Verification: `python3 tools/geditor/tests/patrol_edit/run.py`. Add `--stock` to
compile and check the repository's solo setup corpus when IDO is installed. The suite
covers native encoding, ordering/loop edits, reused assignments, stale requests,
reference protection, save/reload, export, history, allocation failure atomicity,
and repeated edits without setup growth. Inspector/frame routing tests are in
`python3 tools/geditor/tests/character_weapons/run.py`.

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
