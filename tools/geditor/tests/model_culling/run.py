#!/usr/bin/env python3
"""Production native decoding and placed-model culling under ASan/UBSan."""
from pathlib import Path
import os
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
shim = here.parent / 'image_import'
with tempfile.TemporaryDirectory(prefix='geditor-model-culling-') as folder:
    binary = Path(folder) / 'check'
    command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
               '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}',
               str(here / 'check.c')]
    command += [str(src / file) for file in ('modelload.c', 'modelmaterials.c', 'bgmaterial.c', 'bgrender.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
