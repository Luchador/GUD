#!/usr/bin/env python3
"""Check Depot's paired train-door previews using their native model/records.
No ROM, Windows or emulator is required.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    root = src.parents[2]
    shim = here.parent / 'image_import'
    setup = (root / 'assets/obseg/setup/UsetupdepoZ.c').read_text()
    records = [re.search(r'/\* Type = Door; index = ' + str(i)
                        + r' \*/\s*([^\n]+)', setup)[1] for i in (184, 185)]
    header = ('#define _mkshort(a,b) (((a)<<8)|(b))\n'
              '#define _mkword(a,b) (((uint32_t)(a)<<16)|(uint16_t)(b))\n'
              'static const uint32_t doorwords[][64] = {\n'
              + ',\n'.join('{' + record + '}' for record in records) + '\n};\n')
    with tempfile.TemporaryDirectory(prefix='geditor-door-preview-') as temp:
        temp = Path(temp)
        (temp / 'doors.h').write_text(header)
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{root}', f'-I{temp}',
                   str(here / 'check.c'), str(shim / 'platform.c')]
        command += [str(src / name) for name in ('modelload.c', 'bgmaterial.c', 'bgrender.c', 'setupload.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(temp / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(temp / 'check'), str(root)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
