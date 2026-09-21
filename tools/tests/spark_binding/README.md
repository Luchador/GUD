# Spark texture binding reuse

Run from the repository root:

```sh
python3 tools/tests/spark_binding/run.py
```

The host harness compiles the production spark-rendering functions. It records
texture selections and geometry commands, then compares every batched draw with
an independently bound isolated draw. It covers both spark arrays, skipped
sparks, animation changes, aliased images with different sampling settings,
rooms, per-spark colors, all three depth modes, and fresh bindings on each pass.
Seventy matching visible sparks require one texture selection instead of seventy.

Camera/allocation services and GBI emission are recording substitutes. Texture
loading and rasterization are not emulated; appearance and FPS still need an N64
test. No persistent cache or Expansion Pak memory is used.
