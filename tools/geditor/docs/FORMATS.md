# Supported GEditor formats

GEditor reads and writes the current project and asset formats. It does not
migrate files produced by older editors.

| Data | Required format |
| --- | --- |
| Project | `.gep` version 5 with a saved level table (including `chrLODDistance`), environment/memory overrides and matching `base.z64` |
| Base ROM | GUD manifest version 3, with `IMGS`, `OBSG`, `MUSF`, `STGT`, `CMAP`, `FTBL`, `ENVT`, `TXTB`, `TXCF`, `MONA`, `MONT`, and `MOND` entries |
| Level table | 44-byte rows with authored level names and `chrLODDistance`; the final sentinel may have a NULL name |
| Level memory allocations | Optional bounded `LMEM` string slots in the base ROM; see [Level memory](LEVEL_MEMORY.md) |
| Model assets | glTF 2.0 with normalized UV coordinates and the current GEditor metadata |
| Added prop models | `models/newprops.gnp` (`GNP1`), with `NPRP`/`NPMD` support in the base ROM; see [New prop models](../NEW_PROP_MODELS.md) |
| Saved model edits | Current `.gmodel` native overrides (`GMD1`) |
| Saved image edits | `.gtex` metadata (`GTI3` with an import source, or `GTI2` without one) and the associated BMP, except for deletion records |

New ROMs also expose optional audio, animation, model, global AI and text-bank
catalogs. See [MANIFEST.md](MANIFEST.md) for their ranges and record layouts.

The model import workflow still supports Blender glTF/GLB files. Its material,
sampler, and alpha settings remain supported. New imports create unassigned
material slots; image names are not used to bind textures. Assign images by
dragging from the image browser into the Model Editor's Materials panel.
Embedded GLB editor resources and graphics-driver fallbacks are unchanged.

`GMD1` overrides and `GNP1` project entries may append a `GMS1` editor trailer
to their native model bytes. It contains slot names, image assignments, per-face
slot IDs and normalized UVs. A 16-byte footer holds the `GMS1` magic, native
length, metadata length and metadata FNV-1a hash (32-bit big-endian fields).
The payload is slot count and face count, then 132-byte slots (128-byte UTF-8
name plus image ID), then 28-byte faces (slot ID and six IEEE float UVs), with
up to 15 zero padding bytes before the footer. Existing native data without
this trailer derives its slots from its draw states. ROM export strips the
trailer; no game runtime change is needed for material slots.

Removed compatibility paths include project versions 1–4, old manifests,
32-, 36- and 40-byte level rows, the prompt to supply a missing base ROM, image-table
discovery by scanning old ROMs, earlier image metadata, and early model exports
that encoded texel UVs or texture IDs in material names. Model loading and model
browser enumeration now use glTF assets exclusively.

Background geometry is still checked for valid room vertex batches before ROM
export. Unsafe batches are rejected instead of being automatically rewritten.
This preserves the check protecting player-bullet collision and still accepts
valid empty rooms.

Current projects need only a rebuilt GEditor. If a project relies on a removed
format, rebuild GUD from current sources and create a fresh project, then use
the current import tools to bring in edited assets. Keep the matching base ROM
and native edit files with each project.

## Regression checks

From the repository root on Linux/WSL with Python 3 and a native C compiler:

```sh
python3 tools/geditor/tests/current_formats/run.py
python3 tools/geditor/tests/image_import/run.py
```

The checks use address and undefined-behavior sanitizers. They exercise current
project/ROM loading, authored names and fog, the NULL-name level sentinel,
level-table export offsets, rejected old or mismatched formats, safe background
vertex batches, and the image encoding, editing, and ROM export workflows.
The image suite also checks normalized model UVs when a replacement image has
different dimensions. Windows dialogs and in-game rendering need runtime checks.
