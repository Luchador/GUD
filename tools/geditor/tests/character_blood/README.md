# Imported character blood stains

Run `python3 tools/geditor/tests/character_blood/run.py path/to/exported.z64`.
The ROM must contain imported character bodies. No ROM assets are retained.

The test uses the production model reader and export repair under ASan/UBSan.
It checks that every rendered corner belongs to a live vertex buffer, that
collision-point chains reach every buffer vertex without cycles, and that
positions, joints, UVs, colors, materials and face order remain unchanged.
A second repair must be a no-op.

Run `python3 tools/tests/model_onecycle/run.py` for the renderer regression.
Its imported-blood case exercises own-part and cross-part vertex loads, loads
spanning buffers, shared-model player isolation, AA modes, fading, and the
frame-owned fallback after the optional render cache is reclaimed.

The fix requires rebuilding both GUD and GEditor, rebasing the project, and
exporting a new ROM. Export repairs existing imported bodies automatically;
models do not need to be reimported. The repair operates on the export copy,
preserving project edit history. These host tests do not emulate rasterization.
