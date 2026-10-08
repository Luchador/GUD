#!/usr/bin/env python3
"""Image property menu labels, checked values, ownership and message dispatch."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'view_modes/run.py')
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
with tempfile.TemporaryDirectory(prefix='geditor-image-properties-') as temp:
    work = Path(temp)
    browser = (src / 'browser.c').read_text()
    (work / 'menus.inc').write_text(''.join(helpers.function(browser, n) for n in
        ('BrowserAppendSurfaceMenu', 'BrowserAppendImageActions', 'BrowserDispatchSurfaceCommand')))
    executable = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'check.c'),
        str(src / 'texinfo.c'), '-Wl,--gc-sections', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
