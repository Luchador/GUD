# Mission briefings and objectives

Open **Settings > Level Settings > Briefing**, between Intro and Environment.
This tab supports the twenty single-player missions. It reads the current
level's menu briefing, watch briefing and objective definitions from the project
and its base ROM. A ROM with the `TXBK` text-bank catalog is required.

## Text pages

The left list contains the watch title, Background, M, Q, Moneypenny and six
objective slots. Menu and watch text are independent, including the objective
labels. There is no corresponding per-level menu title field in this tab.

Each text box has a source selector:

- Choose an existing string ID from this level's text bank to reference it.
  Later edits to that string in **Tools > Text Editor** also affect this page.
- Type in the box to make custom text for this page. It gets a new string slot
  when exporting the ROM, leaving the original string and other users of it
  intact. Selecting **Custom text** also copies the currently displayed text.

Enter inserts a newline; visual wrapping does not. Briefing text uses English
ASCII and actual line breaks, rather than the raw Text Editor's escape syntax.
Paragraphs are limited to 1499 bytes and words to 31 characters to fit the
native text wrapper. Watch objectives support up to 120 bytes and two lines;
a final newline is added to custom watch objective text when applying edits.
These are storage limits, not a font/layout preview: check visual fit in game.

The shared page headings remain in `LtitleE` (menu) and `LoptionsE` (watch),
accessible through the Text Editor. Custom text owned by this tab is edited here;
its generated slots appear in the exported ROM, not the raw Text Editor's
project bank.

## Objectives

Enable up to six slots. Slot 1 corresponds to Action Block objective ID 0, and
slot 6 to ID 5. Disabling a slot does not renumber later objectives. Existing
Action Blocks are retained and are not rewritten when objectives change;
review scripts that explicitly test an objective you disable.

Each objective has separate minimum-difficulty settings for the menu and for
watch/gameplay. Normally these should agree. The watch/gameplay setting also
determines whether the objective is required by the in-level completion checks;
the menu setting is also used by the mission-results screen.

Use **Add**, **Edit** (or double-click), and **Remove** in the conditions list.
All completion conditions must pass; any active failure condition fails the
objective. Conditions include:

| Condition | Parameters |
| --- | --- |
| Destroy, collect, deposit, photograph | Object tag ID |
| Stage flag set (complete/fail) | Nonzero 32-bit mask, decimal or `0x` hexadecimal |
| Enter room | Pad ID |
| Use/deposit weapon in room | Weapon ID and pad ID |
| Copy GoldenEye key | Native key-analyzer state |
| No condition | Always complete; adds no requirement |

Pad IDs use the native numbering: ordinary pad index, or 10000 plus bound-pad
index. Missing, deleted and occluder pads are rejected. Tag IDs are the game's
object tags, not viewport object indices. Conditions preserve the game's normal
behavior; creating an objective does not create its objects or the Action Blocks
that drive its flags. An enabled objective must contain at least one condition.

## Saving, export and rebase

**Apply** adds the edits to the level's undo history. **Revert edits** discards
unapplied changes. Save Project, Create ROM, switching levels/tabs and closing
Level Settings apply valid pending edits. Invalid drafts stay open for correction
or reversion. Ctrl+Z inside a text box undoes typing; the main Edit menu undoes
applied briefing edits.

The briefing document is saved alongside Action Block metadata in the level's
`.set` file, so setup and briefing edits save atomically. Existing project files
remain readable. The extended `GEDSET02` trailer carries native setup length,
Action Block metadata length and briefing metadata length; older `GEDAI001`
trailers remain supported. Use this editor version after saving briefing edits.

Create ROM compiles the document into the mission's native `Ubrief...Z`, setup
objective/watch records, and text bank. Custom strings are appended without
renumbering existing slots. Repeated exports do not accumulate generated slots
in the project. Relative setup links (including signed tag offsets) and door
shadow links retain their targets. Disabled middle objectives become hidden,
already-complete records so they do not leave gaps that block mission completion.
No GUD runtime changes or clean GUD build are needed for this feature.

Rebase retains the metadata and saved raw text edits. Changed upstream menu
briefing definitions are checked against the original definition and the authored
result; incompatible changes are rejected. Existing setup/text conflict handling
continues to apply. The base ROM is never modified.

## Verification

```sh
python3 tools/geditor/tests/project_rebase/run.py
python3 tools/geditor/tests/project_rebase/run.py --rom /path/to/GUD.z64
python3 tools/geditor/tests/action_blocks/run.py
make -C tools/geditor/src
```

Host tests cover six objectives, independent menu/watch text and difficulty,
custom and raw text edits in the same bank, linked setup records, disabled slots,
metadata cloning and save/reopen, atomic failure recovery, ROM export and rebase.
The optional ROM check loads and exports all twenty native mission briefings.
Windows interaction and the final in-game layout still need a runtime check.
