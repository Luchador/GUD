#!/usr/bin/env python3
"""Test studio folder migration and asset catalogs using the POSIX file shim.

The WIC decoder is stubbed; native BMP decoding and the UI need Windows testing.
"""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    platform = here.parent / 'project_rebase'
    with tempfile.TemporaryDirectory(prefix='geditor-studio-assets-') as temp:
        work = Path(temp)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{platform}', f'-I{src}',
                   f'-I{src.parents[2]}', str(here / 'check.c'), str(platform / 'platform.c'),
                   str(src / 'project.c'), str(src / 'studioassets.c'), '-Wl,--gc-sections', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
