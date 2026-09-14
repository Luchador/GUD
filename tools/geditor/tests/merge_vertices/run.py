#!/usr/bin/env python3
"""Merge geometry, save/reload, transactional rollback and M input routing."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    shim = here.parent / 'image_import'
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_primitives/run.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    with tempfile.TemporaryDirectory(prefix='geditor-merge-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'bg_primitives/check.c').read_text()
        (work / 'fixture.inc').write_text(''.join(helpers.function(fixture, name) for name in
            ('Put', 'Float', 'Fixture', 'RoundTrip')))
        editor = (src / 'geditor.c').read_text()
        (work / 'editor.inc').write_text(''.join(helpers.function(editor, name) for name in
            ('GEditorCanMergeSelectedBgVertices', 'GEditorMergeSelectedBgVertices', 'GEditorHandleMergeVerticesHotkey')))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}']
        sources = ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run(command + [str(here / 'check.c'), str(shim / 'platform.c')]
                       + [str(src / name) for name in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True, env=env)
        viewport = (src / 'viewport.c').read_text()
        (work / 'viewport.inc').write_text(''.join(helpers.function(viewport, name) for name in
            ('ViewportCompareVertexRefs', 'ViewportTriangleHidden', 'ViewportCornerVisible',
             'ViewportFindVertexCorner', 'ViewportSelectBgVertex')))
        subprocess.run(command + [str(here / 'viewport.c'), '-Wl,--gc-sections', '-o', str(work / 'viewport')], check=True)
        subprocess.run([str(work / 'viewport')], check=True, env=env)


if __name__ == '__main__':
    main()
