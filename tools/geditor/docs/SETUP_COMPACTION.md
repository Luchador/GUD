# Setup compaction and long editing sessions

Adding objects, pads, spawn points, or Action Blocks can append replacement
native tables to a setup file. Abandoned copies must not accumulate in the
live document, saved project, or exported ROM.

The compactor follows the ten native setup roots, retains reachable 16-byte
blocks, and relocates file pointers in an independent copy. It preserves object
and pad indices, record order, deleted-record placeholders, script IDs and
bytecode, patrol/navigation references, credits, and pad strings (including
strings shared with table terminators). Empty pad links share a current table
terminator instead of keeping blocks of obsolete terminators alive.

Compaction runs after each completed setup edit, when loading a project setup,
when saving it, and on ROM export. Native object/character source offsets are
relocated together with the live bytes; selection uses unchanged indices.
Inspectors refresh their cached offsets after commit. An edit failure leaves
the transaction available for rollback, without clearing redo or adding undo.
Undo retains independent snapshots and never relies on abandoned native tables.

Saving compacts a separate copy, then atomically replaces the `.set` file with
the native data and its Action Block names/notes trailer. It does not change
the live document or history. Opening and saving an older project therefore
cleans its abandoned tables automatically; no `.gep` format change is needed.

## Placement pads and deleted slots

Moving a model or camera checks ownership by references, regardless of where
its pad table is stored. A pad used only by that placement is updated in place.
Shared pads are detached first. Checks cover other placements, aim targets,
guard presets, spawns/cameras, objectives, navigation, and local/shared scripts.

New private pads carry `GEPP` in their unused native runtime stan-pointer slot;
the game overwrites that slot when assigning stan tiles. This marker survives
save/reload and undo. Unreferenced private pads and explicitly deleted pads can
be recycled without shifting other pad IDs. Unreferenced authored standalone
pads are retained. Old pads without a reliable ownership marker are also
retained, even if an earlier editor may have abandoned them.

Deleted object commands can be reused for the same native record type only
when no tag, relative link, owner, or other setup command references them.
Deleted character slots additionally require an unreferenced character ID,
including preset, equipment, local/shared AI, conditional aim/facing, and clone
references. Special/runtime character IDs remain reserved. Unknown command
types disable command recycling. No surviving command or pad is renumbered.

Shared-script references are read once from the project base ROM and shared
immutably by undo snapshots. Older base ROMs without the optional shared
Action Block catalog still load, compact, and save, but pad/character reuse is
disabled because their references cannot be verified. Rebase such projects
onto a current GUD ROM to enable reference-aware reuse.

Repeated edits plateau at the slots needed by the content. New content and
referenced deleted slots still consume space; compaction does not remove
records whose IDs may be needed by the level.

## ROM allocation and the Depot failure

The exporter also shrinks the setup's actual resource slot, updates following
file-table offsets, and updates the OBSG manifest end. The game determines
resource sizes from adjacent file-table addresses, so zeroing or truncating a
payload inside its old slot would not recover RAM.

In the reported Depot ROM, the setup occupied 279,840 bytes but its active data
needs only 56,192 bytes. The original ROM reached its first frame and stopped in
mempAllocBytesInBank while allocating a 153,664-byte depth buffer; its stage
pool had only 122,160 bytes free and its permanent fallback pool had 864 bytes.
Compacting only Depot's setup recovered 223,648 bytes. A headless emulator run
then advanced through gameplay with 191,536 stage-pool bytes free after the
depth-buffer allocation. The background and stan assets were unchanged.

The lifetime update further reduces that Depot setup to 56,096 bytes by sharing
empty pad-link targets. Its stress test adds two retained props and repeatedly
adds/deletes another; after initial allocation it remains at 56,576 bytes and
362 normal pads through 4,000 edits.

## Validation

- `python3 tools/geditor/tests/setup_compaction/run.py [path/to/GUD.z64]`
  checks all ten native roots, shared/suffix references, inline PRINT strings,
  idempotence, input immutability, invalid pointers, shrinking slots, aliases,
  and manifest bounds under ASan/UBSan. With a ROM, an independent reader
  compares every active section in every embedded setup before and after.
- The reported ROM's 34 setups passed semantic comparison.
- `python3 tools/geditor/tests/setup_lifetime/run.py [path/to/GUD.z64]`
  exercises 12,001 synthetic commits: alternating moves/scales, add/delete
  cycles, cameras/spawns, changing script bytecode, metadata saves, reloads,
  undo/redo, selection, dirty revisions, protected references, and failure
  rollback. Supplying the reported ROM adds 4,000 Depot edits. ASan/UBSan are
  enabled; leak detection is disabled because the sandbox prevents LSan's
  process inspection.
- Project rebase/export regression and the Windows build also cover integration.

The earlier export-only fix was checked in a headless emulator. This lifetime
update has automated native-data and Windows compile coverage; interactive
Windows and real-N64 testing remain necessary.
