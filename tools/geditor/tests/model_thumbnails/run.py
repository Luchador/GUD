#!/usr/bin/env python3
"""Exercise production cache, dependency and refresh logic without a Windows desktop."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / 'src'

def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'

with tempfile.TemporaryDirectory(prefix='geditor-model-thumbnails-') as directory:
    work = Path(directory)
    source = (SRC / 'modelthumbnail.c').read_text()
    header = (SRC / 'modelthumbnail.h').read_text()
    (work / 'thumbnail.inc').write_text('\n'.join(line for line in (header + '\n' + source).splitlines()
        if not line.startswith('#include')))
    browser = (SRC / 'browser.c').read_text()
    (work / 'refresh.inc').write_text(function(browser, 'BrowserRefreshModelThumbnail')
        + function(browser, 'BrowserRefreshModelImage'))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-D_DEFAULT_SOURCE', '-O1', '-g',
        '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
        '-I' + str(work), str(HERE / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], cwd=work, check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
