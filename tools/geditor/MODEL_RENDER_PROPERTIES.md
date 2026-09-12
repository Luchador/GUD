# Model face properties

Open **Tools → Model Editor** and choose a model. The right-hand **Face
properties** panel reads the native model's draw state.

- Click a visible face to select it. **Shift-click** adds faces; **Ctrl-click**
  removes them. Drag either mouse button to orbit; scroll to zoom.
- Choose a **Part / texture group** and press **Select group** to select a
  material-sized group, including faces currently hidden by culling. **Select
  all** selects every visible LOD's faces. Characters also have **Show all LODs**.
- The current culling and render mode are shown separately from the edit
  controls. **Keep current** leaves a property alone, including mixed selections.
- Choose **Disabled (two-sided)**, **Cull back faces**, or **Cull front faces**,
  and/or **Opaque**, **Cutout**, or **Alpha blend**. Press **Apply to selected
  faces**. The viewport and placed instances refresh immediately.
- **Save Project** stores the compiled native replacement in the existing
  `.gmodel` format and refreshes its glTF export. ROM creation uses that saved
  replacement. Export a fresh Blender file after changing native properties;
  an export from before the edit has an obsolete revision fingerprint.

A part here is a native display-list pass. Different faces can have different
properties while sharing the same texture. Editing a texture group changes
only the selected faces of that model; it does not change the image or all
materials using that image. The primary/secondary submission pass is retained.

## Native behavior and limits

| Render mode | Native surface preset | Depth writes in a depth-tested part |
| --- | --- | --- |
| Opaque | AA_ZB_OPA_SURF | Enabled |
| Cutout | AA_ZB_TEX_EDGE | Enabled |
| Alpha blend | AA_ZB_XLU_SURF | Disabled |

Cutout uses the N64's alpha-to-coverage texture-edge mode. The editor approximates
it with an alpha test; filtered edges on hardware can differ. It retains the
part's existing alpha combiner, vertex/material opacity, and alpha-compare
setting. A material that ignores texture alpha will continue to do so.

These are explicit surface presets for ordinary model parts (types 1–4). Fog,
combiner commands, cycle type, alpha/depth source, texture state, and vertex data
are retained. Custom copy/fill pipelines are rejected. Runtime-specific fades
and passes that disable depth buffering are not encoded in a model asset:
applying a surface override can change those behaviors. These presets are
intended primarily for world props such as foliage; check other uses of an
edited model on hardware as well.

**Inherited** culling means the display list has not explicitly established both
cull bits. The game may choose them based on the caller, including mirrored
models. Culling edits on these faces are rejected because native display lists
cannot save and restore an unknown caller state. Faces with explicitly authored
culling, including the two-sided Jungle foliage, support per-face changes.
Render-mode edits can still be made with culling set to **Keep current**.
A type-4 secondary pass has a fresh caller-dependent cull setup; it does not
inherit the primary pass's culling in the inspector.

Property edits preserve face order and split packed TRI4 commands as needed.
Consecutive edited draw commands share their state changes. Render-mode changes
and restoration include PipeSync. Reapplying unchanged settings is byte-exact.
Backface culling is exported as glTF `doubleSided: false`; front/both-side culling
is retained in GoldenEye metadata because glTF has no direct equivalent.

Blender material controls do not rewrite these native states on import. Use this
panel to edit them; geometry/UV/color round trips retain the native commands.

## Verification

```
python3 tools/geditor/tests/model_vertex_uv/run.py
python3 tools/geditor/tests/model_properties_ui/run.py
```

The host regressions use production native parsing/compilation, glTF and edit
storage with ASan/UBSan. They cover real Jungle tree assets, one/two-cycle
fixtures, selected TRI4 faces and untouched neighbours, state preservation,
no-op reapplication, source revision checks, save/reload, ROM replacement, and
subsequent Blender edits. The UI test runs the production orbit-input, culling
and inspector-label functions with window calls stubbed. It does not replace
building GEditor on Windows or testing an edited ROM on an N64.
