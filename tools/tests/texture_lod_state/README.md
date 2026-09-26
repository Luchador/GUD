# Texture LOD state regression

Run from the repository root:

```sh
python3 tools/tests/texture_lod_state/run.py
```

To check every room stream in an uncompressed GEditor background against the
texture records in its exported ROM:

```sh
python3 tools/tests/texture_lod_state/run.py --rom GUD.z64 --bg bg_arec_all_p.seg
```

The test extracts the production `texLoadFromGdl` and `texWriteTextureCmd`
functions. Host adaptations cover command byte order and pointer width;
texture allocation and uploads are stubs. It checks the final command stream
at every draw, including maximum LOD, tile, texture scale and enable state.
It does not emulate rasterization, TMEM, animated water or detail blending.

The regression occurs when a generated `G_TEXTURE` command changes the mip
count but the loader keeps tracking an older command. Returning to that older
count can then skip a required update. A second case involves consecutive
texture markers after a draw: if the first marker emits no command, the next
must not rewrite the command already used by earlier triangles.

Coverage includes all combinations of three mip counts (zero through seven),
TRI1/TRI4, returning to the first texture, consecutive markers, an initially
missing texture command, authored state changes and reserved editor tags.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled. `CC` chooses the
host compiler; `TEX_SOURCE` can select a pre-fix source file for comparison.
