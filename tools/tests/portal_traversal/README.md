# Portal visibility and bounded traversal

Base: GUD `ba6fce2` (Room cache memory recovery). Apply this patch on top of that
change. The exact intermittent Aztec gameplay event has not been reproduced.

## Defects addressed

The old enqueue routine charged the destination room's visit count before
checking portal visibility. Once eight attempts had been queued, later attempts
were discarded, including potentially valid approaches after closed/off-screen
ones. A focused fixture reproduces a missing room and its missing neighbor with
fewer than 20 pending entries; the queue does not need to overflow.

The old 500-entry ring also decremented its write index when full. If that
happened immediately after wrapping to zero, the index became -1. A following
enqueue could then write outside the array.

## Replacement

Each portal direction has one traversal record and at most one pending queue
entry. Repeated approaches merge their parent screen rectangles. An approach
already covered by a tested or pending rectangle needs no more work; a wider
rectangle is processed again. The per-room visit counter remains diagnostic and
never cuts off visibility.

There are at most `2 * PORTMAX` (400) unique directions. The queue has room for
all of them, uses a count to distinguish full from empty, and always wraps within
bounds. The consumer removes an entry before expanding it and passes a local
copy, so a cycle can safely widen/requeue the same direction during processing.
The traversal records plus ordering/state arrays use less memory than the old
500-entry queue.

Portal plane tests, disabled/special portal behavior, room visibility scripts,
room loading budgets, and the previous memory recovery policy are preserved.
Repeated work now terminates when windows stop growing rather than when a room
hits an arbitrary visit limit. As with the existing room rectangle unions,
merging multiple views is conservative and may include extra screen area.

## On-screen diagnostics

The existing solo BG x-ray view works in ordinary builds: D-pad Up enables it;
D-pad Down disables it. Additional lines show:

- `PORTAL PEAK`: maximum simultaneous pending traversals for this camera pass.
- `VISIBLE`: rooms on the current render worklist, including unloaded rooms.
- `WAIT` / `FIRST ROOM`: how many worklist rooms remain unloaded, and the first
  such room's ID. A normal loading-budget delay can produce a short-lived wait.
- `ALLOC FAIL`: the most recent required room allocation failure since this
  camera pass began. It reports allocation failures, not the separate unchecked
  stream-loader error paths.
- `CACHE: OFF`: the previous memory-recovery patch has disabled optional render
  copies for this stage visit. This appears when there was no allocation failure
  in the current pass.

These snapshots do not allocate memory, request room loads, or change visibility.
The additional overlay reserves display-list space and checks available space
before every label. If Aztec still loses rooms, a screenshot with these values
will help separate visibility rejection, load-budget delay and memory pressure.
`WAIT: NONE` by itself does not prove that every room's stream data is valid.

## Validation

```sh
python3 tools/tests/portal_traversal/run.py
python3 tools/tests/room_cache/run.py
python3 tools/tests/bg_debug/run.py
python3 tools/tests/gameplay_optimizations/run.py
```

The new host test uses production queue/traversal/room-window functions with
controlled camera projections. It freezes only the former queue/traversal
functions in `legacy.c` to demonstrate both old failures. Tests cover full rings,
both portal directions, 1,200 reuse steps across multiple wraps, pending-window merges, expanded
revisits, cycles, disabled and special portals, script-added rooms, and read-only
load diagnostics under ASan/UBSan.

Native Aztec and Silo connectivity is read from the repository, then exercised
with full-screen and randomized directed/projected windows. The results match
an independent room/window relaxation oracle in 1,932 Aztec and 1,806 Silo cases.
These are synthetic projection tests, not camera captures or gameplay
reproductions. They do not establish that the old queue overflow caused the
reported Aztec event.

The existing memory-recovery regression and BG debug tests pass, including HUD
labels and 2,087 native geometry streams. Gameplay optimization checks retain
portal plane/adjacency behavior against their earlier reference. Both changed
runtime C files compile with IDO 5.3 for the US N64 target. A full ROM link and
hardware playthrough still require the normal project build and gameplay test.
