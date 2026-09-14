#!/usr/bin/env python3
"""Compare production gameplay optimizations with the frozen pre-patch code."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^(?:static )?[\w *]+\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    assert match, name
    end = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end] + "\n"


def portable(source):
    # Legacy casts use a 32-bit integer as a pointer temporary. The equivalent
    # direct access lets the unchanged traversal logic run on a 64-bit host.
    source = source.replace("i = (s32) &g_PortalTraversalDepths[portalnum];", "")
    source = source.replace("*((u8 *) i) = depth;", "g_PortalTraversalDepths[portalnum] = depth;")
    source = source.replace("if (1);", "(void)0;")
    source = source.replace("if (portal_pts->numPoints);", "(void)0;")
    # These old functions cast storage to compatible N64 union/coordinate
    # pointers. Host C wants the declared destination type.
    source = source.replace("(coord3d*)aim_screen", "(coord2d*)aim_screen")
    for edge in ("left", "right", "top", "bottom"):
        source = source.replace("(coord3d*)screen_" + edge, "(coord2d*)screen_" + edge)
    source = source.replace("&matrices[mtxindex]);", "&matrices[mtxindex].pos);")
    source = source.replace("depth + 1, &screenbox)", "depth + 1, screenbox.f[0])")
    source = source.replace("bgIsRoomOnScreen(i, &g_CurrentPlayer->screensize)",
                            "bgIsRoomOnScreen(i, (struct rectbbox *)&g_CurrentPlayer->screensize)")
    return source


here = Path(__file__).resolve().parent
root = here.parents[2]
bg = (root / "src/game/bg.c").read_text()
chrprop = (root / "src/game/chrprop.c").read_text()
matrix = (root / "src/game/matrixmath.c").read_text()
model = (root / "src/game/model.c").read_text()
chr = (root / "src/game/chr.c").read_text()

# Keep loading and both optimized model call sites connected to the tests.
assert "bgBuildPortalCache();" in function(bg, "bgLoadFile")
assert "matrix_4x4_multiply_translation(" in function(model, "process_15_subposition")
assert "chrUpdateHat(" in function(chr, "chrTick")
assert "matrix_4x4_apply_scale_and_translation(" in function(chr, "chrUpdateHat")

source = (here / "harness.h").read_text()
source += bg[bg.index("#define BG_PORTAL_ROOM_COUNT"):bg.index("s_specialportal specialportalarray")]
for name in ("bgGetRoomPortalList", "bgBuildPortalCache", "bgIsBboxOverlapping",
             "bgGetConnectedRooms", "bgRoomsSharePortal", "bgGetPortalBetweenRooms",
             "bgGetRoomsIntersectingBbox", "bgTick", "bgProcessPortalTraversal", "bgDetermineVisibleRooms"):
    source += portable(function(bg, name))
for name in ("roomGetProps", "chrpropScoreAutoAimTarget"):
    source += function(chrprop, name)
for name in ("matrix_4x4_multiply_homogeneous", "matrix_4x4_set_identity_and_position",
             "matrix_4x4_multiply_translation", "matrix_4x4_apply_scale_and_translation",
             "matrix_column_1_scalar_multiply", "matrix_column_2_scalar_multiply",
             "matrix_column_3_scalar_multiply_2"):
    source += function(matrix, name)
source += portable(function(model, "process_15_subposition"))
source += function((root / "src/game/cam.c").read_text(), "transform3Dto2DCoords")
source += portable((here / "reference.c").read_text())
source += (here / "check.c").read_text()

with tempfile.TemporaryDirectory(prefix="gud-gameplay-optimizations-") as temp:
    work = Path(temp)
    (work / "check.c").write_text(source)
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-unused-but-set-variable",
                "-Wno-sign-compare", "-Wno-maybe-uninitialized", "-fno-builtin",
                "-fno-strict-aliasing", "-ffp-contract=off"]
    command += shlex.split(os.environ.get("TEST_CFLAGS", ""))
    command += [str(work / "check.c"), "-lm", "-o", str(work / "check")]
    subprocess.run(command, check=True)
    subprocess.run([str(work / "check")], check=True)
