#!/usr/bin/env python3
"""Exercise production model-viewer input and culling without a Win32 desktop."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'object_properties/run.py')
    helpers = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    spec.loader.exec_module(helpers)
    viewport = (src / 'viewport.c').read_text()
    editor = (src / 'modeleditor.c').read_text()
    declarations = re.search(r'typedef struct SceneBatch \{.*?\} SceneBatch;', viewport, re.S)[0]
    declarations += '\n' + re.search(r'typedef struct ModelEditorEntry \{.*?\} ModelEditorEntry;', editor, re.S)[0]
    logic = ''.join(helpers.function(viewport, name) for name in
                    ('ViewportBatchCullMode', 'ViewportOrbitInput'))
    logic += ''.join(helpers.function(editor, name) for name in
                     ('ModelEditorCulling', 'ModelEditorSurface', 'ModelEditorOpenModel'))
    with tempfile.TemporaryDirectory(prefix='geditor-model-ui-') as temp:
        work = Path(temp)
        (work / 'types.inc').write_text(declarations)
        (work / 'logic.inc').write_text(logic)
        subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                        str(here / 'check.c'), str(src / 'orbitcamera.c'), '-lm',
                        '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        # Resolve setup selections through the actual game model tables.
        subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                        '-fsanitize=address,undefined', f'-I{here.parent / "image_import"}',
                        f'-I{src}', f'-I{src.parents[2]}', str(here / 'setup.c')]
                       + [str(src / name) for name in ('objectload.c', 'characterload.c', 'modelload.c')]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'setup')], check=True)
        subprocess.run([str(work / 'setup')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
