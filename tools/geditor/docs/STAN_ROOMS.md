# Stan room assignments

In Face mode, select one or more stan tiles. The Properties panel's **Room**
control lets you choose an existing background room from a list, or type its
number and press Enter. Escape restores the displayed value before applying.
A selection spanning several rooms displays **Mixed**; choosing a room assigns
all selected tiles to it in one undoable edit. The tile selection stays active.

Room zero is reserved. Choices are limited to existing background rooms and
room 138, the highest index supported by GoldenEye's stan room tables.
Coordinates, colors, tile IDs, special flags and collision connectivity are
preserved. The change is saved with the project and included in Create ROM.

The tile's room determines where the game's visibility traversal starts when
standing on that tile. Assign it to the background room containing that floor;
changing the assignment does not move geometry or create portals.

For the modified Depot doorway near (352, -242, 950), select tiles **03E711**
and **03E712** and change room **36** to **34**. This matches the direct portal
between rooms 40 and 34 after disconnecting room 36.

## Native file ordering

GoldenEye indexes the last contiguous run of tiles for each room, so simply
changing a room byte can leave earlier tiles out of ground queries and bounds.
Saving therefore groups tiles by room when necessary, retaining room encounter
order and original order within each room. Every edge link and header tile
pointer is relocated; record attributes and the file footer are preserved.
Already grouped files remain byte-identical. The live editor document is not
reordered, preserving tile indices for selection and undo/redo even after save.

Run `python3 tools/geditor/tests/stan_rooms/run.py` from the repository root.
The checks cover multi-selection, invalid input, transactional rollback,
save/reload and ROM resource reads, native grouping and links, and walking
across the reported Depot doorway after assigning its tiles to room 34.
