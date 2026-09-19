#!/usr/bin/env python3
"""Check the real level table/lookup against the former boss.c allocations."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


constants = (ROOT / 'src/bondconstants.h').read_text()
level = (ROOT / 'src/game/lv.c').read_text()
header = (ROOT / 'src/game/lv.h').read_text()
source = '''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int32_t s32;
typedef int16_t s16;
typedef float f32;
#define ARRAYCOUNT(a) (sizeof(a) / sizeof((a)[0]))
'''
for name in ('ENVIRONMENTDATA_IDS', 'LEVELID', 'MUSIC_TRACKS'):
    source += re.search(r'typedef enum ' + name + r'\s*\{.*?\}[^;]*;', constants, re.S)[0] + '\n'
source += re.search(r'#define LEVEL_INFO_COUNT \d+', header)[0] + '\n'
source += re.search(r'struct LevelEntry\s*\{.*?\};', header, re.S)[0] + '\n'
source += re.search(r'struct LevelEntry g_LevelInfoTable\[\] = \{.*?\n\};', level, re.S)[0] + '\n'
source += function(level, 'lvFindLevelInfo')
source += function(level, 'lvFindStageInfo')
source += function(level, 'lvGetMemoryAllocationString')
# Exercise the production filename construction (including its MP prefix guard).
setup = (ROOT / 'src/game/setup.c').read_text()
start = setup.index('        setupFileName[0] = levelInfo->setupFileName[0];')
end = setup.index('        g_ptrStageSetupFile = _fileNameLoadToBank(', start)
source += """static s32 selectedPlayers;
static s32 getPlayerCount(void) { return selectedPlayers; }
static void setupName(LEVELID id, s32 players, char *out) {
    struct LevelEntry *levelInfo = lvFindStageInfo(id, players);
    char setupFileName[0x100] = "";
    selectedPlayers = players;
    if (!levelInfo || !levelInfo->setupFileName) { out[0] = 0; return; }
"""
source += setup[start:end] + 'strcpy(out, setupFileName); }\n'
source += (HERE / 'check.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-level-memory-') as directory:
    work = Path(directory)
    (work / 'check.c').write_text(source)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-sign-compare', '-fsanitize=address,undefined', f'-I{HERE}',
        str(work / 'check.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True, env=dict(
        os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
