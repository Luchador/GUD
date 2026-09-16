#!/usr/bin/env python3
"""Portal deletion, native persistence, history and production controller checks."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-portal-delete-') as directory:
    work = Path(directory)
    (work / 'bg').mkdir()
    fixture = (here.parent / 'portals/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, n) for n in ('Put', 'Get', 'Float', 'Fixture')))
    viewport = (src / 'viewport.c').read_text()
    (work / 'selection.inc').write_text(extract.function(viewport, 'ViewportGetSelectedPortalFaces'))
    editor = (src / 'geditor.c').read_text()
    (work / 'controller.inc').write_text(extract.function(editor, 'GEditorDeleteSelectedPortals'))
    # Delete already dispatches from face mode. Ensure portals precede BG deletion.
    dispatch = editor.split('case VIEWPORT_WM_DELETE_SELECTION:')[1].split('case BROWSER_')[0]
    assert dispatch.index('GEditorDeleteSelectedPortals') < dispatch.index('GEditorDeleteSelectedBgFaces')
    assert 'wparam == VK_DELETE' in viewport
    binary = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'check.c'),
                    str(here.parent / 'image_import/platform.c'),
                    *[str(src / n) for n in ('bgload.c', 'bgdocument.c', 'bgportal.c', 'bgcompile.c',
                                           'bgmaterial.c', 'bgrender.c', 'bghistory.c')],
                    '-Wl,--gc-sections', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
