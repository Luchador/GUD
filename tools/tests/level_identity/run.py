#!/usr/bin/env python3
"""Exercise campaign permutations and catalog permutations with production C."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def function(source, name):
    m = re.search(r'^[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert m, name
    end = source.index('{', m.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[m.start():end] + '\n'


constants = (ROOT / 'src/bondconstants.h').read_text()
front = (ROOT / 'src/game/front.c').read_text()
source = '''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "campaign.h"
typedef unsigned char u8;
typedef int32_t s32;
typedef int16_t s16;
typedef float f32;
typedef int bool;
#define TRUE 1
#define FALSE 0
#define ARRAYCOUNT(a) (sizeof(a) / sizeof((a)[0]))
#define getStringID(bank, string) 1
enum { MISSION_PART, MISSION_HEADER };
'''
for name in ('DIFFICULTY', 'STAGESTATUS', 'MUSIC_TRACKS'):
    source += re.search(r'typedef enum ' + name + r'\s*\{.*?\}[^;]*;', constants, re.S)[0] + '\n'
source += re.search(r'enum WALLETBOND_SWITCHNAMES\s*\{.*?\}\s*;', constants, re.S)[0] + '\n'
source += re.search(r'struct mission_folder_setup mission_folder_setup_entries\[\] = \{.*?\n\};', front, re.S)[0] + '\n'
source += 'const int g_CampaignEntryCount = ARRAYCOUNT(mission_folder_setup_entries) - 1;\n'
source += '''typedef struct { int completed[SP_LEVEL_MAX][4]; } save_data;
static save_data saves[4];
static int missing_saves;
static int fileIsFolderValid(int folder) { return folder >= 0 && folder < 4; }
static save_data *fileGetSaveForFoldernum(int folder) { return !missing_saves && fileIsFolderValid(folder) ? &saves[folder] : NULL; }
static int fileGetSaveStageCompletedForDifficulty(save_data *save, LEVEL_SOLO_SLOT slot, int difficulty)
{ return slot >= 0 && slot < SP_LEVEL_MAX && difficulty >= 0 && difficulty < 3 && save->completed[slot][difficulty]; }
enum { FOLDER1 = 0, MAX_FOLDER_COUNT = 4 };
'''
file = (ROOT / 'src/game/file2.c').read_text()
for name in ('fileIsStageUnlockedAtDifficulty', 'fileGetHighestStageDifficultyCompletedForFolder',
             'fileGetHighestStageUnlockedForFolder', 'fileGetHighestStageUnlockedAnyFolder'):
    source += function(file, name)
source += '''static int selected_folder_num;
static LEVEL_SOLO_SLOT cheatWrittenSlot;
static void fileUnlockStageInFolderAtDifficulty(int folder, LEVEL_SOLO_SLOT slot, DIFFICULTY difficulty, int time)
{
    assert(folder == selected_folder_num && difficulty == DIFFICULTY_AGENT && time == 0x5F5E0FF);
    cheatWrittenSlot = slot;
}
'''
source += function((ROOT / 'src/game/cheat.c').read_text(), 'cheatButtonUnlockMission')
source += (HERE / 'reference.c').read_text()
lv = (ROOT / 'src/game/lv.c').read_text()
header = (ROOT / 'src/game/lv.h').read_text()
source += re.search(r'struct LevelEntry\s*\{.*?\};', header, re.S)[0] + '\n'
source += re.search(r'enum LevelCatalogSlot\s*\{.*?\};', header, re.S)[0] + '\n'
source += '#define LEVEL_MEMORY_STRING_SIZE 64\n'
source += lv[lv.index('#define LEVEL('):lv.index('struct LevelEntry *lvFindLevelInfo')]
for name in ('lvFindLevelInfo', 'lvFindStageInfo', 'lvGetMemoryAllocationString'):
    source += function(lv, name)
bg = (ROOT / 'src/game/bg.c').read_text()
source += re.search(r'typedef struct s_specialportal\s*\{.*?\} s_specialportal;', (ROOT / 'src/game/bg.h').read_text(), re.S)[0] + '\n'
source += re.search(r'static const s_specialportal specialportalarray\[\] = \{.*?\n\};', bg, re.S)[0] + '\n'
source += 'static LEVELID g_CurrentBgLevelId;\nstatic u8 g_BgPortals[128 * 8];\n'
source += function(bg, 'bgMarkSpecialPortals')
source += function((ROOT / 'tools/aiParse.c').read_text(), 'levelIdName')
source += (HERE / 'check.c').read_text()

with tempfile.TemporaryDirectory(prefix='gud-level-identity-') as temp:
    work = Path(temp)
    (work / 'check.c').write_text(source)
    catalog = (ROOT / 'src/game/leveltable.inc').read_text()
    rows = [line for line in catalog.splitlines() if line.startswith('LEVEL')]
    for label, contents in [('original', catalog), ('reversed', '\n'.join(reversed(rows)) + '\n')]:
        (work / 'leveltable.inc').write_text(contents)
        command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-sign-compare', '-fsanitize=address,undefined', f'-I{work}', f'-I{ROOT / "src/game"}',
                   str(work / 'check.c'), str(ROOT / 'src/game/campaign.c'), '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check')], check=True, env=dict(os.environ,
            ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        print(f'PASS: {label} level catalog.')
