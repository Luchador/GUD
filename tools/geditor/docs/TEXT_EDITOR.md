# Text Editor

Open **Tools > Text Editor**, the first item in Tools. A project must be open;
no background or setup selection is required. The current level's text file is
selected initially when it has one.

Choose a **Text file** such as `LsiloE` (Silo), `LrunE` (Runway), or `LoptionsE`
(solo menus). The list includes every English `L...E` bank in the project's
base ROM, including shared weapon names, pickups, menu titles, multiplayer
menus and cheat text. Empty multiplayer banks are visible as unused slots.

Use **Find / ID** to filter the current file by text, decimal slot or hexadecimal
string ID. Select a string and edit it in the multiline box. The ID displayed
above the box is the game's encoded ID, as used by briefing and Action Block
references. IDs come from the ROM's `TXBK` catalog; a ROM without that optional
catalog displays the slot index instead.

Moving between strings keeps edits in the current file's draft. **Revert string**
restores the selected string to its last saved value. The text box supports the
standard Windows editing shortcuts, including Ctrl+Z while editing a string.
**Save File** writes this bank directly into the project. Switching files or
closing with unsaved changes offers Save, Discard or Cancel. Text saves are
independent of the level geometry undo history and do not require a second
File > Save. Use **Tools > Create ROM** to include saved text in a playable ROM.

## Text and IDs

- Enter inserts a game newline. Visual wrapping in the editor does not insert
  one. Trailing newlines are preserved; the status line reports whether the
  selected string has one when it is loaded or accepted.
- An empty text box saves an empty string. `<unused>` means a NULL table slot,
  which remains read-only. Slots cannot be inserted, removed or renumbered.
- Literal backslashes are displayed as `\\`. Escapes `\n`, `\r`, `\t` and `\xHH`
  are accepted; other nonprinting or non-ASCII bytes display as `\xHH` and
  round-trip unchanged. `\x00` is rejected because it terminates a native string.
- New text uses English ASCII. Unsupported Unicode, including smart quotes,
  is rejected explicitly. Escaped game bytes are preserved, but their appearance
  depends on the game's font and text renderer.
- Editing one string does not modify other strings that happened to share
  its original storage. Existing NULL entries and empty strings stay distinct.

The editor shows text contents, not an in-game font/layout preview. Check longer
briefings, objectives and menu labels in game for fit. Native text allocations
grow with the bank; the editor enforces the briefing loader's 450,048-byte bank
capacity. Ordinary stage/permanent memory budgets still apply.

## Storage, export and rebase

Saved files are native binary banks at `text/LsiloE.bin`, `text/LoptionsE.bin`,
and so on, relative to the project folder. They contain a big-endian array of
32-bit relative string offsets followed by NUL-terminated byte strings. Preserve
the whole `text` folder when copying a project. Merely viewing a bank does not
create an override or alter `base.z64`.

Create ROM validates saved banks and their slot counts, replaces the matching
named resources and updates their file-table offsets. Banks may grow or shrink;
resource repacking and ROM checksums are handled by the existing exporter.
No GUD runtime or project-version change is required.

Rebase keeps saved text when the base bank is unchanged and accepts upstream
text when the project bank is unchanged. Different edits to the same file on
both sides are reported as a conflict; strings are not merged individually.
Equivalent banks with different padding or string offsets compare by their
slot contents. A removed nonempty text bank still requires manual migration.

## Verification

From the repository root:

```sh
python3 tools/geditor/tests/text_editor/run.py
python3 tools/geditor/tests/text_editor/run.py --rom /path/to/GUD.z64
python3 tools/geditor/tests/project_rebase/run.py
make -C tools/geditor/src
```

The host tests exercise all source text banks (or the supplied ROM), aliases,
unused and empty strings, line endings, all nonzero byte values, malformed
banks, project saving and failed-write recovery, export with relocated slots,
and rebase preservation/conflicts. Windows dialog interaction and in-game
layout require a runtime check.
