#!/usr/bin/env python3
"""Exercise the production BG x-ray decoder/dispatch without an emulator."""
from pathlib import Path
import os
import re
import shlex
import struct
import subprocess
import tempfile
import zlib

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


source = (HERE.parent / 'render_options/harness.h').read_text().split('static u32 g_TestButtons;')[0]
source += (HERE / 'harness.h').read_text()
source += strip_includes((ROOT / 'src/game/bgdebug.h').read_text())
debug = strip_includes((ROOT / 'src/game/bgdebug.c').read_text())
# Only adapt KSEG0 addresses for the host's memory model; GBI words remain 32-bit.
debug = debug.replace('(const Gfx *)(physical | 0x80000000)', '(const Gfx *)(g_TestRam + physical)')
source += debug
bg = (ROOT / 'src/game/bg.c').read_text()
for name in ('bgRenderRoomPrimary', 'bgRenderRoomSecondary'):
    source += function(bg, name)
source += (HERE / 'check.c').read_text()

lv = (ROOT / 'src/game/lv.c').read_text()
assert 'bgDebugReset();' in function(bg, 'bgLoadFile')
assert 'bgDebugBeginFrame();' in function(lv, 'lvRender')
hud = function(lv, 'lvDrawFrameRateDisplay')
assert hud.index('bgDebugRender(gdl)') < hud.index('gfxSetup2DTextureMode(gdl)') < hud.index('bgDebugDrawHud(gdl)')
input_tick = function(lv, 'lvlViewMoveTick')
assert 'joyGetButtonsPressedThisFrame(local_player_number, U_JPAD | D_JPAD)' in input_tick
assert '!g_ControlsLockedFlag && g_CameraMode == CAMERAMODE_FP' in input_tick
assert '!g_CurrentPlayer->pausing_flag' in input_tick and '!g_CurrentPlayer->pause_state' in input_tick
for section in ('text', 'data', 'rodata', 'bss'):
    assert f'bgdebug.o (.{section})' in (ROOT / f'ld/game.{section}.ld.inc').read_text()
    assert f'bgdebug.o (.{section})' not in (ROOT / f'ld/lib.{section}.ld.inc').read_text()

with tempfile.TemporaryDirectory(prefix='gud-bg-debug-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    (work / 'PR').mkdir()
    gbi = (ROOT / 'include/PR/gbi.h').read_text()
    gbi = gbi.replace('uintptr_t w0;', 'u32 w0;').replace('uintptr_t w1;', 'u32 w1;')
    for arg in ('m1a', 'm1b', 'm2a', 'm2b'):
        gbi = gbi.replace(f'({arg}) <<', f'((u32)({arg})) <<')
    (work / 'PR/gbi.h').write_text(gbi)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-sign-compare', '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
        '-Wno-missing-braces', '-fsanitize=address,undefined',
        '-I', str(work), '-idirafter', str(ROOT / 'include')]
    subprocess.run(command + [str(work / 'check.c'), '-o', str(work / 'check')], check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    subprocess.run([str(work / 'check')], check=True, env=env)
    # Scan authored primary AND secondary geometry across every included stage.
    # Texture markers are intentionally ignored, just like expanded materials;
    # this tests geometry coverage, not texture expansion or rasterization.
    total = 0
    for asset in sorted((ROOT / 'assets/obseg/bg').rglob('*.c')):
        fixture = bytearray()
        for body in re.findall(r'u32 (?:pri|sec)_mapping_binary_\d+\[\] = \{(.*?)\};', asset.read_text(), re.S):
            body = re.sub(r'/\*.*?\*/|//[^\n]*', '', body, flags=re.S)
            data = b''.join(struct.pack('>I', int(word.strip(), 0)) for word in body.split(',') if word.strip())
            if data[:2] != b'\x11\x72':
                continue
            data = zlib.decompress(data[2:], -15)
            commands = []
            expected = 0
            for w0, w1 in struct.iter_unpack('>II', data):
                commands.append((w0, w1))
                if w0 >> 24 in (4, 0xbf, 0xb1):
                    expected += 1
                if w0 >> 24 == 0xb8:
                    break
            data = b''.join(struct.pack('>II', *pair) for pair in commands)
            fixture += struct.pack('>II', len(data), expected) + data
            total += 1
        if fixture:
            path = work / (asset.parent.name + '-' + asset.stem + '.bin')
            path.write_bytes(fixture)
            subprocess.run([str(work / 'check'), str(path)], check=True, env=env)
    print(f'PASS: {total} native primary/secondary streams; paged linkage and frame/input integration.')
