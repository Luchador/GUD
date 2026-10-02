#!/usr/bin/env python3
"""Check single-bank image packing, legacy cleanup and safe ROM sizing.

An optional local ROM is compacted in memory, checked and discarded. No game
assets are needed for the synthetic cases. Production C runs under ASan/UBSan.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    with tempfile.TemporaryDirectory(prefix='geditor-texture-bank-') as temp:
        check = Path(temp) / 'check'
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                   f'-I{here.parent / "image_import"}', f'-I{src}', str(here / 'check.c')]
        command += [str(src / name) for name in ('texrom.c', 'texinfo.c', 'texencode.c')]
        command += ['-Wl,--gc-sections', '-lm', '-o', str(check)]
        subprocess.run(command, check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run([str(check)] + ([str(args.rom.resolve())] if args.rom else []), env=env, check=True)


if __name__ == '__main__':
    main()
