#!/usr/bin/env python3
"""Owner-drawn menus must preserve commands, data, labels and keyboard access."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
HERE = Path(__file__).resolve().parent
SOURCE = (HERE.parents[1] / 'src/theme.c').read_text()
def function(name):
    m = re.search(r'^static [\w *]+\b' + name + r'\([^;]*?\)\s*\{', SOURCE, re.M)
    assert m, name
    p = SOURCE.index('{', m.start())+1
    depth=1
    while depth:
        depth += (SOURCE[p]=='{')-(SOURCE[p]=='}'); p+=1
    return SOURCE[m.start():p]+'\n'
with tempfile.TemporaryDirectory(prefix='geditor-theme-') as directory:
    work=Path(directory)
    declaration=re.search(r'typedef struct ThemeMenuItem \{.*?\} ThemeMenuItem;',SOURCE,re.S)[0]
    (work/'menus.inc').write_text(declaration+'\nstatic ThemeMenuItem *g_MenuItems;\n'+''.join(
        function(n) for n in ('ThemeFindMenuItem','ThemeRestoreMenu','ThemePrepareMenu','ThemeMenuChar')))
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror','-Wno-sign-compare',
        '-fsanitize=address,undefined','-I'+str(work),str(HERE/'check.c'),'-o',str(work/'check')],check=True)
    subprocess.run([str(work/'check')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
