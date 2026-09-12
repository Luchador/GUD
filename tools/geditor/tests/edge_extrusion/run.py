#!/usr/bin/env python3
"""Edge extrusion geometry, native serialization, transactions and selection."""
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
    spec = importlib.util.spec_from_file_location('primitives', here.parent / 'bg_primitives/run.py')
    helpers = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    spec.loader.exec_module(helpers)
    with tempfile.TemporaryDirectory(prefix='geditor-extrusion-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'bg_primitives/check.c').read_text()
        (work / 'fixture.inc').write_text(''.join(helpers.function(fixture, n) for n in
            ('Put', 'Float', 'Fixture', 'RoundTrip')))
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', f'-I{shim}', f'-I{src}', f'-I{work}']
        sources = ['bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c']
        subprocess.run(command + [str(here / 'check.c'), str(shim / 'platform.c')]
                       + [str(src / n) for n in sources]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        subprocess.run([str(work / 'check'), str(work)], check=True, env=env)
        viewport = (src / 'viewport.c').read_text()
        names = ('ViewportCompareVertexRefs', 'ViewportCompareFaceRefs', 'ViewportTriangleHidden',
                 'ViewportCornerVisible', 'ViewportPrepareEdgeExtrusion', 'ViewportSelectBgEdges',
                 'ViewportCancelTransform', 'ViewportEndTransform')
        (work / 'viewport.inc').write_text(''.join(helpers.function(viewport, n) for n in names))
        subprocess.run(command + [str(here / 'viewport.c'), '-Wl,--gc-sections', '-lm',
                                  '-o', str(work / 'viewport')], check=True)
        subprocess.run([str(work / 'viewport')], check=True, env=env)


if __name__ == '__main__':
    main()
