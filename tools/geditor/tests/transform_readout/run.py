#!/usr/bin/env python3
"""Check gizmo readouts against applied transforms and viewport boundaries."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
viewport = (src / 'viewport.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-transform-readout-') as temp:
    work = Path(temp)
    (work / 'logic.inc').write_text(
        re.search(r'^#define VIEWPORT_UNIFORM_SCALE_AXIS .*', viewport, re.M)[0] + '\n'
        + ''.join(extract.function(viewport, name) for name in (
            'ViewportTransformReadoutText', 'ViewportTransformReadoutBounds')))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}',
        f'-I{src}', f'-I{work}', str(here / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
