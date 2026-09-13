#!/usr/bin/env python3
"""Check inherited BG transparency and inspector text using the real decoder."""
import importlib.util
import os
import re
import struct
import zlib
from pathlib import Path
import subprocess
import sys
import tempfile


def jungle_fixture(source):
    """Use the repository's real compressed room streams, without a ROM."""
    arrays = dict(re.findall(r'u32 ((?:point_table|pri_mapping|sec_mapping)_binary_\d+)\[\] = \{(.*?)\};', source, re.S))
    rooms = max(int(name.rsplit('_', 1)[1]) for name in arrays)
    data = bytearray(32 + (rooms + 2) * 24)
    struct.pack_into('>I', data, 4, 0x0e000020)
    for room in range(1, rooms + 1):
        for field, prefix in enumerate(('point_table', 'pri_mapping', 'sec_mapping')):
            body = arrays.get(f'{prefix}_binary_{room}')
            if body is None:
                continue
            stream = b''.join(struct.pack('>I', int(word, 16)) for word in re.findall(r'0x([0-9a-fA-F]+)', body))
            assert stream[:2] == b'\x11\x72' or stream == b'\0' * 4
            raw = zlib.decompress(stream[2:], -15) if stream[:2] == b'\x11\x72' else b''
            data.extend(b'\0' * (-len(data) % 16))
            data.extend(struct.pack('>I', len(raw)))
            struct.pack_into('>I', data, 32 + room * 24 + field * 4, 0x0e000000 | len(data))
            data.extend(raw)
    return data


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_selection/run.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    source = (src / 'faceproperties.c').read_text()
    with tempfile.TemporaryDirectory(prefix='geditor-bg-transparency-') as temp:
        work = Path(temp)
        (work / 'inspector.inc').write_text(''.join(helpers.function(source, name) for name in
            ('FacePropertiesRenderText',)))
        subprocess.run([os.environ.get('CC', 'cc'), '-O2', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}',
                        f'-I{src}', f'-I{work}', str(here / 'check.c'),
                        str(src / 'bgdocument.c'), str(src / 'bgrender.c'),
                        '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

        subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}',
                        str(here / 'edit.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / name) for name in ('bgload.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'edit')], check=True)
        (work / 'jungle.seg').write_bytes(jungle_fixture((here.parents[3] / 'assets/obseg/bg/u/bg_jun_all_p.c').read_text()))
        subprocess.run([str(work / 'edit'), str(work), str(work / 'jungle.seg')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
