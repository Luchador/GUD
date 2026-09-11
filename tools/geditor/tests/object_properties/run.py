#!/usr/bin/env python3
"""Exercise native object edits, persistence and undo/redo under sanitizers."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^(?:static )?\w+ ' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    assert match, name
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    # Every parsed ObjectRecord subtype, with recognizable specialized data.
    sizes = {1:64, 3:32, 4:33, 5:32, 6:59, 7:33, 8:34, 10:64, 11:149,
             12:32, 13:54, 17:32, 20:44, 21:34, 36:32, 39:44, 40:45,
             42:32, 43:32, 45:56, 47:37}
    with tempfile.TemporaryDirectory(prefix='geditor-object-properties-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        data = bytearray(40)
        for kind, words in sizes.items():
            record = bytearray((i * 13 + kind) % 256 for i in range(words * 4))
            struct.pack_into('>I', record, 0, 0x01002000 | kind)
            struct.pack_into('>HHII', record, 4, 15, 0, 0x52514C3B, 0x83001400)
            struct.pack_into('>II', record, 0x70, 0xABCDEF01, 0x03E84000)
            data += record
        # A separate GuardRecord must never be interpreted as ObjectRecord.
        data += struct.pack('>7I', 9, 2 << 16, 1, 0xFFFFFFFF, 0, 0xFFFF, 0)
        data += struct.pack('>I', 48)
        pads = len(data)
        data += struct.pack('>9fII', 10, 20, 30, 0, 1, 0, 0, 0, 1, pads + 156, 0)
        data += bytes(44 + 68) + b'p0\0'
        struct.pack_into('>I', data, 12, 40)
        struct.pack_into('>II', data, 24, pads, pads + 88)
        (work / 'setup/UsetuppropertiesZ.set').write_bytes(data)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{shim}', f'-I{src}', f'-I{root}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / name) for name in ('setupload.c', 'bghistory.c', 'modelload.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        # Exercise actual input logic; native controls and scene callbacks are stubbed.
        source = (src / 'objectproperties.c').read_text()
        types = re.search(r'#define OBJECT_DOOR_FIELD_COUNT .*', source)[0] + '\n'
        types += re.search(r'enum \{ OBJECT_TYPE,.*?\};', source, re.S)[0]
        types += '\n' + re.search(r'typedef struct ObjectPropertiesState \{.*?\} ObjectPropertiesState;', source, re.S)[0]
        types += '\n' + re.search(r'#define OBJECT_CONTENTS_TEXT_MAX .*', source)[0]
        types += '\n' + re.search(r'static const char \*g_AmmoNames\[AMMOTYPE_MAX\] = \{.*?\};', source, re.S)[0]
        types += '\n' + re.search(r'static const struct \{[^}]*\} g_DoorFields\[OBJECT_DOOR_FIELD_COUNT\] = \{.*?\n\};', source, re.S)[0]
        (work / 'input-types.inc').write_text(types)
        names = ('ObjectPropertiesDoorField', 'ObjectPropertiesDoorValue', 'ObjectPropertiesDoorUnits',
                 'ObjectPropertiesDoorFactor', 'ObjectPropertiesResetDoor', 'ObjectPropertiesFormatContents', 'ObjectPropertiesIsEdit', 'ObjectPropertiesControlVisible',
                 'ObjectPropertiesResetHealth', 'ObjectPropertiesParseHealth',
                 'ObjectPropertiesApply', 'ObjectPropertiesParseDoor', 'ObjectPropertiesApplyDoor',
                 'ObjectPropertiesKeyProperty', 'ObjectPropertiesApplyHealth',
                 'ObjectPropertiesParseUnsigned', 'ObjectPropertiesResetExtra',
                 'ObjectPropertiesApplyExtra', 'ObjectPropertiesHandleMessage')
        (work / 'input-logic.inc').write_text(''.join(function(source, name) for name in names))
        subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{shim}', f'-I{src}', f'-I{root}', f'-I{work}', str(here / 'input.c'),
                        '-lm', '-o', str(work / 'input')], check=True)
        subprocess.run([str(work / 'input')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
