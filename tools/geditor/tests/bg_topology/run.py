#!/usr/bin/env python3
"""Save/reopen authoring connectivity independently of native render packing."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
shim = here.parent / 'image_import'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_primitives/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-bg-topology-') as temp:
    work = Path(temp)
    primitive = (here.parent / 'bg_primitives/check.c').read_text()
    merge = (here.parent / 'merge_vertices/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(primitive, name)
        for name in ('Put', 'Float', 'Fixture')) + ''.join(extract.function(merge, name)
        for name in ('Counts', 'Same', 'MakeStrip')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
        '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}',
        str(here / 'check.c'), str(shim / 'platform.c')]
    command += [str(src / name) for name in ('bgload.c', 'bgdocument.c', 'bgcompile.c',
        'bgmaterial.c', 'bgrender.c', 'bgmerge.c', 'bgdisconnect.c')]
    command += ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
