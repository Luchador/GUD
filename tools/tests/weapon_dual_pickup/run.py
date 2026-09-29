#!/usr/bin/env python3
"""Exercise production inventory and pickup eligibility, plus Boris's AI bytes."""
import os
from pathlib import Path
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


constants = (ROOT / 'src/bondconstants.h').read_text()
inventory = (ROOT / 'src/game/bondinv.c').read_text()
objects = (ROOT / 'src/game/propobj.c').read_text()
setup = (ROOT / 'assets/obseg/setup/UsetupcontrolZ.c').read_text()
declarations = ''
for name in ('ITEM_IDS', 'PROP_TYPE', 'INV_ITEM_TYPE', 'TICKOP'):
    declarations += re.search(r'typedef enum ' + name + r'\s*\{.*?\}\s*\w+;', constants, re.S)[0] + '\n'
declarations += re.search(r'^#define WEAPONSTATBITFLAG_CAN_DUAL_WIELD[^\n]*', constants, re.M)[0] + '\n'
declarations += re.search(r'u8 ai_16\[\] = \{.*?\n\};', setup, re.S)[0] + '\n'
production = ''.join(function(inventory, name) for name in (
    'bondinvSortInv', 'bondinvInsertItem', 'bondinvGetNextAvailItem',
    'bondinvGetInvItem', 'bondinvHasInvItem', 'bondinvGetDualWeapon',
    'bondinvHasDualWeapon', 'bondinvAddInvItem', 'bondinvAddDoublesInvItem',
    'bondinvWeaponGrantsDual', 'bondinvAddWeaponByProp'))
production += function(objects, 'propweaponSetDual')
production += function(objects, 'objTickPlayer')

with tempfile.TemporaryDirectory(prefix='gud-dual-pickup-') as directory:
    work = Path(directory)
    (work / 'ultra64.h').write_text('/* Host fixture supplies scalar types. */\n')
    (work / 'bondconstants.h').write_text('/* Required enums are extracted in declarations.inc. */\n')
    (work / 'declarations.inc').write_text(declarations)
    (work / 'production.inc').write_text(production)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-Wno-sign-compare', '-fno-strict-aliasing',
        '-fsanitize=address,undefined', '-I', str(work), '-I', str(ROOT / 'src'),
        str(HERE / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
