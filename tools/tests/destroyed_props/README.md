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

The model harness runs the production node renderer and verifies that clearing
an instance's screen list suppresses both primary and secondary draw commands,
even when the shared model still has its authored lists.

Use Silo on hardware to assess frame rate and the appearance of culled shells.
