#!/usr/bin/env python3
"""Opaque BG batching: native commands, ordering barriers and shipped rooms."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / 'src'
ROOT = HERE.parents[3]
spec = importlib.util.spec_from_file_location('native', HERE.parent / 'bg_transparency/run.py')
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
with tempfile.TemporaryDirectory(prefix='geditor-texture-batching-') as directory:
    work = Path(directory)
    assets = []
    for name in ('depo', 'ark', 'arch', 'sev', 'cryp', 'silo', 'dam', 'jun'):
        path = ROOT / f'assets/obseg/bg/bg_{name}_all_p.c'
        if name == 'jun': path = ROOT / 'assets/obseg/bg/u/bg_jun_all_p.c'
        target = work / f'{name}.seg'
        target.write_bytes(native.jungle_fixture(path.read_text()))
        assets.append(str(target))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
               '-fsanitize=address,undefined', f'-I{HERE.parent / "image_import"}', f'-I{SRC}']
    sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c')
    subprocess.run(command + [str(HERE / 'check.c'), str(HERE.parent / 'image_import/platform.c')]
                   + [str(SRC / file) for file in sources]
                   + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')] + assets + sys.argv[1:], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
