#!/usr/bin/env python3
"""Exercise the production credits renderer's input and completion behavior."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


source = (ROOT / 'src/game/bondview.c').read_text()
constants = (ROOT / 'src/bondconstants.h').read_text()
types = (ROOT / 'src/bondtypes.h').read_text()
declarations = re.search(r'^#define CREDITS_SKIP_HOLD_FRAMES .*$', source, re.M)[0] + '\n'
for name in ('CREDITS_ALIGNMENT', 'CREDITS_STATE'):
    declarations += re.search(r'typedef enum ' + name + r'\s*\{.*?\}\s*' + name + ';', constants, re.S)[0] + '\n'
declarations += re.search(r'typedef struct CreditsEntry_s\s*\{.*?\}\s*CreditsEntry;', types, re.S)[0] + '\n'

with tempfile.TemporaryDirectory(prefix='gud-credits-skip-') as directory:
    work = Path(directory)
    (work / 'declarations.inc').write_text(declarations)
    (work / 'credits.inc').write_text(function(source, 'bondviewRenderCredits'))
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-fsanitize=address,undefined',
        '-I', str(work), str(HERE / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                            UBSAN_OPTIONS='halt_on_error=1'))
