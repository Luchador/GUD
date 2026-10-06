#!/usr/bin/env python3
"""Exercise the production MP roster, unlock limits and imported Bond selection.

Optional --rom verifies the exported actor names and uses its actual model IDs.
No ROM assets are stored in the test fixtures.
"""
import argparse
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


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def declaration(source, pattern):
    match = re.search(pattern, source, re.S)
    assert match, pattern
    return match[0] + '\n'


def records(path):
    if not path:
        return [('CheadmooreZ', 2, 80), ('CheadconneryZ', 2, 81),
                ('CheaddaltonZ', 2, 82), ('CdaltonZ', 1, 83),
                ('CconneryZ', 1, 84), ('CmooreZ', 1, 85)]
    data = path.read_bytes()
    u = lambda at: struct.unpack_from('>I', data, at)[0]
    m = data.index(b'GUDGEDITORMANIF\0')
    entries = {data[a:a+4]: (u(a+4), u(a+8), u(a+12))
               for a in range(m+24, m+24+u(m+20)*16, 16)}
    at = entries[b'NPMD'][0]
    assert u(at) == 0x474e5031 and u(at+8) == 96
    result = []
    for i in range(u(at+4)):
        p = at+16+i*96
        name = data[p:p+64].split(b'\0')[0].decode('ascii')
        result.append((name, u(p+84), u(p+92)))
    start, end, base = entries[b'CMAP']
    for i in range(1024):
        p = entries[b'FTBL'][0]+i*12
        if not u(p+4):
            raise AssertionError('LtitleE is missing')
        s = start+u(p+4)-base
        name = data[s:data.index(0, s)]
        if name == b'LtitleE':
            bank = u(p+8)
            for slot, actor in enumerate(('Brosnan', 'Connery', 'Dalton', 'Moore'), 0x120):
                s = bank+u(bank+slot*4)
                assert data[s:data.index(0, s)].decode('ascii') == actor
            break
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path)
    args = parser.parse_args()
    models = records(args.rom)
    lookup = {name: (kind, index) for name, kind, index in models}
    actors = ('connery', 'dalton', 'moore')
    bodyids, headids = [], []
    for actor in actors:
        kind, index = lookup[f'C{actor}Z']; assert kind == 1; bodyids.append(index)
        kind, index = lookup[f'Chead{actor}Z']; assert kind == 2; headids.append(index)

    front = (ROOT / 'src/game/front.c').read_text()
    constants = (ROOT / 'src/bondconstants.h').read_text()
    code = '''#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
typedef int32_t s32; typedef uint32_t u32; typedef int16_t s16;
typedef uint16_t u16; typedef uint8_t u8; typedef float f32; typedef int bool;
#define TRUE 1
#define FALSE 0
#define ARRAYCOUNT(a) (sizeof(a)/sizeof((a)[0]))
#define MAX_PLAYER_COUNT 4
#define MAX_TEXTURES 4096
#include "src/custompropformat.h"
#include "assets/obseg/text/LtitleE.h"
#include "assets/obseg/text/LtitleE.c"
'''
    for name in ('BODIES', 'HEADS', 'GENDER', 'TEXTBANKS'):
        if name == 'TEXTBANKS':
            # Read the actual language-bank enum rather than duplicating LTITLE.
            match = re.search(r'typedef enum (\w+)\s*\{[^{}]*\bLTITLE\b[^{}]*\}\s*\w+;', constants, re.S)
            assert match
            code += match[0] + '\n'
        else:
            code += declaration(constants, r'typedef enum ' + name + r'\s*\{.*?\}\s*' + name + ';')
    code += re.search(r'^#define getStringID.*$', constants, re.M)[0] + '\n'
    code += declaration((ROOT/'assets/oddtextures.h').read_text(), r'enum MPCHRSELIMAGES\s*\{.*?\};')
    code += declaration((ROOT/'src/game/front.h').read_text(), r'struct MP_selectable_chars\s*\{.*?\};')
    code += declaration(front, r'enum \{ MP_CHARS_DEFAULT_COUNT.*?;')
    code += declaration(front, r's32 num_chars_selectable_mp = .*?;')
    code += declaration(front, r'static const struct \{ const char \*body, \*head; \} g_MpBondModels\[\] = \{.*?\n\};')
    code += declaration(front, r'struct MP_selectable_chars mp_chr_setup\[\] = \{.*?\n\};')
    # Only fields touched by customCharacterFind; registration itself is unchanged.
    code += 'typedef struct {char name[64]; u32 kind, characterId;} CustomPropRuntime;\n'
    code += 'static CustomPropRuntime fixtures[] = {\n'
    code += ''.join('    {%s, %d, %d},\n' % (json.dumps(name), kind, index) for name, kind, index in models)
    code += '};\nstatic CustomPropRuntime *g_CustomProps;\nstatic s32 g_CustomPropCount;\n'
    code += 'static const int bodyids[]={%s}, headids[]={%s};\n' % (','.join(map(str,bodyids)), ','.join(map(str,headids)))
    code += (HERE/'harness.h').read_text()
    code += function((ROOT/'src/game/pobjdata.c').read_text(), 'customCharacterFind')
    for name in ('frontGetMpCharacterIndex', 'frontGetMpCharacterModel', 'get_player_mp_char_head',
                 'get_player_mp_char_body', 'get_player_mp_char_gender', 'get_player_mp_char_height',
                 'unlock_all_mp_chars', 'get_players_who_have_selected_mp_char', 'init_menu0f_mpcharsel'):
        code += function(front, name)
    code += (HERE/'check.c').read_text()
    with tempfile.TemporaryDirectory(prefix='gud-mp-bonds-') as directory:
        work = Path(directory)
        (work/'check.c').write_text(code)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += ['-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-sign-compare', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    '-I', str(ROOT), str(work/'check.c'), '-o', str(work/'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work/'check')], check=True, env=dict(os.environ,
            ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
