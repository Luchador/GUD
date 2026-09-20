#!/usr/bin/env python3
"""Room counts from live documents; optional real Depot regression input."""
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
with tempfile.TemporaryDirectory(prefix='geditor-rooms-') as directory:
    work = Path(directory)
    ui = (src / 'levelmanager.c').read_text()
    match = re.search(r'^static int CALLBACK LevelManagerCompareRooms\([^;{}]*\)\s*\{', ui, re.M)
    start = ui.index('{', match.start()); depth, end = 1, start + 1
    while depth:
        depth += (ui[end] == '{') - (ui[end] == '}'); end += 1
    (work / 'sort.inc').write_text(ui[match.start():end])
    command = [os.environ.get('CC','cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        f'-I{work}', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{src.parents[2]}',
        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
    command += [str(src / n) for n in ('roomstats.c','setupload.c','actionblocks.c','stanquery.c',
        'stanload.c','bgdocument.c','bgload.c','bgmaterial.c','bgrender.c')]
    command += ['-Wl,--gc-sections','-Wl,--wrap=malloc','-Wl,--wrap=calloc','-lm','-o',str(work/'check')]
    subprocess.run(command,check=True)
    subprocess.run([str(work/'check')] + sys.argv[1:],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
