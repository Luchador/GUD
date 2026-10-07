#!/usr/bin/env python3
"""Explicit pad room detection, native persistence and undo. Optional: Frigate audit directory."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
with tempfile.TemporaryDirectory(prefix='geditor-pad-room-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    b = bytearray(512)
    struct.pack_into('>II', b, 24, 48, 224)
    for at in (48, 92, 224):
        struct.pack_into('>9fII', b, at, 10, 50, 10, 0, 1, 0, 0, 0, 1, 400, 0)
    struct.pack_into('>6f', b, 224 + 44, -10, 10, -20, 20, -30, 30)
    b[400:404] = b'p2a\0'
    (work / 'setup/UsetuppadsZ.set').write_bytes(b)
    args = []
    if len(sys.argv) > 1:
        audit = Path(sys.argv[1])
        shutil.copyfile(audit / 'UsetupdestZ', work / 'setup/UsetupdestZ.set')
        shutil.copyfile(audit / 'Tbg_dest_all_p_stanZ', work / 'frigate.stan')
        args = ['frigate']
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{src.parents[2]}', str(here / 'check.c'),
        str(here.parent / 'image_import/platform.c')]
    command += [str(src / n) for n in ('setupstan.c', 'setupload.c', 'actionblocks.c',
        'stanload.c', 'stanedit.c', 'stanquery.c', 'bghistory.c')]
    subprocess.run(command + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)] + args, check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
