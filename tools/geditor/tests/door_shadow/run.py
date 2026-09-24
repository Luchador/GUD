#!/usr/bin/env python3
"""Door Shadow conversion, clipping, linked-door persistence and combined undo.

Optional arguments: a native BG .seg and a setup .set (read-only).
"""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / 'src'
ROOT = SRC.parents[2]
with tempfile.TemporaryDirectory(prefix='geditor-door-shadow-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    # Include a non-object command before the door: object and command indices differ.
    data = bytearray(40) + struct.pack('>II', 2, 0) + bytearray(256) + struct.pack('>I', 48)
    struct.pack_into('>I', data, 12, 40)
    struct.pack_into('>III', data, 48, 0x1002001, 0xffffffff, 0)
    struct.pack_into('>II', data, 24, len(data), len(data) + 44)
    data += bytes(44 + 68)
    (work / 'setup/UsetupshadowZ.set').write_bytes(data)
    if len(sys.argv) > 2:
        shutil.copyfile(sys.argv[2], work / 'setup/UsetupdepotZ.set')
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{HERE.parent / "image_import"}', f'-I{SRC}',
        f'-I{ROOT}', f'-I{ROOT / "src"}', str(HERE / 'check.c'),
        str(HERE.parent / 'image_import/platform.c')]
    command += [str(SRC / n) for n in ('bgdocument.c', 'bgload.c', 'bgcompile.c',
        'bgmaterial.c', 'bgrender.c', 'bgroom.c', 'bghistory.c', 'setupload.c',
        'actionblocks.c', 'stanload.c', 'doorshadow.c')]
    command += [str(ROOT / 'src/game/doorshadowmath.c'), '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work)] + sys.argv[1:2], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
    subprocess.run([sys.executable, str(HERE / 'runtime.py'), str(work / 'shadow.bin')], check=True)
