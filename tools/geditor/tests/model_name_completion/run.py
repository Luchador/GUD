#!/usr/bin/env python3
"""Exercise production model-name completion and commit logic with Win32 stubs.

Native combo drawing, focus and dropdown behavior still need a Windows check.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r'^(?:static )?\w+ (?:CALLBACK )?' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    assert match, name
    opening = source.index('{', match.start())
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    here = Path(__file__).resolve().parent
    source = (here.parents[1] / 'src/modeleditor.c').read_text()
    names = ('ModelEditorFindName', 'ModelEditorCompleteName', 'ModelEditorNameEditProc',
             'ModelEditorSelect', 'ModelEditorAcceptName', 'ModelEditorNameKey',
             'ModelEditorRefreshImages')
    with tempfile.TemporaryDirectory(prefix='geditor-model-completion-') as temporary:
        work = Path(temporary)
        (work / 'logic.inc').write_text(''.join(function(source, name) for name in names))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        f'-I{work}', str(here / 'check.c'), '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
