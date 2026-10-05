# New character heads and bodies

This is the first stage of restoring the Connery, Moore and Dalton characters.
It adds character registration and rig-preserving imports. It does not yet rig
the extracted XBLA meshes or add playable actor choices to the menus.

## Build and use

1. Apply the patch, rebuild **both GUD and GEditor**, and rebase the project onto
   the rebuilt GUD ROM. Character imports require the new ROM capability.
2. Export a stock character model from GEditor's Model Editor. For the Bond
   restoration, `CdjbondZ` is the tuxedo body template and `CheadbrosnanZ` is an
   attachable head template.
3. Edit the exported GLB/glTF. Preserve GoldenEye source attributes, custom
   properties and material slots. Do not apply another unit conversion:
   character exports retain the existing native character coordinate system.
4. Use **File > Import > Import Model...**, the Models panel's import menu, or
   Import Model in an empty Model Editor. Select the edited GLB, choose
   **Characters**, choose **Body** or **Head**, and select the same stock rig
   template. Give the new model a unique name.
5. Save the project. The new model has its own character ID and native data;
   importing it does not replace the template. Later edits use the normal
   Model Editor export/import, material, UV and paint tools.
6. Drag a new body from Models into a level to create a character. Use the
   selected character's **Body** and **Head** properties to pair the models.
   These edits support undo/redo and save to the setup. Bodies with integrated
   heads disable the separate head selector. Use animation preview to inspect
   a body's retained rig.

## What the template supplies

The native hierarchy, animation skeleton, switch/matrix counts, collision
bounds, attachment points, sex, body scale, animation translation scale and
integrated-head policy are retained. Added heads also use their template's hat
fitting information. Importing a different model's export or a stale export
fails without adding a bank entry.

This first pass uses the existing rig-preserving model compiler. Vertex/UV/color
edits and supported face deletion work on animated parts. Retopology remains
limited to the existing rigid, vertex-colored part importer with retained
material-slot associations. It does **not** convert arbitrary glTF skin weights
or infer bone bindings for a new body mesh. The extracted XBLA files must be
fitted and given compatible bindings in a subsequent restoration pass; changing
their names alone is insufficient.

## IDs, project data and multiplayer

- Stock character IDs 0–79 retain their meanings. Up to 48 additional heads and
  bodies share IDs 80–127, within the game's signed-byte character fields.
- Added prop slots and character IDs are separate. Earlier static imports in
  the Characters category retain their existing prop behavior.
- Character data and metadata are saved in the project's `models/newprops.gnp`
  bank and packed into the exported ROM. Reloading an exported ROM recovers
  the registrations. Rebases retain the project's bank and reject conflicting
  ID/type/template assignments or ROMs without character-import support.
- GUD registers added models in the same `CitemZ_entries` table used by guards,
  single-player Bond and multiplayer players. The existing model-loading paths
  can use these IDs. This patch adds setup-character selection in GEditor;
  **single-player actor/outfit selection and multiplayer roster/menu entries
  are the next step**, after the actor geometry has been prepared.

## Verification

`python3 tools/geditor/tests/new_characters/run.py` exercises real native head
and body GLB round trips, independent character/prop IDs, animation posing,
material edits, placement classification, save/reload, ROM extraction, rebase
conflicts and malformed runtime registrations with ASan/UBSan. The runtime
harness adapts word order and pointer width for host execution.

The character equipment/setup, animation, import-flow, static-model import and
project-rebase regression suites also cover the affected paths. The changed
runtime files compile with the N64 IDO compiler, and GEditor is cross-built for
Windows. Hardware/emulator rendering and interactive Windows UI testing remain
necessary when the restored actor meshes are ready.
