#!/usr/bin/env python3
from pathlib import Path
import os
import re
import subprocess
import tempfile
here=Path(__file__).resolve().parent
src=here.parents[1]/'src'
root=src.parents[2]
with tempfile.TemporaryDirectory(prefix='geditor-level-assets-') as folder:
    exe=Path(folder)/'check'
    command=[os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
        '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-Dfopen=TestFopen',
        f'-I{here.parent/"image_import"}',f'-I{src}',f'-I{root}',str(here/'check.c'),str(here.parent/'image_import/platform.c')]
    command += [str(src/name) for name in ('levelslots.c','project.c','environment.c','levelmemory.c','fog.c','rom.c','briefing.c','texinfo.c','texencode.c')]
    subprocess.run(command+['-Wl,--gc-sections','-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe),folder,str(root/'assets/obseg/prop/PwalletbondZ.bin')],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))

    panel=(src/'projectsettings.c').read_text()
    def function(name):
        m=re.search(r'^[\w *]+\b'+name+r'\([^;]*?\)\s*\{',panel,re.M);assert m,name
        end=panel.index('{',m.start())+1;depth=1
        while depth:
            depth+=(panel[end]=='{')-(panel[end]=='}');end+=1
        return panel[m.start():end]+'\n'
    Path(folder,'inspector.inc').write_text(''.join(function(n) for n in
        ('ProjectSettingsHasDraft','ProjectSettingsApply','ProjectSettingsSelectSlot','ProjectSettingsAssignSlot')))
    ui=Path(folder)/'ui'
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        f'-I{folder}',f'-I{src}',f'-I{root/"src"}',str(here/'ui.c'),'-o',str(ui)],check=True)
    subprocess.run([str(ui)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
