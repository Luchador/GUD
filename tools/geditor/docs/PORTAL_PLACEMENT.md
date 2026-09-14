# Add Portal

Drag **Portal** from **Objects > Special** into the viewport. The **Add Portal**
dialog opens with its center at the drop position. A background hit supplies the
first room; an empty-space drop places the center in front of the camera.

Choose the two rooms to connect, the rectangle's orientation and its size.
Room numbers are decimal. Width and height/depth are in meters; the center uses
the same world coordinates as the editor's position fields. Defaults are 2 meters
wide and 3 meters high. Available planes are:

| Plane | Width axis | Height/depth axis |
| --- | --- | --- |
| XY, vertical | X | Y |
| YZ, vertical | Z | Y |
| XZ, horizontal | X | Z |

Place the rectangle at the shared opening through which one room should reveal
the other. Click **Create**. The editor enables portal visibility, selects the
new portal and shows its room links in Properties. The links remain editable.
This version sets geometry in the creation dialog; portal geometry does not yet
support the move, rotate or scale gizmos. Undo and recreate a portal to change
its placement or dimensions. Creation supports **Undo/Redo** as one action;
**Cancel** does not change the level.

New portals are written into the native BG file when the project is saved and
are included by Create ROM. They append to the portal table, preserving existing
indices and original polygon addresses used by visibility commands. Ordinary
saves reuse their storage. Undo/save/redo is supported even when the saved file
and current history snapshot contain different numbers of portals. Repeatedly
undoing, saving and replacing portals can leave unused metadata in the BG file;
it is retained to keep original addresses stable.

The existing native limit is 199 portals. Creation requires two distinct rooms
and an editable portal table. Portals affect room visibility; they do not add
walkable Stan geometry. This tool does not automatically alter Depot's tank
rooms 20–24; their connections and placement must be authored.

## Validation

Run from the repository root:

```
python3 tools/geditor/tests/portal_placement/run.py
python3 tools/geditor/tests/browser_drag/run.py
python3 tools/geditor/tests/portals/run.py
```

These cover rectangle geometry, native BG save/reload, existing pointer and
script preservation, repeat saves, history changes across saves, capacity limits,
drag capture/cancellation and rollback on allocation or display failure. The BG
output also passes the vertex-batch validation used by Create ROM. Native tests
stub window-system calls; they do not replace a Windows UI or N64 smoke test.

Manual check: add a portal, inspect its rectangle and room links, undo/redo,
save/reopen, then create a ROM and check visibility while approaching the opening
from each linked room. Test Depot's tanks from all nearby outdoor rooms to find
which additional connections they need.
