#!/usr/bin/env python3
"""Test sample decoding/rendering and waveOut lifetime. Optional argument: a GUD ROM."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
with tempfile.TemporaryDirectory(prefix='geditor-sfx-playback-') as directory:
    for test in ('check', 'player'):
        binary = str(Path(directory) / test)
        sources = [here / (test + '.c'), src / 'sfxpreview.c', src / 'sfxcatalog.c']
        if test == 'player':
            sources.append(src / 'sfxplayer.c')
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-fsanitize=address,undefined', '-I' + str(here),
                        '-I' + str(src)] + list(map(str, sources)) + ['-lm', '-o', binary], check=True)
        subprocess.run([binary] + (sys.argv[1:] if test == 'check' else []), check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
