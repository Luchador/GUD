#!/usr/bin/env python3
"""Exercise native monitor scripts/models, placement and corrupt-ROM bounds.
No ROM, Windows or emulator is required. Fixture scripts are compiled from the
actual game arrays/macros, with jump relocations assigned synthetic addresses.
"""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    game = (root / 'src/game/propobj.c').read_text()
    arrays = re.findall(r'u32 (mon\w+)\[\] = \{(.*?)\};', game, re.S)
    table = re.search(r'g_MonitorAnimationScripts\[.*?\] = \{(.*?)\};', game, re.S)[1]
    names = re.findall(r'&(\w+)', table)
    assert len(names) == 52
    macros = '\n'.join(l for l in (root / 'src/game/chrai.h').read_text().splitlines()
                       if l.startswith('#define MON') or l.startswith('#define COLOR_') or '_PERCENT_CHANCE ' in l)
    enum = re.search(r'enum MONIMGIDS \{.*?\};', (root / 'assets/oddtextures.h').read_text(), re.S)[0]
    h = '#include <stdint.h>\n' + macros + '\n' + enum + '\n'
    for i, (name, body) in enumerate(arrays):
        h += f'#define {name} (0x80000000u + {i} * 4096u)\n'
    for name, body in arrays:
        h += f'static const uint32_t words_{name}[] = {{{body}}};\n'
    h += 'static void Populate(unsigned char *code, unsigned char *table) {\n'
    for i, (name, _) in enumerate(arrays):
        h += f'for (unsigned i=0; i<sizeof(words_{name})/4; i++) Put32(code+{i}*4096+i*4, words_{name}[i]);\n'
    for i, name in enumerate(names):
        h += f'Put32(table+{i}*4,{name});\n'
    h += '}\n'
    # Encode the game's real initial MonitorRecord, preserving source values.
    initial = re.search(r'g_InitialMonitorAnimController = \{(.*?)\};',
                        (root / 'src/game/initobjects.c').read_text(), re.S)[1].split(',')
    initial = [word.strip() for word in initial]
    native = struct.pack('>IHHI', 0, int(initial[1], 0), int(initial[2], 0), int(initial[3], 0))
    native += struct.pack('>21f', *(float(v) for v in initial[4:25]))
    native += bytes(int(v, 0) for v in initial[25:37])
    native += struct.pack('>2f', *(float(v) for v in initial[37:39]))
    assert len(native) == 116
    h += 'static const unsigned char initial[] = {' + ','.join(map(str, native)) + '};\n'
    h += f'#define CODE_SIZE {len(arrays) * 4096}\n'
    with tempfile.TemporaryDirectory(prefix='geditor-monitors-') as temp:
        temp = Path(temp)
        (temp / 'scripts.h').write_text(h)
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}', f'-I{temp}',
                   str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / n) for n in ('monitoranim.c', 'monitorload.c', 'modelload.c',
                    'bgmaterial.c', 'bgrender.c', 'setupload.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(temp / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(temp / 'check'), str(root)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
