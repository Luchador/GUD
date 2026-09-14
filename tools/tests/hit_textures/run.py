#!/usr/bin/env python3
"""Run production bullet/material lookup paths against loaded texture metadata.

An optional source directory supplies pre-fix bg.c/propobj.c for reproduction.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
COLLISION_ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def portable(source):
    # Retain the N64 packet byte layout while reading multi-byte fields on a
    # little-endian host. Vertex structs remain native with their 16-byte stride.
    source = re.sub(r'\(\(u32 \*\)\s*(gdl|tcmd)\)\[(\d)\]',
                    r'be32((u8 *)\1 + 4 * \2)', source)
    source = re.sub(r'\(\(u16 \*\)\s*gdl\)\[(\d)\]',
                    r'be16((u8 *)gdl + 2 * \1)', source)
    for pointer in ('temp.vertices', 'vtxbase', 'vertices', 'ptr_bgdata_room_fileposition_list'):
        source = source.replace('(s32)' + pointer, '(intptr_t)' + pointer)
        source = source.replace('(s32) ' + pointer, '(intptr_t) ' + pointer)
    # The baseline assumes an eight-byte prefix; the host descriptor pointer
    # widens it. Emulate exactly the N64 big-endian halfword that it would read.
    source = re.sub(r'\*\(\(u16 \*\) \((temp.word|padC) \| 0x80000000\)\)',
                    r'legacyTextureHalfword(\1)', source)
    return source


image = (ROOT / 'src/game/image.c').read_text()
image_header = (ROOT / 'src/game/image.h').read_text()
bg = (COLLISION_ROOT / 'src/game/bg.c').read_text()
props = (COLLISION_ROOT / 'src/game/propobj.c').read_text()
source = (HERE / 'harness.h').read_text()
source += re.search(r'typedef enum IMAGEIDS\s*\{.*?\}\s*\w+;',
                    (ROOT / 'assets/image_externs.h').read_text(), re.S)[0] + '\n'
source += re.search(r'struct tex \{.*?\n};', image_header, re.S)[0] + '\n'
source += re.search(r'#define TEX_DATA_PREFIX_MAGIC[^\n]*', image)[0] + '\n'
source += re.search(r'struct texdataprefix\s*\{.*?\n};', image, re.S)[0] + '\n'
source += function(image, 'texFindByData')
source += '''
static u16 legacyTextureHalfword(u32 address) {
    struct texdataprefix *prefix = (void *)(ram + address + 8 - sizeof(*prefix));
    return prefix->magic >> 16;
}
'''
source += function((ROOT / 'src/game/lightfixture.c').read_text(), 'check_if_imageID_is_light')
source += function((ROOT / 'src/game/line_tri_intersect.c').read_text(), 'intersectRayTriangle')
source += function(bg, 'bgTestRayIntersectsBbox')
source += portable(function(bg, 'bgTestRayIntersectionInRoom'))
source += portable(function(props, 'bgTestHitOnObj'))
source += function((ROOT / 'src/game/chrprop.c').read_text(), 'chrpropCreateBgShotImpact')
source += (HERE / 'check.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-hit-textures-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
                '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
                '-Wno-unused-but-set-variable', '-Wno-empty-body', '-Wno-sign-compare',
                '-Wno-pointer-to-int-cast', '-Wno-maybe-uninitialized']
    command += shlex.split(os.environ.get('TEST_CFLAGS', ''))
    command += [str(work / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True)
