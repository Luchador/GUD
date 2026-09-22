#!/usr/bin/env python3
"""Test the native SFX catalog. Optionally pass a GUD .z64 to inspect as well."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
editor = here.parents[1]
with tempfile.TemporaryDirectory(prefix='geditor-sfx-') as directory:
    binary = str(Path(directory) / 'check')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(here.parent / 'image_import'),
                    '-I' + str(editor / 'src'), str(here / 'check.c'), str(editor / 'src/sfxcatalog.c'),
                    '-o', binary], check=True)
    subprocess.run([binary] + sys.argv[1:], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
