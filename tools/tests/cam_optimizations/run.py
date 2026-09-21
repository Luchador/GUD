#!/usr/bin/env python3
"""Compare camera math with its pre-optimization reference (without retired prop fading)."""
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


here = Path(__file__).resolve().parent
root = here.parents[2]
cam = (root / "src/game/cam.c").read_text()
env = (root / "src/game/environment.c").read_text()
player = (root / "src/game/player.c").read_text()
assert "camInvalidatePlayerCameraScale(player_num);" in function(player, "playerInitData")

# Keep the test connected to production functions and count real scale work.
source = (here / "harness.h").read_text()
source += cam[cam.index("coord3d g_CamFrustumTopNormal;"):cam.index("void camInvalidatePlayerCameraScale(")]
source += "#define sinf countScaleSinf\n"
for name in ("camInvalidatePlayerCameraScale", "camSetPlayerScreenSize", "camSetPlayerScreenPosition",
             "camSetPlayerPerspective", "camSetPlayerCameraScale", "camUpdateFrustumPlanes",
             "camIsPosInScreen", "camPrepareScreenBoxCache", "camIsPosInScreenBox", "camIsPosOnScreen"):
    source += function(cam, name)
source += "#undef sinf\n"
source += function(env, "envIsPropVisibleThroughFog")
source += (here / "reference.c").read_text()
source += (here / "check.c").read_text()

with tempfile.TemporaryDirectory(prefix="gud-cam-optimizations-") as temp:
    work = Path(temp)
    (work / "check.c").write_text(source)
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-fno-builtin",
                "-fno-strict-aliasing", "-ffp-contract=off"]
    command += ["-I", str(root / "src/game"), "-idirafter", str(root / "include"),
                "-idirafter", str(root / "include/ultra64")]
    command += [str(work / "check.c"), str(root / "src/game/math_sincos.c"),
                "-lm", "-o", str(work / "check")]
    subprocess.run(command, check=True)
    subprocess.run([str(work / "check")], check=True)
