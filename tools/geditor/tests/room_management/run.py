#!/usr/bin/env python3
"""Room lifecycle, reference guards, native persistence and combined history.

Optional positional arguments are native BG files to round-trip as well.
"""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
spec = importlib.util.spec_from_file_location('extract', here.parent / 'vertex_eyedropper/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-room-management-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    data = bytearray(40) + struct.pack('>I', 48) + bytes(44 + 68)
    struct.pack_into('>I', data, 12, 40)
    struct.pack_into('>II', data, 24, 44, 88)
    (work / 'setup/UsetuptestZ.set').write_bytes(data)
    fixture = (here.parent / 'door_shadow/check.c').read_text()
    floor = (here.parent / 'room_mode/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, n) for n in ('Put', 'Fixture'))
        + ''.join(extract.function(floor, n) for n in ('Put16', 'Floor')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}',
        f'-I{root}', f'-I{root / "src"}', f'-I{work}', str(here / 'check.c'),
        str(here.parent / 'image_import/platform.c')]
    command += [str(src / name) for name in ('bgdocument.c', 'bgload.c', 'bgcompile.c',
        'bgportal.c', 'bgmaterial.c', 'bgrender.c', 'bgcommands.c', 'bgroom.c', 'bghistory.c',
        'rotation.c', 'scaling.c', 'setupload.c', 'actionblocks.c', 'stanload.c', 'stanedit.c',
        'stanquery.c', 'doorshadow.c', 'roomedit.c', 'roommanage.c')]
    command += [str(root / 'src/game/doorshadowmath.c'), '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work), *sys.argv[1:]], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
