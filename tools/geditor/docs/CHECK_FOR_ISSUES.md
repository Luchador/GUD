# Check for Issues

Open a level and choose **Tools > Check for Issues**. The window checks the
current level, including unsaved edits. The main editor stays usable while the
report is open.

The **Report** selector has two scopes:

- **Current level** runs the checks below on the open level.
- **Last ROM export (all setups)** lists every unresolved pad counted by the
  latest successful Create ROM operation in this project session. Export checks
  all setup resources, including other levels and multiplayer setups, so this
  total can be larger than the current level's report.

An export with pad findings opens the export report automatically. Every pad row
identifies the level, setup resource, ordinary/bound pad number, retained STAN
reference, and world position. Setups without a unique STAN pairing get separate
rows; they are not included in the unresolved-pad count. Aliased setup resources
are counted once, matching export.

Double-clicking an export row opens its level and selects its pad or owning
object when that setup has a directly openable level entry. Unsaved changes get
a save/discard/cancel prompt before switching levels. Legacy setups without a
direct level entry still show their full identities, but cannot navigate to a
different setup's pad. Non-finite positions also disable navigation.

The export report is a snapshot. Level edits do not erase it. Changed/deleted
pad positions or references are rejected during navigation; export again for
an updated report. **Reload Report** reloads that snapshot, without exporting or
saving anything. Failed exports and rebase validations do not replace it. The
snapshot is cleared on closing the project or editor.

The report currently checks:

- Pads that save/export cannot resolve against the saved STAN layout.
- Pad positions that are not finite numbers, and missing/unreadable STAN data.
- STAN edge links to missing tiles and STAN room IDs outside the background.
- Invalid portal data and room connections.
- Invalid patrol/waypoint tables, duplicate patrol IDs, and empty patrol paths.
- Duplicate guard IDs.

An unresolved pad is a warning: it can be intentional or unused. References
that save/export can repair automatically are not reported. Deleted pads are
ignored. The check never moves pads, repairs files, saves the project, or exports
a ROM. It is not an exhaustive gameplay or AI validation.

Double-click a result, or select it and choose **Go to Issue**, to frame its
location in the viewport. An unresolved pad selects its owning object or guard
when there is exactly one placement owner. Otherwise it selects the pad. If the
model cannot be displayed, the pad remains the navigation target. Equipment
assigned to a guard is not mistaken for an object placed at that pad.

STAN and portal reports select the affected tile or portal and enable the
corresponding viewport layer. A hidden STAN tile is revealed without unhiding
other tiles. Reports without a usable location disable **Go to Issue**.

After a level edit, undo, or redo, current-level results are cleared. Choose
**Check Again** to scan the updated level. Selecting things and moving the camera
do not clear the current-level report. Finish active transforms before checking
or navigating. Apply any pending Patrol Paths draft to the level before checking
it. Switching projects closes the report. Switching levels keeps the window open
and clears current-level results while retaining the export snapshot.

## Verification

Run `python3 tools/geditor/tests/level_issues/run.py` for native diagnostics and
modeless navigation tests with address/undefined-behavior sanitizers.
An optional argument names a Depot audit folder containing `rom-depot.set`,
`rom-depot.stan`, and `rom-depot.seg`; these files are read without modification.
The optional fixture verifies the previously audited Depot pads 21, 145, and
bound pad 82, including navigation to the custom lamp that owns bound pad 82.

Run `python3 tools/geditor/tests/zoom_selected/run.py` for viewport framing
regressions. Build the Windows editor with `make -C tools/geditor/src` using
MinGW. A Windows smoke test should exercise double-click/Enter, editing with
the report open, refreshing, and closing/switching levels.

`python3 tools/geditor/tests/resource_cleanup/run.py /path/to/GUD.z64` checks
that the exported pad rows match the cleanup total, tests snapshot lifetime
across failed exports and validation, and checks resource/repeat-export byte
stability. On the previously supplied GUD(7) ROM there are 39 unresolved pads:
3 in Depot and 36 across other setups.
