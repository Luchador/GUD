# Linking stan tiles

Show **Stan Geometry**, switch to **Face** mode, and select two stan tiles
(Shift-click adds the second tile). Right-click in the viewport and choose
**Link Stan Tiles**. The selection is retained when the menu opens.

Selected tiles appear cyan. Their directly linked neighbors appear magenta,
using the current Stan Geometry opacity. With multiple tiles selected, all
of their direct neighbors are highlighted; selected tiles stay cyan even if
they link to one another. The highlight updates with selection and link edits
and is only a viewport overlay, so stored tile colors are unchanged.

The tiles must share exactly one complete boundary edge: both endpoints must
match in X, Y, and Z, with the edge running in opposite directions around the
two tiles. Matching uses the stored native coordinates. If the edges do not
match, align their vertices first. Linking does not move vertices, fill gaps,
or create missing stair risers. Partial edge overlaps cannot be linked.

The operation connects the shared edge in both directions, allowing crossings
from either tile. It repairs an existing one-way link and leaves an already
connected pair unchanged. If either edge connects to a third tile, it reports
the conflict without changing any links. Other edges are left untouched.

Linking is one undoable edit. The editor refreshes shared stan vertices, pad
placement, and object grounding/shading after the edit. **Save Project** writes
the native stan links; **Create ROM** uses the saved resource normally.

Validation:

- `python3 tools/geditor/tests/stan_linking/run.py` checks reciprocal links,
  traversal in both directions, shared vertex groups, invalid/conflicting
  edges, native save/reload, ROM resource reads, undo/redo, and failure rollback.
  It also breaks and restores the original Depot floor-to-bottom-riser link
  involving tile `03FE12`, then traverses the stairs in both directions.
- `python3 tools/geditor/tests/bg_disconnect/run.py` checks viewport context
  menus, preservation of the selected stan pair, and existing BG operations.

The tests use repository data; an edited level still needs an in-game crossing
check after saving and creating its ROM.
