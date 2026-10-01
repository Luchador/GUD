#!/usr/bin/env python3
"""Run the production display-only UV preview against BG and model identities."""
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
spec = importlib.util.spec_from_file_location('extract', here.parent / 'bg_selection/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
viewport = (src / 'viewport.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-live-uv-') as temp:
    work = Path(temp)
    (work / 'types.inc').write_text(''.join(re.search(
        r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
        for name in ('Vertex', 'SceneBatch', 'ViewportUVPreviewCorner', 'ViewportUVPreviewFace')))
    (work / 'logic.inc').write_text(''.join(extract.function(viewport, name) for name in
        ('ViewportCompareVertexRefs', 'ViewportCompareFaceRefs', 'ViewportCompareUVEdits',
         'ViewportCompareUVFaces', 'ViewportClearUVPreview', 'ViewportBuildUVPreview', 'ViewportPreviewUVs')))
    # Check that both owners route previews independently of the commit path.
    for file, target in (('geditor.c', 'g_Viewport, &g_CurrentBgDocument'),
                         ('modeleditor.c', 'g_ModelViewport, NULL')):
        owner = (src / file).read_text().split('case UVEDITOR_WM_PREVIEW:', 1)[1].split('case ', 1)[0]
        assert 'ViewportPreviewUVs(' + target in owner
    assert 'case UVCANVAS_WM_PREVIEW:' in (src / 'uveditor.c').read_text()
    cleanup = extract.function(viewport, 'ViewportFreeScene')
    assert 'free(state->uvpreview)' in cleanup and 'state->uvpreviewactive = FALSE' in cleanup
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-fsanitize=address,undefined',
                    f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}',
                    str(here / 'check.c'), '-lm', '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
