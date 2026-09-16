#!/usr/bin/env python3
"""Portal margin input, native encoding, persistence and history regression checks."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
    extract = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(extract)
    with tempfile.TemporaryDirectory(prefix='geditor-portal-margin-') as temp:
        work = Path(temp)
        fixture = (here.parent / 'portals/check.c').read_text()
        (work / 'fixture.inc').write_text(''.join(extract.function(fixture, name) for name in ('Put', 'Float', 'Fixture')))
        (work / 'input.inc').write_text(extract.function((src / 'portalproperties.c').read_text(), 'PortalPropertiesParseMargin'))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
                        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                        str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                       + [str(src / name) for name in ('bgload.c', 'bgdocument.c', 'bgcompile.c', 'bgmaterial.c', 'bgrender.c', 'bghistory.c')]
                       + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
