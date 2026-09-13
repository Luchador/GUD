# Importing and editing images

GEditor requires a current-format project and its matching `base.z64`, including
the `TXTB` and `TXCF` image manifest entries. Current projects using `GTI2` image
metadata can continue to be used. Projects that depend on retired formats need
to be recreated from a current GUD ROM; see [Supported formats](docs/FORMATS.md).

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
pending image changes. Creating a ROM also saves pending changes first.

Imported and replaced images consist of `images/XXXX.bmp` and `images/native/XXXX.gtex`.
Keep both files. The native file retains the selected format, mipmaps, surface
settings, source BMP path, and base-ROM identity. Edits to an imported image's BMP are re-encoded
with those saved settings during ROM creation. Use **Replace image** for original
ROM images; externally editing an original extracted BMP alone does not change
the ROM. Images are never automatically deleted when they become unused.

## Delete, replace, and reimport

Right-click an image in the browser and choose **Delete image**, **Replace image**,
or **Reimport**. These actions also work for images imported during the
current session. The permanent **No Texture** item has no context actions.

- **Delete image** asks for confirmation and warns that hard-coded texture IDs,
  including light textures, can affect game rendering. Cancel leaves it intact.
  Confirming removes the thumbnail immediately. Save Project removes its BMP
  and retains a deletion record in `images/native/XXXX.gtex`. ROM creation uses
  a 1x1 transparent RGBA16 texture with no mipmaps or surface effects at that
  ID. Geometry referencing it is retained and may look blank or otherwise
  incorrect depending on its material. No other IDs are shifted or reused;
  deleted slots still count toward the 4096-ID limit. Keep the native deletion
  records with the project so deletions propagate to future ROM builds.
- **Replace image** opens a BMP picker followed by the same settings dialog as
  importing, with a **Replace** button. It retains the selected hexadecimal ID
  and overwrites the pixels, palette, format, mipmaps, sound, bullet-hole type,
  and native detail flags with the new choices. All settings start at the import
  dialog defaults; no hidden settings from the old image are carried forward.
  TMEM limits apply to replacements too. Cancel leaves the image unchanged.
- **Reimport** rereads the original source BMP using the format, mipmap count,
  hit sound, and bullet-hole type chosen on the last import or replacement.
  It keeps the image ID and regenerates the pixels, palette, and mipmaps without
  reopening the settings dialog. A missing/unreadable source, invalid dimensions,
  or TMEM overflow reports an error and leaves both saved and pending edits intact.
  Reimport does not reduce mipmaps or change formats to make an oversized image fit.

Source paths are remembered from successful imports/replacements and saved by
**Save Project**, so Reimport also works after reopening the project. Moving the
source or moving the project to another machine may break that path; use
**Replace image** to choose the source again. Images imported by older GEditor
versions, and unmodified images extracted from the ROM, have no saved source.
For those images, use **Replace image** once to establish the file and settings.

Successful actions mark the project as unsaved and refresh the browser, face thumbnail,
main viewport, and any open model viewer. Cameras and geometry selections are
preserved. Replacing with different dimensions does not rescale authored model
UV coordinates; the preview uses the same texel coordinates as the ROM. Use the
UV/model editing tools if you want different mapping.

Save failures preserve the previous saved BMP/settings and leave the edit pending
for retry. As with imports, exiting without saving discards pending image edits.
The `GTI3` native-image metadata includes the source path with a length and
checksum, committed together with the settings and native pixels. Existing
`GTI2` files remain readable; edits without a source (including deletion records)
still use `GTI2`. Earlier metadata versions are not supported.

## ROM support

The `TXTB` manifest entry describes the reserved image table, and `TXCF`
describes its writable ROM address and active image count. ROM creation copies
unmodified GUTX records and their flags unchanged, applies replacements and
blank deletion records at their original IDs, appends imported records, updates
the surface table/configuration and IMGS range, and recalculates the ROM checksum.
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
They also cover original/imported/pending replacement and deletion, stable IDs,
all replacement settings, discard, rollback after late save failures, saved-only
ROM export, thumbnail pixel compaction, blank records, rejected old metadata,
and model UV stability when a replacement has different dimensions.
Reimport checks cover source changes, remembered settings, save/reopen, missing
or malformed BMPs, TMEM/mipmap failures, pending replacement preservation,
source-path rollback, and invalid source metadata.
The Windows BMP decoder and dialog require a Windows runtime for visual testing.
