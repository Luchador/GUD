# Setup compaction on ROM export

Adding objects, pads, spawn points, or Action Blocks can append replacement
native tables to a setup file. Keeping old bytes makes editor offsets and undo
history stable, but exporting all of those bytes wastes N64 stage memory.

ROM export now follows the ten native setup roots, retains reachable 16-byte
blocks, and relocates file pointers in an independent copy. It preserves object
and pad indices, record order, deleted-record placeholders, script IDs and
bytecode, patrol/navigation references, credits, and pad strings (including
strings shared with table terminators). The project setup and undo history are
not rewritten.

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

Validation:

- `python3 tools/geditor/tests/setup_compaction/run.py [path/to/GUD.z64]`
  checks all ten native roots, shared/suffix references, inline PRINT strings,
  idempotence, input immutability, invalid pointers, shrinking slots, aliases,
  and manifest bounds under ASan/UBSan. With a ROM, an independent reader
  compares every active section in every embedded setup before and after.
- The reported ROM's 34 setups passed semantic comparison.
- Project rebase/export regression and the Windows build also cover integration.

The headless run verifies loading and game execution; it does not substitute
for visual or real-hardware testing.
