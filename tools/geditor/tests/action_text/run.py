#!/usr/bin/env python3
"""Text previews resolve native bank IDs, project overrides and missing data."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rom', type=Path, help='Also resolve Control text ID 8220 in a GUD ROM')
args = parser.parse_args()
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
shim = here.parent / 'project_rebase'
with tempfile.TemporaryDirectory(prefix='geditor-action-text-') as directory:
    work = Path(directory)
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
               '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{shim}', f'-I{src}', f'-I{root}',
               str(here / 'check.c'), str(shim / 'platform.c')]
    command += [str(src / f) for f in ('actiontext.c', 'actionblocks.c', 'textbank.c', 'rom.c')]
    command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work)] + ([str(args.rom.resolve())] if args.rom else []),
                   check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
