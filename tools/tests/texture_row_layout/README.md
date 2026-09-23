# Texture row layout regression

Run from the repository root:

```sh
python3 tools/tests/texture_row_layout/run.py
```

Requires a host C compiler and Python 3. The runner uses AddressSanitizer and
UndefinedBehaviorSanitizer. It extracts the production GEditor encoder, raw
texture loader, row-swap code, tile setup, and upload functions without changing
their logic. DMA and texture-address lookup are mocked; packed descriptor-word
access is adapted for the host's pointer size and endianness.

The test inspects the actual GBI LoadBlock commands and checks the resulting
texel bytes with an independent model of odd-row addressing. It covers all 13
image formats, imported base-only images, explicit mipmaps, an implicit mip
chain, mirrored/clamped tile draws, direct texture draws, caller-owned pointers,
and the unchanged handling of zero-count records and special tile banks.
It does not emulate the complete RDP, filtering, or VI output.

GEditor imports have a nonzero level count even with no extra mipmaps. The raw
loader therefore already interleaves their odd rows. Uploading them with a
nonzero DXT reverses this interleaving and produces alternating displaced
texels. The fix sets DXT to zero for these loaded textures in both the model
tile-upload path and base-only `texSelect` draws. Zero-count records keep their
previous DXT; `hasExplicitLods` alone is not a safe layout test.

For a negative check, point `TEX_SOURCE` at the unpatched `src/game/tex.c`:
the runner must fail on the first imported texture's LoadBlock DXT.

After applying the patch, rebuild GUD, rebase the GEditor project onto that ROM,
and export again. No GEditor rebuild or model/texture reimport is required.
Visually check the Cradle door in-game and the Laser muzzle flash.
