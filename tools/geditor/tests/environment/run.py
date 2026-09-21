#!/usr/bin/env python3
"""Environment overrides: real serialization, ROM bytes, merging and runtime transitions."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]

def function(source, name):
    match = re.search(r'^[\w *]+\b' + name + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError(name)
    start = source.index('{', match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'

with tempfile.TemporaryDirectory(prefix='geditor-environment-') as folder:
    work = Path(folder)
    panel = (src/'environmentpanel.c').read_text()
    editor = (src/'geditor.c').read_text()
    (work/'ui_types.inc').write_text(panel[panel.index('enum { ENV_VARIANT_LABEL'):panel.index('static EnvironmentPanel *State')])
    (work/'ui.inc').write_text(''.join(function(editor, name) for name in
        ('GEditorPreviewEnvironment', 'GEditorApplyEnvironment')) + ''.join(function(panel, name) for name in
        ('State', 'Owner', 'Checkbox', 'Ready', 'Preview', 'Status', 'Load', 'Commit', 'Reset', 'EnvironmentPanelApply',
         'EnvironmentPanelHasDraft', 'EnvironmentPanelRefresh', 'EnvironmentPanelShow')) + function(editor, 'GEditorOpenProject'))
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        '-Dfopen=TestFopen', f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}']
    sources = [str(here/'check.c'), str(here.parent/'image_import/platform.c')]
    sources += [str(src/name) for name in ('environment.c', 'fog.c', 'project.c', 'levelmemory.c')]
    subprocess.run(command + sources + ['-Wl,--gc-sections', '-lm', '-o', str(work/'check')], check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    subprocess.run([str(work/'check'), str(work)], env=env, check=True)

    engine = (root/'src/game/environment.c').read_text()
    header = (root/'src/game/environment.h').read_text()
    (work/'types.inc').write_text(header[header.index('typedef struct SkySettings'):header.index('extern EnvironmentRecord')])
    (work/'engine.inc').write_text(''.join(function(engine, name) for name in
        ('envFindEnvironment', 'envLoadLevelEnvironment', 'envSwitchToSoloSky2')))
    subprocess.run(command + [str(here/'engine.c'), '-o', str(work/'engine')], check=True)
    subprocess.run([str(work/'engine')], env=env, check=True)

    character = (root/'src/game/chr.c').read_text()
    prop = (root/'src/game/propobj.c').read_text()
    fogtypes = engine[engine.index('static struct FogDetails'):engine.index('static f32 g_FarFogIntensity')]
    defines = '\n'.join(re.findall(r'^#define (?:CHRFADE|OBJFADE)_.*$', character + '\n' + prop, re.M))
    (work/'fade.inc').write_text(fogtypes + defines + '\n' + ''.join(function(engine, name) for name in
        ('envLoadCurrentEnvironment', 'envIsPropVisibleThroughFog', 'envGetPropDistColor'))
        + function(character, 'chrCalcScreenFadeAlpha') + function(prop, 'objCalcScreenFadeAlpha'))
    subprocess.run(command + [str(here/'visibility.c'), '-lm', '-o', str(work/'fade')], check=True)
    subprocess.run([str(work/'fade')], env=env, check=True)
