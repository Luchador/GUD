#!/usr/bin/env python3
"""Exercise actual native gradient construction and shared math, without a ROM."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
here=Path(__file__).resolve().parent
root=here.parents[3]
def function(source,name):
    match=re.search(r'^[\w *]+\b'+name+r'\([^;{}]*\)\s*\{',source,re.M)
    start=source.index('{',match.start());depth=1;end=start+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[match.start():end]+'\n'
with tempfile.TemporaryDirectory(prefix='geditor-sky-gradient-') as temp:
    work=Path(temp)
    header=(root/'src/game/environment.h').read_text()
    (work/'types.inc').write_text(header[header.index('typedef struct SkySettings'):header.index('extern EnvironmentRecord')])
    sky=(root/'src/game/sky.c').read_text()
    (work/'runtime.inc').write_text(''.join(function(sky,n) for n in
        ('skyChooseCloudVtxColour','skySetCloudVertex','skyRenderGradient')))
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
        '-fsanitize=address,undefined',f'-I{work}',str(here/'runtime.c'),'-lm','-o',str(work/'check')],check=True)
    subprocess.run([str(work/'check')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
