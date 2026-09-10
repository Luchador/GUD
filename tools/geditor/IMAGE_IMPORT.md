# Importing images

Rebuild both GUD and GEditor after applying this patch, then create a new
project from the rebuilt GUD ROM. Older projects can still open and build,
but their base ROMs cannot append images.

```sh
make
make -C tools/geditor/src run
```

Choose **File > Import... > Import Image**, select a BMP, then choose its
texture type, number of mipmaps, hit sound, and bullet-hole type. The dialog
uses radio buttons for the texture types and shows the current TMEM usage.

- All 13 GUD texture types are supported: RGBA32, RGBA16, RGB24, RGB15,
  IA16, IA8, IA4, I8, I4, and CI4/CI8 with either RGBA16 or IA16 palettes.
- Indexed formats choose a palette automatically. Reduced-color formats
  quantize the input; the image browser and viewport preview that conversion.
- Mipmaps are generated with a box filter, accounting for alpha when averaging
  colors, and stored in the native texture. Up to six additional levels can
  be generated, stopping when both dimensions reach one pixel.
- TMEM is 4096 bytes. Paletted textures reserve its upper half for the palette,
  leaving 2048 bytes for pixel data and mipmaps. The check includes row padding
  and every mip level. Import stays disabled when the selected settings exceed
  that budget; choose another format, fewer mipmaps, or a smaller source BMP.
- GUD's texture dimensions are one byte each, so each axis must be 1–255 pixels.
  RGB BMPs are opaque; alpha-bearing BMPs are decoded through Windows Imaging
  Component. Source files are never overwritten.

Click **Import** to append the next hexadecimal image ID and reveal it in the
image browser. It can immediately be dragged onto faces. The title gains an
asterisk until **Save Project** saves it; exiting without saving discards
pending imports. Creating a ROM also saves pending changes first.

Saved imports consist of `images/XXXX.bmp` and `images/native/XXXX.gtex`.
Keep both files. The native file retains the selected format, mipmaps, surface
settings, and base-ROM identity. Edits to an imported image's BMP are re-encoded
with those saved settings during ROM creation. Original extracted BMPs retain
their existing behavior; this feature does not replace original ROM images.
Unused images are never deleted.

## ROM support

The `TXTB` manifest entry describes the reserved image table, and `TXCF`
describes its writable ROM address and active image count. ROM creation copies
the original GUTX records unchanged, appends the imported records, updates the
surface table/configuration and IMGS range, and recalculates the ROM checksum.
The project's `base.z64` stays unchanged.

Image IDs remain 12-bit: the capacity is **4096 images total**, plus a separate
table sentinel. ROMs can grow to **64 MB**; the image bank itself retains its
24-bit relative-offset limit. Missing or corrupt imported assets, mismatched
base-ROM metadata, and capacity overflows stop ROM creation with an error.

## Regression checks

On Linux/WSL with Python 3 and a native C compiler:

```sh
python3 tools/geditor/tests/image_import/run.py
```

The tests use address/undefined-behavior sanitizers and exercise the production
encoder, BMP preview/save routines, imported asset storage, and ROM packer.
They cover all formats, mipmaps and padding, palette quantization, pending and
saved images, orientation, failed saves, reopened projects, missing/corrupt
assets, preserved originals, relocation, growth, and the image/ROM limits.
The Windows BMP decoder and dialog require a Windows runtime for visual testing.
