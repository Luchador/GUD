#!/usr/bin/env python3
"""Native stan clipboard, graph isolation, transforms, transactions and failures."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
src = here.parents[1] / 'src'
spec = importlib.util.spec_from_file_location('extract', here.parent / 'portal_editing/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
with tempfile.TemporaryDirectory(prefix='geditor-stan-clipboard-') as temp:
    work = Path(temp)
    (work / 'stan').mkdir()
    deletion = (here.parent / 'stan_deletion/check.c').read_text()
    topology = (here.parent / 'stan_topology/check.c').read_text()
    extrusion = (here.parent / 'stan_extrusion/check.c').read_text()
    (work / 'fixture.inc').write_text(''.join(extract.function(deletion, n) for n in ('Put', 'Put16', 'Same'))
        + extract.function(topology, 'Fixture')
        + ''.join(extract.function(extrusion, n) for n in ('Find', 'Connections')))
    viewport = (src / 'viewport.c').read_text()
    names = ('ViewportStanVisible', 'ViewportCompareStanIds', 'ViewportStanTileHidden',
        'ViewportCompareStanRefs', 'ViewportStanPointRef', 'ViewportClearStanSelection',
        'ViewportGetStanSelectionCount', 'ViewportGetSelectedStanTiles', 'ViewportSelectStanTiles', 'ViewportFindStanComponent',
        'ViewportSetStanVertex', 'ViewportRefreshStanOverlay', 'ViewportSetStanTiles')
    (work / 'viewport.inc').write_text(re.search(r'^#define VIEWPORT_SELECTION_GOLD .*', viewport, re.M)[0] + '\n'
        + ''.join(extract.function(viewport, n) for n in names))
    harness = topology[topology.index('typedef void *HWND;'):topology.index('static void Visibility(')]
    (work / 'viewport_harness.inc').write_text(harness)
    editor = (src / 'geditor.c').read_text()
    names = ('GEditorCanUseStanClipboard', 'GEditorCanCopyStanTiles', 'GEditorCanPasteStanTiles',
        'GEditorSnapshotStanTiles', 'GEditorCopyStanTiles', 'GEditorPasteStanSnapshot',
        'GEditorPasteStanTiles', 'GEditorDuplicateStanTiles')
    (work / 'controller.inc').write_text(''.join(extract.function(editor, n) for n in names))
    executable = work / 'check'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined',
        f'-I{here.parent / "image_import"}', f'-I{src}', f'-I{work}', str(here / 'check.c'),
        str(here.parent / 'image_import/platform.c'),
        *[str(src / n) for n in ('stanload.c', 'stantopology.c', 'stanedit.c', 'standelete.c',
            'stanquery.c', 'standiscontinuity.c', 'bghistory.c', 'rotation.c', 'scaling.c')],
        '-Wl,--gc-sections', '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-lm', '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(work)], check=True,
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
