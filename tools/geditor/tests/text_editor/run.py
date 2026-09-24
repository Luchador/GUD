#!/usr/bin/env python3
"""Test native text editing; optional --rom exercises an actual GUD ROM's banks."""
import argparse
import ctypes
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def corpus(root, work):
    sources = sorted((root / 'assets/obseg/text').glob('L*E.c'))
    code = '#include <stddef.h>\n'
    for p in sources:
        code += f'#include "{p}"\nsize_t count_{p.stem}(void) {{ return sizeof({p.stem})/sizeof(*{p.stem}); }}\n'
    (work / 'banks.c').write_text(code)
    subprocess.run(['cc', '-shared', '-fPIC', str(work / 'banks.c'), '-o', str(work / 'banks.so')], check=True)
    lib = ctypes.CDLL(str(work / 'banks.so'))
    result = []
    for p in sources:
        count = getattr(lib, f'count_{p.stem}')
        count.restype = ctypes.c_size_t
        n = count()
        array = (ctypes.c_void_p * n).in_dll(lib, p.stem)
        data = bytearray((n * 4 + 15) & ~15)
        for i, ptr in enumerate(array):
            if ptr:
                struct.pack_into('>I', data, i * 4, len(data))
                data += ctypes.string_at(ptr) + b'\0'
                data += b'\0' * (-len(data) % 4)
        data += b'\0' * (-len(data) % 16)
        target = work / (p.stem + '.bin')
        target.write_bytes(data)
        result.append(target)
    return result


def rom_banks(path, work):
    data = path.read_bytes()
    u = lambda at: struct.unpack_from('>I', data, at)[0]
    manifest = data.index(b'GUDGEDITORMANIF\0')
    entries = {data[a:a+4]: (u(a+4), u(a+8), u(a+12))
               for a in range(manifest+24, manifest+24+u(manifest+20)*16, 16)}
    start, end, base = entries[b'CMAP']
    files = []
    for i in range(1024):
        row = entries[b'FTBL'][0] + i*12
        ptr = u(row+4)
        if not ptr:
            break
        at = start + ptr - base
        assert start <= at < end
        name = data[at:data.index(0, at)].decode('ascii')
        files.append((name, u(row+8)))
    result = []
    for name, at in files:
        if not (name.startswith('L') and name.endswith('E')):
            continue
        limit = min([offset for _, offset in files if offset > at] + [entries[b'OBSG'][1]])
        target = work / (name + '.bin')
        target.write_bytes(data[at:limit])
        result.append(target)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'project_rebase'
    with tempfile.TemporaryDirectory(prefix='geditor-text-') as temp:
        work = Path(temp)
        banks = rom_banks(args.rom, work) if args.rom else corpus(root, work)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{shim}', f'-I{src}', str(here / 'check.c'), str(src / 'textbank.c'),
                   '-Wl,--gc-sections', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check')] + [str(p) for p in banks], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
