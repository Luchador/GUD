# Persistent animation-frame cache

`loadAnimationFrame` now retains 64 ROM transfers in private 128-byte slots,
with 16 sets and four least-recently-used entries per set. Lookup examines at
most four tags. The cache uses 8,975 bytes of static storage before linker
alignment, including metadata and DMA alignment padding, replacing the former
56 bytes of scratch-range tracking.
It does not allocate from the stage's room/texture heap.

The stock uncompressed character frames contain 90 bytes and use a 96-byte
aligned DMA; the three vehicle animations use 6-byte frames. Both fit. Transfers
larger than 128 bytes bypass the cache and retain the existing scratch/DMA path.
The existing working-buffer capacity requirement remains unchanged.

Cache hits copy the transfer into the original scratch buffer. Callers retain
the same returned addresses, odd-source adjustment and four-frame blending
lifetime. No animation frame pointer refers to an evictable cache slot. The
cache survives per-model scratch resets and stage changes; its ROM bytes are
immutable. `initAnimationsBuffer` invalidates it, even when the buffer address
is reused. RAM-resident animation data still returns directly.

Run from the repository root on Linux x86/x86-64:

```sh
python3 tools/tests/animation_frame_cache/run.py
```

Python 3 and GCC/Clang are required; the test uses a freestanding 32-bit binary
and needs no 32-bit libc. Kernels without IA32 execution support also need the
Python `unicorn` package. It extracts the production loader, cache and reset
functions. ROM DMA is mocked with source/destination bounds and alignment checks.
It compares first, middle and last samples of the real converted animations;
checks all 64 slots, LRU collision eviction, live blend frames, 1-192-byte frames,
odd source addresses and frame strides, nonaligned scratch starts, same-address
different-size tags, direct RAM, missing scratch and reinitialization.

A synthetic eight-guard workload requests 32 distinct frames for 120 updates:
only the initial 32 of 3,840 requests issue DMA. This demonstrates retention
across guards, not an FPS prediction. Real animations advance, and a working
set with more than four competing tags in one set can evict frames earlier.

Hardware validation: use the same Depot camera position and guard count before
and after the patch, including guards running different animations, weapon
firing, hits/deaths and stage restarts. Compare the existing FPS counter. These
tests do not measure cartridge DMA latency, CPU copy cost or frame rate on N64.
