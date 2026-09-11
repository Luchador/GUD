#!/usr/bin/env python3
"""Check complete flag coverage and native setup persistence under ASan/UBSan."""
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
    definitions = (root / 'src/propconstants.h').read_text()
    expected = dict(re.findall(r'^\s*(PROPFLAG2?_\w+)\s*=\s*(0x[0-9a-fA-F]+)', definitions, re.M))
    catalog = re.findall(r'^OBJECT_FLAG(?:_FOR_TYPE|_GAME_STATE)?\((\d), (PROPFLAG2?_\w+),', (src / 'objectflagdefs.h').read_text(), re.M)
    assert len(catalog) == len(expected) and {name for _, name in catalog} == set(expected)
    for bank, name in catalog:
        assert int(bank) == int(name.startswith('PROPFLAG2_'))
        value = int(expected[name], 16)
        assert 0 < value <= 0x80000000 and value & (value - 1) == 0
    for bank in (0, 1):
        assert {int(expected[name], 16) for b, name in catalog if int(b) == bank} == {1 << bit for bit in range(32)}
    print(f'PASS: all {len(catalog)} named flags and aliases cover both complete 32-bit words.', flush=True)
    with tempfile.TemporaryDirectory(prefix='geditor-object-flags-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        # An object, a guard, and a door, followed by a real pad and both table
        # terminators. Patterned payloads catch unrelated data being rewritten.
        data = bytearray(40)
        for size, kind in ((128, 3), (28, 9), (256, 1)):
            record = bytearray((i * 13 + 7) % 256 for i in range(size))
            struct.pack_into('>I', record, 0, 0x01002000 | kind)
            if kind != 9:
                struct.pack_into('>HHII', record, 4, 15, 0, 0x52514C3B, 0x8310140D)
            data += record
        data += struct.pack('>I', 48)
        pads = len(data)
        data += struct.pack('>9fII', 10, 20, 30, 0, 1, 0, 0, 0, 1, pads + 156, 0)
        data += bytes(44 + 68) + b'p0\0'
        struct.pack_into('>I', data, 12, 40)
        struct.pack_into('>II', data, 24, pads, pads + 88)
        (work / 'setup/UsetupflagsZ.set').write_bytes(data)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{shim}', f'-I{src}', f'-I{root}', str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / name) for name in ('setupload.c', 'bghistory.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
