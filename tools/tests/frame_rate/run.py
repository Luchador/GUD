#!/usr/bin/env python3
"""Exercise the production FPS counter and HUD without a ROM."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    lv = (ROOT / 'src/game/lv.c').read_text()
    with tempfile.TemporaryDirectory(prefix='gud-frame-rate-') as directory:
        work = Path(directory)
        (work / 'hud.inc').write_text(function(lv, 'lvDrawFrameRateText')
                                     + function(lv, 'lvDrawFrameRateDisplay'))
        command = shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-parameter', '-fsanitize=address,undefined']
        subprocess.run(command + ['-I', str(work), str(HERE / 'hud.c'),
                                 '-o', str(work / 'hud')], check=True)
        subprocess.run([str(work / 'hud')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                                UBSAN_OPTIONS='halt_on_error=1'))

    boss = (ROOT / 'src/boss.c').read_text()
    assert boss.count('lvDrawFrameRateDisplay(gdl)') == 1
    for path in (ROOT / 'src').rglob('*'):
        if path.suffix in ('.c', '.h'):
            assert 'frameProfile' not in path.read_text(), path
            assert 'frameprofile.h' not in path.read_text(), path
    for path in [ROOT / 'Makefile', ROOT / 'ge007.ld', *(ROOT / 'ld').glob('*.ld.inc')]:
        text = path.read_text()
        assert 'frameprofile' not in text and 'frameProfile' not in text, path
    print('PASS: one FPS call per frame and no bottleneck module/hooks/linker entries.')


if __name__ == '__main__':
    main()
