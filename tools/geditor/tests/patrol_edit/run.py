#!/usr/bin/env python3
"""Native patrol authoring, guard AI, persistence/history and failure atomicity."""
import os
import importlib.util
import sys
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
shim = here.parent / 'image_import'
with tempfile.TemporaryDirectory(prefix='geditor-patrol-edit-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    names = []
    if '--stock' in sys.argv:
        sys.dont_write_bytecode = True
        spec = importlib.util.spec_from_file_location('actions', here.parent / 'action_blocks/run.py')
        actions = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(actions)
        # Guard integration uses solo setups. Several legacy MP source files
        # still contain old door record sizes unrelated to patrol authoring.
        names = [name for name in actions.corpus(src.parents[2], work, here.parent / 'action_blocks')
                 if not name.startswith('Ump_')]
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
               '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}',
               str(here / 'check.c'), str(shim / 'platform.c')]
    command += [str(src / name) for name in ('patrolpaths.c', 'setupload.c', 'actionblocks.c', 'bghistory.c')]
    command += ['-Wl,--gc-sections', '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-Wl,--wrap=realloc',
                '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work)] + names, check=True,
                   env=dict(os.environ, ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS', 'detect_leaks=0'), UBSAN_OPTIONS='halt_on_error=1'))
