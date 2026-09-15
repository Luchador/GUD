#!/usr/bin/env python3
"""Check production destroyed-prop dispatch without the N64 graphics backend."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


props = (ROOT / 'src/game/propobj.c').read_text()
source = (HERE / 'harness.h').read_text()
for name in ('objGetDestroyedLevel', 'objHideMonitorScreens', 'objRenderPropModel'):
    source += function(props, name)
source += (HERE / 'check.c').read_text()

# Hiding happens once on destruction, including props that have never rendered.
explode = function(props, 'objExplode')
assert explode.index('state |= PROPSTATE_DESTROYED') < explode.index('objHideMonitorScreens(obj);') < explode.index('objDeform(obj, 1);')
assert explode.count('objHideMonitorScreens(obj);') == 1

with tempfile.TemporaryDirectory(prefix='gud-destroyed-props-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter']
    command += shlex.split(os.environ.get('TEST_CFLAGS', ''))
    subprocess.run(command + [str(work / 'check.c'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True)
