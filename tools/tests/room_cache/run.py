#!/usr/bin/env python3
"""Exercise real mema and room loading/cache recovery on Linux x86-64."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def strip_includes(source):
    return re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


bg = (ROOT / 'src/game/bg.c').read_text()
header = (ROOT / 'src/game/bg.h').read_text()
source = '''#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
typedef int32_t s32;
typedef uint32_t u32;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int bool;
#define TRUE 1
#define FALSE 0
#define MAXROOMCOUNT 8
#define SPSEGMENT_BG_VTX 14
#define BG_DEBUG_PRIMARY 1
#define BG_DEBUG_SECONDARY 2
#define BG_CUTOUT_THRESHOLD 128
#define G_AC_NONE 0
typedef struct { float x, y, z; } coord3d;
typedef struct { u8 raw[16]; } Vtx;
typedef struct { u32 words[2]; } Gfx;
'''
for name in ('RoomVtxBatchBounds', 'RoomInfo'):
    source += re.search(r'typedef struct ' + name + r'\s*\{.*?\}\s*' + name + r';', header, re.S)[0] + '\n'
source += strip_includes((ROOT / 'src/mema.h').read_text())
source += strip_includes((ROOT / 'src/mema.c').read_text())
source += strip_includes((ROOT / 'src/game/rendercache.h').read_text())
source += '''
static RoomInfo g_BgRoomInfo[MAXROOMCOUNT];
'''
source += re.search(r'static struct \{[^{}]*\} g_BgOneCycleRooms\[MAXROOMCOUNT\];', bg)[0] + '\n'
source += (HERE / 'harness.h').read_text()
source += function(bg, 'bgClearRoomRenderCaches')
source += strip_includes((ROOT / 'src/game/rendercache.c').read_text())
for name in ('bgLoadRoomVtxData', 'bgLoadRoomPrimaryGdl', 'bgLoadRoomSecondaryGdl',
             'bgBuildRoomOneCycleGdl', 'bgLoadRoomModelData', 'bgFreeRoomData',
             'bgRenderRoomPrimary', 'bgRenderRoomSecondary'):
    source += function(bg, name)

# Exercise the actual boss admission condition, including two queued tasks and
# pending settings. Check that reclamation precedes any new tick/display list.
boss = (ROOT / 'src/boss.c').read_text()
condition = re.search(r'if \((g_MainStageNum < 0 && pendingGfx < 2U.*?)\)\s*\{', boss, re.S)[1]
source += 'static bool testFrameAllowed(u32 pendingGfx) { return ' + condition + '; }\n'
assert boss.index('renderCacheReclaim();') < boss.index('gdl = firstGdl = dynGetMasterDisplayList();')
assert boss.index('renderCacheReclaim();') < boss.index('lvTick();')
assert 'while (g_MainStageNum < 0 || pendingGfx != 0)' in boss
assert boss.index('memaReset(') < boss.index('dynInitMemory()')
dyn = function((ROOT / 'src/game/dyn.c').read_text(), 'dynInitMemory')
assert dyn.index('renderCacheReset();') < dyn.index('modelOneCycleResetCache();')
for section in ('text', 'data', 'rodata', 'bss'):
    assert f'rendercache.o (.{section})' in (ROOT / f'ld/game.{section}.ld.inc').read_text()
source += '#line 1 "room_cache/check.c"\n'
source += (HERE / 'check.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-room-cache-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-sign-compare', '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
        '-Wno-int-conversion', '-fsanitize=address,undefined',
        str(work / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True, env=dict(
        os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
