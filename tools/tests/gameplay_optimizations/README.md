# Gameplay optimization comparisons

Run from the repository root:

```sh
python3 tools/tests/gameplay_optimizations/run.py
```

Requires Python 3 and a native C compiler (`CC` defaults to `cc`). The test
extracts current production functions and compares them against the frozen
pre-patch functions in `reference.c`. Renderer and line-of-sight dependencies
are stubbed so their calls and ordering can be compared independently.

Coverage includes:

- 1,000 portal graphs, including no portals, 200 portals, duplicate connections,
  self-links, all byte-sized room IDs, changed door flags, reversed endpoints,
  stage-cache rebuilds, visibility seeding and outgoing traversal order.
- 30,000 bounded room-overlap queries, including already-full output lists.
- 2,000 prop queries with fragmented chunks, overlapping/repeated rooms,
  empty rooms and prop IDs at bitset word boundaries and the pool's last slot.
- 30,000 auto-aim comparisons: bitwise-identical scores and output coordinates,
  matching line-of-sight calls and player collision toggles.
- 20,000 model transforms: separate and aliased destinations, translated model
  nodes, no-parent nodes and the previous hat scale/translation sequence.
  Matrix values must compare exactly; positive and negative zero are equivalent.

Optional address/undefined-behavior checks:

```sh
ASAN_OPTIONS=detect_leaks=0 TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tools/tests/gameplay_optimizations/run.py
```

The harness makes no heap allocations. These comparisons do not replace a ROM
build or N64 gameplay/performance tests. Useful hardware scenes include portal
transitions and moving doors, crowded rooms, auto-aim while zooming, and guards
with different hats. Portal caches add 5,714 bytes of static data before linker
alignment; the prop-query bitset uses 76 bytes of local storage.
