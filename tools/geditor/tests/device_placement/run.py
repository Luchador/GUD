#!/usr/bin/env python3
"""CCTV/alarm/drone native placement, pad references, persistence and undo/redo."""
import os
from pathlib import Path
import runpy
import struct
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    data = runpy.run_path(str(here.parent / 'door_placement/run.py'))['fixture']()
    with tempfile.TemporaryDirectory(prefix='geditor-device-placement-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        (work / 'setup/UsetupdevicesZ.set').write_bytes(data)
        empty = bytearray(data)
        struct.pack_into('>I', empty, 12, 0)
        struct.pack_into('>I', empty, struct.unpack_from('>I', empty, 24)[0] + 36, 0)
        (work / 'setup/UsetupemptyZ.set').write_bytes(empty)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / name) for name in ('setupload.c', 'bghistory.c', 'modelload.c', 'rotation.c', 'scaling.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
