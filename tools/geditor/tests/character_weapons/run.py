#!/usr/bin/env python3
"""Character hand assignment, native persistence/history and UI routing tests."""
import importlib.util
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
root = src.parents[2]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('choices', src.parent / 'scripts/generate_weapon_choices.py')
choices = importlib.util.module_from_spec(spec)
spec.loader.exec_module(choices)
assert (src / 'weaponchoices.h').read_text() == choices.generate(), 'Regenerate weaponchoices.h'
assert (src / 'hatchoices.h').read_text() == choices.generate_hats(), 'Regenerate hatchoices.h'
with tempfile.TemporaryDirectory(prefix='geditor-character-weapons-') as temp:
    work = Path(temp)
    (work / 'setup').mkdir()
    command = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
               f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{root}', f'-I{work}']
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    subprocess.run(command + [str(here / 'check.c'), str(here.parent / 'image_import/platform.c')]
                   + [str(src / name) for name in ('setupload.c', 'actionblocks.c', 'bghistory.c')]
                   + ['-Wl,--gc-sections', '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-Wl,--wrap=realloc',
                      '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True, env=env)

    spec = importlib.util.spec_from_file_location('extract', here.parent / 'object_properties/run.py')
    extract = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(extract)
    panel = (src / 'characterproperties.c').read_text()
    (work / 'input-types.inc').write_text(re.search(r'enum \{ CHARACTER_RIGHT_LABEL,.*?\};', panel, re.S)[0]
        + '\n' + re.search(r'typedef struct CharacterPropertiesState \{.*?\} CharacterPropertiesState;', panel, re.S)[0])
    (work / 'input.inc').write_text(''.join(extract.function(panel, name) for name in
        ('CharacterPropertiesChoices', 'CharacterPropertiesApply', 'CharacterPropertiesHatChoices', 'CharacterPropertiesApplyHat')))
    (work / 'catalog.inc').write_text(''.join(extract.function((src / 'setupload.c').read_text(), name)
        for name in ('SetupWeaponChoices', 'SetupWeaponChoiceForItem', 'SetupHatChoices', 'SetupHatChoiceForModel')))
    (work / 'editor.inc').write_text(''.join(extract.function((src / 'geditor.c').read_text(), name)
        for name in ('GEditorSetCharacterWeapon', 'GEditorSetCharacterHat')))
    subprocess.run(command + [str(here / 'input.c'), '-Wl,--gc-sections', '-lm', '-o', str(work / 'input')], check=True)
    subprocess.run([str(work / 'input')], check=True, env=env)
    subprocess.run(command + [str(here / 'preview.c'), str(here.parent / 'image_import/platform.c')]
        + [str(src / name) for name in ('modelload.c', 'modelmaterials.c', 'bgmaterial.c', 'bgrender.c', 'setupload.c')]
        + ['-Wl,--gc-sections', '-lm', '-o', str(work / 'preview')], check=True)
    subprocess.run([str(work / 'preview'), str(root)], check=True, env=env)
