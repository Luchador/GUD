#!/usr/bin/env python3
"""Run real monitor scripts and prop/model render dispatch with instrumented graphics."""
from pathlib import Path
import os
import re
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
model = (ROOT / 'src/game/model.c').read_text()
constants = (ROOT / 'src/bondconstants.h').read_text()
source = ''
for name in ('MODELNODE_OPCODE', 'TVCMD', 'PROP'):
    source += re.search(r'typedef enum ' + name + r'\s*\{.*?\}\s*\w+;', constants, re.S)[0] + '\n'
source += '\n'.join(re.findall(r'^#define MODEL_RENDER_.*', (ROOT / 'src/game/model.h').read_text(), re.M)) + '\n'
source += (HERE / 'harness.h').read_text().replace('/* MONITOR_RECORD */',
    re.search(r'typedef struct MonitorRecord\s*\{.*?\}\s*MonitorRecord;',
              (ROOT / 'src/bondtypes.h').read_text(), re.S)[0])
source += re.search(r'struct tvcmd \{.*?\};', props, re.S)[0] + '\n'
for name in ('monitorSetCommandList', 'monitorSetTexture', 'monitorProcessAndRender',
             'objGetDestroyedLevel', 'objHideMonitorScreens'):
    source += function(props, name)
for name in ('sub_GAME_7F074534', 'subdraw'):
    source += function(model, name)
source += function(props, 'objRenderPropModel')
source += (HERE / 'check.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-monitor-occlusion-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    # Low static addresses let the original 32-bit TV scripts contain pointers.
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-no-pie',
               '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-unused-variable',
               '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast', '-Wno-int-conversion',
               '-ffp-contract=off', '-fno-strict-aliasing', '-fsanitize=address,undefined']
    subprocess.run(command + [str(work / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
