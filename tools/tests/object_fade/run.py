#!/usr/bin/env python3
"""Exercise production object fade/render code without an N64 or GUI."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


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
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    objects = (root / 'src/game/propobj.c').read_text()
    props = (root / 'src/game/chrprop.c').read_text()
    constants = '\n'.join(re.findall(r'^#define OBJFADE_.*', objects, re.M)) + '\n'
    constants += '\n'.join(re.findall(r'^#define CHROBJ_TIMETOREGEN.*', (root / 'src/game/propobj.h').read_text(), re.M))
    logic = ''.join(function(objects, name) for name in
                    ('objInitFadeDistances', 'objCalcScreenFadeAlpha', 'objCalcDistanceFadeAlpha', 'objRenderProp'))
    logic += ''.join(function(props, name) for name in ('chrpropAllocate', 'chrpropFree'))
    # Ensure real initialization consumes metadata, not just the test fixture.
    assert 'objInitFadeDistances(prop, obj);' in function(objects, 'objInit')
    with tempfile.TemporaryDirectory(prefix='gud-object-fade-') as temp:
        temp = Path(temp)
        (temp / 'constants.inc').write_text(constants + '\n')
        (temp / 'logic.inc').write_text(logic)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-Wno-unused-variable', '-ffp-contract=off', '-fsanitize=address,undefined',
                        f'-I{root}', f'-I{temp}', str(here / 'check.c'), '-lm', '-o', str(temp / 'check')], check=True)
        subprocess.run([str(temp / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
