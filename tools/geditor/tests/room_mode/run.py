#!/usr/bin/env python3
"""Native room translation, persistence, shared pads, and combined edit history."""
import importlib.util
import os
import re
from pathlib import Path
import struct
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
spec = importlib.util.spec_from_file_location('extract', here.parent / 'vertex_eyedropper/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-room-mode-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    (work / 'stan').mkdir()
    # A valid empty native setup, then production APIs create placements.
    data = bytearray(40) + struct.pack('>I', 48) + bytes(44 + 68)
    struct.pack_into('>I', data, 12, 40)
    struct.pack_into('>II', data, 24, 44, 88)
    (work / 'setup/UsetuptestZ.set').write_bytes(data)
    fixture = (here.parent / 'door_shadow/check.c').read_text()
    (work / 'fixture.inc').write_text(extract.function(fixture, 'Put') + extract.function(fixture, 'Fixture'))
    viewport = (src / 'viewport.c').read_text()
    (work / 'viewtypes.inc').write_text('\n'.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0]
        for name in ('Vertex', 'ViewportPad', 'ViewportPickRay', 'ViewportRoomDragPoint')))
    (work / 'viewlogic.inc').write_text(''.join(extract.function(viewport, name) for name in (
        'ViewportGetSelectedRoom', 'ViewportSelectWholeRoom', 'ViewportRoomPosition', 'ViewportPickRoomAt',
        'ViewportRoomDragPointAdd', 'ViewportBeginRoomDrag', 'ViewportPreviewRoomDrag', 'ViewportCancelRoomDrag')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}',
        f'-I{root}', f'-I{root / "src"}', f'-I{work}', str(here / 'check.c'),
        str(here.parent / 'image_import/platform.c')]
    command += [str(src / name) for name in ('bgdocument.c', 'bgload.c', 'bgcompile.c',
        'bgportal.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c', 'setupload.c',
        'actionblocks.c', 'stanload.c', 'stanedit.c', 'stanquery.c', 'doorshadow.c', 'roomedit.c')]
    command += [str(root / 'src/game/doorshadowmath.c'), '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
