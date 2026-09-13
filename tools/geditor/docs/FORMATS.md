# Supported GEditor formats

GEditor reads and writes the current project and asset formats. It does not
migrate files produced by older editors.

| Data | Required format |
| --- | --- |
| Project | `.gep` version 2 with a saved level table and matching `base.z64` |
| Base ROM | GUD manifest version 2, with `IMGS`, `OBSG`, `MUSF`, `STGT`, `CMAP`, `FTBL`, `ENVT`, `TXTB`, `TXCF`, `MONA`, `MONT`, and `MOND` entries |
| Level table | 36-byte rows with authored level names; the final sentinel may have a NULL name |
| Model assets | glTF 2.0 with normalized UV coordinates and the current GEditor metadata |
| Saved model edits | Current `.gmodel` native overrides (`GMD1`) |
| Saved image edits | `.gtex` metadata (`GTI3` with an import source, or `GTI2` without one) and the associated BMP, except for deletion records |

New ROMs also expose optional audio, animation, model, global AI and text-bank
catalogs. See [MANIFEST.md](MANIFEST.md) for their ranges and record layouts.

The model import workflow still supports Blender glTF/GLB files. Its material,
sampler, and alpha settings remain supported. Embedded GLB editor resources and
graphics-driver fallbacks are also unchanged.

Removed compatibility paths include version-1 projects and manifests, unnamed
32-byte level rows, the prompt to supply a missing base ROM, image-table
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
