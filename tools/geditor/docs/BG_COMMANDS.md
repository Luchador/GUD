# BG Commands

Open a level, then choose **Tools > BG Commands**, immediately below Patrol
Paths. This read-only window inspects the background's special visibility
command stream. It stays open while you work and refreshes when you switch
levels or change the background or its portals.

Each row shows an instruction number, offset within the uncompressed BG file,
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

Inspecting and copying commands does not modify the BG file or add an undo
entry. **Go to** only changes viewport navigation/selection. Command editing
is not part of this window.

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

The runner uses AddressSanitizer and UndefinedBehaviorSanitizer. If the host
cannot run LeakSanitizer under its process tracing, set
`ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks remain on.
Synthetic cases cover all opcodes, literal indices and shared polygon addresses,
live portal changes, malformed lengths and operands, truncation, unknown
opcodes, nesting limits, empty streams, and report generation.

Windows UI checks: open Dam and Control, inspect and navigate room/portal
references, resize the window, copy a report, keep it open while changing levels
and undoing portal edits, and close the project with the inspector open.
