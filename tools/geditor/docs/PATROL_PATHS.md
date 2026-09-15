# Patrol path preview

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
