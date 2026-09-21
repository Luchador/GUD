#!/usr/bin/env python3
"""Exercise production gadget selection, weapon requests, and input decoding."""
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


constants = (ROOT / 'src/bondconstants.h').read_text()
types = (ROOT / 'src/bondtypes.h').read_text()
gun = (ROOT / 'src/game/gun.c').read_text()
view = (ROOT / 'src/game/bondview.c').read_text()
declarations = ''
for name in ('ITEM_IDS', 'GUNHAND', 'GUN_ANIMATION_STATE_IDS', 'CONTROLLER_CONFIG', 'BONDSTATE',
             'WATCH_ANIMATION_STATE_IDS'):
    declarations += re.search(r'typedef enum ' + name + r'\s*\{.*?\}\s*\w+;', constants, re.S)[0] + '\n'
declarations += re.search(r'struct MoveData \{.*?\n\};', types, re.S)[0] + '\n'

with tempfile.TemporaryDirectory(prefix='gud-gadget-cycle-') as directory:
    work = Path(directory)
    (work / 'declarations.inc').write_text(declarations)
    production = function((ROOT / 'src/game/bondinv.c').read_text(), 'bondinvGetNextGadget')
    production += '\n'.join(function(gun, name) for name in (
        'getCurrentPlayerWeaponId', 'get_next_weapon_in_cycle_for_hand',
        'gunRequestHandWeaponChange', 'gunCycleGadget'))
    production += function(view, 'bviewConsumeGadgetCycleInput')
    # Run the input decoder verbatim, returning its output before movement/rendering.
    decoder = function(view, 'bviewProcessInput')
    decoder = decoder[:decoder.index('    g_CurrentPlayer->field_D0 =')]
    decoder = decoder.replace('void bviewProcessInput(', 'static struct MoveData decodeInput(', 1)
    production += decoder + '    return moveData;\n}\n'
    (work / 'production.inc').write_text(production)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-Wno-unused-variable',
        '-fsanitize=address,undefined', '-I', str(work),
        str(HERE / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
