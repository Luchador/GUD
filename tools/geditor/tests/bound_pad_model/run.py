#!/usr/bin/env python3
"""Create props on existing bound pads: native placement, persistence and history."""
import os
from pathlib import Path
import runpy
import struct
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-bound-pad-model-') as temp:
        work = Path(temp)
        (work / 'setup').mkdir()
        data = bytearray(40) + struct.pack('>I', 48) + bytes(44)
        bounds = len(data)
        names = bounds + 2 * 68
        # Non-centred bounds catch accidental anchor/size replacement.
        data += struct.pack('>9fII6f', 100, 200, 300, 0, 1, 0, 0, 0, 1,
                            names, 0, -10, 30, -20, 40, -5, 15)
        data += bytes(68) + b'p1a\0'
        struct.pack_into('>I', data, 12, 40)
        struct.pack_into('>I', data, 24, 44)
        struct.pack_into('>I', data, 28, bounds)
        (work / 'setup/UsetupboundZ.set').write_bytes(data)
        extract = runpy.run_path(str(here.parent / 'object_properties/run.py'))['function']
        character = (src / 'characterload.c').read_text()
        (work / 'character-placement.inc').write_text(''.join(extract(character, name) for name in
            ('CharacterGetPadPlacement', 'CharacterGetPadPosition')))
        # Reuse deterministic mesh/asset IO; the native placement code is real.
        (work / 'assets.c').write_text(
            '#define ModelGetPropDefinition FixtureUnusedDefinition\n'
            '#define ModelEditsGetData FixtureUnusedEdits\n'
            f'#include "{here.parent / "object_duplicate/assets.c"}"\n'
            '#undef ModelGetPropDefinition\n#undef ModelEditsGetData\n'
            'BOOL ModelGetPropDefinition(int id,const char **name,float *scale) {\n'
            ' if (name) { *name="PfixtureZ"; } if (scale) { *scale=1; } return id==1 || id==2; }\n'
            'const unsigned char *ModelEditsGetData(const char *dir,const char *name,\n'
            ' DWORD *size,const char **why) { return NULL; }\n')
        sources = ('setupload.c', 'actionblocks.c', 'bghistory.c', 'bgrender.c', 'objectload.c',
                   'objectshade.c', 'rotation.c', 'scaling.c', 'stanquery.c')
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{src.parents[2]}',
                        f'-I{work}', str(here / 'check.c'), str(work / 'assets.c'), str(shim / 'platform.c'),
                        *[str(src / f) for f in sources], '-Wl,--gc-sections', '-lm',
                        '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
