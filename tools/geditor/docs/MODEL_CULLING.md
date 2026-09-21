# Model preview culling

Rebuild GEditor and reopen the project. This change needs no ROM rebuild or
project rebase.

The level viewport and Model Editor resolve world-model culling per triangle:

- Unspecified culling uses the normal world-model backface-culling default.
- Native set/clear commands override each side independently, including partial
  clears, front-face culling, two-sided faces and culling of both sides.
- Type-3 secondary lists inherit the primary list's culling. Type-4 secondary
  lists start with the renderer's per-pass default again.
- Flipped doors resolve the game's front-face default before the preview's
  winding reversal. Explicit model overrides are handled separately from
  inherited door defaults; other instances of the same model are unaffected.

Props, characters and their equipment use the same model decoder. Face and
object picking already share the viewport's per-batch culling rules. The main
viewport's existing Backface Culling toggle remains available as an override.

The preview retains native culling provenance separately from its resolved draw
flags. The Model Editor still labels unauthored settings as `Inherited`, and
previewing a model does not write culling commands into its native asset.
New glTF exports preserve per-side inheritance in their preview metadata. Old
cached previews use the world-model default for unspecified state; loading
native model data remains preferred, including in existing projects.

Verification: `python3 tools/geditor/tests/model_culling/run.py`, the Depot
`door_preview` checks, `model_properties_ui`, and `model_vertex_uv` (including
native edits, save/reload and glTF round trips).
