# Image build system

GUD builds an uncompressed `GUTX` texture bank. The original ROM's image
positions are used for extraction; the built bank has its own record sizes
and layout.

## Inputs and generated files

| File | Purpose |
| --- | --- |
| `imagelist.u.csv` | Original US ROM offsets and sizes for extraction. |
| `assets/images.def` | Texture IDs (entry order), source names, and surface/detail flags. |
| `assets/images/split/*.bin` | Source texture records extracted from the base ROM or supplied by the author. |
| `build/u/imagelist.csv` | Generated list of source paths in texture-ID order. |
| `build/u/assets/images/combined/combined.bin` | Generated bank of raw `GUTX` records. |
| `build/u/assets/images.raw.def` | Generated texture definitions containing the converted record sizes. |

`VERSION=DEBUG` uses `build/d` for its generated files and shares the US source
assets. Do not hand-edit the generated bank or `images.raw.def`.

## Build path

1. `scripts/make/sync_imagelist_with_def.py` combines names from `images.def`
   with extraction offsets from `imagelist.u.csv`. When source files exist,
   it records their actual sizes in the generated list.
2. `tools/mktex/build/tex2raw` reads those source files in list order, decodes
   their texture data, and writes aligned `GUTX` records and `images.raw.def`.
   Missing or undecodable textures fail the conversion.
3. The Makefile replaces the bank and definitions from temporary outputs
   after conversion succeeds. It links the bank into the ROM and recompiles
   `src/game/image.c` against the generated record sizes.

The Makefile tracks the source texture files and converter sources as
prerequisites. Build the bank alone from the repository root with:

```bash
make VERSION=US combine_images
```

Build the complete ROM with:

```bash
make VERSION=US
```

## Extracting and replacing textures

For initial extraction, put the US base ROM at `baserom.u.z64` and run:

```bash
./scripts/extract_baserom.u.sh
```

To replace a texture, update its file under `assets/images/split/` and rebuild.
The converter recalculates the output size; the original extraction offsets
in `imagelist.u.csv` do not need to change.

To add a texture, append its `IMAGE(...)` entry to `assets/images.def`, supply
the corresponding source `.bin`, and rebuild. Preserve existing entry order:
backgrounds, models, and code refer to textures by ID. The optional
`--update-def` extraction utility is not needed for normal builds; converted
sizes are written to the generated `images.raw.def` instead.

The converter handles format, palettes, mip levels, padding, and output
record lengths. Concatenating the compressed source `.bin` files does not
produce a usable bank for the current runtime.
