#!/usr/bin/env python3
"""Check SP folder identity, briefing strings, stage outfits and body loading.

Executes extracted production functions under ASan/UBSan. Optional --rom checks
actual imported character IDs and the project's four briefing strings.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location('mp_bond_test', HERE.parent/'mp_bonds/run.py')
mp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mp)


def check_rom_text(path):
    data = path.read_bytes()
    u = lambda at: struct.unpack_from('>I', data, at)[0]
    m = data.index(b'GUDGEDITORMANIF\0')
    entries = {data[a:a+4]: (u(a+4), u(a+8), u(a+12))
               for a in range(m+24, m+24+u(m+20)*16, 16)}
    start, end, base = entries[b'CMAP']
    for i in range(1024):
        p = entries[b'FTBL'][0]+i*12
        assert u(p+4), 'LtitleE missing'
        s = start+u(p+4)-base
        if data[s:data.index(0, s)] == b'LtitleE':
            bank = u(p+8)
            assert u(bank) >= 296*4
            for slot, actor in enumerate(('Brosnan','Connery','Dalton','Moore'), 292):
                s = bank+u(bank+slot*4)
                assert data[s:data.index(0,s)].decode('ascii').rstrip('\n') == f': 007 ({actor})'
            return
    raise AssertionError('LtitleE missing')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path)
    args = parser.parse_args()
    models = mp.records(args.rom)
    if args.rom:
        check_rom_text(args.rom)
    front = (ROOT/'src/game/front.c').read_text()
    bondview = (ROOT/'src/game/bondview.c').read_text()
    constants = (ROOT/'src/bondconstants.h').read_text()
    code = '''#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef int32_t s32; typedef uint32_t u32; typedef uint8_t u8;
typedef float f32; typedef int bool;
#define TRUE 1
#define FALSE 0
#define ARRAYCOUNT(a) (sizeof(a)/sizeof((a)[0]))
#define MAX_FOLDER_COUNT 4
#include "src/custompropformat.h"
#include "src/levelids.h"
#include "assets/obseg/text/LtitleE.h"
#include "assets/obseg/text/LtitleE.c"
'''
    for name in ('BODIES','HEADS','BOND','CUFF_TYPES'):
        code += mp.declaration(constants,r'typedef enum '+name+r'\s*\{.*?\}\s*'+name+';')
    code += re.search(r'typedef enum \w+\s*\{[^{}]*\bLTITLE\b[^{}]*\}\s*\w+;',constants,re.S)[0]+'\n'
    code += re.search(r'^#define getStringID.*$',constants,re.M)[0]+'\n'
    code += mp.declaration(front,r'static const struct \{ const char \*body, \*head; \} g_BondModels\[\] = \{.*?\n\};')
    code += 'typedef struct {char name[64]; u32 kind, characterId;} CustomPropRuntime;\n'
    code += 'static CustomPropRuntime fixtures[] = {\n'
    code += ''.join('    {%s, %d, %d},\n' % (json.dumps(n),k,i) for n,k,i in models)
    code += '};\nstatic CustomPropRuntime *g_CustomProps=fixtures;\n'
    code += 'static s32 g_CustomPropCount=ARRAYCOUNT(fixtures);\n'
    code += mp.function((ROOT/'src/game/pobjdata.c').read_text(),'customCharacterFind')
    code += mp.function((ROOT/'src/game/file2.c').read_text(),'fileGetBondForFolder')
    code += mp.function(front,'frontGetWalletBondForFolder')
    code += mp.function(front,'frontGetSoloAgentTextId')
    code += mp.function(front,'frontGetSoloCharacterModels')
    # Confirm the actual menu and player loader consume the selected folder.
    briefing = mp.function(front,'print_current_solo_briefing_stage_name')
    assert 'langGet(frontGetSoloAgentTextId(fileGetBondForCurrentFolder()))' in briefing
    model_h = (ROOT/'src/game/model.h').read_text()
    code += re.search(r'^#define ANIM_MODEL_ALLOCATION_SIZE.*$',model_h,re.M)[0]+'\n'
    code += (HERE/'harness.h').read_text()
    code += mp.function(bondview,'bviewLoadPlayerChr')
    code += mp.function(bondview,'bondviewRemovePlayerBody')
    code += (HERE/'check.c').read_text()
    with tempfile.TemporaryDirectory(prefix='gud-sp-bonds-') as directory:
        work = Path(directory)
        (work/'check.c').write_text(code)
        command = shlex.split(os.environ.get('CC','cc'))
        command += ['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-sign-compare','-Wno-unused-parameter','-Wno-unused-variable',
                    '-Wno-unused-but-set-variable','-Wno-pointer-to-int-cast',
                    '-Wno-int-to-pointer-cast','-fsanitize=address,undefined',
                    '-I',str(ROOT),str(work/'check.c'),'-o',str(work/'check')]
        subprocess.run(command,check=True)
        subprocess.run([str(work/'check')],check=True,env=dict(os.environ,
            ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
