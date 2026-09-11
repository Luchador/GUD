#!/usr/bin/env python3
"""Door placement: native setup bytes, private pads, persistence and history."""
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def fixture():
    data = bytearray(40)
    # Intro camera and terminator: must retain all pointers/commands.
    intro = len(data)
    data += struct.pack('>11I', 6, 100, 200, 300, 0, 0, 0, 0, 0, 0, 9)
    commands = len(data)
    for link, model in ((2, 144), (-2, 150)):
        record = bytearray(256)
        struct.pack_into('>4I', record, 0, 0x01000001, model << 16, 0x10000000, 0)
        struct.pack_into('>I', record, 0x74, 1000 << 16)
        struct.pack_into('>i5IHH3I', record, 0x80, link, 0xf333, 0xe666, 10, 10, 0x2c5, 4, 0, 1, 900, 3)
        data += record
        if link == 2:
            data += struct.pack('>4I', 22, (42 << 16) | 1, 0, 0)  # Tag points at door 2.
    data += struct.pack('>7I', 9, 7 << 16, 1, 0xffffffff, 0, 0xffff, 0)  # Character.
    data += struct.pack('>7I', 46, 100, 200, 300, 0, 0, 0)  # Outro camera.
    data += struct.pack('>I', 48)
    pads = len(data)
    data += struct.pack('>9fII', 10, 20, 30, 0, 1, 0, 0, 0, 1, 0, 0) + bytes(44)
    bounds = len(data)
    data += struct.pack('>9fII6f', 10, 20, 30, 1, 0, 0, 0, 1, 0, 0, 0, -6, 6, -50, 50, 0, 200) + bytes(68)
    name = len(data)
    data += b'p0\0'
    struct.pack_into('>I', data, pads + 36, name)
    struct.pack_into('>I', data, bounds + 36, name)
    struct.pack_into('>I', data, 8, intro)
    struct.pack_into('>I', data, 12, commands)
    struct.pack_into('>II', data, 24, pads, bounds)
    return data


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-door-placement-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        (work / 'setup/UsetupdoorsZ.set').write_bytes(fixture())
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{shim}', f'-I{src}', f'-I{root}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / name) for name in ('setupload.c', 'bghistory.c', 'modelload.c', 'rotation.c', 'scaling.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
