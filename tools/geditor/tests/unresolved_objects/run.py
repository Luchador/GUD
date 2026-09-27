#!/usr/bin/env python3
"""Exercise production fallback-box selection, occlusion, clipping and gizmo position."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

here=Path(__file__).resolve().parent
src=here.parents[1]/'src'
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('extract',here.parent/'portal_editing/run.py')
extract=importlib.util.module_from_spec(spec);spec.loader.exec_module(extract)
viewport=(src/'viewport.c').read_text()
with tempfile.TemporaryDirectory(prefix='geditor-unresolved-') as temp:
    work=Path(temp)
    types=''.join(re.search(r'typedef struct '+n+r' \{.*?\} '+n+';',viewport,re.S)[0]+'\n'
                  for n in ('Vertex','ViewportPickRay','ViewportPad'))
    types+='\n'.join(line for line in viewport.splitlines() if line.startswith((
        '#define VIEWPORT_PICK_', '#define VIEWPORT_FOV_Y ', '#define VIEWPORT_NEAR_Z ',
        '#define VIEWPORT_FAR_Z ', '#define VIEWPORT_DEG_TO_RAD ', '#define VIEWPORT_BOX_VERTICES ')))
    (work/'types.inc').write_text(types+'\n')
    (work/'selection.inc').write_text(extract.function((src/'setupload.c').read_text(),'SetupFileGetModelPad')+
        ''.join(extract.function(viewport,n) for n in ('ViewportSelectedPadIndex','ViewportPadVisible',
        'ViewportPadHasMissingModel','ViewportPadSelectionPosition','ViewportGetBasis','ViewportProject',
        'ViewportBuildPickRay','ViewportRayTriangleDistance','ViewportCoplanarPickTolerance',
        'ViewportProjectEdgePoint','ViewportOverlayPointVisible','ViewportStanComponentVisible',
        'ViewportPadEdgePoint','ViewportTryPickPad')))
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
        '-Wno-unused-parameter','-fsanitize=address,undefined',f'-I{work}',
        f'-I{here.parent/"image_import"}',f'-I{src}',f'-I{src.parents[2]}',
        str(here/'check.c'),'-lm','-o',str(work/'check')],check=True)
    subprocess.run([str(work/'check')],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
