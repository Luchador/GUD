#!/usr/bin/env python3
"""Exercise persistent AA/VI settings and watch rendering/navigation on the host."""
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
source += strip_includes((ROOT / 'src/game/renderconfig.h').read_text())
config = strip_includes((ROOT / 'src/game/renderconfig.c').read_text())
# Only the N64 physical/KSEG0 address translation changes on the host.
config = config.replace('(Gfx *)(physical | 0x80000000)', '(Gfx *)(g_TestRam + physical)')
config = config.replace('((u32)cmd & 0x1fffffff)', '((u8 *)cmd - g_TestRam)')
source += config
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
             'watchDrawRenderOption', 'watchDrawMoreOptionsTriangle', 'watchDrawToggleOptions'):
    source += function(options, name)
# Use the shipped Bank Gothic metrics: lowercase has smaller capital-shaped
# glyphs than uppercase, which is why the new rows previously looked too big.
font = (ROOT / 'assets/font/fontBankGothic.c').read_text()
def font_array(name):
    match = re.search(r'\b' + name + r'\[\]\s*=\s*\{(.*?)\};', font, re.S)
    return [int(word, 16) for word in re.findall(r'0x[0-9a-fA-F]+', match[1])]
kerning = font_array('fontBankGothic_kerning')
glyphs = font_array('fontBankGothic_fontchartable')
assert len(kerning) == 169 and len(glyphs) == 94 * 6
source += 'static void test_load_font(void) {\n'
for i, value in enumerate(kerning):
    source += f'g_TestFont.kerning[{i}] = {value if value < 0x80000000 else value - 0x100000000};\n'
for i in range(94):
    for j, field in enumerate(('index', 'baseline', 'height', 'width', 'kerningindex')):
        source += f'g_TestFont.chars[{i}].{field} = {glyphs[i * 6 + j]};\n'
source += '}\n'
source += (HERE / 'check.c').read_text()

# Ensure the actual integration uses the tested helpers at the correct points.
rsp = (ROOT / 'src/game/rsp.c').read_text()
assert rsp.index('renderApplyAa(firstGdl, gdl)') < rsp.index('osWritebackDCacheAll()')
boss = (ROOT / 'src/boss.c').read_text()
assert '(!renderSettingsPending() || pendingGfx == 0)' in boss
for name in ('data', 'rodata', 'bss'):
    script = (ROOT / f'ld/game.{name}.ld.inc').read_text()
    assert f'renderconfig.o (.{name})' in script
# Retraces begin before bossInitMainthreadData initializes demand paging.
# All code used by VI/scheduler hooks must therefore be in resident .code.
resident = (ROOT / 'ld/lib.text.ld.inc').read_text()
paged = (ROOT / 'ld/game.text.ld.inc').read_text()
for module in ('renderconfig',):
    entry = f'build/OUTCODE/src/game/{module}.o (.text);'
    assert entry in resident and entry not in paged, f'{module} must be resident for scheduler callbacks'
linker = (ROOT / 'ge007.ld').read_text()
for symbol in ('renderConfigureViMode',):
    assert f'{symbol} >= _codeSegmentStart && {symbol} < _codeSegmentEnd' in linker
assert '$(wildcard ld/*.ld.inc)' in (ROOT / 'Makefile').read_text()
print('Boot layout: VI settings remain resident and protected by a linker assertion', flush=True)
for path in ('src/game/renderprofile.c', 'src/game/renderprofile.h'):
    assert not (ROOT / path).exists()
for path in ('src/sched.c', 'src/game/rsp.c', 'src/game/rsp.h', 'src/game/lv.c',
             'src/game/options.c', 'src/game/options.h', 'Makefile', 'ge007.ld',
             'ld/lib.text.ld.inc', 'ld/game.data.ld.inc', 'ld/game.rodata.ld.inc', 'ld/game.bss.ld.inc'):
    text = (ROOT / path).read_text()
    assert not re.search(r'renderprofile|renderProfile|RenderProfile|RENDER_STATS', text), path
assert 'osGetCount' not in function(rsp, 'rspGfxTaskStart')
print('Profiler removal: no AA/VI timing hooks, task metadata, menu row or linker entries remain', flush=True)
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
