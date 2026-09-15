#!/usr/bin/env python3
"""Check cloud metadata and sky sampling using production code; no ROM required."""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-cloud-preview-') as temp:
        exe = Path(temp) / 'check'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                        f'-I{shim}', f'-I{src}', str(here / 'check.c'), str(src / 'rom.c'),
                        str(src / 'clouds.c'), '-Wl,--gc-sections', '-lm', '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
