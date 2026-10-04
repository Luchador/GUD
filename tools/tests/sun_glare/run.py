#!/usr/bin/env python3
"""Exercise production sun glare, camera rays and BG triangle collision on host."""
import os
from pathlib import Path
import re
import shlex
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


here = Path(__file__).resolve().parent
root = here.parents[2]
sky = (root / 'src/game/sky.c').read_text()
bond = (root / 'src/game/bondview.c').read_text()
player = (root / 'src/game/player.c').read_text()
view = function(bond, 'bondviewRenderPlayerView')
normal = view[view.index('gunUpdateAndFireBothHands();'):]
assert normal.index('gunRenderFirstPersonGunModels') < normal.index('skyRenderSunGlare')
assert normal.index('skyRenderSunGlare') < normal.index('bondviewRenderWatch')
assert normal.index('skyRenderSunGlare') < normal.index('bondviewRenderGaugeBars')
assert view.index('skyRenderSunGlare') < view.index('hudmsgBottomRender')  # Intro/cutscene path.
assert 'skyResetGlare(player_num);' in function(player, 'playerInitData')
assert function(sky, 'skyRender').index('target = 0.0f') < function(sky, 'skyRender').index('skyRenderBody')
body = function(sky, 'skyRenderBody')
assert body.index('skyPrepareSunGlare') > body.index('texture->gbiformat')

types = re.search(r'typedef struct SunGlareState\s*\{.*?\} SunGlareState;', sky, re.S)[0]
production = types + '\nstatic SunGlareState g_SunGlare[4];\n'
production += function((root / 'src/game/skybodymath.h').read_text(), 'skyBodyFinite')
production += function((root / 'src/game/cam.c').read_text(), 'transformAndNormalizeByLength2Dto3D')
production += function((root / 'src/game/line_tri_intersect.c').read_text(), 'intersectRayTriangle')
production += ''.join(function(sky, name) for name in (
    'skyResetGlare', 'skyPrepareSunGlare', 'skySunIsBlocked', 'skyRenderSunGlare'))

with tempfile.TemporaryDirectory(prefix='gud-sun-glare-') as temp:
    work = Path(temp)
    (work / 'production.inc').write_text(production)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
        '-fsanitize=address,undefined', '-I', str(root / 'src/game'), '-I', str(work),
        str(here / 'check.c'), '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
