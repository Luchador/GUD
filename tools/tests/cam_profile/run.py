#!/usr/bin/env python3
"""Run the camera capture tests with a native C compiler, without a ROM."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
compiler = shlex.split(os.environ.get("CC", "cc"))

with tempfile.TemporaryDirectory(prefix="gud-cam-profile-") as temp:
    executable = Path(temp) / "check"
    subprocess.run(
        compiler
        + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O2"]
        + ["-I", str(root / "src/game")]
        + ["-idirafter", str(root / "include")]
        + ["-idirafter", str(root / "include/ultra64")]
        + [str(Path(__file__).with_name("check.c")), str(root / "src/game/camprofile.c")]
        + ["-o", str(executable)],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
