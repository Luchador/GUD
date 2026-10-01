#!/usr/bin/env python3
"""Check outliner identities, refreshes and viewport selection/framing dispatch."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


extract = load('extract', here.parent / 'portal_editing/run.py')
names = load('names', here.parents[1] / 'scripts/generate_character_names.py')
assert names.generate() == (src / 'characternames.h').read_text()
source = (src / 'sceneoutliner.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-outliner-') as temp:
    work = Path(temp)
    (work / 'types.inc').write_text(''.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', source, re.S)[0] + '\n'
        for name in ('SceneOutlinerRow', 'SceneOutlinerGroup', 'SceneOutlinerState')))
    (work / 'names.inc').write_text(extract.function(
        (src / 'characterload.c').read_text(), 'CharacterGetBodyName'))
    (work / 'outliner.inc').write_text(''.join(extract.function(source, name) for name in (
        'SceneOutlinerKey', 'SceneOutlinerLabel', 'SceneOutlinerBeginUpdate',
        'SceneOutlinerDeleteRow', 'SceneOutlinerRefreshGroup', 'SceneOutlinerRefresh',
        'SceneOutlinerSelect', 'SceneOutlinerActivate',
        'SceneOutlinerQueueFrame', 'SceneOutlinerFramePending')))
    (work / 'frame_message.inc').write_text(re.search(
        r'    case SCENEOUTLINER_WM_FRAME:.*?return 0;', source, re.S)[0])
    (work / 'double_click.inc').write_text(re.search(
        r'else if \(\(\(NMHDR \*\)lparam\)->code == NM_DBLCLK\)\s*\{(.*?)\n        \}', source, re.S)[1])
    (work / 'dispatch.inc').write_text(extract.function(
        (src / 'geditor.c').read_text(), 'GEditorSelectSceneItem'))
    panel = (src / 'rightpanel.c').read_text()
    (work / 'layout_types.inc').write_text(
        '\n'.join(line for line in panel.splitlines() if line.startswith('#define RIGHTPANEL_')) + '\n'
        + re.search(r'typedef enum RightPanelSplitter \{.*?\} RightPanelSplitter;', panel, re.S)[0] + '\n'
        + re.search(r'typedef struct RightPanelState \{.*?\} RightPanelState;', panel, re.S)[0] + '\n')
    (work / 'layout.inc').write_text(''.join(extract.function(panel, name) for name in (
        'RightPanelTransformTop', 'RightPanelClampLayout', 'RightPanelDragSplitter', 'RightPanelInSplitter')))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
        ]
    for check in ('check', 'layout'):
        subprocess.run(command + [str(here / (check + '.c')), '-o', str(work / check)], check=True)
        subprocess.run([str(work / check)], check=True,
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
