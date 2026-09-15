# Action Blocks

Open **Tools > Action Blocks…** while a level is loaded. It is the first Tools
menu item. If a character is selected in the viewport, the window initially
selects that character's starting behavior.

## Editing a level

- The left list contains level scripts and available shared behaviors. Search
  by name or hexadecimal block ID. Shared entries have a `Shared:` prefix.
- Choose a block, give it a useful **Block name**, then select an instruction.
  Give label sections names such as `Wait for Bond` or `Raise alarm`; jumps show
  those names and their exact destination steps. Any instruction can have a
  name and a note.
- Edit parameters on the right. Character, behavior and pad parameters offer
  lists; numeric input accepts decimal or `0x` hexadecimal values. Known distance
  fields use metres, and known timer/frame fields use seconds. Animation start
  and end fields also offer the native full-animation sentinel.
- **Find command** filters the command list. **Insert** adds the chosen command
  before the selected row. Insert `Section` below a forward jump's intended
  position before adding that jump. **Up**, **Down** and **Delete step** edit the
  sequence. The final bytecode terminator stays in place.
- **Go to destination** visits a branch's section. **Open referenced block**
  follows a behavior reference. Double-click a script reference in **Used by**
  to visit the referring instruction.
- **New behavior** allocates an unused ID in `0401–0FFF`.
  **New level logic** allocates one in `1000–FFFF`; the game automatically starts
  these scripts without a real character. Both start as a harmless waiting loop.
  **Duplicate** copies a script to a new level-local ID, including its names and
  notes. IDs are allocated automatically in this first version.
- To assign a starting behavior, select the desired block and character, then
  click **Assign selected block**. Existing vehicle/aircraft assignments appear
  in **Used by** and protect their scripts from deletion; this window does not
  change those assignments.

Fields commit when changing selection or performing another edit. **Update
instruction** commits them explicitly; Enter in an input field also commits
without closing the window. Invalid input stays available for correction.

**Undo/Redo** operate on the draft (up to 32 snapshots, trimmed to a 64 MiB history
budget). Ctrl+Z/Ctrl+Y work from the instruction and block lists; text controls
retain their normal text-edit undo behavior. **Cancel** discards the draft.
**Apply to project** validates it and records one `Edit Action Blocks` step in the
main project history. Save the project normally, including with Ctrl+S, before
creating a ROM.

## Native behavior is retained

All 253 command layouts come from `src/aicommands.def`. The editor retains the
native instruction sequence, big-endian parameter bytes, variable-length legacy
debug text, table order, shared script offsets and duplicate table entries.
Untouched scripts are not rewritten. Names and notes are editor metadata.

A section is a particular occurrence of a native label, not just its one-byte
number. `GotoNext` and conditional commands search forward; `GotoFirst` searches
from the beginning. A destination list only offers occurrences that command can
reach. Moving a row is refused if it would redirect an existing jump. Referenced
sections must be redirected before deletion. Native label numbers are retained;
readable section names can be changed freely.

Starting an action usually does not wait for it to finish. Successful `Try`
commands branch when the action starts; failed attempts continue to the next
instruction. `Wait for next AI update` yields. A behavior switch to self restarts
execution immediately in another block. Return uses one stored behavior ID and
restarts that block; it is not a call stack. `End of block` returns from the
current update without advancing the actor's saved instruction position.

## Validation

Validation reports unresolved destinations, missing block references, suspicious
character/pad references, duplicate IDs, several actor-dependent commands in
level logic, return behavior assumptions, self-restarts and possible loops
without a wait. Double-click a result to navigate to it. Conditional loop checks
are conservative warnings, not a simulation of the game or a proof that a script
will terminate; inter-block execution is not simulated.

Byte ranges and the native 16-bit saved instruction offset limit are enforced.
Existing unresolved references in untouched vanilla blocks remain warnings so
unrelated edits can preserve their original bytes. Editing or duplicating such
a block requires repairing its missing references. Duplicate native IDs retain
table order: switches resolve the first entry, while automatic level-logic
entries each still start. Duplicate a later entry to obtain a uniquely assignable
behavior. An older base ROM without the optional `AIGL` catalog can edit local
scripts, but cannot display or fully validate shared behavior references.

## Project storage and export

`SetupFile` owns an optional metadata buffer separately from native setup data.
Project `.set` files store it after their native prefix, followed by a 16-byte
footer: `GEDAI001`, native length (big-endian u32), metadata length (big-endian
u32). The payload starts with `AIN1` and a big-endian record count. Each 332-byte
record holds a local table-entry ordinal, instruction index (`FFFFFFFF` for a
block name), opcode, a 64-byte name and a 256-byte note. Strings are NUL-terminated.
Ordinals distinguish duplicate block IDs. Metadata is limited to 4 MiB.

Saving uses a temporary sibling file and atomic replacement, keeping scripts
and annotations together. Setup cloning, project undo/redo, model/spawn placement
and project rebasing retain metadata. ROM export strips the entire trailer;
readable names and notes consume no N64 memory. Native script edits append changed
scripts and a replacement AI table, preserving unrelated setup offsets. This
conservative approach retains obsolete script bytes in the native prefix.

Shared programs are read from the project's base ROM through the `AIGL`/`CMAP`
catalogs, and are read-only. Editing a duplicate changes only the local setup.
No game engine or shared AI implementation changes are required.

## Verification

Run `python3 tools/geditor/tests/action_blocks/run.py` from the repository root.
The suite uses ASan/UBSan and production decoding, editing, setup IO, history and
ROM-resource export code. With IDO available it compiles the repository's setup
sources and shared AI source and tests the emitted native bytecode without a
base ROM. Its layout checks preprocess the authoritative command definitions.

The first version provides native-command authoring, readable sections, notes,
references and validation. Higher-level templates and a flowchart view can build
on this document model later.
