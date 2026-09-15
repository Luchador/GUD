# Deleting unused pads

Select an ordinary pad or bound pad in the viewport and press **Delete**.
Deletion is refused when the pad is referenced by:

- An object, character, CCTV aim target or drone-gun aim target.
- A navigation waypoint, including ones outside the visible patrol routes.
- A spawn, intro camera, outro camera or room-based objective.
- A local or shared Action Block, including conditional pad aim/facing targets.

The message identifies the kind of reference preventing deletion. All local
and shared scripts count, even if a particular script is not currently assigned
to a character. If the shared scripts cannot be read, deletion is refused.
Older project base ROMs without the shared Action Block catalog require a rebase
onto a current GUD ROM before this check can be completed.

**Undo/Redo** restores or reapplies the deletion. Saving, reopening and exporting
retain it. Deleted pads disappear from the viewport and the Action Block pad
picker. Script validation also rejects manually entered references to deleted
pads. Other pads keep their existing numbers.

Like deleted setup objects, pads retain a slot in the native setup rather than
compacting the table. Ordinary and bound pads store the `GEPD` marker in their
runtime stan-pointer word at offset 40. Their authored coordinates, plink strings
and table terminators remain intact. The game's `setupLoadFiles` resolves and
overwrites that pointer through `padAssignStanTile` before using it. This editor
deletion does not reclaim runtime pad-table memory or change the game loader.

Checks: `python3 tools/geditor/tests/pad_deletion/run.py` tests reference blocking,
failure atomicity, native byte preservation, project IO and setup undo/redo under
ASan/UBSan. The patrol preview and selection history suites cover hidden markers,
pad selection restoration and unchanged route previews.
