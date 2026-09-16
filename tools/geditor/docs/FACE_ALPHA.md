# Face Alpha Source

The **Alpha Source** dropdown is at the bottom of Face Properties.

- **Auto** keeps the original material's alpha behavior.
- **Vertex alpha** uses the painted vertex A value: 0 is transparent and 255
  is opaque. The alpha interpolates across the face. Vertex RGB and texture RGB
  keep their existing behavior; texture/environment alpha is not multiplied in.
- **Mixed / Keep current** indicates different sources in a multiple selection
  and leaves each face's setting unchanged.

For untextured window light beams, choose **Vertex alpha**, set **Transparency**
to **Translucent (alpha blend)**, and use the **Secondary** layer. Paint a higher
A near the window and fade to 0 at the outer vertices. Disable backface culling
if the beam should be visible from both sides. These other properties are
independent; changing Alpha Source does not change the layer or transparency.

N64 fog uses the vertex alpha channel, so Vertex alpha automatically disables
fog for those faces. Fog is restored for following geometry. The viewport
previews the same choice. Existing opaque/cutout modes still determine how
the resulting alpha is used, so select Translucent for a soft fade.

Changes apply to all selected faces and support undo/redo, copy/paste, project
saves, and ROM export. Auto restores the original combiner, including custom
RGB settings. Vertex alpha requires explicit one- or two-cycle rendering state
with a standard fog/pass first blender; unsupported selections report an error
before changing any faces.

## Build and format notes

Rebuild both **GEditor and GUD**, and export using the rebuilt GUD ROM as the
project's base. This feature needs the updated game loader; older ROMs interpret
the new tags as texture requests. Older editors cannot preserve these tags.

The compiler writes alpha-policy tags and six reserved NoOp packets per
boundary. Texture expansion passes them through, then the runtime fills the
reserved packets with a pipe sync, fog geometry state and four partial blender
writes. Combiner alpha overrides happen after the game's material LUT. RGB,
depth, coverage and the final surface blender are preserved. Scope entry is
before vertex loading, and exit restores fog/blending/combiner state even at
the end of a display list. The one-cycle optimizer preserves Vertex alpha draws.
Saving rebuilds these packets instead of accumulating copies.

Validation:

```
python3 tools/geditor/tests/face_alpha/run.py
python3 tools/tests/bg_onecycle/run.py
```

The editor tests cover shared vertices, mixed faces/rooms/layers, preview,
history, copy/paste, restoring Auto, 50 save/reload passes and native Depot
geometry. Runtime checks exercise texture-marker dispatch, all four BG LUTs,
fog restoration, combiner RGB preservation, AA toggles and one-cycle behavior.
