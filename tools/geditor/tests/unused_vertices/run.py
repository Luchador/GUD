#!/usr/bin/env python3
"""Unused vertex cleanup, Depot bounds and native display-list preservation."""
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


def main():
    sys.dont_write_bytecode = True
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    spec = importlib.util.spec_from_file_location('native', here.parent / 'bg_transparency/run.py')
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    with tempfile.TemporaryDirectory(prefix='geditor-unused-vertices-') as temp:
        work = Path(temp)
        files = []
        for path in sorted((here.parents[3] / 'assets/obseg/bg').glob('*.c')):
            if path.stem not in ('bg_depo_all_p', 'bg_jun_all_p', 'bg_sev_all_p',
                                 'bg_tra_all_p', 'bg_run_all_p'):
                continue
            source = path.read_text()
            if 'point_table_binary_1[]' not in source:
                continue
            data = native.jungle_fixture(source)
            for room, x, y, z in re.findall(r'\{&point_table_binary_(\d+),[^\n]+?, (-?[\d.]+), (-?[\d.]+), (-?[\d.]+)\}', source):
                struct.pack_into('>fff', data, 32 + int(room) * 24 + 12, float(x), float(y), float(z))
            target = work / (path.stem + '.seg')
            target.write_bytes(data)
            files.append(str(target))
            if path.stem == 'bg_depo_all_p':
                # Reproduce the reported orphan tail without relying on a
                # private attachment. Room 42's six extra points are unreferenced.
                old = struct.unpack_from('>I', data, 32 + 42 * 24)[0] & 0xffffff
                size = struct.unpack_from('>I', data, old - 4)[0]
                vertices = data[old:old + size]
                for y in (-33, -36, -9, -33, 29, -9):
                    vertices.extend(struct.pack('>hhhhhhBBBB', -162, y, 90, 0, 0, 0, 255, 255, 255, 255))
                data.extend(struct.pack('>I', len(vertices)))
                struct.pack_into('>I', data, 32 + 42 * 24, 0x0e000000 | len(data))
                data.extend(vertices)
                (work / 'depot-orphans.seg').write_bytes(data)
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}', f'-I{src}']
        sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c')
        subprocess.run(command + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work / 'depot-orphans.seg')] + files + sys.argv[1:], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
