#!/usr/bin/env python3
"""Check selection history and real viewport snapshot code without a Windows GUI."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^(?:static )?(?:LRESULT CALLBACK|\w+) " + name + r"\([^;{}]*\)\s*\{", source, re.M)
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
    viewport = (src / "viewport.c").read_text()
    uv = (src / "uvcanvas.c").read_text()
    with tempfile.TemporaryDirectory(prefix="geditor-selection-history-") as temp:
        work = Path(temp)
        types = ""
        for source, names in ((viewport, ("SceneBatch", "Vertex", "ViewportComponent", "ViewportStanComponent", "ViewportPad")),
                              (uv, ("UVCanvasNode", "UVCanvasState"))):
            for name in names:
                types += re.search(r"typedef struct " + name + r" \{.*?\} " + name + ";", source, re.S)[0] + "\n"
        (work / "types.inc").write_text(types)
        logic = ""
        for name in ("ViewportTriangleHidden", "ViewportCompareVertexRefs", "ViewportCornerVisible",
                     "ViewportFindVertexCorner", "ViewportRestoreComponents", "ViewportCompareFaceRefs",
                     "ViewportStanVisible", "ViewportSelectedPadIndex", "ViewportClearAllSelection"):
            logic += function(viewport, name)
        logic += viewport[viewport.index("/* Pointer-free, level-local history snapshot."):]
        logic += function(uv, "UVCanvasNodeCompare")
        logic += uv[uv.index("/* Source identity and the selected texture's coordinate basis,"):]
        (work / "snapshots.inc").write_text(logic)
        (work / "frame.inc").write_text(function((src / "geditor.c").read_text(), "GEditorWndProc"))
        for name in ("history", "snapshots"):
            binary = work / name
            subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O1", "-g", "-Wall", "-Wextra",
                            "-Werror", "-Wno-unused-parameter", "-fsanitize=address,undefined",
                            f"-I{here.parent / 'image_import'}", f"-I{src}", f"-I{work}",
                            str(here / (name + ".c")), "-lm", "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))


if __name__ == "__main__":
    main()
