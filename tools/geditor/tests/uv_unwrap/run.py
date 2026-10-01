#!/usr/bin/env python3
"""LSCM shape preservation, connected arches, seams, islands and failure safety."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
with tempfile.TemporaryDirectory(prefix='geditor-uv-unwrap-') as temp:
    binary = str(Path(temp) / 'check')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g',
                    '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    f'-I{src}', str(here / 'check.c'), '-lm', '-o', binary], check=True)
    subprocess.run([binary], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
