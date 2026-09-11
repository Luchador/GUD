#!/usr/bin/env python3
"""Exercise the viewport's real marquee code without a Windows desktop.

Projection, geometry collection, selection updates and drag lifecycle come
from viewport.c. Only window messages and UI refreshes are stubbed.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^static \w+ " + name + r"\([^;{}]*\)\s*\{", source, re.M)
    if match is None:
        raise RuntimeError(f"Missing production function: {name}")
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end] + "\n"


def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / "src"
    source = (src / "viewport.c").read_text()
    types = ""
    for name in ("SceneBatch", "Vertex", "ViewportComponent", "ViewportStanComponent"):
        types += re.search(r"typedef struct " + name + r" \{.*?\} " + name + ";", source, re.S)[0] + "\n"
    for name in ("FOV_Y", "NEAR_Z", "FAR_Z", "DEG_TO_RAD"):
        types += re.search(r"^#define VIEWPORT_" + name + r" .*", source, re.M)[0] + "\n"
    logic = ""
    for name in ("ViewportGetBasis", "ViewportProject", "ViewportTriangleHidden",
                 "ViewportBatchIsPickable", "ViewportCompareVertexRefs",
                 "ViewportStanVisible", "ViewportStanPointVertex", "ViewportStanPointRef"):
        logic += function(source, name)
    start = source.index("static void ViewportCancelBoxSelection(HWND hwnd, ViewportState *state)\n{")
    logic += source[start:source.index("static void ViewportDrawBoxSelection(", start)]
    with tempfile.TemporaryDirectory(prefix="geditor-box-selection-") as temp:
        work = Path(temp)
        (work / "types.inc").write_text(types)
        (work / "logic.inc").write_text(logic)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O1", "-g", "-Wall", "-Wextra",
                        "-Werror", "-Wno-unused-parameter", "-fsanitize=address,undefined",
                        f"-I{here.parent / 'image_import'}", f"-I{src}", f"-I{work}",
                        str(here / "check.c"), "-lm", "-o", str(work / "check")], check=True)
        subprocess.run([str(work / "check")], check=True,
                       env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))


if __name__ == "__main__":
    main()
