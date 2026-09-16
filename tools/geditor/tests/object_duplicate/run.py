#!/usr/bin/env python3
"""Object snapshots, native records, real placement/transform math and history."""
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
    # All prop record sizes, including multi-monitor and expanded ammo crates.
    sizes = {1:64, 3:32, 4:33, 5:32, 6:59, 7:33, 8:34, 10:64, 11:149,
             12:32, 13:54, 17:32, 20:44, 21:34, 36:32, 39:44, 40:45,
             42:32, 43:32, 45:56, 47:37}
    start = (len(data) + 3) & ~3
    data += bytes(start - len(data))
    for kind, words in sizes.items():
        record = bytearray(words * 4)
        struct.pack_into('>4I', record, 0, 0x01000000 | kind, 1 << 16, 0, 0x100)
        struct.pack_into('>I', record, 0x74, 777 << 16)
        if kind == 1:
            struct.pack_into('>I', record, 0x80, 1)  # Relative sibling is not copied.
        if kind in (6, 13):
            struct.pack_into('>I', record, 0x80, 0)
        if words > 33:
            struct.pack_into('>I', record, words * 4 - 4, 0x12345678)
        data += record
    data += struct.pack('>I', 48)
    struct.pack_into('>I', data, 12, start)
    with tempfile.TemporaryDirectory(prefix='geditor-object-duplicate-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        (work / 'setup/UsetupduplicateZ.set').write_bytes(data)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}']
        sources = ('setupload.c', 'actionblocks.c', 'bghistory.c', 'objectload.c',
                   'objectshade.c', 'rotation.c', 'scaling.c', 'stanquery.c')
        subprocess.run(command + [str(here / 'check.c'), str(here / 'assets.c'), str(shim / 'platform.c')]
                       + [str(src / name) for name in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
