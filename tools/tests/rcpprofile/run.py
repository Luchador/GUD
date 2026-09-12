#!/usr/bin/env python3
"""Compile the production accounting module with only libultra types mocked."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix="gud-rcpprofile-") as temporary:
    binary = Path(temporary) / "check"
    subprocess.run(
        shlex.split(os.environ.get("CC", "cc"))
        + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
           "-I", str(here), "-I", str(root / "src"),
           str(here / "check.c"), str(root / "src/rcpprofile.c"),
           "-o", str(binary)], check=True
    )
    subprocess.run([str(binary)], check=True)
