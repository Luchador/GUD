#!/usr/bin/env python3
"""Exercise production AA, VI, watch scrolling, and profiling logic on the host."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

def strip_includes(s):
    return re.sub(r'^#include[^\n]*\n', '', s, flags=re.M)

def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'

source = (HERE / 'harness.h').read_text()
for name in ('renderconfig.h', 'renderprofile.h'):
    source += strip_includes((ROOT / 'src/game' / name).read_text())
source += 'struct GfxInfo_s { OSScTask task; u32 cfb, unk5C; RenderProfileTask renderProfile; };\n'
config = strip_includes((ROOT / 'src/game/renderconfig.c').read_text())
# Only the N64 physical/KSEG0 address translation changes on the host.
config = config.replace('(Gfx *)(physical | 0x80000000)', '(Gfx *)(g_TestRam + physical)')
config = config.replace('((u32)cmd & 0x1fffffff)', '((u8 *)cmd - g_TestRam)')
source += config
source += strip_includes((ROOT / 'src/game/renderprofile.c').read_text())
options = (ROOT / 'src/game/options.c').read_text()
header = (ROOT / 'src/game/options.h').read_text()
source += header[header.index('typedef enum WATCH_GAME_OPTIONS_INDEX'):header.index('typedef enum WATCH_BRIEF_INDEX')]
source += '''
#define WATCH_VISIBLE_TOGGLE_OPTIONS 8
static s32 g_WatchFirstToggleOption;
static u32 g_WatchGameOptionsIndex;
'''
source += function((ROOT / 'src/game/textrelated.c').read_text(), 'textMeasure')
for name in ('draw_options_labels', 'watchScrollGameOptions', 'watchNavToggleOptions',
             'game_options_music_volume_navigation', 'game_options_fx_volume_navigation',
             'watchDrawRenderOption', 'watchDrawToggleOptions'):
    source += function(options, name)
source += function((ROOT / 'src/sched.c').read_text(), '__scExec')
source += (HERE / 'check.c').read_text()

# Ensure the actual integration uses the tested helpers at the correct points.
rsp = (ROOT / 'src/game/rsp.c').read_text()
assert rsp.index('renderApplyAa(firstGdl, gdl)') < rsp.index('osWritebackDCacheAll()')
sched = (ROOT / 'src/sched.c').read_text()
assert '(sp->state & OS_SC_YIELD) == 0' in sched
assert 'renderProfileTaskStart(sp)' in sched
assert sched.index('renderProfileTaskDone(t)') < sched.index('sc->curRDPTask = NULL;')
boss = (ROOT / 'src/boss.c').read_text()
assert '(!renderSettingsPending() || pendingGfx == 0)' in boss
for name in ('data', 'rodata', 'bss'):
    script = (ROOT / f'ld/game.{name}.ld.inc').read_text()
    assert f'renderconfig.o (.{name})' in script
    assert f'renderprofile.o (.{name})' in script
# Retraces begin before bossInitMainthreadData initializes demand paging.
# All code used by VI/scheduler hooks must therefore be in resident .code.
resident = (ROOT / 'ld/lib.text.ld.inc').read_text()
paged = (ROOT / 'ld/game.text.ld.inc').read_text()
for module in ('renderconfig', 'renderprofile'):
    entry = f'build/OUTCODE/src/game/{module}.o (.text);'
    assert entry in resident and entry not in paged, f'{module} must be resident for scheduler callbacks'
linker = (ROOT / 'ge007.ld').read_text()
for symbol in ('renderConfigureViMode', 'renderProfileTaskStart', 'renderProfileTaskDone'):
    assert f'{symbol} >= _codeSegmentStart && {symbol} < _codeSegmentEnd' in linker
assert '$(wildcard ld/*.ld.inc)' in (ROOT / 'Makefile').read_text()
print('Boot layout: VI and profiler hooks are resident and protected by linker assertions', flush=True)
assert 'renderRestoreAaGdl(start, end)' in (ROOT / 'src/game/bgapply.c').read_text()
assert 'renderInvalidateAaCache()' in function((ROOT / 'src/game/tex.c').read_text(), 'texLoadFromGdl')
assert 'renderInvalidateAaCache()' in function((ROOT / 'src/game/dyn.c').read_text(), 'dynInitMemory')

with tempfile.TemporaryDirectory(prefix='gud-render-options-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    (work / 'PR').mkdir()
    # GUD widens Gwords on PC; these tests exercise the N64 8-byte packet.
    gbi = (ROOT / 'include/PR/gbi.h').read_text()
    gbi = gbi.replace('uintptr_t w0;', 'u32 w0;').replace('uintptr_t w1;', 'u32 w1;')
    # SDK blender macros intentionally build unsigned register bits; make the
    # shifts unsigned on the host so UBSan does not flag their legacy int casts.
    for arg in ('m1a', 'm1b', 'm2a', 'm2b'):
        gbi = gbi.replace(f'({arg}) <<', f'((u32)({arg})) <<')
    (work / 'PR/gbi.h').write_text(gbi)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-pointer-to-int-cast',
                '-Wno-sign-compare', '-Wno-unused-but-set-variable', '-Wno-int-to-pointer-cast', '-I', str(work), '-idirafter', str(ROOT / 'include')]
    command += shlex.split(os.environ.get('TEST_CFLAGS', ''))
    command += [str(work / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True)
