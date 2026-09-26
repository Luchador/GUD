#!/usr/bin/env python3
"""Portal clipboard geometry, persistence, selection and production controller."""
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
with tempfile.TemporaryDirectory(prefix='geditor-portal-clipboard-') as directory:
    work = Path(directory)
    (work / 'bg').mkdir()
    fixture = (here.parent / 'portals/check.c').read_text()
    persist = (here.parent / 'portal_deletion/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(fixture, n) for n in ('Put', 'Get', 'Float', 'Fixture'))
                                    + ''.join(extract.function(persist, n) for n in ('Persist',)))
    viewport = (src / 'viewport.c').read_text()
    (work / 'selection.inc').write_text(''.join(extract.function(viewport, n) for n in
        ('ViewportGetSelectedPortalFaces', 'ViewportSelectPortalFaces')))
    editor = (src / 'geditor.c').read_text()
    (work / 'controller.inc').write_text(''.join(extract.function(editor, n) for n in
        ('GEditorCanUsePortalClipboard', 'GEditorCanCopyPortals', 'GEditorCanPastePortals',
         'GEditorCopyPortals', 'GEditorPastePortalSnapshot', 'GEditorPastePortals', 'GEditorDuplicatePortals')))
    assert 'GEditorDuplicatePortals(hwnd,' in editor.split('case VIEWPORT_WM_DUPLICATE_PORTALS:')[1].split('case ',1)[0]
    dispatch = editor.split('case ID_EDIT_COPY_FACES:')[1].split('case ID_GEOMETRY_MERGE_VERTICES:')[0]
    assert 'GEditorCopyPortals(hwnd)' in dispatch and 'GEditorPastePortals(hwnd)' in dispatch
    assert 'BgPortalFileFree(&g_PortalClipboard)' in dispatch
    assert 'BgPortalFileFree(&g_PortalClipboard)' in extract.function(editor, 'GEditorCopyObject')
    # Existing lifecycle sites must clear all three clipboard kinds together.
    assert editor.count('BgPortalFileFree(&g_PortalClipboard)') == 5
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
               f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
    sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c', 'rotation.c', 'scaling.c')
    subprocess.run(command + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                   + [str(src / n) for n in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
