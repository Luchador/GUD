# Destroyed-prop rendering regression

Run from the repository root:

```sh
python3 tools/tests/destroyed_props/run.py
python3 tools/tests/model_onecycle/run.py
```

The first harness compiles the production screen-hiding helper and prop render
dispatcher. It checks instance isolation, surviving body geometry, no monitor
rebuilds while destroyed, screen restoration on respawn, single/four-screen
props, missing nodes, damage levels, deformation allocation fallback, fading
and door culling. Graphics backend calls are stubs; monitor animation and
rasterization are not emulated. It also checks the helper's destruction hook.

Destroyed props suppress the native translucent model pass from their first
destroyed stage. This includes imported lamps' light shafts, regardless of model
name. The primary shell still renders, including when the whole prop is fading.
Filtering is per draw: shared intact instances, live attached children and
respawned props retain their translucent geometry. Both type-3 secondary lists
(submitted with the primary pass) and type-4 secondary lists are covered.
The tests check these cases with and without allocated deformation vertices.

This is a game renderer change. Build the updated GUD ROM and rebase an existing
GEditor project onto it before using **Create ROM**. No model reimport is needed
for imported models whose translucent faces are already in their secondary list.

The model harness runs the production node renderer and verifies that clearing
an instance's screen list suppresses both primary and secondary draw commands,
even when the shared model still has its authored lists.

Use Silo on hardware to assess frame rate and the appearance of culled shells.
