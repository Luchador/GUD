#!/usr/bin/env python3
"""Read-only BG visibility decoding; optionally check supplied .seg files."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('backgrounds', nargs='*', type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    with tempfile.TemporaryDirectory(prefix='geditor-bg-commands-') as temp:
        binary = Path(temp) / 'check'
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', str(here / 'check.c'),
                   str(src / 'bgcommands.c'), str(src / 'bgload.c'), str(shim / 'platform.c'),
                   '-Wl,--gc-sections', '-lm', '-o', str(binary)]
        subprocess.run(command, check=True)
        env = dict(os.environ)
        env.setdefault('ASAN_OPTIONS', 'detect_leaks=1')
        env.setdefault('UBSAN_OPTIONS', 'halt_on_error=1')
        subprocess.run([str(binary)] + [str(p.resolve()) for p in args.backgrounds], check=True, env=env)


if __name__ == '__main__':
    main()
