#!/usr/bin/env python3
"""Check inherited BG transparency and inspector text using the real decoder."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_selection/run.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    source = (src / 'faceproperties.c').read_text()
    with tempfile.TemporaryDirectory(prefix='geditor-bg-transparency-') as temp:
        work = Path(temp)
        (work / 'inspector.inc').write_text(''.join(helpers.function(source, name) for name in
            ('FacePropertiesDepth', 'FacePropertiesRenderText')))
        subprocess.run([os.environ.get('CC', 'cc'), '-O2', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}',
                        f'-I{src}', f'-I{work}', str(here / 'check.c'),
                        str(src / 'bgdocument.c'), str(src / 'bgrender.c'),
                        '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
