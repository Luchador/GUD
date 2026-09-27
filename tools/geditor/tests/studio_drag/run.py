#!/usr/bin/env python3
"""Run the production Models-list callback against Win32 capture semantics.

The native list box captures on button-down. SetCapture sends the previous
owner WM_CAPTURECHANGED, including when the new owner is that same window.
This small message harness covers that reentrant notification; it does not
simulate native painting or replace a Windows drag-and-drop smoke test.
"""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    source = (here.parent.parent / 'src/renderstudio.c').read_text()
    start = source.index('static LRESULT CALLBACK RenderStudioModelDragProc(')
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    with tempfile.TemporaryDirectory(prefix='geditor-studio-drag-') as temp:
        work = Path(temp)
        (work / 'callback.inc').write_text(source[start:end])
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-fsanitize=address,undefined', f'-I{work}',
                        str(here / 'check.c'), '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
