# Selection undo/redo

The existing Undo/Redo menu items and shortcuts now include main-viewport and
UV-vertex selections in the same chronological history as level edits.

- Clicks, box selections, Shift/Ctrl selection changes and clearing a selection
  can be undone. A completed marquee is one step; cancelling it is no step.
- The snapshot includes the selection tool, so restoring an edge selection
  also returns to edge mode. Camera position and visibility settings stay put.
- BG faces/vertices/edges, stan tiles/points/edges, setup objects/characters,
  pads, spawn/spline/camera markers and portals are supported.
- Edits capture their prior selection. Undoing deletion restores both the data
  and selection; redo restores the resulting selection. Automatic reselection
  during edits and the temporary target used by snapping are not extra steps.
- UV selections retain vertex identities and the selected texture's coordinate
  basis. Undo reopens the UV Editor if needed to display a restored UV selection.
- Selection-only steps never mark assets unsaved and are never written to the
  project or ROM. Opening a level resets its history. The existing 64-step limit
  and normal redo-branch replacement apply to selections as well as edits.
- Hidden geometry remains hidden and is excluded when restoring a selection.

Snapshots use document identities rather than draw order. Frame message nesting
coalesces notifications from a rebuild into its edit, while standalone selection
notifications create history entries. Selection snapshots are copied into the
history and transferred between stacks during undo/redo. Asset saved revisions
are independent of selection revisions.

Run the focused native checks from the repository root:

```sh
python3 tools/geditor/tests/selection_history/run.py
python3 tools/geditor/tests/box_selection/run.py
```

These execute production history, frame notification grouping and viewport/UV
snapshot logic with Win32 rendering/IO stubs, using AddressSanitizer and
UndefinedBehaviorSanitizer. They cover mixed edit/selection chronology, save
revisions, branch replacement, stack limits, allocation failure, geometry
reordering, source identity validation, and marquee cancellation. A Windows
visual check is still needed for highlights and focus behavior.
