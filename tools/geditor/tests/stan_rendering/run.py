#!/usr/bin/env python3
"""Exercise the production stan draw paths with recorded OpenGL state."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
sys.dont_write_bytecode = True
spec.loader.exec_module(extract)
viewport = (src / 'viewport.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-stan-rendering-') as temp:
    work = Path(temp)
    vertex = re.search(r'typedef struct Vertex \{.*?\} Vertex;', viewport, re.S)[0]
    (work / 'types.inc').write_text(vertex + '\n')
    (work / 'draw.inc').write_text(''.join(extract.function(viewport, name) for name in (
        'ViewportStanVisible', 'ViewportApplyStanOpacity', 'ViewportDrawStanExtrusion', 'ViewportDrawStanOverlay')))
    # Check that the live repaint and component markers use these tested paths.
    assert 'ViewportDrawStanOverlay(state);' in extract.function(viewport, 'ViewportPaintGL')
    assert 'ViewportApplyStanOpacity(state, FALSE);' in extract.function(viewport, 'ViewportDrawTransformTools')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                    '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                    str(here / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
