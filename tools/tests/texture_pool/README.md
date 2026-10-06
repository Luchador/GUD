# Texture pool allocation regression checks

Run `python3 tools/tests/texture_pool/run.py` from the repository root.
An optional argument names a concatenated GUTX image bank for additional checks.
No ROM assets are included.

The test extracts the production loader, size calculation, texture lookup and
optional bank allocator. DMA and N64 addresses are represented by bounded host
buffers. ASan/UBSan and pool canaries check:

- Exact-fit textures below the old 4300-byte threshold, across all 13 formats.
- Large textures which previously passed that threshold but did not fit.
- Implicit mipmaps and palettes, including a rejected mip that must not be written.
- Loading beyond the initial `-mt` allocation using available stage memory.
- Duplicate lookup, unchanged existing addresses, and reset between stages.
- Private pools remaining private, and exhausted/disabled bank allocations failing
  without overwriting the pool or invoking the allocator's fatal path.
- Invalid descriptor sizes and offsets being rejected before pixel DMA.

With a native image bank, every record is loaded into an exactly sized pool, and
all 13 bullet-impact textures are also loaded with an exhausted main pool.
Host pointer sizes differ from N64; both descriptors and prefixes use `sizeof` in
the test and production code. Compile the engine with IDO separately.

The initial `-mt` reservation remains in use. Additional textures allocate only
the bytes they need from the stage bank; they do not borrow from the permanent
bank, relocate existing textures, or survive the stage reset. This does not add
physical RAM: an exhausted stage bank can still fail a texture load.

The companion `texture_row_layout` test checks pixel ordering and RDP upload
commands. These host checks do not replace in-game testing on N64/ares.
