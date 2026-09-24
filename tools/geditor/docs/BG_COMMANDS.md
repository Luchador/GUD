# BG Commands

Open a level, then choose **Tools > BG Commands**, immediately below Patrol
Paths. This window inspects and edits the background's special visibility
command stream. It stays open while you work and refreshes when you switch
levels or change the background or its portals.

Each row shows an instruction number, offset within the command stream,
opcode, indented command name, and room or portal references. Select a row for
an explanation and the original eight-byte cells. Rows marked `!` have a
diagnostic; malformed streams show the safely decoded portion and the reason
decoding stopped. Levels without a command stream show an explanatory message.

Use the reference dropdown and **Go to** to frame a room or select and frame a
portal in the viewport. Room ranges offer each room in the range; portal
commands also offer their connected rooms. Double-clicking an instruction
visits its first reference. **Copy All** copies a report with descriptions,
references, diagnostics, and the complete raw byte listing. Ctrl+C in the
instruction list does the same; Ctrl+C in the detail box copies selected text.

## What these commands control

The game runs this stream before its regular portal traversal. There are two
separate pieces of state:

- Room tests push conditions onto a value stack. Boolean operations combine
  them and `IF` consumes a condition.
- Portal tests update a visibility result and a working screen rectangle.
  Other commands can add rooms through that rectangle, suppress rooms from
  later visibility scheduling, or preload room models without displaying them.

The inspector describes all 24 opcodes handled by the current GUD interpreter,
including its native branch/throw/catch behavior. Indentation follows the
structural control boundaries; it does not execute the script. The viewport
does not simulate the game's conditional visibility or screen clipping.
Hard-coded stage behavior elsewhere in the game is outside this stream.

Portal arguments can contain polygon addresses (`0x64` operand cells) or literal
portal indices (`0x65`). A polygon can be shared by several portal table entries.
The inspector resolves the first matching entry, as the native loader does,
and identifies shared polygons. Portal indices are zero-based; room IDs match
the rest of GEditor. References use the current portal table, including unsaved
portal edits. A missing polygon is reported rather than silently displaying
the game's fallback to portal 0.

## Add, edit, and delete

- **Add...** inserts before or after the selected instruction. Select the command
  type and its room, inclusive room range, portal, or value. Numeric fields accept
  decimal and `0x` hexadecimal. `PUSH` also accepts signed 32-bit values.
- **Edit...** changes a regular command's type and operands. Control boundaries
  are managed as blocks instead.
- **Delete** removes the selected instruction and all of its operand cells.
  On `IF` / `END IF` or `BRANCH` / `CATCH`, the button reads **Delete Block**:
  it removes the entire block, including its contents and optional ELSE arm.
- **Undo / Redo** use the main editor's shared history. In the list, Insert opens
  Add, F2 opens Edit, and Delete removes the selection. Ctrl+Z and Ctrl+Y also
  work while this window has focus.

Edits apply immediately and mark the project as changed. **Save Project** writes
commands into the native BG asset; **Create ROM** includes those saved changes.
Closing the command window retains edits. Undo/redo works across saves and other
background edits. There is no extra command sidecar file.

`IF` adds its `END IF` automatically; **Include ELSE** creates both arms. Insert
body commands before ELSE/END IF, or after IF. ELSE can also be added to an
existing IF that does not already contain one. `BRANCH` adds CATCH and a return
cell that the native interpreter skips. The return cell is protected and deleted
with its block; insertion at that cell goes after it. A legacy branch returning
through END must be extended inside the branch, before CATCH.

The final `END` is protected. Adding after it inserts before it, and adding to a
level with no stream creates it automatically. After removing all other commands,
END is the only remaining instruction. Structurally malformed or unknown streams
remain inspectable but cannot be edited. Existing invalid reference warnings
remain visible so their commands can be corrected or removed.

New room/portal references are validated. The editor rejects duplicate or
misplaced ELSE commands and preserves balanced blocks. It also limits the stream
to 152 ADD ROOM instructions, matching the game's explicit visible-room array.
These checks do not prove that an authored condition or screen rectangle will
produce the intended visibility: test those changes in game.

Portal choices name the first connection for each polygon, matching the native
address lookup. Unsaved new portals are supported. Removing a portal's last
connection is blocked while a live command references it; removing the command
releases that restriction immediately, even before saving. Existing literal
portal indices are adjusted when earlier table entries are deleted.

The live document owns the command cells, including its original state for undo.
Offsets in the window are relative to that stream, since its final file position
can change during saving. Unsaved portal addresses are editor identities until
compiled. The compiler reuses its command allocation across repeated saves;
disk and ROM compaction discard retired storage and relocate native pointers.

Inspecting, copying, and viewport navigation alone do not modify the background.

## Verification

The supplied Dam background contains 202 instructions plus `END` (3,112 bytes),
and Control contains 46 plus `END` (648 bytes). The tests check every reference,
input immutability, and equivalent decoding after the normal BG compactor
relocates the stream and portal polygons.

Run the portable decoder tests without a ROM:

```sh
python3 tools/geditor/tests/bg_commands/run.py
```

Optionally append paths to uncompressed `.seg` backgrounds to test real data:

```sh
python3 tools/geditor/tests/bg_commands/run.py /path/bg_dam_all_p.seg /path/bg_arec_all_p.seg
```

For editing, native save/reload, undo after save, new portal references, and
repeated-save allocation checks, run:

```sh
python3 tools/geditor/tests/bg_commands/run_edit.py /path/bg_dam_all_p.seg /path/bg_arec_all_p.seg
```

Paths are optional in both runners. The edit tests also cover block operations,
invalid-operand rejection without mutation, and portal deletion safeguards.

The runner uses AddressSanitizer and UndefinedBehaviorSanitizer. If the host
cannot run LeakSanitizer under its process tracing, set
`ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks remain on.
Synthetic cases cover all opcodes, literal indices and shared polygon addresses,
live portal changes, malformed lengths and operands, truncation, unknown
opcodes, nesting limits, empty streams, and report generation.

Windows UI checks: open Dam and Control, inspect and navigate room/portal
references, resize the window, copy a report, keep it open while changing levels
and undoing portal edits, add/edit/delete commands and blocks, save and reopen,
undo after saving, and close the project with the inspector open.
