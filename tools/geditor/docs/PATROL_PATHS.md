# Patrol path preview

The level viewport draws light green lines between the pads on each authored
guard patrol route. An arrow at each connection's midpoint shows travel direction.
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
