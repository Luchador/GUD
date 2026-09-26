#!/usr/bin/env python3
"""Exercise the production credits renderer's input and completion behavior."""
import os
from pathlib import Path
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


source = (ROOT / 'src/game/bondview.c').read_text()
front = (ROOT / 'src/game/front.c').read_text()
constants = (ROOT / 'src/bondconstants.h').read_text()
types = (ROOT / 'src/bondtypes.h').read_text()
header = (ROOT / 'src/game/bondview.h').read_text()
declarations = re.search(r'^#define CREDITS_SKIP_HOLD_FRAMES .*$', header, re.M)[0] + '\n'
declarations += re.search(r'^#define BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH .*$', header, re.M)[0] + '\n'
for name in ('CREDITS_ALIGNMENT', 'CREDITS_STATE', 'MENU'):
    declarations += re.search(r'typedef enum ' + name + r'\s*\{.*?\}\s*' + name + ';', constants, re.S)[0] + '\n'
declarations += (ROOT / 'src/levelids.h').read_text() + '\n'
declarations += re.search(r'typedef struct CreditsEntry_s\s*\{.*?\}\s*CreditsEntry;', types, re.S)[0] + '\n'

with tempfile.TemporaryDirectory(prefix='gud-credits-skip-') as directory:
    work = Path(directory)
    (work / 'declarations.inc').write_text(declarations)
    prompt = function(source, 'bondviewRenderCreditsSkipPrompt')
    ai = (ROOT / 'src/game/chrai.c').read_text()
    roll_case = re.search(r'case AI_CreditsRoll:\s*\{.*?(?=\n\s*case AI_IFCreditsHasCompleted:)', ai, re.S)[0]
    (work / 'credits.inc').write_text('\n'.join(function(source, name) for name in (
        'currentPlayerAdjustFade', 'currentPlayerIsFadeComplete', 'currentPlayerUpdateColourScreenProperties',
        'bviewShowUpperMessage')) + prompt + function(source, 'bondviewRenderCredits')
        + '\nstatic void runCreditsRoll(void)\n{\n'
          '    enum { AI_CreditsRoll = 0 };\n'
          '    typedef struct { unsigned char opcode; } AiCreditsRollRecord;\n'
          '    s32 Offset = 0;\n    switch (0) {\n' + roll_case
        + '\n    }\n    assert(Offset == sizeof(AiCreditsRollRecord));\n}\n')
    menu_init = function(front, 'menu_init')
    real_menu_functions = {'interface_menu17_switchscreens', 'interface_menu18_displaycast',
                           'init_menu18_displaycast', 'update_menu18_displaycast',
                           'init_menu07_missionselect', 'init_menu0B_runstage'}
    stubs = ''.join('static void ' + name + '(void) {}\n' for name in sorted(set(
        re.findall(r'\b((?:init|update|interface)_menu\w+)\(\)', menu_init)) - real_menu_functions))
    (work / 'cast_menu.inc').write_text(stubs + '\n'.join(function(front, name) for name in (
        'frontChangeMenu', 'reset_menutimer', 'init_menu0B_runstage', 'do_extended_cast_display',
        'frontFinishPostCreditsCast', 'frontUpdateCastSkip',
        'interface_menu17_switchscreens', 'interface_menu18_displaycast', 'menu_init')))
    # Run the real title-stage menu reset; omit only its N64 memory allocations.
    reset = function((ROOT / 'src/game/initmenus.c').read_text(), 'init_menus_or_reset')
    reset = reset[:reset.index('    ptr_logo_and_walletbond_DL =')] + '    (void)i;\n}\n'
    (work / 'menu_reset.inc').write_text(reset)
    # Exercise the actual text-overlay tail without mocking the model renderer's many dependencies.
    cast_renderer = function(front, 'constructor_menu18_displaycast')
    cast_text = cast_renderer[cast_renderer.rindex('    DL = gfxSetup2DTextureMode(DL);'):]
    (work / 'cast_text.inc').write_text(prompt + '\nstatic Gfx *drawCastText(Gfx *DL)\n{\n'
        '    s32 x, y, textheight, textwidth;\n    char *text;\n    float fade = 1.0f;\n' + cast_text)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-fsanitize=address,undefined',
        '-I', str(work)]
    for test in ('check', 'cast'):
        # The existing menu dispatcher intentionally splits enum cases across several switches.
        flags = ['-Wno-switch'] if test == 'cast' else []
        subprocess.run(command + flags + [str(HERE / (test + '.c')), '-o', str(work / test)], check=True)
        subprocess.run([str(work / test)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                                UBSAN_OPTIONS='halt_on_error=1'))
