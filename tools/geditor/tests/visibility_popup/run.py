#!/usr/bin/env python3
"""Exercise production popup state and toolbar dispatch under focus reentrancy."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
SRC = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else HERE.parents[1] / 'src'


def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


popup = (SRC / 'visibilitymenu.c').read_text()
toolbar = (SRC / 'tooltoolbar.c').read_text()
constants = '\n'.join(re.findall(r'^#define TOOLTOOLBAR_.*', toolbar, re.M))
enum = re.search(r'typedef enum ToolToolbarMenu \{.*?\} ToolToolbarMenu;',
                 (SRC / 'tooltoolbar.h').read_text(), re.S)[0]
state = re.search(r'typedef struct VisibilityMenuState \{.*?\} VisibilityMenuState;', popup, re.S)[0]
command = toolbar[toolbar.index('    case WM_COMMAND:'):toolbar.index('    case WM_DRAWITEM:')]
with tempfile.TemporaryDirectory(prefix='geditor-visibility-popup-') as tmp:
    work = Path(tmp)
    (work / 'types.inc').write_text(constants + '\n' + enum + '\n' + state)
    (work / 'popup.inc').write_text(''.join(function(popup, name) for name in
        ('VisibilityMenuGetState', 'VisibilityMenuClose', 'VisibilityMenuOpen')))
    (work / 'toolbar.inc').write_text('''
static void ToolbarClick(int tool)
{
    HWND hwnd = TOOLBAR;
    ToolToolbarState *state = &toolbar;
    WPARAM wparam = TOOLTOOLBAR_MENU_FIRST_ID + tool; /* BN_CLICKED is zero. */
    switch (WM_COMMAND) {
''' + command.replace('return 0;', 'return;') + '\n    }\n}\n')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined', '-I'+str(work),
        str(HERE / 'check.c'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
