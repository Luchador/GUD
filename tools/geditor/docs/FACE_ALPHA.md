# Face alpha presets and opacity

Select background faces and open **Properties > Advanced**. **Alpha preset**
chooses the output alpha independently of RGB shading, transparency mode,
decal depth handling, and geometry layer.

| Preset | Output alpha | Fog |
| --- | --- | --- |
| Auto (preserve original) | Original material equation | Original behavior |
| Fully opaque | 100% | Preserved |
| Texture alpha | Base texture alpha | Preserved |
| Vertex alpha | Painted vertex A | Disabled on these faces |
| Texture x vertex alpha | Base texture alpha multiplied by painted A | Disabled on these faces |
| Constant opacity | Opacity value | Preserved |
| Texture x opacity | Base texture alpha multiplied by opacity | Preserved |

**Opacity** is enabled when all selected faces use Constant opacity or
Texture x opacity. Enter a percentage from **0 to 100** and press **Enter**
to apply it; **Escape** restores the displayed value. The game stores 256
alpha levels, so the field shows the nearest representable percentage to
one decimal place (for example, 50% becomes 50.2%, or 128/255).

Different values display **Mixed**. An opacity edit changes only opacity,
including selections mixing the two constant presets. Selecting
**Mixed / Keep current** in the preset dropdown leaves each face unchanged.
Changing presets preserves the stored opacity and vertex colors. Auto restores
the original alpha equation; it does not undo a separate opacity edit.

## Using the presets

For soft transparency, set **Transparency** to **Translucent (alpha blend)**
and use the **Secondary** layer. Opaque and cutout render modes still control
how the chosen alpha is used; reducing opacity alone does not enable blending.

- **Light beams:** use Vertex alpha and paint a higher A near the light source,
  fading to zero at the outer vertices. Disable backface culling if necessary.
- **Faded textured decals:** use Texture x vertex alpha for painted fades, or
  Texture x opacity for a uniform fade that retains the texture's transparent
  areas. Enable Decal for surfaces placed directly over other geometry.
- **Glass or uniform transparency:** use Constant opacity to ignore texture
  alpha, or Texture x opacity to retain it.

The N64 uses vertex alpha for fog. Both presets using painted A disable fog
for those faces and restore it for following geometry. Constant and texture
presets retain the original fog behavior. The viewport and painting preview
use the same alpha factors and fog choice as the exported game.

Texture presets require a base image. Removing the image converts Texture alpha
to Fully opaque, Texture x vertex alpha to Vertex alpha, and Texture x opacity
to Constant opacity. It preserves the remaining alpha factor.

Edits support multiple selections, undo/redo, copy/paste, project saves and ROM
export. Opacity edits preserve the environment color's RGB bytes and restore
the surrounding faces' state. Explicit presets require an ordinary, explicit
one- or two-cycle pipeline; texture presets additionally require explicit
LOD/detail settings in two-cycle mode. Unsupported selections report an error
before changing any faces.

## Preview scope

The viewport evaluates the selected alpha product, including texture alpha,
painted A and constant opacity, while retaining existing transparency and fog
preview behavior. This is not a pixel-exact N64 renderer: texture filtering,
LOD/detail rendering, coverage, dithering and transparent draw ordering retain
the existing viewport's limitations. These presets do not add a general custom
RGB-combiner preview.

## Build and format notes

Rebuild **both GEditor and GUD**, then **rebase the project onto the rebuilt
GUD ROM** before exporting. Existing Auto/Vertex alpha values remain compatible
with current projects. The additional presets need the updated game loader;
older builds do not interpret them correctly. Use the updated editor to retain
the new presets.

Opacity is stored as native environment alpha. The compiler writes alpha-policy
tags and six reserved NoOp packets per boundary. Texture expansion passes them
through, then the runtime fills the reserved packets with a pipe sync, fog
geometry state and four partial blender writes. Combiner alpha overrides happen
after the game's material LUT, preserving its RGB result. In two-cycle texture
presets the first alpha cycle samples the base tile or interpolates the mip pair;
with detail enabled it samples the base tile behind the detail image. The second
cycle applies the vertex or opacity factor. One-cycle presets use the texture
directly without depending on a previous combined result.

Depth, coverage and the final surface blender are preserved. Scope entry is
before vertex loading; exit restores fog, blending and combiner state even at
the end of a display list. The one-cycle optimizer leaves explicit alpha
presets unchanged. Saving rebuilds the policy packets, and repeated opacity
edits discard superseded state writes.

Validation:

```
python3 tools/geditor/tests/face_alpha/run.py
python3 tools/geditor/tests/bg_transparency/run.py
python3 tools/geditor/tests/face_detail/run.py
python3 tools/geditor/tests/face_rooms/run.py
python3 tools/tests/bg_onecycle/run.py
```

The editor checks cover every preset, all 256 opacity values, input validation,
shared vertices, mixed faces/rooms/layers, preview and painting, RGB preservation,
history, copy/paste, texture removal, stable repeated edits/saves and native Depot
geometry. Runtime tests independently evaluate the generated alpha equations
across all four BG LUTs, one/two cycles, mip/detail/sharpen sampling, fog scope
restoration and AA toggles. These automated checks do not replace visual testing
in Windows and on the target emulator or hardware.
