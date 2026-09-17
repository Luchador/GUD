#!/usr/bin/env python3
"""Elevated prop movement using Depot's real Stan and native setup/history code."""
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
    with tempfile.TemporaryDirectory(prefix='geditor-prop-stan-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        (work / 'stan').mkdir()
        # Two props share a pad; moving one must detach without editing the other.
        data = bytearray(40)
        for _ in range(2):
            record = bytearray(128)
            struct.pack_into('>4I', record, 0, 0x01000003, 1 << 16, 0x1008, 0)
            data += record
        data += struct.pack('>I', 48)
        scale = struct.unpack('>f', struct.pack('>f', .21847887))[0]
        pos = [-1260.526 * scale, 248.49382 * scale, 1083.28711 * scale]
        pads = len(data)
        data += struct.pack('>9fII', *pos, 0, 1, 0, 0, 0, 1, 0, 0) + bytes(44)
        bounds = len(data)
        data += struct.pack('>9fII6f', *pos, 0, 1, 0, 0, 0, 1, 0, 0,
                            -2, 2, -3, 3, -1, 1) + bytes(68)
        struct.pack_into('>I', data, 12, 40)
        struct.pack_into('>II', data, 24, pads, bounds)
        struct.pack_into('>I', data, pads + 36, pads + 44 + 36)
        struct.pack_into('>I', data, bounds + 36, bounds + 68 + 36)
        (work / 'setup/UsetupmoveZ.set').write_bytes(data)
        source = (root / 'assets/obseg/stan/Tbg_depo_all_p_stanZ.c').read_text()
        depot = bytearray(struct.pack('>III', 0, 0x0e00000c, 0))
        for body in re.findall(r'StandTile tile_\d+ = \{(.*?)\n\};', source, re.S):
            v = [int(n, 0) for n in re.findall(r'-?0x[0-9a-fA-F]+|-?\d+', body)]
            ident, room, special, red, green, blue, count, a, b, c = v[:10]
            depot += struct.pack('>IHH', ident << 8 | room,
                                 special << 12 | red << 8 | green << 4 | blue,
                                 count << 12 | a << 8 | b << 4 | c)
            for i in range(count):
                depot += struct.pack('>hhhH', *v[10 + 4*i:14 + 4*i])
        depot += bytes(24)
        (work / 'stan/Tbg_depo_all_p_stanZ.stan').write_bytes(depot)
        # Reuse the deterministic model/ROM IO stubs, but load real Stan bytes.
        (work / 'assets.c').write_text('#define StanFileFree FixtureUnusedStanFree\n'
            f'#include "{here.parent / "object_duplicate/assets.c"}"\n')
        sources = ('setupload.c', 'actionblocks.c', 'bghistory.c', 'objectload.c',
                   'objectshade.c', 'rotation.c', 'scaling.c', 'stanload.c', 'stanquery.c')
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}',
                        str(here / 'check.c'), str(work / 'assets.c'), str(shim / 'platform.c'),
                        *[str(src / f) for f in sources], '-Wl,--gc-sections', '-lm',
                        '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
