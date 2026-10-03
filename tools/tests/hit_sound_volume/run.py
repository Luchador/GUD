#!/usr/bin/env python3
"""Exercise GUD's real sound player/queue and positional impact callers on host."""
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


objects = (ROOT / 'src/game/propobj.c').read_text()
gunfire = (ROOT / 'src/game/gunfire.c').read_text()
gun = (ROOT / 'src/game/gun.h').read_text()
production = ''.join(function(objects, name) for name in (
    'sndCalculateVolumeFromDistance', 'sndDistanceToNearestPlayer',
    'sndCalculateVolumeAtPosition', 'sndCalculateFleshHitVolumeAtPosition',
    'chrobjSndPlayAtPosition'))
production += ''.join(function(gunfire, name) for name in (
    'gunGetFreeSfxState', 'gunfirePlayImpactSfx', 'gunfirePlaySfxBulletImpact',
    'gunfirePlaySfxBulletThroughGlass', 'gunfirePlaySfxRicochetSounds'))
declarations = ''.join(re.search(r'struct ' + name + r'\s*\{.*?\};', gun, re.S)[0] + '\n'
                       for name in ('RicochetSoundsSmall', 'PunchSounds', 'BulletFleshSounds',
                                    'LaserRichochetSounds', 'RicochetSoundsLarge'))

with tempfile.TemporaryDirectory(prefix='gud-hit-sound-') as directory:
    work = Path(directory)
    (work / 'production.inc').write_text(production)
    (work / 'declarations.inc').write_text(declarations)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-O1', '-g', '-fno-strict-aliasing',
        '-Wno-pointer-to-int-cast', '-Wno-incompatible-pointer-types',
        '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        '-idirafter', str(ROOT / 'include'), '-idirafter', str(ROOT / 'include/PR'),
        '-I', str(ROOT / 'src'), '-I', str(ROOT), '-I', str(work),
        str(HERE / 'check.c'), '-Wl,--gc-sections', '-lm', '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
