#!/usr/bin/env python3
"""Check production spark draws against independently bound, isolated draws."""
from pathlib import Path
import os
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


fx = (ROOT / 'src/game/fx.c').read_text()
header = (ROOT / 'src/game/fx.h').read_text()
types = (ROOT / 'src/bondtypes.h').read_text()
source = (HERE / 'check.c').read_text().split('/* PRODUCTION */')[0]
source += re.search(r'typedef struct sImageTableEntry\s*\{.*?\n} sImageTableEntry;', types, re.S)[0] + '\n'
source += header[header.index('typedef struct BulletSpark'):header.index('void fxResetAllSparks')]
source += '\n'.join(re.findall(r'^#define BULLET_\w+_MAX[^\n]*', fx, re.M)) + '\n'
source += '''
static BulletSpark g_BulletSparks[BULLET_SPARKS_MAX];
static MovingBulletSpark g_MovingBulletSparks[BULLET_MOVING_SPARKS_MAX];
static Vtx g_BulletSparkVertexTemplate;
static void texSelect(Gfx **gdl, sImageTableEntry *image, s32 style, s32 depth, u32 offset)
{
    Emit((*gdl)++, BIND, (uintptr_t)image, style, depth, offset);
}
'''
for name in ('fxRenderBulletSpark', 'fxRenderBulletSparks', 'fxRenderMovingSparks', 'fxRenderAllSparks'):
    source += function(fx, name)
source += (HERE / 'check.c').read_text().split('/* PRODUCTION */')[1]

with tempfile.TemporaryDirectory(prefix='gud-spark-binding-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror']
    command += shlex.split(os.environ.get('TEST_CFLAGS', ''))
    subprocess.run(command + [str(work / 'check.c'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True)
